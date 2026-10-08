# -*- coding: utf-8 -*-
"""Géométrie écrite deux fois, en YAML et en C++ (le C++ ne lit pas les substitutions
ESPHome) : ce test la tient égale des deux côtés (audit des conteneurs du 01/10/2026).

- Largeur des panneaux de la carte centrale : ${central_w} (Tab5/paquets/tab5-ui-tokens.yaml)
  et kLargeurPanneauCentral (Tab5/ecran/tab5_central.cpp, réponse vocale longue).
- Colonnes du calendrier : les en-têtes « Lun »…« Dim » de calendar_popup.yaml et les
  cases construites par cal_grid_build() (kCalColX0, kCalColPas, kCalColW dans
  Tab5/ecran/tab5_calendar.cpp). Un écart décale les noms des jours de leurs cases.
- Géométrie partagée du C++ (Tab5/socle/tab5_geometrie.h, lot L5 de l'audit du 07/10/2026) :
  carte modale égale aux jetons, corps des popups Énergie et Température égal à leur
  YAML, et aucune copie locale de ces constantes ni des littéraux 1280 / 1250.
"""
import os
import re

import yaml
from tests.commun import ChargeurSansBalises as _Chargeur, lire as _lire, source

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
JOURS = ["Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"]


def _jetons():
    return yaml.load(_lire("Tab5", "paquets", "tab5-ui-tokens.yaml"), Loader=_Chargeur)["substitutions"]


def _constante(fichier, nom):
    m = re.search(r"constexpr\s+int32_t\s+%s\s*=\s*(-?\d+)\s*;" % nom, _lire(source(fichier)))
    assert m, f"{nom} introuvable dans {fichier}"
    return int(m.group(1))


# Tab5/socle/tab5_geometrie.h : constantes entières, éventuellement calculées des précédentes.
GEOMETRIE = "tab5_geometrie.h"
GEOMETRIE_NOMS = ("kEcranL", "kEcranH", "kCarteL", "kCarteH", "kCorpsY", "kCorpsX", "kCorpsW",
                  "kCartesEcart", "kGraphiqueL", "kAxeLibelleL", "kPieces", "kTuiles")
# Fichiers qui s'en servent : aucun ne doit les redéfinir.
GEOMETRIE_UTILISATEURS = ("tab5_energie.cpp", "tab5_historique.cpp", "tab5_maison.cpp", "tab5_zones.cpp",
                          "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp",
                          "tab5_tuiles_priv.h", "tab5_roue.cpp", "tab5_clim.cpp")


def _geometrie():
    valeurs = {}
    for nom, expr in re.findall(r"constexpr\s+int(?:32_t)?\s+(\w+)\s*=\s*([^;]+);", _lire(source(GEOMETRIE))):
        valeurs[nom] = int(eval(expr, {}, dict(valeurs)))   # expressions du fichier, déjà vérifiées
    return valeurs


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
    # Bouton des panneaux : central_bouton.yaml depuis le 08/10/2026 (audit YML-4).
    for chemin in (("Tab5", "paquets", "tab5-lvgl.yaml"), ("Tab5", "ui_components", "central_bouton.yaml")):
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


# ─── Géométrie partagée du C++ (lot L5) ──────────────────────────────────────

def test_geometrie_partagee_egale_aux_jetons_et_aux_popups():
    g = _geometrie()
    assert set(GEOMETRIE_NOMS) <= set(g), set(GEOMETRIE_NOMS) - set(g)
    j = _jetons()
    assert (g["kCarteL"], g["kCarteH"], g["kCorpsY"]) == (
        int(j["modal_card_w"]), int(j["modal_card_h"]), int(j["modal_body_y"]))
    # La carte est centrée dans l'écran : 15 px de chaque côté (commentaire des jetons).
    assert (g["kEcranL"] - g["kCarteL"]) // 2 == (g["kEcranH"] - g["kCarteH"]) // 2 == 15
    # Corps des popups à cartes et zone de leur graphique, comme leur YAML.
    for popup, zone in (("energie_popup.yaml", "energie_zone"), ("historique_popup.yaml", "historique_zone")):
        widgets = list(_widgets([yaml.load(_lire("Tab5", "ui_components", popup), Loader=_Chargeur)]))
        corps = [p for _t, p in widgets if p.get("x") == g["kCorpsX"] and p.get("width") == g["kCorpsW"]]
        assert corps, f"{popup} : pas de corps x {g['kCorpsX']}, largeur {g['kCorpsW']}"
        zones = [p for _t, p in widgets if p.get("id") == zone]
        assert len(zones) == 1 and zones[0].get("width") == g["kGraphiqueL"], f"{popup} : {zone}"
    # Pièces et tuiles : cinq de chaque, comme le blueprint (ADR-0023).
    assert g["kPieces"] == g["kTuiles"] == 5


def test_aucune_copie_locale_de_la_geometrie():
    for fichier in GEOMETRIE_UTILISATEURS:
        cpp = _lire(source(fichier))
        assert '#include "tab5_geometrie.h"' in cpp, fichier
        for nom in GEOMETRIE_NOMS:
            assert not re.search(r"constexpr\s+\w+\s+%s\s*=" % nom, cpp), f"{nom} redéfini dans {fichier}"
        sans_blocs = re.sub(r"/\*.*?\*/", "", cpp, flags=re.S)
        code = "\n".join(l.split("//", 1)[0] for l in sans_blocs.splitlines())
        for litteral in ("1280", "1250", "1202", "1166"):
            assert not re.search(r"\b%s\b" % litteral, code), f"{litteral} en dur dans {fichier}"
