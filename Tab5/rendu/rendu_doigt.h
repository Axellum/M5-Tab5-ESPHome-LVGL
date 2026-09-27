/**
 * [AI-CONTEXT]
 * @file Tab5/rendu/rendu_doigt.h
 * @role Rendu hors tablette SEULEMENT (tab5-rendu-host.yaml) : un doigt virtuel.
 *       Un périphérique d'entrée LVGL de type pointeur, posé et levé par les actions
 *       rendu_toucher / rendu_glisser (Tab5/rendu/bouchons.yaml), que
 *       tools/rendu/capturer.py appelle pour ouvrir chaque écran comme on le ferait
 *       sur la dalle : mêmes gestionnaires (on_click, on_long_press, gestes), même
 *       recherche du widget touché par LVGL. Jamais dans le firmware de la tablette
 *       (tests/test_rendu_host.py).
 *
 * @architecture_constraint Coordonnées LOGIQUES, celles des captures PNG (paysage
 *       1280×720, portrait 720×1280 pour Neon Apron) : ESPHome donne à LVGL la
 *       résolution déjà tournée et ne tourne que les points de SON écran tactile
 *       (LVTouchListener) ; ce pointeur-ci n'en a pas besoin.
 */
#pragma once
#include "esphome.h"

struct RenduDoigt {
    lv_indev_t* indev = nullptr;
    bool appuye = false;
    int32_t x = 0;
    int32_t y = 0;
};

inline RenduDoigt& rendu_doigt() {
    static RenduDoigt d;
    return d;
}

// Lu par LVGL à chaque période de son minuteur d'entrée (~33 ms).
inline void rendu_doigt_lire(lv_indev_t*, lv_indev_data_t* data) {
    const RenduDoigt& d = rendu_doigt();
    data->point.x = d.x;
    data->point.y = d.y;
    data->state = d.appuye ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Pose (ou déplace) le doigt. Le pointeur est créé au premier appui : LVGL est prêt
// depuis longtemps quand la première action arrive par l'API.
inline void rendu_doigt_poser(int32_t x, int32_t y) {
    RenduDoigt& d = rendu_doigt();
    if (d.indev == nullptr) {
        d.indev = lv_indev_create();
        lv_indev_set_type(d.indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(d.indev, rendu_doigt_lire);
    }
    d.x = x;
    d.y = y;
    d.appuye = true;
}

inline void rendu_doigt_lever() { rendu_doigt().appuye = false; }
