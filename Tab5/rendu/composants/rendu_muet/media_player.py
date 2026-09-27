"""Lecteur média muet du rendu hors tablette (lot 7) : retient le volume, ne joue rien."""

import esphome.codegen as cg
from esphome.components import media_player
import esphome.config_validation as cv

from . import rendu_muet_ns

LecteurMuet = rendu_muet_ns.class_("LecteurMuet", media_player.MediaPlayer, cg.Component)

CONFIG_SCHEMA = media_player.media_player_schema(LecteurMuet).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await media_player.new_media_player(config)
    await cg.register_component(var, config)
