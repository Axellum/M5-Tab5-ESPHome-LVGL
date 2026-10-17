/**
 * [AI-CONTEXT]
 * @file tab5_dalle.h
 * @role Vraie extinction de la dalle avec le rétroéclairage, sur les deux cartes
 *       (tab5_dalle.cpp, audit anti-casse du 16/10/2026).
 * @architecture_constraint Listé par tab5-hardware.yaml et carte-waveshare-7b.yaml (`esphome:
 *       includes:`), appelé seulement
 *       par la lumière `backlight`, le script `tab5_dalle_rallumer` et les `ota:` des fichiers
 *       de carte (tab5-hardware.yaml, carte-waveshare-7b.yaml, publication-commune.yaml). Pas
 *       inclus par tab5_custom.h : le rendu hors tablette n'a pas d'écran `mipi_dsi`.
 *       Ce que reçoit chaque dalle : drapeau `TAB5_DALLE_EXTINCTION` (substitution
 *       `carte_dalle_extinction` de la carte ou de la révision de dalle).
 */
#pragma once

namespace esphome {
namespace mipi_dsi {
class MipiDsi;
}
}  // namespace esphome

// Extinction : voile noir dessiné tout de suite, puis, selon la dalle, « display off » (0x28)
// et « sleep in » (0x10). Appelé juste avant `lvgl.pause`.
void dalle_eteindre(esphome::mipi_dsi::MipiDsi* ecran);
// Rallumage, premier temps : « sleep out » (0x11) si la dalle dort. true = envoyé, il faut
// alors attendre 120 ms avant `dalle_rallumer()` (le script `tab5_dalle_rallumer` le fait).
bool dalle_reveiller(esphome::mipi_dsi::MipiDsi* ecran);
// Rallumage, second temps : « display on » (0x29) si la dalle était éteinte, voile retiré.
// Appelé juste avant `lvgl.resume`.
void dalle_rallumer(esphome::mipi_dsi::MipiDsi* ecran);
// true si la carte éteint aussi sa dalle pendant une mise à jour du firmware
// (`TAB5_DALLE_NOIRE_OTA`, substitution `carte_dalle_noire_ota` : la 7B seule).
bool dalle_noire_pendant_ota();
