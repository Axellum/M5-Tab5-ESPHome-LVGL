/**
 * [AI-CONTEXT]
 * @file pinball_game.h
 * @role Jeu « Flip Noir » — flipper rétro type borne arcade 70-80's (Getaway: High Speed).
 * @architecture_constraint Flux PLEIN ECRAN 1280x720, exception assumee au chrome
 *      modal v4 (ADR-0009) : pas de carte 1250x690 ni de barre de titre de 52 px,
 *      le playfield doit dominer. Le YAML (ui_components/pinball_game.yaml) ne
 *      declare QUE 4 conteneurs vides ; tout le contenu (HUD, table, bumpers,
 *      flippers, menus) est construit en C++ ici. Aucune dependance Home Assistant :
 *      les high scores sont persistes en NVS via esphome::global_preferences.
 * @ai_instruction Ne PAS remettre de logique de jeu dans le YAML. Ne PAS appeler
 *      Pinball::tick() manuellement : il est pilote par un lv_timer cree a l'ouverture
 *      et detruit a la fermeture (zero tick gameplay quand le jeu est ferme).
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde inter-sessions (NVS).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout doit s'accompagner d'un bump de PIN_SAVE_MAGIC
// (une sauvegarde au mauvais format est alors rejetee et repart a zero).
// ---------------------------------------------------------------------------
#define PIN_MAX_SCORES 10

struct PinScoreEntry {
    uint32_t score;       // score de la partie
    uint8_t  ctrl_mode;   // 0=boutons, 1=mixte (IMU nudge)
    uint8_t  balls;       // billes jouees
    uint16_t pad;         // alignement
    uint32_t timestamp;   // uptime secondes au moment du score
};

struct PinballSave {
    uint32_t       magic;              // PIN_SAVE_MAGIC — sinon reset usine
    PinScoreEntry  scores[PIN_MAX_SCORES];  // top 10, trie decroissant
    uint8_t        score_count;        // nombre d'entrees valides (0..10)
    uint8_t        ctrl_mode;          // 0=boutons, 1=mixte (IMU nudge)
    uint8_t        sensitivity;        // sensibilite nudge IMU (0..4, defaut 2)
    uint8_t        muted;              // 1 = SFX coupes
    uint32_t       career_games;       // parties totales
    uint32_t       career_tilts;       // tilts accumules
    uint32_t       career_multiballs;  // multiballs declenches
    int16_t        cal_x;              // offset calibration IMU, en milli-g
    int16_t        cal_y;
};

namespace Pinball {

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont declares dans ui_components/pinball_game.yaml ; les
// polices viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors lambda).
struct UI {
    lv_obj_t* root  = nullptr;  // overlay plein ecran 1280x720
    lv_obj_t* field = nullptr;  // aire de jeu 1280x672 (sous le HUD)
    lv_obj_t* hud   = nullptr;  // bandeau compact 1280x48
    lv_obj_t* panel = nullptr;  // calque menus (hub / pause / game over / scores)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
};

// Ouvre le jeu sur le hub (construit l'UI au premier appel, la reutilise ensuite)
// et demarre le lv_timer de gameplay. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrete le timer, sauvegarde en NVS et masque l'overlay.
// Idempotent (sans effet si deja ferme).
void close();

// True tant que l'overlay est visible (utilise pour router les evenements IMU).
bool is_open();

// Alimente le filtre d'inclinaison. Appele par les capteurs BMI270
// (tab5-imu.yaml) — ne fait que stocker, aucun calcul lourd ici.
void on_imu(float ax, float ay, float az);

// Prend l'inclinaison courante comme reference « tablette a plat ».
// Accessible depuis le hub et l'ecran de pause.
void calibrate();

// Ecrit immediatement la sauvegarde en NVS (appele aux moments cles).
void persist_save();

// Recharge la sauvegarde depuis la NVS (appele au premier open()).
void persist_load();

}  // namespace Pinball
