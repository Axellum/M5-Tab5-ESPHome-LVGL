/**
 * [AI-CONTEXT]
 * @file pinball_game.h
 * @role Jeu « Neon Apron » — flipper (pinball) PORTRAIT, table arcade néon.
 *
 * @architecture_constraint ORIENTATION. C'est la seule console de l'Arcade qui
 *      ne se joue PAS dans l'orientation du dashboard. Le dashboard vit en
 *      paysage 1280×720 (`lvgl: rotation: 270` dans tab5-styles.yaml) ; un
 *      flipper couché sur le côté ne ressemble à rien, donc Pinball::open()
 *      bascule LVGL en PORTRAIT 720×1280 et Pinball::close() restaure 270.
 *      La bascule passe par `UI::lvgl->set_rotation(...)` — d'où le pointeur
 *      LvglComponent dans la struct UI (injecté par tab5_pinball_open).
 *      Voir le bloc [AI-CONTEXT] « ORIENTATION » en tête de pinball_game.cpp
 *      pour le détail (pourquoi ça marche, ce que ça coûte, ce qui casserait).
 *
 * @architecture_constraint Flux PLEIN ECRAN, exception assumée au chrome modal
 *      v4 (ADR-0009) : pas de carte, pas de barre de titre, pas de croix — la
 *      table doit occuper toute la dalle. La sortie se fait par le hub
 *      (« Quitter »). Le YAML (ui_components/pinball_game.yaml) ne déclare que
 *      4 conteneurs vides ; tout le contenu est construit en C++ ici.
 *      Aucune dépendance Home Assistant ni réseau : scores et réglages sont
 *      persistés en NVS via esphome::global_preferences.
 *
 * @ai_instruction Ne PAS remettre de logique de jeu dans le YAML. Ne PAS
 *      appeler Pinball::tick() manuellement : la boucle est pilotée par un
 *      lv_timer créé à l'ouverture et détruit à la fermeture (zéro tick
 *      gameplay quand le jeu est fermé).
 *
 * [AI-WARNING] Ne PAS dessiner les murs, guides ou flippers avec
 *      `transform_rotation` : c'est exactement ce qui avait rendu l'ancien
 *      « Flip Noir » (supprimé en 697e2e9) à la fois laid et cher. La table est
 *      construite UNE fois en géométrie fixe (arcs + polylignes + rectangles
 *      arrondis) ; seuls la bille, les flippers, les flashes et le plunger
 *      bougent. Voir la section 10 de pinball_game.cpp.
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde inter-sessions (NVS).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout impose un bump de PINBALL_SAVE_MAGIC — une
// sauvegarde au mauvais format est alors rejetée et repart à zéro.
// [AI-WARNING] Le magic est « PIN2 » et PAS « PIN1 » : « PIN1 » était le layout
//      de l'ancien Flip Noir, incompatible. Réutiliser « PIN1 » ferait charger
//      des octets d'un autre jeu dans cette structure.
// ---------------------------------------------------------------------------
#define PINBALL_NSCORES 10   // taille du classement persistant

// Une entrée du classement. Compacte : 8 octets × 10 = 80 octets.
struct PinballScore {
    uint32_t score;
    uint16_t ball_reached;  // nombre de billes jouées dans cette partie
    uint8_t  multiballs;    // multiballs déclenchés
    uint8_t  tilted;        // 1 = la partie a connu au moins un TILT
};

struct PinballSave {
    uint32_t magic;          // PINBALL_SAVE_MAGIC — sinon reset usine
    uint32_t games;          // parties jouées (carrière)
    uint32_t total_score;    // cumul de points (carrière)
    uint32_t best_ball;      // meilleur score sur une seule bille
    uint32_t tilts;          // TILT subis (carrière)
    uint32_t multiballs;     // multiballs déclenchés (carrière)
    PinballScore top[PINBALL_NSCORES];
    uint8_t  nudge_sens;     // sensibilité du nudge IMU (0..4, défaut 2)
    uint8_t  sfx;            // 1 = bips activés (stubs, cf. sfx_* dans le .cpp)
    uint8_t  flip_screen;    // 0 = portrait « rotation 0 », 1 = portrait retourné (180)
    uint8_t  invert_nudge;   // 1 = inverse le sens du nudge gauche/droite
    int16_t  cal_x;          // référence IMU « à plat », en milli-g
    int16_t  cal_y;
};

namespace Pinball {

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont déclarés dans ui_components/pinball_game.yaml ; les
// polices viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors
// lambda, d'où l'injection depuis le script tab5_pinball_open).
struct UI {
    lv_obj_t* root  = nullptr;   // page LVGL plein écran (portrait 720×1280)
    lv_obj_t* field = nullptr;   // plateau de jeu, sous le fronton
    lv_obj_t* hud   = nullptr;   // fronton / DMD (score, billes, mode)
    lv_obj_t* panel = nullptr;   // calque menus (hub / classement / pause / fin)
    size_t home_idx = 0;         // index de la page de retour (page_arcade = 1)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
    const esphome::font::Font* f_score = nullptr;  // roboto_55_b — chiffres du DMD
    const esphome::font::Font* f_led   = nullptr;  // roboto_mono_24 — libellés DMD
    // [AI-WARNING] Sans ce pointeur, pas de bascule portrait : c'est le seul
    // moyen d'appeler LvglComponent::set_rotation() depuis du C++ hors lambda.
    esphome::lvgl::LvglComponent* lvgl = nullptr;
};

// Ouvre le jeu sur le hub (construit l'UI au premier appel, la réutilise
// ensuite), bascule l'écran en portrait et démarre le lv_timer. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrête le timer, sauvegarde en NVS, masque l'overlay et
// RESTAURE le paysage 270°. Idempotent (sans effet si déjà fermé).
void close();

// True tant que l'overlay est visible (utilisé pour router les événements et
// pour exclure le jeu du retour automatique à l'écran principal).
bool is_open();

// Alimente le détecteur de nudge. Appelé par les capteurs BMI270
// (tab5-imu.yaml) — ne fait que stocker, aucun calcul lourd ici.
void on_imu(float ax, float ay, float az);

// Prend l'inclinaison courante comme référence « tablette à plat ».
// Accessible depuis les réglages du hub et depuis l'écran de pause.
void calibrate_flat();

// Écrit immédiatement la sauvegarde en NVS (appelé aux moments clés).
void persist_save();

// Recharge la sauvegarde depuis la NVS (appelé au premier open()).
void persist_load();

}  // namespace Pinball
