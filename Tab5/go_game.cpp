/**
 * [AI-CONTEXT]
 * @file go_game.cpp
 * @role UI LVGL + NVS + orchestration IA pour « Go Tab ».
 * @architecture_constraint Widgets préalloués (19×19 max). Timer 33 ms :
 *      Ai::step() uniquement, jamais de recherche bloquante. Komi 6.5.
 */
#include "go_game.h"
#include "go_ai.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>

namespace Go {

using Engine::Pos;
using Engine::Color;
using Engine::BLACK;
using Engine::WHITE;
using Engine::EMPTY;
using Engine::PASS;
using Engine::MAX_SQ;
using Engine::MAX_N;

static constexpr uint32_t SAVE_MAGIC = 0x474F5431u;  // "GOT1"
static constexpr uint32_t PREF_KEY   = 0x474F5442u;  // "GOTB"
static constexpr float KOMI = 6.5f;

static const int SIZE_TAB[3] = {9, 13, 19};

enum UiState : uint8_t {
    ST_OFF = 0, ST_HUB, ST_SETUP, ST_PLAYING, ST_THINKING,
    ST_SCORE, ST_STATS, ST_SETTINGS, ST_CONFIRM_RESET
};

static UI g_ui;
static UiState g_state = ST_OFF;
static lv_timer_t* g_timer = nullptr;
static GoSave g_save{};
static esphome::ESPPreferenceObject g_pref;
static bool g_pref_ready = false;
static bool g_ui_built = false;

static Pos g_pos;
static int g_last_sq = PASS;
static char g_status[64] = "";
static Engine::Score g_score{};

static uint8_t g_cfg_size = 0;   // index 0..2
static uint8_t g_cfg_mode = 0;
static uint8_t g_cfg_human = BLACK;
static uint8_t g_cfg_level = 1;

static constexpr int HIST_MAX = 20;
static char g_hist[HIST_MAX][10];
static int g_hist_n = 0;

static constexpr int UNDO_MAX = 40;
static Pos g_undo[UNDO_MAX];
static int g_undo_n = 0;
static int g_undo_last[UNDO_MAX];
static int g_hist_at_undo[UNDO_MAX];

static float g_imu_ax = 0, g_imu_ay = 0, g_imu_az = 1;
static uint32_t g_last_shake = 0;

// Géométrie goban
static int g_n = 9;
static int g_gap = 48;       // espacement intersections
static int g_ox = 40, g_oy = 40;
static int g_panel_x = 980;
static int g_stone_r = 20;

// Widgets
static lv_obj_t* g_goban = nullptr;
static lv_obj_t* g_grid_h[MAX_N] = {};
static lv_obj_t* g_grid_v[MAX_N] = {};
static lv_obj_t* g_hoshi[9] = {};
static lv_obj_t* g_stone[MAX_SQ] = {};
static lv_obj_t* g_last_mark = nullptr;
static lv_obj_t* g_hint_mark = nullptr;
static int g_hint_sq = -1;
static uint32_t g_hint_until = 0;

static lv_obj_t* g_side = nullptr;
static lv_obj_t* g_btn_pass = nullptr;
static lv_obj_t* g_btn_undo = nullptr;
static lv_obj_t* g_btn_hint = nullptr;
static lv_obj_t* g_side_btns[3] = {};
static lv_obj_t* g_hist_lbl[8] = {};

static lv_obj_t* g_hud_trait = nullptr;
static lv_obj_t* g_hud_size = nullptr;
static lv_obj_t* g_hud_lvl = nullptr;
static lv_obj_t* g_hud_caps = nullptr;
static lv_obj_t* g_hud_status = nullptr;

static constexpr int N_SLOTS = 6;
static lv_obj_t* g_slot[N_SLOTS] = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
static lv_obj_t* g_slot_d[N_SLOTS] = {};
static lv_obj_t* g_p_title = nullptr;
static lv_obj_t* g_p_sub = nullptr;

// ===========================================================================
// NVS
// ===========================================================================

void persist_load() {
    if (!g_pref_ready) {
        g_pref = esphome::global_preferences->make_preference<GoSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != SAVE_MAGIC) {
        g_save = GoSave{};
        g_save.magic = SAVE_MAGIC;
        g_save.size_idx = 0;
        g_save.mode = 0;
        g_save.human_color = BLACK;
        g_save.ai_level = Ai::LVL_AMATEUR;
        g_save.imu_hint = 1;
    }
    g_cfg_size = g_save.size_idx < 3 ? g_save.size_idx : 0;
    g_cfg_mode = g_save.mode;
    g_cfg_human = g_save.human_color == WHITE ? WHITE : BLACK;
    g_cfg_level = g_save.ai_level;
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = SAVE_MAGIC;
    g_save.size_idx = g_cfg_size;
    g_save.mode = g_cfg_mode;
    g_save.human_color = g_cfg_human;
    g_save.ai_level = g_cfg_level;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
}

static void save_position() {
    g_save.has_game = 1;
    g_save.n = g_pos.n;
    g_save.side = g_pos.side;
    g_save.ko = g_pos.ko;
    g_save.passes = g_pos.passes;
    g_save.move_no = g_pos.move_no;
    g_save.cap_b = g_pos.captured_by_black;
    g_save.cap_w = g_pos.captured_by_white;
    g_save.setup_size = g_cfg_size;
    g_save.setup_mode = g_cfg_mode;
    g_save.setup_human = g_cfg_human;
    g_save.setup_level = g_cfg_level;
    memset(g_save.board, 0, sizeof(g_save.board));
    memcpy(g_save.board, g_pos.sq, (size_t)(g_pos.n * g_pos.n));
    persist_save();
}

static void clear_saved() { g_save.has_game = 0; persist_save(); }

static bool load_position() {
    if (!g_save.has_game) return false;
    int n = g_save.n;
    if (n != 9 && n != 13 && n != 19) return false;
    Engine::pos_init(g_pos, n);
    g_pos.side = g_save.side;
    g_pos.ko = g_save.ko;
    g_pos.passes = g_save.passes;
    g_pos.move_no = g_save.move_no;
    g_pos.captured_by_black = g_save.cap_b;
    g_pos.captured_by_white = g_save.cap_w;
    memcpy(g_pos.sq, g_save.board, (size_t)(n * n));
    g_cfg_size = g_save.setup_size;
    g_cfg_mode = g_save.setup_mode;
    g_cfg_human = g_save.setup_human;
    g_cfg_level = g_save.setup_level;
    return true;
}

static void record_result(int winner) {
    // winner: 0=noir, 1=blanc, 2=nulle — stats vs Tab seulement
    if (g_cfg_mode != 0) { clear_saved(); return; }
    uint8_t s = g_cfg_size, lv = g_cfg_level;
    if (s >= GO_N_SIZES || lv >= GO_N_LEVELS) return;
    if (winner == 2) g_save.draws[s][lv]++;
    else {
        bool human_black = (g_cfg_human == BLACK);
        bool human_won = human_black ? (winner == 0) : (winner == 1);
        if (human_won) g_save.wins[s][lv]++;
        else g_save.losses[s][lv]++;
    }
    clear_saved();
}

// ===========================================================================
// Helpers LVGL
// ===========================================================================

static void show(lv_obj_t* o, bool on) {
    if (!o) return;
    if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void set_bg(lv_obj_t* o, uint32_t c, lv_opa_t opa) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
}
static void set_border(lv_obj_t* o, uint32_t c, int w, lv_opa_t opa) {
    if (!o) return;
    lv_obj_set_style_border_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, w, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, opa, LV_PART_MAIN);
}
static lv_obj_t* mk_rect(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    return o;
}
static lv_obj_t* mk_label(lv_obj_t* parent, const esphome::font::Font* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    if (f) esphome::lvgl::lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(l, "");
    return l;
}
static void set_text_if(lv_obj_t* l, const char* t) {
    if (!l || !t) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, t) == 0) return;
    lv_label_set_text(l, t);
}
static void panel_on(bool on) { show(g_ui.panel, on); }

static void slot_list(int i, const char* title, const char* desc, uint32_t col, bool on) {
    if (i < 0 || i >= N_SLOTS) return;
    show(g_slot[i], on);
    if (!on) return;
    set_text_if(g_slot_t[i], title);
    set_text_if(g_slot_d[i], desc);
    lv_obj_set_style_text_color(g_slot_t[i], lv_color_hex(col), LV_PART_MAIN);
    set_border(g_slot[i], col, 2, LV_OPA_50);
}

static const char* level_name(uint8_t lv) {
    switch (lv) {
        case 0: return "Debutant";
        case 1: return "Amateur";
        case 2: return "Confirme";
        case 3: return "Expert";
        default: return "?";
    }
}
static const char* mode_name(uint8_t m) {
    switch (m) {
        case 0: return "Joueur vs Tab";
        case 1: return "Joueur vs Joueur";
        case 2: return "Tab vs Tab";
        default: return "?";
    }
}
static const char* size_name(uint8_t i) {
    switch (i) {
        case 0: return "9x9";
        case 1: return "13x13";
        case 2: return "19x19";
        default: return "?";
    }
}

static void sq_name(int sq, int n, char* buf, int len) {
    if (sq == PASS) { snprintf(buf, len, "pass"); return; }
    int r = sq / n, c = sq % n;
    // Coords Go : colonnes A.. (skip I), rangées n..1 depuis le bas
    char col = (char)('A' + c);
    if (col >= 'I') col++;  // convention : pas de I
    int rank = n - r;
    snprintf(buf, len, "%c%d", col, rank);
}

// ===========================================================================
// Layout goban
// ===========================================================================

static void compute_layout() {
    g_n = g_pos.n;
    g_panel_x = 1280 - 300;
    int avail_w = g_panel_x - 30;
    int avail_h = 672 - 20;
    int gap_w = avail_w / (g_n + 1);
    int gap_h = avail_h / (g_n + 1);
    g_gap = gap_w < gap_h ? gap_w : gap_h;
    if (g_gap > 56) g_gap = 56;
    if (g_gap < 22) g_gap = 22;
    g_stone_r = g_gap / 2 - 2;
    if (g_stone_r < 6) g_stone_r = 6;
    int board_px = g_gap * (g_n - 1);
    g_ox = (avail_w - board_px) / 2 + 16;
    g_oy = (672 - board_px) / 2;
}

static void layout_goban() {
    compute_layout();
    int board_px = g_gap * (g_n - 1);
    int pad = g_gap;
    if (g_goban) {
        lv_obj_set_pos(g_goban, g_ox - pad / 2, g_oy - pad / 2);
        lv_obj_set_size(g_goban, board_px + pad, board_px + pad);
        set_bg(g_goban, Pal::WOOD, LV_OPA_COVER);
    }
    for (int i = 0; i < MAX_N; i++) {
        bool use = i < g_n;
        show(g_grid_h[i], use);
        show(g_grid_v[i], use);
        if (!use) continue;
        // Lignes horizontales
        lv_obj_set_pos(g_grid_h[i], g_ox, g_oy + i * g_gap);
        lv_obj_set_size(g_grid_h[i], board_px + 1, 2);
        set_bg(g_grid_h[i], Pal::GRID, LV_OPA_COVER);
        // Verticales
        lv_obj_set_pos(g_grid_v[i], g_ox + i * g_gap, g_oy);
        lv_obj_set_size(g_grid_v[i], 2, board_px + 1);
        set_bg(g_grid_v[i], Pal::GRID, LV_OPA_COVER);
    }
    // Hoshi (points étoiles)
    int stars[9][2] = {};
    int ns = 0;
    if (g_n == 9) {
        const int s[][2] = {{2,2},{2,6},{6,2},{6,6},{4,4}};
        ns = 5; for (int i = 0; i < ns; i++) { stars[i][0] = s[i][0]; stars[i][1] = s[i][1]; }
    } else if (g_n == 13) {
        const int s[][2] = {{3,3},{3,9},{9,3},{9,9},{6,6}};
        ns = 5; for (int i = 0; i < ns; i++) { stars[i][0] = s[i][0]; stars[i][1] = s[i][1]; }
    } else {
        const int s[][2] = {{3,3},{3,9},{3,15},{9,3},{9,9},{9,15},{15,3},{15,9},{15,15}};
        ns = 9; for (int i = 0; i < ns; i++) { stars[i][0] = s[i][0]; stars[i][1] = s[i][1]; }
    }
    for (int i = 0; i < 9; i++) {
        bool use = i < ns;
        show(g_hoshi[i], use);
        if (!use) continue;
        int r = stars[i][0], c = stars[i][1];
        int d = 6;
        lv_obj_set_pos(g_hoshi[i], g_ox + c * g_gap - d / 2, g_oy + r * g_gap - d / 2);
        lv_obj_set_size(g_hoshi[i], d, d);
        lv_obj_set_style_radius(g_hoshi[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(g_hoshi[i], Pal::HOSHI, LV_OPA_COVER);
    }
    if (g_side) {
        lv_obj_set_pos(g_side, g_panel_x, 8);
        lv_obj_set_size(g_side, 288, 656);
    }
    if (g_btn_undo) {
        lv_obj_set_pos(g_btn_undo, 8, 240);
        lv_obj_set_size(g_btn_undo, 52, 100);
    }
    if (g_btn_hint) {
        lv_obj_set_pos(g_btn_hint, g_panel_x - 60, 240);
        lv_obj_set_size(g_btn_hint, 52, 100);
    }
    if (g_btn_pass) {
        lv_obj_set_pos(g_btn_pass, g_panel_x + 16, 520);
        lv_obj_set_size(g_btn_pass, 256, 56);
    }
}

static void sync_stones() {
    const int n = g_pos.n;
    const int N = n * n;
    for (int i = 0; i < MAX_SQ; i++) {
        if (i >= N) { show(g_stone[i], false); continue; }
        uint8_t c = g_pos.sq[i];
        if (c == EMPTY) { show(g_stone[i], false); continue; }
        int r = i / n, col = i % n;
        int x = g_ox + col * g_gap - g_stone_r;
        int y = g_oy + r * g_gap - g_stone_r;
        lv_obj_set_pos(g_stone[i], x, y);
        lv_obj_set_size(g_stone[i], 2 * g_stone_r, 2 * g_stone_r);
        lv_obj_set_style_radius(g_stone[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        bool black = (c == BLACK);
        set_bg(g_stone[i], black ? Pal::STONE_B : Pal::STONE_W, LV_OPA_COVER);
        set_border(g_stone[i], black ? Pal::STONE_B_R : Pal::STONE_W_R, 2, LV_OPA_COVER);
        show(g_stone[i], true);
    }
    if (g_last_sq != PASS && g_last_sq < N) {
        int r = g_last_sq / n, c = g_last_sq % n;
        int d = g_stone_r / 2;
        if (d < 4) d = 4;
        lv_obj_set_pos(g_last_mark, g_ox + c * g_gap - d / 2, g_oy + r * g_gap - d / 2);
        lv_obj_set_size(g_last_mark, d, d);
        lv_obj_set_style_radius(g_last_mark, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(g_last_mark, Pal::LAST, LV_OPA_COVER);
        show(g_last_mark, true);
    } else {
        show(g_last_mark, false);
    }
    if (g_hint_sq >= 0 && esphome::millis() < g_hint_until) {
        int r = g_hint_sq / n, c = g_hint_sq % n;
        int d = g_stone_r;
        lv_obj_set_pos(g_hint_mark, g_ox + c * g_gap - d, g_oy + r * g_gap - d);
        lv_obj_set_size(g_hint_mark, 2 * d, 2 * d);
        lv_obj_set_style_radius(g_hint_mark, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(g_hint_mark, Pal::HL, LV_OPA_40);
        set_border(g_hint_mark, Pal::HL, 2, LV_OPA_COVER);
        show(g_hint_mark, true);
    } else {
        show(g_hint_mark, false);
        g_hint_sq = -1;
    }
}

static void update_hist() {
    for (int i = 0; i < 8; i++) {
        int src = g_hist_n - 8 + i;
        if (src < 0) set_text_if(g_hist_lbl[i], "");
        else set_text_if(g_hist_lbl[i], g_hist[src]);
    }
}

static void push_hist(int sq, Color who) {
    char name[8], buf[10];
    sq_name(sq, g_pos.n, name, sizeof(name));
    snprintf(buf, sizeof(buf), "%c %s", who == BLACK ? 'N' : 'B', name);
    if (g_hist_n < HIST_MAX) {
        strncpy(g_hist[g_hist_n], buf, 9);
        g_hist[g_hist_n][9] = 0;
        g_hist_n++;
    } else {
        for (int i = 1; i < HIST_MAX; i++) memcpy(g_hist[i - 1], g_hist[i], 10);
        strncpy(g_hist[HIST_MAX - 1], buf, 9);
        g_hist[HIST_MAX - 1][9] = 0;
    }
    update_hist();
}

static void update_hud() {
    char buf[48];
    if (g_state == ST_THINKING) snprintf(buf, sizeof(buf), "Tab reflechit...");
    else snprintf(buf, sizeof(buf), "Trait: %s", g_pos.side == BLACK ? "Noir" : "Blanc");
    set_text_if(g_hud_trait, buf);
    set_text_if(g_hud_size, size_name(g_cfg_size));
    snprintf(buf, sizeof(buf), "IA: %s", level_name(g_cfg_level));
    set_text_if(g_hud_lvl, buf);
    snprintf(buf, sizeof(buf), "Cap N%d/B%d", (int)g_pos.captured_by_black, (int)g_pos.captured_by_white);
    set_text_if(g_hud_caps, buf);
    set_text_if(g_hud_status, g_status);
}

static bool is_human_turn() {
    if (g_cfg_mode == 1) return true;
    if (g_cfg_mode == 2) return false;
    return g_pos.side == g_cfg_human;
}

// Forward
static void go_hub();
static void go_setup();
static void go_stats();
static void go_settings();
static void enter_playing();
static void start_new();
static void resume_game();
static void after_move();
static void try_ai();
static void do_pass();
static void do_undo();
static void do_hint();
static void finish_score();
static void apply_move(int sq);

static void push_undo() {
    if (g_undo_n < UNDO_MAX) {
        g_undo[g_undo_n] = g_pos;
        g_undo_last[g_undo_n] = g_last_sq;
        g_hist_at_undo[g_undo_n] = g_hist_n;
        g_undo_n++;
    } else {
        for (int i = 1; i < UNDO_MAX; i++) {
            g_undo[i - 1] = g_undo[i];
            g_undo_last[i - 1] = g_undo_last[i];
            g_hist_at_undo[i - 1] = g_hist_at_undo[i];
        }
        g_undo[UNDO_MAX - 1] = g_pos;
        g_undo_last[UNDO_MAX - 1] = g_last_sq;
        g_hist_at_undo[UNDO_MAX - 1] = g_hist_n;
    }
}

static void apply_move(int sq) {
    Color who = (Color)g_pos.side;
    push_undo();
    if (!Engine::play(g_pos, sq)) {
        if (g_undo_n > 0) g_undo_n--;  // rollback push
        strncpy(g_status, "Coup illegal", sizeof(g_status));
        update_hud();
        return;
    }
    g_last_sq = sq;
    push_hist(sq, who);
    sync_stones();
    save_position();
    strncpy(g_status, sq == PASS ? "Passe" : "OK", sizeof(g_status));
    update_hud();
    after_move();
}

static void finish_score() {
    Engine::score_chinese(g_pos, KOMI, g_score);
    g_state = ST_SCORE;
    panel_on(true);
    int winner = 2;
    if (g_score.black > g_score.white) winner = 0;
    else if (g_score.white > g_score.black) winner = 1;
    record_result(winner);

    char title[48];
    if (winner == 0) snprintf(title, sizeof(title), "Noir gagne");
    else if (winner == 1) snprintf(title, sizeof(title), "Blanc gagne");
    else snprintf(title, sizeof(title), "Partie nulle");
    set_text_if(g_p_title, title);

    char sub[96];
    snprintf(sub, sizeof(sub), "N %.1f  —  B %.1f (komi %.1f)  |  terr %d/%d  dame %d",
             (double)g_score.black, (double)g_score.white, (double)KOMI,
             g_score.black_terr, g_score.white_terr, g_score.dame);
    set_text_if(g_p_sub, sub);
    slot_list(0, "Revanche", "Meme reglage", Pal::ACCENT, true);
    slot_list(1, "Hub", "Menu principal", Pal::TXT_DIM, true);
    for (int i = 2; i < N_SLOTS; i++) slot_list(i, "", "", 0, false);
}

static void after_move() {
    if (Engine::is_over(g_pos)) {
        finish_score();
        return;
    }
    if (!is_human_turn()) try_ai();
    else {
        g_state = ST_PLAYING;
        update_hud();
    }
}

static void try_ai() {
    if (Engine::is_over(g_pos)) { finish_score(); return; }
    g_state = ST_THINKING;
    strncpy(g_status, "Tab reflechit...", sizeof(g_status));
    update_hud();
    Ai::begin(g_pos, (Ai::Level)g_cfg_level);
}

static void do_pass() {
    if (g_state != ST_PLAYING || !is_human_turn()) return;
    apply_move(PASS);
}

static void do_undo() {
    if (g_state != ST_PLAYING && g_state != ST_THINKING) return;
    Ai::abort();
    if (g_undo_n <= 0) {
        strncpy(g_status, "Rien a annuler", sizeof(g_status));
        update_hud();
        return;
    }
    g_undo_n--;
    g_pos = g_undo[g_undo_n];
    g_last_sq = g_undo_last[g_undo_n];
    g_hist_n = g_hist_at_undo[g_undo_n];
    // PvT : annuler aussi la réponse IA
    if (g_cfg_mode == 0 && !is_human_turn() && g_undo_n > 0) {
        g_undo_n--;
        g_pos = g_undo[g_undo_n];
        g_last_sq = g_undo_last[g_undo_n];
        g_hist_n = g_hist_at_undo[g_undo_n];
    }
    g_state = ST_PLAYING;
    sync_stones();
    update_hist();
    save_position();
    strncpy(g_status, "Coup annule", sizeof(g_status));
    update_hud();
    after_move();
}

static void do_hint() {
    if (g_state != ST_PLAYING || !is_human_turn()) return;
    // Hint = 1-ply amateur rapide
    Ai::begin(g_pos, Ai::LVL_AMATEUR);
    // Drain synchrone court (amateur depth 1, OK < 30ms typique)
    for (int i = 0; i < 40 && !Ai::ready(); i++) Ai::step();
    if (Ai::ready()) {
        g_hint_sq = Ai::best_sq();
        if (g_hint_sq == PASS) {
            strncpy(g_status, "Indice: passer", sizeof(g_status));
            g_hint_sq = -1;
        } else {
            g_hint_until = esphome::millis() + 2500;
            strncpy(g_status, "Indice affiche", sizeof(g_status));
            sync_stones();
        }
    }
    Ai::abort();
    g_state = ST_PLAYING;
    update_hud();
}

// ===========================================================================
// Menus
// ===========================================================================

static void go_hub() {
    g_state = ST_HUB;
    Ai::abort();
    panel_on(true);
    set_text_if(g_p_title, "Go Tab");
    set_text_if(g_p_sub, "Go — score chinois, komi 6.5, ko simple");
    slot_list(0, "Nouvelle partie", "Taille / mode / niveau", Pal::ACCENT, true);
    slot_list(1, g_save.has_game ? "Reprendre" : "Reprendre (vide)",
              g_save.has_game ? "Position sauvegardee" : "Aucune partie",
              Pal::HL, g_save.has_game != 0);
    slot_list(2, "Statistiques", "W/D/L vs Tab", Pal::STONE_W, true);
    slot_list(3, "Reglages", "Secousse = Hint", Pal::TXT_DIM, true);
    slot_list(4, "Quitter", "Retour dashboard", Pal::DANGER, true);
    slot_list(5, "", "", 0, false);
}

static void go_setup() {
    g_state = ST_SETUP;
    panel_on(true);
    set_text_if(g_p_title, "Nouvelle partie");
    char buf[64];
    snprintf(buf, sizeof(buf), "%s · %s", size_name(g_cfg_size), mode_name(g_cfg_mode));
    set_text_if(g_p_sub, buf);
    slot_list(0, size_name(g_cfg_size), "9x9 / 13x13 / 19x19", Pal::WOOD, true);
    slot_list(1, mode_name(g_cfg_mode), "vs Tab / PvP / auto", Pal::HL, true);
    snprintf(buf, sizeof(buf), "Humain: %s", g_cfg_human == BLACK ? "Noir" : "Blanc");
    slot_list(2, buf, "Couleur (vs Tab)", Pal::STONE_W, g_cfg_mode == 0);
    slot_list(3, level_name(g_cfg_level), "Niveau IA", Pal::ACCENT, g_cfg_mode != 1);
    slot_list(4, "Jouer !", "Komi 6.5 — Noir commence", Pal::HL, true);
    slot_list(5, "Retour", "", Pal::TXT_DIM, true);
}

static void go_stats() {
    g_state = ST_STATS;
    panel_on(true);
    set_text_if(g_p_title, "Statistiques");
    set_text_if(g_p_sub, size_name(g_cfg_size));
    char buf[72];
    uint8_t s = g_cfg_size;
    for (int lv = 0; lv < 4; lv++) {
        snprintf(buf, sizeof(buf), "%s — %uW / %uD / %uL",
                 level_name((uint8_t)lv),
                 (unsigned)g_save.wins[s][lv],
                 (unsigned)g_save.draws[s][lv],
                 (unsigned)g_save.losses[s][lv]);
        slot_list(lv, buf, "", Pal::TXT, true);
    }
    slot_list(4, "Changer taille stats", size_name(s), Pal::HL, true);
    slot_list(5, "Retour", "", Pal::TXT_DIM, true);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    panel_on(true);
    set_text_if(g_p_title, "Reglages");
    set_text_if(g_p_sub, "Options locales NVS");
    slot_list(0, g_save.imu_hint ? "Secousse Hint: ON" : "Secousse Hint: OFF",
              "BMI270", Pal::HL, true);
    slot_list(1, "Reset statistiques", "Confirmation", Pal::DANGER, true);
    slot_list(2, "Retour", "", Pal::TXT_DIM, true);
    for (int i = 3; i < N_SLOTS; i++) slot_list(i, "", "", 0, false);
}

static void start_new() {
    Engine::pos_init(g_pos, SIZE_TAB[g_cfg_size]);
    g_last_sq = PASS;
    g_hist_n = 0;
    g_undo_n = 0;
    g_hint_sq = -1;
    strncpy(g_status, "Noir commence", sizeof(g_status));
    layout_goban();
    enter_playing();
    save_position();
}

static void resume_game() {
    if (!load_position()) { go_hub(); return; }
    g_last_sq = PASS;
    g_hist_n = 0;
    g_undo_n = 0;
    strncpy(g_status, "Partie reprise", sizeof(g_status));
    layout_goban();
    enter_playing();
}

static void enter_playing() {
    g_state = ST_PLAYING;
    panel_on(false);
    sync_stones();
    update_hist();
    update_hud();
    after_move();
}

// ===========================================================================
// Events
// ===========================================================================

static void on_board_tap(int x, int y) {
    if (g_state != ST_PLAYING || !is_human_turn()) return;
    // Snap à l'intersection la plus proche
    int best = -1;
    int best_d = 1000000;
    for (int r = 0; r < g_n; r++) {
        for (int c = 0; c < g_n; c++) {
            int ix = g_ox + c * g_gap;
            int iy = g_oy + r * g_gap;
            int dx = x - ix, dy = y - iy;
            int d = dx * dx + dy * dy;
            if (d < best_d) { best_d = d; best = Engine::idx(r, c, g_n); }
        }
    }
    int thresh = (g_gap * g_gap) / 3;
    if (best < 0 || best_d > thresh) return;
    if (!Engine::is_legal(g_pos, best)) {
        strncpy(g_status, "Illegal (ko/suicide)", sizeof(g_status));
        update_hud();
        return;
    }
    apply_move(best);
}

static void field_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_indev_t* in = lv_indev_get_act();
    if (!in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    lv_area_t a;
    lv_obj_get_coords(g_ui.field, &a);
    on_board_tap(pt.x - a.x1, pt.y - a.y1);
}

static void pass_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) do_pass();
}
static void undo_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) do_undo();
}
static void hint_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) do_hint();
}

static void side_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == 0) {  // Abandon
        int w = (g_cfg_human == BLACK) ? 1 : 0;
        if (g_cfg_mode != 0) w = (g_pos.side == BLACK) ? 1 : 0;
        // Resign = adversaire gagne
        Engine::score_chinese(g_pos, KOMI, g_score);
        record_result(w);
        g_state = ST_SCORE;
        panel_on(true);
        set_text_if(g_p_title, "Abandon");
        set_text_if(g_p_sub, w == 0 ? "Noir gagne" : "Blanc gagne");
        slot_list(0, "Revanche", "", Pal::ACCENT, true);
        slot_list(1, "Hub", "", Pal::TXT_DIM, true);
        for (int k = 2; k < N_SLOTS; k++) slot_list(k, "", "", 0, false);
    } else if (i == 1) go_setup();
    else if (i == 2) { save_position(); go_hub(); }
}

static void slot_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    switch (g_state) {
        case ST_HUB:
            if (i == 0) go_setup();
            else if (i == 1 && g_save.has_game) resume_game();
            else if (i == 2) go_stats();
            else if (i == 3) go_settings();
            else if (i == 4) close();
            break;
        case ST_SETUP:
            if (i == 0) g_cfg_size = (uint8_t)((g_cfg_size + 1) % 3);
            else if (i == 1) g_cfg_mode = (uint8_t)((g_cfg_mode + 1) % 3);
            else if (i == 2 && g_cfg_mode == 0)
                g_cfg_human = (g_cfg_human == BLACK) ? WHITE : BLACK;
            else if (i == 3 && g_cfg_mode != 1)
                g_cfg_level = (uint8_t)((g_cfg_level + 1) % 4);
            else if (i == 4) { persist_save(); start_new(); break; }
            else if (i == 5) { go_hub(); break; }
            go_setup();
            break;
        case ST_STATS:
            if (i == 4) { g_cfg_size = (uint8_t)((g_cfg_size + 1) % 3); go_stats(); }
            else if (i == 5) go_hub();
            break;
        case ST_SETTINGS:
            if (i == 0) { g_save.imu_hint ^= 1; persist_save(); go_settings(); }
            else if (i == 1) {
                g_state = ST_CONFIRM_RESET;
                set_text_if(g_p_title, "Reset stats ?");
                set_text_if(g_p_sub, "Irreversible");
                slot_list(0, "Confirmer", "", Pal::DANGER, true);
                slot_list(1, "Annuler", "", Pal::TXT_DIM, true);
                for (int k = 2; k < N_SLOTS; k++) slot_list(k, "", "", 0, false);
            } else if (i == 2) go_hub();
            break;
        case ST_CONFIRM_RESET:
            if (i == 0) {
                memset(g_save.wins, 0, sizeof(g_save.wins));
                memset(g_save.draws, 0, sizeof(g_save.draws));
                memset(g_save.losses, 0, sizeof(g_save.losses));
                persist_save();
            }
            go_settings();
            break;
        case ST_SCORE:
            if (i == 0) start_new();
            else go_hub();
            break;
        default: break;
    }
}

static void hud_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_state == ST_PLAYING || g_state == ST_THINKING) {
        save_position();
        go_hub();
    }
}

// ===========================================================================
// Build UI
// ===========================================================================

static void build_ui() {
    if (g_ui_built) return;
    g_ui_built = true;

    set_bg(g_ui.root, Pal::VOID_BG, LV_OPA_COVER);
    set_bg(g_ui.hud, Pal::HUD_BG, LV_OPA_COVER);
    set_bg(g_ui.field, Pal::FLOOR_BG, LV_OPA_COVER);
    set_bg(g_ui.panel, Pal::VOID_BG, (lv_opa_t)230);

    g_goban = mk_rect(g_ui.field);
    for (int i = 0; i < MAX_N; i++) {
        g_grid_h[i] = mk_rect(g_ui.field);
        g_grid_v[i] = mk_rect(g_ui.field);
    }
    for (int i = 0; i < 9; i++) g_hoshi[i] = mk_rect(g_ui.field);
    for (int i = 0; i < MAX_SQ; i++) {
        g_stone[i] = mk_rect(g_ui.field);
        show(g_stone[i], false);
    }
    g_last_mark = mk_rect(g_ui.field);
    g_hint_mark = mk_rect(g_ui.field);
    show(g_last_mark, false);
    show(g_hint_mark, false);

    g_side = mk_rect(g_ui.field);
    set_bg(g_side, Pal::PANEL_BG, LV_OPA_COVER);
    set_border(g_side, Pal::BTN_EDGE, 1, LV_OPA_40);

    for (int i = 0; i < 8; i++) {
        g_hist_lbl[i] = mk_label(g_side, g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(g_hist_lbl[i], 12, 12 + i * 26);
    }

    static const char* SBT[] = {"Abandon", "Nouvelle", "Menu"};
    for (int i = 0; i < 3; i++) {
        g_side_btns[i] = mk_rect(g_side);
        lv_obj_add_flag(g_side_btns[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(g_side_btns[i], 16, 250 + i * 68);
        lv_obj_set_size(g_side_btns[i], 256, 56);
        set_bg(g_side_btns[i], Pal::BTN, LV_OPA_COVER);
        set_border(g_side_btns[i], Pal::BTN_EDGE, 1, LV_OPA_60);
        lv_obj_set_style_radius(g_side_btns[i], 8, LV_PART_MAIN);
        lv_obj_t* l = mk_label(g_side_btns[i], g_ui.f_mid, Pal::TXT);
        set_text_if(l, SBT[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(g_side_btns[i], side_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    g_btn_pass = mk_rect(g_ui.field);
    lv_obj_add_flag(g_btn_pass, LV_OBJ_FLAG_CLICKABLE);
    set_bg(g_btn_pass, Pal::BTN, LV_OPA_COVER);
    set_border(g_btn_pass, Pal::ACCENT, 2, LV_OPA_60);
    lv_obj_set_style_radius(g_btn_pass, 8, LV_PART_MAIN);
    {
        lv_obj_t* l = mk_label(g_btn_pass, g_ui.f_mid, Pal::ACCENT);
        set_text_if(l, "Passer");
        lv_obj_center(l);
    }
    lv_obj_add_event_cb(g_btn_pass, pass_cb, LV_EVENT_CLICKED, nullptr);

    g_btn_undo = mk_rect(g_ui.field);
    lv_obj_add_flag(g_btn_undo, LV_OBJ_FLAG_CLICKABLE);
    set_bg(g_btn_undo, Pal::BTN, LV_OPA_COVER);
    set_border(g_btn_undo, Pal::HL, 2, LV_OPA_50);
    lv_obj_set_style_radius(g_btn_undo, 8, LV_PART_MAIN);
    {
        lv_obj_t* l = mk_label(g_btn_undo, g_ui.f_small, Pal::TXT);
        set_text_if(l, "Undo");
        lv_obj_center(l);
    }
    lv_obj_add_event_cb(g_btn_undo, undo_cb, LV_EVENT_CLICKED, nullptr);

    g_btn_hint = mk_rect(g_ui.field);
    lv_obj_add_flag(g_btn_hint, LV_OBJ_FLAG_CLICKABLE);
    set_bg(g_btn_hint, Pal::BTN, LV_OPA_COVER);
    set_border(g_btn_hint, Pal::ACCENT, 2, LV_OPA_50);
    lv_obj_set_style_radius(g_btn_hint, 8, LV_PART_MAIN);
    {
        lv_obj_t* l = mk_label(g_btn_hint, g_ui.f_small, Pal::TXT);
        set_text_if(l, "Hint");
        lv_obj_center(l);
    }
    lv_obj_add_event_cb(g_btn_hint, hint_cb, LV_EVENT_CLICKED, nullptr);

    g_hud_trait = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT);
    lv_obj_set_pos(g_hud_trait, 16, 12);
    g_hud_size = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_set_pos(g_hud_size, 280, 12);
    g_hud_lvl = mk_label(g_ui.hud, g_ui.f_small, Pal::ACCENT);
    lv_obj_set_pos(g_hud_lvl, 420, 12);
    g_hud_caps = mk_label(g_ui.hud, g_ui.f_small, Pal::HL);
    lv_obj_set_pos(g_hud_caps, 700, 12);
    g_hud_status = mk_label(g_ui.hud, g_ui.f_small, Pal::WOOD);
    lv_obj_set_pos(g_hud_status, 980, 12);

    g_p_title = mk_label(g_ui.panel, g_ui.f_big, Pal::ACCENT);
    lv_obj_set_pos(g_p_title, 80, 40);
    g_p_sub = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_set_pos(g_p_sub, 80, 100);

    for (int i = 0; i < N_SLOTS; i++) {
        g_slot[i] = mk_rect(g_ui.panel);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(g_slot[i], 80, 160 + i * 80);
        lv_obj_set_size(g_slot[i], 1120, 70);
        set_bg(g_slot[i], Pal::PANEL_BG, LV_OPA_COVER);
        lv_obj_set_style_radius(g_slot[i], 10, LV_PART_MAIN);
        g_slot_t[i] = mk_label(g_slot[i], g_ui.f_mid, Pal::TXT);
        lv_obj_set_pos(g_slot_t[i], 24, 8);
        g_slot_d[i] = mk_label(g_slot[i], g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(g_slot_d[i], 24, 40);
        lv_obj_add_event_cb(g_slot[i], slot_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

static void tick_cb(lv_timer_t*) {
    if (g_state == ST_OFF) return;
    if (g_hint_sq >= 0 && esphome::millis() >= g_hint_until) {
        g_hint_sq = -1;
        sync_stones();
    }
    if (g_state == ST_THINKING) {
        Ai::step();
        if (Ai::ready()) {
            int sq = Ai::best_sq();
            apply_move(sq);
        } else {
            update_hud();
        }
    }
}

void ai_step() {
    if (g_state == ST_THINKING) Ai::step();
}

void on_imu(float ax, float ay, float az) {
    if (ax != ax || ay != ay || az != az) return;
    float dax = ax - g_imu_ax, day = ay - g_imu_ay, daz = az - g_imu_az;
    g_imu_ax = ax; g_imu_ay = ay; g_imu_az = az;
    if (g_state != ST_PLAYING || !g_save.imu_hint) return;
    float mag = sqrtf(dax * dax + day * day + daz * daz);
    uint32_t now = esphome::millis();
    if (mag > 1.2f && (now - g_last_shake) > 800) {
        g_last_shake = now;
        do_hint();
    }
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel) return;
    g_ui = ui;
    persist_load();
    build_ui();
    Engine::pos_init(g_pos, SIZE_TAB[g_cfg_size]);
    layout_goban();
    show(g_ui.root, true);
    lv_obj_move_foreground(g_ui.root);
    lv_obj_add_flag(g_ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.hud, hud_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(g_ui.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.field, field_cb, LV_EVENT_CLICKED, nullptr);
    go_hub();
    if (!g_timer) g_timer = lv_timer_create(tick_cb, 33, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;
    Ai::abort();
    if (g_state == ST_PLAYING || g_state == ST_THINKING) save_position();
    persist_save();
    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    if (g_ui.hud) lv_obj_remove_event_cb(g_ui.hud, hud_cb);
    if (g_ui.field) lv_obj_remove_event_cb(g_ui.field, field_cb);
    show(g_ui.root, false);
    g_state = ST_OFF;
}

}  // namespace Go
