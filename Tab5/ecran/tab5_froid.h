/**
 * [AI-CONTEXT]
 * @file tab5_froid.h
 * @role Suivi du froid (tab5_froid.cpp, ADR-0055) : le popup plein écran « Froid » (une carte
 *       par réfrigérateur ou congélateur déclaré dans Home Assistant) et l'icône qui clignote
 *       dans le coin haut-droit de l'horloge tant qu'un appareil est au niveau grave.
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py) ; ce que seule la roue de navigation
 *       appelle (froid_disponible) est dans tab5_internal.h.
 * @ai_instruction Un widget de plus = son champ dans FroidUI et sa ligne dans le script
 *       tab5_froid_lier (Tab5/paquets/tab5-froid.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

// =============================================================================
// Froid : réfrigérateurs et congélateurs (ADR-0055, 10/10/2026, demande d'Axel) — tab5_froid.cpp
// =============================================================================
// Home Assistant (packages/tab5_froid.yaml) mesure, compare aux normes et détecte : la
// tablette ne fait que montrer ce qu'il pousse par tab5_maj_froid (quatre appareils au
// plus : nom, type, température, niveau 0/1/2, cause, depuis quand, min et max des 24 h,
// la norme, la courbe des 24 h, le dernier incident). Elle ne code aucune norme.
//   - le popup « Froid » : une carte par appareil (une rangée jusqu'à trois, deux colonnes
//     pour quatre), chacune son icône (réfrigérateur ou flocon), son nom, sa température
//     colorée par niveau, son statut, ses extrêmes et sa norme, la courbe des 24 h avec
//     les lignes de la norme, et le dernier incident ;
//   - l'icône « fridge-alert » dans le coin haut-droit de l'horloge : montrée tant qu'un
//     appareil est au niveau 2, elle clignote (visible / cachée toutes les 500 ms, sans
//     fondu) ; un tap ouvre le popup. Pas d'acquittement : elle reste tant que dure le
//     niveau grave. Les notifications de la carte centrale viennent des alertes de HA
//     (source « froid », tab5_alertes.jinja), pas d'ici.
constexpr int kFroidCartes = 4;  // = kFroidMax (tab5_parse.h), une carte par appareil

// Widgets posés par le script tab5_froid_lier (tab5-froid.yaml) : id() n'existe que dans
// une lambda YAML.
struct FroidUI {
    lv_obj_t* popup = nullptr;                // froid_popup
    lv_obj_t* attente = nullptr;              // froid_attente (en attente, aucun appareil)
    lv_obj_t* conseil = nullptr;              // froid_conseil
    lv_obj_t* carte[kFroidCartes] = {};       // froid_carte_N (froid_carte.yaml)
    lv_obj_t* icone[kFroidCartes] = {};       // froid_icone_N (mdi_font_45)
    lv_obj_t* nom[kFroidCartes] = {};         // froid_nom_N
    lv_obj_t* valeur[kFroidCartes] = {};      // froid_valeur_N (police de la date)
    lv_obj_t* statut[kFroidCartes] = {};      // froid_statut_N
    lv_obj_t* extremes[kFroidCartes] = {};    // froid_extremes_N (min, max, norme)
    lv_obj_t* incident[kFroidCartes] = {};    // froid_incident_N (dernier incident)
    // L'icône qui clignote dans le coin de l'horloge (tab5-lvgl.yaml) : le bouton (zone
    // tactile) et son glyphe, seul basculé par le clignotement.
    lv_obj_t* alerte = nullptr;               // btn_froid_alerte
    lv_obj_t* alerte_icone = nullptr;         // froid_alerte_icone
};
extern FroidUI g_froid_ui;

// Action tab5_maj_froid : les appareils (tab5_parse.h, section 12). Repeint le popup s'il
// est ouvert ; montre, cache et fait clignoter l'icône de l'horloge.
void froid_recu(const std::string& payload);
// Script tab5_froid_ouvrir : peint le popup (dernier état reçu) avant son ouverture.
void froid_ouvrir();
// Thèmes (ADR-0029) : couleurs des températures, statuts, courbes et lignes de la norme.
// Appelé par theme_rejouer_ui() (tab5_theme.cpp).
void froid_rejouer_theme();
