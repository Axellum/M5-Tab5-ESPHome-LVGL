import os
import re

filepath = 'tab5-lvgl.yaml'
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Corriger les barres de pluie à 60px
code = code.replace('height: 80', 'height: 60')

# 2. Aligner rain_bar_container et alerts_container
# rain_bar_container:
code = re.sub(r'id: rain_bar_container(\s*layout:.*?)\n\s*x: 100\n\s*y: \d+\n\s*width: 550\n\s*height: 100',
              r'id: rain_bar_container\1\n                x: 100\n                y: 315\n                width: 550\n                height: 60\n                scrollbar_mode: OFF',
              code, flags=re.DOTALL)

# alerts_container:
code = re.sub(r'id: alerts_container(\s*layout:.*?)\n\s*x: 720\n\s*y: \d+\n\s*width: 380\n\s*height: 50',
              r'id: alerts_container\1\n                x: 720\n                y: 315\n                width: 380\n                height: 60\n                scrollbar_mode: OFF',
              code, flags=re.DOTALL)

# 3. Empêcher le scroll des MIN/MAX et ajuster leur taille
# On trouve l'obj FLEX en bas de chaque carte qui fait 'height: 40'
code = re.sub(r'(align: BOTTOM_MID\n\s*y: -15\n\s*width: )200(\n\s*height: )40', r'\1 250\2 50\n                            scrollbar_mode: OFF', code)

# 4. Assurer que les lables MIN/MAX ne scrollent pas non plus si jamais, mais d'abord on remplace le width
# Optionnel. 'scrollbar_mode: OFF' sur l'obj container est parfait.

# Ajustement du zoom des icones j1_icon au cas où "recadre tout bien"
# Vu le MAX-CROP, zoom à 0.38 donne 250*0.38 = 95px.
# On peut laisser tel quel, c'est ce que l'utilisateur voit et aime.

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)
