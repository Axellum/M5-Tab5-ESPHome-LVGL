import re

with open('tab5-ha-hmi.yaml', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('\n    resize: 150x150', '')

def repl(m):
    return m.group(0).replace('icons_2_png_main_scrambled', 'icons_2_png_card_scrambled')

text = re.sub(r'  - file: \"icons_2_png_main_scrambled/[^\"]+\"\n    id: img_card_[^\n]+', repl, text)

with open('tab5-ha-hmi.yaml', 'w', encoding='utf-8') as f:
    f.write(text)
