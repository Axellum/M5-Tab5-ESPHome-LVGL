import re
with open('e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-lvgl.yaml', 'r', encoding='utf-8') as f:
    text = f.read()

m = re.search(r'(- image:\s*\n?\s*id: h0_icon.*?)(?=- label|- obj)', text, re.DOTALL)
if m:
    print("MATCH 1:", m.group(1))
else:
    # Try finding h0_icon
    m2 = re.search(r'(.{0,100}h0_icon.{0,100})', text, re.DOTALL)
    print("MATCH 2:", m2.group(1) if m2 else "NOT FOUND")

