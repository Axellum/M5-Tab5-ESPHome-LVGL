import sys
import os
from PyQt5.QtWidgets import QApplication
from PyQt5.QtSvg import QSvgRenderer
from PyQt5.QtGui import QImage, QPainter, QColor
from PyQt5.QtCore import QSize
from PIL import Image

app = QApplication(sys.argv)

def process_svg(svg_path, out_path, bg_color, target_size=None):
    try:
        renderer = QSvgRenderer(svg_path)
        if not renderer.isValid():
            print(f"Invalid SVG: {svg_path}")
            return
            
        size = renderer.defaultSize()
        if target_size:
            size = QSize(target_size, target_size)
    
        img = QImage(size, QImage.Format_ARGB32)
        img.fill(QColor(0, 0, 0, 0)) # transparent background
        
        painter = QPainter(img)
        # For high quality scaling
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.HighQualityAntialiasing)
        renderer.render(painter)
        painter.end()
        
        # Convert QImage to Pillow Image
        # QImage format is ARGB32, memory layout is B, G, R, A (Little Endian)
        ptr = img.bits()
        ptr.setsize(img.byteCount())
        data = bytes(ptr)
        
        # QImage format_ARGB32 has raw bytes BGRA
        # Wait, PyQt5 might have a slightly different memory layout depending on endianness.
        # Better safe: save as PNG via QImage, then open via PIL!
        temp_png = out_path + '.tmp.png'
        img.save(temp_png, "PNG")
        
        # Now use our pixel-perfect mathematical scrambler!
        pil_img = Image.open(temp_png).convert('RGBA')
        r, g, b, a = pil_img.split()
        r_data, g_data, b_data, a_data = list(r.getdata()), list(g.getdata()), list(b.getdata()), list(a.getdata())
        out_r, out_g, out_b = [], [], []
        br, bg, bb = bg_color
        
        for i in range(len(a_data)):
            alpha = a_data[i] / 255.0
            if alpha == 0:
                tr, tg, tb = br, bg, bb
            else:
                tr = int(r_data[i] * alpha + br * (1 - alpha))
                tg = int(g_data[i] * alpha + bg * (1 - alpha))
                tb = int(b_data[i] * alpha + bb * (1 - alpha))
            
            # ESPHome compiled RGB565 word construction
            tr5, tg6, tb5 = tr >> 3, tg >> 2, tb >> 3
            tw = (tr5 << 11) | (tg6 << 5) | tb5
            # Emulate LVGL byte_order: little_endian swap!
            ew = ((tw & 0xFF) << 8) | (tw >> 8)
            
            # Re-convert ESPHome word back to 24-bit PNG representation 
            # so ESPHome compiler produces exactly EW!
            pr5, pg6, pb5 = (ew >> 11) & 0x1F, (ew >> 5) & 0x3F, ew & 0x1F
            out_r.append((pr5 * 255) // 31)
            out_g.append((pg6 * 255) // 63)
            out_b.append((pb5 * 255) // 31)
            
        r.putdata(out_r)
        g.putdata(out_g)
        b.putdata(out_b)
        # Strip alpha forever to force LV_IMG_CF_TRUE_COLOR in compiling
        res = Image.merge('RGB', (r, g, b))
        res.save(out_path, "PNG")
        
        os.remove(temp_png)
    except Exception as e:
        print(f"Error on {svg_path}: {e}")

tasks = []
os.makedirs('icons_2_png_card_fixed', exist_ok=True)
os.makedirs('icons_2_png_main_scrambled', exist_ok=True)

bg_card = (35, 38, 46)
bg_main = (50, 54, 66)

for f in os.listdir('icons_2'):
    if f.endswith('.svg'):
        svg_path = os.path.join('icons_2', f)
        base = f.replace('.svg', '.png')
        tasks.append((svg_path, os.path.join('icons_2_png_card_fixed', base), bg_main, 150))
        tasks.append((svg_path, os.path.join('icons_2_png_main_scrambled', base), bg_main, 270))

import concurrent.futures
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    for t in tasks:
        executor.submit(process_svg, *t)
        
print("SVG_RASTERIZATION_AND_SCRAMBLING_SUCCESS")
