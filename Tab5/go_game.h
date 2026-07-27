/**
 * [AI-CONTEXT]
 * @file go_game.h
 * @role Jeu « Go Tab » — Go 9×9 / 13×13 / 19×19, IA embarquée, 100 % local.
 * @architecture_constraint Flux PLEIN ÉCRAN 1280×720, exception assumée au
 *      chrome modal v4 (ADR-0009) : pas de modal_scrim / modal_header / croix.
 *      La sortie se fait par le menu (« Quitter »). Le YAML
 *      (ui_components/go_game.yaml) ne déclare QUE 4 conteneurs vides ; tout le
 *      contenu (goban, pierres, coordonnées, HUD, panneau, menus) est construit
 *      en C++. Palette LOCALE Go::Pal : ni tab5_custom.h ni tab5-styles.yaml ne
 *      sont touchés (même choix que Chess::Pal et Lode::Pal).
 * @ai_instruction Les règles vivent dans go_engine.* (aucun LVGL), l'IA dans
 *      go_ai.* (aucun LVGL non plus). Ici : rendu, machine à états, NVS.
 *      Ne PAS appeler ai_step() en boucle depuis une lambda ESPHome : la
 *      réflexion est cadencée par le lv_timer créé à l'ouverture et détruit à la
 *      fermeture (zéro tick quand le jeu est fermé).
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"
#include "go_engine.h"

namespace esphome { namespace font { class Font; } }

#define GO_N_SIZES  3
#define GO_N_LEVELS 4

// ---------------------------------------------------------------------------
// Sauvegarde NVS. DOIT rester trivially copyable (ESPPreferences fait un memcpy
// de sizeof(T)). Toute modification de ce layout impose de bumper GO_SAVE_MAGIC
// dans go_game.cpp : une sauvegarde au mauvais format est alors rejetée et les
// réglages repartent des valeurs d'usine.
// ---------------------------------------------------------------------------
struct GoSave {
    uint32_t magic;

    // --- Réglages ---------------------------------------------------------
    uint8_t size_idx;      // 0 = 9×9, 1 = 13×13, 2 = 19×19
    uint8_t mode;          // 0 = joueur vs Tab, 1 = joueur vs joueur, 2 = Tab vs Tab
    uint8_t human_color;   // Engine::BLACK / Engine::WHITE
    uint8_t ai_level;      // 0..3
    uint8_t handicap;      // 0 (aucun) ou 2..9 pierres pour Noir
    uint8_t opt_confirm;   // 1 = un coup se valide en deux touchers
    uint8_t opt_coords;    // 1 = coordonnées A..T / 1..19 affichées
    uint8_t opt_shake;     // 1 = secousse BMI270 = indice
    uint8_t opt_lastmark;  // 1 = marqueur sur le dernier coup
    uint8_t opt_terr;      // 1 = aperçu du territoire pendant le marquage
    uint8_t reserved[2];

    // --- Statistiques -----------------------------------------------------
    uint16_t wins[GO_N_SIZES][GO_N_LEVELS];
    uint16_t draws[GO_N_SIZES][GO_N_LEVELS];
    uint16_t losses[GO_N_SIZES][GO_N_LEVELS];
    uint32_t games;
    uint32_t total_ms;

    // --- Partie en cours (reprise après reboot) ---------------------------
    uint8_t  has_game;
    uint8_t  n;
    uint8_t  side;
    uint8_t  ko;
    uint8_t  passes;
    uint8_t  r_size, r_mode, r_human, r_level, r_handicap;
    uint8_t  reserved2[2];
    uint16_t move_no;
    uint16_t cap_b, cap_w;
    uint8_t  board[Go::Engine::MAX_SQ];
};

namespace Go {

// Palette « bois clair / laque » volontairement LOCALE au module.
namespace Pal {
static constexpr uint32_t VOID_BG   = 0x14100C;  // hors goban
static constexpr uint32_t FIELD_BG  = 0x1E1811;  // tapis sous le goban
static constexpr uint32_t HUD_BG    = 0x0F0C09;  // bandeau supérieur
static constexpr uint32_t PANEL_BG  = 0x1A150F;  // panneau latéral
static constexpr uint32_t CARD_BG   = 0x241C14;  // cartes / pastilles
static constexpr uint32_t CARD_ON   = 0x33281C;  // carte pressée / active
static constexpr uint32_t WOOD      = 0xD9B372;  // plateau (haut du dégradé)
static constexpr uint32_t WOOD_DK   = 0xB68F52;  // plateau (bas du dégradé)
static constexpr uint32_t WOOD_EDGE = 0x6B4A28;  // liseré du plateau
static constexpr uint32_t GRID      = 0x4A331C;  // lignes du goban
static constexpr uint32_t HOSHI     = 0x2E1E0E;  // points étoiles
static constexpr uint32_t STONE_B   = 0x0A0C10;  // pierre noire (bas)
static constexpr uint32_t STONE_B_H = 0x4C545F;  // pierre noire (haut, reflet)
static constexpr uint32_t STONE_W   = 0xCFC7B4;  // pierre blanche (bas)
static constexpr uint32_t STONE_W_H = 0xFFFFFF;  // pierre blanche (haut, reflet)
static constexpr uint32_t STONE_W_E = 0x9C9482;  // liseré de la pierre blanche
static constexpr uint32_t LAST      = 0xE0483C;  // marqueur du dernier coup
static constexpr uint32_t HINT      = 0x35C6DC;  // indice
static constexpr uint32_t GHOST     = 0xF2C94C;  // anneau de coup en attente
static constexpr uint32_t TERR_B    = 0x22262E;  // territoire noir
static constexpr uint32_t TERR_W    = 0xEFE7D4;  // territoire blanc
static constexpr uint32_t DEADMARK  = 0xE0483C;  // croix sur un groupe mort
static constexpr uint32_t TXT       = 0xF4EFE6;  // texte principal
static constexpr uint32_t TXT_DIM   = 0xA79268;  // texte secondaire (bois pâli)
static constexpr uint32_t TXT_MUTED = 0x6B5940;  // texte désactivé
static constexpr uint32_t ACCENT    = 0xF2C94C;  // accent or (titres, actions)
static constexpr uint32_t GOOD      = 0x66D19E;  // avantage / victoire
static constexpr uint32_t DANGER    = 0xE0483C;  // abandon / défaite / reset
static constexpr uint32_t THINK     = 0x7FA8E0;  // « le Tab réfléchit »
static constexpr uint32_t EDGE      = 0x4A3A26;  // liserés discrets
}  // namespace Pal

// Pointeurs LVGL + polices fournis par tab5-scripts.yaml à l'ouverture
// (`id(...)` n'est utilisable que dans une lambda).
struct UI {
    lv_obj_t* root  = nullptr;   // overlay plein écran 1280×720
    lv_obj_t* hud   = nullptr;   // bandeau supérieur 1280×60
    lv_obj_t* field = nullptr;   // aire de jeu 1280×660 (goban + panneau)
    lv_obj_t* panel = nullptr;   // calque des menus 1280×720
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
    const esphome::font::Font* f_mono  = nullptr;  // roboto_mono_24 (liste des coups)
};

// Ouvre le jeu sur le menu principal (construit l'UI au premier appel, la
// réutilise ensuite) et démarre le lv_timer. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrête le timer, écrit la sauvegarde NVS, masque l'overlay.
void close();

// True tant que l'overlay est visible.
bool is_open();

// Alimente le détecteur de secousse (BMI270) : une secousse franche = indice.
// Filtrage scalaire uniquement, aucun calcul lourd.
void on_imu(float ax, float ay, float az);

// Écrit / recharge la sauvegarde NVS.
void persist_save();
void persist_load();

}  // namespace Go
