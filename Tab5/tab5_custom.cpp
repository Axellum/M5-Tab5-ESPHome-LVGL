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
#include <ctime>
#include <cstring>


// =============================================================================
// UTF-8 : normalisation des textes HA (Latin-1 / mojibake) avant affichage LVGL
// =============================================================================

static bool is_valid_utf8(const std::string& s) {
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            i++;
            continue;
        }
        size_t need = 0;
        if ((c & 0xE0) == 0xC0) need = 1;
        else if ((c & 0xF0) == 0xE0) need = 2;
        else if ((c & 0xF8) == 0xF0) need = 3;
        else return false;
        if (i + need >= s.size()) return false;
        for (size_t j = 1; j <= need; j++) {
            if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) return false;
        }
        i += need + 1;
    }
    return true;
}

static std::string latin1_to_utf8(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);
    for (unsigned char c : in) {
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else {
            uint32_t cp = c;
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

static std::string fix_utf8_mojibake(std::string s) {
    struct Rep { const char* bad; const char* good; };
    static const Rep reps[] = {
        {"\xC3\x83\xC2\xA9", "\xC3\xA9"},  // Ã© -> é
        {"\xC3\x83\xC2\xA8", "\xC3\xA8"},  // Ã¨ -> è
        {"\xC3\x83\xC2\xAA", "\xC3\xAA"},  // Ãª -> ê
        {"\xC3\x83\xC2\xA0", "\xC3\xA0"},  // Ã  -> à
        {"\xC3\x83\xC2\xB4", "\xC3\xB4"},  // Ã´ -> ô
        {"\xC3\x83\xC2\xBB", "\xC3\xBB"},  // Ã» -> û
        {"\xC3\x83\xC2\xA7", "\xC3\xA7"},  // Ã§ -> ç
    };
    for (const auto& r : reps) {
        const size_t bad_len = strlen(r.bad);
        const size_t good_len = strlen(r.good);
        size_t pos = 0;
        while ((pos = s.find(r.bad, pos)) != std::string::npos) {
            s.replace(pos, bad_len, r.good);
            pos += good_len;
        }
    }
    return s;
}

static std::string normalize_text_utf8(const std::string& in) {
    if (in.empty()) return in;
    std::string t = is_valid_utf8(in) ? in : latin1_to_utf8(in);
    return fix_utf8_mojibake(std::move(t));
}

static const char* vigilance_alert_banner_utf8(const std::string& couleur) {
    if (couleur.find("Rouge") != std::string::npos) {
        return "Alerte M\xC3\xA9t\xC3\xA9o Rouge en cours ! Restez prudent.";
    }
    if (couleur.find("Orange") != std::string::npos) {
        return "Alerte M\xC3\xA9t\xC3\xA9o Orange en cours ! Restez prudent.";
    }
    return nullptr;
}

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

// Titre court "Lun 16" pour les pages journalieres 2 et 3 (page_index 1/2).
// Utilise l'heure systeme SNTP (timezone Europe/Paris configuree dans tab5-sensors-diagnostics.yaml).
static std::string format_short_day_label(int jour_offset) {
    static const char* days[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
    time_t raw = time(nullptr);
    if (raw <= 0 || jour_offset < 0 || jour_offset >= 15) return "";
    raw += static_cast<time_t>(jour_offset) * 86400;
    struct tm t;
    if (localtime_r(&raw, &t) == nullptr) return "";
    char buf[12];
    snprintf(buf, sizeof(buf), "%s %02d", days[t.tm_wday], t.tm_mday);
    return std::string(buf);
}

void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, const std::string& state, bool is_card, esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s) {
    std::string l1_text = MeteoIcon::CLOUD; // Nuage par defaut
    uint32_t l1_color = UIColor::TEXT_PRIMARY;
    std::string l2_text = "";
    uint32_t l2_color = UIColor::TEXT_PRIMARY;
    int l2_x = 0; int l2_y = 0; int l1_y = 0;
    bool l2_small = false; bool l2_behind = false;

    // Dictionnaire type Classe CSS avec position de base (Grosse icone)
    if (state == "clear-night") { l1_text = MeteoIcon::MOON; l1_color = UIColor::METEO_CELESTIAL; }
    else if (state == "cloudy") { l1_text = MeteoIcon::CLOUD; }
    else if (state == "fog") { l1_text = MeteoIcon::FOG; }
    else if (state == "Clear" || state == "sunny") { l1_text = MeteoIcon::SUNNY; l1_color = UIColor::METEO_CELESTIAL; }
    else if (state == "partlycloudy" || state == "partlycloudy-night" || state == "partlycloudy_night") {
        l1_text = MeteoIcon::CLOUD; 
        l2_text = (state == "partlycloudy") ? MeteoIcon::SUNNY : MeteoIcon::MOON;
        l2_small = true; l2_color = UIColor::METEO_CELESTIAL; l2_behind = true;
        l2_x = -45; l2_y = -45;
    }
    else if (state == "hail" || state == "snowy-rainy") { l2_text = MeteoIcon::HAIL; l2_color = UIColor::METEO_PRECIP; l2_behind = true; l1_y = -30; }
    else if (state == "lightning" || state == "thunder" || state == "lightning-rainy") { l2_text = MeteoIcon::THUNDER; l2_color = UIColor::METEO_THUNDER; l2_behind = true; l1_y = -30; }
    else if (state == "pouring") { l2_text = MeteoIcon::HEAVY_RAIN; l2_color = UIColor::METEO_PRECIP; l2_behind = true; l1_y = -30; }
    else if (state == "rainy") { l2_text = MeteoIcon::RAIN; l2_color = UIColor::METEO_PRECIP; l2_behind = true; l1_y = -30; }
    else if (state == "snowy") { l2_text = MeteoIcon::SNOW; l2_color = UIColor::METEO_PRECIP; l2_behind = true; l1_y = -30; }
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
    if (isnan(x)) return UIColor::MOISTURE_NAN;
    int val = (int)x;
    if (val <= 14) return UIColor::ALERT_RED;
    if (val >= 80) return UIColor::HUMIDITY_WET;
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
    if (isnan(t)) return UIColor::TEMP_NAN;
    if (t <= -12) return UIColor::ALERT_RED;
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

        // Titre : page accueil (0) = nom_jour HA ; pages 2-3 = "Lun 16" via SNTP
        if (cal_toggled[jour]) {
            lv_label_set_text(slot.day_lbl, cal_heures[jour].c_str());
        } else if (page_index > 0) {
            std::string date_lbl = format_short_day_label(jour);
            lv_label_set_text(slot.day_lbl, date_lbl.empty() ? data.nom_jour.c_str() : date_lbl.c_str());
        } else {
            lv_label_set_text(slot.day_lbl, data.nom_jour.c_str());
        }

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
        uint32_t col = UIColor::TEXT_PRIMARY;
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
        lv_obj_set_style_text_color(slot.max_lbl, lv_color_hex(UIColor::TEXT_PRIMARY), LV_PART_MAIN);
        lv_obj_set_style_text_color(slot.min_lbl, lv_color_hex(UIColor::TEXT_PRIMARY), LV_PART_MAIN);

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
        lv_obj_set_style_text_color(slot.temp_lbl, lv_color_hex(UIColor::TEXT_PRIMARY), LV_PART_MAIN);

        char b_p[32];
        if (data.pluvio > 0) {
            sprintf(b_p, "%.1fmm", data.pluvio);
            lv_label_set_text(slot.prob_lbl, b_p);
            lv_obj_set_style_text_color(slot.prob_lbl, lv_color_hex(UIColor::METEO_PRECIP), LV_PART_MAIN);
        } else {
            lv_label_set_text(slot.prob_lbl, "-");
            lv_obj_set_style_text_color(slot.prob_lbl, lv_color_hex(UIColor::CLIM_TRACK_INACTIVE), LV_PART_MAIN);
        }

        update_meteo_icon(slot.icon_l1, slot.icon_l2, data.condition, true, f_main, f_card, f_main_s, f_card_s);
    }
}

// =============================================================================
// Geste de swipe (page_main.on_gesture) : pagination previsions (y >= carte centrale)
// =============================================================================

static constexpr lv_coord_t FORECAST_SWIPE_Y_MIN = 333;  // haut de central_card (tab5-lvgl.yaml)

// Recolor LVGL : vrai seulement si markup #RRGGBB (évite faux positifs sur '#' isolé).
static bool has_lvgl_recolor_markup(const std::string& t) {
    for (size_t i = 0; i + 7 < t.size(); ++i) {
        if (t[i] != '#') continue;
        bool hex6 = true;
        for (int j = 1; j <= 6; ++j) {
            char c = t[i + static_cast<size_t>(j)];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                hex6 = false;
                break;
            }
        }
        if (hex6) return true;
    }
    return false;
}

static void set_label_text_utf8(lv_obj_t* label, const char* text) {
    if (!label || !text) return;
    std::string t(text);
    lv_label_set_recolor(label, has_lvgl_recolor_markup(t));
    lv_label_set_text(label, text);
}

static const char* clock_month_short_utf8(int month) {
    static const char* months[] = {
        "Janv", "F\xC3\xA9vr", "Mars", "Avr", "Mai", "Juin", "Juil",
        "Ao\xC3\xBBt", "Sept", "Oct", "Nov", "D\xC3\xA9" "c"
    };
    if (month < 1 || month > 12) return "";
    return months[month - 1];
}

static const char* forecast_page_title_text(int page) {
    switch (page) {
        // forecast_page_index 0/1 utilisent refresh_hourly(..., 1-page) : données
        // inversées par rapport à l'index UI — titres alignés sur le contenu réel.
        // UTF-8 explicite (\xC3\xA9 = é, \xC3\xA8 = è) — pas Latin-1 (\xE9).
        case 0: return "Pr\xC3\xA9visions horaires 2";
        case 1: return "Pr\xC3\xA9visions horaires 1";
        case 3: return "Pr\xC3\xA9visions journali\xC3\xA8res 2";
        case 4: return "Pr\xC3\xA9visions journali\xC3\xA8res 3";
        default: return nullptr;
    }
}

void update_central_forecast_page_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    lv_obj_t* planning_wrap, lv_obj_t* rain_wrap, lv_obj_t* alert_cont, lv_obj_t* info_wrap,
    int current_panel) {

    if (!page_title_wrap || !lbl_page_title) return;

    if (planning_wrap) lv_obj_add_flag(planning_wrap, LV_OBJ_FLAG_HIDDEN);
    if (rain_wrap) lv_obj_add_flag(rain_wrap, LV_OBJ_FLAG_HIDDEN);
    if (alert_cont) lv_obj_add_flag(alert_cont, LV_OBJ_FLAG_HIDDEN);
    if (info_wrap) lv_obj_add_flag(info_wrap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);

    if (forecast_page == 2) {
        lv_obj_t* active = planning_wrap;
        if (current_panel == 1) active = rain_wrap;
        else if (current_panel == 2) active = alert_cont;
        else if (current_panel == 3) active = info_wrap;
        if (active) lv_obj_clear_flag(active, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const char* title = forecast_page_title_text(forecast_page);
    if (!title) return;
    lv_label_set_text(lbl_page_title, title);
    lv_obj_clear_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);
}

void update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, bool& has_info, int& current_panel,
    esphome::font::Font* font_small, esphome::font::Font* font_large) {

    if (!lbl_info) return;

    std::string t = texte;
    const char* ws = " \t\r\n";
    size_t deb = t.find_first_not_of(ws);
    t = (deb == std::string::npos) ? "" : t.substr(deb, t.find_last_not_of(ws) - deb + 1);

    // Banniere vigilance : texte fixe UTF-8 cote firmware (HA ne fournit que la couleur).
    if (const char* banner = vigilance_alert_banner_utf8(couleur)) {
        t = banner;
    } else if (!t.empty()) {
        t = normalize_text_utf8(t);
    }

    has_info = !t.empty();
    if (t.empty()) {
        lv_label_set_text(lbl_info, "");
        if (current_panel == 3 && info_wrap && planning_wrap) {
            transition_widgets(info_wrap, planning_wrap);
            current_panel = 0;
        }
        return;
    }

    bool multi_ligne = t.find('\n') != std::string::npos;
    bool has_recolor_markup = has_lvgl_recolor_markup(t);
    esphome::font::Font* font = multi_ligne ? font_small : font_large;
    if (font) {
        esphome::lvgl::lv_obj_set_style_text_font(lbl_info, font, LV_PART_MAIN);
    }

    uint32_t c = UIColor::TEXT_PRIMARY;
    if (couleur.find("Rouge") != std::string::npos) c = UIColor::ALERT_RED;
    else if (couleur.find("Orange") != std::string::npos) c = UIColor::WARNING;
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(c), LV_PART_MAIN);

    lv_label_set_recolor(lbl_info, has_recolor_markup);
    lv_label_set_text(lbl_info, t.c_str());
}

void update_rain_phrase_ui(lv_obj_t* lbl, const std::string& phrase) {
    if (!lbl) return;
    std::string t = normalize_text_utf8(phrase);
    lv_label_set_recolor(lbl, false);
    lv_label_set_text(lbl, t.c_str());
}

void update_planning_text_ui(lv_obj_t* lbl, const std::string& l1, const std::string& l2,
    std::string& plan_ligne_1, std::string& plan_ligne_2) {
    if (!lbl) return;
    auto strip_prefix = [](const std::string& s) -> std::string {
        if (s.rfind("1/ ", 0) == 0) return s.substr(3);
        if (s.rfind("2/ ", 0) == 0) return s.substr(3);
        if (s.rfind("1/", 0) == 0) return s.substr(2);
        if (s.rfind("2/", 0) == 0) return s.substr(2);
        return s;
    };
    std::string line1 = strip_prefix(l1);
    std::string line2 = strip_prefix(l2);
    plan_ligne_1 = line1;
    plan_ligne_2 = line2;
    std::string combined = line1;
    if (!line2.empty()) {
        combined += "   |   " + line2;
    }
    combined = normalize_text_utf8(combined);
    set_label_text_utf8(lbl, combined.c_str());
}

void update_clock_date_ui(lv_obj_t* lbl_time, lv_obj_t* lbl_date,
    int hour, int minute, int day_of_week, int day_of_month, int month) {
    if (lbl_time) {
        char buf_time[16];
        snprintf(buf_time, sizeof(buf_time), "%02d:%02d", hour, minute);
        lv_label_set_text(lbl_time, buf_time);
    }
    if (lbl_date) {
        static const char* days[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
        const char* day = (day_of_week >= 1 && day_of_week <= 7) ? days[day_of_week - 1] : "";
        char buf_date[64];
        snprintf(buf_date, sizeof(buf_date), "%s %02d %s", day, day_of_month, clock_month_short_utf8(month));
        lv_label_set_recolor(lbl_date, false);
        lv_label_set_text(lbl_date, buf_date);
    }
}

void handle_swipe_gesture(lv_dir_t dir, lv_coord_t pt_y, int& forecast_page_index,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    lv_obj_t* planning_wrap, lv_obj_t* rain_wrap, lv_obj_t* alert_cont, lv_obj_t* info_wrap,
    int current_panel) {

    if (pt_y < FORECAST_SWIPE_Y_MIN) return;
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;

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

        update_central_forecast_page_ui(page, page_title_wrap, lbl_page_title,
            planning_wrap, rain_wrap, alert_cont, info_wrap, current_panel);
}

// =============================================================================
// Planning jour au tap sur tuile météo (carte centrale 6s)
// =============================================================================

std::string get_day_planning_display_text(int jour) {
    if (jour < 0 || jour >= 15) return "Jour hors plage";
    if (!cal_heures[jour].empty()) return cal_heures[jour];
    const DayForecastData& d = cal_jours_data[jour];
    if (!d.heures_ouverture.empty()) {
        if (!d.nom_jour.empty()) return d.nom_jour + " : " + d.heures_ouverture;
        return d.heures_ouverture;
    }
    if (!d.nom_jour.empty()) return d.nom_jour + " : pas d'horaire";
    return "Pas de planning pour ce jour";
}

static lv_timer_t* planning_restore_timer = nullptr;
static std::string static_plan_l1;
static std::string static_plan_l2;
static lv_obj_t* static_lbl_planning = nullptr;
static bool* static_is_showing_temp = nullptr;
static int static_forecast_page_restore = 2;
static lv_obj_t* static_page_title_wrap = nullptr;
static lv_obj_t* static_lbl_page_title = nullptr;
static lv_obj_t* static_planning_wrap = nullptr;
static lv_obj_t* static_rain_wrap = nullptr;
static lv_obj_t* static_alert_cont = nullptr;
static lv_obj_t* static_info_wrap = nullptr;
static int static_central_panel_restore = 0;

static void planning_restore_timer_cb(lv_timer_t* timer) {
    if (static_is_showing_temp) {
        *static_is_showing_temp = false;
    }
    if (static_forecast_page_restore != 2) {
        update_central_forecast_page_ui(static_forecast_page_restore,
            static_page_title_wrap, static_lbl_page_title,
            static_planning_wrap, static_rain_wrap, static_alert_cont, static_info_wrap,
            static_central_panel_restore);
    } else if (static_lbl_planning) {
        std::string combined = static_plan_l1;
        if (!static_plan_l2.empty()) {
            combined += "   |   " + static_plan_l2;
        }
        lv_label_set_text(static_lbl_planning, combined.c_str());
    }
    lv_timer_del(timer);
    planning_restore_timer = nullptr;
}

void show_temporary_planning(int jour, lv_obj_t* lbl_planning, lv_obj_t* planning_wrap, lv_obj_t* alert_cont, lv_obj_t* rain_wrap,
                             lv_obj_t* info_wrap, lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, int forecast_page,
                             const std::string& plan_l1, const std::string& plan_l2, bool& is_showing_temp, int& current_panel) {
    if (!lbl_planning) return;

    static_central_panel_restore = current_panel;
    is_showing_temp = true;
    current_panel = 0;

    std::string text = get_day_planning_display_text(jour);
    lv_label_set_text(lbl_planning, text.c_str());

    // Stoppe les animations LVGL en cours sur les panneaux centraux (evite conflit avec le rotateur 8s).
    if (planning_wrap) lv_anim_del(planning_wrap, nullptr);
    if (alert_cont) lv_anim_del(alert_cont, nullptr);
    if (rain_wrap) lv_anim_del(rain_wrap, nullptr);
    if (info_wrap) lv_anim_del(info_wrap, nullptr);
    if (page_title_wrap) lv_anim_del(page_title_wrap, nullptr);

    if (page_title_wrap) lv_obj_add_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);
    if (planning_wrap) lv_obj_clear_flag(planning_wrap, LV_OBJ_FLAG_HIDDEN);
    if (alert_cont) lv_obj_add_flag(alert_cont, LV_OBJ_FLAG_HIDDEN);
    if (rain_wrap) lv_obj_add_flag(rain_wrap, LV_OBJ_FLAG_HIDDEN);
    if (info_wrap) lv_obj_add_flag(info_wrap, LV_OBJ_FLAG_HIDDEN);

    static_plan_l1 = plan_l1;
    static_plan_l2 = plan_l2;
    static_lbl_planning = lbl_planning;
    static_is_showing_temp = &is_showing_temp;
    static_forecast_page_restore = forecast_page;
    static_page_title_wrap = page_title_wrap;
    static_lbl_page_title = lbl_page_title;
    static_planning_wrap = planning_wrap;
    static_rain_wrap = rain_wrap;
    static_alert_cont = alert_cont;
    static_info_wrap = info_wrap;

    if (planning_restore_timer != nullptr) {
        lv_timer_del(planning_restore_timer);
        planning_restore_timer = nullptr;
    }

    planning_restore_timer = lv_timer_create(planning_restore_timer_cb, 6000, nullptr);
}

// =============================================================================
// Carte lumiere (epaule j2/j3/j4 + switch associe + popup power) : factorise depuis
// light_chambre_state/light_salon_state/light_led_state (tab5-sensors-domotique.yaml, #T164)
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
// depuis temp_serre/temp_salon (tab5-sensors-domotique.yaml, Phase 3, #T164).
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

// =============================================================================
// Console diagnostic — ligne status (uptime / Wi-Fi / temp CPU), garde #T222
// =============================================================================

bool is_console_layer_visible(lv_obj_t* layer_console) {
    return layer_console != nullptr && !lv_obj_has_flag(layer_console, LV_OBJ_FLAG_HIDDEN);
}

void update_console_uptime_label(lv_obj_t* label, float uptime_s) {
    if (label == nullptr) return;
    int total = (int)uptime_s;
    int days = total / 86400;
    int hours = (total % 86400) / 3600;
    int mins = (total % 3600) / 60;
    char buf[32];
    if (days > 0) {
        sprintf(buf, "%dj %02dh%02d", days, hours, mins);
    } else {
        sprintf(buf, "%02dh%02d", hours, mins);
    }
    lv_label_set_text(label, buf);
}

void update_console_rssi_label(lv_obj_t* label, float rssi_dbm) {
    if (label == nullptr) return;
    char buf[16];
    sprintf(buf, "%.0f dBm", rssi_dbm);
    lv_label_set_text(label, buf);
}

void update_console_temp_label(lv_obj_t* label, float core_temp_c) {
    if (label == nullptr) return;
    char buf[16];
    sprintf(buf, "%.1f \xC2\xB0", core_temp_c);
    lv_label_set_text(label, buf);
}

void refresh_console_status_row_ui(lv_obj_t* lbl_uptime, lv_obj_t* lbl_rssi, lv_obj_t* lbl_temp,
    bool has_uptime, float uptime_s, bool has_rssi, float rssi_dbm, bool has_temp, float core_temp_c) {
    if (has_uptime) update_console_uptime_label(lbl_uptime, uptime_s);
    if (has_rssi) update_console_rssi_label(lbl_rssi, rssi_dbm);
    if (has_temp) update_console_temp_label(lbl_temp, core_temp_c);
}

// Met a jour les widgets de la console diagnostic (SRAM/PSRAM/frag/loop/IP/SSID).
// Factorise depuis l'interval 2s de tab5-sensors-diagnostics.yaml (Phase 3, #T164). Le garde
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

// Callback d'animation de position Y (#T225 : evite cast ABI lv_obj_set_y).
static void anim_y_cb(void* obj, int32_t v) {
    lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)v);
}

static void anim_out_y_ready_cb(lv_anim_t* a) {
    lv_obj_t* o = (lv_obj_t*)a->var;
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(o, 0);
    lv_obj_set_style_opa(o, LV_OPA_COVER, LV_PART_MAIN);
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
        lv_anim_set_exec_cb(&a_out_y, anim_y_cb);
        lv_anim_set_ready_cb(&a_out_y, anim_out_y_ready_cb);
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
        lv_anim_set_exec_cb(&a_in_y, anim_y_cb);
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
