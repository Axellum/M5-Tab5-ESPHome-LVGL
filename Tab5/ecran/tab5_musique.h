/**
 * [AI-CONTEXT]
 * @file tab5_musique.h
 * @role Musique de toute la maison (tab5_musique.cpp, ADR-0067) : les quatre pages du popup
 *       Musique (ADR-0050) sur une seule brique — Lecture, Playlists (ADR-0068, grille peinte
 *       par tab5_lecteur.cpp), Pièces et Bibliothèque, ces deux-là poussées par Home
 *       Assistant (tab5_maj_musique_pieces, tab5_maj_musique_liste) d'après Music Assistant
 *       ou la liste « Tab5 · lecteurs de musique ».
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py). Ce que le lecteur (tab5_lecteur.cpp)
 *       appelle pour l'en-tête et ses pages est dans tab5_internal.h (pas un contrat YAML).
 * @ai_instruction Un widget de plus = son champ dans MusiqueUI et sa ligne dans le script
 *       tab5_musique_lier (tab5-musique.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

// =============================================================================
// Musique de toute la maison (ADR-0067, 18/10/2026, demande d'Axel) — tab5_musique.cpp
// =============================================================================
// La même carte que le popup Musique (940 × 536), d'autres pages par la brique des popups
// à pages (ADR-0046) : « Pièces », une carte par lecteur de la maison (volume, marche,
// grouper au lecteur montré, y envoyer la musique ; appui long : la roue des actions
// rapides, ADR-0036) ; « Bibliothèque », avec Music Assistant seulement : les catégories,
// une grille de 5 × 2 pochettes, un toucher qui joue ou qui ouvre, un appui long qui
// propose maintenant, à la suite, en fin de file ou en radio. La tablette ne nomme aucune
// entité : elle dit « la pièce de rang i » et renvoie la ref que HA lui a donnée (événement
// esphome.tab5_musique, ADR-0025).

constexpr int kMusiquePieces = 8;      // = kMusiquePiecesMax (tab5_parse.h) : une carte par pièce
constexpr int kMusiqueElements = 10;   // = kMusiqueElementsMax : une case de la grille par élément
constexpr int kMusiqueCategories = 7;  // Favoris, Playlists, Albums, Artistes, Radios, Podcasts, Récents
// Onglets sur la ligne du titre : Lecture, Playlists (ADR-0068), Pièces, Bibliothèque.
constexpr int kMusiqueOnglets = 4;

// Boutons d'une carte de pièce (musique_piece.yaml), dans l'ordre de leurs widgets.
enum MusiquePieceBouton : int {
    MUSIQUE_PIECE_GROUPE = 0,     // grouper au lecteur montré, ou l'en séparer
    MUSIQUE_PIECE_TRANSFERT = 1,  // y envoyer la musique du lecteur montré
    MUSIQUE_PIECE_MARCHE = 2,     // allumer / éteindre
};
constexpr int kMusiquePieceBoutons = 3;
// Boutons des pages (musique_calques.yaml).
enum MusiqueBouton : int {
    MUSIQUE_BTN_TOUT_ICI = 0,    // Pièces : tout grouper avec le lecteur montré
    MUSIQUE_BTN_TOUT_SEPARER = 1,
    MUSIQUE_BTN_RETOUR = 2,      // Bibliothèque : le niveau d'avant
    MUSIQUE_BTN_PRECEDENTS = 3,
    MUSIQUE_BTN_SUIVANTS = 4,
};

// Widgets posés par le script tab5_musique_lier (tab5-musique.yaml) avant la première
// réception ou ouverture : id() n'existe que dans une lambda YAML.
struct MusiqueUI {
    lv_obj_t* calque = nullptr;                          // musique_calque (posé en dernier)
    lv_obj_t* onglet[kMusiqueOnglets] = {};              // musique_onglet_N (pages_onglet.yaml)
    lv_obj_t* groupe = nullptr;                          // musique_groupe : « + n » après le nom
    lv_obj_t* groupe_texte = nullptr;                    // musique_groupe_texte
    lv_obj_t* suivant = nullptr;                         // musique_suivant : « À suivre · … »
    lv_obj_t* file = nullptr;                            // musique_file : « 12 titres »
    // Page Pièces.
    lv_obj_t* pieces = nullptr;                          // musique_pieces (la page)
    lv_obj_t* conseil = nullptr;                         // musique_pieces_conseil
    lv_obj_t* tout_ici = nullptr;                        // musique_btn_0
    lv_obj_t* tout_separer = nullptr;                    // musique_btn_1
    lv_obj_t* piece[kMusiquePieces] = {};                // musique_piece_N (la carte)
    lv_obj_t* piece_icone[kMusiquePieces] = {};          // musique_piece_icone_N (mdi_font_32)
    lv_obj_t* piece_nom[kMusiquePieces] = {};            // musique_piece_nom_N
    lv_obj_t* piece_etat[kMusiquePieces] = {};           // musique_piece_etat_N
    lv_obj_t* piece_son[kMusiquePieces] = {};            // musique_piece_son_N (volume / muet)
    lv_obj_t* piece_volume[kMusiquePieces] = {};         // musique_piece_volume_N (curseur 0 à 100)
    lv_obj_t* piece_btn[kMusiquePieces][kMusiquePieceBoutons] = {};  // musique_piece_btn_N_B
    lv_obj_t* piece_lien[kMusiquePieces] = {};           // musique_piece_lien_N (icône du bouton grouper)
    // Page Bibliothèque.
    lv_obj_t* biblio = nullptr;                          // musique_biblio (la page)
    lv_obj_t* categorie[kMusiqueCategories] = {};        // musique_cat_N (nom = 1er enfant)
    lv_obj_t* retour = nullptr;                          // musique_btn_2
    lv_obj_t* titre = nullptr;                           // musique_biblio_titre
    lv_obj_t* precedents = nullptr;                      // musique_btn_3
    lv_obj_t* suivants = nullptr;                        // musique_btn_4
    lv_obj_t* message = nullptr;                         // musique_biblio_message
    lv_obj_t* element[kMusiqueElements] = {};            // musique_element_N (le bouton)
    lv_obj_t* element_cadre[kMusiqueElements] = {};      // musique_element_cadre_N
    lv_obj_t* element_icone[kMusiqueElements] = {};      // musique_element_icone_N (mdi_font_70)
    lv_obj_t* element_img[kMusiqueElements] = {};        // musique_element_img_N (pochette)
    lv_obj_t* element_nom[kMusiqueElements] = {};        // musique_element_nom_N
    lv_obj_t* element_sous[kMusiqueElements] = {};       // musique_element_sous_N
    // Événement esphome.tab5_musique (script tab5_musique_evenement) : action, piece (rang
    // dans la dernière liste de pièces, « -1 » le lecteur montré), valeur.
    void (*envoyer)(const char* action, const char* piece, const char* valeur) = nullptr;
    // Pochette n à télécharger (script tab5_musique_image_n, online_image.set_url) ; ""
    // la libère (online_image.release).
    void (*telecharger)(int n, const char* url) = nullptr;
};
extern MusiqueUI g_musique_ui;

// Action tab5_maj_musique_pieces : les pièces et le contexte du lecteur montré (tab5_parse.h,
// section 9 bis). Repeint la page Pièces, l'en-tête et la ligne « À suivre ».
void musique_pieces_recu(const std::string& pieces, const std::string& contexte);
// Action tab5_maj_musique_liste : une page de la bibliothèque ou les sources d'une pièce.
void musique_liste_recue(const std::string& contexte, const std::string& elements);
// Pochette n téléchargée (on_download_finished de l'image musique_image_n) : `dsc`, son
// descripteur LVGL ; ou en échec (on_error) : l'icône du genre reste. La suivante part.
void musique_image_prete(int n, const lv_image_dsc_t* dsc);
void musique_image_erreur(int n);
// Interval de 1 s : « Chargement… » qui dure plus de 8 s devient une phrase d'erreur
// (l'accent d'un élément touché, lui, part avec un lv_timer de 600 ms, tab5_musique.cpp).
void musique_tic();
// Onglet d'en-tête touché (pages_onglet.yaml) : son rang parmi les onglets montrés.
void musique_onglet(int i);
// Carte de la pièce i (son rang dans la grille) : un toucher la montre sur la page Lecture,
// un appui long ouvre sa roue ; ses boutons (MusiquePieceBouton) ; son curseur de volume
// (`relache` faux pendant le glissement, vrai au relâcher : la commande part, une seule).
void musique_piece_appui(int i);
void musique_piece_long(int i);
void musique_piece_bouton(int i, int bouton);
void musique_piece_volume(int i, bool relache);
// Boutons des pages (MusiqueBouton).
void musique_bouton(int bouton);
// Catégorie i de la bibliothèque (Favoris…), élément i de la grille (toucher, appui long).
void musique_categorie(int i);
void musique_element_appui(int i);
void musique_element_long(int i);
