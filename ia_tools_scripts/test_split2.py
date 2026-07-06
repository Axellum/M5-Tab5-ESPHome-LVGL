import os
import shutil

ui_dir = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\ui_components'
if os.path.exists(ui_dir):
    shutil.rmtree(ui_dir)
os.makedirs(ui_dir)

path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-lvgl.bak2'  # The version BEFORE my broken script
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

out_lines = []
in_widgets = False
i = 0

def get_indent(line):
    return len(line) - len(line.lstrip())

while i < len(lines):
    line = lines[i]
    if line.startswith('      widgets:'):
        in_widgets = True
        out_lines.append(line)
        i += 1
        continue
    
    if in_widgets and line.startswith('        - '):
        block = [line]
        indent = get_indent(line) # 8
        j = i + 1
        while j < len(lines):
            next_line = lines[j]
            if next_line.strip() == '':
                block.append(next_line)
                j += 1
                continue
            if next_line.strip().startswith('#'):
                block.append(next_line)
                j += 1
                continue
            curr_indent = get_indent(next_line)
            if curr_indent <= indent:
                break
            block.append(next_line)
            j += 1
        
        obj_id = None
        if block[0].strip().startswith('- obj:'):
            for b_line in block:
                if b_line.strip().startswith('id: '):
                    obj_id = b_line.strip().split('id: ')[1].strip()
                    break
        elif block[0].strip().startswith('- slider:'):
            for b_line in block:
                if b_line.strip().startswith('id: '):
                    obj_id = b_line.strip().split('id: ')[1].strip()
                    break
        
        # Also let's extract the "Moisture sensors" block based on coordinates or content
        is_moisture = False
        if not obj_id:
            for b_line in block:
                if 'icon_plant_4' in b_line:
                    obj_id = 'moisture_sensors'
                    is_moisture = True
                    break
        
        if obj_id and len(block) > 40:
            comp_name = obj_id.replace('layer_', '').replace('card', 'card') + '.yaml'
            comp_path = os.path.join(ui_dir, comp_name)
            
            with open(comp_path, 'w', encoding='utf-8', newline='\n') as cf:
                # first line         - obj: -> obj:
                cf.write(block[0].lstrip()[2:])
                for b_line in block[1:]:
                    if b_line.strip() == '':
                        cf.write(b_line)
                    else:
                        # Find how many spaces at start, max 10 to strip
                        s_count = len(b_line) - len(b_line.lstrip())
                        strip_count = min(s_count, 10)
                        cf.write(b_line[strip_count:])
                        
            out_lines.append(f"        - !include ui_components/{comp_name}\n")
        else:
            out_lines.extend(block)
        
        i = j
        continue
    
    if in_widgets and line.startswith('    ') and not line.startswith('        '):
        if line.strip() != '' and not line.strip().startswith('#'):
            in_widgets = False
    
    out_lines.append(line)
    i += 1

with open(r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-lvgl.yaml', 'w', encoding='utf-8', newline='\n') as f:
    f.writelines(out_lines)

print("Proper split completed.")
