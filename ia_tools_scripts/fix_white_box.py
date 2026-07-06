import re

filepath = 'tab5-lvgl.yaml'
with open(filepath, 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Ajouter le transparent_style dans les style_definitions si non présent
if 'id: transparent_style' not in code:
    style_def = """    - id: transparent_style
      bg_opa: TRANSP
      border_width: 0
      pad_all: 0

"""
    code = code.replace('  style_definitions:\n', '  style_definitions:\n' + style_def)

# 2. Appliquer transparent_style au rain_bar_container et alerts_container
code = re.sub(r'(id: rain_bar_container\n)', r'\1                styles: transparent_style\n', code)
code = re.sub(r'(id: alerts_container\n)', r'\1                styles: transparent_style\n', code)

# 3. Appliquer transparent_style aux containers FLEX min/max.
# Actuellement, ils ressemblent à :
#                         - obj:
#                             layout:
#                               type: FLEX
#                               flex_flow: ROW
# Remplace "- obj:\n                            layout:" par "- obj:\n                            styles: transparent_style\n                            layout:"
# Mais uniquement pour ceux qui ont `width:  250` etc (les min/max block)
code = re.sub(r'(- obj:\n\s*)(layout:\n\s*type: FLEX\n\s*flex_flow: ROW\n\s*flex_align_main: SPACE_EVENLY\n\s*align: BOTTOM_MID)',
              r'\1styles: transparent_style\n                            \2', code)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(code)
