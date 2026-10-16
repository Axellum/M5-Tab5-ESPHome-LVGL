// [AI-CONTEXT]
// @file carte_7b_version.h
// @role Plancher d'ESPHome propre à la Waveshare 7B (ADR-0061) : 2026.9.1.
// @architecture_constraint Listé seulement par Tab5/paquets/carte-waveshare-7b.yaml
//       (`esphome: includes:`), donc compilé seulement pour cette carte. ESPHome 2026.9.0
//       plante au démarrage de l'écran sur un P4 v3 (horloge DSI « legacy », esphome#18984,
//       corrigé en 2026.9.1) : la carte redémarre en boucle puis passe en mode sans échec.
//       Un `min_version:` posé dans le paquet ne sert à rien : à la fusion des packages,
//       celui de tab5-ha-hmi.yaml (2026.9.0, le plancher du Tab5, lu tel quel par la CI)
//       l'emporte (esphome/components/packages, merge_packages). D'où l'arrêt à la
//       compilation, par la version qu'ESPHome écrit dans le build (writer.py, version.h).
#pragma once

#include "esphome/core/macros.h"
#include "esphome/core/version.h"

static_assert(ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 1),
              "Waveshare 7B : ESPHome 2026.9.1 au moins (2026.9.0 plante au demarrage de l'ecran "
              "d'un P4 v3, esphome#18984)");
