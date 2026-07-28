/**
 * [AI-CONTEXT]
 * @file go_engine.cpp
 * @role Implémentation moteur Go (libertés, capture, ko, score chinois).
 * @architecture_constraint Zéro gros tableau en pile : voir le bloc « Scratch »
 *      ci-dessous. Aucune fonction ne s'appelle elle-même de façon réentrante,
 *      et aucune n'appelle une autre fonction du moteur pendant qu'elle lit un
 *      scratch partagé — chaque bloc de commentaire le rappelle là où c'est
 *      délicat (play() en particulier).
 * @architecture_constraint chain_liberties utilise des compteurs de génération
 *      (stamps) au lieu de memset(N) à chaque appel — indispensable pour
 *      l'invariant O(N) de build_chains dans go_ai.
 */
#include "go_engine.h"

namespace Go {
namespace Engine {

static const int DR[4] = {-1, 1, 0, 0};
static const int DC[4] = {0, 0, -1, 1};
static const int DDR[4] = {-1, -1, 1, 1};
static const int DDC[4] = {-1, 1, -1, 1};

// ---------------------------------------------------------------------------
// Scratch de module
// ---------------------------------------------------------------------------
// Le moteur tourne dans le contexte LVGL, mono-thread. Ces tampons remplacent
// les locales de l'ancienne version (2,2 Ko de pile pour count_liberties, plus
// 1,2 Ko pour try_play), qui débordaient la pile de la tâche principale dès que
// l'IA descendait à 2 ou 3 plis.
//
// Propriété : s_mark/s_stack appartiennent à chain_liberties() SEULE ; s_dead et
// s_grp appartiennent à play() ; s_board/s_seen appartiennent au comptage.
// ---------------------------------------------------------------------------
static uint16_t s_mark_gen[MAX_SQ];  // génération « visité » — chain_liberties
static uint16_t s_lib_gen[MAX_SQ];   // génération « liberté vue » — chain_liberties
static uint16_t s_gen = 1;           // compteur de génération (0 = invalide)
static int16_t s_stack[MAX_SQ];      // pile de parcours — chain_liberties
static int16_t s_grp[MAX_SQ];        // chaîne courante — play()
static uint8_t s_removed[MAX_SQ];    // pierres déjà retirées ce coup — play()
static uint8_t s_board[MAX_SQ];      // plateau « pierres mortes retirées » — comptage
static uint8_t s_seen[MAX_SQ];       // régions déjà visitées — comptage

static void bump_gen() {
    s_gen++;
    if (s_gen == 0) {
        // Wrap : on remet à zéro les tableaux et on repart de 1.
        memset(s_mark_gen, 0, sizeof(s_mark_gen));
        memset(s_lib_gen, 0, sizeof(s_lib_gen));
        s_gen = 1;
    }
}

void pos_init(Pos& p, int n) {
    if (n != 9 && n != 13 && n != 19) n = 9;
    memset(&p, 0, sizeof(p));
    p.n = (uint8_t)n;
    p.side = BLACK;
    p.ko = (int16_t)PASS;
}

// ---------------------------------------------------------------------------
// Libertés
// ---------------------------------------------------------------------------

int chain_liberties(const Pos& p, int sq, int16_t* group_out, int* group_size) {
    if (group_size) *group_size = 0;
    const int n = p.n;
    const int N = n * n;
    if (sq < 0 || sq >= N || p.sq[sq] == EMPTY) return 0;
    const uint8_t col = p.sq[sq];

    bump_gen();
    const uint16_t g = s_gen;

    int sp = 0;
    s_stack[sp++] = (int16_t)sq;
    s_mark_gen[sq] = g;
    int libs = 0;
    int gsz = 0;

    while (sp > 0) {
        const int cur = s_stack[--sp];
        if (group_out) group_out[gsz] = (int16_t)cur;
        gsz++;
        const int r = cur / n, c = cur % n;
        for (int d = 0; d < 4; d++) {
            const int nr = r + DR[d], nc = c + DC[d];
            if (!on(nr, nc, n)) continue;
            const int ni = idx(nr, nc, n);
            if (p.sq[ni] == EMPTY) {
                if (s_lib_gen[ni] != g) { s_lib_gen[ni] = g; libs++; }
            } else if (p.sq[ni] == col && s_mark_gen[ni] != g) {
                s_mark_gen[ni] = g;
                s_stack[sp++] = (int16_t)ni;
            }
        }
    }
    if (group_size) *group_size = gsz;
    return libs;
}

// ---------------------------------------------------------------------------
// Légalité
// ---------------------------------------------------------------------------
// Test exact SANS copie de position ni simulation, en trois questions :
//   1. une liberté directe sous la pierre posée ?      -> légal
//   2. une chaîne amie adjacente qui garde une liberté ? -> légal (extension)
//   3. une chaîne adverse adjacente en atari ?         -> légal (capture)
// Sinon c'est un suicide. L'ancienne version simulait le coup complet (copie de
// Pos + jusqu'à 5 remplissages par diffusion) pour chacune des 361 cases, à
// chaque nœud de l'IA : c'était la source du gel.
// ---------------------------------------------------------------------------

bool is_legal(const Pos& p, int sq) {
    if (sq == PASS) return true;
    const int n = p.n;
    const int N = n * n;
    if (sq < 0 || sq >= N) return false;
    if (p.sq[sq] != EMPTY) return false;
    if (sq == (int)p.ko) return false;

    const uint8_t me = p.side;
    const uint8_t you = (uint8_t)opp((Color)me);
    const int r0 = sq / n, c0 = sq % n;

    // 1. Liberté directe.
    for (int d = 0; d < 4; d++) {
        const int nr = r0 + DR[d], nc = c0 + DC[d];
        if (!on(nr, nc, n)) continue;
        if (p.sq[idx(nr, nc, n)] == EMPTY) return true;
    }
    // 2. Extension d'une chaîne amie qui a une AUTRE liberté que `sq`.
    for (int d = 0; d < 4; d++) {
        const int nr = r0 + DR[d], nc = c0 + DC[d];
        if (!on(nr, nc, n)) continue;
        const int ni = idx(nr, nc, n);
        if (p.sq[ni] != me) continue;
        if (chain_liberties(p, ni) >= 2) return true;
    }
    // 3. Capture d'une chaîne adverse en atari (sa dernière liberté est `sq`).
    for (int d = 0; d < 4; d++) {
        const int nr = r0 + DR[d], nc = c0 + DC[d];
        if (!on(nr, nc, n)) continue;
        const int ni = idx(nr, nc, n);
        if (p.sq[ni] != you) continue;
        if (chain_liberties(p, ni) == 1) return true;
    }
    return false;  // suicide
}

// ---------------------------------------------------------------------------
// Application d'un coup
// ---------------------------------------------------------------------------

bool play(Pos& p, int sq) {
    const int n = p.n;
    const int N = n * n;

    if (sq == PASS) {
        p.passes = (uint8_t)(p.passes + 1);
        p.side = (uint8_t)opp((Color)p.side);
        p.ko = (int16_t)PASS;
        p.move_no++;
        return true;
    }
    if (!is_legal(p, sq)) return false;

    const uint8_t me = p.side;
    const uint8_t you = (uint8_t)opp((Color)me);
    p.sq[sq] = me;

    // Captures. s_grp est consommé immédiatement après chaque appel à
    // chain_liberties : aucun appel imbriqué ne peut l'écraser entre-temps.
    int captured = 0;
    int last_cap = -1;
    memset(s_removed, 0, (size_t)N);
    const int r0 = sq / n, c0 = sq % n;
    for (int d = 0; d < 4; d++) {
        const int nr = r0 + DR[d], nc = c0 + DC[d];
        if (!on(nr, nc, n)) continue;
        const int ni = idx(nr, nc, n);
        if (p.sq[ni] != you || s_removed[ni]) continue;
        int gsz = 0;
        if (chain_liberties(p, ni, s_grp, &gsz) != 0) continue;
        for (int k = 0; k < gsz; k++) {
            const int g = s_grp[k];
            s_removed[g] = 1;
            p.sq[g] = EMPTY;
            captured++;
            last_cap = g;
        }
    }

    // Ko simple : la pierre posée capture exactement 1 pierre, est seule dans sa
    // chaîne et n'a qu'une liberté (la case libérée). Reprise immédiate interdite.
    int16_t new_ko = (int16_t)PASS;
    if (captured == 1) {
        int gsz = 0;
        const int libs = chain_liberties(p, sq, s_grp, &gsz);
        if (libs == 1 && gsz == 1) new_ko = (int16_t)last_cap;
    }

    if (me == BLACK) p.captured_by_black = (uint16_t)(p.captured_by_black + captured);
    else             p.captured_by_white = (uint16_t)(p.captured_by_white + captured);

    p.passes = 0;
    p.side = you;
    p.ko = new_ko;
    p.move_no++;
    return true;
}

int gen_moves(const Pos& p, int* out, int max_out) {
    const int N = p.n * p.n;
    int nout = 0;
    for (int i = 0; i < N && nout < max_out; i++) {
        if (p.sq[i] != EMPTY) continue;
        if (is_legal(p, i)) out[nout++] = i;
    }
    return nout;
}

// ---------------------------------------------------------------------------
// Œils
// ---------------------------------------------------------------------------

bool is_eye(const Pos& p, int sq, uint8_t col) {
    const int n = p.n;
    const int N = n * n;
    if (sq < 0 || sq >= N || p.sq[sq] != EMPTY) return false;
    const int r = sq / n, c = sq % n;

    for (int d = 0; d < 4; d++) {
        const int nr = r + DR[d], nc = c + DC[d];
        if (!on(nr, nc, n)) continue;           // le bord fait office de mur ami
        if (p.sq[idx(nr, nc, n)] != col) return false;
    }
    int diag_total = 0, diag_hostile = 0;
    for (int d = 0; d < 4; d++) {
        const int nr = r + DDR[d], nc = c + DDC[d];
        if (!on(nr, nc, n)) continue;
        diag_total++;
        if (p.sq[idx(nr, nc, n)] == (uint8_t)opp((Color)col)) diag_hostile++;
    }
    // Au centre on tolère une diagonale adverse ; sur un bord ou un coin, aucune.
    return diag_hostile <= (diag_total == 4 ? 1 : 0);
}

void mark_chain(const Pos& p, int sq, uint8_t* flags, uint8_t value) {
    if (!flags) return;
    const int N = p.n * p.n;
    if (sq < 0 || sq >= N || p.sq[sq] == EMPTY) return;
    int gsz = 0;
    chain_liberties(p, sq, s_grp, &gsz);
    for (int k = 0; k < gsz; k++) flags[s_grp[k]] = value;
}

// ---------------------------------------------------------------------------
// Comptage (score chinois, aire)
// ---------------------------------------------------------------------------
// Les pierres marquées mortes sont RETIRÉES du plateau avant le comptage : en
// score d'aire, un groupe mort devient purement et simplement du territoire
// adverse, il n'y a pas de prisonnier à décompter en plus.
// ---------------------------------------------------------------------------

// Prépare s_board = plateau sans les pierres mortes, et compte les morts.
static void build_live_board(const Pos& p, const uint8_t* dead,
                             int* dead_b, int* dead_w) {
    const int N = p.n * p.n;
    *dead_b = 0;
    *dead_w = 0;
    for (int i = 0; i < N; i++) {
        uint8_t c = p.sq[i];
        if (c != EMPTY && dead && dead[i]) {
            if (c == BLACK) (*dead_b)++; else (*dead_w)++;
            c = EMPTY;
        }
        s_board[i] = c;
    }
}

// Parcourt les régions vides de s_board et attribue chaque région.
// `emit` reçoit (taille de la région, touche_noir, touche_blanc).
template <typename F>
static void walk_empty_regions(int n, F emit) {
    const int N = n * n;
    memset(s_seen, 0, (size_t)N);
    for (int s = 0; s < N; s++) {
        if (s_board[s] != EMPTY || s_seen[s]) continue;
        int sp = 0;
        s_stack[sp++] = (int16_t)s;
        s_seen[s] = 1;
        // La région est mémorisée dans s_grp pour que `emit` puisse la peindre
        // (territory_map) sans avoir à la reparcourir.
        int size = 0;
        int rn = 0;
        bool tb = false, tw = false;
        while (sp > 0) {
            const int cur = s_stack[--sp];
            s_grp[rn++] = (int16_t)cur;
            size++;
            const int r = cur / n, c = cur % n;
            for (int d = 0; d < 4; d++) {
                const int nr = r + DR[d], nc = c + DC[d];
                if (!on(nr, nc, n)) continue;
                const int ni = idx(nr, nc, n);
                if (s_board[ni] == BLACK) tb = true;
                else if (s_board[ni] == WHITE) tw = true;
                else if (!s_seen[ni]) { s_seen[ni] = 1; s_stack[sp++] = (int16_t)ni; }
            }
        }
        emit(s_grp, rn, size, tb, tw);
    }
}

void score_chinese(const Pos& p, float komi, const uint8_t* dead, Score& out) {
    memset(&out, 0, sizeof(out));
    const int n = p.n;
    const int N = n * n;

    build_live_board(p, dead, &out.black_dead, &out.white_dead);
    for (int i = 0; i < N; i++) {
        if (s_board[i] == BLACK) out.black_stones++;
        else if (s_board[i] == WHITE) out.white_stones++;
    }

    int bt = 0, wt = 0, dame = 0;
    walk_empty_regions(n, [&](const int16_t*, int, int size, bool tb, bool tw) {
        if (tb && !tw) bt += size;
        else if (tw && !tb) wt += size;
        else dame += size;
    });
    out.black_terr = bt;
    out.white_terr = wt;
    out.dame = dame;

    out.black = (float)(out.black_stones + out.black_terr);
    out.white = (float)(out.white_stones + out.white_terr) + komi;
}

void territory_map(const Pos& p, const uint8_t* dead, uint8_t* out) {
    if (!out) return;
    const int n = p.n;
    const int N = n * n;
    memset(out, T_NONE, (size_t)N);

    int db = 0, dw = 0;
    build_live_board(p, dead, &db, &dw);

    walk_empty_regions(n, [&](const int16_t* cells, int rn, int, bool tb, bool tw) {
        uint8_t t = T_DAME;
        if (tb && !tw) t = T_BLACK;
        else if (tw && !tb) t = T_WHITE;
        for (int k = 0; k < rn; k++) out[cells[k]] = t;
    });
}

// ---------------------------------------------------------------------------
// Handicap
// ---------------------------------------------------------------------------
// Placements standards. `e` = distance du bord au point étoile (2 en 9×9,
// 3 sinon), `m` = ligne médiane. Ordre officiel : coins d'abord, puis côtés,
// le centre n'entrant que pour les handicaps impairs.
// ---------------------------------------------------------------------------

int place_handicap(Pos& p, int h) {
    if (h < 2) return 0;
    if (h > 9) h = 9;
    const int n = p.n;
    const int e = (n == 9) ? 2 : 3;
    const int f = n - 1 - e;   // point étoile opposé
    const int m = n / 2;

    // Coins : bas-gauche, haut-droit, bas-droit, haut-gauche.
    const int corners[4] = { idx(f, e, n), idx(e, f, n), idx(f, f, n), idx(e, e, n) };
    // Côtés : gauche, droit, bas, haut.
    const int sides[4]   = { idx(m, e, n), idx(m, f, n), idx(f, m, n), idx(e, m, n) };
    const int center     = idx(m, m, n);

    int pts[9];
    int np = 0;
    const int ncorner = (h >= 4) ? 4 : h;
    for (int i = 0; i < ncorner; i++) pts[np++] = corners[i];
    if (h == 6 || h == 7) { pts[np++] = sides[0]; pts[np++] = sides[1]; }
    if (h == 8 || h == 9) { for (int i = 0; i < 4; i++) pts[np++] = sides[i]; }
    if (h == 5 || h == 7 || h == 9) pts[np++] = center;

    for (int i = 0; i < np; i++) p.sq[pts[i]] = BLACK;
    p.side = WHITE;            // avec handicap, les Blancs commencent
    p.ko = (int16_t)PASS;
    p.passes = 0;
    p.move_no = 0;
    return np;
}

}  // namespace Engine
}  // namespace Go
