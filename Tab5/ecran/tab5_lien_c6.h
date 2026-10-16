/**
 * [AI-CONTEXT]
 * @file tab5_lien_c6.h
 * @role Redémarrage du P4 quand le lien SDIO avec le C6 ne monte pas (tab5_lien_c6.cpp).
 * @architecture_constraint Listé seulement par Tab5/paquets/carte-waveshare-7b.yaml
 *       (`esphome: includes:`) : compilé pour la 7B seule. Pas inclus par tab5_custom.h.
 */
#pragma once

// true si le redémarrage d'ESP-Hosted est bien remplacé (tab5_lien_c6.cpp).
bool lien_c6_demarrage();
