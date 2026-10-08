/**
 * [AI-CONTEXT]
 * @file tab5_theme.h
 * @role Thèmes de l'écran (tab5_theme.cpp, ADR-0029).
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
// Thèmes de l'écran (tab5_theme.cpp, ADR-0029 lot 2 ; entités : tab5-themes.yaml)
// =============================================================================
// Règle UIColor (et UIBandeau, UIHorloge) sur la palette du thème `theme` (index du
// select « Thème ») dans le mode `mode` (0 Sombre, 1 Clair, 2 Auto : clair sauf `nuit`).
// Vrai si le thème ou son mode a changé.
bool theme_selectionner(int theme, int mode, bool nuit);
// Formes du thème choisi (`formes:` de Tab5/themes/<thème>.yaml : rayons, bordures,
// dégradés, ombres) sur les styles partagés `styles` (dans l'ordre des tables générées
// de tab5_theme.cpp ; tab5_theme_repeindre les passe, eux ne sont atteignables que par id()).
void theme_formes(lv_style_t* const styles[], int n);
// Polices d'affichage du thème choisi (`polices:`) sur les styles style_police_horloge,
// style_police_date et style_police_titre, et géométrie de l'horloge : `polices` = les
// polices compilées (0-2 : les Roboto des trois rôles, puis celles des thèmes), `horloge`
// = les 8 labels des rouleaux (leurs parents sont les cadres) puis le « : », `date` = la
// date sous l'horloge (son parent est la tuile, dont la bordure est retranchée).
void theme_polices(lv_style_t* st_horloge, lv_style_t* st_date, lv_style_t* st_titre,
    esphome::font::Font* const polices[], int n, lv_obj_t* const horloge[9], lv_obj_t* date);
// Vrai une fois le démarrage fini (tous les setup et les on_boot synchrones) : les
// modules ont leurs widgets et peuvent repeindre.
bool theme_ui_pret();
// Après une bascule : chaque module C++ repeint les couleurs qu'il a posées lui-même,
// depuis son dernier état (les styles partagés sont repeints par tab5-themes.yaml ;
// les fonctions de chaque module : tab5_internal.h).
void theme_rejouer_ui();
