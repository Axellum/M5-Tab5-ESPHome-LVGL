import os
import re

filepath = 'tab5-lvgl.yaml'
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

# On remplace tous les 'height: 40' par 'height: 80' dans le widget des barres
code = re.sub(r'width: 20, height: 40', 'width: 20, height: 80', code)
code = re.sub(r'id: rain_bar_container(\s+.*\n)*?\s+height: 30', 'id: rain_bar_container\\1                height: 100', code, count=1)
code = re.sub(r'id: rain_bar_container(\s+.*\n)*?\s+height: \d+', 'id: rain_bar_container\\1                height: 100', code, count=1)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)
