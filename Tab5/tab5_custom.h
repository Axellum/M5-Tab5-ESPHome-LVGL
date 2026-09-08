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

// Date locale à J+jour_offset (0-14) via l'heure système SNTP, normalisée à midi
// par mktime() : immunisé contre les bascules heure d'été/hiver (une journée de
// 23 h ou 25 h décalerait la date d'un jour près de minuit). Renvoie false si
// l'heure n'est pas encore synchronisée ou si l'offset est hors bornes.
// Partagé avec alarm_clock.cpp (calcul de la prochaine sonnerie) — c'était un
// `static` de tab5_custom.cpp jusqu'au 05/08/2026 : le réveil DOIT utiliser
// exactement la même arithmétique de dates que les tuiles météo, sinon les deux
// divergent d'un jour deux fois par an.
bool local_day_from_offset(int jour_offset, struct tm& out);

// Jours et mois en toutes lettres, UTF-8, minuscules (en français ils ne
// prennent pas de majuscule hors début de phrase). wday : 0 = dimanche.
// Partagés avec alarm_clock.cpp (« demain, mercredi 6 août ») — une seule table
// pour tout le projet, sinon deux orthographes finissent par diverger.
const char* fr_day_long_utf8(int wday);
const char* fr_month_long_utf8(int mois_1_12);
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
// anim_y_cb/anim_opa_cb/anim_x_cb/anim_ty_cb).
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

// -----------------------------------------------------------------------------
// Services HA (tab5-api-logic.yaml) : logique LVGL sortie des lambdas le
// 08/09/2026 (ADR-0006, audit du 06/09 §4.1 point 1). Un service ne fait plus
// que résoudre ses `id()`, poser ses globals et appeler l'une de ces fonctions ;
// tools/check_tab5_code_rules.py (pytest) interdit tout `lv_*` dans le contrat
// (hors lv_obj_has_flag, lecture pure).
// -----------------------------------------------------------------------------

// Carte volet : flèche (mouvement / sens de la dernière commande), icône du
// volet, et la ligne « Volet » du panneau switches (sw_icon / sw_label peuvent
// être nuls : la ligne est optionnelle).
struct VoletUI {
    lv_obj_t* arrow;     // icon_card_shutter_arrow
    lv_obj_t* shutter;   // icon_card_shutter1
    lv_obj_t* sw_icon;   // icon_sw1
    lv_obj_t* sw_label;  // lbl_sw1_state
};

// etat_physique poussé par HA : "En_mouvement", "Ouvert" / "Partiel" / "open",
// "Ferme" / "closed" (autre valeur : flèche au repos, icône du volet inchangée).
// target_open = sens de la dernière commande (volet_target_open). Retourne true
// si le volet est en mouvement — à stocker dans volet_en_mouvement.
bool update_volet_ui(const std::string& etat_physique, bool target_open, const VoletUI& ui);

// Vigilance Météo-France : phrase pluie, date recolorée, 4 slots d'icônes.
struct VigilanceUI {
    lv_obj_t* lbl_phrase;      // lbl_proc_pluie
    lv_obj_t* lbl_pluie_val;   // masqués quand la phrase s'affiche
    lv_obj_t* lbl_pluie_unit;
    lv_obj_t* lbl_date;        // recoloré selon la vigilance globale
    lv_obj_t* slots[4];        // alerte_slot_0..3
};

// payload = 11 champs séparés par « | » : phrase pluie, vigilance globale
// (Vert / Jaune / Orange / Rouge), puis vent, inondation, orages,
// pluie-inondation, neige-verglas, grand froid, vagues-submersion, canicule,
// avalanches. Les 4 premiers phénomènes ≠ Vert remplissent les slots (jaune /
// orange / rouge). Retourne true si au moins un phénomène est actif — à
// stocker dans has_alerts.
bool parse_and_update_vigilance(const std::string& payload, const VigilanceUI& ui);

// Histogramme pluie 1 h : 9 barres de 5 min (rb_0_in … rb_8_in). intensite =
// libellé Météo-France (« Pluie faible » … « Pluie très forte »), tout autre
// texte vide la barre. Retourne true si au moins une barre est non vide — à
// stocker dans has_rain.
bool update_rain_bar_ui(int idx, const std::string& intensite, lv_obj_t* const bars[9]);
// Même chose pour les 9 barres en un appel : payload « idx|intensité;… » (ADR-0003,
// service tab5_maj_pluie_1h_bulk). Retourne has_rain.
bool update_rain_bars_bulk_ui(const std::string& payload, lv_obj_t* const bars[9]);

// Icône « pluie prédictive » de la carte centrale : flocon ambre si la
// probabilité de neige ≥ 5, sinon goutte colorée par l'hygrométrie
// (get_humidity_color). Appelée par tab5_maj_probabilites ET
// tab5_maj_meteo_actuelle : les deux services partagent le même rendu.
void update_rain_predict_icon_ui(lv_obj_t* icon, int neige, float humidite);

// Clim, retour HA (service tab5_maj_clim) : cible (carte + popup + arc) et
// température intérieure du popup. Les modes restent dans les globals,
// recolorés par le script tab5_clim_recolor. Distinct de update_clim_target_ui()
// (affichage optimiste local, plus bas), qui n'écrit que la cible.
void update_clim_from_ha_ui(lv_obj_t* lbl_target, lv_obj_t* lbl_target_popup, lv_obj_t* arc,
    lv_obj_t* lbl_current, float target, float current);

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

// Repose la meme valeur de volume (0..1) sur les DEUX sliders de l'ecran + le
// label % de la console. Appele par script.tab5_volume_apply, point d'entree
// unique du volume (ecran, Home Assistant, rattrapage media_player).
// lv_slider_set_value ne declenche PAS LV_EVENT_VALUE_CHANGED : reposer la
// valeur sur le slider d'ou vient le geste ne reboucle pas sur on_value.
void ui_sync_volume_widgets(lv_obj_t* slider_console, lv_obj_t* lbl_console_pct,
    lv_obj_t* slider_assist, float volume);

// Repose l'etat muet/non-muet sur les DEUX icones qui le representent : celle
// de la barre du dashboard (`icon_mute`) et celle du popup assistant
// (`icon_assist_mute`). Elles peignent le meme `system_muted` : chaque endroit
// qui le change doit passer par ici, sinon l'une des deux ment (couper le son
// depuis le popup laissait l'icone du dashboard sur « son actif », et
// inversement).
void ui_sync_mute_icons(lv_obj_t* icon_main, lv_obj_t* icon_assist, bool muted);

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

// -----------------------------------------------------------------------------
// Pipeline vocal — un état, une couleur d'icône micro, un libellé de statut.
// Remplace les 5 blocs identiques des callbacks voice_assistant: de
// tab5-hardware.yaml (audit du 06/09/2026 §4.1 point 6).
// -----------------------------------------------------------------------------
enum class AssistState : uint8_t {
    IDLE,       // gris   « Prêt »
    LISTENING,  // vert   « Écoute… »
    THINKING,   // orange « Analyse… » (sert aussi d'accusé de réception du volet)
    SPEAKING,   // bleu   « Réponse »
    ERROR,      // rouge  « Erreur »
};

// Icône micro du dashboard + label statut du popup Assistant (nuls acceptés).
void assist_set_pipeline_state(lv_obj_t* icon_mic, lv_obj_t* lbl_status, AssistState st);

// Icône micro seule : retour au gris 2 s après une erreur, interruption,
// accusé de réception « Stop » volet — le label du popup n'est pas touché.
void assist_set_mic_state(lv_obj_t* icon_mic, AssistState st);

// Zone image de la réponse (service tab5_assist_reponse + callbacks online_image).
enum class AssistImage : uint8_t {
    NONE,     // pas d'image : indication et image masquées
    LOADING,  // « Chargement image... », image masquée le temps du téléchargement
    READY,    // image affichée, indication masquée
    ERROR,    // « Image indisponible », image masquée
};
void assist_image_state_ui(lv_obj_t* hint, lv_obj_t* img, AssistImage st);

// Indicateur « Ok Nabu: ON / OFF » du panneau switches (switch tab5_wake_word_active).
void assist_wake_word_indicator_ui(lv_obj_t* lbl, bool on);

// -----------------------------------------------------------------------------
// Décision du mot de réveil (on_wake_word_detected, tab5-hardware.yaml) —
// audit §4.1 point 7 : les 5 niveaux d'if/else du YAML deviennent une table.
// Le YAML lit les entrées UNE fois, appelle decide(), puis le script
// tab5_wake_word_dispatch (tab5-scripts.yaml) exécute l'action.
// -----------------------------------------------------------------------------
namespace WakeWord {

enum Action : uint8_t {
    ALARM_STOP,        // le réveil sonne : TOUT mot l'arrête, avant tout le reste
    VOLET_STOP,        // « Stop » pendant que le volet bouge : arrêt local + HA
    INTERRUPT_LISTEN,  // réponse en cours (va_stop_armed + audio) : on coupe et on ré-écoute
    START_PIPELINE,    // « Okay Nabu » au repos, wake word actif et HA joignable
    IGNORE_STOP,       // « Stop » sans rien à arrêter
    IGNORE_INACTIVE,   // « Okay Nabu » mais wake word désactivé ou HA injoignable
};

struct Inputs {
    bool alarm_ringing;
    bool is_stop;            // wake_word == "Stop"
    bool volet_en_mouvement;
    bool va_stop_armed;
    bool audio_busy;         // haut-parleur actif, annonce en cours, ou pipeline pas démarré
    bool wake_word_enabled;  // switch tab5_wake_word_active
    bool api_connected;
};

// Table de décision, dans l'ordre de priorité :
//   alarm_ringing                                  → ALARM_STOP
//   is_stop && volet_en_mouvement                  → VOLET_STOP
//   va_stop_armed && audio_busy                    → INTERRUPT_LISTEN (Stop ou Okay Nabu)
//   is_stop                                        → IGNORE_STOP
//   wake_word_enabled && api_connected             → START_PIPELINE
//   sinon                                          → IGNORE_INACTIVE
Action decide(const Inputs& in);
const char* action_name(Action a);  // libellé pour les logs

}  // namespace WakeWord

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
}

