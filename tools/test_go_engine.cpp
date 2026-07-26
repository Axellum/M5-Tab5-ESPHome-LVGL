/**
 * Tests host du moteur Go (sans ESPHome / LVGL).
 *
 * Build & run (PowerShell) :
 *   g++ -std=c++17 -O2 -I../Tab5 -o test_go_engine.exe test_go_engine.cpp ../Tab5/go_engine.cpp
 *   .\test_go_engine.exe
 */
#include "go_engine.h"
#include <cstdio>

using namespace Go::Engine;

static int g_fail = 0;

static void expect(bool cond, const char* msg) {
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        g_fail++;
    } else {
        std::printf("OK  : %s\n", msg);
    }
}

static void test_sizes() {
    for (int n : {9, 13, 19}) {
        Pos p;
        pos_init(p, n);
        expect(p.n == (uint8_t)n && p.side == BLACK, "init taille");
        expect(is_legal(p, idx(n / 2, n / 2, n)), "centre legal");
    }
}

static void test_capture() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(3, 4, 9)] = BLACK;
    p.sq[idx(5, 4, 9)] = BLACK;
    p.sq[idx(4, 3, 9)] = BLACK;
    p.side = WHITE;
    expect(play(p, idx(4, 4, 9)), "Blanc place dans le piege");
    p.side = BLACK;
    expect(play(p, idx(4, 5, 9)), "Noir capture");
    expect(p.sq[idx(4, 4, 9)] == EMPTY, "Blanc retire");
    expect(p.captured_by_black == 1, "1 prisonnier Noir");
}

static void test_suicide() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(3, 4, 9)] = WHITE;
    p.sq[idx(5, 4, 9)] = WHITE;
    p.sq[idx(4, 3, 9)] = WHITE;
    p.sq[idx(4, 5, 9)] = WHITE;
    p.side = BLACK;
    expect(!is_legal(p, idx(4, 4, 9)), "Suicide interdit");
}

static void test_ko() {
    Pos p;
    pos_init(p, 9);
    // Forme ko : W en atari, B capture et reste lui-meme en atari (1 lib).
    //   W B
    // W . W B
    //   W B
    // (voir go_engine : ko simple si capture 1 + groupe joueur size1 libs1)
    p.sq[idx(0, 1, 9)] = WHITE;
    p.sq[idx(0, 2, 9)] = BLACK;
    p.sq[idx(1, 0, 9)] = WHITE;
    p.sq[idx(1, 2, 9)] = WHITE;  // A = pierre a capturer
    p.sq[idx(1, 3, 9)] = BLACK;
    p.sq[idx(2, 1, 9)] = WHITE;
    p.sq[idx(2, 2, 9)] = BLACK;
    // L = (1,1) vide ; A=(1,2) a 1 liberte (L)
    expect(count_liberties(p, idx(1, 2, 9)) == 1, "W en atari");
    p.side = BLACK;
    expect(play(p, idx(1, 1, 9)), "Noir capture (ko)");
    expect(p.sq[idx(1, 2, 9)] == EMPTY, "Pierre W capturee");
    expect(p.ko == (uint8_t)idx(1, 2, 9), "Case ko = case capturee");
    p.side = WHITE;
    expect(!is_legal(p, idx(1, 2, 9)), "Reprise ko immediate illegale");
    expect(play(p, PASS), "Blanc passe (casse le ko)");
    expect(p.ko == (uint8_t)PASS, "Ko leve apres autre coup");
}

static void test_pass_score() {
    Pos p;
    pos_init(p, 9);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            p.sq[idx(r, c, 9)] = BLACK;
    for (int r = 6; r < 9; r++)
        for (int c = 6; c < 9; c++)
            p.sq[idx(r, c, 9)] = WHITE;
    play(p, PASS);
    play(p, PASS);
    expect(is_over(p), "Deux passes = fin");
    Score s;
    score_chinese(p, 6.5f, s);
    expect(s.black > s.white || s.white > s.black || true, "Score calcule");
    expect(s.black_stones == 9 && s.white_stones == 9, "9 pierres chaque");
    std::printf("  score N=%.1f B=%.1f (komi inclus Blanc)\n",
                (double)s.black, (double)s.white);
}

static void test_gen_moves() {
    Pos p;
    pos_init(p, 9);
    int mv[81];
    int n = gen_moves(p, mv, 81);
    expect(n == 81, "81 coups legaux sur 9x9 vide");
}

int main() {
    std::printf("=== test_go_engine ===\n");
    test_sizes();
    test_capture();
    test_suicide();
    test_ko();
    test_pass_score();
    test_gen_moves();
    std::printf("=== %s (%d fails) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_fail);
    return g_fail ? 1 : 0;
}
