# -*- coding: utf-8 -*-
"""Zones optionnelles (lot 5 de l'audit « ouverture », 27/09/2026) : le contrat entre
la tablette et Home Assistant tient en des clés écrites à quatre endroits, qu'aucun
compilateur ne compare :

- l'enum `Zone` (Tab5/tab5_custom.h) et le tableau `kCles` (Tab5/tab5_zones.cpp) ;
- la demande `esphome.tab5_zones` (Tab5/tab5-zones.yaml), les clés des zones que la
  tablette suit, dans l'ordre de l'enum ;
- la réponse de HA : depuis le lot 6a (ADR-0019), le blueprint « Tab5 — emplacements »
  (liste `cles_zones`), qui ajoute les zones qu'il est seul à connaître (clim, volet,
  planning).

Une clé qui diverge ferait masquer la mauvaise zone, ou jamais la bonne, sans aucune
erreur. On vérifie aussi que chaque capteur de zone signale ses données (zone_vue)."""
import os
import re

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def _lire(*parts):
    with open(os.path.join(REPO, *parts), encoding="utf-8") as f:
        return f.read()


def _enum_zone():
    m = re.search(r"enum class Zone : uint8_t \{(.*?)\};", _lire("Tab5", "tab5_custom.h"), re.S)
    assert m, "enum Zone introuvable dans tab5_custom.h"
    corps = "\n".join(l.split("//", 1)[0] for l in m.group(1).splitlines())
    noms = [n.strip() for n in corps.replace("\n", " ").split(",") if n.strip()]
    assert noms[-1] == "COUNT"
    return noms[:-1]


def _kcles():
    m = re.search(r"kCles\[kNbZones\] = \{(.*?)\};", _lire("Tab5", "tab5_zones.cpp"), re.S)
    assert m, "kCles introuvable dans tab5_zones.cpp"
    return re.findall(r'"([a-z0-9_]+)"', m.group(1))


def _demande():
    m = re.search(r'^\s+zones: "([^"]+)"', _lire("Tab5", "tab5-zones.yaml"), re.M)
    assert m, "chaîne zones: introuvable dans tab5-zones.yaml"
    return m.group(1).split(",")


def _blueprint():
    return _lire("HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")


def _liste_yaml(texte, nom):
    """Les éléments « - x » de la liste `nom:` (une clé par ligne)."""
    m = re.search(rf"^(\s+){nom}:\n((?:\1  - \w+\n)+)", texte, re.M)
    assert m, f"liste {nom} introuvable"
    return re.findall(r"- (\w+)", m.group(2))


def test_cles_suivent_l_enum():
    assert _kcles() == [n.lower() for n in _enum_zone()]


def test_demande_dans_l_ordre_des_zones_suivies():
    cles = _kcles()
    premiere_ha = _enum_zone().index("CLIM")  # kZonesSuivies
    assert _demande() == cles[:premiere_ha]


def test_le_blueprint_repond_toutes_les_zones():
    bp = _blueprint()
    assert "event_type: esphome.tab5_zones" in bp
    assert '_tab5_maj_zones"' in bp
    # Toutes les clés, y compris clim, volet et planning, que seul HA connaît.
    assert _liste_yaml(bp, "cles_zones") == _kcles()


def test_plus_de_reponse_des_zones_dans_le_package():
    # Deux répondeurs s'écraseraient : la tablette remplace toute sa liste à chaque réponse.
    assert "tab5_maj_zones" not in _lire("HomeAssistant_Config", "packages", "tab5_push.yaml")


def test_chaque_capteur_de_zone_signale_ses_donnees():
    sensors = _lire("Tab5", "tab5-sensors-domotique.yaml")
    premiere_ha = _enum_zone().index("CLIM")
    for nom in _enum_zone()[:premiere_ha]:
        if nom.startswith("POT_"):
            continue  # boucle sur Zone::POT_1 + i (ancre &moisture_on_value)
        assert f"zone_vue(Zone::{nom})" in sensors, f"aucun zone_vue(Zone::{nom})"
    assert "static_cast<int>(Zone::POT_1) + i" in sensors
