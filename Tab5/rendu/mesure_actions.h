/**
 * Branche essai/mesure-invalidation (17/10/2026) : JAMAIS MERGÉE.
 * Mesure, sur la tablette virtuelle, des « fenêtres » d'actions courantes (repos, glisser,
 * poussée de HA, popups, curseur) ouvertes et fermées par le pilote
 * (tools/mesure/piloter_actions.py) à travers un fichier de commande : le pilote y écrit
 * « zero NOM » ou « dump NOM », un minuteur LVGL (20 ms) le lit, l'efface (accusé) et agit.
 *   zero : instrumentation callgrind allumée, compteurs remis à zéro ;
 *   dump : une ligne « [mesure] fenetre NOM … » (temps CPU du processus, images dessinées,
 *          temps de dessin, zones invalidées, objets LVGL, objets avec flou ou ombre portée)
 *          puis CALLGRIND_DUMP_STATS_AT(NOM) et instrumentation coupée.
 * Hors valgrind, les macros callgrind ne font rien : les mêmes fenêtres donnent les
 * temps natifs. Chemin du fichier : variable d'environnement MESURE_CMD.
 */
#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
#include <valgrind/callgrind.h>

#include "lvgl.h"
#include "lvgl_private.h"

namespace mesure {
using horloge = std::chrono::steady_clock;

struct Fenetre {
    bool active = false;
    char nom[96] = {0};
    double cpu0 = 0;
    horloge::time_point t0;
    horloge::time_point t_image;
    uint32_t images = 0;
    double dessin_ms = 0;
    double max_ms = 0;
    uint64_t px = 0;
    uint32_t invalidations = 0;
};
inline Fenetre& fen() {
    static Fenetre f;
    return f;
}

inline double cpu_ms() {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    return (r.ru_utime.tv_sec + r.ru_stime.tv_sec) * 1000.0 + (r.ru_utime.tv_usec + r.ru_stime.tv_usec) / 1000.0;
}

// Objets de l'affichage (écrans + calques) ; `flous` : ceux dont un style, dans n'importe
// quel état, pose LV_STYLE_BLUR_RADIUS ou LV_STYLE_DROP_SHADOW_OPA (les deux propriétés que
// blur_walk_cb de LVGL 9.5 cherche). Zéro = le parcours de blur_walk_cb ne sert à rien.
inline void compter(lv_obj_t* o, uint32_t& n, uint32_t& flous, uint32_t& caches) {
    n++;
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) caches++;
    for (uint32_t i = 0; i < o->style_cnt; i++) {
        lv_style_value_t v;
        const lv_style_t* s = o->styles[i].style;
        if (lv_style_get_prop(s, LV_STYLE_BLUR_RADIUS, &v) == LV_STYLE_RES_FOUND ||
            lv_style_get_prop(s, LV_STYLE_DROP_SHADOW_OPA, &v) == LV_STYLE_RES_FOUND) {
            flous++;
            break;
        }
    }
    const uint32_t c = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < c; i++) compter(lv_obj_get_child(o, i), n, flous, caches);
}

inline void debut_image(lv_event_t* e) {
    auto* d = static_cast<lv_display_t*>(lv_event_get_user_data(e));
    Fenetre& f = fen();
    if (!f.active) return;
    for (uint32_t k = 0; k < d->inv_p; k++) {
        if (d->inv_area_joined[k]) continue;
        f.px += lv_area_get_size(&d->inv_areas[k]);
    }
    f.t_image = horloge::now();
}

inline void fin_image(lv_event_t*) {
    Fenetre& f = fen();
    if (!f.active) return;
    const double ms = std::chrono::duration<double, std::milli>(horloge::now() - f.t_image).count();
    f.images++;
    f.dessin_ms += ms;
    if (ms > f.max_ms) f.max_ms = ms;
}

inline void invalidation(lv_event_t*) {
    Fenetre& f = fen();
    if (f.active) f.invalidations++;
}

inline void commande(lv_timer_t*) {
    static const char* chemin = std::getenv("MESURE_CMD");
    if (chemin == nullptr) return;
    FILE* fh = std::fopen(chemin, "r");
    if (fh == nullptr) return;
    char ligne[128] = {0};
    const bool lu = std::fgets(ligne, sizeof ligne, fh) != nullptr;
    std::fclose(fh);
    std::remove(chemin);
    if (!lu) return;
    ligne[std::strcspn(ligne, "\r\n")] = 0;
    Fenetre& f = fen();
    if (std::strncmp(ligne, "zero ", 5) == 0) {
        std::snprintf(f.nom, sizeof f.nom, "%s", ligne + 5);
        CALLGRIND_START_INSTRUMENTATION;
        CALLGRIND_ZERO_STATS;
        f.active = true;
        f.images = 0;
        f.dessin_ms = 0;
        f.max_ms = 0;
        f.px = 0;
        f.invalidations = 0;
        f.cpu0 = cpu_ms();
        f.t0 = horloge::now();
    } else if (std::strncmp(ligne, "dump ", 5) == 0 && f.active) {
        CALLGRIND_DUMP_STATS_AT(f.nom);
        CALLGRIND_STOP_INSTRUMENTATION;
        f.active = false;
        const double cpu = cpu_ms() - f.cpu0;
        const double duree = std::chrono::duration<double, std::milli>(horloge::now() - f.t0).count();
        uint32_t n = 0, flous = 0, caches = 0;
        lv_display_t* d = lv_display_get_default();
        for (uint32_t i = 0; i < d->screen_cnt; i++) compter(d->screens[i], n, flous, caches);
        if (d->top_layer) compter(d->top_layer, n, flous, caches);
        if (d->sys_layer) compter(d->sys_layer, n, flous, caches);
        if (d->bottom_layer) compter(d->bottom_layer, n, flous, caches);
        std::printf(
            "[mesure] fenetre %s cpu_ms %.3f duree_ms %.1f images %u dessin_ms %.3f max_ms %.3f px %llu "
            "invalidations %u objets %u caches %u flous %u\n",
            f.nom, cpu, duree, (unsigned) f.images, f.dessin_ms, f.max_ms, (unsigned long long) f.px,
            (unsigned) f.invalidations, (unsigned) n, (unsigned) caches, (unsigned) flous);
        std::fflush(stdout);
    }
}

inline void brancher() {
    static bool fait = false;
    if (fait) return;
    fait = true;
    lv_display_t* d = lv_display_get_default();
    lv_display_add_event_cb(d, debut_image, LV_EVENT_RENDER_START, d);
    lv_display_add_event_cb(d, fin_image, LV_EVENT_RENDER_READY, d);
    lv_display_add_event_cb(d, invalidation, LV_EVENT_INVALIDATE_AREA, d);
    lv_timer_create(commande, 20, nullptr);
    std::printf("[mesure] branchee\n");
    std::fflush(stdout);
}

}  // namespace mesure
