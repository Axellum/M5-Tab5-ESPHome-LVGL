# -*- coding: utf-8 -*-
"""tools/mesure_conso.py — Consommation de la tablette sur batterie, scénario par scénario.

Mesure ce que consomme une tablette qui tourne sur sa batterie (USB débranché) dans
plusieurs cas : écran à 100 / 50 / 10 %, écran éteint, micro (« Okay Nabu ») coupé,
haut-parleur coupé, tout coupé. Le script pilote la tablette par Home Assistant (API
REST, un jeton longue durée), lit « Tab5 Consommation » (W) et écrit un CSV à partager.
Il remet à la fin les réglages trouvés au départ, même après Ctrl+C.

Measures the tablet's power draw on battery (USB unplugged) in several cases, through
Home Assistant's REST API, and writes a CSV to share. Restores the settings at the end.

Limites :
- « Tab5 Consommation » = tension × courant de l'INA226, sur le chemin de la batterie
  seulement : sur USB rien n'est mesuré, d'où l'exigence « Tab5 Sur batterie » = on.
- Une lecture par minute (update_interval 60 s de l'INA226, tab5-sensors-diagnostics.yaml),
  instantanée (moyenne de 16 conversions) : 5 min par scénario donnent ~4 valeurs
  utiles ; de quoi classer les gros postes, pas les écarts de quelques dixièmes de W.
- Aucune dépendance hors de la bibliothèque standard de Python 3.9+.

Usage :
    set TAB5_HA_TOKEN=<jeton longue durée>        (PowerShell : $env:TAB5_HA_TOKEN = "…")
    python tools/mesure_conso.py --ha http://homeassistant.local:8123
    python tools/mesure_conso.py --ha https://… --insecure      # certificat auto-signé
    python tools/mesure_conso.py --ha … --duree 180              # 3 min par scénario
    python tools/mesure_conso.py --ha … --essai                  # 20 s par scénario, sans
                                                                 # exiger la batterie (vérifie
                                                                 # le pilotage, rien de plus)
Le jeton se crée dans HA : profil → Sécurité → Jetons d'accès longue durée. Il n'est
jamais écrit dans le CSV ni affiché.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import getpass
import json
import os
import re
import ssl
import statistics
import sys
import time
import urllib.error
import urllib.request

# Entités de la tablette, retrouvées par la fin de leur entity_id (le début dépend du nom
# de l'appareil et de la pièce : « m5stack_… », « salon_m5stack_… »). Noms donnés par le
# firmware : ils ne dépendent pas de la langue de HA.
SUFFIXES = {
    "conso": ("sensor", "tab5_consommation"),
    "sur_batterie": ("binary_sensor", "tab5_sur_batterie"),
    "courant": ("sensor", "tab5_courant_batterie"),       # désactivée par défaut
    "tension": ("sensor", "tab5_tension_batterie"),       # désactivée par défaut
    "niveau": ("sensor", "tab5_batterie"),                # désactivée par défaut
    "temp": ("sensor", "tab5_core_temp"),
    "ecran": ("light", "display_backlight"),
    "micro": ("switch", "tab5_wake_word_active"),
    "hp": ("switch", "speaker_enable"),
    "economie": ("select", "tab5_economie_d_energie"),
    "extinction": ("select", "tab5_extinction_auto_de_l_ecran"),
}
OBLIGATOIRES = ("conso", "sur_batterie", "ecran", "micro", "hp", "economie", "extinction")

# (nom, luminosité de l'écran en % ou 0 = éteint, micro, haut-parleur). Le premier sert de
# référence ; il est rejoué à la fin pour voir la dérive (batterie qui baisse, chaleur).
SCENARIOS = (
    ("reference_ecran100", 100, True, True),
    ("ecran50", 50, True, True),
    ("ecran10", 10, True, True),
    ("ecran_eteint", 0, True, True),
    ("ecran100_sans_micro", 100, False, True),
    ("ecran100_sans_hp", 100, True, False),
    ("tout_coupe", 0, False, False),
    ("reference_fin", 100, True, True),
)
ATTENTE_STABLE_S = 20   # lectures ignorées juste après un changement
SONDE_S = 5             # fréquence de lecture de HA


class HA:
    def __init__(self, base: str, jeton: str, insecure: bool):
        self.base = base.rstrip("/")
        self.entetes = {"Authorization": f"Bearer {jeton}", "Content-Type": "application/json"}
        self.ctx = ssl._create_unverified_context() if insecure else None

    def _req(self, methode: str, chemin: str, corps=None):
        donnees = json.dumps(corps).encode() if corps is not None else None
        r = urllib.request.Request(self.base + chemin, data=donnees, headers=self.entetes, method=methode)
        with urllib.request.urlopen(r, timeout=20, context=self.ctx) as rep:
            texte = rep.read().decode("utf-8")
        return json.loads(texte) if texte.startswith(("{", "[")) else texte

    def etats(self):
        return self._req("GET", "/api/states")

    def etat(self, entity_id: str):
        return self._req("GET", f"/api/states/{entity_id}")

    def service(self, domaine: str, service: str, donnees: dict):
        return self._req("POST", f"/api/services/{domaine}/{service}", donnees)

    def modele(self, texte: str) -> str:
        try:
            return str(self._req("POST", "/api/template", {"template": texte})).strip()
        except (urllib.error.URLError, OSError):
            return ""


def trouver_entites(ha: HA) -> dict[str, str]:
    """Une entité par rôle ; erreur si un rôle obligatoire manque ou est ambigu."""
    ids = [e["entity_id"] for e in ha.etats()]
    trouvees, erreurs = {}, []
    for role, (domaine, suffixe) in SUFFIXES.items():
        motif = re.compile(rf"^{domaine}\.(.+_)?{suffixe}(_\d+)?$")
        candidats = sorted(i for i in ids if motif.match(i))
        if len(candidats) == 1:
            trouvees[role] = candidats[0]
        elif len(candidats) > 1:
            erreurs.append(f"{role}: several entities match ({', '.join(candidats)})")
        elif role in OBLIGATOIRES:
            erreurs.append(f"{role}: no entity ending in '{suffixe}' ({domaine})")
    if erreurs:
        sys.exit("Tab5 entities not found:\n  " + "\n  ".join(erreurs))
    return trouvees


def lire(ha: HA, ent: dict[str, str], role: str) -> str:
    if role not in ent:
        return ""
    try:
        return ha.etat(ent[role]).get("state", "")
    except (urllib.error.URLError, OSError):
        return ""


def nombre(texte: str):
    try:
        return float(texte)
    except (TypeError, ValueError):
        return None


def sauvegarder(ha: HA, ent: dict[str, str]) -> dict:
    ecran = ha.etat(ent["ecran"])
    return {
        "ecran_on": ecran["state"] == "on",
        "ecran_brightness": ecran.get("attributes", {}).get("brightness"),
        "micro": ha.etat(ent["micro"])["state"],
        "hp": ha.etat(ent["hp"])["state"],
        "economie": ha.etat(ent["economie"])["state"],
        "extinction": ha.etat(ent["extinction"])["state"],
    }


def restaurer(ha: HA, ent: dict[str, str], s: dict) -> None:
    ha.service("select", "select_option", {"entity_id": ent["economie"], "option": s["economie"]})
    ha.service("select", "select_option", {"entity_id": ent["extinction"], "option": s["extinction"]})
    ha.service("switch", "turn_on" if s["micro"] == "on" else "turn_off", {"entity_id": ent["micro"]})
    ha.service("switch", "turn_on" if s["hp"] == "on" else "turn_off", {"entity_id": ent["hp"]})
    if s["ecran_on"]:
        donnees = {"entity_id": ent["ecran"]}
        if s["ecran_brightness"] is not None:
            donnees["brightness"] = s["ecran_brightness"]
        ha.service("light", "turn_on", donnees)
    else:
        ha.service("light", "turn_off", {"entity_id": ent["ecran"]})


def appliquer(ha: HA, ent: dict[str, str], ecran_pct: int, micro: bool, hp: bool) -> None:
    ha.service("switch", "turn_on" if micro else "turn_off", {"entity_id": ent["micro"]})
    ha.service("switch", "turn_on" if hp else "turn_off", {"entity_id": ent["hp"]})
    if ecran_pct > 0:
        ha.service("light", "turn_on", {"entity_id": ent["ecran"], "brightness_pct": ecran_pct})
    else:
        ha.service("light", "turn_off", {"entity_id": ent["ecran"]})


def main() -> int:
    p = argparse.ArgumentParser(description="Tab5 power draw on battery, scenario by scenario")
    p.add_argument("--ha", required=True, help="Home Assistant URL, e.g. http://homeassistant.local:8123")
    p.add_argument("--duree", type=int, default=300, help="seconds per scenario (default 300)")
    p.add_argument("--sortie", default="", help="CSV file (default mesure_conso_<date>.csv)")
    p.add_argument("--insecure", action="store_true", help="accept a self-signed certificate")
    p.add_argument("--essai", action="store_true",
                   help="20 s per scenario, battery not required: checks the control only")
    a = p.parse_args()

    jeton = os.environ.get("TAB5_HA_TOKEN") or getpass.getpass("Home Assistant long-lived token: ")
    ha = HA(a.ha, jeton, a.insecure)
    duree = 20 if a.essai else a.duree
    ent = trouver_entites(ha)
    absentes = [r for r in ("courant", "tension", "niveau") if r not in ent]
    if absentes:
        print(f"Note: {', '.join(absentes)} not found (disabled by default in HA): columns left empty.")

    if lire(ha, ent, "sur_batterie") != "on" and not a.essai:
        sys.exit("« Tab5 Sur batterie » is not on: unplug the USB cable (the battery current is the "
                 "only thing measured), wait one minute, then start again.")

    version = ha.modele("{{ device_attr(device_id('%s'), 'sw_version') }}" % ent["conso"])
    sortie = a.sortie or f"mesure_conso_{dt.datetime.now():%Y%m%d_%H%M}.csv"
    total_min = duree * len(SCENARIOS) / 60
    print(f"Firmware {version or '?'} · {len(SCENARIOS)} scenarios × {duree} s ≈ {total_min:.0f} min → {sortie}")
    print("Do not touch the tablet during the test. Ctrl+C stops it and restores the settings.")

    sauvegarde = sauvegarder(ha, ent)
    resultats: dict[str, list[float]] = {}
    with open(sortie, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["# firmware", version])
        w.writerow(["heure", "scenario", "ecran_pct", "micro", "hp", "s_depuis_debut",
                    "consommation_w", "courant_a", "tension_v", "niveau_pct", "core_temp_c",
                    "sur_batterie", "retenue"])
        try:
            ha.service("select", "select_option", {"entity_id": ent["economie"], "option": "Jamais"})
            ha.service("select", "select_option", {"entity_id": ent["extinction"], "option": "Jamais"})
            for nom, ecran_pct, micro, hp in SCENARIOS:
                appliquer(ha, ent, ecran_pct, micro, hp)
                debut = time.monotonic()
                vu = ha.etat(ent["conso"])
                derniere = vu.get("last_reported") or vu.get("last_updated")
                valeurs = resultats.setdefault(nom, [])
                print(f"- {nom}: screen {ecran_pct or 'off'}{'%' if ecran_pct else ''}, "
                      f"mic {'on' if micro else 'off'}, speaker {'on' if hp else 'off'}", flush=True)
                while time.monotonic() - debut < duree:
                    time.sleep(SONDE_S)
                    try:
                        vu = ha.etat(ent["conso"])
                    except (urllib.error.URLError, OSError):
                        continue
                    marque = vu.get("last_reported") or vu.get("last_updated")
                    if marque == derniere:
                        continue
                    derniere = marque
                    ecoule = time.monotonic() - debut
                    w_val = nombre(vu.get("state"))
                    retenue = False
                    if w_val is not None and ecoule >= ATTENTE_STABLE_S:
                        valeurs.append(w_val)
                        retenue = True
                    w.writerow([dt.datetime.now().strftime("%H:%M:%S"), nom, ecran_pct, int(micro), int(hp),
                                round(ecoule), vu.get("state"), lire(ha, ent, "courant"),
                                lire(ha, ent, "tension"), lire(ha, ent, "niveau"), lire(ha, ent, "temp"),
                                lire(ha, ent, "sur_batterie"), int(retenue)])
                    f.flush()
                    print(f"    {round(ecoule):4d} s  {vu.get('state')} W{'' if retenue else '  (ignored)'}",
                          flush=True)
        except KeyboardInterrupt:
            print("Stopped.")
        finally:
            restaurer(ha, ent, sauvegarde)
            print("Settings restored.")

    ref = statistics.median(resultats["reference_ecran100"]) if resultats.get("reference_ecran100") else None
    print("\nScenario               n   median W   vs reference")
    for nom, *_ in SCENARIOS:
        v = resultats.get(nom) or []
        if not v:
            print(f"{nom:<22} 0          —")
            continue
        m = statistics.median(v)
        ecart = f"{m - ref:+.2f} W" if ref is not None else ""
        print(f"{nom:<22} {len(v):<3} {m:8.2f}   {ecart}")
    print(f"\nCSV: {os.path.abspath(sortie)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
