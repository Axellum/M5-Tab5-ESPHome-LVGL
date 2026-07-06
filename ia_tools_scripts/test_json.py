# Append a tiny lambda to tab5-api-logic to test ArduinoJson compilation
path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

test_service = '''
    - service: tab5_test_json
      variables:
        payload: string
      then:
        - lambda: |-
            DynamicJsonDocument doc(2048);
            deserializeJson(doc, payload);
'''
if 'tab5_test_json' not in text:
    text = text.replace('  services:', '  services:' + test_service)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
