import re

logic_path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-api-logic.yaml'
with open(logic_path, 'r', encoding='utf-8') as f:
    content = f.read()

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
                else { l1_text = "\U0000F015"; l1_color = 0xA3A8B5; } // Default cloudy

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
                            // Set opacity of layer 2 a bit lower if needed, but white is fine
                        } else {
                            lv_obj_add_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                }
            };
'''

# 1. Update tab5_maj_meteo_actuelle
actuelle_old = r'''            std::string state = condition;
            auto icon = id(img_cloudy);
            if (state == "clear-night") icon = id(img_night);
            else if (state == "cloudy") icon = id(img_cloudy);
            else if (state == "fog") icon = id(img_fog);
            else if (state == "hail") icon = id(img_snowy_rainy);
            else if (state == "lightning") icon = id(img_thunder);
            else if (state == "lightning-rainy") icon = id(img_lightning_rainy);
            else if (state == "Clear" || state == "sunny") icon = id(img_day);
            else if (state == "pouring") icon = id(img_rainy_7);
            else if (state == "partlycloudy") icon = id(img_cloudy_day_3);
            else if (state == "rainy") icon = id(img_rainy_5);
            else if (state == "snowy") icon = id(img_snowy_6);
            else if (state == "windy") icon = id(img_windy);
            else if (state == "snowy-rainy") icon = id(img_snowy_rainy);
            lv_img_set_src(id(icon_actuel), icon->get_lv_img_dsc());'''

content = content.replace(actuelle_old, helper_lambda + '\n            apply_weather_icon(id(icon_actuel), id(icon_actuel_layer1), id(icon_actuel_layer2), condition, false);')

# 2. Update tab5_maj_previsions_jours
# We first need to add l1 and l2 to the long if/else assignments
for i in range(15):
    content = content.replace(
        f"else if(jour == {i}) {{ day_label = id(j{i}_day); max_lbl = id(j{i}_max); min_lbl = id(j{i}_min); icon_img = id(j{i}_icon); }}",
        f"else if(jour == {i}) {{ day_label = id(j{i}_day); max_lbl = id(j{i}_max); min_lbl = id(j{i}_min); icon_img = id(j{i}_icon); l1 = id(j{i}_icon_layer1); l2 = id(j{i}_icon_layer2); }}"
    )
content = content.replace("lv_obj_t* icon_img = NULL;", "lv_obj_t* icon_img = NULL;\n              lv_obj_t* l1 = NULL;\n              lv_obj_t* l2 = NULL;")
content = content.replace("if(jour == 0) { day_label = id(j0_day); max_lbl = id(j0_max); min_lbl = id(j0_min); icon_img = id(j0_icon); }", "if(jour == 0) { day_label = id(j0_day); max_lbl = id(j0_max); min_lbl = id(j0_min); icon_img = id(j0_icon); l1 = id(j0_icon_layer1); l2 = id(j0_icon_layer2); }")

jours_old = r'''            std::string state = condition;
            auto icon = id(img_card_cloudy);
            if (state == "clear-night") icon = id(img_card_night);
            else if (state == "cloudy") icon = id(img_card_cloudy);
            else if (state == "fog") icon = id(img_card_fog);
            else if (state == "hail") icon = id(img_card_snowy_rainy);
            else if (state == "lightning") icon = id(img_card_thunder);
            else if (state == "lightning-rainy") icon = id(img_card_lightning_rainy);
            else if (state == "Clear" || state == "sunny") icon = id(img_card_day);
            else if (state == "pouring") icon = id(img_card_rainy_7);
            else if (state == "partlycloudy") icon = id(img_card_cloudy_day_3);
            else if (state == "rainy") icon = id(img_card_rainy_5);
            else if (state == "snowy") icon = id(img_card_snowy_6);
            else if (state == "windy") icon = id(img_card_windy);
            else if (state == "snowy-rainy") icon = id(img_card_snowy_rainy);'''

content = content.replace(jours_old, helper_lambda + "\n            std::string state = condition;")
content = content.replace("lv_img_set_src(icon_img, icon->get_lv_img_dsc());", "apply_weather_icon(icon_img, l1, l2, state, true);")

# 3. Update tab5_maj_previsions_heures
for i in range(15):
    content = content.replace(
        f"else if(heure_index == {i}) {{ time_lbl = id(h{i}_time); icon_img = id(h{i}_icon); p_icon = id(h{i}_p_icon); prob_lbl = id(h{i}_prob); temp_lbl = id(h{i}_temp); }}",
        f"else if(heure_index == {i}) {{ time_lbl = id(h{i}_time); icon_img = id(h{i}_icon); p_icon = id(h{i}_p_icon); prob_lbl = id(h{i}_prob); temp_lbl = id(h{i}_temp); l1 = id(h{i}_icon_layer1); l2 = id(h{i}_icon_layer2); }}"
    )
content = content.replace("lv_obj_t* temp_lbl = NULL;", "lv_obj_t* temp_lbl = NULL;\n              lv_obj_t* l1 = NULL;\n              lv_obj_t* l2 = NULL;")
content = content.replace("if(heure_index == 0) { time_lbl = id(h0_time); icon_img = id(h0_icon); p_icon = id(h0_p_icon); prob_lbl = id(h0_prob); temp_lbl = id(h0_temp); }", "if(heure_index == 0) { time_lbl = id(h0_time); icon_img = id(h0_icon); p_icon = id(h0_p_icon); prob_lbl = id(h0_prob); temp_lbl = id(h0_temp); l1 = id(h0_icon_layer1); l2 = id(h0_icon_layer2); }")

heures_old = r'''                 auto icon = id(img_card_cloudy);
                 std::string state = condition;
                 if (state == "clear-night") icon = id(img_card_night);
                 else if (state == "cloudy") icon = id(img_card_cloudy);
                 else if (state == "fog") icon = id(img_card_fog);
                 else if (state == "hail") icon = id(img_card_snowy_rainy);
                 else if (state == "lightning") icon = id(img_card_thunder);
                 else if (state == "lightning-rainy") icon = id(img_card_lightning_rainy);
                 else if (state == "Clear" || state == "sunny") icon = id(img_card_day);
                 else if (state == "pouring") icon = id(img_card_rainy_7);
                 else if (state == "partlycloudy") icon = id(img_card_cloudy_day_3);
                 else if (state == "rainy") icon = id(img_card_rainy_5);
                 else if (state == "snowy") icon = id(img_card_snowy_6);
                 else if (state == "windy") icon = id(img_card_windy);
                 else if (state == "snowy-rainy") icon = id(img_card_snowy_rainy);
                 
                 lv_img_set_src(icon_img, icon->get_lv_img_dsc());'''

content = content.replace(heures_old, helper_lambda + "\n                 std::string state = condition;\n                 apply_weather_icon(icon_img, l1, l2, state, true);")

with open(logic_path, 'w', encoding='utf-8') as f:
    f.write(content)
print('Updated tab5-api-logic.yaml')
