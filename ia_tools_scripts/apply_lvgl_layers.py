import re
import os

lvgl_path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-lvgl.yaml'
with open(lvgl_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace icon_actuel
content = re.sub(
    r'([ \t]*)- image: \{\s*id:\s*icon_actuel,\s*src:\s*([a-zA-Z0-9_]+),(.*?)\}',
    r'\1- image: { id: icon_actuel, src: img_lightning_rainy, hidden: true,\3}\n\1- label: { id: icon_actuel_layer1, text: "", hidden: true,\3, text_font: font_meteo_main, text_color: color_text }\n\1- label: { id: icon_actuel_layer2, text: "", hidden: true,\3, text_font: font_meteo_main, text_color: color_text }',
    content
)

# Replace jX_icon and hX_icon
content = re.sub(
    r'([ \t]*)- image: \{\s*id:\s*([jh]\d+)_icon,\s*src:\s*([a-zA-Z0-9_]+),(.*?)\}',
    r'\1- image: { id: \2_icon, src: img_card_lightning_rainy, hidden: true,\4}\n\1- label: { id: \2_icon_layer1, text: "", hidden: true,\4, text_font: font_meteo_card, text_color: color_text }\n\1- label: { id: \2_icon_layer2, text: "", hidden: true,\4, text_font: font_meteo_card, text_color: color_text }',
    content
)

with open(lvgl_path, 'w', encoding='utf-8') as f:
    f.write(content)
print('Updated tab5-lvgl.yaml')
