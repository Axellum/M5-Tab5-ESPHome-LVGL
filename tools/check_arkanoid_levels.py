#!/usr/bin/env python3
"""Garde-fou des niveaux d'« Arcanoïde » (Tab5/jeux/arkanoid_game.cpp).

Les 8 niveaux sont des tableaux `static const uint8_t LVLn[BRICK_ROWS][BRICK_COLS]`.
Une rangée ou une valeur oubliée compile quand même (le C++ complète avec des 0) :
le niveau change sans bruit (audit du 25/09/2026, §7). Ce script relit le fichier
réel (source unique, rien n'est recopié) et vérifie :

  1. chaque niveau a exactement BRICK_ROWS rangées de BRICK_COLS valeurs ;
  2. chaque valeur est un BrickType connu (0 vide, 1 normale, 2 renforcée,
     3 indestructible, 4 bonus) ;
  3. chaque niveau a au moins une brique destructible (sinon `bricks_alive`
     vaut 0 dès le chargement et le niveau se termine tout seul) ;
  4. toute brique destructible est atteignable par la balle : on part des cases
     de la rangée du bas et on avance d'une case à la fois (haut, bas, gauche,
     droite) à travers tout ce qui n'est pas indestructible — une brique emmurée
     par des indestructibles rendrait le niveau impossible à finir ;
  5. `LEVELS` liste LVL1 à LVLn dans l'ordre, `LEVEL_NAMES` a autant de noms, et
     le test de fin de partie `gs->level >= N` vaut bien le dernier index.

Usage : python tools/check_arkanoid_levels.py   (aussi lancé par `pytest`, tests/test_guards.py)
Sortie : 0 si tout est conforme, 1 sinon (liste des écarts sur stdout).
"""

from __future__ import annotations

import re
import sys
from collections import deque
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
GAME = REPO / "Tab5" / "jeux" / "arkanoid_game.cpp"

EMPTY, INDESTRUCT = 0, 3
TYPES = {0, 1, 2, 3, 4}


def _const(text: str, name: str) -> int | None:
    m = re.search(r"static constexpr int\s+" + name + r"\s*=\s*(\d+)\s*;", text)
    return int(m.group(1)) if m else None


def _levels(text: str) -> dict[str, list[list[int]]]:
    levels = {}
    for name, body in re.findall(
            r"static const uint8_t (LVL\d+)\[BRICK_ROWS\]\[BRICK_COLS\]\s*=\s*\{(.*?)\n\};", text, re.S):
        rows = re.findall(r"\{([^{}]*)\}", body)
        levels[name] = [[int(v) for v in re.findall(r"\d+", r)] for r in rows]
    return levels


def _reachable(grid: list[list[int]]) -> set[tuple[int, int]]:
    h, w = len(grid), len(grid[0])
    seen = {(h - 1, c) for c in range(w) if grid[h - 1][c] != INDESTRUCT}
    todo = deque(seen)
    while todo:
        r, c = todo.popleft()
        for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nr, nc = r + dr, c + dc
            if 0 <= nr < h and 0 <= nc < w and (nr, nc) not in seen and grid[nr][nc] != INDESTRUCT:
                seen.add((nr, nc))
                todo.append((nr, nc))
    return seen


def scan(text: str | None = None) -> list[str]:
    text = GAME.read_text(encoding="utf-8") if text is None else text
    problems: list[str] = []
    rows_n, cols_n = _const(text, "BRICK_ROWS"), _const(text, "BRICK_COLS")
    if not rows_n or not cols_n:
        return ["BRICK_ROWS / BRICK_COLS introuvables"]
    levels = _levels(text)
    if not levels:
        return ["aucun tableau LVLn[BRICK_ROWS][BRICK_COLS] trouvé"]
    for name, grid in levels.items():
        if len(grid) != rows_n:
            problems.append(f"{name} : {len(grid)} rangées, attendu {rows_n}")
        bad = [i + 1 for i, row in enumerate(grid) if len(row) != cols_n]
        if bad:
            problems.append(f"{name} : rangée(s) {bad} sans exactement {cols_n} valeurs")
        if len(grid) != rows_n or bad:
            continue
        inconnus = sorted({v for row in grid for v in row} - TYPES)
        if inconnus:
            problems.append(f"{name} : valeur(s) inconnue(s) {inconnus} (BrickType 0 à 4)")
        cibles = {(r, c) for r in range(rows_n) for c in range(cols_n) if grid[r][c] not in (EMPTY, INDESTRUCT)}
        if not cibles:
            problems.append(f"{name} : aucune brique destructible, le niveau se finirait au chargement")
        emmurees = sorted(cibles - _reachable(grid))
        if emmurees:
            problems.append(f"{name} : brique(s) inaccessibles derrière des indestructibles "
                            f"(rangée, colonne comptées depuis 1) : {[(r + 1, c + 1) for r, c in emmurees]}")
    m = re.search(r"static const uint8_t\* LEVELS\[(\d+)\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        problems.append("tableau LEVELS introuvable")
        return problems
    n = int(m.group(1))
    ordre = re.findall(r"\b(LVL\d+)\b", m.group(2))
    if ordre != [f"LVL{i}" for i in range(1, n + 1)] or len(levels) != n:
        problems.append(f"LEVELS[{n}] = {ordre}, niveaux définis : {sorted(levels)} (attendu LVL1 à LVL{n} dans l'ordre)")
    noms = re.search(r"static const char\* LEVEL_NAMES\[(\d+)\]\s*=\s*\{(.*?)\};", text, re.S)
    if not noms or int(noms.group(1)) != n or len(re.findall(r'"[^"]+"', noms.group(2))) != n:
        problems.append(f"LEVEL_NAMES doit avoir {n} noms, comme LEVELS")
    fin = re.search(r"if \(gs->level >= (\d+)\)", text)
    if not fin or int(fin.group(1)) != n - 1:
        problems.append(f"fin de partie : `gs->level >= {fin.group(1) if fin else '?'}` au lieu de >= {n - 1} (dernier niveau)")
    return problems


def main() -> int:
    problems = scan()
    for p in problems:
        print(p)
    if not problems:
        print("Niveaux d'Arcanoïde conformes.")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
