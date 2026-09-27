"""Micro muet du rendu hors tablette (lot 7) : ne capte jamais rien."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import rendu_muet_ns

MicroMuet = rendu_muet_ns.class_("MicroMuet", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(MicroMuet)}).extend(
    cv.COMPONENT_SCHEMA
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
