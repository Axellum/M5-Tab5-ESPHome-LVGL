/**
 * [AI-CONTEXT]
 * @file draughts_game.h
 * @role Jeu « Dames Tab » — dames internationales 10×10 (défaut) + option
 *      anglaises / checkers 8×8, avec IA embarquée time-sliced.
 * @architecture_constraint Flux PLEIN ÉCRAN 1280×720, exception ADR-0009 :
 *      pas de modal_scrim / modal_header / carte 1250×690 / croix. Sortie via
 *      hub « Quitter ». YAML (ui_components/draughts_game.yaml) = 4 conteneurs
 *      vides ; tout le contenu est construit en C++. 100 % local (NVS), zéro HA.
 *      Palette LOCALE (Draughts::Pal) pour ne pas entrer en conflit avec les
 *      autres jeux en développement parallèle sur tab5_custom.h / styles.
 *      Jeu fermé, rien n'est réservé : état de partie et de l'IA (blocs Mem /
 *      Cold) créés par open() et rendus par close(), objets LVGL détruits à la
 *      fermeture et reconstruits à l'ouverture (audit du 26/09/2026, lot 4).
 * @ai_instruction Ne PAS remettre de logique dans le YAML. Ne PAS appeler
 *      tick/ai_step manuellement : lv_timer créé à open(), détruit à close().
 *      Règles FR (prise max, flying kings) et US/UK ne doivent JAMAIS se mélanger
 *      dans le même mode.
 */
#pragma once
#include "esphome.h"
#include "draughts_engine.h"  // Draughts::Engine : plateau, coups, règles (moteur pur)

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde NVS — DOIT rester trivially copyable (memcpy ESPPreferences).
// Bump DRAUGHTS_SAVE_MAGIC à chaque changement de layout.
// ---------------------------------------------------------------------------
#define DRAUGHTS_N_VARIANTS 2
#define DRAUGHTS_N_LEVELS   4

struct DraughtsSave {
    uint32_t magic;  // DRAUGHTS_SAVE_MAGIC

    // Options persistées
    uint8_t variant;      // 0 = international 10×10, 1 = anglais 8×8
    uint8_t mode;         // 0 = Joueur vs Tab, 1 = Joueur vs Joueur, 2 = Tab vs Tab
    uint8_t human_color;  // 0 = Blancs, 1 = Noirs (PvT)
    uint8_t ai_level;     // 0..3 Débutant..Expert
    uint8_t imu_hint;     // 1 = secousse = Hint
    uint8_t reserved_opt[3];

    // Stats W/D/L vs Tab : [variant][level]
    uint16_t wins[DRAUGHTS_N_VARIANTS][DRAUGHTS_N_LEVELS];
    uint16_t draws[DRAUGHTS_N_VARIANTS][DRAUGHTS_N_LEVELS];
    uint16_t losses[DRAUGHTS_N_VARIANTS][DRAUGHTS_N_LEVELS];

    // Partie en cours (reprise)
    uint8_t  has_game;       // 1 = position valide à reprendre
    uint8_t  side;           // 0 = Blancs à jouer, 1 = Noirs
    uint8_t  must_from;      // 255 = libre, sinon case (r*n+c) pour continuer une rafle
    uint8_t  board_n;        // 8 ou 10
    uint8_t  no_progress;    // demi-coups sans prise ni pion bougé (nulle à 50 / 80)
    uint8_t  setup_variant;  // miroir variant au moment du save partie
    uint8_t  setup_mode;
    uint8_t  setup_human;
    uint8_t  setup_level;
    uint8_t  board[100];     // pièces (voir Engine::Piece), index r*n+c
};

namespace Draughts {

// Palette brun/beige contrastée — locale au module (évite merge wars UIColor).
namespace Pal {
static constexpr uint32_t VOID_BG     = 0x1A120C;  // fond hors damier
static constexpr uint32_t FLOOR_BG    = 0x241810;  // fond zone jeu
static constexpr uint32_t HUD_BG      = 0x1C140E;  // bandeau
static constexpr uint32_t PANEL_BG    = 0x2A1E14;  // panneau latéral
static constexpr uint32_t LIGHT_SQ    = 0xD4B896;  // case claire beige
static constexpr uint32_t DARK_SQ     = 0x6B4226;  // case sombre brun
static constexpr uint32_t PIECE_W     = 0xF5F0E6;  // pion blanc
static constexpr uint32_t PIECE_W_RIM = 0xC8C0B0;  // liseré blanc
static constexpr uint32_t PIECE_B     = 0x2A2A2A;  // pion noir
static constexpr uint32_t PIECE_B_RIM = 0x555555;  // liseré noir
static constexpr uint32_t KING_RING   = 0xE8C44A;  // couronne / anneau dame
static constexpr uint32_t HL_MOVE     = 0x2BB3C8;  // surbrille coup silencieux (cyan)
static constexpr uint32_t HL_CAP      = 0xF2853F;  // surbrille rafle (orange)
static constexpr uint32_t HL_SEL      = 0xF7E08A;  // pièce sélectionnée
static constexpr uint32_t TXT         = 0xF1F5F9;
static constexpr uint32_t TXT_DIM     = 0xA89880;
static constexpr uint32_t TXT_OFF     = 0x6E6252;  // entrée de menu inactive
static constexpr uint32_t DANGER      = 0xE05252;
static constexpr uint32_t BTN         = 0x3A2E22;
static constexpr uint32_t BTN_EDGE    = 0x8A7050;
}  // namespace Pal

struct UI {
    lv_obj_t* root  = nullptr;  // page LVGL 1280×720
    lv_obj_t* field = nullptr;  // 1280×672 sous HUD
    lv_obj_t* hud   = nullptr;  // bandeau 1280×48
    lv_obj_t* panel = nullptr;  // calque menus plein écran
    esphome::lvgl::LvglComponent* lvgl = nullptr;  // pour navigation pages
    size_t home_idx = 0;        // index de la page de retour (page_arcade = 1)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
};

// Ouvre le jeu sur le hub : alloue l'état (jeu + IA), construit l'UI et démarre
// le lv_timer. Idempotent. Si la mémoire manque, revient à l'arcade sans ouvrir.
void open(const UI& ui);
// Ferme le jeu : sauvegarde en NVS, arrête le timer, revient à l'arcade puis
// détruit l'UI et rend l'état (rien ne reste réservé). Idempotent.
void close();
bool is_open();

// IMU : secousse = Hint si option active (sinon no-op hors jeu).
void on_imu(float ax, float ay, float az);

// Time-slice IA — appelée par le timer interne ; exposée pour tests.
void ai_step();

// Sans effet jeu fermé : la sauvegarde n'est en mémoire que jeu ouvert
// (persist_load() est appelé à chaque open()).
void persist_save();
void persist_load();

}  // namespace Draughts
