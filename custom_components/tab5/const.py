# -*- coding: utf-8 -*-
"""Constantes de l'intégration « Tab5 » (voir __init__.py)."""
from __future__ import annotations

DOMAIN = "tab5"

# Dossier des fichiers HA embarqués, à côté de ce code (tools/publication/archive_hacs.py).
DOSSIER_FICHIERS = "fichiers"
# Sauvegardes gardées dans config/tab5_sauvegardes/ (les plus anciennes sont retirées).
GARDER_SAUVEGARDES = 5

# Option : lancer la mise à jour de la tablette quand les fichiers sont en place.
CONF_FIRMWARE = "mettre_a_jour_tablette"
# Option (formulaire seulement) : réinstaller les fichiers de cette version maintenant.
CONF_REINSTALLER = "reinstaller"
# Option (formulaire seulement) : enchaîner sur l'assistant de configuration (ADR-0052).
CONF_ASSISTANT = "assistant"

# Modèle d'appareil du firmware (bloc `project:` de tab5-ha-hmi.yaml) : c'est par lui que
# les packages trouvent la tablette (packages/tab5_evenements.yaml), et l'intégration aussi.
MODELE_TABLETTE = "tab5-ha-hmi"
# Capteur des packages qui donne la version des fichiers chargés (packages/tab5_health.yaml) :
# la preuve que HA lit bien les nouveaux fichiers.
CAPTEUR_VERSION = "sensor.tab5_version_des_fichiers_ha"
PACKAGE_TEMOIN = "tab5_health"
ATTENTE_CAPTEUR_S = 20

STOCKAGE_CLE = "tab5.fichiers"
STOCKAGE_VERSION = 1

NOTIFICATION = "tab5_installation"

# Réparations (translations/*.json, section « issues »).
ISSUE_FICHIERS_ABSENTS = "fichiers_absents"
ISSUE_CONFIGURATION = "configuration_invalide"
ISSUE_PACKAGES = "packages_absents"
ISSUE_REDEMARRAGE = "redemarrage_requis"
# Fichiers du Tab5 déjà là (copiés à la main) et différents, remplacés : persistante, elle
# reste après un redémarrage jusqu'à ce que l'utilisateur la valide.
ISSUE_REMPLACES = "fichiers_remplaces"
# La tablette n'a toujours pas la version après firmware.ESSAIS_MAX essais (firmware.py).
ISSUE_FIRMWARE = "firmware_echec"
# Fichiers actifs et aucune automatisation du blueprint : « Configurer la tablette depuis
# vos pièces ». Sa réparation EST l'assistant (assistant_flux.py, ADR-0052).
ISSUE_ASSISTANT = "configurer_pieces"

# Paramètre {signaler} de « configuration_invalide » : hassfest refuse une URL écrite
# dans les traductions.
URL_SIGNALER = "https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/issues"
