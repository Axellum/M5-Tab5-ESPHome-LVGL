/**
 * [AI-CONTEXT]
 * @file tab5_console.h
 * @role Console système (tab5_console.cpp) : statut, volume, diagnostic, CPU, batterie,
 *       version du C6.
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

// Garde #T222 : la console (page « Système » des Réglages depuis le 08/10/2026) ne touche
// LVGL que si elle est affichée : reglages_page_visible(REGLAGES_PAGE_SYSTEME).

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

// Repose l'etat muet/non-muet sur l'icone qui le represente : celle du popup
// assistant (`icon_assist_mute`), seule depuis le retrait du bouton Muet de
// l'accueil (05/10/2026). Chaque endroit qui change `system_muted` passe par ici,
// sinon l'icone ment jusqu'a la prochaine ouverture du popup.
void ui_sync_mute_icon(lv_obj_t* icon_assist, bool muted);

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

// Console système, carte SYSTÈME (discussion #278, 06/10/2026). Appelées par l'interval
// 2 s de tab5-sensors-diagnostics.yaml quand la console est visible, et à son ouverture
// (appui long sur l'engrenage, tab5-lvgl.yaml).
// « Charge CPU » : charge de chaque cœur (« 4% · 37% », cœur 0 puis cœur 1) depuis
// l'appel précédent, d'après le temps de leur tâche inactive (tab5_console.cpp).
// `ouverture` (ou un appel précédent de plus de 5 s) : point de départ seulement, « -- ».
// « -- » aussi sans les statistiques de FreeRTOS (rendu hors tablette).
void update_console_cpu_ui(lv_obj_t* lbl, bool ouverture);
// « Batterie » : niveau et tension (sur batterie : niveau et consommation), « Sur USB »
// sans batterie détectée, « Non montée » interrupteur « Tab5 Batterie montée » éteint
// (batterie_texte_console, tab5_core.h) ;
// l'icône du bandeau à gauche de la valeur, masquée interrupteur éteint (tab5_zones.cpp).
void update_console_batterie_ui(lv_obj_t* icone, lv_obj_t* valeur);

// Version du logiciel ESP-Hosted du co-processeur Wi-Fi (ESP32-C6), « x.y.z », lue par
// RPC sur le lien SDIO (réponse attendue au plus 1 s). false si le lien ne répond pas.
// Capteur « Tab5 C6 Version » de tab5-sensors-diagnostics.yaml.
bool read_c6_firmware_version(char* out, size_t n);
