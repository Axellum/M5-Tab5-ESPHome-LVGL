"""Remplace http_request dans le rendu hors tablette (lot 7) : rien n'est téléchargé.

Accepte la configuration de Tab5/paquets/tab5-assist.yaml (client de l'image de réponse) et
n'en fait rien : l'image téléchargée est remplacée par online_image (bouchon voisin).
"""

import esphome.config_validation as cv

CONFIG_SCHEMA = cv.Schema({}, extra=cv.ALLOW_EXTRA)


async def to_code(config):
    pass
