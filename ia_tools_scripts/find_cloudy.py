import sys
path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

lines = text.split('\n')
for i, line in enumerate(lines):
    if 'img_card_cloudy' in line:
        print(f"Line {i+1}: {line}")
