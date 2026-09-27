"""Remplace speaker dans le rendu hors tablette (lot 7) : celui d'ESPHome tire `audio`,
réservé à l'ESP32. Plateforme muette : Tab5/rendu/composants/rendu_muet/speaker.py.
"""

import esphome.codegen as cg
from esphome.components.rendu_muet import actions_muettes, conditions_fausses

AUTO_LOAD = ["rendu_muet"]
IS_PLATFORM_COMPONENT = True

speaker_ns = cg.esphome_ns.namespace("speaker")
Speaker = speaker_ns.class_("Speaker")

actions_muettes(
    "speaker.play", "speaker.stop", "speaker.finish", "speaker.volume_set",
    "speaker.mute_on", "speaker.mute_off",
)
conditions_fausses("speaker.is_playing", "speaker.is_stopped")


async def to_code(config):
    pass
