# -*- coding: utf-8 -*-
"""Popup Appareil (appareil_popup.yaml) sur chaque carte (lot 4 du portage 1024×600, ADR-0061).

Deux cartes côte à côte (ÉTAT, COMMANDE) dans le corps ; la piste du grand bouton, que
popup_appareil_peindre() remplit à moitié ou en entier (kAppareilBoutonHauteur), et les
largeurs des textes coupés par le C++ suivent la carte. Ce fichier vérifie, carte par
carte, que le YAML et le C++ donnent les mêmes nombres, que tout tient dans le corps sans
se chevaucher, et que le Tab5 garde ses valeurs au pixel."""
import re

import pytest

from tests.commun import CARTES, _jetons as jetons_resolus, constantes_cpp, lire, resoudre, source

POPUP = lire("Tab5", "ui_components", "appareil_popup.yaml").replace("\r\n", "\n")
CPP = lire(source("tab5_tuiles_popups.cpp"))


def _champs(apres: str, jetons: dict, indent: int) -> dict[str, int]:
    """y, width, height, radius (et x) qui suivent `apres`, à cette indentation, résolus."""
    i = POPUP.index(apres) + len(apres)
    champs = {}
    for ligne in POPUP[i:].split("\n")[1:]:
        m = re.fullmatch(" " * indent + r"(x|y|width|height|radius): (.+)", ligne)
        if m:
            champs[m.group(1)] = int(resoudre(m.group(2).strip(), jetons))
        elif not re.match(" " * indent + r"[\w#]", ligne):
            break
    return champs


def _cartes(jetons: dict) -> list[dict[str, int]]:
    """Les cartes ÉTAT et COMMANDE : le `- obj:` qui suit leur commentaire d'en-tête."""
    entetes = (re.search(rf"# ═══ CARTE {nom} .*═══\n          - obj:", POPUP).group(0)
               for nom in ("ÉTAT", "COMMANDE"))
    return [_champs(e, jetons, 14) for e in entetes]


@pytest.mark.parametrize("carte", CARTES)
def test_deux_cartes_dans_le_corps(carte):
    jetons = jetons_resolus(carte)
    corps_w, corps_h = int(jetons["corps_w"]), int(jetons["corps_h"])
    k = constantes_cpp(CPP, carte)
    etat, commande = _cartes(jetons)
    assert etat["x"] == 24 and etat["x"] + etat["width"] < commande["x"]
    assert commande["x"] + commande["width"] == 24 + corps_w
    assert etat["height"] == commande["height"] == corps_h
    assert k["kAppareilTexteLargeur"] == etat["width"] - 60
    assert k["kAppareilActionLargeur"] == commande["width"] - 36
    assert k["kAppareilTitreLargeur"] < int(jetons["modal_card_w"]) - 200
    if carte == "tab5":
        assert (etat["width"], commande["x"], commande["width"]) == (800, 840, 386)
        assert (k["kAppareilTitreLargeur"], k["kAppareilTexteLargeur"], k["kAppareilActionLargeur"]) == (1000, 740, 350)


@pytest.mark.parametrize("carte", CARTES)
def test_piste_et_textes_dans_leur_carte(carte):
    jetons = jetons_resolus(carte)
    k = constantes_cpp(CPP, carte)
    etat, commande = _cartes(jetons)
    pastille = _champs("id: appareil_pastille", jetons, 20)
    piste = _champs("id: appareil_bouton", jetons, 20)
    remplissage = _champs("id: appareil_remplissage", jetons, 26)
    action_y = _champs("id: appareil_action", jetons, 20)["y"]
    etat_y = _champs("id: appareil_etat", jetons, 20)["y"]
    piece_y = _champs("id: appareil_piece", jetons, 20)["y"]
    options_y = _champs("id: appareil_options", jetons, 20)["y"]
    # Pastille ronde, puis l'état (roboto_45_b), la pièce et les options (roboto_32_b, deux lignes).
    assert pastille["width"] == pastille["height"] and pastille["radius"] == pastille["width"] // 2
    assert pastille["y"] + pastille["height"] < etat_y < piece_y < options_y
    lignes = {n: int(jetons[f"ligne_roboto_{n}_b"]) for n in (45, 32)}
    assert etat_y + lignes[45] <= piece_y and piece_y + lignes[32] <= options_y
    assert options_y + 2 * lignes[32] <= etat["height"]
    # La piste : le C++ remplit kAppareilBoutonHauteur ; le texte de l'action dessous.
    assert piste["height"] == k["kAppareilBoutonHauteur"] and piste["radius"] == piste["width"] // 2
    assert remplissage["width"] == piste["width"] and remplissage["y"] + remplissage["height"] <= piste["height"]
    assert piste["width"] <= commande["width"] - 36
    assert piste["y"] + piste["height"] < action_y and action_y + lignes[45] <= commande["height"]
    if carte == "tab5":
        assert pastille == {"y": 84, "width": 150, "height": 150, "radius": 75}
        assert piste == {"y": 64, "width": 180, "height": 380, "radius": 90}
        assert remplissage == {"y": 190, "width": 180, "height": 190, "radius": 90}
        assert (etat_y, piece_y, options_y, action_y) == (256, 346, 406, 480)
