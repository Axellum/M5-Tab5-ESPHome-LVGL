# -*- coding: utf-8 -*-
"""tools/rendu/comparer.py — Compare les captures du rendu à leurs références (lot 7).

Les références sont les PNG de docs/images/rendu/, montrés aussi dans docs/screens.md.
Le rendu est déterministe (deux runs donnent les mêmes pixels, vérifié le 27/09/2026) :
une différence veut donc dire que l'écran a changé. Pour chaque capture :
- identique : rien ;
- différente : une image `diff/<nom>.png` (la capture, les pixels changés en magenta)
  et la zone concernée ;
- sans référence, ou référence sans capture : signalé.

Informatif (choix d'Axel, 27/09/2026) : le résumé part dans le rapport du run
(GITHUB_STEP_SUMMARY) et des avertissements dans la PR, sans faire échouer le job.
`--strict` rend le code de sortie non nul en cas d'écart.

Accepter un changement d'écran : tools/rendu/maj_references.py.
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

REFERENCES = Path(__file__).resolve().parent.parent.parent / "docs" / "images" / "rendu"


def comparer(captures: Path, references: Path) -> list[str]:
    """Écarts, un par ligne de résumé ; écrit les images de différence."""
    from PIL import Image, ImageChops

    ecarts = []
    faites = {p.name: p for p in captures.glob("*.png")}
    attendues = {p.name: p for p in references.glob("*.png")}
    for nom in sorted(faites.keys() - attendues.keys()):
        ecarts.append(f"`{nom}` : nouvelle capture, sans référence")
    for nom in sorted(attendues.keys() - faites.keys()):
        ecarts.append(f"`{nom}` : référence sans capture (scène disparue ?)")
    for nom in sorted(faites.keys() & attendues.keys()):
        with Image.open(faites[nom]) as a, Image.open(attendues[nom]) as b:
            a, b = a.convert("RGB"), b.convert("RGB")
            if a.size != b.size:
                ecarts.append(f"`{nom}` : taille {a.size} au lieu de {b.size}")
                continue
            masque = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v else 0)
            zone = masque.getbbox()
            if zone is None:
                continue
            n = sum(masque.histogram()[255:])
            dossier = captures / "diff"
            dossier.mkdir(exist_ok=True)
            vue = a.copy()
            vue.paste((255, 0, 255), mask=masque)
            vue.save(dossier / nom)
            ecarts.append(f"`{nom}` : {n} pixels changés, zone {zone}")
    return ecarts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--captures", type=Path, required=True, help="dossier des PNG du rendu")
    parser.add_argument("--references", type=Path, default=REFERENCES)
    parser.add_argument("--strict", action="store_true", help="code de sortie 1 en cas d'écart")
    args = parser.parse_args()

    ecarts = comparer(args.captures, args.references)
    if ecarts:
        resume = ["### Rendu : l'écran a changé", "",
                  "Images avant/après dans l'artefact `rendu-captures` (dossier `diff/`). Si c'est "
                  "voulu : `python tools/rendu/maj_references.py --run <id du run>`.", ""]
        resume += [f"- {e}" for e in ecarts]
        for e in ecarts:
            print(f"::warning title=Rendu::{e}")
    else:
        resume = ["### Rendu : identique aux références", ""]
    texte = "\n".join(resume) + "\n"
    print(texte)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as f:
            f.write(texte)
    return 1 if (ecarts and args.strict) else 0


if __name__ == "__main__":
    sys.exit(main())
