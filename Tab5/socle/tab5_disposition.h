/**
 * [AI-CONTEXT]
 * @file tab5_disposition.h
 * @role Disposition des éléments d'une pièce sur les cinq places de la bande du bas
 *       (ADR-0066, lot B, 18/10/2026) : jusqu'à cinq éléments, des tuiles entières ; de six à
 *       vingt, les cinq places, chacune entière (un élément), coupée en deux (haut / bas, deux)
 *       ou en quatre (quatre), avec le moins de quarts, puis le moins de moitiés (la table de
 *       l'ADR). Places de gauche à droite : les entières, puis les moitiés, puis les quarts ;
 *       les éléments les remplissent dans l'ordre de la liste (une moitié : haut puis bas ; un
 *       quart : haut gauche, haut droite, bas gauche, bas droite). À 19, le dernier quart reste
 *       vide. Pour chaque élément : sa place et sa case ; pour chaque case : son rectangle.
 * @architecture_constraint Pure (ni ESPHome ni LVGL), testée sur PC par tools/test_tab5_socle.cpp
 *       (n = 0 … 20, pièces à trous, positions fixes, rectangles des cases). L'index T d'un
 *       élément (son rang dans la liste du blueprint) reste son identité partout (états, clé
 *       de l'événement, popups, roue) : la disposition n'est que son dessin. Le dessin des
 *       cases est dans Tab5/ecran/tab5_tuiles_cases.cpp, les gestes dans tab5_tuiles.cpp.
 * @ai_instruction Positions fixes (le mode météo jusqu'à cinq éléments, tous d'index < 5) :
 *       l'élément T sur la place T, trous compris, comme avant l'ADR-0066 (une pièce aux tuiles
 *       espacées, le mode héritage et son volet en tuile 1). Sinon les éléments présents se
 *       suivent sans trou, et le mode HA centre les `utilisees` premières places.
 */
#pragma once
#include <cstdint>

namespace disposition {

constexpr int kPlaces = 5;        // places de la bande du bas (les cinq tuiles de l'accueil)
constexpr int kElementsMax = 20;  // éléments d'une pièce au plus (kTuilesMax)
constexpr int kCasesMax = 4;      // cases d'une place coupée en quatre

// Ce qu'est une place : vide, une tuile entière, deux moitiés (haut / bas), quatre quarts.
enum class Coupe : uint8_t {
    VIDE,
    ENTIERE,
    MOITIES,
    QUARTS,
};

constexpr int cases_de(Coupe c) {
    return c == Coupe::QUARTS ? 4 : (c == Coupe::MOITIES ? 2 : (c == Coupe::ENTIERE ? 1 : 0));
}

// Places entières, coupées en deux et coupées en quatre pour `n` éléments (table de
// l'ADR-0066). n ≤ 5 : n entières ; 6 ≤ n ≤ 20 : les cinq places, le moins de quarts puis le
// moins de moitiés qui donnent au moins n cases (à q quarts et h moitiés, les cinq places ont
// 5 + h + 3q cases). Hors de 0 … 20 : borné.
struct Compte {
    int entieres = 0;
    int moities = 0;
    int quarts = 0;
};

constexpr Compte compter(int n) {
    if (n <= 0) return {};
    if (n > kElementsMax) n = kElementsMax;
    if (n <= kPlaces) return {n, 0, 0};
    for (int q = 0; q <= kPlaces; q++) {
        int h = n - kPlaces - 3 * q;
        if (h < 0) h = 0;
        if (h + q <= kPlaces) return {kPlaces - q - h, h, q};
    }
    return {0, 0, kPlaces};
}

struct Place {
    Coupe coupe = Coupe::VIDE;
    int8_t element[kCasesMax] = {-1, -1, -1, -1};  // index T de l'élément de chaque case ; -1 : vide
};

struct Disposition {
    int n = 0;                     // éléments posés
    int utilisees = 0;             // places occupées : les `utilisees` premières (centrées en mode HA)
    bool positions_fixes = false;  // l'élément T sur la place T
    Place place[kPlaces];
    int8_t place_de[kElementsMax];  // élément T → sa place ; -1 : absent
    int8_t case_de[kElementsMax];   // élément T → sa case dans la place
};

// Bit T de `presents` : l'élément T existe (T < kElementsMax ; les bits au-delà sont ignorés).
// `positions_fixes` : le mode météo, qui garde l'élément T sur la place T tant que la pièce a
// au plus cinq éléments, tous d'index < 5.
inline Disposition disposer(uint32_t presents, bool positions_fixes) {
    Disposition d;
    for (int t = 0; t < kElementsMax; t++) d.place_de[t] = d.case_de[t] = -1;
    presents &= (1u << kElementsMax) - 1u;
    int8_t liste[kElementsMax] = {};
    int n = 0;
    for (int t = 0; t < kElementsMax; t++)
        if (presents & (1u << t)) liste[n++] = static_cast<int8_t>(t);
    d.n = n;
    if (positions_fixes && n <= kPlaces && (presents >> kPlaces) == 0) {
        d.positions_fixes = true;
        for (int k = 0; k < n; k++) {
            const int t = liste[k];
            d.place[t].coupe = Coupe::ENTIERE;
            d.place[t].element[0] = static_cast<int8_t>(t);
            d.place_de[t] = static_cast<int8_t>(t);
            d.case_de[t] = 0;
            d.utilisees = t + 1;
        }
        return d;
    }
    const Compte c = compter(n);
    const Coupe ordre[3] = {Coupe::ENTIERE, Coupe::MOITIES, Coupe::QUARTS};
    const int nombre[3] = {c.entieres, c.moities, c.quarts};
    int p = 0, k = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < nombre[i] && p < kPlaces; j++, p++) {
            Place& pl = d.place[p];
            pl.coupe = ordre[i];
            for (int cs = 0; cs < cases_de(ordre[i]) && k < n; cs++, k++) {
                pl.element[cs] = liste[k];
                d.place_de[liste[k]] = static_cast<int8_t>(p);
                d.case_de[liste[k]] = static_cast<int8_t>(cs);
            }
        }
    }
    d.utilisees = p;
    return d;
}

// Rectangle d'une case dans sa place (origine : le coin haut gauche de la place).
struct Rect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
};

// La case `k` d'une place `coupe` de `l` × `h` px, au moins `ecart` px entre deux cases :
// une entière couvre la place ; deux moitiés font toute la largeur, en haut et en bas ;
// quatre quarts, deux colonnes de deux. Cases égales, collées aux bords de la place : le pixel
// de reste d'une division va à l'écart du milieu.
constexpr Rect case_rect(Coupe coupe, int k, int32_t l, int32_t h, int32_t ecart) {
    if (coupe != Coupe::MOITIES && coupe != Coupe::QUARTS) return {0, 0, l, h};
    const int32_t hc = (h - ecart) / 2;
    const int32_t y = (k >> 1) != 0 || (coupe == Coupe::MOITIES && k == 1) ? h - hc : 0;
    if (coupe == Coupe::MOITIES) return {0, y, l, hc};
    const int32_t lc = (l - ecart) / 2;
    return {(k & 1) != 0 ? l - lc : 0, y, lc, hc};
}

}  // namespace disposition
