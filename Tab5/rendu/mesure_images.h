/**
 * Branche essai/mesure-popup-musique (17/10/2026) : JAMAIS MERGÉE.
 * Chronomètre chaque image de la tablette virtuelle (LV_EVENT_RENDER_START → RENDER_READY :
 * le dessin et l'envoi, comme « mesure image » de la campagne de nuit sur la 7B) et, sous
 * callgrind, vide un profil par grande image (CALLGRIND_ZERO_STATS au début,
 * CALLGRIND_DUMP_STATS_AT à la fin : chaque fichier = une image). Hors valgrind, les macros
 * ne font rien. Ligne écrite : « [mesure] image N us U px P zones Z ecran E ».
 */
#pragma once
#include <chrono>
#include <cstdio>
#include <valgrind/callgrind.h>

#include "lvgl.h"
#include "lvgl_private.h"
#include "tab5_registry.h"

namespace mesure {

struct Image {
    std::chrono::steady_clock::time_point t0;
    uint32_t px = 0;
    uint32_t zones = 0;
    int numero = 0;
};
inline Image& image() {
    static Image i;
    return i;
}

inline void debut(lv_event_t* e) {
    auto* d = static_cast<lv_display_t*>(lv_event_get_user_data(e));
    Image& i = image();
    i.px = 0;
    i.zones = 0;
    for (uint32_t k = 0; k < d->inv_p; k++) {
        if (d->inv_area_joined[k]) continue;
        i.px += lv_area_get_size(&d->inv_areas[k]);
        i.zones++;
    }
    CALLGRIND_ZERO_STATS;
    i.t0 = std::chrono::steady_clock::now();
}

inline void fin(lv_event_t*) {
    Image& i = image();
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - i.t0).count();
    if (i.px < 100000) return;
    const char* ecran = ModalRegistry::visible_name();
    char nom[96];
    std::snprintf(nom, sizeof nom, "image_%03d_%s", ++i.numero, ecran != nullptr ? ecran : "Accueil");
    for (char* c = nom; *c; c++) {
        if (*c == ' ') *c = '_';
    }
    std::printf("[mesure] image %d us %.0f px %u zones %u ecran %s\n", i.numero, us, (unsigned) i.px,
                (unsigned) i.zones, ecran != nullptr ? ecran : "Accueil");
    std::fflush(stdout);
    CALLGRIND_DUMP_STATS_AT(nom);
}

inline void brancher() {
    static bool fait = false;
    if (fait) return;
    fait = true;
    lv_display_t* d = lv_display_get_default();
    lv_display_add_event_cb(d, debut, LV_EVENT_RENDER_START, d);
    lv_display_add_event_cb(d, fin, LV_EVENT_RENDER_READY, d);
    std::printf("[mesure] branchée\n");
    std::fflush(stdout);
}

}  // namespace mesure
