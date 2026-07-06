import re, glob, os

files = glob.glob(r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\ui_components\*.yaml')
files.append(r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-lvgl.yaml')

# Pattern for transparent obj
pat_trans = r'(^[ \t]*)bg_opa:\s*0\s*\n\s*border_width:\s*0\s*\n\s*pad_all:\s*0\s*\n\s*scrollbar_mode:\s*"OFF"'
# Pattern for climate btn
pat_btn = r'(^[ \t]*)bg_color:\s*color_bg\s*\n\s*bg_opa:\s*100%\s*\n\s*border_width:\s*0\s*\n\s*shadow_width:\s*0\s*\n\s*outline_width:\s*0'
# Pattern for standard invisible
pat_trans2 = r'(^[ \t]*)bg_opa:\s*0\s*\n\s*border_width:\s*0\s*\n\s*shadow_width:\s*0\s*\n\s*outline_width:\s*0'

total_c1 = total_c2 = total_c3 = 0

for path in files:
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    
    text, c1 = re.subn(pat_trans, r'\1styles: style_transparent', text, flags=re.MULTILINE)
    text, c2 = re.subn(pat_btn, r'\1styles: style_clim_btn', text, flags=re.MULTILINE)
    text, c3 = re.subn(pat_trans2, r'\1styles: style_invisible', text, flags=re.MULTILINE)
    
    # Also strip excessive newlines safely
    text = re.sub(r'\n[ \t]*\n([ \t]*\n)+', '\n\n', text)
    
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
        
    total_c1 += c1
    total_c2 += c2
    total_c3 += c3

print(f"Total replaced: {total_c1} trans, {total_c2} btn, {total_c3} invis.")
