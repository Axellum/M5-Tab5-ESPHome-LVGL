"""Remplace rtttl dans le rendu hors tablette (lot 7) : la sonnerie du réveil joue sur le
haut-parleur, absent ici. Garde l'id (`alarm_rtttl`, dont le réveil règle le volume) et
déclare les actions et la condition sans effet.
"""

import esphome.codegen as cg
from esphome.components.rendu_muet import actions_muettes, conditions_fausses, rendu_muet_ns
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["rendu_muet"]

SonnerieMuette = rendu_muet_ns.class_("SonnerieMuette", cg.Component)

CONFIG_SCHEMA = cv.ensure_list(
    cv.Schema({cv.GenerateID(): cv.declare_id(SonnerieMuette)}, extra=cv.ALLOW_EXTRA)
)

actions_muettes("rtttl.play", "rtttl.stop")
conditions_fausses("rtttl.is_playing")


async def to_code(config):
    for conf in config:
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
