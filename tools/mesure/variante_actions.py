"""Branche essai/mesure-invalidation (JAMAIS MERGÉE) : tablette virtuelle mesurée par fenêtres d'actions.

    python tools/mesure/variante_actions.py tab5-mesure.yaml

Ajoute à tab5-rendu-host.yaml : le script PlatformIO des sanitizers (drapeaux de
TAB5_SAN_CCFLAGS, ici -g), l'en-tête Tab5/rendu/mesure_actions.h et une entrée on_boot
qui le branche (mesure::brancher() : fichier de commande MESURE_CMD lu toutes les 20 ms).
"""

from __future__ import annotations

import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RACINE / "tools" / "sanitizers"))

from variante import SOURCE, variante as variante_sanitizers  # noqa: E402


def inserer_apres(lignes: list[str], ligne: str, ajout: str) -> None:
    trouves = [n for n, x in enumerate(lignes) if x.rstrip() == ligne]
    if len(trouves) != 1:
        raise SystemExit(f"Ancre « {ligne.strip()} » trouvée {len(trouves)} fois (1 attendue)")
    lignes.insert(trouves[0] + 1, ajout)


def variante(texte: str) -> str:
    lignes = variante_sanitizers(texte).splitlines(keepends=True)
    # En dernier des includes : après les en-têtes remplacés de la plateforme host.
    inserer_apres(lignes, "    - Tab5/rendu/rendu_mesure.h", "    - Tab5/rendu/mesure_actions.h\n")
    inserer_apres(lignes, "  on_boot:",
                  "    - priority: -200\n      then:\n        - lambda: 'mesure::brancher();'\n")
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
