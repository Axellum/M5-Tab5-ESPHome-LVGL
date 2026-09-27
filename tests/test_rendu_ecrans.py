# -*- coding: utf-8 -*-
"""Plan des écrans capturés par le rendu (tools/rendu/ecrans.py) : cohérent avec le firmware.

Un nom en double écraserait une capture ; un appui hors de l'écran, une option absente
du select « Aller à l'écran » ou une action inconnue de l'API ne s'apercevraient qu'en
CI, sous la forme d'une capture identique à une autre."""
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "rendu"))

import ecrans  # noqa: E402
from ecrans import ECRANS, Aller, Glisser, Service, Toucher  # noqa: E402


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _etapes(ecran):
    return ecran.etapes + ecran.fermer


def test_noms_uniques_et_en_ascii():
    noms = [e.nom for e in ECRANS]
    assert len(noms) == len(set(noms))
    for nom in noms:
        assert re.fullmatch(r"[a-z0-9]+(-[a-z0-9]+)*", nom), nom
        # Les scènes du mode démo commencent par leur numéro (capturer.nom_de).
        assert not nom[0].isdigit(), nom


def test_appuis_dans_l_ecran():
    for ecran in ECRANS:
        # Neon Apron s'ouvre depuis le sélecteur en paysage, puis passe en portrait.
        largeur, hauteur = (1280, 1280) if ecran.portrait else (1280, 720)
        points = []
        for etape in _etapes(ecran):
            if isinstance(etape, Toucher):
                points.append((etape.x, etape.y))
            elif isinstance(etape, Glisser):
                points += [(etape.x1, etape.y1), (etape.x2, etape.y2)]
        for x, y in points:
            assert 0 <= x < largeur and 0 <= y < hauteur, (ecran.nom, x, y)


def test_options_du_select_aller_a_l_ecran():
    bloc = _lire("Tab5", "tab5-ha-controls.yaml").split('name: "Aller à l\'écran"', 1)[1]
    options = set(re.findall(r'^\s+- "([^"]+)"', bloc.split("on_value:", 1)[0], re.M))
    assert "Accueil" in options
    for ecran in ECRANS:
        for etape in _etapes(ecran):
            if isinstance(etape, Aller):
                assert etape.option in options, (ecran.nom, etape.option)


def test_actions_connues_de_l_api():
    textes = _lire("Tab5", "tab5-api-logic.yaml") + _lire("Tab5", "rendu", "bouchons.yaml")
    actions = set(re.findall(r"- service: (\w+)", textes))
    assert {"rendu_capture", "rendu_toucher", "rendu_glisser"} <= actions
    for ecran in ECRANS:
        for etape in _etapes(ecran):
            if isinstance(etape, Service):
                assert etape.nom in actions, (ecran.nom, etape.nom)


def test_portraits():
    assert ecrans.PORTRAITS == {e.nom for e in ECRANS if e.portrait}
