/**
 * [AI-CONTEXT]
 * @file tab5_dalle_7b.h
 * @role Passer l'écran de la Waveshare 7B au noir quand le rétroéclairage s'éteint
 *       (tab5_dalle_7b.cpp).
 * @architecture_constraint Listé seulement par Tab5/paquets/carte-waveshare-7b.yaml
 *       (`esphome: includes:`) : compilé pour la 7B seule. Pas inclus par tab5_custom.h.
 */
#pragma once

// Voile noir sur tout l'écran, dessiné tout de suite (true), ou retiré (false). Appelée par le
// rétroéclairage : avant `lvgl.pause` à l'extinction, avant `lvgl.resume` au rallumage.
void dalle_noire(bool noire);
