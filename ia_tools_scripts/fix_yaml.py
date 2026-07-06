import re

with open('tab5-ha-hmi.yaml', 'r', encoding='utf-8') as f:
    orig = f.read()

# 1. Remove transparency
clean = re.sub(r'^\s*transparency:\s*alpha_channel\s*\n', '', orig, flags=re.MULTILINE)

# 2. Force RGB565 globally inside image definitions
clean = re.sub(r'(type:\s*)RGB(?:\s*\n)', r'\g<1>RGB565\n', clean)
clean = re.sub(r'(type:\s*)RGBA(?:\s*\n)', r'\g<1>RGB565\n', clean)

# 3. Path replacements for main and card folders
clean = clean.replace('icons_2_png_main/', 'icons_2_png_main_scrambled/')
clean = clean.replace('icons_2_png_card/', 'icons_2_png_card_scrambled/')

# 4. Duplicate Png folder images to have both _main and _card versions
png_blocks = re.findall(r'(\s*-\s*file:\s*[\"\']?Png/(\w+\.png)[\"\']?\s*\n\s*id:\s*(img_\w+)\s*\n\s*type:\s*RGB565\s*\n*)', clean)

for block_full, fname, img_id in png_blocks:
    new_main = block_full.replace('Png/', 'Png_main_scrambled/').replace(img_id, f'{img_id}_main')
    new_card = block_full.replace('Png/', 'Png_card_scrambled/').replace(img_id, f'{img_id}_card')
    clean = clean.replace(block_full, new_main + new_card)

with open('tab5-ha-hmi.yaml', 'w', encoding='utf-8') as f:
    f.write(clean)

print('YAML REWRITE SUCCESS')
