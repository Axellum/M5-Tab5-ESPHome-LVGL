# -*- coding: utf-8 -*-
out = """color:
  - id: color_bg
    hex: "232A35" # Darker background
  - id: color_card_bg
    hex: "2D3748" # Dark blue-grey card
  - id: color_border
    hex: "4A5A73" # Soft border
  - id: color_text
    hex: "FFFFFF"
  - id: color_text_dim
    hex: "A3A8B5"
  - id: color_temp_max
    hex: "FF4D4D"
  - id: color_temp_min
    hex: "4D94FF"
  - id: color_bar_active
    hex: "7A8DA6"
  - id: color_bar_inactive
    hex: "343E4E"

font:
  - file: gfonts://Roboto@700
    id: roboto_36_b
    size: 28
    bpp: 4
    glyphs: ' !"#$%&\\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~€‚ƒ„…†‡ˆ‰Š‹ŒŽ‘’“”•–—˜™š›œžŸ¡¢£¤¥¦§¨©ª«¬®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ'
  - file: gfonts://Roboto@700
    id: roboto_120_b
    size: 110
    bpp: 4
    glyphs: '0123456789:'
  - file: gfonts://Roboto@700
    id: roboto_60_b
    size: 50
    bpp: 4
    glyphs: '0123456789: °C.%'

lvgl:
  displays:
    - display_id: my_display
  bg_color: color_bg

  style_definitions:
    - id: transparent_style
      bg_opa: 0
      border_width: 0
    - id: style_card
      bg_color: color_card_bg
      border_width: 1
      border_color: color_border
      radius: 12
      shadow_width: 30
      shadow_color: 0x000000
      shadow_opa: 100
      shadow_ofs_x: 0
      shadow_ofs_y: 5
      opa: 200

  pages:
    - id: page_main
      widgets:
        # ==================== TOP ROW ====================
        
        # --- CARTE 1: MAIN WEATHER BIG BOX ---
        - obj:
            x: 40
            y: 30
            width: 320
            height: 320
            styles: style_card
            widgets:
              - image:
                  id: icon_actuel
                  src: img_cloudy
                  align: CENTER
                  zoom: 0.9

        # --- CARTE 2: STRIP HAUT DATE ET UV ---
        - obj:
            x: 380
            y: 30
            width: 860
            height: 100
            styles: style_card
            widgets:
              - image: { id: icon_uv, src: img_uv, x: 20, y: 15, zoom: 0.7 }
              - image: { id: icon_gel, src: img_gel, x: 120, y: 15, zoom: 0.7 }
              - image: { id: icon_neige, src: img_neige, x: 220, y: 15, zoom: 0.7 }
              - label:
                  id: lbl_date
                  text: "Jeudi 02 Avril"
                  align: RIGHT_MID
                  x: -30
                  text_font: roboto_36_b
                  text_color: color_text

        # --- CARTE 3: TEMPERATURE ET HUMIDITE ---
        - obj:
            x: 380
            y: 150
            width: 410
            height: 200
            styles: style_card
            widgets:
              - image: { id: logo_temp_current, src: img_temper, x: 30, y: 20, zoom: 0.7 }
              - label:
                  id: current_temp
                  text: "-- °C"
                  x: 130
                  y: 35
                  text_font: roboto_60_b
                  text_color: color_temp_max
              - image: { id: logo_hum_current, src: img_hydro, x: 30, y: 110, zoom: 0.7 }
              - label:
                  id: current_hum
                  text: "-- %"
                  x: 130
                  y: 125
                  text_font: roboto_60_b
                  text_color: color_temp_min

        # --- CARTE 4: HEURE BIG ---
        - obj:
            x: 810
            y: 150
            width: 430
            height: 200
            styles: style_card
            widgets:
              - label:
                  id: lbl_time
                  text: "19:50"
                  align: CENTER
                  text_font: roboto_120_b
                  text_color: color_text

        # ==================== MIDDLE STRIP (RAIN & ALERTS) ====================
        - image: { id: alerte_pluie_inon, src: img_pluis, x: 40, y: 375, zoom: 0.5 }
"""
    
# Generate 12 Rain Bars
bx = 110
for i in range(12):
    out += f"""
        - obj:
            id: rb_{i}
            x: {bx}
            y: 380
            width: 25
            height: 35
            bg_color: color_bar_inactive
            border_width: 1
            border_color: color_border
            radius: 4
"""
    bx += 35
    
out += f"""
        - label:
            id: lbl_proc_pluie
            text: "-- mn"
            x: {bx + 10}
            y: 380
            text_font: roboto_36_b
            text_color: color_text

        - image: {{{{ id: alerte_glob, src: img_att, x: 580, y: 375, zoom: 0.5 }}}}
        - image: {{{{ id: alerte_vent, src: img_vent, x: 650, y: 375, zoom: 0.5 }}}}
        - image: {{{{ id: alerte_inon, src: img_hydro, x: 720, y: 375, zoom: 0.5 }}}}
        - image: {{{{ id: alerte_orage, src: img_eclair, x: 790, y: 375, zoom: 0.5 }}}}
        - image: {{{{ id: alerte_neige_verg, src: img_neige, x: 860, y: 375, zoom: 0.5 }}}}
        - image: {{{{ id: alerte_grand_froid, src: img_gel, x: 930, y: 375, zoom: 0.5 }}}}
        - image: {{{{ id: alerte_vagues, src: img_att, x: 1000, y: 375, zoom: 0.5 }}}}

        # ==================== BOTTOM CARDS (FORECAST) ====================
""".replace('{{', '{').replace('}}', '}')

# 4 Forecast cards (Nextion format)
cx = 40
for i in range(4):
    out += f"""
        - obj:
            x: {cx}
            y: 440
            width: 280
            height: 250
            styles: style_card
            widgets:
              - label:
                  id: j{i}_day
                  text: "Jour {i}"
                  align: TOP_MID
                  y: 10
                  text_font: roboto_36_b
                  text_color: color_text
              - image:
                  id: j{i}_icon
                  src: img_card_cloudy
                  align: CENTER
                  y: -10
                  zoom: 0.40
              - image:
                  id: j{i}_icon_max
                  src: img_temper
                  align: BOTTOM_LEFT
                  x: 30
                  y: -60
                  zoom: 0.4
              - label:
                  id: j{i}_max
                  text: "-- °C"
                  align: BOTTOM_RIGHT
                  x: -30
                  y: -65
                  text_font: roboto_60_b
                  text_color: color_temp_max
              - image:
                  id: j{i}_icon_min
                  src: img_temper
                  align: BOTTOM_LEFT
                  x: 30
                  y: -15
                  zoom: 0.4
              - label:
                  id: j{i}_min
                  text: "-- °C"
                  align: BOTTOM_RIGHT
                  x: -30
                  y: -20
                  text_font: roboto_60_b
                  text_color: color_temp_min
"""
    cx += 306
    
out += """
        - obj:
            x: 2000
            y: 2000
            width: 1
            height: 1
            widgets:
              - label: { id: j4_day, text: "" }
              - image: { id: j4_icon, src: img_card_cloudy }
              - image: { id: j4_icon_max, src: img_temper }
              - label: { id: j4_max, text: "" }
              - image: { id: j4_icon_min, src: img_temper }
              - label: { id: j4_min, text: "" }
"""

with open("e:/AuxFilsDesIdees/Nouveaux/tab5-lvgl.yaml", "w", encoding="utf-8") as f:
    f.write(out)

print("LVGL rebuilt.")
