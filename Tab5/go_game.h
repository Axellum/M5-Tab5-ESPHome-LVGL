/**
 * [AI-CONTEXT]
 * @file go_game.h
 * @role Jeu « Go Tab » — Go 9×9 (défaut) / 13×13 / 19×19, IA embarquée.
 * @architecture_constraint Plein écran 1280×720, exception ADR-0009 (pas de
 *      chrome modal / croix). YAML = 4 conteneurs vides. Palette locale
 *      Go::Pal (pas de conflit tab5_custom.h avec jeux parallèles). 100 % NVS.
 * @ai_instruction Logique plateau dans go_engine.* ; IA dans go_ai.* ;
 *      ce module = UI + NVS + orchestration timer.
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"
#include "go_engine.h"

namespace esphome { namespace font { class Font; } }

#define GO_N_SIZES  3
#define GO_N_LEVELS 4

struct GoSave {
    uint32_t magic;
    uint8_t  size_idx;     // 0=9, 1=13, 2=19
    uint8_t  mode;         // 0=PvT, 1=PvP, 2=TvT
    uint8_t  human_color;  // 1=Noir, 2=Blanc (Engine::Color)
    uint8_t  ai_level;     // 0..3
    uint8_t  imu_hint;
    uint8_t  reserved0[3];
    uint16_t wins[GO_N_SIZES][GO_N_LEVELS];
    uint16_t draws[GO_N_SIZES][GO_N_LEVELS];
    uint16_t losses[GO_N_SIZES][GO_N_LEVELS];
    // Partie en cours
    uint8_t  has_game;
    uint8_t  n;
    uint8_t  side;
    uint8_t  ko;
    uint8_t  passes;
    uint8_t  setup_size;
    uint8_t  setup_mode;
    uint8_t  setup_human;
    uint8_t  setup_level;
    uint16_t move_no;
    uint16_t cap_b;
    uint16_t cap_w;
    uint8_t  board[361];
};

namespace Go {

namespace Pal {
static constexpr uint32_t VOID_BG   = 0x1A1410;
static constexpr uint32_t FLOOR_BG  = 0x2A2018;
static constexpr uint32_t HUD_BG    = 0x18120E;
static constexpr uint32_t PANEL_BG  = 0x2E241C;
static constexpr uint32_t WOOD      = 0xC4A574;  // goban
static constexpr uint32_t GRID      = 0x3A2A1A;  // lignes
static constexpr uint32_t HOSHI     = 0x2A1A10;  // points étoiles
static constexpr uint32_t STONE_B   = 0x1A1A1A;
static constexpr uint32_t STONE_B_R = 0x444444;
static constexpr uint32_t STONE_W   = 0xF2EDE4;
static constexpr uint32_t STONE_W_R = 0xC8C0B4;
static constexpr uint32_t HL        = 0x2BB3C8;
static constexpr uint32_t LAST      = 0xE05252;  // dernier coup
static constexpr uint32_t TXT       = 0xF1F5F9;
static constexpr uint32_t TXT_DIM   = 0xA89880;
static constexpr uint32_t DANGER    = 0xE05252;
static constexpr uint32_t ACCENT    = 0xE8C44A;
static constexpr uint32_t BTN       = 0x3A2E22;
static constexpr uint32_t BTN_EDGE  = 0x8A7050;
}  // namespace Pal

struct UI {
    lv_obj_t* root  = nullptr;
    lv_obj_t* field = nullptr;
    lv_obj_t* hud   = nullptr;
    lv_obj_t* panel = nullptr;
    const esphome::font::Font* f_small = nullptr;
    const esphome::font::Font* f_mid   = nullptr;
    const esphome::font::Font* f_big   = nullptr;
};

void open(const UI& ui);
void close();
bool is_open();
void on_imu(float ax, float ay, float az);
void ai_step();
void persist_save();
void persist_load();

}  // namespace Go
