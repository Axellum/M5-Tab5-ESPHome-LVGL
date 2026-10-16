"""Branche essai/mesure-demarrage-avant-apres (JAMAIS MERGÉE) : variante chronométrée de la tablette virtuelle.

    python tools/mesure/variante_demarrage.py tab5-mesure.yaml

Ajoute à tab5-rendu-host.yaml : l'en-tête Tab5/rendu/mesure_demarrage.h, des entrées
on_boot qui ne font que marquer (mêmes priorités que la chronologie de la tablette, plus
-200 après la bande -100 et une marque 1 s après la fin de setup()), et le script
PlatformIO des sanitizers pour les drapeaux (TAB5_SAN_CCFLAGS / TAB5_SAN_LINKFLAGS).
"""

from __future__ import annotations

import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
SOURCE = RACINE / "tab5-rendu-host.yaml"
SCRIPT = RACINE / "tools" / "sanitizers" / "pio_drapeaux.py"

# (priorité, nom de la bande qui SE TERMINE à cette marque)
MARQUES = [
    (2000, "construction"),
    (801, "bande_1000_900"),
    (500, "bande_800_600"),
    (399, "bande_400"),
    (249, "bande_395_250"),
    (199, "bande_220_200"),
    (99, "bande_100"),
    (-200, "bande_99_m100"),
]


def inserer_apres(lignes: list[str], ligne: str, ajout: str) -> None:
    i = next(n for n, x in enumerate(lignes) if x.rstrip() == ligne)
    lignes.insert(i + 1, ajout)


def variante(texte: str) -> str:
    lignes = texte.splitlines(keepends=True)
    inserer_apres(lignes, "esphome:", (
        "  platformio_options:\n"
        "    extra_scripts:\n"
        f"      - pre:{SCRIPT.as_posix()}\n"
    ))
    inserer_apres(lignes, "  includes:", "    - Tab5/rendu/mesure_demarrage.h\n")
    entrees = []
    for prio, nom in MARQUES:
        entrees.append(f"    - priority: {prio}\n      then:\n        - lambda: 'mesure::bande(\"{nom}\");'\n")
        if prio == -200:
            entrees.append("        - delay: 1s\n        - lambda: 'mesure::bande(\"boucle_1s\");'\n")
    inserer_apres(lignes, "  on_boot:", "".join(entrees))
    return "".join(lignes)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    cible = RACINE / sys.argv[1]
    cible.write_text(variante(SOURCE.read_text(encoding="utf-8")), encoding="utf-8")
    print(f"Variante écrite : {cible.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
