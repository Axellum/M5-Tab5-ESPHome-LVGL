# -*- coding: utf-8 -*-
"""Popup « Mes Plantes » (pots_popup.yaml, pot_detail_card.yaml) sur chaque carte (lot 4 du
portage 1024×600, ADR-0061).

Cinq cartes sur la largeur du corps : le YAML (pot_carte_pas, pot_carte_l de
tab5-ui-tokens.yaml) et zones_apply_ui() (kPotPas, kPotEcart de tab5_zones.cpp, qui
recentre les cartes présentes) font le même calcul. Dans une carte, les lignes Fertilité,
Lumière, Température et Batterie gardent leur légende en roboto_22, qui ne rétrécit pas :
« Température » (128 px) doit tenir à droite de son icône. Le Tab5 garde ses valeurs."""
import re

import pytest

from tests.commun import CARTES, _jetons as jetons_resolus, constantes_cpp, lire, resoudre, source

POPUP = lire("Tab5", "ui_components", "pots_popup.yaml")
CARTE = lire("Tab5", "ui_components", "pot_detail_card.yaml").replace("\r\n", "\n")
CPP = lire(source("tab5_zones.cpp"))

# Légende la plus longue des quatre lignes en roboto_22 (roboto_700.ttf), mesurée avec
# PIL comme le fait FreeType : « Température » / « Temperature », 128 px.
LEGENDE_MAX = 128
ROBOTO_22_LIGNE = 27   # ceil(1900·22/2048) + ceil(500·22/2048)


def _valeur(motif: str, jetons: dict) -> int:
    m = re.search(motif, CARTE, re.M)
    assert m, motif
    return int(resoudre(m.group(1).strip().strip('"'), jetons))


@pytest.mark.parametrize("carte", CARTES)
def test_cinq_cartes_dans_le_corps_egales_au_cpp(carte):
    jetons = jetons_resolus(carte)
    k = constantes_cpp(CPP, carte)
    corps_w = int(jetons["corps_w"])
    pas, largeur = int(jetons["pot_carte_pas"]), int(jetons["pot_carte_l"])
    assert (pas, pas - largeur) == (k["kPotPas"], k["kPotEcart"])
    assert 5 * pas - k["kPotEcart"] <= corps_w
    places = [int(x) for x in re.findall(r'x_pos: "(\d+)"', POPUP)]
    assert places == [24, 268, 512, 756, 1000]
    xs = [int(resoudre(re.search(r"^  x: (.+)$", CARTE, re.M).group(1).replace("(x_pos | int)", str(x)), jetons))
          for x in places]
    assert xs == [24 + i * pas for i in range(5)]
    assert xs[-1] + largeur <= 24 + corps_w
    if carte == "tab5":
        assert (pas, largeur, xs) == (244, 226, places)


@pytest.mark.parametrize("carte", CARTES)
def test_lignes_dans_la_carte(carte):
    jetons = jetons_resolus(carte)
    corps_h = int(jetons["corps_h"])
    largeur = int(jetons["pot_carte_l"])
    marge, ligne_l, texte_x = (int(jetons[n]) for n in ("pot_ligne_marge", "pot_ligne_l", "pot_texte_x"))
    # Ligne dans la carte (bordure de 2 px), légende à droite de l'icône (x 2, 32 px).
    assert marge + ligne_l <= largeur - 2
    assert texte_x >= 2 + 32 + 4 and texte_x + LEGENDE_MAX <= ligne_l
    ys = [int(resoudre(y, jetons)) for y in re.findall(r"^        y: (.+)$", CARTE, re.M)]
    hauteur = _valeur(r"^        height: (.+)$", jetons)
    assert len(ys) == 4 and ys == sorted(ys)
    assert all(b - a >= hauteur for a, b in zip(ys, ys[1:])) and ys[-1] + hauteur <= corps_h
    # Légende puis valeur, une ligne de roboto_22 chacune, dans la hauteur de la ligne.
    y_legende = _valeur(r'text: "Fertilité", align: TOP_LEFT, x: "\$\{pot_texte_x\}", y: (".+?")', jetons)
    y_valeur = _valeur(r'_ec", text: "--", align: TOP_LEFT, x: "\$\{pot_texte_x\}", y: (".+?")', jetons)
    assert y_legende >= 0 and y_legende + ROBOTO_22_LIGNE <= y_valeur and y_valeur + ROBOTO_22_LIGNE <= hauteur
    if carte == "tab5":
        assert (marge, ligne_l, texte_x, hauteur, y_legende, y_valeur) == (14, 198, 50, 74, 12, 40)
        assert ys == [236, 316, 396, 476]
