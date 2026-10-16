/**
 * Branche essai/mesure-demarrage-avant-apres (16/10/2026) : JAMAIS MERGÉE.
 * Chronomètre chaque bande de priorité du setup() de la tablette virtuelle et, sous
 * callgrind, vide un profil par bande (CALLGRIND_DUMP_STATS_AT remet les compteurs à zéro :
 * chaque fichier = une bande). Hors valgrind, les macros ne font rien.
 */
#pragma once
#include <chrono>
#include <cstdio>
#include <valgrind/callgrind.h>

namespace mesure {
inline std::chrono::steady_clock::time_point& debut() {
    static auto t = std::chrono::steady_clock::now();
    return t;
}
struct Amorce {
    Amorce() { debut(); }
};
inline Amorce amorce;

inline void bande(const char* nom) {
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - debut()).count();
    std::printf("[mesure] %s %.1f ms\n", nom, ms);
    std::fflush(stdout);
    CALLGRIND_DUMP_STATS_AT(nom);
}
}  // namespace mesure
