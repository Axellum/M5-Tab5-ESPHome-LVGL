/**
 * [AI-CONTEXT]
 * @file draughts_ai.cpp
 * @role Recherche IA time-sliced pour « Dames Tab ».
 * @architecture_constraint Chaque step() consomme un budget de nœuds (~800–2000)
 *      pour rester sous ~25 ms. Iterative deepening + alpha-bêta ; quiescence
 *      (prises seules) au niveau Expert.
 */
#include "draughts_ai.h"
#include <cstring>
#include <cstdlib>

namespace Draughts {
namespace Ai {

using Engine::Pos;
using Engine::Move;
using Engine::Side;
using Engine::SIDE_WHITE;
using Engine::SIDE_BLACK;

static constexpr int MAX_ROOT = Engine::MAX_MOVES;
static constexpr int MAX_DEPTH = 6;

static State   g_state = AI_IDLE;
static Level   g_level = LVL_BEGINNER;
static Pos     g_root;
static Move    g_root_moves[MAX_ROOT];
static int     g_root_n = 0;
static int     g_root_scores[MAX_ROOT];
static int     g_root_i = 0;
static int     g_depth_target = 1;
static int     g_depth_cur = 1;
static Move    g_best;
static bool    g_has_best = false;
static int     g_nodes_left = 0;
static bool    g_truncated = false;
static uint32_t g_rng = 0xC0FFEEu;

static inline uint32_t rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static int max_depth_for(Level lv) {
    switch (lv) {
        case LVL_AMATEUR: return 2;
        case LVL_SOLID:   return 3;
        case LVL_EXPERT:  return 4;
        default:          return 1;
    }
}

static int node_budget_for(Level lv) {
    switch (lv) {
        case LVL_AMATEUR: return 600;
        case LVL_SOLID:   return 1200;
        case LVL_EXPERT:  return 2000;
        default:          return 200;
    }
}

static int eval_side(const Pos& p) {
    int ew = Engine::eval_full(p);
    return (p.side == SIDE_WHITE) ? ew : -ew;
}

// Quiescence negamax : uniquement les prises (Expert).
static int quiescence(Pos p, int alpha, int beta, int qdepth) {
    if (--g_nodes_left <= 0) { g_truncated = true; return eval_side(p); }
    int stand = eval_side(p);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    if (qdepth <= 0) return alpha;

    Move moves[Engine::MAX_MOVES];
    int n = Engine::gen_moves(p, moves, Engine::MAX_MOVES);
    int caps = 0;
    for (int i = 0; i < n; i++) {
        if (moves[i].n_caps == 0) continue;
        if (caps != i) moves[caps] = moves[i];
        caps++;
    }
    if (caps == 0) return alpha;

    for (int i = 0; i < caps; i++) {
        Pos c = p;
        Engine::apply_move(c, moves[i]);
        int sc = -quiescence(c, -beta, -alpha, qdepth - 1);
        if (g_truncated) return alpha;
        if (sc >= beta) return beta;
        if (sc > alpha) alpha = sc;
    }
    return alpha;
}

// Negamax : score du côté au trait, eval toujours +blancs/−noirs.
static int negamax(Pos p, int depth, int alpha, int beta, bool use_q) {
    if (--g_nodes_left <= 0) { g_truncated = true; return 0; }

    int winner = -1;
    if (Engine::is_terminal(p, &winner)) {
        if (winner == 2) return 0;
        // Victoire du côté qui vient de jouer = -côté actuel
        bool white_won = (winner == 0);
        int abs_score = 50000 - (MAX_DEPTH - depth) * 10;
        // Si Blancs ont gagné, score + pour blancs ; negamax veut score pour side
        int for_white = white_won ? abs_score : -abs_score;
        return (p.side == SIDE_WHITE) ? for_white : -for_white;
    }

    if (depth <= 0) {
        if (use_q) return quiescence(p, alpha, beta, 4);
        return eval_side(p);
    }

    Move moves[Engine::MAX_MOVES];
    int n = Engine::gen_moves(p, moves, Engine::MAX_MOVES);
    if (n <= 0) {
        // Pas de coup = défaite du côté au trait
        return -40000 + depth;
    }

    // Tri simple : prises d'abord (meilleur ordre alpha-bêta)
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (moves[j].n_caps > moves[i].n_caps) {
                Move t = moves[i]; moves[i] = moves[j]; moves[j] = t;
            }
        }
    }

    int best = -1000000;
    for (int i = 0; i < n; i++) {
        Pos c = p;
        Engine::apply_move(c, moves[i]);
        int sc = -negamax(c, depth - 1, -beta, -alpha, use_q);
        if (g_truncated) return best;
        if (sc > best) best = sc;
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }
    return best;
}

static void pick_beginner() {
    // Pondération : prises ×4, promotions ×2, sinon 1
    int weights[MAX_ROOT];
    int sum = 0;
    for (int i = 0; i < g_root_n; i++) {
        int w = 1;
        if (g_root_moves[i].n_caps > 0) w = 4 + g_root_moves[i].n_caps;
        if (g_root_moves[i].promote) w += 2;
        weights[i] = w;
        sum += w;
    }
    int r = (int)(rnd() % (uint32_t)sum);
    int acc = 0;
    int choice = 0;
    for (int i = 0; i < g_root_n; i++) {
        acc += weights[i];
        if (r < acc) { choice = i; break; }
    }
    g_best = g_root_moves[choice];
    g_has_best = true;
    g_state = AI_DONE;
}

void begin(const Pos& root, Level level) {
    g_root = root;
    g_level = level;
    g_root_n = Engine::gen_moves(g_root, g_root_moves, MAX_ROOT);
    g_has_best = false;
    g_root_i = 0;
    g_depth_cur = 1;
    g_depth_target = max_depth_for(level);
    g_truncated = false;
    g_rng ^= (uint32_t)esphome::millis();

    if (g_root_n <= 0) {
        g_state = AI_DONE;
        g_has_best = false;
        return;
    }
    if (g_root_n == 1) {
        g_best = g_root_moves[0];
        g_has_best = true;
        g_state = AI_DONE;
        return;
    }
    if (level == LVL_BEGINNER) {
        pick_beginner();
        return;
    }
    // Score initial
    for (int i = 0; i < g_root_n; i++) g_root_scores[i] = -1000000;
    g_best = g_root_moves[0];
    g_has_best = true;
    g_state = AI_THINKING;
}

void step() {
    if (g_state != AI_THINKING) return;

    g_nodes_left = node_budget_for(g_level);
    g_truncated = false;
    bool use_q = (g_level == LVL_EXPERT);

    // Évalue les coups racine un par un à la profondeur courante
    while (g_root_i < g_root_n && g_nodes_left > 0) {
        Pos c = g_root;
        Engine::apply_move(c, g_root_moves[g_root_i]);
        int sc = -negamax(c, g_depth_cur - 1, -1000000, 1000000, use_q);
        if (!g_truncated) {
            g_root_scores[g_root_i] = sc;
            g_root_i++;
        } else {
            break;  // reprendra ce coup au prochain step
        }
    }

    if (g_root_i < g_root_n) return;  // pas fini ce ply

    // Choisit le meilleur à cette profondeur
    int bi = 0;
    for (int i = 1; i < g_root_n; i++) {
        if (g_root_scores[i] > g_root_scores[bi]) bi = i;
        else if (g_root_scores[i] == g_root_scores[bi] && (rnd() & 1)) bi = i;
    }
    g_best = g_root_moves[bi];
    g_has_best = true;

    if (g_depth_cur >= g_depth_target) {
        g_state = AI_DONE;
        return;
    }
    // Iterative deepening : profondeur suivante
    g_depth_cur++;
    g_root_i = 0;
}

State state() { return g_state; }
bool ready() { return g_state == AI_DONE && g_has_best; }
const Move& best() { return g_best; }

void abort() {
    g_state = AI_ABORT;
    g_has_best = false;
}

}  // namespace Ai
}  // namespace Draughts
