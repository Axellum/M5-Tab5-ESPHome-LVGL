/**
 * [AI-CONTEXT] Le VRAI générateur de coups des dames (Draughts::Engine de
 * Tab5/draughts_game.cpp, jeu « Dames Tab ») contre les perft de référence des dames
 * internationales 10×10 et anglaises 8×8, plus les règles que teste son miroir Python.
 *
 * C'est la RÉFÉRENCE : tools/test_draughts_engine.py n'en est qu'un miroir Python, gardé
 * pour le poste de dev (qui n'a qu'un cross-compilateur RISC-V). Si les deux divergent, ce
 * test fait foi (constat OUT-2 de l'audit du 07/10/2026).
 *
 * Le moteur vit dans le même fichier que l'interface LVGL du jeu : la CI en extrait le
 * bloc pur (tools/hote/extraire_moteur_dames.py), puis le compile sous ASan + UBSan :
 *   python tools/hote/extraire_moteur_dames.py "$RUNNER_TEMP/hote/draughts_moteur_extrait.inc"
 *   g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       -Wall -Wextra -I tools/hote -I "$RUNNER_TEMP/hote" -I Tab5 \
 *       tools/test_draughts_engine.cpp -o test_draughts_engine
 * tests/test_moteurs_hote.py tient les valeurs perft égales à celles du miroir Python.
 *
 * Code de sortie 0 = tout concorde.
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>  // for (int c : {1, 3, 5})

#include "draughts_game.h"
#include "draughts_moteur_extrait.inc"

using namespace Draughts::Engine;

namespace {

int g_echecs = 0;
int g_max_coups = 0;

void attendre(bool ok, const char* msg) {
    std::printf(ok ? "OK   : %s\n" : "ECHEC: %s\n", msg);
    if (!ok) g_echecs++;
}

// Mêmes valeurs que PERFT_INTL et PERFT_ENG dans tools/test_draughts_engine.py.
const uint64_t PERFT_INTL[] = {9, 81, 658, 4265, 27117, 167140, 1049442};
const uint64_t PERFT_ENG[] = {7, 49, 302, 1469, 7361, 36768, 179740, 845931};

uint64_t perft(const Pos& p, int profondeur) {
    if (profondeur == 0) return 1;
    Move coups[MAX_MOVES];
    const int n = gen_moves(p, coups, MAX_MOVES);
    if (n > g_max_coups) g_max_coups = n;
    if (profondeur == 1) return static_cast<uint64_t>(n);
    uint64_t total = 0;
    for (int i = 0; i < n; i++) {
        Pos q = p;
        apply_move(q, coups[i]);
        total += perft(q, profondeur - 1);
    }
    return total;
}

int g_profondeurs = 0;

void perft_suite(const char* nom, Variant v, const uint64_t* refs, int n) {
    Pos p;
    pos_init(p, v);
    for (int d = 1; d <= n; d++) {
        const uint64_t obtenu = perft(p, d);
        const bool ok = obtenu == refs[d - 1];
        std::printf("%-22s perft(%d) = %9llu  attendu %9llu  %s\n", nom, d,
                    static_cast<unsigned long long>(obtenu), static_cast<unsigned long long>(refs[d - 1]),
                    ok ? "OK" : "ECHEC");
        if (!ok) g_echecs++;
        g_profondeurs++;
    }
}

// Miroirs de empty_pos() et put() du test Python.
Pos vide(Variant v, Side s = SIDE_WHITE) {
    Pos p;
    std::memset(&p, 0, sizeof(p));
    p.n = (v == VAR_ENG8) ? 8 : 10;
    p.variant = v;
    p.side = s;
    p.must_from = 255;
    return p;
}

void poser(Pos& p, int r, int c, uint8_t piece) {
    if (!is_dark_sq(r, c)) {
        std::printf("ECHEC: case claire (%d,%d) dans un test\n", r, c);
        g_echecs++;
        return;
    }
    p.sq[r * p.n + c] = piece;
}

int coups(const Pos& p, Move* out) { return gen_moves(p, out, MAX_MOVES); }

void test_borne_max_moves() {
    // gen_moves() s'arrête d'écrire à MAX_MOVES : un perft qui l'atteint serait tronqué.
    attendre(g_max_coups > 0 && g_max_coups < MAX_MOVES, "les perft restent sous MAX_MOVES (96)");
}

void test_prise_majoritaire_internationale() {
    Pos p = vide(VAR_INTL10);
    poser(p, 5, 4, W_MAN);
    poser(p, 4, 3, B_MAN);  // prise simple vers (3,2)
    poser(p, 4, 5, B_MAN);  // prise vers (3,6)…
    poser(p, 2, 7, B_MAN);  // …puis (1,8) : rafle de 2
    Move m[MAX_MOVES];
    const int n = coups(p, m);
    bool toutes_doubles = n > 0;
    for (int i = 0; i < n; i++) toutes_doubles = toutes_doubles && m[i].n_caps == 2;
    attendre(toutes_doubles, "prise majoritaire : seule la rafle de 2 est légale");
    attendre(n > 0 && m[0].to == 1 * 10 + 8, "la rafle finit en (1,8)");
}

void test_prise_libre_en_anglais() {
    Pos p = vide(VAR_ENG8);
    poser(p, 5, 2, W_MAN);
    poser(p, 4, 1, B_MAN);
    poser(p, 4, 3, B_MAN);
    poser(p, 2, 5, B_MAN);
    Move m[MAX_MOVES];
    const int n = coups(p, m);
    int simples = 0, doubles = 0;
    for (int i = 0; i < n; i++) {
        if (m[i].n_caps == 1) simples++;
        if (m[i].n_caps == 2) doubles++;
    }
    attendre(n == 2 && simples == 1 && doubles == 1, "anglaises : prise libre, sans majorité");
}

void test_pion_anglais_ne_prend_pas_en_arriere() {
    Pos p = vide(VAR_ENG8);
    poser(p, 3, 4, W_MAN);
    poser(p, 4, 3, B_MAN);  // derrière le pion blanc
    Move m[MAX_MOVES];
    int n = coups(p, m);
    bool aucune = true;
    for (int i = 0; i < n; i++) aucune = aucune && m[i].n_caps == 0;
    attendre(aucune, "anglaises : un pion ne prend pas en arrière");
    Pos q = vide(VAR_INTL10);
    poser(q, 3, 4, W_MAN);
    poser(q, 4, 3, B_MAN);
    n = coups(q, m);
    bool une = false;
    for (int i = 0; i < n; i++) une = une || m[i].n_caps == 1;
    attendre(une, "internationales : un pion prend en arrière");
}

void test_pas_de_promotion_en_cours_de_rafle() {
    Pos p = vide(VAR_INTL10);
    poser(p, 2, 1, W_MAN);
    poser(p, 1, 2, B_MAN);
    poser(p, 1, 4, B_MAN);
    Move m[MAX_MOVES];
    const int n = coups(p, m);
    attendre(n == 1 && m[0].n_caps == 2 && !m[0].promote, "pas de promotion en cours de rafle");
    attendre(n == 1 && m[0].to == 2 * 10 + 5, "la rafle finit en (2,5)");
}

void test_dame_volante_atterrissages_multiples() {
    Pos p = vide(VAR_INTL10);
    poser(p, 9, 0, W_KING);
    poser(p, 6, 3, B_MAN);
    Move m[MAX_MOVES];
    const int n = coups(p, m);
    // Après la prise, tous les atterrissages libres de la diagonale : (5,4)…(0,9).
    bool attendus[6] = {};
    bool ok = n == 6;
    for (int i = 0; i < n; i++) {
        ok = ok && m[i].n_caps == 1;
        bool trouve = false;
        for (int k = 0; k < 6; k++) {
            if (m[i].to == (5 - k) * 10 + (4 + k)) {
                trouve = !attendus[k];
                attendus[k] = true;
            }
        }
        ok = ok && trouve;
    }
    attendre(ok, "dame volante : six atterrissages après la prise");
}

void test_capturee_bloque_et_ne_se_reprend_pas() {
    Pos p = vide(VAR_INTL10);
    poser(p, 5, 4, W_KING);
    poser(p, 4, 5, B_MAN);
    Move m[MAX_MOVES];
    const int n = coups(p, m);
    bool ok = n > 0;
    for (int i = 0; i < n; i++) ok = ok && m[i].n_caps == 1;
    attendre(ok, "une pièce prise ne se reprend pas dans la même rafle");
}

void test_apply_move_promotion_et_camp() {
    Pos p = vide(VAR_INTL10);
    poser(p, 1, 2, W_MAN);
    Move m[MAX_MOVES];
    const int n = coups(p, m);
    int i = 0;
    while (i < n && !m[i].promote) i++;
    attendre(i < n, "un coup de promotion existe");
    if (i < n) {
        Pos q = p;
        apply_move(q, m[i]);
        attendre(q.sq[m[i].to] == W_KING && q.side == SIDE_BLACK && q.no_progress == 0,
                 "promotion : dame posée, trait aux noirs, compteur à zéro");
    }
}

const Move* coup_depuis(const Move* m, int n, int depuis) {
    for (int i = 0; i < n; i++)
        if (m[i].from == depuis) return &m[i];
    return nullptr;
}

void test_compteur_de_nulle() {
    Pos p = vide(VAR_INTL10);
    poser(p, 6, 1, W_MAN);
    poser(p, 9, 8, W_KING);
    poser(p, 0, 1, B_MAN);
    p.no_progress = 30;
    Move m[MAX_MOVES];
    const int n = coups(p, m);
    const Move* pion = coup_depuis(m, n, 6 * 10 + 1);
    const Move* dame = coup_depuis(m, n, 9 * 10 + 8);
    attendre(pion && dame, "coups du pion et de la dame trouvés");
    if (pion && dame) {
        Pos a = p;
        apply_move(a, *pion);
        attendre(a.no_progress == 0, "un coup de pion remet le compteur de nulle à zéro");
        Pos b = p;
        apply_move(b, *dame);
        attendre(b.no_progress == 31, "un coup de dame sans prise l'avance d'un demi-coup");
    }
    Pos q = vide(VAR_INTL10);
    poser(q, 5, 4, W_KING);
    poser(q, 4, 5, B_MAN);
    q.no_progress = 30;
    const int nq = coups(q, m);
    attendre(nq > 0 && m[0].n_caps == 1, "la dame prend");
    if (nq > 0) {
        apply_move(q, m[0]);
        attendre(q.no_progress == 0, "une prise remet le compteur de nulle à zéro");
    }
}

void test_fins_de_partie_reduites_fmjd() {
    Pos p = vide(VAR_INTL10);
    poser(p, 5, 4, W_KING);
    poser(p, 0, 1, B_KING);
    poser(p, 0, 3, B_KING);
    refresh_endgame(p);
    attendre(p.eg_limit == ENDGAME_PLIES_SMALL, "dame seule contre deux pièces dont une dame : 5 coups");
    poser(p, 0, 5, B_MAN);
    refresh_endgame(p);
    attendre(p.eg_limit == ENDGAME_PLIES_THREE, "contre trois pièces dont une dame : 16 coups");
    poser(p, 0, 7, B_MAN);
    refresh_endgame(p);
    attendre(p.eg_limit == 0, "contre quatre pièces : pas de décompte");
    Pos q = vide(VAR_INTL10);
    poser(q, 5, 4, W_KING);
    for (int c : {1, 3, 5}) poser(q, 0, c, B_MAN);
    refresh_endgame(q);
    attendre(q.eg_limit == 0, "contre trois pions sans dame : pas de décompte");
    Pos e = vide(VAR_ENG8);
    poser(e, 5, 4, W_KING);
    poser(e, 0, 1, B_KING);
    refresh_endgame(e);
    attendre(e.eg_limit == 0, "anglaises : pas de décompte");
}

void test_fins_de_partie_reduites_decompte() {
    Pos p = vide(VAR_INTL10);
    poser(p, 9, 0, W_KING);
    poser(p, 0, 9, B_KING);
    refresh_endgame(p);
    Move m[MAX_MOVES];
    int n = coups(p, m);
    attendre(n > 0, "la dame blanche a un coup");
    if (n > 0) {
        Pos q = p;
        apply_move(q, m[0]);
        attendre(q.eg_limit == ENDGAME_PLIES_SMALL && q.eg_plies == 1, "un coup sans prise avance le décompte");
    }
    Pos r = vide(VAR_INTL10);
    poser(r, 5, 4, W_KING);
    poser(r, 4, 5, B_MAN);
    poser(r, 0, 1, B_KING);
    r.eg_plies = 7;
    n = coups(r, m);
    attendre(n > 0 && m[0].n_caps == 1, "la dame blanche prend");
    if (n > 0) {
        apply_move(r, m[0]);
        attendre(r.eg_limit == ENDGAME_PLIES_SMALL && r.eg_plies == 0, "une prise recalcule le décompte depuis zéro");
    }
}

}  // namespace

int main() {
    std::printf("=== test_draughts_engine (Draughts::Engine de Tab5/draughts_game.cpp) ===\n");
    perft_suite("internationales 10x10", VAR_INTL10, PERFT_INTL, sizeof(PERFT_INTL) / sizeof(PERFT_INTL[0]));
    perft_suite("anglaises 8x8", VAR_ENG8, PERFT_ENG, sizeof(PERFT_ENG) / sizeof(PERFT_ENG[0]));
    std::printf("max coups légaux vus : %d (borne MAX_MOVES = %d)\n", g_max_coups, MAX_MOVES);
    test_borne_max_moves();
    test_prise_majoritaire_internationale();
    test_prise_libre_en_anglais();
    test_pion_anglais_ne_prend_pas_en_arriere();
    test_pas_de_promotion_en_cours_de_rafle();
    test_dame_volante_atterrissages_multiples();
    test_capturee_bloque_et_ne_se_reprend_pas();
    test_apply_move_promotion_et_camp();
    test_compteur_de_nulle();
    test_fins_de_partie_reduites_fmjd();
    test_fins_de_partie_reduites_decompte();
    std::printf("=== %s : %d profondeurs perft, %d écart(s) ===\n",
                g_echecs ? "GENERATEUR FAUX" : "Générateur VALIDE", g_profondeurs, g_echecs);
    return g_echecs ? 1 : 0;
}
