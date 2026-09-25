#!/usr/bin/env python3
"""tools/cartographie_counts.py — comptes de lignes de CARTOGRAPHIE_TAB5.md.

[AI-CONTEXT] La cartographie donne la taille de chaque fichier (`| `fichier` | N |`)
pour qu'un agent sache où il met les pieds. Relevés à la main, ces comptes
dérivaient : audit du 25/09/2026, 22 sur 49 faux, dont `tab5-api-logic.yaml`
annoncé à 332 lignes pour 508. Ce script est la seule façon de les tenir à jour.

    python tools/cartographie_counts.py           # vérifie (tolérance 20 %), exit 1 si dérive
    python tools/cartographie_counts.py --write   # réécrit tous les comptes au `wc -l` exact

`fichier.h/.cpp` = somme des deux. Le nom est cherché parmi les fichiers suivis
par git (nom exact ou suffixe `/nom`) : un nom introuvable ou ambigu est une
erreur, pas un silence. pytest joue la vérification (tests/test_guards.py).
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CARTO = REPO / "CARTOGRAPHIE_TAB5.md"
TOLERANCE = 0.20          # au-delà, le compte ment sur l'ordre de grandeur
ROW = re.compile(r"^(\|\s*`([^`]+)`\s*\|\s*)([\d   ]+?)(\s*\|)")


def tracked() -> list[str]:
    out = subprocess.run(["git", "ls-files"], cwd=REPO, capture_output=True,
                         text=True, check=True).stdout
    return [f for f in out.splitlines() if f]


def resolve(name: str, files: list[str]) -> list[str]:
    """`tab5_registry.h/.cpp` → les deux fichiers ; sinon un seul, unique."""
    stem, sep, exts = name.partition("/.")
    names = [name] if not sep else [stem] + [
        stem.rsplit(".", 1)[0] + "." + e for e in exts.split("/.")]
    found = []
    for n in names:
        cands = [f for f in files if f == n or f.endswith("/" + n)]
        if len(cands) != 1:
            raise LookupError(f"{n} : {'introuvable' if not cands else 'ambigu ' + str(cands)}")
        found.append(cands[0])
    return found


def line_count(path: str) -> int:
    with open(REPO / path, encoding="utf-8", errors="replace") as f:
        return sum(1 for _ in f)


def scan(text: str, files: list[str]):
    """(n° de ligne, nom, compte annoncé, compte réel) pour chaque ligne de tableau."""
    rows = []
    for n, line in enumerate(text.splitlines(), 1):
        m = ROW.match(line)
        if not m:
            continue
        announced = int(re.sub(r"\D", "", m.group(3)))
        real = sum(line_count(p) for p in resolve(m.group(2), files))
        rows.append((n, m.group(2), announced, real))
    return rows


def drifts(rows, tolerance: float = TOLERANCE):
    return [r for r in rows if abs(r[3] - r[2]) > tolerance * max(r[3], 1)]


def write(text: str, files: list[str]) -> str:
    out = []
    for line in text.splitlines(keepends=True):
        m = ROW.match(line)
        if m:
            real = sum(line_count(p) for p in resolve(m.group(2), files))
            line = f"{m.group(1)}{real}{m.group(4)}{line[m.end():]}"
        out.append(line)
    return "".join(out)


def main(argv: list[str]) -> int:
    files = tracked()
    text = CARTO.read_text(encoding="utf-8")
    if "--write" in argv:
        new = write(text, files)
        CARTO.write_bytes(new.encode("utf-8"))
        changed = sum(1 for r in scan(new, files) if r[2] != r[3])
        print(f"✅ comptes réécrits ({len(scan(new, files))} lignes, {changed} écart restant)")
        return 0
    rows = scan(text, files)
    bad = drifts(rows)
    for n, name, announced, real in bad:
        print(f"CARTOGRAPHIE_TAB5.md:{n} | {name} : {announced} annoncées, {real} réelles")
    if bad:
        print(f"⚠️ {len(bad)} compte(s) à plus de {TOLERANCE:.0%} — "
              "lancer `python tools/cartographie_counts.py --write`")
        return 1
    print(f"✅ {len(rows)} comptes de lignes dans la tolérance de {TOLERANCE:.0%}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
