/**
 * [AI-CONTEXT]
 * @file tab5_lien_c6.h
 * @role Redémarrage logiciel du P4 en reset CORE, pour le lien SDIO du C6 (tab5_lien_c6.cpp).
 * @architecture_constraint Listé par Tab5/paquets/tab5-hardware.yaml (Tab5) et
 *       carte-waveshare-7b.yaml (7B), `esphome: includes:` : compilé pour les deux cartes,
 *       pas pour le rendu hors tablette (pas d'ESP-Hosted). Pas inclus par tab5_custom.h.
 */
#pragma once

// true si esp_restart() finit bien par un reset CORE (tab5_lien_c6.cpp).
bool lien_c6_demarrage();
