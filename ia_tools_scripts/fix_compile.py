import sys
path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

# Remove redeclaration at 535
text = text.replace('lv_obj_t* l1 = NULL;\n              lv_obj_t* l2 = NULL;', '', 1) # Only removes the first match, but they are exactly identical. Wait, let's just do it explicitly.

lines = text.split('\n')
for i, line in enumerate(lines):
    if 'lv_obj_t* l1 = NULL;' in line and i > 530 and i < 540:
        lines[i] = ""
    if 'lv_obj_t* l2 = NULL;' in line and i > 530 and i < 540:
        lines[i] = ""

text = '\n'.join(lines)

helper_lambda = '''            auto apply_weather_icon = [](lv_obj_t* img_obj, lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card) {
                bool is_png = false;
                auto icon_png = is_card ? id(img_card_lightning_rainy) : id(img_lightning_rainy);
                std::string l1_text = "";
                uint32_t l1_color = 0xFFFFFF;
                std::string l2_text = "";
                uint32_t l2_color = 0xFFFFFF;
                int l2_x = 10;
                int l2_y = 10;

                if (state == "clear-night") { l1_text = "\U0000F00B"; l1_color = 0x8AB4FF; }
                else if (state == "cloudy") { l1_text = "\U0000F015"; l1_color = 0xA3A8B5; }
                else if (state == "fog") { l1_text = "\U0000F00E"; l1_color = 0x6A7B92; }
                else if (state == "Clear" || state == "sunny") { l1_text = "\U0000F00F"; l1_color = 0xFFD700; }
                else if (state == "partlycloudy") {
                    l1_text = "\U0000F00F"; l1_color = 0xFFD700;
                    l2_text = "\U0000F015"; l2_color = 0xFFFFFF; 
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
            };\n\n'''

text = text.replace('std::string state = condition;\n                 apply_weather_icon(icon_img, l1, l2, state, true);', helper_lambda + 'std::string state = condition;\n                 apply_weather_icon(icon_img, l1, l2, state, true);')

with open(path, 'w', encoding='utf-8') as f:
    f.write(text)

