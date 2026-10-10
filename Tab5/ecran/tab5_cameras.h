/**
 * [AI-CONTEXT]
 * @file tab5_cameras.h
 * @role Popup Caméras (tab5_cameras.cpp, ADR-0049, ADR-0057).
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
// Caméras (ADR-0049, 09/10/2026, discussion #278 ; pièces : ADR-0057, 10/10/2026)
// — tab5_cameras.cpp
// =============================================================================
// Popup « Caméras » (cameras_popup.yaml) : les caméras choisies dans la section
// « Caméras » du blueprint (16 au plus), une IMAGE FIXE rafraîchie toutes les quelques
// secondes, pas de vidéo. Rangées par la pièce que Home Assistant leur donne
// (area_name) : à partir de deux pièces, une colonne à gauche de l'image (« Toutes » puis
// une puce par pièce, avec le nombre de caméras) filtre ; glisser à gauche / à droite
// change de caméra DANS la pièce choisie, les pastilles sous l'image disent laquelle.
// Ouvert par « Aller à l'écran → Caméras » (HA, une sonnette par exemple), la roue de
// navigation ou un geste de l'accueil (code « cameras », ADR-0039).
//
// Rien n'est demandé tant que le popup est fermé. À l'ouverture, l'événement
// esphome.tab5_cameras ; le blueprint répond par l'action tab5_maj_cameras (nom,
// entity_picture, pièce et « hors ligne depuis » de chaque caméra). L'image est
// téléchargée et décodée hors de la boucle principale (tab5_cameras_charge.h : tâche
// FreeRTOS, décodeur JPEG matériel) en PSRAM. La dernière image de chaque caméra vue est
// gardée tant que le popup reste ouvert (8 Mio au plus, ADR-0057) : revenir sur une
// caméra la montre tout de suite, l'image neuve la remplace.
//
// Widgets posés par le script tab5_cameras_ouvrir (tab5-cameras.yaml) à la première
// ouverture : id() n'existe que dans une lambda YAML. Les puces et les pastilles sont les
// enfants de leur conteneur, dans l'ordre (lus par cameras_ouvrir). Les commandes sont des
// lambdas SANS capture (pointeurs de fonction), comme celles du popup Énergie.
constexpr int kCamerasPastilles = 16;   // = kCamerasMax (tab5_parse.h), enfants de cameras_pastilles
constexpr int kCamerasPuces = 17;       // « Toutes » + une par pièce, enfants de cameras_pieces
struct CamerasUI {
    lv_obj_t* popup = nullptr;                    // cameras_popup
    lv_obj_t* cadre = nullptr;                    // cameras_cadre : le cadre de l'image
    lv_obj_t* image = nullptr;                    // le widget image, créé dans le cadre (C++)
    lv_obj_t* message = nullptr;                  // cameras_message : au milieu du cadre
    lv_obj_t* nom = nullptr;                      // cameras_nom : « Pièce · Caméra »
    lv_obj_t* heure = nullptr;                    // cameras_heure : « Image de 14:32:05 »
    lv_obj_t* pastilles = nullptr;                // cameras_pastilles : la rangée
    lv_obj_t* pastille[kCamerasPastilles] = {};   // ses enfants
    lv_obj_t* pieces = nullptr;                   // cameras_pieces : la colonne des pièces
    lv_obj_t* puce[kCamerasPuces] = {};           // ses enfants (cameras_puce.yaml)
    // Événement esphome.tab5_cameras : demande la liste à Home Assistant.
    void (*demander)() = nullptr;
    // Rappelle cameras_tic() dans `ms` millisecondes (un seul rappel en attente).
    void (*attendre)(int ms) = nullptr;
};
extern CamerasUI g_cameras_ui;

// Action tab5_maj_cameras : `adresse` = base de Home Assistant vue par la tablette
// (« http://192.0.2.10:8123 », vide = celle du client de l'API), `cameras` =
// « nom|image|pièce|hors_ligne;… » (tab5_parse.h, section 10 ; vide = aucune caméra).
void cameras_recues(const std::string& adresse, const std::string& cameras);
// Ouvre le popup (dernière pièce et dernière caméra montrées, gardées en NVS), le peint,
// et demande la liste à HA.
void cameras_ouvrir();
// Rappel de tab5_cameras_attente : sonde le chargement en cours (toutes les 100 ms), montre
// l'image prête, lance la suivante, ou libère tout si le popup est fermé (plus aucun
// rappel ensuite).
void cameras_tic();
// Tap d'une puce de la colonne (cameras_puce.yaml) : 0 = « Toutes », n = n-ième pièce.
void cameras_puce(int n);
// API : adresse du client « Home Assistant » (on_client_connected, tab5-api-logic.yaml),
// base des chemins relatifs quand le blueprint n'en donne pas.
void cameras_hote_ha(const std::string& adresse);
