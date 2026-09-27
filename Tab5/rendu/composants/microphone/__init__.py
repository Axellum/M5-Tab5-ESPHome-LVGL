"""Remplace microphone dans le rendu hors tablette (lot 7) : celui d'ESPHome tire `audio`,
réservé à l'ESP32. Plateforme muette : Tab5/rendu/composants/rendu_muet/microphone.py.
"""

import esphome.codegen as cg
from esphome.components.rendu_muet import actions_muettes, conditions_fausses

AUTO_LOAD = ["rendu_muet"]
IS_PLATFORM_COMPONENT = True

microphone_ns = cg.esphome_ns.namespace("microphone")
Microphone = microphone_ns.class_("Microphone")

actions_muettes("microphone.capture", "microphone.stop_capture", "microphone.mute", "microphone.unmute")
conditions_fausses("microphone.is_capturing", "microphone.is_muted")


async def to_code(config):
    pass
