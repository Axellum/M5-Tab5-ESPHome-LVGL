# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Où sont rangées les sources du firmware dans Tab5/ (source unique pour les outils et les tests).

@role Depuis le rangement du 08/10/2026, les fichiers du firmware ne sont plus à plat dans
      Tab5/ mais dans quatre sous-dossiers :
        - socle/   : le code pur, compilé et testé sur PC sans ESPHome ni LVGL ;
        - jeux/    : les huit consoles et game_common.h ;
        - ecran/   : la couche LVGL (tab5_custom.h, tab5_internal.h, tab5_*.cpp…) ;
        - paquets/ : les paquets ESPHome (tab5-*.yaml, ecran-*.yaml, publication-*.yaml…).
      Restent à la racine de Tab5/ : user_entities.yaml (+ .example), tuiles_icones.yaml
      (source de tools/gen_tuiles_icones.py, pas un paquet) et README.md ; ui_components/,
      lang/, rendu/, themes/ et fonts/ n'ont pas bougé.
@architecture_constraint ESPHome copie À PLAT dans son src/ chaque fichier listé un par un
      sous `includes:` (esphome/core/config.py, add_includes) : les `#include "x.h"` entre
      fichiers ne portent donc pas de sous-dossier. Les compilations sur PC (g++ de la CI)
      passent les sous-dossiers en `-I`.
@ai_instruction Un outil ou un test qui parcourait `Tab5/*.cpp` (ou .h, .yaml) passe par
      `fichiers()` ; un fichier connu par son nom seul se trouve par `source()`. Ne pas
      reparcourir la racine de Tab5/ : un glob qui y cherche du C++ ne trouve plus rien et
      un garde-fou passerait alors à vide (tests/test_rangement.py le surveille).
      Le contrat C++ des lambdas n'est plus le seul tab5_custom.h (08/10/2026) : c'est lui
      et l'en-tête de chaque module qu'il inclut. Un test qui y cherchait une déclaration
      (enum Ecran, struct ClimUI…) lit `contrat()` ; `contrat_entetes()` donne les fichiers.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
DOSSIERS = ("socle", "ecran", "jeux", "paquets")


def fichiers(*motifs: str, tab5: Path = TAB5) -> list[Path]:
    """Fichiers du firmware qui correspondent à chaque motif (`*.cpp`, `*_game.h`…), dans
    les quatre sous-dossiers ET à la racine de Tab5/ (user_entities*, tuiles_icones.yaml).
    Motif par motif, triés par nom comme l'était l'ancien `sorted(Tab5.glob(motif))`."""
    res: list[Path] = []
    for motif in motifs:
        trouves = list(tab5.glob(motif))
        for dossier in DOSSIERS:
            trouves += (tab5 / dossier).glob(motif)
        res += sorted(trouves, key=lambda p: tab5 / p.name)
    return res


def source(nom: str, tab5: Path = TAB5) -> Path:
    """Chemin d'un fichier du firmware connu par son nom seul (`tab5_cards.cpp`,
    `tab5-lvgl.yaml`…). FileNotFoundError s'il n'est nulle part, ValueError s'il est
    à deux endroits."""
    trouves = [p for p in [tab5 / nom, *(tab5 / d / nom for d in DOSSIERS)] if p.is_file()]
    if not trouves:
        raise FileNotFoundError(f"{nom} : ni dans Tab5/ ni dans Tab5/{{{','.join(DOSSIERS)}}}/")
    if len(trouves) > 1:
        raise ValueError(f"{nom} présent deux fois : {trouves}")
    return trouves[0]


def contrat_entetes(tab5: Path = TAB5) -> list[Path]:
    """tab5_custom.h, puis chaque en-tête de Tab5/ecran/ qu'il inclut, dans son ordre
    (un en-tête par module depuis le 08/10/2026 ; les en-têtes du socle n'en sont pas)."""
    parapluie = tab5 / "ecran" / "tab5_custom.h"
    noms = re.findall(r'^#include "(\w+\.h)"', parapluie.read_text(encoding="utf-8"), re.M)
    return [parapluie] + [tab5 / "ecran" / n for n in noms if (tab5 / "ecran" / n).is_file()]


def contrat(tab5: Path = TAB5) -> str:
    """Texte du contrat C++ des lambdas : tab5_custom.h et les en-têtes de ses modules."""
    return "\n".join(p.read_text(encoding="utf-8") for p in contrat_entetes(tab5))
