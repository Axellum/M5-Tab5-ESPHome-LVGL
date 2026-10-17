"""Branche essai/mesure-popups-decoupe (JAMAIS MERGÉE) : ouvre et ferme des popups sur la tablette virtuelle.

    python tools/mesure/ouvrir_popups.py --passes 5 --attente 1.0 [--captures DOSSIER] [--carte waveshare-7b]

Se connecte comme Home Assistant (clé donnée comme à une tablette neuve), pousse la
première scène de la démo et le lecteur en pause du rendu (tools/rendu/ecrans.py), puis,
pour chaque thème (Zen Sumi, celui de la campagne de nuit sur la 7B, et Relief doux) :
--passes fois chaque popup (« Aller à l'écran », retour à l'accueil), puis --passes fois le
détail du jour du calendrier (Calendrier, appui sur la cellule du 18 de la démo, ramenée à
l'écran de la carte par tools/rendu/vers_carte.py, accueil). Chaque geste écrit une ligne
« [pilote] thème | geste | passe | écran attendu » ; le programme écrit une ligne
« [mesure] » par grande image (Tab5/rendu/mesure_images.h), dans le même journal. Le bilan
(bilan_popup.py) prend la première grande image de l'écran attendu après chaque geste.
--captures : une capture BMP de chaque popup ouvert, à la première passe (rendu_capture).
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
from ecrans import CAL_JOUR_18, LECTEUR_EN_PAUSE  # noqa: E402
from scenarios import SCENES, build_zones_absentes  # noqa: E402
from vers_carte import TAB5, Carte, taille_carte  # noqa: E402

POPUPS = ("Musique", "Climatisation", "Énergie", "Réveil", "Calendrier")
THEMES = ("Zen Sumi", "Relief doux")


def slug(texte: str) -> str:
    return texte.replace(" ", "_").replace("É", "E").replace("é", "e")


async def piloter(hote: str, passes: int, attente: float, captures: Path | None, carte: str) -> None:
    from aioesphomeapi import APIClient

    taille = TAB5 if carte == "tab5" else taille_carte(carte)
    jour = CAL_JOUR_18 if taille == TAB5 else Carte(taille).popup(*CAL_JOUR_18)

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

        async def geste(theme: str, nom: str, p: int, attendu: str, ecran: str | None = None,
                        toucher: tuple[int, int] | None = None) -> None:
            print(f"[pilote] {theme} | {nom} | {p} | {attendu}", flush=True)
            if toucher is not None:
                await rendu.appeler("rendu_toucher", x=toucher[0], y=toucher[1], duree=150)
                await asyncio.sleep(0.15)
            else:
                rendu.choisir("Aller à l'écran", ecran)
            await asyncio.sleep(attente)

        for theme in THEMES:
            rendu.choisir(SELECT_THEME, theme)
            rendu.choisir(SELECT_MODE_THEME, "Sombre")
            await asyncio.sleep(3 * attente)
            for popup in POPUPS:
                for p in range(passes):
                    await geste(theme, popup, p, popup, ecran=popup)
                    if captures is not None and p == 0:
                        await rendu.capturer(f"{slug(theme)}-{slug(popup)}")
                    await geste(theme, f"{popup} (fermer)", p, "Accueil", ecran="Accueil")
            for p in range(passes):
                await geste(theme, "Calendrier (avant le jour)", p, "Calendrier", ecran="Calendrier")
                await geste(theme, "Jour du calendrier", p, "Calendrier", toucher=jour)
                if captures is not None and p == 0:
                    await rendu.capturer(f"{slug(theme)}-Jour")
                await geste(theme, "Jour du calendrier (fermer)", p, "Accueil", ecran="Accueil")
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
    p.add_argument("--carte", default="waveshare-7b", help="tab5 ou waveshare-7b (coordonnées du toucher)")
    a = p.parse_args()
    asyncio.run(piloter(a.host, a.passes, a.attente, a.captures, a.carte))
    return 0


if __name__ == "__main__":
    sys.exit(main())
