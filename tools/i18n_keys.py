#!/usr/bin/env python3
"""tools/i18n_keys.py — relève les textes traduisibles du firmware (clés de tr()).

[AI-CONTEXT] Sert à tests/test_i18n.py et à préparer une nouvelle langue :

    python tools/i18n_keys.py            # liste les clés, et celles qui manquent à chaque langue

Deux sources :
  - les littéraux passés à tr(), tr_ctx(), tr_fill() et tr_noop() (C++ et lambdas
    YAML), y compris les deux branches d'un ternaire `tr(x ? "a" : "b")`. tr_noop()
    marque les textes rangés dans une table, traduits plus tard par tr(table[i]) ;
  - les textes posés par le YAML des écrans (`text: "…"` et les `vars:` d'include
    qui en tiennent lieu : title, name, state_text…), que i18n_apply_boot() traduit
    au démarrage. Les échappements YAML (`\\n`) sont décodés : la clé est le texte
    affiché. Seules les questions du quiz restent hors traduction (JEUX_NON_TRADUITS).
Les clés qui passent par une variable sans tr_noop() (set_toggle, hint, noms de
mélodie…) ne sont pas relevées ici : tests/test_i18n.py vérifie au moins qu'aucune clé
d'une langue n'est orpheline (elle doit exister comme littéral quelque part).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"

# Fichiers hors traduction : les questions du quiz restent en français (choix d'Axel,
# lot 4). Les jeux eux-mêmes sont traduits depuis le lot 4b (27/09/2026).
JEUX_NON_TRADUITS = ("trivia_questions",)
# Clés d'include qui portent un texte affiché (vars: { title: "…" }).
VARS_TEXTE = ("title", "name", "state_text", "label_text", "subtitle", "day_label",
              "text", "pot_nom")
# Seuls les fichiers d'INTERFACE posent des textes à l'écran : ailleurs, `name:` est un
# nom d'entité HA, qui ne se traduit jamais (le renommer casse l'historique).
FICHIERS_UI = ("tab5-lvgl.yaml",)
# Textes YAML jamais traduits : marques, unités, symboles, valeurs d'exemple.
NON_TRADUITS = {
    "Ok Nabu: ON", "Ok Nabu : ON", "Ok Nabu: OFF", "Ok Nabu : OFF", "Home Assistant",
    "Netflix", "Prime", "YouTube", "CANAL+", "Boost", "Flash", "Sys", "LEDs",
    "On / Off", "Menu", "Source", "HA", "PC", "TV", "OK", "SRAM", "PSRAM", "Wi-Fi", "MIN",
    # Noms des consoles : des noms propres, et le libellé « Écran courant » que lit HA
    # (GameRegistry, tab5_registry.cpp). « ARCADE » s'écrit pareil dans les deux langues.
    "Fil d'Or", "Arcanoïde", "Coureur d'Or", "Go Tab", "Trial Poursuite", "Dames Tab",
    "Roi Noir", "Neon Apron", "ARCADE",
}

RE_LIT = r'"((?:[^"\\]|\\.)*)"'
RE_APPEL = re.compile(r"\btr(?:_ctx|_fill|_noop)?\s*\(")
RE_TEXT = re.compile(r'\b(?:text|' + "|".join(VARS_TEXTE) + r')\s*:\s*' + RE_LIT)


def c_decode(lit: str) -> str:
    """Décode un littéral C (échappements \\xNN, \\n, \\", \\\\) en texte UTF-8."""
    out = bytearray()
    i = 0
    while i < len(lit):
        c = lit[i]
        if c == "\\" and i + 1 < len(lit):
            n = lit[i + 1]
            if n == "x":
                j = i + 2
                while j < len(lit) and j < i + 4 and lit[j] in "0123456789abcdefABCDEF":
                    j += 1
                out.append(int(lit[i + 2:j], 16))
                i = j
                continue
            out += {"n": b"\n", "t": b"\t", '"': b'"', "\\": b"\\", "'": b"'"}.get(n, n.encode())
            i += 2
            continue
        out += c.encode("utf-8")
        i += 1
    return out.decode("utf-8", errors="replace")


def fichiers_source(inclure_jeux: bool = False) -> list[Path]:
    fs = sorted(TAB5.glob("*.cpp")) + sorted(TAB5.glob("*.h")) + sorted(TAB5.glob("*.yaml"))
    fs += sorted((TAB5 / "ui_components").glob("*.yaml")) + [REPO / "tab5-ha-hmi.yaml"]
    if not inclure_jeux:
        fs = [f for f in fs if not any(j in f.name for j in JEUX_NON_TRADUITS)]
    return [f for f in fs if f.name not in ("tab5_i18n_data.h", "tab5_i18n.cpp", "tab5_i18n.h")]


def _args_appel(ligne: str, debut: int) -> str:
    """Texte entre la parenthèse ouvrante d'un appel et sa fermante (même ligne)."""
    prof, i = 0, debut
    while i < len(ligne):
        if ligne[i] == '"':
            j = i + 1
            while j < len(ligne) and ligne[j] != '"':
                j += 2 if ligne[j] == "\\" else 1
            i = j + 1
            continue
        if ligne[i] == "(":
            prof += 1
        elif ligne[i] == ")":
            prof -= 1
            if prof == 0:
                return ligne[debut:i + 1]
        i += 1
    return ligne[debut:]


def cles_tr(fichiers: list[Path] | None = None) -> dict[str, list[str]]:
    """Clé → emplacements, pour chaque littéral passé à tr()/tr_ctx()/tr_fill()."""
    res: dict[str, list[str]] = {}
    for f in fichiers or fichiers_source():
        for n, ligne in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            code = ligne.split("//")[0] if f.suffix in (".cpp", ".h") else ligne
            if f.suffix in (".cpp", ".h") and code.lstrip().startswith(("*", "/*")):
                continue
            for m in RE_APPEL.finditer(code):
                args = _args_appel(code, m.end() - 1)
                ctx = m.group(0).startswith("tr_ctx")
                lits = [c_decode(x) for x in re.findall(RE_LIT, args)]
                # Littéraux C adjacents ("parl\xC3\xA9""e") : même ligne, déjà séparés ;
                # on les recolle quand ils se suivent sans rien entre eux.
                lits = _recoller(args, lits)
                if m.group(0).startswith("tr_fill"):
                    lits = lits[:1]
                if ctx and len(lits) >= 2:
                    lits = [f"{lits[0]}|{lits[1]}"]
                for lit in lits:
                    res.setdefault(lit, []).append(f"{f.relative_to(REPO).as_posix()}:{n}")
    return res


def _recoller(args: str, lits: list[str]) -> list[str]:
    bruts = list(re.finditer(RE_LIT, args))
    if len(bruts) < 2:
        return lits
    out, cur = [], lits[0]
    for k in range(1, len(bruts)):
        entre = args[bruts[k - 1].end():bruts[k].start()]
        if entre.strip() == "":
            cur += lits[k]
        else:
            out.append(cur)
            cur = lits[k]
    out.append(cur)
    return out


def textes_yaml(fichiers: list[Path] | None = None) -> dict[str, list[str]]:
    """Textes posés par le YAML des écrans (traduits au démarrage)."""
    res: dict[str, list[str]] = {}
    for f in fichiers or fichiers_source():
        if f.suffix != ".yaml" or not (f.parent.name == "ui_components" or f.name in FICHIERS_UI):
            continue
        for n, ligne in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if ligne.lstrip().startswith("#"):
                continue
            for m in RE_TEXT.finditer(ligne):
                if "\\U000F" in m.group(1):
                    continue
                t = c_decode(m.group(1))  # « \n » YAML → retour à la ligne affiché
                if "${" in t or not re.search(r"[A-Za-zÀ-ÿ]{2,}", t):
                    continue
                if t in NON_TRADUITS:
                    continue
                res.setdefault(t, []).append(f"{f.relative_to(REPO).as_posix()}:{n}")
    return res


def main() -> int:
    sys.path.insert(0, str(REPO / "tools"))
    from gen_i18n import load_languages
    tr_ = cles_tr()
    ya = textes_yaml()
    toutes = sorted(set(tr_) | set(ya))
    print(f"{len(tr_)} clés tr(), {len(ya)} textes YAML, {len(toutes)} au total")
    for l in load_languages()[1:]:
        manque = [k for k in toutes if k not in l["entries"]]
        print(f"\n{l['name']} : {len(manque)} manquante(s)")
        for k in manque:
            print(f"  {k!r}  ← {(tr_.get(k) or ya.get(k))[0]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
