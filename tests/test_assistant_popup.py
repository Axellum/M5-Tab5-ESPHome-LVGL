# -*- coding: utf-8 -*-
"""Popup Assistant vocal (assistant_popup.yaml) sur chaque carte (lot 4 du portage 1024×600,
ADR-0061).

Les littéraux du Tab5 sont ramenés au corps de la carte : ce fichier vérifie que le panneau
Réglages, les cartes « Votre demande » et « Réponse » et la barre d'action tiennent dans le
corps sans se chevaucher, que les boutons du panneau y tiennent, que l'image de réponse
tient dans sa zone, et que le Tab5 garde ses valeurs au pixel."""
import re

import pytest

from tests.commun import CARTES, _jetons as jetons_resolus, lire, resoudre

POPUP = lire("Tab5", "ui_components", "assistant_popup.yaml").replace("\r\n", "\n")
ASSIST = lire("Tab5", "paquets", "tab5-assist.yaml")


def _bloc(debut: str, jetons: dict, indent: int) -> dict[str, int]:
    """x, y, width, height qui suivent `debut` dans le popup (même indentation), résolus."""
    i = POPUP.index(debut)
    champs = {}
    for ligne in POPUP[i + len(debut):].split("\n")[1:]:
        m = re.fullmatch(" " * indent + r"(x|y|width|height): (.+)", ligne)
        if m:
            champs[m.group(1)] = int(resoudre(m.group(2).strip(), jetons))
        elif not re.match(" " * indent + r"[\w#]", ligne):
            break
    return champs


@pytest.mark.parametrize("carte", CARTES)
def test_colonnes_et_barre_dans_le_corps(carte):
    jetons = jetons_resolus(carte)
    corps_w, corps_h, y0 = int(jetons["corps_w"]), int(jetons["corps_h"]), int(jetons["modal_body_y"])
    carte_h = int(jetons["modal_card_h"])
    panneau = _bloc("id: assist_settings_panel", jetons, 14)
    demande = _bloc("          # --- Votre demande ---\n          - obj:", jetons, 14)
    reponse = _bloc("          # --- Réponse (texte + tableaux + image) ---\n          - obj:", jetons, 14)
    barre = _bloc("          # ═══ BARRE D'ACTION (bas) ═══\n          - obj:", jetons, 14)
    zone = _bloc("id: assist_response_scroll", jetons, 20)
    barre_haut = carte_h + int(jetons["modal_bottom_y"]) - barre["height"]
    # Panneau à gauche, colonne de droite jusqu'au bord du corps, 8 px au-dessus de la barre.
    assert panneau["x"] + panneau["width"] < demande["x"] == reponse["x"]
    assert demande["x"] + demande["width"] == reponse["x"] + reponse["width"] == 24 + corps_w
    assert y0 + panneau["height"] + 8 == barre_haut == reponse["y"] + reponse["height"] + 8
    assert demande["y"] + demande["height"] < reponse["y"] == int(jetons["assist_resp_y"])
    assert barre_haut + barre["height"] <= y0 + corps_h
    assert zone["x"] + zone["width"] == reponse["width"] and zone["y"] + zone["height"] < reponse["height"]
    # L'image de réponse dans la zone défilante.
    m = re.search(r'resize: "(.+)"', ASSIST)
    largeur, hauteur = (int(v) for v in resoudre(m.group(1), jetons).split("x"))
    assert largeur <= zone["width"] - 16
    if carte == "tab5":
        assert (panneau["width"], panneau["height"], demande["x"], demande["width"]) == (380, 506, 420, 806)
        assert (reponse["height"], zone["width"], zone["height"], barre["height"]) == (390, 790, 322, 84)
        assert (largeur, hauteur) == (760, 360)


@pytest.mark.parametrize("carte", CARTES)
def test_boutons_du_panneau_dans_le_panneau(carte):
    jetons = jetons_resolus(carte)
    panneau = _bloc("id: assist_settings_panel", jetons, 14)
    boites = {i: _bloc(f"id: {i}", jetons, 20) for i in (
        "btn_assist_pipe_domo", "btn_assist_pipe_discu", "btn_assist_wake", "btn_assist_size_s",
        "btn_assist_size_l", "btn_assist_mute", "slider_assist_volume")}
    for nom, b in boites.items():
        assert b["x"] + b["width"] <= panneau["width"] - 2, nom
        assert b["y"] + b.get("height", 20) <= panneau["height"] - 2, nom
    # Rangées dans l'ordre, sans chevauchement.
    rangs = ["btn_assist_pipe_domo", "btn_assist_wake", "btn_assist_size_s", "btn_assist_mute"]
    for a, b in zip(rangs, rangs[1:]):
        assert boites[a]["y"] + boites[a]["height"] < boites[b]["y"], (a, b)
    assert boites["btn_assist_pipe_domo"]["x"] + boites["btn_assist_pipe_domo"]["width"] < boites["btn_assist_pipe_discu"]["x"]
    assert boites["btn_assist_mute"]["x"] + boites["btn_assist_mute"]["width"] < boites["slider_assist_volume"]["x"]
    # Le bouton rond du curseur (16 px de rayon) déborde du bout de la piste : il garde de
    # l'air avant le bord du panneau (il le touchait sur le Tab5 et y était coupé sur la 7B,
    # 18/10/2026).
    curseur = boites["slider_assist_volume"]
    assert curseur["x"] + curseur["width"] + 16 <= panneau["width"] - 12, carte
    if carte == "tab5":
        assert boites["btn_assist_mute"] == {"x": 16, "y": 422, "width": 80, "height": 60}
        assert curseur == {"x": 112, "y": 442, "width": 236, "height": 20}
