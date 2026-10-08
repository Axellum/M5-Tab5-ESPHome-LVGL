/**
 * [AI-CONTEXT]
 * @file tab5_central.h
 * @role Carte centrale (tab5_central.cpp) : contexte, glissement des prévisions, rotateur,
 *       bandeaux d'alertes HA, planning du tap, réponse vocale.
 * @architecture_constraint Sorti de tab5_custom.h le 08/10/2026, lignes recopiées telles
 *       quelles : tab5_custom.h l'inclut, les lambdas YAML et les unités `tab5_*.cpp` n'ont
 *       rien à changer. Une fonction déclarée ici a un appelant hors de son fichier (règle 12
 *       de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes).
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; un module nouveau = un
 *       en-tête de plus, inclus par tab5_custom.h et listé sous `includes:` des deux
 *       configurations racine (tab5-ha-hmi.yaml, tab5-rendu-host.yaml).
 */
#pragma once
#include "esphome.h"
#include "tab5_forecast.h"
#include <string>

namespace esphome { namespace font { class Font; } }

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
    // Ligne "chapeau" du titre de page en mode pieces de HA (lbl_page_title_sub) : logee ici
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
// uniquement par appui long sur btn_control_console (plus de swipe haut/bas). En mode HA (ADR-0023) :
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
// Renvoie true quand une vigilance rouge nouvelle vient de prendre la carte (même règle
// que les bandeaux d'alertes HA).
bool update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, const std::string& meteo_id,
    std::string& dismissed_local, CentralPanelCtx& ctx,
    esphome::font::Font* font_small);

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

// Un bandeau HA : ses widgets (texte, compteur « 2/6 ») et son id d'acquittement. Sa
// présence est ctx.has_ha[slot], posée par parse_and_update_ha_alerts_bulk.
struct HaAlertSlotUI {
    lv_obj_t* wrap;
    lv_obj_t* lbl;
    std::string* id_store;
    lv_obj_t* cpt;
};

// Les 4 bandeaux (ha_alert_panel.yaml, n = 0-3), seule table : remplie par le script
// tab5_ha_alert_slots_init (tab5-alertes.yaml), que chaque lecteur appelle avant de
// lire. Pointeurs nuls tant qu'il n'a pas tourné.
extern HaAlertSlotUI g_ha_alert_slots[kHaAlertSlotCount];

// La police des bandeaux est celle du YAML (ha_alert_panel.yaml, police de la date) :
// la reposer à chaque push relançait la mise en page pour rien (audit 26/09, lot 3).
// Renvoie true quand une alerte rouge nouvelle vient de prendre la carte : l'appelant
// relance le minuteur du rotateur pour qu'elle reste un tour entier.
bool parse_and_update_ha_alerts_bulk(const std::string& payload, HaAlertSlotUI slots[4],
    CentralPanelCtx& ctx, std::string& dismissed_local);

// Masquage immédiat au tap (feedback visuel avant le round-trip HA).
void dismiss_central_info_immediate(lv_obj_t* lbl_info, CentralPanelCtx& ctx);
void dismiss_ha_alert_slot_immediate(int slot_idx, lv_obj_t* wrap, lv_obj_t* lbl,
    std::string& id_store, CentralPanelCtx& ctx);

// Panneaux pluie (1) et vigilance (2) du rotateur : pose has_rain / has_mf_alerts.
// Fin de la pluie ou de la vigilance → le panneau affiché cède la place tout de
// suite ; début alors que la carte est vide → il s'affiche sans attendre le tour.
void central_set_pluie(bool actif);
void central_set_vigilance(bool actif);

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
    lv_obj_t* page_title_wrap, CentralPanelCtx& ctx);

void hide_vocal_response_ui(lv_obj_t* vocal_wrap, lv_obj_t* lbl_vocal, CentralPanelCtx& ctx);

// Carte centrale : le planning n'est plus un panneau du rotateur quand HA n'a
// pas d'agenda de travail (zone PLANNING).
void central_planning_set_off(bool off);

// « Tout marquer comme lu » sur la carte centrale : les 4 bandeaux d'alertes HA et la
// vigilance du bandeau info sont lus tout de suite (acquittements locaux), comme un tap
// sur chacun ; HA reçoit alert_id « * » et lit aussi celles qui n'avaient pas de bandeau.
void central_tout_marquer_lu(HaAlertSlotUI slots[4], lv_obj_t* lbl_info, const std::string& info_id,
                             std::string& dismissed_local, CentralPanelCtx& ctx);
