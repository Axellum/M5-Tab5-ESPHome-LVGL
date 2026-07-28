/**
 * [AI-CONTEXT]
 * @file trivia_game.h
 * @role Jeu « Trial Poursuite » — clone Trivial Pursuit rétro-salon, QCM 4 choix,
 *      1-6 équipes, roue authentique (couronne 42 cases + 6 rayons de 5 + QG
 *      central), camembert 6 parts, question finale au centre.
 * @architecture_constraint Flux PLEIN ECRAN 1280x720, exception assumée au chrome
 *      modal v4 (ADR-0009) : pas de carte 1250x690 ni de barre de titre de 52 px.
 *      Le YAML (ui_components/trivia_game.yaml) ne déclare QUE 4 conteneurs vides ;
 *      tout le contenu (roue, HUD, modals, camemberts) est construit en C++ ici.
 *      Aucune dépendance Home Assistant : réglages, stats et partie en cours sont
 *      persistés en NVS via esphome::global_preferences.
 * @ai_instruction Ne PAS remettre de logique de jeu dans le YAML. Ne PAS appeler
 *      Trivia::tick() manuellement : il est piloté par un lv_timer créé à l'ouverture
 *      et détruit à la fermeture (zero tick gameplay quand le jeu est fermé).
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Topologie du plateau — une VRAIE roue de Trivial Pursuit.
//   * couronne extérieure : 42 cases, dont 6 QG de catégorie (indices 0,7,…,35)
//   * 6 rayons de 5 cases reliant chaque QG au centre
//   * 1 case centrale (QG final)
// Les nœuds sont numérotés : [0..41] couronne, [42..71] rayons, [72] centre.
// ---------------------------------------------------------------------------
#define TRIVIA_NAME_LEN    12    // longueur du tampon de nom d'equipe (nul inclus)
#define TRIVIA_MAX_TEAMS   6
#define TRIVIA_NCAT        6     // 6 catégories couleurs
#define TRIVIA_RING        42    // cases de la couronne
#define TRIVIA_SPOKE_LEN   5     // cases par rayon
#define TRIVIA_NODES       (TRIVIA_RING + TRIVIA_NCAT * TRIVIA_SPOKE_LEN + 1)  // 73

// ---------------------------------------------------------------------------
// Sauvegarde NVS inter-sessions (réglages + stats + partie en cours).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout doit s'accompagner d'un bump de TRIVIA_SAVE_MAGIC
// (constante SAVE_MAGIC dans trivia_game.cpp) — sinon la NVS est relue de travers.
// ---------------------------------------------------------------------------
struct TriviaStats {
    uint32_t games_played;             // parties terminées
    uint32_t q_ok;                     // bonnes réponses globales
    uint32_t q_ko;                     // mauvaises réponses globales
    uint32_t cat_ok[TRIVIA_NCAT];      // bonnes réponses par catégorie
    uint32_t cat_ko[TRIVIA_NCAT];      // mauvaises réponses par catégorie
    uint32_t wedges_won;               // parts de camembert gagnées
    uint32_t best_turns;               // victoire la plus rapide, en tours (0 = aucune)
};

struct TriviaTeamSave {
    char     name[TRIVIA_NAME_LEN];    // nom équipe (UTF-8, null-terminated)
    uint8_t  color_idx;                // index couleur pion
    uint8_t  wedges;                   // masque bits des parts gagnées (6 bits)
    uint8_t  pos;                      // nœud du plateau (0..72)
    uint8_t  _pad;
};

struct TriviaSave {
    uint32_t        magic;             // SAVE_MAGIC — sinon reset usine
    TriviaStats     stats;             // statistiques globales
    // --- Réglages persistants (indépendants d'une partie) ---
    uint8_t         cfg_difficulty;    // 0=Facile 1=Normal 2=Expert
    uint8_t         cfg_timer;         // index dans TIMER_SEC (15/30/60/illimité)
    uint8_t         cfg_shake;         // 1 = secousse IMU = lancer le dé
    uint8_t         cfg_nteams;        // dernier nombre d'équipes utilisé
    TriviaTeamSave  roster[TRIVIA_MAX_TEAMS];   // derniers noms/couleurs choisis
    // --- Partie en cours (0 équipe = rien à reprendre) ---
    uint8_t         n_teams;
    uint8_t         current_team;
    uint8_t         turn_num;
    uint8_t         _pad;
    uint32_t        rng_seed;
    TriviaTeamSave  teams[TRIVIA_MAX_TEAMS];
    uint32_t        game_ok;           // bonnes réponses de la partie en cours
    uint32_t        game_ko;           // mauvaises réponses de la partie en cours
};

namespace Trivia {

// Palette rétro « salon violet / laiton ». Volontairement LOCALE au module :
// tab5_custom.h et tab5-styles.yaml sont des fichiers partagés du HMI, on évite
// de les toucher pour un sous-module de jeu (même choix que Chess::Pal).
// L'accent 0xFF9800 est celui de la carte Trivia du sélecteur arcade.
namespace Pal {
static constexpr uint32_t VOID_BG    = 0x100A16;  // hors plateau (miroir color_trivia_void)
static constexpr uint32_t FLOOR_BG   = 0x1A1028;  // aire de jeu   (miroir color_trivia_floor)
static constexpr uint32_t HUD_BG     = 0x150E22;  // bandeau HUD   (miroir color_trivia_hud)
static constexpr uint32_t CARD_BG    = 0x241640;  // cartes du panneau droit
static constexpr uint32_t CARD_EDGE  = 0x392458;  // liseré des cartes
static constexpr uint32_t BTN_BG     = 0x2E1D4E;  // fond de bouton
static constexpr uint32_t BTN_BG_ON  = 0x452C70;  // fond de bouton pressé
static constexpr uint32_t BTN_EDGE   = 0x4B3273;  // liseré de bouton
static constexpr uint32_t RING_DECO  = 0x221338;  // disque décoratif sous la roue
static constexpr uint32_t CELL_EDGE  = 0x0D0714;  // liseré des cases
static constexpr uint32_t HQ_EDGE    = 0xF5EDFF;  // liseré des 6 QG de catégorie
static constexpr uint32_t ROLL_BG    = 0xB388FF;  // case « Rejouer »
static constexpr uint32_t HUB_EDGE   = 0xFFE0A3;  // liseré du QG central
static constexpr uint32_t TXT        = 0xF1F5F9;  // texte principal
static constexpr uint32_t TXT_DIM    = 0xB4A7C9;  // texte secondaire
static constexpr uint32_t TXT_MUTED  = 0x6F5F8C;  // texte désactivé
static constexpr uint32_t ACCENT     = 0xFF9800;  // accent orange (titres, dé, actif)
static constexpr uint32_t GOOD       = 0x4ADE80;  // bonne réponse
static constexpr uint32_t BAD        = 0xEF4444;  // mauvaise réponse
static constexpr uint32_t DICE_BG    = 0xF7F3E3;  // face du dé (ivoire)
static constexpr uint32_t DICE_PIP   = 0x2A1B47;  // points du dé
}  // namespace Pal

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont déclarés dans ui_components/trivia_game.yaml ; les
// polices viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors lambda).
struct UI {
    lv_obj_t* root  = nullptr;  // page LVGL plein écran 1280x720
    lv_obj_t* hud   = nullptr;  // bandeau compact 1280x48
    lv_obj_t* board = nullptr;  // aire de jeu 1280x672 (roue + panneau droit)
    lv_obj_t* panel = nullptr;  // calque des menus (hub / setup / stats / pause…)
    esphome::lvgl::LvglComponent* lvgl = nullptr;  // pour navigation pages
    size_t home_idx = 0;        // index de la page de retour (page_arcade = 1)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
};

// Ouvre le jeu (construit l'UI au premier appel, la réutilise ensuite) et démarre
// le lv_timer de gameplay. Reprend la partie en cours s'il y en a une, sinon hub.
// Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrête le timer, sauvegarde réglages/stats/partie et masque
// l'overlay. Idempotent (sans effet si déjà fermé).
void close();

// True tant que l'overlay est visible (utilisé pour router les événements).
bool is_open();

// Alimente la détection de secousse IMU (lancer le dé).
// Appelé par les capteurs BMI270 (tab5-imu.yaml) — debounce interne.
void on_imu(float ax, float ay, float az);

// Écrit immédiatement la sauvegarde (réglages + stats + partie) en NVS.
void persist_save();

// Recharge la sauvegarde depuis la NVS (appelé au premier open()).
void persist_load();

}  // namespace Trivia
