# -*- coding: utf-8 -*-
"""Ajout de l'intégration « Tab5 » (une seule fois) et ses options.

[AI-CONTEXT]
@role Le formulaire d'ajout (Paramètres → Appareils et services → Ajouter une intégration →
      « Tab5 ») ne demande qu'une chose : mettre aussi la tablette à jour quand les
      fichiers sont en place (coché par défaut : la demande d'origine était « un clic »).
      Les options le changent, et proposent de réinstaller les fichiers de la version
      (un fichier effacé ou abîmé à la main).
@contraintes `single_config_entry` (manifest.json) : une seule entrée, HA refuse la seconde.
"""
from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant.config_entries import ConfigEntry, ConfigFlow, ConfigFlowResult, OptionsFlow
from homeassistant.core import callback

from .const import CONF_FIRMWARE, CONF_REINSTALLER, DOMAIN


class Tab5ConfigFlow(ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def async_step_user(self, user_input: dict[str, Any] | None = None) -> ConfigFlowResult:
        if user_input is not None:
            return self.async_create_entry(
                title="Tab5", data={}, options={CONF_FIRMWARE: bool(user_input[CONF_FIRMWARE])})
        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({vol.Required(CONF_FIRMWARE, default=True): bool}))

    @staticmethod
    @callback
    def async_get_options_flow(config_entry: ConfigEntry) -> OptionsFlow:
        return Tab5OptionsFlow()


class Tab5OptionsFlow(OptionsFlow):
    async def async_step_init(self, user_input: dict[str, Any] | None = None) -> ConfigFlowResult:
        if user_input is not None:
            if user_input.get(CONF_REINSTALLER):
                gestionnaire = self.config_entry.runtime_data
                self.hass.async_create_task(gestionnaire.async_installer(forcer=True))
            return self.async_create_entry(data={CONF_FIRMWARE: bool(user_input[CONF_FIRMWARE])})
        actuel = self.config_entry.options.get(CONF_FIRMWARE, True)
        return self.async_show_form(
            step_id="init",
            data_schema=vol.Schema({
                vol.Required(CONF_FIRMWARE, default=actuel): bool,
                vol.Required(CONF_REINSTALLER, default=False): bool,
            }))
