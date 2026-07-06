import glob
from PIL import Image

def process_folder(folder, bg_hex, target_size=None):
    bg_color = tuple(int(bg_hex[i:i+2], 16) for i in (0, 2, 4))
    for f in glob.glob(f"{folder}/*.png"):
        try:
            img = Image.open(f).convert("RGBA")
            if target_size:
                # Resize with Lanczos
                img = img.resize(target_size, Image.Resampling.LANCZOS)
            # Create a solid background image
            bg = Image.new("RGBA", img.size, bg_color + (255,))
            # Composite
            out = Image.alpha_composite(bg, img).convert("RGB")
            out.save(f)
        except Exception as e:
            print(f"Failed {f}: {e}")

# Process main icons: 225x225, bg 323642
process_folder('icons_2_png_main', '323642', (225, 225))

# Process card icons: 100x100, bg 23262E
process_folder('icons_2_png_card', '23262E', (100, 100))

# Process smaller Png icons: 130x130 (for scale), bg 323642
process_folder('Png', '323642', (130, 130))

print("Done blending and resizing!")
