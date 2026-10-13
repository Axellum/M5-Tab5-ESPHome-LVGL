# -*- coding: utf-8 -*-
"""Popup Caméras (ADR-0049, ADR-0057) : le YAML a autant de puces, de pastilles et de cases
de mosaïque que le C++ en attend.

tab5_cameras.cpp ne nomme ni les puces, ni les pastilles, ni les cases : cameras_ouvrir()
lit les enfants de leur conteneur, dans l'ordre. Un `!include` de moins laisse un pointeur
nul (une puce ou une case jamais montrée), un de plus un widget jamais peint. Les
static_assert du C++ ne comparent que ses constantes entre elles ; ce test relie le YAML.

Sur chaque carte (lot 4 du portage 1024×600, ADR-0061) : l'image se demande à Home Assistant
en huitièmes de 1920 × 1080, la plus grande qui tient dans le corps de la carte avec le nom,
l'heure et les pastilles dessous. Le YAML (cameras_huitiemes, cameras_cadre_l / _h de
tab5-ui-tokens.yaml, cameras_popup.yaml, cameras_vignette.yaml, cameras_puce.yaml) et
tab5_cameras.cpp (kImageHuitiemes, kCadreL, kCaseL…) font le même calcul : les derniers tests
vérifient, carte par carte, que les deux côtés donnent les mêmes nombres, que tout tient dans
la carte et que le Tab5 garde ses valeurs."""
import re

import pytest

from tests.commun import CARTES, _jetons as jetons_resolus, constantes_cpp, lire, resoudre, source


def _constante(nom: str) -> int:
    m = re.search(rf"constexpr int {nom} = (\d+);", source("tab5_cameras.h").read_text(encoding="utf-8"))
    assert m, f"{nom} introuvable dans tab5_cameras.h"
    return int(m.group(1))


POPUP = lire("Tab5", "ui_components", "cameras_popup.yaml").replace("\r\n", "\n")
VIGNETTE = lire("Tab5", "ui_components", "cameras_vignette.yaml").replace("\r\n", "\n")
PUCE = lire("Tab5", "ui_components", "cameras_puce.yaml").replace("\r\n", "\n")
CPP = lire(source("tab5_cameras.cpp"))


def _includes(fichier: str) -> list[int]:
    return [int(n) for n in re.findall(rf"file: {re.escape(fichier)}, vars: \{{ n: (\d+) \}}", POPUP)]


def test_puces():
    assert _includes("cameras_puce.yaml") == list(range(_constante("kCamerasPuces")))


def test_cases_de_la_mosaique():
    assert _includes("cameras_vignette.yaml") == list(range(_constante("kCamerasVignettes")))


def test_pastilles():
    bloc = POPUP.split("id: cameras_pastilles", 1)[1]
    assert len(re.findall(r"^\s+- obj: \{ width: \d+, height: 4,", bloc, re.M)) == _constante("kCamerasPastilles")


def test_une_pastille_par_camera():
    entete = source("tab5_parse.h").read_text(encoding="utf-8")
    m = re.search(r"constexpr int kCamerasMax = (\d+);", entete)
    assert m, "kCamerasMax introuvable dans tab5_parse.h"
    assert int(m.group(1)) == _constante("kCamerasPastilles")


def _widget(ident: str, jetons: dict) -> dict[str, int]:
    """x, y, width, height d'un widget de cameras_popup.yaml (ceux qu'il a), résolus."""
    m = re.search(rf"id: {ident}\n((?:\s+\w+: .+\n)+)", POPUP)
    assert m, f"{ident} introuvable"
    champs = dict(re.findall(r"^\s+(x|y|width|height): (.+)$", m.group(1), re.M))
    return {c: int(resoudre(v.strip(), jetons)) for c, v in champs.items() if v.strip() != "SIZE_CONTENT"}


@pytest.mark.parametrize("carte", CARTES)
def test_image_et_cadre_egaux_au_cpp(carte):
    jetons = jetons_resolus(carte)
    k = constantes_cpp(CPP, carte)
    corps_w, corps_h, carte_l = int(jetons["corps_w"]), int(jetons["corps_h"]), int(jetons["modal_card_w"])
    assert int(jetons["cameras_huitiemes"]) == k["kImageHuitiemes"]
    assert (k["kImageL"], k["kImageH"]) == (240 * k["kImageHuitiemes"], 135 * k["kImageHuitiemes"])
    cadre = _widget("cameras_cadre", jetons)
    assert (cadre["width"], cadre["height"]) == (k["kCadreL"], k["kImageH"] + 4)
    assert cadre["x"] == k["kCadreXCentre"]
    # Le cadre, le nom, l'heure et les pastilles dans le corps.
    assert k["kImageH"] + 58 <= corps_h and cadre["width"] <= corps_w
    nom, heure, pastilles = (_widget(i, jetons) for i in ("cameras_nom", "cameras_heure", "cameras_pastilles"))
    assert nom["y"] == heure["y"] == int(jetons["modal_body_y"]) + cadre["height"] + 10
    assert nom["x"] == cadre["x"] and heure["x"] == -cadre["x"]
    assert nom["width"] + 24 + heure["width"] == cadre["width"], "le nom et l'heure côte à côte"
    # « Touchez une image pour l'agrandir » (339 px en roboto_22, qui ne rétrécit pas) sur une ligne.
    assert heure["width"] >= 339
    assert pastilles["y"] + 4 <= int(jetons["modal_body_y"]) + corps_h
    # La colonne des pièces à gauche du cadre placé à droite, kMarge de chaque côté.
    pieces = _widget("cameras_pieces", jetons)
    assert pieces["x"] + pieces["width"] + 16 == k["kCadreXColonne"]
    assert k["kCadreXColonne"] + k["kCadreL"] + 16 == carte_l
    assert pieces["height"] == cadre["height"]
    if carte == "tab5":
        assert (cadre["x"], cadre["width"], cadre["height"], pieces["width"]) == (143, 964, 544, 238)
        assert (nom["y"], pastilles["y"], nom["width"], heure["width"]) == (626, 666, 580, 360)
        assert (k["kCadreXColonne"], k["kImageL"], k["kImageH"]) == (270, 960, 540)


@pytest.mark.parametrize("carte", CARTES)
def test_cases_de_la_mosaique_egales_au_cpp(carte):
    jetons = jetons_resolus(carte)
    k = constantes_cpp(CPP, carte)
    m = re.search(r"^  width: (.+)\n  height: (.+)\n", VIGNETTE, re.M)
    w, h = (int(resoudre(v.strip(), jetons)) for v in m.groups())
    assert (w, h) == (k["kCaseL"], k["kCaseH"])
    assert 2 * w + k["kCaseEcart"] <= k["kImageL"] and 2 * h + k["kCaseEcart"] <= k["kImageH"]
    assert (k["kVignetteL"], k["kVignetteH"]) == (w + 4, h + 4)
    if carte == "tab5":
        assert (w, h, k["kVignetteL"], k["kVignetteH"]) == (476, 266, 480, 270)


@pytest.mark.parametrize("carte", CARTES)
def test_puce_dans_sa_colonne(carte):
    jetons = jetons_resolus(carte)
    colonne = _widget("cameras_pieces", jetons)["width"]
    m = re.search(r'align: LEFT_MID, x: 16, width: "(.+?)"', PUCE)
    nom_l = int(resoudre(m.group(1), jetons))
    # Le nom (x 16) puis le nombre (40 px, à 16 px du bord droit) : la même place que sur
    # le Tab5, où le nombre, aligné à droite, laisse son début vide.
    assert nom_l == colonne - 70 and nom_l > 0
    if carte == "tab5":
        assert nom_l == 168
