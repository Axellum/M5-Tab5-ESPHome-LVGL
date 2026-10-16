# -*- coding: utf-8 -*-
"""Popup Météo (ADR-0043) sur chaque carte (lot 4 du portage 1024×600, ADR-0061).

Le YAML (meteo_popup.yaml, meteo_carte.yaml) pose les cartes et les trois zones des tracés,
ramenées au corps de la carte ; tab5_meteo.cpp dessine dans ces zones avec leurs hauteurs
(kZoneHeuresH, kZoneJoursH, kZonePluieH) et place ses lignes et ses colonnes. Ce fichier
vérifie, carte par carte, que les deux côtés donnent les mêmes nombres, que tout tient
dans sa zone et dans le corps, et que le Tab5 garde ses valeurs au pixel."""
import re

import pytest

from tests.commun import CARTES, _jetons as jetons_resolus, constantes_cpp, lire, resoudre, source

POPUP = lire("Tab5", "ui_components", "meteo_popup.yaml")
CARTE = lire("Tab5", "ui_components", "meteo_carte.yaml")
CPP = lire(source("tab5_meteo.cpp"))


def _zone(ident: str, jetons: dict) -> tuple[int, int]:
    """(y, hauteur) d'une zone de tracé de meteo_popup.yaml, résolus pour la carte."""
    m = re.search(rf"id: {ident}\s+align: TOP_LEFT\s+x: 18\s+y: (.+)\s+width: .+\s+height: (.+)", POPUP)
    assert m, f"{ident} introuvable"
    return tuple(int(resoudre(v.strip(), jetons)) for v in m.groups())


def _cartes(jetons: dict) -> list[tuple[int, int]]:
    """(y, hauteur) des cartes vitrées d'une page qui commencent à x 24, dans l'ordre."""
    blocs = re.findall(r"x: 24\s+y: (.+)\s+width: \$\{corps_w\}\s+height: (.+)\s+styles: style_glass_card", POPUP)
    return [tuple(int(resoudre(v.strip(), jetons)) for v in b) for b in blocs]


@pytest.mark.parametrize("carte", CARTES)
def test_zones_des_traces_egales_au_cpp(carte):
    jetons = jetons_resolus(carte)
    k = constantes_cpp(CPP, carte)
    corps_h = int(jetons["corps_h"])
    (m_y, m_h), (h_y, h_h), (j_y, j_h), (p_y, p_h) = _cartes(jetons)
    # Aujourd'hui : « Maintenant » puis les heures, 16 px d'écart, jusqu'au bas du corps.
    assert m_y == 0 and h_y == m_h + 16 and h_y + h_h == corps_h
    zy, zh = _zone("meteo_zone_heures", jetons)
    assert zh == k["kZoneHeuresH"] and zy + zh <= h_h
    assert k["kCourbeHaut"] < k["kCourbeBas"] < k["kPluieBase"] - k["kPluieH"]
    assert k["kPluieTexteY"] + 26 <= k["kZoneHeuresH"], "hauteur d'eau sous les barres, dans la zone"
    # 10 jours : une carte du corps entier ; la zone a l'écart icône − ligne de plus.
    assert (j_y, j_h) == (0, corps_h)
    zy, zh = _zone("meteo_zone_jours", jetons)
    assert zh == k["kZoneJoursH"] and zy >= 0 and zy + zh <= corps_h
    assert k["kLignesY0"] + 10 * k["kLigneH"] <= k["kZoneJoursH"]
    assert k["kNomX"] + k["kNomL"] <= k["kJourIconeX"] and k["kJourIconeX"] + k["kJourIconeL"] <= k["kMinX"]
    assert k["kMinX"] + k["kTempL"] <= k["kPisteX"] and k["kPisteX"] + k["kPisteL"] < k["kMaxX"]
    assert k["kMaxX"] + k["kTempL"] <= k["kGraphiqueL"]
    # Détails : la pluie dans l'heure, puis les quatre cartes jusqu'au bas du corps.
    assert p_y == 0
    zy, zh = _zone("meteo_zone_pluie", jetons)
    assert zh == k["kZonePluieH"] and zy + zh <= p_h
    assert k["kPiedY"] - 4 * k["kNiveauH"] >= 34 - 4 and k["kAxeY"] + 26 <= k["kZonePluieH"]
    assert k["kTempsX1"] <= k["kGraphiqueL"]
    if carte == "tab5":
        assert (m_h, h_y, h_h, zh) == (150, 166, 432, 290)
        assert (k["kZoneHeuresH"], k["kPluieBase"], k["kPluieTexteY"], k["kCourbeBas"]) == (404, 350, 356, 236)
        assert (k["kLigneH"], k["kLignesY0"], k["kZoneJoursH"]) == (58, 0, 580)
        assert (k["kMaxX"], k["kPisteL"], k["kTempsX1"], k["kPiedY"], k["kNiveauH"], k["kAxeY"]) == (
            1060, 580, 1150, 226, 48, 244)


@pytest.mark.parametrize("carte", CARTES)
def test_cartes_des_details_dans_le_corps(carte):
    jetons = jetons_resolus(carte)
    corps_w, corps_h = int(jetons["corps_w"]), int(jetons["corps_h"])
    xs = [int(x) for x in re.findall(r'file: meteo_carte\.yaml, vars: \{ n: "\d", x: "(\d+)"', POPUP)]
    assert xs == [24, 329, 633, 938]
    m = re.search(r"  x: (.+)\n  y: (.+)\n  width: (.+)\n  height: (.+)\n", CARTE.replace("\r\n", "\n"))
    y, w, h = (int(resoudre(v.strip(), jetons)) for v in m.groups()[1:])
    places = [int(resoudre(m.group(1).replace("(x | int)", str(x)), jetons)) for x in xs]
    _p_y, p_h = _cartes(jetons)[3]
    assert y == p_h + 16 and y + h == corps_h, "sous la pluie dans l'heure, jusqu'au bas du corps"
    for a, b in zip(places, places[1:]):
        assert a + w < b, "cartes séparées"
    assert places[-1] + w <= 24 + corps_w
    if carte == "tab5":
        assert (places, y, w, h) == ([24, 329, 633, 938], 388, 288, 210)
