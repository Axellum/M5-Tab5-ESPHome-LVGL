"""Branche essai/mesure-popup-musique (JAMAIS MERGÉE) : ouvre et ferme des popups sur la tablette virtuelle.

    python tools/mesure/ouvrir_popups.py --passes 5 --attente 1.0 [--captures DOSSIER]

Se connecte comme Home Assistant (clé donnée comme à une tablette neuve), pousse la
première scène de la démo et le lecteur en pause du rendu (tools/rendu/ecrans.py), puis,
pour chaque thème (Zen Sumi, celui de la campagne de nuit sur la 7B, et Relief doux), ouvre
--passes fois chaque popup (« Aller à l'écran ») et revient à l'accueil. Le programme écrit
une ligne « [mesure] » par grande image (Tab5/rendu/mesure_images.h) ; ce script écrit une
ligne « [pilote] » avant chaque ouverture, dans le même journal (ajout en fin de fichier).
--captures : une capture BMP de chaque popup ouvert, à la première passe (rendu_capture),
pour comparer les variantes au pixel près.
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RACINE / "tools" / "rendu"))

from capturer import ATTENTE_PANNEAU, ATTENTE_RENDU, PANNEAUX, SELECT_MODE_THEME, SELECT_THEME, Rendu  # noqa: E402
from demo_pusher import FICHIER_CLE_DEMO, _donner_une_cle, _pousser_scene  # noqa: E402
from ecrans import LECTEUR_EN_PAUSE  # noqa: E402
from scenarios import SCENES, build_zones_absentes  # noqa: E402

POPUPS = ("Musique", "Climatisation", "Énergie")
THEMES = ("Zen Sumi", "Relief doux")


def slug(texte: str) -> str:
    return texte.replace(" ", "_").replace("É", "E").replace("é", "e")


async def piloter(hote: str, passes: int, attente: float, captures: Path | None) -> None:
    from aioesphomeapi import APIClient

    FICHIER_CLE_DEMO.unlink(missing_ok=True)
    cle = await _donner_une_cle(hote)
    client = APIClient(hote, 6053, "", noise_psk=cle, client_info="Tab5 mesure")
    await client.connect(login=True)
    try:
        entites, services = await client.list_entities_services()
        rendu = Rendu(client, entites, services, captures or Path("."), "")
        await rendu.appeler("tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
        await _pousser_scene(client, rendu.services, SCENES[0], frozenset())
        await asyncio.sleep(ATTENTE_RENDU * max(1.0, attente))
        await rendu.appeler("rendu_panneau", panneau=PANNEAUX.get(1, 0))
        await asyncio.sleep(ATTENTE_PANNEAU * max(1.0, attente))
        await rendu.jouer(LECTEUR_EN_PAUSE)
        await asyncio.sleep(attente)
        for theme in THEMES:
            rendu.choisir(SELECT_THEME, theme)
            rendu.choisir(SELECT_MODE_THEME, "Sombre")
            await asyncio.sleep(3 * attente)
            for popup in POPUPS:
                for p in range(passes):
                    print(f"[pilote] {theme} | {popup} | {p}", flush=True)
                    rendu.choisir("Aller à l'écran", popup)
                    await asyncio.sleep(attente)
                    if captures is not None and p == 0:
                        await rendu.capturer(f"{slug(theme)}-{slug(popup)}")
                    rendu.choisir("Aller à l'écran", "Accueil")
                    await asyncio.sleep(attente)
        print("[pilote] fin", flush=True)
    finally:
        await client.disconnect()


def main() -> int:
    logging.basicConfig(level=logging.WARNING)
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--passes", type=int, default=5)
    p.add_argument("--attente", type=float, default=1.0, help="secondes après chaque geste")
    p.add_argument("--captures", type=Path, help="dossier des BMP (le ESPHOME_SNAPSHOT_DIR du programme)")
    a = p.parse_args()
    asyncio.run(piloter(a.host, a.passes, a.attente, a.captures))
    return 0


if __name__ == "__main__":
    sys.exit(main())
