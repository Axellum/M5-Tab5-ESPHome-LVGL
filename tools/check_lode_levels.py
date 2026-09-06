#!/usr/bin/env python3
"""Garde-fou des niveaux de « Coureur d'Or » (clone Lode Runner du Tab5).

Les 10 maps vivent dans Tab5/lode_game.cpp sous forme de tableaux
`static const char* const MAPn[GRID_H]`. Ce script les relit LA (source unique de
verite, aucune duplication) et rejoue le MEME modele de deplacement que le C++ :

    passable(c)          : ni brique intacte, ni beton
    supported(c)         : echelle, barre, ou appui solide dessous
    can_step(a -> b)     : lateral si appui ; montee seulement sur echelle ;
                           descente toujours autorisee si la case est passable
    arete de creusement  : depuis une case avec appui SOLIDE, on peut rejoindre
                           (x+d, y+1) si (x+d, y) est passable et (x+d, y+1) une
                           brique — c'est exactement ce que fait try_dig()

Il verifie pour chaque niveau :
  1. dimensions, un seul 'P', au plus 4 'G', au moins 1 '$'
  2. l'echelle de sortie 'S' atteint la rangee 0
  3. tout l'or est atteignable depuis le depart (sortie encore inactive)
  4. depuis le depart ET depuis CHAQUE lingot, la rangee 0 reste joignable une
     fois la sortie activee (pas de cul-de-sac apres le dernier lingot)
  5. le nombre d'objets LVGL du damier tient dans MAX_TILEOBJ (runs de beton
     fusionnes, comme build_tiles())
  6. le nombre de lingots tient dans MAX_GOLD

Usage : python tools/check_lode_levels.py   (aussi lance par `pytest`, tests/test_guards.py)
Sortie : 0 si tout est conforme, 1 sinon (liste des ecarts sur stdout).

Historique : vivait dans le workspace prive (`scripts/`) jusqu'au 06/09/2026 —
la section Lode de Tab5/README.md parlait d'un « garde-fou Python hors depot ».
"""

from __future__ import annotations

import re
import sys
from collections import deque
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "Tab5" / "lode_game.cpp"

W, H = 30, 16
MAX_TILEOBJ, MAX_GOLD, MAX_GUARDS = 340, 40, 4

MAP_RE = re.compile(
    r"static const char\* const (MAP\d+)\[GRID_H\]\s*=\s*\{(.*?)\};",
    re.DOTALL,
)
STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


# ---------------------------------------------------------------------------
# Lecture des maps depuis le C++
# ---------------------------------------------------------------------------
def load_maps(text: str) -> list[tuple[str, list[str]]]:
    out = []
    for m in MAP_RE.finditer(text):
        rows = [s.encode().decode("unicode_escape") for s in STR_RE.findall(m.group(2))]
        out.append((m.group(1), [r.ljust(W)[:W] for r in rows]))
    return out


# ---------------------------------------------------------------------------
# Modele de deplacement — miroir de lode_game.cpp (section 7)
# ---------------------------------------------------------------------------
def at(g, x, y):
    if x < 0 or x >= W or y < 0 or y >= H:
        return "@"  # hors carte = beton : le monde est ferme
    return g[y][x]


def passable(g, x, y):
    return at(g, x, y) not in "#@"


def is_ladder(g, x, y, exit_on):
    c = at(g, x, y)
    return c == "H" or (c == "S" and exit_on)


def supported(g, x, y, exit_on):
    if is_ladder(g, x, y, exit_on) or at(g, x, y) == "-":
        return True
    b = at(g, x, y + 1)
    return b in "#@H" or (b == "S" and exit_on)


def can_step(g, fx, fy, tx, ty, exit_on):
    if not passable(g, tx, ty):
        return False
    if ty == fy:
        return supported(g, fx, fy, exit_on)
    if ty == fy - 1:
        return is_ladder(g, fx, fy, exit_on)
    return True


def dig_targets(g, fx, fy, exit_on):
    """Cases atteintes en creusant : on ne creuse que debout sur un sol solide."""
    if is_ladder(g, fx, fy, exit_on) or at(g, fx, fy) == "-":
        return []
    if at(g, fx, fy + 1) not in "#@":
        return []
    out = []
    for d in (-1, 1):
        if passable(g, fx + d, fy) and at(g, fx + d, fy + 1) == "#":
            out.append((fx + d, fy + 1))
    return out


def reach(g, start, exit_on, allow_dig=True):
    seen = {start}
    q = deque([start])
    while q:
        x, y = q.popleft()
        digs = dig_targets(g, x, y, exit_on) if allow_dig else []
        for tx, ty in [(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)] + digs:
            if (tx, ty) in seen or not (0 <= tx < W and 0 <= ty < H):
                continue
            if (tx, ty) in digs or can_step(g, x, y, tx, ty, exit_on):
                seen.add((tx, ty))
                q.append((tx, ty))
    return seen


def cells(g, ch):
    return [(x, y) for y in range(H) for x in range(W) if g[y][x] == ch]


def drawn_tiles(g):
    """1 objet par brique/echelle/barre/sortie + 1 par run horizontal de beton."""
    n = 0
    for y in range(H):
        run = False
        for x in range(W):
            c = g[y][x]
            if c == "@":
                if not run:
                    n += 1
                    run = True
            else:
                run = False
                if c in "#H-S":
                    n += 1
    return n


# ---------------------------------------------------------------------------
def check(name: str, g: list[str]) -> list[str]:
    errs: list[str] = []
    if len(g) != H:
        return [f"{len(g)} rangees au lieu de {H}"]
    # Les lignes courtes sont completees par du vide, exactement comme
    # load_level() cote C++ ; on ne veut pas planter sur une map malformee.
    g = [r.ljust(W)[:W] for r in g]

    P, G, gold, S = cells(g, "P"), cells(g, "G"), cells(g, "$"), cells(g, "S")
    if len(P) != 1:
        errs.append(f"{len(P)} depart(s) 'P' (il en faut exactement 1)")
    if not gold:
        errs.append("aucun lingot")
    if len(gold) > MAX_GOLD:
        errs.append(f"{len(gold)} lingots > MAX_GOLD ({MAX_GOLD})")
    if len(G) > MAX_GUARDS:
        errs.append(f"{len(G)} gardes > MAX_GUARDS ({MAX_GUARDS})")
    if not any(y == 0 for _, y in S):
        errs.append("l'echelle de sortie 'S' n'atteint pas la rangee 0")

    nt = drawn_tiles(g)
    if nt > MAX_TILEOBJ:
        errs.append(f"{nt} tuiles dessinees > MAX_TILEOBJ ({MAX_TILEOBJ})")

    if len(P) == 1:
        playable = reach(g, P[0], exit_on=False)
        miss = [c for c in gold if c not in playable]
        if miss:
            errs.append(f"lingot(s) inatteignable(s) : {miss}")
        for c in [P[0]] + gold:
            if not any(y == 0 for _, y in reach(g, c, exit_on=True)):
                errs.append(f"sortie injoignable depuis {c}")
                break

    return errs


def main() -> int:
    if not SRC.is_file():
        print(f"introuvable : {SRC}")
        return 1

    maps = load_maps(SRC.read_text(encoding="utf-8"))
    if not maps:
        print("aucune map trouvee dans lode_game.cpp")
        return 1

    bad = 0
    for name, g in maps:
        errs = check(name, g)
        ngold, nguard = len(cells(g, "$")), len(cells(g, "G"))
        nodig = 0
        P = cells(g, "P")
        if P:
            r = reach(g, P[0], exit_on=False, allow_dig=False)
            nodig = sum(1 for c in cells(g, "$") if c not in r)
        status = "OK " if not errs else "KO "
        print(
            f"{status}{name:<6} or={ngold:<3} gardes={nguard}  tuiles={drawn_tiles(g):<4}"
            f" or-derriere-dig={nodig}"
        )
        for e in errs:
            print(f"      - {e}")
            bad += 1

    print()
    if bad:
        print(f"*** {bad} probleme(s) — corriger les maps de lode_game.cpp ***")
        return 1
    print(f"{len(maps)} niveaux verifies : tout est jouable.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
