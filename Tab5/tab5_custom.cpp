/**
 * [AI-CONTEXT]
 * @file tab5_custom.cpp
 * @role Couche logique centrale et unique point de contact avec l'API LVGL.
 * @regle_absolue Aucune autre partie du code (YAML ou autre) ne doit appeler lv_obj_set_* directement. 
 *                Tout widget LVGL doit être mis à jour via une fonction helper définie ici.
 * @memory_constraint Éviter std::string dans les boucles parsing. Utiliser char* et strtok_r.
 *                    La SRAM est critique (768KB), privilégier le stack (char buf[32]).
 * @data_flow Réceptionne les payloads bulk (jours/heures) depuis tab5-api-logic.yaml,
 *            parse en place, et met à jour les structs DayForecastData/HourForecastData.
 * @ai_instruction Si tu dois ajouter un nouveau capteur, crée ici une fonction `update_mon_capteur_ui(lv_obj_t* label, float val)` 
 *                 et appelle-la depuis le YAML. Ne génère pas de code LVGL dans le YAML.
 */
// tab5_custom.cpp — Implementation : mise a jour LVGL (update_meteo_icon,
// sort_and_update_moisture_slots, transition_widgets), mappeurs de couleur
// (get_temperature_color, get_humidity_color), parsing des payloads bulk
// (parse_and_update_heures_bulk, parse_and_update_jours_bulk). Toutes les
// fonctions ici gardent contre les lv_obj_t* nuls (LVGL pas encore init).
#include "tab5_custom.h"
#include "lvgl.h" // Needed for lv_label_set_text etc
#include "esphome/components/lvgl/lvgl_esphome.h"


// we can declare references to ESPHome LVGL objects but we can't easily include main.h
std::string cal_heures[15] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
bool cal_toggled[15] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}; 

DayForecastData cal_jours_data[15];
HourForecastData cal_heures_data[15];

void tab5_calendar_toggle(int jour) {
    if (jour >= 0 && jour < 15) {
        cal_toggled[jour] = !cal_toggled[jour];
    }
}

void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, const std::string& state, bool is_card, esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s) {
    std::string l1_text = MeteoIcon::CLOUD; // Nuage par defaut
    uint32_t l1_color = 0xFFFFFF;
    std::string l2_text = "";
    uint32_t l2_color = 0xFFFFFF;
    int l2_x = 0; int l2_y = 0; int l1_y = 0;
    bool l2_small = false; bool l2_behind = false;

    // Dictionnaire type Classe CSS avec position de base (Grosse icone)
    if (state == "clear-night") { l1_text = MeteoIcon::MOON; l1_color = 0xFFD700; }
    else if (state == "cloudy") { l1_text = MeteoIcon::CLOUD; }
    else if (state == "fog") { l1_text = MeteoIcon::FOG; }
    else if (state == "Clear" || state == "sunny") { l1_text = MeteoIcon::SUNNY; l1_color = 0xFFD700; }
    else if (state == "partlycloudy" || state == "partlycloudy-night" || state == "partlycloudy_night") {
        l1_text = MeteoIcon::CLOUD; 
        l2_text = (state == "partlycloudy") ? MeteoIcon::SUNNY : MeteoIcon::MOON;
        l2_small = true; l2_color = 0xFFD700; l2_behind = true;
        l2_x = -45; l2_y = -45;
    }
    else if (state == "hail" || state == "snowy-rainy") { l2_text = MeteoIcon::HAIL; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "lightning" || state == "thunder" || state == "lightning-rainy") { l2_text = MeteoIcon::THUNDER; l2_color = 0xFF6600; l2_behind = true; l1_y = -30; }
    else if (state == "pouring") { l2_text = MeteoIcon::HEAVY_RAIN; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "rainy") { l2_text = MeteoIcon::RAIN; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "snowy") { l2_text = MeteoIcon::SNOW; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "windy" || state == "windy-variant") { l1_text = MeteoIcon::WIND; }

    // Systeme de Ratio automatique pour avoir une justesse pixel perfect !
    // Grosse icone = 270px, Petite = 120px. Ratio exact = 120/270 = 0.4444...
    float ratio = is_card ? 0.4444f : 1.0f;
    l2_x = (int)(l2_x * ratio);
    l2_y = (int)(l2_y * ratio);
    l1_y = (int)(l1_y * ratio);

    if (l1_obj) {
        lv_obj_clear_flag(l1_obj, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(l1_obj, l1_text.c_str());
        lv_obj_set_style_text_color(l1_obj, lv_color_hex(l1_color), LV_PART_MAIN);
        lv_obj_set_style_translate_y(l1_obj, l1_y, LV_PART_MAIN);
        esphome::lvgl::lv_obj_set_style_text_font(l1_obj, is_card ? f_card : f_main, LV_PART_MAIN);
        if (l2_behind && l2_obj) { lv_obj_move_foreground(l1_obj); }
    }
    if (l2_obj) {
        if (l2_text != "") {
            lv_obj_clear_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(l2_obj, l2_text.c_str());
            lv_obj_set_style_text_color(l2_obj, lv_color_hex(l2_color), LV_PART_MAIN);
            lv_obj_set_style_translate_x(l2_obj, l2_x, LV_PART_MAIN);
            lv_obj_set_style_translate_y(l2_obj, l2_y, LV_PART_MAIN);
            esphome::lvgl::lv_obj_set_style_text_font(l2_obj, is_card ? (l2_small ? f_card_s : f_card) : (l2_small ? f_main_s : f_main), LV_PART_MAIN);
        } else {
            lv_obj_add_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

uint32_t get_humidity_color(float x) {
    if (isnan(x)) return 0x404552;
    int val = (int)x;
    if (val <= 14) return 0xFF0000;
    if (val >= 80) return 0x0000CC;
    if (val >= 30) {
        float step = floor((val - 30) / 3.0) * 3.0;
        float ratio = step / 50.0;
        int r = 255 - (255 * ratio);
        int g = 255 - (255 * ratio);
        int b = 255 - ((255 - 204) * ratio);
        return (r << 16) | (g << 8) | b;
    }
    if (val >= 22) {
        float ratio = (val - 22) / 8.0;
        int b = 255 * ratio;
        return (255 << 16) | (255 << 8) | b;
    }
    float ratio = (val - 14) / 8.0;
    int g = 255 * ratio;
    return (255 << 16) | (g << 8) | 0;
}

uint32_t get_temperature_color(float t) {
    if (isnan(t)) return 0xA3A8B5;
    if (t <= -12) return 0xFF0000;
    if (t <= 0) {
        float r = floor((t + 12) / 2.0) * 2.0 / 12.0;
        return (255 << 16) | (0 << 8) | (int)(255 * r);
    }
    if (t <= 14) {
        float r = floor(t / 2.0) * 2.0 / 14.0;
        return ((int)(255 * r) << 16) | ((int)(255 * r) << 8) | 255;
    }
    if (t <= 24) {
        float r = floor((t - 14) / 2.0) * 2.0 / 10.0;
        return (255 << 16) | ((int)(255 - (255 * r)) << 8) | 255;
    }
    float s = floor((t - 24) / 2.0) * 2.0;
    if (s > 11) s = 11;
    return (255 << 16) | (0 << 8) | (int)(255 - (255 * (s / 11.0)));
}

// =============================================================================
// AXE8 (Phase 4) : Helpers de parsing bulk pour previsions meteo
// Centralise le parsing du payload serialise et la mise a jour LVGL
// =============================================================================

// Remplacement de split_token par un parsing in-place avec strtok_r pour éviter la fragmentation de la SRAM.
void parse_and_update_heures_bulk(const std::string& payload) {
    if (payload.empty()) return;
    if (payload.length() > 2048) {
        ESP_LOGE("TAB5", "Payload heures trop long (%d octets). Rejeté pour éviter OOM.", payload.length());
        return;
    }
    ESP_LOGI("TAB5", "Received heures bulk payload length: %d", payload.length());
    // Buffer stack plutot que "std::string s = payload;" (copie heap evitable
    // jusqu'a 2048 octets) - mirroir du fix deja applique a tab5_maj_alerte_meteo_france.
    char buf[2049];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        char* parts[6];
        int num_parts = 0;

        char* p = token;
        while (true) {
            if (num_parts >= 6) break;
            parts[num_parts++] = p;
            char* next = strchr(p, '|');
            if (next) {
                *next = '\0';
                p = next + 1;
            } else {
                break;
            }
        }

        if (num_parts >= 5) {
            int idx = std::atoi(parts[0]);
            if (idx >= 0 && idx < 15) {
                cal_heures_data[idx].heure_texte = parts[1];
                cal_heures_data[idx].condition = parts[2];
                cal_heures_data[idx].temp = std::atof(parts[3]);
                cal_heures_data[idx].pluvio = std::atof(parts[4]);
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
}

void parse_and_update_jours_bulk(const std::string& payload) {
    if (payload.empty()) return;
    if (payload.length() > 2048) {
        ESP_LOGE("TAB5", "Payload jours trop long (%d octets). Rejeté pour éviter OOM.", payload.length());
        return;
    }
    ESP_LOGI("TAB5", "Received jours bulk payload length: %d", payload.length());
    // Buffer stack plutot que "std::string s = payload;" (copie heap evitable
    // jusqu'a 2048 octets) - mirroir du fix deja applique a tab5_maj_alerte_meteo_france.
    char buf[2049];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        // Découper chaque token par '|' — in-place, pas de std::vector
        char* parts[10];  // 9 champs attendus + marge
        int num_parts = 0;

        char* p = token;
        while (true) {
            if (num_parts >= 10) break;
            parts[num_parts++] = p;
            char* next = strchr(p, '|');
            if (next) {
                *next = '\0';
                p = next + 1;
            } else {
                break;
            }
        }

        if (num_parts >= 9) {
            int jour = std::atoi(parts[0]);
            if (jour >= 0 && jour < 15) {
                cal_jours_data[jour].nom_jour = parts[1];
                cal_jours_data[jour].condition = parts[2];
                cal_jours_data[jour].tmin = std::atof(parts[3]);
                cal_jours_data[jour].tmax = std::atof(parts[4]);
                cal_jours_data[jour].est_repos = (parts[5][0] == '1');
                cal_jours_data[jour].est_dimanche = (parts[6][0] == '1');
                cal_jours_data[jour].est_passe = (parts[7][0] == '1');
                cal_jours_data[jour].heures_ouverture = parts[8];

                // cal_heures[] reste utilise (toggle jour/heure sur tap, cf. refresh_daily_forecast()
                // et forecast_day_temp_tab.yaml) — cal_jour_nom[] jumeau retire le 06/07/2026 (write-only,
                // jamais lu, cf. cartographie CARTOGRAPHIE_TAB5.md §4.2)
                cal_heures[jour] = parts[8];
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
}

void refresh_daily_forecast(WeatherDaySlot slots[], int page_index,
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s) {
    
    if (page_index < 0 || page_index > 2) return;

    for (int i = 0; i < 5; i++) {
        int jour = page_index * 5 + i;
        WeatherDaySlot& slot = slots[i];
        if (!slot.day_lbl) continue;

        DayForecastData& data = cal_jours_data[jour];

        // Toggle calendar / day name
        lv_label_set_text(slot.day_lbl, cal_toggled[jour] ? cal_heures[jour].c_str() : data.nom_jour.c_str());

        if(cal_toggled[jour]) {
            lv_obj_add_flag(slot.max_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(slot.min_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(slot.max_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(slot.min_lbl, LV_OBJ_FLAG_HIDDEN);
        }

        // Tmin / Tmax colors
        uint32_t cmax = get_temperature_color(data.tmax);
        uint32_t cmin = get_temperature_color(data.tmin);
        char buftx[64]; sprintf(buftx, "#%06x %.0f# / ", cmax, data.tmax);
        char buftn[64]; sprintf(buftn, " #%06x %.0f# \xC2\xB0", cmin, data.tmin);

        lv_label_set_text(slot.max_lbl, data.est_passe ? "-- / " : buftx);
        lv_label_set_text(slot.min_lbl, data.est_passe ? "-- \xC2\xB0" : buftn);
        update_meteo_icon(slot.icon_l1, slot.icon_l2, data.condition, true, f_main, f_card, f_main_s, f_card_s);

        // Coloring day names
        uint8_t opa = data.est_passe ? 100 : 255;
        uint32_t col = 0xFFFFFF;
        bool is_early = false;
        if (!data.est_repos && data.heures_ouverture.length() >= 5) {
            int hour = atoi(data.heures_ouverture.substr(0, 2).c_str());
            int minute = atoi(data.heures_ouverture.substr(3, 2).c_str());
            if (hour < 9 || (hour == 9 && minute == 0)) is_early = true;
        }

        if (jour == 0) col = UIColor::INFO;                                              // Aujourd'hui : cyan info
        else if (data.est_dimanche) col = data.est_repos ? UIColor::WARNING : UIColor::ERROR;
        else if (data.est_repos) col = UIColor::SUCCESS;                                 // Jour de repos : emeraude
        else if (is_early) col = UIColor::EARLY;                                         // Debauche matinale : rose
        if (data.est_passe) col = UIColor::PAST;                                         // Jour passe : ardoise estompee

        lv_obj_set_style_text_color(slot.day_lbl, lv_color_hex(col), LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.day_lbl, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.icon_l1, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.icon_l2, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.max_lbl, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.min_lbl, opa, LV_PART_MAIN);
        
        lv_label_set_recolor(slot.max_lbl, true);
        lv_label_set_recolor(slot.min_lbl, true);
        lv_obj_set_style_text_color(slot.max_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(slot.min_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

        // Show/hide action elements depending on page_index (only show actions on page 0)
        if (slot.action_btn) {
            if (page_index == 0) {
                lv_obj_clear_flag(slot.action_btn, LV_OBJ_FLAG_HIDDEN);
                if (slot.action_icon1) lv_obj_clear_flag(slot.action_icon1, LV_OBJ_FLAG_HIDDEN);
                if (slot.action_icon2) lv_obj_clear_flag(slot.action_icon2, LV_OBJ_FLAG_HIDDEN);
                if (slot.extra_btn) lv_obj_clear_flag(slot.extra_btn, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(slot.action_btn, LV_OBJ_FLAG_HIDDEN);
                if (slot.action_icon1) lv_obj_add_flag(slot.action_icon1, LV_OBJ_FLAG_HIDDEN);
                if (slot.action_icon2) lv_obj_add_flag(slot.action_icon2, LV_OBJ_FLAG_HIDDEN);
                if (slot.extra_btn) lv_obj_add_flag(slot.extra_btn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void refresh_hourly_forecast(WeatherHourSlot slots[], int page_index,
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s) {
    
    if (page_index < 0 || page_index > 2) return;

    for (int i = 0; i < 5; i++) {
        // Slot i on screen (left-to-right) corresponds to time index: page_index * 5 + (4 - i)
        int idx = page_index * 5 + (4 - i);
        WeatherHourSlot& slot = slots[i];
        if (!slot.time_lbl) continue;

        HourForecastData& data = cal_heures_data[idx];

        lv_label_set_text(slot.time_lbl, data.heure_texte.c_str());
        
        uint32_t c_t = get_temperature_color(data.temp);
        char b_t[32]; sprintf(b_t, "#%06x %.0f#\xC2\xB0", c_t, data.temp);
        lv_label_set_text(slot.temp_lbl, b_t);
        lv_label_set_recolor(slot.temp_lbl, true);
        lv_obj_set_style_text_color(slot.temp_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

        char b_p[32];
        if (data.pluvio > 0) {
            sprintf(b_p, "%.1fmm", data.pluvio);
            lv_label_set_text(slot.prob_lbl, b_p);
            lv_obj_set_style_text_color(slot.prob_lbl, lv_color_hex(0x8AB4FF), LV_PART_MAIN);
        } else {
            lv_label_set_text(slot.prob_lbl, "-");
            lv_obj_set_style_text_color(slot.prob_lbl, lv_color_hex(UIColor::CLIM_TRACK_INACTIVE), LV_PART_MAIN);
        }

        update_meteo_icon(slot.icon_l1, slot.icon_l2, data.condition, true, f_main, f_card, f_main_s, f_card_s);
    }
}

// =============================================================================
// Geste de swipe (page_main.on_gesture) : deplace depuis tab5-lvgl.yaml (Phase 3,
// #T164). Comportement IDENTIQUE a l'original - seul l'emplacement change.
// =============================================================================

void handle_swipe_gesture(lv_dir_t dir, lv_coord_t pt_y, int& forecast_page_index,
    lv_obj_t* layer_console_sys, lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5]) {

    if (dir == LV_DIR_TOP) {
        lv_obj_clear_flag(layer_console_sys, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(layer_console_sys);
    } else if (dir == LV_DIR_BOTTOM) {
        lv_obj_add_flag(layer_console_sys, LV_OBJ_FLAG_HIDDEN);
    } else if (pt_y > 400) {
        int page = forecast_page_index;
        // NE PAS "corriger" en wrap 0<->4 : comportement volontaire, deja teste et
        // valide par Axel (revert du 05/07/2026 d'un changement fait a tort suite a
        // un audit LLM qui l'avait signale comme un bug de pagination "confuse").
        // Pages 0-1 = horaire, 2-4 = journalier. LEFT boucle sur 2/3/4 une fois
        // dans le journalier (ne revient pas seul vers l'horaire) ; RIGHT traverse
        // tout vers le bas et boucle 0->2 (retour au debut du journalier, pas un
        // tour complet vers 4).
        if (dir == LV_DIR_LEFT) {
            if (page >= 4) page = 2;
            else page = page + 1;
        } else if (dir == LV_DIR_RIGHT) {
            if (page <= 0) page = 2;
            else page = page - 1;
        }
        forecast_page_index = page;

        if (page >= 2) {
            lv_obj_clear_flag(layer_forecast_daily, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(layer_forecast_hourly, LV_OBJ_FLAG_HIDDEN);
            refresh_daily_forecast(day_slots, page - 2, f_main, f_card, f_main_s, f_card_s);
        } else {
            lv_obj_add_flag(layer_forecast_daily, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(layer_forecast_hourly, LV_OBJ_FLAG_HIDDEN);
            refresh_hourly_forecast(hour_slots, 1 - page, f_main, f_card, f_main_s, f_card_s);
        }

        for (int i = 0; i < 5; i++) {
            if (i == page) {
                lv_obj_set_width(pbars[i], 30);
                lv_obj_set_style_bg_opa(pbars[i], 255, LV_PART_MAIN);
            } else {
                lv_obj_set_width(pbars[i], 16);
                lv_obj_set_style_bg_opa(pbars[i], 100, LV_PART_MAIN);
            }
        }
    }
}

// =============================================================================
// Carte lumiere (epaule j2/j3/j4 + switch associe + popup power) : factorise depuis
// light_chambre_state/light_salon_state/light_led_state (tab5-sensors.yaml, #T164)
// =============================================================================

void update_light_card_ui(lv_obj_t* icon_room, lv_obj_t* icon_light, lv_obj_t* icon_switch,
    lv_obj_t* lbl_switch_state, lv_obj_t* btn_power_icon,
    const std::string& current_light_entity, const std::string& this_entity, bool is_on) {

    if (icon_room == nullptr || icon_light == nullptr) return;

    uint32_t color = is_on ? UIColor::WARNING : UIColor::TEXT_DIM;
    lv_obj_set_style_text_color(icon_room, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_color(icon_light, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(icon_light, is_on ? "\U000F06E8" : "\U000F0335");

    if (icon_switch != nullptr && lbl_switch_state != nullptr) {
        lv_obj_set_style_text_color(icon_switch, lv_color_hex(color), LV_PART_MAIN);
        lv_label_set_text(lbl_switch_state, is_on ? "Allumé" : "Éteint");
        lv_obj_set_style_text_color(lbl_switch_state, lv_color_hex(color), LV_PART_MAIN);
    }
    if (btn_power_icon != nullptr && current_light_entity == this_entity) {
        lv_obj_set_style_text_color(btn_power_icon, lv_color_hex(color), LV_PART_MAIN);
    }
}

// =============================================================================
// Tri dynamique plantes : 5 capteurs -> 4 slots (2 secs + mediane + humide)
// =============================================================================

void sort_and_update_moisture_slots(float values[5], const char* icons_utf8[5],
    MoistureSlotUI slots[4]) {

    // Garde de securite contre les pointeurs nuls si LVGL n'est pas encore initialise
    for (int s = 0; s < 4; s++) {
        if (slots[s].icon_lbl == nullptr || slots[s].val_lbl == nullptr) {
            return;
        }
    }

    // 1) Construire un tableau d'indices valides (pas NaN)
    struct Entry { int idx; float val; };
    Entry valid[5];
    int n_valid = 0;

    for (int i = 0; i < 5; i++) {
        if (!isnan(values[i])) {
            valid[n_valid++] = {i, values[i]};
        }
    }

    // 2) Tri par valeur croissante (bubble sort, max 5 elements)
    for (int i = 0; i < n_valid - 1; i++) {
        for (int j = 0; j < n_valid - i - 1; j++) {
            if (valid[j].val > valid[j+1].val) {
                Entry tmp = valid[j];
                valid[j] = valid[j+1];
                valid[j+1] = tmp;
            }
        }
    }

    // 3) Selectionner les 4 indices a afficher :
    //    - slot 0 : le plus sec (valid[0])
    //    - slot 1 : le 2e plus sec (valid[1])
    //    - slot 2 : la mediane (valid[n_valid/2])
    //    - slot 3 : le plus humide (valid[n_valid-1])
    int selected[4] = {-1, -1, -1, -1};
    if (n_valid >= 4) {
        selected[0] = 0;
        selected[1] = 1;
        selected[2] = n_valid / 2;
        selected[3] = n_valid - 1;
        // Eviter les doublons si mediane == slot 1 ou slot 3
        if (selected[2] <= selected[1]) selected[2] = selected[1] + 1;
        if (selected[2] >= selected[3] && selected[3] > 0) selected[2] = selected[3] - 1;
    } else if (n_valid == 3) {
        selected[0] = 0; selected[1] = 1; selected[2] = 1; selected[3] = 2;
    } else if (n_valid == 2) {
        selected[0] = 0; selected[1] = 0; selected[2] = 1; selected[3] = 1;
    } else if (n_valid == 1) {
        selected[0] = 0; selected[1] = 0; selected[2] = 0; selected[3] = 0;
    }

    // 4) Mise a jour des 4 slots LVGL
    for (int s = 0; s < 4; s++) {
        if (selected[s] < 0 || selected[s] >= n_valid) {
            // Slot vide (pas assez de capteurs)
            lv_label_set_text(slots[s].val_lbl, "");
            lv_obj_set_style_text_color(slots[s].icon_lbl, lv_color_hex(UIColor::INACTIVE), LV_PART_MAIN);
            lv_obj_set_style_text_color(slots[s].val_lbl, lv_color_hex(UIColor::INACTIVE), LV_PART_MAIN);
            continue;
        }

        Entry& e = valid[selected[s]];
        // Icone du capteur d'origine
        lv_label_set_text(slots[s].icon_lbl, icons_utf8[e.idx]);

        // Texte sous l'icone : "Pot X" ou "Moy:"
        if (s == 2) {
            lv_label_set_text(slots[s].val_lbl, "Moy:");
        } else {
            char buf[16];
            sprintf(buf, "Pot %d", e.idx + 1);
            lv_label_set_text(slots[s].val_lbl, buf);
        }

        // Couleur colorimetrique
        uint32_t c = get_humidity_color(e.val);
        lv_obj_set_style_text_color(slots[s].icon_lbl, lv_color_hex(c), LV_PART_MAIN);
        lv_obj_set_style_text_color(slots[s].val_lbl, lv_color_hex(UIColor::TEXT_DIM), LV_PART_MAIN);
    }
}

// Met a jour un label de temperature (texte + couleur gradient). Factorise
// depuis temp_serre/temp_salon (tab5-sensors.yaml, Phase 3, #T164).
void update_temp_ui(lv_obj_t* label, float x) {
    if (label == nullptr) return;
    if (isnan(x)) {
        lv_label_set_text(label, "-- \xC2\xB0");
        lv_obj_set_style_text_color(label, lv_color_hex(UIColor::TEXT_DIM), LV_PART_MAIN);
    } else {
        char buf[32];
        sprintf(buf, "%.1f \xC2\xB0", x);
        lv_label_set_text(label, buf);
        uint32_t c_int = get_temperature_color(x);
        lv_obj_set_style_text_color(label, lv_color_hex(c_int), LV_PART_MAIN);
    }
}

// Met a jour les widgets de la console diagnostic (SRAM/PSRAM/frag/loop/IP/SSID).
// Factorise depuis l'interval 2s de tab5-sensors.yaml (Phase 3, #T164). Le garde
// "console visible ?" reste dans le YAML (evite de passer layer_console_sys ici).
void update_console_diagnostics_ui(lv_obj_t* lbl_sram, lv_obj_t* bar_sram,
    lv_obj_t* lbl_psram, lv_obj_t* bar_psram, lv_obj_t* lbl_frag, lv_obj_t* lbl_flash,
    bool loop_time_has_state, float loop_time, lv_obj_t* lbl_loop,
    bool wifi_ip_has_state, const char* wifi_ip, lv_obj_t* lbl_ip,
    bool wifi_ssid_has_state, const char* wifi_ssid, lv_obj_t* lbl_ssid) {

    if (lbl_sram != nullptr) {
        #ifdef USE_ESP_IDF
        float sram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024.0f;
        float sram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024.0f;
        float psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024.0f / 1024.0f;
        float psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024.0f / 1024.0f;
        float frag = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024.0f;
        #else
        float sram_free = ESP.getFreeHeap() / 1024.0f;
        float sram_total = ESP.getHeapSize() / 1024.0f;
        float psram_free = ESP.getFreePsram() / (1024.0f * 1024.0f);
        float psram_total = ESP.getPsramSize() / (1024.0f * 1024.0f);
        float frag = ESP.getMaxAllocHeap() / 1024.0f;
        #endif

        float sram_used = sram_total - sram_free;
        if (sram_used < 0) sram_used = 0;
        float psram_used = psram_total - psram_free;
        if (psram_used < 0) psram_used = 0;

        int sram_pct = (int)((sram_used / sram_total) * 100.0f);
        int psram_pct = (int)((psram_used / psram_total) * 100.0f);

        char b_sram[32]; sprintf(b_sram, "%d%% (%.1f KB)", sram_pct, sram_used);
        lv_label_set_text(lbl_sram, b_sram);
        lv_bar_set_value(bar_sram, sram_pct, LV_ANIM_ON);

        char b_psram[32]; sprintf(b_psram, "%d%% (%.2f MB)", psram_pct, psram_used);
        lv_label_set_text(lbl_psram, b_psram);
        lv_bar_set_value(bar_psram, psram_pct, LV_ANIM_ON);

        auto set_bar_color = [](lv_obj_t* bar, int pct) {
            lv_color_t color = lv_color_hex(UIColor::SUCCESS); // Vert (Normal)
            if (pct > 50) color = lv_color_hex(UIColor::INFO); // Bleu (Bien-Rempli)
            if (pct > 75) color = lv_color_hex(UIColor::WARNING); // Orange (Attention)
            if (pct > 90) color = lv_color_hex(UIColor::ERROR); // Rouge (Critique)
            lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
        };
        set_bar_color(bar_sram, sram_pct);
        set_bar_color(bar_psram, psram_pct);

        char b_frag[32]; sprintf(b_frag, "%.1f KB", frag);
        lv_label_set_text(lbl_frag, b_frag);
        lv_label_set_text(lbl_flash, "16.0 MB");
    }

    if (loop_time_has_state && lbl_loop != nullptr) {
        char buf[32];
        sprintf(buf, "%.0f ms", loop_time);
        lv_label_set_text(lbl_loop, buf);
    }

    if (wifi_ip_has_state && lbl_ip != nullptr) {
        lv_label_set_text(lbl_ip, wifi_ip);
    }

    if (wifi_ssid_has_state && lbl_ssid != nullptr) {
        lv_label_set_text(lbl_ssid, wifi_ssid);
    }
}

// Callback d'animation d'opacite : signature compatible lv_anim_exec_xcb_t (void*, int32_t).
// lv_obj_set_style_opa() prend 3 arguments et ne peut donc pas etre castee directement.
static void anim_opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, LV_PART_MAIN);
}

// Transition "verre depoli" : glissement vertical + fondu croise.
//   - Sortie : descend en accelerant (ease_in) tout en s'effacant.
//   - Entree : arrive du haut en decelerant (ease_out) tout en apparaissant.
// Duree allongee a 450ms pour une sensation plus fluide/posee.
void transition_widgets(lv_obj_t* out_obj, lv_obj_t* in_obj) {
    if (out_obj == in_obj) return;

    const uint32_t DUR    = 450;  // ms
    const int32_t  OFFSET = 84;   // px de glissement

    if (out_obj) {
        // Glissement vertical sortant (ease_in : accelere en quittant l'ecran)
        lv_anim_t a_out_y;
        lv_anim_init(&a_out_y);
        lv_anim_set_var(&a_out_y, out_obj);
        lv_anim_set_values(&a_out_y, 0, OFFSET);
        lv_anim_set_time(&a_out_y, DUR);
        lv_anim_set_path_cb(&a_out_y, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&a_out_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_ready_cb(&a_out_y, [](lv_anim_t* a) {
            // Masque puis remet l'objet a son etat neutre pour sa prochaine apparition
            lv_obj_t* o = (lv_obj_t*)a->var;
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(o, 0);
            lv_obj_set_style_opa(o, LV_OPA_COVER, LV_PART_MAIN);
        });
        lv_anim_start(&a_out_y);

        // Fondu sortant synchronise
        lv_anim_t a_out_o;
        lv_anim_init(&a_out_o);
        lv_anim_set_var(&a_out_o, out_obj);
        lv_anim_set_values(&a_out_o, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&a_out_o, DUR);
        lv_anim_set_path_cb(&a_out_o, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&a_out_o, anim_opa_cb);
        lv_anim_start(&a_out_o);
    }

    if (in_obj) {
        // Etat de depart : au-dessus et transparent
        lv_obj_clear_flag(in_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(in_obj, -OFFSET);
        lv_obj_set_style_opa(in_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        // Glissement vertical entrant (ease_out : decelere en se posant -> fluide)
        lv_anim_t a_in_y;
        lv_anim_init(&a_in_y);
        lv_anim_set_var(&a_in_y, in_obj);
        lv_anim_set_values(&a_in_y, -OFFSET, 0);
        lv_anim_set_time(&a_in_y, DUR);
        lv_anim_set_path_cb(&a_in_y, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_in_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_start(&a_in_y);

        // Fondu entrant synchronise
        lv_anim_t a_in_o;
        lv_anim_init(&a_in_o);
        lv_anim_set_var(&a_in_o, in_obj);
        lv_anim_set_values(&a_in_o, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a_in_o, DUR);
        lv_anim_set_path_cb(&a_in_o, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_in_o, anim_opa_cb);
        lv_anim_start(&a_in_o);
    }
}
