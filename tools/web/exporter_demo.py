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
Horloge figée : scenarios.py calcule à son import les heures des prévisions et les noms
des jours (datetime.now()), demo_pusher les dates de l'énergie et de l'historique. Ce
script fixe donc « maintenant » à DEPART AVANT d'importer la démo (processus neuf
seulement : une démo déjà importée garderait la vraie heure), et la page règle
l'horloge de la tablette sur le même instant (web_horloge, champ « horloge ») : la
maison de la démo vit ce jour-là à partir de cette heure, comme les captures du rendu
de la CI (rendu-host.yml, DEPART). demo.json ne dépend donc ni du jour ni de la
machine qui l'exporte.

Usage : python tools/web/exporter_demo.py --sortie <dossier>/demo.json
"""
from __future__ import annotations

import argparse
import asyncio
import datetime as _dt
import importlib
import json
import sys
from dataclasses import asdict
from pathlib import Path

DOSSIER_DEMO = Path(__file__).resolve().parent.parent / "demo"
# Instant de la maison de la démo, heure de Paris : celui des captures du rendu de la CI
# (rendu-host.yml, DEPART et TZ ; tests/test_demo_web.py les compare).
DEPART = _dt.datetime.fromisoformat("2026-06-16T07:45:00+02:00")


def figer_horloge(depart: _dt.datetime) -> None:
    """datetime.now() rend désormais `depart` (heure locale sans fuseau, ou dans le
    fuseau demandé). À appeler avant d'importer scenarios.py et demo_pusher.py."""
    deja = {"scenarios", "demo_pusher"} & set(sys.modules)
    if deja:
        raise RuntimeError(f"{sorted(deja)} déjà importé : ses heures ne suivraient pas DEPART")
    vrai = _dt.datetime

    class Horloge(vrai):
        @classmethod
        def now(cls, tz=None):
            return depart.astimezone(tz) if tz is not None else depart.replace(tzinfo=None)

    _dt.datetime = Horloge


def _appels(tablette, debut: int) -> list:
    return [[nom, donnees] for nom, donnees in tablette.appels[debut:]]


def exporter(depart: _dt.datetime = DEPART) -> dict:
    figer_horloge(depart)
    sys.path.insert(0, str(DOSSIER_DEMO))
    dp = importlib.import_module("demo_pusher")
    sc = importlib.import_module("scenarios")
    services = dp.services_du_contrat(dp.lire_contrat(dp.API_LOGIC))
    tablette = dp._TabletteABlanc()
    absentes = frozenset()

    asyncio.run(dp._pousser_zones(tablette, services, absentes))
    zones = _appels(tablette, 0)

    scenes = []
    for scene in sc.SCENES:
        debut = len(tablette.appels)
        asyncio.run(dp._pousser_scene(tablette, services, scene, absentes, attendre=dp._sans_attente))
        niveau, dans_min = scene.pluie
        scenes.append({"nom": scene.nom, "pluie": [niveau, dans_min], "appels": _appels(tablette, debut)})

    energie = {}
    aujourd_hui = depart.date()
    for vue in sc.ENERGIE_VUES:
        debut = len(tablette.appels)
        asyncio.run(dp._pousser_energie(tablette, services, (vue,), aujourd_hui=aujourd_hui))
        energie[vue] = _appels(tablette, debut)

    historique = {}
    maintenant = _dt.datetime.now()
    for cle in sc.HISTORIQUE_CLES:
        for vue in sc.HISTORIQUE_VUES:
            debut = len(tablette.appels)
            asyncio.run(dp._pousser_historique(tablette, services, cle, vue, maintenant))
            historique[f"{cle}|{vue}"] = _appels(tablette, debut)

    pieces = {}
    for r, piece in sorted(sc.PIECES.items()):
        pieces[str(r)] = {
            "nom": sc.nom_de_la_piece(r, piece),
            "tuiles": {sc.cle_tuile(r, t): asdict(tuile) for t, tuile in sorted(piece.tuiles.items())},
            "climat": asdict(piece.climat) if piece.climat else None,
        }
    reglables = {f"r{n}": asdict(reglable) for n, reglable in enumerate(sc.REGLABLES)}

    return {
        # Départ de l'horloge de la tablette (la page appelle web_horloge) : avec son fuseau.
        "horloge": depart.isoformat(timespec="seconds"),
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
