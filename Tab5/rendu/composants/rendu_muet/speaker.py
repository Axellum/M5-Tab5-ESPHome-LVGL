"""Haut-parleur muet du rendu hors tablette (lot 7) : ne joue jamais."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import rendu_muet_ns

HautParleurMuet = rendu_muet_ns.class_("HautParleurMuet", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(HautParleurMuet)}).extend(
    cv.COMPONENT_SCHEMA
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
