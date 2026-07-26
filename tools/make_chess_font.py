#!/usr/bin/env python3
"""
[AI-CONTEXT]
@file tools/make_chess_font.py
@role Regenere Tab5/ChessPieces.ttf : le sous-ensemble de 12 glyphes utilise par
      le jeu « Roi Noir » pour dessiner les figurines d'echecs.

@architecture_constraint Meme demarche que Tab5/IconeMeteo.ttf : on n'embarque
      dans le firmware que les glyphes reellement affiches. La police source
      (DejaVu Sans) fait 757 Ko ; le sous-ensemble en fait ~17.

      Les 12 glyphes sont le bloc Unicode « Chess Symbols » :
        U+2654..2659  pieces CREUSES (dites blanches)  -> servent de CONTOUR
        U+265A..265F  pieces PLEINES (dites noires)    -> servent de CORPS
      chess_game.cpp superpose les deux pour une piece blanche (corps ivoire +
      contour sombre) et n'utilise que le plein pour une piece noire.

@ai_instruction La licence Bitstream Vera reserve les noms « Bitstream »,
      « Vera » et « DejaVu » : un derive DOIT etre renomme. D'ou NEW_FAMILY.
      Ne pas retirer les name IDs 0/13/14 (copyright et licence) du sous-ensemble.
      Voir Tab5/ChessPieces.LICENSE.txt.

Usage (depuis 00ProjetTab/) :
    python tools/make_chess_font.py [chemin/vers/DejaVuSans.ttf]
"""

import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont

DEFAULT_SRC = r"C:\Windows\Fonts\DejaVuSans.ttf"
DST = os.path.join("Tab5", "ChessPieces.ttf")
CODEPOINTS = list(range(0x2654, 0x2660))
NEW_FAMILY = "RoiNoir Chess"
NEW_PS = "RoiNoirChess"


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    if not os.path.isfile(src):
        print(f"Police source introuvable : {src}")
        print("Passer le chemin en argument, ou installer DejaVu Sans.")
        return 1

    opt = subset.Options()
    opt.layout_features = []
    # On CONSERVE les name IDs 0 (copyright), 13 (licence) et 14 (URL) : la
    # licence Bitstream Vera impose que la notice accompagne toute copie.
    opt.name_IDs = [0, 1, 2, 4, 5, 6, 13, 14]
    opt.name_legacy = True
    opt.notdef_outline = True
    opt.recalc_bounds = True
    opt.glyph_names = True
    opt.drop_tables += ["DSIG"]

    font = subset.load_font(src, opt)
    s = subset.Subsetter(options=opt)
    s.populate(unicodes=CODEPOINTS)
    s.subset(font)

    for rec in font["name"].names:
        if rec.nameID in (1, 4):
            rec.string = NEW_FAMILY
        elif rec.nameID == 6:
            rec.string = NEW_PS
        elif rec.nameID == 2:
            rec.string = "Regular"

    subset.save_font(font, DST, opt)

    chk = TTFont(DST)
    cm = chk.getBestCmap()
    missing = [f"U+{c:04X}" for c in CODEPOINTS if c not in cm]
    print(f"ecrit {DST} — {os.path.getsize(DST)} octets")
    print(f"glyphes : {len(CODEPOINTS) - len(missing)}/{len(CODEPOINTS)}"
          f" | manquants : {missing or 'aucun'}")
    print(f"famille : {chk['name'].getDebugName(1)} | PostScript : {chk['name'].getDebugName(6)}")
    if missing:
        return 1

    # Metriques utiles a chess_game.cpp : taille de police retenue, encre, et
    # decalage optique PIECE_DY. PIL est le meme rasteriseur que celui d'ESPHome.
    try:
        from PIL import ImageFont
        f = ImageFont.truetype(DST, 80)
        asc, desc = f.getmetrics()
        bbs = [f.getbbox(chr(c)) for c in CODEPOINTS]
        w = max(b[2] - b[0] for b in bbs)
        h = max(b[3] - b[1] for b in bbs)
        top = min(b[1] for b in bbs)
        bot = max(b[3] for b in bbs)
        dy = (asc + desc) / 2 - (top + bot) / 2
        print(f"taille 80 : encre {w}x{h} px (case 84), hauteur de ligne {asc + desc},"
              f" PIECE_DY attendu = {dy:+.0f}")
    except ImportError:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
