"""Rapports ASan/UBSan dans le journal de la tablette virtuelle (job sanitizers).

Les rapports d'UBSan partent sur la sortie d'erreur du programme et non dans le
`log_path` demandé (GCC, ASan et UBSan ensemble : constaté le 30/09/2026 sur un
témoin positif ; le premier bilan de l'audit disait « 0 rapport » à tort). Le job ne
donne donc aucun `log_path` : il envoie les sorties standard et d'erreur de la tablette
dans un seul fichier et le lit ici.

    python tools/sanitizers/rapports.py <journal>...          # code 1 si un rapport inconnu
    python tools/sanitizers/rapports.py --temoin <journal>    # code 1 si AUCUN rapport

Rapports connus (CONNUS) : un défaut amont, bénin et impossible à éviter de notre côté
sans changer le rendu, est toujours affiché (« connu, amont ») mais ne fait pas échouer
le job. Clé = la ligne d'erreur exacte sans la valeur (fichier:ligne:colonne et genre) :
si la bibliothèque change, la ligne bouge, le rapport redevient inconnu et le job
rouge, ce qui oblige à revérifier. Chaque entrée a sa justification et son lien amont
(tests/test_sanitizers.py). Une entrée = une décision, jamais un rapport de notre code.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

MARQUE = re.compile(r"runtime error:|ERROR: AddressSanitizer|ERROR: LeakSanitizer")
PILE = re.compile(r"^\s+#\d+ ")
ADRESSES = re.compile(r"==\d+==|0x[0-9a-fA-F]+")

# Ligne d'erreur exacte, sans la valeur (le rapport doit s'arrêter à la clé, puis
# éventuellement un nombre) → justification, lien amont compris.
CONNUS: dict[str, str] = {
    "lvgl/src/widgets/roller/lv_roller.c:598:46: runtime error: left shift of negative value": (
        "LVGL 9.5.0, draw_main du rouleau (lv_roller) : label_y_prop, l'écart entre le texte "
        "et la ligne du milieu, est négatif dès que l'option choisie n'est pas la première "
        "(toujours en mode infini), puis décalé de 14 bits. Indéfini en C, mais GCC (firmware "
        "et tablette virtuelle) ne le traite pas comme tel (« Integers implementation » de "
        "son manuel) : le résultat est la valeur × 16384, sans débordement (mode infini "
        "plafonné à 15 pages, texte de 54 000 px au plus contre 131 072). Inchangé sur la "
        "branche master de LVGL au 09/10/2026. Popup Réveil, 09/10/2026. "
        "https://github.com/lvgl/lvgl/blob/v9.5.0/src/widgets/roller/lv_roller.c#L598"
    ),
}
_FIN_CONNUE = re.compile(r"\s*-?\d*\s*$")


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


def connu(bloc: str) -> str | None:
    """La clé de CONNUS dont ce rapport est l'occurrence, sinon None : la ligne d'erreur
    contient la clé, suivie au plus d'une valeur numérique."""
    ligne = bloc.splitlines()[0] if bloc else ""
    for cle in CONNUS:
        i = ligne.find(cle)
        if i >= 0 and _FIN_CONNUE.fullmatch(ligne[i + len(cle):]):
            return cle
    return None


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
    inconnus = [b for b in blocs if connu(b) is None]
    print(f"{len(blocs)} rapport(s), {len(uniques)} distinct(s), {len(blocs) - len(inconnus)} connu(s) (amont)")
    for bloc in uniques[:40]:
        cle = connu(bloc)
        if cle is not None:
            print(f"connu, amont : {CONNUS[cle]}")
        print(bloc, end="\n\n")
    if args.temoin:
        return 0 if blocs else 1
    return 1 if inconnus else 0


if __name__ == "__main__":
    sys.exit(main())
