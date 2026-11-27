#!/usr/bin/env python3
"""Exporte la maison de la démo en JSON pour la page web (tools/web/index.html).

[AI-CONTEXT]
Rôle : la page fait office de Home Assistant pour la tablette compilée en WebAssembly
(Tab5/web/web_main.cpp). Elle ne parle pas Python : ce script joue la démo
(tools/demo/demo_pusher.py) contre une tablette « à blanc » qui note chaque appel, et
écrit ces appels dans demo.json, avec le modèle de la maison (pièces, tuiles, appareils
de la tuile − / +) dont la page tient l'état.

Contenu de demo.json :
  - zones : la réponse à esphome.tab5_zones ;
  - scenes : par scène météo, ses appels dans l'ordre de demo_pusher (_pousser_scene),
    et sa pluie (niveau, dans_min) : la page recalcule l'epoch du code « @niveau,début »
    à l'envoi (code_pluie), sinon « dans 10 mn » vieillirait ;
  - energie : par vue, la réponse à esphome.tab5_energie ;
  - historique : par « clé|vue », la réponse à esphome.tab5_historique ;
  - maison : pièces (nom, tuiles tRT avec type, nom, options, état, valeur, couleur),
    appareils de la tuile − / + (rN) et climat des pièces.

@ai_instruction : les appels viennent de demo_pusher, pas d'une copie : un changement
de contrat (tests/test_contrat.py, --dry-run) se retrouve ici au prochain export.
Limite connue : les heures des prévisions et les noms des jours sont calculés à
l'import de scenarios.py (_heures_depuis_maintenant, _jours_depuis_aujourdhui), de
même que les dates de l'énergie et de l'historique : un demo.json exporté un autre jour
montre d'autres heures. Réexporter au moment de construire la page.

Usage : python tools/web/exporter_demo.py --sortie <dossier>/demo.json
"""
from __future__ import annotations

import argparse
import asyncio
import datetime as _dt
import json
import sys
from dataclasses import asdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "demo"))

import demo_pusher as dp  # noqa: E402
from scenarios import (  # noqa: E402
    ENERGIE_VUES,
    HISTORIQUE_CLES,
    HISTORIQUE_VUES,
    PIECES,
    REGLABLES,
    SCENES,
    cle_tuile,
    nom_de_la_piece,
)


def _appels(tablette: dp._TabletteABlanc, debut: int) -> list:
    return [[nom, donnees] for nom, donnees in tablette.appels[debut:]]


def exporter() -> dict:
    services = dp.services_du_contrat(dp.lire_contrat(dp.API_LOGIC))
    tablette = dp._TabletteABlanc()
    absentes = frozenset()

    asyncio.run(dp._pousser_zones(tablette, services, absentes))
    zones = _appels(tablette, 0)

    scenes = []
    for scene in SCENES:
        debut = len(tablette.appels)
        asyncio.run(dp._pousser_scene(tablette, services, scene, absentes, attendre=dp._sans_attente))
        niveau, dans_min = scene.pluie
        scenes.append({"nom": scene.nom, "pluie": [niveau, dans_min], "appels": _appels(tablette, debut)})

    energie = {}
    for vue in ENERGIE_VUES:
        debut = len(tablette.appels)
        asyncio.run(dp._pousser_energie(tablette, services, (vue,)))
        energie[vue] = _appels(tablette, debut)

    historique = {}
    maintenant = _dt.datetime.now()
    for cle in HISTORIQUE_CLES:
        for vue in HISTORIQUE_VUES:
            debut = len(tablette.appels)
            asyncio.run(dp._pousser_historique(tablette, services, cle, vue, maintenant))
            historique[f"{cle}|{vue}"] = _appels(tablette, debut)

    pieces = {}
    for r, piece in sorted(PIECES.items()):
        pieces[str(r)] = {
            "nom": nom_de_la_piece(r, piece),
            "tuiles": {cle_tuile(r, t): asdict(tuile) for t, tuile in sorted(piece.tuiles.items())},
            "climat": asdict(piece.climat) if piece.climat else None,
        }
    reglables = {f"r{n}": asdict(reglable) for n, reglable in enumerate(REGLABLES)}

    return {
        "exporte": maintenant.isoformat(timespec="seconds"),
        "zones": zones,
        "scenes": scenes,
        "energie": energie,
        "historique": historique,
        "maison": {"pieces": pieces, "reglables": reglables},
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Exporte la maison de la démo en JSON pour la page web.")
    parser.add_argument("--sortie", type=Path, required=True, help="fichier demo.json à écrire")
    args = parser.parse_args()
    donnees = exporter()
    args.sortie.parent.mkdir(parents=True, exist_ok=True)
    texte = json.dumps(donnees, ensure_ascii=False, separators=(",", ":"))
    args.sortie.write_text(texte, encoding="utf-8")
    nb = sum(len(s["appels"]) for s in donnees["scenes"])
    print(f"{args.sortie} : {len(texte.encode('utf-8')) // 1024} Kio, {len(donnees['scenes'])} scènes "
          f"({nb} appels), {len(donnees['historique'])} historiques")


if __name__ == "__main__":
    main()
