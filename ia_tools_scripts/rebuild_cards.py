import os
import glob
import sys
from PyQt5.QtWidgets import QApplication
from PyQt5.QtSvg import QSvgRenderer
from PyQt5.QtGui import QImage, QPainter, QColor
from PyQt5.QtCore import Qt

def swap_bytes_color(r, g, b):
    # Convertir en RGB565 classique
    orig16 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    # Echanger les octets (Big Endian)
    swapped16 = ((orig16 & 0xFF) << 8) | (orig16 >> 8)
    # Reconvertir en RGB888 pour que ESPHome gnre le swapped16 en flash
    swR = (swapped16 >> 11) & 0x1F
    swR = (swR * 255) // 31
    swG = (swapped16 >> 5) & 0x3F
    swG = (swG * 255) // 63
    swB = swapped16 & 0x1F
    swB = (swB * 255) // 31
    return QColor(swR, swG, swB)

def main():
    app = QApplication(sys.argv)
    out_folder = "icons_2_png_card_scrambled"
    os.makedirs(out_folder, exist_ok=True)
    
    width, height = 150, 150
    svg_files = glob.glob("icons_2/*.svg")
    bg_color = QColor("#323642")
    
    for svg_file in svg_files:
        img = QImage(width, height, QImage.Format_RGB32)
        img.fill(bg_color)
        
        p = QPainter(img)
        p.setRenderHint(QPainter.Antialiasing)
        renderer = QSvgRenderer(svg_file)
        renderer.render(p)
        p.end()
        
        # Le pixel de Chroma Key originel en haut a gauche (0,0) = Vert pur
        img.setPixelColor(0, 0, QColor(0, 255, 0))
        
        for y in range(height):
            for x in range(width):
                c = img.pixelColor(x, y)
                img.setPixelColor(x, y, swap_bytes_color(c.red(), c.green(), c.blue()))
                
        filename = os.path.basename(svg_file).replace(".svg", ".png")
        out_path = os.path.join(out_folder, filename)
        img.save(out_path, "PNG")

if __name__ == "__main__":
    main()
