/**
 * [AI-CONTEXT]
 * @file tab5_cameras.h
 * @role Popup Caméras (tab5_cameras.cpp, ADR-0049).
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

namespace esphome {
namespace image {
class Image;
}
}

// =============================================================================
// Caméras (ADR-0049, 09/10/2026, discussion #278) — tab5_cameras.cpp
// =============================================================================
// Popup « Caméras » (cameras_popup.yaml) : une page par caméra choisie dans la section
// « Caméras » du blueprint, une IMAGE FIXE de la caméra rafraîchie toutes les quelques
// secondes, pas de vidéo. Glisser à gauche / à droite change de caméra ; les pastilles
// sous l'image disent laquelle. Ouvert par « Aller à l'écran → Caméras » (HA, une
// sonnette par exemple) ou par un geste de l'accueil (code « cameras », ADR-0039).
//
// Rien n'est demandé tant que le popup est fermé. À l'ouverture, l'événement
// esphome.tab5_cameras ; le blueprint répond par l'action tab5_maj_cameras (nom et
// entity_picture de chaque caméra). L'image est téléchargée et décodée hors de la boucle
// principale (tab5_cameras_charge.h : tâche FreeRTOS, décodeur JPEG matériel) en PSRAM,
// réduite par Home Assistant (width / height du proxy des caméras), montrée à sa taille
// ou réduite par LVGL si elle dépasse 960 × 540.
//
// Widgets posés par le script tab5_cameras_ouvrir (tab5-cameras.yaml) à la première
// ouverture : id() n'existe que dans une lambda YAML. Les commandes sont des lambdas SANS
// capture (pointeurs de fonction), comme celles du popup Énergie.
constexpr int kCamerasPastilles = 8;   // = kCamerasMax (tab5_parse.h)
struct CamerasUI {
    lv_obj_t* popup = nullptr;                    // cameras_popup
    lv_obj_t* cadre = nullptr;                    // cameras_cadre : le cadre de l'image
    lv_obj_t* image = nullptr;                    // le widget image, créé dans le cadre (C++)
    lv_obj_t* message = nullptr;                  // cameras_message : au milieu du cadre
    lv_obj_t* nom = nullptr;                      // cameras_nom : nom de la caméra
    lv_obj_t* heure = nullptr;                    // cameras_heure : « Image de 14:32:05 »
    lv_obj_t* pastilles = nullptr;                // cameras_pastilles : la rangée
    lv_obj_t* pastille[kCamerasPastilles] = {};   // cameras_pastille_N
    // Événement esphome.tab5_cameras : demande la liste à Home Assistant.
    void (*demander)() = nullptr;
    // Rappelle cameras_tic() dans `ms` millisecondes (un seul rappel en attente).
    void (*attendre)(int ms) = nullptr;
};
extern CamerasUI g_cameras_ui;

// Action tab5_maj_cameras : `adresse` = base de Home Assistant vue par la tablette
// (« http://192.0.2.10:8123 », vide = celle du client de l'API), `cameras` =
// « nom|image;… » (tab5_parse.h, section 9 ; vide = aucune caméra choisie).
void cameras_recues(const std::string& adresse, const std::string& cameras);
// Ouvre le popup (dernière caméra montrée), le peint, et demande la liste à HA.
void cameras_ouvrir();
// Rappel de tab5_cameras_attente : sonde le chargement en cours (toutes les 100 ms), montre
// l'image prête, lance la suivante, ou libère tout si le popup est fermé (plus aucun
// rappel ensuite).
void cameras_tic();
// API : adresse du client « Home Assistant » (on_client_connected, tab5-api-logic.yaml),
// base des chemins relatifs quand le blueprint n'en donne pas.
void cameras_hote_ha(const std::string& adresse);
