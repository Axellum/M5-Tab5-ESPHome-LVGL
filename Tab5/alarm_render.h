/**
 * [AI-CONTEXT]
 * @file alarm_render.h
 * @role Rendu LVGL du réveil (structures de widgets + fonctions de peinture).
 *       Les pointeurs sont injectés par les scripts YAML, seuls capables de faire
 *       `id(...)`. Séparé d'alarm_clock.h le 25/09/2026 (lot 8b) : le moteur n'a
 *       plus besoin d'ESPHome ni de LVGL.
 */
#pragma once
#include "esphome.h"
#include "alarm_clock.h"

#include <string>

// ═══════════════════════════════════════════════════════════════════════════
// Rendu LVGL — les pointeurs sont injectés par les scripts YAML (seuls capables
// de faire `id(...)`), comme CalCellUI / HaAlertSlotUI.
// ═══════════════════════════════════════════════════════════════════════════
struct AlarmSettingsUI {
  lv_obj_t* btn_enable;      // grande bascule « Réveil »
  lv_obj_t* icon_enable;
  lv_obj_t* lbl_enable;
  lv_obj_t* lbl_time;        // « 05:15 » (roboto_55_b)
  lv_obj_t* day_btn[7];      // chips L M M J V S D
  lv_obj_t* day_lbl[7];
  lv_obj_t* mode_btn[AlarmMode::COUNT];
  lv_obj_t* lbl_mode_hint;   // phrase qui explique le mode retenu
  lv_obj_t* lbl_lead;        // « 90 min »
  lv_obj_t* lbl_early;       // « 05:00 »
  lv_obj_t* lbl_late;        // « 09:00 »
  lv_obj_t* lbl_rest;        // « 11 h » / « — »
  lv_obj_t* btn_repos;       // bascule « jour de repos : sonner quand même »
  lv_obj_t* lbl_repos;
  lv_obj_t* lbl_next;        // « Demain 05:15 »
  lv_obj_t* lbl_next_sub;    // « mercredi 6 août · Travail 06:45 – 15:30 »
  lv_obj_t* lbl_melody;
  lv_obj_t* lbl_vol;
  lv_obj_t* slider_vol;
  lv_obj_t* btn_cresc;
  lv_obj_t* lbl_cresc;
  lv_obj_t* lbl_snooze;
  lv_obj_t* lbl_maxring;
  lv_obj_t* btn_tts;
  lv_obj_t* lbl_tts;
  lv_obj_t* btn_rdv;
  lv_obj_t* lbl_rdv;
  lv_obj_t* lbl_rdv_lead;
  lv_obj_t* lbl_rdv_next;
};

// Repeint TOUT le popup depuis `g_alarm_cfg` + les 3 interrupteurs qui n'y ont
// pas de miroir (le C++ ne peut pas lire les entités ESPHome lui-même).
void alarm_render_settings(const AlarmSettingsUI& ui, time_t now, bool crescendo, bool tts_on,
                           bool rdv_on);

struct AlarmRingUI {
  lv_obj_t* root;       // calque plein écran
  lv_obj_t* icon;       // cloche
  lv_obj_t* lbl_time;   // heure courante en très gros
  lv_obj_t* lbl_title;  // « Réveil » / « Répétition 2 »
  lv_obj_t* lbl_sub;    // « mercredi 6 août · Travail 06:45 – 15:30 »
  lv_obj_t* lbl_snooze; // libellé du bouton répéter (« Répéter · 9 min »)
};
void alarm_ring_show(const AlarmRingUI& ui, time_t now, const std::string& sub);
void alarm_ring_refresh(const AlarmRingUI& ui, time_t now, int snooze_min, int snooze_count);
void alarm_ring_hide(const AlarmRingUI& ui);

// Icône + couleur de la pastille réveil de la barre d'état (tab5-lvgl.yaml).
void alarm_render_status_icon(lv_obj_t* icon, time_t now);
