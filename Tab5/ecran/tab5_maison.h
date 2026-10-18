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
// Depuis l'ADR-0066 (dix pièces), les objets sont des PLACES d'une page : colonne C (0 à 4)
// et ligne K (0 à 4) ; disposer() (tab5_maison.cpp) dit quelle tuile tRT chacune montre.
// Plus de cinq pièces : des pages de cinq colonnes au plus, au glissement (ADR-0046) et par
// les onglets « Pièces 1 à 5 » à côté du titre.
constexpr int kMaisonColonnes = 5;
constexpr int kMaisonLignes = 5;
constexpr int kMaisonPagesMax = 2;  // dix pièces, cinq colonnes par page
struct MaisonUI {
    lv_obj_t* popup = nullptr;      // maison_popup
    lv_obj_t* vide = nullptr;       // maison_vide : « Aucun appareil »
    lv_obj_t* eteindre = nullptr;   // btn_maison_eteindre : « Éteindre les lumières »
    lv_obj_t* onglet[kMaisonPagesMax] = {};                 // maison_onglet_P : la page P
    lv_obj_t* entete[kMaisonColonnes] = {};                 // maison_entete_C : nom de la pièce de la colonne C
    lv_obj_t* ligne[kMaisonColonnes][kMaisonLignes] = {};   // maison_ligne_CK : une tuile (bouton)
};
extern MaisonUI g_maison_ui;

// Ouvre le popup (disposé et peint à chaque ouverture : il suit les définitions même fermé),
// sur la page de la pièce du mode HA, sinon la première.
void maison_ouvrir();
// Tap sur le titre de la pièce (carte centrale) : vrai en mode HA et sur une page de pièce
// de la météo (ADR-0066), hors d'un glissement.
bool maison_titre_appui_valide();
// Toucher (long_appui faux), appui long ou « ⋯ » (vrai) de la ligne K de la colonne C.
void maison_ligne_appui(int c, int k, bool long_appui);
// Tap sur l'onglet d'une page (pages_onglet.yaml) ; le glissement passe par la même page.
void maison_page(int page);
// « Éteindre les lumières » : « Pièce : tout éteindre » de chaque pièce qui a des lumières.
void maison_eteindre_lumieres();
