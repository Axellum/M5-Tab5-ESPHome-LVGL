import re

with open('tab5-ha-hmi.yaml', 'r', encoding='utf-8') as f:
    text = f.read()

# Add resize to icons_2_png_main (22 images)
text = re.sub(r'(file: "icons_2_png_main/[^"]+"\n\s+id: img_(?!card_)[^\n]+)', r'\1\n    resize: 220x220', text)

# Add resize to icons_2_png_card (22 images)
text = re.sub(r'(file: "icons_2_png_card/[^"]+"\n\s+id: img_card_[^\n]+)', r'\1\n    resize: 80x80', text)

# Change resize of Png from 150x150 to 90x90
text = re.sub(r'resize: 150x150', r'resize: 90x90', text)

with open('tab5-ha-hmi.yaml', 'w', encoding='utf-8') as f:
    f.write(text)

with open('tab5-lvgl.yaml', 'r', encoding='utf-8') as f:
    lvgl_text = f.read()

# If icons_2_png_main are 220x220 instead of 250x250, zoom: 0.9 (225) -> zoom: 1.0
lvgl_text = re.sub(r'src: img_cloudy, x: 80, y: 50, zoom: 0.9', r'src: img_cloudy, x: 80, y: 50, zoom: 1.0', lvgl_text)

# Png icons are 90x90. Previously 150x150.
# zoom 0.4 on 150 -> 60. To get 60 from 90 -> zoom 0.66
# zoom 0.5 on 150 -> 75. To get 75 from 90 -> zoom 0.8
# zoom 0.3 on 150 -> 45. To get 45 from 90 -> zoom 0.5
lvgl_text = re.sub(r'(src: img_(uv|gel|neige)[^}]+)zoom: 0\.4', r'\1zoom: 0.6', lvgl_text)
lvgl_text = re.sub(r'(src: img_(temper|hydro|pluis|att|vent|eclair)[^}]+)zoom: 0\.5', r'\1zoom: 0.8', lvgl_text)
lvgl_text = re.sub(r'(src: img_(att|vent|hydro|eclair|neige|gel|pluis)[^}]+)zoom: 0\.4', r'\1zoom: 0.6', lvgl_text)

# icons_2_png_card are 80x80. Previously 250x250 * 0.4 = 100x100
# let's set zoom: 1.2 on cards, which is 96x96
lvgl_text = re.sub(r'(src: img_card_cloudy[^}]+)zoom: 0\.4', r'\1zoom: 1.2', lvgl_text)
lvgl_text = re.sub(r'(src: img_temper[^}]+)zoom: 0\.3', r'\1zoom: 0.5', lvgl_text)

with open('tab5-lvgl.yaml', 'w', encoding='utf-8') as f:
    f.write(lvgl_text)
