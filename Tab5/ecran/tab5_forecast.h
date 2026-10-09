/**
 * [AI-CONTEXT]
 * @file tab5_forecast.h
 * @role Prévisions météo (tab5_forecast.cpp) : tuiles journalières et horaires, icônes
 *       météo, poussées des prévisions et leur fraîcheur.
 * @architecture_constraint Sorti de tab5_custom.h le 08/10/2026, lignes recopiées telles
 *       quelles : tab5_custom.h l'inclut, les lambdas YAML et les unités `tab5_*.cpp` n'ont
 *       rien à changer. Une fonction déclarée ici a un appelant hors de son fichier (règle 12
 *       de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes).
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; un module nouveau = un
 *       en-tête de plus, inclus par tab5_custom.h et listé sous `includes:` des deux
 *       configurations racine (tab5-ha-hmi.yaml, tab5-rendu-host.yaml).
 */
#pragma once
#include "esphome.h"
#include "tab5_tokens.h"
#include <string>

namespace esphome { namespace font { class Font; } }

// Icône météo d'une tuile (police 120 px, 80 px pour le petit calque 2). echelle_pct : la
// taille des polices passées en % de celles des tuiles, qui met les décalages du calque 2
// à l'échelle (popup Météo, ADR-0043 : 48 et 32 px, 40 %).
void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, const std::string& state, esphome::font::Font* f_card, esphome::font::Font* f_card_s, int echelle_pct = 100);

// Palette : UIColor, ou UIBandeau dans le bandeau central (icône de la pluie).
uint32_t get_humidity_color(float x, const Palette& p = UIColor);

struct WeatherHourSlot {
    lv_obj_t* time_lbl;
    lv_obj_t* temp_lbl;
    lv_obj_t* prob_lbl;
    lv_obj_t* icon_l1;
    lv_obj_t* icon_l2;
};

struct WeatherDaySlot {
    lv_obj_t* day_lbl;
    lv_obj_t* max_lbl;
    lv_obj_t* min_lbl;
    lv_obj_t* icon_l1;
    lv_obj_t* icon_l2;
    // Bouton et épaules d'appareil : plus lus depuis l'ADR-0023 (pièces) — ces widgets
    // sont dans g_tuiles_ui (tab5_tuiles.cpp). Les champs restent : l'on_boot de
    // tab5-ha-hmi.yaml, intouchable, initialise la structure entière.
    lv_obj_t* action_btn;
    lv_obj_t* action_icon1;
    lv_obj_t* action_icon2;
    lv_obj_t* extra_btn; // e.g. direction shutter button
};

void parse_and_update_jours_bulk(const std::string& payload);

// Prévisions horaires reçues par blocs de 5 créneaux (idx 0, 5 ou 10 en tête) :
// analyse le bloc s'il a changé et renvoie true seulement s'il faut repeindre les
// tuiles À L'ÉCRAN — calque horaire visible (pages 0-1) et bloc de cette page
// (page 0 = créneaux 5-9, page 1 = 0-4 ; le bloc 10-14 n'est pas sur les tuiles, seulement
// dans le popup Météo, ADR-0043).
bool accept_heures_bulk(const std::string& payload, int forecast_page);

// Prévisions périmées (08/10/2026, tab5_forecast.cpp). `zone` = previsions_perimees
// (icône + texte, tab5-lvgl.yaml), `lbl` = lbl_previsions_perimees.
// previsions_recues() : à CHAQUE poussée des jours ou des heures, avant toute garde
// (une poussée identique dit aussi « HA pousse toujours ») ; `il_y_a_min` ne sert
// qu'au rendu hors tablette (scène des prévisions périmées), 0 sinon.
// previsions_fraicheur_tick() : chaque seconde (tab5-scripts.yaml) et au basculement
// du mode HA ; ne repeint que si la mention change (ui_text / ui_hidden).
void previsions_recues(lv_obj_t* zone, lv_obj_t* lbl, int il_y_a_min = 0);
void previsions_fraicheur_tick(lv_obj_t* zone, lv_obj_t* lbl);

// Tableaux globaux des slots meteo (initialises au boot, fixes car ids LVGL constants).
// Evite la reconstruction identique dans chaque lambda YAML (D2).
extern WeatherDaySlot g_day_slots[5];
extern WeatherHourSlot g_hour_slots[5];

void refresh_daily_forecast(WeatherDaySlot slots[], int page_index,
    esphome::font::Font* f_card, esphome::font::Font* f_card_s);
void refresh_hourly_forecast(WeatherHourSlot slots[], int page_index,
    esphome::font::Font* f_card, esphome::font::Font* f_card_s);

// AXE5 : Constantes nommees pour les icones meteo (UTF-8 de la police IconeMeteo.ttf)
// Evite les bytes bruts non-documentés, facilite la maintenance si la police change
namespace MeteoIcon {
    static constexpr const char* WIND         = "\xEF\x80\x80"; // windy
    static constexpr const char* SNOW         = "\xEF\x80\x82"; // snowy
    static constexpr const char* HAIL         = "\xEF\x80\x81"; // hail / snowy-rainy
    static constexpr const char* HEAVY_RAIN   = "\xEF\x80\x85"; // pouring
    static constexpr const char* RAIN         = "\xEF\x80\x86"; // rainy
    static constexpr const char* THUNDER      = "\xEF\x80\x87"; // lightning
    static constexpr const char* MOON         = "\xEF\x80\x8B"; // clear-night
    static constexpr const char* FOG          = "\xEF\x80\x8E"; // fog
    static constexpr const char* SUNNY        = "\xEF\x80\x8F"; // sunny / Clear
    static constexpr const char* CLOUD        = "\xEF\x80\x95"; // cloudy / default
}

// Tuiles météo de la page courante, icônes comprises (polices du YAML : elles ne sont
// atteignables que par id()).
void forecast_rejouer_theme(esphome::font::Font* f_card, esphome::font::Font* f_card_s);
