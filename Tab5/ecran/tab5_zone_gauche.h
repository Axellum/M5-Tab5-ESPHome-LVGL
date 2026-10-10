/**
 * [AI-CONTEXT]
 * @file tab5_zone_gauche.h
 * @role Zone à gauche de l'horloge au choix (tab5_zone_gauche.cpp, ADR-0051).
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py) ; ce que seules les autres unités C++
 *       appellent est dans tab5_internal.h (zone_gauche_recu, zone_gauche_donnees_changees,
 *       zone_gauche_rejouer_theme).
 * @ai_instruction Les contenus (enum ZoneGauche) et leurs codes vivent dans
 *       Tab5/socle/tab5_parse.h, avec leur lecture (zone_gauche_lire) : un contenu de plus
 *       y va d'abord, à la fin.
 */
#pragma once
#include "esphome.h"

namespace esphome {
namespace font {
class Font;
}
}

// =============================================================================
// Zone à gauche de l'horloge (ADR-0051, 10/10/2026, demande d'Axel) — tab5_zone_gauche.cpp
// =============================================================================
// La colonne de gauche de l'accueil, au-dessus du cadre « Ok Nabu » (qui ne change pas),
// montre un contenu au choix :
//   - « vocal » : le micro et les boutons Domo / Discu (zone_vocal, tab5-lvgl.yaml : l'écran
//     d'avant, rendu et gestes inchangés) ;
//   - « graphique » : les 15 heures qui viennent (cal_heures_data, déjà poussées par
//     tab5_maj_previsions_heures_bulk) en courbe des températures, repères du minimum et du
//     maximum, barres de pluie en mm ; un tap ouvre le popup Météo ;
//   - « lecteur » : le lecteur de musique compact (lot 2, sur le lecteur de l'ADR-0050 :
//     mêmes données, mêmes commandes ; lecteur_zone.yaml, peint par tab5_lecteur.cpp), sauté
//     quand HA a dit qu'aucun lecteur n'est choisi ; un tap hors des boutons ouvre le popup
//     Musique, et la mini-barre « en lecture » se masque tant qu'il est montré ;
//   - « capteur » (ADR-0053) : le premier capteur de « Tab5 · capteurs suivis » (nom, valeur,
//     variation, courbe des 24 h ; suivi_zone.yaml, peint par tab5_suivi.cpp), sauté quand HA
//     a dit qu'aucun capteur n'est choisi ; un tap ouvre le popup Suivi.
// Le blueprint choisit le contenu de départ et ceux qu'un tap fait défiler (clé « gauche »
// de tab5_maj_emplacements) ; le tap sur la seconde température (btn_serre_games) et le
// code de geste « zone_gauche_suivante » passent au suivant. Le choix courant est gardé en
// NVS.
struct ZoneGaucheUI {
    lv_obj_t* vocal = nullptr;              // zone_vocal (micro, Domo, Discu)
    lv_obj_t* graphique = nullptr;          // zone_graphique (carte du graphique)
    lv_obj_t* lecteur = nullptr;            // zone_lecteur (lecteur compact, tab5_lecteur.cpp)
    lv_obj_t* capteur = nullptr;            // zone_suivi (capteur suivi, tab5_suivi.cpp, ADR-0053)
    esphome::font::Font* police = nullptr;  // roboto_22 : heures, valeurs, pluie
};
extern ZoneGaucheUI g_zone_gauche_ui;

// Montre le contenu courant (les autres masqués ; le graphique construit à sa première
// apparition et repeint s'il a changé, le lecteur et le capteur repeints s'ils ont changé). Script tab5_zones_apply (tab5-zones.yaml), une fois
// les pointeurs posés ; sans effet sur un pointeur nul.
void zone_gauche_appliquer();
// Passe au contenu suivant parmi ceux proposés et disponibles, dans l'ordre de l'enum
// ZoneGauche, en boucle ; rien s'il n'y en a pas d'autre. Gardé en NVS. Tap sur la seconde
// température (climate_card.yaml) et geste « zone_gauche_suivante » (script tab5_geste).
void zone_gauche_suivante();
