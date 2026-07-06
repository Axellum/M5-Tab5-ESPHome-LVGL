import os
import re

filepath = 'tab5-ha-hmi.yaml'
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Traiter le bloc des images
image_block_match = re.search(r'image:\n(.*?)packages:', code, re.DOTALL)
if image_block_match:
    original_images = image_block_match.group(1)
    
    # Remplacer icons_2_png par icons_2_png_main
    main_images = original_images.replace('icons_2_png/', 'icons_2_png_main/')
    
    # Créer les copies pour cards (icons_2_png_card, id: img_card_)
    card_images = original_images.replace('icons_2_png/', 'icons_2_png_card/').replace('id: img_', 'id: img_card_')
    
    new_image_block = "image:\n" + main_images + card_images + "\npackages:"
    code = code.replace(image_block_match.group(0), new_image_block)

# 2. Remplacer les appels img_ par img_card_ dans les blocs meteo_jx_cond
# On sait que ces blocs se trouvent dans la partie finale du fichier où on a j1_icon, j2_icon, etc.
# Au lieu de parser finement, on peut séparer le code à partir de "meteo_j1_cond"
split_index = code.find('meteo_j1_cond')
if split_index != -1:
    top_part = code[:split_index]
    bottom_part = code[split_index:]
    
    # Remplacer id(img_ par id(img_card_
    bottom_part = bottom_part.replace('id(img_', 'id(img_card_')
    
    code = top_part + bottom_part

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)
