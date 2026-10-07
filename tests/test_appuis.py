# -*- coding: utf-8 -*-
"""Appuis longs des trois boutons du haut au choix (07/10/2026, demande d'Axel) : la
section « Boutons du haut » du blueprint « Tab5 — emplacements » choisit l'écran qu'ouvre
l'appui long de la maison, de l'engrenage et de la manette. Les taps ne changent pas.

Une clé de tab5_maj_emplacements, « appuis|maison|engrenage|manette », plutôt qu'une
variable ou une action : un firmware plus ancien ignore une clé inconnue (comme solaire
et climr, ADR-0026). Côté tablette, une seule routine d'ouverture, le script
tab5_ecran_ouvrir, partagée avec le select « Aller à l'écran » (règle 5).

On vérifie :
- les mêmes codes des deux côtés (firmware, sélecteurs du blueprint, liste du modèle) ;
- l'enum Ecran (tab5_custom.h) = les options du select, dans l'ordre, puis l'Arcade ;
- les trois boutons : leur appui long passe par bouton_haut_ecran() et la routine
  unique, leur mini icône existe (même géométrie) et est branchée ; le select aussi passe
  par la routine, la console n'a qu'une ouverture ;
- la mini icône d'un écran choisi = le glyphe de l'en-tête de son popup ;
- au rendu des VRAIS modèles Jinja du blueprint : défauts, choix, valeurs inconnues, et
  quand la clé part."""
import os
import re

import pytest
import yaml

from tests.test_tuiles_blueprint import Passage, _evenement

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAB5 = os.path.join(REPO, "Tab5")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
ZONES_CPP = os.path.join(TAB5, "tab5_zones.cpp")
CUSTOM_H = os.path.join(TAB5, "tab5_custom.h")
LVGL = os.path.join(TAB5, "tab5-lvgl.yaml")
ZONES_YAML = os.path.join(TAB5, "tab5-zones.yaml")
CONTROLES = os.path.join(TAB5, "tab5-ha-controls.yaml")
STYLES = os.path.join(TAB5, "tab5-styles.yaml")
REGLES = os.path.join(REPO, "tools", "check_tab5_code_rules.py")

# Option du select « Aller à l'écran » de chaque valeur d'Ecran, dans l'ordre de l'enum.
OPTIONS = {
    "AUCUN": "—", "ACCUEIL": "Accueil", "ASSISTANT": "Assistant vocal", "CALENDRIER": "Calendrier",
    "REVEIL": "Réveil", "CLIM": "Climatisation", "PLANTES": "Plantes", "TV": "Télécommande TV",
    "CONSOLE": "Console système", "ENERGIE": "Énergie", "REGLAGES": "Réglages", "ALERTES": "Alertes",
}
# Code du blueprint → valeur d'Ecran.
CODES = {
    "rien": "AUCUN", "assistant": "ASSISTANT", "calendrier": "CALENDRIER", "reveil": "REVEIL",
    "clim": "CLIM", "plantes": "PLANTES", "tv": "TV", "console": "CONSOLE", "energie": "ENERGIE",
    "reglages": "REGLAGES", "alertes": "ALERTES", "arcade": "ARCADE",
}
# Bouton (ordre de BoutonHaut) → (widget, mini icône).
BOUTONS = (("BOUTON_MAISON", "btn_control_ha", "icon_mini_ha"),
           ("BOUTON_ENGRENAGE", "btn_control_console", "icon_mini_sys"),
           ("BOUTON_MANETTE", "btn_control_tv", "icon_mini_tv"))
# Popup dont l'en-tête donne le glyphe de la mini icône (modal_header.yaml, `icon`).
EN_TETES = {
    "ASSISTANT": "assistant_popup.yaml", "CALENDRIER": "calendar_popup.yaml", "REVEIL": "alarm_popup.yaml",
    "CLIM": "climate_popup.yaml", "PLANTES": "pots_popup.yaml", "TV": "tv_remote_popup.yaml",
    "ENERGIE": "energie_popup.yaml", "REGLAGES": "reglages_popup.yaml", "ALERTES": "alertes_popup.yaml",
}


def _lire(chemin):
    with open(chemin, encoding="utf-8") as f:
        return f.read()


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
    corps = re.search(rf"enum (?:class )?{nom}\b[^{{]*\{{(.*?)\}};", _lire(CUSTOM_H), re.S).group(1)
    corps = re.sub(r"//[^\n]*", "", corps)
    return re.findall(r"\b([A-Z][A-Z_0-9]*)\b", corps)


def _codes_firmware():
    table = re.search(r"kCodesEcran\[\] = \{(.*?)\};", _lire(ZONES_CPP), re.S).group(1)
    return dict(re.findall(r'\{"(\w+)", Ecran::(\w+)\}', table))


def _bloc_bouton(ident):
    texte = _lire(LVGL)
    debut = texte.index(f"id: {ident}\n")
    fin = texte.find("        - button:", debut)
    fin = texte.find("        # ====", debut) if fin < 0 else fin
    return texte[debut:fin]


# ─── Contrat : codes et écrans ────────────────────────────────────────────────

def test_cle_appuis_des_deux_cotes():
    cle = re.search(r'kCleAppuis\[\] = "(\w+)"', _lire(ZONES_CPP)).group(1)
    assert cle == "appuis"
    assert f"{cle}|" in _lire(BLUEPRINT), "le blueprint ne pousse pas la clé que lit le firmware"


def test_memes_codes_firmware_et_blueprint():
    assert _codes_firmware() == CODES
    bp = yaml.load(_lire(BLUEPRINT).replace("!input", "!!str"), Loader=yaml.SafeLoader)
    section = bp["blueprint"]["input"]["boutons_haut"]
    assert section.get("collapsed") is True
    entrees = section["input"]
    assert list(entrees) == ["appui_maison", "appui_engrenage", "appui_manette"]
    for nom, e in entrees.items():
        assert e["default"] == "auto", nom
        valeurs = [o["value"] for o in e["selector"]["select"]["options"]]
        assert valeurs == ["auto"] + list(CODES), nom
        assert all(" · " in o["label"] or o["value"] == "arcade" for o in e["selector"]["select"]["options"]), nom
    modele = bp["variables"]["appuis"]
    assert re.findall(r"'(\w+)'", modele.split("%}", 1)[0]) == ["auto"] + list(CODES)


def test_ecran_suit_les_options_du_select():
    noms = _enum("Ecran")
    assert noms[-2:] == ["ARCADE", "NB"]
    assert noms[:-2] == list(OPTIONS)
    bloc = _lire(CONTROLES).split("id: tab5_goto_screen", 1)[1].split("on_value:", 1)[0]
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
        appui = bloc.split("on_long_press:", 1)[1].split("widgets:", 1)[0]
        assert f"bouton_haut_ecran({bouton})" in appui, ident
        assert "id(tab5_ecran_ouvrir).execute(e)" in appui, ident
        label = re.search(rf"- label: \{{ id: {mini},[^}}]*\}}", bloc).group(0)
        for attendu in ("hidden: true", "text_font: mdi_font_26", "styles: style_text_dim"):
            assert attendu in label, (mini, attendu)
        g = re.search(r"align: TOP_RIGHT, x: (-?\d+), y: (\d+)", label).groups()
        assert geometrie in (None, g), f"{mini} : pas la géométrie des autres mini icônes"
        geometrie = g
        assert f"u.mini[{bouton}] = id({mini});" in zones


def test_select_par_la_routine_unique():
    texte = _lire(CONTROLES)
    on_value = texte.split("id: tab5_goto_screen", 1)[1].split("on_value:", 1)[1].split("\n  # Langue", 1)[0]
    assert "id: tab5_ecran_ouvrir" in on_value and "switch" not in on_value
    script = texte.split("- id: tab5_ecran_ouvrir", 1)[1]
    assert "ecran: int" in script and "Ecran::ARCADE" in script and "tab5_console_ouvrir" in script
    # Une seule ouverture de la console dans tout le firmware : tab5_console_ouvrir.
    ouvertures = []
    for racine, _, fichiers in os.walk(TAB5):
        for f in fichiers:
            if f.endswith((".yaml", ".cpp", ".h")) and "rendu" not in racine:
                if "lv_obj_remove_flag(id(layer_console_sys)" in _lire(os.path.join(racine, f)):
                    ouvertures.append(f)
    assert ouvertures == ["tab5-scripts.yaml"], ouvertures


def test_mini_glyphes_des_en_tetes():
    corps = _fonction(_lire(ZONES_CPP), "const char* mini_glyphe(")
    glyphes = dict(re.findall(r'case Ecran::(\w+): return "\\U(000F[0-9A-F]{4})"', corps))
    assert set(glyphes) == set(CODES.values()) - {"AUCUN"}
    for ecran, fichier in EN_TETES.items():
        en_tete = re.search(r'modal_header\.yaml, vars: \{ icon: "\\U(000F[0-9A-F]{4})"',
                            _lire(os.path.join(TAB5, "ui_components", fichier))).group(1)
        assert glyphes[ecran] == en_tete, f"{ecran} : pas le glyphe de l'en-tête de {fichier}"
    # Exceptions écrites dans mini_glyphe : la console (son en-tête garde le flocon de la clim)
    # et l'Arcade (sans en-tête : la manette du bouton).
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
