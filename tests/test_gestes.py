# -*- coding: utf-8 -*-
"""Gestes de l'accueil au choix (09/10/2026, lot A, ADR-0039, demande d'Axel) : l'horloge en
trois zones (heures, minutes, date) et les trois boutons du haut, un tap court et un appui
long chacun, choisis dans la section « Horloge et boutons du haut » du blueprint
« Tab5 — emplacements ».

Une clé de tab5_maj_emplacements, « gestes|c1|…|c12 », et pas une variable de service : le
contrat (contrat/contrat.yaml) ne change pas, un firmware plus ancien ignore la clé. La clé
« appuis » d'avant (appuis longs des boutons, tests/test_appuis.py) reste poussée et lue.

On vérifie :
- l'ordre des 12 champs : enum Geste (tab5_zones.h) = gestes_choix du blueprint = entrées
  de la section ;
- les codes : kCodesGestes (index gardé en NVS, les actions à la fin) = codes_gestes du
  blueprint = options de chaque entrée ; « auto » de chaque geste = le comportement d'avant ;
- chaque action a sa branche dans le script tab5_geste et un glyphe dans les deux polices ;
- les trois zones de l'horloge couvrent la tuile sans trou ni recouvrement, coupées entre
  les chiffres et la date dans les 21 thèmes ; le rendu les touche ;
- la NVS : nouvelle préférence, taille de SauvegardeAppuis inchangée ;
- l'icône de la clim, à gauche de sa consigne restée au centre, tient avant − dans les 21 thèmes ;
- au rendu des VRAIS modèles Jinja du blueprint : défauts, choix, valeurs inconnues, quand
  la clé part."""
import os
import re

import pytest
import yaml

from tests.commun import lire as _lire
from tests.test_appuis import ACTIONS, CODES, ROUE_CODES, _codes_firmware, _fonction
from tests.test_tuiles_blueprint import Passage, _evenement

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAB5 = os.path.join(REPO, "Tab5")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
ZONES_CPP = os.path.join(TAB5, "ecran", "tab5_zones.cpp")
ZONES_H = os.path.join(TAB5, "ecran", "tab5_zones.h")
LVGL = os.path.join(TAB5, "paquets", "tab5-lvgl.yaml")
NAVIGATION = os.path.join(TAB5, "paquets", "tab5-navigation.yaml")
STYLES = os.path.join(TAB5, "paquets", "tab5-styles.yaml")
CLIM_CARTE = os.path.join(TAB5, "ui_components", "climate_card.yaml")
THEME_CPP = os.path.join(TAB5, "ecran", "tab5_theme.cpp")

# Ordre des 12 champs de la clé (enum Geste) → entrée du blueprint, et « auto » attendu :
# le comportement d'avant le lot A (None : appui long d'un bouton, qui suit la clé appuis),
# sauf le tap des heures, la ligne suivante du panneau Ok Nabu depuis le lot 3 (ADR-0041 ;
# avec le panneau d'origine, l'écoute seule, il ne fait toujours rien).
GESTES = (
    ("GESTE_HEURES_COURT", "geste_heures_court", "nabu_suivant"),
    ("GESTE_HEURES_LONG", "geste_heures_long", "reveil"),
    ("GESTE_MINUTES_COURT", "geste_minutes_court", "appareil_suivant"),
    ("GESTE_MINUTES_LONG", "geste_minutes_long", "reveil"),
    ("GESTE_DATE_COURT", "geste_date_court", "rangee_suivante"),
    ("GESTE_DATE_LONG", "geste_date_long", "calendrier"),
    ("GESTE_MAISON_COURT", "geste_maison_court", "mode_domo"),
    ("GESTE_MAISON_LONG", "appui_maison", None),
    ("GESTE_ENGRENAGE_COURT", "geste_engrenage_court", "reglages"),
    ("GESTE_ENGRENAGE_LONG", "appui_engrenage", None),
    ("GESTE_MANETTE_COURT", "geste_manette_court", "arcade"),
    ("GESTE_MANETTE_LONG", "appui_manette", None),
)
# Action (GesteAction) de chaque code d'action, et ce que sa branche de tab5_geste appelle.
BRANCHES = {
    "MODE_DOMO": "tuiles_mode_ha(!g_central_ctx.ha_mode);",
    "APPAREIL_SUIVANT": "reglables_suivant();",
    "RANGEE_SUIVANTE": "rangee_toucher();",
    "ECOUTE": "id(tab5_wake_word_active).toggle();",
    "NABU_SUIVANTE": "nabu_suivant();",
    "ROUE": "roue_navigation_ouvrir();",
}


def _bp():
    return yaml.load(_lire(BLUEPRINT).replace("!input", "!!str"), Loader=yaml.SafeLoader)


def _enum_geste():
    corps = re.search(r"enum Geste : uint8_t \{(.*?)\};", _lire(ZONES_H), re.S).group(1)
    corps = re.sub(r"//[^\n]*", "", corps)
    return re.findall(r"\b(GESTE_[A-Z_]+)\b", corps)


def _actions_firmware():
    table = re.search(r"kCodesGestes\[\] = \{(.*?)\n\};", _lire(ZONES_CPP), re.S).group(1)
    return dict(re.findall(r'\{"(\w+)", Ecran::\w+, GesteAction::(\w+)\}', table))


# ─── Contrat : ordre des champs et codes ──────────────────────────────────────

def test_ordre_des_douze_champs():
    assert _enum_geste() == [g for g, _, _ in GESTES] + ["GESTE_NB"]
    bp = _bp()
    entrees = bp["blueprint"]["input"]["boutons_haut"]["input"]
    assert set(entrees) == {e for _, e, _ in GESTES}
    # gestes_choix : les !input dans l'ordre de l'enum.
    bloc = _lire(BLUEPRINT).split("  gestes_choix:\n", 1)[1].split("  gestes:", 1)[0]
    assert re.findall(r"!input (\w+)", bloc) == [e for _, e, _ in GESTES]
    assert re.search(r'kCleGestes\[\] = "gestes"', _lire(ZONES_CPP))


def test_memes_codes_firmware_et_blueprint():
    codes = _codes_firmware()
    assert list(codes) == list(CODES) + ACTIONS + list(ROUE_CODES)
    bp = _bp()
    attendus = ["auto"] + list(codes)
    assert bp["variables"]["codes_gestes"] == attendus
    assert "codes_gestes" in bp["variables"]["gestes"]
    section = bp["blueprint"]["input"]["boutons_haut"]
    assert section.get("collapsed") is True
    assert section["name"] == "Horloge et boutons du haut · Clock and top buttons"
    for nom, e in section["input"].items():
        assert e["default"] == "auto", nom
        options = e["selector"]["select"]["options"]
        assert [o["value"] for o in options] == attendus, nom
        assert all(" · " in o["label"] or o["value"] == "arcade" for o in options), nom
        assert " · " in e["name"], nom


def test_actions_de_l_accueil():
    actions = _actions_firmware()
    assert [c for c, a in actions.items() if a not in ("ECRAN", "RIEN")] == ACTIONS + ["roue"]
    assert actions["rien"] == "RIEN"
    assert all(actions[c] == "ECRAN" for c in CODES if c != "rien")
    script = _lire(NAVIGATION).split("- id: tab5_geste", 1)[1].split("\n  - id:", 1)[0]
    assert "geste_cible(geste)" in script
    assert "id(tab5_ecran_ouvrir).execute(c.ecran);" in script, "un écran s'ouvre par la routine unique"
    for action in set(actions.values()) - {"ECRAN", "RIEN"}:
        branche = script.split(f"GesteAction::{action})", 1)[1].split("} else if", 1)[0]
        assert BRANCHES[action] in branche, action
    # « nabu_suivant » (lot 3) ajouté à la fin, puis la roue de navigation (ADR-0042) : la
    # NVS garde l'index du code.
    assert list(actions).index("nabu_suivant") == 17
    assert list(actions)[18:] == list(ROUE_CODES)


def test_auto_comme_avant():
    corps = re.search(r"kGestesAuto\[GESTE_NB\] = \{(.*?)\};", _lire(ZONES_CPP), re.S).group(1)
    corps = re.sub(r"//[^\n]*", "", corps)
    valeurs = re.findall(r'"(\w+)"|(nullptr)', corps)
    assert [v or None for v, _ in valeurs] == [a for _, _, a in GESTES]
    assert all(v in _codes_firmware() for v, _ in valeurs if v)


def test_glyphes_des_actions():
    corps = _fonction(_lire(ZONES_CPP), "const char* code_glyphe(")
    glyphes = dict(re.findall(r'case GesteAction::(\w+): return "\\U(000F[0-9A-F]{4})"', corps))
    assert set(glyphes) == set(BRANCHES)
    # Mode Domo : l'icône du bouton maison (son tap d'origine).
    assert glyphes["MODE_DOMO"] == "000F07D0"
    styles = _lire(STYLES)
    for police in ("mdi_font_26", "mdi_font_70"):
        bloc = re.search(rf"id: {police}\n(.*?)\n  - ", styles, re.S).group(1)
        tous = set(re.findall(r'\\U(000F[0-9A-F]{4})', corps))
        for g in tous:
            assert f"\\U{g}" in bloc, f"U+{g[3:]} absent de {police} (règle 9)"


# ─── Firmware : NVS, clé lue, boutons ────────────────────────────────────────

def test_nvs():
    cpp = _lire(ZONES_CPP)
    assert re.search(r"struct SauvegardeAppuis \{\s+uint32_t magic;\s+int8_t code\[BOUTON_HAUT_NB\];\s+\};", cpp)
    assert re.search(r"struct SauvegardeGestes \{\s+uint32_t magic;\s+int8_t code\[GESTE_NB\];\s+\};", cpp)
    magics = re.findall(r"constexpr uint32_t kMagic\w* = (0x[0-9A-F]+);", cpp)
    cles = re.findall(r"constexpr uint32_t kPrefKey\w* = (0x[0-9A-F]+);", cpp)
    assert len(set(magics)) == len(magics) == 3 and len(set(cles)) == len(cles) == 3
    assert "static_assert(GESTE_NB == 12" in cpp


def test_cles_lues_avant_la_table_3x():
    corps = _fonction(_lire(ZONES_CPP), "int emplacements_appliquer(")
    assert corps.index("gestes_recu(") < corps.index("cibles[i].cle"), "la table 3.x ignorerait la clé"
    # La fin du payload décide si la clé appuis compte seule (blueprint d'avant le lot A).
    assert corps.rstrip().endswith("gestes_fin_payload();\n    return appliquees;\n}")
    fin = _fonction(_lire(ZONES_CPP), "void gestes_fin_payload(")
    assert "s_appuis_vue && !s_gestes_vue" in fin


def test_plus_de_zone_unique_sur_l_horloge():
    for racine, _, fichiers in os.walk(TAB5):
        for f in fichiers:
            if f.endswith((".yaml", ".cpp", ".h")):
                texte = _lire(os.path.join(racine, f))
                assert not re.search(r"id(: |\()btn_clock_calendar_zone", texte), f


# ─── Les trois zones de l'horloge ─────────────────────────────────────────────

def _zones():
    zones = {}
    for ligne in re.findall(r"^.*file: \.\./ui_components/horloge_zone\.yaml, vars: \{(.*)\} \}$", _lire(LVGL), re.M):
        v = dict(re.findall(r'(\w+): "?(\w+)"?', ligne))
        zones[v["id"]] = (int(v["x"]), int(v["y"]), int(v["largeur"]), int(v["hauteur"]), v["court"], v["long_appui"])
    return zones


def _tuile():
    bloc = _lire(LVGL).split("id: clock_tile", 1)[1]
    g = {k: int(re.search(rf"^\s+{k}: (\d+)$", bloc, re.M).group(1)) for k in ("y", "width", "height")}
    assert "align: TOP_MID" in _lire(LVGL).split("id: clock_tile", 1)[0].rsplit("- obj:", 1)[1] + bloc[:80]
    x = 1280 // 2 - g["width"] // 2  # TOP_MID, x 0 : pw / 2 - w / 2 (lv_obj_pos.c de LVGL 9.5)
    return x, g["y"], g["width"], g["height"]


def test_trois_zones_couvrent_la_tuile():
    zones = _zones()
    assert set(zones) == {"btn_horloge_heures", "btn_horloge_minutes", "btn_horloge_date"}
    tx, ty, tw, th = _tuile()
    assert (tx, ty, tw, th) == (440, 20, 401, 210)
    couverts = {}
    for ident, (x, y, w, h, _, _) in zones.items():
        for px in range(x, x + w):
            for py in range(y, y + h):
                assert (px, py) not in couverts, f"{ident} recouvre {couverts[(px, py)]} en {px}, {py}"
                couverts[(px, py)] = ident
    assert len(couverts) == tw * th, "trou dans l'horloge"
    assert all(tx <= px < tx + tw and ty <= py < ty + th for px, py in couverts)
    gestes = {ident: (c, l) for ident, (*_, c, l) in zones.items()}
    assert gestes == {"btn_horloge_heures": ("GESTE_HEURES_COURT", "GESTE_HEURES_LONG"),
                      "btn_horloge_minutes": ("GESTE_MINUTES_COURT", "GESTE_MINUTES_LONG"),
                      "btn_horloge_date": ("GESTE_DATE_COURT", "GESTE_DATE_LONG")}
    gabarit = _lire(os.path.join(TAB5, "ui_components", "horloge_zone.yaml"))
    assert "id(tab5_geste).execute(${court});" in gabarit and "id(tab5_geste).execute(${long_appui});" in gabarit
    assert "styles: style_invisible" in gabarit


def test_coupes_entre_chiffres_et_date_dans_chaque_theme():
    """Coupe horizontale : sous le cadre des chiffres, au-dessus de l'encre de la date (au
    plus 40 px au-dessus de sa ligne de base, police_theme.DATE_BASE) ; coupe verticale :
    dans le « : » (x et avance du « : » de la police d'heure de chaque thème)."""
    import gen_themes
    import police_theme
    zones = _zones()
    tx, ty = 440, 20
    x_h, y_h, w_h, h_h = zones["btn_horloge_heures"][:4]
    coupe_y = y_h + h_h - ty  # dans la tuile, bordure de 1 px comprise
    assert coupe_y == 1 + police_theme.DATE_BASE - 40
    coupe_x = x_h + w_h - tx
    assert coupe_x == 200
    rangees = re.findall(r"^    \{(-?\d+(?:, -?\d+){8})\},  // (\w+)$", _lire(THEME_CPP), re.M)
    themes = {t.fichier: t for t in gen_themes.charger()}
    mesures = gen_themes.lire_polices()
    assert len(rangees) == len(themes) == 21
    for valeurs, fichier in rangees:
        _, _, _, _, x_dp, _, _, cadre_y, _ = (int(v) for v in valeurs.split(", "))
        assert 1 + cadre_y + police_theme.CADRE_H <= coupe_y, f"{fichier} : chiffres sous la coupe"
        m = mesures[themes[fichier].polices.get("horloge", police_theme.REFERENCE)]
        avance = round(m["chiffres"][":"][0] * m["horloge"]["taille"] / m["unites_em"])
        assert 1 + x_dp <= coupe_x <= 1 + x_dp + avance, f"{fichier} : la coupe ne tombe pas dans le « : »"


def test_le_rendu_touche_chaque_zone():
    import ecrans
    zones = _zones()
    for point, ident in ((ecrans.HEURES, "btn_horloge_heures"), (ecrans.MINUTES, "btn_horloge_minutes"),
                         (ecrans.DATE, "btn_horloge_date")):
        x, y, w, h = zones[ident][:4]
        assert x + 10 <= point[0] < x + w - 10 and y + 10 <= point[1] < y + h - 10, (ident, point)


# ─── Tuile − / + : l'icône de la clim tient entre − et + ─────────────────────

def test_icone_de_la_clim_entre_moins_et_plus():
    """La consigne reste au centre exact de la zone − / + (75..330 : 255 px), comme avant ;
    l'icône de 45 px est posée à 8 px à sa gauche (align_to, puis suivie en C++ à chaque
    changement de taille). Avec la consigne la plus large (« 88.8 », un °F à décimale) dans
    la police de date de chaque thème, la moitié gauche (demi-consigne, 8 px, icône) laisse
    au moins 10 px d'air avant −."""
    import gen_themes
    import police_theme
    carte = _lire(CLIM_CARTE)
    zone = carte.split("id: climate_controls_zone", 1)[1].split("id: reglable_rangee", 1)[0]
    assert "clim_consigne_rangee" not in carte
    cible = zone.split("id: clim_target\n", 1)[1].split("- label:", 1)[0]
    assert "align: CENTER" in cible and "styles: style_police_date" in cible
    icone = zone.split("id: clim_consigne_icone\n", 1)[1].split("\n\n", 1)[0]
    assert "text_font: mdi_font_45" in icone
    assert "align_to: { id: clim_target, align: OUT_LEFT_MID, x: -8 }" in icone
    placer = _fonction(_lire(os.path.join(TAB5, "ecran", "tab5_reglables.cpp")), "void consigne_icone_placer(")
    assert "lv_obj_align(u.consigne_icone, LV_ALIGN_CENTER, -(w / 2) - kEcartIconeConsigne - wi + wi / 2, 0);" in placer
    suivre = _fonction(_lire(os.path.join(TAB5, "ecran", "tab5_reglables.cpp")), "void consigne_icone_suivre(")
    assert suivre.count("LV_EVENT_SIZE_CHANGED") == 2
    mesures = gen_themes.lire_polices()
    ref_cle = police_theme.REFERENCE
    textes = ("88.8", "--")
    chars = "".join(sorted(set("".join(textes))))
    ref = police_theme.mesurer(police_theme.fichier(*ref_cle.rsplit("@", 1)), chars)
    for theme in gen_themes.charger():
        cle = theme.polices.get("date", ref_cle)
        m = police_theme.mesurer(police_theme.fichier(*cle.rsplit("@", 1)), chars)
        taille = mesures[cle]["date"]["taille"]
        texte = max(police_theme.largeur(m, t, taille, ref) for t in textes)
        gauche = texte // 2 + 8 + 45
        assert gauche <= 255 // 2 - 10, f"{theme.fichier} : {gauche} px à gauche du centre, entre − et +"


# ─── Blueprint : rendu des vrais modèles ─────────────────────────────────────

def _payload(declencheur, **entrees):
    return Passage(entrees, [], _evenement(declencheur)).variables_du_bloc("payload")


def test_defauts():
    p = _payload("connexion")
    assert "gestes|" + "|".join(["auto"] * 12) + ";" in p
    # appuis d'abord, gestes ensuite, dans le même payload.
    assert p.index("appuis|") < p.index("gestes|")


def test_un_choix_par_geste():
    choix = {e: c for (_, e, _), c in zip(GESTES, ["ecoute", "rien", "calendrier", "alertes", "assistant",
                                                    "plantes", "tv", "clim", "console", "energie",
                                                    "mode_domo", "rangee_suivante"])}
    p = _payload("connexion", **choix)
    assert ("gestes|ecoute|rien|calendrier|alertes|assistant|plantes|tv|clim|console|energie|"
            "mode_domo|rangee_suivante;") in p
    # La clé appuis porte les trois appuis longs (un firmware 3.7 met une action en « auto »).
    assert "appuis|clim|energie|rangee_suivante;" in p
    assert p.count("gestes|") == 1


@pytest.mark.parametrize("valeur", ["cuisine", "", None, "AUTO", "Ecoute", "Nabu_suivant"])
def test_valeur_inconnue_part_en_auto(valeur):
    p = _payload("connexion", geste_heures_court=valeur, geste_date_long="calendrier")
    assert "gestes|auto|auto|auto|auto|auto|calendrier|auto|auto|auto|auto|auto|auto;" in p


@pytest.mark.parametrize("declencheur", ["connexion", "rechargement", "maj_ecran", "demarrage_ha"])
def test_part_avec_tous_les_etats(declencheur):
    assert "gestes|auto|auto|appareil_suivant|" in _payload(declencheur, geste_minutes_court="appareil_suivant")


@pytest.mark.parametrize("declencheur", ["zones", "action"])
def test_pas_avec_les_autres_declencheurs(declencheur):
    assert "gestes|" not in _payload(declencheur)
