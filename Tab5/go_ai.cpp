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
 *   3. PILE PLATE. Aucun tableau de taille MAX_SQ en local : un `Pos` (~372 o)
 *      et la liste de candidats du niveau, rien de plus. Les tables partagées
 *      vivent dans le bloc `Scratch`, pris à l'ouverture du jeu et rendu à sa
 *      fermeture (audit du 26/09/2026, lot 4).
 */
#include "go_ai.h"
#include "esphome.h"
#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif
#include <cstdlib>
#include <cstring>
#include <new>

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
static constexpr float RESIGN_MARGIN = 45.0f;  // écart d'aire → abandon
// Komi de la partie : Scratch::komi (posé par begin()).

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
// Scratch de l'IA (contexte LVGL mono-thread, jamais réentrant)
// ---------------------------------------------------------------------------
// build_chains / gen_cands / static_eval sont TOUJOURS appelés en séquence au
// début d'un nœud, et leurs résultats sont consommés avant toute descente
// récursive. Les partager entre niveaux est donc sûr — et c'est ce qui garde la
// pile plate.
// Tables et état de la recherche vivent dans un bloc pris par scratch_acquire()
// (Go::open) et rendu par scratch_release() (Go::close) : jeu fermé, l'IA ne
// réserve rien. Mêmes valeurs initiales que les anciens globaux ; begin()
// réécrit de toute façon tout l'état avant de chercher.
// ---------------------------------------------------------------------------
struct Cand { int16_t sq; int32_t score; };

struct Scratch {
    float komi = 6.5f;         // komi de la partie (posé par begin())

    uint8_t c_libs[MAX_SQ];    // libertés de la chaîne de la case (0 si vide)
    int16_t c_size[MAX_SQ];    // taille de la chaîne de la case
    int16_t c_root[MAX_SQ];    // représentant de la chaîne (plus petit index)
    uint8_t c_seen[MAX_SQ];
    int16_t c_grp[MAX_SQ];

    uint8_t e_dist[MAX_SQ];    // influence : distance à la pierre la plus proche
    uint8_t e_own[MAX_SQ];     // influence : couleur dominante (3 = neutre)
    int16_t e_queue[MAX_SQ];

    Cand cand[MAX_CAND];
    int  nc = 0;
    int  ci = 0;

    State    state = AI_IDLE;
    Level    level = LVL_BEGINNER;
    Pos      root;
    int      best = PASS;
    int      depth = 1;
    int      depth_target = 1;
    int      done_depth = 0;
    uint32_t cpu_ms = 0;
    uint32_t budget_ms = 1;
    uint32_t deadline = 0;
    uint32_t nodes = 0;
    bool     abort_slice = false;
};
static Scratch* ws = nullptr;

// Seul état hors du bloc : l'aléa. begin() y MÉLANGE la graine de chaque coup
// sans jamais le ré-amorcer ; le remettre à sa valeur d'usine à chaque ouverture
// changerait la suite des tirages d'une session à l'autre.
static uint32_t g_rng = 0x60A10001u;

// Chemin chaud (build_chains à chaque nœud) : RAM interne d'abord, PSRAM en
// secours — la règle MemPref::Internal de game_common.h, sans dépendre de LVGL.
bool scratch_acquire() {
    if (ws) return true;
#if defined(ESP_PLATFORM)
    void* p = heap_caps_malloc(sizeof(Scratch), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(sizeof(Scratch), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    void* p = std::malloc(sizeof(Scratch));
#endif
    if (!p) return false;
    ws = new (p) Scratch();
    return true;
}

void scratch_release() {
    if (!ws) return;
    ws->~Scratch();
#if defined(ESP_PLATFORM)
    heap_caps_free(ws);
#else
    std::free(ws);
#endif
    ws = nullptr;
}

static inline uint32_t rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

// ---------------------------------------------------------------------------
// Tables de chaînes — UNE passe O(N) par position
// ---------------------------------------------------------------------------

// Référence locale au bloc (ici et dans build_influence) : une écriture uint8_t
// peut aliaser le pointeur `ws`, que le compilateur relirait sinon à chaque tour.
static void build_chains(const Pos& p) {
    Scratch& s = *ws;
    const int N = p.n * p.n;
    memset(s.c_libs, 0, (size_t)N);
    memset(s.c_seen, 0, (size_t)N);
    for (int i = 0; i < N; i++) {
        if (p.sq[i] == EMPTY || s.c_seen[i]) continue;
        int gsz = 0;
        const int libs = Engine::chain_liberties(p, i, s.c_grp, &gsz);
        const uint8_t l = (uint8_t)(libs > 255 ? 255 : libs);
        for (int k = 0; k < gsz; k++) {
            const int g = s.c_grp[k];
            s.c_seen[g] = 1;
            s.c_libs[g] = l;
            s.c_size[g] = (int16_t)gsz;
            s.c_root[g] = (int16_t)i;   // i est le plus petit index de la chaîne
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
        if (col == me  && ws->c_libs[ni] >= 2) friendly_ok = true;
        if (col == you && ws->c_libs[ni] == 1) capture = true;
    }
    return friendly_ok || capture;
}

// ---------------------------------------------------------------------------
// Influence — attribution des intersections vides par diffusion multi-source
// ---------------------------------------------------------------------------

static void build_influence(const Pos& p) {
    Scratch& s = *ws;
    const int n = p.n;
    const int N = n * n;
    memset(s.e_dist, 0xFF, (size_t)N);
    memset(s.e_own, 0, (size_t)N);
    int head = 0, tail = 0;
    for (int i = 0; i < N; i++) {
        if (p.sq[i] == EMPTY) continue;
        s.e_dist[i] = 0;
        s.e_own[i] = p.sq[i];
        s.e_queue[tail++] = (int16_t)i;
    }
    while (head < tail) {
        const int cur = s.e_queue[head++];
        const uint8_t o = s.e_own[cur];
        if (o == 3) continue;                 // une zone neutre ne rayonne plus
        const uint8_t d = s.e_dist[cur];
        if (d >= 8) continue;                 // au-delà, l'influence est nulle
        const int r = cur / n, c = cur % n;
        for (int k = 0; k < 4; k++) {
            const int nr = r + DR[k], nc = c + DC[k];
            if (!Engine::on(nr, nc, n)) continue;
            const int ni = Engine::idx(nr, nc, n);
            if (p.sq[ni] != EMPTY) continue;  // on ne diffuse que dans le vide
            if (s.e_dist[ni] == 0xFF) {
                s.e_dist[ni] = (uint8_t)(d + 1);
                s.e_own[ni] = o;
                s.e_queue[tail++] = (int16_t)ni;
            } else if (s.e_dist[ni] == d + 1 && s.e_own[ni] != o) {
                s.e_own[ni] = 3;                // égalité de distance = point neutre
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
        if (col == EMPTY || ws->c_root[i] != (int16_t)i) continue;
        const int gsz = ws->c_size[i];
        const int libs = ws->c_libs[i];
        int v = gsz * 10;
        if (libs <= 1) v -= gsz * 8;
        else if (libs == 2) v -= gsz * 2;
        sc += (col == BLACK) ? v : -v;
    }

    build_influence(p);
    for (int i = 0; i < N; i++) {
        if (p.sq[i] != EMPTY) continue;
        const uint8_t o = ws->e_own[i];
        if (o != BLACK && o != WHITE) continue;
        const int d = ws->e_dist[i] > 8 ? 8 : ws->e_dist[i];
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
            const int libs = ws->c_libs[ni];
            const int gsz = ws->c_size[ni];
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
    if ((++ws->nodes & 31u) == 0u && esphome::millis() >= ws->deadline) {
        ws->abort_slice = true;
        return 0;
    }
    if (Engine::is_over(p)) return eval_side(p);
    if (depth <= 0) return eval_side(p);

    Cand moves[MAX_CAND];
    build_chains(p);
    const int k = LEVELS[ws->level].node_cands;
    int nm = gen_cands(p, moves, k < MAX_CAND ? k : MAX_CAND, 0);
    if (nm == 0) nm = fill_legal_fallback(p, moves, k < MAX_CAND ? k : MAX_CAND);

    int best = -INF;
    for (int i = 0; i < nm; i++) {
        Pos ch = p;
        if (!Engine::play(ch, moves[i].sq)) continue;
        const int sc = -negamax(ch, depth - 1, -beta, -alpha);
        if (ws->abort_slice) return best > -INF ? best : 0;
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
    ws->state = AI_DONE;
}

// Le meilleur candidat après une profondeur entièrement terminée.
static void commit_best() {
    int bi = 0;
    for (int i = 1; i < ws->nc; i++) {
        if (ws->cand[i].score > ws->cand[bi].score) bi = i;
        else if (ws->cand[i].score == ws->cand[bi].score && (rnd() & 1u)) bi = i;
    }
    ws->best = ws->cand[bi].sq;
}

// Faut-il passer d'office ? Deux cas nets, pour éviter les parties sans fin.
static bool should_pass_now(const Pos& p) {
    if (p.move_no > (uint16_t)(p.n * p.n * 2)) return true;
    if (p.passes != 1) return false;              // l'adversaire vient de passer
    Engine::Score s;
    Engine::score_chinese(p, ws->komi, nullptr, s);
    return (p.side == BLACK) ? (s.black > s.white) : (s.white > s.black);
}

// Abandon si l'écart d'aire (toutes pierres vivantes) est désespéré.
// Approximation : pas de marquage morts, mais suffisant pour ne pas jouer
// jusqu'au remplissage du goban en salon.
static bool should_resign_now(const Pos& p) {
    if (ws->level < LVL_AMATEUR) return false;
    if (p.move_no < (uint16_t)(p.n * p.n / 3)) return false;
    Engine::Score s;
    Engine::score_chinese(p, ws->komi, nullptr, s);
    const float me  = (p.side == BLACK) ? s.black : s.white;
    const float you = (p.side == BLACK) ? s.white : s.black;
    return (you - me) >= RESIGN_MARGIN;
}

void begin(const Pos& root, Level level, uint32_t seed, float komi) {
    if (!ws) return;  // brouillon absent : jamais jeu ouvert (Go::open le prend)
    ws->root = root;
    ws->komi = komi;
    ws->level = (level <= LVL_EXPERT) ? level : LVL_AMATEUR;
    const LevelCfg& L = LEVELS[ws->level];
    g_rng ^= seed * 0x9E3779B9u + 0x85EBCA6Bu;
    if (g_rng == 0) g_rng = 0x60A10001u;

    ws->ci = 0;
    ws->nodes = 0;
    ws->cpu_ms = 0;
    ws->done_depth = 0;
    ws->abort_slice = false;
    ws->depth = 1;
    ws->depth_target = L.depth[size_slot(root.n)];
    ws->budget_ms = L.budget_ms ? L.budget_ms : 1;
    ws->best = PASS;

    if (should_resign_now(root)) { ws->best = RESIGN; ws->nc = 0; finish(); return; }
    if (should_pass_now(root)) { ws->nc = 0; finish(); return; }

    build_chains(root);
    const int cap = L.root_cands < MAX_CAND ? L.root_cands : MAX_CAND;
    ws->nc = gen_cands(root, ws->cand, cap, L.noise);
    if (ws->nc == 0) ws->nc = fill_legal_fallback(root, ws->cand, cap);
    if (ws->nc == 0) { finish(); return; }     // vraiment aucun coup légal → passe

    // L'adversaire vient de passer et il ne reste aucun coup TACTIQUE (ni
    // capture, ni atari, ni sauvetage — le meilleur candidat ne vaut qu'un
    // remplissage de dame) : on passe aussi et la partie se termine. Sans cette
    // règle, un Tab en position perdante ferait durer la partie jusqu'à remplir
    // le goban, ce qui est correct au Go mais insupportable en salon.
    // Écartée au niveau Débutant, dont le bruit de ±40 rendrait le seuil absurde.
    if (root.passes == 1 && ws->level >= LVL_AMATEUR && ws->cand[0].score < 45) {
        ws->best = PASS;
        finish();
        return;
    }

    // Un coup valide est disponible dès maintenant : si la recherche est
    // interrompue, on rend le meilleur candidat statique.
    ws->best = ws->cand[0].sq;

    if (ws->depth_target <= 0) {
        // Débutant : tirage pondéré sur les scores statiques (déjà bruités).
        int min_sc = ws->cand[0].score;
        for (int i = 1; i < ws->nc; i++) {
            if (ws->cand[i].score < min_sc) min_sc = ws->cand[i].score;
        }
        uint32_t sum = 0;
        uint32_t weights[MAX_CAND];
        for (int i = 0; i < ws->nc; i++) {
            weights[i] = (uint32_t)(ws->cand[i].score - min_sc + 1);
            sum += weights[i];
        }
        uint32_t pick = (sum > 0) ? (rnd() % sum) : 0;
        for (int i = 0; i < ws->nc; i++) {
            if (pick < weights[i]) { ws->best = ws->cand[i].sq; break; }
            pick -= weights[i];
        }
        finish();
        return;
    }

    for (int i = 0; i < ws->nc; i++) ws->cand[i].score = -INF;
    ws->state = AI_THINKING;
}

void step(uint32_t slice_ms) {
    if (!ws || ws->state != AI_THINKING) return;
    const uint32_t t0 = esphome::millis();
    ws->deadline = t0 + (slice_ms ? slice_ms : 1);
    ws->abort_slice = false;

    bool sliced_out = false;
    while (!sliced_out) {
        while (ws->ci < ws->nc) {
            if (esphome::millis() >= ws->deadline) { sliced_out = true; break; }
            Pos ch = ws->root;
            if (!Engine::play(ch, ws->cand[ws->ci].sq)) {
                ws->cand[ws->ci].score = -INF;
                ws->ci++;
                continue;
            }
            const int sc = (ws->depth <= 1) ? -eval_side(ch)
                                          : -negamax(ch, ws->depth - 1, -INF, INF);
            if (ws->abort_slice) { sliced_out = true; break; }  // candidat rejoué
            ws->cand[ws->ci].score = sc;
            ws->ci++;
        }
        if (sliced_out) break;

        // Profondeur entièrement explorée : on peut publier son résultat.
        commit_best();
        ws->done_depth = ws->depth;
        const uint32_t spent = ws->cpu_ms + (esphome::millis() - t0);
        if (ws->depth >= ws->depth_target || spent >= ws->budget_ms) {
            ws->cpu_ms = spent;
            finish();
            return;
        }
        // Approfondissement itératif : on rejoue dans l'ordre du pli précédent.
        for (int i = 1; i < ws->nc; i++) {
            const Cand t = ws->cand[i];
            int j = i - 1;
            while (j >= 0 && ws->cand[j].score < t.score) { ws->cand[j + 1] = ws->cand[j]; j--; }
            ws->cand[j + 1] = t;
        }
        for (int i = 0; i < ws->nc; i++) ws->cand[i].score = -INF;
        ws->depth++;
        ws->ci = 0;
    }

    ws->cpu_ms += esphome::millis() - t0;

    // Filet de sécurité : quoi qu'il arrive on rend un coup. Si une profondeur a
    // été terminée, `ws->best` en vient ; sinon c'est le meilleur candidat statique.
    if (ws->cpu_ms >= ws->budget_ms && ws->done_depth >= 1) finish();
    else if (ws->cpu_ms >= ws->budget_ms * 3u) finish();
}

// Sans brouillon (jeu fermé) : rien en cours, rien de prêt, la passe par défaut.
State state()      { return ws ? ws->state : AI_IDLE; }
bool  ready()      { return ws && ws->state == AI_DONE; }
int   best_sq()    { return ws ? ws->best : PASS; }
void  abort()      { if (ws) { ws->state = AI_ABORT; ws->best = PASS; } }

int progress_pct() {
    if (!ws) return 0;
    if (ws->state == AI_DONE) return 100;
    if (ws->budget_ms == 0) return 100;
    uint32_t pct = (ws->cpu_ms * 100u) / ws->budget_ms;
    return (int)(pct > 99u ? 99u : pct);
}

}  // namespace Ai
}  // namespace Go
