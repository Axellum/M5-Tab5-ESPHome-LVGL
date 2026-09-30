"""Audit qualité : écrit une variante de tab5-rendu-host.yaml qui branche tools/audit/pio_drapeaux.py.

    python tools/audit/variante.py tab5-audit.yaml

La variante est identique à la tablette virtuelle, sauf le script PlatformIO ajouté
sous `esphome: platformio_options: extra_scripts` (une liste : ESPHome l'ajoute à son
propre `pre:ccache.py` au lieu de le remplacer, components/host/__init__.py). Les
drapeaux eux-mêmes viennent de AUDIT_CCFLAGS / AUDIT_LINKFLAGS au moment du build.
Écrite à la racine du dépôt pour que tous les chemins relatifs restent justes.
"""

from __future__ import annotations

import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
SOURCE = RACINE / "tab5-rendu-host.yaml"
SCRIPT = RACINE / "tools" / "audit" / "pio_drapeaux.py"


def variante(texte: str) -> str:
    lignes = texte.splitlines(keepends=True)
    try:
        i = next(n for n, ligne in enumerate(lignes) if ligne.rstrip() == "esphome:")
    except StopIteration:
        raise SystemExit("bloc « esphome: » introuvable dans tab5-rendu-host.yaml") from None
    ajout = (
        "  platformio_options:\n"
        "    extra_scripts:\n"
        f"      - pre:{SCRIPT.as_posix()}\n"
    )
    return "".join(lignes[: i + 1]) + ajout + "".join(lignes[i + 1:])


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    cible = RACINE / sys.argv[1]
    cible.write_text(variante(SOURCE.read_text(encoding="utf-8")), encoding="utf-8")
    print(f"Variante écrite : {cible.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
