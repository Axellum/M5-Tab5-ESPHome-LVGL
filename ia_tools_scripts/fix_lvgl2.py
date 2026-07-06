import re

with open("tab5-lvgl.yaml", "r", encoding="utf-8") as f:
    lvgl_content = f.read()

# Remove anything from layer_calendar onwards
idx = lvgl_content.find("        # --- CALENDAR LAYER")
if idx != -1:
    lvgl_content = lvgl_content[:idx]

yaml_str = """        # --- CALENDAR LAYER (Instant transition optimization) ---
        - obj:
            id: layer_calendar
            x: 0
            y: 0
            width: 1280
            height: 720
            bg_opa: COVER
            bg_color: color_bg
            border_width: 0
            radius: 0
            hidden: true
            widgets:
"""

x_pos = [25, 275, 525, 775, 1025]
y_pos = [10, 245, 480]

c = 0
for y in y_pos:
    for x in x_pos:
        
        if c == 0:
            click_action = "                    - lambda: 'lv_obj_add_flag(id(layer_calendar), LV_OBJ_FLAG_HIDDEN);'"
        else:
            click_action = f"""                    - lambda: |-
                        if(!cal_toggled[{c}]) {{
                           lv_label_set_text(id(c{c}_day), cal_heures[{c}].c_str());
                           lv_obj_add_flag(id(c{c}_max), LV_OBJ_FLAG_HIDDEN);
                           cal_toggled[{c}] = true;
                        }} else {{
                           lv_label_set_text(id(c{c}_day), cal_jour_nom[{c}].c_str());
                           lv_obj_clear_flag(id(c{c}_max), LV_OBJ_FLAG_HIDDEN);
                           cal_toggled[{c}] = false;
                        }}"""

        yaml_str += f"""              - button:
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
{click_action}
                  widgets:
                    - label: {{ id: c{c}_day, text: "Jour", align: TOP_MID, y: 10, text_font: roboto_36_b, text_color: color_text }}
                    - label: {{ id: c{c}_max, text: "--°", x: 10, y: 10, text_font: roboto_36_b, text_color: color_temp_max }}
                    - image: {{ id: c{c}_icon, src: img_card_cloudy, align: CENTER, y: 35, zoom: 1.0 }}
"""
        c += 1

lvgl_content = lvgl_content.rstrip() + "\n" + yaml_str

# Also fix the clicking on card_4 in page_main to clear the SCROLLABLE flag when opening!
# We just replace any occurrence of the `lambda: lv_obj_clear_flag(id(layer_calendar), LV_OBJ_FLAG_HIDDEN);` 
# with the dual flag clear.

old_click = """            on_short_click:
              - lambda: 'lv_obj_clear_flag(id(layer_calendar), LV_OBJ_FLAG_HIDDEN);'"""
new_click = """            on_short_click:
              - lambda: |-
                  lv_obj_clear_flag(id(layer_calendar), LV_OBJ_FLAG_HIDDEN);
                  lv_obj_clear_flag(id(layer_calendar), LV_OBJ_FLAG_SCROLLABLE);"""

lvgl_content = lvgl_content.replace(old_click, new_click)

with open("tab5-lvgl.yaml", "w", encoding="utf-8") as f:
    f.write(lvgl_content)

print("fixed!")
