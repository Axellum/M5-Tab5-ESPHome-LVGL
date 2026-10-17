/**
 * Branche essai/mesure-bascule-theme (17/10/2026) : JAMAIS MERGÉE.
 * Chronomètre la bascule de thème de la tablette virtuelle : chaque étape du script
 * tab5_theme_repeindre (mesure::etape, comme diag_etape() du build de mesure de la 7B) et
 * chaque module de theme_rejouer_ui() (mesure::module). Sous callgrind (lancé avec
 * --instr-atstart=no), l'instrumentation n'est allumée que pendant la bascule et chaque
 * étape vide un profil (CALLGRIND_DUMP_STATS_AT remet les compteurs à zéro). Hors valgrind,
 * les macros ne font rien.
 * Compte aussi, à chaque étape, les objets LVGL et les abonnés de l'affichage (labels dont le
 * texte attend la prochaine mise en page, lv_label.c : lv_label_mark_need_refr_text).
 */
#pragma once
#include <chrono>
#include <cstdio>
#include <valgrind/callgrind.h>
#include "lvgl.h"
#include "lvgl_private.h"

namespace mesure {
using horloge = std::chrono::steady_clock;

inline horloge::time_point& t_etape() {
    static horloge::time_point t;
    return t;
}
inline horloge::time_point& t_module() {
    static horloge::time_point t;
    return t;
}
inline int& bascule() {
    static int n = 0;
    return n;
}
inline bool& en_cours() {
    static bool b = false;
    return b;
}

inline uint32_t compter(lv_obj_t* o) {
    uint32_t n = 1;
    const uint32_t c = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < c; i++) n += compter(lv_obj_get_child(o, i));
    return n;
}
inline uint32_t objets() {
    lv_display_t* d = lv_display_get_default();
    if (d == nullptr) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < d->screen_cnt; i++) n += compter(d->screens[i]);
    if (d->top_layer) n += compter(d->top_layer);
    if (d->sys_layer) n += compter(d->sys_layer);
    return n;
}
inline uint32_t abonnes() {
    lv_display_t* d = lv_display_get_default();
    return d != nullptr ? lv_display_get_event_count(d) : 0;
}

inline double ms_depuis(horloge::time_point t) {
    return std::chrono::duration<double, std::milli>(horloge::now() - t).count();
}

// Image suivante : le temps entre la fin du script et la fin de la première image dessinée
// (mise en page, vidage des abonnés, dessin de l'écran entier).
inline void fin_image(lv_event_t* e) {
    auto* d = static_cast<lv_display_t*>(lv_event_get_user_data(e));
    lv_display_remove_event_cb_with_user_data(d, fin_image, d);
    char nom[48];
    std::snprintf(nom, sizeof nom, "b%02d_10_image_suivante", bascule());
    std::printf("[mesure] b%02d etape 10_image_suivante %.3f ms abonnes %u\n", bascule(), ms_depuis(t_etape()),
                (unsigned) abonnes());
    std::fflush(stdout);
    CALLGRIND_DUMP_STATS_AT(nom);
    CALLGRIND_STOP_INSTRUMENTATION;
    en_cours() = false;
}

inline void etape(const char* nom) {
    const double ms = ms_depuis(t_etape());
    if (nom[0] == '0') {
        bascule()++;
        en_cours() = true;
        std::printf("[mesure] b%02d debut objets %u abonnes %u\n", bascule(), (unsigned) objets(), (unsigned) abonnes());
        std::fflush(stdout);
        CALLGRIND_START_INSTRUMENTATION;
        CALLGRIND_ZERO_STATS;
    } else {
        char complet[48];
        std::snprintf(complet, sizeof complet, "b%02d_%s", bascule(), nom);
        std::printf("[mesure] b%02d etape %s %.3f ms abonnes %u\n", bascule(), nom, ms, (unsigned) abonnes());
        std::fflush(stdout);
        CALLGRIND_DUMP_STATS_AT(complet);
        if (nom[0] == '9') {
            lv_display_t* d = lv_display_get_default();
            if (d != nullptr) lv_display_add_event_cb(d, fin_image, LV_EVENT_REFR_READY, d);
        }
    }
    t_etape() = horloge::now();
    t_module() = t_etape();
}

// Un module de theme_rejouer_ui() : temps depuis le module précédent (ou le début de l'étape).
inline void module(const char* nom) {
    if (!en_cours()) return;
    std::printf("[mesure] b%02d module %s %.3f ms abonnes %u\n", bascule(), nom, ms_depuis(t_module()),
                (unsigned) abonnes());
    t_module() = horloge::now();
}
}  // namespace mesure
