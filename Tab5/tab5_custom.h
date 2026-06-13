#pragma once
#include "esphome.h"
#include <string>
#include <vector>

extern std::string cal_jour_nom[15];
extern std::string cal_heures[15];
extern bool cal_toggled[15];

struct DayForecastData {
    std::string nom_jour;
    std::string condition;
    float tmin = 0.0f;
    float tmax = 0.0f;
    bool est_repos = false;
    bool est_dimanche = false;
    bool est_passe = false;
    std::string heures_ouverture;
};

struct HourForecastData {
    std::string heure_texte;
    std::string condition;
    float temp = 0.0f;
    float pluvio = 0.0f;
};

extern DayForecastData cal_jours_data[15];
extern HourForecastData cal_heures_data[15];

void tab5_calendar_toggle(int jour);
namespace esphome { namespace font { class Font; } }
void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card, esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);

uint32_t get_humidity_color(float x);
uint32_t get_temperature_color(float t);

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
    // Pointers for action widgets
    lv_obj_t* action_btn;
    lv_obj_t* action_icon1;
    lv_obj_t* action_icon2;
    lv_obj_t* extra_btn; // e.g. direction shutter button
};

void parse_and_update_heures_bulk(const std::string& payload);
void parse_and_update_jours_bulk(const std::string& payload);

void refresh_daily_forecast(WeatherDaySlot slots[], int page_index,
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);
void refresh_hourly_forecast(WeatherHourSlot slots[], int page_index,
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

// Structure pour les 4 slots UI d'humidite plantes (triés dynamiquement)
struct MoistureSlotUI {
    lv_obj_t* icon_lbl;
    lv_obj_t* val_lbl;
};

// Tri dynamique : prend 5 valeurs, affiche les 2 plus secs + médiane + plus humide
// icons_utf8[5] = codes MDI pour chaque capteur, slots[4] = widgets LVGL de destination
void sort_and_update_moisture_slots(float values[5], const char* icons_utf8[5],
    MoistureSlotUI slots[4]);

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

