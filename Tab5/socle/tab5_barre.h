/**
 * [AI-CONTEXT]
 * @file tab5_barre.h
 * @role Barre de chargement du démarrage (16/10/2026, demande d'Axel), partie pure : la
 *       pilule dessinée dans un tampon RGB565 à l'endroit (bords lissés), sa recopie tournée
 *       vers la dalle, la progression affichée. L'écran, la tâche et le rétroéclairage sont
 *       dans Tab5/ecran/tab5_barre_demarrage.cpp.
 *       Pourquoi : LVGL ne dessine sa première image qu'après setup() (21,5 s après la mise
 *       sous tension sur le Tab5 le 10/10/2026, « Tab5 Chronologie du démarrage ») ; la dalle,
 *       elle, est prête vers 8,7 s. Entre les deux, un écran noir qui fait croire à une panne.
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier se compile sur PC
 *       (tools/test_tab5_socle.cpp, g++ en CI).
 * @ai_instruction La rotation suit LvglComponent::draw_buffer_() d'ESPHome 2026.9 (rotation
 *       logicielle de LVGL, `lvgl: rotation:`) : un point (u, v) de l'interface va en
 *       (v, H − 1 − u) à 270°, (L − 1 − u, H − 1 − v) à 180°, (L − 1 − v, u) à 90°, L × H
 *       étant la dalle. test_barre_demarrage() le vérifie point par point.
 */
#pragma once
#include <cmath>
#include <cstdint>

namespace barre {

// Couleur de la palette (0xRRGGBB) → RGB565, l'ordre des pixels de la dalle.
constexpr uint16_t rgb565(uint32_t c) {
    return static_cast<uint16_t>(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

// Mélange de deux couleurs 0xRRGGBB, a = part de `c` (0 à 1).
inline uint32_t melanger(uint32_t fond, uint32_t c, float a) {
    if (a <= 0.0f) return fond;
    if (a >= 1.0f) return c;
    uint32_t r = 0;
    for (int dec = 0; dec <= 16; dec += 8) {
        const float f = static_cast<float>((fond >> dec) & 0xFF);
        const float v = static_cast<float>((c >> dec) & 0xFF);
        r |= static_cast<uint32_t>(std::lround(f + (v - f) * a)) << dec;
    }
    return r;
}

// Rectangle en pixels (interface ou dalle).
struct Rect {
    int x, y, l, h;
};

// Place de la barre dans l'interface (paysage, l'écran tel que LVGL le voit) : 40 % de la
// largeur, 12 px de haut à 720 (proportionnel, pair), centrée.
constexpr Rect geometrie(int ecran_l, int ecran_h) {
    const int h = (ecran_h * 12 / 720) / 2 * 2 < 8 ? 8 : (ecran_h * 12 / 720) / 2 * 2;
    const int l = ecran_l * 2 / 5;
    return {(ecran_l - l) / 2, (ecran_h - h) / 2, l, h};
}

// Part (0 à 1) de la pilule [x0, x1) × [0, h) qui couvre le pixel (c, r) : bords lissés sur
// un pixel. Une pilule plus courte que sa hauteur reste un disque de diamètre h.
inline float couverture(int c, int r, float x0, float x1, float h) {
    const float rayon = h * 0.5f;
    if (x1 - x0 < h) x1 = x0 + h;
    const float px = static_cast<float>(c) + 0.5f;
    const float py = static_cast<float>(r) + 0.5f;
    const float cx = std::fmin(std::fmax(px, x0 + rayon), x1 - rayon);
    const float dx = px - cx;
    const float dy = py - rayon;
    const float d = std::sqrt(dx * dx + dy * dy);
    return std::fmin(std::fmax(rayon + 0.5f - d, 0.0f), 1.0f);
}

// Pixels remplis pour une fraction (0 à 1) d'une barre de l px.
inline int remplis(float fraction, int l) {
    if (!(fraction > 0.0f)) return 0;
    if (fraction >= 1.0f) return l;
    return static_cast<int>(std::lround(fraction * static_cast<float>(l)));
}

// La barre à l'endroit (l × h, ligne par ligne) : piste sur le fond, partie pleine sur la
// piste, `plein_px` pixels remplis depuis la gauche.
inline void peindre(uint16_t* buf, int l, int h, int plein_px, uint32_t fond, uint32_t piste, uint32_t plein) {
    const float fh = static_cast<float>(h);
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < l; c++) {
            uint32_t col = melanger(fond, piste, couverture(c, r, 0.0f, static_cast<float>(l), fh));
            if (plein_px > 0) col = melanger(col, plein, couverture(c, r, 0.0f, static_cast<float>(plein_px), fh));
            buf[r * l + c] = rgb565(col);
        }
    }
}

// Rectangle de la dalle (dalle_l × dalle_h) qui reçoit le rectangle `ui` de l'interface.
constexpr Rect rect_dalle(Rect ui, int rotation, int dalle_l, int dalle_h) {
    return rotation == 90    ? Rect{dalle_l - ui.y - ui.h, ui.x, ui.h, ui.l}
           : rotation == 180 ? Rect{dalle_l - ui.x - ui.l, dalle_h - ui.y - ui.h, ui.l, ui.h}
           : rotation == 270 ? Rect{ui.y, dalle_h - ui.x - ui.l, ui.h, ui.l}
                             : ui;
}

// Recopie `src` (l × h, à l'endroit) dans `dst`, rangé comme le rectangle de rect_dalle().
inline void tourner(const uint16_t* src, uint16_t* dst, int l, int h, int rotation) {
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < l; c++) {
            const uint16_t p = src[r * l + c];
            if (rotation == 90) dst[c * h + (h - 1 - r)] = p;
            else if (rotation == 180) dst[(h - 1 - r) * l + (l - 1 - c)] = p;
            else if (rotation == 270) dst[(l - 1 - c) * h + r] = p;
            else dst[r * l + c] = p;
        }
    }
}

// Progression affichée : chaque étape du démarrage part de la fraction déjà montrée et
// approche la sienne sans la dépasser (1 − e^(−2t/d), 86 % du chemin au bout de sa durée
// typique d), puis la marque suivante la relance. La barre bouge tant que setup() avance,
// sans promettre une fin qu'on ne connaît pas.
inline float progression(float depart, float cible, uint32_t ecoule_ms, uint32_t duree_ms) {
    if (duree_ms == 0 || cible <= depart) return cible > depart ? cible : depart;
    const float k = 1.0f - std::exp(-2.0f * static_cast<float>(ecoule_ms) / static_cast<float>(duree_ms));
    return depart + (cible - depart) * k;
}

// Étapes de la barre, aux marques de la chronologie (on_boot de tab5-ha-hmi.yaml) : où elle
// va et la durée typique de l'étape (Tab5, ST7123, 10/10/2026 : dalle prête à 8,7 s, setup()
// de LVGL fini à 15,1 s, Wi-Fi initialisé à 21,3 s, première image à 21,5 s).
struct Jalon {
    float cible;
    uint32_t duree_ms;
};
constexpr Jalon kJalons[] = {
    {0.50f, 6400},  // dalle prête → setup() de LVGL fini (on_boot 500 → 399)
    {0.97f, 6300},  // → Wi-Fi initialisé (399 → 249)
    {1.00f, 200},   // → fin de setup() (249 → -100), la première image suit
};
constexpr int kEtapes = static_cast<int>(sizeof(kJalons) / sizeof(kJalons[0]));

}  // namespace barre
