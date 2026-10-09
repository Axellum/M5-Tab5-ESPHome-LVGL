/**
 * [AI-CONTEXT]
 * @file tab5_roue.h
 * @role Roue d'actions rapides (tab5_roue.cpp, ADR-0036).
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
// Roue d'actions rapides (tab5_roue.cpp, ADR-0036, discussion #278) : l'appui long d'une
// lampe, d'un volet ou d'une clim ouvre, autour de sa tuile, un moyeu (l'appareil) et deux
// anneaux de boutons ronds : le premier pour ses commandes, ses familles de réglages et
// deux liens (« Maison », « Détails » : le popup d'avant) ; le second, déplié au-dessus
// d'une famille, pour ses choix (luminosités, couleurs, modes…).
// La même roue sert à la navigation (ADR-0042, tab5_roue_navigation.cpp) : l'appui long de
// la carte centrale ouvre les écrans de la tablette, rangés en familles.
// =============================================================================
constexpr int kRoueBoutons = 6;  // premier anneau : 4 commandes ou familles + 2 liens
constexpr int kRoueChoix = 6;    // second anneau : les 6 couleurs d'une lampe au plus
// Widgets (ui_components/roue_actions.yaml, roue_bouton.yaml, roue_choix.yaml,
// roue_legende.yaml), posés par le script tab5_roue_ui (tab5-roue.yaml), que tab5_tuiles_ui
// lance avant le premier dessin.
struct RoueUI {
    lv_obj_t* fond = nullptr;                    // roue_actions : voile plein écran, son toucher replie ou ferme
    lv_obj_t* bande[2] = {};                     // roue_bande_0, roue_bande_1 : arcs de verre sous les anneaux
    lv_obj_t* jauge = nullptr;                   // roue_jauge : arc autour du moyeu
    lv_obj_t* moyeu = nullptr;                   // roue_moyeu : l'appareil, sur l'ancre
    lv_obj_t* moyeu_icone = nullptr;             // roue_moyeu_icone (mdi_font_45)
    lv_obj_t* moyeu_valeur = nullptr;            // roue_moyeu_valeur (ligne d'état)
    lv_obj_t* nom = nullptr;                     // roue_nom : nom de l'appareil
    lv_obj_t* legende[kRoueBoutons] = {};        // roue_bouton_N_legende : le mot du bouton N
    lv_obj_t* bouton[kRoueBoutons] = {};         // roue_bouton_N
    lv_obj_t* icone[kRoueBoutons] = {};          // roue_bouton_N_icone (mdi_font_36)
    lv_obj_t* point[kRoueBoutons] = {};          // roue_bouton_N_point : marque une famille
    lv_obj_t* choix[kRoueChoix] = {};            // roue_choix_N
    lv_obj_t* choix_icone[kRoueChoix] = {};      // roue_choix_N_icone (mdi_font_36)
    lv_obj_t* choix_texte[kRoueChoix] = {};      // roue_choix_N_texte (« 50 % »)
    lv_obj_t* choix_legende[kRoueChoix] = {};    // roue_choix_N_legende (« Chaud »)
    void (*ouvrir_ecran)(int ecran) = nullptr;   // script tab5_ecran_ouvrir (lien « Maison », navigation)
    lv_obj_t* carte_centrale = nullptr;          // central_card : ancre de la roue de navigation
};
extern RoueUI g_roue_ui;
// Roue de navigation (ADR-0042, tab5_roue_navigation.cpp) : appui long de la carte
// centrale (central_bouton.yaml, btn_page_title_tap) ou geste « roue » de l'accueil
// (ADR-0039). Ancrée au centre de la carte centrale ; rien au bout d'un glissement.
void roue_navigation_ouvrir();
// Une fois, les widgets posés : arcs de dessin seulement, geste bloqué sur le voile, effet
// d'appui des boutons.
void roue_brancher();
// Toucher du bouton N du premier anneau : une famille se déplie (ou se replie) ; sinon la
// roue se ferme, puis sa commande part (ou sa fenêtre s'ouvre).
void roue_actions_choisir(int n);
// Toucher du choix N du second anneau : la roue se ferme, puis sa commande part.
void roue_choix_toucher(int n);
// Toucher du moyeu ou du voile : le second anneau se replie ; replié, la roue se ferme.
void roue_replier_ou_fermer();
// Inactivité, popup ouvert, écran éteint, page changée : la roue se ferme.
void roue_actions_fermer();
bool roue_actions_ouverte();
