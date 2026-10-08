/**
 * [AI-CONTEXT]
 * @file tab5_champs.cpp
 * @role Implémentation de tab5_champs.h : logique pure, compilée à l'identique dans le
 *       firmware et dans les tests hôte (tools/test_tab5_socle.cpp).
 *       Reprend les copies qu'elle remplace (audit du 07/10/2026, lot L5) ; les cas
 *       limites où elles différaient sont réglés une fois ici (voir tab5_champs.h).
 */
#include "tab5_champs.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

Champ champ_suivant(const char*& p, const char* fin, char sep) {
    Champ c{p, 0};
    while (p < fin && *p != sep) p++;
    c.n = static_cast<size_t>(p - c.p);
    if (p < fin) p++;
    return c;
}

namespace {

int decouper(const char* s, size_t n, char sep, Champ* out, int max, bool reste) {
    int k = 0;
    size_t debut = 0;
    for (size_t i = 0; i <= n && k < max; i++) {
        if (i == n || (s[i] == sep && (!reste || k < max - 1))) {
            out[k++] = {s + debut, i - debut};
            debut = i + 1;
        }
    }
    return k;
}

}  // namespace

int champs_decouper(const char* s, size_t n, char sep, Champ* out, int max) {
    return decouper(s, n, sep, out, max, false);
}

int champs_decouper_reste(const char* s, size_t n, char sep, Champ* out, int max) {
    return decouper(s, n, sep, out, max, true);
}

bool champ_est(const Champ& c, const char* mot) {
    const size_t l = std::strlen(mot);
    return c.n == l && std::memcmp(c.p, mot, l) == 0;
}

float champ_nombre(const char* p, size_t n, float defaut) {
    char tmp[kChampNombreMax + 1];
    if (p == nullptr || n == 0 || n > kChampNombreMax) return defaut;
    std::memcpy(tmp, p, n);
    tmp[n] = '\0';
    char* bout = nullptr;
    const float v = std::strtof(tmp, &bout);
    return (bout == tmp || !std::isfinite(v)) ? defaut : v;
}

uint32_t champ_entier(const char* p, size_t n, uint32_t max, uint32_t defaut) {
    char tmp[16];
    if (p == nullptr || n == 0 || n >= sizeof(tmp)) return defaut;
    std::memcpy(tmp, p, n);
    tmp[n] = '\0';
    char* bout = nullptr;
    const unsigned long v = std::strtoul(tmp, &bout, 10);
    return (bout == tmp || v > max) ? defaut : static_cast<uint32_t>(v);
}
