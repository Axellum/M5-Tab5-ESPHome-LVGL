/**
 * [AI-CONTEXT]
 * @file tab5_services.cpp
 * @role Services HA (tab5-api-logic.yaml) : volet, vigilance Météo-France, pluie 1 h,
 *       icône neige/pluie, cible clim depuis HA, texte du planning. Logique sortie des
 *       lambdas le 08/09/2026 (lot (a)), à l'identique.
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

// -----------------------------------------------------------------------------
// Services HA (tab5-api-logic.yaml) — logique sortie des lambdas le 08/09/2026.
// Le comportement est celui des anciens lambdas, à l'identique ; seules les
// gardes contre les pointeurs nuls ont été étendues à chaque widget.
// -----------------------------------------------------------------------------

bool update_volet_ui(const std::string& etat, bool target_open, const VoletUI& ui) {
    if (ui.arrow == nullptr) return false;
    const bool has_sw = ui.sw_icon != nullptr && ui.sw_label != nullptr;

    if (etat == "En_mouvement") {
        lv_label_set_text(ui.arrow, "\U000F03E4");
        lv_obj_set_style_text_color(ui.arrow, lv_color_hex(UIColor::INFO), LV_PART_MAIN);
        if (has_sw) {
            lv_obj_set_style_text_color(ui.sw_icon, lv_color_hex(UIColor::INFO), LV_PART_MAIN);
            lv_label_set_text(ui.sw_label, "Mouvement");
            lv_obj_set_style_text_color(ui.sw_label, lv_color_hex(UIColor::INFO), LV_PART_MAIN);
        }
        return true;
    }

    // Au repos : la flèche montre le sens de la dernière commande, en gris.
    lv_label_set_text(ui.arrow, target_open ? "\U000F005D" : "\U000F0045");
    lv_obj_set_style_text_color(ui.arrow, lv_color_hex(UIColor::TEXT_DIM), LV_PART_MAIN);

    if (etat == "Ouvert" || etat == "Partiel" || etat == "open") {
        if (ui.shutter != nullptr) {
            lv_label_set_text(ui.shutter, "\U000F111E");
            lv_obj_set_style_text_color(ui.shutter, lv_color_hex(UIColor::SUCCESS), LV_PART_MAIN);
        }
        if (has_sw) {
            lv_obj_set_style_text_color(ui.sw_icon, lv_color_hex(UIColor::SUCCESS), LV_PART_MAIN);
            lv_label_set_text(ui.sw_label, "Ouvert");
            lv_obj_set_style_text_color(ui.sw_label, lv_color_hex(UIColor::SUCCESS), LV_PART_MAIN);
        }
    } else if (etat == "Ferme" || etat == "closed") {
        if (ui.shutter != nullptr) {
            lv_label_set_text(ui.shutter, "\U000F111C");
            lv_obj_set_style_text_color(ui.shutter, lv_color_hex(UIColor::ERROR), LV_PART_MAIN);
        }
        if (has_sw) {
            lv_obj_set_style_text_color(ui.sw_icon, lv_color_hex(UIColor::TEXT_DIM), LV_PART_MAIN);
            lv_label_set_text(ui.sw_label, "Fermé");
            lv_obj_set_style_text_color(ui.sw_label, lv_color_hex(UIColor::TEXT_DIM), LV_PART_MAIN);
        }
    }
    return false;
}

bool parse_and_update_vigilance(const std::string& payload, const VigilanceUI& ui) {
    if (ui.lbl_phrase == nullptr) return false;
    // 1024 (était 512) : la phrase de vigilance peut être longue, un payload
    // complet dépassait parfois 512 et tronquait les derniers champs (#T165).
    char buf[1024];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // strtok_r saute les champs vides consécutifs ("||"), comme l'ancien lambda :
    // un champ vide décalerait les suivants. Contrat HA inchangé — HA envoie
    // toujours "Vert" plutôt qu'une chaîne vide.
    char* saveptr = nullptr;
    const char* fields[11];
    for (int i = 0; i < 11; i++) {
        char* tok = strtok_r(i == 0 ? buf : nullptr, "|", &saveptr);
        fields[i] = tok ? tok : "";
    }
    const char* phrase_pluie = fields[0];
    const char* globale = fields[1];

    update_rain_phrase_ui(ui.lbl_phrase, std::string(phrase_pluie));
    if (ui.lbl_pluie_val != nullptr)  lv_obj_add_flag(ui.lbl_pluie_val, LV_OBJ_FLAG_HIDDEN);
    if (ui.lbl_pluie_unit != nullptr) lv_obj_add_flag(ui.lbl_pluie_unit, LV_OBJ_FLAG_HIDDEN);

    // Couleur de la date selon la vigilance globale.
    uint32_t col_date = UIColor::SUCCESS;
    if (strcmp(globale, "Jaune") == 0)       col_date = UIColor::ALERT_DATE_YELLOW;
    else if (strcmp(globale, "Orange") == 0) col_date = UIColor::ALERT_DATE_ORANGE;
    else if (strcmp(globale, "Rouge") == 0)  col_date = UIColor::ALERT_DATE_RED;
    if (ui.lbl_date != nullptr) lv_obj_set_style_text_color(ui.lbl_date, lv_color_hex(col_date), LV_PART_MAIN);

    // Phénomènes, dans l'ordre du payload, avec leur glyphe MDI.
    static const char* const kIcons[9] = {
        "\U000F059D",  // vent
        "\U000F0EFA",  // inondation
        "\U000F0593",  // orages
        "\U000F0596",  // pluie-inondation
        "\U000F0F36",  // neige-verglas
        "\U000F0F29",  // grand froid
        "\U000F078D",  // vagues-submersion
        "\U000F0E01",  // canicule
        "\U000F067E",  // avalanches
    };
    struct AlertEntry { const char* icon; const char* level; };
    constexpr size_t MAX_ALERTES = 4;
    AlertEntry actives[MAX_ALERTES];
    size_t active_count = 0;
    for (int i = 0; i < 9 && active_count < MAX_ALERTES; i++) {
        const char* state = fields[2 + i];
        if (strlen(state) == 0 || strcmp(state, "Vert") == 0 || strcmp(state, "unknown") == 0) continue;
        actives[active_count++] = AlertEntry{kIcons[i], state};
    }

    for (size_t i = 0; i < 4; i++) {
        lv_obj_t* slot = ui.slots[i];
        if (slot == nullptr) continue;
        if (i < active_count) {
            lv_label_set_text(slot, actives[i].icon);
            uint32_t c = UIColor::ALERT_YELLOW;
            if (strcmp(actives[i].level, "Orange") == 0)     c = UIColor::WARNING;
            else if (strcmp(actives[i].level, "Rouge") == 0) c = UIColor::ALERT_RED;
            lv_obj_set_style_text_color(slot, lv_color_hex(c), LV_PART_MAIN);
            lv_obj_set_style_text_opa(slot, 255, LV_PART_MAIN);
            lv_obj_clear_flag(slot, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(slot, LV_OBJ_FLAG_HIDDEN);
        }
    }
    return active_count > 0;
}

// Niveau Météo-France → couleur + hauteur (px) d'une barre. Isolé pour le jour
// où un service bulk remplacera les 9 appels par rafraîchissement (audit §4.1
// point 3) : il n'aura que cette table à réutiliser.
static void rain_level_style(const std::string& intensite, uint32_t& color, int& height) {
    color = UIColor::CLIM_TRACK_INACTIVE;  // barre vide
    height = 0;
    if (intensite == "Pluie faible")          { color = UIColor::RAIN_LIGHT;    height = 13; }  // ~1/4 hauteur
    else if (intensite == "Pluie modérée")    { color = UIColor::RAIN_MODERATE; height = 25; }  // 1/2
    else if (intensite == "Pluie forte")      { color = UIColor::RAIN_HEAVY;    height = 38; }  // 3/4
    else if (intensite == "Pluie très forte" || intensite == "Pluie trés forte") {
        color = UIColor::RAIN_EXTREME; height = 50;                                            // max
    }
}

bool update_rain_bar_ui(int idx, const std::string& intensite, lv_obj_t* const bars[9]) {
    if (idx >= 0 && idx < 9 && bars[idx] != nullptr) {
        uint32_t c;
        int h;
        rain_level_style(intensite, c, h);
        lv_obj_set_style_bg_color(bars[idx], lv_color_hex(c), LV_PART_MAIN);
        lv_obj_set_height(bars[idx], h);
    }
    for (int i = 0; i < 9; i++) {
        if (bars[i] != nullptr && lv_obj_get_height(bars[i]) > 0) return true;
    }
    return false;
}

void update_rain_predict_icon_ui(lv_obj_t* icon, int neige, float humidite) {
    if (icon == nullptr) return;
    if (neige >= 5) {
        lv_label_set_text(icon, "\U000F0598");  // flocon
        lv_obj_set_style_text_color(icon, lv_color_hex(UIColor::WARNING), LV_PART_MAIN);
    } else {
        lv_label_set_text(icon, "\U000F0597");  // goutte
        lv_obj_set_style_text_color(icon, lv_color_hex(get_humidity_color(humidite)), LV_PART_MAIN);
    }
}

void update_clim_from_ha_ui(lv_obj_t* lbl_target, lv_obj_t* lbl_target_popup, lv_obj_t* arc,
    lv_obj_t* lbl_current, float target, float current) {
    char buf_target[16];
    snprintf(buf_target, sizeof(buf_target), "%.1f", target);
    if (lbl_target != nullptr)       lv_label_set_text(lbl_target, buf_target);
    if (lbl_target_popup != nullptr) lv_label_set_text(lbl_target_popup, buf_target);
    if (arc != nullptr)              lv_arc_set_value(arc, (int)target);
    char buf_curr[16];
    snprintf(buf_curr, sizeof(buf_curr), "%.1f \xC2\xB0" "C", current);
    if (lbl_current != nullptr) lv_label_set_text(lbl_current, buf_curr);
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

void build_planning_lines_from_jours(std::string& out_l1, std::string& out_l2) {
    out_l1.clear();
    out_l2.clear();
    static const char* days_short[] = {"Dim.", "Lun.", "Mar.", "Mer.", "Jeu.", "Ven.", "Sam."};

    time_t now_raw = time(nullptr);
    if (now_raw <= 0) {
        out_l1 = "#aaaaaa Aucun travail de prevu#";
        return;
    }
    struct tm now_tm;
    if (localtime_r(&now_raw, &now_tm) == nullptr) {
        out_l1 = "#aaaaaa Aucun travail de prevu#";
        return;
    }

    std::string lines[2];
    int n = 0;
    for (int jour = 0; jour < 15 && n < 2; jour++) {
        const DayForecastData& d = cal_jours_data[jour];
        const std::string& h = d.heures_ouverture.empty() ? cal_heures[jour] : d.heures_ouverture;
        if (h.size() < 11 || d.est_repos) continue;  // "HH:MM-HH:MM"

        const int start_h = atoi(h.substr(0, 2).c_str());
        const int start_m = atoi(h.substr(3, 2).c_str());
        // Aujourd'hui : ignorer le créneau s'il a déjà commencé (même règle que l'ancien Jinja HA)
        if (jour == 0) {
            const int now_min = now_tm.tm_hour * 60 + now_tm.tm_min;
            if (now_min >= start_h * 60 + start_m) continue;
        }

        std::string j_name;
        if (jour == 0) j_name = "Auj.";
        else if (jour == 1) j_name = "Dem.";
        else {
            time_t t = now_raw + static_cast<time_t>(jour) * 86400;
            struct tm day_tm;
            if (localtime_r(&t, &day_tm) == nullptr) continue;
            j_name = days_short[day_tm.tm_wday];
        }

        const bool early = cal_is_early_shift(h);
        const char* hex = early ? "fb923c" : "ffffff";
        std::string j_colored;
        if (jour == 1) j_colored = "#44aaff " + j_name + "#";
        else j_colored = std::string("#") + hex + " " + j_name + "#";

        char line[96];
        snprintf(line, sizeof(line), "%d/ %s : #%s %s#", n + 1, j_colored.c_str(), hex, h.c_str());
        lines[n++] = line;
    }

    if (n == 0) out_l1 = "#aaaaaa Aucun travail de prevu#";
    else {
        out_l1 = lines[0];
        if (n > 1) out_l2 = lines[1];
    }
}
