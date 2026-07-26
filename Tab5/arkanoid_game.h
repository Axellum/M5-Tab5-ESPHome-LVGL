/**
 * [AI-CONTEXT]
 * @file arkanoid_game.h
 * @role Jeu « Arcanoïde » — casse-briques rétro Atari / borne arcade 80's.
 * @architecture_constraint Flux PLEIN ECRAN 1280x720, exception assumée au chrome
 *      modal v4 (ADR-0009) : pas de carte 1250x690 ni de barre de titre de 52 px,
 *      le playfield doit dominer. Le YAML (ui_components/arkanoid_game.yaml) ne
 *      déclare QUE 4 conteneurs vides ; tout le contenu (HUD, menus, briques,
 *      balles, raquette, power-ups) est construit en C++ ici. Aucune dépendance
 *      Home Assistant : les high scores sont persistés en NVS via
 *      esphome::global_preferences.
 * @ai_instruction Ne PAS remettre de logique de jeu dans le YAML. Ne PAS appeler
 *      Arkanoid::tick() manuellement : il est piloté par un lv_timer créé à
 *      l'ouverture et détruit à la fermeture (zéro tick gameplay quand le jeu
 *      est fermé).
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde inter-sessions (NVS).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout doit s'accompagner d'un bump de ARK_SAVE_MAGIC
// (une sauvegarde au mauvais format est alors rejetée et repart à zéro).
// ---------------------------------------------------------------------------
#define ARK_MAX_SCORES 10

struct ArkScoreEntry {
    uint32_t score;       // score de la partie
    uint8_t  level;       // niveau atteint (1..8)
    uint8_t  ctrl_mode;   // 0=IMU, 1=boutons, 2=les deux
    uint16_t pad;         // alignement
    uint32_t timestamp;   // uptime secondes au moment du score
};

struct ArkanoidSave {
    uint32_t       magic;              // ARK_SAVE_MAGIC — sinon reset usine
    ArkScoreEntry  scores[ARK_MAX_SCORES];  // top 10, trié décroissant
    uint8_t        score_count;        // nombre d'entrées valides (0..10)
    uint8_t        ctrl_mode;          // 0=IMU, 1=boutons, 2=les deux
    uint8_t        sensitivity;        // sensibilité IMU (0..4, défaut 2)
    uint8_t        muted;              // 1 = SFX coupés
    int16_t        cal_x;              // offset calibration IMU, en milli-g
    int16_t        cal_y;
};

namespace Arkanoid {

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont déclarés dans ui_components/arkanoid_game.yaml ; les
// polices viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors lambda).
struct UI {
    lv_obj_t* root  = nullptr;  // overlay plein écran 1280x720
    lv_obj_t* field = nullptr;  // aire de jeu 1280x672 (sous le HUD)
    lv_obj_t* hud   = nullptr;  // bandeau compact 1280x48
    lv_obj_t* panel = nullptr;  // calque menus (hub / pause / game over / scores)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
};

// Ouvre le jeu sur le hub (construit l'UI au premier appel, la réutilise ensuite)
// et démarre le lv_timer de gameplay. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrête le timer, sauvegarde en NVS et masque l'overlay.
// Idempotent (sans effet si déjà fermé).
void close();

// True tant que l'overlay est visible (utilisé pour router les événements IMU).
bool is_open();

// Alimente le filtre d'inclinaison. Appelé par les capteurs BMI270
// (tab5-imu.yaml) — ne fait que stocker, aucun calcul lourd ici.
void on_imu(float ax, float ay, float az);

// Prend l'inclinaison courante comme référence « tablette à plat ».
// Accessible depuis le hub et l'écran de pause.
void calibrate();

// Écrit immédiatement la sauvegarde en NVS (appelé aux moments clés).
void persist_save();

// Recharge la sauvegarde depuis la NVS (appelé au premier open()).
void persist_load();

}  // namespace Arkanoid
