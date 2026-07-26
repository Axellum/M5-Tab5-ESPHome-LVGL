/**
 * [AI-CONTEXT]
 * @file trivia_game.h
 * @role Jeu « Trial Poursuite » — clone Trivial Pursuit rétro-salon, QCM 4 choix,
 *      1-6 équipes, plateau circulaire 42 cases, camembert 6 parts, finale centre.
 * @architecture_constraint Flux PLEIN ECRAN 1280x720, exception assumée au chrome
 *      modal v4 (ADR-0009) : pas de carte 1250x690 ni de barre de titre de 52 px.
 *      Le YAML (ui_components/trivia_game.yaml) ne déclare QUE 4 conteneurs vides ;
 *      tout le contenu (plateau, HUD, modals, camemberts) est construit en C++ ici.
 *      Aucune dépendance Home Assistant : stats et sauvegarde en NVS via
 *      esphome::global_preferences.
 * @ai_instruction Ne PAS remettre de logique de jeu dans le YAML. Ne PAS appeler
 *      Trivia::tick() manuellement : il est piloté par un lv_timer créé à l'ouverture
 *      et détruit à la fermeture (zero tick gameplay quand le jeu est fermé).
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde NVS inter-sessions (stats + dernière partie en cours).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout doit s'accompagner d'un bump de TRIVIA_SAVE_MAGIC.
// ---------------------------------------------------------------------------
#define TRIVIA_MAX_TEAMS   6
#define TRIVIA_NCAT        6     // 6 catégories couleurs
#define TRIVIA_BOARD_SIZE  42    // cases du plateau circulaire

struct TriviaStats {
    uint32_t games_played;             // parties terminées
    uint32_t q_ok;                     // bonnes réponses globales
    uint32_t q_ko;                     // mauvaises réponses globales
    uint32_t cat_ok[TRIVIA_NCAT];     // bonnes réponses par catégorie
    uint32_t cat_ko[TRIVIA_NCAT];     // mauvaises réponses par catégorie
};

struct TriviaTeamSave {
    char     name[16];                 // nom équipe (UTF-8, null-terminated)
    uint8_t  color_idx;                // index couleur pion
    uint8_t  wedges;                   // masque bits des parts gagnées (6 bits)
    uint8_t  pos;                      // position sur le plateau (0..41)
    uint8_t  _pad;
};

struct TriviaSave {
    uint32_t        magic;             // TRIVIA_SAVE_MAGIC — sinon reset usine
    TriviaStats     stats;             // statistiques globales
    // --- Sauvegarde partie en cours ---
    uint8_t         n_teams;           // 0 = pas de partie sauvegardée
    uint8_t         current_team;      // équipe active
    uint8_t         turn_num;          // numéro du tour
    uint8_t         difficulty;        // 0=Mixte 1=Facile+Moyen 2=Tout
    uint8_t         timer_sec;         // durée réponse (15/30/60/0=illimité)
    uint8_t         _pad[3];
    uint32_t        rng_seed;          // seed du générateur (reproductibilité)
    TriviaTeamSave  teams[TRIVIA_MAX_TEAMS];
    // Solo : meilleurs scores
    uint32_t        solo_best_ms;      // meilleur temps solo (0 = aucun)
    uint8_t         solo_best_pct;     // meilleur % réussite solo
    uint8_t         _pad2[3];
};

namespace Trivia {

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont déclarés dans ui_components/trivia_game.yaml ; les
// polices viennent de tab5-styles.yaml.
struct UI {
    lv_obj_t* root  = nullptr;  // overlay plein écran 1280x720
    lv_obj_t* hud   = nullptr;  // bandeau compact 1280x48
    lv_obj_t* board = nullptr;  // aire plateau 1280x672
    lv_obj_t* panel = nullptr;  // calque menus (hub / setup / question / victoire)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
};

// Ouvre le jeu sur le hub (construit l'UI au premier appel, la réutilise ensuite)
// et démarre le lv_timer de gameplay. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrête le timer, sauvegarde la partie en cours en NVS et
// masque l'overlay. Idempotent (sans effet si déjà fermé).
void close();

// True tant que l'overlay est visible (utilisé pour router les événements).
bool is_open();

// Alimente la détection de secousse IMU (lancer le dé).
// Appelé par les capteurs BMI270 (tab5-imu.yaml) — debounce 800 ms en interne.
void on_imu(float ax, float ay, float az);

// Écrit immédiatement la sauvegarde (stats + partie) en NVS.
void persist_save();

// Recharge la sauvegarde depuis la NVS (appelé au premier open()).
void persist_load();

}  // namespace Trivia
