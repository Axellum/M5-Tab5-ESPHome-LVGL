# -*- coding: utf-8 -*-
"""tools/rendu/capturer.py — Captures de l'écran : scènes du mode démo, puis chaque fenêtre.

Le rendu hors tablette (tab5-rendu-host.yaml, plateforme `host` d'ESPHome) tourne sur
la machine et dessine l'interface dans un affichage en mémoire. Ce script s'y connecte
comme Home Assistant, lui pousse chaque scène de tools/demo/scenarios.py (les mêmes
appels que le mode démo, pièces comprises quand le rendu a l'action tab5_maj_tuiles,
ADR-0023) et lui demande une capture (action `rendu_capture`). Les BMP
sont écrits par le rendu dans ESPHOME_SNAPSHOT_DIR ; avec Pillow, ce script en fait
des PNG en paysage (la dalle du Tab5 est en portrait, LVGL tourne l'image de 270°,
vérifié sur les premières captures). Neon Apron, en portrait, n'est pas tourné.

Après les scènes, chaque écran de tools/rendu/ecrans.py (fenêtres, sous-fenêtres,
sélecteur Arcade et jeux) est ouvert comme sur la dalle, par le doigt virtuel du rendu
(actions rendu_toucher / rendu_glisser, Tab5/rendu/rendu_doigt.h) ou par le select HA
« Aller à l'écran », capturé, puis refermé (« Aller à l'écran » → Accueil). Une capture
identique à une précédente est signalée : l'appui n'a rien ouvert (coordonnées
périmées après un changement de mise en page ?).

Clé API : un rendu qui n'en a pas en reçoit une (comme une tablette neuve ajoutée à HA),
gardée dans tools/demo/cle_demo.txt et reprise au lancement suivant.

Langue : `--langue Deutsch` choisit seulement la langue dans le select « Langue »
(`--puis-langue`, la même chose après les captures). Comme la tablette, le rendu
enregistre et redémarre, c'est-à-dire qu'il s'arrête sur la plateforme host : on le
relance (même ESPHOME_PREFDIR), puis `--suffixe de` pour les captures allemandes.

Thèmes (ADR-0029) : `--bascule Clair` passe le select « Clair ou sombre » à Clair juste
avant chaque capture, puis le remet à Sombre : l'écran a été peint en sombre, il est
capturé après la bascule à chaud. `--puis-mode-theme Clair` choisit le mode après les
captures et enregistre les préférences (action rendu_enregistrer) : relancé, le rendu
démarre dans ce mode. Le job compare les deux séries au pixel près. `--sans-jeux` saute
les consoles (« jeu-… », 56 écrans qui restent sombres, ADR-0014) : sans elles, les deux
passes tiennent dans le délai de la tâche ; le sélecteur Arcade reste, en témoin.

Galerie des thèmes (lot 3) : `--galerie` pousse la première scène, puis, pour chaque
thème de Tab5/themes/ et chaque mode (Sombre, Clair), choisit le thème et le mode à
chaud et capture l'accueil et le popup de la climatisation (« theme-<fichier>-<mode>-
accueil » / « -clim »). Passés l'un après l'autre, les thèmes montrent aussi ce qu'un
thème laisserait au suivant. `--galerie-un theme-<fichier>-<mode>` fait les deux mêmes
captures sans rien choisir (démarrage à froid dans ce thème) ; `--puis-theme NOM`,
avec `--puis-mode-theme`, prépare le démarrage suivant ; `--preparer` saute toute
capture. `--liste-galerie` écrit les combinaisons « nom|fichier|mode », une par ligne.
Le job compare la série à chaud et la série à froid au pixel près.

Usage (voir .github/workflows/rendu-host.yml, une tâche par langue) :
    ESPHOME_SNAPSHOT_DIR=captures ESPHOME_PREFDIR=prefs ./program &
    python tools/rendu/capturer.py --dossier captures --langue Deutsch
    ESPHOME_SNAPSHOT_DIR=captures ESPHOME_PREFDIR=prefs ./program &
    python tools/rendu/capturer.py --dossier captures --suffixe de
    python tools/rendu/capturer.py --dossier captures --seulement calendrier   # mise au point
"""
from __future__ import annotations

import argparse
import asyncio
import hashlib
import logging
import sys
import unicodedata
from pathlib import Path

RACINE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RACINE / "demo"))
sys.path.insert(0, str(RACINE))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from demo_pusher import _appeler, _donner_une_cle, _lire_cle_demo, _pousser_scene  # noqa: E402
from ecrans import ECRANS, PORTRAITS, Aller, Attendre, Glisser, Service, Toucher  # noqa: E402
from scenarios import SCENES, build_zones_absentes  # noqa: E402

logger = logging.getLogger("capturer")

# Laisse LVGL finir ses animations (rouleau de l'horloge, bascules) avant la capture.
ATTENTE_RENDU = 3.0

# Panneau de la carte centrale montré par chaque scène (action rendu_panneau : 0
# planning, 1 pluie, 3 info). Sans ça, le rotateur (8 s) décidait selon le moment de
# la capture, parfois en plein fondu entre deux panneaux.
PANNEAUX = {1: 0, 2: 1, 3: 3}
# rendu_panneau avance d'un pas toutes les 500 ms, 8 pas au plus.
ATTENTE_PANNEAU = 5.0

# Après le retour à l'accueil entre deux écrans : fondu de fermeture des fenêtres.
ATTENTE_RETOUR = 1.0

# Après un changement de thème : la repeinture est faite dans la boucle, LVGL redessine
# l'écran à l'image suivante.
ATTENTE_BASCULE = 1.0
SELECT_MODE_THEME = "Clair ou sombre"
SELECT_THEME = "Thème"
MODES_THEME = ("Sombre", "Clair")


def combinaisons_galerie() -> list[tuple[str, str, str]]:
    """(nom du thème, fichier, mode) de chaque capture de la galerie, dans l'ordre du select."""
    sys.path.insert(0, str(RACINE))
    import gen_themes

    return [(t.nom, t.fichier, m) for t in gen_themes.charger() for m in MODES_THEME]


def nom_galerie(fichier: str, mode: str) -> str:
    return f"theme-{fichier}-{mode.lower()}"


def nom_de(index: int, nom_scene: str, suffixe: str = "") -> str:
    """« 1-journee-ensoleillee » (ou « …-en ») : ASCII, sans espace, dans l'ordre des scènes."""
    ascii_ = unicodedata.normalize("NFKD", nom_scene).encode("ascii", "ignore").decode()
    mots = "".join(c if c.isalnum() else " " for c in ascii_.lower()).split()
    return f"{index}-{'-'.join(mots)}" + (f"-{suffixe}" if suffixe else "")


def _portrait(nom_fichier: str) -> bool:
    """Capture d'un écran en portrait (Neon Apron), quel que soit le suffixe de langue."""
    return any(nom_fichier == p or nom_fichier.startswith(p + "-") for p in PORTRAITS)


def en_png(dossier: Path) -> list[Path]:
    """Chaque BMP du dossier devient un PNG à l'endroit (Pillow requis), puis le BMP
    (2,7 Mo, ~330 par run) est supprimé."""
    try:
        from PIL import Image
    except ImportError:
        logger.warning("Pillow absent : les captures restent en BMP portrait")
        return []
    pngs = []
    for bmp in sorted(dossier.glob("*.bmp")):
        png = bmp.with_suffix(".png")
        with Image.open(bmp) as image:
            (image if _portrait(bmp.stem) else image.rotate(270, expand=True)).save(png, optimize=True)
        bmp.unlink()
        pngs.append(png)
    return pngs


class Rendu:
    """Connexion au rendu : services, selects, captures et contrôle des doublons."""

    def __init__(self, client, entites, services, dossier: Path, suffixe: str,
                 bascule: str | None = None):
        from aioesphomeapi import SelectInfo

        self.client = client
        self.services = {s.name: s for s in services}
        self.selects = {e.name: e for e in entites if isinstance(e, SelectInfo)}
        self.dossier = dossier
        self.suffixe = suffixe
        self.bascule = bascule
        self.empreintes: dict[str, str] = {}
        self.alertes: list[str] = []

    async def appeler(self, nom: str, **data) -> None:
        await _appeler(self.client, self.services, nom, **data)

    def choisir(self, select: str, option: str) -> None:
        self.client.select_command(self.selects[select].key, option)

    async def capturer(self, nom: str) -> None:
        fichier = f"{nom}-{self.suffixe}" if self.suffixe else nom
        if self.bascule:
            self.choisir(SELECT_MODE_THEME, self.bascule)
            await asyncio.sleep(ATTENTE_BASCULE)
        await self.appeler("rendu_capture", fichier=fichier)
        await asyncio.sleep(1.0)
        if self.bascule:
            self.choisir(SELECT_MODE_THEME, "Sombre")
            await asyncio.sleep(ATTENTE_BASCULE)
        bmp = self.dossier / f"{fichier}.bmp"
        if not bmp.exists():
            self.alerter(f"{fichier} : pas de BMP écrit par le rendu")
            return
        empreinte = hashlib.sha256(bmp.read_bytes()).hexdigest()
        if empreinte in self.empreintes:
            self.alerter(f"{fichier} : identique à {self.empreintes[empreinte]} (l'appui n'a rien ouvert ?)")
        self.empreintes.setdefault(empreinte, fichier)

    def alerter(self, texte: str) -> None:
        logger.warning(texte)
        self.alertes.append(texte)

    async def jouer(self, etape) -> None:
        if isinstance(etape, Toucher):
            await self.appeler("rendu_toucher", x=etape.x, y=etape.y, duree=etape.duree)
            await asyncio.sleep(etape.duree / 1000 + etape.apres)
        elif isinstance(etape, Glisser):
            await self.appeler("rendu_glisser", x1=etape.x1, y1=etape.y1, x2=etape.x2, y2=etape.y2)
            await asyncio.sleep(0.4 + etape.apres)
        elif isinstance(etape, Aller):
            self.choisir("Aller à l'écran", etape.option)
            await asyncio.sleep(etape.apres)
        elif isinstance(etape, Service):
            await self.appeler(etape.nom, **dict(etape.donnees))
            await asyncio.sleep(etape.apres)
        elif isinstance(etape, Attendre):
            await asyncio.sleep(etape.secondes)
        else:
            raise TypeError(f"étape inconnue : {etape!r}")

    async def accueil(self) -> None:
        """Referme tout (fenêtres, sous-fenêtres, jeu en cours) : même chemin que HA."""
        self.choisir("Aller à l'écran", "Accueil")
        await asyncio.sleep(ATTENTE_RETOUR)

    async def galerie(self, prefixe: str) -> None:
        """L'accueil et le popup de la climatisation, dans le thème affiché."""
        await self.capturer(f"{prefixe}-accueil")
        clim = next(e for e in ECRANS if e.nom == "climatisation")
        for etape in clim.etapes:
            await self.jouer(etape)
        await asyncio.sleep(clim.attente)
        await self.capturer(f"{prefixe}-clim")
        for etape in clim.fermer:
            await self.jouer(etape)
        await self.accueil()


async def capturer(hote: str, dossier: Path, suffixe: str, puis_langue: str | None,
                   seulement: set[str] | None, langue: str | None = None,
                   bascule: str | None = None, puis_mode_theme: str | None = None,
                   sans_jeux: bool = False, galerie: bool = False, galerie_un: str | None = None,
                   puis_theme: str | None = None, preparer: bool = False) -> list[str]:
    from aioesphomeapi import APIClient

    cle = _lire_cle_demo() or await _donner_une_cle(hote)
    client = APIClient(hote, 6053, "", noise_psk=cle, client_info="Tab5 captures")
    await client.connect(login=True)
    try:
        entites, services = await client.list_entities_services()
        rendu = Rendu(client, entites, services, dossier, suffixe, bascule)
        if langue:
            logger.info("Langue -> %s, sans capture (le rendu enregistre et s'arrête)", langue)
            rendu.choisir("Langue", langue)
            await asyncio.sleep(3.0)
            return []
        if galerie or galerie_un or preparer:
            if not preparer:
                await rendu.appeler("tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
                await _pousser_scene(client, rendu.services, SCENES[0], frozenset())
                await asyncio.sleep(ATTENTE_RENDU)
                await rendu.appeler("rendu_panneau", panneau=PANNEAUX.get(1, 0))
                await asyncio.sleep(ATTENTE_PANNEAU)
            if galerie:
                for nom, fichier, mode in combinaisons_galerie():
                    logger.info("Galerie : %s, %s", nom, mode)
                    rendu.choisir(SELECT_THEME, nom)
                    rendu.choisir(SELECT_MODE_THEME, mode)
                    await asyncio.sleep(ATTENTE_BASCULE)
                    await rendu.galerie(nom_galerie(fichier, mode))
            if galerie_un:
                await rendu.galerie(galerie_un)
            if puis_theme:
                rendu.choisir(SELECT_THEME, puis_theme)
            if puis_mode_theme:
                rendu.choisir(SELECT_MODE_THEME, puis_mode_theme)
            if puis_theme or puis_mode_theme:
                logger.info("Thème -> %s, %s, préférences enregistrées", puis_theme, puis_mode_theme)
                await asyncio.sleep(ATTENTE_BASCULE)
                await rendu.appeler("rendu_enregistrer")
                await asyncio.sleep(1.0)
            return rendu.alertes
        await rendu.appeler("tab5_maj_zones", absentes=build_zones_absentes(frozenset()))
        for index, scene in enumerate(SCENES, 1):
            logger.info("Scène %d : %s", index, scene.nom)
            await _pousser_scene(client, rendu.services, scene, frozenset())
            await asyncio.sleep(ATTENTE_RENDU)
            await rendu.appeler("rendu_panneau", panneau=PANNEAUX.get(index, 0))
            await asyncio.sleep(ATTENTE_PANNEAU)
            if seulement is None:
                await rendu.capturer(nom_de(index, scene.nom))
        # Les écrans s'ouvrent sur la dernière scène, carte centrale arrêtée.
        for ecran in ECRANS:
            if seulement is not None and ecran.nom not in seulement:
                continue
            if sans_jeux and ecran.nom.startswith("jeu-"):
                continue
            logger.info("Écran : %s", ecran.nom)
            for etape in ecran.etapes:
                await rendu.jouer(etape)
            await asyncio.sleep(ecran.attente)
            await rendu.capturer(ecran.nom)
            for etape in ecran.fermer:
                await rendu.jouer(etape)
            await rendu.accueil()
        if puis_mode_theme:
            logger.info("Thème -> %s, préférences enregistrées", puis_mode_theme)
            rendu.choisir(SELECT_MODE_THEME, puis_mode_theme)
            await asyncio.sleep(ATTENTE_BASCULE)
            await rendu.appeler("rendu_enregistrer")
            await asyncio.sleep(1.0)
        if puis_langue:
            logger.info("Langue -> %s (le rendu enregistre et s'arrête)", puis_langue)
            rendu.choisir("Langue", puis_langue)
            await asyncio.sleep(3.0)
        return rendu.alertes
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
    parser.add_argument("--seulement", nargs="+", metavar="ECRAN",
                        help="ces écrans seulement (noms de tools/rendu/ecrans.py), sans les scènes")
    parser.add_argument("--langue", help="choisit seulement cette langue (« Deutsch »), sans capture")
    parser.add_argument("--bascule", metavar="MODE",
                        help="avant chaque capture, passe « Clair ou sombre » à MODE (« Clair »), puis à Sombre")
    parser.add_argument("--sans-jeux", action="store_true",
                        help="sans les consoles (écrans « jeu-… », qui restent sombres)")
    parser.add_argument("--puis-mode-theme", metavar="MODE",
                        help="après les captures, choisit ce mode et enregistre les préférences")
    parser.add_argument("--galerie", action="store_true",
                        help="accueil et climatisation de chaque thème, dans les deux modes, à chaud")
    parser.add_argument("--galerie-un", metavar="NOM",
                        help="accueil et climatisation dans le thème affiché (theme-<fichier>-<mode>)")
    parser.add_argument("--puis-theme", metavar="NOM",
                        help="avec la galerie : choisit ce thème à la fin et enregistre les préférences")
    parser.add_argument("--preparer", action="store_true",
                        help="aucune capture : seulement --puis-theme / --puis-mode-theme")
    parser.add_argument("--liste-galerie", action="store_true",
                        help="écrit « nom|fichier|mode » de chaque capture de la galerie et s'arrête")
    args = parser.parse_args()

    if args.liste_galerie:
        for nom, fichier, mode in combinaisons_galerie():
            print(f"{nom}|{fichier}|{mode}")
        return 0
    alertes = asyncio.run(capturer(args.host, args.dossier, args.suffixe, args.puis_langue,
                                   set(args.seulement) if args.seulement else None, args.langue,
                                   args.bascule, args.puis_mode_theme, args.sans_jeux,
                                   args.galerie, args.galerie_un, args.puis_theme, args.preparer))
    if args.langue or args.preparer:
        return 0
    pngs = en_png(args.dossier)
    logger.info("%d captures PNG dans %s", len(pngs), args.dossier)
    for texte in alertes:
        print(f"::warning title=Captures::{texte}")
    return 0 if pngs else 1


if __name__ == "__main__":
    sys.exit(main())
