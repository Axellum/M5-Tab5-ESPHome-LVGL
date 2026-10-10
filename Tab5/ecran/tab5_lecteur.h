/**
 * [AI-CONTEXT]
 * @file tab5_lecteur.h
 * @role Lecteur de musique (tab5_lecteur.cpp, ADR-0050) : le popup « Musique », sa
 *       mini-barre de l'accueil et le lecteur compact de la zone à gauche de l'horloge
 *       (ADR-0051, lot 2).
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes).
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; un module nouveau = un
 *       en-tête de plus, inclus par tab5_custom.h et listé sous `includes:` des deux
 *       configurations racine (tab5-ha-hmi.yaml, tab5-rendu-host.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

// =============================================================================
// Lecteur de musique (ADR-0050, 10/10/2026, demande d'Axel) — tab5_lecteur.cpp
// =============================================================================
// Un popup centré (940 × 536, le tableau de bord voilé autour) qui montre le lecteur
// multimédia de Home Assistant choisi : pochette, titre, artiste, album, application,
// barre de lecture touchable, commandes (aléatoire, précédent, lecture / pause, suivant,
// répétition), volume et muet ; en bas, une pastille par lecteur de la liste
// « Tab5 · lecteurs de musique » (packages/tab5_reglages.yaml), l'actif en accent. Une
// commande que le lecteur n'offre pas (supported_features) est masquée à sa place.
// Ouvert par l'appui long d'une tuile med sans option t, par la mini-barre « en lecture »
// de l'accueil, par un geste de l'accueil (code « musique ») ou par « Aller à l'écran →
// Musique ». La tablette ne nomme aucune entité : elle dit ce qu'elle veut par l'événement
// esphome.tab5_lecteur (ADR-0025) et HA pousse ce qu'elle montre (tab5_maj_lecteur).

// Boutons du popup et de la mini-barre (lecteur_appui), dans le YAML en nombre.
enum LecteurBouton : int {
    LECTEUR_BTN_LECTURE = 0,
    LECTEUR_BTN_PRECEDENT = 1,
    LECTEUR_BTN_SUIVANT = 2,
    LECTEUR_BTN_ALEATOIRE = 3,
    LECTEUR_BTN_REPETITION = 4,
    LECTEUR_BTN_MUET = 5,
    LECTEUR_BTN_ALLUMER = 6,
};
// Curseurs du popup (lecteur_curseur).
enum LecteurCurseur : int {
    LECTEUR_CURSEUR_POSITION = 0,
    LECTEUR_CURSEUR_VOLUME = 1,
};
constexpr int kLecteurPuces = 6;  // = kLecteursMax (tab5_parse.h), une pastille par lecteur

// Widgets posés par le script tab5_lecteur_lier (tab5-lecteur.yaml) avant la première
// réception ou ouverture : id() n'existe que dans une lambda YAML.
struct LecteurUI {
    lv_obj_t* popup = nullptr;                    // lecteur_popup
    lv_obj_t* nom = nullptr;                      // lecteur_popup_titre (en-tête : le lecteur)
    lv_obj_t* app = nullptr;                      // lecteur_app (pastille de l'application)
    lv_obj_t* app_texte = nullptr;                // lecteur_app_texte
    lv_obj_t* pochette = nullptr;                 // lecteur_pochette_img (image téléchargée)
    lv_obj_t* pochette_vide = nullptr;            // lecteur_pochette_vide (note de musique)
    lv_obj_t* titre = nullptr;                    // lecteur_titre (roboto_32_b)
    lv_obj_t* artiste = nullptr;                  // lecteur_artiste
    lv_obj_t* album = nullptr;                    // lecteur_album
    lv_obj_t* lecture = nullptr;                  // lecteur_lecture : position, commandes, volume
    lv_obj_t* position = nullptr;                 // lecteur_barre (curseur 0 à 1000)
    lv_obj_t* ecoule = nullptr;                   // lecteur_ecoule
    lv_obj_t* duree = nullptr;                    // lecteur_duree
    lv_obj_t* btn[7] = {};                        // lecteur_btn_N, ordre de LecteurBouton
    lv_obj_t* ico_lecture = nullptr;              // lecteur_ico_0 (lecture / pause)
    lv_obj_t* ico_aleatoire = nullptr;            // lecteur_ico_3 (lecteur_commande.yaml)
    lv_obj_t* ico_repetition = nullptr;           // lecteur_ico_4 (lecteur_commande.yaml)
    lv_obj_t* ico_muet = nullptr;                 // lecteur_ico_muet
    lv_obj_t* volume = nullptr;                   // lecteur_volume (curseur 0 à 100)
    lv_obj_t* volume_texte = nullptr;             // lecteur_volume_texte
    lv_obj_t* message = nullptr;                  // lecteur_message (aucune lecture, éteint…)
    lv_obj_t* conseil = nullptr;                  // lecteur_conseil
    lv_obj_t* puce[kLecteurPuces] = {};           // lecteur_puce_N
    lv_obj_t* puce_icone[kLecteurPuces] = {};     // lecteur_puce_icone_N
    lv_obj_t* puce_nom[kLecteurPuces] = {};       // lecteur_puce_nom_N
    // Mini-barre de l'accueil (lecteur_mini.yaml), par-dessus le cadre « Ok Nabu ».
    lv_obj_t* mini = nullptr;                     // lecteur_mini
    lv_obj_t* mini_pochette = nullptr;            // lecteur_mini_img
    lv_obj_t* mini_vide = nullptr;                // lecteur_mini_vide
    lv_obj_t* mini_titre = nullptr;               // lecteur_mini_titre
    lv_obj_t* mini_artiste = nullptr;             // lecteur_mini_artiste
    lv_obj_t* mini_ico = nullptr;                 // lecteur_mini_ico (lecture / pause, son bouton = parent)
    // Lecteur compact de la zone à gauche de l'horloge (lecteur_zone.yaml, ADR-0051) :
    // montré par zone_gauche_appliquer() ; la mini-barre se masque tant qu'il l'est.
    lv_obj_t* zone = nullptr;                     // zone_lecteur (la carte)
    lv_obj_t* zone_cadre = nullptr;               // lecteur_zone_cadre (cadre de la pochette)
    lv_obj_t* zone_pochette = nullptr;            // lecteur_zone_img
    lv_obj_t* zone_vide = nullptr;                // lecteur_zone_vide (note de musique)
    lv_obj_t* zone_titre = nullptr;               // lecteur_zone_titre
    lv_obj_t* zone_artiste = nullptr;             // lecteur_zone_artiste
    lv_obj_t* zone_barre = nullptr;               // lecteur_zone_barre (0 à 1000)
    lv_obj_t* zone_btn[3] = {};                   // précédent, lecture / pause, suivant
    lv_obj_t* zone_ico = nullptr;                 // lecteur_zone_ico (lecture / pause)
    // Événement esphome.tab5_lecteur (script tab5_lecteur_evenement) : action, lecteur
    // (index dans la liste, « -1 » ou la clé d'une tuile), valeur.
    void (*envoyer)(const char* action, const char* lecteur, const char* valeur) = nullptr;
    // Pochette à télécharger (script tab5_lecteur_image, online_image.set_url) ; "" la
    // libère (online_image.release).
    void (*image)(const char* url) = nullptr;
};
extern LecteurUI g_lecteur_ui;

// Action tab5_maj_lecteur : la liste des lecteurs choisis et le lecteur montré
// (tab5_parse.h, section 9). Repeint le popup et la mini-barre ; télécharge la pochette
// si son adresse change.
void lecteur_recu(const std::string& lecteurs, const std::string& etat);
// Script tab5_lecteur_ouvrir : prépare le popup avant son ouverture (dernier état reçu)
// et demande à HA l'état frais (événement « ouvrir »). `cle` : la tuile med touchée
// (« tRT »), vide pour le lecteur courant.
void lecteur_ouvrir(const std::string& cle);
// Interval de 1 s : la position avance en lecture (popup ouvert, lecteur compact montré),
// la mini-barre se masque 5 min après une pause.
void lecteur_tic();
// Pochette téléchargée (on_download_finished de l'image lecteur_pochette) : `dsc` est son
// descripteur LVGL, relu après chaque téléchargement ; ou en échec (on_error).
void lecteur_image_prete(const lv_image_dsc_t* dsc);
void lecteur_image_erreur();
// on_client_connected : l'adresse de HA, base des pochettes relatives (http://hôte:8123).
void lecteur_hote_ha(const std::string& adresse);
// Bouton du popup ou de la mini-barre (LecteurBouton).
void lecteur_appui(int bouton);
// Curseur (LecteurCurseur) : `relache` faux pendant le glissement (le texte suit), vrai au
// relâcher (la commande part, une seule).
void lecteur_curseur(int curseur, bool relache);
// Pastille i : ce lecteur devient le lecteur montré.
void lecteur_puce(int i);
// Thèmes (ADR-0029) : couleurs posées d'ici (pastille active, aléatoire et répétition
// actifs). Appelé par theme_rejouer_ui() (tab5_theme.cpp).
void lecteur_rejouer_theme();
