#!/usr/bin/env python3
"""tools/prochain_adr.py — le prochain numéro d'ADR libre, PR ouvertes comprises.

[AI-CONTEXT] Audit du 30/10/2026 (lot A) : des sessions parallèles prenaient le même
numéro d'ADR, chacune n'ayant regardé que `docs/decisions/` de sa branche (0065 à 0067
pris en double le 18/10). Le prochain numéro libre tient donc compte :

- de `docs/decisions/` de la copie de travail et de `origin/main` (`git ls-tree`) ;
- des ADR ajoutés par les PR ouvertes (`gh pr list --json files`) ; sans `gh`, des
  branches distantes (`git ls-tree` de chaque `origin/*`, après `git fetch`).

    python tools/prochain_adr.py           # affiche le numéro, et qui a pris les plus hauts
    python tools/prochain_adr.py --write   # recale le nombre d'ADR du README et de la cartographie

Prendre le numéro AU DÉBUT du travail et pousser tôt la branche (même en brouillon) :
une PR ouverte réserve son numéro pour les autres sessions. `--write` réécrit les comptes
que `tests/test_doc_comptes.py` vérifie (README en anglais et en français,
`CARTOGRAPHIE_TAB5.md`) : à lancer après avoir ajouté un ADR, ou après une fusion de
`main` qui en a ajouté — une résolution mécanique plutôt qu'un conflit à la main.
"""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DECISIONS = "docs/decisions"
ADR = re.compile(r"(?:^|/)docs/decisions/(\d{4})-[^/]+\.md$")
# Comptes écrits dans la doc (lus aussi par tests/test_doc_comptes.py, une seule liste).
COMPTES = (
    ("README.md", r"\b(\d+) \[architecture decision records\]"),
    ("README.md", r"\b(\d+) \[décisions d'architecture\]"),
    ("CARTOGRAPHIE_TAB5.md", r"\((\d+) ADR\)"),
)


def _git(*args: str) -> str:
    r = subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    return r.stdout if r.returncode == 0 else ""


def numeros(chemins) -> set[int]:
    return {int(m.group(1)) for c in chemins if (m := ADR.search(c.replace("\\", "/")))}


def adr_locaux() -> list[Path]:
    return sorted((REPO / DECISIONS).glob("[0-9][0-9][0-9][0-9]-*.md"))


def de_la_reference(ref: str) -> set[int]:
    return numeros(_git("ls-tree", "-r", "--name-only", ref, "--", DECISIONS).splitlines())


def des_pr_ouvertes() -> dict[int, list[str]] | None:
    """{numéro d'ADR: ['#PR branche', …]} ; None si `gh` ne répond pas."""
    if not shutil.which("gh"):
        return None
    r = subprocess.run(["gh", "pr", "list", "--state", "open", "--limit", "200",
                        "--json", "number,headRefName,files"],
                       cwd=REPO, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        return None
    pris: dict[int, list[str]] = {}
    for pr in json.loads(r.stdout or "[]"):
        for n in numeros(f["path"] for f in pr.get("files") or []):
            pris.setdefault(n, []).append(f"#{pr['number']} {pr['headRefName']}")
    return pris


def des_branches_distantes() -> dict[int, list[str]]:
    pris: dict[int, list[str]] = {}
    for ref in _git("for-each-ref", "--format=%(refname:short)", "refs/remotes/origin").split():
        if ref in ("origin", "origin/HEAD", "origin/main"):
            continue
        for n in de_la_reference(ref):
            pris.setdefault(n, []).append(ref)
    return pris


def prochain(pris: set[int]) -> int:
    """Le numéro qui suit le plus haut pris (on ne bouche pas un trou : un numéro
    abandonné a pu être cité ailleurs)."""
    return max(pris, default=0) + 1


def recaler_comptes(n: int) -> list[str]:
    """Réécrit les comptes de COMPTES à `n` ; rend les fichiers changés."""
    changes = []
    for nom, motif in COMPTES:
        chemin = REPO / nom
        brut = chemin.read_bytes().decode("utf-8")
        nouveau, k = re.subn(motif, lambda m: m.group(0).replace(m.group(1), str(n), 1), brut)
        if not k:
            raise SystemExit(f"❌ {nom} : plus rien ne correspond à {motif!r}, adapter COMPTES")
        if nouveau != brut:
            chemin.write_bytes(nouveau.encode("utf-8"))
            changes.append(nom)
    return sorted(set(changes))


def main(argv: list[str]) -> int:
    if "--write" in argv:
        n = len(adr_locaux())
        changes = recaler_comptes(n)
        print(f"✅ {n} ADR ; " + (f"recalé : {', '.join(changes)}" if changes else "comptes déjà justes"))
        return 0

    locaux = numeros(str(p.relative_to(REPO)) for p in adr_locaux())
    main_ = de_la_reference("origin/main")
    pr = des_pr_ouvertes()
    source = "PR ouvertes (gh)"
    if pr is None:
        pr = des_branches_distantes()
        source = "branches distantes (git, faire `git fetch` avant)"
    pris = locaux | main_ | set(pr)
    n = prochain(pris)
    print(f"Prochain ADR libre : {n:04d}")
    print(f"  plus haut dans la copie de travail : {max(locaux, default=0):04d}, "
          f"sur origin/main : {max(main_, default=0):04d}")
    hors_main = sorted(k for k in pr if k not in main_)
    if hors_main:
        print(f"  pris par des {source} :")
        for k in hors_main:
            print(f"    {k:04d} — {', '.join(sorted(set(pr[k])))}")
    else:
        print(f"  aucun ADR hors de main dans les {source}")
    doublons = sorted(k for k, v in pr.items() if len(set(v)) > 1 and k not in main_)
    for k in doublons:
        print(f"⚠️ {k:04d} pris par plusieurs : {', '.join(sorted(set(pr[k])))} — renuméroter l'un d'eux")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
