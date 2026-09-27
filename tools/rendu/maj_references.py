# -*- coding: utf-8 -*-
"""tools/rendu/maj_references.py — Accepte les captures d'un run comme nouvelles références (lot 7).

Quand l'écran change pour de bon, le job « Rendu hors tablette » le signale
(tools/rendu/comparer.py). Ce script télécharge les captures de ce run (artefact
`rendu-captures`, avec la CLI `gh`) et remplace les PNG de docs/images/rendu/, qui
servent à la fois de références et de galerie (docs/screens.md). Il n'y a plus qu'à
relire les images et à les committer.

Usage :
    python tools/rendu/maj_references.py --run 36318894541
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REFERENCES = Path(__file__).resolve().parent.parent.parent / "docs" / "images" / "rendu"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--run", required=True, help="identifiant du run GitHub Actions")
    parser.add_argument("--depot", default="Axellum/M5-Tab5-ESPHome-LVGL")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(["gh", "run", "download", args.run, "--repo", args.depot,
                        "--name", "rendu-captures", "--dir", tmp], check=True)
        pngs = sorted(Path(tmp).glob("*.png"))
        if not pngs:
            print("Aucune capture PNG dans l'artefact de ce run.", file=sys.stderr)
            return 1
        REFERENCES.mkdir(parents=True, exist_ok=True)
        for ancienne in REFERENCES.glob("*.png"):
            ancienne.unlink()
        for png in pngs:
            shutil.copy2(png, REFERENCES / png.name)
            print(f"{png.name}")
    print(f"{len(pngs)} références dans {REFERENCES} : relire les images, puis committer.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
