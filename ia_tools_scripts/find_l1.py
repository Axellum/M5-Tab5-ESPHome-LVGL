import sys
path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

print("== l1/l2 search ==")
for i, line in enumerate(lines):
    if 'lv_obj_t* l1' in line or 'apply_weather_icon' in line:
        print(f"{i+1}: {line.strip()}")

