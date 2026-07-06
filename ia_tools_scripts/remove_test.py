path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

import re
text = re.sub(r'    - service: tab5_test_json.*?deserializeJson\(doc, payload\);', '', text, flags=re.DOTALL)

with open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(text)
