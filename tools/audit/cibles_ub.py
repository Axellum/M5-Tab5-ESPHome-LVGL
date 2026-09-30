"""Audit qualité : cas ciblés de comportement indéfini, popups OUVERTES, sous ASan/UBSan.

    python tools/audit/cibles_ub.py --programme <program> --resultats <dossier>

Le fuzzing (fuzz_services.py) n'ouvre aucune fenêtre : les conversions float → entier
repérées à la lecture du code ne s'exécutent que popup ouverte, il ne pouvait pas les
atteindre. Ici, pour chaque cas : une scène valide, la charge utile piégée, puis la
fenêtre qui affiche la valeur (mêmes gestes que tools/rendu/ecrans.py), et les rapports
des sanitizers apparus pendant ce cas.

  R2  consigne de clim « inf » / « 1e30 » → (int) / static_cast<int32_t> (tab5_cards.cpp)
  R1  luminosité « inf » dans les emplacements → static_cast<int> (tab5_tuiles.cpp)
  R2b humidité « inf » → (int) dans get_humidity_color (tab5_forecast.cpp)

Sorties : cibles.json, cibles.md, tablette.log.
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
sys.path.insert(0, str(RACINE / "tools" / "audit"))

from capturer import Rendu  # noqa: E402
from demo_pusher import _donner_une_cle, _lire_cle_demo, _pousser_scene  # noqa: E402
import demo_pusher  # noqa: E402
from ecrans import ECRANS  # noqa: E402
from fuzz_services import nouveaux_rapports, rapports_sanitizer  # noqa: E402
from scenarios import SCENES, build_zones_absentes  # noqa: E402

logger = logging.getLogger("cibles")

# Luminosité piégée sur toutes les clés de lampe possibles (anciennes et tuiles tRT).
LAMPES_INF = "".join(f"lumiere_{i}|on|inf;" for i in range(1, 7)) + "".join(
    f"t{r}{t}|on|inf|FFB347;" for r in range(6) for t in range(10))

CAS = [
    ("R2 consigne clim inf", "tab5_maj_clim",
     {"target": "inf", "current": "20.8", "mode": "cool", "preset": "none", "fan": "auto", "swing": "off"},
     "climatisation"),
    ("R2 consigne clim 1e30", "tab5_maj_clim",
     {"target": "1e30", "current": "1e30", "mode": "heat", "preset": "none", "fan": "auto", "swing": "off"},
     "climatisation"),
    ("R2 consigne clim -inf", "tab5_maj_clim",
     {"target": "-inf", "current": "-inf", "mode": "cool", "preset": "none", "fan": "auto", "swing": "off"},
     "climatisation"),
    ("R1 luminosité inf (salon)", "tab5_maj_emplacements", {"payload": LAMPES_INF}, "lumieres-salon"),
    ("R1 luminosité inf (chambre)", "tab5_maj_emplacements", {"payload": LAMPES_INF}, "lumieres-chambre"),
    ("R2b humidité inf", "tab5_maj_meteo_actuelle",
     {"condition": "sunny", "temperature": "inf", "humidite": "inf"}, None),
    ("R2b humidité 1e30", "tab5_maj_meteo_actuelle",
     {"condition": "sunny", "temperature": "1e30", "humidite": "1e30"}, None),
]


async def principal(args) -> dict:
    from aioesphomeapi import APIClient

    san = Path(os.environ["AUDIT_SAN_DIR"])
    san.mkdir(parents=True, exist_ok=True)
    res = Path(args.resultats)
    captures = Path(os.environ.get("ESPHOME_SNAPSHOT_DIR", res / "captures"))
    captures.mkdir(parents=True, exist_ok=True)
    journal = open(res / "tablette.log", "w", encoding="utf-8", errors="replace")  # noqa: SIM115
    proc = subprocess.Popen([args.programme], stdout=journal, stderr=subprocess.STDOUT)
    await asyncio.sleep(8)
    demo_pusher.DELAI_ENTRE_BLOCS = 0.3
    cle = _lire_cle_demo() or await _donner_une_cle(args.hote)
    client = APIClient(args.hote, 6053, "", noise_psk=cle, client_info="Tab5 audit cibles")
    await client.connect(login=True)
    entites, services = await client.list_entities_services()
    rendu = Rendu(client, entites, services, captures, "")
    par_ecran = {e.nom: e for e in ECRANS}
    await rendu.appeler("tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
    sortie = {"cas": []}
    for nom, service, donnees, ecran in CAS:
        avant = rapports_sanitizer(san)
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
                await rendu.capturer(f"cible-{len(sortie['cas']) + 1}")
                # La valeur piégée repoussée popup OUVERTE (chemin « depuis HA »).
                await rendu.appeler(service, **donnees)
                await asyncio.sleep(1.5)
                for etape in e.fermer:
                    await rendu.jouer(etape)
                await rendu.accueil()
                ouvert = "oui"
        rapports = nouveaux_rapports(san, avant)
        vivante = proc.poll() is None
        sortie["cas"].append({"nom": nom, "service": service, "ecran": ecran, "ouvert": ouvert,
                              "rapports": rapports, "vivante": vivante})
        logger.info("%s : %d rapport(s), vivante=%s", nom, len(rapports), vivante)
        if not vivante:
            break
    sortie["alertes_rendu"] = rendu.alertes
    try:
        await client.disconnect()
    except Exception as exc:  # la tablette a pu s'arrêter pendant un cas
        logger.info("déconnexion : %r", exc)
    if proc.poll() is None:
        proc.terminate()
        proc.wait(10)
    sortie["code_sortie"] = proc.returncode
    return sortie


def resume(s: dict, temoin: str) -> str:
    L = ["## Cas ciblés de comportement indéfini (popups ouvertes, ASan/UBSan)", "",
         f"Témoin positif (même compilateur, mêmes drapeaux, même log_path) : {temoin}", "",
         "| Cas | Fenêtre ouverte | Rapports | Tablette vivante |", "|---|---|---|---|"]
    for c in s["cas"]:
        L.append(f"| {c['nom']} | {c['ecran'] or '—'} ({c['ouvert'] or 'sans fenêtre'}) | {len(c['rapports'])} | {'oui' if c['vivante'] else 'NON'} |")
    for c in s["cas"]:
        for r in c["rapports"]:
            lignes = [x for x in r.splitlines() if "runtime error" in x or "ERROR: AddressSanitizer" in x or " #0 " in x or " #1 " in x]
            L += ["", f"**{c['nom']}**", "```", *lignes[:8], "```"]
    if s.get("alertes_rendu"):
        L += ["", "Alertes du rendu (appui tombé à côté…) : " + " ; ".join(s["alertes_rendu"][:10])]
    return "\n".join(L) + "\n"


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--programme", required=True)
    p.add_argument("--resultats", required=True)
    p.add_argument("--hote", default="127.0.0.1")
    p.add_argument("--temoin", default="non lancé", help="résultat du témoin positif, pour le résumé")
    args = p.parse_args()
    Path(args.resultats).mkdir(parents=True, exist_ok=True)
    s = asyncio.run(principal(args))
    (Path(args.resultats) / "cibles.json").write_text(json.dumps(s, ensure_ascii=False, indent=1), encoding="utf-8")
    texte = resume(s, args.temoin)
    (Path(args.resultats) / "cibles.md").write_text(texte, encoding="utf-8")
    print(texte)
    return 0


if __name__ == "__main__":
    sys.exit(main())
