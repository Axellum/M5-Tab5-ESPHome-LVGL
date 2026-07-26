/**
 * [AI-CONTEXT]
 * @file go_ai.cpp
 * @role IA Go time-sliced — candidats locaux + alpha-bêta limité.
 * @architecture_constraint step() budget ~800–2000 nœuds pour rester < ~25 ms.
 */
#include "go_ai.h"
#include <cstring>

namespace Go {
namespace Ai {

using Engine::Pos;
using Engine::Color;
using Engine::BLACK;
using Engine::WHITE;
using Engine::EMPTY;
using Engine::PASS;

static constexpr int MAX_CAND = 64;

static State g_state = AI_IDLE;
static Level g_level = LVL_BEGINNER;
static Pos   g_root;
static int   g_cands[MAX_CAND];
static int   g_scores[MAX_CAND];
static int   g_nc = 0;
static int   g_ci = 0;
static int   g_best = PASS;
static bool  g_has = false;
static int   g_depth = 1;
static int   g_depth_target = 1;
static int   g_nodes_left = 0;
static bool  g_trunc = false;
static uint32_t g_rng = 0x60A10001u;

static inline uint32_t rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static int max_depth(Level lv) {
    switch (lv) {
        case LVL_AMATEUR: return 1;
        case LVL_SOLID:   return 2;
        case LVL_EXPERT:  return 3;
        default:          return 0;
    }
}

static int budget(Level lv) {
    switch (lv) {
        case LVL_AMATEUR: return 500;
        case LVL_SOLID:   return 1200;
        case LVL_EXPERT:  return 2200;
        default:          return 200;
    }
}

static int max_cands(Level lv) {
    switch (lv) {
        case LVL_AMATEUR: return 24;
        case LVL_SOLID:   return 32;
        case LVL_EXPERT:  return 48;
        default:          return 16;
    }
}

// Candidats : cases vides à ≤2 d'une pierre, + toutes les captures légales.
static int collect_candidates(const Pos& p, int* out, int max_out) {
    const int n = p.n;
    const int N = n * n;
    uint8_t near[Engine::MAX_SQ];
    memset(near, 0, (size_t)N);

    bool any_stone = false;
    for (int i = 0; i < N; i++) {
        if (p.sq[i] == EMPTY) continue;
        any_stone = true;
        int r = i / n, c = i % n;
        for (int dr = -2; dr <= 2; dr++) {
            for (int dc = -2; dc <= 2; dc++) {
                int nr = r + dr, nc = c + dc;
                if (!Engine::on(nr, nc, n)) continue;
                near[Engine::idx(nr, nc, n)] = 1;
            }
        }
    }

    int nout = 0;
    // Priorité : coups qui capturent
    for (int i = 0; i < N && nout < max_out; i++) {
        if (p.sq[i] != EMPTY) continue;
        if (!Engine::is_legal(p, i)) continue;
        Pos t = p;
        if (!Engine::play(t, i)) continue;
        bool cap = (t.captured_by_black + t.captured_by_white) >
                   (p.captured_by_black + p.captured_by_white);
        if (cap) out[nout++] = i;
    }
    for (int i = 0; i < N && nout < max_out; i++) {
        if (p.sq[i] != EMPTY) continue;
        if (!any_stone || near[i]) {
            if (!Engine::is_legal(p, i)) continue;
            bool dup = false;
            for (int k = 0; k < nout; k++) if (out[k] == i) { dup = true; break; }
            if (!dup) out[nout++] = i;
        }
    }
    // Ouverture : si vide, centre et hoshi approximatifs
    if (!any_stone && nout == 0) {
        int mid = n / 2;
        int star = (n >= 13) ? 3 : 2;
        int seeds[] = {
            Engine::idx(mid, mid, n),
            Engine::idx(star, star, n),
            Engine::idx(star, n - 1 - star, n),
            Engine::idx(n - 1 - star, star, n),
            Engine::idx(n - 1 - star, n - 1 - star, n),
        };
        for (int s : seeds) {
            if (nout >= max_out) break;
            if (Engine::is_legal(p, s)) out[nout++] = s;
        }
    }
    return nout;
}

static int eval_side(const Pos& p) {
    int e = Engine::eval(p);
    return (p.side == BLACK) ? e : -e;
}

static int negamax(Pos p, int depth, int alpha, int beta) {
    if (--g_nodes_left <= 0) { g_trunc = true; return 0; }
    if (Engine::is_over(p) || depth <= 0) return eval_side(p);

    int moves[MAX_CAND];
    int nm = collect_candidates(p, moves, max_cands(g_level));
    // Toujours autoriser la passe en recherche profonde faible
    if (nm < MAX_CAND) moves[nm++] = PASS;

    int best = -1000000;
    for (int i = 0; i < nm; i++) {
        Pos c = p;
        if (!Engine::play(c, moves[i])) continue;
        int sc = -negamax(c, depth - 1, -beta, -alpha);
        if (g_trunc) return best;
        if (sc > best) best = sc;
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }
    if (best == -1000000) return eval_side(p);
    return best;
}

static void pick_beginner() {
    if (g_nc <= 0) {
        g_best = PASS;
        g_has = true;
        g_state = AI_DONE;
        return;
    }
    // Pondère : capture ×5, proche centre ×2
    int weights[MAX_CAND];
    int sum = 0;
    const int n = g_root.n;
    const int mid = n / 2;
    for (int i = 0; i < g_nc; i++) {
        int w = 1;
        Pos t = g_root;
        Engine::play(t, g_cands[i]);
        if ((t.captured_by_black + t.captured_by_white) >
            (g_root.captured_by_black + g_root.captured_by_white)) w += 5;
        int r = g_cands[i] / n, c = g_cands[i] % n;
        int dist = (r > mid ? r - mid : mid - r) + (c > mid ? c - mid : mid - c);
        if (dist <= 2) w += 2;
        weights[i] = w;
        sum += w;
    }
    int r = (int)(rnd() % (uint32_t)sum);
    int acc = 0, choice = 0;
    for (int i = 0; i < g_nc; i++) {
        acc += weights[i];
        if (r < acc) { choice = i; break; }
    }
    g_best = g_cands[choice];
    g_has = true;
    g_state = AI_DONE;
}

void begin(const Pos& root, Level level) {
    g_root = root;
    g_level = level;
    g_nc = collect_candidates(g_root, g_cands, max_cands(level));
    g_ci = 0;
    g_has = false;
    g_best = PASS;
    g_depth = 1;
    g_depth_target = max_depth(level);
    g_trunc = false;
    g_rng ^= 0x9E3779B9u + (uint32_t)root.move_no * 0x85EBCA6Bu;

    if (g_nc == 0) {
        g_best = PASS;
        g_has = true;
        g_state = AI_DONE;
        return;
    }
    if (level == LVL_BEGINNER || g_depth_target <= 0) {
        pick_beginner();
        return;
    }
    for (int i = 0; i < g_nc; i++) g_scores[i] = -1000000;
    g_best = g_cands[0];
    g_has = true;
    g_state = AI_THINKING;
}

void step() {
    if (g_state != AI_THINKING) return;
    g_nodes_left = budget(g_level);
    g_trunc = false;

    while (g_ci < g_nc && g_nodes_left > 0) {
        Pos c = g_root;
        if (!Engine::play(c, g_cands[g_ci])) {
            g_scores[g_ci] = -1000000;
            g_ci++;
            continue;
        }
        int sc = -negamax(c, g_depth - 1, -1000000, 1000000);
        if (!g_trunc) {
            g_scores[g_ci] = sc;
            g_ci++;
        } else break;
    }
    if (g_ci < g_nc) return;

    int bi = 0;
    for (int i = 1; i < g_nc; i++) {
        if (g_scores[i] > g_scores[bi]) bi = i;
        else if (g_scores[i] == g_scores[bi] && (rnd() & 1)) bi = i;
    }
    g_best = g_cands[bi];
    g_has = true;

    // Aussi comparer à PASS
    Pos pass_pos = g_root;
    Engine::play(pass_pos, PASS);
    int pass_sc = -negamax(pass_pos, g_depth - 1, -1000000, 1000000);
    if (!g_trunc && pass_sc > g_scores[bi] + 50) g_best = PASS;

    if (g_depth >= g_depth_target) {
        g_state = AI_DONE;
        return;
    }
    g_depth++;
    g_ci = 0;
}

State state() { return g_state; }
bool ready() { return g_state == AI_DONE && g_has; }
int best_sq() { return g_best; }
void abort() { g_state = AI_ABORT; g_has = false; }

}  // namespace Ai
}  // namespace Go
