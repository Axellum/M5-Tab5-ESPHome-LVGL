// Build de MESURE seulement (campagne du 07/10/2026), jamais commité.
// Surface invalidée de l'image qui commence (LV_EVENT_RENDER_START : inv_areas encore
// valides, lv_refr.c refr_invalid_areas), en pixels, zones fusionnées exclues.
#pragma once
#include "lvgl.h"
#include "lvgl_private.h"

inline uint32_t diag_inv_px() {
    lv_display_t* d = lv_display_get_default();
    if (d == nullptr) return 0;
    uint32_t px = 0;
    for (uint32_t i = 0; i < d->inv_p; i++)
        if (!d->inv_area_joined[i]) px += lv_area_get_size(&d->inv_areas[i]);
    return px;
}

inline uint32_t diag_inv_n() {
    lv_display_t* d = lv_display_get_default();
    return d != nullptr ? d->inv_p : 0;
}

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"

// Nombre d'objets LVGL de tous les écrans (récursif).
inline uint32_t diag_compter(lv_obj_t* o) {
    uint32_t n = 1;
    const uint32_t c = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < c; i++) n += diag_compter(lv_obj_get_child(o, i));
    return n;
}
inline uint32_t diag_compter_objets() {
    uint32_t n = 0;
    lv_display_t* d = lv_display_get_default();
    if (d == nullptr) return 0;
    for (uint32_t i = 0; i < d->screen_cnt; i++) n += diag_compter(d->screens[i]);
    if (d->top_layer) n += diag_compter(d->top_layer);
    if (d->sys_layer) n += diag_compter(d->sys_layer);
    return n;
}

// Étapes de la bascule de thème : temps écoulé depuis l'étape précédente.
inline int64_t& diag_t_etape() {
    static int64_t t = 0;
    return t;
}
inline void diag_etape(const char* nom) {
    const int64_t now = esp_timer_get_time();
    if (nom[0] == '0') {
        ESP_LOGI("mesure", "theme debut, %u objets LVGL", (unsigned) diag_compter_objets());
    } else {
        ESP_LOGI("mesure", "theme etape %s %lld us", nom, (long long) (now - diag_t_etape()));
    }
    diag_t_etape() = esp_timer_get_time();
}

inline void diag_memoire() {
    ESP_LOGI("mesure", "memoire interne libre %u min %u bloc %u | psram libre %u total %u min %u bloc %u",
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned) heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
             (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
             (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}
