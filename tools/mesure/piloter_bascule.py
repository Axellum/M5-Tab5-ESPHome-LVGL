"""Branche essai/mesure-bascule-theme (JAMAIS MERGÉE) : pilote les bascules de thème de la tablette virtuelle.

    ./programme > journal.log 2>&1 &
    python tools/mesure/piloter_bascule.py --journal journal.log [--facteur 10]

Comme la nuit de mesure de la 7B (16/10/2026) : la première scène de la démo poussée, puis
phase « demarrage » (popups pas encore construits), les 20 écrans du select « Aller à
l'écran » ouverts une fois chacun (ils se construisent à la demande), puis phase « ecrans ».
Chaque bascule = UN select changé (thème ou mode : une seule repeinture) ; le pilote attend
dans le journal du programme la ligne « bNN etape 10_image_suivante » de mesure_bascule.h.
`--facteur` multiplie les attentes (sous valgrind).
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import sys
import time
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RACINE / "tools" / "demo"))
sys.path.insert(0, str(RACINE / "tools"))

from demo_pusher import _appeler, _donner_une_cle, _lire_cle_demo, _pousser_scene  # noqa: E402
from scenarios import SCENES, build_zones_absentes  # noqa: E402

logger = logging.getLogger("pilote")

ECRANS = ["Assistant vocal", "Calendrier", "Réveil", "Climatisation", "Plantes", "Télécommande TV",
          "Console système", "Énergie", "Réglages", "Alertes", "Maison", "Lumières", "Volet", "Température",
          "Météo", "Musique", "Caméras", "Suivi", "Froid", "Serveur IA"]
THEME, MODE = "Thème", "Clair ou sombre"
PHASES = {
    "demarrage": [(THEME, "Relief doux"), (MODE, "Clair"), (THEME, "Zen Sumi")],
    "ecrans": [(THEME, "Almanach"), (MODE, "Sombre"), (THEME, "Pixel"), (MODE, "Clair"),
               (THEME, "Relief doux"), (MODE, "Sombre"), (THEME, "Bonbon"), (THEME, "Ardoise")],
}


def images_finies(journal: Path) -> int:
    try:
        return journal.read_text(encoding="utf-8", errors="replace").count("etape 10_image_suivante")
    except FileNotFoundError:
        return 0


async def attendre_image(journal: Path, n: int, delai: float) -> bool:
    fin = time.monotonic() + delai
    while time.monotonic() < fin:
        if images_finies(journal) >= n:
            return True
        await asyncio.sleep(0.2)
    return False


async def connecter(hote: str, delai: float):
    from aioesphomeapi import APIClient

    fin = time.monotonic() + delai
    while True:
        try:
            cle = _lire_cle_demo() or await _donner_une_cle(hote)
            client = APIClient(hote, 6053, "", noise_psk=cle, client_info="Tab5 mesure bascule")
            await client.connect(login=True)
            return client
        except Exception as exc:  # noqa: BLE001 — le programme démarre encore
            if time.monotonic() > fin:
                raise
            logger.info("API pas encore prête (%s), nouvel essai", exc)
            await asyncio.sleep(2.0)


async def piloter(hote: str, journal: Path, facteur: float) -> int:
    client = await connecter(hote, 600 * facteur)
    try:
        entites, services = await client.list_entities_services()
        from aioesphomeapi import SelectInfo

        selects = {e.name: e for e in entites if isinstance(e, SelectInfo)}
        par_nom = {s.name: s for s in services}
        await _appeler(client, par_nom, "tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
        await _pousser_scene(client, par_nom, SCENES[0], frozenset())
        await asyncio.sleep(3.0 * facteur)
        attendues = images_finies(journal)
        manquees = 0
        for phase, bascules in PHASES.items():
            if phase == "ecrans":
                for ecran in ECRANS:
                    logger.info("Écran : %s", ecran)
                    client.select_command(selects["Aller à l'écran"].key, ecran)
                    await asyncio.sleep(1.5 * facteur)
                    client.select_command(selects["Aller à l'écran"].key, "Accueil")
                    await asyncio.sleep(1.0 * facteur)
            for select, option in bascules:
                attendues += 1
                print(f"[pilote] b{attendues:02d} phase {phase} {select} -> {option}", flush=True)
                client.select_command(selects[select].key, option)
                if not await attendre_image(journal, attendues, 60 * facteur):
                    logger.warning("b%02d : pas d'image suivante dans le journal", attendues)
                    manquees += 1
                    attendues = images_finies(journal)
                await asyncio.sleep(1.0 * facteur)
        return 1 if manquees else 0
    finally:
        await client.disconnect()


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--journal", type=Path, required=True, help="sortie du programme (stdout)")
    p.add_argument("--facteur", type=float, default=1.0, help="multiplie les attentes (valgrind)")
    args = p.parse_args()
    return asyncio.run(piloter(args.host, args.journal, args.facteur))


if __name__ == "__main__":
    sys.exit(main())
