/**
 * [AI-CONTEXT]
 * @file draughts_ai.cpp
 * @role Recherche IA time-sliced pour « Dames Tab ».
 * @architecture_constraint Chaque step() consomme un budget de nœuds (~600–2000).
 *      Iterative deepening + alpha-bêta ; quiescence (prises seules) au niveau
 *      Expert. Les listes de coups ne sont JAMAIS sur la pile : un Move[96] pèse
 *      4,7 Ko et la tâche ESPHome n'en a que 8. Elles vivent dans g_mbuf (une
 *      ligne par ply), alloué au premier coup réfléchi et rendu par release().
 */
#include "draughts_ai.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdlib>

namespace Draughts {
namespace Ai {

static const char* const TAG = "dames";

using Engine::Pos;
using Engine::Move;
using Engine::Side;
using Engine::SIDE_WHITE;
using Engine::SIDE_BLACK;

static constexpr int MAX_ROOT = Engine::MAX_MOVES;
static constexpr int MAX_DEPTH = 6;

// Listes de coups de la recherche, une ligne de MAX_MOVES par ply. En Expert,
// negamax génère sur 3 plies (profondeur 4 moins la racine) et la quiescence sur
// 4 de plus : 7 lignes servent, la 8e est une marge (au-delà, on évalue sans
// générer). La ligne EVAL_ROW est le brouillon d'eval_full(), appelée en feuille.
static constexpr int MAX_PLY_BUF = 8;
static constexpr int EVAL_ROW = MAX_PLY_BUF;
static constexpr size_t MBUF_BYTES = sizeof(Move) * (MAX_PLY_BUF + 1) * Engine::MAX_MOVES;
static Move* g_mbuf = nullptr;

static inline Move* ply_moves(int ply) { return g_mbuf + ply * Engine::MAX_MOVES; }

// Élargissement maximal du budget d'une tranche quand un coup racine n'y tient pas.
static constexpr int MAX_BUDGET_MUL = 4;

static State   g_state = AI_IDLE;
static Level   g_level = LVL_BEGINNER;
static Pos     g_root;
static Move    g_root_moves[MAX_ROOT];
static int     g_root_n = 0;
static int     g_root_scores[MAX_ROOT];
static int     g_root_i = 0;
static int     g_depth_target = 1;
static int     g_depth_cur = 1;
static int     g_depth_done = 0;     // dernière profondeur entièrement évaluée
static int     g_budget_mul = 1;
static Move    g_best;
static bool    g_has_best = false;
static int     g_nodes_left = 0;
static bool    g_truncated = false;
static uint32_t g_rng = 0xC0FFEEu;
// Relevé de la réflexion en cours, pour le log du coup joué.
static uint32_t g_t_start = 0;
static int      g_steps = 0;
static int32_t  g_nodes_total = 0;

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

static const char* level_name(Level lv) {
    switch (lv) {
        case LVL_AMATEUR: return "Amateur";
        case LVL_SOLID:   return "Confirme";
        case LVL_EXPERT:  return "Expert";
        default:          return "Debutant";
    }
}

static bool ensure_buffers() {
    if (g_mbuf) return true;
    // RAM interne d'abord (plus rapide), PSRAM si elle est trop fragmentée.
    g_mbuf = static_cast<Move*>(heap_caps_malloc(MBUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!g_mbuf)
        g_mbuf = static_cast<Move*>(heap_caps_malloc(MBUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return g_mbuf != nullptr;
}

static int eval_side(const Pos& p) {
    int ew = Engine::eval_full(p, ply_moves(EVAL_ROW));
    return (p.side == SIDE_WHITE) ? ew : -ew;
}

// Quiescence negamax : uniquement les prises (Expert).
static int quiescence(const Pos& p, int alpha, int beta, int qdepth, int ply) {
    if (--g_nodes_left <= 0) { g_truncated = true; return eval_side(p); }
    int stand = eval_side(p);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    if (qdepth <= 0 || ply >= MAX_PLY_BUF) return alpha;

    Move* moves = ply_moves(ply);
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
        int sc = -quiescence(c, -beta, -alpha, qdepth - 1, ply + 1);
        if (g_truncated) return alpha;
        if (sc >= beta) return beta;
        if (sc > alpha) alpha = sc;
    }
    return alpha;
}

// Negamax : score du côté au trait, eval toujours +blancs/−noirs.
static int negamax(const Pos& p, int depth, int alpha, int beta, bool use_q, int ply) {
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
        if (use_q) return quiescence(p, alpha, beta, 4, ply);
        return eval_side(p);
    }
    if (ply >= MAX_PLY_BUF) return eval_side(p);

    Move* moves = ply_moves(ply);
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
        int sc = -negamax(c, depth - 1, -beta, -alpha, use_q, ply + 1);
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

static void finish() {
    g_state = AI_DONE;
    ESP_LOGI(TAG, "IA %s : coup joue apres %u ms, %d tranches, %ld noeuds, profondeur %d/%d, "
                  "pile libre min %u o",
             level_name(g_level), (unsigned) (esphome::millis() - g_t_start), g_steps,
             (long) g_nodes_total, g_depth_done, g_depth_target,
             (unsigned) uxTaskGetStackHighWaterMark(nullptr));
}

void begin(const Pos& root, Level level) {
    g_root = root;
    g_level = level;
    g_root_n = Engine::gen_moves(g_root, g_root_moves, MAX_ROOT);
    g_has_best = false;
    g_root_i = 0;
    g_depth_cur = 1;
    g_depth_done = 0;
    g_depth_target = max_depth_for(level);
    g_budget_mul = 1;
    g_truncated = false;
    g_rng ^= (uint32_t)esphome::millis();
    ESP_LOGI(TAG, "IA %s : a son tour, %d coups possibles", level_name(level), g_root_n);

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
    if (!ensure_buffers()) {
        ESP_LOGW(TAG, "IA %s : %u o introuvables pour la recherche, coup du niveau Debutant",
                 level_name(level), (unsigned) MBUF_BYTES);
        pick_beginner();
        return;
    }
    // Score initial
    for (int i = 0; i < g_root_n; i++) g_root_scores[i] = -1000000;
    g_best = g_root_moves[0];
    g_has_best = true;
    g_t_start = esphome::millis();
    g_steps = 0;
    g_nodes_total = 0;
    g_state = AI_THINKING;
}

void step() {
    if (g_state != AI_THINKING) return;

    const int budget = node_budget_for(g_level) * g_budget_mul;
    g_nodes_left = budget;
    g_truncated = false;
    bool use_q = (g_level == LVL_EXPERT);
    const int first = g_root_i;
    g_steps++;

    // Évalue les coups racine un par un à la profondeur courante
    while (g_root_i < g_root_n && g_nodes_left > 0) {
        Pos c = g_root;
        Engine::apply_move(c, g_root_moves[g_root_i]);
        int sc = -negamax(c, g_depth_cur - 1, -1000000, 1000000, use_q, 0);
        if (!g_truncated) {
            g_root_scores[g_root_i] = sc;
            g_root_i++;
        } else {
            break;  // reprendra ce coup au prochain step
        }
    }
    g_nodes_total += budget - (g_nodes_left > 0 ? g_nodes_left : 0);

    if (g_root_i < g_root_n) {
        // Le premier coup racine de la tranche a eu tout le budget sans aboutir. La
        // recherche est déterministe : il échouerait à l'identique à chaque tranche,
        // et l'IA « réfléchissait » alors sans fin (vu en Expert). On élargit le
        // budget, puis on joue le meilleur coup de la dernière profondeur complète
        // (à défaut, le premier coup légal).
        if (g_truncated && g_root_i == first) {
            if (g_budget_mul < MAX_BUDGET_MUL) {
                g_budget_mul *= 2;
                return;
            }
            finish();
        }
        return;  // pas fini ce ply
    }

    // Choisit le meilleur à cette profondeur
    int bi = 0;
    for (int i = 1; i < g_root_n; i++) {
        if (g_root_scores[i] > g_root_scores[bi]) bi = i;
        else if (g_root_scores[i] == g_root_scores[bi] && (rnd() & 1)) bi = i;
    }
    g_best = g_root_moves[bi];
    g_has_best = true;
    g_depth_done = g_depth_cur;

    if (g_depth_cur >= g_depth_target) {
        finish();
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

void release() {
    abort();
    heap_caps_free(g_mbuf);
    g_mbuf = nullptr;
}

}  // namespace Ai
}  // namespace Draughts
