import re

path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-sensors.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

pattern_temp = r"float t = x;[\s\r\n]*uint32_t c_int;[\s\r\n]*if \(t <= -12\) c_int = 0xFF0000;[\s\r\n]*else if \(t <= 0\) \{.*?\}[\s\r\n]*else if \(t <= 14\) \{.*?\}[\s\r\n]*else if \(t <= 24\) \{.*?\}[\s\r\n]*else \{.*?s/11\.0\)\);[\s\r\n]*\}"

text, num_temp = re.subn(pattern_temp, "uint32_t c_int = get_temperature_color(x);", text, flags=re.DOTALL)

with open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(text)
print(f"Modifications temp: {num_temp}")
