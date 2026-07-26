/**
 * [AI-CONTEXT]
 * @file draughts_game.cpp
 * @role Jeu « Dames Tab » — règles, UI LVGL, NVS, orchestration IA.
 * @architecture_constraint Plein écran 1280×720. Widgets PRÉALLOUÉS. Hot-path
 *      timer : pas de std::string / to_string. Pièces capturées retirées en FIN
 *      de rafle (FMJD). Flying kings = international uniquement.
 * @ai_instruction RèglesInternational10 ≠ RulesEnglish8 — branchement via
 *      Pos.variant. ai_step() via Ai::step(), jamais de recherche bloquante.
 */
#include "draughts_game.h"
#include "draughts_ai.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>

namespace Draughts {

using Engine::Pos;
using Engine::Move;
using Engine::Piece;
using Engine::Side;
using Engine::Variant;
using Engine::MAX_N;
using Engine::MAX_SQ;
using Engine::MAX_MOVES;
using Engine::EMPTY;
using Engine::W_MAN;
using Engine::W_KING;
using Engine::B_MAN;
using Engine::B_KING;
using Engine::SIDE_WHITE;
using Engine::SIDE_BLACK;
using Engine::VAR_INTL10;
using Engine::VAR_ENG8;

// "DAM1"
static constexpr uint32_t SAVE_MAGIC = 0x44414D31u;
static constexpr uint32_t PREF_KEY   = 0x44414D54u;  // "DAMT"

// ===========================================================================
// Engine — règles
// ===========================================================================
namespace Engine {

static const int DR[4] = {-1, -1, +1, +1};
static const int DC[4] = {-1, +1, -1, +1};

static inline int idx(int r, int c, int n) { return r * n + c; }
static inline bool on_board(int r, int c, int n) {
    return r >= 0 && c >= 0 && r < n && c < n;
}
static inline bool dark_playable(int r, int c) { return is_dark_sq(r, c); }

static inline bool enemy(uint8_t p, Side s) {
    return s == SIDE_WHITE ? is_black(p) : is_white(p);
}
static inline bool friend_p(uint8_t p, Side s) {
    return s == SIDE_WHITE ? is_white(p) : is_black(p);
}
static inline bool already_cap(const uint8_t* caps, int nc, uint8_t sq) {
    for (int i = 0; i < nc; i++) if (caps[i] == sq) return true;
    return false;
}

void pos_init(Pos& p, Variant v) {
    memset(&p, 0, sizeof(p));
    p.n = (v == VAR_ENG8) ? 8 : 10;
    p.variant = (uint8_t)v;
    p.side = SIDE_WHITE;
    p.must_from = 255;
    p.no_progress = 0;
    const int n = p.n;
    const int black_rows = (v == VAR_ENG8) ? 3 : 4;
    const int white_start = n - black_rows;
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            int i = idx(r, c, n);
            p.sq[i] = EMPTY;
            if (!dark_playable(r, c)) continue;
            if (r < black_rows) p.sq[i] = B_MAN;
            else if (r >= white_start) p.sq[i] = W_MAN;
        }
    }
}

// --- Coups silencieux -------------------------------------------------------

static void add_silent(Move* out, int* nout, int max_out, uint8_t from, uint8_t to, bool promote) {
    if (*nout >= max_out) return;
    Move& m = out[*nout];
    memset(&m, 0, sizeof(m));
    m.from = from;
    m.to = to;
    m.n_path = 1;
    m.path[0] = to;
    m.promote = promote ? 1 : 0;
    (*nout)++;
}

static void gen_silent_man(const Pos& p, int r, int c, Move* out, int* nout, int max_out) {
    const int n = p.n;
    const Side s = (Side)p.side;
    const int forward = (s == SIDE_WHITE) ? -1 : +1;
    const uint8_t from = (uint8_t)idx(r, c, n);
    for (int d = 0; d < 4; d++) {
        if (DR[d] != forward) continue;
        int nr = r + DR[d], nc = c + DC[d];
        if (!on_board(nr, nc, n) || !dark_playable(nr, nc)) continue;
        if (p.sq[idx(nr, nc, n)] != EMPTY) continue;
        bool promo = (s == SIDE_WHITE) ? (nr == 0) : (nr == n - 1);
        add_silent(out, nout, max_out, from, (uint8_t)idx(nr, nc, n), promo);
    }
}

static void gen_silent_king_eng(const Pos& p, int r, int c, Move* out, int* nout, int max_out) {
    const int n = p.n;
    const uint8_t from = (uint8_t)idx(r, c, n);
    for (int d = 0; d < 4; d++) {
        int nr = r + DR[d], nc = c + DC[d];
        if (!on_board(nr, nc, n) || !dark_playable(nr, nc)) continue;
        if (p.sq[idx(nr, nc, n)] != EMPTY) continue;
        add_silent(out, nout, max_out, from, (uint8_t)idx(nr, nc, n), false);
    }
}

// Flying king : longue portée diagonale (dames internationales).
static void gen_silent_king_intl(const Pos& p, int r, int c, Move* out, int* nout, int max_out) {
    const int n = p.n;
    const uint8_t from = (uint8_t)idx(r, c, n);
    for (int d = 0; d < 4; d++) {
        int nr = r + DR[d], nc = c + DC[d];
        while (on_board(nr, nc, n) && dark_playable(nr, nc)) {
            int i = idx(nr, nc, n);
            if (p.sq[i] != EMPTY) break;
            add_silent(out, nout, max_out, from, (uint8_t)i, false);
            nr += DR[d]; nc += DC[d];
        }
    }
}

// --- Rafles (prises en chaîne) ----------------------------------------------

static void emit_capture(Move* out, int* nout, int max_out,
                         uint8_t from, uint8_t to,
                         const uint8_t* caps, int nc,
                         const uint8_t* path, int np,
                         bool promote) {
    if (*nout >= max_out) return;
    Move& m = out[*nout];
    memset(&m, 0, sizeof(m));
    m.from = from;
    m.to = to;
    m.n_caps = (uint8_t)nc;
    for (int i = 0; i < nc && i < MAX_CAPS; i++) m.caps[i] = caps[i];
    m.n_path = (uint8_t)np;
    for (int i = 0; i < np && i < MAX_PATH; i++) m.path[i] = path[i];
    m.promote = promote ? 1 : 0;
    (*nout)++;
}

// Recherche récursive des rafles d'un pion.
// [AI-CONTEXT] Pendant la rafle les capturées RESTENT sur le plateau (blocage)
// mais ne peuvent plus être reprises (liste caps). Retrait réel = apply_move.
static void search_man_caps(const Pos& p, Side s, int r, int c,
                            uint8_t from,
                            uint8_t* caps, int nc,
                            uint8_t* path, int np,
                            Move* out, int* nout, int max_out,
                            bool* found_ext) {
    const int n = p.n;
    bool extended = false;
    // International : prises avant/arrière ; Anglais : avant seulement.
    const bool fwd_only = (p.variant == VAR_ENG8);
    const int forward = (s == SIDE_WHITE) ? -1 : +1;

    for (int d = 0; d < 4; d++) {
        if (fwd_only && DR[d] != forward) continue;
        int mr = r + DR[d], mc = c + DC[d];
        int lr = r + 2 * DR[d], lc = c + 2 * DC[d];
        if (!on_board(mr, mc, n) || !on_board(lr, lc, n)) continue;
        if (!dark_playable(mr, mc) || !dark_playable(lr, lc)) continue;
        int mi = idx(mr, mc, n);
        int li = idx(lr, lc, n);
        uint8_t vic = p.sq[mi];
        if (!enemy(vic, s)) continue;
        if (already_cap(caps, nc, (uint8_t)mi)) continue;
        if (p.sq[li] != EMPTY) continue;

        caps[nc] = (uint8_t)mi;
        path[np] = (uint8_t)li;
        // Une extension existe : la feuille sera émise dans l'appel récursif.
        extended = true;
        search_man_caps(p, s, lr, lc, from, caps, nc + 1, path, np + 1,
                        out, nout, max_out, nullptr);
    }

    if (nc > 0 && !extended) {
        // Fin de rafle : promotion si dernière rangée
        bool promo = (s == SIDE_WHITE) ? (r == 0) : (r == n - 1);
        emit_capture(out, nout, max_out, from, (uint8_t)idx(r, c, n),
                     caps, nc, path, np, promo);
    }
    if (found_ext) *found_ext = extended;
}

// Flying king captures (international).
static void search_king_caps_intl(const Pos& p, Side s, int r, int c,
                                  uint8_t from,
                                  uint8_t* caps, int nc,
                                  uint8_t* path, int np,
                                  Move* out, int* nout, int max_out,
                                  bool* found_ext) {
    const int n = p.n;
    bool extended = false;

    for (int d = 0; d < 4; d++) {
        // Avance jusqu'à la première pièce
        int nr = r + DR[d], nc2 = c + DC[d];
        while (on_board(nr, nc2, n) && dark_playable(nr, nc2) && p.sq[idx(nr, nc2, n)] == EMPTY) {
            // Cases vides : ignore (approche). Note : capturées restent occupées.
            nr += DR[d]; nc2 += DC[d];
        }
        if (!on_board(nr, nc2, n) || !dark_playable(nr, nc2)) continue;
        int mi = idx(nr, nc2, n);
        uint8_t vic = p.sq[mi];
        if (!enemy(vic, s)) continue;
        if (already_cap(caps, nc, (uint8_t)mi)) continue;

        // Au-delà : toutes les cases vides sont des atterrissages possibles
        int lr = nr + DR[d], lc = nc2 + DC[d];
        bool any_land = false;
        while (on_board(lr, lc, n) && dark_playable(lr, lc) && p.sq[idx(lr, lc, n)] == EMPTY) {
            any_land = true;
            caps[nc] = (uint8_t)mi;
            path[np] = (uint8_t)idx(lr, lc, n);
            bool child_ext = false;
            search_king_caps_intl(p, s, lr, lc, from, caps, nc + 1, path, np + 1,
                                  out, nout, max_out, &child_ext);
            if (child_ext) extended = true;
            // Si aucune extension depuis cet atterrissage, ce n'est PAS encore
            // une fin : on continue la boucle pour d'autres landings. La fin
            // sera émise après si AUCUNE direction n'a étendu depuis (r,c)...
            // En fait chaque landing sans extension EST une feuille.
            if (!child_ext) {
                emit_capture(out, nout, max_out, from, (uint8_t)idx(lr, lc, n),
                             caps, nc + 1, path, np + 1, false);
            }
            lr += DR[d]; lc += DC[d];
        }
        if (any_land) extended = true;
    }

    if (found_ext) *found_ext = extended;
}

// Dame courte anglaise : saut d'une case.
static void search_king_caps_eng(const Pos& p, Side s, int r, int c,
                                 uint8_t from,
                                 uint8_t* caps, int nc,
                                 uint8_t* path, int np,
                                 Move* out, int* nout, int max_out,
                                 bool* found_ext) {
    const int n = p.n;
    bool extended = false;
    for (int d = 0; d < 4; d++) {
        int mr = r + DR[d], mc = c + DC[d];
        int lr = r + 2 * DR[d], lc = c + 2 * DC[d];
        if (!on_board(mr, mc, n) || !on_board(lr, lc, n)) continue;
        if (!dark_playable(mr, mc) || !dark_playable(lr, lc)) continue;
        int mi = idx(mr, mc, n);
        int li = idx(lr, lc, n);
        if (!enemy(p.sq[mi], s)) continue;
        if (already_cap(caps, nc, (uint8_t)mi)) continue;
        if (p.sq[li] != EMPTY) continue;

        caps[nc] = (uint8_t)mi;
        path[np] = (uint8_t)li;
        bool child = false;
        search_king_caps_eng(p, s, lr, lc, from, caps, nc + 1, path, np + 1,
                             out, nout, max_out, &child);
        if (!child) {
            emit_capture(out, nout, max_out, from, (uint8_t)li, caps, nc + 1, path, np + 1, false);
        }
        extended = true;
    }
    if (found_ext) *found_ext = extended;
}

static void gen_caps_from(const Pos& p, int r, int c, Move* out, int* nout, int max_out) {
    const int n = p.n;
    uint8_t piece = p.sq[idx(r, c, n)];
    Side s = piece_side(piece);
    if ((s == SIDE_WHITE && p.side != SIDE_WHITE) ||
        (s == SIDE_BLACK && p.side != SIDE_BLACK)) return;

    uint8_t caps[MAX_CAPS];
    uint8_t path[MAX_PATH];
    uint8_t from = (uint8_t)idx(r, c, n);
    bool dummy = false;

    if (is_man(piece)) {
        search_man_caps(p, s, r, c, from, caps, 0, path, 0, out, nout, max_out, &dummy);
    } else if (p.variant == VAR_INTL10) {
        search_king_caps_intl(p, s, r, c, from, caps, 0, path, 0, out, nout, max_out, &dummy);
    } else {
        search_king_caps_eng(p, s, r, c, from, caps, 0, path, 0, out, nout, max_out, &dummy);
    }
}

int gen_moves(const Pos& p, Move* out, int max_out) {
    int nout = 0;
    const int n = p.n;
    const Side s = (Side)p.side;

    // 1) Lister TOUTES les prises (éventuellement depuis must_from)
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            if (!dark_playable(r, c)) continue;
            int i = idx(r, c, n);
            if (p.must_from != 255 && i != p.must_from) continue;
            uint8_t pc = p.sq[i];
            if (pc == EMPTY) continue;
            if (piece_side(pc) != s) continue;
            gen_caps_from(p, r, c, out, &nout, max_out);
        }
    }

    if (nout > 0) {
        // International : prise MAXIMALE obligatoire (nombre de pièces).
        // Anglais : toute prise légale suffit (pas de filtre max).
        if (p.variant == VAR_INTL10) {
            int best = 0;
            for (int i = 0; i < nout; i++)
                if (out[i].n_caps > best) best = out[i].n_caps;
            int w = 0;
            for (int i = 0; i < nout; i++) {
                if (out[i].n_caps == best) {
                    if (w != i) out[w] = out[i];
                    w++;
                }
            }
            nout = w;
        }
        return nout;
    }

    // 2) Sinon coups silencieux (interdits si must_from — en pratique pas de caps)
    if (p.must_from != 255) return 0;

    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            if (!dark_playable(r, c)) continue;
            int i = idx(r, c, n);
            uint8_t pc = p.sq[i];
            if (pc == EMPTY) continue;
            if (piece_side(pc) != s) continue;
            if (is_man(pc)) gen_silent_man(p, r, c, out, &nout, max_out);
            else if (p.variant == VAR_INTL10) gen_silent_king_intl(p, r, c, out, &nout, max_out);
            else gen_silent_king_eng(p, r, c, out, &nout, max_out);
        }
    }
    return nout;
}

void apply_move(Pos& p, const Move& m) {
    const int n = p.n;
    uint8_t piece = p.sq[m.from];
    p.sq[m.from] = EMPTY;
    // Retrait des capturées en FIN de rafle
    for (int i = 0; i < m.n_caps; i++) p.sq[m.caps[i]] = EMPTY;
    if (m.promote) {
        piece = is_white(piece) ? W_KING : B_KING;
    }
    p.sq[m.to] = piece;

    if (m.n_caps > 0 || m.promote) p.no_progress = 0;
    else if (p.no_progress < 250) p.no_progress++;

    p.must_from = 255;
    p.side = (p.side == SIDE_WHITE) ? SIDE_BLACK : SIDE_WHITE;
}

int count_pieces(const Pos& p, Side s) {
    int n = 0;
    const int N = p.n * p.n;
    for (int i = 0; i < N; i++) {
        uint8_t pc = p.sq[i];
        if (pc == EMPTY) continue;
        if (piece_side(pc) == s) n++;
    }
    return n;
}

int eval_material(const Pos& p) {
    int sc = 0;
    const int N = p.n * p.n;
    for (int i = 0; i < N; i++) {
        switch (p.sq[i]) {
            case W_MAN:  sc += 100; break;
            case W_KING: sc += 300; break;
            case B_MAN:  sc -= 100; break;
            case B_KING: sc -= 300; break;
            default: break;
        }
    }
    return sc;
}

int eval_full(const Pos& p) {
    int sc = eval_material(p);
    // Mobilité légère (coût limité : génère pour le côté au trait seulement)
    Move tmp[MAX_MOVES];
    int mob = gen_moves(p, tmp, MAX_MOVES);
    if (p.side == SIDE_WHITE) sc += mob * 2;
    else sc -= mob * 2;
    // Avance des pions
    const int n = p.n;
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            int i = idx(r, c, n);
            if (p.sq[i] == W_MAN) sc += (n - 1 - r);
            else if (p.sq[i] == B_MAN) sc -= r;
        }
    }
    return sc;
}

bool has_legal_move(const Pos& p) {
    Move tmp[MAX_MOVES];
    return gen_moves(p, tmp, MAX_MOVES) > 0;
}

bool is_terminal(const Pos& p, int* winner) {
    if (count_pieces(p, SIDE_WHITE) == 0) { if (winner) *winner = 1; return true; }
    if (count_pieces(p, SIDE_BLACK) == 0) { if (winner) *winner = 0; return true; }
    if (!has_legal_move(p)) {
        // Le côté au trait a perdu
        if (winner) *winner = (p.side == SIDE_WHITE) ? 1 : 0;
        return true;
    }
    if (p.no_progress >= DRAW_PLIES) { if (winner) *winner = 2; return true; }
    return false;
}

}  // namespace Engine

// ===========================================================================
// État UI / partie
// ===========================================================================

enum UiState : uint8_t {
    ST_OFF = 0,
    ST_HUB,
    ST_SETUP,
    ST_PLAYING,
    ST_THINKING,
    ST_GAMEOVER,
    ST_STATS,
    ST_SETTINGS,
    ST_CONFIRM_RESET
};

static UI g_ui;
static UiState g_state = ST_OFF;
static lv_timer_t* g_timer = nullptr;
static DraughtsSave g_save{};
static esphome::ESPPreferenceObject g_pref;
static bool g_pref_ready = false;

static Pos g_pos;
static Move g_legal[MAX_MOVES];
static int  g_n_legal = 0;
static int  g_sel = -1;               // case sélectionnée (−1 = aucune)
static int  g_hint_from = -1, g_hint_to = -1;
static uint32_t g_hint_until = 0;
static int  g_winner = -1;            // 0/1/2
static char g_status[64] = "";
static bool g_ui_built = false;

// Setup brouillon (avant Nouvelle partie)
static uint8_t g_cfg_variant = 0;
static uint8_t g_cfg_mode = 0;
static uint8_t g_cfg_human = 0;
static uint8_t g_cfg_level = 1;

// Historique coups (notation)
static constexpr int HIST_MAX = 24;
static char g_hist[HIST_MAX][12];
static int  g_hist_n = 0;

// Undo : pile de positions (+ historique count)
static constexpr int UNDO_MAX = 32;
static Pos g_undo[UNDO_MAX];
static int g_undo_n = 0;

// Animation capture fade
static int g_fade_sq[Engine::MAX_CAPS];
static int g_fade_n = 0;
static uint32_t g_fade_until = 0;

// IMU shake
static float g_imu_ax = 0, g_imu_ay = 0, g_imu_az = 1;
static uint32_t g_last_shake_ms = 0;

// Géométrie damier
static int g_board_n = 10;
static int g_cell = 60;
static int g_board_x = 40;
static int g_board_y = 26;
static int g_panel_x = 980;

// Widgets préalloués
static lv_obj_t* g_sq_obj[MAX_SQ] = {};
static lv_obj_t* g_piece_obj[MAX_SQ] = {};
static lv_obj_t* g_king_ring[MAX_SQ] = {};
static lv_obj_t* g_hl_obj[MAX_SQ] = {};
static lv_obj_t* g_side_panel = nullptr;
static lv_obj_t* g_btn_undo = nullptr;
static lv_obj_t* g_btn_hint = nullptr;
static lv_obj_t* g_hud_trait = nullptr;
static lv_obj_t* g_hud_var = nullptr;
static lv_obj_t* g_hud_lvl = nullptr;
static lv_obj_t* g_hud_cap = nullptr;
static lv_obj_t* g_hud_status = nullptr;
static lv_obj_t* g_hist_lbl[8] = {};
static lv_obj_t* g_side_btns[4] = {};
static lv_obj_t* g_side_btns_l[4] = {};

// Panel menus (slots)
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
        g_pref = esphome::global_preferences->make_preference<DraughtsSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != SAVE_MAGIC) {
        g_save = DraughtsSave{};
        g_save.magic = SAVE_MAGIC;
        g_save.variant = VAR_INTL10;
        g_save.mode = 0;
        g_save.human_color = SIDE_WHITE;
        g_save.ai_level = Ai::LVL_AMATEUR;
        g_save.imu_hint = 1;
    }
    g_cfg_variant = g_save.variant;
    g_cfg_mode = g_save.mode;
    g_cfg_human = g_save.human_color;
    g_cfg_level = g_save.ai_level;
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = SAVE_MAGIC;
    g_save.variant = g_cfg_variant;
    g_save.mode = g_cfg_mode;
    g_save.human_color = g_cfg_human;
    g_save.ai_level = g_cfg_level;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
}

static void save_position() {
    g_save.has_game = 1;
    g_save.side = g_pos.side;
    g_save.must_from = g_pos.must_from;
    g_save.board_n = g_pos.n;
    g_save.no_progress = g_pos.no_progress;
    g_save.setup_variant = g_pos.variant;
    g_save.setup_mode = g_cfg_mode;
    g_save.setup_human = g_cfg_human;
    g_save.setup_level = g_cfg_level;
    memset(g_save.board, 0, sizeof(g_save.board));
    memcpy(g_save.board, g_pos.sq, (size_t)(g_pos.n * g_pos.n));
    persist_save();
}

static void clear_saved_game() {
    g_save.has_game = 0;
    persist_save();
}

static bool load_position() {
    if (!g_save.has_game) return false;
    if (g_save.board_n != 8 && g_save.board_n != 10) return false;
    memset(&g_pos, 0, sizeof(g_pos));
    g_pos.n = g_save.board_n;
    g_pos.variant = g_save.setup_variant;
    g_pos.side = g_save.side;
    g_pos.must_from = g_save.must_from;
    g_pos.no_progress = g_save.no_progress;
    memcpy(g_pos.sq, g_save.board, (size_t)(g_pos.n * g_pos.n));
    g_cfg_variant = g_save.setup_variant;
    g_cfg_mode = g_save.setup_mode;
    g_cfg_human = g_save.setup_human;
    g_cfg_level = g_save.setup_level;
    return true;
}

static void record_result(int winner) {
    // Stats vs Tab seulement
    if (g_cfg_mode != 0) return;
    uint8_t v = g_pos.variant;
    uint8_t lv = g_cfg_level;
    if (v >= DRAUGHTS_N_VARIANTS || lv >= DRAUGHTS_N_LEVELS) return;
    // winner 0=blancs, 1=noirs, 2=nulle — du point de vue humain
    if (winner == 2) {
        g_save.draws[v][lv]++;
    } else {
        bool human_white = (g_cfg_human == SIDE_WHITE);
        bool human_won = human_white ? (winner == 0) : (winner == 1);
        if (human_won) g_save.wins[v][lv]++;
        else g_save.losses[v][lv]++;
    }
    clear_saved_game();
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
static const char* variant_name(uint8_t v) {
    return v == VAR_ENG8 ? "Anglais 8x8" : "Intl 10x10";
}
static const char* mode_name(uint8_t m) {
    switch (m) {
        case 0: return "Joueur vs Tab";
        case 1: return "Joueur vs Joueur";
        case 2: return "Tab vs Tab";
        default: return "?";
    }
}

static void sq_to_name(int sq, int n, char* buf, int buflen) {
    int r = sq / n, c = sq % n;
    // Rang 1 en bas (blancs)
    int rank = n - r;
    snprintf(buf, buflen, "%c%d", (char)('a' + c), rank);
}

static void move_to_notation(const Move& m, int n, char* buf, int buflen) {
    char a[8], b[8];
    sq_to_name(m.from, n, a, sizeof(a));
    sq_to_name(m.to, n, b, sizeof(b));
    if (m.n_caps > 0) snprintf(buf, buflen, "%sx%s", a, b);
    else snprintf(buf, buflen, "%s-%s", a, b);
}

// ===========================================================================
// Layout damier
// ===========================================================================

static void compute_layout() {
    g_board_n = g_pos.n;
    // Panneau latéral 280 px ; damier centré dans le reste
    g_panel_x = 1280 - 300;
    int avail_w = g_panel_x - 20;
    int avail_h = 672 - 16;
    int cell_w = avail_w / g_board_n;
    int cell_h = avail_h / g_board_n;
    g_cell = cell_w < cell_h ? cell_w : cell_h;
    if (g_cell > 64) g_cell = 64;
    if (g_cell < 40) g_cell = 40;
    int bw = g_cell * g_board_n;
    int bh = g_cell * g_board_n;
    g_board_x = (avail_w - bw) / 2 + 10;
    g_board_y = (672 - bh) / 2;
}

static void layout_squares() {
    compute_layout();
    const int n = g_board_n;
    for (int r = 0; r < MAX_N; r++) {
        for (int c = 0; c < MAX_N; c++) {
            int i = r * MAX_N + c;  // widgets indexés 10×10 max
            // Remap : on utilise idx r*n+c pour la logique, widgets sur grille MAX_N
            int wi = r * MAX_N + c;
            bool used = (r < n && c < n);
            show(g_sq_obj[wi], used);
            show(g_hl_obj[wi], false);
            show(g_piece_obj[wi], false);
            show(g_king_ring[wi], false);
            if (!used) continue;
            int x = g_board_x + c * g_cell;
            int y = g_board_y + r * g_cell;
            lv_obj_set_pos(g_sq_obj[wi], x, y);
            lv_obj_set_size(g_sq_obj[wi], g_cell - 1, g_cell - 1);
            bool dark = Engine::is_dark_sq(r, c);
            set_bg(g_sq_obj[wi], dark ? Pal::DARK_SQ : Pal::LIGHT_SQ, LV_OPA_COVER);

            lv_obj_set_pos(g_hl_obj[wi], x + 2, y + 2);
            lv_obj_set_size(g_hl_obj[wi], g_cell - 5, g_cell - 5);

            int pr = (g_cell - 1) / 2 - 4;
            if (pr < 10) pr = 10;
            int px = x + (g_cell - 1 - 2 * pr) / 2;
            int py = y + (g_cell - 1 - 2 * pr) / 2;
            lv_obj_set_pos(g_piece_obj[wi], px, py);
            lv_obj_set_size(g_piece_obj[wi], 2 * pr, 2 * pr);
            lv_obj_set_style_radius(g_piece_obj[wi], LV_RADIUS_CIRCLE, LV_PART_MAIN);

            lv_obj_set_pos(g_king_ring[wi], px + 3, py + 3);
            lv_obj_set_size(g_king_ring[wi], 2 * pr - 6, 2 * pr - 6);
            lv_obj_set_style_radius(g_king_ring[wi], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        }
    }
    if (g_side_panel) {
        lv_obj_set_pos(g_side_panel, g_panel_x, 8);
        lv_obj_set_size(g_side_panel, 288, 656);
    }
    if (g_btn_undo) {
        lv_obj_set_pos(g_btn_undo, 8, 280);
        lv_obj_set_size(g_btn_undo, 56, 120);
    }
    if (g_btn_hint) {
        lv_obj_set_pos(g_btn_hint, g_panel_x - 64, 280);
        lv_obj_set_size(g_btn_hint, 56, 120);
    }
}

static int widget_i(int r, int c) { return r * MAX_N + c; }

static void sync_pieces() {
    const int n = g_pos.n;
    for (int r = 0; r < MAX_N; r++) {
        for (int c = 0; c < MAX_N; c++) {
            int wi = widget_i(r, c);
            if (r >= n || c >= n) {
                show(g_piece_obj[wi], false);
                show(g_king_ring[wi], false);
                continue;
            }
            int i = r * n + c;
            uint8_t pc = g_pos.sq[i];
            // Fade capture
            bool fading = false;
            if (g_fade_n > 0 && esphome::millis() < g_fade_until) {
                for (int k = 0; k < g_fade_n; k++) if (g_fade_sq[k] == i) fading = true;
            }
            if (pc == EMPTY && !fading) {
                show(g_piece_obj[wi], false);
                show(g_king_ring[wi], false);
                continue;
            }
            uint8_t draw = pc;
            if (fading && pc == EMPTY) draw = B_MAN;  // placeholder sombre
            bool white = Engine::is_white(draw) || (fading && false);
            if (pc != EMPTY) white = Engine::is_white(pc);
            set_bg(g_piece_obj[wi], white ? Pal::PIECE_W : Pal::PIECE_B, fading ? LV_OPA_50 : LV_OPA_COVER);
            set_border(g_piece_obj[wi], white ? Pal::PIECE_W_RIM : Pal::PIECE_B_RIM, 2, LV_OPA_COVER);
            show(g_piece_obj[wi], true);
            bool king = Engine::is_king(pc);
            show(g_king_ring[wi], king);
            if (king) {
                set_bg(g_king_ring[wi], 0, LV_OPA_TRANSP);
                set_border(g_king_ring[wi], Pal::KING_RING, 3, LV_OPA_COVER);
            }
            if (i == g_sel) set_border(g_piece_obj[wi], Pal::HL_SEL, 3, LV_OPA_COVER);
        }
    }
}

static void clear_highlights() {
    for (int i = 0; i < MAX_SQ; i++) show(g_hl_obj[i], false);
}

static void show_highlights_for_sel() {
    clear_highlights();
    if (g_sel < 0) return;
    const int n = g_pos.n;
    for (int i = 0; i < g_n_legal; i++) {
        if (g_legal[i].from != (uint8_t)g_sel) continue;
        // Surbrille chaque étape du path pour rafles guidées
        for (int k = 0; k < g_legal[i].n_path; k++) {
            int sq = g_legal[i].path[k];
            int r = sq / n, c = sq % n;
            int wi = widget_i(r, c);
            set_bg(g_hl_obj[wi], g_legal[i].n_caps ? Pal::HL_CAP : Pal::HL_MOVE, LV_OPA_60);
            show(g_hl_obj[wi], true);
        }
    }
    if (g_hint_from >= 0 && esphome::millis() < g_hint_until) {
        int r = g_hint_from / n, c = g_hint_from % n;
        int wi = widget_i(r, c);
        set_bg(g_hl_obj[wi], Pal::HL_SEL, LV_OPA_70);
        show(g_hl_obj[wi], true);
        r = g_hint_to / n; c = g_hint_to % n;
        wi = widget_i(r, c);
        set_bg(g_hl_obj[wi], Pal::HL_MOVE, LV_OPA_80);
        show(g_hl_obj[wi], true);
    }
}

static void refresh_legal() {
    g_n_legal = Engine::gen_moves(g_pos, g_legal, MAX_MOVES);
}

static void update_hud() {
    char buf[48];
    const char* side = (g_pos.side == SIDE_WHITE) ? "Blancs" : "Noirs";
    if (g_state == ST_THINKING) snprintf(buf, sizeof(buf), "Tab reflechit...");
    else snprintf(buf, sizeof(buf), "Trait: %s", side);
    set_text_if(g_hud_trait, buf);
    set_text_if(g_hud_var, variant_name(g_pos.variant));
    snprintf(buf, sizeof(buf), "IA: %s", level_name(g_cfg_level));
    set_text_if(g_hud_lvl, buf);

    int max_cap = 0;
    for (int i = 0; i < g_n_legal; i++)
        if (g_legal[i].n_caps > max_cap) max_cap = g_legal[i].n_caps;
    if (max_cap > 0) snprintf(buf, sizeof(buf), "Prise x%d", max_cap);
    else snprintf(buf, sizeof(buf), "—");
    set_text_if(g_hud_cap, buf);
    set_text_if(g_hud_status, g_status);
}

static void update_hist_panel() {
    for (int i = 0; i < 8; i++) {
        int src = g_hist_n - 8 + i;
        if (src < 0) set_text_if(g_hist_lbl[i], "");
        else set_text_if(g_hist_lbl[i], g_hist[src]);
    }
}

static void push_hist(const Move& m) {
    char buf[12];
    move_to_notation(m, g_pos.n, buf, sizeof(buf));
    if (g_hist_n < HIST_MAX) {
        strncpy(g_hist[g_hist_n], buf, 11);
        g_hist[g_hist_n][11] = 0;
        g_hist_n++;
    } else {
        for (int i = 1; i < HIST_MAX; i++) memcpy(g_hist[i - 1], g_hist[i], 12);
        strncpy(g_hist[HIST_MAX - 1], buf, 11);
        g_hist[HIST_MAX - 1][11] = 0;
    }
    update_hist_panel();
}

static void push_undo() {
    if (g_undo_n < UNDO_MAX) g_undo[g_undo_n++] = g_pos;
    else {
        for (int i = 1; i < UNDO_MAX; i++) g_undo[i - 1] = g_undo[i];
        g_undo[UNDO_MAX - 1] = g_pos;
    }
}

// ===========================================================================
// Déclarations forward UI
// ===========================================================================
static void go_hub();
static void go_setup();
static void go_stats();
static void go_settings();
static void go_gameover();
static void start_new_game();
static void resume_game();
static void enter_playing();
static void after_human_or_ai_move();
static void try_start_ai();
static void apply_player_move(const Move& m);
static void do_undo();
static void do_hint();
static bool is_human_turn();

static bool is_human_turn() {
    if (g_cfg_mode == 1) return true;                 // PvP
    if (g_cfg_mode == 2) return false;                // TvT
    // PvT
    return g_pos.side == g_cfg_human;
}

// ===========================================================================
// Menus
// ===========================================================================

static void go_hub() {
    g_state = ST_HUB;
    Ai::abort();
    panel_on(true);
    set_text_if(g_p_title, "Dames Tab");
    set_text_if(g_p_sub, "Dames internationales — flying kings");
    slot_list(0, "Nouvelle partie", "Setup variante / mode / niveau", Pal::KING_RING, true);
    slot_list(1, g_save.has_game ? "Reprendre" : "Reprendre (vide)",
              g_save.has_game ? "Position sauvegardee" : "Aucune partie en cours",
              Pal::HL_MOVE, g_save.has_game != 0);
    slot_list(2, "Statistiques", "Victoires / nulle / defaites vs Tab", Pal::HL_CAP, true);
    slot_list(3, "Reglages", "Secousse = Hint, reset stats", Pal::TXT_DIM, true);
    slot_list(4, "Quitter", "Retour au tableau de bord", Pal::DANGER, true);
    slot_list(5, "", "", 0, false);
}

static void go_setup() {
    g_state = ST_SETUP;
    panel_on(true);
    set_text_if(g_p_title, "Nouvelle partie");
    char buf[64];
    snprintf(buf, sizeof(buf), "%s · %s", variant_name(g_cfg_variant), mode_name(g_cfg_mode));
    set_text_if(g_p_sub, buf);
    slot_list(0, variant_name(g_cfg_variant), "Tap: Intl 10x10 / Anglais 8x8", Pal::LIGHT_SQ, true);
    slot_list(1, mode_name(g_cfg_mode), "Joueur vs Tab / PvP / Tab vs Tab", Pal::HL_MOVE, true);
    snprintf(buf, sizeof(buf), "Humain: %s", g_cfg_human == 0 ? "Blancs" : "Noirs");
    slot_list(2, buf, "Couleur (mode vs Tab)", Pal::PIECE_W, g_cfg_mode == 0);
    slot_list(3, level_name(g_cfg_level), "Niveau IA", Pal::KING_RING, g_cfg_mode != 1);
    slot_list(4, "Jouer !", "Lance la partie", Pal::HL_CAP, true);
    slot_list(5, "Retour", "", Pal::TXT_DIM, true);
}

static void go_stats() {
    g_state = ST_STATS;
    panel_on(true);
    set_text_if(g_p_title, "Statistiques");
    set_text_if(g_p_sub, "Bilan vs Tab (par variante / niveau)");
    char buf[72];
    uint8_t v = g_cfg_variant;
    for (int lv = 0; lv < 4; lv++) {
        snprintf(buf, sizeof(buf), "%s — %uW / %uD / %uL",
                 level_name((uint8_t)lv),
                 (unsigned)g_save.wins[v][lv],
                 (unsigned)g_save.draws[v][lv],
                 (unsigned)g_save.losses[v][lv]);
        slot_list(lv, buf, variant_name(v), Pal::TXT, true);
    }
    slot_list(4, "Changer variante stats", variant_name(v), Pal::HL_MOVE, true);
    slot_list(5, "Retour", "", Pal::TXT_DIM, true);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    panel_on(true);
    set_text_if(g_p_title, "Reglages");
    set_text_if(g_p_sub, "Options locales (NVS)");
    slot_list(0, g_save.imu_hint ? "Secousse Hint: ON" : "Secousse Hint: OFF",
              "IMU BMI270", Pal::HL_MOVE, true);
    slot_list(1, "Reset statistiques", "Demande confirmation", Pal::DANGER, true);
    slot_list(2, "Retour", "", Pal::TXT_DIM, true);
    slot_list(3, "", "", 0, false);
    slot_list(4, "", "", 0, false);
    slot_list(5, "", "", 0, false);
}

static void go_gameover() {
    g_state = ST_GAMEOVER;
    panel_on(true);
    const char* msg = "Partie nulle";
    if (g_winner == 0) msg = "Victoire des Blancs";
    else if (g_winner == 1) msg = "Victoire des Noirs";
    set_text_if(g_p_title, msg);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s · %s", variant_name(g_pos.variant), mode_name(g_cfg_mode));
    set_text_if(g_p_sub, buf);
    slot_list(0, "Revanche", "Meme reglage", Pal::KING_RING, true);
    slot_list(1, "Hub", "Menu principal", Pal::TXT_DIM, true);
    slot_list(2, "", "", 0, false);
    slot_list(3, "", "", 0, false);
    slot_list(4, "", "", 0, false);
    slot_list(5, "", "", 0, false);
}

static void start_new_game() {
    Engine::pos_init(g_pos, (Variant)g_cfg_variant);
    g_hist_n = 0;
    g_undo_n = 0;
    g_sel = -1;
    g_hint_from = -1;
    g_winner = -1;
    g_fade_n = 0;
    strncpy(g_status, "Nouvelle partie", sizeof(g_status));
    layout_squares();
    enter_playing();
    save_position();
}

static void resume_game() {
    if (!load_position()) { go_hub(); return; }
    g_hist_n = 0;
    g_undo_n = 0;
    g_sel = -1;
    g_hint_from = -1;
    g_winner = -1;
    strncpy(g_status, "Partie reprise", sizeof(g_status));
    layout_squares();
    enter_playing();
}

static void enter_playing() {
    g_state = ST_PLAYING;
    panel_on(false);
    refresh_legal();
    sync_pieces();
    clear_highlights();
    update_hud();
    update_hist_panel();
    after_human_or_ai_move();
}

static void finish_if_terminal() {
    int w = -1;
    if (!Engine::is_terminal(g_pos, &w)) return;
    g_winner = w;
    record_result(w);
    go_gameover();
}

static void try_start_ai() {
    if (g_state != ST_PLAYING && g_state != ST_THINKING) return;
    if (is_human_turn()) return;
    int w = -1;
    if (Engine::is_terminal(g_pos, &w)) { finish_if_terminal(); return; }
    g_state = ST_THINKING;
    strncpy(g_status, "Tab reflechit...", sizeof(g_status));
    update_hud();
    Ai::begin(g_pos, (Ai::Level)g_cfg_level);
}

static void after_human_or_ai_move() {
    finish_if_terminal();
    if (g_state == ST_GAMEOVER) return;
    if (!is_human_turn()) try_start_ai();
    else {
        g_state = ST_PLAYING;
        strncpy(g_status, g_n_legal && g_legal[0].n_caps ? "Prise obligatoire" : "A vous",
                sizeof(g_status));
        update_hud();
    }
}

static void apply_player_move(const Move& m) {
    push_undo();
    // Prep fade
    g_fade_n = m.n_caps;
    for (int i = 0; i < m.n_caps; i++) g_fade_sq[i] = m.caps[i];
    g_fade_until = esphome::millis() + 180;

    push_hist(m);
    Engine::apply_move(g_pos, m);
    g_sel = -1;
    refresh_legal();
    sync_pieces();
    clear_highlights();
    save_position();
    update_hud();
    after_human_or_ai_move();
}

static void do_undo() {
    if (g_state != ST_PLAYING && g_state != ST_THINKING) return;
    Ai::abort();
    if (g_undo_n <= 0) {
        strncpy(g_status, "Rien a annuler", sizeof(g_status));
        update_hud();
        return;
    }
    // Annule aussi la réponse IA : si PvT et dernier coup = humain aurait
    // besoin de 2 pops. On pop jusqu'au tour humain.
    g_pos = g_undo[--g_undo_n];
    if (g_cfg_mode == 0 && !is_human_turn() && g_undo_n > 0) {
        g_pos = g_undo[--g_undo_n];
    }
    if (g_hist_n > 0) g_hist_n--;
    if (g_cfg_mode == 0 && g_hist_n > 0) g_hist_n--;  // retire aussi le coup IA
    g_sel = -1;
    g_state = ST_PLAYING;
    refresh_legal();
    sync_pieces();
    clear_highlights();
    update_hist_panel();
    save_position();
    strncpy(g_status, "Coup annule", sizeof(g_status));
    update_hud();
    after_human_or_ai_move();
}

static void do_hint() {
    if (g_state != ST_PLAYING) return;
    if (!is_human_turn()) return;
    refresh_legal();
    if (g_n_legal <= 0) return;
    // Choisit une prise max ou le premier coup
    int best = 0;
    for (int i = 1; i < g_n_legal; i++)
        if (g_legal[i].n_caps > g_legal[best].n_caps) best = i;
    g_hint_from = g_legal[best].from;
    g_hint_to = g_legal[best].to;
    g_hint_until = esphome::millis() + 2500;
    g_sel = g_hint_from;
    show_highlights_for_sel();
    strncpy(g_status, "Indice affiche", sizeof(g_status));
    update_hud();
}

// ===========================================================================
// Input
// ===========================================================================

static void on_square_tap(int r, int c) {
    if (g_state != ST_PLAYING) return;
    if (!is_human_turn()) return;
    const int n = g_pos.n;
    if (r < 0 || c < 0 || r >= n || c >= n) return;
    if (!Engine::is_dark_sq(r, c)) return;
    int sq = r * n + c;

    // Destination d'un coup légal depuis la sélection ?
    if (g_sel >= 0) {
        // Cherche un coup from=sel dont path contient sq (étape ou finale)
        int match = -1;
        int match_partial = -1;
        for (int i = 0; i < g_n_legal; i++) {
            if (g_legal[i].from != (uint8_t)g_sel) continue;
            if (g_legal[i].to == (uint8_t)sq) { match = i; break; }
            // Tap intermédiaire : si un seul coup max passe par cette case
            for (int k = 0; k < g_legal[i].n_path; k++) {
                if (g_legal[i].path[k] == (uint8_t)sq) {
                    if (match_partial < 0) match_partial = i;
                    else if (match_partial >= 0 &&
                             g_legal[match_partial].to != g_legal[i].to)
                        match_partial = -2;  // ambigu
                }
            }
        }
        if (match >= 0) {
            apply_player_move(g_legal[match]);
            return;
        }
        // Ambiguïté résolue : un seul chemin
        if (match_partial >= 0) {
            // Filtre les légaux à ceux passant par sq ; si un seul to, joue-le
            int only = -1;
            for (int i = 0; i < g_n_legal; i++) {
                if (g_legal[i].from != (uint8_t)g_sel) continue;
                bool ok = false;
                for (int k = 0; k < g_legal[i].n_path; k++)
                    if (g_legal[i].path[k] == (uint8_t)sq) ok = true;
                if (!ok) continue;
                if (only < 0) only = i;
                else if (g_legal[only].to != g_legal[i].to) { only = -2; break; }
            }
            if (only >= 0) { apply_player_move(g_legal[only]); return; }
        }
        // Re-sélection pièce alliée
    }

    uint8_t pc = g_pos.sq[sq];
    if (pc != EMPTY && Engine::piece_side(pc) == (Side)g_pos.side) {
        // Vérifie qu'un coup légal part de cette case
        bool ok = false;
        for (int i = 0; i < g_n_legal; i++)
            if (g_legal[i].from == (uint8_t)sq) { ok = true; break; }
        if (ok) {
            g_sel = sq;
            show_highlights_for_sel();
            sync_pieces();
            return;
        }
    }
    g_sel = -1;
    clear_highlights();
    sync_pieces();
}

static void field_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_state != ST_PLAYING) return;
    lv_indev_t* in = lv_indev_get_act();
    if (!in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    // Coordonnées relatives au field
    lv_area_t a;
    lv_obj_get_coords(g_ui.field, &a);
    int x = pt.x - a.x1;
    int y = pt.y - a.y1;
    int c = (x - g_board_x) / g_cell;
    int r = (y - g_board_y) / g_cell;
    on_square_tap(r, c);
}

static void undo_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    do_undo();
}
static void hint_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    do_hint();
}

static void side_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_state != ST_PLAYING && g_state != ST_THINKING) return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == 0) {  // Abandon
        g_winner = (g_cfg_human == SIDE_WHITE) ? 1 : 0;
        if (g_cfg_mode != 0) g_winner = (g_pos.side == SIDE_WHITE) ? 1 : 0;
        record_result(g_winner);
        go_gameover();
    } else if (i == 1) {  // Nouvelle
        go_setup();
    } else if (i == 2) {  // Menu
        save_position();
        go_hub();
    }
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
            if (i == 0) g_cfg_variant = g_cfg_variant ? 0 : 1;
            else if (i == 1) g_cfg_mode = (uint8_t)((g_cfg_mode + 1) % 3);
            else if (i == 2 && g_cfg_mode == 0) g_cfg_human ^= 1;
            else if (i == 3 && g_cfg_mode != 1)
                g_cfg_level = (uint8_t)((g_cfg_level + 1) % 4);
            else if (i == 4) {
                persist_save();
                start_new_game();
                break;
            } else if (i == 5) { go_hub(); break; }
            go_setup();
            break;
        case ST_STATS:
            if (i == 4) { g_cfg_variant ^= 1; go_stats(); }
            else if (i == 5) go_hub();
            break;
        case ST_SETTINGS:
            if (i == 0) { g_save.imu_hint ^= 1; persist_save(); go_settings(); }
            else if (i == 1) {
                g_state = ST_CONFIRM_RESET;
                set_text_if(g_p_title, "Reset stats ?");
                set_text_if(g_p_sub, "Irréversible");
                slot_list(0, "Confirmer reset", "", Pal::DANGER, true);
                slot_list(1, "Annuler", "", Pal::TXT_DIM, true);
                slot_list(2, "", "", 0, false);
                slot_list(3, "", "", 0, false);
                slot_list(4, "", "", 0, false);
                slot_list(5, "", "", 0, false);
            } else if (i == 2) go_hub();
            break;
        case ST_CONFIRM_RESET:
            if (i == 0) {
                memset(g_save.wins, 0, sizeof(g_save.wins));
                memset(g_save.draws, 0, sizeof(g_save.draws));
                memset(g_save.losses, 0, sizeof(g_save.losses));
                persist_save();
                go_settings();
            } else go_settings();
            break;
        case ST_GAMEOVER:
            if (i == 0) start_new_game();
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

    // Cases / pièces / highlights (pool 10×10)
    for (int i = 0; i < MAX_SQ; i++) {
        // Cases NON cliquables : le tap est géré par field_event_cb (coordonnées).
        g_sq_obj[i] = mk_rect(g_ui.field);
        g_hl_obj[i] = mk_rect(g_ui.field);
        lv_obj_set_style_radius(g_hl_obj[i], 6, LV_PART_MAIN);
        show(g_hl_obj[i], false);
        g_piece_obj[i] = mk_rect(g_ui.field);
        g_king_ring[i] = mk_rect(g_ui.field);
        show(g_piece_obj[i], false);
        show(g_king_ring[i], false);
    }

    // Panneau latéral gameplay
    g_side_panel = mk_rect(g_ui.field);
    set_bg(g_side_panel, Pal::PANEL_BG, LV_OPA_COVER);
    set_border(g_side_panel, Pal::BTN_EDGE, 1, LV_OPA_40);

    for (int i = 0; i < 8; i++) {
        g_hist_lbl[i] = mk_label(g_side_panel, g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(g_hist_lbl[i], 12, 16 + i * 28);
    }

    static const char* SBTNS[] = {"Abandon", "Nouvelle", "Menu"};
    for (int i = 0; i < 3; i++) {
        g_side_btns[i] = mk_rect(g_side_panel);
        lv_obj_add_flag(g_side_btns[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(g_side_btns[i], 16, 280 + i * 70);
        lv_obj_set_size(g_side_btns[i], 256, 58);
        set_bg(g_side_btns[i], Pal::BTN, LV_OPA_COVER);
        set_border(g_side_btns[i], Pal::BTN_EDGE, 1, LV_OPA_60);
        lv_obj_set_style_radius(g_side_btns[i], 8, LV_PART_MAIN);
        g_side_btns_l[i] = mk_label(g_side_btns[i], g_ui.f_mid, Pal::TXT);
        set_text_if(g_side_btns_l[i], SBTNS[i]);
        lv_obj_center(g_side_btns_l[i]);
        lv_obj_add_event_cb(g_side_btns[i], side_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // Boutons latéraux Undo / Hint
    g_btn_undo = mk_rect(g_ui.field);
    lv_obj_add_flag(g_btn_undo, LV_OBJ_FLAG_CLICKABLE);
    set_bg(g_btn_undo, Pal::BTN, LV_OPA_COVER);
    set_border(g_btn_undo, Pal::HL_CAP, 2, LV_OPA_60);
    lv_obj_set_style_radius(g_btn_undo, 10, LV_PART_MAIN);
    {
        lv_obj_t* l = mk_label(g_btn_undo, g_ui.f_small, Pal::TXT);
        set_text_if(l, "Undo");
        lv_obj_center(l);
    }
    lv_obj_add_event_cb(g_btn_undo, undo_cb, LV_EVENT_CLICKED, nullptr);

    g_btn_hint = mk_rect(g_ui.field);
    lv_obj_add_flag(g_btn_hint, LV_OBJ_FLAG_CLICKABLE);
    set_bg(g_btn_hint, Pal::BTN, LV_OPA_COVER);
    set_border(g_btn_hint, Pal::HL_MOVE, 2, LV_OPA_60);
    lv_obj_set_style_radius(g_btn_hint, 10, LV_PART_MAIN);
    {
        lv_obj_t* l = mk_label(g_btn_hint, g_ui.f_small, Pal::TXT);
        set_text_if(l, "Hint");
        lv_obj_center(l);
    }
    lv_obj_add_event_cb(g_btn_hint, hint_cb, LV_EVENT_CLICKED, nullptr);

    // HUD
    g_hud_trait = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT);
    lv_obj_set_pos(g_hud_trait, 16, 12);
    g_hud_var = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_set_pos(g_hud_var, 280, 12);
    g_hud_lvl = mk_label(g_ui.hud, g_ui.f_small, Pal::KING_RING);
    lv_obj_set_pos(g_hud_lvl, 520, 12);
    g_hud_cap = mk_label(g_ui.hud, g_ui.f_small, Pal::HL_CAP);
    lv_obj_set_pos(g_hud_cap, 780, 12);
    g_hud_status = mk_label(g_ui.hud, g_ui.f_small, Pal::HL_MOVE);
    lv_obj_set_pos(g_hud_status, 960, 12);

    // Panel menus
    g_p_title = mk_label(g_ui.panel, g_ui.f_big, Pal::KING_RING);
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

// ===========================================================================
// Timer
// ===========================================================================

static void tick_cb(lv_timer_t* /*t*/) {
    if (g_state == ST_OFF) return;

    // Fade capture
    if (g_fade_n > 0 && esphome::millis() >= g_fade_until) {
        g_fade_n = 0;
        sync_pieces();
    }
    // Hint timeout
    if (g_hint_from >= 0 && esphome::millis() >= g_hint_until) {
        g_hint_from = -1;
        if (g_state == ST_PLAYING) show_highlights_for_sel();
    }

    if (g_state == ST_THINKING) {
        Ai::step();
        if (Ai::ready()) {
            const Move& m = Ai::best();
            push_undo();
            g_fade_n = m.n_caps;
            for (int i = 0; i < m.n_caps; i++) g_fade_sq[i] = m.caps[i];
            g_fade_until = esphome::millis() + 180;
            push_hist(m);
            Engine::apply_move(g_pos, m);
            g_sel = -1;
            refresh_legal();
            sync_pieces();
            clear_highlights();
            save_position();
            g_state = ST_PLAYING;
            after_human_or_ai_move();
        } else {
            update_hud();
        }
    }
}

void ai_step() {
    if (g_state == ST_THINKING) Ai::step();
}

// ===========================================================================
// API
// ===========================================================================

void on_imu(float ax, float ay, float az) {
    if (ax != ax || ay != ay || az != az) return;
    float dax = ax - g_imu_ax, day = ay - g_imu_ay, daz = az - g_imu_az;
    g_imu_ax = ax; g_imu_ay = ay; g_imu_az = az;
    if (g_state != ST_PLAYING || !g_save.imu_hint) return;
    float mag = sqrtf(dax * dax + day * day + daz * daz);
    uint32_t now = esphome::millis();
    if (mag > 1.2f && (now - g_last_shake_ms) > 800) {
        g_last_shake_ms = now;
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
    Engine::pos_init(g_pos, (Variant)g_cfg_variant);
    layout_squares();

    show(g_ui.root, true);
    lv_obj_move_foreground(g_ui.root);

    lv_obj_add_flag(g_ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.hud, hud_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(g_ui.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.field, field_event_cb, LV_EVENT_CLICKED, nullptr);

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
    if (g_ui.field) lv_obj_remove_event_cb(g_ui.field, field_event_cb);
    show(g_ui.root, false);
    g_state = ST_OFF;
}

}  // namespace Draughts
