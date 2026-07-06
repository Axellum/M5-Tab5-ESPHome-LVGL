import os

filepath = 'tab5-lvgl.yaml'
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

code = code.replace('scrollbar_mode: OFF', 'scrollbar_mode: "OFF"')

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)
