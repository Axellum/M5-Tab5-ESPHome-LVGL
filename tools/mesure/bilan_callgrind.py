"""Branche essai/mesure-demarrage-avant-apres (JAMAIS MERGÉE) : instructions par bande, avant / après.

    python tools/mesure/bilan_callgrind.py resultats/callgrind_avant resultats/callgrind_apres

Chaque fichier cg.out.* de callgrind est une bande (CALLGRIND_DUMP_STATS_AT remet les
compteurs à zéro) : son nom est dans « desc: Trigger: Client Request: », son total dans
« summary: ». Imprime un tableau Markdown et la somme des bandes de setup().
"""

from __future__ import annotations

import sys
from pathlib import Path

SETUP = ["construction", "bande_1000_900", "bande_800_600", "bande_400", "bande_395_250",
         "bande_220_200", "bande_100", "bande_99_m100"]
DESC = "desc: Trigger: Client Request: "


def bandes(dossier: Path) -> dict[str, int]:
    res: dict[str, int] = {}
    for f in sorted(dossier.glob("cg.out.*")):
        nom, total = None, None
        for ligne in f.read_text(encoding="utf-8", errors="replace").splitlines():
            if ligne.startswith(DESC):
                nom = ligne.removeprefix(DESC).strip()
            elif ligne.startswith(("summary:", "totals:")) and total is None:
                total = int(ligne.split()[1])
        if nom and total is not None:
            res[nom] = total
    return res


def main() -> int:
    avant, apres = bandes(Path(sys.argv[1])), bandes(Path(sys.argv[2]))
    print("| Bande | Avant (M instr.) | Après (M instr.) |")
    print("|---|---:|---:|")
    for nom in [*SETUP, "boucle_1s"]:
        a, b = avant.get(nom), apres.get(nom)
        print(f"| {nom} | {a / 1e6:,.0f} | {b / 1e6:,.0f} |" if a is not None and b is not None
              else f"| {nom} | {a} | {b} |")
    sa = sum(avant.get(n, 0) for n in SETUP)
    sb = sum(apres.get(n, 0) for n in SETUP)
    print(f"| **setup() entier** | **{sa / 1e6:,.0f}** | **{sb / 1e6:,.0f}** |")
    if sa:
        print(f"\nÉcart : {100 * (sb - sa) / sa:+.1f} %")
    return 0


if __name__ == "__main__":
    sys.exit(main())
