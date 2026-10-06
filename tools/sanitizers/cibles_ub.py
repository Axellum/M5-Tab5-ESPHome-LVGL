"""Cas ciblés de comportement indéfini, fenêtres OUVERTES, sous ASan/UBSan.

    python tools/sanitizers/cibles_ub.py --programme <program> --resultats <dossier>

Le fuzzing (fuzz_services.py) n'ouvre aucune fenêtre : les conversions float → entier
qui ne s'exécutent que popup ouverte lui échappent (la luminosité d'une lampe, audit du
30/09/2026). Ici, pour chaque cas : une scène valide, la charge utile piégée, puis la
fenêtre qui affiche la valeur (mêmes gestes que tools/rendu/ecrans.py), la charge utile
repoussée fenêtre ouverte (chemin « depuis HA »), et les rapports des sanitizers apparus
pendant ce cas, lus dans le journal de la tablette (tools/sanitizers/rapports.py).

  R2  consigne de clim « inf » / « 1e30 » / « -inf »   (tab5_cards.cpp, corrigé au lot A)
  R2c bornes de clim « -1e30 » (climr)                  (tab5_cards.cpp puis lv_map de LVGL)
  R1  luminosité « inf » dans les emplacements          (tab5_tuiles.cpp)
  R2b humidité « inf » / « 1e30 »                        (tab5_forecast.cpp)
  R3  historique « 1e30 » : pas, minutes, températures   (tab5_historique.cpp)

Sorties : cibles.json, cibles.md, tablette.log. Code de sortie 1 si un rapport, si la
tablette s'arrête ou si une fenêtre attendue manque dans ecrans.py.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import subprocess
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RACINE / "tools" / "demo"))
sys.path.insert(0, str(RACINE / "tools" / "rendu"))
sys.path.insert(0, str(RACINE / "tools" / "sanitizers"))

from capturer import Rendu  # noqa: E402
from demo_pusher import _donner_une_cle, _lire_cle_demo, _pousser_scene  # noqa: E402
import demo_pusher  # noqa: E402
from ecrans import ECRANS  # noqa: E402
from rapports import Journal, distincts  # noqa: E402
from scenarios import SCENES, build_zones_absentes  # noqa: E402

logger = logging.getLogger("cibles")

# Luminosité piégée sur toutes les clés de lampe possibles (anciennes et tuiles tRT).
LAMPES_INF = "".join(f"lumiere_{i}|on|inf;" for i in range(1, 7)) + "".join(
    f"t{r}{t}|on|inf|FFB347;" for r in range(6) for t in range(10))
CLIM = {"current": "20.8", "mode": "cool", "preset": "none", "fan": "auto", "swing": "off"}

CAS = [
    ("R2 consigne clim inf", "tab5_maj_clim", {**CLIM, "target": "inf"}, "climatisation"),
    ("R2 consigne clim 1e30", "tab5_maj_clim", {**CLIM, "target": "1e30", "current": "1e30", "mode": "heat"},
     "climatisation"),
    ("R2 consigne clim -inf", "tab5_maj_clim", {**CLIM, "target": "-inf", "current": "-inf"}, "climatisation"),
    ("R2c bornes de clim -1e30", "tab5_maj_emplacements", {"payload": "climr|-1e30|1e30|0.5|°C|hcfda|Salon;"},
     "climatisation"),
    ("R1 luminosité inf (salon)", "tab5_maj_emplacements", {"payload": LAMPES_INF}, "lumieres-salon"),
    ("R1 luminosité inf (chambre)", "tab5_maj_emplacements", {"payload": LAMPES_INF}, "lumieres-chambre"),
    ("R2b humidité inf", "tab5_maj_meteo_actuelle",
     {"condition": "sunny", "temperature": "inf", "humidite": "inf"}, None),
    ("R2b humidité 1e30", "tab5_maj_meteo_actuelle",
     {"condition": "sunny", "temperature": "1e30", "humidite": "-1e30"}, None),
    # Popup Température ouvert (vue 24 h, la réponse valide vient des gestes de l'écran) :
    # pas, maintenant et minutes de prévision hors des bornes, températures infinies.
    ("R3 historique 1e30", "tab5_maj_historique",
     {"cle": "serre", "vue": "jour", "entete": "Serre|2026-06-15T07:00|1e30|1e30|1e30|1",
      "mesures": "1e30,-1e30,inf;-1e30,1e30,nan;" * 8,
      "previsions": "1e30,20;-1e30,1e30,-1e30,1e30;99999999999999999999,20;2147483648,1e30"},
     "temperature-serre"),
    ("R3 historique date et pas", "tab5_maj_historique",
     {"cle": "serre", "vue": "jour", "entete": "Serre|2147483647-12-31T23:59|0.5|-1e30|-1e30|0",
      "mesures": "20,19,21;" * 64 + "20,19,21",
      "previsions": ";".join(f"{1000000 * k},20,-1e30,1e30" for k in range(60))},
     "temperature-serre"),
]


async def principal(args) -> dict:
    from aioesphomeapi import APIClient

    res = Path(args.resultats)
    captures = Path(os.environ.get("ESPHOME_SNAPSHOT_DIR", res / "captures"))
    captures.mkdir(parents=True, exist_ok=True)
    chemin = res / "tablette.log"
    sortie = open(chemin, "w", encoding="utf-8", errors="replace")  # noqa: SIM115 — vit avec le processus
    proc = subprocess.Popen([args.programme], stdout=sortie, stderr=subprocess.STDOUT)
    journal = Journal(chemin)
    await asyncio.sleep(8)
    demo_pusher.DELAI_ENTRE_BLOCS = 0.3
    cle = _lire_cle_demo() or await _donner_une_cle(args.hote)
    client = APIClient(args.hote, 6053, "", noise_psk=cle, client_info="Tab5 cas cibles")
    await client.connect(login=True)
    entites, services = await client.list_entities_services()
    rendu = Rendu(client, entites, services, captures, "")
    par_ecran = {e.nom: e for e in ECRANS}
    await rendu.appeler("tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
    journal.nouveaux()  # démarrage : ce qui précède le premier cas ne compte pas pour lui
    sortie_cas = {"cas": []}
    for nom, service, donnees, ecran in CAS:
        await _pousser_scene(client, rendu.services, SCENES[0], frozenset())
        await asyncio.sleep(1.5)
        await rendu.appeler(service, **donnees)
        await asyncio.sleep(1.5)
        ouvert = None
        if ecran:
            e = par_ecran.get(ecran)
            if e is None:
                ouvert = f"écran {ecran} absent de ecrans.py"
            else:
                for etape in e.etapes:
                    await rendu.jouer(etape)
                await asyncio.sleep(e.attente + 1.0)
                await rendu.capturer(f"cible-{len(sortie_cas['cas']) + 1}")
                await rendu.appeler(service, **donnees)  # repoussé fenêtre ouverte
                await asyncio.sleep(1.5)
                for etape in e.fermer:
                    await rendu.jouer(etape)
                await rendu.accueil()
                ouvert = "oui"
        rapports = journal.nouveaux()
        vivante = proc.poll() is None
        sortie_cas["cas"].append({"nom": nom, "service": service, "ecran": ecran, "ouvert": ouvert,
                                  "rapports": rapports, "vivante": vivante})
        logger.info("%s : %d rapport(s), vivante=%s", nom, len(rapports), vivante)
        if not vivante:
            break
    sortie_cas["alertes_rendu"] = rendu.alertes
    try:
        await client.disconnect()
    except Exception as exc:  # la tablette a pu s'arrêter pendant un cas
        logger.info("déconnexion : %r", exc)
    if proc.poll() is None:
        proc.terminate()
        proc.wait(10)
    sortie_cas["code_sortie"] = proc.returncode
    return sortie_cas


def resume(s: dict) -> str:
    lignes = ["## Cas ciblés de comportement indéfini (fenêtres ouvertes, ASan/UBSan)", "",
              "| Cas | Fenêtre | Rapports | Tablette vivante |", "|---|---|---|---|"]
    for c in s["cas"]:
        lignes.append(f"| {c['nom']} | {c['ecran'] or '—'} ({c['ouvert'] or 'sans fenêtre'}) | "
                      f"{len(c['rapports'])} | {'oui' if c['vivante'] else 'NON'} |")
    for texte in distincts([r for c in s["cas"] for r in c["rapports"]])[:20]:
        lignes += ["", "```", texte, "```"]
    if s.get("alertes_rendu"):
        lignes += ["", "Alertes du rendu : " + " ; ".join(s["alertes_rendu"][:10])]
    return "\n".join(lignes) + "\n"


def en_echec(s: dict) -> bool:
    return len(s["cas"]) < len(CAS) or any(
        c["rapports"] or not c["vivante"] or (c["ecran"] and c["ouvert"] != "oui") for c in s["cas"])


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--programme", required=True)
    p.add_argument("--resultats", required=True)
    p.add_argument("--hote", default="127.0.0.1")
    args = p.parse_args()
    Path(args.resultats).mkdir(parents=True, exist_ok=True)
    s = asyncio.run(principal(args))
    (Path(args.resultats) / "cibles.json").write_text(json.dumps(s, ensure_ascii=False, indent=1), encoding="utf-8")
    texte = resume(s)
    (Path(args.resultats) / "cibles.md").write_text(texte, encoding="utf-8")
    print(texte)
    return 1 if en_echec(s) else 0


if __name__ == "__main__":
    sys.exit(main())
