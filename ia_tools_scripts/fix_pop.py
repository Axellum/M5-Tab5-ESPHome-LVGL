import re

path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('pt_pop', 'pt_prob')
text = re.sub(r'id\(h(\d+)_pop\)', r'id(h\1_prob)', text)

with open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(text)
