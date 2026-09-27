"""Remplace micro_wake_word dans le rendu hors tablette (lot 7) : pas de micro ni de modèle.

Garde l'id (`mww`) et déclare les actions et conditions de l'interface sans effet ;
les modèles (`okay_nabu`, `stop`…) ne sont pas téléchargés.
"""

import esphome.codegen as cg
from esphome.components.rendu_muet import ReveilMuet, actions_muettes, conditions_fausses
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["rendu_muet"]

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(ReveilMuet)}, extra=cv.ALLOW_EXTRA)

actions_muettes(
    "micro_wake_word.start",
    "micro_wake_word.stop",
    "micro_wake_word.enable_model",
    "micro_wake_word.disable_model",
)
conditions_fausses("micro_wake_word.is_running", "micro_wake_word.model_is_enabled")


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
