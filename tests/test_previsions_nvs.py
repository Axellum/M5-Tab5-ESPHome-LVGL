# -*- coding: utf-8 -*-
"""Prévisions gardées en NVS (16/10/2026, tab5_forecast.cpp).

Avant la première poussée de HA, les tuiles météo repartent des dernières prévisions :
le script tab5_previsions_relire (tab5-scripts.yaml) pose les slots des tuiles et appelle
previsions_relire(), avant la première image (tab5_zones_apply). L'on_boot de
tab5-ha-hmi.yaml pose les mêmes slots, mais après l'attente de HA, et ne se modifie pas
sans Axel : les deux listes doivent rester identiques."""
import re

from tests.commun import lire, source

SLOT = re.compile(r"^\s*(g_(?:day|hour)_slots\[\d\] = \{.*\};)\s*$", re.M)


def _slots(texte: str) -> list[str]:
    return SLOT.findall(texte)


def _script(texte: str, nom: str) -> str:
    debut = texte.index(f"  - id: {nom}\n")
    suite = re.search(r"^  - id: ", texte[debut + 1:], re.M)
    return texte[debut: debut + 1 + suite.start()] if suite else texte[debut:]


def test_slots_du_script_egaux_a_ceux_de_l_on_boot():
    on_boot = _slots(lire("tab5-ha-hmi.yaml"))
    script = _slots(_script(lire("Tab5", "paquets", "tab5-scripts.yaml"), "tab5_previsions_relire"))
    assert len(on_boot) == 10, on_boot
    assert script == on_boot
    # Le rendu hors tablette recopie l'on_boot (tab5-rendu-host.yaml) : mêmes slots.
    assert _slots(lire("tab5-rendu-host.yaml")) == on_boot


def test_relecture_avant_la_premiere_image():
    zones = _script(lire("Tab5", "paquets", "tab5-zones.yaml"), "tab5_zones_apply")
    assert "script.execute: tab5_previsions_relire" in zones
    script = _script(lire("Tab5", "paquets", "tab5-scripts.yaml"), "tab5_previsions_relire")
    assert "previsions_relire(g_day_slots, g_hour_slots," in script


def test_gardees_apres_chaque_lecture_qui_change():
    cpp = source("tab5_forecast.cpp").read_text(encoding="utf-8")
    heures = cpp[cpp.index("bool accept_heures_bulk("):]
    heures = heures[: heures.index("\n}\n")]
    # Après la garde « inchangé », et seulement un bloc lu en entier : un bloc identique
    # n'écrit rien, un bloc abîmé n'écrase pas le bon.
    garde = "if (parse_and_update_heures_bulk(payload)) garder_heures(bloc, payload);"
    assert heures.index("push_unchanged(") < heures.index(garde)
    jours = cpp[cpp.index("void parse_and_update_jours_bulk("):]
    jours = jours[: jours.index("\n}\n")]
    assert jours.index("previsions_jours_lire(") < jours.index("else\n        garder_jours(payload);")
    # La relecture garde la date du jour 0 de la poussée d'origine.
    relire = cpp[cpp.index("void previsions_relire("):]
    relire = relire[: relire.index("\n}\n")]
    assert "cal_jours_anchor_day = s_jours_nvs.ancre;" in relire
