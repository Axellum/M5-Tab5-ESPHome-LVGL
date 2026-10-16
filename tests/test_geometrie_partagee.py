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
- Une taille d'écran par carte (ADR-0061) : l'écran et la carte modale du C++ (drapeaux
  -DTAB5_… de chaque carte) égaux aux jetons calculés pour cette carte, pour CHAQUE carte ;
  les défauts de l'en-tête sont ceux du Tab5. Le reste (calendrier, corps des popups à
  cartes) est au pixel pour 1280×720 : vérifié pour le Tab5 seulement.
"""
import os
import re

import yaml
import pytest
from tests.commun import (CARTES, DRAPEAUX_CARTE, ChargeurSansBalises as _Chargeur, geometrie, geometrie_cpp,
                          jeton, lire as _lire, source)

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
                  "kCartesEcart", "kGraphiqueL", "kAxeLibelleL", "kPieces", "kTuiles",
                  "kLecteurCarteL", "kLecteurCarteH")
# Fichiers qui s'en servent : aucun ne doit les redéfinir.
GEOMETRIE_UTILISATEURS = ("tab5_energie.cpp", "tab5_historique.cpp", "tab5_maison.cpp", "tab5_zones.cpp",
                          "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp",
                          "tab5_tuiles_priv.h", "tab5_roue.cpp", "tab5_clim.cpp", "tab5_lecteur.cpp")


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
    carte = jeton("modal_card_w")
    assert pas > w, "les cases se chevauchent"
    droite = carte - (x0 + 6 * pas + w)
    assert droite == x0, f"grille décentrée : {x0} px à gauche, {droite} px à droite"


# ─── Géométrie partagée du C++ (lot L5) ──────────────────────────────────────

def test_geometrie_partagee_egale_aux_jetons_et_aux_popups():
    g = _geometrie()
    assert set(GEOMETRIE_NOMS) <= set(g), set(GEOMETRIE_NOMS) - set(g)
    assert g == _geometrie("tab5"), "défauts de tab5_geometrie.h ≠ Tab5/paquets/geometrie-tab5.yaml"
    # Corps des popups à cartes et zone de leur graphique, comme leur YAML.
    for popup, zone in (("energie_popup.yaml", "energie_zone"), ("historique_popup.yaml", "historique_zone")):
        widgets = list(_widgets([yaml.load(_lire("Tab5", "ui_components", popup), Loader=_Chargeur)]))
        corps = [p for _t, p in widgets if p.get("x") == g["kCorpsX"] and p.get("width") == g["kCorpsW"]]
        assert corps, f"{popup} : pas de corps x {g['kCorpsX']}, largeur {g['kCorpsW']}"
        zones = [p for _t, p in widgets if p.get("id") == zone]
        assert len(zones) == 1 and zones[0].get("width") == g["kGraphiqueL"], f"{popup} : {zone}"
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
