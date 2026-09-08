/**
 * [AI-CONTEXT]
 * @file tab5_console.cpp
 * @role Console système : ligne status (uptime / Wi-Fi / temp CPU), synchro volume et
 *       icônes mute, diagnostics mémoire/réseau.
 *       Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Éviter std::string dans les boucles de parsing ; char* + strtok_r.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// =============================================================================
// Console diagnostic — ligne status (uptime / Wi-Fi / temp CPU), garde #T222
// =============================================================================

bool is_console_layer_visible(lv_obj_t* layer_console) {
    return layer_console != nullptr && !lv_obj_has_flag(layer_console, LV_OBJ_FLAG_HIDDEN);
}

void update_console_uptime_label(lv_obj_t* label, float uptime_s) {
    if (label == nullptr) return;
    int total = (int)uptime_s;
    int days = total / 86400;
    int hours = (total % 86400) / 3600;
    int mins = (total % 3600) / 60;
    char buf[32];
    if (days > 0) {
        snprintf(buf, sizeof(buf), "%dj %02dh%02d", days, hours, mins);
    } else {
        snprintf(buf, sizeof(buf), "%02dh%02d", hours, mins);
    }
    lv_label_set_text(label, buf);
}

void update_console_rssi_label(lv_obj_t* label, float rssi_dbm) {
    if (label == nullptr) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f dBm", rssi_dbm);
    lv_label_set_text(label, buf);
}

void update_console_temp_label(lv_obj_t* label, float core_temp_c) {
    if (label == nullptr) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f \xC2\xB0", core_temp_c);
    lv_label_set_text(label, buf);
}

void refresh_console_status_row_ui(lv_obj_t* lbl_uptime, lv_obj_t* lbl_rssi, lv_obj_t* lbl_temp,
    bool has_uptime, float uptime_s, bool has_rssi, float rssi_dbm, bool has_temp, float core_temp_c) {
    if (has_uptime) update_console_uptime_label(lbl_uptime, uptime_s);
    if (has_rssi) update_console_rssi_label(lbl_rssi, rssi_dbm);
    if (has_temp) update_console_temp_label(lbl_temp, core_temp_c);
}

// Volume : un seul endroit repeint les trois affichages (slider console, label %
// de la console, slider du popup assistant). Avant, chaque slider ne peignait que
// le sien et un reglage venu de Home Assistant n'en peignait aucun.
void ui_sync_volume_widgets(lv_obj_t* slider_console, lv_obj_t* lbl_console_pct,
    lv_obj_t* slider_assist, float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    const int pct = (int)(volume * 100.0f + 0.5f);
    if (slider_console != nullptr) lv_slider_set_value(slider_console, pct, LV_ANIM_OFF);
    if (slider_assist != nullptr) lv_slider_set_value(slider_assist, pct, LV_ANIM_OFF);
    if (lbl_console_pct != nullptr) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(lbl_console_pct, buf);
    }
}

// Muet : deux icones peignent le meme `system_muted` (barre du dashboard et
// popup assistant). Un seul endroit les met d'accord.
void ui_sync_mute_icons(lv_obj_t* icon_main, lv_obj_t* icon_assist, bool muted) {
    const char* glyph = muted ? "\U000F0581" : "\U000F057E";
    const uint32_t color = muted ? UIColor::ERROR : UIColor::TEXT_SOFT;
    for (lv_obj_t* icon : {icon_main, icon_assist}) {
        if (icon == nullptr) continue;
        lv_label_set_text(icon, glyph);
        lv_obj_set_style_text_color(icon, lv_color_hex(color), LV_PART_MAIN);
    }
}

// Met a jour les widgets de la console diagnostic (SRAM/PSRAM/frag/loop/IP/SSID).
// Factorise depuis l'interval 2s de tab5-sensors-diagnostics.yaml (Phase 3, #T164). Le garde
// "console visible ?" reste dans le YAML (evite de passer layer_console_sys ici).
void update_console_diagnostics_ui(lv_obj_t* lbl_sram, lv_obj_t* bar_sram,
    lv_obj_t* lbl_psram, lv_obj_t* bar_psram, lv_obj_t* lbl_frag, lv_obj_t* lbl_flash,
    bool loop_time_has_state, float loop_time, lv_obj_t* lbl_loop,
    bool wifi_ip_has_state, const char* wifi_ip, lv_obj_t* lbl_ip,
    bool wifi_ssid_has_state, const char* wifi_ssid, lv_obj_t* lbl_ssid) {

    if (lbl_sram != nullptr) {
        #ifdef USE_ESP_IDF
        float sram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024.0f;
        float sram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024.0f;
        float psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024.0f / 1024.0f;
        float psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024.0f / 1024.0f;
        float frag = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024.0f;
        #else
        float sram_free = ESP.getFreeHeap() / 1024.0f;
        float sram_total = ESP.getHeapSize() / 1024.0f;
        float psram_free = ESP.getFreePsram() / (1024.0f * 1024.0f);
        float psram_total = ESP.getPsramSize() / (1024.0f * 1024.0f);
        float frag = ESP.getMaxAllocHeap() / 1024.0f;
        #endif

        float sram_used = sram_total - sram_free;
        if (sram_used < 0) sram_used = 0;
        float psram_used = psram_total - psram_free;
        if (psram_used < 0) psram_used = 0;

        int sram_pct = (int)((sram_used / sram_total) * 100.0f);
        int psram_pct = (int)((psram_used / psram_total) * 100.0f);

        char b_sram[32]; snprintf(b_sram, sizeof(b_sram), "%d%% (%.1f KB)", sram_pct, sram_used);
        lv_label_set_text(lbl_sram, b_sram);
        lv_bar_set_value(bar_sram, sram_pct, LV_ANIM_ON);

        char b_psram[32]; snprintf(b_psram, sizeof(b_psram), "%d%% (%.2f MB)", psram_pct, psram_used);
        lv_label_set_text(lbl_psram, b_psram);
        lv_bar_set_value(bar_psram, psram_pct, LV_ANIM_ON);

        auto set_bar_color = [](lv_obj_t* bar, int pct) {
            lv_color_t color = lv_color_hex(UIColor::SUCCESS); // Vert (Normal)
            if (pct > 50) color = lv_color_hex(UIColor::INFO); // Bleu (Bien-Rempli)
            if (pct > 75) color = lv_color_hex(UIColor::WARNING); // Orange (Attention)
            if (pct > 90) color = lv_color_hex(UIColor::ERROR); // Rouge (Critique)
            lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
        };
        set_bar_color(bar_sram, sram_pct);
        set_bar_color(bar_psram, psram_pct);

        char b_frag[32]; snprintf(b_frag, sizeof(b_frag), "%.1f KB", frag);
        lv_label_set_text(lbl_frag, b_frag);
        lv_label_set_text(lbl_flash, "16.0 MB");
    }

    if (loop_time_has_state && lbl_loop != nullptr) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f ms", loop_time);
        lv_label_set_text(lbl_loop, buf);
    }

    if (wifi_ip_has_state && lbl_ip != nullptr) {
        lv_label_set_text(lbl_ip, wifi_ip);
    }

    if (wifi_ssid_has_state && lbl_ssid != nullptr) {
        lv_label_set_text(lbl_ssid, wifi_ssid);
    }
}
