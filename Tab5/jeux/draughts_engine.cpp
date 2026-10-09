/**
 * [AI-CONTEXT]
 * @file draughts_engine.cpp
 * @role Moteur PUR du jeu « Dames Tab » : position de départ, génération des coups
 *      légaux (prise majoritaire et dames volantes en 10×10, prise libre en 8×8),
 *      application d'un coup, compteurs de nulle, évaluation.
 * @architecture_constraint Ni LVGL, ni ESPHome, ni préférences : <cstdint> et
 *      <cstring> seulement. tools/test_draughts_engine.cpp le compile tel quel sur PC
 *      (job `python` de la CI, g++ sous ASan + UBSan, perft 10×10 et 8×8) ; si ce
 *      fichier a besoin d'un en-tête d'ESPHome, il n'est plus pur et ce test casse.
 *      Pièces capturées retirées en FIN de rafle (FMJD). Dames volantes =
 *      international uniquement. Extrait tel quel de draughts_game.cpp (lot G de
 *      l'audit « niveau pro », 09/10/2026) : même code, même comportement.
 * @ai_instruction Règles 10×10 ≠ 8×8 : branchement par Pos.variant, jamais de
 *      mélange. Toute modification de gen_moves() / search_*_caps() / apply_move()
 *      se répercute dans le miroir Python tools/test_draughts_engine.py.
 */
#include "draughts_engine.h"
#include <cstdint>
#include <cstring>

namespace Draughts {
namespace Engine {

// Code déplacé tel quel de draughts_game.cpp, avec ses alignements : le hook
// clang-format (.pre-commit-config.yaml) ne contrôle pas ce bloc, comme le C++
// existant n'est jamais reformaté en entier (.clang-format).
// clang-format off
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
// clang-format on

}  // namespace Engine
}  // namespace Draughts
