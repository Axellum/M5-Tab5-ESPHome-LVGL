/**
 * [AI-CONTEXT]
 * @file tab5_suivi.h
 * @role Capteurs suivis (tab5_suivi.cpp, ADR-0053) : le popup plein écran « Suivi » et la
 *       carte du premier capteur dans la zone à gauche de l'horloge (ADR-0051).
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py) ; ce que seule la zone gauche appelle
 *       (suivi_zone_montrer, suivi_zone_disponible) est dans tab5_internal.h.
 * @ai_instruction Un widget de plus = son champ dans SuiviUI et sa ligne dans le script
 *       tab5_suivi_lier (Tab5/paquets/tab5-suivi.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

// =============================================================================
// Capteurs suivis (ADR-0053, 10/10/2026, demande d'Axel) — tab5_suivi.cpp
// =============================================================================
// Home Assistant pousse (tab5_maj_suivi, packages/tab5_suivi.yaml) les capteurs choisis
// dans sa liste « Tab5 · capteurs suivis » (six au plus) : nom, valeur et unité, variation
// (du jour quand l'entité donne change_pct, sinon depuis le début de la courbe) et la
// courbe des 24 dernières heures. La tablette ne nomme aucune entité et ne demande rien :
// HA pousse à chaque changement (cinq minutes au plus entre deux), à la connexion et à
// « MAJ Écran ».
//   - le popup « Suivi » : une carte par capteur, en grille (une ou deux rangées, trois
//     colonnes au plus), chacune son nom, sa valeur, sa variation colorée (flèche de
//     tendance) et sa courbe ;
//   - la zone à gauche de l'horloge (contenu « capteur ») : le PREMIER capteur de la liste,
//     sa courbe sur un dégradé ; un tap ouvre le popup.
constexpr int kSuiviCartes = 6;  // = kSuivisMax (tab5_parse.h), une carte par capteur

// Widgets posés par le script tab5_suivi_lier (tab5-suivi.yaml) : id() n'existe que dans
// une lambda YAML.
struct SuiviUI {
    lv_obj_t* popup = nullptr;                     // suivi_popup
    lv_obj_t* attente = nullptr;                   // suivi_attente (en attente, aucun capteur)
    lv_obj_t* conseil = nullptr;                   // suivi_conseil
    lv_obj_t* carte[kSuiviCartes] = {};            // suivi_carte_N (suivi_carte.yaml)
    lv_obj_t* nom[kSuiviCartes] = {};              // suivi_nom_N
    lv_obj_t* valeur[kSuiviCartes] = {};           // suivi_valeur_N
    lv_obj_t* unite[kSuiviCartes] = {};            // suivi_unite_N
    lv_obj_t* fleche[kSuiviCartes] = {};           // suivi_fleche_N (mdi_font_32)
    lv_obj_t* variation[kSuiviCartes] = {};        // suivi_variation_N
    lv_obj_t* periode[kSuiviCartes] = {};          // suivi_periode_N (« Aujourd'hui », « 24 h »)
    // Carte de la zone à gauche de l'horloge (suivi_zone.yaml), montrée par
    // zone_gauche_appliquer().
    lv_obj_t* zone = nullptr;                      // zone_suivi
    lv_obj_t* zone_nom = nullptr;                  // suivi_zone_nom
    lv_obj_t* zone_valeur = nullptr;               // suivi_zone_valeur
    lv_obj_t* zone_unite = nullptr;                // suivi_zone_unite
    lv_obj_t* zone_fleche = nullptr;               // suivi_zone_fleche (mdi_font_32)
    lv_obj_t* zone_variation = nullptr;            // suivi_zone_variation
    lv_obj_t* zone_attente = nullptr;              // suivi_zone_attente
};
extern SuiviUI g_suivi_ui;

// Action tab5_maj_suivi : les capteurs suivis (tab5_parse.h, section 10). Repeint le popup
// s'il est ouvert, la carte de la zone si elle est montrée (sinon à leur prochaine
// apparition) ; la zone gauche saute ou retrouve le contenu « capteur » quand la liste se
// vide ou se remplit.
void suivi_recu(const std::string& payload);
// Script tab5_suivi_ouvrir : peint le popup (dernier état reçu) avant son ouverture.
void suivi_ouvrir();
// Thèmes (ADR-0029) : couleurs de la variation et des courbes, dégradé de la zone.
// Appelé par theme_rejouer_ui() (tab5_theme.cpp).
void suivi_rejouer_theme();
