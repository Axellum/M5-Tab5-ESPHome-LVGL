/**
 * [AI-CONTEXT]
 * @file tab5_historique.h
 * @role Popup Température, historique (tab5_historique.cpp, ADR-0032).
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
// Historique des températures (ADR-0032) — tab5_historique.cpp
// =============================================================================
// Popup « Température » (historique_popup.yaml) : la courbe d'une température sur 24 h,
// 7 jours ou 30 jours, son humidité quand HA en envoie (ADR-0047), et, pour la seconde
// température de l'accueil (serre ou dehors), la prévision de la météo à sa suite. Ouvert
// par un appui long sur une température (clé salon, serre ou pR, climate_card.yaml) ; un
// onglet par température connue, et un glissement passe à la suivante. Home Assistant
// répond à l'événement esphome.tab5_historique (cle, vue) par l'action tab5_maj_historique.
//
// Widgets posés par le script tab5_historique_ouvrir (tab5-historique.yaml) à la première
// ouverture. Cartes : 0 maintenant, 1 minimum, 2 maximum, 3 prévu (seconde température) ou
// humidité (une pièce qui en a une) ; vues : 0 jour (24 h), 1 semaine (7 jours), 2 mois
// (30 jours).
struct HistoriqueUI {
    lv_obj_t* popup = nullptr;            // historique_popup
    lv_obj_t* carte[4] = {};              // historique_carte_N
    lv_obj_t* nom[4] = {};                // historique_nom_N
    lv_obj_t* valeur[4] = {};             // historique_valeur_N (police de la date du thème)
    lv_obj_t* detail[4] = {};             // historique_detail_N
    lv_obj_t* titre = nullptr;            // historique_titre : « Chambre · 24 dernières heures »
    lv_obj_t* vue_btn[3] = {};            // historique_vue_N
    lv_obj_t* zone = nullptr;             // historique_zone : tracé, construit en C++
    lv_obj_t* titre_popup = nullptr;      // historique_popup_titre : les onglets commencent après
    lv_obj_t* onglet[7] = {};             // historique_onglet_N : une page par température
    const esphome::font::Font* police = nullptr;   // roboto_22 : graduations, axe, légende
    // Événement esphome.tab5_historique (script tab5_historique_demande, lambda sans capture).
    void (*demander)(const char* cle, const char* vue) = nullptr;
};
extern HistoriqueUI g_historique_ui;

// Action tab5_maj_historique. cle : salon | serre | p0 à p4 ; vue : jour | semaine | mois ;
// entete : « nom|debut|pas|maintenant|actuel|exterieur[|humidite] » (nom affiché, vide =
// celui de l'emplacement ; debut = AAAA-MM-JJTHH:MM local du premier créneau ; pas =
// minutes par créneau ; maintenant = minutes depuis debut ; actuel = valeur actuelle ;
// exterieur = 1 si la seconde température est dehors ; humidite = humidité actuelle en %,
// absente ou vide sans capteur d'humidité, ADR-0047) ; mesures : « moy,min,max » par
// créneau, plus « ,h_moy,h_min,h_max » avec l'humidité, séparés par « ; » (vide = pas de
// donnée), 64 au plus ; previsions : « minute,moy[,min,max] » séparés par « ; » (minute
// depuis debut, dans l'ordre), 48 au plus. Lecture : historique_lire (tab5_parse.h). Une
// réponse pour une autre température que celle montrée est gardée pour son onglet.
void historique_recu(const std::string& cle, const std::string& vue, const std::string& entete,
                     const std::string& mesures, const std::string& previsions);
// Ouvre le popup sur une clé (salon | serre | pR, vue 24 h), le peint, et demande ses données.
void historique_ouvrir(const std::string& cle);
// Boutons 24 h / 7 jours / 30 jours : change la vue et la demande à HA.
void historique_choisir_vue(int vue);
// Onglet N (historique_onglet.yaml) : montre sa température, à la même vue.
void historique_onglet(int n);
