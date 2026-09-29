/**
 * [AI-CONTEXT]
 * @file tab5_cards.cpp
 * @role Cartes domotique : clim (cible optimiste et retour de HA, réglages venus de
 *       l'appareil — clé « climr », ADR-0026), tri dynamique des 5 plantes vers 4 slots,
 *       popup détails pots (EC / lux / température / batterie), température colorée.
 *       Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée. update_clim_from_ha_ui() y est venue de
 *       tab5_services.cpp le 29/09/2026 (ADR-0026) : tout l'affichage de la clim ici.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Éviter std::string dans les boucles de parsing ; char* + strtok_r.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// Carte et popup lumière : tab5_tuiles.cpp depuis les pièces (ADR-0023, 28/09/2026) —
// épaules des tuiles, cartes du mode HA, sélecteur des lumières de la pièce.

// =============================================================================
// Clim : réglages de l'appareil (ADR-0026), cible optimiste (arc + boutons -/+) et
// retour de HA (tab5_maj_clim)
// =============================================================================

// ─── Réglages venus de l'appareil (ADR-0026) ─────────────────────────────────────
// Clé « climr » de tab5_maj_emplacements : « climr|min|max|pas|unité|capacités|nom »,
// poussée par le blueprint avant tab5_maj_clim (connexion, rechargement, changement de
// la clim). Pas de NVS. Tant que rien n'est reçu, les valeurs par défaut ci-dessous
// sont celles de la 3.2 (Daikin de l'auteur) et aucun widget n'est touché : un
// blueprint plus ancien garde l'écran d'avant.
namespace {

struct ClimReglages {
    float min = 16.0f;
    float max = 30.0f;
    float pas = 0.5f;
    bool fahrenheit = false;
    // Lettres des boutons que l'appareil gère (tableau de l'ADR-0026) : c froid, h chaud,
    // d sec, f ventilation, e Éco, b Boost, q Silence, s Oscillation, w Brise.
    char capacites[16] = "chdfebqsw";
    char nom[49] = "";  // friendly_name de la clim, 48 octets au plus
    bool recu = false;
};
ClimReglages s_clim;
// Dernières valeurs affichées : reposées quand les réglages changent (bornes de l'arc,
// format de la cible, unité de la pièce).
float s_clim_consigne = NAN;
float s_clim_piece = NAN;

// Titre du popup : de x = 52 (modal_header.yaml) à la croix (80 px à 10 px du bord de la
// carte de 1250), avec de l'air.
constexpr int32_t kLargeurTitreClim = 1080;

// Positions de la carte OPTIONS (climate_popup.yaml, tout affiché) : titre de section,
// son contenu 26 px dessous ; rangée des préréglages 92 px, bouton 88 px, 10 px entre deux
// boutons, 24 px entre deux sections.
constexpr int32_t kOptionsY0 = 50;
constexpr int32_t kOptionsSousTitre = 26;
constexpr int32_t kOptionsRangee = 92;
constexpr int32_t kOptionsBouton = 88;
constexpr int32_t kOptionsEntreBoutons = 10;
constexpr int32_t kOptionsEntreSections = 24;

bool clim_capacite(char lettre) {
    return lettre != '\0' && std::strchr(s_clim.capacites, lettre) != nullptr;
}

// Format de la cible : « %.1f » si le pas est fractionnaire (0,5), « %.0f » sinon (1 °F).
bool clim_pas_entier() {
    return std::fabs(s_clim.pas - std::round(s_clim.pas)) < 0.001f;
}

const char* clim_unite() {
    return s_clim.fahrenheit ? "\xC2\xB0" "F" : "\xC2\xB0" "C";
}

// Consigne inconnue (capteur HA indisponible = NaN) : « -- », comme au boot — "%.1f"
// écrirait « nan », sans glyphe dans roboto_55_b.
void clim_format_consigne(char* buf, size_t n, float t) {
    if (std::isnan(t)) snprintf(buf, n, "--");
    else snprintf(buf, n, clim_pas_entier() ? "%.0f" : "%.1f", t);
}

// Nombre d'un champ (point décimal) ; `defaut` s'il est vide ou illisible.
float lire_nombre(const char* p, size_t n, float defaut) {
    char tmp[16];
    if (n == 0 || n >= sizeof(tmp)) return defaut;
    std::memcpy(tmp, p, n);
    tmp[n] = '\0';
    char* bout = nullptr;
    const float v = strtof(tmp, &bout);
    return (bout == tmp || std::isnan(v) || std::isinf(v)) ? defaut : v;
}

// Carte OPTIONS : les sections qui ont un bouton visible s'empilent depuis le haut, une
// section sans bouton disparaît avec son titre. Dans la rangée flex des préréglages et
// dans la pile des modes, LVGL saute les objets masqués : pas de trou.
void clim_options_empiler() {
    const ClimUI& u = g_clim_ui;
    const bool eco = clim_capacite('e');
    const bool boost = clim_capacite('b');
    const bool silence = clim_capacite('q');
    const bool oscillation = clim_capacite('s');
    const bool brise = clim_capacite('w');
    int32_t y = kOptionsY0;

    const bool presets = eco || boost;
    ui_hidden(u.eco, !eco);
    ui_hidden(u.boost, !boost);
    ui_hidden(u.titre_presets, !presets);
    ui_hidden(u.rangee_presets, !presets);
    if (presets) {
        ui_y(u.titre_presets, y);
        ui_y(u.rangee_presets, y + kOptionsSousTitre);
        y += kOptionsSousTitre + kOptionsRangee + kOptionsEntreSections;
    }

    ui_hidden(u.silence, !silence);
    ui_hidden(u.titre_ventilation, !silence);
    if (silence) {
        ui_y(u.titre_ventilation, y);
        ui_y(u.silence, y + kOptionsSousTitre);
        y += kOptionsSousTitre + kOptionsBouton + kOptionsEntreSections;
    }

    const bool flux = oscillation || brise;
    ui_hidden(u.oscillation, !oscillation);
    ui_hidden(u.brise, !brise);
    ui_hidden(u.titre_flux, !flux);
    if (flux) {
        ui_y(u.titre_flux, y);
        int32_t yb = y + kOptionsSousTitre;
        if (oscillation) {
            ui_y(u.oscillation, yb);
            yb += kOptionsBouton + kOptionsEntreBoutons;
        }
        if (brise) ui_y(u.brise, yb);
    }
}

void clim_reglages_appliquer_ui() {
    if (!s_clim.recu) return;
    const ClimUI& u = g_clim_ui;

    // Arc : bornes entières qui englobent celles de l'appareil, puis la consigne de
    // nouveau (lv_arc_set_range la ramène dans les anciennes bornes ; aucun des deux
    // n'émet LV_EVENT_VALUE_CHANGED, donc pas de on_value ni d'envoi à HA).
    if (u.arc != nullptr) {
        const int32_t bas = static_cast<int32_t>(std::floor(s_clim.min));
        const int32_t haut = static_cast<int32_t>(std::ceil(s_clim.max));
        if (lv_arc_get_min_value(u.arc) != bas || lv_arc_get_max_value(u.arc) != haut)
            lv_arc_set_range(u.arc, bas, haut);
        if (!std::isnan(s_clim_consigne)) lv_arc_set_value(u.arc, static_cast<int32_t>(s_clim_consigne));
    }

    // Cible (format du pas) et températures (unité). Consigne pas encore reçue : les
    // labels gardent leur texte de démarrage (tab5_maj_clim suit juste après).
    if (!std::isnan(s_clim_consigne)) {
        char buf[16];
        clim_format_consigne(buf, sizeof(buf), s_clim_consigne);
        ui_text(u.consigne_carte, buf);
        ui_text(u.consigne_popup, buf);
    }
    ui_text(u.unite, clim_unite());
    if (!std::isnan(s_clim_piece) && u.piece != nullptr) {
        char piece[20];
        snprintf(piece, sizeof(piece), "%.1f %s", s_clim_piece, clim_unite());
        ui_text(u.piece, piece);
    }

    // Titre : le nom de la clim dans HA ; sans nom, « Climatisation ».
    texte_ha_coupe(u.titre, s_clim.nom[0] != '\0' ? s_clim.nom : tr("Climatisation"), kLargeurTitreClim);

    // Carte MODE : les modes que l'appareil n'a pas disparaissent (« Éteint » reste).
    ui_hidden(u.mode_froid, !clim_capacite('c'));
    ui_hidden(u.mode_chaud, !clim_capacite('h'));
    ui_hidden(u.mode_sec, !clim_capacite('d'));
    ui_hidden(u.mode_ventilation, !clim_capacite('f'));

    clim_options_empiler();
}

}  // namespace

ClimUI g_clim_ui;

void clim_reglages_recu(const char* reste, size_t n) {
    // Six champs : min, max, pas, unité, capacités, puis le nom (tout le reste : le
    // blueprint y a remplacé « | » par « / »).
    const char* champ[6] = {};
    size_t taille[6] = {};
    int k = 0;
    size_t debut = 0;
    for (size_t i = 0; i <= n && k < 6; i++) {
        if (i == n || (reste[i] == '|' && k < 5)) {
            champ[k] = reste + debut;
            taille[k] = i - debut;
            k++;
            debut = i + 1;
        }
    }
    if (k < 5) {
        ESP_LOGW("tab5.clim", "Reglages de la clim illisibles (%d champs) : ignores", k);
        return;
    }
    const float mn = lire_nombre(champ[0], taille[0], s_clim.min);
    const float mx = lire_nombre(champ[1], taille[1], s_clim.max);
    if (mn < mx) {
        s_clim.min = mn;
        s_clim.max = mx;
    }
    const float pas = lire_nombre(champ[2], taille[2], s_clim.pas);
    if (pas > 0.0f && pas <= 10.0f) s_clim.pas = pas;
    // « °F » ou « °C » (UTF-8) : la dernière lettre suffit.
    s_clim.fahrenheit = taille[3] > 0 && champ[3][taille[3] - 1] == 'F';
    size_t j = 0;
    for (size_t i = 0; i < taille[4] && j + 1 < sizeof(s_clim.capacites); i++)
        if (champ[4][i] >= 'a' && champ[4][i] <= 'z') s_clim.capacites[j++] = champ[4][i];
    s_clim.capacites[j] = '\0';
    if (k == 6) texte_ha_copier(s_clim.nom, sizeof(s_clim.nom), champ[5], taille[5]);
    else s_clim.nom[0] = '\0';
    s_clim.recu = true;
    ESP_LOGI("tab5.clim", "Reglages de la clim : %.1f-%.1f, pas %.2f, %s, [%s]",
             s_clim.min, s_clim.max, s_clim.pas, s_clim.fahrenheit ? "F" : "C", s_clim.capacites);
    clim_reglages_appliquer_ui();
}

float clim_consigne_suivante(float t, int sens) {
    float v = t + (sens < 0 ? -s_clim.pas : s_clim.pas);
    if (v < s_clim.min) v = s_clim.min;
    if (v > s_clim.max) v = s_clim.max;
    return v;
}

std::string clim_consigne_texte(float t) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", t);
    // « 21.50 » → « 21.5 », « 72.00 » → « 72 » (« nan » reste « nan »).
    if (std::strchr(buf, '.') != nullptr) {
        size_t n = std::strlen(buf);
        while (n > 0 && buf[n - 1] == '0') buf[--n] = '\0';
        if (n > 0 && buf[n - 1] == '.') buf[--n] = '\0';
    }
    return std::string(buf);
}

bool clim_eco_actif(const std::string& preset) {
    return preset == "eco" || preset == "away";
}

bool clim_silence_actif(const std::string& fan) {
    return fan == "quiet" || fan == "silence" || fan == "Silence" || fan == "low";
}

bool clim_oscillation_actif(const std::string& swing) {
    return swing == "swing" || swing == "on" || swing == "both" || swing == "vertical" ||
           swing == "3d" || swing == "horizontal";
}

bool clim_preset_actif(const std::string& preset, const char* bouton) {
    return std::strcmp(bouton, "away") == 0 ? clim_eco_actif(preset) : preset == bouton;
}

// ─── Cible et températures ───────────────────────────────────────────────────────

// arc peut etre nullptr : la carte clim de l'accueil (climate_card.yaml) a le label
// cible mais pas d'arc — on met a jour le label sans toucher a l'arc dans ce cas.
// Consigne inconnue : « -- », et l'arc reste où il est.
void update_clim_target_ui(lv_obj_t* lbl_target, lv_obj_t* arc, float target) {
    if (lbl_target == nullptr) return;
    s_clim_consigne = target;
    char buf[16];
    clim_format_consigne(buf, sizeof(buf), target);
    ui_text(lbl_target, buf);
    if (std::isnan(target)) return;
    if (arc != nullptr) lv_arc_set_value(arc, (int) target);
}

void update_clim_from_ha_ui(lv_obj_t* lbl_target, lv_obj_t* lbl_target_popup, lv_obj_t* arc,
    lv_obj_t* lbl_current, float target, float current) {
    s_clim_consigne = target;
    s_clim_piece = current;
    const bool known = !std::isnan(target);
    char buf_target[16];
    clim_format_consigne(buf_target, sizeof(buf_target), target);
    if (lbl_target != nullptr)       lv_label_set_text(lbl_target, buf_target);
    if (lbl_target_popup != nullptr) lv_label_set_text(lbl_target_popup, buf_target);
    if (arc != nullptr && known)     lv_arc_set_value(arc, (int)target);
    char buf_curr[20];
    snprintf(buf_curr, sizeof(buf_curr), "%.1f %s", current, clim_unite());
    if (lbl_current != nullptr) lv_label_set_text(lbl_current, buf_curr);
}

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
        if (slots[s].icon_lbl == nullptr || slots[s].val_lbl == nullptr) {
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
    //      la médiane (« Moy: ») et le plus humide ;
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
        // Icone du capteur d'origine
        ui_text(slots[s].icon_lbl, icons_utf8[pot]);

        // Texte sous l'icone : "Pot X" ou "Moy:"
        if (resume && s == 2) {
            ui_text(slots[s].val_lbl, tr("Moy:"));
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), tr("Pot %d"), pot + 1);
            ui_text(slots[s].val_lbl, buf);
        }

        // Couleur colorimetrique (grise hors ligne)
        ui_text_color(slots[s].icon_lbl, std::isnan(val) ? UIColor::INACTIVE : get_humidity_color(val));
        ui_text_color(slots[s].val_lbl, UIColor::TEXT_DIM);
    }
}

// =============================================================================
// Popup details pots : 5 cartes fixes (humidite/statut + EC/lux/temp/batterie)
// =============================================================================

uint32_t get_battery_color(float x) {
    if (std::isnan(x)) return UIColor::INACTIVE;
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
        ui_text_color(cards[i].icon_lbl, c);
        if (std::isnan(v)) {
            ui_text(cards[i].moist_lbl, "--");
            ui_text_color(cards[i].moist_lbl, UIColor::INACTIVE);
            ui_text(cards[i].status_lbl, tr("Hors ligne"));
            ui_text_color(cards[i].status_lbl, UIColor::TEXT_DIM);
            continue;
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f %%", v);
        ui_text(cards[i].moist_lbl, buf);
        ui_text_color(cards[i].moist_lbl, c);
        // Seuils alignes sur get_humidity_color : <=14 = zone rouge (ALERT_RED)
        if (v <= 14.0f) {
            ui_text(cards[i].status_lbl, tr("\xC3\x80 arroser !"));
            ui_text_color(cards[i].status_lbl, UIColor::ERROR);
        } else if (v <= 20.0f) {
            ui_text(cards[i].status_lbl, tr("Bient\xC3\xB4t sec"));
            ui_text_color(cards[i].status_lbl, UIColor::WARNING);
        } else {
            ui_text(cards[i].status_lbl, "OK");
            ui_text_color(cards[i].status_lbl, UIColor::SUCCESS);
        }
    }
}

void update_pot_metric_ui(lv_obj_t* value_lbl, float x, PotMetric metric) {
    if (value_lbl == nullptr) return;
    if (std::isnan(x)) {
        ui_text(value_lbl, "--");
        ui_text_color(value_lbl, UIColor::INACTIVE);
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
    ui_text(value_lbl, buf);
    ui_text_color(value_lbl, color);
}

// Met a jour un label de temperature (texte + couleur gradient). Factorise
// depuis temp_serre/temp_salon (tab5-sensors-domotique.yaml, Phase 3, #T164).
void update_temp_ui(lv_obj_t* label, float x) {
    if (label == nullptr) return;
    if (std::isnan(x)) {
        ui_text(label, "-- \xC2\xB0");
        ui_text_color(label, UIColor::TEXT_DIM);
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
    set_icon_active_ui(icon_pc, active, UIColor::SUCCESS, UIColor::TEXT_PRIMARY);
}
