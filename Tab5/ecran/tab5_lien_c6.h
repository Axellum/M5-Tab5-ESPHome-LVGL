/**
 * [AI-CONTEXT]
 * @file tab5_lien_c6.h
 * @role Redémarrage logiciel du P4 en reset CORE, pour le lien SDIO du C6 (tab5_lien_c6.cpp).
 * @architecture_constraint Listé seulement par Tab5/paquets/carte-waveshare-7b.yaml
 *       (`esphome: includes:`) : compilé pour la 7B seule. Pas inclus par tab5_custom.h.
 */
#pragma once

// true si esp_restart() finit bien par un reset CORE (tab5_lien_c6.cpp).
bool lien_c6_demarrage();
