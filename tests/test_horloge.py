# -*- coding: utf-8 -*-
"""Horloge à rouleau : sa géométrie est écrite dans Tab5/tab5-lvgl.yaml seulement
(01/10/2026). Avant, layout_clock_roller() la recalculait en C++ 2 s après le boot, et
les cadres provisoires du YAML (105 px) coupaient le bas des chiffres pendant ~1,5 s.

Ce test refait le calcul depuis les métriques de la police et vérifie les valeurs du
YAML : chaque cadre contient toute l'encre des chiffres (sinon l'horloge est coupée),
il a la largeur d'un chiffre, HH:MM est centré dans la tuile et le « : » est à la
même hauteur que les chiffres. Changer la taille de roboto_130_b ou une valeur de la
tuile sans refaire le calcul le fait échouer.

Métriques de Roboto 700 (Google Fonts, version 3.015), en unités de police, relevées
avec fontTools le 01/10/2026 dans le fichier que télécharge ESPHome. Arrondis de
FreeType, celui d'ESPHome : ascendante au pixel supérieur, haut de l'encre au pixel
supérieur, bas de l'encre au pixel inférieur, avance au plus proche. Contrôlé sur la
tablette : son journal de démarrage donnait ligne de base 121, avance 75 et 37.
"""
import math
import os

import yaml

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

UNITES_EM = 2048
ASCENDANTE = 1900          # hhea.ascent
AVANCE_CHIFFRE = 1175      # même avance pour 0-9 (chiffres tabulaires)
AVANCE_DEUX_POINTS = 577
ENCRE_HAUT = 1477          # yMax le plus haut des chiffres (2, 3, 8, 9)
ENCRE_BAS = -20            # yMin le plus bas des chiffres (0, 3, 5, 6, 8)
MARGE_MIN = 2              # px d'air exigés entre l'encre et le bord du cadre


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _charger(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return yaml.load(f, Loader=_Chargeur)


def _widgets(liste):
    """Parcourt l'arbre : (type, propriétés) de chaque widget."""
    for entree in liste or []:
        if not isinstance(entree, dict):
            continue
        for type_, props in entree.items():
            props = props or {}
            yield type_, props
            yield from _widgets(props.get("widgets"))


def _tuile():
    page = _charger("Tab5", "tab5-lvgl.yaml")["lvgl"]["pages"][0]
    for _, props in _widgets(page["widgets"]):
        if props.get("id") == "clock_tile":
            return props
    raise AssertionError("clock_tile introuvable dans tab5-lvgl.yaml")


def _styles():
    lvgl = _charger("Tab5", "tab5-styles.yaml")
    polices = {p["id"]: p for p in lvgl["font"]}
    styles = {s["id"]: s for s in lvgl["lvgl"]["style_definitions"]}
    return polices, styles


def _px():
    polices, _ = _styles()
    taille = polices["roboto_130_b"]["size"]
    e = taille / UNITES_EM
    asc = math.ceil(ASCENDANTE * e)
    return {
        "chiffre": round(AVANCE_CHIFFRE * e),
        "deux_points": round(AVANCE_DEUX_POINTS * e),
        "encre_haut": asc - math.ceil(ENCRE_HAUT * e),   # depuis le haut du label
        "encre_bas": asc - math.floor(ENCRE_BAS * e),
    }


def _enfants(tuile):
    rouleaux, deux_points, date = {}, None, None
    for entree in tuile["widgets"]:
        (type_, props), = entree.items()
        if type_ == "obj" and str(props.get("id", "")).startswith("clock_roll_"):
            rouleaux[props["id"]] = props
        elif props.get("id") == "lbl_time_colon":
            deux_points = props
        elif props.get("id") == "lbl_date":
            date = props
    assert list(rouleaux) == ["clock_roll_h10", "clock_roll_h1", "clock_roll_m10", "clock_roll_m1"]
    assert deux_points and date
    return list(rouleaux.values()), deux_points, date


def test_police_de_l_horloge():
    tuile = _tuile()
    rouleaux, deux_points, _ = _enfants(tuile)
    labels = [l["label"] for r in rouleaux for l in r["widgets"]]
    assert len(labels) == 8
    assert all(l["text_font"] == "roboto_130_b" for l in labels + [deux_points])


def test_chaque_cadre_contient_l_encre_des_chiffres():
    px = _px()
    for r in _enfants(_tuile())[0]:
        assert r["align"] == "TOP_LEFT" and r["styles"] == "style_transparent"
        assert r["scrollable"] is False
        assert r["width"] == px["chiffre"], f"{r['id']} : largeur ≠ avance d'un chiffre"
        ys = {l["label"]["y"] for l in r["widgets"]}
        assert len(ys) == 1, f"{r['id']} : les deux labels du rouleau à des hauteurs différentes"
        y_label = ys.pop()
        haut = y_label + px["encre_haut"]
        bas = y_label + px["encre_bas"]
        assert haut >= MARGE_MIN, f"{r['id']} : le haut des chiffres est coupé ({haut} px)"
        assert r["height"] - bas >= MARGE_MIN, (
            f"{r['id']} : le bas des chiffres est coupé (encre jusqu'à {bas} px, cadre de {r['height']} px)")
        assert all(l["label"]["align"] == "TOP_MID" for l in r["widgets"])


def test_cadres_alignes_et_au_dessus_de_la_date():
    rouleaux, _, date = _enfants(_tuile())
    assert len({(r["y"], r["height"]) for r in rouleaux}) == 1
    assert rouleaux[0]["y"] + rouleaux[0]["height"] <= date["y"], "les cadres empiètent sur la date"


def test_hhmm_centre_dans_la_tuile():
    px = _px()
    tuile = _tuile()
    rouleaux, deux_points, _ = _enfants(tuile)
    _, styles = _styles()
    bordure = styles[tuile["styles"]]["border_width"]
    assert styles[tuile["styles"]]["pad_all"] == 0
    largeur_utile = tuile["width"] - 2 * bordure
    x = [r["x"] for r in rouleaux]
    w = px["chiffre"]
    assert x[1] == x[0] + w and x[3] == x[2] + w, "les deux chiffres d'un groupe doivent se toucher"
    avant = deux_points["x"] - (x[1] + w)
    apres = x[2] - (deux_points["x"] + px["deux_points"])
    assert avant == apres >= 0, f"« : » décentré entre les heures et les minutes ({avant} / {apres} px)"
    gauche, droite = x[0], largeur_utile - (x[3] + w)
    assert abs(gauche - droite) <= 1, f"HH:MM décentré dans la tuile ({gauche} / {droite} px)"


def test_deux_points_a_la_hauteur_des_chiffres():
    rouleaux, deux_points, _ = _enfants(_tuile())
    y_label = rouleaux[0]["widgets"][0]["label"]["y"]
    assert deux_points["y"] == rouleaux[0]["y"] + y_label
