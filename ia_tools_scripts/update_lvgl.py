import re

with open("tab5-lvgl.yaml", "r", encoding="utf-8") as f:
    lvgl_content = f.read()

# 1. Update Card 4 to be a button and add on_short_click
card4_search = """        # Card 4
        - obj:
            x: 1025
            y: 430
            width: 230
            height: 280
            bg_opa: 0
            border_width: 2
            border_color: color_border
            radius: 12
            widgets:"""

card4_replace = """        # Card 4
        - button:
            id: btn_card_4
            x: 1025
            y: 430
            width: 230
            height: 280
            bg_opa: 0
            border_width: 2
            border_color: color_border
            radius: 12
            on_short_click:
              - lambda: 'lv_disp_load_scr(id(page_calendar));'
            widgets:"""

lvgl_content = lvgl_content.replace(card4_search, card4_replace)

# 2. Add page_calendar at the end of the pages block
with open("calendar_gen.txt", "r") as f:
    cal_buttons = f.read()

page_cal = """
    - id: page_calendar
      widgets:
        - button:
            x: 0
            y: 0
            width: 1280
            height: 720
            bg_opa: 0
            border_width: 0
            on_short_click:
              - lambda: 'lv_disp_load_scr(id(page_main));'
""" + cal_buttons

lvgl_content += page_cal

# 3. Remove zoom: 1.0 from icon_actuel (just in case they complain it's still there)
lvgl_content = lvgl_content.replace('image: { id: icon_actuel, src: img_cloudy, x: 35, y: 10, zoom: 1.0 }', 'image: { id: icon_actuel, src: img_cloudy, x: 35, y: 10 }')

with open("tab5-lvgl.yaml", "w", encoding="utf-8") as f:
    f.write(lvgl_content)
