/**
 * Branche essai/mesure-demarrage (16/10/2026) : JAMAIS MERGÉE.
 * Chronomètre chaque bande de priorité du setup() de la tablette virtuelle et, sous
 * callgrind, vide un profil par bande (CALLGRIND_DUMP_STATS_AT remet les compteurs à zéro :
 * chaque fichier = une bande). Hors valgrind, les macros ne font rien.
 */
#pragma once
#include <chrono>
#include <cstdio>
#include <valgrind/callgrind.h>
#include "lvgl.h"

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

// Essai du correctif (-DMESURE_DRAIN) : LVGL 9.5 abonne chaque label dont le texte est à
// recalculer à LV_EVENT_UPDATE_LAYOUT_COMPLETED de l'affichage (lv_label.c), et chaque
// lv_obj_mark_layout_as_dirty() envoie LV_EVENT_REFR_REQUEST à ce même affichage : la liste
// est parcourue en entier. Tant qu'aucune mise en page n'a lieu (construction, setup()),
// elle grossit d'un label à l'autre. Au-delà de 64 abonnés, on envoie nous-mêmes la fin de
// mise en page : les labels recalculent leur texte et se désabonnent (ce que LVGL fait déjà
// pour les labels des écrans non affichés à chaque mise en page).
inline void drainer_cb(lv_event_t* e) {
    static bool en_cours = false;
    if (en_cours) return;
    auto* disp = static_cast<lv_display_t*>(lv_event_get_user_data(e));
    if (lv_display_get_event_count(disp) < 64) return;
    en_cours = true;
    lv_display_send_event(disp, LV_EVENT_UPDATE_LAYOUT_COMPLETED, nullptr);
    en_cours = false;
}
inline void drainer_installer() {
    lv_display_t* disp = lv_display_get_default();
    std::printf("[mesure] drain %s\n", disp ? "installe" : "SANS AFFICHAGE");
    if (disp) lv_display_add_event_cb(disp, drainer_cb, LV_EVENT_REFR_REQUEST, disp);
}
}  // namespace mesure
