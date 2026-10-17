/**
 * [AI-CONTEXT]
 * @file tab5_decoupe.h
 * @role Découpe d'une zone à redessiner autour d'une carte opaque (17/10/2026), partie pure de
 *       popup_voile_carte() (Tab5/ecran/tab5_anim.cpp) : la part d'une carte arrondie que son
 *       fond couvre à coup sûr (couverte()), et une zone coupée entre cette part et le reste
 *       (couper()). Pourquoi : le popup Musique a un voile à 70 % (ADR-0050), le détail du jour
 *       un voile à 60 % sur le calendrier ; pour LVGL un voile translucide ne couvre rien, et
 *       une zone d'un seul tenant qui déborde de la carte se dessine depuis ce qu'il laisse voir
 *       (tableau de bord, calendrier). Coupée, la part couverte se dessine à partir de la carte
 *       seule.
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier se compile sur PC
 *       (tools/test_tab5_socle.cpp, g++ en CI). Zones à bornes comprises (x2, y2 inclus), comme
 *       lv_area_t ; tab5_anim.cpp passe de l'une à l'autre.
 */
#pragma once
#include <algorithm>
#include <cstdint>

namespace decoupe {

// Zone de l'écran, bornes comprises. Vide quand x1 > x2 ou y1 > y2 (la valeur par défaut).
struct Zone {
    int32_t x1 = 0;
    int32_t y1 = 0;
    int32_t x2 = -1;
    int32_t y2 = -1;
    bool vide() const { return x1 > x2 || y1 > y2; }
    bool operator==(const Zone& o) const { return x1 == o.x1 && y1 == o.y1 && x2 == o.x2 && y2 == o.y2; }
};

// Part d'une carte (rectangle aux coins de rayon `rayon`) que son fond couvre à coup sûr : sans
// les coins arrondis (le rayon plus une ligne, en haut et en bas) ni la colonne de chaque côté
// (sous une bordure opaque, LVGL rentre le fond d'un pixel, lv_draw_rect), alignée sur
// `arrondi` (draw_rounding d'ESPHome, que son rappel rounder_cb impose à toute zone) : début
// multiple de `arrondi`, fin + 1 multiple de `arrondi`. Vide pour une carte trop petite ou qui
// sort de l'écran par la gauche ou le haut.
inline Zone couverte(const Zone& carte, int32_t rayon, int32_t arrondi) {
    const int32_t n = arrondi > 0 ? arrondi : 1;
    if (carte.vide() || carte.x1 < 0 || carte.y1 < 0) return {};
    const int32_t demi = std::min(carte.x2 - carte.x1 + 1, carte.y2 - carte.y1 + 1) / 2;
    const int32_t r = std::max<int32_t>(0, std::min(rayon, demi));
    Zone c;
    c.x1 = (carte.x1 + 1 + n - 1) / n * n;
    c.y1 = (carte.y1 + r + 1 + n - 1) / n * n;
    c.x2 = carte.x2 / n * n - 1;
    c.y2 = (carte.y2 - r) / n * n - 1;
    return c.vide() ? Zone{} : c;
}

// Coupe `a` autour de `c` : `dans` = a ∩ c, `morceaux` = le reste, au plus quatre (au-dessus et
// au-dessous sur toute la largeur de `a`, à gauche et à droite à la hauteur de `dans`), deux à
// deux disjoints et disjoints de `dans`, leur union avec `dans` étant `a`. Rend le nombre de
// morceaux, ou -1 quand il n'y a rien à couper (`a` hors de `c`, ou déjà dedans).
inline int couper(const Zone& a, const Zone& c, Zone& dans, Zone morceaux[4]) {
    if (a.vide() || c.vide()) return -1;
    dans = {std::max(a.x1, c.x1), std::max(a.y1, c.y1), std::min(a.x2, c.x2), std::min(a.y2, c.y2)};
    if (dans.vide() || dans == a) return -1;
    const Zone reste[4] = {
        {a.x1, a.y1, a.x2, dans.y1 - 1},
        {a.x1, dans.y2 + 1, a.x2, a.y2},
        {a.x1, dans.y1, dans.x1 - 1, dans.y2},
        {dans.x2 + 1, dans.y1, a.x2, dans.y2},
    };
    int n = 0;
    for (const Zone& z : reste) {
        if (!z.vide()) morceaux[n++] = z;
    }
    return n;
}

}  // namespace decoupe
