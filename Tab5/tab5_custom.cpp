/**
 * [AI-CONTEXT]
 * @file tab5_custom.cpp
 * @role Globals partagés de la couche C++ et carte de ses unités. Depuis le 08/09/2026
 *       (lot (e) de l'audit du 06/09), la logique vit dans une unité par responsabilité,
 *       toutes déclarées dans tab5_custom.h (unique en-tête public, inchangé) :
 *         tab5_text.cpp      UTF-8 / mojibake, store des alertes rejetées, libellés de jours
 *         tab5_forecast.cpp  icônes et couleurs météo, parsing bulk jours/heures, tuiles
 *         tab5_central.cpp   carte centrale, alertes HA, pagination au swipe, planning tap
 *         tab5_services.cpp  logique des services HA (volet, vigilance, pluie, clim, planning)
 *         tab5_assist.cpp    popup Assistant (Markdown, états), décision du mot de réveil
 *         tab5_cards.cpp     cartes lumière/clim/plantes/pots, température colorée
 *         tab5_console.cpp   console système (status, volume, diagnostics)
 *         tab5_anim.cpp      animations, inactivité, rouleaux (icône météo, horloge), boutons
 *         tab5_calendar.cpp  popup calendrier mensuel
 *       tab5_internal.h déclare les quelques helpers partagés entre unités.
 * @regle_absolue Aucune autre partie du code (YAML ou autre) ne doit appeler lv_obj_set_*
 *                directement : tout widget LVGL est mis à jour via un helper de ces unités,
 *                déclaré dans tab5_custom.h (ADR-0006, garde-fou tools/check_tab5_code_rules.py).
 * @memory_constraint Éviter std::string dans les boucles de parsing. char* + strtok_r ;
 *                    la SRAM est critique (768KB), privilégier le stack (char buf[32]).
 * @ai_instruction Un nouveau capteur = une fonction `update_mon_capteur_ui(lv_obj_t*, float)`
 *                 dans l'unité de sa responsabilité + sa déclaration dans tab5_custom.h,
 *                 appelée depuis le YAML. Ne génère pas de code LVGL dans le YAML.
 */
#include "tab5_custom.h"

// Contexte global carte centrale (initialise au boot via YAML on_boot).
CentralPanelCtx g_central_ctx;

// Tableaux globaux des slots meteo (initialises au boot via YAML on_boot).
WeatherDaySlot g_day_slots[5];
WeatherHourSlot g_hour_slots[5];

// Donnees planning/previsions remplies par parse_and_update_*_bulk (tab5_forecast.cpp),
// lues par la carte centrale, les services et le calendrier.
DayForecastData cal_jours_data[15];
HourForecastData cal_heures_data[15];
