#!/usr/bin/env python3
"""[AI-CONTEXT] Extrait le moteur des dames de Tab5/draughts_game.cpp pour le compiler sur PC.

Le générateur de coups des dames (namespace Draughts::Engine : pos_init, gen_moves,
apply_move, refresh_endgame…) est pur, mais il vit dans le même fichier que l'interface
LVGL et la sauvegarde NVS du jeu. Pour tester le VRAI C++ sans toucher au firmware, ce
script recopie tel quel le début du fichier, de « namespace Draughts { » jusqu'à
« }  // namespace Engine », ferme le namespace, et pose une directive #line : une erreur
de compilation pointe la vraie ligne de Tab5/draughts_game.cpp.

tools/test_draughts_engine.cpp l'inclut (job `python` de la CI, esphome-tab5.yml) :
    python tools/hote/extraire_moteur_dames.py "$RUNNER_TEMP/hote/draughts_moteur_extrait.inc"

tests/test_moteurs_hote.py vérifie à chaque pytest, sans g++, que les deux bornes existent
une seule fois et que le bloc extrait ne touche ni à LVGL ni aux préférences.

Usage :
    python tools/hote/extraire_moteur_dames.py SORTIE
"""
from __future__ import annotations

import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
SOURCE = RACINE / "Tab5" / "draughts_game.cpp"
DEBUT = "namespace Draughts {"
FIN = "}  // namespace Engine"


def extraire(source: str, nom: str = "Tab5/draughts_game.cpp") -> str:
    """Le bloc pur du moteur, prêt à inclure ; ValueError si une borne manque ou se répète."""
    lignes = source.splitlines()
    for borne in (DEBUT, FIN):
        n = sum(1 for ligne in lignes if ligne.rstrip() == borne)
        if n != 1:
            raise ValueError(f"« {borne} » trouvé {n} fois dans {nom} (attendu : une fois)")
    debut = next(i for i, ligne in enumerate(lignes) if ligne.rstrip() == DEBUT)
    fin = next(i for i, ligne in enumerate(lignes) if ligne.rstrip() == FIN)
    if fin <= debut:
        raise ValueError(f"« {FIN} » avant « {DEBUT} » dans {nom}")
    bloc = lignes[debut:fin + 1]
    return "\n".join([f'#line {debut + 1} "{nom}"', *bloc, "}  // namespace Draughts", ""])


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print(__doc__)
        return 2
    sortie = Path(argv[0])
    sortie.parent.mkdir(parents=True, exist_ok=True)
    sortie.write_text(extraire(SOURCE.read_text(encoding="utf-8")), encoding="utf-8")
    print(f"Moteur des dames extrait dans {sortie}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
