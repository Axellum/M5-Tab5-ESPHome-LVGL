"""Remplace online_image dans le rendu hors tablette (lot 7) : voir image.py."""

from esphome.components.rendu_muet import actions_muettes

AUTO_LOAD = ["rendu_muet"]

actions_muettes("online_image.set_url", "online_image.release")
