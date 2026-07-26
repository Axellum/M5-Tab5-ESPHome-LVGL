/**
 * [AI-CONTEXT]
 * @file go_engine.cpp
 * @role Implémentation moteur Go (libertés, capture, ko, score chinois).
 */
#include "go_engine.h"

namespace Go {
namespace Engine {

static const int DR[4] = {-1, 1, 0, 0};
static const int DC[4] = {0, 0, -1, 1};

void pos_init(Pos& p, int n) {
    if (n != 9 && n != 13 && n != 19) n = 9;
    memset(&p, 0, sizeof(p));
    p.n = (uint8_t)n;
    p.side = BLACK;
    p.ko = (uint8_t)PASS;
}

int count_liberties(const Pos& p, int sq, uint8_t* group_mask) {
    const int n = p.n;
    const int N = n * n;
    if (sq < 0 || sq >= N || p.sq[sq] == EMPTY) return 0;
    const Color col = (Color)p.sq[sq];

    uint8_t seen[MAX_SQ];
    uint8_t lib[MAX_SQ];
    memset(seen, 0, (size_t)N);
    memset(lib, 0, (size_t)N);
    if (group_mask) memset(group_mask, 0, (size_t)N);

    int stack[MAX_SQ];
    int sp = 0;
    stack[sp++] = sq;
    seen[sq] = 1;
    int libs = 0;

    while (sp > 0) {
        int cur = stack[--sp];
        if (group_mask) group_mask[cur] = 1;
        int r = cur / n, c = cur % n;
        for (int d = 0; d < 4; d++) {
            int nr = r + DR[d], nc = c + DC[d];
            if (!on(nr, nc, n)) continue;
            int ni = idx(nr, nc, n);
            if (p.sq[ni] == EMPTY) {
                if (!lib[ni]) { lib[ni] = 1; libs++; }
            } else if (p.sq[ni] == col && !seen[ni]) {
                seen[ni] = 1;
                stack[sp++] = ni;
            }
        }
    }
    return libs;
}

// Simule placement + captures ; écrit le résultat dans `tmp` (copie de p puis coup).
// Retourne le nombre de pierres capturées, ou -1 si illégal (occupé / suicide / ko).
static int try_play(const Pos& p, int sq, Pos& tmp, int* ko_out) {
    const int n = p.n;
    const int N = n * n;
    if (sq == PASS) {
        tmp = p;
        tmp.passes = (uint8_t)(p.passes + 1);
        tmp.side = (uint8_t)opp((Color)p.side);
        tmp.ko = (uint8_t)PASS;
        tmp.move_no++;
        if (ko_out) *ko_out = PASS;
        return 0;
    }
    if (sq < 0 || sq >= N) return -1;
    if (p.sq[sq] != EMPTY) return -1;
    if (sq == (int)p.ko) return -1;

    tmp = p;
    const Color me = (Color)p.side;
    const Color you = opp(me);
    tmp.sq[sq] = (uint8_t)me;
    tmp.ko = (uint8_t)PASS;
    tmp.passes = 0;

    int captured = 0;
    int cap_sq = -1;
    uint8_t remove[MAX_SQ];
    memset(remove, 0, (size_t)N);

    int r0 = sq / n, c0 = sq % n;
    for (int d = 0; d < 4; d++) {
        int nr = r0 + DR[d], nc = c0 + DC[d];
        if (!on(nr, nc, n)) continue;
        int ni = idx(nr, nc, n);
        if (tmp.sq[ni] != you) continue;
        uint8_t gmask[MAX_SQ];
        int libs = count_liberties(tmp, ni, gmask);
        if (libs == 0) {
            for (int i = 0; i < N; i++) {
                if (!gmask[i] || remove[i]) continue;
                remove[i] = 1;
                captured++;
                cap_sq = i;
            }
        }
    }
    for (int i = 0; i < N; i++) {
        if (remove[i]) tmp.sq[i] = EMPTY;
    }

    // Suicide ?
    if (count_liberties(tmp, sq, nullptr) == 0) return -1;

    // Ko simple : capture exactement 1 pierre et le coup est un « snapback » 1×1
    int new_ko = PASS;
    if (captured == 1) {
        // Si le groupe joué n'a qu'une liberté et une seule pierre → ko
        uint8_t gmask[MAX_SQ];
        int libs = count_liberties(tmp, sq, gmask);
        int gsize = 0;
        for (int i = 0; i < N; i++) if (gmask[i]) gsize++;
        if (libs == 1 && gsize == 1) new_ko = cap_sq;
    }
    if (ko_out) *ko_out = new_ko;

    if (me == BLACK) tmp.captured_by_black = (uint16_t)(tmp.captured_by_black + captured);
    else tmp.captured_by_white = (uint16_t)(tmp.captured_by_white + captured);

    tmp.side = (uint8_t)you;
    tmp.ko = (uint8_t)new_ko;
    tmp.move_no++;
    return captured;
}

bool is_legal(const Pos& p, int sq) {
    if (sq == PASS) return true;
    Pos tmp;
    return try_play(p, sq, tmp, nullptr) >= 0;
}

bool play(Pos& p, int sq) {
    Pos tmp;
    int ko = PASS;
    int cap = try_play(p, sq, tmp, &ko);
    if (cap < 0) return false;
    p = tmp;
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

void score_chinese(const Pos& p, float komi, Score& out) {
    memset(&out, 0, sizeof(out));
    const int n = p.n;
    const int N = n * n;
    uint8_t seen[MAX_SQ];
    memset(seen, 0, (size_t)N);

    for (int i = 0; i < N; i++) {
        if (p.sq[i] == BLACK) out.black_stones++;
        else if (p.sq[i] == WHITE) out.white_stones++;
    }

    // Territoire : régions vides touchant une seule couleur
    for (int s = 0; s < N; s++) {
        if (p.sq[s] != EMPTY || seen[s]) continue;
        int stack[MAX_SQ];
        int sp = 0;
        stack[sp++] = s;
        seen[s] = 1;
        int region = 0;
        bool touch_b = false, touch_w = false;
        while (sp > 0) {
            int cur = stack[--sp];
            region++;
            int r = cur / n, c = cur % n;
            for (int d = 0; d < 4; d++) {
                int nr = r + DR[d], nc = c + DC[d];
                if (!on(nr, nc, n)) continue;
                int ni = idx(nr, nc, n);
                if (p.sq[ni] == BLACK) touch_b = true;
                else if (p.sq[ni] == WHITE) touch_w = true;
                else if (!seen[ni]) {
                    seen[ni] = 1;
                    stack[sp++] = ni;
                }
            }
        }
        if (touch_b && !touch_w) out.black_terr += region;
        else if (touch_w && !touch_b) out.white_terr += region;
        else out.dame += region;
    }

    out.black = (float)(out.black_stones + out.black_terr);
    out.white = (float)(out.white_stones + out.white_terr) + komi;
}

int eval(const Pos& p) {
    // Heuristique rapide : pierres + captures + bonus libertés grossier
    int sc = 0;
    const int N = p.n * p.n;
    for (int i = 0; i < N; i++) {
        if (p.sq[i] == BLACK) sc += 100;
        else if (p.sq[i] == WHITE) sc -= 100;
    }
    sc += (int)p.captured_by_black * 100;
    sc -= (int)p.captured_by_white * 100;

    // Échantillon territoire (score chinois sans komi, coût OK sur 9×9)
    Score s;
    score_chinese(p, 0.0f, s);
    sc += (s.black_terr - s.white_terr) * 40;
    return sc;
}

}  // namespace Engine
}  // namespace Go
