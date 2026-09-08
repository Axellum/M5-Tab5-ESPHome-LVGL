/**
 * [AI-CONTEXT]
 * @file tab5_central.cpp
 * @role Carte centrale : rotateur planning/pluie/alertes/info, bandeaux d'alertes HA,
 *       titre de page, pagination des prévisions au swipe (apply_forecast_page,
 *       handle_swipe_gesture, reset_forecast_to_main_page), planning temporaire au tap
 *       sur une tuile, réponse vocale.
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
// Geste de swipe (page_main.on_gesture) : pagination previsions (y >= carte centrale)
// =============================================================================

static constexpr lv_coord_t FORECAST_SWIPE_Y_MIN = 333;  // haut de central_card (tab5-lvgl.yaml)

// Page de repos des previsions : journalier J0-J4, celle du boot
// (forecast_page_index initial_value: 2) et celle ou la carte centrale reprend
// son rotateur planning/pluie/alertes. C'est la cible du retour automatique.
static constexpr int FORECAST_MAIN_PAGE = 2;



// Titre de la carte centrale sur les pages de previsions autres que l'accueil.
//   chapeau : famille de page + rang, ex "Pr\xC3\xA9visions journali\xC3\xA8res \xC2\xB7 2/3"
//   plage   : bornes reelles des 5 tuiles visibles, ex
//             "Du mercredi 5 ao\xC3\xBBt au dimanche 9 ao\xC3\xBBt" ou "De 14:00 \xC3\xA0 18:00"
// Renvoie false quand la page n'a pas de titre (page 2 = accueil : la carte
// centrale y reprend son rotateur planning/pluie/alertes).
// Les bornes sont toujours donnees dans l'ordre chronologique (la plus tot ->
// la plus tard), y compris sur les pages horaires ou les tuiles sont affichees
// dans l'ordre inverse (cf. forecast_hourly.yaml).
static bool forecast_page_title_parts(int page, std::string& chapeau, std::string& plage) {
    chapeau.clear();
    plage.clear();
    char buf[96];

    if (page == 3 || page == 4) {
        const int daily_pi = page - 2;                  // 1 = J5-J9, 2 = J10-J14
        snprintf(buf, sizeof(buf), "Pr\xC3\xA9visions journali\xC3\xA8res \xC2\xB7 %d/3", daily_pi + 1);
        chapeau = buf;

        const int premier = daily_pi * 5;
        const int dernier = premier + 4;
        std::string debut = format_long_day_label(premier);
        std::string fin   = format_long_day_label(dernier);
        if (debut.empty() || fin.empty()) {
            // SNTP pas encore synchronise : repli sur les libelles courts pousses
            // par HA ("Mer 05"), comme le fait deja refresh_daily_forecast().
            debut = cal_jours_data[premier].nom_jour;
            fin   = cal_jours_data[dernier].nom_jour;
        }
        if (!debut.empty() && !fin.empty()) {
            snprintf(buf, sizeof(buf), "Du %s au %s", debut.c_str(), fin.c_str());
            plage = buf;
        }
        return true;
    }

    if (page == 0 || page == 1) {
        // Pages horaires : l'index UI est inverse par rapport aux donnees
        // (apply_forecast_page appelle refresh_hourly_forecast(..., 1 - page)).
        const int hourly_pi = 1 - page;                 // 0 = 5 prochaines heures, 1 = les 5 suivantes
        snprintf(buf, sizeof(buf), "Pr\xC3\xA9visions horaires \xC2\xB7 %d/2", hourly_pi + 1);
        chapeau = buf;

        const std::string& debut = cal_heures_data[hourly_pi * 5].heure_texte;
        const std::string& fin   = cal_heures_data[hourly_pi * 5 + 4].heure_texte;
        if (!debut.empty() && !fin.empty()) {
            // Plage a cheval sur minuit (22:00 -> 02:00) : sans mention explicite
            // le titre se lirait comme une plage a rebours.
            const bool lendemain = atoi(fin.c_str()) < atoi(debut.c_str());
            snprintf(buf, sizeof(buf), "De %s \xC3\xA0 %s%s", debut.c_str(), fin.c_str(),
                     lendemain ? " le lendemain" : "");
            plage = buf;
        }
        return true;
    }

    return false;
}

static uint32_t ha_alert_color_from_couleur(const std::string& couleur) {
    if (couleur.find("Rouge") != std::string::npos) return UIColor::ALERT_RED;
    if (couleur.find("Orange") != std::string::npos) return UIColor::WARNING;
    return UIColor::TEXT_PRIMARY;
}

lv_obj_t* central_panel_wrapper(int panel, CentralPanelCtx& ctx) {
    switch (panel) {
        case 0: return ctx.planning_wrap;
        case 1: return ctx.rain_wrap;
        case 2: return ctx.alert_cont;
        case 3: return ctx.info_wrap;
        case 4: return ctx.ha_wrap[0];
        case 5: return ctx.ha_wrap[1];
        case 6: return ctx.ha_wrap[2];
        case 7: return ctx.ha_wrap[3];
        default: return nullptr;
    }
}

bool central_panel_is_active(int panel, const CentralPanelCtx& ctx) {
    switch (panel) {
        case 0: return true;
        case 1: return ctx.has_rain;
        case 2: return ctx.has_mf_alerts;
        case 3: return ctx.has_info;
        case 4: return ctx.has_ha[0];
        case 5: return ctx.has_ha[1];
        case 6: return ctx.has_ha[2];
        case 7: return ctx.has_ha[3];
        default: return false;
    }
}

// Synchronise g_central_ctx depuis les valeurs fournies (issues des globals YAML).
// Factorise le bloc de 8 lignes répété 7× dans tab5-scripts.yaml.
void sync_central_ctx(CentralPanelCtx& ctx, bool rain, bool alerts, bool info,
                      bool ha0, bool ha1, bool ha2, bool ha3, int panel) {
    ctx.has_rain      = rain;
    ctx.has_mf_alerts = alerts;
    ctx.has_info      = info;
    ctx.has_ha[0]     = ha0;
    ctx.has_ha[1]     = ha1;
    ctx.has_ha[2]     = ha2;
    ctx.has_ha[3]     = ha3;
    ctx.current_panel = panel;
}

void advance_central_panel_rotator(CentralPanelCtx& ctx) {
    int next_panel = ctx.current_panel;
    int attempts = 0;
    while (attempts < kCentralPanelCount) {
        next_panel = (next_panel + 1) % kCentralPanelCount;
        if (central_panel_is_active(next_panel, ctx)) break;
        attempts++;
    }
    if (next_panel == ctx.current_panel) return;

    lv_obj_t* out_obj = central_panel_wrapper(ctx.current_panel, ctx);
    lv_obj_t* in_obj = central_panel_wrapper(next_panel, ctx);
    transition_widgets(out_obj, in_obj);
    ctx.current_panel = next_panel;
}

static void hide_central_panel(lv_obj_t* wrap) {
    if (!wrap) return;
    lv_obj_add_flag(wrap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(wrap, 0);
    lv_obj_set_style_opa(wrap, LV_OPA_COVER, LV_PART_MAIN);
}

void sync_central_panel_visibility(CentralPanelCtx& ctx) {
    hide_central_panel(ctx.planning_wrap);
    hide_central_panel(ctx.rain_wrap);
    hide_central_panel(ctx.alert_cont);
    hide_central_panel(ctx.info_wrap);
    for (int i = 0; i < 4; i++) hide_central_panel(ctx.ha_wrap[i]);

    if (!central_panel_is_active(ctx.current_panel, ctx)) {
        ctx.current_panel = 0;
        for (int p = 0; p < kCentralPanelCount; p++) {
            if (central_panel_is_active(p, ctx)) {
                ctx.current_panel = p;
                break;
            }
        }
    }

    lv_obj_t* active = central_panel_wrapper(ctx.current_panel, ctx);
    if (active) lv_obj_clear_flag(active, LV_OBJ_FLAG_HIDDEN);
}

static void clear_ha_alert_slot(HaAlertSlotUI& slot) {
    if (slot.has_flag) *slot.has_flag = false;
    if (slot.id_store) slot.id_store->clear();
    if (slot.lbl) {
        lv_label_set_recolor(slot.lbl, false);
        lv_label_set_text(slot.lbl, "");
    }
    if (slot.wrap) {
        lv_obj_add_flag(slot.wrap, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(slot.wrap, 0);  // Reset X (animate_alert_enter peut avoir laisse un offset)
        lv_obj_set_style_opa(slot.wrap, LV_OPA_COVER, LV_PART_MAIN);
    }
}

void parse_and_update_ha_alerts_bulk(const std::string& payload, HaAlertSlotUI slots[4],
    CentralPanelCtx& ctx, esphome::font::Font* font, std::string& dismissed_local) {

    // 1E : Sauvegarde des IDs precedents pour detecter les nouvelles alertes.
    std::string prev_ids[4];
    for (int i = 0; i < kHaAlertSlotCount; i++) {
        if (slots[i].id_store) prev_ids[i] = *slots[i].id_store;
    }
    int new_alert_slot = -1;

    for (int i = 0; i < kHaAlertSlotCount; i++) {
        clear_ha_alert_slot(slots[i]);
    }

    if (payload.empty()) {
        for (int i = 0; i < 4; i++)
            ctx.has_ha[i] = slots[i].has_flag ? *slots[i].has_flag : false;
        sync_central_panel_visibility(ctx);
        return;
    }
    if (payload.length() > 1024) {
        ESP_LOGE("TAB5", "Payload alertes HA trop long (%d octets).", (int) payload.length());
        return;
    }

    char buf[1025];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int slot_idx = 0;
    std::vector<std::string> ids_seen;
    char* saveptr1 = nullptr;
    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr && slot_idx < kHaAlertSlotCount) {
        char* parts[3];
        int num_parts = 0;
        char* p = token;
        while (true) {
            if (num_parts >= 3) break;
            parts[num_parts++] = p;
            char* next = strchr(p, '|');
            if (next) {
                *next = '\0';
                p = next + 1;
            } else {
                break;
            }
        }
        if (num_parts >= 3 && slots[slot_idx].wrap && slots[slot_idx].lbl && slots[slot_idx].has_flag && slots[slot_idx].id_store) {
            std::string aid = parts[0];
            ids_seen.push_back(aid);
            if (tab5_dismiss_local_has(dismissed_local, aid)) {
                token = strtok_r(nullptr, ";", &saveptr1);
                continue;
            }
            *slots[slot_idx].id_store = aid;
            std::string texte = normalize_text_utf8(parts[2]);
            *slots[slot_idx].has_flag = !texte.empty();
            if (font) {
                esphome::lvgl::lv_obj_set_style_text_font(slots[slot_idx].lbl, font, LV_PART_MAIN);
            }
            lv_obj_set_style_text_color(slots[slot_idx].lbl, lv_color_hex(ha_alert_color_from_couleur(parts[1])), LV_PART_MAIN);
            lv_label_set_recolor(slots[slot_idx].lbl, false);
            lv_label_set_text(slots[slot_idx].lbl, texte.c_str());
            // 1E : Detecte si cette alerte est nouvelle (ID absent du precedent batch).
            if (new_alert_slot < 0) {
                bool is_new = true;
                for (int j = 0; j < kHaAlertSlotCount; j++) {
                    if (prev_ids[j] == aid) { is_new = false; break; }
                }
                if (is_new) new_alert_slot = slot_idx;
            }
            slot_idx++;
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }

    tab5_dismiss_local_prune(dismissed_local, ids_seen);

    for (int i = 0; i < 4; i++)
        ctx.has_ha[i] = slots[i].has_flag ? *slots[i].has_flag : false;
    sync_central_panel_visibility(ctx);

    // 1E : Anime l'entree du bandeau si une nouvelle alerte est active.
    if (new_alert_slot >= 0) {
        int alert_panel = kHaAlertPanelBase + new_alert_slot;
        if (ctx.current_panel == alert_panel && slots[new_alert_slot].wrap) {
            animate_alert_enter(slots[new_alert_slot].wrap);
        }
    }
}

void dismiss_central_info_immediate(lv_obj_t* lbl_info, CentralPanelCtx& ctx) {
    ctx.has_info = false;
    if (lbl_info) {
        lv_label_set_recolor(lbl_info, false);
        lv_label_set_text(lbl_info, "");
    }
    if (ctx.info_wrap) lv_obj_add_flag(ctx.info_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.current_panel == 3) {
        advance_central_panel_rotator(ctx);
    } else {
        sync_central_panel_visibility(ctx);
    }
}

void dismiss_ha_alert_slot_immediate(int slot_idx, lv_obj_t* wrap, lv_obj_t* lbl,
    bool& has_flag, std::string& id_store, CentralPanelCtx& ctx) {

    if (slot_idx < 0 || slot_idx >= kHaAlertSlotCount) return;
    id_store.clear();
    has_flag = false;
    ctx.has_ha[slot_idx] = false;
    if (lbl) {
        lv_label_set_recolor(lbl, false);
        lv_label_set_text(lbl, "");
    }
    if (wrap) lv_obj_add_flag(wrap, LV_OBJ_FLAG_HIDDEN);
    const int dismissed_panel = kHaAlertPanelBase + slot_idx;
    if (ctx.current_panel == dismissed_panel) {
        advance_central_panel_rotator(ctx);
    } else {
        sync_central_panel_visibility(ctx);
    }
}

// Pose les deux lignes du titre sans rien decider de la visibilite : chapeau
// discret (roboto_22 attenue) + plage en gras dessous. Si les bornes manquent
// (donnees HA pas encore recues et SNTP muet), le chapeau prend la ligne
// principale et se recentre verticalement.
// Renvoie false quand la page n'a pas de titre (accueil) : rien n'est ecrit.
static bool set_forecast_page_title_text(int forecast_page, lv_obj_t* lbl_page_title,
                                         CentralPanelCtx& ctx) {
    std::string chapeau, plage;
    if (!forecast_page_title_parts(forecast_page, chapeau, plage)) return false;

    const bool deux_lignes = !plage.empty();
    if (ctx.page_title_sub) {
        lv_label_set_recolor(ctx.page_title_sub, false);
        lv_label_set_text(ctx.page_title_sub, deux_lignes ? chapeau.c_str() : "");
    }
    lv_label_set_recolor(lbl_page_title, false);
    lv_label_set_text(lbl_page_title, deux_lignes ? plage.c_str() : chapeau.c_str());
    lv_obj_align(lbl_page_title, LV_ALIGN_CENTER, 0, deux_lignes ? 13 : 0);
    return true;
}

void update_central_forecast_page_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx) {

    if (!page_title_wrap || !lbl_page_title) return;

    if (ctx.planning_wrap) lv_obj_add_flag(ctx.planning_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.rain_wrap) lv_obj_add_flag(ctx.rain_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.alert_cont) lv_obj_add_flag(ctx.alert_cont, LV_OBJ_FLAG_HIDDEN);
    if (ctx.info_wrap) lv_obj_add_flag(ctx.info_wrap, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; i++)
        if (ctx.ha_wrap[i]) lv_obj_add_flag(ctx.ha_wrap[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);

    if (forecast_page == 2) {
        lv_obj_t* active = central_panel_wrapper(ctx.current_panel, ctx);
        if (active) lv_obj_clear_flag(active, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (!set_forecast_page_title_text(forecast_page, lbl_page_title, ctx)) return;
    lv_obj_clear_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);
}

void refresh_forecast_page_title_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx) {

    if (!page_title_wrap || !lbl_page_title) return;
    // No-op si le titre n'est pas a l'ecran (accueil, planning temporaire 6 s,
    // reponse vocale) : un push HA ne doit jamais reprendre la carte centrale a
    // ce qui l'occupe. On se contente de reecrire le texte, sans toucher a la
    // visibilite des panneaux — contrairement a update_central_forecast_page_ui().
    if (lv_obj_has_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN)) return;
    set_forecast_page_title_text(forecast_page, lbl_page_title, ctx);
}

void update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, const std::string& meteo_id,
    std::string& dismissed_local, bool& has_info, int& current_panel,
    esphome::font::Font* font_small, esphome::font::Font* font_large) {

    if (!lbl_info) return;

    std::string t = texte;
    const char* ws = " \t\r\n";
    size_t deb = t.find_first_not_of(ws);
    t = (deb == std::string::npos) ? "" : t.substr(deb, t.find_last_not_of(ws) - deb + 1);

    // Banniere vigilance : texte fixe UTF-8 cote firmware (HA ne fournit que la couleur).
    if (const char* banner = vigilance_alert_banner_utf8(couleur)) {
        if (!meteo_id.empty() && tab5_dismiss_local_has(dismissed_local, meteo_id)) {
            if (t.empty()) {
                has_info = false;
                lv_label_set_recolor(lbl_info, false);
                lv_label_set_text(lbl_info, "");
                return;
            }
            t = normalize_text_utf8(t);
        } else {
            t = banner;
        }
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


// Applique une page de previsions : donnees, calque, pastilles, carte centrale.
// Factorise entre les deux seules facons de changer de page — le swipe manuel
// (handle_swipe_gesture) et le retour automatique d'inactivite
// (reset_forecast_to_main_page) — pour qu'elles laissent l'ecran dans
// exactement le meme etat. `dir` ne sert qu'a l'animation de changement de
// calque (horaire <-> journalier).
static void apply_forecast_page(int old_page, int page, lv_dir_t dir,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx) {

        // Detection de changement de layer (horaire <-> journalier).
        // L'animation de swipe horizontal n'a de sens que lors d'un changement de layer.
        bool old_is_daily = (old_page >= 2);
        bool new_is_daily = (page >= 2);

        if (old_is_daily != new_is_daily) {
            // Changement de layer : refresh des donnees AVANT l'animation,
            // puis animation du glissement horizontal + fondu croise.
            // Le calque entier glisse deja : pas de rouleau d'icone en plus
            // (deux animations sur la meme zone = bruit visuel + repaint double).
            g_forecast_roll_suppress = true;
            if (new_is_daily) {
                refresh_daily_forecast(day_slots, page - 2, f_main, f_card, f_main_s, f_card_s);
            } else {
                refresh_hourly_forecast(hour_slots, 1 - page, f_main, f_card, f_main_s, f_card_s);
            }
            g_forecast_roll_suppress = false;
            lv_obj_t* out_layer = old_is_daily ? layer_forecast_daily : layer_forecast_hourly;
            lv_obj_t* in_layer  = new_is_daily ? layer_forecast_daily : layer_forecast_hourly;
            animate_swipe_horizontal(out_layer, in_layer, dir);
        } else {
            // Meme layer (page intra-journalier ou intra-horaire) : refresh instantane.
            if (new_is_daily) {
                lv_obj_clear_flag(layer_forecast_daily, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(layer_forecast_hourly, LV_OBJ_FLAG_HIDDEN);
                refresh_daily_forecast(day_slots, page - 2, f_main, f_card, f_main_s, f_card_s);
            } else {
                lv_obj_add_flag(layer_forecast_daily, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(layer_forecast_hourly, LV_OBJ_FLAG_HIDDEN);
                refresh_hourly_forecast(hour_slots, 1 - page, f_main, f_card, f_main_s, f_card_s);
            }
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

        update_central_forecast_page_ui(page, page_title_wrap, lbl_page_title, ctx);
}

void handle_swipe_gesture(lv_dir_t dir, lv_coord_t pt_y, int& forecast_page_index,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx) {

    // [AI-DEBUG] Un swipe qui ne pagine pas se diagnostique ici : si cette ligne n'apparait
    // pas pendant le geste, LVGL a consomme le drag en scroll et n'a jamais emis
    // LV_EVENT_GESTURE — chercher l'objet scrollable sous le doigt (cf. [AI-WARNING] du
    // panneau titre dans tab5-lvgl.yaml), pas dans cette fonction.
    // Le logger du projet tourne en `level: INFO` (tab5-hardware.yaml) : passer
    // temporairement a DEBUG pour voir cette trace, elle est muette autrement.
    ESP_LOGD("TAB5", "swipe: dir=%d y=%d page=%d", (int) dir, (int) pt_y, forecast_page_index);

    if (pt_y < FORECAST_SWIPE_Y_MIN) return;
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;

    int old_page = forecast_page_index;
    int page = old_page;
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

    apply_forecast_page(old_page, page, dir,
        layer_forecast_daily, layer_forecast_hourly, day_slots, hour_slots,
        f_main, f_card, f_main_s, f_card_s, pbars,
        page_title_wrap, lbl_page_title, ctx);
}

void reset_forecast_to_main_page(int& forecast_page_index,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx) {

    const int old_page = forecast_page_index;
    if (old_page == FORECAST_MAIN_PAGE) return;  // deja au panneau principal
    forecast_page_index = FORECAST_MAIN_PAGE;

    // Sens de l'animation : depuis l'horaire (0/1) le calque journalier arrive
    // par la droite, comme un swipe vers la gauche ; depuis 3/4 on recule, donc
    // swipe vers la droite (sans effet visible : meme calque, refresh direct).
    const lv_dir_t dir = (old_page < FORECAST_MAIN_PAGE) ? LV_DIR_LEFT : LV_DIR_RIGHT;

    apply_forecast_page(old_page, FORECAST_MAIN_PAGE, dir,
        layer_forecast_daily, layer_forecast_hourly, day_slots, hour_slots,
        f_main, f_card, f_main_s, f_card_s, pbars,
        page_title_wrap, lbl_page_title, ctx);
}


// =============================================================================
// Planning jour au tap sur tuile météo (carte centrale 6s)
// =============================================================================

std::string get_day_planning_display_text(int jour) {
    if (jour < 0 || jour >= 15) return "Jour hors plage";
    const DayForecastData& d = cal_jours_data[jour];
    const std::string& h = !cal_heures[jour].empty() ? cal_heures[jour] : d.heures_ouverture;

    std::string label;
    if (jour == 0) label = "Auj.";
    else {
        std::string short_lbl = format_short_day_label(jour);
        label = short_lbl.empty() ? d.nom_jour : short_lbl;
    }
    if (label.empty()) label = "Jour";

    if (!h.empty()) {
        // Recolor early (< 9h) en orange EARLY — le reste en blanc
        if (cal_is_early_shift(h)) {
            return label + " : #fb923c " + h + "#";
        }
        return label + " : " + h;
    }
    if (d.est_repos) return label + " : repos";
    return label + " : pas d'horaire";
}

static lv_timer_t* planning_restore_timer = nullptr;
static std::string static_plan_l1;
static std::string static_plan_l2;
static lv_obj_t* static_lbl_planning = nullptr;
static bool* static_is_showing_temp = nullptr;
static int static_forecast_page_restore = 2;
static lv_obj_t* static_page_title_wrap = nullptr;
static lv_obj_t* static_lbl_page_title = nullptr;
static int static_central_panel_restore = 0;

static void planning_restore_timer_cb(lv_timer_t* timer) {
    if (static_is_showing_temp) {
        *static_is_showing_temp = false;
    }
    g_central_ctx.current_panel = static_central_panel_restore;
    if (static_forecast_page_restore != 2) {
        update_central_forecast_page_ui(static_forecast_page_restore,
            static_page_title_wrap, static_lbl_page_title, g_central_ctx);
    } else if (static_lbl_planning) {
        std::string combined = static_plan_l1;
        if (!static_plan_l2.empty()) {
            combined += "   |   " + static_plan_l2;
        }
        set_label_text_utf8(static_lbl_planning, combined.c_str());
    }
    lv_timer_del(timer);
    planning_restore_timer = nullptr;
}

void show_temporary_planning(int jour, lv_obj_t* lbl_planning,
                             lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, int forecast_page,
                             const std::string& plan_l1, const std::string& plan_l2,
                             bool& is_showing_temp, CentralPanelCtx& ctx) {
    if (!lbl_planning) return;

    static_central_panel_restore = ctx.current_panel;
    is_showing_temp = true;
    ctx.current_panel = 0;

    std::string text = get_day_planning_display_text(jour);
    set_label_text_utf8(lbl_planning, text.c_str());

    // Stoppe les animations LVGL en cours sur les panneaux centraux.
    if (ctx.planning_wrap) lv_anim_del(ctx.planning_wrap, nullptr);
    if (ctx.alert_cont) lv_anim_del(ctx.alert_cont, nullptr);
    if (ctx.rain_wrap) lv_anim_del(ctx.rain_wrap, nullptr);
    if (ctx.info_wrap) lv_anim_del(ctx.info_wrap, nullptr);
    for (int i = 0; i < 4; i++)
        if (ctx.ha_wrap[i]) lv_anim_del(ctx.ha_wrap[i], nullptr);
    if (page_title_wrap) lv_anim_del(page_title_wrap, nullptr);

    if (page_title_wrap) lv_obj_add_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.planning_wrap) lv_obj_clear_flag(ctx.planning_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.alert_cont) lv_obj_add_flag(ctx.alert_cont, LV_OBJ_FLAG_HIDDEN);
    if (ctx.rain_wrap) lv_obj_add_flag(ctx.rain_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.info_wrap) lv_obj_add_flag(ctx.info_wrap, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; i++)
        if (ctx.ha_wrap[i]) lv_obj_add_flag(ctx.ha_wrap[i], LV_OBJ_FLAG_HIDDEN);

    static_plan_l1 = plan_l1;
    static_plan_l2 = plan_l2;
    static_lbl_planning = lbl_planning;
    static_is_showing_temp = &is_showing_temp;
    static_forecast_page_restore = forecast_page;
    static_page_title_wrap = page_title_wrap;
    static_lbl_page_title = lbl_page_title;

    if (planning_restore_timer != nullptr) {
        lv_timer_del(planning_restore_timer);
        planning_restore_timer = nullptr;
    }

    planning_restore_timer = lv_timer_create(planning_restore_timer_cb, 6000, nullptr);
}

static void hide_all_central_panels_for_overlay(lv_obj_t* page_title_wrap, CentralPanelCtx& ctx) {
    if (page_title_wrap) lv_obj_add_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.planning_wrap) lv_obj_add_flag(ctx.planning_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.rain_wrap) lv_obj_add_flag(ctx.rain_wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.alert_cont) lv_obj_add_flag(ctx.alert_cont, LV_OBJ_FLAG_HIDDEN);
    if (ctx.info_wrap) lv_obj_add_flag(ctx.info_wrap, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; i++)
        if (ctx.ha_wrap[i]) lv_obj_add_flag(ctx.ha_wrap[i], LV_OBJ_FLAG_HIDDEN);
}

void show_vocal_response_ui(const std::string& texte,
    lv_obj_t* vocal_wrap, lv_obj_t* lbl_vocal,
    lv_obj_t* page_title_wrap, CentralPanelCtx& ctx,
    esphome::font::Font* font) {

    if (!vocal_wrap || !lbl_vocal) return;

    std::string t = normalize_text_utf8(texte);
    const char* ws = " \t\r\n";
    size_t deb = t.find_first_not_of(ws);
    t = (deb == std::string::npos) ? "" : t.substr(deb, t.find_last_not_of(ws) - deb + 1);
    if (t.empty()) return;

    if (ctx.planning_wrap) lv_anim_del(ctx.planning_wrap, nullptr);
    if (ctx.rain_wrap) lv_anim_del(ctx.rain_wrap, nullptr);
    if (ctx.alert_cont) lv_anim_del(ctx.alert_cont, nullptr);
    if (ctx.info_wrap) lv_anim_del(ctx.info_wrap, nullptr);
    if (vocal_wrap) lv_anim_del(vocal_wrap, nullptr);
    for (int i = 0; i < 4; i++)
        if (ctx.ha_wrap[i]) lv_anim_del(ctx.ha_wrap[i], nullptr);
    if (page_title_wrap) lv_anim_del(page_title_wrap, nullptr);

    hide_all_central_panels_for_overlay(page_title_wrap, ctx);

    if (font) {
        esphome::lvgl::lv_obj_set_style_text_font(lbl_vocal, font, LV_PART_MAIN);
    }
    lv_obj_set_style_text_color(lbl_vocal, lv_color_hex(UIColor::TEXT_PRIMARY), LV_PART_MAIN);
    lv_label_set_recolor(lbl_vocal, false);

    // Phrase longue : défilement horizontal sur la largeur carte centrale.
    constexpr size_t kScrollMinChars = 42;
    if (t.size() > kScrollMinChars) {
        lv_obj_set_width(lbl_vocal, 1180);
        lv_label_set_long_mode(lbl_vocal, LV_LABEL_LONG_SCROLL_CIRCULAR);
    } else {
        lv_obj_set_width(lbl_vocal, LV_SIZE_CONTENT);
        lv_label_set_long_mode(lbl_vocal, LV_LABEL_LONG_CLIP);
    }
    lv_label_set_text(lbl_vocal, t.c_str());

    lv_obj_clear_flag(vocal_wrap, LV_OBJ_FLAG_HIDDEN);
}

void hide_vocal_response_ui(lv_obj_t* vocal_wrap, lv_obj_t* lbl_vocal, CentralPanelCtx& ctx) {
    if (lbl_vocal) {
        lv_label_set_text(lbl_vocal, "");
        lv_label_set_long_mode(lbl_vocal, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl_vocal, LV_SIZE_CONTENT);
    }
    if (vocal_wrap) lv_obj_add_flag(vocal_wrap, LV_OBJ_FLAG_HIDDEN);

    sync_central_panel_visibility(ctx);
}
