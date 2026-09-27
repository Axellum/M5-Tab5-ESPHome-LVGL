# -*- coding: utf-8 -*-
"""tools/rendu/ecrans.py — Les écrans que le rendu capture après les scènes du mode démo.

Chaque écran s'ouvre depuis l'accueil comme sur la dalle : appuis du doigt virtuel du
rendu (Toucher, Glisser : coordonnées LOGIQUES, celles des captures PNG, paysage
1280×720), ou select HA « Aller à l'écran » (Aller). tools/rendu/capturer.py joue les
étapes, capture, joue `fermer` puis revient à l'accueil par « Aller à l'écran » →
Accueil, qui referme fenêtres, sous-fenêtres et jeu en cours.

Les coordonnées viennent des captures de référence (docs/images/rendu/) et des
positions déclarées dans Tab5/*.yaml. Si la mise en page change, capturer.py signale
une capture identique à une autre : l'appui est tombé à côté.

Module pur (stdlib), vérifié par tests/test_rendu_ecrans.py.
"""
from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class Toucher:
    """Appui du doigt en (x, y). `duree` en ms : 1000 et plus pour un appui long."""
    x: int
    y: int
    duree: int = 150
    apres: float = 0.6


def Long(x: int, y: int, apres: float = 0.6) -> Toucher:  # noqa: N802 — se lit comme une étape
    """Appui long (on_long_press d'LVGL : 400 ms par défaut, 1,2 s par sécurité)."""
    return Toucher(x, y, duree=1200, apres=apres)


@dataclass(frozen=True)
class Glisser:
    """Geste du doigt de (x1, y1) à (x2, y2), 300 ms."""
    x1: int
    y1: int
    x2: int
    y2: int
    apres: float = 0.8


@dataclass(frozen=True)
class Aller:
    """Option du select HA « Aller à l'écran » (Tab5/tab5-ha-controls.yaml)."""
    option: str
    apres: float = 0.8


@dataclass(frozen=True)
class Service:
    """Action de l'API du rendu (tab5_maj_* comme HA, ou rendu_*)."""
    nom: str
    donnees: tuple = ()
    apres: float = 0.8


@dataclass(frozen=True)
class Attendre:
    secondes: float


@dataclass(frozen=True)
class Ecran:
    """Un écran à capturer : `nom` = nom du fichier (ASCII, tirets, sans suffixe de langue)."""
    nom: str
    etapes: tuple
    fermer: tuple = ()
    attente: float = 1.2
    portrait: bool = False


ECRANS: tuple[Ecran, ...] = (
    Ecran("calendrier", (Aller("Calendrier"),)),
    Ecran("console-systeme", (Toucher(1061, 65),)),
    Ecran("telecommande-tv", (Toucher(1205, 65),)),
)

# Captures en portrait (pas de rotation en PNG).
PORTRAITS: frozenset = frozenset(e.nom for e in ECRANS if e.portrait)
