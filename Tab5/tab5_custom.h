/**
 * [AI-CONTEXT]
 * @file tab5_custom.h
 * @role Contrat C++ du HMI : déclarations appelées par les lambdas YAML, structures
 *       des widgets, état partagé. Seul en-tête public de la couche `tab5_*.cpp`.
 * @architecture_constraint Les jetons (UIColor, UIAnim, UIIdle) vivent dans
 *       `tab5_tokens.h`, inclus ici : les lambdas les voient comme avant, et les
 *       jeux peuvent n'inclure que les jetons (25/09/2026, audit lot 8a).
 * @ai_instruction Ne JAMAIS recréer des constantes de couleurs ailleurs : ajouter
 *       un jeton dans `tab5_tokens.h`.
 */
#pragma once
#include "esphome.h"
#include "tab5_tokens.h"
#include "tab5_core.h"
#include "tab5_i18n.h"
#include <initializer_list>
#include <string>
#include <vector>

// Données calendrier/prévisions, dates locales, jours et mois en toutes lettres :
// logique PURE, déclarée dans tab5_core.h (compilable et testable sur PC).
namespace esphome { namespace font { class Font; } }
// Icône météo d'une tuile (police 120 px, 80 px pour le petit calque 2).
void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, const std::string& state, esphome::font::Font* f_card, esphome::font::Font* f_card_s);

uint32_t get_humidity_color(float x);

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
    // Bouton et épaules d'appareil : plus lus depuis l'ADR-0023 (pièces) — ces widgets
    // sont dans g_tuiles_ui (tab5_tuiles.cpp). Les champs restent : l'on_boot de
    // tab5-ha-hmi.yaml, intouchable, initialise la structure entière.
    lv_obj_t* action_btn;
    lv_obj_t* action_icon1;
    lv_obj_t* action_icon2;
    lv_obj_t* extra_btn; // e.g. direction shutter button
};

void parse_and_update_jours_bulk(const std::string& payload);

// Garde anti-rendu des poussées HA (audit du 25/09/2026, lot 3). HA repousse tout
// au cycle /10 min et à chaque (re)connexion, le plus souvent à l'identique ; or en
// LVGL 9 un setter réécrit et invalide même à valeur égale, et chaque poussée
// repeignait tout le bandeau météo (boucle 66 ms au repos → 144 ms au push).
// Vrai si `payload` est identique au précédent reçu sur ce canal (empreinte
// FNV-1a 32 bits + longueur ; la première réception après boot n'est jamais
// « identique »). Une collision ferait sauter UNE mise à jour, rattrapée au
// changement suivant.
enum class PushChannel : uint8_t {
    JOURS, HEURES_0, HEURES_1, HEURES_2, VIGILANCE, PLUIE, ALERTES_HA, INFO, COUNT
};
bool push_unchanged(PushChannel ch, const char* data, size_t len);
bool push_unchanged(PushChannel ch, const std::string& payload);
// Les variables `string` des actions API arrivent en esphome::StringRef (vue sans
// copie) : passer `payload.c_str(), payload.size()` évite une copie de 2 Ko sur le
// tas pour le cas courant (push identique, rien d'autre à faire).

// Prévisions horaires reçues par blocs de 5 créneaux (idx 0, 5 ou 10 en tête) :
// analyse le bloc s'il a changé et renvoie true seulement s'il faut repeindre les
// tuiles À L'ÉCRAN — calque horaire visible (pages 0-1) et bloc de cette page
// (page 0 = créneaux 5-9, page 1 = 0-4 ; le bloc 10-14 n'est jamais affiché).
bool accept_heures_bulk(const std::string& payload, int forecast_page);

// Tableaux globaux des slots meteo (initialises au boot, fixes car ids LVGL constants).
// Evite la reconstruction identique dans chaque lambda YAML (D2).
extern WeatherDaySlot g_day_slots[5];
extern WeatherHourSlot g_hour_slots[5];

void refresh_daily_forecast(WeatherDaySlot slots[], int page_index,
    esphome::font::Font* f_card, esphome::font::Font* f_card_s);
void refresh_hourly_forecast(WeatherHourSlot slots[], int page_index,
    esphome::font::Font* f_card, esphome::font::Font* f_card_s);

// UIAnim (durées/amplitudes d'animation) et UIIdle (retour à l'accueil par
// inactivité) : voir tab5_tokens.h.

// Millisecondes écoulées depuis la dernière activité (appui tactile ou
// ui_mark_activity()).
uint32_t ui_idle_ms();

// Remet le compteur d'inactivité à zéro sans qu'il y ait eu de toucher.
// À appeler sur toute activité « invisible » qui doit garder l'écran en place
// (événements du pipeline vocal, ouverture programmée d'un popup).
void ui_mark_activity();

// Ouverture d'un popup : affichage instantané (pas de fondu) ET passage au
// premier plan de son parent — ne pas le refaire après l'appel.
void animate_popup_open(lv_obj_t* card);

// Fermeture d'un popup : masquage instantané (LV_OBJ_FLAG_HIDDEN).
void animate_popup_close(lv_obj_t* card);

// Fondu croisé pur (sans glissement) entre deux calques plein cadre —
// bascule prévisions <-> switches HA (bouton « HA »). Le calque sortant est
// masqué à la fin. Durée UIAnim::SWIPE_DUR.
void animate_crossfade_layers(lv_obj_t* out_layer, lv_obj_t* in_layer);

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
// La géométrie (cadres, position des labels et du « : ») est posée dans
// tab5-lvgl.yaml seulement, vérifiée par tests/test_horloge.py ; la course du
// rouleau est la hauteur du cadre, lue dans son style.
// =============================================================================
struct ClockDigitRoller {
    lv_obj_t* wrap = nullptr;
    lv_obj_t* lbl[2] = {nullptr, nullptr};
    uint8_t   cur = 0;      // index du label actuellement affiché (0/1)
    char      shown = 0;    // chiffre peint ('0'..'9'), 0 = jamais peint
};

struct ClockRollerCtx {
    ClockDigitRoller d[4];    // HH:MM -> d[0] d[1] : d[2] d[3]
};
extern ClockRollerCtx g_clock_roller;

// --- 1D : Micro-interactions boutons verre ---

// Parcourt l'arbre LVGL depuis root et applique setup_button_press_animation()
// a tout objet clickable avec radius 18 (caracteristique du style_clim_btn verre).
// Appele une fois au boot via un interval one-shot (apres layout LVGL).
void apply_pressed_scale_to_tree(lv_obj_t* root);

// --- Langue de l'écran (lot 4, 27/09/2026) ---

// Traduit une fois, en fin de setup, les textes posés par le YAML sous chacune des
// racines (les pages LVGL). Sans effet en français. Voir tab5_i18n.h.
void i18n_apply_boot(std::initializer_list<lv_obj_t*> racines);

// Le jeu de bille vit desormais dans marble_game.h / marble_game.cpp
// (namespace Marble). L'ancien prototype `namespace Game` a ete retire.

// Surbrillance bordure bouton (actif = couleur + active_width px, 2 par défaut ;
// inactif = GLASS_RIM + 1px). Le sélecteur du popup lumière passe 3 px.
void highlight_button_border(lv_obj_t* btn, bool active, uint32_t color, int32_t active_width = 2);

// =============================================================================
// Contexte carte centrale : regroupe les 8 wrappers LVGL + 7 flags d'activite
// + l'index du panneau courant. Reduit les signatures de 16 parametres a 1.
// Pointeurs LVGL poses une fois dans on_boot (ids fixes). Les flags et
// current_panel n'existent QUE ici : services HA, scripts YAML et C++ lisent et
// ecrivent g_central_ctx directement (source unique depuis le lot 7 de l'audit
// du 26/09/2026 ; avant, 8 globals ESPHome les doublaient, recopies avant et
// apres chaque appel). Les services HA peuvent y ecrire avant on_boot, qui
// attend l'API jusqu'a 30 s : d'ou les pointeurs nuls toleres partout.
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
    // Zone PLANNING absente (lot 5) : le panneau 0 sort du rotateur.
    bool planning_off = false;
    int current_panel = 0;
    // Qui occupe la carte (audit du 25/09/2026, §2.4) : le rotateur n'a la main que
    // sur l'accueil (page 2), hors planning temporaire et hors réponse vocale. Tenus à
    // jour côté C++ (apply_forecast_page, show/hide_vocal_response_ui) plutôt que par
    // des pointeurs vers les globals ESPHome, pour ne pas toucher à on_boot.
    // forecast_page = page des prévisions affichée (0-1 horaire, 2-4 journalier, 2 =
    // accueil, non restaurée) : seule source depuis le 28/09/2026, le global
    // forecast_page_index qui la recopiait est retiré. Les lambdas la lisent ici.
    int forecast_page = 2;
    bool vocal_shown = false;
    lv_obj_t* vocal_wrap = nullptr;   // posé par show_vocal_response_ui
    // Mode HA (bouton « HA », ADR-0023) : les cartes montrent la pièce de la page
    // courante, la carte centrale son titre ; le rotateur n'a pas la main. Seule source
    // (plus de global show_switches), basculé par tuiles_mode_ha() (tab5_tuiles.cpp).
    bool ha_mode = false;
};

// Contexte global unique (initialise dans tab5-ha-hmi.yaml on_boot ou premier usage).
extern CentralPanelCtx g_central_ctx;

// Gestion du geste de swipe (page_main.on_gesture) : pagination previsions
// horaires/journalieres (0-4) dans la bande centrale+basse (y >= 333). Console diag :
// uniquement via btn_control_console (plus de swipe haut/bas). En mode HA (ADR-0023) :
// pièce suivante / précédente qui a des appareils, les calques météo restent masqués.
// La page courante est ctx.forecast_page.
void handle_swipe_gesture(lv_dir_t dir, int32_t pt_y,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_card, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx);

// Retour au panneau météo principal (page 2 = prévisions journalières J0-J4),
// déclenché par l'inactivité tactile — mêmes effets qu'un swipe manuel jusqu'à
// cette page (données, calque, pastilles, carte centrale), le chemin est
// factorisé avec handle_swipe_gesture().
// Ne fait rien si on y est déjà : c'est appelé une fois par seconde.
void reset_forecast_to_main_page(
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_card, esphome::font::Font* f_card_s,
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
// Pose ctx.has_info ; bandeau vidé alors qu'il est affiché → retour au planning.
void update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, const std::string& meteo_id,
    std::string& dismissed_local, CentralPanelCtx& ctx,
    esphome::font::Font* font_small, esphome::font::Font* font_large);

// Phrase pluie (lot 4c, 27/09/2026) : HA envoie un code « @niveau,début » ; la
// tablette compose la phrase dans sa langue et décompte « dans N mn ». Appelée à
// chaque minute (on_time de sntp_time, tab5-sensors-diagnostics.yaml).
void rain_phrase_tick();

// Rotateur carte centrale : 0 planning, 1 pluie, 2 vigilance MF, 3 info (phrase test),
// 4-7 alertes HA individuelles (8s, même timer global).
constexpr int kCentralPanelCount = 8;
constexpr int kHaAlertPanelBase = 4;
constexpr int kHaAlertSlotCount = 4;

void advance_central_panel_rotator(CentralPanelCtx& ctx);

// Un bandeau HA : ses widgets et son id d'acquittement. Sa présence est
// ctx.has_ha[slot], posée par parse_and_update_ha_alerts_bulk.
struct HaAlertSlotUI {
    lv_obj_t* wrap;
    lv_obj_t* lbl;
    std::string* id_store;
};

// La police des bandeaux est celle du YAML (ha_alert_panel.yaml, roboto_45_b) :
// la reposer à chaque push relançait la mise en page pour rien (audit 26/09, lot 3).
void parse_and_update_ha_alerts_bulk(const std::string& payload, HaAlertSlotUI slots[4],
    CentralPanelCtx& ctx, std::string& dismissed_local);

// Masquage immédiat au tap (feedback visuel avant le round-trip HA).
void dismiss_central_info_immediate(lv_obj_t* lbl_info, CentralPanelCtx& ctx);
void dismiss_ha_alert_slot_immediate(int slot_idx, lv_obj_t* wrap, lv_obj_t* lbl,
    std::string& id_store, CentralPanelCtx& ctx);

void tab5_dismiss_local_add(std::string& store, const std::string& id);

// -----------------------------------------------------------------------------
// Services HA (tab5-api-logic.yaml) : logique LVGL sortie des lambdas le
// 08/09/2026 (ADR-0006, audit du 06/09 §4.1 point 1). Un service ne fait plus
// que résoudre ses `id()`, poser ses globals et appeler l'une de ces fonctions ;
// tools/check_tab5_code_rules.py (pytest) interdit tout `lv_*` dans le contrat
// (hors lv_obj_has_flag, lecture pure).
// -----------------------------------------------------------------------------

// Volet (tab5_maj_volet_etat) : voir « Pièces et tuiles » plus bas — le volet 3.x est
// la tuile 1 de la pièce 0 du mode héritage (tuiles_heritage_volet).

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
// passer à central_set_vigilance().
bool parse_and_update_vigilance(const std::string& payload, const VigilanceUI& ui);

// Même chose pour les 9 barres en un appel : payload « idx|intensité;… » (ADR-0003,
// service tab5_maj_pluie_1h_bulk). Retourne has_rain, à passer à central_set_pluie().
bool update_rain_bars_bulk_ui(const std::string& payload, lv_obj_t* const bars[9]);

// Panneaux pluie (1) et vigilance (2) du rotateur : pose has_rain / has_mf_alerts.
// Fin de la pluie ou de la vigilance → le panneau affiché cède la place tout de
// suite ; début alors que la carte est vide → il s'affiche sans attendre le tour.
void central_set_pluie(bool actif);
void central_set_vigilance(bool actif);

// Icône « pluie prédictive » de la carte centrale : flocon ambre si la
// probabilité de neige ≥ 5, sinon goutte colorée par l'hygrométrie
// (get_humidity_color). Appelée par tab5_maj_probabilites ET
// tab5_maj_meteo_actuelle : les deux services partagent le même rendu.
void update_rain_predict_icon_ui(lv_obj_t* icon, int neige, float humidite);

// Clim, retour de HA : clim_blueprint_recu() (service tab5_maj_clim), plus bas avec
// les réglages de la clim et le popup (ADR-0026, ADR-0027, tab5_cards.cpp).

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

// Console système, carte RÉSEAU : ligne « HA » (Connecte / Hors ligne). Appelée par
// l'interval 2 s de tab5-sensors-diagnostics.yaml quand la console est visible.
void update_console_ha_status_ui(lv_obj_t* lbl, bool ha_ok);

// Version du logiciel ESP-Hosted du co-processeur Wi-Fi (ESP32-C6), « x.y.z », lue par
// RPC sur le lien SDIO (réponse attendue au plus 1 s). false si le lien ne répond pas.
// Capteur « Tab5 C6 Version » de tab5-sensors-diagnostics.yaml.
bool read_c6_firmware_version(char* out, size_t n);

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
// icons_utf8[5] = codes MDI pour chaque capteur, slots[4] = widgets LVGL de destination.
// Seuls les pots présents comptent (zones POT_1 à POT_5) : jusqu'à 4, chacun a son
// emplacement ; les emplacements en trop sont masqués. Les entrées sont gardées pour
// moisture_slots_refresh(), que zones_apply_ui() rejoue quand un pot apparaît.
void sort_and_update_moisture_slots(float values[5], const char* icons_utf8[5],
    MoistureSlotUI slots[4]);
void moisture_slots_refresh();

// Couleur batterie par niveau (échelle icône téléphone du bandeau, réutilisée
// par la ligne Batterie du popup détails pots).
uint32_t get_battery_color(float x);

// Icônes d'état du bandeau et des cartes (règle 2 : les sensors n'appellent pas
// LVGL). set_icon_color_ui pose une couleur calculée (batterie, humidité) ;
// set_icon_active_ui choisit entre deux tokens UIColor selon un booléen (API HA,
// Wi-Fi, TV). Tolèrent un widget nullptr (valeur reçue avant le layout).
void set_icon_color_ui(lv_obj_t* icon, uint32_t color);
void set_icon_active_ui(lv_obj_t* icon, bool active, uint32_t color_on, uint32_t color_off);

// PC (text_sensor pc_status) : icône du bandeau d'état. La carte du mode HA est la
// tuile 0 de la pièce 0 en mode héritage (tuiles_heritage_pc). Ne fait rien sans icon_pc.
void update_pc_status_ui(bool active, lv_obj_t* icon_pc);

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

// Affichage optimiste de la cible de la carte clim de l'accueil (boutons − / +) avant
// le retour de HA (tab5_maj_clim). La carte montre toujours la clim du blueprint ; le
// popup a ses propres gestes (clim_popup_*, plus bas).
void update_clim_target_ui(lv_obj_t* lbl_target, lv_obj_t* arc, float target);

// =============================================================================
// Réglages de la clim venus de l'appareil (tab5_cards.cpp, ADR-0026) : clé « climr »
// de tab5_maj_emplacements, « climr|min|max|pas|unité|capacités|nom », que le
// blueprint pousse avant tab5_maj_clim. Bornes et pas des boutons − / + et de l'arc,
// °C ou °F, boutons que l'appareil gère, nom de la clim en titre du popup. Tant que
// rien n'est reçu (blueprint plus ancien), rien ne change : 16-30, pas de 0,5, °C,
// tous les boutons, titre du YAML. Pas de NVS : le blueprint les renvoie à chaque
// connexion.
//
// Clims des tuiles (ADR-0027) : une tuile cli sans l'option m a sa clim à elle, décrite
// par les clés « crRT|… » (mêmes réglages que climr) et « ceRT|consigne|pièce|mode|
// préréglage|ventilation|oscillation » (champs de tab5_maj_clim). Le popup montre la
// « clim affichée » : celle du blueprint (défaut), ou celle de la tuile touchée ; ses
// gestes et ses commandes vont à celle-là. La carte de l'accueil reste celle du
// blueprint, dont l'état est dans ses globals (clim_target_temp, clim_hvac_mode…).
// =============================================================================
// Widgets adaptés, posés par le script tab5_clim_ui (tab5-scripts.yaml), que lance
// tab5_zones_apply à la fin du setup.
struct ClimUI {
    lv_obj_t* popup = nullptr;              // clim_options_popup
    lv_obj_t* consigne_carte = nullptr;     // clim_target (carte de l'accueil)
    lv_obj_t* consigne_popup = nullptr;     // clim_target_popup
    lv_obj_t* arc = nullptr;                // arc_temp_popup
    lv_obj_t* unite = nullptr;              // clim_unite_popup (sous la cible du popup)
    lv_obj_t* piece = nullptr;              // val_temp_int_popup (température de la pièce)
    lv_obj_t* titre = nullptr;              // popup_clim_title
    // Carte MODE (pile flex : un bouton masqué ne laisse pas de trou).
    lv_obj_t* mode_froid = nullptr;         // popup_btn_clim_cool
    lv_obj_t* mode_chaud = nullptr;         // popup_btn_clim_heat
    lv_obj_t* mode_sec = nullptr;           // popup_btn_clim_dry
    lv_obj_t* mode_ventilation = nullptr;   // popup_btn_clim_fan
    // Carte OPTIONS (positions absolues, recalculées : les sections s'empilent).
    lv_obj_t* titre_presets = nullptr;      // clim_titre_presets
    lv_obj_t* rangee_presets = nullptr;     // clim_rangee_presets (rangée flex Éco / Boost)
    lv_obj_t* eco = nullptr;                // popup_btn_clim_eco
    lv_obj_t* boost = nullptr;              // popup_btn_clim_boost
    lv_obj_t* titre_ventilation = nullptr;  // clim_titre_ventilation
    lv_obj_t* silence = nullptr;            // popup_btn_clim_quiet
    lv_obj_t* titre_flux = nullptr;         // clim_titre_flux
    lv_obj_t* oscillation = nullptr;        // popup_btn_clim_swing
    lv_obj_t* brise = nullptr;              // popup_btn_clim_windnice
    // Icônes que clim_recolorer() colore selon l'état de la clim affichée, et le libellé
    // « Chaud » (traduction propre à la clim, clé « clim|Chaud »).
    lv_obj_t* icone_froid = nullptr;        // popup_icon_clim_cool
    lv_obj_t* icone_chaud = nullptr;        // popup_icon_clim_heat
    lv_obj_t* icone_sec = nullptr;          // popup_icon_clim_dry
    lv_obj_t* icone_ventilation = nullptr;  // popup_icon_clim_fan
    lv_obj_t* icone_eteint = nullptr;       // popup_icon_clim_off
    lv_obj_t* icone_eco = nullptr;          // popup_icon_clim_eco
    lv_obj_t* icone_boost = nullptr;        // popup_icon_clim_boost
    lv_obj_t* icone_silence = nullptr;      // popup_icon_clim_quiet
    lv_obj_t* icone_oscillation = nullptr;  // popup_icon_clim_swing
    lv_obj_t* icone_brise = nullptr;        // popup_icon_clim_windnice
    lv_obj_t* libelle_chaud = nullptr;      // popup_lbl_clim_heat
    // État de la clim du blueprint : ses globals, qu'écrivent tab5_maj_clim et les gestes
    // (carte, popup quand il la montre).
    float* consigne_bp = nullptr;           // &id(clim_target_temp)
    std::string* mode_bp = nullptr;         // &id(clim_hvac_mode)
    std::string* preset_bp = nullptr;       // &id(clim_preset_mode)
    std::string* ventilation_bp = nullptr;  // &id(clim_fan_mode)
    std::string* oscillation_bp = nullptr;  // &id(clim_swing_mode)
    // Débounces de la consigne du popup (lambdas sans capture, comme g_tuiles_ui.envoyer) :
    // tab5_debounce_clim_temp (clim du blueprint, emplacement « clim ») et
    // tab5_debounce_clim_tuile (clim d'une tuile, emplacement et valeur pris au geste).
    void (*debounce_blueprint)() = nullptr;
    void (*debounce_tuile)() = nullptr;
};
extern ClimUI g_clim_ui;

// Boutons − / + de la carte de l'accueil (clim du blueprint) : un pas de plus (sens > 0)
// ou de moins, borné aux limites de l'appareil. NaN reste NaN (consigne inconnue).
float clim_consigne_suivante(float t, int sens);
// Consigne envoyée à HA (scripts tab5_debounce_clim_temp et _tuile) : deux décimales au
// plus, sans zéro final (« 21.5 », « 72 », « 21.25 ») ; le blueprint la relit en nombre.
std::string clim_consigne_texte(float t);

// Bascules du popup et coloration (clim_recolorer) : un mode « actif » sous tous les
// noms que lui donnent les appareils, les mêmes que les listes du blueprint (clim_eco,
// clim_silence, clim_oscillation ; tests/test_clim.py compare). La bascule envoie alors
// none / auto / stop, sinon away / quiet / swing : le blueprint traduit vers le mode que
// l'appareil connaît.
bool clim_eco_actif(const std::string& preset);
bool clim_silence_actif(const std::string& fan);
bool clim_oscillation_actif(const std::string& swing);
// Bouton de préréglage `bouton` (away = Éco, boost) : actif sur ce préréglage.
bool clim_preset_actif(const std::string& preset, const char* bouton);

// Retour de HA pour la clim du blueprint (tab5_maj_clim, après ses globals) : cible de la
// carte, et du popup (cible, arc, température de la pièce) s'il montre cette clim ; puis
// les couleurs.
void clim_blueprint_recu(float consigne, float piece);
// Couleurs : la cible de la carte d'après le mode de la clim du blueprint, la cible et
// les icônes du popup d'après la clim affichée.
void clim_recolorer();

// Clim affichée par le popup (ADR-0027). Celle du blueprint : carte de l'accueil, tuile
// avec l'option m, « Aller à l'écran → Climatisation », et à la fermeture (croix, voile).
// Celle d'une tuile : clim_afficher_tuile() (tab5_internal.h), à l'appui de la tuile.
void clim_afficher_blueprint();
// Emplacement des commandes du popup : « clim » (clim du blueprint) ou « tRT ».
const char* clim_affichee_cle();
// Gestes du popup, sur la clim affichée : affichage optimiste et couleurs (l'envoi suit,
// dans le YAML, avec clim_affichee_cle() et la valeur ci-dessous). La consigne part par
// l'un des deux débounces de ClimUI.
void clim_popup_consigne(float t);            // arc
void clim_popup_pas(int sens);                // − / + (consigne inconnue : rien)
void clim_popup_mode(const char* mode);       // Froid, Chaud, Sec, Ventilation, Éteint (off)
void clim_popup_preset(const char* bouton);   // Éco (away), Boost : le bouton ou none
void clim_popup_silence();                    // quiet ↔ auto
void clim_popup_oscillation();                // swing ↔ stop
void clim_popup_brise();                      // windnice ↔ stop
const std::string& clim_affichee_preset();
const std::string& clim_affichee_ventilation();
const std::string& clim_affichee_oscillation();
// Consigne d'une clim de tuile en attente (script tab5_debounce_clim_tuile) : emplacement
// et valeur pris au geste, pour qu'elle parte à sa clim même si le popup est déjà fermé.
const char* clim_tuile_attente_cle();
std::string clim_tuile_attente_texte();

// Tap tuile météo : affiche le planning/horaires du jour dans la carte centrale (6s).
// tuile = position 0-4 sur le calque journalier ; le jour (0-14) se déduit de
// ctx.forecast_page (pages 2-4). Le timer de restauration rétablit ctx.current_panel
// (le tap l'a mis à 0).
void show_temporary_planning(int tuile, lv_obj_t* lbl_planning,
                             lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
                             const std::string& plan_l1, const std::string& plan_l2,
                             CentralPanelCtx& ctx);
// Vrai pendant les 6 s du planning du tap (son timer tourne). Lu par les scripts du
// rotateur et par la poussée des prévisions (bandeau planning laissé tel quel).
bool temp_planning_active();

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

// -----------------------------------------------------------------------------
// Pipeline vocal — un état, une couleur d'icône micro, un libellé de statut.
// Remplace les 5 blocs identiques des callbacks voice_assistant: (alors dans
// tab5-hardware.yaml, dans tab5-assist.yaml depuis le lot 8c ; audit du 06/09/2026 §4.1 point 6).
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
// Décision du mot de réveil (on_wake_word_detected, tab5-assist.yaml) —
// audit §4.1 point 7 : les 5 niveaux d'if/else du YAML deviennent une table.
// Le YAML lit les entrées UNE fois, appelle decide(), puis le script
// tab5_wake_word_dispatch (tab5-assist.yaml) exécute l'action.
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

// Police de la réponse pour la taille `assist_text_size` : 2 → L, toute autre
// valeur → S (dont le 1 de l'ancien M, essai D8 du 26/09/2026).
esphome::font::Font* assist_font(int size_idx, esphome::font::Font* f_s, esphome::font::Font* f_l);

// Applique la taille de police de la réponse (0=S 2=L) SANS perdre le texte déjà
// affiché (relit lv_label_get_text). Met aussi à jour les 2 boutons A- / A+.
void assist_apply_text_size(lv_obj_t* lbl_response, int size_idx,
    esphome::font::Font* f_s, esphome::font::Font* f_l, lv_obj_t* btn_s, lv_obj_t* btn_l);

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

struct CalDetailLineUI {
    lv_obj_t* icon;   // glyphe MDI typé (travail/férié/vacances/RDV/anniv/fête)
    lv_obj_t* txt;    // texte de la ligne
};

// Cache mensuel (TTL + eviction : max 3 mois M-1/M/M+1, stale-while-revalidate)
bool cal_month_needs_fetch(int year, int month);
// true si le mois est en cache mais plus vieux que ttl_ms (refresh silencieux conseillé)
bool cal_month_is_stale(int year, int month, uint32_t ttl_ms = 600000);  // défaut 10 min
// Pré-fetch du démarrage et de la reconnexion (tab5_cal_prefetch, tab5-calendar.yaml) :
// un mois reçu il y a moins de 30 s n'est pas redemandé. Au démarrage, on_boot et
// status_ha lancent tous deux le pré-fetch, à 3 s d'écart ; une reconnexion de HA
// plus tard redemande le mois, reçu il y a plus de 30 s.
constexpr uint32_t CAL_PREFETCH_FRESH_MS = 30000;
// Évince les mois distants de >1 par rapport à (year, month) — garde max 3 entrées.
void cal_cache_evict_distant(int year, int month);
// Décale (year, month) de `delta` mois en passant l'année (déc. + 1 = janv. suivant).
void cal_shift_month(int& year, int& month, int delta);
void cal_store_month_data(const std::string& annee, const std::string& mois,
    const std::string& codes, const std::string& heures, const std::string& details = "");

// Construit les 42 cellules de la grille (168 px de large, lundi en tête) dans le
// parent de `anchor`, juste avant lui (la légende) : même rang que l'ancien gabarit
// cal_day_cell.yaml (lot 8, 26/09/2026). `grid_y` = ${cal_grid_y}, `grid_h` =
// ${cal_grid_h} : la hauteur et le y des lignes sont posés par cal_render_month()
// selon le nombre de semaines du mois. Un tap court sur la cellule i appelle
// on_tap(i). Une seule fois : false si la grille existe déjà (appeler à chaque
// ouverture ne coûte rien).
bool cal_grid_build(lv_obj_t* anchor, int32_t grid_y, int32_t grid_h,
                    const esphome::font::Font* font_num,
                    const esphome::font::Font* font_text, void (*on_tap)(int));

// Rendu complet du mois affiché dans la grille : numéros + alignement lundi-dimanche
// + weekend + aujourd'hui calculés localement, enrichissement HA appliqué si le mois
// est en cache. Sans effet sur les cellules tant que cal_grid_build() n'a pas tourné.
void cal_render_month(lv_obj_t* lbl_month,
    int view_year, int view_month, int today_year, int today_month, int today_day);

// "" si la cellule est hors mois, sinon date ISO "YYYY-MM-DD" du jour tapé.
std::string cal_date_for_cell(int view_year, int view_month, int cell_idx);

// Détail jour embarqué dans le payload mois (champs séparés par ~). "" si absent.
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

// =============================================================================
// Journal des démarrages et des coupures (tab5_journal.cpp, 26/09/2026) : plantages
// et pannes du lien Wi-Fi (C6), envoyés à HA dans l'événement esphome.tab5_journal.
// =============================================================================
// logger: on_message (tab5-hardware.yaml) : garde les erreurs, et les avertissements
// tant que Home Assistant n'est pas connecté.
void journal_log_message(uint8_t level, const char* tag, const char* message);
// interval 30 s (tab5-sensors-diagnostics.yaml) : copie en NVS quand le Wi-Fi manque 90 s.
void journal_tick();
// Script tab5_journal_envoi, à chaque connexion de HA :
bool journal_has_report();          // autre chose qu'un démarrage normal
bool journal_is_serious();          // plantage, erreur ou démarrage sans HA
std::string journal_reset_reason(); // raison du dernier démarrage, en clair
// Filtre du capteur « Tab5 Raison du redémarrage » : premier démarrage après une
// installation par l'USB (flash effacée) → « First boot after install (…) ».
std::string journal_raison_ha(const std::string& raison);
std::string journal_boot_count();   // démarrages depuis le dernier envoi
std::string journal_report_text();  // lignes en attente, une par ligne
void journal_mark_delivered();      // vide le journal (et sa copie NVS)

// =============================================================================
// Fuseau horaire de Home Assistant (tab5_services.cpp, lot 6b, ADR-0020). HA l'envoie
// avec l'heure (time: platform: homeassistant) mais rien ne le garde : sans ces deux
// fonctions, un démarrage sans HA repartirait avec le fuseau de la compilation.
// =============================================================================
// Démarrage (interval de tab5-sensors-diagnostics.yaml) : remet le dernier fuseau reçu.
// N'agit qu'au premier appel.
void fuseau_restaurer();
// on_time_sync de l'horloge homeassistant : HA a donné l'heure, donc son fuseau.
void fuseau_recu_de_ha();
// Tick minute : range en NVS le fuseau de HA s'il a changé depuis le dernier rangement.
void fuseau_memoriser();

// =============================================================================
// Zones optionnelles (tab5_zones.cpp, lot 5 de l'audit « ouverture », 27/09/2026)
// Une zone dont l'entité n'existe pas dans Home Assistant disparaît, avec ses
// boutons. La tablette ne décide pas seule : une entité créée pendant le démarrage
// de HA n'est transmise qu'à son prochain changement (manager.py de l'intégration
// ESPHome), un silence ne prouve donc rien. Elle demande à HA (esphome.tab5_zones,
// tab5-zones.yaml) et HA répond les zones absentes (action tab5_maj_zones) : depuis le
// lot 6a, le blueprint « Tab5 — emplacements », qui sait quels emplacements sont
// choisis et si leurs entités existent (ADR-0019). Une entité en panne existe :
// sa zone reste affichée (« -- », « Hors ligne »). Sans le package, rien ne
// disparaît. La liste est gardée en NVS (pas de clignotement au démarrage) et une
// zone réapparaît dès sa première donnée.
// =============================================================================
enum class Zone : uint8_t {
    // Emplacement suivi par la tablette (tab5-sensors-domotique.yaml), même ordre que
    // la liste envoyée par le script tab5_zones_demande.
    LUMIERE_1, LUMIERE_2, LUMIERE_3,   // chambre, salon, LEDs (tuiles J2 à J4)
    PC, TV, TELEPHONE, SALON, SERRE,
    POT_1, POT_2, POT_3, POT_4, POT_5,
    // Décidées par HA seul (entités du package, pas de la tablette).
    CLIM, VOLET, PLANNING,
    // Pipeline de discussion (29/09/2026) : absente quand la liste « Tab5 · pipeline de
    // discussion » vaut « Aucun » ; masque les boutons Domo / Discu. Toute nouvelle zone
    // s'ajoute ICI, à la fin : les bits sont gardés en NVS dans cet ordre.
    DISCUSSION,
    COUNT
};
constexpr int kZonesSuivies = static_cast<int>(Zone::CLIM);

// Bandeau d'état (haut gauche) : ses icônes dans l'ordre d'affichage, de gauche à droite.
// Les icônes visibles se suivent au pas de 35 px depuis x = 10 (bandeau_apply_ui,
// tab5_zones.cpp) : une icône masquée ne laisse pas de trou. Une icône de plus :
// 1. sa valeur ici, à sa place dans l'ordre (avant BANDEAU_NB) ;
// 2. son label dans tab5-lvgl.yaml (mdi_font_26, y: 10, x de sa place) et ses glyphes
//    dans mdi_font_26 (tab5-styles.yaml, règle 9) ;
// 3. son pointeur dans le script tab5_zones_apply (tab5-zones.yaml) ;
// 4. si elle peut disparaître, sa condition dans bandeau_masquee() (tab5_zones.cpp).
enum BandeauIcone : uint8_t {
    BANDEAU_PC,         // icon_pc : PC allumé (zone PC)
    BANDEAU_TELEPHONE,  // icon_phone : batterie du téléphone (zone TELEPHONE)
    BANDEAU_WIFI,       // icon_wifi
    BANDEAU_REVEIL,     // icon_alarm_status
    BANDEAU_BATTERIE,   // icon_batterie : batterie de la tablette, si elle est montée
    BANDEAU_NB
};

// Widgets que le masquage touche, posés par le script tab5_zones_apply (tab5-zones.yaml).
struct ZonesUI {
    lv_obj_t* bandeau[BANDEAU_NB] = {};  // bandeau d'état, indexé par BandeauIcone
    lv_obj_t* btn_ha = nullptr;        // rangée HA / Sys / TV
    lv_obj_t* btn_sys = nullptr;
    lv_obj_t* btn_tv = nullptr;
    // Les cartes du calque « HA » et le sélecteur du popup lumière suivent les pièces
    // depuis l'ADR-0023 (g_tuiles_ui, tab5_tuiles.cpp).
    lv_obj_t* clim_zone = nullptr;     // − / consigne / +
    lv_obj_t* icon_salon = nullptr;
    lv_obj_t* val_salon = nullptr;
    lv_obj_t* icon_serre = nullptr;    // devient une manette sans capteur de serre
    lv_obj_t* val_serre = nullptr;
    lv_obj_t* pots_row = nullptr;      // rangée des pots de l'accueil
    lv_obj_t* pots_zone = nullptr;     // sa zone d'appui long
    lv_obj_t* pot_card[5] = {};        // cartes du popup « Mes Plantes »
    // Mode vocal Domotique / Discussion (zone DISCUSSION) : boutons de l'accueil, du
    // popup assistant, et le titre « Cerveau / LLM » de ce dernier.
    lv_obj_t* btn_domo = nullptr;          // btn_mode_domo
    lv_obj_t* btn_discu = nullptr;         // btn_mode_discu
    lv_obj_t* assist_domo = nullptr;       // btn_assist_pipe_domo
    lv_obj_t* assist_discu = nullptr;      // btn_assist_pipe_discu
    lv_obj_t* assist_cerveau = nullptr;    // lbl_assist_cerveau
};
extern ZonesUI g_zones_ui;

bool zone_absente(Zone z);
// Donnée reçue : la zone réapparaît si elle était masquée. Vrai si l'affichage
// doit changer (le YAML relance alors tab5_zones_apply).
bool zone_vue(Zone z);
// Réponse de HA : clés des zones absentes, séparées par des virgules. Vrai si
// l'affichage doit changer.
bool zones_reponse_ha(const std::string& absentes);
// Applique l'état des zones aux widgets de g_zones_ui et aux tuiles (g_day_slots).
void zones_apply_ui();

// Batterie de la tablette, icône du bandeau d'état (tab5_zones.cpp). L'icône n'est
// visible que si l'interrupteur « Tab5 Batterie montée » est allumé
// (tab5-ha-controls.yaml) : sans batterie, le chargeur dit « en charge, 100 % »
// (relevé le 03/10/2026), la tablette ne peut donc pas savoir seule qu'il n'y en a
// pas. Glyphe selon le niveau (et « en charge »), couleur de get_battery_color(),
// la même échelle que le téléphone. Chaque appel garde sa valeur : appelés avant le
// premier zones_apply_ui() (restauration de l'interrupteur au setup), ils ne
// dessinent rien, et zones_apply_ui() peint ensuite l'état gardé.
void batterie_montee_ui(bool montee);   // on_state de l'interrupteur
void batterie_niveau_ui(float niveau);  // % de batterie_niveau, NAN = inconnu
void batterie_charge_ui(bool en_charge);  // batterie_en_charge (CHG_STAT)
// Tuile i (0 à 4) de l'accueil : son appareil est-il absent ?
bool zone_tuile_absente(int tuile);
// Nombre de pots présents (0 à 5).
int zones_pots_presents();
// « aucune » ou « clim, pot_4, pot_5 » (capteur « Zones masquées » dans HA).
std::string zones_texte_masquees();
// Demande à HA une fois par connexion : remis à zéro à chaque connexion de HA,
// consommé par la première poussée des prévisions.
void zones_nouvelle_connexion();
bool zones_demande_a_envoyer();

// Carte centrale : le planning n'est plus un panneau du rotateur quand HA n'a
// pas d'agenda de travail (zone PLANNING).
void central_planning_set_off(bool off);

// =============================================================================
// Emplacements de la maison (lot 6a, ADR-0019) : la tablette ne connaît plus aucune
// entité. Le blueprint « Tab5 — emplacements » pousse « clé|état|valeur;… »
// (action tab5_maj_emplacements) ; chaque clé est publiée dans le capteur interne qui
// la porte, dont le on_value met l'écran à jour comme avant. Les commandes repartent
// en événements esphome.tab5_action (script tab5_action, tab5-scripts.yaml).
// =============================================================================
struct EmplacementCible {
    const char* cle;
    esphome::text_sensor::TextSensor* texte;  // état HA tel quel (on, off, home…), ou nullptr
    esphome::sensor::Sensor* valeur;          // nombre affiché, NaN pour « nan » ou illisible, ou nullptr
};
// Applique la chaîne aux capteurs de la table ; une clé inconnue est ignorée. Les clés
// de tuile « tRT » (ADR-0023) vont d'abord aux pièces (tab5_tuiles.cpp).
// Renvoie le nombre d'entrées appliquées.
int emplacements_appliquer(const std::string& payload, const EmplacementCible* cibles, size_t n);

// =============================================================================
// Pièces et tuiles génériques (tab5_tuiles.cpp, ADR-0023) : chaque page du bas est une
// pièce de cinq appareils au plus, décrits par Home Assistant. Pièce R ↔ page : R0 = 2
// (accueil), R1 = 3, R2 = 4, R3 = 1, R4 = 0 ; tuile T = position visuelle (0 = gauche).
// Tant qu'aucune définition n'est arrivée (drapeau en NVS), la pièce 0 est construite
// depuis les emplacements 3.x (mode héritage).
// =============================================================================
// Action tab5_maj_tuiles : instantané complet « pR|nom;tRT|type|icône|options|complément|
// nom;… » (ce qui n'est pas listé est vide). Gardé en NVS s'il change. Vrai si changé.
bool tuiles_definir(const std::string& payload);

// Widgets des pièces, posés par le script tab5_tuiles_ui (tab5-tuiles.yaml), que
// tab5_zones_apply lance à la fin du setup, avant la première image : id() n'existe que
// dans les lambdas YAML, et l'on_boot n'est pas à nous (tab5-ha-hmi.yaml).
struct TuilesUI {
    // Navigation : calques du bas, pastilles, titre de la carte centrale, bouton « HA ».
    lv_obj_t* calque_jours = nullptr;     // layer_forecast_daily
    lv_obj_t* calque_heures = nullptr;    // layer_forecast_hourly
    lv_obj_t* calque_ha = nullptr;        // layer_switches
    lv_obj_t* pastilles[5] = {};          // pbar_0 … pbar_4
    lv_obj_t* titre_cadre = nullptr;      // page_title_wrapper
    lv_obj_t* titre = nullptr;            // lbl_page_title
    lv_obj_t* bouton_ha = nullptr;        // btn_control_ha
    lv_obj_t* icone_ha = nullptr;         // icon_ha
    esphome::font::Font* police_meteo = nullptr;         // font_meteo_card
    esphome::font::Font* police_meteo_petite = nullptr;  // font_meteo_card_small
    // Mode météo : épaules (icône à gauche, ampoule ou flèche à droite) et bouton
    // invisible de chaque tuile, par position visuelle T (0 = gauche). Journalières :
    // mêmes objets sur les pages 2 à 4 ; horaires : l'objet h(4−T).
    lv_obj_t* jour_g[5] = {};
    lv_obj_t* jour_d[5] = {};
    lv_obj_t* jour_bouton[5] = {};
    lv_obj_t* jour_sens = nullptr;        // btn_j1_dir : sens du volet 3.x (mode héritage)
    lv_obj_t* heure_g[5] = {};
    lv_obj_t* heure_d[5] = {};
    lv_obj_t* heure_bouton[5] = {};
    // Libellés des onglets de titre (leur parent devient cliquable : sens d'un volet).
    lv_obj_t* jour_titre[5] = {};          // j{T}_day
    lv_obj_t* heure_titre[5] = {};         // h(4−T)_time
    // Cartes du mode HA (switches_card.yaml) : carte T = tuile T de la pièce courante.
    lv_obj_t* carte[5] = {};
    lv_obj_t* carte_icone[5] = {};
    lv_obj_t* carte_nom[5] = {};
    lv_obj_t* carte_etat[5] = {};
    // Popups qu'une tuile ouvre (télécommande de la TV, climatisation : celle du blueprint
    // avec l'option m, sinon celle de la tuile, ADR-0027).
    lv_obj_t* popup_tv = nullptr;
    lv_obj_t* popup_clim = nullptr;
    // Popup lumière (light_popup.yaml) : sélecteur des lumières de la pièce (5 au plus).
    lv_obj_t* lum_popup = nullptr;        // light_options_popup
    lv_obj_t* lum_titre = nullptr;        // popup_light_title
    lv_obj_t* lum_sel[5] = {};            // btn_light_sel_N
    lv_obj_t* lum_sel_icone[5] = {};      // icon_light_sel_N
    lv_obj_t* lum_sel_nom[5] = {};        // lbl_light_sel_N
    lv_obj_t* lum_power = nullptr;        // btn_light_power_icon
    lv_obj_t* lum_arc = nullptr;          // arc_light_brightness
    lv_obj_t* lum_pct = nullptr;          // lbl_light_brightness_val
    std::string* lum_cle = nullptr;       // &id(current_light_slot) : cible des commandes
    // Volet 3.x (mode héritage) : sens de la prochaine commande.
    bool* volet_sens = nullptr;           // &id(volet_target_open)
    // Commandes, posées par le script (lambdas sans capture) : événement
    // esphome.tab5_action (script tab5_action) et tap du volet 3.x (tab5_volet_tap).
    void (*envoyer)(const char* emplacement, const char* action, const char* valeur) = nullptr;
    void (*volet_tap)() = nullptr;
    // Option e (ADR-0028) : ouvre le popup Énergie (script tab5_energie_ouvrir).
    void (*energie_ouvrir)() = nullptr;
};
extern TuilesUI g_tuiles_ui;

// =============================================================================
// Énergie (ADR-0028, discussion #278) — tab5_energie.cpp
// =============================================================================
// Popup « Énergie » (energie_popup.yaml) : l'instantané d'une installation solaire
// (solaire, maison, réseau, batterie) et l'historique de la production en barres
// (heures du jour, 30 derniers jours, 12 derniers mois). Ouvert par une tuile dont
// l'entité est un capteur de la section « Énergie » du blueprint (option e), ou par
// « Aller à l'écran → Énergie ». Home Assistant répond à l'événement esphome.tab5_energie
// (vue demandée) par les actions tab5_maj_energie et tab5_maj_energie_historique.
//
// Widgets posés par le script tab5_energie_ouvrir (tab5-energie.yaml) à la première
// ouverture : id() n'existe que dans une lambda YAML. Cartes : 0 solaire, 1 maison,
// 2 réseau, 3 batterie (energie_carte.yaml) ; vues : 0 heures, 1 jours, 2 mois.
struct EnergieUI {
    lv_obj_t* popup = nullptr;            // energie_popup
    lv_obj_t* carte[4] = {};              // energie_carte_N
    lv_obj_t* nom[4] = {};                // energie_nom_N
    lv_obj_t* icone[4] = {};            // energie_icone_N (mdi_font_45)
    lv_obj_t* valeur[4] = {};             // energie_valeur_N (roboto_45_b)
    lv_obj_t* ligne1[4] = {};             // energie_ligne1_N
    lv_obj_t* ligne2[4] = {};             // energie_ligne2_N
    lv_obj_t* graphique = nullptr;        // energie_graphique : carte du bas
    lv_obj_t* titre = nullptr;            // energie_titre : « Aujourd'hui · 12.4 kWh »
    lv_obj_t* vue_btn[3] = {};            // energie_vue_N
    lv_obj_t* zone = nullptr;             // energie_zone : barres, construites en C++
    lv_obj_t* attente = nullptr;          // energie_attente : avant le premier instantané
    const esphome::font::Font* police = nullptr;   // roboto_22 : libellés de l'axe
    // Événement esphome.tab5_energie (script tab5_energie_demande, lambda sans capture).
    void (*demander)(const char* vue) = nullptr;
};
extern EnergieUI g_energie_ui;

// Action tab5_maj_energie : « solaire|maison|reseau|batterie|batterie_puissance|
// batterie_temperature|unite_temperature|jour ». Champ vide = capteur non choisi (sa
// carte ou sa ligne disparaît), « nan » = choisi sans valeur (« -- »). Puissances en W
// (réseau : + achat, − vente ; batterie : + charge, − décharge), jour en kWh.
void energie_instantane(const std::string& payload);
// Action tab5_maj_energie_historique : vue (heures | jours | mois), début (AAAA-MM-JJ :
// le jour des heures, le premier des jours, le 1er du premier mois), valeurs en kWh
// séparées par « ; » (vide = pas de donnée). 24, 30 et 12 valeurs au plus.
void energie_historique(const std::string& vue, const std::string& debut, const std::string& valeurs);
// Ouvre le popup (vue des heures), le peint, et demande ses données à HA.
void energie_ouvrir();
// Boutons Heures / Jours / Mois : change la vue et la demande à HA.
void energie_choisir_vue(int vue);

// Appui sur la tuile T de la pièce de la page courante (tuile météo ou carte du mode
// HA) : commande selon le type et les options (tableau de l'ADR-0023), popup, ou rien.
void tuile_appui(int tuile, bool long_appui);

// Toucher du titre de la tuile T : bascule le sens d'un volet (flèche, puis appui).
void tuile_titre_appui(int tuile);
// Rend cliquables les onglets de titre (une fois, depuis tab5_tuiles_ui).
void tuiles_brancher_titres();

// Mode HA (bouton « HA », « Aller à l'écran → Accueil ») : cartes de la pièce de la page
// courante, ou de la plus proche qui a des appareils ; titre de la pièce dans la carte
// centrale ; en sortant, la météo de la page courante.
void tuiles_mode_ha(bool actif);

// Mode héritage (blueprint 3.x) : les emplacements 3.x forment la pièce 0 — PC/TV
// (tuile 0), volet (1), lumiere_1..3 (2-4). Appelés par leurs capteurs
// (tab5-sensors-domotique.yaml) et par tab5_maj_volet_etat, quel que soit le mode.
void tuiles_heritage_pc(bool actif);
void tuiles_heritage_tv(bool actif);
void tuiles_heritage_lumiere(int i, bool allumee);
// Luminosité 0-255 (NaN éteinte) de lumiere_1..3 : l'arc du popup s'il la montre.
void tuiles_heritage_luminosite(int i, float luminosite);
// Renvoie vrai si le volet est en mouvement (volet_en_mouvement).
bool tuiles_heritage_volet(const std::string& etat_physique);
// Bouton btn_j1_dir (haut de la tuile du volet, mode héritage) : inverse le sens de la
// prochaine commande (volet_target_open) et repeint la flèche.
void tuiles_heritage_volet_sens();

// Popup lumière (ouvert par l'appui long d'une tuile lum) : ses lignes sont les lumières
// de la pièce, dans l'ordre des tuiles. Choisit la ligne `idx` (script
// tab5_light_popup_show, boutons du sélecteur) : titre, surbrillance, arc, et
// current_light_slot = clé de la tuile (tRT, ou lumiere_N en mode héritage).
void popup_lumiere_choisir(int idx);
// « Tout éteindre » : pR / eteindre (toutes les lumières de la pièce), lumieres /
// eteindre en mode héritage.
void popup_lumiere_tout_eteindre();

// UIColor (couleurs sémantiques) : voir tab5_tokens.h.
