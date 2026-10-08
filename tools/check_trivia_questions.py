#!/usr/bin/env python3
"""Garde-fou de la banque de questions de « Trial Poursuite » (Tab5/jeux/trivia_questions.h).

Le `static_assert` du fichier ne vérifie que la somme des `#define` : une ligne
supprimée dans un tableau `QUESTIONS_xxx[TRIVIA_Q_xxx]` compile quand même, le C++
complète avec une entrée nulle (pointeurs `nullptr`), et la partie plante ou
affiche une question vide le jour où le tirage tombe dessus (audit du 25/09/2026,
§7). Ce script relit le fichier réel (source unique, rien n'est recopié) et vérifie :

  1. chaque tableau a exactement le nombre d'entrées de son `#define`, et chaque
     ligne `{…}` du tableau est une entrée lisible (catégorie, difficulté, 5 textes) ;
  2. le champ catégorie vaut l'index du tableau dans `TRIVIA_BANKS` (0 à 5), dans
     le même ordre que `TRIVIA_BANK_SIZES` ;
  3. difficulté 0, 1 ou 2, et chaque catégorie a au moins une question de chaque ;
  4. aucun texte vide ; la bonne réponse diffère des trois leurres, et les leurres
     entre eux (sans tenir compte de la casse) ;
  5. énoncé ≤ 140 caractères, réponses ≤ 48 (limites écrites dans la struct) ;
  6. aucun énoncé en double dans toute la banque.

Usage : python tools/check_trivia_questions.py   (aussi lancé par `pytest`, tests/test_guards.py)
Sortie : 0 si tout est conforme, 1 sinon (liste des écarts sur stdout).
"""

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BANK = REPO / "Tab5" / "jeux" / "trivia_questions.h"

MAX_Q = 140
MAX_A = 48

_S = r'"((?:[^"\\]|\\.)*)"'
_ENTRY = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*" + r"\s*,\s*".join([_S] * 5) + r"\s*\}")
_ARRAY = re.compile(r"static const TriviaQuestion (QUESTIONS_\w+)\[(\w+)\]\s*=\s*\{(.*?)\n\};", re.S)
_DEFINE = re.compile(r"#define\s+(TRIVIA_Q_\w+)\s+(\d+)\b")


def _list(text: str, name: str) -> list[str]:
    m = re.search(name + r"\[TRIVIA_NCAT\]\s*=\s*\{(.*?)\};", text, re.S)
    return re.findall(r"\w+", m.group(1)) if m else []


def scan(text: str | None = None) -> list[str]:
    text = BANK.read_text(encoding="utf-8") if text is None else text
    problems: list[str] = []
    defines = {k: int(v) for k, v in _DEFINE.findall(text)}
    arrays = {name: (size, body) for name, size, body in _ARRAY.findall(text)}
    banks = _list(text, "TRIVIA_BANKS")
    sizes = _list(text, "TRIVIA_BANK_SIZES")
    if not banks or len(banks) != len(sizes):
        return [f"TRIVIA_BANKS ({len(banks)}) et TRIVIA_BANK_SIZES ({len(sizes)}) illisibles ou de tailles différentes"]
    enonces: list[str] = []
    for cat, (name, size_name) in enumerate(zip(banks, sizes)):
        if name not in arrays:
            problems.append(f"{name} : tableau introuvable")
            continue
        declared, body = arrays[name]
        if declared != size_name:
            problems.append(f"{name} : dimensionné par {declared}, mais TRIVIA_BANK_SIZES[{cat}] = {size_name}")
        attendu = defines.get(size_name)
        lignes = [ligne.strip() for ligne in body.splitlines() if ligne.strip().startswith("{")]
        entrees = _ENTRY.findall(body)
        if len(lignes) != len(entrees):
            problems.append(f"{name} : {len(lignes) - len(entrees)} ligne(s) {{…}} illisible(s) (champ manquant ?)")
        if attendu is None or len(entrees) != attendu:
            problems.append(f"{name} : {len(entrees)} questions pour {size_name} = {attendu}"
                            " (une entrée manquante devient une question nulle en C++)")
        difficultes = Counter()
        for c, d, q, a, w1, w2, w3 in entrees:
            ou = f"{name} « {q[:50]} »"
            if int(c) != cat:
                problems.append(f"{ou} : catégorie {c}, attendu {cat}")
            if int(d) not in (0, 1, 2):
                problems.append(f"{ou} : difficulté {d} (0, 1 ou 2)")
            difficultes[int(d)] += 1
            textes = (q, a, w1, w2, w3)
            if any(not s.strip() for s in textes):
                problems.append(f"{ou} : texte vide")
            reponses = [s.strip().lower() for s in (a, w1, w2, w3)]
            if len(set(reponses)) != 4:
                problems.append(f"{ou} : bonne réponse et leurres pas tous différents")
            if len(q) > MAX_Q:
                problems.append(f"{ou} : énoncé de {len(q)} caractères (> {MAX_Q})")
            for s in (a, w1, w2, w3):
                if len(s) > MAX_A:
                    problems.append(f"{ou} : réponse « {s} » de {len(s)} caractères (> {MAX_A})")
            enonces.append(q)
        for d in (0, 1, 2):
            if entrees and difficultes[d] == 0:
                problems.append(f"{name} : aucune question de difficulté {d}")
    for q, n in Counter(enonces).items():
        if n > 1:
            problems.append(f"énoncé présent {n} fois : « {q[:60]} »")
    return problems


def main() -> int:
    problems = scan()
    for p in problems:
        print(p)
    if not problems:
        print("Banque de Trial Poursuite conforme.")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
