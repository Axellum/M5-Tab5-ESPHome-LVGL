/**
 * [AI-CONTEXT]
 * @file tab5_economie.cpp
 * @role Mode économie d'énergie (06/10/2026) : l'état vivant de la tablette et les
 *       appels du YAML. Les règles sont dans tab5_economie.h (pures, testées sur PC) ;
 *       la décision est prise par le script tab5_economie_appliquer
 *       (Tab5/paquets/tab5-economie.yaml), lancé chaque seconde et à chaque événement utile.
 * @architecture_constraint Ni ESPHome ni LVGL ici : le YAML lit les entités, applique
 *       le PWM et la période de LVGL ; les animations sont coupées dans tab5_anim.cpp
 *       (animations_niveau).
 *       Le plafond est appliqué À LA SORTIE du rétroéclairage (output template
 *       `backlight_plafonne`, tab5-hardware.yaml), pas à l'état de la lumière : HA, le
 *       curseur des Réglages et le réveil gardent la luminosité choisie, et elle revient
 *       telle quelle quand le mode s'arrête (aucune luminosité « d'avant » à retenir).
 */
#include "tab5_economie.h"

#include <algorithm>

static EtatEconomie s_etat;
static float s_demande = 0.0f;          // dernier niveau écrit par la lumière (après gamma)
static float s_plafond = 1.0f;          // plafond de la sortie (après gamma)
static uint32_t s_periode_ms = kEcoPeriodeNormaleMs;

bool economie_courant(float courant_a, bool batterie_presente, bool en_charge) {
    return economie_courant_lu(s_etat, courant_a, batterie_presente, en_charge);
}

bool economie_charge(bool en_charge) {
    return economie_en_charge(s_etat, en_charge);
}

void economie_niveau(float niveau) {
    economie_niveau_lu(s_etat, niveau);
}

bool economie_sur_batterie() {
    return s_etat.sur_batterie;
}

bool economie_batterie_basse() {
    return s_etat.batterie_basse;
}

float economie_sortie_retro(float demande) {
    s_demande = demande;
    return std::min(demande, s_plafond);
}

bool economie_plafond_sortie(float plafond) {
    if (plafond == s_plafond) return false;
    s_plafond = plafond;
    return true;
}

float economie_sortie_actuelle() {
    return std::min(s_demande, s_plafond);
}

bool economie_periode(uint32_t periode_ms) {
    if (periode_ms == s_periode_ms) return false;
    s_periode_ms = periode_ms;
    return true;
}
