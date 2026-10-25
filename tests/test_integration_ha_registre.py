# -*- coding: utf-8 -*-
"""Noms anglais : le code de l'intégration contre les vrais registres de Home Assistant.

Ne tourne qu'avec le banc de test des intégrations (pytest-homeassistant-custom-component,
qui installe Home Assistant) : sauté sinon, donc dans la CI. À lancer à la main avant un
essai sur un vrai HA :
    python -m venv venv_ha && venv_ha/Scripts/pip install pytest-homeassistant-custom-component
    venv_ha/Scripts/python -m pytest tests/test_integration_ha_registre.py -p no:cacheprovider
"""
from __future__ import annotations

import pytest

pytest.importorskip("pytest_homeassistant_custom_component")

from homeassistant.core import HomeAssistant, State  # noqa: E402
from homeassistant.helpers import device_registry as dr  # noqa: E402
from homeassistant.helpers import entity_registry as er  # noqa: E402
from homeassistant.helpers import issue_registry as ir  # noqa: E402
from homeassistant.setup import async_setup_component  # noqa: E402
from pytest_homeassistant_custom_component.common import (  # noqa: E402
    MockConfigEntry,
    mock_restore_cache,
)

from custom_components.tab5 import Gestionnaire  # noqa: E402
from custom_components.tab5.const import DOMAIN, ISSUE_MIGRATION, MODELE_TABLETTE  # noqa: E402

TAB5 = "E8:F6:0A:E2:F6:B6"
SEPT_B = "AA:BB:CC:DD:EE:FF"


@pytest.fixture
def gestionnaire(hass: HomeAssistant) -> Gestionnaire:
    entree = MockConfigEntry(domain=DOMAIN)
    entree.add_to_hass(hass)
    return Gestionnaire(hass, entree, "9.9.9")


def _tablette(hass: HomeAssistant, mac: str, nom: str, entites: list[tuple[str, str, str]]) -> None:
    """Une tablette ESPHome et ses entités : [(domaine, nom dans le firmware, objet de l'identifiant)]."""
    esphome = MockConfigEntry(domain="esphome", unique_id=mac)
    esphome.add_to_hass(hass)
    appareil = dr.async_get(hass).async_get_or_create(
        config_entry_id=esphome.entry_id, connections={(dr.CONNECTION_NETWORK_MAC, mac)},
        name=nom, model=MODELE_TABLETTE)
    registre = er.async_get(hass)
    for domaine, nom_fw, objet in entites:
        type_ = {"sensor_texte": "text_sensor"}.get(domaine, domaine)
        domaine = "sensor" if domaine == "sensor_texte" else domaine
        registre.async_get_or_create(domaine, "esphome", f"{mac}/0/{type_}/{nom_fw}",
                                     suggested_object_id=objet, config_entry=esphome,
                                     device_id=appareil.id, original_name=nom_fw)


async def test_migration_deux_tablettes(hass: HomeAssistant, gestionnaire: Gestionnaire) -> None:
    _tablette(hass, TAB5, "Tab5", [
        ("switch", "Réveil", "tab5_reveil"),
        ("select", "Thème", "tab5_theme"),
        ("sensor", "Tab5 Uptime", "tab5_tab5_uptime"),
        ("sensor_texte", "Prochain réveil", "tab5_prochain_reveil"),
        ("number", "Volume", "tab5_volume"),
    ])
    _tablette(hass, SEPT_B, "M5Stack Tab5 Home Assistant HMI", [
        ("sensor", "Tab5 Uptime", "m5stack_tab5_home_assistant_hmi_tab5_uptime_2"),
        ("switch", "Réveil", "chambre_reveil_perso"),  # identifiant choisi par l'utilisateur
    ])
    registre = er.async_get(hass)
    registre.async_update_entity("switch.chambre_reveil_perso", name="Mon réveil")

    gestionnaire.proposer_migration()
    issue = ir.async_get(hass).async_get_issue(DOMAIN, ISSUE_MIGRATION)
    assert issue is not None and issue.is_fixable
    assert issue.translation_placeholders == {"nombre": "5"}  # Volume et le réveil perso nommé : rien

    assert await gestionnaire.async_migrer() == 5

    def entree(eid: str) -> er.RegistryEntry | None:
        return registre.async_get(eid)

    assert entree("switch.tab5_reveil") is None
    assert entree("switch.tab5_alarm").name == "Alarm"
    assert entree("select.tab5_theme").name == "Theme"  # même identifiant, nom traduit
    assert entree("sensor.tab5_uptime").name == "Uptime"
    assert entree("sensor.tab5_next_alarm").unique_id == f"{TAB5}/0/text_sensor/Prochain réveil"
    assert entree("number.tab5_volume").name is None  # hors table
    # la 7B : « _2 » retiré (libre) ; l'identifiant et le nom de l'utilisateur gardés
    assert entree("sensor.m5stack_tab5_home_assistant_hmi_uptime").name == "Uptime"
    assert entree("switch.chambre_reveil_perso").name == "Mon réveil"
    # l'identifiant unique ne bouge jamais : le firmware garde ses noms
    assert entree("switch.tab5_alarm").unique_id == f"{TAB5}/0/switch/Réveil"
    # tout fait : la réparation disparaît
    assert ir.async_get(hass).async_get_issue(DOMAIN, ISSUE_MIGRATION) is None
    assert await gestionnaire.async_migrer() == 0


async def test_second_ecran_garde_son_suffixe(hass: HomeAssistant, gestionnaire: Gestionnaire) -> None:
    _tablette(hass, TAB5, "Tab5", [("switch", "Réveil", "tab5_reveil")])
    _tablette(hass, SEPT_B, "Tab5", [("switch", "Réveil", "tab5_reveil_2")])
    await gestionnaire.async_migrer()
    registre = er.async_get(hass)
    assert registre.async_get("switch.tab5_alarm").unique_id.startswith(TAB5)
    assert registre.async_get("switch.tab5_alarm_2").unique_id.startswith(SEPT_B)


async def test_option_gardee_au_rechargement(hass: HomeAssistant, gestionnaire: Gestionnaire) -> None:
    eid = "input_select.tab5_alertes_froid"
    assert await async_setup_component(hass, "input_select", {"input_select": {
        "tab5_alertes_froid": {"name": "froid", "options": ["Oui", "Non"]}}})
    await hass.services.async_call("input_select", "select_option", {"entity_id": eid, "option": "Non"},
                                   blocking=True)
    choisies = gestionnaire._options_choisies()
    # les nouveaux fichiers : options anglaises, HA reprend la première
    await hass.services.async_call("input_select", "set_options", {"entity_id": eid, "options": ["Yes", "No"]},
                                   blocking=True)
    assert hass.states.get(eid).state == "Yes"
    await gestionnaire._options_gardees(choisies)
    assert hass.states.get(eid).state == "No"


async def test_option_gardee_au_demarrage(hass: HomeAssistant, gestionnaire: Gestionnaire) -> None:
    eid = "input_select.tab5_alertes_vigilance"
    mock_restore_cache(hass, [State(eid, "Rouge")])
    assert await async_setup_component(hass, "input_select", {"input_select": {
        "tab5_alertes_vigilance": {"name": "vigilance", "options": ["Yellow", "Orange", "Red", "None"]}}})
    await hass.async_block_till_done()
    assert hass.states.get(eid).state == "Yellow"  # « Rouge » n'existe plus
    await gestionnaire.async_options_au_demarrage()
    assert hass.states.get(eid).state == "Red"
