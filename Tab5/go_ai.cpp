/**
 * [AI-CONTEXT]
 * @file go_ai.cpp
 * @role IA Go time-slicée — génération de candidats tactiques + alpha-bêta.
 * @architecture_constraint Trois principes non négociables :
 *   1. BORNÉE PAR LE TEMPS. `step(slice_ms)` regarde l'horloge tous les 32
 *      nœuds ; un budget CPU total par niveau garantit qu'on rend un coup.
 *   2. O(N) PAR NŒUD. Les libertés de TOUTES les chaînes sont calculées une
 *      seule fois par position (build_chains), puis la cotation des candidats
 *      et l'évaluation ne font que lire ces tables. L'ancienne version
 *      appelait is_legal() — qui simulait le coup — sur les 361 intersections
 *      à chaque nœud.
 *   3. PILE PLATE. Aucun tableau de taille MAX_SQ en local : un `Pos` (368 o)
 *      et la liste de candidats du niveau, rien de plus.
 */
#include "go_ai.h"
#include "esphome.h"
#include <cstring>

namespace Go {
namespace Ai {

using Engine::Pos;
using Engine::Color;
using Engine::BLACK;
using Engine::WHITE;
using Engine::EMPTY;
using Engine::PASS;
using Engine::MAX_SQ;

static const int DR[4] = {-1, 1, 0, 0};
static const int DC[4] = {0, 0, -1, 1};

static constexpr int INF = 1000000;
static constexpr int MAX_CAND = 24;   // candidats retenus à la racine
static constexpr float KOMI = 6.5f;

// ---------------------------------------------------------------------------
// Table des niveaux
// ---------------------------------------------------------------------------
// `depth` est indexé par taille de plateau (9 / 13 / 19) : la profondeur baisse
// quand le facteur de branchement explose, sinon « Expert » en 19×19 mettrait
// plus d'une minute à jouer.
// ---------------------------------------------------------------------------
struct LevelCfg {
    const char* name;
    uint8_t  depth[3];    // profondeur cible pour 9×9 / 13×13 / 19×19
    uint8_t  root_cands;  // candidats explorés à la racine
    uint8_t  node_cands;  // candidats explorés aux nœuds internes
    uint16_t budget_ms;   // budget CPU total de réflexion
    uint8_t  noise;       // bruit ajouté au score statique (variété des parties)
};

static const LevelCfg LEVELS[4] = {
    // nom            9  13 19  root node  budget noise
    { "Debutant",   { 0, 0, 0 },  20,  0,     80,  40 },
    { "Amateur",    { 1, 1, 1 },  20, 10,    350,  14 },
    { "Confirme",   { 2, 2, 1 },  18,  9,    900,   5 },
    { "Expert",     { 3, 2, 2 },  16,  8,   1900,   0 },
};

const char* level_name(Level lv) {
    return (lv <= LVL_EXPERT) ? LEVELS[lv].name : "?";
}

static inline int size_slot(int n) { return n <= 9 ? 0 : (n <= 13 ? 1 : 2); }

// ---------------------------------------------------------------------------
// Scratch de module (contexte LVGL mono-thread, jamais réentrant)
// ---------------------------------------------------------------------------
// build_chains / gen_cands / static_eval sont TOUJOURS appelés en séquence au
// début d'un nœud, et leurs résultats sont consommés avant toute descente
// récursive. Les partager entre niveaux est donc sûr — et c'est ce qui garde la
// pile plate.
// ---------------------------------------------------------------------------
static uint8_t c_libs[MAX_SQ];    // libertés de la chaîne de la case (0 si vide)
static int16_t c_size[MAX_SQ];    // taille de la chaîne de la case
static int16_t c_root[MAX_SQ];    // représentant de la chaîne (plus petit index)
static uint8_t c_seen[MAX_SQ];
static int16_t c_grp[MAX_SQ];

static uint8_t e_dist[MAX_SQ];    // influence : distance à la pierre la plus proche
static uint8_t e_own[MAX_SQ];     // influence : couleur dominante (3 = neutre)
static int16_t e_queue[MAX_SQ];

struct Cand { int16_t sq; int32_t score; };
static Cand g_cand[MAX_CAND];
static int  g_nc = 0;
static int  g_ci = 0;

static State    g_state = AI_IDLE;
static Level    g_level = LVL_BEGINNER;
static Pos      g_root;
static int      g_best = PASS;
static int      g_depth = 1;
static int      g_depth_target = 1;
static int      g_done_depth = 0;
static uint32_t g_cpu_ms = 0;
static uint32_t g_budget_ms = 1;
static uint32_t g_deadline = 0;
static uint32_t g_nodes = 0;
static bool     g_abort_slice = false;
static uint32_t g_rng = 0x60A10001u;

static inline uint32_t rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

// ---------------------------------------------------------------------------
// Tables de chaînes — UNE passe O(N) par position
// ---------------------------------------------------------------------------

static void build_chains(const Pos& p) {
    const int N = p.n * p.n;
    memset(c_libs, 0, (size_t)N);
    memset(c_seen, 0, (size_t)N);
    for (int i = 0; i < N; i++) {
        if (p.sq[i] == EMPTY || c_seen[i]) continue;
        int gsz = 0;
        const int libs = Engine::chain_liberties(p, i, c_grp, &gsz);
        const uint8_t l = (uint8_t)(libs > 255 ? 255 : libs);
        for (int k = 0; k < gsz; k++) {
            const int g = c_grp[k];
            c_seen[g] = 1;
            c_libs[g] = l;
            c_size[g] = (int16_t)gsz;
            c_root[g] = (int16_t)i;   // i est le plus petit index de la chaîne
        }
    }
}

// Légalité rapide qui s'appuie sur les tables (aucun parcours de chaîne).
static bool fast_legal(const Pos& p, int sq) {
    const int n = p.n;
    if (p.sq[sq] != EMPTY) return false;
    if (sq == (int)p.ko) return false;
    const uint8_t me = p.side;
    const uint8_t you = (uint8_t)Engine::opp((Color)me);
    const int r = sq / n, c = sq % n;
    bool friendly_ok = false, capture = false;
    for (int d = 0; d < 4; d++) {
        const int nr = r + DR[d], nc = c + DC[d];
        if (!Engine::on(nr, nc, n)) continue;
        const int ni = Engine::idx(nr, nc, n);
        const uint8_t col = p.sq[ni];
        if (col == EMPTY) return true;                    // liberté directe
        if (col == me  && c_libs[ni] >= 2) friendly_ok = true;
        if (col == you && c_libs[ni] == 1) capture = true;
    }
    return friendly_ok || capture;
}

// ---------------------------------------------------------------------------
// Influence — attribution des intersections vides par diffusion multi-source
// ---------------------------------------------------------------------------

static void build_influence(const Pos& p) {
    const int n = p.n;
    const int N = n * n;
    memset(e_dist, 0xFF, (size_t)N);
    memset(e_own, 0, (size_t)N);
    int head = 0, tail = 0;
    for (int i = 0; i < N; i++) {
        if (p.sq[i] == EMPTY) continue;
        e_dist[i] = 0;
        e_own[i] = p.sq[i];
        e_queue[tail++] = (int16_t)i;
    }
    while (head < tail) {
        const int cur = e_queue[head++];
        const uint8_t o = e_own[cur];
        if (o == 3) continue;                 // une zone neutre ne rayonne plus
        const uint8_t d = e_dist[cur];
        if (d >= 8) continue;                 // au-delà, l'influence est nulle
        const int r = cur / n, c = cur % n;
        for (int k = 0; k < 4; k++) {
            const int nr = r + DR[k], nc = c + DC[k];
            if (!Engine::on(nr, nc, n)) continue;
            const int ni = Engine::idx(nr, nc, n);
            if (p.sq[ni] != EMPTY) continue;  // on ne diffuse que dans le vide
            if (e_dist[ni] == 0xFF) {
                e_dist[ni] = (uint8_t)(d + 1);
                e_own[ni] = o;
                e_queue[tail++] = (int16_t)ni;
            } else if (e_dist[ni] == d + 1 && e_own[ni] != o) {
                e_own[ni] = 3;                // égalité de distance = point neutre
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Évaluation statique — en dixièmes de point, du point de vue de NOIR
// ---------------------------------------------------------------------------
// Trois termes seulement, mais ce sont les trois qui se voient à l'écran :
//   · matière     : une pierre vivante vaut un point d'aire ;
//   · sécurité    : une chaîne à 1 liberté est quasiment perdue, à 2 elle est
//                   fragile — c'est ce terme qui fait sauver et attaquer les
//                   ataris, la compétence la plus visible d'un bot de Go ;
//   · influence   : chaque intersection vide va au plus proche, pondérée par la
//                   distance (le territoire lointain est incertain).
// ---------------------------------------------------------------------------

static const int INF_W[9] = { 0, 10, 10, 8, 6, 4, 3, 2, 1 };

static int static_eval(const Pos& p) {
    const int n = p.n;
    const int N = n * n;
    int sc = 0;

    build_chains(p);
    // Une chaîne n'est comptée qu'une fois : au niveau de son représentant.
    for (int i = 0; i < N; i++) {
        const uint8_t col = p.sq[i];
        if (col == EMPTY || c_root[i] != (int16_t)i) continue;
        const int gsz = c_size[i];
        const int libs = c_libs[i];
        int v = gsz * 10;
        if (libs <= 1) v -= gsz * 8;
        else if (libs == 2) v -= gsz * 2;
        sc += (col == BLACK) ? v : -v;
    }

    build_influence(p);
    for (int i = 0; i < N; i++) {
        if (p.sq[i] != EMPTY) continue;
        const uint8_t o = e_own[i];
        if (o != BLACK && o != WHITE) continue;
        const int d = e_dist[i] > 8 ? 8 : e_dist[i];
        const int w = INF_W[d];
        sc += (o == BLACK) ? w : -w;
    }
    return sc;
}

// Évaluation du point de vue du camp au trait (convention négamax).
static inline int eval_side(const Pos& p) {
    const int e = static_eval(p);
    return (p.side == BLACK) ? e : -e;
}

// ---------------------------------------------------------------------------
// Génération de candidats
// ---------------------------------------------------------------------------
// On ne retient que des coups « qui ont l'air d'un coup de Go » : proches d'une
// pierre existante, jamais dans son propre œil, et cotés par une heuristique
// tactique (capture, sauvetage, atari, extension) plus un terme positionnel.
// Sélection des `max_out` meilleurs par tri partiel (max_out ≤ 20, donc O(N·k)
// reste très inférieur au coût d'une évaluation).
// ---------------------------------------------------------------------------

static int gen_cands(const Pos& p, Cand* out, int max_out, int noise) {
    const int n = p.n;
    const int N = n * n;
    const uint8_t me = p.side;
    const uint8_t you = (uint8_t)Engine::opp((Color)me);
    const int mid = n / 2;
    const bool opening = (p.move_no < (uint16_t)(n * 2));

    // Rayon de proximité : 2 intersections autour des pierres posées.
    bool any_stone = false;
    for (int i = 0; i < N; i++) { if (p.sq[i] != EMPTY) { any_stone = true; break; } }

    int nout = 0;
    // Tri par insertion dans un tableau borné : on garde les `max_out` meilleurs.
    for (int i = 0; i < N; i++) {
        if (p.sq[i] != EMPTY) continue;
        if (!fast_legal(p, i)) continue;

        const int r = i / n, c = i % n;
        int sc = 0;
        int empty_nb = 0;
        bool captures = false;
        bool near_stone = !any_stone;

        for (int d = 0; d < 4; d++) {
            const int nr = r + DR[d], nc = c + DC[d];
            if (!Engine::on(nr, nc, n)) continue;
            const int ni = Engine::idx(nr, nc, n);
            const uint8_t col = p.sq[ni];
            if (col == EMPTY) { empty_nb++; continue; }
            near_stone = true;
            const int libs = c_libs[ni];
            const int gsz = c_size[ni];
            if (col == you) {
                if (libs == 1) { sc += 120 + 14 * gsz; captures = true; }
                else if (libs == 2) sc += 26 + 2 * gsz;   // mise en atari
                else sc += 6;                             // contact
            } else {
                if (libs == 1) sc += 100 + 12 * gsz;      // sauvetage
                else if (libs == 2) sc += 18 + gsz;       // renfort
                else sc += 4;                             // extension
            }
        }
        if (!near_stone) {
            // Loin de tout : on regarde le voisinage élargi (rayon 2).
            for (int dr = -2; dr <= 2 && !near_stone; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    const int nr = r + dr, nc = c + dc;
                    if (!Engine::on(nr, nc, n)) continue;
                    if (p.sq[Engine::idx(nr, nc, n)] != EMPTY) { near_stone = true; break; }
                }
            }
            if (!near_stone) continue;
        }
        // Ne jamais se crever un œil (sauf si le coup capture vraiment).
        if (!captures && Engine::is_eye(p, i, me)) continue;

        sc += 4 * empty_nb;

        // Terme positionnel : 3e/4e ligne en ouverture, jamais la 1re.
        const int edge = (r < c ? r : c);
        const int edge2 = (n - 1 - r < n - 1 - c ? n - 1 - r : n - 1 - c);
        const int line = (edge < edge2 ? edge : edge2);
        if (opening) {
            if (line == 0) sc -= 40;
            else if (line == 1) sc -= 12;
            else if (line == 2 || line == 3) sc += 14;
            const int dc_ = (r > mid ? r - mid : mid - r) + (c > mid ? c - mid : mid - c);
            sc += (n - dc_) / 2;
        } else if (line == 0) {
            sc -= 8;
        }

        if (noise) sc += (int)(rnd() % (uint32_t)(noise * 2 + 1)) - noise;

        if (nout < max_out) {
            out[nout].sq = (int16_t)i;
            out[nout].score = sc;
            nout++;
            // Remonter le nouveau venu à sa place (liste triée décroissante).
            for (int k = nout - 1; k > 0 && out[k].score > out[k - 1].score; k--) {
                const Cand t = out[k]; out[k] = out[k - 1]; out[k - 1] = t;
            }
        } else if (sc > out[max_out - 1].score) {
            out[max_out - 1].sq = (int16_t)i;
            out[max_out - 1].score = sc;
            for (int k = max_out - 1; k > 0 && out[k].score > out[k - 1].score; k--) {
                const Cand t = out[k]; out[k] = out[k - 1]; out[k - 1] = t;
            }
        }
    }
    return nout;
}

// Filet de sécurité : si gen_cands a tout filtré (coups trop loin, yeux…),
// on retombe sur les coups strictement légaux. Sans ça, Ai::begin passerait
// alors qu'il reste des coups jouables — Bugbot #72.
static int fill_legal_fallback(const Pos& p, Cand* out, int max_out) {
    int moves[MAX_SQ];
    const int nm = Engine::gen_moves(p, moves, MAX_SQ);
    int nout = 0;
    for (int i = 0; i < nm && nout < max_out; i++) {
        out[nout].sq = (int16_t)moves[i];
        out[nout].score = 1;
        nout++;
    }
    return nout;
}

// ---------------------------------------------------------------------------
// Recherche
// ---------------------------------------------------------------------------

static int negamax(const Pos& p, int depth, int alpha, int beta) {
    // Garde-fou temporel : testé tous les 32 nœuds (un nœud de Go coûte cher,
    // inutile d'aller plus fin).
    if ((++g_nodes & 31u) == 0u && esphome::millis() >= g_deadline) {
        g_abort_slice = true;
        return 0;
    }
    if (Engine::is_over(p)) return eval_side(p);
    if (depth <= 0) return eval_side(p);

    Cand moves[MAX_CAND];
    build_chains(p);
    const int k = LEVELS[g_level].node_cands;
    int nm = gen_cands(p, moves, k < MAX_CAND ? k : MAX_CAND, 0);
    if (nm == 0) nm = fill_legal_fallback(p, moves, k < MAX_CAND ? k : MAX_CAND);

    int best = -INF;
    for (int i = 0; i < nm; i++) {
        Pos ch = p;
        if (!Engine::play(ch, moves[i].sq)) continue;
        const int sc = -negamax(ch, depth - 1, -beta, -alpha);
        if (g_abort_slice) return best > -INF ? best : 0;
        if (sc > best) best = sc;
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }
    if (best == -INF) {
        // Aucun coup jouable : la passe est le seul recours.
        Pos ch = p;
        Engine::play(ch, PASS);
        best = -eval_side(ch);
    }
    return best;
}

static void finish() {
    g_state = AI_DONE;
}

// Le meilleur candidat après une profondeur entièrement terminée.
static void commit_best() {
    int bi = 0;
    for (int i = 1; i < g_nc; i++) {
        if (g_cand[i].score > g_cand[bi].score) bi = i;
        else if (g_cand[i].score == g_cand[bi].score && (rnd() & 1u)) bi = i;
    }
    g_best = g_cand[bi].sq;
}

// Faut-il passer d'office ? Deux cas nets, pour éviter les parties sans fin.
static bool should_pass_now(const Pos& p) {
    if (p.move_no > (uint16_t)(p.n * p.n * 2)) return true;
    if (p.passes != 1) return false;              // l'adversaire vient de passer
    Engine::Score s;
    Engine::score_chinese(p, KOMI, nullptr, s);
    return (p.side == BLACK) ? (s.black > s.white) : (s.white > s.black);
}

void begin(const Pos& root, Level level, uint32_t seed) {
    g_root = root;
    g_level = (level <= LVL_EXPERT) ? level : LVL_AMATEUR;
    const LevelCfg& L = LEVELS[g_level];
    g_rng ^= seed * 0x9E3779B9u + 0x85EBCA6Bu;
    if (g_rng == 0) g_rng = 0x60A10001u;

    g_ci = 0;
    g_nodes = 0;
    g_cpu_ms = 0;
    g_done_depth = 0;
    g_abort_slice = false;
    g_depth = 1;
    g_depth_target = L.depth[size_slot(root.n)];
    g_budget_ms = L.budget_ms ? L.budget_ms : 1;
    g_best = PASS;

    if (should_pass_now(root)) { g_nc = 0; finish(); return; }

    build_chains(root);
    const int cap = L.root_cands < MAX_CAND ? L.root_cands : MAX_CAND;
    g_nc = gen_cands(root, g_cand, cap, L.noise);
    if (g_nc == 0) g_nc = fill_legal_fallback(root, g_cand, cap);
    if (g_nc == 0) { finish(); return; }     // vraiment aucun coup légal → passe

    // L'adversaire vient de passer et il ne reste aucun coup TACTIQUE (ni
    // capture, ni atari, ni sauvetage — le meilleur candidat ne vaut qu'un
    // remplissage de dame) : on passe aussi et la partie se termine. Sans cette
    // règle, un Tab en position perdante ferait durer la partie jusqu'à remplir
    // le goban, ce qui est correct au Go mais insupportable en salon.
    // Écartée au niveau Débutant, dont le bruit de ±40 rendrait le seuil absurde.
    if (root.passes == 1 && g_level >= LVL_AMATEUR && g_cand[0].score < 45) {
        g_best = PASS;
        finish();
        return;
    }

    // Un coup valide est disponible dès maintenant : si la recherche est
    // interrompue, on rend le meilleur candidat statique.
    g_best = g_cand[0].sq;

    if (g_depth_target <= 0) {
        // Débutant : tirage pondéré sur les scores statiques (déjà bruités).
        int min_sc = g_cand[0].score;
        for (int i = 1; i < g_nc; i++) {
            if (g_cand[i].score < min_sc) min_sc = g_cand[i].score;
        }
        uint32_t sum = 0;
        uint32_t weights[MAX_CAND];
        for (int i = 0; i < g_nc; i++) {
            weights[i] = (uint32_t)(g_cand[i].score - min_sc + 1);
            sum += weights[i];
        }
        uint32_t pick = (sum > 0) ? (rnd() % sum) : 0;
        for (int i = 0; i < g_nc; i++) {
            if (pick < weights[i]) { g_best = g_cand[i].sq; break; }
            pick -= weights[i];
        }
        finish();
        return;
    }

    for (int i = 0; i < g_nc; i++) g_cand[i].score = -INF;
    g_state = AI_THINKING;
}

void step(uint32_t slice_ms) {
    if (g_state != AI_THINKING) return;
    const uint32_t t0 = esphome::millis();
    g_deadline = t0 + (slice_ms ? slice_ms : 1);
    g_abort_slice = false;

    bool sliced_out = false;
    while (!sliced_out) {
        while (g_ci < g_nc) {
            if (esphome::millis() >= g_deadline) { sliced_out = true; break; }
            Pos ch = g_root;
            if (!Engine::play(ch, g_cand[g_ci].sq)) {
                g_cand[g_ci].score = -INF;
                g_ci++;
                continue;
            }
            const int sc = (g_depth <= 1) ? -eval_side(ch)
                                          : -negamax(ch, g_depth - 1, -INF, INF);
            if (g_abort_slice) { sliced_out = true; break; }  // candidat rejoué
            g_cand[g_ci].score = sc;
            g_ci++;
        }
        if (sliced_out) break;

        // Profondeur entièrement explorée : on peut publier son résultat.
        commit_best();
        g_done_depth = g_depth;
        const uint32_t spent = g_cpu_ms + (esphome::millis() - t0);
        if (g_depth >= g_depth_target || spent >= g_budget_ms) {
            g_cpu_ms = spent;
            finish();
            return;
        }
        // Approfondissement itératif : on rejoue dans l'ordre du pli précédent.
        for (int i = 1; i < g_nc; i++) {
            const Cand t = g_cand[i];
            int j = i - 1;
            while (j >= 0 && g_cand[j].score < t.score) { g_cand[j + 1] = g_cand[j]; j--; }
            g_cand[j + 1] = t;
        }
        for (int i = 0; i < g_nc; i++) g_cand[i].score = -INF;
        g_depth++;
        g_ci = 0;
    }

    g_cpu_ms += esphome::millis() - t0;

    // Filet de sécurité : quoi qu'il arrive on rend un coup. Si une profondeur a
    // été terminée, `g_best` en vient ; sinon c'est le meilleur candidat statique.
    if (g_cpu_ms >= g_budget_ms && g_done_depth >= 1) finish();
    else if (g_cpu_ms >= g_budget_ms * 3u) finish();
}

State state()      { return g_state; }
bool  ready()      { return g_state == AI_DONE; }
int   best_sq()    { return g_best; }
void  abort()      { g_state = AI_ABORT; g_best = PASS; }

int progress_pct() {
    if (g_state == AI_DONE) return 100;
    if (g_budget_ms == 0) return 100;
    uint32_t pct = (g_cpu_ms * 100u) / g_budget_ms;
    return (int)(pct > 99u ? 99u : pct);
}

}  // namespace Ai
}  // namespace Go
