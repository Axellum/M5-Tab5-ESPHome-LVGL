#include "tab5_custom.h"
#include "lvgl.h" // Needed for lv_label_set_text etc
#include "esphome/components/lvgl/lvgl_esphome.h"


// we can declare references to ESPHome LVGL objects but we can't easily include main.h 
std::string cal_jour_nom[15] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
std::string cal_heures[15] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
bool cal_toggled[15] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}; 
// Actually ESPHome compiles everything. We can just use id() but id() is ESPHome macro.
// Better way: write the toggle inside the ESPHome lambda!

void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card, esphome::font::Font* f_main, esphome::font::Font* f_card, esphome::font::Font* f_main_s, esphome::font::Font* f_card_s) {
    std::string l1_text = "\xEF\x80\x95"; // Nuage par defaut
    uint32_t l1_color = 0xFFFFFF;
    std::string l2_text = "";
    uint32_t l2_color = 0xFFFFFF;
    int l2_x = 0; int l2_y = 0; int l1_y = 0;
    bool l2_small = false; bool l2_behind = false;

    // Dictionnaire type Classe CSS avec position de base (Grosse icone)
    if (state == "clear-night") { l1_text = "\xEF\x80\x8B"; l1_color = 0xFFD700; }
    else if (state == "cloudy") { l1_text = "\xEF\x80\x95"; }
    else if (state == "fog") { l1_text = "\xEF\x80\x8E"; }
    else if (state == "Clear" || state == "sunny") { l1_text = "\xEF\x80\x8F"; l1_color = 0xFFD700; }
    else if (state == "partlycloudy" || state == "partlycloudy-night" || state == "partlycloudy_night") {
        l1_text = "\xEF\x80\x95"; 
        l2_text = (state == "partlycloudy") ? "\xEF\x80\x8F" : "\xEF\x80\x8B";
        l2_small = true; l2_color = 0xFFD700; l2_behind = true;
        l2_x = -45; l2_y = -45;
    }
    else if (state == "hail" || state == "snowy-rainy") { l2_text = "\xEF\x80\x81"; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "lightning" || state == "thunder" || state == "lightning-rainy") { l2_text = "\xEF\x80\x87"; l2_color = 0xFF6600; l2_behind = true; l1_y = -30; }
    else if (state == "pouring") { l2_text = "\xEF\x80\x85"; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "rainy") { l2_text = "\xEF\x80\x86"; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "snowy") { l2_text = "\xEF\x80\x82"; l2_color = 0x8AB4FF; l2_behind = true; l1_y = -30; }
    else if (state == "windy" || state == "windy-variant") { l1_text = "\xEF\x80\x80"; }

    // Systeme de Ratio automatique pour avoir une justesse pixel perfect !
    // Grosse icone = 270px, Petite = 150px. Ratio exact = 150/270 = 0.5555...
    float ratio = is_card ? 0.5555f : 1.0f;
    l2_x = (int)(l2_x * ratio);
    l2_y = (int)(l2_y * ratio);
    l1_y = (int)(l1_y * ratio);

    if (l1_obj) {
        lv_obj_clear_flag(l1_obj, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(l1_obj, l1_text.c_str());
        lv_obj_set_style_text_color(l1_obj, lv_color_hex(l1_color), LV_PART_MAIN);
        lv_obj_set_style_translate_y(l1_obj, l1_y, LV_PART_MAIN);
        esphome::lvgl::lv_obj_set_style_text_font(l1_obj, is_card ? f_card : f_main, LV_PART_MAIN);
        if (l2_behind && l2_obj) { lv_obj_move_foreground(l1_obj); }
    }
    if (l2_obj) {
        if (l2_text != "") {
            lv_obj_clear_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(l2_obj, l2_text.c_str());
            lv_obj_set_style_text_color(l2_obj, lv_color_hex(l2_color), LV_PART_MAIN);
            lv_obj_set_style_translate_x(l2_obj, l2_x, LV_PART_MAIN);
            lv_obj_set_style_translate_y(l2_obj, l2_y, LV_PART_MAIN);
            esphome::lvgl::lv_obj_set_style_text_font(l2_obj, is_card ? (l2_small ? f_card_s : f_card) : (l2_small ? f_main_s : f_main), LV_PART_MAIN);
        } else {
            lv_obj_add_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

uint32_t get_humidity_color(float x) {
    if (isnan(x)) return 0x404552;
    int val = (int)x;
    if (val <= 14) return 0xFF0000;
    if (val >= 80) return 0x0000CC;
    if (val >= 30) {
        float step = floor((val - 30) / 3.0) * 3.0;
        float ratio = step / 50.0;
        int r = 255 - (255 * ratio);
        int g = 255 - (255 * ratio);
        int b = 255 - ((255 - 204) * ratio);
        return (r << 16) | (g << 8) | b;
    }
    if (val >= 22) {
        float ratio = (val - 22) / 8.0;
        int b = 255 * ratio;
        return (255 << 16) | (255 << 8) | b;
    }
    float ratio = (val - 14) / 8.0;
    int g = 255 * ratio;
    return (255 << 16) | (g << 8) | 0;
}

uint32_t get_temperature_color(float t) {
    if (isnan(t)) return 0xA3A8B5;
    if (t <= -12) return 0xFF0000;
    if (t <= 0) {
        float r = floor((t + 12) / 2.0) * 2.0 / 12.0;
        return (255 << 16) | (0 << 8) | (int)(255 * r);
    }
    if (t <= 14) {
        float r = floor(t / 2.0) * 2.0 / 14.0;
        return ((int)(255 * r) << 16) | ((int)(255 * r) << 8) | 255;
    }
    if (t <= 24) {
        float r = floor((t - 14) / 2.0) * 2.0 / 10.0;
        return (255 << 16) | ((int)(255 - (255 * r)) << 8) | 255;
    }
    float s = floor((t - 24) / 2.0) * 2.0;
    if (s > 11) s = 11;
    return (255 << 16) | (0 << 8) | (int)(255 - (255 * (s / 11.0)));
}
