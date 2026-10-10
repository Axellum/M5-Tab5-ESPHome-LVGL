# -*- coding: utf-8 -*-
"""Zone à gauche de l'horloge au choix (ADR-0051, 10/10/2026, demande d'Axel) : au-dessus du
cadre Ok Nabu, le vocal (micro, Domo, Discu : l'écran d'avant) ou le graphique des heures qui
viennent ; le lecteur audio compact est réservé (lot 2).

Une clé de tab5_maj_emplacements, « gauche|départ|c1|… », et pas une variable de service : le
contrat (contrat/contrat.yaml) ne change pas, un firmware plus ancien ignore la clé. Sa
lecture est du C++ pur (zone_gauche_lire, Tab5/socle/tab5_parse.cpp), testée par
tools/test_parse.cpp et fuzzée par tools/fuzz/fuzz_parse.cpp (tests/test_fuzz_parse.py).

On vérifie :
- les mêmes codes et le même ordre des deux côtés (firmware, blueprint), et que le blueprint
  ne propose que ce que le firmware sait montrer ;
- la clé lue avant la table 3.x, le choix gardé en NVS sous une clé à lui ;
- le vocal regroupé sans changer de place, le graphique dans la colonne, entre le bandeau
  d'état et le cadre Ok Nabu ;
- le tap de la seconde température et le geste « zone_gauche_suivante » ; l'Arcade toujours
  joignable (bouton manette, roue de navigation) ;
- le graphique repeint à l'arrivée des prévisions horaires et au changement de thème ;
- au rendu des VRAIS modèles Jinja du blueprint : défauts, choix, valeurs inconnues, quand la
  clé part."""
import os
import re

import pytest
import yaml

from tests.commun import lire as _lire
from tests.test_appuis import _fonction
from tests.test_tuiles_blueprint import Passage, _evenement

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAB5 = os.path.join(REPO, "Tab5")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
PARSE_H = os.path.join(TAB5, "socle", "tab5_parse.h")
GAUCHE_CPP = os.path.join(TAB5, "ecran", "tab5_zone_gauche.cpp")
ZONES_CPP = os.path.join(TAB5, "ecran", "tab5_zones.cpp")
LVGL = os.path.join(TAB5, "paquets", "tab5-lvgl.yaml")
CLIMAT = os.path.join(TAB5, "ui_components", "climate_card.yaml")

VOCAL = ("icon_mic_status", "btn_assist_trigger", "btn_mode_domo", "btn_mode_discu")


def _bp():
    return yaml.load(_lire(BLUEPRINT).replace("!input", "!!str"), Loader=yaml.SafeLoader)


def _codes_firmware():
    ligne = re.search(r"kZoneGaucheCodes\[[^\]]*\] = \{([^}]*)\};", _lire(PARSE_H)).group(1)
    return re.findall(r'"(\w+)"', ligne)


def _enum_firmware():
    corps = re.search(r"enum class ZoneGauche : uint8_t \{(.*?)\};", _lire(PARSE_H), re.S).group(1)
    return re.findall(r"^\s+([A-Z]+)", corps, re.M)


def _disponibles():
    corps = _fonction(_lire(GAUCHE_CPP), "bool disponible(ZoneGauche z)")
    return re.findall(r"ZoneGauche::(\w+)", corps)


def _bloc_lvgl(ident):
    texte = _lire(LVGL)
    debut = texte.index(f"id: {ident}\n")
    return texte[debut:].split("\n        # ", 1)[0].split("\n        - ", 1)[0]


# ─── Codes ─────────────────────────────────────────────────────────────────────

def test_memes_codes_firmware_et_blueprint():
    codes = _codes_firmware()
    assert codes == ["vocal", "graphique", "lecteur"], "un contenu de plus va à la FIN (NVS)"
    assert [c.lower() for c in _enum_firmware()] == codes + ["nb"]
    bp = _bp()
    assert bp["variables"]["codes_gauche"] == codes
    assert re.search(r'kCleZoneGauche\[\] = "gauche"', _lire(ZONES_CPP))


def test_le_blueprint_ne_propose_que_ce_que_l_ecran_sait_montrer():
    proposes = [z.lower() for z in _disponibles()]
    assert proposes == ["vocal", "graphique"], "lecteur : lot 2 (ADR-0050), sauté en attendant"
    section = _bp()["blueprint"]["input"]["zone_gauche"]
    assert section.get("collapsed") is True
    assert section["name"] == "Zone à gauche de l'horloge · Left of the clock"
    entrees = section["input"]
    assert set(entrees) == {"gauche_depart", "gauche_cycle"}
    for nom, e in entrees.items():
        assert " · " in e["name"], nom
        options = e["selector"]["select"]["options"]
        assert [o["value"] for o in options] == proposes, nom
        assert all(" · " in o["label"] for o in options), nom
    assert entrees["gauche_depart"]["default"] == "vocal", "l'écran d'avant au départ"
    assert entrees["gauche_cycle"]["default"] == proposes
    assert entrees["gauche_cycle"]["selector"]["select"]["multiple"] is True
    # Le défaut du firmware sans la clé est le même que celui du blueprint.
    parse = _lire(PARSE_H)
    assert "kZoneGaucheDefaut = ZoneGauche::VOCAL;" in parse
    assert ("kZoneGaucheCycleDefaut = zone_gauche_bit(ZoneGauche::VOCAL) | zone_gauche_bit(ZoneGauche::GRAPHIQUE);"
            in parse)


# ─── Firmware ──────────────────────────────────────────────────────────────────

def test_cle_lue_avant_la_table_3x():
    corps = _fonction(_lire(ZONES_CPP), "int emplacements_appliquer(")
    assert "zone_gauche_recu(e.reste.p, e.reste.n);" in corps
    assert corps.index("zone_gauche_recu(") < corps.index("cibles[i].cle"), "la table 3.x ignorerait la clé"
    recu = _fonction(_lire(GAUCHE_CPP), "void zone_gauche_recu(")
    assert "zone_gauche_lire(valeur, n)" in recu, "la lecture est celle de tab5_parse.cpp, testée sur PC"


def test_nvs_sous_une_cle_a_elle():
    cpp = _lire(GAUCHE_CPP)
    assert re.search(r"struct Sauvegarde \{\s+uint32_t magic;\s+uint8_t courant;\s+uint8_t defaut;\s+uint8_t cycle;\s+\};",
                     cpp)
    cles = []
    for racine, _, fichiers in os.walk(TAB5):
        for f in fichiers:
            if f.endswith(".cpp"):
                cles += re.findall(r"constexpr uint32_t kPrefKey\w* = (0x[0-9A-Fa-f]+);", _lire(os.path.join(racine, f)))
    assert "0x7A676175" in cles and len(cles) == len(set(cles)), "deux préférences sur la même clé NVS"
    suivante = _fonction(cpp, "void zone_gauche_suivante(")
    assert "sauver();" in suivante, "le contenu affiché survit au redémarrage"


def test_vocal_regroupe_sans_changer_de_place():
    bloc = _bloc_lvgl("zone_vocal")
    for cle, valeur in (("align", "TOP_LEFT"), ("x", "0"), ("y", "0"), ("styles", "style_transparent"),
                        ("clickable", "false")):
        assert re.search(rf"^            {cle}: {valeur}$", bloc, re.M), cle
    hauteur = int(re.search(r"^            height: (\d+)$", bloc, re.M).group(1))
    assert hauteur <= 236, "le conteneur ne doit pas couvrir le cadre Ok Nabu (y 236)"
    enfants = re.findall(r"id: (\w+)", bloc.split("widgets:", 1)[1])
    assert [e for e in enfants if e in VOCAL] == list(VOCAL)
    # Les coordonnées d'écran d'avant (le conteneur est au coin, sans padding ni bordure).
    styles = _lire(TAB5, "paquets", "tab5-styles.yaml")
    transparent = styles.split("- id: style_transparent\n", 1)[1].split("- id:", 1)[0]
    assert "border_width: 0" in transparent and "pad_all: 0" in transparent
    assert "x: 162, y: 78" in bloc
    assert re.search(r"id: btn_mode_domo\n\s+align: TOP_LEFT\n\s+x: 20\n\s+y: 90\n", bloc)
    assert re.search(r"id: btn_mode_discu\n\s+align: TOP_LEFT\n\s+x: 300\n\s+y: 90\n", bloc)


def test_graphique_dans_la_colonne():
    bloc = _bloc_lvgl("zone_graphique")
    g = {k: int(re.search(rf"^            {k}: (\d+)$", bloc, re.M).group(1)) for k in ("x", "y", "width", "height")}
    assert g["x"] == 20 and g["x"] + g["width"] == 425, "la colonne de gauche, 20-425"
    assert g["y"] >= 40, "sous l'encre du bandeau d'état (y 34)"
    nabu = _bloc_lvgl("btn_ok_nabu")
    y_nabu = int(re.search(r"^            y: (\d+)$", nabu, re.M).group(1))
    assert g["y"] + g["height"] <= y_nabu - 8, "au-dessus du cadre Ok Nabu"
    assert "hidden: true" in bloc and "styles: style_clim_btn_page" in bloc and "pad_all: 0" in bloc
    assert "id(tab5_ecran_ouvrir).execute((int) Ecran::METEO);" in bloc and "ui_appui_glisse()" in bloc
    # Pointeurs posés avant la première image, puis le contenu montré.
    zones = _lire(TAB5, "paquets", "tab5-zones.yaml")
    for ligne in ("g.vocal = id(zone_vocal);", "g.graphique = id(zone_graphique);", "zone_gauche_appliquer();"):
        assert ligne in zones, ligne


def test_points_de_la_courbe_vivent_avec_elle():
    cpp = _lire(GAUCHE_CPP)
    assert re.search(r"^lv_point_precise_t s_pts\[", cpp, re.M), "lv_line_set_points() ne copie pas les points"
    assert "lv_line_set_points(s_g.courbe, s_pts," in cpp
    assert "ui_courbe_lisse(" in cpp, "la courbe du popup Météo, une seule source"
    assert "ui_courbe_lisse(xs, ys, n, kLisse, s_courbe_pts)" in _lire(TAB5, "ecran", "tab5_meteo.cpp")


def test_repeint_aux_previsions_et_au_theme():
    forecast = _fonction(_lire(TAB5, "ecran", "tab5_forecast.cpp"), "static void parse_and_update_heures_bulk(")
    assert "zone_gauche_donnees_changees();" in forecast
    theme = _fonction(_lire(TAB5, "ecran", "tab5_theme.cpp"), "void theme_rejouer_ui()")
    assert "zone_gauche_rejouer_theme();" in theme
    # Rien n'est peint caché : marqué « sale », repeint à la prochaine apparition.
    for f in ("void zone_gauche_donnees_changees(", "void zone_gauche_rejouer_theme("):
        corps = _fonction(_lire(GAUCHE_CPP), f)
        assert "s_g.sale = true;" in corps and "if (graphique_visible()) peindre();" in corps, f
    appliquer = _fonction(_lire(GAUCHE_CPP), "void zone_gauche_appliquer(")
    assert "if (s_g.sale) peindre();" in appliquer


def test_module_inclus_partout():
    for racine in ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"):
        texte = _lire(racine)
        assert "- Tab5/ecran/tab5_zone_gauche.h" in texte and "- Tab5/ecran/tab5_zone_gauche.cpp" in texte, racine
    assert '#include "tab5_zone_gauche.h"' in _lire(TAB5, "ecran", "tab5_custom.h")


# ─── Gestes ────────────────────────────────────────────────────────────────────

def test_tap_de_la_seconde_temperature():
    serre = _lire(CLIMAT).split("id: btn_serre_games", 1)[1].split("on_long_press:", 1)[0]
    assert "zone_gauche_suivante();" in serre and "tab5_arcade_open" not in serre
    script = _lire(TAB5, "paquets", "tab5-navigation.yaml").split("- id: tab5_geste", 1)[1].split("\n  - id:", 1)[0]
    branche = script.split("GesteAction::ZONE_GAUCHE_SUIVANTE)", 1)[1].split("}", 1)[0]
    assert "zone_gauche_suivante();" in branche
    # Sans seconde sonde, l'icône à sa place dit ce que fait le tap : celle du geste.
    piece = _lire(TAB5, "ecran", "tab5_piece_climat.cpp")
    assert r'ui_text(u.icon_serre, sans_serre ? "\U000F056C" : "\U000F002D");' in piece
    glyphe = _fonction(_lire(ZONES_CPP), "const char* code_glyphe(")
    assert r'case GesteAction::ZONE_GAUCHE_SUIVANTE: return "\U000F056C";' in glyphe


def test_arcade_toujours_joignable():
    zones = _lire(ZONES_CPP)
    auto = re.search(r"kGestesAuto\[GESTE_NB\] = \{(.*?)\};", zones, re.S).group(1)
    assert re.findall(r'"(\w+)"|(nullptr)', re.sub(r"//[^\n]*", "", auto))[10][0] == "arcade", \
        "le tap du bouton manette (« auto ») ouvre l'Arcade"
    roue = _lire(TAB5, "ecran", "tab5_roue_navigation.cpp")
    assert "{Ecran::ARCADE, RoueIcone::JEUX," in roue, "« Jeux » dans la roue de navigation"
    import ecrans
    assert ecrans.Toucher(*ecrans.BOUTON_TV, apres=1.0) in ecrans._arcade("go")


# ─── Blueprint : rendu des vrais modèles ───────────────────────────────────────

def _payload(declencheur, **entrees):
    return Passage(entrees, [], _evenement(declencheur)).variables_du_bloc("payload")


def test_defauts():
    p = _payload("connexion")
    assert "gauche|vocal|graphique;" in p
    assert p.index("defil|") < p.index("gauche|"), "avec les gestes, après eux"


@pytest.mark.parametrize("depart, cycle, attendu", [
    ("graphique", ["vocal", "graphique"], "gauche|graphique|vocal;"),
    ("graphique", ["graphique"], "gauche|graphique;"),
    ("vocal", [], "gauche|vocal;"),
    ("vocal", "graphique", "gauche|vocal|graphique;"),
    ("radio", ["graphique", "camera"], "gauche|vocal|graphique;"),
    (None, None, "gauche|vocal;"),
    ("vocal", ["lecteur"], "gauche|vocal|lecteur;"),
])
def test_choix(depart, cycle, attendu):
    p = _payload("connexion", gauche_depart=depart, gauche_cycle=cycle)
    assert attendu in p and p.count("gauche|") == 1


@pytest.mark.parametrize("declencheur", ["connexion", "rechargement", "maj_ecran", "demarrage_ha"])
def test_part_avec_tous_les_etats(declencheur):
    assert "gauche|graphique|vocal;" in _payload(declencheur, gauche_depart="graphique")


@pytest.mark.parametrize("declencheur", ["zones", "action"])
def test_pas_avec_les_autres_declencheurs(declencheur):
    assert "gauche|" not in _payload(declencheur)
