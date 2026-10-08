/**
 * [AI-CONTEXT]
 * @file tab5_rangee.h
 * @role Rangée sous l'horloge (tab5_rangee.cpp, ADR-0031).
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

namespace esphome { namespace font { class Font; } }

// =============================================================================
// Rangée sous l'horloge (tab5_rangee.cpp, ADR-0031) : la ligne des plantes et jusqu'à
// trois lignes de quatre éléments décrits par HA (clés hLI de tab5_maj_tuiles), trois
// lignes à l'écran au plus, qui se relaient calées sur la carte centrale.
// =============================================================================
// Widgets, posés par le script tab5_rangee_ui (tab5-rangee.yaml) avant le premier dessin.
struct RangeeUI {
    lv_obj_t* zone = nullptr;              // rangee_capteurs : 401 × 70 sous l'horloge
    lv_obj_t* toucher = nullptr;           // btn_rangee : toucher, appui long
    lv_obj_t* panneau_plantes = nullptr;   // rangee_plantes (moisture_sensors_card dedans)
    lv_obj_t* panneau[2] = {};             // rangee_a, rangee_b : lignes de capteurs, en alternance
    lv_obj_t* element[2][4] = {};          // rangee_el_a0 … rangee_el_b3
    lv_obj_t* icone[2][4] = {};            // rangee_icone_a0 …
    lv_obj_t* texte[2][4] = {};            // rangee_texte_a0 …
    lv_obj_t* pastilles_cadre = nullptr;   // rangee_pastilles
    lv_obj_t* pastilles[3] = {};           // rangee_pastille_0 … 2
    lv_obj_t* date = nullptr;              // lbl_date : sa police (celle du thème) = valeurs de 45 px
    esphome::font::Font* police_icone[3] = {};  // mdi_font_70, mdi_font_45, mdi_font_32
    esphome::font::Font* police_texte[2] = {};  // roboto_32_b, roboto_22
};
extern RangeeUI g_rangee_ui;
// Rotateur de la carte centrale (tab5_central_rotator_auto), 0,2 s avant qu'elle tourne :
// un tour de plus ; au N-ième, la ligne suivante.
void rangee_tour();
// Toucher de la rangée : la ligne suivante, le compte des tours repart.
void rangee_toucher();
// La ligne des plantes est-elle à l'écran ? (son appui long ouvre « Mes Plantes »)
bool rangee_plantes_affichees();
// Rendu hors tablette (rendu_panneau) : la première ligne, sans animation, compte à zéro.
void rangee_recaler();
