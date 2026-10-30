# -*- coding: utf-8 -*-
"""tools/prouver_flash.py — Prouver un flash : ce qui tournait avant, ce qui tourne après.

« OTA successful » ne prouve rien (AGENTS.md, Build & verify) : un `upload` sans `--file`
a déjà renvoyé un vieux binaire, une autre session a déjà reflashé la tablette 25 s après.
Ce script lit l'appareil par son API (aioesphomeapi, la clé gardée par Home Assistant,
trouvée par tools/tab5_cle_api.py et jamais affichée), en deux temps :

1. `--avant FICHIER`, avant le flash : un instantané JSON (MAC, nom, version du projet,
   heure de compilation, version d'ESPHome, heure du dernier démarrage, raison du
   redémarrage, état de chaque entité, réglages compris).
2. `--apres FICHIER` (le même fichier), après le flash : un nouvel instantané, puis la
   connexion reste ouverte `--attente` secondes (120 par défaut) pour voir un second
   redémarrage, et un dernier instantané. Contrôles :
   - même appareil (MAC de l'instantané, ou `--mac`) ;
   - version attendue (`--version`), heure de compilation attendue (`--compilation`, ou
     `--build-info` : le `build_time_str` du `build_info.json` du dossier de build) ;
   - le firmware a changé (version ou heure de compilation ; `--meme-firmware` si c'est
     voulu) ;
   - l'appareil a redémarré une fois depuis l'instantané, il y a moins de `--uptime-max`
     secondes, sans plantage dans la raison (`--raison` : motif attendu, ex. esphome.ota) ;
   - pas de second redémarrage ni de coupure pendant l'attente ;
   - aucune entité qui avait un état avant n'en a plus (indisponible), aucune disparue ;
   - aucun réglage (entités de catégorie « config ») changé (`--reglages-libres` après une
     installation avec effacement, qui les remet à zéro).
   `--ignorer MOTIF` (répétable, expression régulière sur « domaine.object_id » ou le nom)
   retire une entité des deux derniers contrôles.

Ce que le script ne voit pas : l'écran (rallumé ? à l'endroit ?), ce que Home Assistant
voit de son côté (entités du registre, push `tab5_connected`), le binaire envoyé (son
SHA-256 se relève à part). Ces preuves restent à faire.

Usage (PowerShell ; la clé : --config-ha, ou la variable TAB5_CONFIG_HA) :
    python tools/prouver_flash.py --host 192.168.1.42 --config-ha \\\\192.168.1.10\\config --avant avant.json
    (flash)
    python tools/prouver_flash.py --host 192.168.1.42 --config-ha \\\\192.168.1.10\\config --apres avant.json --version 3.9.0-essai --build-info H:\\.esphome_data\\build\\tab5-ha-hmi\\build_info.json --raison esphome.ota

Code de sortie : 0 tous les contrôles passent, 1 un contrôle échoue, 2 appareil injoignable
ou instantané illisible.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import math
import re
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tab5_cle_api import ajouter_options, trouver_cle  # noqa: E402

# Précision de « Tab5 Uptime » : un horodatage Unix passé par un float 32 bits, arrondi
# au multiple de 128 s (Tab5/paquets/tab5-sensors-diagnostics.yaml, ±64 s).
PRECISION_DEMARRAGE_S = 128

# « Tab5 Uptime » (heure du dernier démarrage) et « Tab5 Raison du redémarrage » : trouvés
# par leur nom, pas par un identifiant (le nom de l'appareil change d'une carte à l'autre).
MOTIF_DEMARRAGE = re.compile(r"(?i)\buptime$")
MOTIF_RAISON = re.compile(r"(?i)raison du red[ée]marrage|restart reason|reset reason")
# Une raison qui dit un plantage. Les deux chiens de garde que journal_raison_ha()
# (Tab5/ecran/tab5_journal.cpp) donne pour normaux sont exclus d'abord.
MOTIF_PLANTAGE = re.compile(r"(?i)crash|exception|panic|brownout|watchdog")
RAISONS_NORMALES = ("First boot after install", "Power button or RTC watchdog")

FORMATS_COMPILATION = ("%Y-%m-%d %H:%M:%S %z", "%Y-%m-%d %H:%M:%S", "%b %d %Y, %H:%M:%S")


# --------------------------------------------------------------------------------------
# Instantanés (purs : testés sans appareil par tests/test_prouver_flash.py)
# --------------------------------------------------------------------------------------

def domaine_de(info) -> str:
    """« SensorInfo » → « sensor », « TextSensorInfo » → « text_sensor »."""
    nom = type(info).__name__
    if nom.endswith("Info"):
        nom = nom[:-4]
    return re.sub(r"(?<!^)(?=[A-Z])", "_", nom).lower()


def valeur_json(etat):
    """L'état d'une entité en valeur JSON ; None si l'entité n'a pas d'état."""
    if etat is None or getattr(etat, "missing_state", False):
        return None
    v = getattr(etat, "state", None)
    if isinstance(v, float) and not math.isfinite(v):
        return None
    if isinstance(v, (bool, int, float, str)) or v is None:
        return v
    return str(v)  # énumérations (volet, clim…) : leur nom suffit à comparer


def instantane(info_appareil, entites, etats: dict, horodatage: float, hote: str) -> dict:
    """Instantané JSON : `entites` = EntityInfo, `etats` = {clé: EntityState}."""
    vues = {}
    for e in entites:
        cle = f"{domaine_de(e)}.{e.object_id or e.key}"
        cat = getattr(e, "entity_category", 0)
        vues[cle] = {
            "nom": e.name,
            "categorie": {1: "config", 2: "diagnostic"}.get(int(cat or 0), ""),
            "etat": valeur_json(etats.get(e.key)),
        }
    return {
        "horodatage": horodatage,
        "heure": datetime.fromtimestamp(horodatage, timezone.utc).isoformat(timespec="seconds"),
        "hote": hote,
        "appareil": {
            "mac": (info_appareil.mac_address or "").upper(),
            "nom": info_appareil.name,
            "nom_affiche": info_appareil.friendly_name,
            "version": info_appareil.project_version,
            "compilation": info_appareil.compilation_time,
            "esphome": info_appareil.esphome_version,
        },
        "entites": vues,
    }


def _entite(inst: dict, motif: re.Pattern, domaine: str | None = None):
    for cle, e in inst.get("entites", {}).items():
        if domaine and not cle.startswith(domaine + "."):
            continue
        if motif.search(e.get("nom") or ""):
            return e
    return None


def heure_demarrage(inst: dict) -> float | None:
    """Heure Unix du dernier démarrage (« Tab5 Uptime », horodatage), ou None."""
    e = _entite(inst, MOTIF_DEMARRAGE, "sensor")
    v = e and e.get("etat")
    return float(v) if isinstance(v, (int, float)) and v > 1e9 else None


def raison(inst: dict) -> str | None:
    e = _entite(inst, MOTIF_RAISON, "text_sensor")
    return e.get("etat") if e else None


def est_plantage(texte: str | None) -> bool:
    if not texte or texte.startswith(RAISONS_NORMALES):
        return False
    return bool(MOTIF_PLANTAGE.search(texte))


def compilation_lue(texte: str | None) -> datetime | None:
    """« 2026-10-30 08:40:56 +0100 » (build_info.json) ou « Oct 30 2026, 08:40:56 »."""
    if not texte:
        return None
    for fmt in FORMATS_COMPILATION:
        try:
            return datetime.strptime(texte.strip(), fmt).replace(tzinfo=None)
        except ValueError:
            continue
    return None


def compilations_egales(a: str | None, b: str | None) -> bool:
    """Même heure de compilation, quel que soit le format (heure locale du PC qui compile)."""
    if not a or not b:
        return False
    if a.strip() == b.strip():
        return True
    da, db = compilation_lue(a), compilation_lue(b)
    return da is not None and da == db


def _egales(a, b) -> bool:
    if isinstance(a, float) or isinstance(b, float):
        try:
            return math.isclose(float(a), float(b), rel_tol=1e-6, abs_tol=1e-6)
        except (TypeError, ValueError):
            return False
    return a == b


# --------------------------------------------------------------------------------------
# Contrôles (purs)
# --------------------------------------------------------------------------------------

@dataclass
class Attentes:
    version: str | None = None
    mac: str | None = None
    compilation: str | None = None
    raison: str | None = None          # expression régulière
    uptime_max: float = 600.0
    meme_firmware: bool = False
    reglages_libres: bool = False
    ignorer: list[str] = field(default_factory=list)


@dataclass
class Controle:
    nom: str
    ok: bool | None                    # None : pas contrôlé (dit pourquoi)
    detail: str


def _ignoree(cle: str, nom: str, motifs: list[str]) -> bool:
    return any(re.search(m, cle) or re.search(m, nom or "") for m in motifs)


def comparer(avant: dict, apres: dict, fin: dict | None, attentes: Attentes,
             coupures: int = 0, maintenant: float | None = None) -> list[Controle]:
    """Les contrôles de l'en-tête. `apres` : juste après le flash ; `fin` : après l'attente
    (None si l'attente était nulle) ; `coupures` : déconnexions vues pendant l'attente."""
    res: list[Controle] = []
    a_app, p_app = avant["appareil"], apres["appareil"]
    maintenant = apres["horodatage"] if maintenant is None else maintenant

    mac = (attentes.mac or a_app["mac"]).upper()
    res.append(Controle("appareil", p_app["mac"] == mac,
                        f"MAC {p_app['mac']} (attendue {mac}), nom {p_app['nom']}"))

    if attentes.version:
        res.append(Controle("version", p_app["version"] == attentes.version,
                            f"{p_app['version']} (attendue {attentes.version}, avant {a_app['version']})"))
    else:
        res.append(Controle("version", None,
                            f"{p_app['version']} (avant {a_app['version']}) ; --version non donnée"))

    if attentes.compilation:
        res.append(Controle("compilation", compilations_egales(p_app["compilation"], attentes.compilation),
                            f"{p_app['compilation']} (attendue {attentes.compilation})"))
    else:
        res.append(Controle("compilation", None,
                            f"{p_app['compilation']} (avant {a_app['compilation']}) ; "
                            "--compilation / --build-info non donnés"))

    meme = (p_app["version"] == a_app["version"]
            and compilations_egales(p_app["compilation"], a_app["compilation"]))
    if attentes.meme_firmware:
        res.append(Controle("firmware changé", None, "même firmware accepté (--meme-firmware)"))
    else:
        res.append(Controle("firmware changé", not meme,
                            "même version et même heure de compilation qu'avant : flash refusé, pas fait, "
                            "ou ancien binaire envoyé (upload sans --file ?)" if meme else
                            f"{a_app['version']} → {p_app['version']}"))

    dem = heure_demarrage(apres)
    if dem is None:
        res.append(Controle("redémarrage", False,
                            "heure de démarrage illisible (« Uptime » sans valeur : heure pas encore "
                            "synchronisée ? relancer --apres dans une minute)"))
    else:
        depuis = maintenant - dem
        apres_avant = dem > avant["horodatage"] - PRECISION_DEMARRAGE_S
        ok = apres_avant and depuis <= attentes.uptime_max
        heure = datetime.fromtimestamp(dem, timezone.utc).isoformat(timespec="seconds")
        if not apres_avant:
            pourquoi = "démarré AVANT l'instantané : pas de redémarrage depuis"
        elif not ok:
            pourquoi = f"démarré il y a {depuis:.0f} s, plus que --uptime-max {attentes.uptime_max:.0f}"
        else:
            pourquoi = f"il y a {depuis:.0f} s (±64 s)"
        res.append(Controle("redémarrage", ok, f"démarré à {heure} : {pourquoi}"))

    r = raison(apres)
    if r is None:
        res.append(Controle("raison", None, "raison du redémarrage absente"))
    elif est_plantage(r):
        res.append(Controle("raison", False, f"« {r} » : plantage"))
    elif attentes.raison:
        res.append(Controle("raison", bool(re.search(attentes.raison, r)),
                            f"« {r} » (motif attendu {attentes.raison})"))
    else:
        res.append(Controle("raison", True, f"« {r} »"))

    if fin is None:
        res.append(Controle("second redémarrage", None, "--attente 0 : pas contrôlé"))
    else:
        d_fin = heure_demarrage(fin)
        meme_boot = dem is not None and d_fin is not None and abs(d_fin - dem) < 1
        ok = coupures == 0 and meme_boot and fin["appareil"]["version"] == p_app["version"]
        detail = (f"{coupures} coupure(s) pendant {fin['horodatage'] - apres['horodatage']:.0f} s, "
                  f"démarrage {'inchangé' if meme_boot else 'CHANGÉ'}, version {fin['appareil']['version']}")
        res.append(Controle("second redémarrage", ok, detail))

    ref = fin or apres
    perdues, disparues, changes = [], [], []
    for cle, e in avant["entites"].items():
        if _ignoree(cle, e.get("nom"), attentes.ignorer):
            continue
        p = ref["entites"].get(cle)
        if p is None:
            disparues.append(cle)
            continue
        if e.get("etat") is not None and p.get("etat") is None:
            perdues.append(cle)
        if (e.get("categorie") == "config" and e.get("etat") is not None
                and p.get("etat") is not None and not _egales(e["etat"], p["etat"])):
            changes.append(f"{cle} : {e['etat']!r} → {p['etat']!r}")
    res.append(Controle("entités", not perdues and not disparues,
                        f"{len(ref['entites'])} entités ; sans état : {perdues or 'aucune'} ; "
                        f"disparues : {disparues or 'aucune'}"))
    if attentes.reglages_libres:
        res.append(Controle("réglages", None, f"non contrôlés (--reglages-libres) ; changés : {changes or 'aucun'}"))
    else:
        res.append(Controle("réglages", not changes, f"changés : {changes or 'aucun'}"))
    return res


def echecs(controles: list[Controle]) -> list[Controle]:
    return [c for c in controles if c.ok is False]


def afficher(controles: list[Controle], sortie=sys.stdout) -> None:
    for c in controles:
        marque = {True: "OK   ", False: "ÉCHEC", None: "--   "}[c.ok]
        print(f"{marque} {c.nom} : {c.detail}", file=sortie)


# --------------------------------------------------------------------------------------
# Lecture de l'appareil (aioesphomeapi)
# --------------------------------------------------------------------------------------

async def _connecter(hote: str, cle: str | None, delai: float, on_stop=None):
    from aioesphomeapi import ZERO_NOISE_PSK, APIClient
    fin = time.monotonic() + delai
    derniere = None
    while True:
        cli = APIClient(hote, 6053, "", noise_psk=cle or ZERO_NOISE_PSK,
                        client_info="Tab5 prouver_flash (PC)")
        try:
            await cli.connect(on_stop=on_stop, login=True)
            return cli
        except Exception as err:  # noqa: BLE001 — appareil qui redémarre : on réessaie
            derniere = err
            if time.monotonic() >= fin:
                raise ConnectionError(f"{hote} injoignable : {derniere}") from err
            await asyncio.sleep(5)


async def _lire(cli, hote: str, calme: float = 1.5, plafond: float = 10.0):
    """(info, entités, états vivants) : les états restent mis à jour tant que `cli` vit."""
    info = await cli.device_info()
    entites, _ = await cli.list_entities_services()
    etats: dict = {}
    vu = [time.monotonic()]

    def on_state(s) -> None:
        etats[s.key] = s
        vu[0] = time.monotonic()

    cli.subscribe_states(on_state)
    debut = time.monotonic()
    # Les états arrivent en rafale à l'abonnement : attendre `calme` s sans nouvel état.
    while time.monotonic() - debut < plafond and time.monotonic() - vu[0] < calme:
        await asyncio.sleep(0.25)
    return info, entites, etats


async def prendre(hote: str, cle: str | None, delai: float) -> dict:
    cli = await _connecter(hote, cle, delai)
    try:
        info, entites, etats = await _lire(cli, hote)
        return instantane(info, entites, etats, time.time(), hote)
    finally:
        await cli.disconnect()


async def prendre_et_suivre(hote: str, cle: str | None, delai: float, attente: float):
    """(instantané après, instantané de fin ou None, coupures pendant l'attente)."""
    coupures = [0]

    async def on_stop(attendue: bool) -> None:
        if not attendue:
            coupures[0] += 1

    cli = await _connecter(hote, cle, delai, on_stop=on_stop)
    try:
        info, entites, etats = await _lire(cli, hote)
        apres = instantane(info, entites, etats, time.time(), hote)
        if attente <= 0:
            return apres, None, 0
        print(f"Instantané pris ; attente de {attente:.0f} s (second redémarrage ?)…", flush=True)
        await asyncio.sleep(attente)
        if coupures[0] == 0:
            return apres, instantane(info, entites, etats, time.time(), hote), 0
    finally:
        await cli.disconnect()
    fin = await prendre(hote, cle, delai)  # reconnecté après une coupure
    return apres, fin, coupures[0]


# --------------------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="IP de l'appareil (la lire dans HA : DHCP)")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--avant", type=Path, metavar="FICHIER", help="écrit l'instantané d'avant le flash")
    mode.add_argument("--apres", type=Path, metavar="FICHIER", help="compare à l'instantané d'avant")
    parser.add_argument("--version", help="version du projet attendue après le flash")
    parser.add_argument("--mac", help="MAC attendue (défaut : celle de l'instantané)")
    parser.add_argument("--compilation", help="heure de compilation attendue (build_time_str)")
    parser.add_argument("--build-info", type=Path, help="build_info.json du build envoyé (son build_time_str)")
    parser.add_argument("--raison", help="motif attendu de la raison du redémarrage (ex. esphome.ota)")
    parser.add_argument("--attente", type=float, default=120, help="secondes à surveiller (défaut 120)")
    parser.add_argument("--uptime-max", type=float, default=600,
                        help="démarré il y a au plus N s (défaut 600)")
    parser.add_argument("--delai-connexion", type=float, default=180,
                        help="secondes pour joindre l'appareil (défaut 180 : il redémarre)")
    parser.add_argument("--ignorer", action="append", default=[], metavar="MOTIF",
                        help="entité retirée des contrôles « entités » et « réglages » (répétable)")
    parser.add_argument("--meme-firmware", action="store_true", help="le même firmware qu'avant est voulu")
    parser.add_argument("--reglages-libres", action="store_true",
                        help="ne pas exiger les mêmes réglages (installation avec effacement)")
    parser.add_argument("--rapport", type=Path, help="écrit les instantanés et les contrôles en JSON")
    ajouter_options(parser)
    args = parser.parse_args(argv)

    try:
        cle = trouver_cle(args.cle, args.host, args.config_ha)
    except OSError as err:
        parser.error(f"clé de HA illisible : {err}")
    if not cle:
        print("Aucune clé trouvée : clé nulle d'appairage (tablette jamais ajoutée à HA).", file=sys.stderr)

    if args.avant:
        try:
            inst = asyncio.run(prendre(args.host, cle, args.delai_connexion))
        except (ConnectionError, OSError) as err:
            print(f"Appareil injoignable : {err}", file=sys.stderr)
            return 2
        args.avant.write_text(json.dumps(inst, ensure_ascii=False, indent=1), encoding="utf-8")
        a = inst["appareil"]
        print(f"Avant : {a['mac']} {a['nom']} {a['version']} compilé {a['compilation']}, "
              f"raison « {raison(inst)} », {len(inst['entites'])} entités → {args.avant}")
        return 0

    try:
        avant = json.loads(args.apres.read_text(encoding="utf-8"))
    except (OSError, ValueError) as err:
        print(f"Instantané d'avant illisible : {err}", file=sys.stderr)
        return 2
    compilation = args.compilation
    if args.build_info:
        compilation = json.loads(args.build_info.read_text(encoding="utf-8"))["build_time_str"]
    attentes = Attentes(version=args.version, mac=args.mac, compilation=compilation, raison=args.raison,
                        uptime_max=args.uptime_max, meme_firmware=args.meme_firmware,
                        reglages_libres=args.reglages_libres, ignorer=args.ignorer)
    try:
        apres, fin, coupures = asyncio.run(
            prendre_et_suivre(args.host, cle, args.delai_connexion, args.attente))
    except (ConnectionError, OSError) as err:
        print(f"Appareil injoignable : {err}", file=sys.stderr)
        return 2
    controles = comparer(avant, apres, fin, attentes, coupures)
    afficher(controles)
    if args.rapport:
        args.rapport.write_text(json.dumps(
            {"avant": avant, "apres": apres, "fin": fin, "coupures": coupures,
             "controles": [c.__dict__ for c in controles]}, ensure_ascii=False, indent=1), encoding="utf-8")
    rates = echecs(controles)
    print(f"{len(rates)} contrôle(s) en échec." if rates else "Tous les contrôles passent.")
    print("Reste à prouver à part : l'écran (à l'œil), les entités vues par HA, le SHA-256 du binaire.")
    return 1 if rates else 0


if __name__ == "__main__":
    sys.exit(main())
