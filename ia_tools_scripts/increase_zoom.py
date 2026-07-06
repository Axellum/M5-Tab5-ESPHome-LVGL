import re

with open("e:/AuxFilsDesIdees/Nouveaux/tab5-lvgl.yaml", "r", encoding="utf-8") as f:
    text = f.read()

# Pour les alertes, icon_uv, icon_neige, icon_gel, logo_temp_current, logo_hum_current
# Ils avaient zoom: 0.5 ou zoom: 0.3
text = re.sub(r'id:\s*(icon_uv|icon_neige|icon_gel|logo_temp_current|logo_hum_current)([^}]*)zoom:\s*0\.5', r'id: \1\2zoom: 1.5', text)

text = re.sub(r'id:\s*(alerte_glob|alerte_vent|alerte_inon|alerte_orage|alerte_pluie_inon|alerte_neige_verg|alerte_grand_froid|alerte_vagues)([^}]*)zoom:\s*0\.3', r'id: \1\2zoom: 1.2', text)

text = re.sub(r'id:\s*j(\d)_icon_(max|min)([^}]*)zoom:\s*0\.3', r'id: j\1_icon_\2\3zoom: 1.0', text)

with open("e:/AuxFilsDesIdees/Nouveaux/tab5-lvgl.yaml", "w", encoding="utf-8") as f:
    f.write(text)
