import sys
path = 'e:/AuxFilsDesIdees/00ProjetTab/Tab5/tab5-api-logic.yaml'
with open(path, 'r', encoding='utf-8') as f:
    text = f.read()

if 'lv_obj_t* temp_lbl = NULL;' in text:
    text = text.replace('lv_obj_t* temp_lbl = NULL;', 'lv_obj_t* temp_lbl = NULL;\n              lv_obj_t* l1 = NULL;\n              lv_obj_t* l2 = NULL;')

with open(path, 'w', encoding='utf-8') as f:
    f.write(text)
