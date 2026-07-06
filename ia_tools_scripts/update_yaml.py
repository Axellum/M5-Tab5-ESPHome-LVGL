import os

filepath = 'tab5-lvgl.yaml'
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

# Remplacement des définitions de police
code = code.replace(
    'gfonts://Roboto\n    id: roboto_24\n    size: 24',
    'gfonts://Roboto:wght=700\n    id: roboto_36_b\n    size: 36'
)
code = code.replace(
    'gfonts://Roboto\n    id: roboto_48\n    size: 48',
    'gfonts://Roboto:wght=700\n    id: roboto_60_b\n    size: 60'
)
code = code.replace(
    'gfonts://Roboto\n    id: roboto_96\n    size: 96',
    'gfonts://Roboto:wght=700\n    id: roboto_120_b\n    size: 120'
)

# Remplacement de l'utilisation des polices
code = code.replace('text_font: roboto_24', 'text_font: roboto_36_b')
code = code.replace('text_font: roboto_48', 'text_font: roboto_60_b')
code = code.replace('text_font: roboto_96', 'text_font: roboto_120_b')

# Remplacement pour les labels 'j1_day' etc pour inclure text_align: CENTER
code = code.replace('text_color: color_text\n                            align: TOP_MID', 'text_color: color_text\n                            text_align: CENTER\n                            align: TOP_MID')

# Ajustement des cartes
code = code.replace('zoom: 0.64', 'zoom: 0.55')
code = code.replace('y: 15\n                        - image:', 'y: 5\n                        - image:')

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)
