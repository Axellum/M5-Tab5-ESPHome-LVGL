#!/usr/bin/env python3
"""tools/changelog_assembler.py — fragments de `changelog.d/` → `CHANGELOG.md`.

[AI-CONTEXT] Audit du 30/10/2026 (lot A) : chaque PR ajoutait sa puce en tête de
`## [Unreleased]`, donc deux PR ouvertes en même temps entraient TOUJOURS en conflit
(plus de 30 résolutions à la main en deux semaines, 70 commits depuis le 16/10). Depuis,
une PR ne touche plus `CHANGELOG.md` : elle dépose un fragment, un fichier à elle seule,
`changelog.d/<slug>.md` (slug = nom de la branche, `/` → `-`), qui ne peut entrer en
conflit avec aucun autre.

Un fragment est un morceau de `[Unreleased]`, écrit comme dans le CHANGELOG (liens
relatifs à la racine du dépôt) :

    ### Ajouté

    - **Titre court** : ce qui change, pour qui, et la PR ou l'ADR.

    ### Corrigé

    - **…**

Titres permis : ceux de Keep a Changelog en français, dans cet ordre — Ajouté, Modifié,
Obsolète, Retiré, Corrigé, Sécurité. Un texte placé avant le premier titre (par ex.
l'étiquette « **Contrat HA ↔ firmware** : … (depuis vX.Y.Z). » de
`tools/contrat_api.py`) va en tête de la section, avant ses sous-titres ;
`tests/test_contrat_versions.py` lit déjà les fragments comme `[Unreleased]`.

    python tools/changelog_assembler.py --nouveau Ajouté  # crée changelog.d/<branche>.md
    python tools/changelog_assembler.py --check           # fragments valides ? (pytest le joue)
    python tools/changelog_assembler.py --dry-run         # affiche le CHANGELOG assemblé, n'écrit rien
    python tools/changelog_assembler.py                   # assemble dans [Unreleased], supprime les fragments
    python tools/changelog_assembler.py --section 3.10.0  # … dans `## [3.10.0]` (PR de release)

Assemblage : chaque fragment va en TÊTE de son sous-titre (le plus récent d'abord, comme
le CHANGELOG), un sous-titre absent est créé à sa place dans l'ordre ci-dessus. Les
fragments sont rangés du plus récent au plus ancien selon le commit qui les a ajoutés
(`git log --diff-filter=A`, donc l'ordre des merges sur `main`) ; un fragment pas encore
commité passe en premier. Les entrées déjà sous `[Unreleased]` ne bougent pas. Les fins
de ligne de `CHANGELOG.md` (CRLF au checkout Windows, LF dans l'index) sont gardées.

On assemble au moment de la release (skill `release-tab5`), ou dans une PR
`chore(changelog)` à part : jamais dans une PR de fonctionnalité.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CHANGELOG = REPO / "CHANGELOG.md"
FRAGMENTS = REPO / "changelog.d"

# Keep a Changelog (Added, Changed, Deprecated, Removed, Fixed, Security), en français
# comme le CHANGELOG depuis la 3.0. L'ordre est celui des sous-titres d'une section.
SOUS_TITRES = ("Ajouté", "Modifié", "Obsolète", "Retiré", "Corrigé", "Sécurité")
# Un nom de fragment : celui de la branche, `/` changé en `-` (claude/x → claude-x.md).
NOM = re.compile(r"^[a-z0-9][a-z0-9._-]*\.md$")
NON_FRAGMENTS = {"README.md"}
SECTION = re.compile(r"^## \[([^\]]+)\]")
TITRE = re.compile(r"^### (.+?)\s*$")


class ErreurFragment(ValueError):
    pass


@dataclass
class Fragment:
    chemin: Path
    preambule: str = ""                         # texte avant le premier « ### »
    blocs: dict[str, str] = field(default_factory=dict)   # sous-titre → entrées

    @property
    def nom(self) -> str:
        return self.chemin.name


def slug(branche: str) -> str:
    """Nom de fragment tiré d'un nom de branche (`claude/Un_Sujet` → `claude-un_sujet.md`)."""
    s = re.sub(r"[^a-z0-9._-]+", "-", branche.strip().lower()).strip("-.")
    return f"{s or 'fragment'}.md"


def lire_fragment(chemin: Path) -> Fragment:
    """Découpe un fragment ; lève ErreurFragment avec une raison lisible."""
    if not NOM.match(chemin.name):
        raise ErreurFragment(f"{chemin.name} : nom attendu en minuscules, chiffres, « . _ - » "
                             "(le nom de la branche, `/` → `-`)")
    texte = chemin.read_text(encoding="utf-8").replace("\r\n", "\n")
    if texte.startswith("﻿"):
        raise ErreurFragment(f"{chemin.name} : BOM UTF-8 en tête")
    frag = Fragment(chemin)
    courant, lignes = None, []

    def ranger():
        corps = "\n".join(lignes).strip("\n")
        if courant is None:
            frag.preambule = corps
        elif corps.strip():
            if courant in frag.blocs:
                raise ErreurFragment(f"{chemin.name} : « ### {courant} » deux fois")
            frag.blocs[courant] = corps
        else:
            raise ErreurFragment(f"{chemin.name} : « ### {courant} » sans entrée")

    for ligne in texte.split("\n"):
        if ligne.startswith("## ") or ligne.startswith("# "):
            raise ErreurFragment(f"{chemin.name} : « {ligne.strip()} » — un fragment n'a que des "
                                 "titres « ### », la section vient de l'assemblage")
        t = TITRE.match(ligne)
        if t:
            ranger()
            courant, lignes = t.group(1), []
            if courant not in SOUS_TITRES:
                raise ErreurFragment(f"{chemin.name} : « ### {courant} », attendu l'un de "
                                     + ", ".join(SOUS_TITRES))
            continue
        lignes.append(ligne.rstrip())
    ranger()
    if not frag.blocs and not frag.preambule.strip():
        raise ErreurFragment(f"{chemin.name} : vide")
    for titre, corps in frag.blocs.items():
        if not corps.lstrip().startswith("- "):
            raise ErreurFragment(f"{chemin.name} : « ### {titre} » doit commencer par une puce « - »")
    return frag


def fichiers_fragments(dossier: Path = FRAGMENTS) -> list[Path]:
    if not dossier.is_dir():
        return []
    return sorted(p for p in dossier.iterdir() if p.is_file() and p.name not in NON_FRAGMENTS)


def _date_ajout(chemin: Path) -> float:
    """Heure du commit qui a ajouté le fragment ; maintenant s'il n'est pas commité."""
    try:
        sortie = subprocess.run(
            ["git", "log", "-1", "--diff-filter=A", "--format=%ct", "--", str(chemin)],
            cwd=chemin.parent, capture_output=True, text=True, timeout=30).stdout.strip()
        return float(sortie) if sortie else time.time() + 1
    except (OSError, subprocess.SubprocessError, ValueError):
        return time.time() + 1


def lire_fragments(dossier: Path = FRAGMENTS, *, ordre_git: bool = True) -> list[Fragment]:
    """Les fragments, du plus récent au plus ancien (même date : par nom)."""
    chemins = fichiers_fragments(dossier)
    frags = [lire_fragment(p) for p in chemins]
    if ordre_git:
        dates = {f.nom: _date_ajout(f.chemin) for f in frags}
        frags.sort(key=lambda f: (-dates[f.nom], f.nom))
    return frags


def _bornes_section(lignes: list[str], nom: str) -> tuple[int, int]:
    """(indice du titre `## [nom]`, indice de la section suivante ou fin)."""
    debut = next((i for i, l in enumerate(lignes)
                  if (m := SECTION.match(l)) and m.group(1) == nom), None)
    if debut is None:
        raise ErreurFragment(f"CHANGELOG.md : pas de section « ## [{nom}] »")
    fin = next((i for i in range(debut + 1, len(lignes)) if lignes[i].startswith("## ")), len(lignes))
    return debut, fin


def assembler(texte: str, frags: list[Fragment], section: str = "Unreleased") -> str:
    """Le CHANGELOG avec les fragments insérés dans `## [section]` (texte en « \\n »)."""
    lignes = texte.split("\n")
    debut, fin = _bornes_section(lignes, section)
    corps = lignes[debut + 1:fin]

    # Préambules : en tête de la section, avant le premier sous-titre, dans l'ordre des fragments.
    preambules = [f.preambule for f in frags if f.preambule.strip()]
    if preambules:
        ajout = "\n\n".join(preambules).split("\n")
        corps = [""] + ajout + [""] + _sans_vides_en_tete(corps)

    for titre in reversed(SOUS_TITRES):       # l'ordre d'insertion n'importe pas, chacun à sa place
        entrees = [f.blocs[titre] for f in frags if titre in f.blocs]
        if not entrees:
            continue
        nouvelles = "\n".join(entrees).split("\n")
        idx = next((i for i, l in enumerate(corps) if TITRE.match(l) and TITRE.match(l).group(1) == titre), None)
        if idx is not None:
            j = idx + 1
            while j < len(corps) and not corps[j].strip():
                j += 1
            corps[idx + 1:j] = [""] + nouvelles
            continue
        # Sous-titre absent : avant le premier sous-titre connu qui le suit dans l'ordre.
        suivants = SOUS_TITRES[SOUS_TITRES.index(titre) + 1:]
        place = next((i for i, l in enumerate(corps)
                      if TITRE.match(l) and TITRE.match(l).group(1) in suivants), None)
        bloc = [f"### {titre}", ""] + nouvelles + [""]
        if place is None:
            while corps and not corps[-1].strip():
                corps.pop()
            corps = corps + [""] + bloc
        else:
            corps[place:place] = bloc
    if corps and corps[-1].strip():
        corps.append("")
    return "\n".join(lignes[:debut + 1] + corps + lignes[fin:])


def _sans_vides_en_tete(lignes: list[str]) -> list[str]:
    i = 0
    while i < len(lignes) and not lignes[i].strip():
        i += 1
    return lignes[i:]


def _lire_changelog(chemin: Path) -> tuple[str, str]:
    brut = chemin.read_bytes().decode("utf-8")
    fin_ligne = "\r\n" if "\r\n" in brut else "\n"
    return brut.replace("\r\n", "\n"), fin_ligne


def verifier(changelog: Path = CHANGELOG, dossier: Path = FRAGMENTS, section: str = "Unreleased") -> list[str]:
    """Erreurs (vide = tout va bien) : chaque fragment lisible, et l'assemblage possible."""
    erreurs, frags = [], []
    for p in fichiers_fragments(dossier):
        try:
            frags.append(lire_fragment(p))
        except ErreurFragment as e:
            erreurs.append(str(e))
    try:
        texte, _ = _lire_changelog(changelog)
        assembler(texte, frags, section)
    except ErreurFragment as e:
        erreurs.append(str(e))
    return erreurs


def _branche() -> str:
    return subprocess.run(["git", "branch", "--show-current"], cwd=REPO,
                          capture_output=True, text=True).stdout.strip()


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="vérifie les fragments, n'écrit rien")
    ap.add_argument("--dry-run", action="store_true", help="affiche le CHANGELOG assemblé, n'écrit rien")
    ap.add_argument("--section", default="Unreleased", help="section visée (défaut : Unreleased)")
    ap.add_argument("--nouveau", metavar="SOUS_TITRE", choices=SOUS_TITRES,
                    help="crée changelog.d/<branche>.md avec ce sous-titre")
    ap.add_argument("--changelog", type=Path, default=CHANGELOG)
    ap.add_argument("--fragments", type=Path, default=FRAGMENTS)
    a = ap.parse_args(argv)

    if a.nouveau:
        branche = _branche()
        if not branche or branche == "main":
            print("❌ branche introuvable ou `main` : créer d'abord la branche de la PR")
            return 1
        cible = a.fragments / slug(branche)
        if cible.exists():
            print(f"❌ {cible.relative_to(REPO) if cible.is_relative_to(REPO) else cible} existe déjà")
            return 1
        cible.parent.mkdir(exist_ok=True)
        cible.write_text(f"### {a.nouveau}\n\n- **Titre court** : ce qui change, pour qui.\n", encoding="utf-8")
        print(f"✅ {cible} — à remplir, puis `git add`")
        return 0

    if a.check:
        erreurs = verifier(a.changelog, a.fragments, a.section)
        for e in erreurs:
            print(f"❌ {e}")
        if erreurs:
            return 1
        print(f"✅ {len(fichiers_fragments(a.fragments))} fragment(s) valide(s) dans {a.fragments.name}/")
        return 0

    try:
        frags = lire_fragments(a.fragments)
        texte, fin_ligne = _lire_changelog(a.changelog)
        nouveau = assembler(texte, frags, a.section)
    except ErreurFragment as e:
        print(f"❌ {e}")
        return 1
    if a.dry_run:
        sys.stdout.reconfigure(encoding="utf-8")
        print(nouveau, end="")
        return 0
    if not frags:
        print("✅ aucun fragment : rien à assembler")
        return 0
    a.changelog.write_bytes(nouveau.replace("\n", fin_ligne).encode("utf-8"))
    for f in frags:
        f.chemin.unlink()
    print(f"✅ {len(frags)} fragment(s) assemblé(s) dans [{a.section}] puis supprimé(s) : "
          + ", ".join(f.nom for f in frags))
    return 0


if __name__ == "__main__":
    sys.exit(main())
