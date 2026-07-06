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
    # Reconvertir en RGB888 pour que ESPHome génère le swapped16 en flash
    swR = (swapped16 >> 11) & 0x1F
    swR = (swR * 255) // 31
    swG = (swapped16 >> 5) & 0x3F
    swG = (swG * 255) // 63
    swB = swapped16 & 0x1F
    swB = (swB * 255) // 31
    return QColor(swR, swG, swB)

def main():
    app = QApplication(sys.argv)
    os.makedirs("icons_2_png_main", exist_ok=True)
    os.makedirs("icons_2_png_card", exist_ok=True)
    
    width, height = 250, 250
    svg_files = glob.glob("icons_2/*.svg")
    
    backgrounds = {
        "icons_2_png_main": QColor("#323642"),  # Pour l'écran principal
        "icons_2_png_card": QColor("#23262E")   # Pour les cartes prévisionnelles
    }
    
    for svg_file in svg_files:
        for out_folder, bg_color in backgrounds.items():
            # 1. Préparer une image de destination avec la couleur de fond corrigée
            img = QImage(width, height, QImage.Format_RGB32)
            img.fill(bg_color)
            
            # 2. Rendre le SVG directement : puisqu'il est recadré par l'utilisateur,
            #    QSvgRenderer va automatiquement l'étendre jusqu'aux bords de 250x250.
            p = QPainter(img)
            p.setRenderHint(QPainter.Antialiasing)
            renderer = QSvgRenderer(svg_file)
            renderer.render(p)
            p.end()
            
            # 3. Le pixel de Chroma Key originel en haut à gauche (0,0) = Vert pur
            img.setPixelColor(0, 0, QColor(0, 255, 0))
            
            # 4. Application du filtre matériel pour contrecarrer l'inversion de l'ESP32-P4
            for y in range(height):
                for x in range(width):
                    c = img.pixelColor(x, y)
                    img.setPixelColor(x, y, swap_bytes_color(c.red(), c.green(), c.blue()))
                    
            filename = os.path.basename(svg_file).replace(".svg", ".png")
            out_path = os.path.join(out_folder, filename)
            img.save(out_path, "PNG")
            print(f"Generated clean SVG render: {out_path}")

if __name__ == "__main__":
    main()
