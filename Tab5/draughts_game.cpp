/**
 * [AI-CONTEXT]
 * @file draughts_game.cpp
 * @role Jeu « Dames Tab » — règles, UI LVGL, NVS, orchestration IA.
 * @architecture_constraint Plein écran 1280×720. Widgets PRÉALLOUÉS à chaque
 *      ouverture (pool), réutilisés par show/hide. Jeu fermé, il ne reste rien :
 *      objets LVGL détruits, état rendu (struct Mem / Cold ici, idem pour l'IA ;
 *      audit du 26/09/2026, lot 4). Hot-path
 *      timer : pas de std::string / to_string. Pièces capturées retirées en FIN
 *      de rafle (FMJD). Flying kings = international uniquement.
 * @ai_instruction RèglesInternational10 ≠ RulesEnglish8 — branchement via
 *      Pos.variant. ai_step() via Ai::step(), jamais de recherche bloquante.
 */
#include "draughts_game.h"
#include "game_common.h"
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

// Pièces, couronnes et surbrillances n'existent que sur les cases foncées : 50 sur
// 100, 5 par rangée (audit ressources du 26/09/2026, lot 5 — 150 objets LVGL de
// moins). dark_slot() : index de case de la grille MAX_N -> 0..49, ou -1 (claire).
// Un damier 8×8 occupe le coin haut-gauche de la grille : sous-ensemble exact.
static constexpr int MAX_DARK = MAX_SQ / 2;
static inline int dark_slot(int wi) {
    const int r = wi / MAX_N, c = wi % MAX_N;
    return Engine::is_dark_sq(r, c) ? r * (MAX_N / 2) + c / 2 : -1;
}

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

void refresh_endgame(Pos& p) {
    p.eg_limit = 0;
    p.eg_plies = 0;
    if (p.variant != VAR_INTL10) return;
    int w = 0, b = 0, wk = 0, bk = 0;
    const int N = p.n * p.n;
    for (int i = 0; i < N; i++) {
        const uint8_t pc = p.sq[i];
        if (pc == EMPTY) continue;
        if (is_white(pc)) { w++; if (is_king(pc)) wk++; }
        else { b++; if (is_king(pc)) bk++; }
    }
    // Une dame seule d'un côté ; de l'autre, 1 à 3 pièces dont au moins une dame.
    int other = 0, other_k = 0;
    if (w == 1 && wk == 1) { other = b; other_k = bk; }
    else if (b == 1 && bk == 1) { other = w; other_k = wk; }
    else return;
    if (other_k == 0 || other > 3) return;
    p.eg_limit = (uint8_t) ((other <= 2) ? ENDGAME_PLIES_SMALL : ENDGAME_PLIES_THREE);
}

void apply_move(Pos& p, const Move& m) {
    const int n = p.n;
    uint8_t piece = p.sq[m.from];
    const bool man_moved = is_man(piece);   // avant une éventuelle promotion
    p.sq[m.from] = EMPTY;
    // Retrait des capturées en FIN de rafle
    for (int i = 0; i < m.n_caps; i++) p.sq[m.caps[i]] = EMPTY;
    if (m.promote) {
        piece = is_white(piece) ? W_KING : B_KING;
    }
    p.sq[m.to] = piece;

    if (m.n_caps > 0 || man_moved) p.no_progress = 0;
    else if (p.no_progress < 250) p.no_progress++;

    // Le matériel ne change qu'à une prise ou une promotion : on ne recompte qu'alors.
    if (m.n_caps > 0 || m.promote) refresh_endgame(p);
    else if (p.eg_limit && p.eg_plies < 250) p.eg_plies++;

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

int eval_full(const Pos& p, Move* scratch) {
    int sc = eval_material(p);
    // Mobilité légère (coût limité : génère pour le côté au trait seulement)
    int mob = gen_moves(p, scratch, MAX_MOVES);
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

// Une case suffit : gen_moves() émet toujours le premier coup trouvé, prise ou non,
// et s'arrête d'écrire à max_out. Une liste complète (4,7 Ko) ici, appelée par
// is_terminal() au fond de la recherche de l'IA, faisait déborder la pile de 8 Ko.
bool has_legal_move(const Pos& p) {
    Move one[1];
    return gen_moves(p, one, 1) > 0;
}

bool is_terminal(const Pos& p, int* winner) {
    if (count_pieces(p, SIDE_WHITE) == 0) { if (winner) *winner = 1; return true; }
    if (count_pieces(p, SIDE_BLACK) == 0) { if (winner) *winner = 0; return true; }
    if (!has_legal_move(p)) {
        // Le côté au trait a perdu
        if (winner) *winner = (p.side == SIDE_WHITE) ? 1 : 0;
        return true;
    }
    const int draw_plies = (p.variant == VAR_ENG8) ? DRAW_PLIES_ENG : DRAW_PLIES_INTL;
    if (p.no_progress >= draw_plies) { if (winner) *winner = 2; return true; }
    if (p.eg_limit && p.eg_plies >= p.eg_limit) { if (winner) *winner = 2; return true; }
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

// Ce qui reste global jeu fermé, et rien d'autre :
// - l'état ouvert/fermé, lu par le registre (tab5_registry.cpp) ;
static UiState g_state = ST_OFF;
// - le slot NVS : le recréer à chaque ouverture ferait fuir un backend de préférences ;
static NvsSlot<DraughtsSave> g_nvs(PREF_KEY, SAVE_MAGIC);
// - IMU shake : on_imu() met à jour l'échantillon précédent même jeu fermé (le
//   registre l'appelle pour tous les jeux) ; repartir de (0, 0, 1) à l'ouverture
//   pourrait faire passer le premier écart pour une secousse (faux indice) ;
static float g_imu_ax = 0, g_imu_ay = 0, g_imu_az = 1;
// - l'anti-rebond de la secousse (800 ms), qui court d'une ouverture à l'autre.
static uint32_t g_last_shake_ms = 0;

static constexpr int HIST_MAX = 24;   // historique coups (notation)
static constexpr int UNDO_MAX = 32;   // pile d'annulation
static constexpr int N_SLOTS = 6;     // panel menus (slots)
// Empreintes de répétition : au plus 80 demi-coups (nulle anglaise) + le départ.
static constexpr int REP_MAX = 82;

// Tout le reste n'existe que jeu ouvert : créé par open(), rendu par close(),
// avec les objets LVGL qu'il pointe (game_common.h, « Mémoire d'un jeu »).
struct Mem {
    UI ui;
    lv_timer_t* timer = nullptr;
    DraughtsSave save{};  // relue de la NVS à chaque ouverture

    Pos pos;
    int  n_legal = 0;            // coups de Cold::legal
    int  sel = -1;               // case sélectionnée (−1 = aucune)
    int  hint_from = -1, hint_to = -1;
    uint32_t hint_until = 0;
    int  winner = -1;            // 0/1/2
    char status[64] = "";

    // Setup brouillon (avant Nouvelle partie)
    uint8_t cfg_variant = 0;
    uint8_t cfg_mode = 0;
    uint8_t cfg_human = 0;
    uint8_t cfg_level = 1;

    // Historique coups (notation)
    char hist[HIST_MAX][12];
    int  hist_n = 0;

    // Undo : nombre de positions de Cold::undo
    int undo_n = 0;

    // Animation capture fade
    int fade_sq[Engine::MAX_CAPS];
    int fade_n = 0;
    uint32_t fade_until = 0;

    // Géométrie damier
    int board_n = 10;
    int cell = 60;
    int board_x = 40;
    int board_y = 26;
    int panel_x = 980;

    // Widgets préalloués (à chaque ouverture). Cases : grille MAX_N complète ;
    // pièces, couronnes et surbrillances : cases foncées seules (dark_slot).
    lv_obj_t* sq_obj[MAX_SQ] = {};
    lv_obj_t* piece_obj[MAX_DARK] = {};
    lv_obj_t* king_ring[MAX_DARK] = {};
    lv_obj_t* hl_obj[MAX_DARK] = {};
    // Ce que chaque case foncée affiche (clé de sync_pieces, 0 = rien). Sans lui,
    // chaque coup restylait les 40 pièces : plus de 32 zones à redessiner, et LVGL
    // repeignait alors tout l'écran (LV_INV_BUF_SIZE, lv_refr.c).
    uint8_t drawn[MAX_DARK] = {};
    lv_obj_t* side_panel = nullptr;
    lv_obj_t* btn_undo = nullptr;
    lv_obj_t* btn_hint = nullptr;
    lv_obj_t* hud_trait = nullptr;
    lv_obj_t* hud_var = nullptr;
    lv_obj_t* hud_lvl = nullptr;
    lv_obj_t* hud_cap = nullptr;
    lv_obj_t* hud_status = nullptr;
    lv_obj_t* hist_lbl[8] = {};
    lv_obj_t* side_btns[4] = {};
    lv_obj_t* side_btns_l[4] = {};

    // Panel menus (slots)
    lv_obj_t* slot[N_SLOTS] = {};
    lv_obj_t* slot_t[N_SLOTS] = {};
    lv_obj_t* slot_d[N_SLOTS] = {};
    lv_obj_t* p_title = nullptr;
    lv_obj_t* p_sub = nullptr;

    // Triple répétition (voir rep_record())
    uint32_t rep[REP_MAX];
    int rep_n = 0;
    const char* draw_reason = nullptr;   // sous-titre de l'écran de fin si nulle
};

// Tampons froids, touchés une fois par coup : bloc COLD (PSRAM d'abord), comme
// l'ancien EXT_RAM_BSS_ATTR.
struct Cold {
    // Coups legaux de l'interface (4,7 Ko) : recalcules une fois par coup,
    // l'IA a ses propres tampons.
    Move legal[MAX_MOVES];
    // Undo : pile de positions (+ historique count)
    Pos undo[UNDO_MAX];
};
static Mem*  gs = nullptr;
static Cold* gc = nullptr;

// ===========================================================================
// NVS
// ===========================================================================

void persist_load() {
    if (!gs) return;  // la sauvegarde n'est en mémoire que jeu ouvert
    if (!g_nvs.load(gs->save)) {
        gs->save = DraughtsSave{};
        gs->save.magic = SAVE_MAGIC;
        gs->save.variant = VAR_INTL10;
        gs->save.mode = 0;
        gs->save.human_color = SIDE_WHITE;
        gs->save.ai_level = Ai::LVL_AMATEUR;
        gs->save.imu_hint = 1;
    }
    gs->cfg_variant = gs->save.variant;
    gs->cfg_mode = gs->save.mode;
    gs->cfg_human = gs->save.human_color;
    gs->cfg_level = gs->save.ai_level;
}

void persist_save() {
    if (!gs || !g_nvs.ready()) return;
    gs->save.variant = gs->cfg_variant;
    gs->save.mode = gs->cfg_mode;
    gs->save.human_color = gs->cfg_human;
    gs->save.ai_level = gs->cfg_level;
    g_nvs.save(gs->save);
}

static void save_position() {
    gs->save.has_game = 1;
    gs->save.side = gs->pos.side;
    gs->save.must_from = gs->pos.must_from;
    gs->save.board_n = gs->pos.n;
    gs->save.no_progress = gs->pos.no_progress;
    gs->save.setup_variant = gs->pos.variant;
    gs->save.setup_mode = gs->cfg_mode;
    gs->save.setup_human = gs->cfg_human;
    gs->save.setup_level = gs->cfg_level;
    memset(gs->save.board, 0, sizeof(gs->save.board));
    memcpy(gs->save.board, gs->pos.sq, (size_t)(gs->pos.n * gs->pos.n));
    persist_save();
}

static void clear_saved_game() {
    gs->save.has_game = 0;
    persist_save();
}

// Triple répétition (FFJD/FMJD) : empreintes des positions (cases + trait) depuis le
// dernier coup irréversible — prise ou coup de pion, après lesquels aucune position
// antérieure ne peut revenir. Au plus 80 demi-coups (nulle anglaise) + le départ
// (REP_MAX ; empreintes dans Mem::rep).

static uint32_t pos_hash(const Pos& p) {
    uint32_t h = 2166136261u;  // FNV-1a
    const int N = p.n * p.n;
    for (int i = 0; i < N; i++) { h ^= p.sq[i]; h *= 16777619u; }
    h ^= p.side;
    h *= 16777619u;
    return h;
}

// À appeler après chaque coup appliqué à Mem::pos, et sur la position de départ.
static void rep_record() {
    if (gs->pos.no_progress == 0) gs->rep_n = 0;
    if (gs->rep_n < REP_MAX) gs->rep[gs->rep_n++] = pos_hash(gs->pos);
}

static bool rep_threefold() {
    if (gs->rep_n == 0) return false;
    const uint32_t h = gs->rep[gs->rep_n - 1];
    int seen = 0;
    for (int i = 0; i < gs->rep_n; i++)
        if (gs->rep[i] == h) seen++;
    return seen >= 3;
}

static bool load_position() {
    if (!gs->save.has_game) return false;
    if (gs->save.board_n != 8 && gs->save.board_n != 10) return false;
    memset(&gs->pos, 0, sizeof(gs->pos));
    gs->pos.n = gs->save.board_n;
    gs->pos.variant = gs->save.setup_variant;
    gs->pos.side = gs->save.side;
    gs->pos.must_from = gs->save.must_from;
    gs->pos.no_progress = gs->save.no_progress;
    memcpy(gs->pos.sq, gs->save.board, (size_t)(gs->pos.n * gs->pos.n));
    // Le compteur de fin de partie réduite n'est pas sauvegardé (layout NVS inchangé) :
    // il repart de zéro à la reprise, ce qui ne peut que retarder la nulle.
    Engine::refresh_endgame(gs->pos);
    gs->cfg_variant = gs->save.setup_variant;
    gs->cfg_mode = gs->save.setup_mode;
    gs->cfg_human = gs->save.setup_human;
    gs->cfg_level = gs->save.setup_level;
    return true;
}

static void record_result(int winner) {
    // Stats vs Tab seulement
    if (gs->cfg_mode != 0) return;
    uint8_t v = gs->pos.variant;
    uint8_t lv = gs->cfg_level;
    if (v >= DRAUGHTS_N_VARIANTS || lv >= DRAUGHTS_N_LEVELS) return;
    // winner 0=blancs, 1=noirs, 2=nulle — du point de vue humain
    if (winner == 2) {
        gs->save.draws[v][lv]++;
    } else {
        bool human_white = (gs->cfg_human == SIDE_WHITE);
        bool human_won = human_white ? (winner == 0) : (winner == 1);
        if (human_won) gs->save.wins[v][lv]++;
        else gs->save.losses[v][lv]++;
    }
    clear_saved_game();
}

// ===========================================================================
// Helpers LVGL
// ===========================================================================







static void panel_on(bool on) { show(gs->ui.panel, on); }

static void slot_list(int i, const char* title, const char* desc, uint32_t col, bool on) {
    if (i < 0 || i >= N_SLOTS) return;
    show(gs->slot[i], on);
    if (!on) return;
    set_text_if(gs->slot_t[i], title);
    set_text_if(gs->slot_d[i], desc);
    lv_obj_set_style_text_color(gs->slot_t[i], lv_color_hex(col), LV_PART_MAIN);
    set_border(gs->slot[i], col, 2, LV_OPA_50);
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
    // rank vaut 1..10 : le modulo borne l'analyse du compilateur (-Wformat-truncation)
    snprintf(buf, buflen, "%c%u", (char)('a' + c), (unsigned) rank % 100u);
}

static void move_to_notation(const Move& m, int n, char* buf, int buflen) {
    char a[4], b[4];  // « a10 » au plus (n <= 10) : borne connue du compilateur, pas de -Wformat-truncation
    sq_to_name(m.from, n, a, sizeof(a));
    sq_to_name(m.to, n, b, sizeof(b));
    if (m.n_caps > 0) snprintf(buf, buflen, "%sx%s", a, b);
    else snprintf(buf, buflen, "%s-%s", a, b);
}

// ===========================================================================
// Layout damier
// ===========================================================================

static void compute_layout() {
    gs->board_n = gs->pos.n;
    // Panneau latéral 280 px ; damier centré dans le reste
    gs->panel_x = 1280 - 300;
    int avail_w = gs->panel_x - 20;
    int avail_h = 672 - 16;
    int cell_w = avail_w / gs->board_n;
    int cell_h = avail_h / gs->board_n;
    gs->cell = cell_w < cell_h ? cell_w : cell_h;
    if (gs->cell > 64) gs->cell = 64;
    if (gs->cell < 40) gs->cell = 40;
    int bw = gs->cell * gs->board_n;
    int bh = gs->cell * gs->board_n;
    gs->board_x = (avail_w - bw) / 2 + 10;
    gs->board_y = (672 - bh) / 2;
}

static void layout_squares() {
    compute_layout();
    const int n = gs->board_n;
    for (int r = 0; r < MAX_N; r++) {
        for (int c = 0; c < MAX_N; c++) {
            int i = r * MAX_N + c;  // widgets indexés 10×10 max
            // Remap : on utilise idx r*n+c pour la logique, widgets sur grille MAX_N
            int wi = r * MAX_N + c;
            bool used = (r < n && c < n);
            show(gs->sq_obj[wi], used);
            const int d = dark_slot(wi);
            if (d >= 0) {
                show(gs->hl_obj[d], false);
                show(gs->piece_obj[d], false);
                show(gs->king_ring[d], false);
                gs->drawn[d] = 0;   // pièce masquée : la prochaine sync la restyle
            }
            if (!used) continue;
            int x = gs->board_x + c * gs->cell;
            int y = gs->board_y + r * gs->cell;
            lv_obj_set_pos(gs->sq_obj[wi], x, y);
            lv_obj_set_size(gs->sq_obj[wi], gs->cell - 1, gs->cell - 1);
            bool dark = Engine::is_dark_sq(r, c);
            set_bg(gs->sq_obj[wi], dark ? Pal::DARK_SQ : Pal::LIGHT_SQ, LV_OPA_COVER);
            if (d < 0) continue;   // case claire : ni pièce ni surbrillance

            lv_obj_set_pos(gs->hl_obj[d], x + 2, y + 2);
            lv_obj_set_size(gs->hl_obj[d], gs->cell - 5, gs->cell - 5);

            int pr = (gs->cell - 1) / 2 - 4;
            if (pr < 10) pr = 10;
            int px = x + (gs->cell - 1 - 2 * pr) / 2;
            int py = y + (gs->cell - 1 - 2 * pr) / 2;
            lv_obj_set_pos(gs->piece_obj[d], px, py);
            lv_obj_set_size(gs->piece_obj[d], 2 * pr, 2 * pr);
            lv_obj_set_style_radius(gs->piece_obj[d], LV_RADIUS_CIRCLE, LV_PART_MAIN);

            lv_obj_set_pos(gs->king_ring[d], px + 3, py + 3);
            lv_obj_set_size(gs->king_ring[d], 2 * pr - 6, 2 * pr - 6);
            lv_obj_set_style_radius(gs->king_ring[d], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        }
    }
    if (gs->side_panel) {
        lv_obj_set_pos(gs->side_panel, gs->panel_x, 8);
        lv_obj_set_size(gs->side_panel, 288, 656);
    }
    if (gs->btn_undo) {
        lv_obj_set_pos(gs->btn_undo, 8, 280);
        lv_obj_set_size(gs->btn_undo, 56, 120);
    }
    if (gs->btn_hint) {
        lv_obj_set_pos(gs->btn_hint, gs->panel_x - 64, 280);
        lv_obj_set_size(gs->btn_hint, 56, 120);
    }
}

static int widget_i(int r, int c) { return r * MAX_N + c; }

// Ne restyle que les cases dont le contenu a changé (clé drawn[]) : un coup en
// touche 2 à 3, plus les prises.
static void sync_pieces() {
    const int n = gs->pos.n;
    const bool fade_on = gs->fade_n > 0 && esphome::millis() < gs->fade_until;
    for (int wi = 0; wi < MAX_SQ; wi++) {
        const int d = dark_slot(wi);
        if (d < 0) continue;   // case claire : jamais de pièce
        const int r = wi / MAX_N, c = wi % MAX_N;
        uint8_t key = 0;       // 0 = case vide (ou hors damier 8×8) : rien d'affiché
        uint8_t pc = EMPTY;
        bool fading = false;   // pièce prise, encore visible 180 ms
        int i = -1;
        if (r < n && c < n) {
            i = r * n + c;
            pc = gs->pos.sq[i];
            if (fade_on)
                for (int k = 0; k < gs->fade_n; k++) if (gs->fade_sq[k] == i) fading = true;
            if (pc != EMPTY || fading) {
                const bool w = (pc != EMPTY) && Engine::is_white(pc);  // prise : pion sombre
                key = (uint8_t)(1 | (w << 1) | (Engine::is_king(pc) << 2) |
                                (fading << 3) | ((i == gs->sel) << 4));
            }
        }
        if (key == gs->drawn[d]) continue;
        gs->drawn[d] = key;
        if (key == 0) {
            show(gs->piece_obj[d], false);
            show(gs->king_ring[d], false);
            continue;
        }
        const bool white = key & 2, king = key & 4;
        set_bg(gs->piece_obj[d], white ? Pal::PIECE_W : Pal::PIECE_B, fading ? LV_OPA_50 : LV_OPA_COVER);
        set_border(gs->piece_obj[d], white ? Pal::PIECE_W_RIM : Pal::PIECE_B_RIM, 2, LV_OPA_COVER);
        show(gs->piece_obj[d], true);
        show(gs->king_ring[d], king);
        if (king) {
            set_bg(gs->king_ring[d], 0, LV_OPA_TRANSP);
            set_border(gs->king_ring[d], Pal::KING_RING, 3, LV_OPA_COVER);
        }
        if (i == gs->sel) set_border(gs->piece_obj[d], Pal::HL_SEL, 3, LV_OPA_COVER);
    }
}

static void clear_highlights() {
    for (int d = 0; d < MAX_DARK; d++) show(gs->hl_obj[d], false);
}

// Surbrillance de la case (r, c) du damier courant ; nullptr pour une case claire
// (show/set_bg l'ignorent).
static lv_obj_t* hl_at(int r, int c) {
    const int d = dark_slot(widget_i(r, c));
    return d < 0 ? nullptr : gs->hl_obj[d];
}

static void show_highlights_for_sel() {
    clear_highlights();
    if (gs->sel < 0) return;
    const int n = gs->pos.n;
    for (int i = 0; i < gs->n_legal; i++) {
        if (gc->legal[i].from != (uint8_t)gs->sel) continue;
        // Surbrille chaque étape du path pour rafles guidées
        for (int k = 0; k < gc->legal[i].n_path; k++) {
            int sq = gc->legal[i].path[k];
            lv_obj_t* h = hl_at(sq / n, sq % n);
            set_bg(h, gc->legal[i].n_caps ? Pal::HL_CAP : Pal::HL_MOVE, LV_OPA_60);
            show(h, true);
        }
    }
    if (gs->hint_from >= 0 && esphome::millis() < gs->hint_until) {
        lv_obj_t* h = hl_at(gs->hint_from / n, gs->hint_from % n);
        set_bg(h, Pal::HL_SEL, LV_OPA_70);
        show(h, true);
        h = hl_at(gs->hint_to / n, gs->hint_to % n);
        set_bg(h, Pal::HL_MOVE, LV_OPA_80);
        show(h, true);
    }
}

static void refresh_legal() {
    gs->n_legal = Engine::gen_moves(gs->pos, gc->legal, MAX_MOVES);
}

static void update_hud() {
    char buf[48];
    const char* side = (gs->pos.side == SIDE_WHITE) ? "Blancs" : "Noirs";
    if (g_state == ST_THINKING) snprintf(buf, sizeof(buf), "Tab reflechit...");
    else snprintf(buf, sizeof(buf), "Trait: %s", side);
    set_text_if(gs->hud_trait, buf);
    set_text_if(gs->hud_var, variant_name(gs->pos.variant));
    snprintf(buf, sizeof(buf), "IA: %s", level_name(gs->cfg_level));
    set_text_if(gs->hud_lvl, buf);

    int max_cap = 0;
    for (int i = 0; i < gs->n_legal; i++)
        if (gc->legal[i].n_caps > max_cap) max_cap = gc->legal[i].n_caps;
    if (max_cap > 0) snprintf(buf, sizeof(buf), "Prise x%d", max_cap);
    else snprintf(buf, sizeof(buf), "—");
    set_text_if(gs->hud_cap, buf);
    set_text_if(gs->hud_status, gs->status);
}

static void update_hist_panel() {
    for (int i = 0; i < 8; i++) {
        int src = gs->hist_n - 8 + i;
        if (src < 0) set_text_if(gs->hist_lbl[i], "");
        else set_text_if(gs->hist_lbl[i], gs->hist[src]);
    }
}

static void push_hist(const Move& m) {
    char buf[12];
    move_to_notation(m, gs->pos.n, buf, sizeof(buf));
    if (gs->hist_n < HIST_MAX) {
        memcpy(gs->hist[gs->hist_n], buf, sizeof(buf));   // même taille (12) : pas de strncpy tronquant
        gs->hist[gs->hist_n][11] = 0;
        gs->hist_n++;
    } else {
        for (int i = 1; i < HIST_MAX; i++) memcpy(gs->hist[i - 1], gs->hist[i], 12);
        memcpy(gs->hist[HIST_MAX - 1], buf, sizeof(buf));
        gs->hist[HIST_MAX - 1][11] = 0;
    }
    update_hist_panel();
}

static void push_undo() {
    if (gs->undo_n < UNDO_MAX) gc->undo[gs->undo_n++] = gs->pos;
    else {
        for (int i = 1; i < UNDO_MAX; i++) gc->undo[i - 1] = gc->undo[i];
        gc->undo[UNDO_MAX - 1] = gs->pos;
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
    if (gs->cfg_mode == 1) return true;                 // PvP
    if (gs->cfg_mode == 2) return false;                // TvT
    // PvT
    return gs->pos.side == gs->cfg_human;
}

// ===========================================================================
// Menus
// ===========================================================================

static void go_hub() {
    g_state = ST_HUB;
    Ai::abort();
    panel_on(true);
    set_text_if(gs->p_title, "Dames Tab");
    set_text_if(gs->p_sub, "Dames internationales — flying kings");
    slot_list(0, "Nouvelle partie", "Setup variante / mode / niveau", Pal::KING_RING, true);
    slot_list(1, gs->save.has_game ? "Reprendre" : "Reprendre (vide)",
              gs->save.has_game ? "Position sauvegardee" : "Aucune partie en cours",
              Pal::HL_MOVE, gs->save.has_game != 0);
    slot_list(2, "Statistiques", "Victoires / nulle / defaites vs Tab", Pal::HL_CAP, true);
    slot_list(3, "Reglages", "Secousse = Hint, reset stats", Pal::TXT_DIM, true);
    slot_list(4, "Quitter", "Retour au tableau de bord", Pal::DANGER, true);
    slot_list(5, "", "", 0, false);
}

static void go_setup() {
    g_state = ST_SETUP;
    panel_on(true);
    set_text_if(gs->p_title, "Nouvelle partie");
    char buf[64];
    snprintf(buf, sizeof(buf), "%s · %s", variant_name(gs->cfg_variant), mode_name(gs->cfg_mode));
    set_text_if(gs->p_sub, buf);
    slot_list(0, variant_name(gs->cfg_variant), "Tap: Intl 10x10 / Anglais 8x8", Pal::LIGHT_SQ, true);
    slot_list(1, mode_name(gs->cfg_mode), "Joueur vs Tab / PvP / Tab vs Tab", Pal::HL_MOVE, true);
    snprintf(buf, sizeof(buf), "Humain: %s", gs->cfg_human == 0 ? "Blancs" : "Noirs");
    slot_list(2, buf, "Couleur (mode vs Tab)", Pal::PIECE_W, gs->cfg_mode == 0);
    slot_list(3, level_name(gs->cfg_level), "Niveau IA", Pal::KING_RING, gs->cfg_mode != 1);
    slot_list(4, "Jouer !", "Lance la partie", Pal::HL_CAP, true);
    slot_list(5, "Retour", "", Pal::TXT_DIM, true);
}

static void go_stats() {
    g_state = ST_STATS;
    panel_on(true);
    set_text_if(gs->p_title, "Statistiques");
    set_text_if(gs->p_sub, "Bilan vs Tab (par variante / niveau)");
    char buf[72];
    uint8_t v = gs->cfg_variant;
    for (int lv = 0; lv < 4; lv++) {
        snprintf(buf, sizeof(buf), "%s — %uW / %uD / %uL",
                 level_name((uint8_t)lv),
                 (unsigned)gs->save.wins[v][lv],
                 (unsigned)gs->save.draws[v][lv],
                 (unsigned)gs->save.losses[v][lv]);
        slot_list(lv, buf, variant_name(v), Pal::TXT, true);
    }
    slot_list(4, "Changer variante stats", variant_name(v), Pal::HL_MOVE, true);
    slot_list(5, "Retour", "", Pal::TXT_DIM, true);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    panel_on(true);
    set_text_if(gs->p_title, "Reglages");
    set_text_if(gs->p_sub, "Options locales (NVS)");
    slot_list(0, gs->save.imu_hint ? "Secousse Hint: ON" : "Secousse Hint: OFF",
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
    if (gs->winner == 0) msg = "Victoire des Blancs";
    else if (gs->winner == 1) msg = "Victoire des Noirs";
    set_text_if(gs->p_title, msg);
    char buf[64];
    // Une nulle dit pourquoi : sans raison affichée, la règle des 25 coups passait
    // pour un bug (partie « finie sans raison », 25/09/2026).
    if (gs->winner == 2 && gs->draw_reason != nullptr)
        snprintf(buf, sizeof(buf), "%s", gs->draw_reason);
    else
        snprintf(buf, sizeof(buf), "%s · %s", variant_name(gs->pos.variant), mode_name(gs->cfg_mode));
    set_text_if(gs->p_sub, buf);
    slot_list(0, "Revanche", "Meme reglage", Pal::KING_RING, true);
    slot_list(1, "Hub", "Menu principal", Pal::TXT_DIM, true);
    slot_list(2, "", "", 0, false);
    slot_list(3, "", "", 0, false);
    slot_list(4, "", "", 0, false);
    slot_list(5, "", "", 0, false);
}

static void start_new_game() {
    Engine::pos_init(gs->pos, (Variant)gs->cfg_variant);
    gs->hist_n = 0;
    gs->undo_n = 0;
    gs->rep_n = 0;
    rep_record();
    gs->sel = -1;
    gs->hint_from = -1;
    gs->winner = -1;
    gs->fade_n = 0;
    strncpy(gs->status, "Nouvelle partie", sizeof(gs->status));
    layout_squares();
    enter_playing();
    save_position();
}

static void resume_game() {
    if (!load_position()) { go_hub(); return; }
    gs->hist_n = 0;
    gs->undo_n = 0;
    gs->rep_n = 0;   // les positions d'avant la sauvegarde ne sont pas connues
    rep_record();
    gs->sel = -1;
    gs->hint_from = -1;
    gs->winner = -1;
    strncpy(gs->status, "Partie reprise", sizeof(gs->status));
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
    gs->draw_reason = nullptr;
    if (Engine::is_terminal(gs->pos, &w)) {
        if (w == 2) {
            if (gs->pos.eg_limit && gs->pos.eg_plies >= gs->pos.eg_limit)
                gs->draw_reason = (gs->pos.eg_limit == Engine::ENDGAME_PLIES_SMALL)
                                    ? "Fin de partie : 5 coups chacun"
                                    : "Fin de partie : 16 coups chacun";
            else
                gs->draw_reason = (gs->pos.variant == VAR_ENG8) ? "40 coups sans pion ni prise"
                                                            : "25 coups sans pion ni prise";
        }
    } else if (rep_threefold()) {
        w = 2;
        gs->draw_reason = "Position repetee 3 fois";
    } else {
        return;
    }
    gs->winner = w;
    record_result(w);
    go_gameover();
}

static void try_start_ai() {
    if (g_state != ST_PLAYING && g_state != ST_THINKING) return;
    if (is_human_turn()) return;
    int w = -1;
    if (Engine::is_terminal(gs->pos, &w)) { finish_if_terminal(); return; }
    g_state = ST_THINKING;
    strncpy(gs->status, "Tab reflechit...", sizeof(gs->status));
    update_hud();
    Ai::begin(gs->pos, (Ai::Level)gs->cfg_level);
}

static void after_human_or_ai_move() {
    finish_if_terminal();
    if (g_state == ST_GAMEOVER) return;
    if (!is_human_turn()) try_start_ai();
    else {
        g_state = ST_PLAYING;
        strncpy(gs->status, gs->n_legal && gc->legal[0].n_caps ? "Prise obligatoire" : "A vous",
                sizeof(gs->status));
        update_hud();
    }
}

static void apply_player_move(const Move& m) {
    push_undo();
    // Prep fade
    gs->fade_n = m.n_caps;
    for (int i = 0; i < m.n_caps; i++) gs->fade_sq[i] = m.caps[i];
    gs->fade_until = esphome::millis() + 180;

    push_hist(m);
    Engine::apply_move(gs->pos, m);
    rep_record();
    gs->sel = -1;
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
    if (gs->undo_n <= 0) {
        strncpy(gs->status, "Rien a annuler", sizeof(gs->status));
        update_hud();
        return;
    }
    // Annule aussi la réponse IA : si PvT et dernier coup = humain aurait
    // besoin de 2 pops. On pop jusqu'au tour humain.
    gs->pos = gc->undo[--gs->undo_n];
    if (gs->cfg_mode == 0 && !is_human_turn() && gs->undo_n > 0) {
        gs->pos = gc->undo[--gs->undo_n];
    }
    if (gs->hist_n > 0) gs->hist_n--;
    if (gs->cfg_mode == 0 && gs->hist_n > 0) gs->hist_n--;  // retire aussi le coup IA
    gs->rep_n = 0;   // prudent : le décompte des répétitions repart de cette position
    rep_record();
    gs->sel = -1;
    g_state = ST_PLAYING;
    refresh_legal();
    sync_pieces();
    clear_highlights();
    update_hist_panel();
    save_position();
    strncpy(gs->status, "Coup annule", sizeof(gs->status));
    update_hud();
    after_human_or_ai_move();
}

static void do_hint() {
    if (g_state != ST_PLAYING) return;
    if (!is_human_turn()) return;
    refresh_legal();
    if (gs->n_legal <= 0) return;
    // Choisit une prise max ou le premier coup
    int best = 0;
    for (int i = 1; i < gs->n_legal; i++)
        if (gc->legal[i].n_caps > gc->legal[best].n_caps) best = i;
    gs->hint_from = gc->legal[best].from;
    gs->hint_to = gc->legal[best].to;
    gs->hint_until = esphome::millis() + 2500;
    gs->sel = gs->hint_from;
    show_highlights_for_sel();
    strncpy(gs->status, "Indice affiche", sizeof(gs->status));
    update_hud();
}

// ===========================================================================
// Input
// ===========================================================================

static void on_square_tap(int r, int c) {
    if (g_state != ST_PLAYING) return;
    if (!is_human_turn()) return;
    const int n = gs->pos.n;
    if (r < 0 || c < 0 || r >= n || c >= n) return;
    if (!Engine::is_dark_sq(r, c)) return;
    int sq = r * n + c;

    // Destination d'un coup légal depuis la sélection ?
    if (gs->sel >= 0) {
        // Cherche un coup from=sel dont path contient sq (étape ou finale)
        int match = -1;
        int match_partial = -1;
        for (int i = 0; i < gs->n_legal; i++) {
            if (gc->legal[i].from != (uint8_t)gs->sel) continue;
            if (gc->legal[i].to == (uint8_t)sq) { match = i; break; }
            // Tap intermédiaire : si un seul coup max passe par cette case
            for (int k = 0; k < gc->legal[i].n_path; k++) {
                if (gc->legal[i].path[k] == (uint8_t)sq) {
                    if (match_partial < 0) match_partial = i;
                    else if (match_partial >= 0 &&
                             gc->legal[match_partial].to != gc->legal[i].to)
                        match_partial = -2;  // ambigu
                }
            }
        }
        if (match >= 0) {
            apply_player_move(gc->legal[match]);
            return;
        }
        // Ambiguïté résolue : un seul chemin
        if (match_partial >= 0) {
            // Filtre les légaux à ceux passant par sq ; si un seul to, joue-le
            int only = -1;
            for (int i = 0; i < gs->n_legal; i++) {
                if (gc->legal[i].from != (uint8_t)gs->sel) continue;
                bool ok = false;
                for (int k = 0; k < gc->legal[i].n_path; k++)
                    if (gc->legal[i].path[k] == (uint8_t)sq) ok = true;
                if (!ok) continue;
                if (only < 0) only = i;
                else if (gc->legal[only].to != gc->legal[i].to) { only = -2; break; }
            }
            if (only >= 0) { apply_player_move(gc->legal[only]); return; }
        }
        // Re-sélection pièce alliée
    }

    uint8_t pc = gs->pos.sq[sq];
    if (pc != EMPTY && Engine::piece_side(pc) == (Side)gs->pos.side) {
        // Vérifie qu'un coup légal part de cette case
        bool ok = false;
        for (int i = 0; i < gs->n_legal; i++)
            if (gc->legal[i].from == (uint8_t)sq) { ok = true; break; }
        if (ok) {
            gs->sel = sq;
            show_highlights_for_sel();
            sync_pieces();
            return;
        }
    }
    gs->sel = -1;
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
    lv_obj_get_coords(gs->ui.field, &a);
    int x = pt.x - a.x1;
    int y = pt.y - a.y1;
    int c = (x - gs->board_x) / gs->cell;
    int r = (y - gs->board_y) / gs->cell;
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
        gs->winner = (gs->cfg_human == SIDE_WHITE) ? 1 : 0;
        if (gs->cfg_mode != 0) gs->winner = (gs->pos.side == SIDE_WHITE) ? 1 : 0;
        record_result(gs->winner);
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
            else if (i == 1 && gs->save.has_game) resume_game();
            else if (i == 2) go_stats();
            else if (i == 3) go_settings();
            else if (i == 4) close();
            break;
        case ST_SETUP:
            if (i == 0) gs->cfg_variant = gs->cfg_variant ? 0 : 1;
            else if (i == 1) gs->cfg_mode = (uint8_t)((gs->cfg_mode + 1) % 3);
            else if (i == 2 && gs->cfg_mode == 0) gs->cfg_human ^= 1;
            else if (i == 3 && gs->cfg_mode != 1)
                gs->cfg_level = (uint8_t)((gs->cfg_level + 1) % 4);
            else if (i == 4) {
                persist_save();
                start_new_game();
                break;
            } else if (i == 5) { go_hub(); break; }
            go_setup();
            break;
        case ST_STATS:
            if (i == 4) { gs->cfg_variant ^= 1; go_stats(); }
            else if (i == 5) go_hub();
            break;
        case ST_SETTINGS:
            if (i == 0) { gs->save.imu_hint ^= 1; persist_save(); go_settings(); }
            else if (i == 1) {
                g_state = ST_CONFIRM_RESET;
                set_text_if(gs->p_title, "Reset stats ?");
                set_text_if(gs->p_sub, "Irréversible");
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
                memset(gs->save.wins, 0, sizeof(gs->save.wins));
                memset(gs->save.draws, 0, sizeof(gs->save.draws));
                memset(gs->save.losses, 0, sizeof(gs->save.losses));
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

// Appelée à chaque ouverture : close() a détruit les objets de l'ouverture précédente.
static void build_ui() {
    set_bg(gs->ui.root, Pal::VOID_BG, LV_OPA_COVER);
    set_bg(gs->ui.hud, Pal::HUD_BG, LV_OPA_COVER);
    set_bg(gs->ui.field, Pal::FLOOR_BG, LV_OPA_COVER);
    set_bg(gs->ui.panel, Pal::VOID_BG, (lv_opa_t)230);

    // Cases (grille 10×10), puis surbrillances / pièces / couronnes des seules cases
    // foncées. Créées après les cases, donc dessinées par-dessus.
    // Cases NON cliquables : le tap est géré par field_event_cb (coordonnées).
    for (int i = 0; i < MAX_SQ; i++) gs->sq_obj[i] = mk_rect(gs->ui.field);
    for (int d = 0; d < MAX_DARK; d++) {
        gs->hl_obj[d] = mk_rect(gs->ui.field);
        lv_obj_set_style_radius(gs->hl_obj[d], 6, LV_PART_MAIN);
        show(gs->hl_obj[d], false);
        gs->piece_obj[d] = mk_rect(gs->ui.field);
        gs->king_ring[d] = mk_rect(gs->ui.field);
        show(gs->piece_obj[d], false);
        show(gs->king_ring[d], false);
        gs->drawn[d] = 0;
    }

    // Panneau latéral gameplay
    gs->side_panel = mk_rect(gs->ui.field);
    set_bg(gs->side_panel, Pal::PANEL_BG, LV_OPA_COVER);
    set_border(gs->side_panel, Pal::BTN_EDGE, 1, LV_OPA_40);

    for (int i = 0; i < 8; i++) {
        gs->hist_lbl[i] = mk_label(gs->side_panel, gs->ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(gs->hist_lbl[i], 12, 16 + i * 28);
    }

    static const char* SBTNS[] = {"Abandon", "Nouvelle", "Menu"};
    for (int i = 0; i < 3; i++) {
        gs->side_btns[i] = mk_rect(gs->side_panel);
        lv_obj_add_flag(gs->side_btns[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(gs->side_btns[i], 16, 280 + i * 70);
        lv_obj_set_size(gs->side_btns[i], 256, 58);
        set_bg(gs->side_btns[i], Pal::BTN, LV_OPA_COVER);
        set_border(gs->side_btns[i], Pal::BTN_EDGE, 1, LV_OPA_60);
        lv_obj_set_style_radius(gs->side_btns[i], 8, LV_PART_MAIN);
        gs->side_btns_l[i] = mk_label(gs->side_btns[i], gs->ui.f_mid, Pal::TXT);
        set_text_if(gs->side_btns_l[i], SBTNS[i]);
        lv_obj_center(gs->side_btns_l[i]);
        lv_obj_add_event_cb(gs->side_btns[i], side_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // Boutons latéraux Undo / Hint
    gs->btn_undo = mk_rect(gs->ui.field);
    lv_obj_add_flag(gs->btn_undo, LV_OBJ_FLAG_CLICKABLE);
    set_bg(gs->btn_undo, Pal::BTN, LV_OPA_COVER);
    set_border(gs->btn_undo, Pal::HL_CAP, 2, LV_OPA_60);
    lv_obj_set_style_radius(gs->btn_undo, 10, LV_PART_MAIN);
    {
        lv_obj_t* l = mk_label(gs->btn_undo, gs->ui.f_small, Pal::TXT);
        set_text_if(l, "Undo");
        lv_obj_center(l);
    }
    lv_obj_add_event_cb(gs->btn_undo, undo_cb, LV_EVENT_CLICKED, nullptr);

    gs->btn_hint = mk_rect(gs->ui.field);
    lv_obj_add_flag(gs->btn_hint, LV_OBJ_FLAG_CLICKABLE);
    set_bg(gs->btn_hint, Pal::BTN, LV_OPA_COVER);
    set_border(gs->btn_hint, Pal::HL_MOVE, 2, LV_OPA_60);
    lv_obj_set_style_radius(gs->btn_hint, 10, LV_PART_MAIN);
    {
        lv_obj_t* l = mk_label(gs->btn_hint, gs->ui.f_small, Pal::TXT);
        set_text_if(l, "Hint");
        lv_obj_center(l);
    }
    lv_obj_add_event_cb(gs->btn_hint, hint_cb, LV_EVENT_CLICKED, nullptr);

    // HUD
    gs->hud_trait = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT);
    lv_obj_set_pos(gs->hud_trait, 16, 12);
    gs->hud_var = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_set_pos(gs->hud_var, 280, 12);
    gs->hud_lvl = mk_label(gs->ui.hud, gs->ui.f_small, Pal::KING_RING);
    lv_obj_set_pos(gs->hud_lvl, 520, 12);
    gs->hud_cap = mk_label(gs->ui.hud, gs->ui.f_small, Pal::HL_CAP);
    lv_obj_set_pos(gs->hud_cap, 780, 12);
    gs->hud_status = mk_label(gs->ui.hud, gs->ui.f_small, Pal::HL_MOVE);
    lv_obj_set_pos(gs->hud_status, 960, 12);

    // Panel menus
    gs->p_title = mk_label(gs->ui.panel, gs->ui.f_big, Pal::KING_RING);
    lv_obj_set_pos(gs->p_title, 80, 40);
    gs->p_sub = mk_label(gs->ui.panel, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_set_pos(gs->p_sub, 80, 100);

    for (int i = 0; i < N_SLOTS; i++) {
        gs->slot[i] = mk_rect(gs->ui.panel);
        lv_obj_add_flag(gs->slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(gs->slot[i], 80, 160 + i * 80);
        lv_obj_set_size(gs->slot[i], 1120, 70);
        set_bg(gs->slot[i], Pal::PANEL_BG, LV_OPA_COVER);
        lv_obj_set_style_radius(gs->slot[i], 10, LV_PART_MAIN);
        gs->slot_t[i] = mk_label(gs->slot[i], gs->ui.f_mid, Pal::TXT);
        lv_obj_set_pos(gs->slot_t[i], 24, 8);
        gs->slot_d[i] = mk_label(gs->slot[i], gs->ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(gs->slot_d[i], 24, 40);
        lv_obj_add_event_cb(gs->slot[i], slot_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

// ===========================================================================
// Timer
// ===========================================================================

static void tick_cb(lv_timer_t* /*t*/) {
    if (g_state == ST_OFF) return;

    // Fade capture
    if (gs->fade_n > 0 && esphome::millis() >= gs->fade_until) {
        gs->fade_n = 0;
        sync_pieces();
    }
    // Hint timeout
    if (gs->hint_from >= 0 && esphome::millis() >= gs->hint_until) {
        gs->hint_from = -1;
        if (g_state == ST_PLAYING) show_highlights_for_sel();
    }

    if (g_state == ST_THINKING) {
        Ai::step();
        if (Ai::ready()) {
            const Move& m = Ai::best();
            push_undo();
            gs->fade_n = m.n_caps;
            for (int i = 0; i < m.n_caps; i++) gs->fade_sq[i] = m.caps[i];
            gs->fade_until = esphome::millis() + 180;
            push_hist(m);
            Engine::apply_move(gs->pos, m);
            rep_record();
            gs->sel = -1;
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
    // Jeu fermé, seuls les globaux IMU ci-dessus sont écrits : rien d'autre n'existe.
    const float mag = accel_delta_norm(ax, ay, az, g_imu_ax, g_imu_ay, g_imu_az);
    if (!gs || g_state != ST_PLAYING || !gs->save.imu_hint) return;
    if (shake_fire(mag, 1.2f, g_last_shake_ms, esphome::millis(), 800)) do_hint();
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel) return;
    gs = game_mem_new<Mem>(MemPref::Internal);
    gc = game_mem_new<Cold>(MemPref::Psram);
    // L'IA prend son état avant go_hub(), qui l'interrompt (Ai::abort).
    if (!gs || !gc || !Ai::acquire()) {
        ESP_LOGW("dames", "memoire introuvable (%u + %u o + IA) : jeu non ouvert",
                 (unsigned) sizeof(Mem), (unsigned) sizeof(Cold));
        game_mem_free(gc);
        game_mem_free(gs);
        if (ui.lvgl) ui.lvgl->show_page(ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
        return;
    }
    gs->ui = ui;
    persist_load();
    build_ui();
    Engine::pos_init(gs->pos, (Variant)gs->cfg_variant);
    layout_squares();

    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).

    lv_obj_add_flag(gs->ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gs->ui.hud, hud_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(gs->ui.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gs->ui.field, field_event_cb, LV_EVENT_CLICKED, nullptr);

    go_hub();
    if (!gs->timer) gs->timer = lv_timer_create(tick_cb, 33, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;
    Ai::release();  // abort + rend l'état de l'IA et ses listes de coups
    if (g_state == ST_PLAYING || g_state == ST_THINKING) save_position();
    persist_save();
    if (gs->timer) { lv_timer_delete(gs->timer); gs->timer = nullptr; }
    if (gs->ui.hud) lv_obj_remove_event_cb(gs->ui.hud, hud_cb);
    if (gs->ui.field) lv_obj_remove_event_cb(gs->ui.field, field_event_cb);
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (gs->ui.lvgl) gs->ui.lvgl->show_page(gs->ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
    g_state = ST_OFF;

    // Rien ne reste réservé : les objets LVGL du jeu (dont le slot dont le callback
    // nous appelle peut-être), puis les blocs qui les pointaient.
    ui_destroy(nullptr, {gs->ui.field, gs->ui.hud, gs->ui.panel});
    game_mem_free(gc);
    game_mem_free(gs);
}

}  // namespace Draughts
