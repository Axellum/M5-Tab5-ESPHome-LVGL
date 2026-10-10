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
// ouverture : id() n'existe que dans une lambda YAML. Cartes (energie_carte.yaml) : 0
// solaire, 1 maison, 2 réseau, 3 batterie (page Production), 4 autoconsommé, 5 vendu,
// 6 acheté, 7 gains (page Bilan) ; vues : 0 heures, 1 jours, 2 mois. Pages (ADR-0058) :
// 0 Flux, 1 Aujourd'hui, 2 Production, 3 Bilan.
constexpr int kEnergieCartes = 8;
constexpr int kEnergiePages = 4;
struct EnergieUI {
    lv_obj_t* popup = nullptr;            // energie_popup
    lv_obj_t* page[kEnergiePages] = {};   // energie_page_flux, _soleil, _production, _bilan
    lv_obj_t* onglet[kEnergiePages] = {}; // energie_onglet_N (pages_onglet.yaml)
    lv_obj_t* carte[kEnergieCartes] = {}; // energie_carte_N
    lv_obj_t* nom[kEnergieCartes] = {};   // energie_nom_N
    lv_obj_t* icone[kEnergieCartes] = {}; // energie_icone_N (mdi_font_45)
    lv_obj_t* valeur[kEnergieCartes] = {};  // energie_valeur_N (police de la date du thème)
    lv_obj_t* ligne1[kEnergieCartes] = {};  // energie_ligne1_N
    lv_obj_t* ligne2[kEnergieCartes] = {};  // energie_ligne2_N
    lv_obj_t* graphique = nullptr;        // energie_graphique : carte du bas
    lv_obj_t* titre = nullptr;            // energie_titre : « Aujourd'hui · 12.4 kWh »
    lv_obj_t* vue_btn[3] = {};            // energie_vue_N
    lv_obj_t* zone = nullptr;             // energie_zone : barres, construites en C++
    lv_obj_t* attente = nullptr;          // energie_attente : avant le premier instantané
    // Page Flux : un cercle par carte (0 solaire, 1 maison, 2 réseau, 3 batterie,
    // energie_flux_noeud.yaml), sous eux la zone des traits et de l'anneau.
    lv_obj_t* flux_zone = nullptr;        // energie_flux_zone
    lv_obj_t* flux_noeud[4] = {};         // energie_flux_N
    lv_obj_t* flux_icone[4] = {};         // energie_flux_icone_N (mdi_font_45)
    lv_obj_t* flux_valeur[4] = {};        // energie_flux_valeur_N
    lv_obj_t* flux_texte[4] = {};         // energie_flux_texte_N : « Aujourd'hui », « Charge »…
    lv_obj_t* flux_detail[4] = {};        // energie_flux_detail_N : « 1.47 kWh », « 400 W »…
    // Page Aujourd'hui : soleil et heures (carte de gauche), chiffres (carte de droite).
    lv_obj_t* soleil_zone = nullptr;      // energie_soleil_zone
    lv_obj_t* soleil_infos = nullptr;     // energie_soleil_infos
    // Page Bilan : carte du graphique, comme celle de Production.
    lv_obj_t* bilan_titre = nullptr;      // energie_bilan_titre
    lv_obj_t* bilan_vue_btn[3] = {};      // energie_bilan_vue_N
    lv_obj_t* bilan_zone = nullptr;       // energie_bilan_zone
    const esphome::font::Font* police = nullptr;          // roboto_22 : libellés de l'axe
    const esphome::font::Font* police_grasse = nullptr;   // roboto_32_b : chiffres
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
// Action tab5_maj_energie_soleil (ADR-0058) : « lever|midi|coucher|prevu_jour|
// prevu_demain|source|creneau_debut|creneau_fin|prevu|clair » (page « Aujourd'hui »).
void energie_soleil(const std::string& payload);
// Action tab5_maj_energie_bilan (ADR-0058) : vue, début comme l'historique, payload
// « devise|vente|achat|gain » (page « Bilan »).
void energie_bilan(const std::string& vue, const std::string& debut, const std::string& payload);
// Ouvre le popup (vue des heures) sur la première page qui a des données (Flux, puis
// Aujourd'hui, sinon Production ; ADR-0058), le peint, et demande ses données à HA.
void energie_ouvrir();
// Boutons Heures / Jours / Mois : change la vue et la demande à HA.
void energie_choisir_vue(int vue);
// Mêmes boutons sur la page Bilan (energie_bilan_vue_N) : la même vue que Production.
void energie_bilan_choisir_vue(int vue);
// Nom de page touché en haut (pages_onglet.yaml) : `rang` parmi les pages montrées.
void energie_page(int rang);
