#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""tools/render_ha_config.py — Rend les fichiers Home Assistant publics (placeholders)
en fichiers déployables (valeurs réelles), et vérifie qu'aucune valeur réelle
n'est retombée dans un fichier suivi par git.

[AI-CONTEXT]
@role Les fichiers de `HomeAssistant_Config/packages/` et `*_examples*` sont
      PUBLICS : ils ne contiennent que des placeholders (`VOTRE_VILLE`,
      `calendar.VOTRE_EMAIL_gmail_com`, `media_player.VOTRE_TV`…). Les valeurs
      réelles vivent dans `HomeAssistant_Config/placeholders.yaml` (gitignoré ;
      modèle : `placeholders.example.yaml`). Ce script produit
      `HomeAssistant_Config/rendered/` (gitignoré) = copie déployable sur HA.
@architecture_constraint Sens unique : PUBLIC -> RENDU. On édite les fichiers
      suivis (relus en PR), jamais le rendu. Le 06/09/2026 les packages
      déployés sur HA étaient octet pour octet les fichiers suivis, avec les
      identifiants réels dedans : ce script est ce qui permet de les
      anonymiser sans changer ce que HA charge.
@ai_instruction `--check` ne doit JAMAIS imprimer une valeur réelle : il ne cite
      que le nom du placeholder, le fichier et la ligne. C'est ce qui le rend
      utilisable en CI (journal public).

Usage :
    python tools/render_ha_config.py            # rend packages/ + snippets/ + custom_templates/ + exemples
    python tools/render_ha_config.py --check    # exit 1 si une valeur réelle traîne dans un fichier suivi
    python tools/render_ha_config.py --map autre.yaml --out /tmp/rendu
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HA_DIR = REPO / "HomeAssistant_Config"
DEFAULT_MAP = HA_DIR / "placeholders.yaml"
DEFAULT_OUT = HA_DIR / "rendered"

# Fichiers publics rendus (relatifs à HomeAssistant_Config/). Les fichiers de
# production gitignorés (automations_tab5.yaml…) ne sont ni lus ni écrits.
PUBLIC_GLOBS = (
    "packages/*.yaml",
    "snippets/*.yaml",
    "custom_templates/*.jinja",
    "automations_examples.yaml.example",
    "scripts_examples.yaml",
    "template_sensors_examples.yaml",
)

# Une valeur réelle plus courte que ça est trop ambiguë pour être cherchée
# dans les fichiers publics (ex. « 40 » apparaît dans n'importe quel YAML).
MIN_CHECK_LEN = 6


def load_map(path: Path) -> dict[str, str]:
    """Lit `placeholder: valeur` (une entrée par ligne, YAML plat, sans dépendance).

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


def render_text(text: str, mapping: dict[str, str]) -> str:
    """Remplace chaque placeholder par sa valeur, les plus longs d'abord.

    Ordre par longueur décroissante : `VOTRE_VILLE_next_rain` n'existe pas comme
    clé mais `VOTRE_VILLE` oui — un remplacement mot-à-mot suffit ; en revanche
    `number.m5stack…rendez_vous_annoncer_avant` doit passer AVANT un éventuel
    placeholder plus court qu'il contiendrait.
    """
    for key in sorted(mapping, key=len, reverse=True):
        value = mapping[key]
        if value == "" or value == key:
            continue
        text = text.replace(key, value)
    return text


def public_files(base: Path = HA_DIR) -> list[Path]:
    files: list[Path] = []
    for pattern in PUBLIC_GLOBS:
        files.extend(sorted(base.glob(pattern)))
    return files


def render(mapping: dict[str, str], out_dir: Path, base: Path = HA_DIR) -> list[Path]:
    written: list[Path] = []
    for src in public_files(base):
        rel = src.relative_to(base)
        dst = out_dir / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(render_text(src.read_text(encoding="utf-8"), mapping),
                       encoding="utf-8", newline="\n")
        written.append(dst)
    return written


def check(mapping: dict[str, str], base: Path = HA_DIR) -> list[tuple[str, int, str]]:
    """Cherche les VALEURS réelles dans les fichiers publics.

    Retourne (fichier relatif, ligne, placeholder) — jamais la valeur elle-même.
    Les valeurs identiques au placeholder ou trop courtes sont ignorées.
    """
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
                    help="fichier placeholder: valeur (défaut : HomeAssistant_Config/placeholders.yaml)")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT,
                    help="dossier de sortie du rendu (défaut : HomeAssistant_Config/rendered/)")
    ap.add_argument("--check", action="store_true",
                    help="ne rend rien ; échoue si une valeur réelle est présente dans un fichier public")
    args = ap.parse_args(argv)

    try:
        mapping = load_map(args.map)
    except ValueError as exc:
        print(f"ERREUR : {exc}")
        return 2
    if not mapping:
        print(f"Aucune correspondance dans {args.map} — copier placeholders.example.yaml "
              f"vers placeholders.yaml et y mettre vos identifiants réels.")
        return 0 if args.check else 2

    if args.check:
        findings = check(mapping)
        if findings:
            print("⚠️ VALEURS RÉELLES DANS DES FICHIERS PUBLICS :")
            for rel, n, key in findings:
                print(f"Fichier: {rel} | Ligne: {n} | Placeholder attendu: {key}")
            return 1
        print("✅ Aucune valeur réelle dans les fichiers Home Assistant publics.")
        return 0

    written = render(mapping, args.out)
    print(f"{len(written)} fichier(s) rendu(s) dans {args.out}")
    for p in written:
        print("  -", p.relative_to(args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
