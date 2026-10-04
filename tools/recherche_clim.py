# -*- coding: utf-8 -*-
"""Recherche en LECTURE SEULE des fichiers candidats de l'écran de climatisation.

Parcourt le dépôt (extensions source) et cherche dans le CONTENU les mots-clés
'climatisation', 'optimiste', 'température' (insensible à la casse, accents
normalisés). Exclut le bruit : .claude/worktrees, archives, __pycache__,
.pytest_cache, .git. Aucun fichier n'est modifié.
"""
import os
import re
import sys
import time
import unicodedata

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # 00ProjetTab
EXTS = (".py", ".yaml", ".yml", ".json", ".js", ".ts", ".cpp", ".c", ".h")
MOTS = ["climatisation", "optimiste", "temperature"]  # 'température' normalisé
EXCLUS = {".claude", "archives", "__pycache__", ".pytest_cache", ".git", ".qoder"}
BORE_S = 30.0


def normalise(s: str) -> str:
    """Minuscules + suppression des accents (température -> temperature)."""
    s = unicodedata.normalize("NFD", s.lower())
    return "".join(c for c in s if unicodedata.category(c) != "Mn")


def main() -> int:
    t0 = time.monotonic()
    hits = {}  # chemin relatif -> {mots-clés trouvés}
    for dp, dn, fn in os.walk(RACINE):
        # Coupe les répertoires de bruit (et les worktrees).
        dn[:] = [d for d in dn if d not in EXCLUS]
        for f in fn:
            if not f.lower().endswith(EXTS):
                continue
            p = os.path.join(dp, f)
            try:
                if os.path.getsize(p) > 2_000_000:  # garde-fou taille
                    continue
                with open(p, encoding="utf-8", errors="ignore") as fh:
                    contenu = normalise(fh.read())
            except OSError:
                continue
            trouves = {m for m in MOTS if m in contenu}
            if trouves:
                rel = os.path.relpath(p, RACINE).replace(os.sep, "/")
                hits[rel] = sorted(trouves)
            if time.monotonic() - t0 > BORE_S:
                print("BORNE 30 s DÉPASSÉE — arrêt partiel", file=sys.stderr)
                break
    for rel in sorted(hits):
        print(f"{rel}  [{', '.join(hits[rel])}]")
    print(f"-- {len(hits)} fichier(s) candidat(s) en {time.monotonic() - t0:.1f} s --")
    return 0


if __name__ == "__main__":
    sys.exit(main())
