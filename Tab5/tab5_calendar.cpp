/**
 * [AI-CONTEXT]
 * @file tab5_calendar.cpp
 * @role Popup calendrier mensuel : cache des mois (TTL, éviction), rendu de la grille
 *       7×6, détail d'un jour (chargement + rendu des lignes).
 *       Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Éviter std::string dans les boucles de parsing ; char* + strtok_r.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// =============================================================================
// Popup calendrier mensuel (calendar_popup.yaml, appui long sur l'horloge)
// =============================================================================

struct CalMonthData {
    std::string codes;    // 62 hex (2/jour) — bits CAL_BIT_* de tab5_custom.h
    std::string heures;   // 31 champs "HH:MM-HH:MM" séparés par | (vides autorisés)
    std::string details;  // 31 champs "type|texte;..." séparés par ~ (vide = rien prévu)
    bool has_details = false;  // true si HA a fourni le champ details (même tout vide)
    uint32_t stored_at = 0;    // millis() au stockage — pour le TTL stale-while-revalidate
};

// Cache par mois (clé = annee*12 + mois-1). Stratégie stale-while-revalidate :
// les données sont affichées immédiatement même si périmées, un refresh silencieux
// est lancé en parallèle. Eviction : max 3 mois (M-1, M, M+1) autour de la vue courante.
static std::map<int, CalMonthData> s_cal_month_cache;

static int cal_cache_key(int y, int m) { return y * 12 + (m - 1); }

void cal_cache_clear() { s_cal_month_cache.clear(); }

bool cal_month_needs_fetch(int year, int month) {
    return s_cal_month_cache.find(cal_cache_key(year, month)) == s_cal_month_cache.end();
}

bool cal_month_is_stale(int year, int month, uint32_t ttl_ms) {
    const auto it = s_cal_month_cache.find(cal_cache_key(year, month));
    if (it == s_cal_month_cache.end()) return true;  // absent = périmé
    return (esphome::millis() - it->second.stored_at) > ttl_ms;
}

void cal_cache_evict_distant(int year, int month) {
    // Garde uniquement les mois dans [M-1, M+1] autour de la vue courante.
    const int center = cal_cache_key(year, month);
    for (auto it = s_cal_month_cache.begin(); it != s_cal_month_cache.end(); ) {
        if (abs(it->first - center) > 1) it = s_cal_month_cache.erase(it);
        else ++it;
    }
}

void cal_store_month_data(const std::string& annee, const std::string& mois,
    const std::string& codes, const std::string& heures, const std::string& details) {
    const int y = atoi(annee.c_str());
    const int m = atoi(mois.c_str());
    if (y < 2000 || y > 2100 || m < 1 || m > 12) return;
    CalMonthData data;
    data.codes = codes;
    data.heures = heures;
    data.details = details;
    // HA envoie toujours 30× '~' minimum (31 champs). Absent / "" = vieux HA → fallback jour.
    data.has_details = !details.empty();
    data.stored_at = esphome::millis();
    s_cal_month_cache[cal_cache_key(y, m)] = data;
}

bool cal_month_has_details(int year, int month) {
    const auto it = s_cal_month_cache.find(cal_cache_key(year, month));
    return it != s_cal_month_cache.end() && it->second.has_details;
}

// n-ième champ d'une chaîne délimitée par un séparateur — champs vides autorisés
static std::string cal_field_delim(const std::string& s, int idx, char delim) {
    size_t start = 0;
    for (int i = 0; i < idx; i++) {
        const size_t p = s.find(delim, start);
        if (p == std::string::npos) return "";
        start = p + 1;
    }
    size_t end = s.find(delim, start);
    if (end == std::string::npos) end = s.size();
    return s.substr(start, end - start);
}

std::string cal_cached_day_detail(int year, int month, int day) {
    if (day < 1 || day > 31) return "";
    const auto it = s_cal_month_cache.find(cal_cache_key(year, month));
    if (it == s_cal_month_cache.end() || !it->second.has_details) return "";
    return cal_field_delim(it->second.details, day - 1, '~');
}

bool cal_day_has_embedded_detail(int year, int month, int day) {
    // Champ ~ vide ≠ « rien de prévu confirmé » : avec get_events borné à aujourd'hui,
    // les jours passés n'ont souvent pas de détail embarqué → fallback script _jour.
    return !cal_cached_day_detail(year, month, day).empty();
}

static int cal_days_in_month(int y, int m) {
    static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 30;
    int d = dm[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) d = 29;
    return d;
}

// Jour de la semaine (0 = lundi ... 6 = dimanche), algorithme de Sakamoto —
// aucune dépendance à mktime/timezone, valable pour tout le calendrier grégorien.
static int cal_weekday_mon0(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    const int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;  // 0 = dimanche
    return (w + 6) % 7;
}

static const char* cal_month_name_utf8(int month) {
    static const char* months[] = {"Janvier", "F\xC3\xA9vrier", "Mars", "Avril",
        "Mai", "Juin", "Juillet", "Ao\xC3\xBBt", "Septembre", "Octobre",
        "Novembre", "D\xC3\xA9" "cembre"};
    return (month >= 1 && month <= 12) ? months[month - 1] : "";
}

static const char* cal_weekday_name_utf8(int wd_mon0) {
    static const char* days[] = {"Lundi", "Mardi", "Mercredi", "Jeudi",
        "Vendredi", "Samedi", "Dimanche"};
    return (wd_mon0 >= 0 && wd_mon0 <= 6) ? days[wd_mon0] : "";
}

// n-ième champ d'une chaîne délimitée par | — champs vides autorisés
// (strtok_r fusionnerait les séparateurs consécutifs, donc parcours manuel).
static std::string cal_field(const std::string& s, int idx) {
    return cal_field_delim(s, idx, '|');
}

static int cal_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

void cal_render_month(CalCellUI cells[42], lv_obj_t* lbl_month,
    int view_year, int view_month, int today_year, int today_month, int today_day) {
    if (!lbl_month || view_month < 1 || view_month > 12) return;

    char buf[48];
    snprintf(buf, sizeof(buf), "%s %d", cal_month_name_utf8(view_month), view_year);
    lv_label_set_text(lbl_month, buf);

    const int first_col = cal_weekday_mon0(view_year, view_month, 1);
    const int ndays = cal_days_in_month(view_year, view_month);

    const CalMonthData* data = nullptr;
    const auto it = s_cal_month_cache.find(cal_cache_key(view_year, view_month));
    if (it != s_cal_month_cache.end()) data = &it->second;

    const bool has_today = (today_year > 0);
    for (int i = 0; i < 42; i++) {
        const CalCellUI& c = cells[i];
        if (!c.cell || !c.num || !c.sub || !c.dot || !c.dot2) continue;

        const int day = i - first_col + 1;
        if (day < 1 || day > ndays) {
            // Cellule hors mois : tout éteint (le tap est neutralisé par
            // cal_date_for_cell qui renvoie "").
            lv_label_set_text(c.num, "");
            lv_label_set_text(c.sub, "");
            lv_obj_add_flag(c.dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(c.dot2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(c.cell, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_opa(c.cell, LV_OPA_TRANSP, LV_PART_MAIN);
            continue;
        }

        int code = 0;
        std::string heures;
        if (data) {
            if ((int)data->codes.size() >= day * 2) {
                code = cal_hex_val(data->codes[(day - 1) * 2]) * 16
                     + cal_hex_val(data->codes[(day - 1) * 2 + 1]);
            }
            heures = cal_field(data->heures, day - 1);
        }

        snprintf(buf, sizeof(buf), "%d", day);
        lv_label_set_text(c.num, buf);

        const bool is_today = has_today && view_year == today_year
            && view_month == today_month && day == today_day;
        const bool is_past = has_today && (view_year < today_year
            || (view_year == today_year && view_month < today_month)
            || (view_year == today_year && view_month == today_month && day < today_day));
        const int col = i % 7;  // 5-6 = samedi/dimanche

        // Priorités du numéro : aujourd'hui > passé > férié > weekend > normal
        uint32_t num_color = UIColor::TEXT_SOFT;
        if (col >= 5) num_color = UIColor::TEXT_DIM;
        if (code & CAL_BIT_FERIE) num_color = UIColor::ERROR;
        if (is_past) num_color = UIColor::PAST;
        if (is_today) num_color = UIColor::ACCENT;
        lv_obj_set_style_text_color(c.num, lv_color_hex(num_color), LV_PART_MAIN);

        // Heures de travail dans la case (orange si embauche < 9h — même
        // convention que les tuiles / bandeau, estompé si jour passé)
        if (!heures.empty()) {
            lv_label_set_text(c.sub, heures.c_str());
            uint32_t h_color = UIColor::TEXT_SOFT;
            if (cal_is_early_shift(heures)) {
                h_color = UIColor::EARLY;
            }
            if (is_past) h_color = UIColor::PAST;
            lv_obj_set_style_text_color(c.sub, lv_color_hex(h_color), LV_PART_MAIN);
        } else {
            lv_label_set_text(c.sub, "");
        }

        // Fond violet doux = vacances scolaires ; bordure cyan = aujourd'hui
        lv_obj_set_style_bg_opa(c.cell,
            (code & CAL_BIT_VACANCES) ? LV_OPA_30 : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_opa(c.cell,
            is_today ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);

        if (code & CAL_BIT_RDV) lv_obj_clear_flag(c.dot, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(c.dot, LV_OBJ_FLAG_HIDDEN);
        if (code & CAL_BIT_ANNIV) lv_obj_clear_flag(c.dot2, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(c.dot2, LV_OBJ_FLAG_HIDDEN);
    }
}

std::string cal_date_for_cell(int view_year, int view_month, int cell_idx) {
    if (view_month < 1 || view_month > 12 || cell_idx < 0 || cell_idx >= 42) return "";
    const int day = cell_idx - cal_weekday_mon0(view_year, view_month, 1) + 1;
    if (day < 1 || day > cal_days_in_month(view_year, view_month)) return "";
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", view_year, view_month, day);
    return std::string(buf);
}

// Icône MDI + couleur d'une ligne de détail selon son type (payload HA).
static void cal_detail_type_style(const char* type, const char** icon, uint32_t* color) {
    if (strcmp(type, "travail") == 0)       { *icon = "\U000F00D6"; *color = UIColor::INFO; }
    else if (strcmp(type, "ferie") == 0)    { *icon = "\U000F1056"; *color = UIColor::ERROR; }
    else if (strcmp(type, "vacances") == 0) { *icon = "\U000F0474"; *color = UIColor::ACCENT_ALT; }
    else if (strcmp(type, "rdv") == 0)      { *icon = "\U000F00F0"; *color = UIColor::GOLD; }
    else if (strcmp(type, "anniv") == 0)    { *icon = "\U000F00EB"; *color = UIColor::WARM_PINK; }
    else if (strcmp(type, "fete") == 0)     { *icon = "\U000F09D3"; *color = UIColor::TEXT_DIM; }
    else                                    { *icon = "\U000F00F0"; *color = UIColor::TEXT_DIM; }
}

void cal_show_day_detail_loading(lv_obj_t* day_popup, lv_obj_t* lbl_title,
    lv_obj_t* lbl_status, CalDetailLineUI lines[6], const std::string& date_iso,
    bool ha_online) {
    if (!day_popup || !lbl_title || !lbl_status) return;
    int y = 0, m = 0, d = 0;
    if (sscanf(date_iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return;

    char buf[48];
    snprintf(buf, sizeof(buf), "%s %d %s",
        cal_weekday_name_utf8(cal_weekday_mon0(y, m, d)), d, cal_month_name_utf8(m));
    lv_label_set_text(lbl_title, buf);

    lv_label_set_text(lbl_status,
        ha_online ? "Chargement..." : "Home Assistant hors ligne");
    lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 6; i++) {
        if (lines[i].icon) lv_obj_add_flag(lines[i].icon, LV_OBJ_FLAG_HIDDEN);
        if (lines[i].txt) lv_obj_add_flag(lines[i].txt, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(day_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(day_popup);
}

void cal_render_day_detail(const std::string& payload, lv_obj_t* lbl_status,
    CalDetailLineUI lines[6]) {
    if (!lbl_status) return;

    // Payload "type|texte;type|texte;..." construit par script.tab5_calendrier_jour
    // (HA) — textes déjà sanitisés (| et ; remplacés) et limités à 6 lignes.
    char buf[1024];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int line_count = 0;
    char* saveptr;
    char* tok = strtok_r(buf, ";", &saveptr);
    while (tok != nullptr && line_count < 6) {
        char* sep = strchr(tok, '|');
        if (sep != nullptr && *(sep + 1) != '\0'
            && lines[line_count].icon && lines[line_count].txt) {
            *sep = '\0';
            const char* icon;
            uint32_t color;
            cal_detail_type_style(tok, &icon, &color);
            lv_label_set_text(lines[line_count].icon, icon);
            lv_obj_set_style_text_color(lines[line_count].icon, lv_color_hex(color), LV_PART_MAIN);
            const std::string txt = normalize_text_utf8(std::string(sep + 1));
            set_label_text_utf8(lines[line_count].txt, txt.c_str());
            lv_obj_clear_flag(lines[line_count].icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lines[line_count].txt, LV_OBJ_FLAG_HIDDEN);
            line_count++;
        }
        tok = strtok_r(nullptr, ";", &saveptr);
    }

    for (int i = line_count; i < 6; i++) {
        if (lines[i].icon) lv_obj_add_flag(lines[i].icon, LV_OBJ_FLAG_HIDDEN);
        if (lines[i].txt) lv_obj_add_flag(lines[i].txt, LV_OBJ_FLAG_HIDDEN);
    }

    if (line_count == 0) {
        lv_label_set_text(lbl_status, "Rien de pr\xC3\xA9vu ce jour");
        lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    }
}
