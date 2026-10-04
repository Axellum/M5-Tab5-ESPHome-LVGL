# -*- coding: utf-8 -*-
"""Icône solaire du bandeau d'état : la production des panneaux en % de leur puissance
crête, poussée par le blueprint « Tab5 — emplacements » (clé solaire de
tab5_maj_emplacements) et montrée par sa couleur en haut à gauche de l'écran.

Une clé de tab5_maj_emplacements plutôt qu'une nouvelle action : un firmware plus ancien
ignore une clé inconnue, alors qu'une action absente est une erreur pour Home Assistant
(même raison que climr, ADR-0026, et crRT/ceRT, ADR-0027).

On vérifie :
- la même clé des deux côtés, et le chemin firmware complet (enum, widget masqué au
  démarrage, pointeur, masquage, glyphes, couleurs par le barème des batteries) ;
- au rendu des VRAIS modèles Jinja du blueprint (outillage de test_tuiles_blueprint) :
  le pourcentage contre un calcul Python indépendant, « nan » sans capteur, sans crête
  ou sans valeur, et quand il part (tous les états ; les mesures lentes seulement si le
  capteur a changé)."""
import os
import re

import pytest

from tests.test_tuiles_blueprint import Etat, Passage, _evenement

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
ZONES_CPP = os.path.join(REPO, "Tab5", "tab5_zones.cpp")
CUSTOM_H = os.path.join(REPO, "Tab5", "tab5_custom.h")
LVGL = os.path.join(REPO, "Tab5", "tab5-lvgl.yaml")
ZONES_YAML = os.path.join(REPO, "Tab5", "tab5-zones.yaml")
STYLES = os.path.join(REPO, "Tab5", "tab5-styles.yaml")
REGLES = os.path.join(REPO, "tools", "check_tab5_code_rules.py")

CAPTEUR = "sensor.solaire_puissance"
MESURES = {"id": "mesures", "platform": "time_pattern"}


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


# ─── Firmware ────────────────────────────────────────────────────────────────

def test_meme_cle_des_deux_cotes():
    cle = re.search(r'kCleSolaire\[\] = "(\w+)"', _lire(ZONES_CPP)).group(1)
    assert f"'{cle}|'" in _lire(BLUEPRINT), "le blueprint ne pousse pas la clé que lit le firmware"


def test_icone_du_bandeau_branchee():
    enum = re.search(r"enum BandeauIcone[^{]*\{(.*?)\};", _lire(CUSTOM_H), re.S).group(1)
    noms = re.findall(r"\b(BANDEAU_\w+)", enum)
    assert "BANDEAU_SOLAIRE" in noms and noms[-1] == "BANDEAU_NB"
    assert "u.bandeau[BANDEAU_SOLAIRE] = id(icon_solaire);" in _lire(ZONES_YAML)
    label = re.search(r"- label: \{ id: icon_solaire,[^}]*\}", _lire(LVGL)).group(0)
    assert "hidden: true" in label, "l'icône doit rester masquée tant que HA n'a rien envoyé"
    assert "text_font: mdi_font_26" in label


def test_masquee_tant_que_rien_n_est_arrive():
    cpp = _lire(ZONES_CPP)
    masquee = _fonction(cpp, "bool bandeau_masquee(")
    assert re.search(r"case BANDEAU_SOLAIRE:\s*return std::isnan\(s_solaire\);", masquee)
    assert re.search(r"float s_solaire = NAN;", cpp)


def test_glyphes_listes_et_couleurs_par_jetons():
    cpp = _lire(ZONES_CPP)
    glyphes = re.findall(r'\\U(000F[0-9A-F]{4})', _fonction(cpp, "const char* solaire_glyphe("))
    assert glyphes, "solaire_glyphe ne donne aucun glyphe"
    police = re.search(r"id: mdi_font_26\n(.*?)\n  - ", _lire(STYLES), re.S).group(1)
    for g in glyphes:
        assert f"\\U{g}" in police, f"glyphe U+{g[3:]} absent de mdi_font_26 (règle 9)"
    couleur = _fonction(cpp, "uint32_t solaire_couleur(")
    assert "get_battery_color(" in couleur and "UIColor::INACTIVE" in couleur
    assert not re.search(r"0x[0-9A-Fa-f]{6}", couleur), "couleur en dur (règle 1)"
    assert '("tab5_zones.cpp", "solaire_glyphe"): ("icon_solaire",)' in _lire(REGLES)


# ─── Blueprint ───────────────────────────────────────────────────────────────

def _passage(trigger, etat="3000", unite="W", crete=6, age=60, capteur=True):
    entrees = {"energie_crete": crete}
    etats = []
    if capteur:
        entrees["energie_solaire"] = CAPTEUR
        etats.append(Etat(CAPTEUR, etat, age=age, unit_of_measurement=unite, device_class="power"))
    return Passage(entrees, etats, trigger)


def _attendu(etat, unite, crete):
    """Calcul indépendant : W → % de la crête (kWc), arrondi, borné 0-100."""
    try:
        w = float(etat) * {"kW": 1e3, "MW": 1e6}.get(unite, 1)
    except ValueError:
        return "nan"
    if crete <= 0:
        return "nan"
    return str(min(max(round(w / (crete * 1000) * 100), 0), 100))


@pytest.mark.parametrize("etat,unite,crete", [
    ("3000", "W", 6), ("3.0", "kW", 6), ("1450", "W", 3.2), ("0", "W", 6),
    ("-3", "W", 6),           # onduleur qui lit un peu sous zéro la nuit
    ("29", "W", 6),           # veille : 0,48 % → 0, l'icône « nuit »
    ("7200", "W", 6),         # au-dessus de la crête (froid et soleil) : borné à 100
    ("0.9", "MW", 1000), ("12000", "W", 9.99),
    ("unavailable", "W", 6), ("unknown", "W", 6),
    ("3000", "W", 0),         # crête non remplie
])
def test_pourcentage(etat, unite, crete):
    p = _passage(_evenement("connexion"), etat, unite, crete)
    assert str(p["solaire_pourcent"]) == _attendu(etat, unite, crete)
    assert f"solaire|{_attendu(etat, unite, crete)};" in p.variables_du_bloc("payload")


def test_sans_capteur_aucune_valeur():
    p = _passage(_evenement("connexion"), capteur=False)
    assert p["solaire_pourcent"] == "nan"
    assert p.variables_du_bloc("payload").endswith("solaire|nan;")


@pytest.mark.parametrize("declencheur", ["connexion", "rechargement", "maj_ecran", "demarrage_ha"])
def test_part_avec_tous_les_etats(declencheur):
    p = _passage(_evenement(declencheur), age=86400)
    assert p["solaire_a_pousser"] is True
    assert "solaire|50;" in p.variables_du_bloc("payload")


def test_mesures_capteur_change_seul_il_fait_partir_le_passage():
    p = _passage(MESURES, age=60)
    assert p["cles"] == [] and p["solaire_a_pousser"] is True
    assert p.conditions(), "le passage des mesures s'arrête alors que la production a changé"
    assert p.variables_du_bloc("payload") == "solaire|50;"


@pytest.mark.parametrize("surcharge", [
    {"age": 3600},            # capteur inchangé depuis le passage d'avant
    {"crete": 0},             # pas de crête : pas d'icône, rien à rafraîchir
    {"capteur": False},       # pas de capteur
])
def test_mesures_rien_de_neuf(surcharge):
    p = _passage(MESURES, **surcharge)
    assert p["solaire_a_pousser"] is False
    assert not p.conditions(), "un passage des mesures sans rien de neuf doit s'arrêter aux conditions"


def test_pas_avec_les_autres_declencheurs():
    p = _passage(_evenement("zones"))
    assert p["solaire_a_pousser"] is False
    assert "solaire|" not in p.variables_du_bloc("payload")


def test_entree_facultative_dans_la_section_energie():
    from tests.test_tuiles_blueprint import _blueprint
    entree = _blueprint()["blueprint"]["input"]["energie"]["input"]["energie_crete"]
    assert entree["default"] == 0
    assert entree["selector"]["number"]["unit_of_measurement"] == "kWc"
