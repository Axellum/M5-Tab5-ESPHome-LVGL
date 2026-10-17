"""Branche essai/mesure-invalidation (JAMAIS MERGÉE) : actions courantes de la tablette virtuelle, par fenêtres.

    MESURE_CMD=/tmp/cmd ./programme > journal.log 2>&1 &
    python tools/mesure/piloter_actions.py --cmd /tmp/cmd --journal journal.log [--facteur 5] [--captures X]

Tablette virtuelle en 1024×600 (Waveshare 7B ; points de tools/rendu/ecrans.py ramenés par
tools/rendu/vers_carte.py). Démo : zones, scène 1 puis scène 2 (pluie + alerte : la carte
centrale tourne). Phase « demarrage » (aucun popup construit au début), les 20 écrans du
select « Aller à l'écran » ouverts une fois (ils se construisent à la demande), phase
« ecrans ». Chaque action est une fenêtre : « zero PHASE__ACTION » écrit dans le fichier de
commande (mesure_actions.h l'efface quand il l'a lu), l'action, l'attente, puis « dump … ».
`--facteur` multiplie les attentes (sous valgrind). `--captures` : un BMP à la fin de chaque
phase (action rendu_capture), pour comparer les variantes au pixel.
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
# Points de la 7B (1024×600) : vers_carte.adapter() des étapes d'ecrans.py.
PREVISIONS_GAUCHE = (812, 433, 212, 433)   # VERS_LA_GAUCHE (1015, 520 → 265, 520)
PREVISIONS_DROITE = (212, 433, 812, 433)   # VERS_LA_DROITE
BOUTON_REGLAGES = (849, 54)                # BOUTON_SYS
CURSEUR_LUMINOSITE = (717, 189, 323, 189)  # REGLAGES_GLISSER_CURSEUR
CURSEUR_A_100 = (323, 189, 1008, 189)      # REGLAGES_CURSEUR_A_100


class Pilote:
    def __init__(self, client, cmd: Path, facteur: float, captures: str | None):
        self.client = client
        self.cmd = cmd
        self.facteur = facteur
        self.captures = captures
        self.selects: dict = {}
        self.services: dict = {}

    async def preparer(self) -> None:
        from aioesphomeapi import SelectInfo

        entites, services = await self.client.list_entities_services()
        self.selects = {e.name: e for e in entites if isinstance(e, SelectInfo)}
        self.services = {s.name: s for s in services}

    async def appeler(self, nom: str, **data) -> None:
        await _appeler(self.client, self.services, nom, **data)

    async def attendre(self, s: float) -> None:
        await asyncio.sleep(s * self.facteur)

    async def commande(self, texte: str) -> None:
        tmp = self.cmd.with_suffix(".tmp")
        tmp.write_text(texte + "\n", encoding="utf-8")
        tmp.replace(self.cmd)
        fin = time.monotonic() + 60 * self.facteur
        while self.cmd.exists():
            if time.monotonic() > fin:
                raise SystemExit(f"Commande « {texte} » pas lue par la tablette")
            await asyncio.sleep(0.05)

    async def aller(self, ecran: str) -> None:
        self.client.select_command(self.selects["Aller à l'écran"].key, ecran)

    async def glisser(self, x1: int, y1: int, x2: int, y2: int) -> None:
        await self.appeler("rendu_glisser", x1=x1, y1=y1, x2=x2, y2=y2)

    async def fenetre(self, phase: str, nom: str, action, attente: float) -> None:
        await self.attendre(1.0)
        print(f"[pilote] fenetre {phase}__{nom}", flush=True)
        await self.commande(f"zero {phase}__{nom}")
        await action()
        await self.attendre(attente)
        await self.commande(f"dump {phase}__{nom}")

    # --- Actions -------------------------------------------------------------------------
    async def rien(self) -> None:
        return None

    async def previsions(self) -> None:
        await self.glisser(*PREVISIONS_GAUCHE)
        await self.attendre(1.5)
        await self.glisser(*PREVISIONS_DROITE)

    async def scene(self) -> None:
        await _pousser_scene(self.client, self.services, SCENES[2], frozenset())
        await self.attendre(1.0)
        await _pousser_scene(self.client, self.services, SCENES[1], frozenset())

    def popup(self, ecran: str):
        async def ouvrir_fermer() -> None:
            await self.aller(ecran)
            await self.attendre(1.5)
            await self.aller("Accueil")
        return ouvrir_fermer

    async def curseur(self) -> None:
        await self.glisser(*CURSEUR_LUMINOSITE)
        await self.attendre(1.0)
        await self.glisser(*CURSEUR_A_100)

    async def phase(self, phase: str) -> None:
        await self.fenetre(phase, "repos_carte_centrale", self.rien, 20.0)
        await self.fenetre(phase, "glisser_previsions", self.previsions, 1.5)
        await self.fenetre(phase, "poussee_ha", self.scene, 1.5)
        if phase == "demarrage":
            # Première ouverture = construction : hors fenêtre ; la fenêtre mesure la seconde.
            await self.popup("Musique")()
            await self.attendre(2.0)
        await self.fenetre(phase, "popup_musique", self.popup("Musique"), 1.0)
        if phase == "ecrans":
            await self.fenetre(phase, "popup_lumieres", self.popup("Lumières"), 1.0)
            await self.fenetre(phase, "popup_calendrier", self.popup("Calendrier"), 1.0)
        await self.appeler("rendu_toucher", x=BOUTON_REGLAGES[0], y=BOUTON_REGLAGES[1], duree=150)
        await self.attendre(2.0)
        await self.fenetre(phase, "curseur_reglages", self.curseur, 1.0)
        await self.aller("Accueil")
        await self.attendre(1.5)
        if self.captures:
            await self.appeler("rendu_capture", fichier=f"{self.captures}_{phase}")
            await self.attendre(1.0)


async def connecter(hote: str, delai: float):
    from aioesphomeapi import APIClient

    fin = time.monotonic() + delai
    while True:
        try:
            cle = _lire_cle_demo() or await _donner_une_cle(hote)
            client = APIClient(hote, 6053, "", noise_psk=cle, client_info="Tab5 mesure actions")
            await client.connect(login=True)
            return client
        except Exception as exc:  # noqa: BLE001 — le programme démarre encore
            if time.monotonic() > fin:
                raise
            logger.info("API pas encore prête (%s), nouvel essai", exc)
            await asyncio.sleep(2.0)


async def piloter(hote: str, cmd: Path, facteur: float, captures: str | None) -> int:
    client = await connecter(hote, 600 * facteur)
    try:
        p = Pilote(client, cmd, facteur, captures)
        await p.preparer()
        await p.appeler("tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
        await _pousser_scene(client, p.services, SCENES[0], frozenset())
        await p.attendre(2.0)
        await _pousser_scene(client, p.services, SCENES[1], frozenset())
        await p.attendre(3.0)
        await p.phase("demarrage")
        for ecran in ECRANS:
            logger.info("Écran : %s", ecran)
            await p.aller(ecran)
            await p.attendre(1.5)
            await p.aller("Accueil")
            await p.attendre(1.0)
        await p.phase("ecrans")
        return 0
    finally:
        await client.disconnect()


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--cmd", type=Path, required=True, help="fichier de commande (MESURE_CMD du programme)")
    p.add_argument("--facteur", type=float, default=1.0, help="multiplie les attentes (valgrind)")
    p.add_argument("--captures", default=None, help="préfixe des BMP de fin de phase")
    args = p.parse_args()
    return asyncio.run(piloter(args.host, args.cmd, args.facteur, args.captures))


if __name__ == "__main__":
    sys.exit(main())
