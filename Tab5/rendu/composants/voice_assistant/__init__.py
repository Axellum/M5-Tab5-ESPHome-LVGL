"""Remplace voice_assistant dans le rendu hors tablette (lot 7) : pas de micro ni d'assistant.

Garde l'id (`va`), lu par les lambdas de l'interface (is_running, is_continuous), et
déclare ses actions et conditions sans effet. Le reste de la configuration de
Tab5/paquets/tab5-assist.yaml est accepté puis ignoré (ses automatismes ne se déclenchent jamais).
"""

import esphome.codegen as cg
from esphome.components.rendu_muet import AssistantMuet, actions_muettes, conditions_fausses
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["rendu_muet"]

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(AssistantMuet)}, extra=cv.ALLOW_EXTRA)

actions_muettes("voice_assistant.start", "voice_assistant.start_continuous", "voice_assistant.stop")
conditions_fausses("voice_assistant.is_running", "voice_assistant.connected")


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
