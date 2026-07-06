import re

path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

bulk_heures_service = '''
    - service: tab5_maj_previsions_heures_bulk
      variables:
        payload: string
      then:
        - lambda: |-
            std::string s = payload;
            size_t pos = 0;
            auto get_c = [](float t) -> uint32_t {
                if (t <= -12) return 0xFF0000;
                else if (t <= 0) { float r = floor((t + 12)/2.0)*2.0/12.0; return (255<<16)|(0<<8)|(int)(255*r); }
                else if (t <= 14) { float r = floor(t/2.0)*2.0/14.0; return ((int)(255*r)<<16)|((int)(255*r)<<8)|255; }
                else if (t <= 24) { float r = floor((t - 14)/2.0)*2.0/10.0; return (255<<16)|((int)(255-(255*r))<<8)|255; }
                else { float x = floor((t - 24)/2.0)*2.0; if(x>11) x=11; return (255<<16)|(0<<8)|(int)(255-(255*(x/11.0))); }
            };
            auto apply_weather_icon = [](lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card) {
                update_meteo_icon(l1_obj, l2_obj, state, is_card, font_meteo_main, font_meteo_card, font_meteo_main_small, font_meteo_card_small);
            };

            lv_obj_t* pt_time[] = {id(h0_time), id(h1_time), id(h2_time), id(h3_time), id(h4_time), id(h5_time), id(h6_time), id(h7_time), id(h8_time), id(h9_time), id(h10_time), id(h11_time), id(h12_time), id(h13_time), id(h14_time)};
            lv_obj_t* pt_temp[] = {id(h0_temp), id(h1_temp), id(h2_temp), id(h3_temp), id(h4_temp), id(h5_temp), id(h6_temp), id(h7_temp), id(h8_temp), id(h9_temp), id(h10_temp), id(h11_temp), id(h12_temp), id(h13_temp), id(h14_temp)};
            lv_obj_t* pt_pop[]  = {id(h0_pop), id(h1_pop), id(h2_pop), id(h3_pop), id(h4_pop), id(h5_pop), id(h6_pop), id(h7_pop), id(h8_pop), id(h9_pop), id(h10_pop), id(h11_pop), id(h12_pop), id(h13_pop), id(h14_pop)};
            lv_obj_t* pt_l1[]   = {id(h0_icon_layer1), id(h1_icon_layer1), id(h2_icon_layer1), id(h3_icon_layer1), id(h4_icon_layer1), id(h5_icon_layer1), id(h6_icon_layer1), id(h7_icon_layer1), id(h8_icon_layer1), id(h9_icon_layer1), id(h10_icon_layer1), id(h11_icon_layer1), id(h12_icon_layer1), id(h13_icon_layer1), id(h14_icon_layer1)};
            lv_obj_t* pt_l2[]   = {id(h0_icon_layer2), id(h1_icon_layer2), id(h2_icon_layer2), id(h3_icon_layer2), id(h4_icon_layer2), id(h5_icon_layer2), id(h6_icon_layer2), id(h7_icon_layer2), id(h8_icon_layer2), id(h9_icon_layer2), id(h10_icon_layer2), id(h11_icon_layer2), id(h12_icon_layer2), id(h13_icon_layer2), id(h14_icon_layer2)};

            while((pos = s.find(";")) != std::string::npos) {
                std::string token = s.substr(0, pos);
                s.erase(0, pos + 1);

                std::vector<std::string> parts;
                size_t ppos = 0;
                while(true) {
                    size_t np = token.find("|", ppos);
                    if(np == std::string::npos) { parts.push_back(token.substr(ppos)); break; }
                    parts.push_back(token.substr(ppos, np - ppos));
                    ppos = np + 1;
                }

                if(parts.size() >= 5) {
                    int heure_index = std::atoi(parts[0].c_str());
                    if(heure_index < 0 || heure_index > 14) continue;
                    std::string heure_texte = parts[1];
                    std::string condition = parts[2];
                    float temp = std::atof(parts[3].c_str());
                    float pluvio = std::atof(parts[4].c_str());

                    lv_obj_t* time_lbl = pt_time[heure_index];
                    lv_obj_t* temp_lbl = pt_temp[heure_index];
                    lv_obj_t* pop_lbl = pt_pop[heure_index];
                    lv_obj_t* l1 = pt_l1[heure_index];
                    lv_obj_t* l2 = pt_l2[heure_index];

                    if(time_lbl) {
                        lv_label_set_text(time_lbl, heure_texte.c_str());
                        uint32_t c_t = get_c(temp);
                        char b_t[32]; sprintf(b_t, "#%06x %.0f#°", c_t, temp);
                        lv_label_set_text(temp_lbl, b_t);
                        
                        char b_p[32];
                        if (pluvio > 0) {
                            sprintf(b_p, "%.1fmm", pluvio);
                            lv_label_set_text(pop_lbl, b_p);
                            lv_obj_set_style_text_color(pop_lbl, lv_color_hex(0x8AB4FF), LV_PART_MAIN);
                        } else {
                            lv_label_set_text(pop_lbl, "-");
                            lv_obj_set_style_text_color(pop_lbl, lv_color_hex(0x4A596E), LV_PART_MAIN);
                        }
                        
                        apply_weather_icon(l1, l2, condition, true);
                    }
                }
            }
'''

if 'tab5_maj_previsions_heures_bulk' not in text:
    text = text.replace('  services:', '  services:\n' + bulk_heures_service)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
