/**
 * Tests host du moteur Go (sans ESPHome / LVGL).
 *
 * Build & run (PowerShell, si un g++ natif est disponible) :
 *   g++ -std=c++17 -O2 -I../Tab5 -o test_go_engine.exe test_go_engine.cpp ../Tab5/go_engine.cpp
 *   .\test_go_engine.exe
 *
 * Le poste de dev n'a qu'un cross-compilateur RISC-V : le miroir Python
 * tools/test_go_engine.py couvre les MÊMES règles et tourne, lui, sans
 * toolchain. Garder les deux en phase.
 */
#include "go_engine.h"
#include <cstdio>

using namespace Go::Engine;

static int g_fail = 0;

static void expect(bool cond, const char* msg) {
    std::printf(cond ? "OK   : %s\n" : "FAIL : %s\n", msg);
    if (!cond) g_fail++;
}

static void test_sizes() {
    for (int n : {9, 13, 19}) {
        Pos p;
        pos_init(p, n);
        expect(p.n == (uint8_t)n && p.side == BLACK, "init taille");
        expect(is_legal(p, idx(n / 2, n / 2, n)), "centre legal");
        int mv[MAX_SQ];
        expect(gen_moves(p, mv, MAX_SQ) == n * n, "tous les points sont legaux sur un goban vide");
    }
}

static void test_capture() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(3, 4, 9)] = BLACK;
    p.sq[idx(5, 4, 9)] = BLACK;
    p.sq[idx(4, 3, 9)] = BLACK;
    p.side = WHITE;
    expect(play(p, idx(4, 4, 9)), "Blanc entre dans le piege");
    p.side = BLACK;
    expect(play(p, idx(4, 5, 9)), "Noir ferme et capture");
    expect(p.sq[idx(4, 4, 9)] == EMPTY, "la pierre blanche est retiree");
    expect(p.captured_by_black == 1, "1 prisonnier au compteur de Noir");
}

static void test_capture_groupe() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(4, 4, 9)] = WHITE;
    p.sq[idx(4, 5, 9)] = WHITE;
    p.sq[idx(3, 4, 9)] = BLACK;
    p.sq[idx(3, 5, 9)] = BLACK;
    p.sq[idx(5, 4, 9)] = BLACK;
    p.sq[idx(5, 5, 9)] = BLACK;
    p.sq[idx(4, 3, 9)] = BLACK;
    p.side = BLACK;
    expect(play(p, idx(4, 6, 9)), "Noir capture un groupe de 2");
    expect(p.sq[idx(4, 4, 9)] == EMPTY && p.sq[idx(4, 5, 9)] == EMPTY,
           "les 2 pierres sont retirees");
    expect(p.captured_by_black == 2, "2 prisonniers");
}

static void test_suicide() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(3, 4, 9)] = WHITE;
    p.sq[idx(5, 4, 9)] = WHITE;
    p.sq[idx(4, 3, 9)] = WHITE;
    p.sq[idx(4, 5, 9)] = WHITE;
    p.side = BLACK;
    expect(!is_legal(p, idx(4, 4, 9)), "suicide simple interdit");
}

// Un coup qui n'a aucune liberte propre reste legal s'il capture d'abord.
static void test_capture_avant_suicide() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(0, 0, 9)] = WHITE;
    p.sq[idx(1, 0, 9)] = BLACK;
    p.sq[idx(1, 1, 9)] = BLACK;
    p.side = BLACK;
    expect(count_liberties(p, idx(0, 0, 9)) == 1, "la pierre blanche est en atari");
    expect(is_legal(p, idx(0, 1, 9)), "capture autorisee meme sans liberte propre");
    expect(play(p, idx(0, 1, 9)), "coup joue");
    expect(p.sq[idx(0, 0, 9)] == EMPTY, "pierre blanche capturee");
}

static void test_ko() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(0, 1, 9)] = WHITE;
    p.sq[idx(0, 2, 9)] = BLACK;
    p.sq[idx(1, 0, 9)] = WHITE;
    p.sq[idx(1, 2, 9)] = WHITE;
    p.sq[idx(1, 3, 9)] = BLACK;
    p.sq[idx(2, 1, 9)] = WHITE;
    p.sq[idx(2, 2, 9)] = BLACK;
    expect(count_liberties(p, idx(1, 2, 9)) == 1, "la pierre blanche est en atari");
    p.side = BLACK;
    expect(play(p, idx(1, 1, 9)), "Noir capture (forme de ko)");
    expect(p.ko == (int16_t) idx(1, 2, 9), "la case du ko est memorisee");
    p.side = WHITE;
    expect(!is_legal(p, idx(1, 2, 9)), "reprise immediate du ko interdite");
    expect(play(p, PASS), "Blanc passe");
    expect(p.ko == (int16_t) PASS, "le ko est leve apres un autre coup");
}

static void test_pass_hors_domaine_19() {
    expect(PASS == -1, "PASS = -1");
    Pos p;
    pos_init(p, 19);
    const int sq255 = 255;
    expect(is_legal(p, sq255), "intersection 255 jouable");
    expect(play(p, sq255), "poser en 255");
    expect(p.sq[sq255] == BLACK && p.passes == 0, "pose reelle, pas une passe");
}

static void test_ko_haut_indice_19() {
    Pos p;
    pos_init(p, 19);
    const int n = 19;
    p.sq[idx(15, 7, n)] = WHITE;
    p.sq[idx(15, 8, n)] = BLACK;
    p.sq[idx(16, 6, n)] = WHITE;
    p.sq[idx(16, 8, n)] = WHITE;
    p.sq[idx(16, 9, n)] = BLACK;
    p.sq[idx(17, 7, n)] = WHITE;
    p.sq[idx(17, 8, n)] = BLACK;
    const int cap_sq = idx(16, 8, n);
    const int play_sq = idx(16, 7, n);
    expect(cap_sq > 255, "case capturee > 255");
    p.side = BLACK;
    expect(play(p, play_sq), "Noir capture ko bas-plateau");
    expect(p.ko == (int16_t) cap_sq, "ko non tronque");
    expect(!is_legal(p, cap_sq), "reprise ko interdite");
}

static void test_eye() {
    Pos p;
    pos_init(p, 9);
    p.sq[idx(3, 4, 9)] = BLACK;
    p.sq[idx(5, 4, 9)] = BLACK;
    p.sq[idx(4, 3, 9)] = BLACK;
    p.sq[idx(4, 5, 9)] = BLACK;
    expect(is_eye(p, idx(4, 4, 9), BLACK), "4 orthogonaux amis, diagonales vides : oeil");
    p.sq[idx(3, 3, 9)] = WHITE;
    p.sq[idx(3, 5, 9)] = WHITE;
    expect(!is_eye(p, idx(4, 4, 9), BLACK), "2 diagonales adverses : plus un oeil");
}

static void test_score_et_morts() {
    Pos p;
    pos_init(p, 9);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 9; c++) p.sq[idx(r, c, 9)] = BLACK;
    for (int r = 5; r < 9; r++)
        for (int c = 0; c < 9; c++) p.sq[idx(r, c, 9)] = WHITE;
    play(p, PASS);
    play(p, PASS);
    expect(is_over(p), "deux passes = fin de partie");

    Score s;
    score_chinese(p, 6.5f, nullptr, s);
    expect(s.black_stones == 36 && s.white_stones == 36, "36 pierres chacun");
    expect(s.dame == 9, "la ligne mediane est neutre");
    expect(s.black == 36.0f && s.white == 42.5f, "aires + komi");

    uint8_t dead[MAX_SQ] = {};
    mark_chain(p, idx(6, 4, 9), dead, 1);
    Score s2;
    score_chinese(p, 6.5f, dead, s2);
    expect(s2.white_dead == 36 && s2.white_stones == 0, "groupe blanc retire");
    expect(s2.black == 81.0f, "Noir prend tout le goban");

    uint8_t terr[MAX_SQ];
    territory_map(p, dead, terr);
    expect(terr[idx(8, 0, 9)] == T_BLACK, "le territoire suit les pierres mortes");
}

static void test_handicap() {
    Pos p;
    pos_init(p, 19);
    expect(place_handicap(p, 9) == 9, "9 pierres de handicap en 19x19");
    expect(p.side == WHITE, "Blanc commence avec handicap");
    expect(p.sq[idx(9, 9, 19)] == BLACK, "le tengen fait partie du handicap 9");
    Pos q;
    pos_init(q, 9);
    expect(place_handicap(q, 0) == 0, "handicap 0 = aucune pierre");
    expect(q.side == BLACK, "sans handicap, Noir commence");
}

int main() {
    std::printf("=== test_go_engine ===\n");
    test_sizes();
    test_capture();
    test_capture_groupe();
    test_suicide();
    test_capture_avant_suicide();
    test_ko();
    test_pass_hors_domaine_19();
    test_ko_haut_indice_19();
    test_eye();
    test_score_et_morts();
    test_handicap();
    std::printf("=== %s (%d fails) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_fail);
    return g_fail ? 1 : 0;
}
