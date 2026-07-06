x_pos = [25, 275, 525, 775, 1025]
y_pos = [10, 245, 480]
yaml_str = ""
c = 0
for y in y_pos:
    for x in x_pos:
        yaml_str += f"""        - button:
            id: btn_c{c}
            x: {x}
            y: {y}
            width: 230
            height: 230
            bg_opa: 0
            border_width: 2
            border_color: color_border
            radius: 12
            on_short_click:
              - lambda: |-
                  if(!cal_toggled[{c}]) {{
                     lv_label_set_text(id(c{c}_day), cal_heures[{c}].c_str());
                     cal_toggled[{c}] = true;
                  }} else {{
                     lv_label_set_text(id(c{c}_day), cal_jour_nom[{c}].c_str());
                     cal_toggled[{c}] = false;
                  }}
            widgets:
              - label: {{ id: c{c}_day, text: "Jour", align: TOP_MID, y: 10, text_font: roboto_36_b, text_color: color_text }}
              - label: {{ id: c{c}_max, text: "--°", x: 10, y: 10, text_font: roboto_36_b, text_color: color_temp_max }}
              - image: {{ id: c{c}_icon, src: img_card_cloudy, align: CENTER, y: 35, zoom: 1.0 }}
"""
        c += 1
with open("calendar_gen.txt", "w") as f:
    f.write(yaml_str)
