import os
import sys

search_path = r'e:\AuxFilsDesIdees\00ProjetTab\.esphome\build\tab5-ha-hmi\src'

results = []
for root, dirs, files in os.walk(search_path):
    for f in files:
        if f.endswith('.cpp') or f.endswith('.h'):
            path = os.path.join(root, f)
            with open(path, 'r', encoding='utf-8', errors='ignore') as fp:
                lines = fp.readlines()
                for i, line in enumerate(lines):
                    if 'font_meteo_main' in line:
                        results.append(f"{f}:{i+1}: {line.strip()}")
                        
for r in results[:20]:
    print(r)
