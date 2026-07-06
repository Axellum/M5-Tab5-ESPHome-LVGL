import re

path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-lvgl.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

# Match the blocks, capturing the indentation of the first property
# Pattern for transparent obj
pat_trans = r'(^[ \t]*)bg_opa:\s*0\s*\n\s*border_width:\s*0\s*\n\s*pad_all:\s*0\s*\n\s*scrollbar_mode:\s*"OFF"'
text, c1 = re.subn(pat_trans, r'\1styles: style_transparent', text, flags=re.MULTILINE)

# Pattern for climate btn
pat_btn = r'(^[ \t]*)bg_color:\s*color_bg\s*\n\s*bg_opa:\s*100%\s*\n\s*border_width:\s*0\s*\n\s*shadow_width:\s*0\s*\n\s*outline_width:\s*0'
text, c2 = re.subn(pat_btn, r'\1styles: style_clim_btn', text, flags=re.MULTILINE)

# Pattern for standard transparent without pad_all
pat_trans2 = r'(^[ \t]*)bg_opa:\s*0\s*\n\s*border_width:\s*0\s*\n\s*shadow_width:\s*0\s*\n\s*outline_width:\s*0'
text, c3 = re.subn(pat_trans2, r'\1styles: style_invisible', text, flags=re.MULTILINE)

with open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(text)

print(f"Replaced {c1} transparent blocks, {c2} climate btn blocks, {c3} invisible blocks.")
