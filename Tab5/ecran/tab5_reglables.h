/**
 * [AI-CONTEXT]
 * @file tab5_reglables.h
 * @role Tuile − / + au choix (tab5_reglables.cpp, ADR-0033).
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

// =============================================================================
// Tuile − / + au choix (tab5_reglables.cpp, ADR-0033) : les boutons − / + de la carte
// clim de l'accueil règlent la clim du blueprint, un appareil choisi dans le blueprint
// (clés rN) ou le volume de la tablette, choisi dans une liste qui se déroule à l'appui
// long sur la valeur entre − et + (ADR-0038). Le choix reste en NVS.
// =============================================================================
constexpr int kReglablesLignes = 10;  // clim + huit appareils du blueprint + tablette
// Widgets, posés par le script tab5_reglables_ui (tab5-reglables.yaml) avant le premier
// dessin ; commandes posées par le même script (lambdas sans capture).
struct ReglablesUI {
    lv_obj_t* zone = nullptr;             // climate_controls_zone : − / valeur / +
    lv_obj_t* consigne_clim = nullptr;    // clim_target : la consigne de la clim, quand elle est choisie
    lv_obj_t* consigne_icone = nullptr;   // clim_consigne_icone : l'icône de la clim, à gauche de sa consigne
    lv_obj_t* rangee = nullptr;           // reglable_rangee : icône + valeur d'un autre appareil
    lv_obj_t* icone = nullptr;            // reglable_icone
    lv_obj_t* valeur = nullptr;           // reglable_valeur
    lv_obj_t* liste = nullptr;            // reglables_liste : plein écran, un toucher hors du panneau la ferme
    lv_obj_t* ligne[kReglablesLignes] = {};         // reglable_ligne_N
    lv_obj_t* ligne_icone[kReglablesLignes] = {};   // reglable_ligne_N_icone
    lv_obj_t* ligne_nom[kReglablesLignes] = {};     // reglable_ligne_N_nom
    lv_obj_t* ligne_valeur[kReglablesLignes] = {};  // reglable_ligne_N_valeur
    float* volume = nullptr;                            // &id(system_volume), 0 à 1
    const bool* muet = nullptr;                         // &id(system_muted) : haut-parleur barré
    void (*volume_regler)(float v) = nullptr;           // script tab5_volume_apply (v, true)
    void (*envoyer)(const char* emplacement, const char* action, const char* valeur) = nullptr;  // tab5_action
    void (*debounce)() = nullptr;                       // script tab5_debounce_reglable
};
extern ReglablesUI g_reglables_ui;
// La clim est-elle l'appareil choisi ? (− / + et toucher de la valeur gardent alors leur
// chemin d'avant : clim_target_temp, tab5_debounce_clim_temp, popup clim.)
bool reglables_clim_choisie();
// − (sens < 0) ou + d'un autre appareil : valeur affichée tout de suite, envoi au débounce.
void reglables_pas(int sens);
// Script tab5_debounce_reglable : envoie la valeur en attente (rien si aucune).
void reglables_envoyer_attente();
// Toucher de la valeur d'un autre appareil : le popup de sa tuile, ou la télécommande.
void reglables_valeur_appui();
// Appui long sur la valeur entre − et + (ou toucher de la température du salon quand la
// tablette ne connaît aucune clim) : la liste s'ouvre (ou se ferme) ; toucher d'une
// ligne : cet appareil est choisi, la liste se ferme ; ailleurs : elle se ferme.
void reglables_liste_basculer();
void reglables_choisir(int ligne);
// Appareil suivant de la liste, sans la dérouler (geste « appareil_suivant », par défaut le
// tap court sur les minutes de l'horloge, 09/10/2026) : après le dernier, le premier.
// Rien quand la tuile est masquée.
void reglables_suivant();
void reglables_liste_fermer();
bool reglables_liste_ouverte();
// tab5_volume_apply : le volume de la tablette a changé (carte et liste s'il est choisi).
void reglables_volume_tablette();
