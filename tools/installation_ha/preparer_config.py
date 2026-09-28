# -*- coding: utf-8 -*-
"""tools/installation_ha/preparer_config.py — Le dossier config/ d'un Home Assistant neuf, installé
comme le fait un nouvel utilisateur (docs/installation.md, étape 4).

[AI-CONTEXT]
@role Première moitié du job « installation dans un HA neuf »
      (.github/workflows/installation-ha.yml) : écrit, sans rien lancer, le dossier que le
      conteneur Home Assistant monte en /config. La seconde moitié (verifier_installation.py) crée le
      compte, choisit les sources dans les listes « Tab5 · … », ajoute la tablette virtuelle
      et vérifie.
@contenu Ce que donne l'archive `tab5_home_assistant.zip` d'une release, décompressée dans
      config/ (liste de tools/publication/archive_ha.py, fichiers() : les mêmes) :
      - configuration.yaml : celui d'une installation neuve + la SEULE ligne que le guide
        fait ajouter, celle des packages (tools/installation_ha/configuration.yaml) ;
        secrets.yaml tel que HA l'écrit : AUCUNE ligne pour le Tab5 (ADR-0024 : plus de
        `!secret` dans les packages, tests/test_installation_ha.py le vérifie) ;
      - packages/ : TOUS les packages publics, tels quels (plus de placeholder à rendre),
        plus les données de test (donnees_test.yaml → packages/ci_donnees_test.yaml) ;
        avec --optionnels, aussi ceux d'optionnel/ (volet à course simulée) ;
      - custom_templates/ (packages/tab5_calendar.yaml les importe) ;
      - blueprints/automation/tab5/tab5_emplacements.yaml.
@ai_instruction Module pur (stdlib) : tests/test_installation_ha.py l'exécute dans un
      dossier temporaire. Les snippets/ ne sont pas copiés : HA ne les charge pas.

Usage :
    python tools/installation_ha/preparer_config.py --sortie ha-config
    python tools/installation_ha/preparer_config.py --sortie ha-config-optionnels --optionnels
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parent.parent
sys.path.insert(0, str(RACINE / "tools" / "publication"))

from archive_ha import HA_DIR, fichiers  # noqa: E402

CONFIGURATION = ICI / "configuration.yaml"
DONNEES_TEST = ICI / "donnees_test.yaml"
BLUEPRINT = HA_DIR / "blueprints" / "automation" / "tab5" / "tab5_emplacements.yaml"
# Dossier de l'archive qui n'est PAS installé par défaut (à copier dans packages/).
OPTIONNEL = "tab5_optionnel/"

# Chemin du blueprint vu par HA (use_blueprint.path), relatif à blueprints/automation/.
CHEMIN_BLUEPRINT = "tab5/tab5_emplacements.yaml"

# Fichiers qu'écrit HA à sa première installation, à côté de configuration.yaml.
# secrets.yaml tel quel : jusqu'au 28/09/2026, packages/tab5_tv.yaml exigeait une ligne
# `tab5_tv_app_url` sans laquelle HA refusait TOUTE sa configuration ; plus maintenant.
FICHIERS_VIDES = {
    "automations.yaml": "[]\n",
    "scripts.yaml": "",
    "scenes.yaml": "",
    "secrets.yaml": (
        "# Use this file to store secrets like usernames and passwords.\n"
        "# Learn more at https://www.home-assistant.io/docs/configuration/secrets/\n"
        "some_password: welcome\n"
    ),
}


def preparer(sortie: Path, optionnels: bool = False) -> list[Path]:
    """Écrit le dossier config/ dans `sortie` (créé, doit être vide ou absent)."""
    if sortie.exists() and any(sortie.iterdir()):
        raise SystemExit(f"{sortie} n'est pas vide : un Home Assistant NEUF part d'un dossier vide.")
    sortie.mkdir(parents=True, exist_ok=True)
    ecrits: list[Path] = []

    shutil.copyfile(CONFIGURATION, sortie / "configuration.yaml")
    ecrits.append(sortie / "configuration.yaml")
    for nom, contenu in FICHIERS_VIDES.items():
        (sortie / nom).write_text(contenu, encoding="utf-8", newline="\n")
        ecrits.append(sortie / nom)
    (sortie / "themes").mkdir()

    # L'archive décompressée dans config/ ; ses packages optionnels copiés dans
    # packages/ comme le dit son LISEZMOI, s'ils sont demandés.
    for source, chemin in fichiers():
        if chemin.startswith(OPTIONNEL):
            if not optionnels:
                continue
            chemin = "packages/" + chemin.removeprefix(OPTIONNEL)
        cible = sortie / chemin
        cible.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, cible)
        ecrits.append(cible)

    shutil.copyfile(DONNEES_TEST, sortie / "packages" / "ci_donnees_test.yaml")
    ecrits.append(sortie / "packages" / "ci_donnees_test.yaml")
    return ecrits


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--sortie", type=Path, required=True, help="dossier config/ à écrire (vide)")
    parser.add_argument("--optionnels", action="store_true",
                        help="copier aussi les packages d'optionnel/ dans packages/")
    args = parser.parse_args()
    for chemin in preparer(args.sortie, args.optionnels):
        print("  -", chemin.relative_to(args.sortie).as_posix())
    return 0


if __name__ == "__main__":
    sys.exit(main())
