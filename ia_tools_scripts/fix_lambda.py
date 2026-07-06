import sys
import re
path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

# Replace the entire lambda block with the new scaling/translation logic
pattern = r'            auto apply_weather_icon = \[\]\(.*?\}\n                \}\n            \};\n'

new_lambda = '''            auto apply_weather_icon = [](lv_obj_t* img_obj, lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card) {
                bool is_png = false;
                auto icon_png = is_card ? id(img_card_lightning_rainy) : id(img_lightning_rainy);
                std::string l1_text = "";
                uint32_t l1_color = 0xFFFFFF;
                std::string l2_text = "";
                uint32_t l2_color = 0xFFFFFF;
                int l1_x = 0;
                int l1_y = 0;
                int l2_x = 0;
                int l2_y = 0;
                bool l1_small = false;

                if (state == "clear-night") { l1_text = "\U0000F00B"; l1_color = 0x8AB4FF; }
                else if (state == "cloudy") { l1_text = "\U0000F015"; l1_color = 0xA3A8B5; }
                else if (state == "fog") { l1_text = "\U0000F00E"; l1_color = 0x6A7B92; }
                else if (state == "Clear" || state == "sunny") { l1_text = "\U0000F00F"; l1_color = 0xFFD700; }
                else if (state == "partlycloudy") {
                    l1_text = "\U0000F00F"; l1_color = 0xFFD700;
                    l2_text = "\U0000F015"; l2_color = 0xFFFFFF; 
                    l1_small = true;
                    // Déplacement du soleil (l1) plus petit, en haut à gauche
                    l1_x = is_card ? -25 : -40;
                    l1_y = is_card ? -10 : -20;
                    // Déplacement du nuage (l2) un peu vers la droite/bas
                    l2_x = is_card ? 8 : 15;
                    l2_y = is_card ? 8 : 15;
                }
                else if (state == "hail") { icon_png = is_card ? id(img_card_snowy_rainy) : id(img_snowy_rainy); is_png = true; }
                else if (state == "lightning") { icon_png = is_card ? id(img_card_thunder) : id(img_thunder); is_png = true; }
                else if (state == "lightning-rainy") { icon_png = is_card ? id(img_card_lightning_rainy) : id(img_lightning_rainy); is_png = true; }
                else if (state == "pouring") { icon_png = is_card ? id(img_card_rainy_7) : id(img_rainy_7); is_png = true; }
                else if (state == "rainy") { icon_png = is_card ? id(img_card_rainy_5) : id(img_rainy_5); is_png = true; }
                else if (state == "snowy") { icon_png = is_card ? id(img_card_snowy_6) : id(img_snowy_6); is_png = true; }
                else if (state == "windy") { icon_png = is_card ? id(img_card_windy) : id(img_windy); is_png = true; }
                else if (state == "snowy-rainy") { icon_png = is_card ? id(img_card_snowy_rainy) : id(img_snowy_rainy); is_png = true; }
                else { l1_text = "\U0000F015"; l1_color = 0xA3A8B5; } 

                if (is_png) {
                    lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
                    if(l1_obj) lv_obj_add_flag(l1_obj, LV_OBJ_FLAG_HIDDEN);
                    if(l2_obj) lv_obj_add_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
                    lv_img_set_src(img_obj, icon_png->get_lv_img_dsc());
                } else {
                    lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
                    if(l1_obj) {
                        lv_obj_clear_flag(l1_obj, LV_OBJ_FLAG_HIDDEN);
                        lv_label_set_text(l1_obj, l1_text.c_str());
                        lv_obj_set_style_text_color(l1_obj, lv_color_hex(l1_color), LV_PART_MAIN);
                        
                        if (l1_small) {
                             lv_obj_set_style_text_font(l1_obj, is_card ? font_meteo_card_small : font_meteo_main_small, LV_PART_MAIN);
                        } else {
                             lv_obj_set_style_text_font(l1_obj, is_card ? font_meteo_card : font_meteo_main, LV_PART_MAIN);
                        }
                        lv_obj_set_style_translate_x(l1_obj, l1_x, LV_PART_MAIN);
                        lv_obj_set_style_translate_y(l1_obj, l1_y, LV_PART_MAIN);
                    }
                    if(l2_obj) {
                        if (l2_text != "") {
                            lv_obj_clear_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
                            lv_label_set_text(l2_obj, l2_text.c_str());
                            lv_obj_set_style_text_color(l2_obj, lv_color_hex(l2_color), LV_PART_MAIN);
                            lv_obj_set_style_translate_x(l2_obj, l2_x, LV_PART_MAIN);
                            lv_obj_set_style_translate_y(l2_obj, l2_y, LV_PART_MAIN);
                        } else {
                            lv_obj_add_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                }
            };\n'''

text = re.sub(pattern, new_lambda, text, flags=re.DOTALL)
with open(path, 'w', encoding='utf-8') as f:
    f.write(text)
