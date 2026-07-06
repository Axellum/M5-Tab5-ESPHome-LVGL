import re

# 1. Fix tab5-lvgl.yaml
path_lvgl = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-lvgl.yaml'
with open(path_lvgl, 'r', encoding='utf-8') as f:
    t = f.read()

pattern = r'([ \t]*)- image:\s*\n\s*id:\s*(h\d+)_icon\s*\n\s*src:\s*[a-zA-Z0-9_]+\s*\n\s*align:\s*CENTER\s*\n\s*y:\s*0\s*\n\s*zoom:\s*1\.0'
rep = r'\1- image:\n\1    id: \2_icon\n\1    src: img_card_lightning_rainy\n\1    hidden: True\n\1    align: CENTER\n\1    y: 0\n\1- label:\n\1    id: \2_icon_layer1\n\1    text: ""\n\1    hidden: True\n\1    align: CENTER\n\1    y: 0\n\1    text_font: font_meteo_card\n\1    text_color: color_text\n\1- label:\n\1    id: \2_icon_layer2\n\1    text: ""\n\1    hidden: True\n\1    align: CENTER\n\1    y: 0\n\1    text_font: font_meteo_card\n\1    text_color: color_text'

t = re.sub(pattern, rep, t)
with open(path_lvgl, 'w', encoding='utf-8') as f:
    f.write(t)

# 2. Fix tab5-api-logic.yaml (remove old img_card_cloudy references that failed to replace)
path_api = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-api-logic.yaml'
with open(path_api, 'r', encoding='utf-8') as f:
    t2 = f.read()

# Replace specifically the hourly section legacy code
heures_old_regex = r'auto icon = id\(img_card_cloudy\);\s*std::string state = condition;\s*if \(state == "clear-night"\) icon = id\(img_card_night\);\s*else if \(state == "cloudy"\) icon = id\(img_card_cloudy\);\s*else if \(state == "fog"\) icon = id\(img_card_fog\);\s*else if \(state == "hail"\) icon = id\(img_card_snowy_rainy\);\s*else if \(state == "lightning"\) icon = id\(img_card_thunder\);\s*else if \(state == "lightning-rainy"\) icon = id\(img_card_lightning_rainy\);\s*else if \(state == "Clear" \|\| state == "sunny"\) icon = id\(img_card_day\);\s*else if \(state == "pouring"\) icon = id\(img_card_rainy_7\);\s*else if \(state == "partlycloudy"\) icon = id\(img_card_cloudy_day_3\);\s*else if \(state == "rainy"\) icon = id\(img_card_rainy_5\);\s*else if \(state == "snowy"\) icon = id\(img_card_snowy_6\);\s*else if \(state == "windy"\) icon = id\(img_card_windy\);\s*else if \(state == "snowy-rainy"\) icon = id\(img_card_snowy_rainy\);\s*lv_img_set_src\(icon_img, icon->get_lv_img_dsc\(\)\);'

t2 = re.sub(heures_old_regex, 'std::string state = condition;\n                 apply_weather_icon(icon_img, l1, l2, state, true);', t2)

with open(path_api, 'w', encoding='utf-8') as f:
    f.write(t2)

print('Fixed hX_icon and API logic')
