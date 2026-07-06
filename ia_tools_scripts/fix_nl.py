import os

files = [r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-sensors.yaml', r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-api-logic.yaml']

for path in files:
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    
    # Remove double newlines that might have been accidentally introduced
    while '\n\n\n' in text:
        text = text.replace('\n\n\n', '\n\n')
    
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
