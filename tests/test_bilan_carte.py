# -*- coding: utf-8 -*-
"""Bilan d'une autre carte contre le Tab5 (tools/rendu/bilan_carte.py, lot 2 du portage
1024×600, ADR-0061) et lecture de la mesure publiée par le rendu à chaque capture
(capturer.lire_mesure, Tab5/rendu/rendu_mesure.h)."""
import bilan_carte
import capturer
from tests.commun import lire

TAB5 = {
    "1-journee-ensoleillee": {"ecran": "Accueil", "coupes": 3, "hors": 0, "doublon_de": None},
    "accueil-rangee-ligne-2": {"ecran": "Accueil", "coupes": 3, "hors": 0, "doublon_de": None},
    "reglages": {"ecran": "Réglages", "coupes": 1, "hors": 0, "doublon_de": None},
    "volet": {"ecran": "Volet", "coupes": 0, "hors": 0, "doublon_de": None},
    "aller-volet": {"ecran": "Volet", "coupes": 0, "hors": 0, "doublon_de": "volet"},
    "jeu-go": {"ecran": "Jeu · Go", "coupes": 0, "hors": 0, "doublon_de": None},
    "meteo-jours": {"ecran": None, "coupes": None, "hors": None, "doublon_de": None},
}
CARTE = {
    "1-journee-ensoleillee": {"ecran": "Accueil", "coupes": 40, "hors": 30, "doublon_de": None},
    # L'appui n'a rien changé : l'accueil, copie de la scène.
    "accueil-rangee-ligne-2": {"ecran": "Accueil", "coupes": 40, "hors": 30, "doublon_de": "1-journee-ensoleillee"},
    "reglages": {"ecran": "Réglages", "coupes": 12, "hors": 0, "doublon_de": None},
    "volet": {"ecran": "Volet", "coupes": 5, "hors": 1, "doublon_de": None},
    # Copie aussi sur le Tab5 : ouvert.
    "aller-volet": {"ecran": "Volet", "coupes": 5, "hors": 1, "doublon_de": "volet"},
    # Autre écran que sur le Tab5 : pas ouvert.
    "jeu-go": {"ecran": "Arcade", "coupes": 9, "hors": 9, "doublon_de": None},
}


def test_ouverts_par_famille():
    comptes = bilan_carte.compter(TAB5, CARTE)
    # Les scènes et les captures sans mesure ne sont pas attendues.
    assert comptes["total"] == (3, 5)
    assert comptes["popups"] == (3, 3)
    assert comptes["accueil"] == (0, 1)
    assert comptes["jeux"] == (0, 1)


def test_coupes_des_popups_ouverts_et_ecart_avec_un_bilan_precedent():
    assert bilan_carte.coupes_popups(TAB5, CARTE) == [
        ("reglages", "Réglages", 1, 12, 0), ("volet", "Volet", 0, 5, 1), ("aller-volet", "Volet", 0, 5, 1)]
    avant = {**CARTE, "reglages": {**CARTE["reglages"], "coupes": 30, "hors": 7}}
    texte = bilan_carte.bilan(TAB5, CARTE, "Essai", avant)
    assert "**3 / 5** écrans ouverts (avant : 3)" in texte
    assert "**22** objets coupés sur la carte, dont **2** hors de l'écran ; 1 sur le Tab5" in texte
    assert "40 → 22 coupés, 9 → 2 hors écran" in texte
    assert "| jeu-go | Jeu · Go | Arcade |" in texte
    assert "| accueil-rangee-ligne-2 | Accueil | Accueil (= 1-journee-ensoleillee) |" in texte


def test_lecture_de_la_mesure_publiee_par_le_rendu():
    assert capturer.lire_mesure("reglages|Réglages|12|0|2") == (
        "reglages", {"ecran": "Réglages", "coupes": 12, "hors": 0, "barres": 2})
    assert capturer.lire_mesure("jeu-go|Jeu · Go|3|1|0")[1]["ecran"] == "Jeu · Go"
    assert capturer.lire_mesure("") is None
    assert capturer.lire_mesure("a|b|c|d|e") is None
    # L'ancien format, sans les barres, n'est plus celui du rendu.
    assert capturer.lire_mesure("reglages|Réglages|12|0") is None
    # Même format que l'action rendu_capture (Tab5/rendu/bouchons.yaml).
    assert ('"%s|%s|%d|%d|%d", fichier.c_str(), ecran, r.coupes, r.hors, r.barres'
            in lire("Tab5", "rendu", "bouchons.yaml"))


def test_captures_avec_une_barre_de_defilement():
    """Les barres de défilement (18/10/2026) : chaque capture de la carte qui en montre, avec
    le compte du Tab5 pour le même écran ; les consoles de jeu n'y sont pas."""
    tab5 = {**TAB5, "reglages": {**TAB5["reglages"], "barres": 1}, "volet": {**TAB5["volet"], "barres": 0}}
    carte = {**CARTE, "reglages": {**CARTE["reglages"], "barres": 1}, "volet": {**CARTE["volet"], "barres": 2},
             "jeu-go": {**CARTE["jeu-go"], "ecran": "Jeu · Go", "barres": 4},
             "1-journee-ensoleillee": {**CARTE["1-journee-ensoleillee"], "barres": 0}}
    assert bilan_carte.barres(tab5, carte) == [("reglages", "Réglages", 1, 1), ("volet", "Volet", 0, 2)]
    texte = bilan_carte.bilan(tab5, carte, "Essai")
    assert "**2** captures de la carte en montrent au moins une" in texte
    assert "| volet | Volet | 0 | 2 |" in texte
    # Un bilan d'avant les barres n'en parle pas.
    assert "Barres de défilement" not in bilan_carte.bilan(TAB5, CARTE, "Essai")
