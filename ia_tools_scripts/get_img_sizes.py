import struct
def get_png_size(file_path):
    with open(file_path, 'rb') as f:
        f.read(16)
        return struct.unpack('>LL', f.read(8))
print("Main:", get_png_size("e:/AuxFilsDesIdees/00ProjetTab/Tab5/icons_2_png_main_scrambled/rainy-5.png"))
print("Card:", get_png_size("e:/AuxFilsDesIdees/00ProjetTab/Tab5/icons_2_png_card_fixed/rainy-5.png"))
