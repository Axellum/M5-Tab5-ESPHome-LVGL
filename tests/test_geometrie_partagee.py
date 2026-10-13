# -*- coding: utf-8 -*-
"""Géométrie écrite deux fois, en YAML et en C++ (le C++ ne lit pas les substitutions
ESPHome) : ce test la tient égale des deux côtés (audit des conteneurs du 01/10/2026).

- Largeur des panneaux de la carte centrale : ${central_w} (Tab5/paquets/tab5-ui-tokens.yaml)
  et kPanneauCentralL (tab5_geometrie.h ; kLargeurPanneauCentral de tab5_central.cpp,
  réponse vocale longue).
- Colonnes du calendrier : les en-têtes « Lun »…« Dim » de calendar_popup.yaml (jetons
  cal_col_*) et les cases construites par cal_grid_build() (kCalColX0, kCalColPas, kCalColW
  de tab5_geometrie.h). Un écart décale les noms des jours de leurs cases.
- Géométrie partagée du C++ (Tab5/socle/tab5_geometrie.h, lot L5 de l'audit du 07/10/2026) :
  carte modale égale aux jetons, corps des popups Énergie et Température égal à leur
  YAML, et aucune copie locale de ces constantes ni des littéraux 1280 / 1250.
- Une taille d'écran par carte (ADR-0061) : l'écran et la carte modale du C++ (drapeaux
  -DTAB5_… de chaque carte) égaux aux jetons calculés pour cette carte, pour CHAQUE carte ;
  les défauts de l'en-tête sont ceux du Tab5. Depuis le lot 2 du portage, ce que les popups
  ont en commun (corps, graphique, demi-cartes, onglets d'en-tête, colonnes du calendrier,
  lignes des pièces, panneaux de la carte centrale) suit la carte : égal des deux côtés et
  dans la carte, carte par carte ; sur le Tab5, les valeurs au pixel d'avant.
"""
import os
import re

import yaml
import pytest
from tests.commun import (CARTES, DRAPEAUX_CARTE, ChargeurSansBalises as _Chargeur, _jetons as jetons_resolus,
                          geometrie, geometrie_cpp, jeton, lire as _lire, resoudre, source)

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
JOURS = ["Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"]


def _resolu(valeur, carte="tab5"):
    """Une valeur YAML (littéral ou `${jeton}`), calculée pour la carte."""
    v = resoudre(valeur, jetons_resolus(carte))
    return int(v) if isinstance(v, str) and re.fullmatch(r"-?\d+", v) else v


# Tab5/socle/tab5_geometrie.h : constantes entières, éventuellement calculées des précédentes.
GEOMETRIE = "tab5_geometrie.h"
GEOMETRIE_NOMS = ("kEcranL", "kEcranH", "kCarteL", "kCarteH", "kCorpsY", "kCorpsX", "kCorpsW", "kCorpsH",
                  "kCartesEcart", "kGraphiqueL", "kAxeLibelleL", "kDemiL", "kDemiX", "kPieces", "kTuiles",
                  "kLecteurCarteL", "kLecteurCarteH", "kOngletsFin", "kCalColPas", "kCalColW", "kCalColX0",
                  "kPieceLignePas", "kPieceLigneH", "kCentralCarteL", "kPanneauCentralL",
                  "kCorpsWTab5", "kCorpsHTab5", "kPieceLigneL")
# Le même nombre, jeton YAML (tab5-ui-tokens.yaml) ↔ constante C++, sur chaque carte.
JETONS_CPP = {"corps_w": "kCorpsW", "corps_h": "kCorpsH", "graphique_l": "kGraphiqueL", "demi_w": "kDemiL",
              "demi_x": "kDemiX", "onglets_fin": "kOngletsFin", "cal_col_pas": "kCalColPas",
              "cal_col_w": "kCalColW", "cal_col_x0": "kCalColX0", "piece_ligne_pas": "kPieceLignePas",
              "piece_ligne_h": "kPieceLigneH", "central_carte_w": "kCentralCarteL", "central_w": "kPanneauCentralL",
              "piece_ligne_l": "kPieceLigneL"}
# Valeurs du Tab5 d'avant le lot 2 : elles ne bougent pas.
TAB5_AVANT = {"kCorpsW": 1202, "kCorpsH": 598, "kGraphiqueL": 1166, "kDemiL": 589, "kDemiX": 637,
              "kOngletsFin": 1148, "kCalColPas": 172, "kCalColW": 168, "kCalColX0": 25, "kPieceLignePas": 92,
              "kPieceLigneH": 84, "kCentralCarteL": 1240, "kPanneauCentralL": 1180,
              "kPieceLigneL": 342}
# Fichiers qui s'en servent : aucun ne doit les redéfinir.
GEOMETRIE_UTILISATEURS = ("tab5_energie.cpp", "tab5_historique.cpp", "tab5_maison.cpp", "tab5_zones.cpp",
                          "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp",
                          "tab5_tuiles_priv.h", "tab5_roue.cpp", "tab5_clim.cpp", "tab5_lecteur.cpp",
                          "tab5_calendar.cpp", "tab5_central.cpp", "tab5_pages.cpp")


def _geometrie(carte=None):
    """Constantes de tab5_geometrie.h : défauts de l'en-tête, ou drapeaux de la carte."""
    return geometrie_cpp(carte)


def _widgets(liste):
    for entree in liste or []:
        if not isinstance(entree, dict):
            continue
        for type_, props in entree.items():
            props = props or {}
            yield type_, props
            yield from _widgets(props.get("widgets"))


@pytest.mark.parametrize("carte", CARTES)
def test_jetons_communs_egaux_au_cpp_et_dans_la_carte(carte):
    g = _geometrie(carte)
    for nom, cpp in JETONS_CPP.items():
        assert jeton(nom, carte) == g[cpp], (carte, nom, cpp)
    if carte == "tab5":
        assert {k: g[k] for k in TAB5_AVANT} == TAB5_AVANT, "Tab5 : valeurs d'avant le lot 2"
    # Corps sous la barre de titre, dans la carte ; deux demi-cartes côte à côte.
    assert g["kCorpsX"] + g["kCorpsW"] + g["kCorpsX"] == g["kCarteL"]
    assert g["kCorpsY"] + g["kCorpsH"] < g["kCarteH"]
    assert g["kDemiX"] + g["kDemiL"] <= g["kCorpsX"] + g["kCorpsW"]
    # Lignes d'une pièce : les cinq au-dessus des boutons de la pièce (88 px du bas du corps).
    assert 50 + 4 * g["kPieceLignePas"] + g["kPieceLigneH"] <= g["kCorpsH"] - 88
    assert g["kPieceLigneH"] >= 56, "une ligne garde sa pastille et ses deux textes"
    # Carte centrale : 20 px des bords de l'écran.
    assert g["kCentralCarteL"] + 40 == g["kEcranL"]


@pytest.mark.parametrize("carte", CARTES)
def test_mise_en_page_des_popups_ramenee_au_corps(carte):
    """Lot 4 du portage : un littéral de la mise en page d'un popup (dessinée pour le corps du
    Tab5, 1202 × 598) est ramené au corps de la carte, en YAML par l'expression Jinja
    `${ v * (corps_w | int) // 1202 }` et en C++ par largeur_carte() / hauteur_carte() :
    les deux donnent le même nombre sur chaque carte, et sur le Tab5 le nombre lui-même."""
    g = _geometrie(carte)
    assert (g["kCorpsWTab5"], g["kCorpsHTab5"]) == (1202, 598)
    entete = _lire(source(GEOMETRIE))
    assert "v * kCorpsW / kCorpsWTab5" in entete and "v * kCorpsH / kCorpsHTab5" in entete
    jetons = jetons_resolus(carte)
    for v in (0, 1, 22, 56, 386, 456, 598, 1202):
        l = v * g["kCorpsW"] // g["kCorpsWTab5"]
        h = v * g["kCorpsH"] // g["kCorpsHTab5"]
        assert resoudre(f"${{ {v} * (corps_w | int) // 1202 }}", jetons) == l
        assert resoudre(f"${{ {v} * (corps_h | int) // 598 }}", jetons) == h
        assert resoudre(f"${{ 0 - {v} * (corps_h | int) // 598 }}", jetons) == -h
        if carte == "tab5":
            assert l == h == v
        assert l <= v and h <= v, "jamais plus grand que sur le Tab5"


def test_largeur_des_panneaux_centraux():
    assert "kLargeurPanneauCentral = kPanneauCentralL;" in _lire(source("tab5_central.cpp"))
    # Plus de 1180 écrit en clair dans le YAML : tout passe par le jeton.
    # Bouton des panneaux : central_bouton.yaml depuis le 08/10/2026 (audit YML-4).
    for chemin in (("Tab5", "paquets", "tab5-lvgl.yaml"), ("Tab5", "ui_components", "central_bouton.yaml")):
        texte = _lire(*chemin)
        assert "${central_w}" in texte, "/".join(chemin)
        assert not re.search(r"^\s*width: (1180|1240)\s*$", texte, re.M), "/".join(chemin)


@pytest.mark.parametrize("carte", CARTES)
def test_en_tetes_du_calendrier_sur_les_colonnes_des_cases(carte):
    g = _geometrie(carte)
    x0, pas, w = g["kCalColX0"], g["kCalColPas"], g["kCalColW"]
    popup = yaml.load(_lire("Tab5", "ui_components", "calendar_popup.yaml"), Loader=_Chargeur)
    en_tetes = {props["text"]: props for type_, props in _widgets([popup])
                if type_ == "label" and props.get("text") in JOURS}
    assert sorted(en_tetes, key=JOURS.index) == JOURS
    for c, jour in enumerate(JOURS):
        props = en_tetes[jour]
        assert props["align"] == "TOP_LEFT", jour
        assert _resolu(props["x"], carte) == x0 + c * pas, f"« {jour} » décalé de sa colonne ({carte})"
        assert _resolu(props["width"], carte) == w, f"« {jour} » plus large ou plus étroit que sa case ({carte})"


@pytest.mark.parametrize("carte", CARTES)
def test_grille_du_calendrier_centree_dans_la_carte(carte):
    g = _geometrie(carte)
    x0, pas, w = g["kCalColX0"], g["kCalColPas"], g["kCalColW"]
    assert pas > w, "les cases se chevauchent"
    droite = g["kCarteL"] - (x0 + 6 * pas + w)
    assert abs(droite - x0) <= 1, f"grille décentrée : {x0} px à gauche, {droite} px à droite ({carte})"
    if carte == "tab5":
        assert droite == x0


# ─── Géométrie partagée du C++ (lot L5) ──────────────────────────────────────

def test_geometrie_partagee_egale_aux_jetons_et_aux_popups():
    g = _geometrie()
    assert set(GEOMETRIE_NOMS) <= set(g), set(GEOMETRIE_NOMS) - set(g)
    assert g == _geometrie("tab5"), "défauts de tab5_geometrie.h ≠ Tab5/paquets/geometrie-tab5.yaml"
    # Corps des popups à cartes et zone de leur graphique, comme leur YAML.
    for popup, zone in (("energie_popup.yaml", "energie_zone"), ("historique_popup.yaml", "historique_zone")):
        widgets = list(_widgets([yaml.load(_lire("Tab5", "ui_components", popup), Loader=_Chargeur)]))
        corps = [p for _t, p in widgets if p.get("x") == g["kCorpsX"] and _resolu(p.get("width")) == g["kCorpsW"]]
        assert corps, f"{popup} : pas de corps x {g['kCorpsX']}, largeur {g['kCorpsW']}"
        zones = [p for _t, p in widgets if p.get("id") == zone]
        assert len(zones) == 1 and _resolu(zones[0].get("width")) == g["kGraphiqueL"], f"{popup} : {zone}"
    # Carte du popup Musique (ADR-0050) : égale à ses jetons, centrée, plus petite que la
    # carte modale, et le popup la prend par ses jetons.
    assert (g["kLecteurCarteL"], g["kLecteurCarteH"]) == (jeton("lecteur_card_w"), jeton("lecteur_card_h"))
    assert g["kLecteurCarteL"] < g["kCarteL"] and g["kLecteurCarteH"] < g["kCarteH"]
    popup = _lire("Tab5", "ui_components", "lecteur_popup.yaml")
    assert "width: ${lecteur_card_w}" in popup and "height: ${lecteur_card_h}" in popup
    # Pièces et tuiles : cinq de chaque, comme le blueprint (ADR-0023).
    assert g["kPieces"] == g["kTuiles"] == 5


@pytest.mark.parametrize("carte", CARTES)
def test_ecran_et_carte_modale_egaux_aux_jetons_de_chaque_carte(carte):
    g = _geometrie(carte)
    assert (g["kEcranL"], g["kEcranH"]) == (int(geometrie(carte)["carte_ecran_l"]), int(geometrie(carte)["carte_ecran_h"]))
    assert (g["kCarteL"], g["kCarteH"], g["kCorpsY"]) == (
        jeton("modal_card_w", carte), jeton("modal_card_h", carte), jeton("modal_body_y", carte))
    # La carte est centrée dans l'écran : 15 px de chaque côté (commentaire des jetons).
    assert (g["kEcranL"] - g["kCarteL"]) // 2 == (g["kEcranH"] - g["kCarteH"]) // 2 == 15
    # La grille du calendrier suit la hauteur de la carte.
    assert jeton("cal_grid_h", carte) == jeton("modal_card_h", carte) - 162


def test_drapeaux_de_la_carte_dans_les_deux_configurations():
    """Le C++ ne voit la carte que par ses -DTAB5_… : les deux configurations (tablette et
    rendu hors tablette) les passent, chacun égal à sa substitution `carte_*`."""
    for config in ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"):
        texte = _lire(config)
        for drapeau, substitution in DRAPEAUX_CARTE.items():
            assert texte.count(f"-D{drapeau}=") == 1 and f"- -D{drapeau}=${{{substitution}}}" in texte, (config, drapeau)
    entete = _lire(source(GEOMETRIE))
    for drapeau in DRAPEAUX_CARTE:
        assert re.search(rf"#ifndef {drapeau}\s+#define {drapeau} -?\d+\s+#endif", entete), drapeau


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
