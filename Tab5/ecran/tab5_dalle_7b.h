/**
 * [AI-CONTEXT]
 * @file tab5_dalle_7b.h
 * @role Éteindre et rallumer la dalle de la Waveshare 7B (EK79007) par ses commandes DCS,
 *       en plus du rétroéclairage (tab5_dalle_7b.cpp).
 * @architecture_constraint Listé seulement par Tab5/paquets/carte-waveshare-7b.yaml
 *       (`esphome: includes:`) : compilé pour la 7B seule. Pas inclus par tab5_custom.h.
 */
#pragma once

namespace esphome::mipi_dsi {
class MipiDsi;
}

// Dalle allumée (DCS 0x29, « display on ») ou éteinte (0x28, « display off »). Sans effet
// tant que l'écran n'est pas initialisé. Appelée par le rétroéclairage (on_turn_on / on_turn_off).
void dalle_afficher(esphome::mipi_dsi::MipiDsi* ecran, bool allumee);
