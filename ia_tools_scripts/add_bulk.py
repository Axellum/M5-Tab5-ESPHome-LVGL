import re

path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

bulk_service = '''
    - service: tab5_maj_previsions_jours_bulk
      variables:
        payload: string
      then:
        - lambda: |-
            std::string s = payload;
            size_t pos = 0;
            
            // Re-utilisation des lambdas de couleurs
            auto apply_weather_icon = [](lv_obj_t* l1_obj, lv_obj_t* l2_obj, std::string state, bool is_card) {
                update_meteo_icon(l1_obj, l2_obj, state, is_card, font_meteo_main, font_meteo_card, font_meteo_main_small, font_meteo_card_small);
            };
            auto get_c = [](float t) -> uint32_t {
                if (t <= -12) return 0xFF0000;
                else if (t <= 0) { float r = floor((t + 12)/2.0)*2.0/12.0; return (255<<16)|(0<<8)|(int)(255*r); }
                else if (t <= 14) { float r = floor(t/2.0)*2.0/14.0; return ((int)(255*r)<<16)|((int)(255*r)<<8)|255; }
                else if (t <= 24) { float r = floor((t - 14)/2.0)*2.0/10.0; return (255<<16)|((int)(255-(255*r))<<8)|255; }
                else { float x = floor((t - 24)/2.0)*2.0; if(x>11) x=11; return (255<<16)|(0<<8)|(int)(255-(255*(x/11.0))); }
            };

            lv_obj_t* pt_day[] = {id(j0_day), id(j1_day), id(j2_day), id(j3_day), id(j4_day), id(j5_day), id(j6_day), id(j7_day), id(j8_day), id(j9_day), id(j10_day), id(j11_day), id(j12_day), id(j13_day), id(j14_day)};
            lv_obj_t* pt_max[] = {id(j0_max), id(j1_max), id(j2_max), id(j3_max), id(j4_max), id(j5_max), id(j6_max), id(j7_max), id(j8_max), id(j9_max), id(j10_max), id(j11_max), id(j12_max), id(j13_max), id(j14_max)};
            lv_obj_t* pt_min[] = {id(j0_min), id(j1_min), id(j2_min), id(j3_min), id(j4_min), id(j5_min), id(j6_min), id(j7_min), id(j8_min), id(j9_min), id(j10_min), id(j11_min), id(j12_min), id(j13_min), id(j14_min)};
            lv_obj_t* pt_l1[]  = {id(j0_icon_layer1), id(j1_icon_layer1), id(j2_icon_layer1), id(j3_icon_layer1), id(j4_icon_layer1), id(j5_icon_layer1), id(j6_icon_layer1), id(j7_icon_layer1), id(j8_icon_layer1), id(j9_icon_layer1), id(j10_icon_layer1), id(j11_icon_layer1), id(j12_icon_layer1), id(j13_icon_layer1), id(j14_icon_layer1)};
            lv_obj_t* pt_l2[]  = {id(j0_icon_layer2), id(j1_icon_layer2), id(j2_icon_layer2), id(j3_icon_layer2), id(j4_icon_layer2), id(j5_icon_layer2), id(j6_icon_layer2), id(j7_icon_layer2), id(j8_icon_layer2), id(j9_icon_layer2), id(j10_icon_layer2), id(j11_icon_layer2), id(j12_icon_layer2), id(j13_icon_layer2), id(j14_icon_layer2)};

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

                if(parts.size() >= 9) {
                    int jour = std::atoi(parts[0].c_str());
                    if(jour < 0 || jour > 14) continue;
                    std::string nom_jour = parts[1];
                    std::string condition = parts[2];
                    float tmin = std::atof(parts[3].c_str());
                    float tmax = std::atof(parts[4].c_str());
                    bool est_repos = (parts[5] == "1" || parts[5] == "true");
                    bool est_dimanche = (parts[6] == "1" || parts[6] == "true");
                    bool est_passe = (parts[7] == "1" || parts[7] == "true");
                    std::string heures_ouverture = parts[8];

                    cal_jour_nom[jour] = nom_jour;
                    cal_heures[jour] = heures_ouverture;
                    bool is_early = false;
                    if (!est_repos && heures_ouverture.length() >= 5) {
                        int hour = atoi(heures_ouverture.substr(0, 2).c_str());
                        int minute = atoi(heures_ouverture.substr(3, 2).c_str());
                        if (hour < 9 || (hour == 9 && minute == 0)) is_early = true;
                    }

                    uint32_t cmax = get_c(tmax);
                    uint32_t cmin = get_c(tmin);
                    char buftx[64]; sprintf(buftx, "#%06x %.0f# / ", cmax, tmax);
                    char buftn[64]; sprintf(buftn, " #%06x %.0f# °C", cmin, tmin);

                    lv_obj_t* day_label = pt_day[jour];
                    lv_obj_t* max_lbl = pt_max[jour];
                    lv_obj_t* min_lbl = pt_min[jour];
                    lv_obj_t* l1 = pt_l1[jour];
                    lv_obj_t* l2 = pt_l2[jour];

                    if(day_label) {
                        lv_label_set_text(day_label, cal_toggled[jour] ? cal_heures[jour].c_str() : nom_jour.c_str());
                        lv_label_set_text(max_lbl, est_passe ? "-- / " : buftx);
                        lv_label_set_text(min_lbl, est_passe ? "-- °C" : buftn);
                        apply_weather_icon(l1, l2, condition, true);

                        if(cal_toggled[jour]) {
                            lv_obj_add_flag(max_lbl, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_add_flag(min_lbl, LV_OBJ_FLAG_HIDDEN);
                        } else {
                            lv_obj_clear_flag(max_lbl, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_clear_flag(min_lbl, LV_OBJ_FLAG_HIDDEN);
                        }

                        uint8_t opa = est_passe ? 100 : 255;
                        uint32_t col = 0xFFFFFF;
                        if(jour == 0) col = 0x4D94FF;
                        else if(est_dimanche) col = est_repos ? 0xFFA500 : 0xFF4D4D;
                        else if(est_repos) col = 0x4CD964;
                        else if(is_early) col = 0xFF8888;
                        if(est_passe) col = 0x6A7B92;

                        lv_obj_set_style_text_color(day_label, lv_color_hex(col), LV_PART_MAIN);
                        lv_obj_set_style_text_opa(day_label, opa, LV_PART_MAIN);
                        lv_obj_set_style_text_opa(l1, opa, LV_PART_MAIN);
                        lv_obj_set_style_text_opa(l2, opa, LV_PART_MAIN);
                        lv_obj_set_style_text_opa(max_lbl, opa, LV_PART_MAIN);
                        lv_obj_set_style_text_opa(min_lbl, opa, LV_PART_MAIN);
                        lv_label_set_recolor(max_lbl, true);
                        lv_label_set_recolor(min_lbl, true);
                        lv_obj_set_style_text_color(max_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                        lv_obj_set_style_text_color(min_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                    }
                }
            }
'''

if 'tab5_maj_previsions_jours_bulk' not in text:
    text = text.replace('  services:', '  services:\n' + bulk_service)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
