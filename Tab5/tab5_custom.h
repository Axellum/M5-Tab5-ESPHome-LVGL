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

// Embauche "tôt" = heure de début < 9h (même seuil partout : tuiles, popup, bandeau).
bool cal_is_early_shift(const std::string& heures_hhmm_hhmm);
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

// Tableaux globaux des slots meteo (initialises au boot, fixes car ids LVGL constants).
// Evite la reconstruction identique dans chaque lambda YAML (D2).
extern WeatherDaySlot g_day_slots[5];
extern WeatherHourSlot g_hour_slots[5];

void refresh_daily_forecast(WeatherDaySlot slots[], int page_index,
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);
void refresh_hourly_forecast(WeatherHourSlot slots[], int page_index,
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s);
void transition_widgets(lv_obj_t* out_obj, lv_obj_t* in_obj);

// =============================================================================
// Helpers d'animation LVGL (popups, swipe, alertes)
// Réutilisent les patterns lv_anim_t de transition_widgets() (callbacks
// anim_y_cb/anim_opa_cb/anim_scale_cb/anim_x_cb/anim_ty_cb).
//
// [28/07/2026] Passe « animations légères » : toutes les durées et amplitudes
// sont regroupées ici (UIAnim) — c'était la seule façon de les régler d'un
// coup. L'écran est en `update_interval: never` : c'est LVGL qui redessine
// depuis la loop ESPHome, donc chaque frame d'animation = un repaint de la
// zone animée (fond verre + dégradé compris). Durée courte = moins de frames
// = moins de charge ET moins de latence perçue. Les amplitudes ont été
// réduites en même temps : un glissement de 84px sur un panneau plein cadre
// coûte le même repaint qu'un de 28px, mais se « traîne » visuellement.
// =============================================================================
namespace UIAnim {
    constexpr uint32_t PANEL_DUR    = 190;  // rotateur central (etait 450)
    constexpr int32_t  PANEL_OFFSET = 28;   // px glissement vertical (etait 84)
    constexpr uint32_t POPUP_IN     = 150;  // ouverture popup (etait 280)
    constexpr uint32_t POPUP_OUT    = 110;  // fermeture popup (etait 200)
    constexpr uint32_t SWIPE_DUR    = 200;  // swipe previsions (etait 350)
    constexpr int32_t  SWIPE_OFFSET = 110;  // px (etait 200)
    constexpr uint32_t ALERT_DUR    = 180;  // entree bandeau alerte (etait 300)
    constexpr int32_t  ALERT_OFFSET = 44;   // px (etait 100)
    constexpr uint32_t BTN_PRESS    = 80;   // feedback tactile (inchange)

    // Effet « rouleau » (horloge + icones meteo).
    constexpr uint32_t ROLL_CLOCK   = 240;  // minutes / heures
    constexpr uint32_t ROLL_ICON    = 190;  // icone de prevision
    constexpr int32_t  ROLL_ICON_PX = 22;   // amplitude d'entree de l'icone
    constexpr uint32_t ROLL_STAGGER = 28;   // decalage entre 2 tuiles (effet vague)
}

// =============================================================================
// Retour automatique à l'écran principal (inactivité tactile)
// [28/07/2026, demande Axel] Un popup ou une page météo laissés ouverts
// reviennent seuls au dashboard. Les deux délais sont des délais d'INACTIVITÉ,
// pas des délais depuis l'ouverture : toucher la dalle remet le compteur à
// zéro, donc rien ne se ferme sous les doigts. Le compteur est celui de LVGL
// (lv_display_get_inactive_time), remis à zéro par l'indev à chaque appui —
// et par ui_mark_activity() sur les événements vocaux, sinon une conversation
// mains libres (qui ne touche jamais l'écran) fermerait le popup Assistant.
// =============================================================================
namespace UIIdle {
    constexpr uint32_t POPUP_MS    = 45000;  // popup ouvert -> fermeture
    constexpr uint32_t FORECAST_MS = 25000;  // page météo -> retour panneau principal
}

// Millisecondes écoulées depuis la dernière activité (appui tactile ou
// ui_mark_activity()).
uint32_t ui_idle_ms();

// Remet le compteur d'inactivité à zéro sans qu'il y ait eu de toucher.
// À appeler sur toute activité « invisible » qui doit garder l'écran en place
// (événements du pipeline vocal, ouverture programmée d'un popup).
void ui_mark_activity();

// Animation d'ouverture d'un popup : fondu card + scrim.
// Le scrim doit être visible (clear flag) AVANT l'appel.
// Durée UIAnim::POPUP_IN, ease_out.
void animate_popup_open(lv_obj_t* card, lv_obj_t* scrim);

// Animation de fermeture : fondu inverse. Cache automatiquement card + scrim
// à la fin de l'animation (LV_OBJ_FLAG_HIDDEN).
// Durée UIAnim::POPUP_OUT, ease_in (plus court que l'ouverture pour le "dismiss").
void animate_popup_close(lv_obj_t* card, lv_obj_t* scrim);

// Ferme un popup UNIQUEMENT s'il est réellement affiché et qu'aucun fondu n'est
// déjà en cours dessus. Renvoie true si une fermeture a été lancée.
// Le garde-fou sur l'animation évite un clignotement : animate_popup_close()
// repart de LV_OPA_COVER, la rejouer sur un popup à moitié effacé le
// rallumerait d'un coup avant de le refaire disparaître.
bool close_popup_if_open(lv_obj_t* card);

// true si au moins un des popups passés est visible (flag HIDDEN absent).
bool any_popup_visible(lv_obj_t* const* cards, int n);

// Glissement horizontal + fondu croisé entre deux layers (swipe prévisions).
// dir = LV_DIR_LEFT (in arrive de la droite, out part à gauche) ou
//       LV_DIR_RIGHT (in arrive de la gauche, out part à droite).
// Durée UIAnim::SWIPE_DUR. Dérivée de transition_widgets() mais en horizontal.
void animate_swipe_horizontal(lv_obj_t* out_layer, lv_obj_t* in_layer, lv_dir_t dir);

// Fondu croisé pur (sans glissement) entre deux calques plein cadre —
// bascule prévisions <-> switches HA (bouton « HA »). Le calque sortant est
// masqué à la fin. Durée UIAnim::SWIPE_DUR.
void animate_crossfade_layers(lv_obj_t* out_layer, lv_obj_t* in_layer);

// Slide-in depuis la droite + fondu pour un bandeau d'alerte qui entre
// dans le rotateur central (alertes HA, alertes Météo-France).
// Durée UIAnim::ALERT_DUR, ease_out.
void animate_alert_enter(lv_obj_t* alert_wrap);

// « Rouleau » d'icône météo : la nouvelle icône monte depuis le bas en
// apparaissant (translate_y relatif à l'offset de base posé par
// update_meteo_icon(), donc compatible avec les icônes composées l1+l2).
// delay_ms permet d'échelonner les 5 tuiles (effet vague).
void animate_icon_roll_in(lv_obj_t* l1, lv_obj_t* l2, uint32_t delay_ms);

// Suppression temporaire du rouleau d'icônes : mis à true pendant un
// changement de calque (le calque glisse déjà, un rouleau en plus = bruit).
extern bool g_forecast_roll_suppress;

// =============================================================================
// Horloge à rouleau — un rouleau PAR CHIFFRE (H H : M M)
// Chaque chiffre est un conteneur qui rogne (LVGL clippe les enfants au
// parent) contenant 2 labels : celui affiché et celui qui arrive. Au
// changement, les deux glissent d'une hauteur de boîte vers le haut — l'ancien
// sort, le nouveau entre.
// Découpage par chiffre et non par nombre : à 22 → 23 seule l'unité des
// minutes tourne, la dizaine ne bouge pas. C'est aussi 2× moins de surface
// repeinte par frame qu'un rouleau à deux chiffres.
// Repose sur des chiffres tabulaires (même avance pour 0-9, vrai pour Roboto :
// 75 px en 130 gras) — sinon les chiffres danseraient horizontalement.
// La géométrie (avance des chiffres, hauteur d'encre, centrage dans la tuile)
// est mesurée au boot depuis la police réelle : rien n'est codé en dur.
// =============================================================================
struct ClockDigitRoller {
    lv_obj_t* wrap = nullptr;
    lv_obj_t* lbl[2] = {nullptr, nullptr};
    uint8_t   cur = 0;      // index du label actuellement affiché (0/1)
    char      shown = 0;    // chiffre peint ('0'..'9'), 0 = jamais peint
};

struct ClockRollerCtx {
    ClockDigitRoller d[4];    // HH:MM -> d[0] d[1] : d[2] d[3]
    lv_obj_t* colon = nullptr;
    int       box_h = 0;      // hauteur de la boîte de rognage = course du rouleau
    bool      ready = false;  // layout mesuré
};
extern ClockRollerCtx g_clock_roller;

// Dimensionne/centre les deux rouleaux + le « : » dans la tuile horloge, à
// partir des métriques réelles de la police (hauteur de ligne, ligne de base,
// hauteur de capitale). `clock_font` doit être la police posée sur les 4 labels
// dans le YAML. À appeler une fois après le layout LVGL (interval one-shot du
// boot, comme apply_pressed_scale_to_tree).
void layout_clock_roller(lv_obj_t* clock_tile, esphome::font::Font* clock_font);

// --- 1D : Micro-interactions boutons verre ---
// Applique un style pressed (transform_scale 94% + bg_opa 30%) avec transition
// 80ms ease_out sur un bouton. ESPHome ne supporte pas state_pressed dans les
// styles partagees (style_definitions), donc on l'injecte en C++ via lv_obj_add_style.
void setup_button_press_animation(lv_obj_t* btn);

// Parcourt l'arbre LVGL depuis root et applique setup_button_press_animation()
// a tout objet clickable avec radius 18 (caracteristique du style_clim_btn verre).
// Appele une fois au boot via un interval one-shot (apres layout LVGL).
void apply_pressed_scale_to_tree(lv_obj_t* root);

// Le jeu de bille vit desormais dans marble_game.h / marble_game.cpp
// (namespace Marble). L'ancien prototype `namespace Game` a ete retire.

// Surbrillance bordure bouton (actif = couleur + 2px, inactif = GLASS_RIM + 1px).
void highlight_button_border(lv_obj_t* btn, bool active, uint32_t color);

// =============================================================================
// Contexte carte centrale : regroupe les 8 wrappers LVGL + 7 flags d'activite
// + l'index du panneau courant. Reduit les signatures de 16 parametres a 1.
// Initialise une fois au boot (ids LVGL fixes), les bools sont mis a jour par
// les services HA / scripts YAML avant chaque appel.
// =============================================================================
struct CentralPanelCtx {
    lv_obj_t* planning_wrap = nullptr;
    lv_obj_t* rain_wrap = nullptr;
    lv_obj_t* alert_cont = nullptr;
    lv_obj_t* info_wrap = nullptr;
    lv_obj_t* ha_wrap[4] = {};
    // Ligne "chapeau" du titre de page previsions (lbl_page_title_sub) : logee ici
    // plutot qu'ajoutee aux signatures deja passees en parametre (page_title_wrap /
    // lbl_page_title), qui traversent 3 fonctions et 2 sites d'appel YAML.
    lv_obj_t* page_title_sub = nullptr;
    bool has_rain = false;
    bool has_mf_alerts = false;
    bool has_info = false;
    bool has_ha[4] = {};
    int current_panel = 0;
};

// Contexte global unique (initialise dans tab5-ha-hmi.yaml on_boot ou premier usage).
extern CentralPanelCtx g_central_ctx;

// Gestion du geste de swipe (page_main.on_gesture) : pagination previsions
// horaires/journalieres (0-4) dans la bande centrale+basse (y >= 333). Console diag :
// uniquement via btn_control_console (plus de swipe haut/bas).
void handle_swipe_gesture(lv_dir_t dir, lv_coord_t pt_y, int& forecast_page_index,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx);

// Retour au panneau météo principal (page 2 = prévisions journalières J0-J4),
// déclenché par l'inactivité tactile — mêmes effets qu'un swipe manuel jusqu'à
// cette page (données, calque, pastilles, carte centrale), le chemin est
// factorisé avec handle_swipe_gesture().
// Ne fait rien si on y est déjà : c'est appelé une fois par seconde.
void reset_forecast_to_main_page(int& forecast_page_index,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx);

// Carte centrale : rotateur planning/pluie/alertes (page 2) ou titre de page (autres).
void update_central_forecast_page_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx);

// Reecrit le titre de page previsions en place, sans toucher a la visibilite des
// panneaux, et ne fait rien si ce titre n'est pas affiche. A appeler depuis les
// services bulk (tab5-api-logic.yaml) : ils rafraichissent les 5 tuiles, donc
// sans ca la plage annoncee reste figee sur les anciennes bornes tant que
// l'utilisateur ne reswipe pas (signale par Cursor Bugbot sur la PR #83).
void refresh_forecast_page_title_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx);

// Panneau info central (récap calendrier ou bannière alerte) — logique déplacée
// depuis tab5-api-logic.yaml pour fiabiliser polices LVGL et accents UTF-8.
void update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, const std::string& meteo_id,
    std::string& dismissed_local, bool& has_info, int& current_panel,
    esphome::font::Font* font_small, esphome::font::Font* font_large);

// Rotateur carte centrale : 0 planning, 1 pluie, 2 vigilance MF, 3 info (phrase test),
// 4-7 alertes HA individuelles (8s, même timer global).
constexpr int kCentralPanelCount = 8;
constexpr int kHaAlertPanelBase = 4;
constexpr int kHaAlertSlotCount = 4;

lv_obj_t* central_panel_wrapper(int panel, CentralPanelCtx& ctx);
bool central_panel_is_active(int panel, const CentralPanelCtx& ctx);

// Synchronise g_central_ctx depuis les globals YAML (factorise le bloc 8 lignes
// répété 7× dans tab5-scripts.yaml). Appelée avant chaque advance/dismiss/show.
void sync_central_ctx(CentralPanelCtx& ctx, bool rain, bool alerts, bool info,
                      bool ha0, bool ha1, bool ha2, bool ha3, int panel);

void advance_central_panel_rotator(CentralPanelCtx& ctx);
void sync_central_panel_visibility(CentralPanelCtx& ctx);

struct HaAlertSlotUI {
    lv_obj_t* wrap;
    lv_obj_t* lbl;
    bool* has_flag;
    std::string* id_store;
};

void parse_and_update_ha_alerts_bulk(const std::string& payload, HaAlertSlotUI slots[4],
    CentralPanelCtx& ctx, esphome::font::Font* font, std::string& dismissed_local);

// Masquage immédiat au tap (feedback visuel avant le round-trip HA).
void dismiss_central_info_immediate(lv_obj_t* lbl_info, CentralPanelCtx& ctx);
void dismiss_ha_alert_slot_immediate(int slot_idx, lv_obj_t* wrap, lv_obj_t* lbl,
    bool& has_flag, std::string& id_store, CentralPanelCtx& ctx);

void tab5_dismiss_local_add(std::string& store, const std::string& id);
bool tab5_dismiss_local_has(const std::string& store, const std::string& id);
void tab5_dismiss_local_prune(std::string& store, const std::vector<std::string>& ids_seen);

void update_rain_phrase_ui(lv_obj_t* lbl, const std::string& phrase);

void update_planning_text_ui(lv_obj_t* lbl, const std::string& l1, const std::string& l2,
    std::string& plan_ligne_1, std::string& plan_ligne_2);

// Construit les 2 prochaines lignes du bandeau planning depuis cal_jours_data[15]
// (après parse_and_update_jours_bulk) — remplace l'ancien push HA tab5_maj_planning.
void build_planning_lines_from_jours(std::string& out_l1, std::string& out_l2);

// L'heure passe par g_clock_roller (plus de label lbl_time unique) : seul le
// groupe qui change roule. La date reste un label simple.
void update_clock_date_ui(lv_obj_t* lbl_date,
    int hour, int minute, int day_of_week, int day_of_month, int month);

// Met a jour un label de temperature (texte + couleur gradient). Factorise
// depuis temp_serre/temp_salon (tab5-sensors-domotique.yaml, Phase 3, #T164).
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
// Factorise depuis l'interval 2s de tab5-sensors-diagnostics.yaml (Phase 3, #T164).
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

// Couleur batterie par niveau (échelle icône téléphone du bandeau, réutilisée
// par la ligne Batterie du popup détails pots).
uint32_t get_battery_color(float x);

// Popup détails pots (appui long sur les slots pots) : 5 cartes FIXES, carte N =
// capteur moisture_N (pas de tri dynamique, contrairement au dashboard).
struct PotDetailUI {
    lv_obj_t* icon_lbl;    // icône plante (couleur = humidité)
    lv_obj_t* moist_lbl;   // grande valeur % humidité
    lv_obj_t* status_lbl;  // OK / Bientôt sec / À arroser / Hors ligne
};

// Humidité + statut des 5 cartes — appelé par l'ancre &moisture_on_value
// (tab5-sensors-domotique.yaml) à chaque mise à jour d'un des 5 capteurs.
void update_pots_popup_moisture_ui(const float values[5], PotDetailUI cards[5]);

// Une métrique secondaire d'une carte pot (texte + couleur). L'humidité passe par
// update_pots_popup_moisture_ui, pas par cet enum.
enum class PotMetric { CONDUCTIVITY, ILLUMINANCE, TEMPERATURE, BATTERY };
void update_pot_metric_ui(lv_obj_t* value_lbl, float x, PotMetric metric);

// Met a jour l'icone carte (epaule j2/j3/j4), l'icone/label du switch associe et le
// bouton popup power si c'est la lampe actuellement affichee. Factorise depuis les 3
// blocs identiques light_chambre_state/light_salon_state/light_led_state (#T164).
void update_light_card_ui(lv_obj_t* icon_room, lv_obj_t* icon_light, lv_obj_t* icon_switch,
    lv_obj_t* lbl_switch_state, lv_obj_t* btn_power_icon,
    const std::string& current_light_entity, const std::string& this_entity, bool is_on);

// Icone du selecteur du popup lumiere (lit/canape/ruban LED) : doree si allumee.
void update_light_selector_icon(lv_obj_t* icon, bool is_on);

// Reflete l'attribut brightness HA (0-255, NAN si eteinte) sur l'arc + le label %
// du popup lumiere. Inerte si le popup est ferme ou pendant un drag utilisateur.
void sync_light_popup_brightness(lv_obj_t* popup, lv_obj_t* arc, lv_obj_t* pct_lbl,
    float brightness);

// Affichage optimiste de la cible clim (label + arc du popup) avant le retour HA.
// Appele par l'arc et les boutons -/+ du popup clim ; le retour reel arrive ensuite
// par le service tab5_maj_clim qui reecrit les memes widgets.
void update_clim_target_ui(lv_obj_t* lbl_target, lv_obj_t* arc, float target);

// Ouvre/resynchronise le popup lumiere sur light_idx (0=Chambre 1=Salon 2=LEDs) :
// titre, bordure cyan du selecteur, icones d'etat, icone power, arc + % depuis
// l'etat HA reel. Appele par script tab5_light_popup_show (tab5-scripts.yaml).
void show_light_popup_ui(int light_idx, const char* const titles[3],
    const bool is_on[3], const float brightness[3],
    lv_obj_t* popup, lv_obj_t* title_lbl,
    lv_obj_t* btn0, lv_obj_t* btn1, lv_obj_t* btn2,
    lv_obj_t* icon0, lv_obj_t* icon1, lv_obj_t* icon2,
    lv_obj_t* power_icon, lv_obj_t* arc, lv_obj_t* pct_lbl);

// Tap tuile météo : affiche le planning/horaires du jour dans la carte centrale (6s).
std::string get_day_planning_display_text(int jour);
void show_temporary_planning(int jour, lv_obj_t* lbl_planning,
                             lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, int forecast_page,
                             const std::string& plan_l1, const std::string& plan_l2,
                             bool& is_showing_temp, CentralPanelCtx& ctx);

// Réponse vocale IA : carte centrale dédiée (8s), défilement si phrase longue.
void show_vocal_response_ui(const std::string& texte,
    lv_obj_t* vocal_wrap, lv_obj_t* lbl_vocal,
    lv_obj_t* page_title_wrap, CentralPanelCtx& ctx,
    esphome::font::Font* font);

void hide_vocal_response_ui(lv_obj_t* vocal_wrap, lv_obj_t* lbl_vocal, CentralPanelCtx& ctx);

// =============================================================================
// Popup Assistant vocal (assistant_popup.yaml)
// Affiche la demande (STT) + la réponse écrite du moteur, avec prise en charge
// des tableaux Markdown (alignés en police monospace) et d'une image (online_image).
// Logique centralisée ici (décision 0006 : pas de logique complexe dans le YAML LVGL).
// =============================================================================

// Nettoie un texte Markdown "léger" pour affichage monospace LVGL :
//  - retire les marqueurs **gras**, __gras__, `code`, les # de titres ;
//  - convertit les puces "- " / "* " en "• " ;
//  - ré-aligne les tableaux Markdown (colonnes séparées par |) en largeur fixe
//    (comptage en points de code UTF-8, pas en octets) et supprime la ligne
//    séparatrice |---|---|. Rend les tableaux lisibles sans moteur de rendu.
std::string format_assist_markdown(const std::string& in);

// Renseigne la bulle "Votre demande" (texte STT normalisé UTF-8).
void assist_set_request(lv_obj_t* lbl_request, const std::string& texte);

// Renseigne la zone "Réponse" : normalise + format_assist_markdown + applique la
// police (monospace) puis le texte. Le retour à la ligne LVGL est géré par le YAML.
void assist_set_response(lv_obj_t* lbl_response, const std::string& texte,
    esphome::font::Font* font);

// Applique la taille de police de la réponse (0=S 1=M 2=L) SANS perdre le texte
// déjà affiché (relit lv_label_get_text). Met aussi à jour les 3 boutons S/M/L.
void assist_apply_text_size(lv_obj_t* lbl_response, int size_idx,
    esphome::font::Font* f_s, esphome::font::Font* f_m, esphome::font::Font* f_l,
    lv_obj_t* btn_s, lv_obj_t* btn_m, lv_obj_t* btn_l);

// =============================================================================
// Popup calendrier mensuel (calendar_popup.yaml, appui long sur l'horloge)
// Grille 7×6 lundi-en-tête calculée EN LOCAL (SNTP) ; HA enrichit chaque mois à
// la demande via tab5_maj_calendrier_mois (codes + heures + details optionnels)
// et chaque jour tapé via tab5_maj_calendrier_jour si le cache details est vide.
// =============================================================================

// Bits des codes jour (2 chars hex par jour, poussés par script.tab5_calendrier_mois)
constexpr int CAL_BIT_TRAVAIL  = 1;
constexpr int CAL_BIT_FERIE    = 2;
constexpr int CAL_BIT_VACANCES = 4;   // vacances scolaires (Zone A)
constexpr int CAL_BIT_RDV      = 8;
constexpr int CAL_BIT_ANNIV    = 16;

struct CalCellUI {
    lv_obj_t* cell;   // fond (teinte vacances scolaires) + bordure (aujourd'hui)
    lv_obj_t* num;    // numéro du jour
    lv_obj_t* sub;    // heures de travail "09:30-20:15"
    lv_obj_t* dot;    // pastille RDV (dorée)
    lv_obj_t* dot2;   // pastille anniversaire (rose)
};

struct CalDetailLineUI {
    lv_obj_t* icon;   // glyphe MDI typé (travail/férié/vacances/RDV/anniv/fête)
    lv_obj_t* txt;    // texte de la ligne
};

// Cache mensuel (TTL + eviction : max 3 mois M-1/M/M+1, stale-while-revalidate)
void cal_cache_clear();
bool cal_month_needs_fetch(int year, int month);
// true si le mois est en cache mais plus vieux que ttl_ms (refresh silencieux conseillé)
bool cal_month_is_stale(int year, int month, uint32_t ttl_ms = 600000);  // défaut 10 min
// Évince les mois distants de >1 par rapport à (year, month) — garde max 3 entrées.
void cal_cache_evict_distant(int year, int month);
void cal_store_month_data(const std::string& annee, const std::string& mois,
    const std::string& codes, const std::string& heures, const std::string& details = "");

// Rendu complet du mois affiché : numéros + alignement lundi-dimanche + weekend +
// aujourd'hui calculés localement, enrichissement HA appliqué si le mois est en cache.
void cal_render_month(CalCellUI cells[42], lv_obj_t* lbl_month,
    int view_year, int view_month, int today_year, int today_month, int today_day);

// "" si la cellule est hors mois, sinon date ISO "YYYY-MM-DD" du jour tapé.
std::string cal_date_for_cell(int view_year, int view_month, int cell_idx);

// Détail jour embarqué dans le payload mois (champs séparés par ~). "" si absent.
// true si le mois est en cache ET le champ details a été fourni (même vide = canal présent).
bool cal_month_has_details(int year, int month);
std::string cal_cached_day_detail(int year, int month, int day);
// true seulement si le champ ~ de CE jour est non vide (sinon fallback script _jour).
bool cal_day_has_embedded_detail(int year, int month, int day);

// Sous-popup détail : titre "Mardi 21 Juillet" + statut Chargement/HA hors ligne.
void cal_show_day_detail_loading(lv_obj_t* day_popup, lv_obj_t* lbl_title,
    lv_obj_t* lbl_status, CalDetailLineUI lines[6], const std::string& date_iso,
    bool ha_online);

// Remplit les 6 lignes du détail depuis le payload HA ("type|texte;...").
void cal_render_day_detail(const std::string& payload, lv_obj_t* lbl_status,
    CalDetailLineUI lines[6]);

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
    static constexpr uint32_t EARLY        = 0xFB923C;  // orange-400 (embauche < 9h — distinct de ERROR)
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
    // --- Jeu « Fil d'Or » (marble_game.cpp) ------------------------------------
    // Palette dediee laiton/sarcelle : volontairement distincte du dashboard
    // (et du cliche « purple glow ») pour que le jeu se lise comme un autre monde.
    // Utilisee uniquement par le namespace Marble — ne pas melanger avec le HMI.
    static constexpr uint32_t MARBLE_VOID     = 0x080C14;  // fond hors terrain
    static constexpr uint32_t MARBLE_FLOOR    = 0x111A28;  // sol jouable
    static constexpr uint32_t MARBLE_WALL     = 0x3B4A63;  // murs / obstacles
    static constexpr uint32_t MARBLE_WALL_LIT = 0x63789B;  // arete eclairee des murs
    static constexpr uint32_t MARBLE_HUD_BG   = 0x0C1220;  // bandeau HUD
    static constexpr uint32_t MARBLE_BALL     = 0xE8B44A;  // bille — skin 0 (or)
    static constexpr uint32_t MARBLE_BALL_ALT = 0xD9E4F5;  // bille — skin 1 (argent)
    static constexpr uint32_t MARBLE_BALL_CU  = 0xE2725B;  // bille — skin 2 (cuivre)
    static constexpr uint32_t MARBLE_EXIT     = 0x2BB3A3;  // portail de sortie actif
    static constexpr uint32_t MARBLE_EXIT_OFF = 0x1E4A47;  // portail verrouille (runes manquantes)
    static constexpr uint32_t MARBLE_DANGER   = 0xE05252;  // pieges mortels (spikes, scies, orbes)
    static constexpr uint32_t MARBLE_PIT      = 0x03060C;  // trou / vide
    static constexpr uint32_t MARBLE_SLOW     = 0x7C5CBF;  // glu / zone lente
    static constexpr uint32_t MARBLE_BOOST    = 0xF2853F;  // zone d'acceleration
    static constexpr uint32_t MARBLE_WIND     = 0x4E88C7;  // courant lateral
    static constexpr uint32_t MARBLE_SHIELD   = 0x5AD1E8;  // pickup bouclier
    static constexpr uint32_t MARBLE_MAGNET   = 0xB68CE8;  // pickup aimant
    static constexpr uint32_t MARBLE_BRAKE    = 0x8FBF6A;  // pickup frein
    static constexpr uint32_t MARBLE_DASH     = 0xF2C14E;  // pickup dash
    static constexpr uint32_t MARBLE_RUNE     = 0xF7E08A;  // rune / cle d'objectif
    static constexpr uint32_t MARBLE_BRASS_CHEST = 0x9A6B2F;  // coffre au tresor (laiton)
    // --- Paires de degrade : c'est d'ICI que vient le volume ---------------------
    // [AI-CONTEXT] Meme recette que la table du flipper (PIN_*) : chaque piece a
    // un ton HAUT (face eclairee, vers le haut de l'ecran) et un ton BAS (face a
    // l'ombre). Un `set_grad(obj, HI, LO)` remplace un aplat et ne coute AUCUN
    // objet LVGL supplementaire — une seule passe de dessin.
    // @ai_instruction N'ajoute pas une couleur seule : ajoute une paire, sinon la
    //      piece redeviendra plate au milieu des autres.
    static constexpr uint32_t MARBLE_FLOOR_HI = 0x18243A;  // sol, haut du degrade
    static constexpr uint32_t MARBLE_FLOOR_LO = 0x090E18;  // sol, bas du degrade
    static constexpr uint32_t MARBLE_SLAB     = 0x16202F;  // dalles peintes au sol (decor)
    static constexpr uint32_t MARBLE_WALL_HI  = 0x4C5E7C;  // pierre, face eclairee
    static constexpr uint32_t MARBLE_WALL_LO  = 0x202A3B;  // pierre, face a l'ombre
    static constexpr uint32_t MARBLE_WALL_EDGE= 0x92A7CA;  // arete vive au sommet du mur
    static constexpr uint32_t MARBLE_DANGER_HI= 0xFF8A7A;  // pointe / lame, arete eclairee
    static constexpr uint32_t MARBLE_DANGER_LO= 0x71171C;  // pointe / lame, base sombre
    static constexpr uint32_t MARBLE_BALL_SH  = 0x03060C;  // ombre portee de la bille
    static constexpr uint32_t MARBLE_BALL_HI  = 0xFFF0CE;  // reflet speculaire — skin or
    static constexpr uint32_t MARBLE_BALL_ALT_HI = 0xFFFFFF;  // reflet — skin argent
    static constexpr uint32_t MARBLE_BALL_CU_HI  = 0xFFD3C2;  // reflet — skin cuivre
    static constexpr uint32_t MARBLE_PIT_RIM  = 0x2C3648;  // margelle du trou (rebord eclaire)
    static constexpr uint32_t MARBLE_EXIT_HI  = 0x6FF0DC;  // coeur du portail ouvert
    static constexpr uint32_t MARBLE_RUNE_LO  = 0xA8863A;  // or / rune, bas du degrade
    static constexpr uint32_t MARBLE_CHEST_LO = 0x543813;  // coffre, bas du degrade
    static constexpr uint32_t MARBLE_EMBER    = 0xD8873A;  // braise des torches (decor)
    // --- Jeu « Arcanoïde » (arkanoid_game.cpp) ------------------------------------
    // Palette rétro Atari / borne arcade 80's : fond sombre, briques vives,
    // contraste fort. Utilisée uniquement par le namespace Arkanoid.
    static constexpr uint32_t ARK_VOID      = 0x0A0A0F;  // fond hors terrain (noir bleuté)
    static constexpr uint32_t ARK_FLOOR     = 0x111118;  // sol jouable (gris très sombre)
    static constexpr uint32_t ARK_HUD_BG    = 0x0D0D14;  // bandeau HUD
    static constexpr uint32_t ARK_PADDLE    = 0xE8E8E8;  // raquette (blanc cassé)
    static constexpr uint32_t ARK_BALL      = 0xFFDD44;  // balle (jaune arcade)
    static constexpr uint32_t ARK_WALL      = 0x556677;  // briques indestructibles
    static constexpr uint32_t ARK_TOUGH     = 0x8899AA;  // briques renforcées (plein)
    static constexpr uint32_t ARK_TOUGH_HIT = 0xBB6633;  // briques renforcées (entamées)
    static constexpr uint32_t ARK_DANGER    = 0xFF3333;  // flash de mort
    static constexpr uint32_t ARK_CYAN      = 0x44DDDD;  // accent cyan
    static constexpr uint32_t ARK_GREEN     = 0x44DD44;  // accent vert (expand, vie)
    static constexpr uint32_t ARK_ORANGE    = 0xFF8800;  // accent orange (shrink)
    static constexpr uint32_t ARK_MAGENTA   = 0xFF44AA;  // accent magenta (bonus, colle)
    static constexpr uint32_t ARK_BTN       = 0x334455;  // boutons tactiles latéraux
    // --- Jeu « Neon Apron » (pinball_game.cpp) ------------------------------------
    // Flipper portrait 720×1280. Direction artistique : table sombre bleu nuit,
    // rails d'acier froid, 3 néons seulement (cyan / ambre / magenta) + un vert
    // réservé aux modes actifs. Pas de photoréalisme, pas de bitmap : tout le
    // volume vient de paires ombre/arête (chaque pièce a un ton bas et un ton
    // haut). Utilisée uniquement par le namespace Pinball.
    // @ai_instruction Si tu ajoutes une pièce à la table, réutilise une paire
    //     existante (_DIM / _HI) plutôt que d'inventer une 4ᵉ teinte néon : la
    //     lisibilité du plateau tient au fait qu'il n'y en a que trois.
    static constexpr uint32_t PIN_VOID        = 0x05070E;  // fond hors table
    static constexpr uint32_t PIN_FELT_HI     = 0x121C2E;  // sol, haut du dégradé
    static constexpr uint32_t PIN_FELT_LO     = 0x070B14;  // sol, bas du dégradé
    static constexpr uint32_t PIN_HUD_BG      = 0x080C16;  // fronton / DMD
    static constexpr uint32_t PIN_RAIL        = 0x35435C;  // corps des rails et guides
    static constexpr uint32_t PIN_RAIL_HI     = 0x8CA3C4;  // arête éclairée des rails
    static constexpr uint32_t PIN_CHROME      = 0xC8D4E6;  // chrome du tablier (apron)
    static constexpr uint32_t PIN_APRON       = 0x0C1220;  // fond du tablier
    static constexpr uint32_t PIN_BALL        = 0xC9D6E8;  // corps de la bille (acier)
    static constexpr uint32_t PIN_BALL_HI     = 0xFFFFFF;  // reflet spéculaire de la bille
    static constexpr uint32_t PIN_BALL_SH     = 0x1B2333;  // ombre portée de la bille
    static constexpr uint32_t PIN_FLIP_BASE   = 0x3E1B33;  // flanc sombre du flipper
    static constexpr uint32_t PIN_FLIP_EDGE   = 0xFF3D8A;  // arête néon du flipper
    static constexpr uint32_t PIN_CYAN        = 0x35E6FF;  // néon 1 — bumpers, lanes
    static constexpr uint32_t PIN_CYAN_DIM    = 0x11485C;  // néon 1 éteint
    static constexpr uint32_t PIN_AMBER       = 0xFFB020;  // néon 2 — score, cibles
    static constexpr uint32_t PIN_AMBER_DIM   = 0x53390B;  // néon 2 éteint
    static constexpr uint32_t PIN_MAGENTA     = 0xFF3D8A;  // néon 3 — slingshots, multi
    static constexpr uint32_t PIN_MAGENTA_DIM = 0x521230;  // néon 3 éteint
    static constexpr uint32_t PIN_MODE        = 0x3DFF9E;  // vert « mode en cours »
    static constexpr uint32_t PIN_DANGER      = 0xFF4757;  // TILT, drain, perte de bille
    static constexpr uint32_t PIN_WHITE       = 0xF2F6FF;  // texte principal
    static constexpr uint32_t PIN_TEXT_DIM    = 0x6C7C98;  // texte secondaire
    static constexpr uint32_t PIN_INSERT_OFF  = 0x141E2C;  // insert lumineux éteint
}

