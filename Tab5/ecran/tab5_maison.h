/**
 * [AI-CONTEXT]
 * @file tab5_maison.h
 * @role Popup Maison (tab5_maison.cpp, ADR-0037).
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

// =============================================================================
// Popup « Maison » (ADR-0037, 07/10/2026, discussion #278) — tab5_maison.cpp
// =============================================================================
// Toute la maison dans un popup plein écran, comme un tableau de bord HA : une colonne
// par pièce qui a des appareils (ordre du blueprint, Pièce 1 → 5), une ligne par tuile
// (pastille de l'icône, nom, état, « ⋯ » pour l'appui long). Rien de nouveau : les
// définitions et états des tuiles (tab5_maj_tuiles, tab5_maj_emplacements), leurs gestes
// et leurs commandes. Ouvert par « Aller à l'écran → Maison » ou, en mode HA, par un tap
// sur le titre de la pièce dans la carte centrale.
//
// Widgets posés par le script tab5_maison_ouvrir (tab5-maison.yaml) : id() n'existe que
// dans une lambda YAML. Les widgets d'une ligne sont lus dans l'ordre de ses enfants
// (maison_ligne.yaml : pastille [icône], nom, état, « ⋯ »).
struct MaisonUI {
    lv_obj_t* popup = nullptr;      // maison_popup
    lv_obj_t* vide = nullptr;       // maison_vide : « Aucun appareil »
    lv_obj_t* eteindre = nullptr;   // btn_maison_eteindre : « Éteindre les lumières »
    lv_obj_t* entete[5] = {};       // maison_entete_R : nom de la pièce R
    lv_obj_t* ligne[5][5] = {};     // maison_ligne_RT : la tuile tRT (bouton)
};
extern MaisonUI g_maison_ui;

// Ouvre le popup (disposé et peint à chaque ouverture : il suit les définitions même fermé).
void maison_ouvrir();
// Tap sur le titre de la pièce (carte centrale) : vrai en mode HA, hors d'un glissement.
bool maison_titre_appui_valide();
// Toucher (long_appui faux), appui long ou « ⋯ » (vrai) de la ligne de la tuile tRT.
void maison_ligne_appui(int r, int t, bool long_appui);
// « Éteindre les lumières » : « Pièce : tout éteindre » de chaque pièce qui a des lumières.
void maison_eteindre_lumieres();
