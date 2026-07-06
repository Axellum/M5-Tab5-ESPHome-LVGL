import os

filepath = 'tab5-lvgl.yaml'
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

# Inverser color_bg et color_card_bg
code = code.replace('id: color_bg\n    hex: "23262E"', 'id: color_bg\n    hex: "323642"')
code = code.replace('id: color_card_bg\n    hex: "323642"', 'id: color_card_bg\n    hex: "23262E"')

# Ajuster rain_bar_container
import re

# On remplace tous les 'width: 15, height: XX' par 'width: 20, height: 40'
code = re.sub(r'width: 15, height: \d+', 'width: 20, height: 40', code)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)
