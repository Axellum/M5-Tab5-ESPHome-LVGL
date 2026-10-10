# -*- coding: utf-8 -*-
"""Réparation « redémarrage requis » : un bouton qui redémarre Home Assistant.

[AI-CONTEXT]
@role Quand les nouveaux fichiers ne prennent pas effet par un simple rechargement (domaine
      impossible à charger à chaud, configuration déjà invalide avant), __init__.py ouvre la
      réparation « redemarrage_requis ». Sa validation vérifie la configuration (sinon
      homeassistant.restart refuserait en silence, l'appel n'attend pas l'arrêt) puis
      redémarre. « configurer_pieces » ouvre l'assistant de configuration (ADR-0052,
      assistant_flux.AssistantFlux) ; sa fin retire la réparation. Les autres réparations
      réparables (« fichiers_remplaces ») ne font que confirmer : ConfirmRepairFlow, qui
      reprend les paramètres de la réparation.
"""
from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant import config as conf_util
from homeassistant.components.repairs import ConfirmRepairFlow, RepairsFlow, RepairsFlowResult
from homeassistant.core import HomeAssistant

from .assistant_flux import AssistantFlux
from .const import ISSUE_ASSISTANT, ISSUE_CONFIGURATION, ISSUE_REDEMARRAGE


class RedemarrageFlow(ConfirmRepairFlow):
    async def async_step_confirm(self, user_input: dict[str, Any] | None = None) -> RepairsFlowResult:
        erreurs: dict[str, str] = {}
        if user_input is not None:
            if not await conf_util.async_check_ha_config_file(self.hass):
                await self.hass.services.async_call("homeassistant", "restart", blocking=False)
                return self.async_create_entry(data={})
            erreurs["base"] = ISSUE_CONFIGURATION
        return self.async_show_form(step_id="confirm", data_schema=vol.Schema({}), errors=erreurs)


class AssistantReparation(AssistantFlux, RepairsFlow):
    async def async_step_init(self, user_input: dict[str, Any] | None = None) -> RepairsFlowResult:
        return await self.async_step_pieces()

    async def _assistant_fin(self) -> RepairsFlowResult:
        return self.async_create_entry(data={})


async def async_create_fix_flow(hass: HomeAssistant, issue_id: str,
                                data: dict[str, Any] | None) -> RepairsFlow:
    if issue_id == ISSUE_REDEMARRAGE:
        return RedemarrageFlow()
    if issue_id == ISSUE_ASSISTANT:
        return AssistantReparation()
    return ConfirmRepairFlow()
