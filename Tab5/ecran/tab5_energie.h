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
// solaire, 1 maison, 2 réseau, 3 batterie (page Production) ; vues : 0 heures, 1 jours,
// 2 mois. Pages (ADR-0058) : 0 Flux, 1 Aujourd'hui, 2 Production, 3 Charges (les dernières
// charges des voitures, ADR-0077), 4 Bilan (carte « Batterie réseau » et graphique du net,
// construits en C++ : ADR-0062).
constexpr int kEnergieCartes = 4;
constexpr int kEnergiePages = 5;
// Cercles de la page Flux : les quatre cartes, puis une voiture ou deux (ADR-0076).
constexpr int kEnergieNoeuds = 6;
struct EnergieUI {
    lv_obj_t* popup = nullptr;            // energie_popup
    lv_obj_t* page[kEnergiePages] = {};   // energie_page_flux, _soleil, _production, _charges, _bilan
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
    // Page Flux : un cercle par nœud (0 solaire, 1 maison, 2 réseau, 3 batterie, 4 et 5 les
    // voitures, energie_flux_noeud.yaml), sous eux la zone des traits et des anneaux.
    lv_obj_t* flux_zone = nullptr;        // energie_flux_zone
    lv_obj_t* flux_noeud[kEnergieNoeuds] = {};   // energie_flux_N
    lv_obj_t* flux_icone[kEnergieNoeuds] = {};   // energie_flux_icone_N (mdi_font_45)
    lv_obj_t* flux_valeur[kEnergieNoeuds] = {};  // energie_flux_valeur_N
    lv_obj_t* flux_texte[kEnergieNoeuds] = {};   // energie_flux_texte_N : « Aujourd'hui », « Charge », le nom d'une voiture…
    lv_obj_t* flux_detail[kEnergieNoeuds] = {};  // energie_flux_detail_N : « 1.47 kWh », « 400 W », « 7.20 kW »…
    // Page Aujourd'hui : soleil et heures (carte de gauche), chiffres (carte de droite).
    lv_obj_t* soleil_zone = nullptr;      // energie_soleil_zone
    lv_obj_t* soleil_infos = nullptr;     // energie_soleil_infos
    // Page Charges (ADR-0077) : la liste, dessinée en C++ dans la carte de verre.
    lv_obj_t* charges_zone = nullptr;     // energie_charges_zone
    // Page Bilan (ADR-0062) : carte « Batterie réseau » à gauche, graphique du net à droite.
    lv_obj_t* bilan_heros = nullptr;      // energie_bilan_heros : la carte de gauche
    lv_obj_t* bilan_titre = nullptr;      // energie_bilan_titre : « Octobre », « Aujourd'hui »…
    lv_obj_t* bilan_vue_btn[3] = {};      // energie_bilan_vue_N
    lv_obj_t* bilan_zone = nullptr;       // energie_bilan_zone : sous le titre et les boutons
    const esphome::font::Font* police = nullptr;          // roboto_22 : libellés de l'axe
    const esphome::font::Font* police_grasse = nullptr;   // roboto_32_b : chiffres
    const esphome::font::Font* police_grande = nullptr;   // roboto_55_b : net du mois (Bilan)
    const esphome::font::Font* police_icone = nullptr;    // mdi_font_45 : icônes de la page Charges
    // Événement esphome.tab5_energie (script tab5_energie_demande, lambda sans capture).
    void (*demander)(const char* vue) = nullptr;
};
extern EnergieUI g_energie_ui;

// Action tab5_maj_energie : « solaire|maison|reseau|batterie|batterie_puissance|
// batterie_temperature|unite_temperature|jour ». Champ vide = capteur non choisi (sa
// carte ou sa ligne disparaît), « nan » = choisi sans valeur (« -- »). Puissances en W
// (réseau : + achat, − vente ; batterie : + charge, − décharge), jour en kWh.
void energie_instantane(const std::string& payload);
// Action tab5_maj_voiture (ADR-0076) : rang (« 1 » ou « 2 ») et « nom|batterie_pct|autonomie|
// unite_distance|etat|puissance_w|energie_ajoutee_kwh|limite_pct|minutes_restantes|
// compteur_compte » (tab5_parse.h, voiture_lire). Payload vide = voiture absente. Page Flux :
// un cercle par voiture, la charge retranchée de la maison.
void energie_voiture(const std::string& rang, const std::string& payload);
// Action tab5_maj_voiture_charges (ADR-0077) : jusqu'à six lignes séparées par « ; », la plus
// récente d'abord, « rang|jour|heure|duree_min|kwh|part_soleil_pct » (tab5_parse.h,
// charges_lire ; la sortie de `charges` du script tab5_voitures_analyse, ADR-0074). Payload vide
// = aucune charge : la page « Charges » disparaît (elle n'existe que depuis la première reçue).
void energie_voiture_charges(const std::string& payload);
// Action tab5_maj_voiture_jour (ADR-0078) : rang (« 0 » la maison sans les voitures, « 1 » et « 2 » les
// voitures), début (AAAA-MM-JJ) et 24 kWh par heure séparés par « ; » (tab5_parse.h, conso_jour_lire ;
// les sorties `jour_1`, `jour_2` et `jour_maison` du script tab5_voitures_analyse). Page « Aujourd'hui » :
// la consommation de chaque heure empilée à côté de la production, une légende avec les kWh du jour et la
// part des voitures venue du soleil. Sans voiture qui ait une heure connue, la page est celle d'avant.
void energie_voiture_jour(const std::string& rang, const std::string& debut, const std::string& valeurs);
// Action tab5_maj_energie_historique : vue (heures | jours | mois), début (AAAA-MM-JJ :
// le jour des heures, le premier des jours, le 1er du premier mois), valeurs en kWh
// séparées par « ; » (vide = pas de donnée). 24, 30 et 12 valeurs au plus.
void energie_historique(const std::string& vue, const std::string& debut, const std::string& valeurs);
// Action tab5_maj_energie_soleil (ADR-0058) : « lever|midi|coucher|prevu_jour|
// prevu_demain|source|creneau_debut|creneau_fin|prevu|clair » (page « Aujourd'hui »).
void energie_soleil(const std::string& payload);
// Action tab5_maj_energie_bilan (ADR-0058, ADR-0062) : vue (heures, mois_civil,
// annee_civile ; jours et mois d'un HA d'avant : fenêtres glissantes de l'historique),
// début (AAAA-MM-JJ), payload « devise|vente|achat|gain|prod|conso|estime » (page
// « Bilan » ; les trois derniers champs absents d'un HA d'avant).
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
