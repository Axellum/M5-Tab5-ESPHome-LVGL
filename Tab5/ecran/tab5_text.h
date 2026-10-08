/**
 * [AI-CONTEXT]
 * @file tab5_text.h
 * @role Textes (tab5_text.cpp) : traduction des textes du YAML au démarrage, acquittements
 *       locaux des alertes.
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
#include <initializer_list>
#include <string>

// --- Langue de l'écran (lot 4, 27/09/2026) ---

// Traduit une fois, en fin de setup, les textes posés par le YAML sous chacune des
// racines (les pages LVGL). Sans effet en français. Voir tab5_i18n.h.
void i18n_apply_boot(std::initializer_list<lv_obj_t*> racines);

void tab5_dismiss_local_add(std::string& store, const std::string& id);
