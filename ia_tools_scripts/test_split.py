import os

path = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-lvgl.yaml'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

ui_dir = r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\ui_components'
os.makedirs(ui_dir, exist_ok=True)

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
        # We found a top-level widget item
        block = [line]
        indent = get_indent(line) # should be 8
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
        
        # Check if the block is an "obj" with an ID and is large enough
        is_obj = False
        obj_id = None
        if block[0].strip().startswith('- obj:'):
            is_obj = True
            for b_line in block:
                if b_line.strip().startswith('id: '):
                    obj_id = b_line.strip().split('id: ')[1].strip()
                    break
        
        # Also let's check for forecast sliders
        if block[0].strip().startswith('- slider:'):
            for b_line in block:
                if b_line.strip().startswith('id: '):
                    obj_id = b_line.strip().split('id: ')[1].strip()
                    break
        
        if obj_id and len(block) > 40:
            # We save it as a separate component
            comp_name = obj_id.replace('layer_', '').replace('card', 'card') + '.yaml'
            comp_path = os.path.join(ui_dir, comp_name)
            
            # The included file in ESPHome must have the exact type at root, 
            # e.g., "obj:" instead of "- obj:". But wait, if we use !include inside a list,
            # ESPHome supports returning a list or dictionary. 
            # If we just put obj: at the root of the included file (stripping 10 spaces of indent),
            # it becomes a mapping, which !include will insert.
            # Example: 
            # block[0] is         - obj:\n
            # we need it to be obj:\n -> indent=10 to strip
            
            with open(comp_path, 'w', encoding='utf-8', newline='\n') as cf:
                # First line is         - obj: => we change it to just obj:
                # What if it's - slider:? Same thing.
                first_line = block[0].lstrip()[2:] # remove "- "
                cf.write(first_line)
                for b_line in block[1:]:
                    if b_line.strip() == '':
                        cf.write(b_line)
                    else:
                        cf.write(b_line[10:]) # remove 10 spaces of indentation
                        
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

with open(r'e:\AuxFilsDesIdees\00ProjetTab\Tab5\tab5-lvgl-split.yaml', 'w', encoding='utf-8', newline='\n') as f:
    f.writelines(out_lines)

print("Split completed.")
