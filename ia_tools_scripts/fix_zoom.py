import re
path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-lvgl.yaml'
with open(path, 'r', encoding='utf-8') as f:
    t = f.read()
t = re.sub(r'(- label: \{.*?)(,\s*zoom:\s*[0-9\.]+)(.*?\})', r'\1\3', t)
with open(path, 'w', encoding='utf-8') as f:
    f.write(t)
print('Fixed zoom in labels')
