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
    // Pointers for action widgets
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

// Animation d'ouverture d'un popup : fondu card + scrim.
// Le scrim doit être visible (clear flag) AVANT l'appel.
// Instantanée : pas de fondu (réactivité maximale).
void animate_popup_open(lv_obj_t* card, lv_obj_t* scrim);

// Animation de fermeture : fondu inverse. Cache automatiquement card + scrim
// à la fin de l'animation (LV_OBJ_FLAG_HIDDEN).
// Instantanée elle aussi.
void animate_popup_close(lv_obj_t* card, lv_obj_t* scrim);

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
    // Qui occupe la carte (audit du 25/09/2026, §2.4) : le rotateur n'a la main que
    // sur l'accueil (page 2), hors planning temporaire et hors réponse vocale. Tenus à
    // jour côté C++ (apply_forecast_page, show/hide_vocal_response_ui) plutôt que par
    // des pointeurs vers les globals ESPHome, pour ne pas toucher à on_boot.
    // forecast_page démarre à 2 comme le global forecast_page_index (non restauré).
    int forecast_page = 2;
    bool vocal_shown = false;
    lv_obj_t* vocal_wrap = nullptr;   // posé par show_vocal_response_ui
};

// Contexte global unique (initialise dans tab5-ha-hmi.yaml on_boot ou premier usage).
extern CentralPanelCtx g_central_ctx;

// Gestion du geste de swipe (page_main.on_gesture) : pagination previsions
// horaires/journalieres (0-4) dans la bande centrale+basse (y >= 333). Console diag :
// uniquement via btn_control_console (plus de swipe haut/bas).
void handle_swipe_gesture(lv_dir_t dir, lv_coord_t pt_y, int& forecast_page_index,
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
void reset_forecast_to_main_page(int& forecast_page_index,
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
void update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, const std::string& meteo_id,
    std::string& dismissed_local, bool& has_info, int& current_panel,
    esphome::font::Font* font_small, esphome::font::Font* font_large);

// Rotateur carte centrale : 0 planning, 1 pluie, 2 vigilance MF, 3 info (phrase test),
// 4-7 alertes HA individuelles (8s, même timer global).
constexpr int kCentralPanelCount = 8;
constexpr int kHaAlertPanelBase = 4;
constexpr int kHaAlertSlotCount = 4;

// Synchronise g_central_ctx depuis les globals YAML (factorise le bloc 8 lignes
// répété 7× dans tab5-scripts.yaml). Appelée avant chaque advance/dismiss/show.
void sync_central_ctx(CentralPanelCtx& ctx, bool rain, bool alerts, bool info,
                      bool ha0, bool ha1, bool ha2, bool ha3, int panel);

void advance_central_panel_rotator(CentralPanelCtx& ctx);

struct HaAlertSlotUI {
    lv_obj_t* wrap;
    lv_obj_t* lbl;
    bool* has_flag;
    std::string* id_store;
};

// La police des bandeaux est celle du YAML (ha_alert_panel.yaml, roboto_45_b) :
// la reposer à chaque push relançait la mise en page pour rien (audit 26/09, lot 3).
void parse_and_update_ha_alerts_bulk(const std::string& payload, HaAlertSlotUI slots[4],
    CentralPanelCtx& ctx, std::string& dismissed_local);

// Masquage immédiat au tap (feedback visuel avant le round-trip HA).
void dismiss_central_info_immediate(lv_obj_t* lbl_info, CentralPanelCtx& ctx);
void dismiss_ha_alert_slot_immediate(int slot_idx, lv_obj_t* wrap, lv_obj_t* lbl,
    bool& has_flag, std::string& id_store, CentralPanelCtx& ctx);

void tab5_dismiss_local_add(std::string& store, const std::string& id);

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
// icons_utf8[5] = codes MDI pour chaque capteur, slots[4] = widgets LVGL de destination
void sort_and_update_moisture_slots(float values[5], const char* icons_utf8[5],
    MoistureSlotUI slots[4]);

// Couleur batterie par niveau (échelle icône téléphone du bandeau, réutilisée
// par la ligne Batterie du popup détails pots).
uint32_t get_battery_color(float x);

// Icônes d'état du bandeau et des cartes (règle 2 : les sensors n'appellent pas
// LVGL). set_icon_color_ui pose une couleur calculée (batterie, humidité) ;
// set_icon_active_ui choisit entre deux tokens UIColor selon un booléen (API HA,
// Wi-Fi, TV). Tolèrent un widget nullptr (valeur reçue avant le layout).
void set_icon_color_ui(lv_obj_t* icon, uint32_t color);
void set_icon_active_ui(lv_obj_t* icon, bool active, uint32_t color_on, uint32_t color_off);

// Carte PC (text_sensor pc_status) : icône du bandeau + interrupteur 0 de la carte
// switches (icône et libellé « Allumé » / « Éteint »). Ne fait rien sans icon_pc.
void update_pc_status_ui(bool active, lv_obj_t* icon_pc, lv_obj_t* icon_sw, lv_obj_t* lbl_sw_state);

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
// `current_panel_global` = le global ESPHome current_central_panel : le timer de
// restauration doit l'écrire aussi, sinon le prochain script le recopie (0, posé
// par le tap) dans ctx.current_panel et le panneau d'origine est perdu.
void show_temporary_planning(int jour, lv_obj_t* lbl_planning,
                             lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, int forecast_page,
                             const std::string& plan_l1, const std::string& plan_l2,
                             bool& is_showing_temp, int& current_panel_global, CentralPanelCtx& ctx);

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

// UIColor (couleurs sémantiques) : voir tab5_tokens.h.
