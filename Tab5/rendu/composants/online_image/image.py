"""Plateforme `image: online_image` du rendu hors tablette (lot 7) : un pixel noir.

L'image de réponse de l'assistant (Tab5/paquets/tab5-assist.yaml) est téléchargée à la demande
sur la tablette. Ici, l'id reste une vraie image::Image, pour que le widget LVGL qui la
montre compile ; elle n'est jamais remplacée.
"""

import esphome.codegen as cg
from esphome.components import image
from esphome.components.rendu_muet import rendu_muet_ns
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["rendu_muet"]

ImageMuette = rendu_muet_ns.class_("ImageMuette", image.Image_)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(ImageMuette)}, extra=cv.ALLOW_EXTRA)


async def to_code(config):
    image.add_metadata(config[CONF_ID], 1, 1, "RGB565", image.CONF_OPAQUE)
    cg.new_Pvariable(config[CONF_ID])
