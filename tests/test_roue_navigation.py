# -*- coding: utf-8 -*-
"""Roue de navigation (ADR-0042, 09/10/2026) : l'appui long de la carte centrale ouvre la
roue d'actions rapides (ADR-0036) sur les écrans de la tablette. Aucun compilateur ne
vérifie ce qui suit ; ce fichier lit le C++ et le YAML, comme les autres tests statiques :

- tables : chaque page de la demande d'Axel a son bouton ou son choix, chaque destination
  est une valeur d'Ecran ouverte par la routine unique, aucune famille trop longue ;
- branchements : panneaux de la carte centrale et titre d'une page → la roue, ancre posée
  par tab5-roue.yaml, fichier inclus dans les deux configurations, garde du swipe ;
- géométrie : les deux anneaux tiennent dans l'écran au-dessus de la carte centrale, sans
  chevauchement, pour toutes les tailles que la maison peut donner ;
- rien en NVS, rien vers Home Assistant ; ADR présente.
"""
import math
import re

import ecrans
from tests.commun import lire, source

FICHIER = "tab5_roue_navigation.cpp"


def _nav():
    return lire(source(FICHIER))


def _table(nom):
    m = re.search(rf"constexpr Destination {nom}\[\] = \{{(.*?)\n\}};", _nav(), re.S)
    assert m, nom
    return re.findall(r"\{Ecran::(\w+), RoueIcone::(\w+), tr_noop\(\"([^\"]+)\"\)\}", m.group(1))


def _premier():
    m = re.search(r"constexpr Premier kPremier\[\] = \{(.*?)\n\};", _nav(), re.S)
    assert m
    return [ligne.strip() for ligne in m.group(1).strip().splitlines()]


def _enum(fichier, nom):
    m = re.search(rf"enum class {nom} : uint8_t \{{(.*?)\}};", lire(source(fichier)), re.S)
    assert m, nom
    corps = re.sub(r"//[^\n]*", "", m.group(1))
    return [v.strip() for v in corps.split(",") if v.strip()]


# ─────────────────────────────────────────────────────────────────────────────
# Tables
# ─────────────────────────────────────────────────────────────────────────────


def test_premier_anneau():
    lignes = _premier()
    assert [re.search(r'tr_noop\("([^"]+)"\)', ligne).group(1) for ligne in lignes] == [
        "Alertes", "Pièces", "Appareils", "Agenda", "Tablette", "Assistant"]
    assert lignes[0].startswith("{Genre::ECRAN, RoueIcone::ALERTES") and "Ecran::ALERTES" in lignes[0]
    assert lignes[1].startswith("{Genre::PIECES, RoueIcone::PIECES")
    assert lignes[2].startswith("famille(RoueIcone::APPAREILS") and lignes[2].endswith("kAppareils),")
    assert lignes[3].startswith("famille(RoueIcone::AGENDA") and lignes[3].endswith("kAgenda),")
    assert lignes[4].startswith("famille(RoueIcone::TABLETTE") and lignes[4].endswith("kTablette),")
    assert lignes[5].startswith("{Genre::ECRAN, RoueIcone::ASSISTANT") and "Ecran::ASSISTANT" in lignes[5]
    # Le rendu touche les boutons d'une roue de cette taille, à cette ancre.
    assert ecrans.ROUE_NAVIGATION == len(lignes)


def test_chaque_page_de_la_demande_a_sa_place():
    """« alertes, jeux, discussions, paramètres, pages des pièces, des lumières, des
    clims, des températures, météo, calendrier, réveil, volets » (Axel, 09/10/2026). Les
    pièces : le bouton Pièces ; la météo : l'emplacement réservé en tête de kAgenda pour le
    lot feat/meteo-graphique (Ecran::METEO n'existe pas encore)."""
    ecrans_nav = {e for nom in ("kAppareils", "kAgenda", "kTablette") for e, _, _ in _table(nom)}
    ecrans_nav |= set(re.findall(r"Genre::ECRAN, RoueIcone::\w+, tr_noop\(\"[^\"]+\"\), Ecran::(\w+)", _nav()))
    assert ecrans_nav == {"ALERTES", "ARCADE", "ASSISTANT", "REGLAGES", "LUMIERES", "CLIM", "TEMPERATURE",
                          "CALENDRIER", "REVEIL", "VOLET", "ENERGIE", "PLANTES", "CONSOLE"}
    assert "Genre::PIECES" in _nav()
    assert "{Ecran::METEO, RoueIcone::METEO, tr_noop(\"Météo\")}" in _nav().split("constexpr Destination kAgenda")[0]


def test_destinations_valeurs_d_ecran_et_icones_connues():
    ecran = _enum("tab5_zones.h", "Ecran")
    icones = _enum("tab5_internal.h", "RoueIcone")
    for nom in ("kAppareils", "kAgenda", "kTablette"):
        table = _table(nom)
        assert 2 <= len(table) <= 6, nom  # kRoueChoix
        for e, icone, _ in table:
            assert e in ecran and e not in ("AUCUN", "ACCUEIL", "NB"), (nom, e)
            assert icone in icones, (nom, icone)
    for icone in re.findall(r"RoueIcone::(\w+)", "\n".join(_premier())):
        assert icone in icones, icone


def test_familles_vides_omises_et_ecrans_disponibles():
    nav = _nav()
    # Un écran sans rien à montrer dans cette maison n'est pas proposé ; une famille vide
    # disparaît (les boutons se resserrent) ; Pièces sans pièce occupée aussi.
    assert "if (!ecran_disponible(d.ecran)) continue;" in nav
    assert "if (p.genre == Genre::ECRAN && !ecran_disponible(p.ecran)) continue;" in nav
    assert "if (choix_de(p, c, cible) == 0) continue;" in nav
    assert "return m > 1 ? m : 0;" in nav
    # Les trois écrans nouveaux ont leur règle de disponibilité.
    zones = lire(source("tab5_zones.cpp"))
    for e in ("LUMIERES", "VOLET", "TEMPERATURE"):
        assert f"case Ecran::{e}" in zones, e


def test_un_toucher_ouvre_par_la_routine_unique():
    nav = _nav()
    # Un écran : tab5_ecran_ouvrir (RoueUI::ouvrir_ecran) ; une pièce : le mode HA sur elle.
    assert "g_roue_ui.ouvrir_ecran(cible)" in nav
    assert "tuiles_aller_piece(-1 - cible)" in nav
    assert "lv_obj_clear_flag" not in nav and "lv_obj_add_flag" not in nav
    # Rien en NVS, rien vers Home Assistant (ADR-0025 : la roue ne fait que montrer).
    for interdit in ("global_preferences", "Preferences", "fire_event", "tab5_evenement", "homeassistant"):
        assert interdit not in nav, interdit


# ─────────────────────────────────────────────────────────────────────────────
# Branchements
# ─────────────────────────────────────────────────────────────────────────────


def test_appui_long_de_la_carte_centrale_et_du_titre():
    bouton = lire("Tab5", "ui_components", "central_bouton.yaml")
    assert re.search(r"on_long_press:\s*\n\s*- lambda: 'roue_navigation_ouvrir\(\);'", bouton)
    lvgl = lire("Tab5", "paquets", "tab5-lvgl.yaml")
    titre = lvgl.split("id: btn_page_title_tap", 1)[1].split("\n                    - ", 1)[0]
    assert "roue_navigation_ouvrir();" in titre
    # Plus aucun panneau n'ouvre l'historique directement : il est le premier bouton.
    assert "roue_navigation_ouvrir" in lire(source("tab5_roue.h"))
    # Un appui long au bout d'un swipe n'ouvre rien.
    corps = _nav().split("void roue_navigation_ouvrir() {", 1)[1]
    assert corps.lstrip().startswith("// Un appui long") and "if (ui_appui_glisse()) return;" in corps


def test_ancre_posee_et_fichier_inclus():
    assert "roue.carte_centrale = id(central_card);" in lire("Tab5", "paquets", "tab5-roue.yaml")
    assert "lv_obj_t* carte_centrale" in lire(source("tab5_roue.h"))
    for config in ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"):
        texte = lire(config)
        roue = texte.index("- Tab5/ecran/tab5_roue.cpp")
        assert texte.index(f"- Tab5/ecran/{FICHIER}") > roue, config


def test_geste_roue():
    zones = lire(source("tab5_zones.cpp"))
    assert '{"roue", Ecran::AUCUN, GesteAction::ROUE},' in zones
    navigation = lire("Tab5", "paquets", "tab5-navigation.yaml")
    assert re.search(r"GesteAction::ROUE\)\s*\{\s*roue_navigation_ouvrir\(\);", navigation)


# ─────────────────────────────────────────────────────────────────────────────
# Géométrie
# ─────────────────────────────────────────────────────────────────────────────


def test_deux_anneaux_au_dessus_de_la_carte_centrale():
    """Toutes les tailles que la maison peut donner : 2 à 6 boutons au premier anneau
    (familles vides omises), 2 à 6 choix par famille (Maison et cinq pièces au plus)."""
    xa, ya = ecrans.CARTE_CENTRALE
    r1, r2 = ecrans.ROUE_DIAMETRE // 2, ecrans.ROUE_DIAMETRE2 // 2
    assert not ecrans.roue_dessous(ya)
    for n in range(2, 7):
        premier = ecrans.roue_centres(xa, ya, n)
        for x, y in premier:
            assert ecrans.ROUE_MARGE + r1 <= x <= 1280 - ecrans.ROUE_MARGE - r1, n
            assert ecrans.ROUE_MARGE + r1 <= y < ya, n
        for i in range(n - 1):
            (x1, y1), (x2, y2) = premier[i], premier[i + 1]
            assert math.hypot(x2 - x1, y2 - y1) >= ecrans.ROUE_DIAMETRE, (n, i)
        for famille in range(n):
            for m in range(2, 7):
                second = ecrans.roue_choix_centres(xa, ya, n, famille, m)
                for x, y in second:
                    assert ecrans.ROUE_MARGE + r2 <= x <= 1280 - ecrans.ROUE_MARGE - r2, (n, famille, m)
                    assert ecrans.ROUE_MARGE + r2 <= y, (n, famille, m)
                for (x1, y1) in premier:
                    for (x2, y2) in second:
                        assert math.hypot(x2 - x1, y2 - y1) >= r1 + r2, (n, famille, m)


def test_mots_du_premier_anneau_mesures():
    """Mode « mots » : chaque mot dans l'axe de son bouton, à une distance mesurée sur sa
    largeur (jamais sur le bouton voisin), les mots d'une rangée coupés entre eux."""
    roue = lire(source("tab5_roue.cpp"))
    assert "mot_recul(" in roue and "largeurs_libres(" in roue
    assert "tete.mots = true;" in _nav()


# ─────────────────────────────────────────────────────────────────────────────
# Documentation
# ─────────────────────────────────────────────────────────────────────────────


def test_adr_et_notice():
    adr = lire("docs", "decisions", "0042-navigation-wheel.md")
    assert "roue_navigation_ouvrir" in adr and "0036" in adr
    assert "[0042](0042-navigation-wheel.md)" in lire("docs", "decisions", "README.md")
    assert "roue_navigation_ouvrir" in lire("AGENTS.md")
    notice = lire("docs", "notice", "home.md")
    assert "navigation wheel" in notice and "roue de navigation" in notice
