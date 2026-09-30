"""Rapports ASan/UBSan dans le journal de la tablette virtuelle (job sanitizers).

Les rapports d'UBSan partent sur la sortie d'erreur du programme et non dans le
`log_path` demandé (GCC, ASan et UBSan ensemble : constaté le 30/09/2026 sur un
témoin positif ; le premier bilan de l'audit disait « 0 rapport » à tort). Le job ne
donne donc aucun `log_path` : il envoie les sorties standard et d'erreur de la tablette
dans un seul fichier et le lit ici.

    python tools/sanitizers/rapports.py <journal>...          # code 1 si un rapport
    python tools/sanitizers/rapports.py --temoin <journal>    # code 1 si AUCUN rapport
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

MARQUE = re.compile(r"runtime error:|ERROR: AddressSanitizer|ERROR: LeakSanitizer")
PILE = re.compile(r"^\s+#\d+ ")
ADRESSES = re.compile(r"==\d+==|0x[0-9a-fA-F]+")


def extraire(texte: str, pile: int = 8) -> list[str]:
    """Chaque rapport : sa ligne d'erreur puis au plus `pile` lignes de pile d'appels."""
    lignes = texte.splitlines()
    blocs = []
    for i, ligne in enumerate(lignes):
        if not MARQUE.search(ligne):
            continue
        bloc = [ligne.rstrip()]
        j = i + 1
        # ASan : une ligne « READ of size… » avant la pile ; UBSan : la pile tout de suite.
        while j < len(lignes) and j <= i + 3 and not PILE.match(lignes[j]):
            j += 1
        while j < len(lignes) and PILE.match(lignes[j]):
            if len(bloc) <= pile:
                bloc.append(lignes[j].rstrip())
            j += 1
        blocs.append("\n".join(bloc))
    return blocs


def distincts(blocs: list[str]) -> list[str]:
    """Un rapport par ligne d'erreur (numéro de processus et adresses ignorés)."""
    vus, sortie = set(), []
    for bloc in blocs:
        cle = ADRESSES.sub("", bloc.splitlines()[0])
        if cle not in vus:
            vus.add(cle)
            sortie.append(bloc)
    return sortie


class Journal:
    """Un journal lu au fur et à mesure : les rapports apparus depuis la lecture précédente."""

    def __init__(self, chemin: Path):
        self.chemin = Path(chemin)
        self.position = 0

    def nouveaux(self) -> list[str]:
        if not self.chemin.exists():
            return []
        with open(self.chemin, "rb") as f:
            f.seek(self.position)
            brut = f.read()
            self.position = f.tell()
        return extraire(brut.decode("utf-8", errors="replace"))


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("journaux", nargs="+", type=Path)
    p.add_argument("--temoin", action="store_true",
                   help="témoin positif : échoue s'il n'y a AUCUN rapport (chaîne de détection cassée)")
    args = p.parse_args()
    blocs = []
    for chemin in args.journaux:
        blocs += extraire(chemin.read_text(encoding="utf-8", errors="replace"))
    uniques = distincts(blocs)
    print(f"{len(blocs)} rapport(s), {len(uniques)} distinct(s)")
    for bloc in uniques[:40]:
        print(bloc, end="\n\n")
    if args.temoin:
        return 0 if blocs else 1
    return 1 if blocs else 0


if __name__ == "__main__":
    sys.exit(main())
