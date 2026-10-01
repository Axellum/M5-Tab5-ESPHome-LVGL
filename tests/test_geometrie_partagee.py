# -*- coding: utf-8 -*-
"""Géométrie écrite deux fois, en YAML et en C++ (le C++ ne lit pas les substitutions
ESPHome) : ce test la tient égale des deux côtés (audit des conteneurs du 01/10/2026).

- Largeur des panneaux de la carte centrale : ${central_w} (Tab5/tab5-ui-tokens.yaml)
  et kLargeurPanneauCentral (Tab5/tab5_central.cpp, réponse vocale longue).
- Colonnes du calendrier : les en-têtes « Lun »…« Dim » de calendar_popup.yaml et les
  cases construites par cal_grid_build() (kCalColX0, kCalColPas, kCalColW dans
  Tab5/tab5_calendar.cpp). Un écart décale les noms des jours de leurs cases.
"""
import os
import re

import yaml

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
JOURS = ["Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"]


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _jetons():
    return yaml.load(_lire("Tab5", "tab5-ui-tokens.yaml"), Loader=_Chargeur)["substitutions"]


def _constante(fichier, nom):
    m = re.search(r"constexpr\s+int32_t\s+%s\s*=\s*(-?\d+)\s*;" % nom, _lire("Tab5", fichier))
    assert m, f"{nom} introuvable dans Tab5/{fichier}"
    return int(m.group(1))


def _widgets(liste):
    for entree in liste or []:
        if not isinstance(entree, dict):
            continue
        for type_, props in entree.items():
            props = props or {}
            yield type_, props
            yield from _widgets(props.get("widgets"))


def test_largeur_des_panneaux_centraux():
    largeur = int(_jetons()["central_w"])
    assert largeur == _constante("tab5_central.cpp", "kLargeurPanneauCentral")
    # Plus de 1180 écrit en clair dans le YAML : tout passe par le jeton.
    for chemin in (("Tab5", "tab5-lvgl.yaml"), ("Tab5", "ui_components", "ha_alert_panel.yaml")):
        texte = _lire(*chemin)
        assert "${central_w}" in texte, "/".join(chemin)
        assert not re.search(r"^\s*width: %d\s*$" % largeur, texte, re.M), "/".join(chemin)


def test_en_tetes_du_calendrier_sur_les_colonnes_des_cases():
    x0 = _constante("tab5_calendar.cpp", "kCalColX0")
    pas = _constante("tab5_calendar.cpp", "kCalColPas")
    w = _constante("tab5_calendar.cpp", "kCalColW")
    popup = yaml.load(_lire("Tab5", "ui_components", "calendar_popup.yaml"), Loader=_Chargeur)
    en_tetes = {props["text"]: props for type_, props in _widgets([popup])
                if type_ == "label" and props.get("text") in JOURS}
    assert sorted(en_tetes, key=JOURS.index) == JOURS
    for c, jour in enumerate(JOURS):
        props = en_tetes[jour]
        assert props["align"] == "TOP_LEFT", jour
        assert props["x"] == x0 + c * pas, f"« {jour} » décalé de sa colonne"
        assert props["width"] == w, f"« {jour} » plus large ou plus étroit que sa case"


def test_grille_du_calendrier_centree_dans_la_carte():
    x0 = _constante("tab5_calendar.cpp", "kCalColX0")
    pas = _constante("tab5_calendar.cpp", "kCalColPas")
    w = _constante("tab5_calendar.cpp", "kCalColW")
    carte = int(_jetons()["modal_card_w"])
    assert pas > w, "les cases se chevauchent"
    droite = carte - (x0 + 6 * pas + w)
    assert droite == x0, f"grille décentrée : {x0} px à gauche, {droite} px à droite"
