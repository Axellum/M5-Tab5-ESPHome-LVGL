"""Images de la notice d'utilisation (docs/notice/), tirées du rendu hors tablette.

Les pages de la notice citent leurs images sous la forme
`images/notice/<écran>-<langue>.webp` : <écran> est un nom de capture du rendu
(tools/rendu/ecrans.py, workflow rendu-host.yml), <langue> fr ou en. Ce script les
reproduit depuis les captures d'un run du rendu, sans liste à tenir ailleurs : la page est
la seule source. En plus, l'accueil annoté (`accueil-annote-<langue>.webp`) : des numéros
posés aux points que le doigt virtuel du rendu touche (tools/rendu/ecrans.py) ou au centre
d'un widget de Tab5/paquets/tab5-lvgl.yaml, ceux de la légende de docs/notice/README.md.

Usage :
    gh run download <id> --name rendu-captures-fr --dir <dossier>/fr
    gh run download <id> --name rendu-captures-en --dir <dossier>/en
    python tools/site/images_notice.py --rendu <dossier>

<id> : un run vert de rendu-host.yml sur main (gh run list --workflow rendu-host.yml
--branch main). Captures françaises sans suffixe, anglaises en « -en ».
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

RACINE = Path(__file__).resolve().parents[2]
NOTICE = RACINE / "docs" / "notice"
IMAGES = RACINE / "docs" / "images" / "notice"
LVGL = RACINE / "Tab5" / "paquets" / "tab5-lvgl.yaml"
CITATION = re.compile(r"images/notice/([a-z0-9-]+)-(fr|en)\.webp")
ANNOTE = "accueil-annote"
# Capture de l'accueil sous les numéros : la première scène de la démo.
ACCUEIL = "1-journee-ensoleillee"
QUALITE_WEBP = 88

sys.path.insert(0, str(RACINE / "tools" / "rendu"))
import ecrans  # noqa: E402


def centre_du_widget(ident: str) -> tuple[int, int]:
    """Centre d'un widget placé en TOP_LEFT dans tab5-lvgl.yaml (x, y, width, height
    écrits après son id)."""
    texte = LVGL.read_text(encoding="utf-8")
    debut = texte.index(f"id: {ident}\n")
    bloc = texte[debut:debut + 600]
    valeurs = {cle: int(re.search(rf"\n\s+{cle}: (\d+)\n", bloc).group(1))
               for cle in ("x", "y", "width", "height")}
    return valeurs["x"] + valeurs["width"] // 2, valeurs["y"] + valeurs["height"] // 2


# Numéro de la légende → (point de la zone, décalage de la pastille). Le point vient du
# rendu (ce que le doigt virtuel touche) ou du YAML ; le décalage pose la pastille sur un
# coin de la zone, sans cacher ce qu'elle montre. Même ordre que la légende de
# docs/notice/README.md (tests/test_notice.py compare les deux).
REPERES = {
    1: (ecrans.DOMO, (-44, -62)),
    2: (ecrans.MICRO, (-26, -60)),
    3: (ecrans.DISCU, (-34, -62)),
    4: (centre_du_widget("btn_ok_nabu"), (-194, -38)),
    5: (ecrans.HORLOGE, (-190, -76)),
    6: (ecrans.BOUTON_HA, (-55, -40)),
    7: (ecrans.BOUTON_SYS, (-59, -40)),
    8: (ecrans.BOUTON_TV, (-63, -40)),
    9: (ecrans.SERRE, (84, 34)),
    10: (ecrans.CONSIGNE_CLIM, (-113, -24)),
    11: (ecrans.SOUS_HORLOGE, (-190, -27)),
    12: (centre_du_widget("central_card"), (-612, -36)),
    13: (ecrans.TUILES["chambre"], (-92, -70)),
    14: (ecrans.TUILE_J1_TEMP, (-72, -22)),
}


def annoter(capture: Path, sortie: Path) -> None:
    image = Image.open(capture).convert("RGB")
    dessin = ImageDraw.Draw(image)
    police = ImageFont.load_default(size=26)
    rayon = 21
    for numero, ((x, y), (dx, dy)) in REPERES.items():
        cx, cy = x + dx, y + dy
        dessin.ellipse((cx - rayon, cy - rayon, cx + rayon, cy + rayon),
                       fill=(255, 179, 0), outline=(20, 22, 26), width=3)
        dessin.text((cx, cy + 1), str(numero), fill=(20, 22, 26), font=police, anchor="mm")
    image.save(sortie, "WEBP", quality=QUALITE_WEBP, method=6)


def capture_de(rendu: Path, ecran: str, langue: str) -> Path:
    nom = f"{ecran}.png" if langue == "fr" else f"{ecran}-en.png"
    chemin = rendu / langue / nom
    if not chemin.exists():
        raise SystemExit(f"capture absente : {chemin} (nom d'écran de tools/rendu/ecrans.py ?)")
    return chemin


def citees() -> set[tuple[str, str]]:
    """(écran, langue) de chaque image citée par une page de la notice."""
    return {m.groups() for page in sorted(NOTICE.glob("*.md"))
            for m in CITATION.finditer(page.read_text(encoding="utf-8"))}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--rendu", type=Path, required=True,
                        help="dossier avec fr/ et en/ (artefacts rendu-captures-fr et -en)")
    args = parser.parse_args()
    IMAGES.mkdir(parents=True, exist_ok=True)
    for ecran, langue in sorted(citees()):
        sortie = IMAGES / f"{ecran}-{langue}.webp"
        if ecran == ANNOTE:
            annoter(capture_de(args.rendu, ACCUEIL, langue), sortie)
        else:
            Image.open(capture_de(args.rendu, ecran, langue)).convert("RGB").save(
                sortie, "WEBP", quality=QUALITE_WEBP, method=6)
        print(f"{sortie.relative_to(RACINE)} : {sortie.stat().st_size // 1024} Ko")
    return 0


if __name__ == "__main__":
    sys.exit(main())
