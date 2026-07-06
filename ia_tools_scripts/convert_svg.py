import os
from svglib.svglib import svg2rlg
from reportlab.graphics import renderPM

input_dir = 'icons_2'
output_dir = 'icons_2_png'

if not os.path.exists(output_dir):
    os.makedirs(output_dir)

width = 150
height = 150

for filename in os.listdir(input_dir):
    if filename.endswith('.svg'):
        svg_path = os.path.join(input_dir, filename)
        png_path = os.path.join(output_dir, filename.replace('.svg', '.png'))
        
        try:
            # Load SVG
            drawing = svg2rlg(svg_path)
            
            # Scale
            scale_x = width / drawing.width
            scale_y = height / drawing.height
            drawing.scale(scale_x, scale_y)
            drawing.width = width
            drawing.height = height
            
            # Save to PNG with transparent background
            # renderPM transparent bg setup:
            renderPM.drawToFile(drawing, png_path, fmt='PNG', bg=0x00000000, configPM='transparent')
            print(f"Converted {filename} to {png_path}")
        except Exception as e:
            print(f"Failed to convert {filename}: {e}")
