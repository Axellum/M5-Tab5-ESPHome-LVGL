#pragma once
#include "esphome.h"
#include <string>

extern std::string cal_jour_nom[15];
extern std::string cal_heures[15];
extern bool cal_toggled[15];

void tab5_calendar_toggle(int jour);
namespace esphome { namespace font { class Font; } }
void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card, esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);

uint32_t get_humidity_color(float x);
uint32_t get_temperature_color(float t);

// AXE8 : Structures pour les slots previsions (Phase 4)
// Permettent de passer des tableaux de pointeurs LVGL aux helpers C++
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
};

// Helpers de parsing bulk + mise a jour LVGL
// Les pointeurs de polices sont passes par la lambda YAML (seul endroit ou id() fonctionne)
void parse_and_update_heures_bulk(const std::string& payload, WeatherHourSlot slots[], int count,
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);
void parse_and_update_jours_bulk(const std::string& payload, WeatherDaySlot slots[], int count,
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);

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

// Couleurs semantiques centralisees (miroir des tokens YAML color:)
// Utiliser dans les lambdas C++ au lieu des hex bruts
namespace UIColor {
    static constexpr uint32_t SUCCESS      = 0x4CD964;  // Vert vif (actif, OK)
    static constexpr uint32_t WARNING      = 0xFFA500;  // Orange (attention)
    static constexpr uint32_t ERROR        = 0xFF4D4D;  // Rouge (erreur, critique)
    static constexpr uint32_t INFO         = 0x4D94FF;  // Bleu vif (info, connecte)
    static constexpr uint32_t GOLD         = 0xFFD700;  // Or (soleil, lune)
    static constexpr uint32_t TEXT_DIM     = 0xA3A8B5;  // Texte secondaire / repos
    static constexpr uint32_t INACTIVE     = 0x404552;  // Hors ligne / NaN
    static constexpr uint32_t WARM_PINK    = 0xFF66B2;  // Temperature interieure chaude
    static constexpr uint32_t ALERT_YELLOW = 0xFFFF00;  // Vigilance jaune MF
    static constexpr uint32_t ALERT_RED    = 0xFF0000;  // Vigilance rouge MF
}

