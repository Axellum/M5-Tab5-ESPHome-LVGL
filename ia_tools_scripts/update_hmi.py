import re

with open("tab5-ha-hmi.yaml", "r", encoding="utf-8") as f:
    hmi_content = f.read()

# Replace tab5_maj_previsions_jours lambda block
search_block = """    - service: tab5_maj_previsions_jours
      variables:
        jour: int
        nom_jour: string
        condition: string
        tmin: float
        tmax: float
        est_repos: bool
        est_dimanche: bool
      then:
        - lambda: |-
            std::string state = condition;
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
            else if (state == "snowy-rainy") icon = id(img_card_snowy_rainy);

            char buftx[32]; sprintf(buftx, "%.0f / ", tmax);
            char buftn[32]; sprintf(buftn, "%.0f °C", tmin);

            if (jour >= 0 && jour <= 4) {
              lv_obj_t* day_label = NULL;
              if(jour == 0) { day_label = id(j0_day); lv_img_set_src(id(j0_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j0_max), buftx); lv_label_set_text(id(j0_min), buftn); }
              else if(jour == 1) { day_label = id(j1_day); lv_img_set_src(id(j1_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j1_max), buftx); lv_label_set_text(id(j1_min), buftn); }
              else if(jour == 2) { day_label = id(j2_day); lv_img_set_src(id(j2_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j2_max), buftx); lv_label_set_text(id(j2_min), buftn); }
              else if(jour == 3) { day_label = id(j3_day); lv_img_set_src(id(j3_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j3_max), buftx); lv_label_set_text(id(j3_min), buftn); }
              else if(jour == 4) { day_label = id(j4_day); lv_img_set_src(id(j4_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j4_max), buftx); lv_label_set_text(id(j4_min), buftn); }
              
              if(day_label != NULL) {
                lv_label_set_text(day_label, nom_jour.c_str());
                if(est_dimanche) {
                  if(est_repos) {
                    lv_obj_set_style_text_color(day_label, lv_color_hex(0xFFA500), LV_PART_MAIN); // Orange si repos le dimanche
                  } else {
                    lv_obj_set_style_text_color(day_label, lv_color_hex(0xFF4D4D), LV_PART_MAIN); // Rouge si travail le dimanche
                  }
                } else if(est_repos) {
                  lv_obj_set_style_text_color(day_label, lv_color_hex(0x4CD964), LV_PART_MAIN); // Vert Vif pour les autres repos
                } else {
                  lv_obj_set_style_text_color(day_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // Blanc par défaut
                }
              }
            }"""

replace_block = """    - service: tab5_maj_previsions_jours
      variables:
        jour: int
        nom_jour: string
        condition: string
        tmin: float
        tmax: float
        est_repos: bool
        est_dimanche: bool
        est_passe: bool
        heures_ouverture: string
      then:
        - lambda: |-
            std::string state = condition;
            auto icon = id(img_card_cloudy);
            if (state == "clear-night") icon = id(img_card_night);
            else if (state == "cloudy") icon = id(img_card_cloudy);
            else if (state == "fog") icon = id(img_card_fog);
            else if (state == "hail") icon = id(img_card_snowy_rainy);
            else if (state == "lightning") icon = id(img_card_thunder);
            else if (state == "lightning-rainy") icon = id(img_card_lightning_rainy);
            else if (state == "Clear" || state == "sunny" || state == "partlycloudy") icon = id(img_card_day);
            else if (state == "pouring") icon = id(img_card_rainy_7);
            else if (state == "rainy") icon = id(img_card_rainy_5);
            else if (state == "snowy") icon = id(img_card_snowy_6);
            else if (state == "windy") icon = id(img_card_windy);
            else if (state == "snowy-rainy") icon = id(img_card_snowy_rainy);

            char buftx[32]; sprintf(buftx, "%.0f / ", tmax);
            char buftn[32]; sprintf(buftn, "%.0f °C", tmin);
            char buftx_cal[32]; sprintf(buftx_cal, "%.0f°", tmax);

            // Set main page Cards
            if (jour >= 0 && jour <= 4) {
              lv_obj_t* day_label = NULL;
              if(jour == 0) { day_label = id(j0_day); lv_img_set_src(id(j0_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j0_max), buftx); lv_label_set_text(id(j0_min), buftn); }
              else if(jour == 1) { day_label = id(j1_day); lv_img_set_src(id(j1_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j1_max), buftx); lv_label_set_text(id(j1_min), buftn); }
              else if(jour == 2) { day_label = id(j2_day); lv_img_set_src(id(j2_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j2_max), buftx); lv_label_set_text(id(j2_min), buftn); }
              else if(jour == 3) { day_label = id(j3_day); lv_img_set_src(id(j3_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j3_max), buftx); lv_label_set_text(id(j3_min), buftn); }
              else if(jour == 4) { day_label = id(j4_day); lv_img_set_src(id(j4_icon), icon->get_lv_img_dsc()); lv_label_set_text(id(j4_max), buftx); lv_label_set_text(id(j4_min), buftn); }
              
              if(day_label != NULL) {
                lv_label_set_text(day_label, nom_jour.c_str());
                if(jour == 0) {
                    lv_obj_set_style_text_color(day_label, lv_color_hex(0x4D94FF), LV_PART_MAIN); // Aujourd'hui = Bleu
                } else {
                    if(est_dimanche) {
                      if(est_repos) lv_obj_set_style_text_color(day_label, lv_color_hex(0xFFA500), LV_PART_MAIN);
                      else lv_obj_set_style_text_color(day_label, lv_color_hex(0xFF4D4D), LV_PART_MAIN);
                    } else if(est_repos) {
                      lv_obj_set_style_text_color(day_label, lv_color_hex(0x4CD964), LV_PART_MAIN);
                    } else {
                      lv_obj_set_style_text_color(day_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                    }
                }
              }
            }
            
            // Set Calendar Grid
            if (jour >= 0 && jour <= 14) {
              lv_obj_t* cal_label = NULL;
              lv_obj_t* cal_icon = NULL;
              lv_obj_t* cal_temp = NULL;
              
              switch(jour) {
                case 0: cal_label = id(c0_day); cal_icon = id(c0_icon); cal_temp = id(c0_max); break;
                case 1: cal_label = id(c1_day); cal_icon = id(c1_icon); cal_temp = id(c1_max); break;
                case 2: cal_label = id(c2_day); cal_icon = id(c2_icon); cal_temp = id(c2_max); break;
                case 3: cal_label = id(c3_day); cal_icon = id(c3_icon); cal_temp = id(c3_max); break;
                case 4: cal_label = id(c4_day); cal_icon = id(c4_icon); cal_temp = id(c4_max); break;
                case 5: cal_label = id(c5_day); cal_icon = id(c5_icon); cal_temp = id(c5_max); break;
                case 6: cal_label = id(c6_day); cal_icon = id(c6_icon); cal_temp = id(c6_max); break;
                case 7: cal_label = id(c7_day); cal_icon = id(c7_icon); cal_temp = id(c7_max); break;
                case 8: cal_label = id(c8_day); cal_icon = id(c8_icon); cal_temp = id(c8_max); break;
                case 9: cal_label = id(c9_day); cal_icon = id(c9_icon); cal_temp = id(c9_max); break;
                case 10: cal_label = id(c10_day); cal_icon = id(c10_icon); cal_temp = id(c10_max); break;
                case 11: cal_label = id(c11_day); cal_icon = id(c11_icon); cal_temp = id(c11_max); break;
                case 12: cal_label = id(c12_day); cal_icon = id(c12_icon); cal_temp = id(c12_max); break;
                case 13: cal_label = id(c13_day); cal_icon = id(c13_icon); cal_temp = id(c13_max); break;
                case 14: cal_label = id(c14_day); cal_icon = id(c14_icon); cal_temp = id(c14_max); break;
              }
              
              if(cal_label != NULL) {
                // save info globally to be exchanged on click
                id(cal_jour_nom)[jour] = nom_jour;
                id(cal_heures)[jour] = heures_ouverture.empty() ? "Repos" : heures_ouverture;
                
                lv_label_set_text(cal_label, nom_jour.c_str());
                lv_label_set_text(cal_temp, buftx_cal);
                lv_img_set_src(cal_icon, icon->get_lv_img_dsc());
                
                uint32_t col = 0xFFFFFF;
                if(jour == 0) col = 0x4D94FF; // Aujourd'hui = Bleu
                else {
                    if(est_dimanche) {
                      if(est_repos) col = 0xFFA500;
                      else col = 0xFF4D4D;
                    } else if(est_repos) {
                      col = 0x4CD964;
                    }
                }
                
                lv_obj_set_style_text_color(cal_label, lv_color_hex(col), LV_PART_MAIN);
                
                if (state == "unknown" || est_passe) {
                     lv_obj_set_style_img_recolor_opa(cal_icon, 150, LV_PART_MAIN);
                     lv_obj_set_style_img_recolor(cal_icon, lv_color_hex(0x555555), LV_PART_MAIN);
                     lv_obj_set_style_text_color(cal_temp, lv_color_hex(0x666666), LV_PART_MAIN);
                     lv_obj_set_style_text_color(cal_label, lv_color_hex(0x666666), LV_PART_MAIN);
                } else {
                     lv_obj_set_style_img_recolor_opa(cal_icon, 0, LV_PART_MAIN);
                     lv_obj_set_style_text_color(cal_temp, lv_color_hex(0xFF8888), LV_PART_MAIN);
                }
              }
            }"""

if search_block not in hmi_content:
    print("Block not found!")
else:
    hmi_content = hmi_content.replace(search_block, replace_block)
    # Also add the globals for calendar logic
    global_search = """globals:
  - id: clim_target_temp
    type: float
    initial_value: '20.0'"""
    
    global_replace = global_search + """
  - id: cal_jour_nom
    type: std::vector<std::string>
    initial_value: 'std::vector<std::string>(15, "")'
  - id: cal_heures
    type: std::vector<std::string>
    initial_value: 'std::vector<std::string>(15, "")'
  - id: cal_toggled
    type: std::vector<bool>
    initial_value: 'std::vector<bool>(15, false)'"""
    
    hmi_content = hmi_content.replace(global_search, global_replace)
    
    # Also we need to add the lambda for toggling inside api block or as a script
    # Wait, ESPHome api limits what we can do. I'll add an API service or just a C++ function in globals / includes?
    # No, let's add a script toggle.

with open("tab5-ha-hmi.yaml", "w", encoding="utf-8") as f:
    f.write(hmi_content)
    print("Updates applied.")
