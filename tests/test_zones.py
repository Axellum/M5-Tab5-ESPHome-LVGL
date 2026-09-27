# -*- coding: utf-8 -*-
"""Zones optionnelles (lot 5 de l'audit « ouverture », 27/09/2026) : le contrat entre
la tablette et Home Assistant tient en des clés écrites à quatre endroits, qu'aucun
compilateur ne compare :

- l'enum `Zone` (Tab5/tab5_custom.h) et le tableau `kCles` (Tab5/tab5_zones.cpp) ;
- la demande `esphome.tab5_zones` (Tab5/tab5-zones.yaml), « clé=entité » dans l'ordre
  de l'enum pour les zones que la tablette suit ;
- la réponse de HA (automatisation `tab5_zones_reponse`, package tab5_push.yaml), qui
  ajoute seule les zones qu'elle seule connaît (clim, volet, planning).

Une clé qui diverge ferait masquer la mauvaise zone, ou jamais la bonne, sans aucune
erreur. On vérifie aussi que chaque entité de zone a une valeur par défaut (une ligne
commentée dans user_entities.yaml doit compiler) et que chaque capteur de zone signale
ses données (zone_vue)."""
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
    return [p.split("=", 1) for p in m.group(1).split(",")]


def test_cles_suivent_l_enum():
    assert _kcles() == [n.lower() for n in _enum_zone()]


def test_demande_dans_l_ordre_des_zones_suivies():
    cles = _kcles()
    premiere_ha = _enum_zone().index("CLIM")  # kZonesSuivies
    assert [c for c, _ in _demande()] == cles[:premiere_ha]


def test_entites_de_zone_ont_un_defaut():
    zones_yaml = _lire("Tab5", "tab5-zones.yaml")
    bloc = zones_yaml.split("\nsubstitutions:\n", 1)[1].split("\nscript:", 1)[0]
    defauts = set(re.findall(r"^  (entity_\w+): \w+\.tab5_zone_absente\s*$", bloc, re.M))
    utilisees = {e[2:-1] for _, e in _demande()}
    for fichier in ("tab5-sensors-domotique.yaml", "pot_sensors.yaml"):
        utilisees |= set(re.findall(r"\$\{(entity_\w+)\}", _lire("Tab5", fichier)))
    # pot_sensors.yaml : ${entity_plante_${n}_ec} → les 5 pots.
    utilisees = {u for u in utilisees if "$" not in u}
    utilisees |= {f"entity_plante_{n}_{m}" for n in range(1, 6) for m in ("ec", "lux", "temp", "bat")}
    assert utilisees - defauts == set()


def test_ha_repond_les_zones_qu_il_est_seul_a_connaitre():
    push = _lire("HomeAssistant_Config", "packages", "tab5_push.yaml")
    debut = push.index("- id: tab5_zones_reponse")
    auto = push[debut:push.index("\n  - id:", debut + 1)]
    assert "event_type: esphome.tab5_zones" in auto
    assert "action: esphome.tab5_ha_hmi_tab5_maj_zones" in auto
    premiere_ha = _enum_zone().index("CLIM")
    for cle in _kcles()[premiere_ha:]:
        assert f"['{cle}']" in auto, f"HA ne répond jamais « {cle} »"


def test_chaque_capteur_de_zone_signale_ses_donnees():
    sensors = _lire("Tab5", "tab5-sensors-domotique.yaml")
    premiere_ha = _enum_zone().index("CLIM")
    for nom in _enum_zone()[:premiere_ha]:
        if nom.startswith("POT_"):
            continue  # boucle sur Zone::POT_1 + i (ancre &moisture_on_value)
        assert f"zone_vue(Zone::{nom})" in sensors, f"aucun zone_vue(Zone::{nom})"
    assert "static_cast<int>(Zone::POT_1) + i" in sensors
