/**
 * [AI-CONTEXT]
 * @file alarm_render.cpp
 * @role Rendu LVGL du réveil : popup de réglages, calque de sonnerie, pastille de
 *       la barre d'état. Sorti d'alarm_clock.cpp le 25/09/2026 (audit, lot 8b) pour
 *       que le moteur reste compilable et testable sur PC. Ne lit l'état du moteur
 *       que par son API publique (alarm_clock.h).
 */
#include "alarm_render.h"
#include "tab5_custom.h"

#include <cstdio>

// ═══════════════════════════════════════════════════════════════════════════
// Rendu LVGL
// ═══════════════════════════════════════════════════════════════════════════

// Bascule visuelle commune aux 5 interrupteurs du popup : bordure colorée
// (highlight_button_border, déjà utilisé par l'assistant vocal) + libellé
// ON/OFF assorti. Une seule recette, donc aucun interrupteur ne peut prendre
// une apparence différente des autres.
static void set_toggle(lv_obj_t* btn, lv_obj_t* lbl, bool on, const char* on_txt, const char* off_txt,
                       uint32_t on_color) {
  if (btn != nullptr) highlight_button_border(btn, on, on_color);
  if (lbl != nullptr) {
    lv_label_set_text(lbl, on ? on_txt : off_txt);
    lv_obj_set_style_text_color(lbl, lv_color_hex(on ? on_color : UIColor::TEXT_DIM), LV_PART_MAIN);
  }
}

void alarm_render_settings(const AlarmSettingsUI& ui, time_t now, int melody_idx, float volume,
                           bool crescendo, bool tts_on, bool rdv_on, int rdv_lead_min) {
  const AlarmCfg& c = g_alarm_cfg;
  char buf[96];

  set_toggle(ui.btn_enable, ui.lbl_enable, c.enabled, "R\xC3\xA9veil actif", "R\xC3\xA9veil \xC3\xA9teint",
             UIColor::SUCCESS);
  if (ui.icon_enable != nullptr) {
    // F0020 = alarm, F0023 = alarm-off (codepoints vérifiés dans le TTF du projet).
    lv_label_set_text(ui.icon_enable, c.enabled ? "\U000F0020" : "\U000F0023");
    lv_obj_set_style_text_color(ui.icon_enable,
                                lv_color_hex(c.enabled ? UIColor::SUCCESS : UIColor::TEXT_DIM), LV_PART_MAIN);
  }

  if (ui.lbl_time != nullptr) {
    alarm_hhmm(c.fixed_min, buf, sizeof(buf));
    lv_label_set_text(ui.lbl_time, buf);
  }

  // Chips des jours. Le sélecteur ne gouverne QUE l'heure fixe : on l'estompe
  // quand le mode en cours ne s'en sert pas pour les jours travaillés, plutôt
  // que de le masquer (il reste utile pour le repli des jours de repos).
  static const char* const kDays[7] = {"L", "M", "M", "J", "V", "S", "D"};
  const bool days_govern_all = (c.mode == AlarmMode::FIXE);
  for (int i = 0; i < 7; i++) {
    const bool on = (c.days_mask >> i) & 1;
    if (ui.day_btn[i] != nullptr) highlight_button_border(ui.day_btn[i], on, UIColor::ACCENT);
    if (ui.day_lbl[i] != nullptr) {
      lv_label_set_text(ui.day_lbl[i], kDays[i]);
      lv_obj_set_style_text_color(ui.day_lbl[i],
                                  lv_color_hex(on ? (days_govern_all ? UIColor::TEXT_SOFT : UIColor::ACCENT)
                                                  : UIColor::TEXT_DIM),
                                  LV_PART_MAIN);
    }
  }

  for (int i = 0; i < AlarmMode::COUNT; i++) {
    if (ui.mode_btn[i] != nullptr) highlight_button_border(ui.mode_btn[i], c.mode == i, UIColor::INFO);
  }
  if (ui.lbl_mode_hint != nullptr) {
    const char* hint = "";
    switch (c.mode) {
      case AlarmMode::FIXE:
        hint = "Sonne \xC3\xA0 l'heure fixe, les jours coch\xC3\xA9s ci-dessus.";
        break;
      case AlarmMode::TRAVAIL:
        hint = "Sonne \xC3\xA0 l'heure fixe, uniquement les jours travaill\xC3\xA9s.";
        break;
      default:
        hint = "Sonne avant l'ouverture lue dans le calendrier.";
        break;
    }
    lv_label_set_text(ui.lbl_mode_hint, hint);
  }

  if (ui.lbl_lead != nullptr) {
    snprintf(buf, sizeof(buf), "%d min", c.lead_min);
    lv_label_set_text(ui.lbl_lead, buf);
  }
  if (ui.lbl_early != nullptr) {
    alarm_hhmm(c.earliest_min, buf, sizeof(buf));
    lv_label_set_text(ui.lbl_early, buf);
  }
  if (ui.lbl_late != nullptr) {
    alarm_hhmm(c.latest_min, buf, sizeof(buf));
    lv_label_set_text(ui.lbl_late, buf);
  }
  if (ui.lbl_rest != nullptr) {
    if (c.rest_hours <= 0) {
      lv_label_set_text(ui.lbl_rest, "\xE2\x80\x94");
    } else {
      snprintf(buf, sizeof(buf), "%d h", c.rest_hours);
      lv_label_set_text(ui.lbl_rest, buf);
    }
  }

  set_toggle(ui.btn_repos, ui.lbl_repos, c.rest_mode == AlarmRepos::FIXE, "Repos : heure fixe",
             "Repos : silence", UIColor::WARNING);

  if (ui.lbl_next != nullptr) lv_label_set_text(ui.lbl_next, alarm_next_label(now).c_str());
  if (ui.lbl_next_sub != nullptr) lv_label_set_text(ui.lbl_next_sub, alarm_next_detail(now).c_str());

  if (ui.lbl_melody != nullptr) lv_label_set_text(ui.lbl_melody, alarm_melody_name(melody_idx));
  if (ui.slider_vol != nullptr) {
    const int pct = static_cast<int>(volume * 100.0f + 0.5f);
    // lv_slider_set_value ne déclenche pas LV_EVENT_VALUE_CHANGED : pas de
    // rebouclage sur on_value (même motif que ui_sync_volume_widgets).
    lv_slider_set_value(ui.slider_vol, pct, LV_ANIM_OFF);
    if (ui.lbl_vol != nullptr) {
      snprintf(buf, sizeof(buf), "%d %%", pct);
      lv_label_set_text(ui.lbl_vol, buf);
    }
  }
  set_toggle(ui.btn_cresc, ui.lbl_cresc, crescendo, "Progressif", "Volume constant", UIColor::ACCENT);

  if (ui.lbl_snooze != nullptr) {
    snprintf(buf, sizeof(buf), "%d min", c.snooze_min);
    lv_label_set_text(ui.lbl_snooze, buf);
  }
  if (ui.lbl_maxring != nullptr) {
    snprintf(buf, sizeof(buf), "%d min", c.max_ring_min);
    lv_label_set_text(ui.lbl_maxring, buf);
  }

  set_toggle(ui.btn_tts, ui.lbl_tts, tts_on, "Annonce parl\xC3\xA9""e", "Sonnerie seule", UIColor::INFO);
  set_toggle(ui.btn_rdv, ui.lbl_rdv, rdv_on, "Annonce des RDV", "RDV silencieux", UIColor::INFO);
  if (ui.lbl_rdv_lead != nullptr) {
    snprintf(buf, sizeof(buf), "%d min avant", rdv_lead_min);
    lv_label_set_text(ui.lbl_rdv_lead, buf);
  }
  if (ui.lbl_rdv_next != nullptr) {
    const std::string n = rdv_next_label(now);
    lv_label_set_text(ui.lbl_rdv_next, n.empty() ? "Aucun rendez-vous \xC3\xA0 venir" : n.c_str());
    lv_obj_set_style_text_color(ui.lbl_rdv_next,
                                lv_color_hex(n.empty() ? UIColor::TEXT_DIM : UIColor::TEXT_SOFT),
                                LV_PART_MAIN);
  }
}

static void ring_paint_clock(const AlarmRingUI& ui, time_t now) {
  if (ui.lbl_time == nullptr) return;
  struct tm t;
  if (localtime_r(&now, &t) == nullptr) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  lv_label_set_text(ui.lbl_time, buf);
}

void alarm_ring_show(const AlarmRingUI& ui, time_t now, const std::string& sub) {
  if (ui.root == nullptr) return;
  ring_paint_clock(ui, now);
  if (ui.lbl_sub != nullptr) lv_label_set_text(ui.lbl_sub, sub.c_str());
  if (ui.icon != nullptr) lv_label_set_text(ui.icon, "\U000F0020");
  alarm_ring_refresh(ui, now, g_alarm_cfg.snooze_min, alarm_snooze_count());
  lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.root);
}

void alarm_ring_refresh(const AlarmRingUI& ui, time_t now, int snooze_min, int snooze_count) {
  ring_paint_clock(ui, now);
  char buf[64];
  if (ui.lbl_title != nullptr) {
    if (snooze_count > 0) {
      snprintf(buf, sizeof(buf), "R\xC3\xA9p\xC3\xA9tition %d", snooze_count);
    } else {
      snprintf(buf, sizeof(buf), "R\xC3\xA9veil");
    }
    lv_label_set_text(ui.lbl_title, buf);
  }
  if (ui.lbl_snooze != nullptr) {
    snprintf(buf, sizeof(buf), "R\xC3\xA9p\xC3\xA9ter \xC2\xB7 %d min", snooze_min);
    lv_label_set_text(ui.lbl_snooze, buf);
  }
}

void alarm_ring_hide(const AlarmRingUI& ui) {
  if (ui.root != nullptr) lv_obj_add_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
}

void alarm_render_status_icon(lv_obj_t* icon, time_t now) {
  if (icon == nullptr) return;
  if (!g_alarm_cfg.enabled) {
    lv_label_set_text(icon, "\U000F0023");  // alarm-off
    lv_obj_set_style_text_color(icon, lv_color_hex(UIColor::INACTIVE), LV_PART_MAIN);
    return;
  }
  lv_label_set_text(icon, "\U000F0020");  // alarm
  // Vert quand la prochaine sonnerie est réellement calculée, ambre quand le
  // réveil est armé mais qu'aucun jour n'est retenu (piège classique : mode
  // « jours travaillés » + semaine de congés, ou tous les jours décochés).
  const bool armed = alarm_next_ring(now) != 0;
  lv_obj_set_style_text_color(icon, lv_color_hex(armed ? UIColor::SUCCESS : UIColor::WARNING), LV_PART_MAIN);
}
