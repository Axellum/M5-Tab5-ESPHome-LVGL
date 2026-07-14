/**
 * [AI-CONTEXT]
 * @file tab5_custom.h
 * @role Déclarations des fonctions C++ et du dictionnaire de couleurs.
 * @architecture_constraint C'est ici que se trouve le namespace UIColor qui contient 
 *                          toutes les constantes de couleurs sémantiques.
 * @ai_instruction Ne JAMAIS recréer des constantes de couleurs ailleurs. Utiliser UIColor::*.
 */
#pragma once
#include "esphome.h"
#include <string>
#include <vector>

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
void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, const std::string& state, bool is_card, esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);

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
void transition_widgets(lv_obj_t* out_obj, lv_obj_t* in_obj);

// Gestion du geste de swipe (page_main.on_gesture) : pagination previsions
// horaires/journalieres (0-4) dans la bande centrale+basse (y >= 333). Console diag :
// uniquement via btn_control_console (plus de swipe haut/bas).
void handle_swipe_gesture(lv_dir_t dir, lv_coord_t pt_y, int& forecast_page_index,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    lv_obj_t* planning_wrap, lv_obj_t* rain_wrap, lv_obj_t* alert_cont, lv_obj_t* info_wrap,
    int current_panel);

// Carte centrale : rotateur planning/pluie/alertes (page 2) ou titre de page (autres).
void update_central_forecast_page_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    lv_obj_t* planning_wrap, lv_obj_t* rain_wrap, lv_obj_t* alert_cont, lv_obj_t* info_wrap,
    int current_panel);

// Panneau info central (récap calendrier ou bannière alerte) — logique déplacée
// depuis tab5-api-logic.yaml pour fiabiliser polices LVGL et accents UTF-8.
void update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, bool& has_info, int& current_panel,
    esphome::font::Font* font_small, esphome::font::Font* font_large);

// Met a jour un label de temperature (texte + couleur gradient). Factorise
// depuis temp_serre/temp_salon (tab5-sensors.yaml, Phase 3, #T164).
void update_temp_ui(lv_obj_t* label, float x);

// Garde #T222 : ne touche LVGL que si l'overlay console est affiche.
bool is_console_layer_visible(lv_obj_t* layer_console);

// Ligne 1 console (uptime / RSSI / temp CPU) — capteurs 60s, refresh a l'ouverture.
void update_console_uptime_label(lv_obj_t* label, float uptime_s);
void update_console_rssi_label(lv_obj_t* label, float rssi_dbm);
void update_console_temp_label(lv_obj_t* label, float core_temp_c);
void refresh_console_status_row_ui(lv_obj_t* lbl_uptime, lv_obj_t* lbl_rssi, lv_obj_t* lbl_temp,
    bool has_uptime, float uptime_s, bool has_rssi, float rssi_dbm, bool has_temp, float core_temp_c);

// Met a jour les widgets de la console diagnostic (SRAM/PSRAM/frag/loop/IP/SSID).
// Factorise depuis l'interval 2s de tab5-sensors.yaml (Phase 3, #T164).
void update_console_diagnostics_ui(lv_obj_t* lbl_sram, lv_obj_t* bar_sram,
    lv_obj_t* lbl_psram, lv_obj_t* bar_psram, lv_obj_t* lbl_frag, lv_obj_t* lbl_flash,
    bool loop_time_has_state, float loop_time, lv_obj_t* lbl_loop,
    bool wifi_ip_has_state, const char* wifi_ip, lv_obj_t* lbl_ip,
    bool wifi_ssid_has_state, const char* wifi_ssid, lv_obj_t* lbl_ssid);

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

// Met a jour l'icone carte (epaule j2/j3/j4), l'icone/label du switch associe et le
// bouton popup power si c'est la lampe actuellement affichee. Factorise depuis les 3
// blocs identiques light_chambre_state/light_salon_state/light_led_state (#T164).
void update_light_card_ui(lv_obj_t* icon_room, lv_obj_t* icon_light, lv_obj_t* icon_switch,
    lv_obj_t* lbl_switch_state, lv_obj_t* btn_power_icon,
    const std::string& current_light_entity, const std::string& this_entity, bool is_on);

// Tap tuile météo : affiche le planning/horaires du jour dans la carte centrale (6s).
std::string get_day_planning_display_text(int jour);
void show_temporary_planning(int jour, lv_obj_t* lbl_planning, lv_obj_t* planning_wrap, lv_obj_t* alert_cont, lv_obj_t* rain_wrap,
                             lv_obj_t* info_wrap, lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, int forecast_page,
                             const std::string& plan_l1, const std::string& plan_l2, bool& is_showing_temp, int& current_panel);

// Couleurs semantiques centralisees (miroir des tokens YAML color:)
// Utiliser dans les lambdas C++ au lieu des hex bruts
// Palette "Dark Mode Slate" : miroir EXACT des tokens YAML color: (les garder synchro).
namespace UIColor {
    // --- Semantiques HSL vibrantes ---
    static constexpr uint32_t SUCCESS      = 0x34D399;  // emerald-400 (actif, OK)
    static constexpr uint32_t WARNING      = 0xFBBF24;  // amber-400 (attention)
    static constexpr uint32_t ERROR        = 0xFB7185;  // rose-400 (erreur, critique)
    static constexpr uint32_t INFO         = 0x38BDF8;  // sky-400 (info, connecte / aujourd'hui)
    static constexpr uint32_t GOLD         = 0xFCD34D;  // amber-300 (soleil, lune)
    static constexpr uint32_t TEXT_DIM     = 0x94A3B8;  // slate-400 (texte secondaire / repos)
    static constexpr uint32_t INACTIVE     = 0x334155;  // slate-700 (hors ligne / NaN)
    static constexpr uint32_t WARM_PINK    = 0xF472B6;  // pink-400 (temperature interieure chaude)
    // --- Accents "verre" ---
    static constexpr uint32_t ACCENT       = 0x22D3EE;  // cyan-400 (accent primaire / halo)
    static constexpr uint32_t ACCENT_ALT   = 0xA78BFA;  // violet-400 (accent secondaire)
    static constexpr uint32_t GLASS_RIM    = 0x93A3BC;  // Liseré lumineux (arête de verre)
    static constexpr uint32_t EARLY        = 0xFB7185;  // rose-400 (journee a debauche matinale)
    static constexpr uint32_t PAST         = 0x64748B;  // slate-500 (jour passe, estompe)
    // --- Vigilance Meteo-France : NE PAS modifier (semantique officielle) ---
    static constexpr uint32_t ALERT_YELLOW = 0xFFFF00;  // Vigilance jaune MF
    static constexpr uint32_t ALERT_RED    = 0xFF0000;  // Vigilance rouge MF
    // --- Climatisation (popup grille 3x3, tab5_maj_clim) : valeurs inchangees,
    // seulement nommees pour sortir des hex en dur de tab5-api-logic.yaml ---
    static constexpr uint32_t CLIM_COOL_ACTIVE     = 0x4D94FF;  // Bleu vif
    static constexpr uint32_t CLIM_COOL_INACTIVE   = 0x60748F;  // Bleu grisatre inactif
    static constexpr uint32_t CLIM_HEAT_ACTIVE     = 0xFF4D4D;  // Rouge vif
    static constexpr uint32_t CLIM_HEAT_INACTIVE   = 0x8F6060;  // Rouge grisatre inactif
    static constexpr uint32_t CLIM_OFF_ACTIVE      = 0xFFA500;  // Orange
    static constexpr uint32_t CLIM_OFF_INACTIVE    = 0xB48154;  // Orange grise
    static constexpr uint32_t CLIM_TRACK_INACTIVE  = 0x4A596E;  // Gris (fan/swing/quiet inactifs)
    static constexpr uint32_t CLIM_ECO             = 0x4CD964;  // Vert standard
    // --- Forecast / alertes / pluie (tab5-api-logic.yaml) ---
    static constexpr uint32_t TEXT_PRIMARY         = 0xFFFFFF;  // Blanc labels forecast
    static constexpr uint32_t ALERT_DATE_YELLOW    = 0xFCF3CF;
    static constexpr uint32_t ALERT_DATE_ORANGE    = 0xF8C471;
    static constexpr uint32_t ALERT_DATE_RED       = 0xF1948A;
    static constexpr uint32_t RAIN_LIGHT           = 0x81D4FA;
    static constexpr uint32_t RAIN_MODERATE        = 0x29B6F6;
    static constexpr uint32_t RAIN_HEAVY           = 0x0277BD;
    static constexpr uint32_t RAIN_EXTREME         = 0x01579B;
    // --- Icones meteo / humidite / arc (miroir YAML + algorithmes) ---
    static constexpr uint32_t METEO_CELESTIAL      = 0xFFD700;  // Soleil / lune (IconeMeteo)
    static constexpr uint32_t METEO_PRECIP         = 0x8AB4FF;  // Pluie / neige / grele
    static constexpr uint32_t METEO_THUNDER        = 0xFF6600;  // Orage
    static constexpr uint32_t MOISTURE_NAN         = 0x404552;  // Humidite plante indisponible
    static constexpr uint32_t HUMIDITY_WET         = 0x0000CC;  // Air tres humide
    static constexpr uint32_t TEMP_NAN             = 0xA3A8B5;  // Temperature indisponible
    static constexpr uint32_t TEXT_SOFT            = 0xF1F5F9;  // Miroir color_text
    static constexpr uint32_t ICON_MUTED           = 0x555555;  // Miroir color_icon_muted
    static constexpr uint32_t ARC_TRACK            = 0x2A2D35;  // Miroir color_arc_track
    static constexpr uint32_t MODAL_SCRIM          = 0x05080F;  // Miroir color_modal_scrim
}

