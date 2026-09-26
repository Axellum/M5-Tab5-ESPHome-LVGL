/**
 * [AI-CONTEXT]
 * @file go_game.cpp
 * @role UI LVGL + NVS + machine à états pour « Go Tab ».
 * @architecture_constraint Widgets PRÉALLOUÉS (19×19 max) à l'ouverture et
 *      repositionnés à chaque changement de taille : aucune création/destruction
 *      d'objet LVGL pendant une partie. Le lv_timer (25 ms) ne fait qu'UNE tranche
 *      d'IA (13 ms max) ; il n'y a jamais de recherche bloquante.
 * @architecture_constraint Jeu fermé, il ne reste rien (audit du 26/09/2026,
 *      lot 4) : objets LVGL détruits, état rendu (struct Mem en RAM interne,
 *      struct Cold en PSRAM), brouillons du moteur et de l'IA rendus. Seuls
 *      survivent l'état ouvert/fermé, la NVS, les lectures IMU et l'anti-rebond.
 * @architecture_constraint NVS : une partie de Go fait des centaines de coups.
 *      L'écriture flash est donc DIFFÉRÉE (drapeau `Mem::dirty` + fenêtre de 15 s),
 *      et forcée seulement aux moments qui comptent (menu, fin de partie,
 *      fermeture). La version précédente appelait sync() à chaque coup.
 * @ai_instruction Règles = go_engine.*, IA = go_ai.*. Ici : rendu, entrées,
 *      persistance. Ne rien remonter dans le YAML : il ne déclare que 4
 *      conteneurs vides.
 */
#include "go_game.h"
#include "game_common.h"
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

// Ce qui survit à la fermeture — une trentaine d'octets : l'état ouvert/fermé
// (lu par le registre), l'accès NVS (le recréer ferait fuir un backend de
// préférences par ouverture), les dernières lectures IMU (dispatch_imu appelle
// on_imu même jeu fermé ; reparties de (0, 0, 1) à l'ouverture, la première
// variation pourrait passer pour une secousse) et l'anti-rebond de la secousse.
// L'aléa de l'IA (Ai::g_rng) survit aussi, dans go_ai.cpp.
static UiState     g_state = ST_OFF;
static NvsSlot<GoSave> g_nvs(PREF_KEY, SAVE_MAGIC);
static float g_ax = 0, g_ay = 0, g_az = 1;
static uint32_t g_last_shake = 0;

// Tout le reste n'existe que jeu ouvert : créé par open(), rendu par close(),
// avec les objets LVGL qu'il pointe (game_common.h, « Mémoire d'un jeu »). Le
// moteur et l'IA ont chacun leur brouillon, pris et rendus au même moment
// (Engine::scratch_acquire, Ai::scratch_acquire).
struct Mem {
    UI          ui;
    lv_timer_t* timer = nullptr;
    uint32_t    tick_period = 0;  // période réelle du timer (voir tick_cb)
    GoSave      save{};           // relue de la NVS à chaque ouverture

    Pos  pos;
    bool in_game = false;     // une partie est en cours (menu « Reprendre »)
    int  last_sq = PASS;
    int  pending = -1;        // coup en attente de validation (opt_confirm)
    int  hint_sq = -1;
    uint32_t hint_until = 0;

    uint8_t dead[MAX_SQ];     // 1 = groupe marqué mort (écran de marquage)
    uint8_t terr[MAX_SQ];     // carte de territoire (Engine::Terr)
    Engine::Score score{};
    int  winner = 2;          // 0 = Noir, 1 = Blanc, 2 = nulle
    bool resigned = false;    // la partie s'est terminée par un abandon

    // Réglages de la partie à créer (copie de travail des réglages NVS).
    uint8_t cfg_size  = 0;
    uint8_t cfg_mode  = 0;
    uint8_t cfg_human = BLACK;
    uint8_t cfg_level = 1;
    uint8_t cfg_hcap  = 0;
    uint8_t stats_size = 0;   // taille affichée dans l'écran statistiques

    // Historique des coups (pour la liste) et pile d'annulation.
    int16_t mv[MV_MAX];
    int     mv_n = 0;
    uint8_t first_color = BLACK;

    // Pile d'annulation : les positions (11 Ko) sont dans Cold, en PSRAM.
    int undo_last[UNDO_MAX];
    int undo_mv[UNDO_MAX];
    int undo_n = 0;

    // Cadence IA / temps de jeu. next_move (respiration Tab contre Tab) repart
    // de 0 à chaque ouverture : jeu rouvert, ses 550 ms sont de toute façon
    // écoulées, et 0 veut déjà dire « pas d'attente ».
    uint32_t think_t0  = 0;
    uint32_t next_move = 0;
    uint32_t game_t0   = 0;
    uint32_t last_nvs  = 0;
    bool     dirty     = false;

    // Message de statut (2,6 s). Repart vide à l'ouverture : le menu principal,
    // opaque, couvre le HUD, un reste de message n'y serait de toute façon pas vu.
    char     msg[56] = "";
    uint32_t msg_until = 0;

    // Cache de rendu : n'invalider LVGL que sur les intersections qui changent.
    // Les objets étant neufs à chaque ouverture, il repart « invalide » avec eux.
    uint8_t vis_col[MAX_SQ];    // dernière couleur peinte (EMPTY/BLACK/WHITE)
    uint8_t vis_mode[MAX_SQ];   // 0=caché, 1=pierre, 2=pierre morte, 3=terr
    bool    vis_dirty = true;   // true → redraw intégral (changement de taille)
    int     vis_last_sq = -999;
    int     vis_pending = -999;
    int     vis_hint_sq = -999;
    int     think_pct_drawn = -1;

    // Géométrie du goban (recalculée à chaque changement de taille).
    int n = 9;
    int gap = 64;
    int ox = 0, oy = 0;
    int stone_r = 28;
    int plate_x = 0, plate_y = 0, plate_sz = 0;

    // =======================================================================
    // 3. Widgets (créés par build_ui() à chaque ouverture, détruits par close())
    // =======================================================================

    // --- Aire de jeu ---
    lv_obj_t* plate = nullptr;
    lv_obj_t* grid_h[MAX_N] = {};
    lv_obj_t* grid_v[MAX_N] = {};
    lv_obj_t* hoshi[9] = {};
    lv_obj_t* coord_c[MAX_N] = {};    // lettres (sous le plateau)
    lv_obj_t* coord_r[MAX_N] = {};    // chiffres (à gauche)
    lv_obj_t* stone[MAX_SQ] = {};     // pierre OU marque de territoire
    lv_obj_t* mark_last = nullptr;
    lv_obj_t* mark_ghost = nullptr;
    lv_obj_t* mark_hint = nullptr;

    // --- HUD ---
    lv_obj_t* pill[2] = {};           // 0 = Noir, 1 = Blanc
    lv_obj_t* pill_dot[2] = {};
    lv_obj_t* pill_name[2] = {};
    lv_obj_t* pill_sub[2] = {};
    lv_obj_t* h_move = nullptr;
    lv_obj_t* h_status = nullptr;

    // --- Panneau latéral (enfant de ui.field ; ui.panel est le calque des menus) ---
    lv_obj_t* panel = nullptr;
    lv_obj_t* p_title = nullptr;
    lv_obj_t* p_head[2] = {};
    lv_obj_t* ml_num[MOVE_ROWS] = {};
    lv_obj_t* ml_a[MOVE_ROWS] = {};
    lv_obj_t* ml_b[MOVE_ROWS] = {};
    lv_obj_t* think_lbl = nullptr;
    lv_obj_t* think_bar = nullptr;
    lv_obj_t* think_fill = nullptr;
    lv_obj_t* btn_ok = nullptr;       // « Valider » (coup en attente / marquage)
    lv_obj_t* btn_ok_lbl = nullptr;
    lv_obj_t* btn[4] = {};
    lv_obj_t* btn_lbl[4] = {};

    // --- Calque des menus ---
    lv_obj_t* m_title = nullptr;
    lv_obj_t* m_sub = nullptr;
    lv_obj_t* m_foot = nullptr;
    SlotMenu<N_SLOTS> slots;   // entrées des menus (game_common.h)
    // Carte de fin de partie (géométrie propre : le goban reste visible autour).
    lv_obj_t* card = nullptr;
    lv_obj_t* card_title = nullptr;
    lv_obj_t* card_sub = nullptr;
    lv_obj_t* card_line[CARD_LINES] = {};
    lv_obj_t* card_btn[2] = {};
    lv_obj_t* card_btn_lbl[2] = {};
};
static Mem* gs = nullptr;

// Gros tampon froid, touché une fois par coup joué ou annulé : PSRAM d'abord,
// comme l'EXT_RAM_BSS_ATTR qu'il remplace.
struct Cold {
    Pos undo[UNDO_MAX];   // pile d'annulation (11 Ko)
};
static Cold* gc = nullptr;

// ===========================================================================
// 4. Helpers LVGL
// ===========================================================================


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

static void msg(const char* t) {
    snprintf(gs->msg, sizeof(gs->msg), "%s", t);
    gs->msg_until = esphome::millis() + MSG_MS;
}

// Komi effectif : 0,5 seulement quand les pierres d'handicap sont réellement
// placées (Joueur contre Tab + handicap ≥ 2) — même condition que place_handicap.
static float effective_komi() {
    return (gs->cfg_mode == 0 && gs->cfg_hcap >= 2) ? KOMI_HCAP : KOMI;
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
    // Rangée 1..19 ; le modulo borne l'entier pour -O2 (-Wformat-truncation).
    snprintf(buf, len, "%c%u", col, static_cast<unsigned>(n - r) % 100u);
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
    memset(&gs->save, 0, sizeof(gs->save));
    gs->save.magic = SAVE_MAGIC;
    gs->save.size_idx = 0;
    gs->save.mode = 0;
    gs->save.human_color = BLACK;
    gs->save.ai_level = Ai::LVL_AMATEUR;
    gs->save.handicap = 0;
    gs->save.opt_confirm = 1;
    gs->save.opt_coords = 1;
    gs->save.opt_shake = 1;
    gs->save.opt_lastmark = 1;
    gs->save.opt_terr = 1;
}

void persist_load() {
    if (!gs) return;  // la sauvegarde n'est en mémoire que jeu ouvert
    if (!g_nvs.load(gs->save)) save_defaults();
    if (gs->save.size_idx >= GO_N_SIZES) gs->save.size_idx = 0;
    if (gs->save.mode > 2) gs->save.mode = 0;
    if (gs->save.ai_level >= GO_N_LEVELS) gs->save.ai_level = Ai::LVL_AMATEUR;
    if (gs->save.handicap == 1 || gs->save.handicap > 9) gs->save.handicap = 0;
    if (gs->save.human_color != WHITE) gs->save.human_color = BLACK;

    gs->cfg_size  = gs->save.size_idx;
    gs->cfg_mode  = gs->save.mode;
    gs->cfg_human = gs->save.human_color;
    gs->cfg_level = gs->save.ai_level;
    gs->cfg_hcap  = gs->save.handicap;
    gs->stats_size = gs->cfg_size;
}

void persist_save() {
    if (!gs || !g_nvs.ready()) return;
    gs->save.size_idx = gs->cfg_size;
    gs->save.mode = gs->cfg_mode;
    gs->save.human_color = gs->cfg_human;
    gs->save.ai_level = gs->cfg_level;
    gs->save.handicap = gs->cfg_hcap;
    g_nvs.save(gs->save);
    gs->dirty = false;
    gs->last_nvs = esphome::millis();
}

// Recopie les réglages de travail dans la sauvegarde EN RAM, sans toucher au
// flash : parcourir le menu de configuration ne doit pas écrire 10 fois en NVS.
static void stash_settings() {
    gs->save.size_idx = gs->cfg_size;
    gs->save.mode = gs->cfg_mode;
    gs->save.human_color = gs->cfg_human;
    gs->save.ai_level = gs->cfg_level;
    gs->save.handicap = gs->cfg_hcap;
    gs->dirty = true;
}

// Copie la position courante dans la sauvegarde (RAM) — l'écriture flash, elle,
// est différée par flush().
static void stash_position() {
    gs->save.has_game = gs->in_game ? 1 : 0;
    gs->save.n = gs->pos.n;
    gs->save.side = gs->pos.side;
    gs->save.ko = gs->pos.ko;
    gs->save.passes = gs->pos.passes;
    gs->save.move_no = gs->pos.move_no;
    gs->save.cap_b = gs->pos.captured_by_black;
    gs->save.cap_w = gs->pos.captured_by_white;
    gs->save.r_size = gs->cfg_size;
    gs->save.r_mode = gs->cfg_mode;
    gs->save.r_human = gs->cfg_human;
    gs->save.r_level = gs->cfg_level;
    gs->save.r_handicap = gs->cfg_hcap;
    memset(gs->save.board, 0, sizeof(gs->save.board));
    memcpy(gs->save.board, gs->pos.sq, (size_t)(gs->pos.n * gs->pos.n));
    gs->dirty = true;
}

static void flush(bool force) {
    if (!gs->dirty) return;
    const uint32_t now = esphome::millis();
    if (!force && (now - gs->last_nvs) < NVS_MIN_MS) return;
    persist_save();
}

static bool restore_position() {
    if (!gs->save.has_game) return false;
    const int n = gs->save.n;
    if (n != 9 && n != 13 && n != 19) return false;
    Engine::pos_init(gs->pos, n);
    gs->pos.side = gs->save.side;
    gs->pos.ko = gs->save.ko;
    gs->pos.passes = gs->save.passes;
    gs->pos.move_no = gs->save.move_no;
    gs->pos.captured_by_black = gs->save.cap_b;
    gs->pos.captured_by_white = gs->save.cap_w;
    memcpy(gs->pos.sq, gs->save.board, (size_t)(n * n));
    gs->cfg_size  = gs->save.r_size < GO_N_SIZES ? gs->save.r_size : 0;
    gs->cfg_mode  = gs->save.r_mode <= 2 ? gs->save.r_mode : 0;
    gs->cfg_human = gs->save.r_human == WHITE ? WHITE : BLACK;
    gs->cfg_level = gs->save.r_level < GO_N_LEVELS ? gs->save.r_level : 1;
    gs->cfg_hcap  = (gs->save.r_handicap == 1 || gs->save.r_handicap > 9) ? 0 : gs->save.r_handicap;
    return true;
}

static void record_result(int winner) {
    gs->save.games++;
    if (gs->cfg_mode == 0 && gs->cfg_size < GO_N_SIZES && gs->cfg_level < GO_N_LEVELS) {
        const uint8_t s = gs->cfg_size, lv = gs->cfg_level;
        if (winner == 2) gs->save.draws[s][lv]++;
        else {
            const bool human_black = (gs->cfg_human == BLACK);
            const bool human_won = human_black ? (winner == 0) : (winner == 1);
            if (human_won) gs->save.wins[s][lv]++;
            else gs->save.losses[s][lv]++;
        }
    }
    if (gs->game_t0) { gs->save.total_ms += esphome::millis() - gs->game_t0; gs->game_t0 = 0; }
    gs->in_game = false;
    gs->save.has_game = 0;
    gs->dirty = true;
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
    gs->n = gs->pos.n;
    const int zone_w = PANEL_X - 12;
    const int zone_h = FIELD_H - 8;
    int m = zone_w < zone_h ? zone_w : zone_h;
    gs->gap = m / (gs->n + 1);
    if (gs->gap > 68) gs->gap = 68;
    if (gs->gap < 20) gs->gap = 20;
    gs->stone_r = (gs->gap * 45) / 100;
    if (gs->stone_r < 6) gs->stone_r = 6;
    gs->plate_sz = gs->gap * (gs->n + 1);
    gs->plate_x = (PANEL_X - gs->plate_sz) / 2;
    gs->plate_y = (FIELD_H - gs->plate_sz) / 2;
    if (gs->plate_y < 4) gs->plate_y = 4;
    gs->ox = gs->plate_x + gs->gap;
    gs->oy = gs->plate_y + gs->gap;
}

static void layout_board() {
    compute_geometry();
    gs->vis_dirty = true;
    gs->think_pct_drawn = -1;
    const int span = gs->gap * (gs->n - 1);

    lv_obj_set_pos(gs->plate, gs->plate_x, gs->plate_y);
    lv_obj_set_size(gs->plate, gs->plate_sz, gs->plate_sz);

    for (int i = 0; i < MAX_N; i++) {
        const bool use = i < gs->n;
        show(gs->grid_h[i], use);
        show(gs->grid_v[i], use);
        if (!use) continue;
        // Les deux lignes extrêmes sont plus épaisses : c'est ce qui donne au
        // goban son cadre net, comme sur un vrai plateau.
        const int w = (i == 0 || i == gs->n - 1) ? 2 : 1;
        lv_obj_set_pos(gs->grid_h[i], gs->ox, gs->oy + i * gs->gap - w / 2);
        lv_obj_set_size(gs->grid_h[i], span + 1, w);
        lv_obj_set_pos(gs->grid_v[i], gs->ox + i * gs->gap - w / 2, gs->oy);
        lv_obj_set_size(gs->grid_v[i], w, span + 1);
    }

    // Points étoiles.
    int stars[9][2];
    int ns;
    if (gs->n == 9) {
        const int s[5][2] = {{2,2},{2,6},{6,2},{6,6},{4,4}};
        ns = 5; for (int i = 0; i < ns; i++) { stars[i][0]=s[i][0]; stars[i][1]=s[i][1]; }
    } else if (gs->n == 13) {
        const int s[5][2] = {{3,3},{3,9},{9,3},{9,9},{6,6}};
        ns = 5; for (int i = 0; i < ns; i++) { stars[i][0]=s[i][0]; stars[i][1]=s[i][1]; }
    } else {
        const int s[9][2] = {{3,3},{3,9},{3,15},{9,3},{9,9},{9,15},{15,3},{15,9},{15,15}};
        ns = 9; for (int i = 0; i < ns; i++) { stars[i][0]=s[i][0]; stars[i][1]=s[i][1]; }
    }
    const int hd = gs->gap >= 46 ? 10 : (gs->gap >= 32 ? 8 : 6);
    for (int i = 0; i < 9; i++) {
        const bool use = i < ns;
        show(gs->hoshi[i], use);
        if (!use) continue;
        lv_obj_set_pos(gs->hoshi[i], gs->ox + stars[i][1] * gs->gap - hd / 2,
                                   gs->oy + stars[i][0] * gs->gap - hd / 2);
        lv_obj_set_size(gs->hoshi[i], hd, hd);
        lv_obj_set_style_radius(gs->hoshi[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }

    // Coordonnées : lettres sous le plateau, chiffres à gauche.
    char buf[4];
    const bool co = gs->save.opt_coords != 0;
    for (int i = 0; i < MAX_N; i++) {
        const bool use = co && i < gs->n;
        show(gs->coord_c[i], use);
        show(gs->coord_r[i], use);
        if (!use) continue;
        col_letter(i, buf, sizeof(buf));
        set_text_if(gs->coord_c[i], buf);
        lv_obj_set_width(gs->coord_c[i], gs->gap);
        lv_obj_set_pos(gs->coord_c[i], gs->ox + i * gs->gap - gs->gap / 2,
                                     gs->oy + span + (gs->gap - 22) / 2);
        snprintf(buf, sizeof(buf), "%d", gs->n - i);
        set_text_if(gs->coord_r[i], buf);
        lv_obj_set_width(gs->coord_r[i], gs->gap - 10);
        lv_obj_set_pos(gs->coord_r[i], gs->plate_x + 4, gs->oy + i * gs->gap - 11);
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
    const int n = gs->pos.n;
    const int N = n * n;
    const bool scoring = (g_state == ST_MARKING || g_state == ST_SCORE);
    const bool terr_on = scoring && gs->save.opt_terr;
    const int td = gs->gap / 3 < 6 ? 6 : gs->gap / 3;
    const bool force = gs->vis_dirty;

    for (int i = 0; i < MAX_SQ; i++) {
        lv_obj_t* o = gs->stone[i];
        if (i >= N) {
            if (force || gs->vis_mode[i] != 0) {
                show(o, false);
                gs->vis_mode[i] = 0;
                gs->vis_col[i] = EMPTY;
            }
            continue;
        }
        const int r = i / n, c = i % n;
        const int cx = gs->ox + c * gs->gap, cy = gs->oy + r * gs->gap;
        const uint8_t col = gs->pos.sq[i];

        if (col == EMPTY) {
            if (terr_on && (gs->terr[i] == Engine::T_BLACK || gs->terr[i] == Engine::T_WHITE)) {
                const uint8_t want_col = gs->terr[i];
                if (force || gs->vis_mode[i] != 3 || gs->vis_col[i] != want_col) {
                    lv_obj_set_pos(o, cx - td / 2, cy - td / 2);
                    lv_obj_set_size(o, td, td);
                    lv_obj_set_style_radius(o, 3, LV_PART_MAIN);
                    set_bg(o, want_col == Engine::T_BLACK ? Pal::TERR_B : Pal::TERR_W,
                           (lv_opa_t) 220);
                    set_border(o, 0x000000, 0, LV_OPA_TRANSP);
                    show(o, true);
                    gs->vis_mode[i] = 3;
                    gs->vis_col[i] = want_col;
                }
            } else if (force || gs->vis_mode[i] != 0) {
                show(o, false);
                gs->vis_mode[i] = 0;
                gs->vis_col[i] = EMPTY;
            }
            continue;
        }
        const bool dead = scoring && gs->dead[i];
        const uint8_t mode = dead ? 2 : 1;
        if (force || gs->vis_mode[i] != mode || gs->vis_col[i] != col) {
            lv_obj_set_pos(o, cx - gs->stone_r, cy - gs->stone_r);
            style_stone(o, col == BLACK, gs->stone_r,
                        dead ? (lv_opa_t) 70 : (lv_opa_t) LV_OPA_COVER, dead);
            show(o, true);
            gs->vis_mode[i] = mode;
            gs->vis_col[i] = col;
        }
    }

    // Dernier coup.
    const bool last_ok = gs->save.opt_lastmark && gs->last_sq != PASS && gs->last_sq >= 0 &&
                         gs->last_sq < N && gs->pos.sq[gs->last_sq] != EMPTY && !scoring;
    if (last_ok) {
        if (force || gs->vis_last_sq != gs->last_sq) {
            const int r = gs->last_sq / n, c = gs->last_sq % n;
            const int d = gs->stone_r;
            lv_obj_set_pos(gs->mark_last, gs->ox + c * gs->gap - d / 2, gs->oy + r * gs->gap - d / 2);
            lv_obj_set_size(gs->mark_last, d, d);
            lv_obj_set_style_radius(gs->mark_last, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(gs->mark_last, LV_OPA_TRANSP, LV_PART_MAIN);
            set_border(gs->mark_last, Pal::LAST, 3, LV_OPA_COVER);
            show(gs->mark_last, true);
            gs->vis_last_sq = gs->last_sq;
        }
    } else if (force || gs->vis_last_sq != -1) {
        show(gs->mark_last, false);
        gs->vis_last_sq = -1;
    }

    // Coup en attente de validation.
    if (gs->pending >= 0 && gs->pending < N && g_state == ST_PLAYING) {
        if (force || gs->vis_pending != gs->pending || gs->vis_pending == -999) {
            const int r = gs->pending / n, c = gs->pending % n;
            lv_obj_set_pos(gs->mark_ghost, gs->ox + c * gs->gap - gs->stone_r,
                                         gs->oy + r * gs->gap - gs->stone_r);
            style_stone(gs->mark_ghost, gs->pos.side == BLACK, gs->stone_r, (lv_opa_t) 130, false);
            set_border(gs->mark_ghost, Pal::GHOST, 3, LV_OPA_COVER);
            show(gs->mark_ghost, true);
            gs->vis_pending = gs->pending;
        }
    } else if (force || gs->vis_pending != -1) {
        show(gs->mark_ghost, false);
        gs->vis_pending = -1;
    }

    // Indice.
    if (gs->hint_sq >= 0 && gs->hint_sq < N && esphome::millis() < gs->hint_until) {
        if (force || gs->vis_hint_sq != gs->hint_sq) {
            const int r = gs->hint_sq / n, c = gs->hint_sq % n;
            const int d = gs->stone_r + 4;
            lv_obj_set_pos(gs->mark_hint, gs->ox + c * gs->gap - d, gs->oy + r * gs->gap - d);
            lv_obj_set_size(gs->mark_hint, 2 * d, 2 * d);
            lv_obj_set_style_radius(gs->mark_hint, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            set_bg(gs->mark_hint, Pal::HINT, (lv_opa_t) 60);
            set_border(gs->mark_hint, Pal::HINT, 3, LV_OPA_COVER);
            show(gs->mark_hint, true);
            gs->vis_hint_sq = gs->hint_sq;
        }
    } else if (force || gs->vis_hint_sq != -1) {
        show(gs->mark_hint, false);
        gs->vis_hint_sq = -1;
    }

    gs->vis_dirty = false;
}

// ===========================================================================
// 10. Rendu du HUD et du panneau
// ===========================================================================

// Qui tient les pierres de cette couleur ?
static const char* seat_name(uint8_t color, char* buf, int len) {
    if (gs->cfg_mode == 1) { snprintf(buf, len, "Joueur %s", color == BLACK ? "1" : "2"); return buf; }
    if (gs->cfg_mode == 2) { snprintf(buf, len, "Tab %s", level_name(gs->cfg_level)); return buf; }
    if (color == gs->cfg_human) { snprintf(buf, len, "Vous"); return buf; }
    snprintf(buf, len, "Tab %s", level_name(gs->cfg_level));
    return buf;
}

static void render_hud() {
    char buf[64];
    const bool playing = (g_state == ST_PLAYING || g_state == ST_THINKING);

    for (int i = 0; i < 2; i++) {
        const uint8_t col = (i == 0) ? BLACK : WHITE;
        const bool active = playing && gs->pos.side == col;
        set_bg(gs->pill[i], active ? Pal::CARD_ON : Pal::CARD_BG, LV_OPA_COVER);
        set_border(gs->pill[i], active ? Pal::ACCENT : Pal::EDGE, 2,
                   active ? LV_OPA_COVER : LV_OPA_40);
        set_text_if(gs->pill_name[i], seat_name(col, buf, sizeof(buf)));
        const int caps = (col == BLACK) ? (int) gs->pos.captured_by_black
                                        : (int) gs->pos.captured_by_white;
        snprintf(buf, sizeof(buf), "%s  ·  %d prise%s",
                 col == BLACK ? "Noir" : "Blanc", caps, caps > 1 ? "s" : "");
        set_text_if(gs->pill_sub[i], buf);
        set_text_color_if(gs->pill_name[i], active ? Pal::ACCENT : Pal::TXT);
    }

    if (g_state == ST_MARKING || g_state == ST_SCORE) {
        // Comptage en direct : le joueur voit l'effet de chaque groupe marqué.
        snprintf(buf, sizeof(buf), "Noir %.1f   contre   Blanc %.1f",
                 (double) gs->score.black, (double) gs->score.white);
    } else if (gs->pos.move_no > 0) {
        snprintf(buf, sizeof(buf), "Coup %u  ·  komi %.1f",
                 (unsigned) gs->pos.move_no, (double) effective_komi());
    } else {
        snprintf(buf, sizeof(buf), "%s  ·  komi %.1f", size_name(gs->cfg_size),
                 (double) effective_komi());
    }
    set_text_if(gs->h_move, buf);

    char line[64];
    const char* st = "";
    uint32_t stc = Pal::TXT_DIM;
    if (gs->msg_until && (int32_t)(esphome::millis() - gs->msg_until) < 0) {
        st = gs->msg; stc = Pal::ACCENT;
    } else if (g_state == ST_THINKING) {
        st = "Le Tab reflechit..."; stc = Pal::THINK;
    } else if (g_state == ST_MARKING) {
        st = "Touchez les groupes MORTS, puis Valider"; stc = Pal::ACCENT;
    } else if (g_state == ST_PLAYING) {
        if (gs->pending >= 0) {
            char nm[8];
            sq_name(gs->pending, gs->pos.n, nm, sizeof(nm));
            snprintf(line, sizeof(line), "Touchez a nouveau %s pour valider", nm);
            st = line; stc = Pal::GHOST;
        } else {
            st = (gs->pos.side == BLACK) ? "Au tour de Noir" : "Au tour de Blanc";
            stc = Pal::TXT_DIM;
        }
    }
    set_text_if(gs->h_status, st);
    set_text_color_if(gs->h_status, stc);
}

static void render_movelist() {
    char buf[16];
    const int rows = (gs->mv_n + 1) / 2;
    int start = rows - MOVE_ROWS;
    if (start < 0) start = 0;

    set_text_if(gs->p_head[0], gs->first_color == BLACK ? "Noir" : "Blanc");
    set_text_if(gs->p_head[1], gs->first_color == BLACK ? "Blanc" : "Noir");

    for (int i = 0; i < MOVE_ROWS; i++) {
        const int row = start + i;
        if (row >= rows) {
            set_text_if(gs->ml_num[i], "");
            set_text_if(gs->ml_a[i], "");
            set_text_if(gs->ml_b[i], "");
            continue;
        }
        snprintf(buf, sizeof(buf), "%d.", row + 1);
        set_text_if(gs->ml_num[i], buf);
        sq_name(gs->mv[row * 2], gs->pos.n, buf, sizeof(buf));
        set_text_if(gs->ml_a[i], buf);
        if (row * 2 + 1 < gs->mv_n) {
            sq_name(gs->mv[row * 2 + 1], gs->pos.n, buf, sizeof(buf));
            set_text_if(gs->ml_b[i], buf);
        } else {
            set_text_if(gs->ml_b[i], "");
        }
    }
}

// Les 4 boutons changent de rôle selon l'état — un seul jeu de widgets.
static void render_panel_buttons() {
    // `const` des deux côtés : tables en flash, pas 32 o de .data en RAM interne.
    static const char* const PLAY_LBL[4] = {"Passer", "Annuler", "Indice", "Menu"};
    static const char* const MARK_LBL[4] = {"Tout vivant", "Reprendre", "", "Menu"};
    const bool marking = (g_state == ST_MARKING);
    const char* const* L = marking ? MARK_LBL : PLAY_LBL;

    for (int i = 0; i < 4; i++) {
        const bool on = L[i][0] != 0;
        show(gs->btn[i], on);
        if (!on) continue;
        set_text_if(gs->btn_lbl[i], L[i]);
    }
    // Bouton large : valider le coup en attente, ou valider le score.
    if (marking) {
        set_text_if(gs->btn_ok_lbl, "Valider le score");
        set_bg(gs->btn_ok, Pal::CARD_BG, LV_OPA_COVER);
        set_border(gs->btn_ok, Pal::GOOD, 2, LV_OPA_80);
        set_text_color_if(gs->btn_ok_lbl, Pal::GOOD);
        show(gs->btn_ok, true);
    } else if (g_state == ST_PLAYING && gs->pending >= 0) {
        char nm[8], buf[24];
        sq_name(gs->pending, gs->pos.n, nm, sizeof(nm));
        snprintf(buf, sizeof(buf), "Jouer %s", nm);
        set_text_if(gs->btn_ok_lbl, buf);
        set_bg(gs->btn_ok, Pal::CARD_BG, LV_OPA_COVER);
        set_border(gs->btn_ok, Pal::GHOST, 2, LV_OPA_COVER);
        set_text_color_if(gs->btn_ok_lbl, Pal::GHOST);
        show(gs->btn_ok, true);
    } else {
        show(gs->btn_ok, false);
    }

    const bool think = (g_state == ST_THINKING);
    show(gs->think_lbl, think);
    show(gs->think_bar, think);
    if (think) {
        const int pct = Ai::progress_pct();
        if (pct != gs->think_pct_drawn) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Reflexion  %d %%", pct);
            set_text_if(gs->think_lbl, buf);
            const int w = (270 * pct) / 100;
            lv_obj_set_size(gs->think_fill, w < 2 ? 2 : w, 8);
            gs->think_pct_drawn = pct;
        }
    } else {
        gs->think_pct_drawn = -1;
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
    if (gs->cfg_mode == 1) return true;
    if (gs->cfg_mode == 2) return false;
    return gs->pos.side == gs->cfg_human;
}

static void push_undo() {
    if (gs->undo_n >= UNDO_MAX) {
        for (int i = 1; i < UNDO_MAX; i++) {
            gc->undo[i - 1] = gc->undo[i];
            gs->undo_last[i - 1] = gs->undo_last[i];
            gs->undo_mv[i - 1] = gs->undo_mv[i];
        }
        gs->undo_n = UNDO_MAX - 1;
    }
    gc->undo[gs->undo_n] = gs->pos;
    gs->undo_last[gs->undo_n] = gs->last_sq;
    gs->undo_mv[gs->undo_n] = gs->mv_n;
    gs->undo_n++;
}

static void push_move(int sq) {
    if (gs->mv_n < MV_MAX) gs->mv[gs->mv_n++] = (int16_t) sq;
}

static void apply_move(int sq) {
    const bool by_human = is_human_turn();
    push_undo();
    int played = sq;
    if (!Engine::play(gs->pos, sq)) {
        gs->undo_n--;                       // rien n'a bougé : on défait la pile
        if (by_human) { msg("Coup illegal"); refresh_all(); return; }
        // Un coup illégal proposé par l'IA ne doit JAMAIS bloquer la partie :
        // on passe à sa place et la partie continue.
        msg("Le Tab passe");
        push_undo();
        Engine::play(gs->pos, PASS);
        played = PASS;
    }
    push_move(played);
    gs->last_sq = played;
    gs->pending = -1;
    gs->hint_sq = -1;
    stash_position();
    refresh_all();
    after_move();
}

static void after_move() {
    if (Engine::is_over(gs->pos)) { enter_marking(); return; }
    if (is_human_turn()) {
        g_state = ST_PLAYING;
        refresh_all();
    } else {
        begin_thinking();
    }
}

static void begin_thinking() {
    if (Engine::is_over(gs->pos)) { enter_marking(); return; }
    g_state = ST_THINKING;
    gs->think_t0 = esphome::millis();
    gs->think_pct_drawn = -1;
    Ai::begin(gs->pos, (Ai::Level) gs->cfg_level,
              (uint32_t) gs->pos.move_no * 2654435761u ^ esphome::millis(),
              effective_komi());
    refresh_all();
}

static void do_pass() {
    if (g_state != ST_PLAYING || !is_human_turn()) return;
    gs->pending = -1;
    msg(gs->pos.passes == 1 ? "Passe — fin de partie" : "Passe");
    apply_move(PASS);
}

static void do_undo() {
    if (g_state != ST_PLAYING && g_state != ST_THINKING && g_state != ST_MARKING) return;
    Ai::abort();
    gs->pending = -1;
    // Depuis l'écran de marquage, « Reprendre » revient au coup d'avant la
    // seconde passe : on efface aussi les marques de groupes morts.
    if (g_state == ST_MARKING) memset(gs->dead, 0, sizeof(gs->dead));
    if (gs->undo_n <= 0) { g_state = ST_PLAYING; msg("Rien a annuler"); refresh_all(); return; }
    gs->undo_n--;
    gs->pos = gc->undo[gs->undo_n];
    gs->last_sq = gs->undo_last[gs->undo_n];
    gs->mv_n = gs->undo_mv[gs->undo_n];
    // Contre le Tab, on remonte aussi sa réponse : sinon « Annuler » ne rend pas
    // la main au joueur.
    if (gs->cfg_mode == 0 && gs->pos.side != gs->cfg_human && gs->undo_n > 0) {
        gs->undo_n--;
        gs->pos = gc->undo[gs->undo_n];
        gs->last_sq = gs->undo_last[gs->undo_n];
        gs->mv_n = gs->undo_mv[gs->undo_n];
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
    Ai::begin(gs->pos, Ai::LVL_SOLID, esphome::millis(), effective_komi());
    for (int i = 0; i < 4 && !Ai::ready(); i++) Ai::step(8);
    const int sq = Ai::best_sq();
    Ai::abort();
    g_state = ST_PLAYING;
    if (sq == Ai::RESIGN || sq == PASS || sq < 0) {
        gs->hint_sq = -1;
        msg("Indice : passer");
    } else {
        gs->hint_sq = sq;
        gs->hint_until = esphome::millis() + HINT_MS;
        char nm[8], buf[32];
        sq_name(sq, gs->pos.n, nm, sizeof(nm));
        snprintf(buf, sizeof(buf), "Indice : %s", nm);
        msg(buf);
    }
    refresh_all();
}

// --- Fin de partie : marquage des pierres mortes --------------------------

static void refresh_territory() {
    Engine::territory_map(gs->pos, gs->dead, gs->terr);
    Engine::score_chinese(gs->pos, effective_komi(), gs->dead, gs->score);
    gs->vis_dirty = true;  // pastilles territoire / opacités morts
}

static void enter_marking() {
    Ai::abort();
    // Ne réinitialise gs->dead que si on arrive depuis le jeu (pas une reprise
    // après pause pendant le marquage — géré par enter_playing).
    const bool first = (g_state != ST_MARKING);
    g_state = ST_MARKING;
    gs->pending = -1;
    if (first) {
        memset(gs->dead, 0, sizeof(gs->dead));
        msg("Deux passes : marquez les groupes morts");
    }
    refresh_territory();
    refresh_all();
}

static void finish_scoring() {
    refresh_territory();
    gs->resigned = false;
    gs->winner = 2;
    if (gs->score.black > gs->score.white) gs->winner = 0;
    else if (gs->score.white > gs->score.black) gs->winner = 1;
    record_result(gs->winner);
    g_state = ST_SCORE;
    refresh_all();
    show_score_card();
    flush(true);
}

static void resign(uint8_t who_resigns) {
    refresh_territory();
    gs->resigned = true;
    gs->winner = (who_resigns == BLACK) ? 1 : 0;
    record_result(gs->winner);
    g_state = ST_SCORE;
    refresh_all();
    show_score_card();
    flush(true);
}

// --- Nouvelle partie ------------------------------------------------------

static void start_new() {
    Ai::abort();
    Engine::pos_init(gs->pos, SIZE_TAB[gs->cfg_size < GO_N_SIZES ? gs->cfg_size : 0]);
    if (gs->cfg_mode == 0 && gs->cfg_hcap >= 2) Engine::place_handicap(gs->pos, gs->cfg_hcap);
    gs->first_color = gs->pos.side;
    gs->last_sq = PASS;
    gs->pending = -1;
    gs->hint_sq = -1;
    gs->mv_n = 0;
    gs->undo_n = 0;
    gs->in_game = true;
    gs->game_t0 = esphome::millis();
    memset(gs->dead, 0, sizeof(gs->dead));
    memset(gs->terr, 0, sizeof(gs->terr));
    layout_board();
    stash_position();
    enter_playing();
}

static void resume_game() {
    if (!restore_position()) { menu_main(); return; }
    // La liste des coups et la pile d'annulation ne sont pas persistées : on
    // reprend la position, pas l'historique. C'est assumé (et documenté au
    // README) — sauvegarder 512 coups en NVS à chaque partie n'a pas de sens.
    gs->first_color = (gs->cfg_hcap >= 2) ? WHITE : BLACK;
    gs->last_sq = PASS;
    gs->pending = -1;
    gs->hint_sq = -1;
    gs->mv_n = 0;
    gs->undo_n = 0;
    gs->in_game = true;
    gs->game_t0 = esphome::millis();
    memset(gs->dead, 0, sizeof(gs->dead));
    memset(gs->terr, 0, sizeof(gs->terr));
    layout_board();
    msg("Partie reprise");
    enter_playing();
}

static void enter_playing() {
    show(gs->ui.panel, false);
    show(gs->card, false);
    // Reprise après pause pendant le marquage : conserver gs->dead.
    if (Engine::is_over(gs->pos) && gs->in_game) {
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

// Entrée de menu à position fixe (build_ui) ; `on` = false la masque.
static void slot_set(int i, const char* title, const char* desc, uint32_t col, bool on) {
    if (i < 0 || i >= N_SLOTS) return;
    show(gs->slots.box[i], on);
    if (!on) return;
    set_text_if(gs->slots.title[i], title);
    set_text_if(gs->slots.desc[i], desc ? desc : "");
    set_text_color_if(gs->slots.title[i], col);
    set_border(gs->slots.box[i], col, 2, LV_OPA_40);
}

static void menu_open(const char* title, const char* sub, const char* foot) {
    show_front(gs->ui.panel, true);
    set_bg(gs->ui.panel, Pal::VOID_BG, LV_OPA_COVER);
    show(gs->card, false);
    set_text_if(gs->m_title, title);
    set_text_if(gs->m_sub, sub ? sub : "");
    set_text_if(gs->m_foot, foot ? foot : "");
    gs->slots.hide_from(0);
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
    if (gs->in_game) {
        slot_set(i++, "Reprendre la partie", "Retour au goban", Pal::GOOD, true);
    } else if (gs->save.has_game) {
        char d[48];
        snprintf(d, sizeof(d), "%dx%d, coup %u", (int) gs->save.n, (int) gs->save.n,
                 (unsigned) gs->save.move_no);
        slot_set(i++, "Reprendre la sauvegarde", d, Pal::GOOD, true);
    }
    slot_set(i++, "Nouvelle partie", "Taille, mode, niveau, handicap", Pal::ACCENT, true);
    slot_set(i++, "Statistiques", "Bilan face au Tab", Pal::TXT, true);
    slot_set(i++, "Reglages", "Confirmation, coordonnees, secousse", Pal::TXT_DIM, true);
    slot_set(i++, "Quitter", "Retour a l'arcade", Pal::DANGER, true);
    gs->slots.hide_from(i);
    flush(true);
}

static void menu_pause() {
    g_state = ST_PAUSE;
    Ai::abort();
    char sub[64];
    snprintf(sub, sizeof(sub), "%s  ·  %s  ·  coup %u", size_name(gs->cfg_size),
             mode_name(gs->cfg_mode), (unsigned) gs->pos.move_no);
    menu_open("Pause", sub, "Le goban vous attend.");
    slot_set(0, "Reprendre la partie", "", Pal::GOOD, true);
    slot_set(1, "Nouvelle partie", "Changer les reglages", Pal::ACCENT, true);
    slot_set(2, "Abandonner", "L'adversaire gagne", Pal::DANGER, true);
    slot_set(3, "Statistiques", "", Pal::TXT, true);
    slot_set(4, "Reglages", "", Pal::TXT_DIM, true);
    slot_set(5, "Quitter le jeu", "La partie est sauvegardee", Pal::TXT_MUTED, true);
    gs->slots.hide_from(6);
    stash_position();
    flush(true);
}

static void menu_setup() {
    g_state = ST_MENU_SETUP;
    char sub[80], b[64];
    snprintf(sub, sizeof(sub), "%s  ·  %s", size_name(gs->cfg_size), mode_name(gs->cfg_mode));
    menu_open("Nouvelle partie", sub, "Touchez une ligne pour changer sa valeur.");

    slot_set(0, size_name(gs->cfg_size), "Taille du goban  —  9x9 / 13x13 / 19x19",
             Pal::WOOD, true);
    slot_set(1, mode_name(gs->cfg_mode), "Adversaire", Pal::THINK, true);

    snprintf(b, sizeof(b), "Vous jouez %s", gs->cfg_human == BLACK ? "Noir (premier)" : "Blanc");
    slot_set(2, b, "Couleur du joueur", Pal::TXT, gs->cfg_mode == 0);

    snprintf(b, sizeof(b), "Niveau : %s", level_name(gs->cfg_level));
    slot_set(3, b, "Force du Tab", Pal::ACCENT, gs->cfg_mode != 1);

    if (gs->cfg_hcap >= 2) snprintf(b, sizeof(b), "Handicap : %d pierres", (int) gs->cfg_hcap);
    else                 snprintf(b, sizeof(b), "Handicap : aucun");
    slot_set(4, b, "Pierres offertes a Noir (Blanc commence)", Pal::WOOD, gs->cfg_mode == 0);

    slot_set(5, "Jouer !",
             effective_komi() == KOMI_HCAP ? "Komi 0,5 pour Blanc" : "Komi 6,5 pour Blanc",
             Pal::GOOD, true);
    slot_set(6, "Retour", "", Pal::TXT_MUTED, true);
}

static void menu_stats() {
    g_state = ST_MENU_STATS;
    char sub[64];
    const unsigned mins = (unsigned) (gs->save.total_ms / 60000u);
    snprintf(sub, sizeof(sub), "%u parties  ·  %u min de jeu",
             (unsigned) gs->save.games, mins);
    menu_open("Statistiques", sub, "Comptabilise uniquement le mode Joueur contre Tab.");

    char t[64], d[64];
    const uint8_t s = gs->stats_size < GO_N_SIZES ? gs->stats_size : 0;
    for (int lv = 0; lv < GO_N_LEVELS; lv++) {
        const unsigned w = gs->save.wins[s][lv], dr = gs->save.draws[s][lv], l = gs->save.losses[s][lv];
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
    slot_set(0, gs->save.opt_confirm ? "Confirmation du coup : ACTIVEE"
                                   : "Confirmation du coup : DESACTIVEE",
             "Un premier toucher place un fantome, le second valide", Pal::ACCENT, true);
    slot_set(1, gs->save.opt_coords ? "Coordonnees : AFFICHEES" : "Coordonnees : MASQUEES",
             "Lettres A..T et chiffres autour du goban", Pal::TXT, true);
    slot_set(2, gs->save.opt_lastmark ? "Dernier coup : MARQUE" : "Dernier coup : NON MARQUE",
             "Anneau rouge sur la derniere pierre posee", Pal::TXT, true);
    slot_set(3, gs->save.opt_terr ? "Apercu du territoire : ACTIVE" : "Apercu du territoire : DESACTIVE",
             "Pastilles de territoire pendant le comptage", Pal::TXT, true);
    slot_set(4, gs->save.opt_shake ? "Secousse = indice : ACTIVE" : "Secousse = indice : DESACTIVE",
             "Detection BMI270", Pal::TXT_DIM, true);
    slot_set(5, "Retour", "", Pal::TXT_MUTED, true);
    gs->slots.hide_from(6);
}

static void menu_confirm_reset() {
    g_state = ST_CONFIRM_RESET;
    menu_open("Effacer les statistiques ?", "Victoires, nulles et defaites de toutes les tailles",
              "Cette action est definitive.");
    slot_set(0, "Oui, tout effacer", "", Pal::DANGER, true);
    slot_set(1, "Annuler", "", Pal::TXT_MUTED, true);
    gs->slots.hide_from(2);
}

// --- Carte de fin de partie ----------------------------------------------

static void show_score_card() {
    // Panneau semi-transparent : le goban et son territoire restent visibles.
    show_front(gs->ui.panel, true);
    set_bg(gs->ui.panel, Pal::VOID_BG, (lv_opa_t) 195);
    gs->slots.hide_from(0);
    set_text_if(gs->m_title, "");
    set_text_if(gs->m_sub, "");
    set_text_if(gs->m_foot, "");
    show(gs->card, true);

    char b[80];
    if (gs->winner == 0) { set_text_if(gs->card_title, "Noir l'emporte"); set_text_color_if(gs->card_title, Pal::TXT); }
    else if (gs->winner == 1) { set_text_if(gs->card_title, "Blanc l'emporte"); set_text_color_if(gs->card_title, Pal::TXT); }
    else { set_text_if(gs->card_title, "Partie nulle"); set_text_color_if(gs->card_title, Pal::ACCENT); }

    if (gs->resigned) {
        snprintf(b, sizeof(b), "Abandon  —  comptage indicatif : %.1f contre %.1f",
                 (double) gs->score.black, (double) gs->score.white);
    } else {
        const float diff = gs->score.black - gs->score.white;
        const float ad = diff < 0 ? -diff : diff;
        snprintf(b, sizeof(b), "%.1f  contre  %.1f      (ecart %.1f)",
                 (double) gs->score.black, (double) gs->score.white, (double) ad);
    }
    set_text_if(gs->card_sub, b);

    snprintf(b, sizeof(b), "Noir   pierres %d   territoire %d", gs->score.black_stones, gs->score.black_terr);
    set_text_if(gs->card_line[0], b);
    snprintf(b, sizeof(b), "Blanc  pierres %d   territoire %d   komi %.1f",
             gs->score.white_stones, gs->score.white_terr, (double) effective_komi());
    set_text_if(gs->card_line[1], b);
    snprintf(b, sizeof(b), "Groupes morts retires : %d noirs, %d blancs",
             gs->score.black_dead, gs->score.white_dead);
    set_text_if(gs->card_line[2], b);
    snprintf(b, sizeof(b), "Points neutres (dame) : %d", gs->score.dame);
    set_text_if(gs->card_line[3], b);
    snprintf(b, sizeof(b), "Prisonniers de la partie : Noir %u, Blanc %u",
             (unsigned) gs->pos.captured_by_black, (unsigned) gs->pos.captured_by_white);
    set_text_if(gs->card_line[4], b);
    snprintf(b, sizeof(b), "%s  ·  %s  ·  %u coups", size_name(gs->cfg_size),
             mode_name(gs->cfg_mode), (unsigned) gs->pos.move_no);
    set_text_if(gs->card_line[5], b);

    set_text_if(gs->card_btn_lbl[0], "Revanche");
    set_text_if(gs->card_btn_lbl[1], "Menu principal");
}

// ===========================================================================
// 13. Événements
// ===========================================================================

// Intersection la plus proche du point touché, ou -1 si le doigt est trop loin.
static int hit_intersection(int x, int y) {
    // Hors du plateau (panneau latéral, tapis) : ce n'est pas un coup.
    if (x < gs->plate_x || y < gs->plate_y ||
        x >= gs->plate_x + gs->plate_sz || y >= gs->plate_y + gs->plate_sz) return -1;
    int best = -1, best_d = 1 << 30;
    for (int r = 0; r < gs->n; r++) {
        for (int c = 0; c < gs->n; c++) {
            const int dx = x - (gs->ox + c * gs->gap);
            const int dy = y - (gs->oy + r * gs->gap);
            const int d = dx * dx + dy * dy;
            if (d < best_d) { best_d = d; best = Engine::idx(r, c, gs->n); }
        }
    }
    // Tolérance : un demi-écart. Au-delà, le doigt visait autre chose.
    const int lim = (gs->gap * gs->gap) / 2;
    return (best_d <= lim) ? best : -1;
}

static void on_board_tap(int x, int y) {
    const int sq = hit_intersection(x, y);

    if (g_state == ST_MARKING) {
        if (sq < 0 || gs->pos.sq[sq] == EMPTY) return;
        const uint8_t v = gs->dead[sq] ? 0 : 1;
        Engine::mark_chain(gs->pos, sq, gs->dead, v);
        refresh_territory();
        refresh_all();
        return;
    }
    if (g_state != ST_PLAYING || !is_human_turn()) return;

    if (sq < 0) {                       // toucher hors du goban : on annule l'attente
        if (gs->pending >= 0) { gs->pending = -1; refresh_all(); }
        return;
    }
    if (!Engine::is_legal(gs->pos, sq)) {
        gs->pending = -1;
        msg(gs->pos.sq[sq] != EMPTY ? "Intersection occupee" : "Coup interdit (ko ou suicide)");
        refresh_all();
        return;
    }
    if (!gs->save.opt_confirm) { apply_move(sq); return; }
    if (gs->pending == sq) { apply_move(sq); return; }
    gs->pending = sq;
    refresh_all();
}

// Posé sur le conteneur YAML `field` par build_ui() et retiré par close() ;
// la garde couvre quand même un tap jeu fermé : on_board_tap lit la géométrie
// du bloc AVANT de tester l'état.
static void field_cb(lv_event_t* e) {
    if (!gs || lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_indev_t* in = lv_indev_active();
    if (!in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    lv_area_t a;
    lv_obj_get_coords(gs->ui.field, &a);
    on_board_tap(pt.x - a.x1, pt.y - a.y1);
}

static void btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const int i = (int) (intptr_t) lv_event_get_user_data(e);
    if (g_state == ST_MARKING) {
        if (i == 0) {                      // Tout vivant
            memset(gs->dead, 0, sizeof(gs->dead));
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
    if (g_state == ST_PLAYING && gs->pending >= 0) apply_move(gs->pending);
}

static void hud_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_state == ST_PLAYING || g_state == ST_THINKING || g_state == ST_MARKING)
        menu_pause();
}

static void card_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const int i = (int) (intptr_t) lv_event_get_user_data(e);
    show(gs->card, false);
    if (i == 0) start_new();
    else menu_main();
}

static void slot_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const int i = (int) (intptr_t) lv_event_get_user_data(e);

    switch (g_state) {
        case ST_MENU_MAIN: {
            // Le nombre d'entrées varie selon qu'une partie est reprenable.
            const bool resumable = gs->in_game || gs->save.has_game;
            int k = i;
            if (resumable) {
                if (k == 0) { if (gs->in_game) enter_playing(); else resume_game(); return; }
                k--;
            }
            if (k == 0) menu_setup();
            else if (k == 1) { gs->stats_size = gs->cfg_size; menu_stats(); }
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
                show(gs->ui.panel, false);
                resign(gs->cfg_mode == 0 ? gs->cfg_human : gs->pos.side);
            }
            else if (i == 3) { gs->stats_size = gs->cfg_size; menu_stats(); }
            else if (i == 4) menu_opts();
            else if (i == 5) close();
            break;
        case ST_MENU_SETUP:
            if (i == 0) gs->cfg_size = (uint8_t) ((gs->cfg_size + 1) % GO_N_SIZES);
            else if (i == 1) gs->cfg_mode = (uint8_t) ((gs->cfg_mode + 1) % 3);
            else if (i == 2 && gs->cfg_mode == 0)
                gs->cfg_human = (gs->cfg_human == BLACK) ? WHITE : BLACK;
            else if (i == 3 && gs->cfg_mode != 1)
                gs->cfg_level = (uint8_t) ((gs->cfg_level + 1) % GO_N_LEVELS);
            else if (i == 4 && gs->cfg_mode == 0)
                gs->cfg_hcap = (uint8_t) (gs->cfg_hcap == 0 ? 2 : (gs->cfg_hcap >= 9 ? 0 : gs->cfg_hcap + 1));
            else if (i == 5) { persist_save(); start_new(); return; }
            else if (i == 6) { persist_save(); gs->in_game ? menu_pause() : menu_main(); return; }
            stash_settings();
            menu_setup();
            break;
        case ST_MENU_STATS:
            if (i == 4) { gs->stats_size = (uint8_t) ((gs->stats_size + 1) % GO_N_SIZES); menu_stats(); }
            else if (i == 5) menu_confirm_reset();
            else if (i == 6) { gs->in_game ? menu_pause() : menu_main(); }
            break;
        case ST_MENU_OPTS:
            if (i == 0) gs->save.opt_confirm ^= 1;
            else if (i == 1) { gs->save.opt_coords ^= 1; layout_board(); }
            else if (i == 2) gs->save.opt_lastmark ^= 1;
            else if (i == 3) gs->save.opt_terr ^= 1;
            else if (i == 4) gs->save.opt_shake ^= 1;
            else if (i == 5) { persist_save(); gs->in_game ? menu_pause() : menu_main(); return; }
            gs->dirty = true;
            menu_opts();
            break;
        case ST_CONFIRM_RESET:
            if (i == 0) {
                memset(gs->save.wins, 0, sizeof(gs->save.wins));
                memset(gs->save.draws, 0, sizeof(gs->save.draws));
                memset(gs->save.losses, 0, sizeof(gs->save.losses));
                gs->save.games = 0;
                gs->save.total_ms = 0;
                persist_save();
            }
            menu_stats();
            break;
        default: break;
    }
}

// ===========================================================================
// 14. Construction de l'UI (à chaque ouverture ; close() la détruit)
// ===========================================================================

static void build_ui() {
    set_bg(gs->ui.root, Pal::VOID_BG, LV_OPA_COVER);
    set_bg(gs->ui.hud, Pal::HUD_BG, LV_OPA_COVER);
    set_bg(gs->ui.field, Pal::FIELD_BG, LV_OPA_COVER);
    set_bg(gs->ui.panel, Pal::VOID_BG, LV_OPA_COVER);
    // Le calque des menus DOIT absorber les taps, sinon ils traversent jusqu'au
    // goban qui vit en dessous.
    lv_obj_add_flag(gs->ui.panel, LV_OBJ_FLAG_CLICKABLE);
    show(gs->ui.panel, false);

    // --- Plateau ---------------------------------------------------------
    gs->plate = mk_rect(gs->ui.field);
    set_bg_grad(gs->plate, Pal::WOOD, Pal::WOOD_DK, LV_OPA_COVER);
    set_border(gs->plate, Pal::WOOD_EDGE, 3, LV_OPA_COVER);
    lv_obj_set_style_radius(gs->plate, 8, LV_PART_MAIN);

    for (int i = 0; i < MAX_N; i++) {
        gs->grid_h[i] = mk_rect(gs->ui.field);
        set_bg(gs->grid_h[i], Pal::GRID, LV_OPA_COVER);
        gs->grid_v[i] = mk_rect(gs->ui.field);
        set_bg(gs->grid_v[i], Pal::GRID, LV_OPA_COVER);
    }
    for (int i = 0; i < 9; i++) {
        gs->hoshi[i] = mk_rect(gs->ui.field);
        set_bg(gs->hoshi[i], Pal::HOSHI, LV_OPA_COVER);
    }
    for (int i = 0; i < MAX_N; i++) {
        gs->coord_c[i] = mk_label(gs->ui.field, gs->ui.f_small, Pal::WOOD_EDGE);
        lv_obj_set_style_text_align(gs->coord_c[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        gs->coord_r[i] = mk_label(gs->ui.field, gs->ui.f_small, Pal::WOOD_EDGE);
        lv_obj_set_style_text_align(gs->coord_r[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    }
    for (int i = 0; i < MAX_SQ; i++) {
        gs->stone[i] = mk_rect(gs->ui.field);
        show(gs->stone[i], false);
    }
    gs->mark_last  = mk_rect(gs->ui.field);
    gs->mark_ghost = mk_rect(gs->ui.field);
    gs->mark_hint  = mk_rect(gs->ui.field);
    show(gs->mark_last, false);
    show(gs->mark_ghost, false);
    show(gs->mark_hint, false);

    lv_obj_add_flag(gs->ui.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gs->ui.field, field_cb, LV_EVENT_CLICKED, nullptr);

    // --- HUD --------------------------------------------------------------
    for (int i = 0; i < 2; i++) {
        gs->pill[i] = mk_rect(gs->ui.hud);
        lv_obj_set_size(gs->pill[i], 296, 46);
        lv_obj_set_pos(gs->pill[i], i == 0 ? 10 : 974, 7);
        lv_obj_set_style_radius(gs->pill[i], 12, LV_PART_MAIN);
        set_bg(gs->pill[i], Pal::CARD_BG, LV_OPA_COVER);

        gs->pill_dot[i] = mk_rect(gs->pill[i]);
        lv_obj_set_size(gs->pill_dot[i], 26, 26);
        lv_obj_set_pos(gs->pill_dot[i], 12, 10);
        lv_obj_set_style_radius(gs->pill_dot[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        if (i == 0) set_bg_grad(gs->pill_dot[i], Pal::STONE_B_H, Pal::STONE_B, LV_OPA_COVER);
        else        set_bg_grad(gs->pill_dot[i], Pal::STONE_W_H, Pal::STONE_W, LV_OPA_COVER);

        gs->pill_name[i] = mk_label(gs->pill[i], gs->ui.f_small, Pal::TXT);
        lv_obj_set_pos(gs->pill_name[i], 48, 3);
        gs->pill_sub[i] = mk_label(gs->pill[i], gs->ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(gs->pill_sub[i], 48, 24);
    }
    gs->h_move = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_set_width(gs->h_move, 640);
    lv_obj_set_style_text_align(gs->h_move, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(gs->h_move, 320, 6);
    gs->h_status = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_set_width(gs->h_status, 640);
    lv_obj_set_style_text_align(gs->h_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(gs->h_status, 320, 31);

    lv_obj_add_flag(gs->ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gs->ui.hud, hud_cb, LV_EVENT_CLICKED, nullptr);

    // --- Panneau latéral --------------------------------------------------
    gs->panel = mk_rect(gs->ui.field);
    lv_obj_set_pos(gs->panel, PANEL_X, PANEL_Y);
    lv_obj_set_size(gs->panel, PANEL_W, PANEL_H);
    lv_obj_set_style_radius(gs->panel, 14, LV_PART_MAIN);
    set_bg(gs->panel, Pal::PANEL_BG, LV_OPA_COVER);
    set_border(gs->panel, Pal::EDGE, 1, LV_OPA_60);

    gs->p_title = mk_label(gs->panel, gs->ui.f_small, Pal::ACCENT);
    lv_obj_set_pos(gs->p_title, 16, 12);
    set_text_if(gs->p_title, "COUPS");
    gs->p_head[0] = mk_label(gs->panel, gs->ui.f_small, Pal::TXT_MUTED);
    lv_obj_set_pos(gs->p_head[0], 108, 12);
    gs->p_head[1] = mk_label(gs->panel, gs->ui.f_small, Pal::TXT_MUTED);
    lv_obj_set_pos(gs->p_head[1], 208, 12);

    for (int i = 0; i < MOVE_ROWS; i++) {
        const int y = 46 + i * 27;
        gs->ml_num[i] = mk_label(gs->panel, gs->ui.f_mono, Pal::TXT_MUTED);
        lv_obj_set_pos(gs->ml_num[i], 16, y);
        gs->ml_a[i] = mk_label(gs->panel, gs->ui.f_mono, Pal::TXT);
        lv_obj_set_pos(gs->ml_a[i], 108, y);
        gs->ml_b[i] = mk_label(gs->panel, gs->ui.f_mono, Pal::TXT_DIM);
        lv_obj_set_pos(gs->ml_b[i], 208, y);
    }

    gs->think_lbl = mk_label(gs->panel, gs->ui.f_small, Pal::THINK);
    lv_obj_set_pos(gs->think_lbl, 16, 380);
    gs->think_bar = mk_rect(gs->panel);
    lv_obj_set_pos(gs->think_bar, 16, 410);
    lv_obj_set_size(gs->think_bar, 270, 8);
    lv_obj_set_style_radius(gs->think_bar, 4, LV_PART_MAIN);
    set_bg(gs->think_bar, Pal::CARD_BG, LV_OPA_COVER);
    gs->think_fill = mk_rect(gs->think_bar);
    lv_obj_set_pos(gs->think_fill, 0, 0);
    lv_obj_set_size(gs->think_fill, 2, 8);
    lv_obj_set_style_radius(gs->think_fill, 4, LV_PART_MAIN);
    set_bg(gs->think_fill, Pal::THINK, LV_OPA_COVER);
    show(gs->think_lbl, false);
    show(gs->think_bar, false);

    gs->btn_ok = mk_rect(gs->panel);
    lv_obj_set_pos(gs->btn_ok, 16, 432);
    lv_obj_set_size(gs->btn_ok, 270, 56);
    lv_obj_set_style_radius(gs->btn_ok, 12, LV_PART_MAIN);
    set_bg(gs->btn_ok, Pal::CARD_BG, LV_OPA_COVER);
    set_pressed_bg(gs->btn_ok, Pal::CARD_ON);
    lv_obj_add_flag(gs->btn_ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gs->btn_ok, btn_ok_cb, LV_EVENT_CLICKED, nullptr);
    gs->btn_ok_lbl = mk_label(gs->btn_ok, gs->ui.f_small, Pal::GHOST);
    lv_obj_center(gs->btn_ok_lbl);
    show(gs->btn_ok, false);

    static const char* const BTN0[4] = {"Passer", "Annuler", "Indice", "Menu"};
    for (int i = 0; i < 4; i++) {
        gs->btn[i] = mk_rect(gs->panel);
        lv_obj_set_pos(gs->btn[i], (i & 1) ? 154 : 16, 500 + (i >> 1) * 68);
        lv_obj_set_size(gs->btn[i], 132, 60);
        lv_obj_set_style_radius(gs->btn[i], 12, LV_PART_MAIN);
        set_bg(gs->btn[i], Pal::CARD_BG, LV_OPA_COVER);
        set_border(gs->btn[i], Pal::EDGE, 1, LV_OPA_80);
        set_pressed_bg(gs->btn[i], Pal::CARD_ON);
        lv_obj_add_flag(gs->btn[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(gs->btn[i], btn_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        gs->btn_lbl[i] = mk_label(gs->btn[i], gs->ui.f_small, Pal::TXT);
        set_text_if(gs->btn_lbl[i], BTN0[i]);
        lv_obj_center(gs->btn_lbl[i]);
    }

    // --- Calque des menus -------------------------------------------------
    gs->m_title = mk_label(gs->ui.panel, gs->ui.f_big, Pal::ACCENT);
    lv_obj_set_width(gs->m_title, 1280);
    lv_obj_set_style_text_align(gs->m_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(gs->m_title, 0, 38);
    gs->m_sub = mk_label(gs->ui.panel, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_set_width(gs->m_sub, 1280);
    lv_obj_set_style_text_align(gs->m_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(gs->m_sub, 0, 96);
    gs->m_foot = mk_label(gs->ui.panel, gs->ui.f_small, Pal::TXT_MUTED);
    lv_obj_set_width(gs->m_foot, 1280);
    lv_obj_set_style_text_align(gs->m_foot, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(gs->m_foot, 0, 684);

    gs->slots.build(gs->ui.panel, slot_cb, gs->ui.f_mid, Pal::TXT, gs->ui.f_small, Pal::TXT_DIM,
                    [](lv_obj_t* b, int i) {
        lv_obj_set_pos(b, 90, 148 + i * 74);
        lv_obj_set_size(b, 1100, 64);
        lv_obj_set_style_radius(b, 14, LV_PART_MAIN);
        set_bg(b, Pal::CARD_BG, LV_OPA_COVER);   // Go::set_bg (dégradé remis à NONE)
        set_pressed_bg(b, Pal::CARD_ON);
    });
    for (int i = 0; i < N_SLOTS; i++) {
        lv_obj_set_pos(gs->slots.title[i], 26, 6);
        lv_obj_set_pos(gs->slots.desc[i], 28, 40);
    }

    // --- Carte de fin de partie -------------------------------------------
    gs->card = mk_rect(gs->ui.panel);
    lv_obj_set_pos(gs->card, 230, 120);
    lv_obj_set_size(gs->card, 820, 480);
    lv_obj_set_style_radius(gs->card, 18, LV_PART_MAIN);
    set_bg(gs->card, Pal::PANEL_BG, LV_OPA_COVER);
    set_border(gs->card, Pal::ACCENT, 2, LV_OPA_60);
    show(gs->card, false);

    gs->card_title = mk_label(gs->card, gs->ui.f_big, Pal::TXT);
    lv_obj_set_width(gs->card_title, 820);
    lv_obj_set_style_text_align(gs->card_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(gs->card_title, 0, 26);
    gs->card_sub = mk_label(gs->card, gs->ui.f_mid, Pal::ACCENT);
    lv_obj_set_width(gs->card_sub, 820);
    lv_obj_set_style_text_align(gs->card_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(gs->card_sub, 0, 86);
    for (int i = 0; i < CARD_LINES; i++) {
        gs->card_line[i] = mk_label(gs->card, gs->ui.f_small, Pal::TXT_DIM);
        lv_obj_set_width(gs->card_line[i], 760);
        lv_obj_set_style_text_align(gs->card_line[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_pos(gs->card_line[i], 30, 148 + i * 32);
    }
    static const char* const CBTN[2] = {"Revanche", "Menu principal"};
    for (int i = 0; i < 2; i++) {
        gs->card_btn[i] = mk_rect(gs->card);
        lv_obj_set_pos(gs->card_btn[i], 60 + i * 380, 380);
        lv_obj_set_size(gs->card_btn[i], 320, 66);
        lv_obj_set_style_radius(gs->card_btn[i], 14, LV_PART_MAIN);
        set_bg(gs->card_btn[i], Pal::CARD_BG, LV_OPA_COVER);
        set_border(gs->card_btn[i], i == 0 ? Pal::ACCENT : Pal::EDGE, 2, LV_OPA_70);
        set_pressed_bg(gs->card_btn[i], Pal::CARD_ON);
        lv_obj_add_flag(gs->card_btn[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(gs->card_btn[i], card_btn_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        gs->card_btn_lbl[i] = mk_label(gs->card_btn[i], gs->ui.f_mid,
                                     i == 0 ? Pal::ACCENT : Pal::TXT);
        set_text_if(gs->card_btn_lbl[i], CBTN[i]);
        lv_obj_center(gs->card_btn_lbl[i]);
    }
}

// ===========================================================================
// 15. Timer
// ===========================================================================

// Période réelle du timer : champ du bloc (Mem::tick_period) et non `static`
// locale : open() recrée le timer à TICK_IDLE_MS, le cache doit repartir de là
// (fermé pendant la réflexion de l'IA, le jeu restait bloqué à 50 ms — audit
// 25/09/2026, lot 5).
static void tick_cb(lv_timer_t*) {
    if (g_state == ST_OFF || !gs) return;

    // Tick adaptatif : 25 ms pendant la réflexion IA, 50 ms sinon (menus, attente
    // humain, marquage). Réduit la charge CPU de ~50 % entre les coups.
    const uint32_t want_ms = (g_state == ST_THINKING) ? TICK_THINK_MS : TICK_IDLE_MS;
    timer_period_sync(gs->timer, gs->tick_period, want_ms);

    const uint32_t now = esphome::millis();

    // Expiration de l'indice et du message de statut.
    bool need_board = false;
    if (gs->hint_sq >= 0 && (int32_t)(now - gs->hint_until) >= 0) {
        gs->hint_sq = -1;
        need_board = true;
    }
    if (gs->msg_until && (int32_t)(now - gs->msg_until) >= 0) {
        gs->msg_until = 0;
        gs->msg[0] = 0;
        render_hud();
    }
    if (need_board) render_board();

    if (g_state == ST_THINKING) {
        Ai::step(AI_SLICE_MS);
        render_panel_buttons();
        if (Ai::ready() && (now - gs->think_t0) >= MIN_THINK_MS) {
            const int sq = Ai::best_sq();
            if (sq == Ai::RESIGN) {
                msg("Le Tab abandonne");
                resign(gs->pos.side);
                return;
            }
            if (gs->cfg_mode == 2 && gs->next_move && (int32_t)(now - gs->next_move) < 0) return;
            gs->next_move = now + TVT_PAUSE_MS;
            apply_move(sq);
        }
        return;
    }

    flush(false);
}

// ===========================================================================
// 16. API publique
// ===========================================================================

// Appelée à chaque échantillon, jeu ouvert ou non : avant la garde du bloc, elle
// ne touche que les globaux restants (g_ax/g_ay/g_az).
void on_imu(float ax, float ay, float az) {
    if (ax != ax || ay != ay || az != az) return;   // NaN
    const float mag = accel_delta_norm(ax, ay, az, g_ax, g_ay, g_az);
    if (!gs || g_state != ST_PLAYING || !gs->save.opt_shake) return;
    if (!is_human_turn()) return;
    if (shake_fire(mag, 1.4f, g_last_shake, esphome::millis(), 1200)) do_hint();
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel) return;
    // État du jeu (RAM interne), pile d'annulation (PSRAM), brouillons du moteur
    // et de l'IA (RAM interne) : tout ou rien.
    gs = game_mem_new<Mem>(MemPref::Internal);
    gc = game_mem_new<Cold>(MemPref::Psram);
    const bool scratch_ok = Engine::scratch_acquire() && Ai::scratch_acquire();
    if (!gs || !gc || !scratch_ok) {
        Ai::scratch_release();
        Engine::scratch_release();
        game_mem_free(gc);
        game_mem_free(gs);
        ESP_LOGW("go", "memoire introuvable (%u + %u o, brouillons moteur/IA) : jeu non ouvert",
                 (unsigned) sizeof(Mem), (unsigned) sizeof(Cold));
        if (ui.lvgl) ui.lvgl->show_page(ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
        return;
    }
    gs->ui = ui;
    persist_load();
    build_ui();

    Engine::pos_init(gs->pos, SIZE_TAB[gs->cfg_size < GO_N_SIZES ? gs->cfg_size : 0]);
    gs->in_game = false;
    gs->last_sq = PASS;
    gs->pending = -1;
    gs->hint_sq = -1;
    gs->mv_n = 0;
    gs->undo_n = 0;
    gs->first_color = BLACK;
    memset(gs->dead, 0, sizeof(gs->dead));
    memset(gs->terr, 0, sizeof(gs->terr));
    gs->last_nvs = esphome::millis();

    layout_board();
    refresh_all();
    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).
    menu_main();
    if (!gs->timer) {
        gs->timer = lv_timer_create(tick_cb, TICK_IDLE_MS, nullptr);
        gs->tick_period = TICK_IDLE_MS;
    }
}

void close() {
    if (g_state == ST_OFF) return;
    Ai::abort();
    if (gs->in_game) {
        stash_position();
        if (gs->game_t0) { gs->save.total_ms += esphome::millis() - gs->game_t0; gs->game_t0 = 0; }
    }
    persist_save();
    if (gs->timer) { lv_timer_delete(gs->timer); gs->timer = nullptr; }
    show(gs->ui.panel, false);
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (gs->ui.lvgl) gs->ui.lvgl->show_page(gs->ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
    g_state = ST_OFF;

    // Rien ne reste réservé. D'abord les callbacks que build_ui() pose sur les
    // conteneurs YAML : reposés à la réouverture, ils doublonneraient (deux
    // on_board_tap par toucher : avec la confirmation, le second validerait aussitôt
    // le coup en attente). Puis les objets LVGL du jeu (dont peut-être le slot dont
    // le callback nous appelle), les brouillons du moteur et de l'IA, et enfin les
    // blocs qui pointaient les objets.
    lv_obj_remove_event_cb(gs->ui.field, field_cb);
    lv_obj_remove_event_cb(gs->ui.hud, hud_cb);
    ui_destroy(nullptr, {gs->ui.field, gs->ui.hud, gs->ui.panel});
    Ai::scratch_release();
    Engine::scratch_release();
    game_mem_free(gc);
    game_mem_free(gs);
}

}  // namespace Go
