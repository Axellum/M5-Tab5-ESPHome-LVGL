/**
 * [AI-CONTEXT]
 * @file tab5_energie.h
 * @role Popup Énergie (tab5_energie.cpp, ADR-0028).
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
#include <string>

namespace esphome { namespace font { class Font; } }

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
    lv_obj_t* valeur[4] = {};             // energie_valeur_N (police de la date du thème)
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
