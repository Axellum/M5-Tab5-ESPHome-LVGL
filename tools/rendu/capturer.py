# -*- coding: utf-8 -*-
"""tools/rendu/capturer.py — Captures de l'écran par scène du mode démo (lot 7).

Le rendu hors tablette (tab5-rendu-host.yaml, plateforme `host` d'ESPHome) tourne sur
la machine et dessine l'interface dans un affichage en mémoire. Ce script s'y connecte
comme Home Assistant, lui pousse chaque scène de tools/demo/scenarios.py (les mêmes
appels que le mode démo) et lui demande une capture (action `rendu_capture`). Les BMP
sont écrits par le rendu dans ESPHOME_SNAPSHOT_DIR ; avec Pillow, ce script en fait
des PNG en paysage (la dalle du Tab5 est en portrait, LVGL tourne l'image de 270°,
vérifié sur les premières captures).

Clé API : un rendu qui n'en a pas en reçoit une (comme une tablette neuve ajoutée à HA),
gardée dans tools/demo/cle_demo.txt et reprise au lancement suivant.

Langue : `--puis-langue English` choisit la langue dans le select « Langue » après les
captures. Comme la tablette, le rendu enregistre et redémarre, c'est-à-dire qu'il
s'arrête sur la plateforme host : on le relance (même ESPHOME_PREFDIR), puis
`--suffixe en` pour les captures anglaises.

Usage (voir .github/workflows/rendu-host.yml) :
    ESPHOME_SNAPSHOT_DIR=captures ESPHOME_PREFDIR=prefs ./program &
    python tools/rendu/capturer.py --dossier captures --puis-langue English
    ESPHOME_SNAPSHOT_DIR=captures ESPHOME_PREFDIR=prefs ./program &
    python tools/rendu/capturer.py --dossier captures --suffixe en
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import sys
import unicodedata
from pathlib import Path

RACINE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RACINE / "demo"))
sys.path.insert(0, str(RACINE))

from demo_pusher import _appeler, _donner_une_cle, _lire_cle_demo, _pousser_scene  # noqa: E402
from scenarios import SCENES, build_zones_absentes  # noqa: E402

logger = logging.getLogger("capturer")

# Laisse LVGL finir ses animations (rouleau de l'horloge, bascules) avant la capture.
ATTENTE_RENDU = 3.0


def nom_de(index: int, nom_scene: str, suffixe: str = "") -> str:
    """« 1-journee-ensoleillee » (ou « …-en ») : ASCII, sans espace, dans l'ordre des scènes."""
    ascii_ = unicodedata.normalize("NFKD", nom_scene).encode("ascii", "ignore").decode()
    mots = "".join(c if c.isalnum() else " " for c in ascii_.lower()).split()
    return f"{index}-{'-'.join(mots)}" + (f"-{suffixe}" if suffixe else "")


def en_png(dossier: Path) -> list[Path]:
    """Chaque BMP du dossier devient un PNG en paysage (Pillow requis)."""
    try:
        from PIL import Image
    except ImportError:
        logger.warning("Pillow absent : les captures restent en BMP portrait")
        return []
    pngs = []
    for bmp in sorted(dossier.glob("*.bmp")):
        png = bmp.with_suffix(".png")
        with Image.open(bmp) as image:
            image.rotate(270, expand=True).save(png, optimize=True)
        pngs.append(png)
    return pngs


async def capturer(hote: str, suffixe: str, puis_langue: str | None) -> None:
    from aioesphomeapi import APIClient, SelectInfo

    cle = _lire_cle_demo() or await _donner_une_cle(hote)
    client = APIClient(hote, 6053, "", noise_psk=cle, client_info="Tab5 captures")
    await client.connect(login=True)
    try:
        entites, services = await client.list_entities_services()
        services_par_nom = {s.name: s for s in services}
        await _appeler(client, services_par_nom, "tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
        for index, scene in enumerate(SCENES, 1):
            logger.info("Scène %d : %s", index, scene.nom)
            await _pousser_scene(client, services_par_nom, scene, frozenset())
            await asyncio.sleep(ATTENTE_RENDU)
            await _appeler(client, services_par_nom, "rendu_capture", fichier=nom_de(index, scene.nom, suffixe))
            await asyncio.sleep(1.0)
        if puis_langue:
            langue = next(e for e in entites if isinstance(e, SelectInfo) and e.name == "Langue")
            logger.info("Langue -> %s (le rendu enregistre et s'arrête)", puis_langue)
            client.select_command(langue.key, puis_langue)
            await asyncio.sleep(3.0)
    finally:
        await client.disconnect()


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1", help="adresse du rendu (défaut 127.0.0.1)")
    parser.add_argument("--dossier", type=Path, required=True,
                        help="dossier des BMP (le ESPHOME_SNAPSHOT_DIR du rendu)")
    parser.add_argument("--suffixe", default="", help="ajouté au nom des captures (ex. « en »)")
    parser.add_argument("--puis-langue", help="après les captures, choisit cette langue (« English »)")
    args = parser.parse_args()

    asyncio.run(capturer(args.host, args.suffixe, args.puis_langue))
    pngs = en_png(args.dossier)
    bmps = sorted(args.dossier.glob("*.bmp"))
    logger.info("%d captures BMP, %d PNG dans %s", len(bmps), len(pngs), args.dossier)
    return 0 if bmps else 1


if __name__ == "__main__":
    sys.exit(main())
