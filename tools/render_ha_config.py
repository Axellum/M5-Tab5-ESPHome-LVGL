#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""tools/render_ha_config.py — Copie déployable des fichiers Home Assistant publics, et
vérification qu'aucune valeur réelle n'est retombée dans un fichier suivi par git.

[AI-CONTEXT]
@role Depuis le 28/09/2026 (ADR-0024), les fichiers de `HomeAssistant_Config/`
      (packages/, optionnel/, snippets/, custom_templates/) ne contiennent PLUS de
      placeholder : chaque valeur de la maison (agendas, ville, téléphone, TV…) se
      choisit dans Home Assistant, et la tablette est trouvée par le modèle de son
      appareil. Ils s'installent donc TELS QUELS (archive `tab5_home_assistant.zip`
      des releases). Le « rendu » n'est plus qu'une copie dans
      `HomeAssistant_Config/rendered/` (gitignoré), gardée pour le déploiement de
      l'auteur (même dossier qu'avant).
@architecture_constraint `--check` reste le garde-fou de fuite (ADR-0017) : il lit
      `HomeAssistant_Config/placeholders.yaml` (gitignoré, secret HA_PLACEHOLDERS en
      CI), liste `nom: valeur réelle` des identifiants de l'auteur qui ne doivent
      JAMAIS apparaître dans un fichier public. Il refuse aussi tout placeholder
      restant (`VOTRE_…`) : un fichier public doit marcher sans être rempli.
@ai_instruction `--check` ne doit JAMAIS imprimer une valeur réelle : il ne cite
      que le nom de la clé, le fichier et la ligne. C'est ce qui le rend
      utilisable en CI (journal public).

Usage :
    python tools/render_ha_config.py            # copie packages/ + optionnel/ + snippets/ + custom_templates/ dans rendered/
    python tools/render_ha_config.py --check    # exit 1 si une valeur réelle ou un placeholder traîne dans un fichier suivi
    python tools/render_ha_config.py --map autre.yaml --out /tmp/rendu
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HA_DIR = REPO / "HomeAssistant_Config"
DEFAULT_MAP = HA_DIR / "placeholders.yaml"
DEFAULT_OUT = HA_DIR / "rendered"

# Fichiers publics (relatifs à HomeAssistant_Config/). Depuis le 26/09/2026 (lot 3),
# ce sont eux qui tournent chez l'auteur ; `optionnel/` depuis le 28/09/2026
# (package du volet à course simulée, à copier dans config/packages/ au besoin).
PUBLIC_GLOBS = (
    "packages/*.yaml",
    "optionnel/*.yaml",
    "snippets/*.yaml",
    "custom_templates/*.jinja",
)

# Une valeur réelle plus courte que ça est trop ambiguë pour être cherchée
# dans les fichiers publics (ex. « 40 » apparaît dans n'importe quel YAML).
MIN_CHECK_LEN = 6

# Placeholder des anciennes versions (≤ 3.1) : VOTRE_VILLE, calendar.VOTRE_EMAIL_gmail_com…
PLACEHOLDER = re.compile(r"VOTRE_[A-Z]")


def load_map(path: Path) -> dict[str, str]:
    """Lit `nom: valeur` (une entrée par ligne, YAML plat, sans dépendance).

    Les clés peuvent contenir des points (`number.xxx`), d'où un parseur maison
    plutôt qu'un YAML strict : on ne veut ni dépendance ni surprise de type.
    """
    mapping: dict[str, str] = {}
    if not path.exists():
        return mapping
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            raise ValueError(f"{path.name}: ligne sans ':' → {raw!r}")
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.split(" #", 1)[0].strip().strip('"').strip("'")
        if not key:
            raise ValueError(f"{path.name}: clé vide → {raw!r}")
        mapping[key] = value
    return mapping


def public_files(base: Path | None = None) -> list[Path]:
    base = base or HA_DIR  # lu à l'appel : les tests le remplacent
    files: list[Path] = []
    for pattern in PUBLIC_GLOBS:
        files.extend(sorted(base.glob(pattern)))
    return files


def render(out_dir: Path, base: Path | None = None) -> list[Path]:
    """Copie les fichiers publics dans `out_dir`, même arborescence (octet pour octet)."""
    base = base or HA_DIR
    written: list[Path] = []
    for src in public_files(base):
        dst = out_dir / src.relative_to(base)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        written.append(dst)
    return written


def placeholders(base: Path | None = None) -> list[tuple[str, int]]:
    """(fichier relatif, ligne) de chaque placeholder `VOTRE_…` restant."""
    base = base or HA_DIR
    findings: list[tuple[str, int]] = []
    for src in public_files(base):
        rel = str(src.relative_to(base)).replace("\\", "/")
        for n, line in enumerate(src.read_text(encoding="utf-8").splitlines(), 1):
            if PLACEHOLDER.search(line):
                findings.append((rel, n))
    return findings


def check(mapping: dict[str, str], base: Path | None = None) -> list[tuple[str, int, str]]:
    """Cherche les VALEURS réelles dans les fichiers publics.

    Retourne (fichier relatif, ligne, nom de la clé) — jamais la valeur elle-même.
    Les valeurs identiques à leur clé ou trop courtes sont ignorées.
    """
    base = base or HA_DIR
    findings: list[tuple[str, int, str]] = []
    needles = [(k, v) for k, v in mapping.items()
               if v and v != k and len(v) >= MIN_CHECK_LEN]
    for src in public_files(base):
        rel = str(src.relative_to(base)).replace("\\", "/")
        for n, line in enumerate(src.read_text(encoding="utf-8").splitlines(), 1):
            for key, value in needles:
                # Sous-chaîne brute, pas de frontière de mot : une entité DÉRIVÉE
                # (`sensor.<ville>_next_rain`) doit être attrapée comme l'entité
                # de base. MIN_CHECK_LEN écarte les valeurs trop courtes.
                if value in line:
                    findings.append((rel, n, key))
    return findings


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("--map", type=Path, default=DEFAULT_MAP,
                    help="valeurs réelles à ne jamais publier, `nom: valeur` (défaut : HomeAssistant_Config/placeholders.yaml)")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT,
                    help="dossier de la copie (défaut : HomeAssistant_Config/rendered/)")
    ap.add_argument("--check", action="store_true",
                    help="ne copie rien ; échoue si une valeur réelle ou un placeholder est présent dans un fichier public")
    args = ap.parse_args(argv)

    if not args.check:
        written = render(args.out)
        print(f"{len(written)} fichier(s) copié(s) dans {args.out}")
        for p in written:
            print("  -", p.relative_to(args.out))
        return 0

    try:
        mapping = load_map(args.map)
    except ValueError as exc:
        print(f"ERREUR : {exc}")
        return 2
    echec = False
    for rel, n in placeholders():
        print(f"⚠️ PLACEHOLDER dans un fichier public : {rel} ligne {n} "
              f"(ADR-0024 : la valeur se choisit dans Home Assistant)")
        echec = True
    if not mapping:
        print(f"Aucune valeur réelle dans {args.map} : seule l'absence de placeholder est vérifiée.")
    else:
        findings = check(mapping)
        if findings:
            print("⚠️ VALEURS RÉELLES DANS DES FICHIERS PUBLICS :")
            for rel, n, key in findings:
                print(f"Fichier: {rel} | Ligne: {n} | Clé: {key}")
            echec = True
    if echec:
        return 1
    print("✅ Aucune valeur réelle ni placeholder dans les fichiers Home Assistant publics.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
