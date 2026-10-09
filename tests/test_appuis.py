# -*- coding: utf-8 -*-
"""Appuis longs des trois boutons du haut au choix (07/10/2026, demande d'Axel) : la
section « Boutons du haut » du blueprint « Tab5 — emplacements » choisit l'écran qu'ouvre
l'appui long de la maison, de l'engrenage et de la manette. Les taps ne changent pas.

Une clé de tab5_maj_emplacements, « appuis|maison|engrenage|manette », plutôt qu'une
variable ou une action : un firmware plus ancien ignore une clé inconnue (comme solaire
et climr, ADR-0026). Côté tablette, une seule routine d'ouverture, le script
tab5_ecran_ouvrir, partagée avec le select « Aller à l'écran » (règle 5).

Depuis le 09/10/2026 (lot A, ADR-0039), les taps et l'horloge aussi se choisissent (clé
gestes, tests/test_gestes.py) ; la clé appuis reste poussée et lue (firmware 3.7, vieux
blueprint).

On vérifie :
- les mêmes codes d'écran des deux côtés (firmware, sélecteurs du blueprint) ;
- l'enum Ecran (tab5_zones.h, inclus par tab5_custom.h) = les options du select, dans l'ordre, puis l'Arcade ;
- les trois boutons : leurs gestes passent par le script tab5_geste (geste_bouton()) et
  la routine unique, leur mini icône existe (même géométrie) et est branchée ; le select
  aussi passe par la routine, la console n'a qu'une ouverture ;
- la mini icône d'un écran choisi = le glyphe de l'en-tête de son popup ;
- au rendu des VRAIS modèles Jinja du blueprint : défauts, choix, valeurs inconnues, et
  quand la clé part."""
import os
import re

import pytest
import yaml

from tests.test_tuiles_blueprint import Passage, _evenement
from tests.commun import contrat, lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAB5 = os.path.join(REPO, "Tab5")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
ZONES_CPP = os.path.join(TAB5, "ecran", "tab5_zones.cpp")
LVGL = os.path.join(TAB5, "paquets", "tab5-lvgl.yaml")
ZONES_YAML = os.path.join(TAB5, "paquets", "tab5-zones.yaml")
# Select « Aller à l'écran », tab5_ecran_ouvrir et registre des fenêtres (08/10/2026, YML-2).
NAVIGATION = os.path.join(TAB5, "paquets", "tab5-navigation.yaml")
STYLES = os.path.join(TAB5, "paquets", "tab5-styles.yaml")
REGLES = os.path.join(REPO, "tools", "check_tab5_code_rules.py")

# Option du select « Aller à l'écran » de chaque valeur d'Ecran, dans l'ordre de l'enum.
OPTIONS = {
    "AUCUN": "—", "ACCUEIL": "Accueil", "ASSISTANT": "Assistant vocal", "CALENDRIER": "Calendrier",
    "REVEIL": "Réveil", "CLIM": "Climatisation", "PLANTES": "Plantes", "TV": "Télécommande TV",
    "CONSOLE": "Console système", "ENERGIE": "Énergie", "REGLAGES": "Réglages", "ALERTES": "Alertes",
    "MAISON": "Maison",
    # Roue de navigation (ADR-0042, 09/10/2026) : à la fin aussi.
    "LUMIERES": "Lumières", "VOLET": "Volet", "TEMPERATURE": "Température",
    # Popup Météo (ADR-0043, 09/10/2026) : après eux.
    "METEO": "Météo",
}
# Code du blueprint → valeur d'Ecran.
CODES = {
    "rien": "AUCUN", "assistant": "ASSISTANT", "calendrier": "CALENDRIER", "reveil": "REVEIL",
    "clim": "CLIM", "plantes": "PLANTES", "tv": "TV", "console": "CONSOLE", "energie": "ENERGIE",
    "reglages": "REGLAGES", "alertes": "ALERTES", "arcade": "ARCADE",
    # Ajouté à la fin (07/10/2026) : la NVS garde l'index du code dans kCodesGestes.
    "maison": "MAISON",
}
# Après les écrans, les actions de l'accueil (09/10/2026, lot A ; tests/test_gestes.py).
ACTIONS = ["mode_domo", "appareil_suivant", "rangee_suivante", "ecoute", "nabu_suivant"]
# Puis la roue de navigation (09/10/2026, ADR-0042) : ses trois écrans et la roue elle-même
# (une action : Ecran::AUCUN), à la fin (NVS).
ROUE_CODES = {"lumieres": "LUMIERES", "volet": "VOLET", "temperature": "TEMPERATURE", "roue": "AUCUN"}
# Puis le popup Météo (09/10/2026, ADR-0043), à la fin aussi.
ECRANS_APRES = {"meteo": "METEO"}
# Tous les codes, dans l'ordre de kCodesGestes, avec « auto » en tête : le blueprint.
TOUS = ["auto"] + list(CODES) + ACTIONS + list(ROUE_CODES) + list(ECRANS_APRES)
# Bouton (ordre de BoutonHaut) → (widget, mini icône).
BOUTONS = (("BOUTON_MAISON", "btn_control_ha", "icon_mini_ha"),
           ("BOUTON_ENGRENAGE", "btn_control_console", "icon_mini_sys"),
           ("BOUTON_MANETTE", "btn_control_tv", "icon_mini_tv"))
# Popup dont l'en-tête donne le glyphe de la mini icône (modal_header.yaml, `icon`).
EN_TETES = {
    "ASSISTANT": "assistant_popup.yaml", "CALENDRIER": "calendar_popup.yaml", "REVEIL": "alarm_popup.yaml",
    "CLIM": "climate_popup.yaml", "PLANTES": "pots_popup.yaml", "TV": "tv_remote_popup.yaml",
    "ENERGIE": "energie_popup.yaml", "REGLAGES": "reglages_popup.yaml", "ALERTES": "alertes_popup.yaml",
    "MAISON": "maison_popup.yaml",
    "LUMIERES": "light_popup.yaml", "VOLET": "volet_popup.yaml", "TEMPERATURE": "historique_popup.yaml",
    "METEO": "meteo_popup.yaml",
}


def _fonction(texte, signature):
    """Corps d'une fonction C++ (accolades équilibrées) à partir de sa signature."""
    debut = texte.index(signature)
    i = texte.index("{", debut)
    profondeur = 0
    for j in range(i, len(texte)):
        profondeur += {"{": 1, "}": -1}.get(texte[j], 0)
        if profondeur == 0:
            return texte[i:j + 1]
    raise AssertionError(f"fin de {signature} introuvable")


def _enum(nom):
    corps = re.search(rf"enum (?:class )?{nom}\b[^{{]*\{{(.*?)\}};", contrat(), re.S).group(1)
    corps = re.sub(r"//[^\n]*", "", corps)
    return re.findall(r"\b([A-Z][A-Z_0-9]*)\b", corps)


def _codes_firmware():
    """kCodesGestes : code → écran, dans l'ordre (les actions ont Ecran::AUCUN)."""
    table = re.search(r"kCodesGestes\[\] = \{(.*?)\n\};", _lire(ZONES_CPP), re.S).group(1)
    return dict(re.findall(r'\{"(\w+)", Ecran::(\w+), GesteAction::\w+\}', table))


def _bloc_bouton(ident):
    """Le bouton du haut `ident` : ui_components/bouton_haut.yaml (08/10/2026, audit YML-4)
    déplié avec les vars de son inclusion dans tab5-lvgl.yaml, valeurs écrites comme là-bas."""
    ligne = re.search(rf"^.*file: \.\./ui_components/bouton_haut\.yaml, vars: \{{ id: {ident},.*$", _lire(LVGL), re.M)
    assert ligne, f"{ident} : pas inclus par bouton_haut.yaml"
    valeurs = re.findall(r"(\w+):\s*(\"(?:[^\"\\]|\\.)*\"|'[^']*'|\[[^\]]*\]|[\w.-]+)",
                         ligne.group(0).split("vars:", 1)[1])
    texte = _lire(os.path.join(TAB5, "ui_components", "bouton_haut.yaml"))
    for cle, brut in valeurs:
        texte = texte.replace(f'"${{{cle}}}"', brut).replace(f"${{{cle}}}", brut.strip("\"'"))
    return texte


# ─── Contrat : codes et écrans ────────────────────────────────────────────────

def test_cle_appuis_des_deux_cotes():
    cle = re.search(r'kCleAppuis\[\] = "(\w+)"', _lire(ZONES_CPP)).group(1)
    assert cle == "appuis"
    assert f"{cle}|" in _lire(BLUEPRINT), "le blueprint ne pousse pas la clé que lit le firmware"


def test_memes_codes_firmware_et_blueprint():
    codes = _codes_firmware()
    # Même ORDRE : la NVS garde l'index du code, un code de plus va à la fin.
    assert list(codes)[:len(CODES)] == list(CODES)
    assert {c: codes[c] for c in CODES} == CODES
    assert list(codes)[len(CODES):] == ACTIONS + list(ROUE_CODES) + list(ECRANS_APRES)
    assert {c: codes[c] for c in ROUE_CODES} == ROUE_CODES
    assert {c: codes[c] for c in ECRANS_APRES} == ECRANS_APRES
    bp = yaml.load(_lire(BLUEPRINT).replace("!input", "!!str"), Loader=yaml.SafeLoader)
    section = bp["blueprint"]["input"]["boutons_haut"]
    assert section.get("collapsed") is True
    entrees = section["input"]
    # Les trois entrées d'avant le lot A restent (automatisations déjà enregistrées).
    for nom in ("appui_maison", "appui_engrenage", "appui_manette"):
        e = entrees[nom]
        assert e["default"] == "auto", nom
        valeurs = [o["value"] for o in e["selector"]["select"]["options"]]
        assert valeurs == TOUS, nom
        assert all(" · " in o["label"] or o["value"] == "arcade" for o in e["selector"]["select"]["options"]), nom
    assert bp["variables"]["codes_gestes"] == TOUS
    assert "codes_gestes" in bp["variables"]["appuis"]


def test_ecran_suit_les_options_du_select():
    noms = _enum("Ecran")
    assert noms[-2:] == ["ARCADE", "NB"]
    assert noms[:-2] == list(OPTIONS)
    bloc = _lire(NAVIGATION).split("id: tab5_goto_screen", 1)[1].split("on_value:", 1)[0]
    options = re.findall(r'^\s+- "([^"]+)"', bloc, re.M)
    assert options == list(OPTIONS.values()), "l'index d'une option du select doit rester sa valeur d'Ecran"


def test_auto_comme_avant():
    assert _enum("BoutonHaut") == [b for b, _, _ in BOUTONS] + ["BOUTON_HAUT_NB"]
    auto = re.search(r"kEcranAuto\[BOUTON_HAUT_NB\] = \{(.*?)\};", _lire(ZONES_CPP)).group(1)
    assert re.findall(r"Ecran::(\w+)", auto) == ["ENERGIE", "CONSOLE", "TV"]
    dispo = _fonction(_lire(ZONES_CPP), "bool ecran_disponible(")
    assert "Ecran::ENERGIE && !solaire_present()" in dispo, "Énergie : seulement avec la production solaire"
    sans_zone = _fonction(_lire(ZONES_CPP), "bool ecran_sans_zone(")
    for cas in ("Zone::CLIM", "zones_pots_presents() == 0", "Zone::TV"):
        assert cas in sans_zone


# ─── Firmware : boutons, routine unique, mini icônes ─────────────────────────

def test_trois_boutons_par_la_routine_unique():
    zones = _lire(ZONES_YAML)
    geometrie = None
    for bouton, ident, mini in BOUTONS:
        bloc = _bloc_bouton(ident)
        court = bloc.split("on_short_click:", 1)[1].split("on_long_press:", 1)[0]
        appui = bloc.split("on_long_press:", 1)[1].split("widgets:", 1)[0]
        assert f"id(tab5_geste).execute(geste_bouton({bouton}, false))" in court, ident
        assert f"id(tab5_geste).execute(geste_bouton({bouton}, true))" in appui, ident
        label = re.search(rf"- label: \{{ id: {mini},[^}}]*\}}", bloc).group(0)
        for attendu in ("hidden: true", "text_font: mdi_font_26", "styles: style_text_dim"):
            assert attendu in label, (mini, attendu)
        g = re.search(r"align: TOP_RIGHT, x: (-?\d+), y: (\d+)", label).groups()
        assert geometrie in (None, g), f"{mini} : pas la géométrie des autres mini icônes"
        geometrie = g
        assert f"u.mini[{bouton}] = id({mini});" in zones


def test_select_par_la_routine_unique():
    texte = _lire(NAVIGATION)
    on_value = texte.split("id: tab5_goto_screen", 1)[1].split("on_value:", 1)[1]
    assert "id: tab5_ecran_ouvrir" in on_value and "switch" not in on_value
    script = texte.split("- id: tab5_ecran_ouvrir", 1)[1].split("\ntext_sensor:", 1)[0]
    assert "ecran: int" in script and "Ecran::ARCADE" in script
    # Pas de switch qui recopierait la liste des écrans : l'ouverture d'un écran est
    # celle que lui donne le registre (ModalRegistry::ouvrir), sinon animate_popup_open.
    assert "switch" not in script and "ModalRegistry::ouvrir(target)" in script
    # « Console système » est la page Système des Réglages depuis le 08/10/2026 : la même
    # fenêtre, ouverte sur cette page, ou cette page montrée si elle est déjà ouverte.
    assert 'ModalRegistry::find(e == Ecran::CONSOLE ? "Réglages" : nom)' in script
    assert "id(tab5_reglages_ouvrir).execute(REGLAGES_PAGE_SYSTEME);" in script
    assert "if (e == Ecran::CONSOLE) reglages_afficher_page(REGLAGES_PAGE_SYSTEME);" in script
    assert re.search(r'"Réglages",\s+ModalRegistry::POPUP,\s+'
                     r'\[\] \{ id\(tab5_reglages_ouvrir\)\.execute\(REGLAGES_PAGE_ECRAN\); \}\);', texte)
    # Une seule ouverture des Réglages dans tout le firmware : tab5_reglages_ouvrir ; plus
    # aucune console à part.
    ouvertures, console = [], []
    for racine, _, fichiers in os.walk(TAB5):
        for f in fichiers:
            if f.endswith((".yaml", ".cpp", ".h")) and "rendu" not in racine:
                texte_f = _lire(os.path.join(racine, f))
                if "animate_popup_open(id(reglages_popup))" in texte_f:
                    ouvertures.append(f)
                if "layer_console_sys" in texte_f:
                    console.append(f)
    assert ouvertures == ["tab5-reglages.yaml"], ouvertures
    assert console == [], console


def test_mini_glyphes_des_en_tetes():
    corps = _fonction(_lire(ZONES_CPP), "const char* code_glyphe(")
    glyphes = dict(re.findall(r'case Ecran::(\w+): return "\\U(000F[0-9A-F]{4})"', corps))
    assert set(glyphes) == (set(CODES.values()) | set(ROUE_CODES.values()) | set(ECRANS_APRES.values())) - {"AUCUN"}
    corps += _fonction(_lire(ZONES_CPP), "const char* mini_glyphe(")
    for ecran, fichier in EN_TETES.items():
        en_tete = re.search(r'modal_header\.yaml, vars: \{ icon: "\\U(000F[0-9A-F]{4})"',
                            _lire(os.path.join(TAB5, "ui_components", fichier))).group(1)
        assert glyphes[ecran] == en_tete, f"{ecran} : pas le glyphe de l'en-tête de {fichier}"
    # Exceptions écrites dans mini_glyphe : la console (page Système des Réglages depuis le
    # 08/10/2026, sans en-tête à elle ; avant, le sien gardait le flocon de la clim) et
    # l'Arcade (sans en-tête : la manette du bouton).
    assert glyphes["CONSOLE"] != glyphes["CLIM"]
    assert f'"\\U{glyphes["ARCADE"]}", align: CENTER' in _bloc_bouton("btn_control_tv")
    police = re.search(r"id: mdi_font_26\n(.*?)\n  - ", _lire(STYLES), re.S).group(1)
    for g in set(glyphes.values()) | set(re.findall(r'\\U(000F[0-9A-F]{4})', corps)):
        assert f"\\U{g}" in police, f"glyphe U+{g[3:]} absent de mdi_font_26 (règle 9)"
    assert '("tab5_zones.cpp", "mini_glyphe"): ("icon_mini_*",)' in _lire(REGLES)


def test_cle_lue_avant_la_table_3x():
    corps = _fonction(_lire(ZONES_CPP), "int emplacements_appliquer(")
    assert "appuis_recu(" in corps
    assert corps.index("appuis_recu(") < corps.index("cibles[i].cle"), "la table 3.x ignorerait la clé"


# ─── Blueprint : rendu des vrais modèles ─────────────────────────────────────

def _payload(declencheur, **entrees):
    return Passage(entrees, [], _evenement(declencheur)).variables_du_bloc("payload")


def test_defauts():
    assert "appuis|auto|auto|auto;" in _payload("connexion")


def test_un_choix():
    p = _payload("connexion", appui_maison="calendrier", appui_engrenage="rien", appui_manette="arcade")
    assert "appuis|calendrier|rien|arcade;" in p
    assert p.count("appuis|") == 1


@pytest.mark.parametrize("valeur", ["cuisine", "", None, "AUTO", "Calendrier"])
def test_valeur_inconnue_part_en_auto(valeur):
    p = _payload("connexion", appui_maison=valeur, appui_manette="tv")
    assert "appuis|auto|auto|tv;" in p


@pytest.mark.parametrize("declencheur", ["connexion", "rechargement", "maj_ecran", "demarrage_ha"])
def test_part_avec_tous_les_etats(declencheur):
    assert "appuis|auto|console|auto;" in _payload(declencheur, appui_engrenage="console")


@pytest.mark.parametrize("declencheur", ["zones", "action"])
def test_pas_avec_les_autres_declencheurs(declencheur):
    assert "appuis|" not in _payload(declencheur)


def test_pas_avec_les_mesures():
    p = Passage({}, [], {"id": "mesures", "platform": "time_pattern"})
    assert "appuis|" not in p.variables_du_bloc("payload")
