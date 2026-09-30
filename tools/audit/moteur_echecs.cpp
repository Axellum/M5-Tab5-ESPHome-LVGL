// Audit qualité (item 4) : le VRAI générateur d'échecs (Tab5/chess_ai.cpp) contre la suite
// perft de tools/test_chess_perft.py (son miroir Python), sous ASan/UBSan, puis quelques
// recherches pour exercer la table de transposition et le découpage en tranches.
//
//   g++ -std=c++17 -O1 -g -fsanitize=address,undefined -Wall -Wextra \
//       -I tools/audit/hote -I Tab5/rendu/hote -I Tab5 \
//       tools/audit/moteur_echecs.cpp Tab5/chess_ai.cpp Tab5/tab5_i18n.cpp -o moteur_echecs
//
// Code de sortie 0 = tout concorde.
#include "chess_ai.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

struct Cas {
    const char* nom;
    const char* fen;
    std::vector<uint64_t> refs;
};

// Mêmes positions et valeurs que tools/test_chess_perft.py (SUITE).
const Cas SUITE[] = {
    {"Initiale", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {20, 400, 8902, 197281}},
    {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", {48, 2039, 97862}},
    {"Position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {14, 191, 2812, 43238}},
    {"Position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {6, 264, 9467}},
    {"Position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379}},
};

}  // namespace

int main() {
    int echecs = 0;
    for (const Cas& c : SUITE) {
        Chess::Position p;
        if (!Chess::set_fen(p, c.fen)) {
            std::printf("ECHEC %s : FEN refusée\n", c.nom);
            echecs++;
            continue;
        }
        for (size_t d = 1; d <= c.refs.size(); d++) {
            const uint64_t n = Chess::perft(p, static_cast<int>(d));
            const bool ok = n == c.refs[d - 1];
            std::printf("%-12s perft(%zu) = %9llu  attendu %9llu  %s\n", c.nom, d,
                        static_cast<unsigned long long>(n), static_cast<unsigned long long>(c.refs[d - 1]),
                        ok ? "OK" : "ECHEC");
            echecs += ok ? 0 : 1;
        }
    }
    // Le selftest embarqué dans le firmware, à la profondeur 5 (4 865 609 nœuds).
    if (!Chess::perft_selftest(5)) echecs++;

    // Recherches : table de transposition, tranches, libération.
    for (const Cas& c : SUITE) {
        Chess::Position p;
        Chess::set_fen(p, c.fen);
        int score = 0;
        const Chess::Move m = Chess::search_quick(p, 4, 3000, &score);
        char uci[8] = {0};
        Chess::move_to_uci(m, uci, sizeof(uci));
        std::printf("%-12s search_quick(4) = %s (score %d)\n", c.nom, uci, score);
    }
    Chess::Position depart;
    Chess::set_start(depart);
    Chess::search_start(depart, 3, 12345u);
    int tranches = 0;
    while (Chess::search_active() && tranches < 100000) {
        Chess::search_step();
        tranches++;
    }
    char uci[8] = {0};
    Chess::move_to_uci(Chess::search_best(), uci, sizeof(uci));
    std::printf("search_start(niveau 3) : %s après %d tranches, profondeur %d\n", uci, tranches,
                Chess::search_depth_done());
    Chess::search_release();

    std::printf("%s (%d écart(s))\n", echecs ? "GENERATEUR FAUX" : "Générateur VALIDE", echecs);
    return echecs ? 1 : 0;
}
