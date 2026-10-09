/**
 * [AI-CONTEXT]
 * @file alarm_render.h
 * @role Rendu LVGL du réveil (structures de widgets + fonctions de peinture).
 *       Les pointeurs sont injectés par les scripts YAML, seuls capables de faire
 *       `id(...)`. Séparé d'alarm_clock.h le 25/09/2026 (lot 8b) : le moteur n'a
 *       plus besoin d'ESPHome ni de LVGL.
 *       Popup de réglage en cinq pages depuis le 09/10/2026 (demande d'Axel) : Heure,
 *       Jours, Ouverture, Sonnerie, Annonces ; les valeurs se règlent par des rouleaux
 *       (lv_roller) et des boutons à choix, comme les Réglages (tab5_pages.cpp).
 */
#pragma once
#include "esphome.h"
#include "alarm_clock.h"

#include <string>

// ═══════════════════════════════════════════════════════════════════════════
// Popup de réglage (alarm_popup.yaml) : pages et rouleaux
// ═══════════════════════════════════════════════════════════════════════════
// Pages, dans l'ordre de leurs noms en haut (alarm_popup.yaml, `page:` des onglets).
enum ReveilPage : uint8_t {
    REVEIL_PAGE_HEURE = 0,     // heure fixe (rouleaux), marche / arrêt, prochaine sonnerie, test
    REVEIL_PAGE_JOURS,         // mode, jours de l'heure fixe, préréglages, jours de repos
    REVEIL_PAGE_OUVERTURE,     // délai, pas avant, pas après, repos mini (mode « Ouverture »)
    REVEIL_PAGE_SONNERIE,      // mélodie, volume, progressif, répétition, durée max
    REVEIL_PAGE_ANNONCES,      // annonce parlée, annonce des rendez-vous, prochain rendez-vous
    REVEIL_NB_PAGES
};

// Rouleaux, `quoi:` de rouleau.yaml et du script tab5_alarm_rouleau (tab5-alarm.yaml).
// Bornes et pas : kRouleaux (alarm_render.cpp), égaux à ceux des entités
// (tests/test_alarme_popup.py).
enum ReveilRouleau : uint8_t {
    REVEIL_ROULEAU_HEURE = 0,  // heure de l'heure fixe (00-23)
    REVEIL_ROULEAU_MINUTES,    // minutes de l'heure fixe (de 5 en 5)
    REVEIL_ROULEAU_PAS_AVANT,  // « jamais avant » (de 15 en 15 min)
    REVEIL_ROULEAU_PAS_APRES,  // « jamais après »
    REVEIL_ROULEAU_DELAI,      // délai avant l'ouverture (0-240 min, de 5 en 5)
    REVEIL_ROULEAU_REPOS,      // repos mini après la fermeture (0-14 h)
    REVEIL_ROULEAU_REPETITION, // répétition (1-30 min)
    REVEIL_ROULEAU_DUREE_MAX,  // durée max de sonnerie (1-60 min)
    REVEIL_ROULEAU_RDV_AVANT,  // annonce des rendez-vous, minutes avant (0-120, de 5 en 5)
    REVEIL_NB_ROULEAUX
};

constexpr int REVEIL_NB_PREREGLAGES = 4;  // préréglages de jours montrés (sans « Personnalisé »)

struct ReveilUI {
    lv_obj_t* popup = nullptr;                          // alarm_popup
    lv_obj_t* page[REVEIL_NB_PAGES] = {};               // alarm_page_heure… alarm_page_annonces
    lv_obj_t* onglet[REVEIL_NB_PAGES] = {};             // alarm_onglet_heure… alarm_onglet_annonces
    lv_obj_t* rouleau[REVEIL_NB_ROULEAUX] = {};         // roller_alarm_*
  // Page Heure
    lv_obj_t* btn_enable = nullptr;                     // grande bascule « Réveil »
    lv_obj_t* icon_enable = nullptr;
    lv_obj_t* lbl_enable = nullptr;
    lv_obj_t* lbl_next = nullptr;                       // « Demain 05:15 »
    lv_obj_t* lbl_next_sub = nullptr;                   // « mercredi 6 août · Travail 06:45 – 15:30 »
  // Page Jours
    lv_obj_t* mode[AlarmMode::COUNT] = {};
    lv_obj_t* lbl_mode_hint = nullptr;                  // phrase qui explique le mode retenu
    lv_obj_t* day_btn[7] = {};                          // pastilles lundi … dimanche
    lv_obj_t* day_lbl[7] = {};
    lv_obj_t* prereglage[REVEIL_NB_PREREGLAGES] = {};   // Tous les jours, Lun-Ven, Lun-Sam, Week-end
    lv_obj_t* repos[2] = {};                            // jours de repos : 0 = Silence, 1 = Heure fixe
    lv_obj_t* lbl_repos_hint = nullptr;
    // Page Ouverture
    lv_obj_t* lbl_ouverture_hint = nullptr;             // résumé du mode, ou « ne sert qu'au mode Ouverture »
  // Page Sonnerie
    lv_obj_t* melodie[ALARM_MELODY_COUNT] = {};
    lv_obj_t* slider_vol = nullptr;
    lv_obj_t* lbl_vol = nullptr;
    lv_obj_t* progressif[2] = {};                       // 0 = Oui, 1 = Non
  // Page Annonces
    lv_obj_t* tts[2] = {};                              // annonce parlée : 0 = Oui, 1 = Non
    lv_obj_t* rdv[2] = {};                              // annonce des rendez-vous : 0 = Oui, 1 = Non
    lv_obj_t* lbl_rdv_next = nullptr;                   // prochain rendez-vous, sur plusieurs lignes
};
extern ReveilUI g_reveil_ui;

// Une fois, à la première ouverture (tab5_alarm_open), pointeurs posés : noms des jours,
// options des rouleaux, geste de page branché (tab5_pages.cpp).
void reveil_preparer();
// Montre une page (onglets, geste ; hors bornes : la page Heure).
void reveil_afficher_page(int page);
// Valeur choisie sur un rouleau (index `index` de son on_value) : heure, minutes, minutes
// depuis minuit, minutes ou heures selon le rouleau ; −1 si l'index ne correspond à rien.
int reveil_rouleau_valeur(int quoi, int index);

// Repeint TOUT le popup depuis `g_alarm_cfg` + les 3 interrupteurs qui n'y ont
// pas de miroir (le C++ ne peut pas lire les entités ESPHome lui-même). Sans effet tant
// que le popup n'a jamais été ouvert (g_reveil_ui.popup nul).
void alarm_render_settings(time_t now, bool crescendo, bool tts_on, bool rdv_on);

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
