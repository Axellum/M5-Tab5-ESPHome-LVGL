/**
 * [AI-CONTEXT] Le VRAI générateur d'échecs (Tab5/jeux/chess_ai.cpp, jeu « Roi Noir ») contre la
 * suite perft standard, puis quelques recherches qui exercent la table de transposition,
 * le découpage en tranches et la libération de la mémoire.
 *
 * C'est la RÉFÉRENCE : tools/test_chess_perft.py n'en est qu'un miroir Python, gardé pour
 * le poste de dev (qui n'a qu'un cross-compilateur RISC-V). Si les deux divergent, ce test
 * fait foi. Repris de tools/audit/moteur_echecs.cpp (branche audit/relances, audit du
 * 30/09/2026 ; constat OUT-2 de l'audit du 07/10/2026 : le miroir ne suivait plus le C++).
 *
 * La CI (job `python` d'esphome-tab5.yml) le compile avec g++ sous ASan + UBSan et
 * l'exécute à chaque PR :
 *   g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       -Wall -Wextra -I tools/hote -I Tab5/rendu/hote -I Tab5/socle -I Tab5/jeux \
 *       tools/test_chess_engine.cpp Tab5/jeux/chess_ai.cpp Tab5/socle/tab5_i18n.cpp -o test_chess_engine
 * tools/hote/esphome.h remplace l'en-tête d'ESPHome, Tab5/rendu/hote/esp_heap_caps.h
 * celui d'ESP-IDF. tests/test_moteurs_hote.py tient SUITE égale à celle du miroir Python.
 *
 * Code de sortie 0 = tout concorde.
 */
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

// Mêmes positions et valeurs que SUITE dans tools/test_chess_perft.py.
const Cas SUITE[] = {
    {"Initiale", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {20, 400, 8902, 197281}},
    {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", {48, 2039, 97862}},
    {"Position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {14, 191, 2812, 43238}},
    {"Position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {6, 264, 9467}},
    {"Position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379}},
};

}  // namespace

int main() {
    std::printf("=== test_chess_engine (Tab5/jeux/chess_ai.cpp) ===\n");
    int echecs = 0;
    int positions = 0;
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
            positions++;
        }
    }
    // Le selftest embarqué dans le firmware, à la profondeur 5 (4 865 609 nœuds).
    if (!Chess::perft_selftest(5)) {
        std::printf("ECHEC perft_selftest(5)\n");
        echecs++;
    }

    // Recherches : table de transposition, tranches, libération.
    for (const Cas& c : SUITE) {
        Chess::Position p;
        Chess::set_fen(p, c.fen);
        int score = 0;
        const Chess::Move m = Chess::search_quick(p, 4, 3000, &score);
        char uci[8] = {0};
        Chess::move_to_uci(m, uci, sizeof(uci));
        std::printf("%-12s search_quick(4) = %s (score %d)\n", c.nom, uci, score);
        if (m.from == m.to) {  // coup nul : move_to_uci écrirait « a1a1 »
            std::printf("ECHEC %s : search_quick n'a rendu aucun coup\n", c.nom);
            echecs++;
        }
    }
    Chess::Position depart;
    Chess::set_start(depart);
    Chess::search_start(depart, 3, 12345u);
    int tranches = 0;
    while (Chess::search_active() && tranches < 100000) {
        Chess::search_step();
        tranches++;
    }
    const Chess::Move meilleur = Chess::search_best();
    char uci[8] = {0};
    Chess::move_to_uci(meilleur, uci, sizeof(uci));
    std::printf("search_start(niveau 3) : %s après %d tranches, profondeur %d\n", uci, tranches,
                Chess::search_depth_done());
    if (Chess::search_active() || meilleur.from == meilleur.to) {
        std::printf("ECHEC search_start : recherche pas finie ou sans coup\n");
        echecs++;
    }
    Chess::search_release();

    std::printf("=== %s : %d profondeurs perft, %d écart(s) ===\n",
                echecs ? "GENERATEUR FAUX" : "Générateur VALIDE", positions, echecs);
    return echecs ? 1 : 0;
}
