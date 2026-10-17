"""Écrit une variante de tab5-rendu-host.yaml qui branche tools/sanitizers/pio_drapeaux.py.

    python tools/sanitizers/variante.py tab5-sanitizers.yaml

La variante est identique à la tablette virtuelle, sauf le script PlatformIO ajouté
sous `esphome: platformio_options: extra_scripts` (une liste : ESPHome l'ajoute à son
propre `pre:ccache.py` au lieu de le remplacer, components/host/__init__.py). Les
drapeaux eux-mêmes viennent de TAB5_SAN_CCFLAGS / TAB5_SAN_LINKFLAGS au moment du
build. Écrite à la racine du dépôt pour que tous les chemins relatifs restent justes ;
le nom de l'appareil ne change pas (build dans .esphome/build/tab5-rendu).
"""

from __future__ import annotations

import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
SOURCE = RACINE / "tab5-rendu-host.yaml"
SCRIPT = RACINE / "tools" / "sanitizers" / "pio_drapeaux.py"


def variante(texte: str, script: Path = SCRIPT) -> str:
    lignes = texte.splitlines(keepends=True)
    try:
        i = next(n for n, ligne in enumerate(lignes) if ligne.rstrip() == "esphome:")
    except StopIteration:
        raise SystemExit("bloc « esphome: » introuvable dans tab5-rendu-host.yaml") from None
    script_ligne = f"      - pre:{script.as_posix()}\n"
    # Un `platformio_options:` déjà là (les drapeaux de LVGL, 17/10/2026) : le script va
    # dessous, une seconde clé du même nom serait un doublon refusé par ESPHome.
    for n in range(i + 1, len(lignes)):
        ligne = lignes[n]
        if ligne.strip() and not ligne.startswith(" "):
            break   # fin du bloc esphome:
        if ligne.rstrip() == "  platformio_options:":
            ajout = "    extra_scripts:\n" + script_ligne
            return "".join(lignes[: n + 1]) + ajout + "".join(lignes[n + 1:])
    ajout = "  platformio_options:\n    extra_scripts:\n" + script_ligne
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
