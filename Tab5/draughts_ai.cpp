/**
 * [AI-CONTEXT]
 * @file draughts_ai.cpp
 * @role Recherche IA time-sliced pour « Dames Tab ».
 * @architecture_constraint Chaque step() consomme un budget de nœuds (~600–2000).
 *      Iterative deepening + alpha-bêta ; quiescence (prises seules) au niveau
 *      Expert. Les listes de coups ne sont JAMAIS sur la pile : un Move[96] pèse
 *      4,7 Ko et la tâche ESPHome n'en a que 8. Elles vivent dans Mem::mbuf (une
 *      ligne par ply), alloué au premier coup réfléchi et rendu par release().
 *      Jeu fermé, l'IA ne réserve rien (audit du 26/09/2026, lot 4) : son état
 *      (struct Mem, RAM interne d'abord) et ses coups racine (struct Cold, PSRAM
 *      d'abord) sont pris par acquire() à l'ouverture du jeu et rendus par
 *      release() à sa fermeture. Sans eux, begin/step/abort ne font rien.
 */
#include "draughts_ai.h"
#include "game_common.h"
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

// Élargissement maximal du budget d'une tranche quand un coup racine n'y tient pas.
static constexpr int MAX_BUDGET_MUL = 4;

// Seul état qui survit à la fermeture du jeu : le générateur pseudo-aléatoire. Il
// évolue de partie en partie et begin() n'y mêle que millis() (ce n'est pas un
// ré-amorçage) : le remettre à 0xC0FFEE à chaque ouverture changerait la suite des
// coups du Débutant et des départages d'égalité.
static uint32_t g_rng = 0xC0FFEEu;

// Tout le reste n'existe que jeu ouvert : pris par acquire() (Draughts::open),
// rendu par release() (Draughts::close) — game_common.h, « Mémoire d'un jeu ».
struct Mem {
    // Listes de coups de la recherche (MBUF_BYTES), prises au premier coup réfléchi
    // par ensure_buffers() et rendues par release().
    Move*   mbuf = nullptr;

    State   state = AI_IDLE;
    Level   level = LVL_BEGINNER;
    Pos     root;
    int     root_n = 0;
    int     root_scores[MAX_ROOT];
    int     root_i = 0;
    int     depth_target = 1;
    int     depth_cur = 1;
    int     depth_done = 0;     // dernière profondeur entièrement évaluée
    int     budget_mul = 1;
    Move    best;
    bool    has_best = false;
    int     nodes_left = 0;
    bool    truncated = false;
    // Relevé de la réflexion en cours, pour le log du coup joué.
    uint32_t t_start = 0;
    int      steps = 0;
    int32_t  nodes_total = 0;
};
// Coups racine (4,7 Ko) : lus une fois par coup racine, pas par nœud → bloc COLD
// (PSRAM d'abord), comme l'ancien EXT_RAM_BSS_ATTR.
struct Cold {
    Move root_moves[MAX_ROOT];
};
static Mem*  gs = nullptr;
static Cold* gc = nullptr;

static inline Move* ply_moves(int ply) { return gs->mbuf + ply * Engine::MAX_MOVES; }

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
    if (gs->mbuf) return true;
    // RAM interne d'abord (plus rapide), PSRAM si elle est trop fragmentée.
    gs->mbuf = static_cast<Move*>(heap_caps_malloc(MBUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!gs->mbuf)
        gs->mbuf = static_cast<Move*>(heap_caps_malloc(MBUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return gs->mbuf != nullptr;
}

static int eval_side(const Pos& p) {
    int ew = Engine::eval_full(p, ply_moves(EVAL_ROW));
    return (p.side == SIDE_WHITE) ? ew : -ew;
}

// Quiescence negamax : uniquement les prises (Expert).
static int quiescence(const Pos& p, int alpha, int beta, int qdepth, int ply) {
    if (--gs->nodes_left <= 0) { gs->truncated = true; return eval_side(p); }
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
        if (gs->truncated) return alpha;
        if (sc >= beta) return beta;
        if (sc > alpha) alpha = sc;
    }
    return alpha;
}

// Negamax : score du côté au trait, eval toujours +blancs/−noirs.
static int negamax(const Pos& p, int depth, int alpha, int beta, bool use_q, int ply) {
    if (--gs->nodes_left <= 0) { gs->truncated = true; return 0; }

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
        if (gs->truncated) return best;
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
    for (int i = 0; i < gs->root_n; i++) {
        int w = 1;
        if (gc->root_moves[i].n_caps > 0) w = 4 + gc->root_moves[i].n_caps;
        if (gc->root_moves[i].promote) w += 2;
        weights[i] = w;
        sum += w;
    }
    int r = (int)(rnd() % (uint32_t)sum);
    int acc = 0;
    int choice = 0;
    for (int i = 0; i < gs->root_n; i++) {
        acc += weights[i];
        if (r < acc) { choice = i; break; }
    }
    gs->best = gc->root_moves[choice];
    gs->has_best = true;
    gs->state = AI_DONE;
}

static void finish() {
    gs->state = AI_DONE;
    ESP_LOGI(TAG, "IA %s : coup joue apres %u ms, %d tranches, %ld noeuds, profondeur %d/%d, "
                  "pile libre min %u o",
             level_name(gs->level), (unsigned) (esphome::millis() - gs->t_start), gs->steps,
             (long) gs->nodes_total, gs->depth_done, gs->depth_target,
             (unsigned) uxTaskGetStackHighWaterMark(nullptr));
}

void begin(const Pos& root, Level level) {
    if (!gs) return;  // jeu fermé : aucun état où chercher
    gs->root = root;
    gs->level = level;
    gs->root_n = Engine::gen_moves(gs->root, gc->root_moves, MAX_ROOT);
    gs->has_best = false;
    gs->root_i = 0;
    gs->depth_cur = 1;
    gs->depth_done = 0;
    gs->depth_target = max_depth_for(level);
    gs->budget_mul = 1;
    gs->truncated = false;
    g_rng ^= (uint32_t)esphome::millis();
    ESP_LOGI(TAG, "IA %s : a son tour, %d coups possibles", level_name(level), gs->root_n);

    if (gs->root_n <= 0) {
        gs->state = AI_DONE;
        gs->has_best = false;
        return;
    }
    if (gs->root_n == 1) {
        gs->best = gc->root_moves[0];
        gs->has_best = true;
        gs->state = AI_DONE;
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
    for (int i = 0; i < gs->root_n; i++) gs->root_scores[i] = -1000000;
    gs->best = gc->root_moves[0];
    gs->has_best = true;
    gs->t_start = esphome::millis();
    gs->steps = 0;
    gs->nodes_total = 0;
    gs->state = AI_THINKING;
}

void step() {
    if (!gs || gs->state != AI_THINKING) return;

    const int budget = node_budget_for(gs->level) * gs->budget_mul;
    gs->nodes_left = budget;
    gs->truncated = false;
    bool use_q = (gs->level == LVL_EXPERT);
    const int first = gs->root_i;
    gs->steps++;

    // Évalue les coups racine un par un à la profondeur courante
    while (gs->root_i < gs->root_n && gs->nodes_left > 0) {
        Pos c = gs->root;
        Engine::apply_move(c, gc->root_moves[gs->root_i]);
        int sc = -negamax(c, gs->depth_cur - 1, -1000000, 1000000, use_q, 0);
        if (!gs->truncated) {
            gs->root_scores[gs->root_i] = sc;
            gs->root_i++;
        } else {
            break;  // reprendra ce coup au prochain step
        }
    }
    gs->nodes_total += budget - (gs->nodes_left > 0 ? gs->nodes_left : 0);

    if (gs->root_i < gs->root_n) {
        // Le premier coup racine de la tranche a eu tout le budget sans aboutir. La
        // recherche est déterministe : il échouerait à l'identique à chaque tranche,
        // et l'IA « réfléchissait » alors sans fin (vu en Expert). On élargit le
        // budget, puis on joue le meilleur coup de la dernière profondeur complète
        // (à défaut, le premier coup légal).
        if (gs->truncated && gs->root_i == first) {
            if (gs->budget_mul < MAX_BUDGET_MUL) {
                gs->budget_mul *= 2;
                return;
            }
            finish();
        }
        return;  // pas fini ce ply
    }

    // Choisit le meilleur à cette profondeur
    int bi = 0;
    for (int i = 1; i < gs->root_n; i++) {
        if (gs->root_scores[i] > gs->root_scores[bi]) bi = i;
        else if (gs->root_scores[i] == gs->root_scores[bi] && (rnd() & 1)) bi = i;
    }
    gs->best = gc->root_moves[bi];
    gs->has_best = true;
    gs->depth_done = gs->depth_cur;

    if (gs->depth_cur >= gs->depth_target) {
        finish();
        return;
    }
    // Iterative deepening : profondeur suivante
    gs->depth_cur++;
    gs->root_i = 0;
}

// Sans état (jeu fermé) : aucune recherche, rien de prêt.
State state() { return gs ? gs->state : AI_IDLE; }
bool ready() { return gs && gs->state == AI_DONE && gs->has_best; }
const Move& best() {
    static const Move NONE{};  // jeu fermé : ready() est faux, personne ne le joue
    return gs ? gs->best : NONE;
}

void abort() {
    if (!gs) return;
    gs->state = AI_ABORT;
    gs->has_best = false;
}

bool acquire() {
    if (!gs) gs = game_mem_new<Mem>(MemPref::Internal);
    if (!gc) gc = game_mem_new<Cold>(MemPref::Psram);
    if (gs && gc) return true;
    ESP_LOGW(TAG, "IA : %u + %u o introuvables", (unsigned) sizeof(Mem), (unsigned) sizeof(Cold));
    release();
    return false;
}

void release() {
    abort();
    if (gs) heap_caps_free(gs->mbuf);  // listes de coups (ensure_buffers), nul si jamais prises
    game_mem_free(gc);
    game_mem_free(gs);
}

}  // namespace Ai
}  // namespace Draughts
