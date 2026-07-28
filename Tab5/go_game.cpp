/**
 * [AI-CONTEXT]
 * @file go_game.cpp
 * @role UI LVGL + NVS + machine à états pour « Go Tab ».
 * @architecture_constraint Widgets PRÉALLOUÉS (19×19 max) et repositionnés à
 *      chaque changement de taille : aucune création/destruction d'objet LVGL
 *      pendant une partie. Le lv_timer (25 ms) ne fait qu'UNE tranche d'IA
 *      (13 ms max) ; il n'y a jamais de recherche bloquante.
 * @architecture_constraint NVS : une partie de Go fait des centaines de coups.
 *      L'écriture flash est donc DIFFÉRÉE (drapeau `g_dirty` + fenêtre de 15 s),
 *      et forcée seulement aux moments qui comptent (menu, fin de partie,
 *      fermeture). La version précédente appelait sync() à chaque coup.
 * @ai_instruction Règles = go_engine.*, IA = go_ai.*. Ici : rendu, entrées,
 *      persistance. Ne rien remonter dans le YAML : il ne déclare que 4
 *      conteneurs vides.
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

// ===========================================================================
// 1. Constantes
// ===========================================================================

static constexpr uint32_t SAVE_MAGIC = 0x474F5434u;  // « GOT4 » — bump = reset
static constexpr uint32_t PREF_KEY   = 0x474F5442u;  // « GOTB »
static constexpr float    KOMI       = 6.5f;
static constexpr float    KOMI_HCAP  = 0.5f;  // convention handicap : komi réduit

static constexpr int HUD_H     = 60;
static constexpr int FIELD_H   = 660;
static constexpr int PANEL_X   = 966;   // dans le repère de `field`
static constexpr int PANEL_W   = 302;
static constexpr int PANEL_Y   = 6;
static constexpr int PANEL_H   = 648;

static constexpr uint32_t TICK_THINK_MS = 25;   // réflexion IA : tick rapide
static constexpr uint32_t TICK_IDLE_MS  = 50;   // menus / attente humain : tick lent
static constexpr uint32_t AI_SLICE_MS  = 13;   // ≈ 50 % du tick : l'UI reste fluide
static constexpr uint32_t MIN_THINK_MS = 150;  // délai minimal avant réponse (réactivité)
static constexpr uint32_t TVT_PAUSE_MS = 550;  // respiration entre deux coups en Tab vs Tab
static constexpr uint32_t HINT_MS      = 3000;
static constexpr uint32_t NVS_MIN_MS   = 15000;
static constexpr uint32_t MSG_MS       = 2600;

static constexpr int MOVE_ROWS = 12;
static constexpr int UNDO_MAX  = 30;
static constexpr int MV_MAX    = 512;
static constexpr int N_SLOTS   = 7;
static constexpr int CARD_LINES = 6;

static const int SIZE_TAB[GO_N_SIZES] = {9, 13, 19};

enum UiState : uint8_t {
    ST_OFF = 0, ST_MENU_MAIN, ST_MENU_SETUP, ST_MENU_STATS, ST_MENU_OPTS,
    ST_CONFIRM_RESET, ST_PAUSE, ST_PLAYING, ST_THINKING, ST_MARKING, ST_SCORE
};

// ===========================================================================
// 2. État
// ===========================================================================

static UI          g_ui;
static UiState     g_state = ST_OFF;
static lv_timer_t* g_timer = nullptr;
static GoSave      g_save{};
static esphome::ESPPreferenceObject g_pref;
static bool g_pref_ready = false;
static bool g_ui_built   = false;

static Pos  g_pos;
static bool g_in_game = false;     // une partie est en cours (menu « Reprendre »)
static int  g_last_sq = PASS;
static int  g_pending = -1;        // coup en attente de validation (opt_confirm)
static int  g_hint_sq = -1;
static uint32_t g_hint_until = 0;

static uint8_t g_dead[MAX_SQ];     // 1 = groupe marqué mort (écran de marquage)
static uint8_t g_terr[MAX_SQ];     // carte de territoire (Engine::Terr)
static Engine::Score g_score{};
static int  g_winner = 2;          // 0 = Noir, 1 = Blanc, 2 = nulle
static bool g_resigned = false;    // la partie s'est terminée par un abandon

// Réglages de la partie à créer (copie de travail des réglages NVS).
static uint8_t g_cfg_size  = 0;
static uint8_t g_cfg_mode  = 0;
static uint8_t g_cfg_human = BLACK;
static uint8_t g_cfg_level = 1;
static uint8_t g_cfg_hcap  = 0;
static uint8_t g_stats_size = 0;   // taille affichée dans l'écran statistiques

// Historique des coups (pour la liste) et pile d'annulation.
static int16_t g_mv[MV_MAX];
static int     g_mv_n = 0;
static uint8_t g_first_color = BLACK;

static Pos g_undo[UNDO_MAX];
static int g_undo_last[UNDO_MAX];
static int g_undo_mv[UNDO_MAX];
static int g_undo_n = 0;

// Cadence IA / temps de jeu.
static uint32_t g_think_t0  = 0;
static uint32_t g_next_move = 0;
static uint32_t g_game_t0   = 0;
static uint32_t g_last_nvs  = 0;
static bool     g_dirty     = false;

static char     g_msg[56] = "";
static uint32_t g_msg_until = 0;

// Cache de rendu : n'invalider LVGL que sur les intersections qui changent.
static uint8_t g_vis_col[MAX_SQ];    // dernière couleur peinte (EMPTY/BLACK/WHITE)
static uint8_t g_vis_mode[MAX_SQ];   // 0=caché, 1=pierre, 2=pierre morte, 3=terr
static bool    g_vis_dirty = true;   // true → redraw intégral (changement de taille)
static int     g_vis_last_sq = -999;
static int     g_vis_pending = -999;
static int     g_vis_hint_sq = -999;
static int     g_think_pct_drawn = -1;

// IMU
static float g_ax = 0, g_ay = 0, g_az = 1;
static uint32_t g_last_shake = 0;

// Géométrie du goban (recalculée à chaque changement de taille).
static int g_n = 9;
static int g_gap = 64;
static int g_ox = 0, g_oy = 0;
static int g_stone_r = 28;
static int g_plate_x = 0, g_plate_y = 0, g_plate_sz = 0;

// ===========================================================================
// 3. Widgets
// ===========================================================================

// --- Aire de jeu ---
static lv_obj_t* g_plate = nullptr;
static lv_obj_t* g_grid_h[MAX_N] = {};
static lv_obj_t* g_grid_v[MAX_N] = {};
static lv_obj_t* g_hoshi[9] = {};
static lv_obj_t* g_coord_c[MAX_N] = {};    // lettres (sous le plateau)
static lv_obj_t* g_coord_r[MAX_N] = {};    // chiffres (à gauche)
static lv_obj_t* g_stone[MAX_SQ] = {};     // pierre OU marque de territoire
static lv_obj_t* g_mark_last = nullptr;
static lv_obj_t* g_mark_ghost = nullptr;
static lv_obj_t* g_mark_hint = nullptr;

// --- HUD ---
static lv_obj_t* g_pill[2] = {};           // 0 = Noir, 1 = Blanc
static lv_obj_t* g_pill_dot[2] = {};
static lv_obj_t* g_pill_name[2] = {};
static lv_obj_t* g_pill_sub[2] = {};
static lv_obj_t* g_h_move = nullptr;
static lv_obj_t* g_h_status = nullptr;

// --- Panneau latéral ---
static lv_obj_t* g_panel = nullptr;
static lv_obj_t* g_p_title = nullptr;
static lv_obj_t* g_p_head[2] = {};
static lv_obj_t* g_ml_num[MOVE_ROWS] = {};
static lv_obj_t* g_ml_a[MOVE_ROWS] = {};
static lv_obj_t* g_ml_b[MOVE_ROWS] = {};
static lv_obj_t* g_think_lbl = nullptr;
static lv_obj_t* g_think_bar = nullptr;
static lv_obj_t* g_think_fill = nullptr;
static lv_obj_t* g_btn_ok = nullptr;       // « Valider » (coup en attente / marquage)
static lv_obj_t* g_btn_ok_lbl = nullptr;
static lv_obj_t* g_btn[4] = {};
static lv_obj_t* g_btn_lbl[4] = {};

// --- Calque des menus ---
static lv_obj_t* g_m_title = nullptr;
static lv_obj_t* g_m_sub = nullptr;
static lv_obj_t* g_m_foot = nullptr;
static lv_obj_t* g_slot[N_SLOTS] = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
static lv_obj_t* g_slot_d[N_SLOTS] = {};
// Carte de fin de partie (géométrie propre : le goban reste visible autour).
static lv_obj_t* g_card = nullptr;
static lv_obj_t* g_card_title = nullptr;
static lv_obj_t* g_card_sub = nullptr;
static lv_obj_t* g_card_line[CARD_LINES] = {};
static lv_obj_t* g_card_btn[2] = {};
static lv_obj_t* g_card_btn_lbl[2] = {};

// ===========================================================================
// 4. Helpers LVGL
// ===========================================================================

static inline void show(lv_obj_t* o, bool v) {
    if (!o) return;
    if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else   lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static inline void set_bg(lv_obj_t* o, uint32_t c, lv_opa_t opa) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, LV_PART_MAIN);
}
// Dégradé vertical : c'est lui qui donne du relief aux pierres et au plateau
// sans coûter un seul objet LVGL supplémentaire.
static inline void set_bg_grad(lv_obj_t* o, uint32_t top, uint32_t bottom, lv_opa_t opa) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(top), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(bottom), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
}
static inline void set_border(lv_obj_t* o, uint32_t c, int w, lv_opa_t opa) {
    if (!o) return;
    lv_obj_set_style_border_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, w, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, opa, LV_PART_MAIN);
}
static inline void set_color(lv_obj_t* l, uint32_t c) {
    if (l) lv_obj_set_style_text_color(l, lv_color_hex(c), LV_PART_MAIN);
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
// N'écrit que si le texte change : évite des invalidations LVGL inutiles.
static void set_text_if(lv_obj_t* l, const char* t) {
    if (!l || !t) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, t) == 0) return;
    lv_label_set_text(l, t);
}
static void press_fx(lv_obj_t* o, uint32_t c) {
    lv_obj_set_style_bg_color(o, lv_color_hex(c),
                              (lv_style_selector_t) LV_PART_MAIN |
                              (lv_style_selector_t) LV_STATE_PRESSED);
}
static void msg(const char* t) {
    snprintf(g_msg, sizeof(g_msg), "%s", t);
    g_msg_until = esphome::millis() + MSG_MS;
}

// Komi effectif : 0,5 en partie à handicap (convention), 6,5 sinon.
static float effective_komi() {
    return (g_cfg_hcap >= 2) ? KOMI_HCAP : KOMI;
}

// ===========================================================================
// 5. Libellés
// ===========================================================================

static const char* size_name(uint8_t i) {
    switch (i) { case 0: return "9x9"; case 1: return "13x13"; case 2: return "19x19"; }
    return "?";
}
static const char* mode_name(uint8_t m) {
    switch (m) {
        case 0: return "Joueur contre Tab";
        case 1: return "Joueur contre joueur";
        case 2: return "Tab contre Tab";
    }
    return "?";
}
static const char* level_name(uint8_t lv) { return Ai::level_name((Ai::Level) lv); }

// Coordonnées de Go : colonnes A.. sans le I, rangées n..1 depuis le haut.
static void sq_name(int sq, int n, char* buf, int len) {
    if (sq == PASS || sq < 0) { snprintf(buf, len, "passe"); return; }
    const int r = sq / n, c = sq % n;
    char col = (char) ('A' + c);
    if (col >= 'I') col++;
    snprintf(buf, len, "%c%d", col, n - r);
}
static void col_letter(int c, char* buf, int len) {
    char col = (char) ('A' + c);
    if (col >= 'I') col++;
    snprintf(buf, len, "%c", col);
}

// ===========================================================================
// 6. NVS
// ===========================================================================

static void save_defaults() {
    memset(&g_save, 0, sizeof(g_save));
    g_save.magic = SAVE_MAGIC;
    g_save.size_idx = 0;
    g_save.mode = 0;
    g_save.human_color = BLACK;
    g_save.ai_level = Ai::LVL_AMATEUR;
    g_save.handicap = 0;
    g_save.opt_confirm = 1;
    g_save.opt_coords = 1;
    g_save.opt_shake = 1;
    g_save.opt_lastmark = 1;
    g_save.opt_terr = 1;
}

void persist_load() {
    if (!g_pref_ready) {
        g_pref = esphome::global_preferences->make_preference<GoSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != SAVE_MAGIC) save_defaults();
    if (g_save.size_idx >= GO_N_SIZES) g_save.size_idx = 0;
    if (g_save.mode > 2) g_save.mode = 0;
    if (g_save.ai_level >= GO_N_LEVELS) g_save.ai_level = Ai::LVL_AMATEUR;
    if (g_save.handicap == 1 || g_save.handicap > 9) g_save.handicap = 0;
    if (g_save.human_color != WHITE) g_save.human_color = BLACK;

    g_cfg_size  = g_save.size_idx;
    g_cfg_mode  = g_save.mode;
    g_cfg_human = g_save.human_color;
    g_cfg_level = g_save.ai_level;
    g_cfg_hcap  = g_save.handicap;
    g_stats_size = g_cfg_size;
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = SAVE_MAGIC;
    g_save.size_idx = g_cfg_size;
    g_save.mode = g_cfg_mode;
    g_save.human_color = g_cfg_human;
    g_save.ai_level = g_cfg_level;
    g_save.handicap = g_cfg_hcap;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
    g_dirty = false;
    g_last_nvs = esphome::millis();
}

// Recopie les réglages de travail dans la sauvegarde EN RAM, sans toucher au
// flash : parcourir le menu de configuration ne doit pas écrire 10 fois en NVS.
static void stash_settings() {
    g_save.size_idx = g_cfg_size;
    g_save.mode = g_cfg_mode;
    g_save.human_color = g_cfg_human;
    g_save.ai_level = g_cfg_level;
    g_save.handicap = g_cfg_hcap;
    g_dirty = true;
}

// Copie la position courante dans la sauvegarde (RAM) — l'écriture flash, elle,
// est différée par flush().
static void stash_position() {
    g_save.has_game = g_in_game ? 1 : 0;
    g_save.n = g_pos.n;
    g_save.side = g_pos.side;
    g_save.ko = g_pos.ko;
    g_save.passes = g_pos.passes;
    g_save.move_no = g_pos.move_no;
    g_save.cap_b = g_pos.captured_by_black;
    g_save.cap_w = g_pos.captured_by_white;
    g_save.r_size = g_cfg_size;
    g_save.r_mode = g_cfg_mode;
    g_save.r_human = g_cfg_human;
    g_save.r_level = g_cfg_level;
    g_save.r_handicap = g_cfg_hcap;
    memset(g_save.board, 0, sizeof(g_save.board));
    memcpy(g_save.board, g_pos.sq, (size_t)(g_pos.n * g_pos.n));
    g_dirty = true;
}

static void flush(bool force) {
    if (!g_dirty) return;
    const uint32_t now = esphome::millis();
    if (!force && (now - g_last_nvs) < NVS_MIN_MS) return;
    persist_save();
}

static bool restore_position() {
    if (!g_save.has_game) return false;
    const int n = g_save.n;
    if (n != 9 && n != 13 && n != 19) return false;
    Engine::pos_init(g_pos, n);
    g_pos.side = g_save.side;
    g_pos.ko = g_save.ko;
    g_pos.passes = g_save.passes;
    g_pos.move_no = g_save.move_no;
    g_pos.captured_by_black = g_save.cap_b;
    g_pos.captured_by_white = g_save.cap_w;
    memcpy(g_pos.sq, g_save.board, (size_t)(n * n));
    g_cfg_size  = g_save.r_size < GO_N_SIZES ? g_save.r_size : 0;
    g_cfg_mode  = g_save.r_mode <= 2 ? g_save.r_mode : 0;
    g_cfg_human = g_save.r_human == WHITE ? WHITE : BLACK;
    g_cfg_level = g_save.r_level < GO_N_LEVELS ? g_save.r_level : 1;
    g_cfg_hcap  = (g_save.r_handicap == 1 || g_save.r_handicap > 9) ? 0 : g_save.r_handicap;
    return true;
}

static void record_result(int winner) {
    g_save.games++;
    if (g_cfg_mode == 0 && g_cfg_size < GO_N_SIZES && g_cfg_level < GO_N_LEVELS) {
        const uint8_t s = g_cfg_size, lv = g_cfg_level;
        if (winner == 2) g_save.draws[s][lv]++;
        else {
            const bool human_black = (g_cfg_human == BLACK);
            const bool human_won = human_black ? (winner == 0) : (winner == 1);
            if (human_won) g_save.wins[s][lv]++;
            else g_save.losses[s][lv]++;
        }
    }
    if (g_game_t0) { g_save.total_ms += esphome::millis() - g_game_t0; g_game_t0 = 0; }
    g_in_game = false;
    g_save.has_game = 0;
    g_dirty = true;
}

// ===========================================================================
// 7. Déclarations avancées
// ===========================================================================

static void menu_main();
static void menu_setup();
static void menu_stats();
static void menu_opts();
static void menu_pause();
static void show_score_card();
static void start_new();
static void enter_playing();
static void after_move();
static void begin_thinking();
static void apply_move(int sq);
static void do_pass();
static void do_undo();
static void do_hint();
static void enter_marking();
static void finish_scoring();
static void render_board();
static void render_hud();
static void render_movelist();
static void render_panel_buttons();
static void layout_board();

// ===========================================================================
// 8. Géométrie
// ===========================================================================

static void compute_geometry() {
    g_n = g_pos.n;
    const int zone_w = PANEL_X - 12;
    const int zone_h = FIELD_H - 8;
    int m = zone_w < zone_h ? zone_w : zone_h;
    g_gap = m / (g_n + 1);
    if (g_gap > 68) g_gap = 68;
    if (g_gap < 20) g_gap = 20;
    g_stone_r = (g_gap * 45) / 100;
    if (g_stone_r < 6) g_stone_r = 6;
    g_plate_sz = g_gap * (g_n + 1);
    g_plate_x = (PANEL_X - g_plate_sz) / 2;
    g_plate_y = (FIELD_H - g_plate_sz) / 2;
    if (g_plate_y < 4) g_plate_y = 4;
    g_ox = g_plate_x + g_gap;
    g_oy = g_plate_y + g_gap;
}

static void layout_board() {
    compute_geometry();
    g_vis_dirty = true;
    g_think_pct_drawn = -1;
    const int span = g_gap * (g_n - 1);

    lv_obj_set_pos(g_plate, g_plate_x, g_plate_y);
    lv_obj_set_size(g_plate, g_plate_sz, g_plate_sz);

    for (int i = 0; i < MAX_N; i++) {
        const bool use = i < g_n;
        show(g_grid_h[i], use);
        show(g_grid_v[i], use);
        if (!use) continue;
        // Les deux lignes extrêmes sont plus épaisses : c'est ce qui donne au
        // goban son cadre net, comme sur un vrai plateau.
        const int w = (i == 0 || i == g_n - 1) ? 2 : 1;
        lv_obj_set_pos(g_grid_h[i], g_ox, g_oy + i * g_gap - w / 2);
        lv_obj_set_size(g_grid_h[i], span + 1, w);
        lv_obj_set_pos(g_grid_v[i], g_ox + i * g_gap - w / 2, g_oy);
        lv_obj_set_size(g_grid_v[i], w, span + 1);
    }

    // Points étoiles.
    int stars[9][2];
    int ns;
    if (g_n == 9) {
        const int s[5][2] = {{2,2},{2,6},{6,2},{6,6},{4,4}};
        ns = 5; for (int i = 0; i < ns; i++) { stars[i][0]=s[i][0]; stars[i][1]=s[i][1]; }
    } else if (g_n == 13) {
        const int s[5][2] = {{3,3},{3,9},{9,3},{9,9},{6,6}};
        ns = 5; for (int i = 0; i < ns; i++) { stars[i][0]=s[i][0]; stars[i][1]=s[i][1]; }
    } else {
        const int s[9][2] = {{3,3},{3,9},{3,15},{9,3},{9,9},{9,15},{15,3},{15,9},{15,15}};
        ns = 9; for (int i = 0; i < ns; i++) { stars[i][0]=s[i][0]; stars[i][1]=s[i][1]; }
    }
    const int hd = g_gap >= 46 ? 10 : (g_gap >= 32 ? 8 : 6);
    for (int i = 0; i < 9; i++) {
        const bool use = i < ns;
        show(g_hoshi[i], use);
        if (!use) continue;
        lv_obj_set_pos(g_hoshi[i], g_ox + stars[i][1] * g_gap - hd / 2,
                                   g_oy + stars[i][0] * g_gap - hd / 2);
        lv_obj_set_size(g_hoshi[i], hd, hd);
        lv_obj_set_style_radius(g_hoshi[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }

    // Coordonnées : lettres sous le plateau, chiffres à gauche.
    char buf[4];
    const bool co = g_save.opt_coords != 0;
    for (int i = 0; i < MAX_N; i++) {
        const bool use = co && i < g_n;
        show(g_coord_c[i], use);
        show(g_coord_r[i], use);
        if (!use) continue;
        col_letter(i, buf, sizeof(buf));
        set_text_if(g_coord_c[i], buf);
        lv_obj_set_width(g_coord_c[i], g_gap);
        lv_obj_set_pos(g_coord_c[i], g_ox + i * g_gap - g_gap / 2,
                                     g_oy + span + (g_gap - 22) / 2);
        snprintf(buf, sizeof(buf), "%d", g_n - i);
        set_text_if(g_coord_r[i], buf);
        lv_obj_set_width(g_coord_r[i], g_gap - 10);
        lv_obj_set_pos(g_coord_r[i], g_plate_x + 4, g_oy + i * g_gap - 11);
    }
}

// ===========================================================================
// 9. Rendu du plateau
// ===========================================================================

static void style_stone(lv_obj_t* o, bool black, int r, lv_opa_t opa, bool dead) {
    lv_obj_set_size(o, 2 * r, 2 * r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    if (black) set_bg_grad(o, Pal::STONE_B_H, Pal::STONE_B, opa);
    else       set_bg_grad(o, Pal::STONE_W_H, Pal::STONE_W, opa);
    if (dead)          set_border(o, Pal::DEADMARK, 3, LV_OPA_COVER);
    else if (black)    set_border(o, 0x000000, 0, LV_OPA_TRANSP);
    else               set_border(o, Pal::STONE_W_E, 1, LV_OPA_60);
}

static void render_board() {
    const int n = g_pos.n;
    const int N = n * n;
    const bool scoring = (g_state == ST_MARKING || g_state == ST_SCORE);
    const bool terr_on = scoring && g_save.opt_terr;
    const int td = g_gap / 3 < 6 ? 6 : g_gap / 3;
    const bool force = g_vis_dirty;

    for (int i = 0; i < MAX_SQ; i++) {
        lv_obj_t* o = g_stone[i];
        if (i >= N) {
            if (force || g_vis_mode[i] != 0) {
                show(o, false);
                g_vis_mode[i] = 0;
                g_vis_col[i] = EMPTY;
            }
            continue;
        }
        const int r = i / n, c = i % n;
        const int cx = g_ox + c * g_gap, cy = g_oy + r * g_gap;
        const uint8_t col = g_pos.sq[i];

        if (col == EMPTY) {
            if (terr_on && (g_terr[i] == Engine::T_BLACK || g_terr[i] == Engine::T_WHITE)) {
                const uint8_t want_col = g_terr[i];
                if (force || g_vis_mode[i] != 3 || g_vis_col[i] != want_col) {
                    lv_obj_set_pos(o, cx - td / 2, cy - td / 2);
                    lv_obj_set_size(o, td, td);
                    lv_obj_set_style_radius(o, 3, LV_PART_MAIN);
                    set_bg(o, want_col == Engine::T_BLACK ? Pal::TERR_B : Pal::TERR_W,
                           (lv_opa_t) 220);
                    set_border(o, 0x000000, 0, LV_OPA_TRANSP);
                    show(o, true);
                    g_vis_mode[i] = 3;
                    g_vis_col[i] = want_col;
                }
            } else if (force || g_vis_mode[i] != 0) {
                show(o, false);
                g_vis_mode[i] = 0;
                g_vis_col[i] = EMPTY;
            }
            continue;
        }
        const bool dead = scoring && g_dead[i];
        const uint8_t mode = dead ? 2 : 1;
        if (force || g_vis_mode[i] != mode || g_vis_col[i] != col) {
            lv_obj_set_pos(o, cx - g_stone_r, cy - g_stone_r);
            style_stone(o, col == BLACK, g_stone_r,
                        dead ? (lv_opa_t) 70 : (lv_opa_t) LV_OPA_COVER, dead);
            show(o, true);
            g_vis_mode[i] = mode;
            g_vis_col[i] = col;
        }
    }

    // Dernier coup.
    const bool last_ok = g_save.opt_lastmark && g_last_sq != PASS && g_last_sq >= 0 &&
                         g_last_sq < N && g_pos.sq[g_last_sq] != EMPTY && !scoring;
    if (last_ok) {
        if (force || g_vis_last_sq != g_last_sq) {
            const int r = g_last_sq / n, c = g_last_sq % n;
            const int d = g_stone_r;
            lv_obj_set_pos(g_mark_last, g_ox + c * g_gap - d / 2, g_oy + r * g_gap - d / 2);
            lv_obj_set_size(g_mark_last, d, d);
            lv_obj_set_style_radius(g_mark_last, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(g_mark_last, LV_OPA_TRANSP, LV_PART_MAIN);
            set_border(g_mark_last, Pal::LAST, 3, LV_OPA_COVER);
            show(g_mark_last, true);
            g_vis_last_sq = g_last_sq;
        }
    } else if (force || g_vis_last_sq != -1) {
        show(g_mark_last, false);
        g_vis_last_sq = -1;
    }

    // Coup en attente de validation.
    if (g_pending >= 0 && g_pending < N && g_state == ST_PLAYING) {
        if (force || g_vis_pending != g_pending || g_vis_pending == -999) {
            const int r = g_pending / n, c = g_pending % n;
            lv_obj_set_pos(g_mark_ghost, g_ox + c * g_gap - g_stone_r,
                                         g_oy + r * g_gap - g_stone_r);
            style_stone(g_mark_ghost, g_pos.side == BLACK, g_stone_r, (lv_opa_t) 130, false);
            set_border(g_mark_ghost, Pal::GHOST, 3, LV_OPA_COVER);
            show(g_mark_ghost, true);
            g_vis_pending = g_pending;
        }
    } else if (force || g_vis_pending != -1) {
        show(g_mark_ghost, false);
        g_vis_pending = -1;
    }

    // Indice.
    if (g_hint_sq >= 0 && g_hint_sq < N && esphome::millis() < g_hint_until) {
        if (force || g_vis_hint_sq != g_hint_sq) {
            const int r = g_hint_sq / n, c = g_hint_sq % n;
            const int d = g_stone_r + 4;
            lv_obj_set_pos(g_mark_hint, g_ox + c * g_gap - d, g_oy + r * g_gap - d);
            lv_obj_set_size(g_mark_hint, 2 * d, 2 * d);
            lv_obj_set_style_radius(g_mark_hint, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            set_bg(g_mark_hint, Pal::HINT, (lv_opa_t) 60);
            set_border(g_mark_hint, Pal::HINT, 3, LV_OPA_COVER);
            show(g_mark_hint, true);
            g_vis_hint_sq = g_hint_sq;
        }
    } else if (force || g_vis_hint_sq != -1) {
        show(g_mark_hint, false);
        g_vis_hint_sq = -1;
    }

    g_vis_dirty = false;
}

// ===========================================================================
// 10. Rendu du HUD et du panneau
// ===========================================================================

// Qui tient les pierres de cette couleur ?
static const char* seat_name(uint8_t color, char* buf, int len) {
    if (g_cfg_mode == 1) { snprintf(buf, len, "Joueur %s", color == BLACK ? "1" : "2"); return buf; }
    if (g_cfg_mode == 2) { snprintf(buf, len, "Tab %s", level_name(g_cfg_level)); return buf; }
    if (color == g_cfg_human) { snprintf(buf, len, "Vous"); return buf; }
    snprintf(buf, len, "Tab %s", level_name(g_cfg_level));
    return buf;
}

static void render_hud() {
    char buf[64];
    const bool playing = (g_state == ST_PLAYING || g_state == ST_THINKING);

    for (int i = 0; i < 2; i++) {
        const uint8_t col = (i == 0) ? BLACK : WHITE;
        const bool active = playing && g_pos.side == col;
        set_bg(g_pill[i], active ? Pal::CARD_ON : Pal::CARD_BG, LV_OPA_COVER);
        set_border(g_pill[i], active ? Pal::ACCENT : Pal::EDGE, 2,
                   active ? LV_OPA_COVER : LV_OPA_40);
        set_text_if(g_pill_name[i], seat_name(col, buf, sizeof(buf)));
        const int caps = (col == BLACK) ? (int) g_pos.captured_by_black
                                        : (int) g_pos.captured_by_white;
        snprintf(buf, sizeof(buf), "%s  ·  %d prise%s",
                 col == BLACK ? "Noir" : "Blanc", caps, caps > 1 ? "s" : "");
        set_text_if(g_pill_sub[i], buf);
        set_color(g_pill_name[i], active ? Pal::ACCENT : Pal::TXT);
    }

    if (g_state == ST_MARKING || g_state == ST_SCORE) {
        // Comptage en direct : le joueur voit l'effet de chaque groupe marqué.
        snprintf(buf, sizeof(buf), "Noir %.1f   contre   Blanc %.1f",
                 (double) g_score.black, (double) g_score.white);
    } else if (g_pos.move_no > 0) {
        snprintf(buf, sizeof(buf), "Coup %u  ·  komi %.1f",
                 (unsigned) g_pos.move_no, (double) effective_komi());
    } else {
        snprintf(buf, sizeof(buf), "%s  ·  komi %.1f", size_name(g_cfg_size),
                 (double) effective_komi());
    }
    set_text_if(g_h_move, buf);

    char line[64];
    const char* st = "";
    uint32_t stc = Pal::TXT_DIM;
    if (g_msg_until && (int32_t)(esphome::millis() - g_msg_until) < 0) {
        st = g_msg; stc = Pal::ACCENT;
    } else if (g_state == ST_THINKING) {
        st = "Le Tab reflechit..."; stc = Pal::THINK;
    } else if (g_state == ST_MARKING) {
        st = "Touchez les groupes MORTS, puis Valider"; stc = Pal::ACCENT;
    } else if (g_state == ST_PLAYING) {
        if (g_pending >= 0) {
            char nm[8];
            sq_name(g_pending, g_pos.n, nm, sizeof(nm));
            snprintf(line, sizeof(line), "Touchez a nouveau %s pour valider", nm);
            st = line; stc = Pal::GHOST;
        } else {
            st = (g_pos.side == BLACK) ? "Au tour de Noir" : "Au tour de Blanc";
            stc = Pal::TXT_DIM;
        }
    }
    set_text_if(g_h_status, st);
    set_color(g_h_status, stc);
}

static void render_movelist() {
    char buf[16];
    const int rows = (g_mv_n + 1) / 2;
    int start = rows - MOVE_ROWS;
    if (start < 0) start = 0;

    set_text_if(g_p_head[0], g_first_color == BLACK ? "Noir" : "Blanc");
    set_text_if(g_p_head[1], g_first_color == BLACK ? "Blanc" : "Noir");

    for (int i = 0; i < MOVE_ROWS; i++) {
        const int row = start + i;
        if (row >= rows) {
            set_text_if(g_ml_num[i], "");
            set_text_if(g_ml_a[i], "");
            set_text_if(g_ml_b[i], "");
            continue;
        }
        snprintf(buf, sizeof(buf), "%d.", row + 1);
        set_text_if(g_ml_num[i], buf);
        sq_name(g_mv[row * 2], g_pos.n, buf, sizeof(buf));
        set_text_if(g_ml_a[i], buf);
        if (row * 2 + 1 < g_mv_n) {
            sq_name(g_mv[row * 2 + 1], g_pos.n, buf, sizeof(buf));
            set_text_if(g_ml_b[i], buf);
        } else {
            set_text_if(g_ml_b[i], "");
        }
    }
}

// Les 4 boutons changent de rôle selon l'état — un seul jeu de widgets.
static void render_panel_buttons() {
    static const char* PLAY_LBL[4] = {"Passer", "Annuler", "Indice", "Menu"};
    static const char* MARK_LBL[4] = {"Tout vivant", "Reprendre", "", "Menu"};
    const bool marking = (g_state == ST_MARKING);
    const char* const* L = marking ? MARK_LBL : PLAY_LBL;

    for (int i = 0; i < 4; i++) {
        const bool on = L[i][0] != 0;
        show(g_btn[i], on);
        if (!on) continue;
        set_text_if(g_btn_lbl[i], L[i]);
    }
    // Bouton large : valider le coup en attente, ou valider le score.
    if (marking) {
        set_text_if(g_btn_ok_lbl, "Valider le score");
        set_bg(g_btn_ok, Pal::CARD_BG, LV_OPA_COVER);
        set_border(g_btn_ok, Pal::GOOD, 2, LV_OPA_80);
        set_color(g_btn_ok_lbl, Pal::GOOD);
        show(g_btn_ok, true);
    } else if (g_state == ST_PLAYING && g_pending >= 0) {
        char nm[8], buf[24];
        sq_name(g_pending, g_pos.n, nm, sizeof(nm));
        snprintf(buf, sizeof(buf), "Jouer %s", nm);
        set_text_if(g_btn_ok_lbl, buf);
        set_bg(g_btn_ok, Pal::CARD_BG, LV_OPA_COVER);
        set_border(g_btn_ok, Pal::GHOST, 2, LV_OPA_COVER);
        set_color(g_btn_ok_lbl, Pal::GHOST);
        show(g_btn_ok, true);
    } else {
        show(g_btn_ok, false);
    }

    const bool think = (g_state == ST_THINKING);
    show(g_think_lbl, think);
    show(g_think_bar, think);
    if (think) {
        const int pct = Ai::progress_pct();
        if (pct != g_think_pct_drawn) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Reflexion  %d %%", pct);
            set_text_if(g_think_lbl, buf);
            const int w = (270 * pct) / 100;
            lv_obj_set_size(g_think_fill, w < 2 ? 2 : w, 8);
            g_think_pct_drawn = pct;
        }
    } else {
        g_think_pct_drawn = -1;
    }
}

static void refresh_all() {
    render_board();
    render_hud();
    render_movelist();
    render_panel_buttons();
}

// ===========================================================================
// 11. Déroulement de la partie
// ===========================================================================

static bool is_human_turn() {
    if (g_cfg_mode == 1) return true;
    if (g_cfg_mode == 2) return false;
    return g_pos.side == g_cfg_human;
}

static void push_undo() {
    if (g_undo_n >= UNDO_MAX) {
        for (int i = 1; i < UNDO_MAX; i++) {
            g_undo[i - 1] = g_undo[i];
            g_undo_last[i - 1] = g_undo_last[i];
            g_undo_mv[i - 1] = g_undo_mv[i];
        }
        g_undo_n = UNDO_MAX - 1;
    }
    g_undo[g_undo_n] = g_pos;
    g_undo_last[g_undo_n] = g_last_sq;
    g_undo_mv[g_undo_n] = g_mv_n;
    g_undo_n++;
}

static void push_move(int sq) {
    if (g_mv_n < MV_MAX) g_mv[g_mv_n++] = (int16_t) sq;
}

static void apply_move(int sq) {
    const bool by_human = is_human_turn();
    push_undo();
    int played = sq;
    if (!Engine::play(g_pos, sq)) {
        g_undo_n--;                       // rien n'a bougé : on défait la pile
        if (by_human) { msg("Coup illegal"); refresh_all(); return; }
        // Un coup illégal proposé par l'IA ne doit JAMAIS bloquer la partie :
        // on passe à sa place et la partie continue.
        msg("Le Tab passe");
        push_undo();
        Engine::play(g_pos, PASS);
        played = PASS;
    }
    push_move(played);
    g_last_sq = played;
    g_pending = -1;
    g_hint_sq = -1;
    stash_position();
    refresh_all();
    after_move();
}

static void after_move() {
    if (Engine::is_over(g_pos)) { enter_marking(); return; }
    if (is_human_turn()) {
        g_state = ST_PLAYING;
        refresh_all();
    } else {
        begin_thinking();
    }
}

static void begin_thinking() {
    if (Engine::is_over(g_pos)) { enter_marking(); return; }
    g_state = ST_THINKING;
    g_think_t0 = esphome::millis();
    g_think_pct_drawn = -1;
    Ai::begin(g_pos, (Ai::Level) g_cfg_level,
              (uint32_t) g_pos.move_no * 2654435761u ^ esphome::millis(),
              effective_komi());
    refresh_all();
}

static void do_pass() {
    if (g_state != ST_PLAYING || !is_human_turn()) return;
    g_pending = -1;
    msg(g_pos.passes == 1 ? "Passe — fin de partie" : "Passe");
    apply_move(PASS);
}

static void do_undo() {
    if (g_state != ST_PLAYING && g_state != ST_THINKING && g_state != ST_MARKING) return;
    Ai::abort();
    g_pending = -1;
    // Depuis l'écran de marquage, « Reprendre » revient au coup d'avant la
    // seconde passe : on efface aussi les marques de groupes morts.
    if (g_state == ST_MARKING) memset(g_dead, 0, sizeof(g_dead));
    if (g_undo_n <= 0) { g_state = ST_PLAYING; msg("Rien a annuler"); refresh_all(); return; }
    g_undo_n--;
    g_pos = g_undo[g_undo_n];
    g_last_sq = g_undo_last[g_undo_n];
    g_mv_n = g_undo_mv[g_undo_n];
    // Contre le Tab, on remonte aussi sa réponse : sinon « Annuler » ne rend pas
    // la main au joueur.
    if (g_cfg_mode == 0 && g_pos.side != g_cfg_human && g_undo_n > 0) {
        g_undo_n--;
        g_pos = g_undo[g_undo_n];
        g_last_sq = g_undo_last[g_undo_n];
        g_mv_n = g_undo_mv[g_undo_n];
    }
    g_state = ST_PLAYING;
    msg("Coup annule");
    stash_position();
    refresh_all();
    after_move();
}

static void do_hint() {
    if (g_state != ST_PLAYING || !is_human_turn()) return;
    // Recherche courte et SYNCHRONE, mais bornée : 4 tranches de 8 ms au pire.
    Ai::begin(g_pos, Ai::LVL_SOLID, esphome::millis(), effective_komi());
    for (int i = 0; i < 4 && !Ai::ready(); i++) Ai::step(8);
    const int sq = Ai::best_sq();
    Ai::abort();
    g_state = ST_PLAYING;
    if (sq == Ai::RESIGN || sq == PASS || sq < 0) {
        g_hint_sq = -1;
        msg("Indice : passer");
    } else {
        g_hint_sq = sq;
        g_hint_until = esphome::millis() + HINT_MS;
        char nm[8], buf[32];
        sq_name(sq, g_pos.n, nm, sizeof(nm));
        snprintf(buf, sizeof(buf), "Indice : %s", nm);
        msg(buf);
    }
    refresh_all();
}

// --- Fin de partie : marquage des pierres mortes --------------------------

static void refresh_territory() {
    Engine::territory_map(g_pos, g_dead, g_terr);
    Engine::score_chinese(g_pos, effective_komi(), g_dead, g_score);
    g_vis_dirty = true;  // pastilles territoire / opacités morts
}

static void enter_marking() {
    Ai::abort();
    // Ne réinitialise g_dead que si on arrive depuis le jeu (pas une reprise
    // après pause pendant le marquage — géré par enter_playing).
    const bool first = (g_state != ST_MARKING);
    g_state = ST_MARKING;
    g_pending = -1;
    if (first) {
        memset(g_dead, 0, sizeof(g_dead));
        msg("Deux passes : marquez les groupes morts");
    }
    refresh_territory();
    refresh_all();
}

static void finish_scoring() {
    refresh_territory();
    g_resigned = false;
    g_winner = 2;
    if (g_score.black > g_score.white) g_winner = 0;
    else if (g_score.white > g_score.black) g_winner = 1;
    record_result(g_winner);
    g_state = ST_SCORE;
    refresh_all();
    show_score_card();
    flush(true);
}

static void resign(uint8_t who_resigns) {
    refresh_territory();
    g_resigned = true;
    g_winner = (who_resigns == BLACK) ? 1 : 0;
    record_result(g_winner);
    g_state = ST_SCORE;
    refresh_all();
    show_score_card();
    flush(true);
}

// --- Nouvelle partie ------------------------------------------------------

static void start_new() {
    Ai::abort();
    Engine::pos_init(g_pos, SIZE_TAB[g_cfg_size < GO_N_SIZES ? g_cfg_size : 0]);
    if (g_cfg_mode == 0 && g_cfg_hcap >= 2) Engine::place_handicap(g_pos, g_cfg_hcap);
    g_first_color = g_pos.side;
    g_last_sq = PASS;
    g_pending = -1;
    g_hint_sq = -1;
    g_mv_n = 0;
    g_undo_n = 0;
    g_in_game = true;
    g_game_t0 = esphome::millis();
    memset(g_dead, 0, sizeof(g_dead));
    memset(g_terr, 0, sizeof(g_terr));
    layout_board();
    stash_position();
    enter_playing();
}

static void resume_game() {
    if (!restore_position()) { menu_main(); return; }
    // La liste des coups et la pile d'annulation ne sont pas persistées : on
    // reprend la position, pas l'historique. C'est assumé (et documenté au
    // README) — sauvegarder 512 coups en NVS à chaque partie n'a pas de sens.
    g_first_color = (g_cfg_hcap >= 2) ? WHITE : BLACK;
    g_last_sq = PASS;
    g_pending = -1;
    g_hint_sq = -1;
    g_mv_n = 0;
    g_undo_n = 0;
    g_in_game = true;
    g_game_t0 = esphome::millis();
    memset(g_dead, 0, sizeof(g_dead));
    memset(g_terr, 0, sizeof(g_terr));
    layout_board();
    msg("Partie reprise");
    enter_playing();
}

static void enter_playing() {
    show(g_ui.panel, false);
    show(g_card, false);
    // Reprise après pause pendant le marquage : conserver g_dead.
    if (Engine::is_over(g_pos) && g_in_game) {
        g_state = ST_MARKING;
        refresh_territory();
        refresh_all();
        return;
    }
    g_state = ST_PLAYING;
    refresh_all();
    after_move();
}

// ===========================================================================
// 12. Menus
// ===========================================================================

static void slot_set(int i, const char* title, const char* desc, uint32_t col, bool on) {
    if (i < 0 || i >= N_SLOTS) return;
    show(g_slot[i], on);
    if (!on) return;
    set_text_if(g_slot_t[i], title);
    set_text_if(g_slot_d[i], desc ? desc : "");
    set_color(g_slot_t[i], col);
    set_border(g_slot[i], col, 2, LV_OPA_40);
}

static void menu_open(const char* title, const char* sub, const char* foot) {
    show(g_ui.panel, true);
    lv_obj_move_foreground(g_ui.panel);
    set_bg(g_ui.panel, Pal::VOID_BG, LV_OPA_COVER);
    show(g_card, false);
    set_text_if(g_m_title, title);
    set_text_if(g_m_sub, sub ? sub : "");
    set_text_if(g_m_foot, foot ? foot : "");
    for (int i = 0; i < N_SLOTS; i++) show(g_slot[i], false);
}

static void menu_main() {
    g_state = ST_MENU_MAIN;
    Ai::abort();
    char sub[72];
    snprintf(sub, sizeof(sub), "Score chinois d'aire  ·  komi %.1f  ·  ko simple",
             (double) effective_komi());
    menu_open("Go Tab", sub,
              "Toutes les parties et les reglages sont conserves dans le Tab.");
    int i = 0;
    if (g_in_game) {
        slot_set(i++, "Reprendre la partie", "Retour au goban", Pal::GOOD, true);
    } else if (g_save.has_game) {
        char d[48];
        snprintf(d, sizeof(d), "%dx%d, coup %u", (int) g_save.n, (int) g_save.n,
                 (unsigned) g_save.move_no);
        slot_set(i++, "Reprendre la sauvegarde", d, Pal::GOOD, true);
    }
    slot_set(i++, "Nouvelle partie", "Taille, mode, niveau, handicap", Pal::ACCENT, true);
    slot_set(i++, "Statistiques", "Bilan face au Tab", Pal::TXT, true);
    slot_set(i++, "Reglages", "Confirmation, coordonnees, secousse", Pal::TXT_DIM, true);
    slot_set(i++, "Quitter", "Retour a l'arcade", Pal::DANGER, true);
    for (; i < N_SLOTS; i++) slot_set(i, "", "", 0, false);
    flush(true);
}

static void menu_pause() {
    g_state = ST_PAUSE;
    Ai::abort();
    char sub[64];
    snprintf(sub, sizeof(sub), "%s  ·  %s  ·  coup %u", size_name(g_cfg_size),
             mode_name(g_cfg_mode), (unsigned) g_pos.move_no);
    menu_open("Pause", sub, "Le goban vous attend.");
    slot_set(0, "Reprendre la partie", "", Pal::GOOD, true);
    slot_set(1, "Nouvelle partie", "Changer les reglages", Pal::ACCENT, true);
    slot_set(2, "Abandonner", "L'adversaire gagne", Pal::DANGER, true);
    slot_set(3, "Statistiques", "", Pal::TXT, true);
    slot_set(4, "Reglages", "", Pal::TXT_DIM, true);
    slot_set(5, "Quitter le jeu", "La partie est sauvegardee", Pal::TXT_MUTED, true);
    slot_set(6, "", "", 0, false);
    stash_position();
    flush(true);
}

static void menu_setup() {
    g_state = ST_MENU_SETUP;
    char sub[80], b[64];
    snprintf(sub, sizeof(sub), "%s  ·  %s", size_name(g_cfg_size), mode_name(g_cfg_mode));
    menu_open("Nouvelle partie", sub, "Touchez une ligne pour changer sa valeur.");

    slot_set(0, size_name(g_cfg_size), "Taille du goban  —  9x9 / 13x13 / 19x19",
             Pal::WOOD, true);
    slot_set(1, mode_name(g_cfg_mode), "Adversaire", Pal::THINK, true);

    snprintf(b, sizeof(b), "Vous jouez %s", g_cfg_human == BLACK ? "Noir (premier)" : "Blanc");
    slot_set(2, b, "Couleur du joueur", Pal::TXT, g_cfg_mode == 0);

    snprintf(b, sizeof(b), "Niveau : %s", level_name(g_cfg_level));
    slot_set(3, b, "Force du Tab", Pal::ACCENT, g_cfg_mode != 1);

    if (g_cfg_hcap >= 2) snprintf(b, sizeof(b), "Handicap : %d pierres", (int) g_cfg_hcap);
    else                 snprintf(b, sizeof(b), "Handicap : aucun");
    slot_set(4, b, "Pierres offertes a Noir (Blanc commence)", Pal::WOOD, g_cfg_mode == 0);

    slot_set(5, "Jouer !", "Komi 6,5 pour Blanc", Pal::GOOD, true);
    slot_set(6, "Retour", "", Pal::TXT_MUTED, true);
}

static void menu_stats() {
    g_state = ST_MENU_STATS;
    char sub[64];
    const unsigned mins = (unsigned) (g_save.total_ms / 60000u);
    snprintf(sub, sizeof(sub), "%u parties  ·  %u min de jeu",
             (unsigned) g_save.games, mins);
    menu_open("Statistiques", sub, "Comptabilise uniquement le mode Joueur contre Tab.");

    char t[64], d[64];
    const uint8_t s = g_stats_size < GO_N_SIZES ? g_stats_size : 0;
    for (int lv = 0; lv < GO_N_LEVELS; lv++) {
        const unsigned w = g_save.wins[s][lv], dr = g_save.draws[s][lv], l = g_save.losses[s][lv];
        snprintf(t, sizeof(t), "%s", level_name((uint8_t) lv));
        snprintf(d, sizeof(d), "%u victoire%s  ·  %u nulle%s  ·  %u defaite%s",
                 w, w > 1 ? "s" : "", dr, dr > 1 ? "s" : "", l, l > 1 ? "s" : "");
        slot_set(lv, t, d, w > l ? Pal::GOOD : Pal::TXT, true);
    }
    snprintf(t, sizeof(t), "Taille affichee : %s", size_name(s));
    slot_set(4, t, "Toucher pour changer", Pal::WOOD, true);
    slot_set(5, "Remettre les compteurs a zero", "Irreversible", Pal::DANGER, true);
    slot_set(6, "Retour", "", Pal::TXT_MUTED, true);
}

static void menu_opts() {
    g_state = ST_MENU_OPTS;
    menu_open("Reglages", "Options locales, conservees dans le Tab", "");
    slot_set(0, g_save.opt_confirm ? "Confirmation du coup : ACTIVEE"
                                   : "Confirmation du coup : DESACTIVEE",
             "Un premier toucher place un fantome, le second valide", Pal::ACCENT, true);
    slot_set(1, g_save.opt_coords ? "Coordonnees : AFFICHEES" : "Coordonnees : MASQUEES",
             "Lettres A..T et chiffres autour du goban", Pal::TXT, true);
    slot_set(2, g_save.opt_lastmark ? "Dernier coup : MARQUE" : "Dernier coup : NON MARQUE",
             "Anneau rouge sur la derniere pierre posee", Pal::TXT, true);
    slot_set(3, g_save.opt_terr ? "Apercu du territoire : ACTIVE" : "Apercu du territoire : DESACTIVE",
             "Pastilles de territoire pendant le comptage", Pal::TXT, true);
    slot_set(4, g_save.opt_shake ? "Secousse = indice : ACTIVE" : "Secousse = indice : DESACTIVE",
             "Detection BMI270", Pal::TXT_DIM, true);
    slot_set(5, "Retour", "", Pal::TXT_MUTED, true);
    slot_set(6, "", "", 0, false);
}

static void menu_confirm_reset() {
    g_state = ST_CONFIRM_RESET;
    menu_open("Effacer les statistiques ?", "Victoires, nulles et defaites de toutes les tailles",
              "Cette action est definitive.");
    slot_set(0, "Oui, tout effacer", "", Pal::DANGER, true);
    slot_set(1, "Annuler", "", Pal::TXT_MUTED, true);
    for (int i = 2; i < N_SLOTS; i++) slot_set(i, "", "", 0, false);
}

// --- Carte de fin de partie ----------------------------------------------

static void show_score_card() {
    // Panneau semi-transparent : le goban et son territoire restent visibles.
    show(g_ui.panel, true);
    lv_obj_move_foreground(g_ui.panel);
    set_bg(g_ui.panel, Pal::VOID_BG, (lv_opa_t) 195);
    for (int i = 0; i < N_SLOTS; i++) show(g_slot[i], false);
    set_text_if(g_m_title, "");
    set_text_if(g_m_sub, "");
    set_text_if(g_m_foot, "");
    show(g_card, true);

    char b[80];
    if (g_winner == 0) { set_text_if(g_card_title, "Noir l'emporte"); set_color(g_card_title, Pal::TXT); }
    else if (g_winner == 1) { set_text_if(g_card_title, "Blanc l'emporte"); set_color(g_card_title, Pal::TXT); }
    else { set_text_if(g_card_title, "Partie nulle"); set_color(g_card_title, Pal::ACCENT); }

    if (g_resigned) {
        snprintf(b, sizeof(b), "Abandon  —  comptage indicatif : %.1f contre %.1f",
                 (double) g_score.black, (double) g_score.white);
    } else {
        const float diff = g_score.black - g_score.white;
        const float ad = diff < 0 ? -diff : diff;
        snprintf(b, sizeof(b), "%.1f  contre  %.1f      (ecart %.1f)",
                 (double) g_score.black, (double) g_score.white, (double) ad);
    }
    set_text_if(g_card_sub, b);

    snprintf(b, sizeof(b), "Noir   pierres %d   territoire %d", g_score.black_stones, g_score.black_terr);
    set_text_if(g_card_line[0], b);
    snprintf(b, sizeof(b), "Blanc  pierres %d   territoire %d   komi %.1f",
             g_score.white_stones, g_score.white_terr, (double) effective_komi());
    set_text_if(g_card_line[1], b);
    snprintf(b, sizeof(b), "Groupes morts retires : %d noirs, %d blancs",
             g_score.black_dead, g_score.white_dead);
    set_text_if(g_card_line[2], b);
    snprintf(b, sizeof(b), "Points neutres (dame) : %d", g_score.dame);
    set_text_if(g_card_line[3], b);
    snprintf(b, sizeof(b), "Prisonniers de la partie : Noir %u, Blanc %u",
             (unsigned) g_pos.captured_by_black, (unsigned) g_pos.captured_by_white);
    set_text_if(g_card_line[4], b);
    snprintf(b, sizeof(b), "%s  ·  %s  ·  %u coups", size_name(g_cfg_size),
             mode_name(g_cfg_mode), (unsigned) g_pos.move_no);
    set_text_if(g_card_line[5], b);

    set_text_if(g_card_btn_lbl[0], "Revanche");
    set_text_if(g_card_btn_lbl[1], "Menu principal");
}

// ===========================================================================
// 13. Événements
// ===========================================================================

// Intersection la plus proche du point touché, ou -1 si le doigt est trop loin.
static int hit_intersection(int x, int y) {
    // Hors du plateau (panneau latéral, tapis) : ce n'est pas un coup.
    if (x < g_plate_x || y < g_plate_y ||
        x >= g_plate_x + g_plate_sz || y >= g_plate_y + g_plate_sz) return -1;
    int best = -1, best_d = 1 << 30;
    for (int r = 0; r < g_n; r++) {
        for (int c = 0; c < g_n; c++) {
            const int dx = x - (g_ox + c * g_gap);
            const int dy = y - (g_oy + r * g_gap);
            const int d = dx * dx + dy * dy;
            if (d < best_d) { best_d = d; best = Engine::idx(r, c, g_n); }
        }
    }
    // Tolérance : un demi-écart. Au-delà, le doigt visait autre chose.
    const int lim = (g_gap * g_gap) / 2;
    return (best_d <= lim) ? best : -1;
}

static void on_board_tap(int x, int y) {
    const int sq = hit_intersection(x, y);

    if (g_state == ST_MARKING) {
        if (sq < 0 || g_pos.sq[sq] == EMPTY) return;
        const uint8_t v = g_dead[sq] ? 0 : 1;
        Engine::mark_chain(g_pos, sq, g_dead, v);
        refresh_territory();
        refresh_all();
        return;
    }
    if (g_state != ST_PLAYING || !is_human_turn()) return;

    if (sq < 0) {                       // toucher hors du goban : on annule l'attente
        if (g_pending >= 0) { g_pending = -1; refresh_all(); }
        return;
    }
    if (!Engine::is_legal(g_pos, sq)) {
        g_pending = -1;
        msg(g_pos.sq[sq] != EMPTY ? "Intersection occupee" : "Coup interdit (ko ou suicide)");
        refresh_all();
        return;
    }
    if (!g_save.opt_confirm) { apply_move(sq); return; }
    if (g_pending == sq) { apply_move(sq); return; }
    g_pending = sq;
    refresh_all();
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

static void btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const int i = (int) (intptr_t) lv_event_get_user_data(e);
    if (g_state == ST_MARKING) {
        if (i == 0) {                      // Tout vivant
            memset(g_dead, 0, sizeof(g_dead));
            refresh_territory();
            refresh_all();
        } else if (i == 1) {               // Reprendre le jeu (annule la 2e passe)
            do_undo();
        } else if (i == 3) {
            menu_pause();
        }
        return;
    }
    switch (i) {
        case 0: do_pass(); break;
        case 1: do_undo(); break;
        case 2: do_hint(); break;
        case 3: menu_pause(); break;
        default: break;
    }
}

static void btn_ok_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_state == ST_MARKING) { finish_scoring(); return; }
    if (g_state == ST_PLAYING && g_pending >= 0) apply_move(g_pending);
}

static void hud_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_state == ST_PLAYING || g_state == ST_THINKING || g_state == ST_MARKING)
        menu_pause();
}

static void card_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const int i = (int) (intptr_t) lv_event_get_user_data(e);
    show(g_card, false);
    if (i == 0) start_new();
    else menu_main();
}

static void slot_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const int i = (int) (intptr_t) lv_event_get_user_data(e);

    switch (g_state) {
        case ST_MENU_MAIN: {
            // Le nombre d'entrées varie selon qu'une partie est reprenable.
            const bool resumable = g_in_game || g_save.has_game;
            int k = i;
            if (resumable) {
                if (k == 0) { if (g_in_game) enter_playing(); else resume_game(); return; }
                k--;
            }
            if (k == 0) menu_setup();
            else if (k == 1) { g_stats_size = g_cfg_size; menu_stats(); }
            else if (k == 2) menu_opts();
            else if (k == 3) close();
            break;
        }
        case ST_PAUSE:
            if (i == 0) enter_playing();
            else if (i == 1) menu_setup();
            else if (i == 2) {
                // En Joueur contre Tab, c'est TOUJOURS le joueur qui abandonne,
                // même si le Tab était en train de réfléchir quand le menu s'est
                // ouvert (le trait est alors au Tab).
                show(g_ui.panel, false);
                resign(g_cfg_mode == 0 ? g_cfg_human : g_pos.side);
            }
            else if (i == 3) { g_stats_size = g_cfg_size; menu_stats(); }
            else if (i == 4) menu_opts();
            else if (i == 5) close();
            break;
        case ST_MENU_SETUP:
            if (i == 0) g_cfg_size = (uint8_t) ((g_cfg_size + 1) % GO_N_SIZES);
            else if (i == 1) g_cfg_mode = (uint8_t) ((g_cfg_mode + 1) % 3);
            else if (i == 2 && g_cfg_mode == 0)
                g_cfg_human = (g_cfg_human == BLACK) ? WHITE : BLACK;
            else if (i == 3 && g_cfg_mode != 1)
                g_cfg_level = (uint8_t) ((g_cfg_level + 1) % GO_N_LEVELS);
            else if (i == 4 && g_cfg_mode == 0)
                g_cfg_hcap = (uint8_t) (g_cfg_hcap == 0 ? 2 : (g_cfg_hcap >= 9 ? 0 : g_cfg_hcap + 1));
            else if (i == 5) { persist_save(); start_new(); return; }
            else if (i == 6) { persist_save(); g_in_game ? menu_pause() : menu_main(); return; }
            stash_settings();
            menu_setup();
            break;
        case ST_MENU_STATS:
            if (i == 4) { g_stats_size = (uint8_t) ((g_stats_size + 1) % GO_N_SIZES); menu_stats(); }
            else if (i == 5) menu_confirm_reset();
            else if (i == 6) { g_in_game ? menu_pause() : menu_main(); }
            break;
        case ST_MENU_OPTS:
            if (i == 0) g_save.opt_confirm ^= 1;
            else if (i == 1) { g_save.opt_coords ^= 1; layout_board(); }
            else if (i == 2) g_save.opt_lastmark ^= 1;
            else if (i == 3) g_save.opt_terr ^= 1;
            else if (i == 4) g_save.opt_shake ^= 1;
            else if (i == 5) { persist_save(); g_in_game ? menu_pause() : menu_main(); return; }
            g_dirty = true;
            menu_opts();
            break;
        case ST_CONFIRM_RESET:
            if (i == 0) {
                memset(g_save.wins, 0, sizeof(g_save.wins));
                memset(g_save.draws, 0, sizeof(g_save.draws));
                memset(g_save.losses, 0, sizeof(g_save.losses));
                g_save.games = 0;
                g_save.total_ms = 0;
                persist_save();
            }
            menu_stats();
            break;
        default: break;
    }
}

// ===========================================================================
// 14. Construction de l'UI (une seule fois)
// ===========================================================================

static void build_ui() {
    if (g_ui_built) return;
    g_ui_built = true;

    set_bg(g_ui.root, Pal::VOID_BG, LV_OPA_COVER);
    set_bg(g_ui.hud, Pal::HUD_BG, LV_OPA_COVER);
    set_bg(g_ui.field, Pal::FIELD_BG, LV_OPA_COVER);
    set_bg(g_ui.panel, Pal::VOID_BG, LV_OPA_COVER);
    // Le calque des menus DOIT absorber les taps, sinon ils traversent jusqu'au
    // goban qui vit en dessous.
    lv_obj_add_flag(g_ui.panel, LV_OBJ_FLAG_CLICKABLE);
    show(g_ui.panel, false);

    // --- Plateau ---------------------------------------------------------
    g_plate = mk_rect(g_ui.field);
    set_bg_grad(g_plate, Pal::WOOD, Pal::WOOD_DK, LV_OPA_COVER);
    set_border(g_plate, Pal::WOOD_EDGE, 3, LV_OPA_COVER);
    lv_obj_set_style_radius(g_plate, 8, LV_PART_MAIN);

    for (int i = 0; i < MAX_N; i++) {
        g_grid_h[i] = mk_rect(g_ui.field);
        set_bg(g_grid_h[i], Pal::GRID, LV_OPA_COVER);
        g_grid_v[i] = mk_rect(g_ui.field);
        set_bg(g_grid_v[i], Pal::GRID, LV_OPA_COVER);
    }
    for (int i = 0; i < 9; i++) {
        g_hoshi[i] = mk_rect(g_ui.field);
        set_bg(g_hoshi[i], Pal::HOSHI, LV_OPA_COVER);
    }
    for (int i = 0; i < MAX_N; i++) {
        g_coord_c[i] = mk_label(g_ui.field, g_ui.f_small, Pal::WOOD_EDGE);
        lv_obj_set_style_text_align(g_coord_c[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        g_coord_r[i] = mk_label(g_ui.field, g_ui.f_small, Pal::WOOD_EDGE);
        lv_obj_set_style_text_align(g_coord_r[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    }
    for (int i = 0; i < MAX_SQ; i++) {
        g_stone[i] = mk_rect(g_ui.field);
        show(g_stone[i], false);
    }
    g_mark_last  = mk_rect(g_ui.field);
    g_mark_ghost = mk_rect(g_ui.field);
    g_mark_hint  = mk_rect(g_ui.field);
    show(g_mark_last, false);
    show(g_mark_ghost, false);
    show(g_mark_hint, false);

    lv_obj_add_flag(g_ui.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.field, field_cb, LV_EVENT_CLICKED, nullptr);

    // --- HUD --------------------------------------------------------------
    for (int i = 0; i < 2; i++) {
        g_pill[i] = mk_rect(g_ui.hud);
        lv_obj_set_size(g_pill[i], 296, 46);
        lv_obj_set_pos(g_pill[i], i == 0 ? 10 : 974, 7);
        lv_obj_set_style_radius(g_pill[i], 12, LV_PART_MAIN);
        set_bg(g_pill[i], Pal::CARD_BG, LV_OPA_COVER);

        g_pill_dot[i] = mk_rect(g_pill[i]);
        lv_obj_set_size(g_pill_dot[i], 26, 26);
        lv_obj_set_pos(g_pill_dot[i], 12, 10);
        lv_obj_set_style_radius(g_pill_dot[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        if (i == 0) set_bg_grad(g_pill_dot[i], Pal::STONE_B_H, Pal::STONE_B, LV_OPA_COVER);
        else        set_bg_grad(g_pill_dot[i], Pal::STONE_W_H, Pal::STONE_W, LV_OPA_COVER);

        g_pill_name[i] = mk_label(g_pill[i], g_ui.f_small, Pal::TXT);
        lv_obj_set_pos(g_pill_name[i], 48, 3);
        g_pill_sub[i] = mk_label(g_pill[i], g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(g_pill_sub[i], 48, 24);
    }
    g_h_move = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_set_width(g_h_move, 640);
    lv_obj_set_style_text_align(g_h_move, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_h_move, 320, 6);
    g_h_status = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_set_width(g_h_status, 640);
    lv_obj_set_style_text_align(g_h_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_h_status, 320, 31);

    lv_obj_add_flag(g_ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.hud, hud_cb, LV_EVENT_CLICKED, nullptr);

    // --- Panneau latéral --------------------------------------------------
    g_panel = mk_rect(g_ui.field);
    lv_obj_set_pos(g_panel, PANEL_X, PANEL_Y);
    lv_obj_set_size(g_panel, PANEL_W, PANEL_H);
    lv_obj_set_style_radius(g_panel, 14, LV_PART_MAIN);
    set_bg(g_panel, Pal::PANEL_BG, LV_OPA_COVER);
    set_border(g_panel, Pal::EDGE, 1, LV_OPA_60);

    g_p_title = mk_label(g_panel, g_ui.f_small, Pal::ACCENT);
    lv_obj_set_pos(g_p_title, 16, 12);
    set_text_if(g_p_title, "COUPS");
    g_p_head[0] = mk_label(g_panel, g_ui.f_small, Pal::TXT_MUTED);
    lv_obj_set_pos(g_p_head[0], 108, 12);
    g_p_head[1] = mk_label(g_panel, g_ui.f_small, Pal::TXT_MUTED);
    lv_obj_set_pos(g_p_head[1], 208, 12);

    for (int i = 0; i < MOVE_ROWS; i++) {
        const int y = 46 + i * 27;
        g_ml_num[i] = mk_label(g_panel, g_ui.f_mono, Pal::TXT_MUTED);
        lv_obj_set_pos(g_ml_num[i], 16, y);
        g_ml_a[i] = mk_label(g_panel, g_ui.f_mono, Pal::TXT);
        lv_obj_set_pos(g_ml_a[i], 108, y);
        g_ml_b[i] = mk_label(g_panel, g_ui.f_mono, Pal::TXT_DIM);
        lv_obj_set_pos(g_ml_b[i], 208, y);
    }

    g_think_lbl = mk_label(g_panel, g_ui.f_small, Pal::THINK);
    lv_obj_set_pos(g_think_lbl, 16, 380);
    g_think_bar = mk_rect(g_panel);
    lv_obj_set_pos(g_think_bar, 16, 410);
    lv_obj_set_size(g_think_bar, 270, 8);
    lv_obj_set_style_radius(g_think_bar, 4, LV_PART_MAIN);
    set_bg(g_think_bar, Pal::CARD_BG, LV_OPA_COVER);
    g_think_fill = mk_rect(g_think_bar);
    lv_obj_set_pos(g_think_fill, 0, 0);
    lv_obj_set_size(g_think_fill, 2, 8);
    lv_obj_set_style_radius(g_think_fill, 4, LV_PART_MAIN);
    set_bg(g_think_fill, Pal::THINK, LV_OPA_COVER);
    show(g_think_lbl, false);
    show(g_think_bar, false);

    g_btn_ok = mk_rect(g_panel);
    lv_obj_set_pos(g_btn_ok, 16, 432);
    lv_obj_set_size(g_btn_ok, 270, 56);
    lv_obj_set_style_radius(g_btn_ok, 12, LV_PART_MAIN);
    set_bg(g_btn_ok, Pal::CARD_BG, LV_OPA_COVER);
    press_fx(g_btn_ok, Pal::CARD_ON);
    lv_obj_add_flag(g_btn_ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_btn_ok, btn_ok_cb, LV_EVENT_CLICKED, nullptr);
    g_btn_ok_lbl = mk_label(g_btn_ok, g_ui.f_small, Pal::GHOST);
    lv_obj_center(g_btn_ok_lbl);
    show(g_btn_ok, false);

    static const char* const BTN0[4] = {"Passer", "Annuler", "Indice", "Menu"};
    for (int i = 0; i < 4; i++) {
        g_btn[i] = mk_rect(g_panel);
        lv_obj_set_pos(g_btn[i], (i & 1) ? 154 : 16, 500 + (i >> 1) * 68);
        lv_obj_set_size(g_btn[i], 132, 60);
        lv_obj_set_style_radius(g_btn[i], 12, LV_PART_MAIN);
        set_bg(g_btn[i], Pal::CARD_BG, LV_OPA_COVER);
        set_border(g_btn[i], Pal::EDGE, 1, LV_OPA_80);
        press_fx(g_btn[i], Pal::CARD_ON);
        lv_obj_add_flag(g_btn[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_btn[i], btn_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_btn_lbl[i] = mk_label(g_btn[i], g_ui.f_small, Pal::TXT);
        set_text_if(g_btn_lbl[i], BTN0[i]);
        lv_obj_center(g_btn_lbl[i]);
    }

    // --- Calque des menus -------------------------------------------------
    g_m_title = mk_label(g_ui.panel, g_ui.f_big, Pal::ACCENT);
    lv_obj_set_width(g_m_title, 1280);
    lv_obj_set_style_text_align(g_m_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_m_title, 0, 38);
    g_m_sub = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_set_width(g_m_sub, 1280);
    lv_obj_set_style_text_align(g_m_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_m_sub, 0, 96);
    g_m_foot = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT_MUTED);
    lv_obj_set_width(g_m_foot, 1280);
    lv_obj_set_style_text_align(g_m_foot, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_m_foot, 0, 684);

    for (int i = 0; i < N_SLOTS; i++) {
        g_slot[i] = mk_rect(g_ui.panel);
        lv_obj_set_pos(g_slot[i], 90, 148 + i * 74);
        lv_obj_set_size(g_slot[i], 1100, 64);
        lv_obj_set_style_radius(g_slot[i], 14, LV_PART_MAIN);
        set_bg(g_slot[i], Pal::CARD_BG, LV_OPA_COVER);
        press_fx(g_slot[i], Pal::CARD_ON);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_slot[i], slot_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_slot_t[i] = mk_label(g_slot[i], g_ui.f_mid, Pal::TXT);
        lv_obj_set_pos(g_slot_t[i], 26, 6);
        g_slot_d[i] = mk_label(g_slot[i], g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(g_slot_d[i], 28, 40);
        show(g_slot[i], false);
    }

    // --- Carte de fin de partie -------------------------------------------
    g_card = mk_rect(g_ui.panel);
    lv_obj_set_pos(g_card, 230, 120);
    lv_obj_set_size(g_card, 820, 480);
    lv_obj_set_style_radius(g_card, 18, LV_PART_MAIN);
    set_bg(g_card, Pal::PANEL_BG, LV_OPA_COVER);
    set_border(g_card, Pal::ACCENT, 2, LV_OPA_60);
    show(g_card, false);

    g_card_title = mk_label(g_card, g_ui.f_big, Pal::TXT);
    lv_obj_set_width(g_card_title, 820);
    lv_obj_set_style_text_align(g_card_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_card_title, 0, 26);
    g_card_sub = mk_label(g_card, g_ui.f_mid, Pal::ACCENT);
    lv_obj_set_width(g_card_sub, 820);
    lv_obj_set_style_text_align(g_card_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_card_sub, 0, 86);
    for (int i = 0; i < CARD_LINES; i++) {
        g_card_line[i] = mk_label(g_card, g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_width(g_card_line[i], 760);
        lv_obj_set_style_text_align(g_card_line[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(g_card_line[i], 30, 148 + i * 32);
    }
    static const char* const CBTN[2] = {"Revanche", "Menu principal"};
    for (int i = 0; i < 2; i++) {
        g_card_btn[i] = mk_rect(g_card);
        lv_obj_set_pos(g_card_btn[i], 60 + i * 380, 380);
        lv_obj_set_size(g_card_btn[i], 320, 66);
        lv_obj_set_style_radius(g_card_btn[i], 14, LV_PART_MAIN);
        set_bg(g_card_btn[i], Pal::CARD_BG, LV_OPA_COVER);
        set_border(g_card_btn[i], i == 0 ? Pal::ACCENT : Pal::EDGE, 2, LV_OPA_70);
        press_fx(g_card_btn[i], Pal::CARD_ON);
        lv_obj_add_flag(g_card_btn[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_card_btn[i], card_btn_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_card_btn_lbl[i] = mk_label(g_card_btn[i], g_ui.f_mid,
                                     i == 0 ? Pal::ACCENT : Pal::TXT);
        set_text_if(g_card_btn_lbl[i], CBTN[i]);
        lv_obj_center(g_card_btn_lbl[i]);
    }
}

// ===========================================================================
// 15. Timer
// ===========================================================================

static void tick_cb(lv_timer_t*) {
    if (g_state == ST_OFF) return;

    // Tick adaptatif : 25 ms pendant la réflexion IA, 50 ms sinon (menus, attente
    // humain, marquage). Réduit la charge CPU de ~50 % entre les coups.
    const uint32_t want_ms = (g_state == ST_THINKING) ? TICK_THINK_MS : TICK_IDLE_MS;
    static uint32_t cur_period = TICK_IDLE_MS;
    if (g_timer && cur_period != want_ms) {
        cur_period = want_ms;
        lv_timer_set_period(g_timer, want_ms);
    }

    const uint32_t now = esphome::millis();

    // Expiration de l'indice et du message de statut.
    bool need_board = false;
    if (g_hint_sq >= 0 && (int32_t)(now - g_hint_until) >= 0) {
        g_hint_sq = -1;
        need_board = true;
    }
    if (g_msg_until && (int32_t)(now - g_msg_until) >= 0) {
        g_msg_until = 0;
        g_msg[0] = 0;
        render_hud();
    }
    if (need_board) render_board();

    if (g_state == ST_THINKING) {
        Ai::step(AI_SLICE_MS);
        render_panel_buttons();
        if (Ai::ready() && (now - g_think_t0) >= MIN_THINK_MS) {
            const int sq = Ai::best_sq();
            if (sq == Ai::RESIGN) {
                msg("Le Tab abandonne");
                resign(g_pos.side);
                return;
            }
            if (g_cfg_mode == 2 && g_next_move && (int32_t)(now - g_next_move) < 0) return;
            g_next_move = now + TVT_PAUSE_MS;
            apply_move(sq);
        }
        return;
    }

    flush(false);
}

// ===========================================================================
// 16. API publique
// ===========================================================================

void on_imu(float ax, float ay, float az) {
    if (ax != ax || ay != ay || az != az) return;   // NaN
    const float dax = ax - g_ax, day = ay - g_ay, daz = az - g_az;
    g_ax = ax; g_ay = ay; g_az = az;
    if (g_state != ST_PLAYING || !g_save.opt_shake) return;
    if (!is_human_turn()) return;
    const float mag = sqrtf(dax * dax + day * day + daz * daz);
    const uint32_t now = esphome::millis();
    if (mag > 1.4f && (now - g_last_shake) > 1200) {
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

    Engine::pos_init(g_pos, SIZE_TAB[g_cfg_size < GO_N_SIZES ? g_cfg_size : 0]);
    g_in_game = false;
    g_last_sq = PASS;
    g_pending = -1;
    g_hint_sq = -1;
    g_mv_n = 0;
    g_undo_n = 0;
    g_first_color = BLACK;
    memset(g_dead, 0, sizeof(g_dead));
    memset(g_terr, 0, sizeof(g_terr));
    g_last_nvs = esphome::millis();

    layout_board();
    refresh_all();
    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).
    menu_main();
    if (!g_timer) g_timer = lv_timer_create(tick_cb, TICK_IDLE_MS, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;
    Ai::abort();
    if (g_in_game) {
        stash_position();
        if (g_game_t0) { g_save.total_ms += esphome::millis() - g_game_t0; g_game_t0 = 0; }
    }
    persist_save();
    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    show(g_ui.panel, false);
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (g_ui.lvgl) g_ui.lvgl->show_page(g_ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
    g_state = ST_OFF;
}

}  // namespace Go
