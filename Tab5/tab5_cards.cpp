/**
 * [AI-CONTEXT]
 * @file tab5_cards.cpp
 * @role Cartes domotique : tri dynamique des 5 plantes vers 4 slots, popup détails pots
 *       (EC / lux / température / batterie), température colorée, icônes d'état (PC) et
 *       leur repeint au changement de thème (cartes_rejouer_theme). Unité de compilation
 *       issue de la scission de tab5_custom.cpp (lot (e) de l'audit du 06/09/2026, faite le
 *       08/09/2026) : mêmes fonctions, même ordre, aucune logique modifiée. La clim, qui
 *       était ici depuis le 29/09/2026 (ADR-0026, ADR-0027), est dans tab5_clim.cpp depuis
 *       le 08/10/2026 (lot L7 de l'audit du 07/10/2026), lignes identiques.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Carte et popup lumière : tab5_tuiles.cpp et tab5_tuiles_popups.cpp depuis les pièces (ADR-0023, 28/09/2026) —
// épaules des tuiles, cartes du mode HA, sélecteur des lumières de la pièce.
// =============================================================================
// Tri dynamique plantes : 5 capteurs -> 4 slots (2 secs + mediane + humide)
// =============================================================================

// Dernières entrées reçues, rejouées par moisture_slots_refresh() quand les zones
// changent (un pot déclaré absent par HA, ou de retour).
static float s_pots_vals[5] = {NAN, NAN, NAN, NAN, NAN};
static const char* s_pots_icons[5] = {};
static MoistureSlotUI s_pots_slots[4] = {};
static bool s_pots_prets = false;

void sort_and_update_moisture_slots(float values[5], const char* icons_utf8[5],
    MoistureSlotUI slots[4]) {

    // Garde de securite contre les pointeurs nuls si LVGL n'est pas encore initialise
    for (int s = 0; s < 4; s++) {
        if (slots[s].icon_lbl == nullptr) {
            return;
        }
    }
    for (int i = 0; i < 5; i++) {
        s_pots_vals[i] = values[i];
        s_pots_icons[i] = icons_utf8[i];
    }
    for (int s = 0; s < 4; s++) s_pots_slots[s] = slots[s];
    s_pots_prets = true;
    moisture_slots_refresh();
}

void moisture_slots_refresh() {
    if (!s_pots_prets) return;
    const float* values = s_pots_vals;
    const char* const* icons_utf8 = s_pots_icons;
    MoistureSlotUI* slots = s_pots_slots;

    // 1) Pots présents (zones, lot 5) : valides (pas NaN) d'un côté, hors ligne de
    // l'autre, dans l'ordre des capteurs.
    struct Entry { int idx; float val; };
    Entry valid[5];
    int n_valid = 0;
    int hors_ligne[5];
    int n_hors_ligne = 0;

    for (int i = 0; i < 5; i++) {
        if (zone_absente(static_cast<Zone>(static_cast<int>(Zone::POT_1) + i))) continue;
        if (!std::isnan(values[i])) {
            valid[n_valid++] = {i, values[i]};
        } else {
            hors_ligne[n_hors_ligne++] = i;
        }
    }
    const int n_presents = n_valid + n_hors_ligne;
    const int n_slots = n_presents < 4 ? n_presents : 4;

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

    // 3) Deux façons de remplir les emplacements :
    //    - résumé, avec 5 pots dont au moins 4 valides : le plus sec, le 2e plus sec,
    //      la médiane et le plus humide ;
    //    - sinon, un emplacement par pot : les valides du plus sec au plus humide, puis
    //      les hors ligne en gris. Plus de pot répété sur deux emplacements.
    const bool resume = (n_presents == 5 && n_valid >= 4);
    int selected[4] = {0, 1, n_valid / 2, n_valid - 1};
    if (resume) {
        // Eviter les doublons si mediane == slot 1 ou slot 3
        if (selected[2] <= selected[1]) selected[2] = selected[1] + 1;
        if (selected[2] >= selected[3] && selected[3] > 0) selected[2] = selected[3] - 1;
    }

    // 4) Mise a jour des emplacements LVGL ; ceux en trop sont masqués (la rangée,
    // en flex SPACE_EVENLY, recentre les autres).
    for (int s = 0; s < 4; s++) {
        ui_hidden(lv_obj_get_parent(slots[s].icon_lbl), s >= n_slots);
        if (s >= n_slots) continue;

        int pot;
        float val;
        if (resume) {
            pot = valid[selected[s]].idx;
            val = valid[selected[s]].val;
        } else if (s < n_valid) {
            pot = valid[s].idx;
            val = valid[s].val;
        } else {
            pot = hors_ligne[s - n_valid];
            val = NAN;
        }
        // Icone du capteur d'origine, seule depuis le 05/10/2026 (plus de « Pot X » /
        // « Moy: » dessous) : le nom et la valeur sont dans le popup « Mes Plantes ».
        ui_text(slots[s].icon_lbl, icons_utf8[pot]);

        // Couleur colorimetrique (grise hors ligne)
        ui_text_color(slots[s].icon_lbl, std::isnan(val) ? UIColor.INACTIVE : get_humidity_color(val));
    }
}

// =============================================================================
// Popup details pots : 5 cartes fixes (humidite/statut + EC/lux/temp/batterie)
// =============================================================================

uint32_t get_battery_color(float x) {
    if (std::isnan(x)) return UIColor.INACTIVE;
    if (x > 80.0f) return UIColor.SUCCESS;
    if (x > 40.0f) return UIColor.INFO;
    if (x >= 20.0f) return UIColor.WARNING;
    return UIColor.ERROR;
}

// Thèmes (ADR-0029) : ce que les capteurs ont peint (valeur par label, cartes du popup
// des pots, carte PC), rejoué par cartes_rejouer_theme().
struct MesurePeinte {
    lv_obj_t* lbl;
    float x;
    int metrique;  // PotMetric, ou -1 : update_temp_ui()
};
static MesurePeinte s_mesures[24] = {};
static int s_nb_mesures = 0;
static float s_pots_popup_vals[5] = {};
static PotDetailUI s_pots_popup_cartes[5] = {};
static bool s_pots_popup_peint = false;
static lv_obj_t* s_icone_pc = nullptr;
static bool s_pc_actif = false;

static void mesure_retenir(lv_obj_t* lbl, float x, int metrique) {
    for (int i = 0; i < s_nb_mesures; i++) {
        if (s_mesures[i].lbl == lbl) {
            s_mesures[i].x = x;
            s_mesures[i].metrique = metrique;
            return;
        }
    }
    if (s_nb_mesures < static_cast<int>(sizeof(s_mesures) / sizeof(s_mesures[0])))
        s_mesures[s_nb_mesures++] = {lbl, x, metrique};
}

void update_pots_popup_moisture_ui(const float values[5], PotDetailUI cards[5]) {
    for (int i = 0; i < 5; i++) {
        s_pots_popup_vals[i] = values[i];
        s_pots_popup_cartes[i] = cards[i];
    }
    s_pots_popup_peint = true;
    for (int i = 0; i < 5; i++) {
        if (cards[i].icon_lbl == nullptr || cards[i].moist_lbl == nullptr
            || cards[i].status_lbl == nullptr) {
            continue;
        }
        const float v = values[i];
        const uint32_t c = get_humidity_color(v);  // NaN -> MOISTURE_NAN (gris)
        ui_text_color(cards[i].icon_lbl, c);
        if (std::isnan(v)) {
            ui_text(cards[i].moist_lbl, "--");
            ui_text_color(cards[i].moist_lbl, UIColor.INACTIVE);
            ui_text(cards[i].status_lbl, tr("Hors ligne"));
            ui_text_color(cards[i].status_lbl, UIColor.TEXT_DIM);
            continue;
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f %%", v);
        ui_text(cards[i].moist_lbl, buf);
        ui_text_color(cards[i].moist_lbl, c);
        // Seuils alignes sur get_humidity_color : <=14 = zone rouge (ALERT_RED)
        if (v <= 14.0f) {
            ui_text(cards[i].status_lbl, tr("\xC3\x80 arroser !"));
            ui_text_color(cards[i].status_lbl, UIColor.ERROR);
        } else if (v <= 20.0f) {
            ui_text(cards[i].status_lbl, tr("Bient\xC3\xB4t sec"));
            ui_text_color(cards[i].status_lbl, UIColor.WARNING);
        } else {
            ui_text(cards[i].status_lbl, "OK");
            ui_text_color(cards[i].status_lbl, UIColor.SUCCESS);
        }
    }
}

void update_pot_metric_ui(lv_obj_t* value_lbl, float x, PotMetric metric) {
    if (value_lbl == nullptr) return;
    mesure_retenir(value_lbl, x, static_cast<int>(metric));
    if (std::isnan(x)) {
        ui_text(value_lbl, "--");
        ui_text_color(value_lbl, UIColor.INACTIVE);
        return;
    }
    char buf[16];
    uint32_t color = UIColor.TEXT_SOFT;
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
    ui_text(value_lbl, buf);
    ui_text_color(value_lbl, color);
}

// Met a jour un label de temperature (texte + couleur gradient). Factorise
// depuis temp_serre/temp_salon (tab5-sensors-domotique.yaml, Phase 3, #T164).
void update_temp_ui(lv_obj_t* label, float x) {
    if (label == nullptr) return;
    mesure_retenir(label, x, -1);
    if (std::isnan(x)) {
        ui_text(label, "-- \xC2\xB0");
        ui_text_color(label, UIColor.TEXT_DIM);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f \xC2\xB0", x);
        ui_text(label, buf);
        uint32_t c_int = get_temperature_color(x);
        ui_text_color(label, c_int);
    }
}

// =============================================================================
// Icônes d'état (bandeau + cartes) et carte PC — appelées par les sensors YAML,
// qui ne touchent plus LVGL (règle 2, audit du 06/09/2026 §4.1 point 12).
// =============================================================================

void set_icon_color_ui(lv_obj_t* icon, uint32_t color) {
    if (icon == nullptr) return;
    ui_text_color(icon, color);
}

void set_icon_active_ui(lv_obj_t* icon, bool active, uint32_t color_on, uint32_t color_off) {
    set_icon_color_ui(icon, active ? color_on : color_off);
}

void update_pc_status_ui(bool active, lv_obj_t* icon_pc) {
    if (icon_pc == nullptr) return;
    s_icone_pc = icon_pc;
    s_pc_actif = active;
    set_icon_active_ui(icon_pc, active, UIColor.SUCCESS, UIColor.TEXT_PRIMARY);
}

void cartes_rejouer_theme() {
    for (int i = 0; i < s_nb_mesures; i++) {
        const MesurePeinte m = s_mesures[i];
        if (m.metrique < 0) update_temp_ui(m.lbl, m.x);
        else update_pot_metric_ui(m.lbl, m.x, static_cast<PotMetric>(m.metrique));
    }
    if (s_pots_popup_peint) {
        const float vals[5] = {s_pots_popup_vals[0], s_pots_popup_vals[1], s_pots_popup_vals[2],
                               s_pots_popup_vals[3], s_pots_popup_vals[4]};
        PotDetailUI cartes[5];
        for (int i = 0; i < 5; i++) cartes[i] = s_pots_popup_cartes[i];
        update_pots_popup_moisture_ui(vals, cartes);
    }
    if (s_icone_pc != nullptr) update_pc_status_ui(s_pc_actif, s_icone_pc);
    clim_recolorer();
    moisture_slots_refresh();
}
