import re

with open("tab5-ha-hmi.yaml", "r", encoding="utf-8") as f:
    hmi_yaml = f.read()

# We need to find the `tab5_maj_previsions_jours` definition and replace it completely.
# Here is the target structure.

service_start_marker = "    - service: tab5_maj_previsions_jours"
service_end_marker = "ota:"

start_idx = hmi_yaml.find(service_start_marker)
end_idx = hmi_yaml.find(service_end_marker, start_idx)

if start_idx != -1 and end_idx != -1:
    new_yaml = """    - service: tab5_maj_previsions_jours
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
            id(cal_jour_nom)[jour] = nom_jour;
            id(cal_heures)[jour] = heures_ouverture;
            
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
            char bufmax[32]; sprintf(bufmax, "%.0f°", tmax); // Pour la card calendrier

            // 1) Mise à jour de la page d'accueil (J0 à J4)
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
                  lv_obj_set_style_text_color(day_label, lv_color_hex(0x4D94FF), LV_PART_MAIN); // Bleu pour Aujourd'hui
                } else if(est_dimanche) {
                  if(est_repos) lv_obj_set_style_text_color(day_label, lv_color_hex(0xFFA500), LV_PART_MAIN);
                  else lv_obj_set_style_text_color(day_label, lv_color_hex(0xFF4D4D), LV_PART_MAIN);
                } else if(est_repos) {
                  lv_obj_set_style_text_color(day_label, lv_color_hex(0x4CD964), LV_PART_MAIN);
                } else {
                  lv_obj_set_style_text_color(day_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
                }
              }
            }

            // 2) Mise à jour du calendrier 15 jours (C0 à C14)
            if (jour >= 0 && jour <= 14) {
              lv_obj_t* c_day = NULL;
              lv_obj_t* c_max = NULL;
              lv_obj_t* c_icon = NULL;
              
              if(jour == 0) { c_day = id(c0_day); c_max = id(c0_max); c_icon = id(c0_icon); }
              else if(jour == 1) { c_day = id(c1_day); c_max = id(c1_max); c_icon = id(c1_icon); }
              else if(jour == 2) { c_day = id(c2_day); c_max = id(c2_max); c_icon = id(c2_icon); }
              else if(jour == 3) { c_day = id(c3_day); c_max = id(c3_max); c_icon = id(c3_icon); }
              else if(jour == 4) { c_day = id(c4_day); c_max = id(c4_max); c_icon = id(c4_icon); }
              else if(jour == 5) { c_day = id(c5_day); c_max = id(c5_max); c_icon = id(c5_icon); }
              else if(jour == 6) { c_day = id(c6_day); c_max = id(c6_max); c_icon = id(c6_icon); }
              else if(jour == 7) { c_day = id(c7_day); c_max = id(c7_max); c_icon = id(c7_icon); }
              else if(jour == 8) { c_day = id(c8_day); c_max = id(c8_max); c_icon = id(c8_icon); }
              else if(jour == 9) { c_day = id(c9_day); c_max = id(c9_max); c_icon = id(c9_icon); }
              else if(jour == 10) { c_day = id(c10_day); c_max = id(c10_max); c_icon = id(c10_icon); }
              else if(jour == 11) { c_day = id(c11_day); c_max = id(c11_max); c_icon = id(c11_icon); }
              else if(jour == 12) { c_day = id(c12_day); c_max = id(c12_max); c_icon = id(c12_icon); }
              else if(jour == 13) { c_day = id(c13_day); c_max = id(c13_max); c_icon = id(c13_icon); }
              else if(jour == 14) { c_day = id(c14_day); c_max = id(c14_max); c_icon = id(c14_icon); }

              if (c_day != NULL) {
                lv_label_set_text(c_day, nom_jour.c_str());
                lv_label_set_text(c_max, est_passe ? "--" : bufmax);
                lv_img_set_src(c_icon, icon->get_lv_img_dsc());
                
                uint8_t opa = est_passe ? 100 : 255;
                uint32_t col = 0xFFFFFF; // Blanc (Défaut)
                
                if (jour == 0) {
                  col = 0x4D94FF; // Bleu
                } else if(est_dimanche) {
                  col = est_repos ? 0xFFA500 : 0xFF4D4D;
                } else if(est_repos) {
                  col = 0x4CD964;
                }

                if (est_passe) col = 0x6A7B92; // Gris

                lv_obj_set_style_text_color(c_day, lv_color_hex(col), LV_PART_MAIN);
                lv_obj_set_style_text_opa(c_day, opa, LV_PART_MAIN);
                lv_obj_set_style_img_opa(c_icon, opa, LV_PART_MAIN);
                lv_obj_set_style_text_opa(c_max, opa, LV_PART_MAIN);
              }
            }


"""
    hmi_yaml = hmi_yaml[:start_idx] + new_yaml + hmi_yaml[end_idx:]
    with open("tab5-ha-hmi.yaml", "w", encoding="utf-8") as f:
        f.write(hmi_yaml)
    print("tab5-ha-hmi.yaml patched successfully.")
else:
    print("Could not find the service definition.")
