/**
 * [AI-CONTEXT]
 * @file tab5_cards.cpp
 * @role Cartes domotique : carte et popup lumière (sélecteur, arc, pastilles), cible
 *       clim optimiste, tri dynamique des 5 plantes vers 4 slots, popup détails pots
 *       (EC / lux / température / batterie), température colorée.
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
// Carte lumiere (epaule j2/j3/j4 + switch associe + popup power) : factorise depuis
// light_chambre_state/light_salon_state/light_led_state (tab5-sensors-domotique.yaml, #T164)
// =============================================================================

void update_light_card_ui(lv_obj_t* icon_room, lv_obj_t* icon_light, lv_obj_t* icon_switch,
    lv_obj_t* lbl_switch_state, lv_obj_t* btn_power_icon,
    const std::string& current_light_entity, const std::string& this_entity, bool is_on) {

    if (icon_room == nullptr || icon_light == nullptr) return;

    uint32_t color = is_on ? UIColor::INFO : UIColor::TEXT_DIM;
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
// Popup clim v2 : affichage optimiste de la cible (arc + boutons -/+)
// =============================================================================

// arc peut etre nullptr : la carte clim de l'accueil (climate_card.yaml) a le label
// cible mais pas d'arc — on met a jour le label sans toucher a l'arc dans ce cas.
void update_clim_target_ui(lv_obj_t* lbl_target, lv_obj_t* arc, float target) {
    if (lbl_target == nullptr) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", target);
    lv_label_set_text(lbl_target, buf);
    if (arc != nullptr) lv_arc_set_value(arc, (int) target);
}

// =============================================================================
// Popup lumiere v2 : selecteur 3 lumieres, arc luminosite synchronise, pastilles
// (script tab5_light_popup_show + capteurs light_*_state / light_*_brightness)
// =============================================================================

void update_light_selector_icon(lv_obj_t* icon, bool is_on) {
    if (icon == nullptr) return;
    lv_obj_set_style_text_color(icon,
        lv_color_hex(is_on ? UIColor::INFO : UIColor::TEXT_DIM), LV_PART_MAIN);
}

// Ecrit "NN %" dans pct_lbl et positionne l'arc — helper interne commun.
static void set_light_arc_and_label(lv_obj_t* arc, lv_obj_t* pct_lbl, int arcv) {
    if (arcv < 0) arcv = 0;
    if (arcv > 255) arcv = 255;
    lv_arc_set_value(arc, arcv);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d %%", arcv * 100 / 255);
    lv_label_set_text(pct_lbl, buf);
}

void sync_light_popup_brightness(lv_obj_t* popup, lv_obj_t* arc, lv_obj_t* pct_lbl,
    float brightness) {

    if (popup == nullptr || arc == nullptr || pct_lbl == nullptr) return;
    // Popup ferme : rien a rafraichir (resynchronise de toute facon a l'ouverture)
    if (lv_obj_has_flag(popup, LV_OBJ_FLAG_HIDDEN)) return;
    // Drag en cours : le retour HA differe ferait sauter le knob sous le doigt
    if (lv_obj_has_state(arc, LV_STATE_PRESSED)) return;
    set_light_arc_and_label(arc, pct_lbl, std::isnan(brightness) ? 0 : (int) brightness);
}

void show_light_popup_ui(int light_idx, const char* const titles[3],
    const bool is_on[3], const float brightness[3],
    lv_obj_t* popup, lv_obj_t* title_lbl,
    lv_obj_t* btn0, lv_obj_t* btn1, lv_obj_t* btn2,
    lv_obj_t* icon0, lv_obj_t* icon1, lv_obj_t* icon2,
    lv_obj_t* power_icon, lv_obj_t* arc, lv_obj_t* pct_lbl) {

    if (popup == nullptr || title_lbl == nullptr || arc == nullptr || pct_lbl == nullptr) return;
    if (light_idx < 0 || light_idx > 2) return;

    lv_label_set_text(title_lbl, titles[light_idx]);

    lv_obj_t* btns[3]  = { btn0, btn1, btn2 };
    lv_obj_t* icons[3] = { icon0, icon1, icon2 };
    for (int i = 0; i < 3; i++) {
        if (btns[i] == nullptr) continue;
        bool sel = (i == light_idx);
        lv_obj_set_style_border_width(btns[i], sel ? 3 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(btns[i],
            lv_color_hex(sel ? UIColor::ACCENT : UIColor::GLASS_RIM), LV_PART_MAIN);
        lv_obj_set_style_border_opa(btns[i], sel ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
        update_light_selector_icon(icons[i], is_on[i]);
    }

    if (power_icon != nullptr) {
        lv_obj_set_style_text_color(power_icon,
            lv_color_hex(is_on[light_idx] ? UIColor::INFO : UIColor::TEXT_DIM), LV_PART_MAIN);
    }

    // Lumiere eteinte : l'arc affiche 0 (l'attribut brightness HA est NAN ou obsolete)
    int arcv = (!is_on[light_idx] || std::isnan(brightness[light_idx]))
        ? 0 : (int) brightness[light_idx];
    set_light_arc_and_label(arc, pct_lbl, arcv);

    lv_obj_clear_flag(popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(popup);
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
            snprintf(buf, sizeof(buf), "Pot %d", e.idx + 1);
            lv_label_set_text(slots[s].val_lbl, buf);
        }

        // Couleur colorimetrique
        uint32_t c = get_humidity_color(e.val);
        lv_obj_set_style_text_color(slots[s].icon_lbl, lv_color_hex(c), LV_PART_MAIN);
        lv_obj_set_style_text_color(slots[s].val_lbl, lv_color_hex(UIColor::TEXT_DIM), LV_PART_MAIN);
    }
}

// =============================================================================
// Popup details pots : 5 cartes fixes (humidite/statut + EC/lux/temp/batterie)
// =============================================================================

uint32_t get_battery_color(float x) {
    if (isnan(x)) return UIColor::INACTIVE;
    if (x > 80.0f) return UIColor::SUCCESS;
    if (x > 40.0f) return UIColor::INFO;
    if (x >= 20.0f) return UIColor::WARNING;
    return UIColor::ERROR;
}

void update_pots_popup_moisture_ui(const float values[5], PotDetailUI cards[5]) {
    for (int i = 0; i < 5; i++) {
        if (cards[i].icon_lbl == nullptr || cards[i].moist_lbl == nullptr
            || cards[i].status_lbl == nullptr) {
            continue;
        }
        const float v = values[i];
        const uint32_t c = get_humidity_color(v);  // NaN -> MOISTURE_NAN (gris)
        lv_obj_set_style_text_color(cards[i].icon_lbl, lv_color_hex(c), LV_PART_MAIN);
        if (isnan(v)) {
            lv_label_set_text(cards[i].moist_lbl, "--");
            lv_obj_set_style_text_color(cards[i].moist_lbl, lv_color_hex(UIColor::INACTIVE), LV_PART_MAIN);
            lv_label_set_text(cards[i].status_lbl, "Hors ligne");
            lv_obj_set_style_text_color(cards[i].status_lbl, lv_color_hex(UIColor::TEXT_DIM), LV_PART_MAIN);
            continue;
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f %%", v);
        lv_label_set_text(cards[i].moist_lbl, buf);
        lv_obj_set_style_text_color(cards[i].moist_lbl, lv_color_hex(c), LV_PART_MAIN);
        // Seuils alignes sur get_humidity_color : <=14 = zone rouge (ALERT_RED)
        if (v <= 14.0f) {
            lv_label_set_text(cards[i].status_lbl, "\xC3\x80 arroser !");
            lv_obj_set_style_text_color(cards[i].status_lbl, lv_color_hex(UIColor::ERROR), LV_PART_MAIN);
        } else if (v <= 20.0f) {
            lv_label_set_text(cards[i].status_lbl, "Bient\xC3\xB4t sec");
            lv_obj_set_style_text_color(cards[i].status_lbl, lv_color_hex(UIColor::WARNING), LV_PART_MAIN);
        } else {
            lv_label_set_text(cards[i].status_lbl, "OK");
            lv_obj_set_style_text_color(cards[i].status_lbl, lv_color_hex(UIColor::SUCCESS), LV_PART_MAIN);
        }
    }
}

void update_pot_metric_ui(lv_obj_t* value_lbl, float x, PotMetric metric) {
    if (value_lbl == nullptr) return;
    if (isnan(x)) {
        lv_label_set_text(value_lbl, "--");
        lv_obj_set_style_text_color(value_lbl, lv_color_hex(UIColor::INACTIVE), LV_PART_MAIN);
        return;
    }
    char buf[16];
    uint32_t color = UIColor::TEXT_SOFT;
    switch (metric) {
        case PotMetric::CONDUCTIVITY:
            snprintf(buf, sizeof(buf), "%.0f \xC2\xB5S/cm", x);
            break;
        case PotMetric::ILLUMINANCE:
            snprintf(buf, sizeof(buf), "%.0f lx", x);
            break;
        case PotMetric::TEMPERATURE:
            snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", x);
            color = get_temperature_color(x);
            break;
        case PotMetric::BATTERY:
            snprintf(buf, sizeof(buf), "%.0f %%", x);
            color = get_battery_color(x);
            break;
    }
    lv_label_set_text(value_lbl, buf);
    lv_obj_set_style_text_color(value_lbl, lv_color_hex(color), LV_PART_MAIN);
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
        snprintf(buf, sizeof(buf), "%.1f \xC2\xB0", x);
        lv_label_set_text(label, buf);
        uint32_t c_int = get_temperature_color(x);
        lv_obj_set_style_text_color(label, lv_color_hex(c_int), LV_PART_MAIN);
    }
}
