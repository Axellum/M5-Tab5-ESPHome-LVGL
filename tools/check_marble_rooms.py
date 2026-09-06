#!/usr/bin/env python3
"""Verifie que toutes les salles de « Fil d'Or » sont jouables de bout en bout.

Lit les specs DIRECTEMENT dans Tab5/marble_game.cpp (pas de duplication : si le
contenu change, le test suit). Pour chaque salle :

  1. la position de depart laisse-t-elle tenir la bille (rayon 11) ?
  2. la sortie est-elle atteignable depuis le depart ?
  3. chaque pickup / rune est-il atteignable ?
  4. les scies laissent-elles un passage a au moins une phase de leur course ?

Methode : grille d'occupation du CENTRE de la bille. Un mur (x,y,w,h) interdit au
centre la zone du rectangle dilate de BALL_R. On utilise la dilatation par
rectangle (et non le vrai arrondi de Minkowski aux coins) : elle bloque un poil
PLUS que la realite, donc un chemin trouve ici existe forcement en jeu.

Usage : python tools/check_marble_rooms.py   (aussi lance par `pytest`, tests/test_guards.py)
Dependance : numpy (requirements-dev.txt).

Historique : vivait dans le workspace prive (`scripts/`) jusqu'au 06/09/2026 —
cite par la section Marble de Tab5/README.md sans qu'un clone puisse le lancer.
"""
from __future__ import annotations
import re
import sys
from collections import deque
from pathlib import Path

import numpy as np

CPP = Path(__file__).resolve().parent.parent / "Tab5" / "marble_game.cpp"

FW, FH, BALL_R = 1280, 672, 11
STEP = 2  # resolution de la grille (px)

SOLID = {"K_WALL"}                       # seuls les murs bloquent
PICKUPS = {"K_GOLD", "K_SHIELD", "K_MAGNET", "K_BRAKE", "K_DASH", "K_RUNE", "K_CHEST"}
MOBILE = {"K_SAW", "K_ORB", "K_HUNTER"}  # (x,y) = centre du trajet


def parse():
    txt = CPP.read_text(encoding="utf-8", errors="replace")
    rooms = {}
    for m in re.finditer(r"static const Spec (R\d)\[\]\s*=\s*\{(.*?)\n\};", txt, re.S):
        name, body = m.group(1), m.group(2)
        specs = []
        for line in body.splitlines():
            line = line.split("//")[0].strip()
            mm = re.match(r"\{\s*(K_\w+)\s*,(.*?)\}\s*,?\s*$", line)
            if not mm:
                continue
            nums = [int(v) for v in re.findall(r"-?\d+", mm.group(2))]
            if len(nums) < 6:
                continue
            specs.append((mm.group(1), *nums[:6]))
        rooms[name] = specs

    order = []
    for m in re.finditer(
        r'\{"([^"]+)",\s*"[^"]*",\s*"[^"]*",\s*(-?\d+),\s*(-?\d+),\s*(R\d),', txt
    ):
        order.append((m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)))
    return rooms, order


def rect_of(kind, x, y, w, h):
    """Rect (x0,y0,x1,y1) reellement occupe. Les mobiles sont centres sur (x,y)."""
    if kind in MOBILE:
        return (x - w // 2, y - h // 2, x - w // 2 + w, y - h // 2 + h)
    return (x, y, x + w, y + h)


def build_free(specs):
    """Grille booleenne : True = le centre de la bille peut s'y trouver."""
    gw, gh = FW // STEP, FH // STEP
    xs = np.arange(gw) * STEP + STEP // 2
    ys = np.arange(gh) * STEP + STEP // 2
    free = np.ones((gh, gw), dtype=bool)

    # Bords du terrain
    free[:, xs < BALL_R] = False
    free[:, xs > FW - BALL_R] = False
    free[ys < BALL_R, :] = False
    free[ys > FH - BALL_R, :] = False

    for kind, x, y, w, h, _a, _b in specs:
        if kind not in SOLID:
            continue
        x0, y0, x1, y1 = rect_of(kind, x, y, w, h)
        cx = (xs >= x0 - BALL_R) & (xs <= x1 + BALL_R)
        cy = (ys >= y0 - BALL_R) & (ys <= y1 + BALL_R)
        free[np.ix_(cy, cx)] = False
    return free, xs, ys


def bfs(free, start_cell):
    gh, gw = free.shape
    seen = np.zeros_like(free)
    sy, sx = start_cell
    if not free[sy, sx]:
        return seen, False
    seen[sy, sx] = True
    q = deque([(sy, sx)])
    while q:
        cy, cx = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = cy + dy, cx + dx
            if 0 <= ny < gh and 0 <= nx < gw and free[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True
                q.append((ny, nx))
    return seen, True


def touch_mask(rect, xs, ys):
    """Cellules ou le CENTRE de la bille touche le rect (dist < BALL_R)."""
    x0, y0, x1, y1 = rect
    px = np.clip(xs[None, :], x0, x1)
    py = np.clip(ys[:, None], y0, y1)
    dx = xs[None, :] - px
    dy = ys[:, None] - py
    return (dx * dx + dy * dy) < (BALL_R * BALL_R)


def check_saw(specs, kind, x, y, w, h, a, b):
    """Une scie doit laisser un passage a au moins une extremite de sa course."""
    problems = []
    horiz = w >= h  # barre horizontale => oscille verticalement
    for off in (-a, 0, a):
        if horiz:
            r = (x - w // 2, y - h // 2 + off, x - w // 2 + w, y - h // 2 + off + h)
        else:
            r = (x - w // 2 + off, y - h // 2, x - w // 2 + off + w, y - h // 2 + h)
        problems.append(r)
    return problems


def main():
    rooms, order = parse()
    if not order:
        print("[KO] impossible de parser les salles")
        return 1

    fail = 0
    for idx, (name, sx, sy, key) in enumerate(order, 1):
        specs = rooms[key]
        free, xs, ys = build_free(specs)
        cell = (min(sy // STEP, FH // STEP - 1), min(sx // STEP, FW // STEP - 1))
        seen, ok_start = bfs(free, cell)

        print(f"\n=== Salle {idx} — {name} ({key}, {len(specs)} entites) ===")
        if not ok_start:
            print(f"  [KO] depart ({sx},{sy}) est DANS un mur / hors zone libre")
            fail += 1
            continue
        print(f"  [OK] depart ({sx},{sy}) libre — {seen.sum()} cellules atteignables")

        for kind, x, y, w, h, a, b in specs:
            if kind not in PICKUPS and kind != "K_EXIT":
                continue
            r = rect_of(kind, x, y, w, h)
            reachable = (touch_mask(r, xs, ys) & seen).any()
            exists = (touch_mask(r, xs, ys) & free).any()
            tag = "SORTIE" if kind == "K_EXIT" else kind[2:]
            if reachable:
                print(f"  [OK] {tag:7s} ({x},{y}) atteignable")
            else:
                why = "aucune position valide autour" if not exists else "isole du depart"
                print(f"  [KO] {tag:7s} ({x},{y}) INATTEIGNABLE — {why}")
                fail += 1

        # Scies : verifier qu'un passage existe a une phase de la course
        for kind, x, y, w, h, a, b in specs:
            if kind != "K_SAW":
                continue
            blocked_all = True
            for r in check_saw(specs, kind, x, y, w, h, a, b):
                tmp = free.copy()
                x0, y0, x1, y1 = r
                cx = (xs >= x0 - BALL_R) & (xs <= x1 + BALL_R)
                cy = (ys >= y0 - BALL_R) & (ys <= y1 + BALL_R)
                tmp[np.ix_(cy, cx)] = False
                s2, ok2 = bfs(tmp, cell)
                # la sortie reste-t-elle atteignable a cette phase ?
                ex = [sp for sp in specs if sp[0] == "K_EXIT"]
                if ok2 and ex:
                    er = rect_of(ex[0][0], *ex[0][1:5])
                    if (touch_mask(er, xs, ys) & s2).any():
                        blocked_all = False
                        break
            if blocked_all:
                print(f"  [KO] SCIE   ({x},{y}) bloque le passage a TOUTES ses phases")
                fail += 1
            else:
                print(f"  [OK] SCIE   ({x},{y}) laisse passer a au moins une phase")

    print("\n" + "=" * 60)
    if fail:
        print(f"[KO] {fail} probleme(s) de parcours")
    else:
        print("[OK] toutes les salles sont traversables, tout le loot est atteignable")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
