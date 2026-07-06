from PIL import Image

def get_alpha_bbox(path):
    try:
        img = Image.open(path).convert('RGBA')
        return img.split()[3].getbbox()
    except Exception as e:
        return str(e)

print("cloudy original:", get_alpha_bbox('icons_2_png_main/cloudy.png'))
print("pluis original:", get_alpha_bbox('Png/pluis.png'))
