# -*- coding: utf-8 -*-
"""tools/installation_ha/preparer_config.py — Le dossier config/ d'un Home Assistant neuf, installé
comme le fait un nouvel utilisateur (docs/installation.md, étape 4).

[AI-CONTEXT]
@role Première moitié du job « installation dans un HA neuf »
      (.github/workflows/installation-ha.yml) : écrit, sans rien lancer, le dossier que le
      conteneur Home Assistant monte en /config. La seconde moitié (verifier_installation.py) crée le
      compte, ajoute la tablette virtuelle et vérifie.
@contenu
      - configuration.yaml : celui d'une installation neuve + la ligne des packages
        (tools/installation_ha/configuration.yaml) ; secrets.yaml avec la ligne
        qu'exige packages/tab5_tv.yaml (SECRETS) ;
      - packages/ : TOUS les packages publics, rendus avec les valeurs factices de
        placeholders_ci.yaml par tools/render_ha_config.py (« copiez
        rendered/packages/*.yaml », étape 4, point 4), plus les données de test
        (donnees_test.yaml → packages/ci_donnees_test.yaml) ;
      - custom_templates/ : rendus aussi (packages/tab5_calendar.yaml les importe) ;
      - blueprints/automation/tab5/tab5_emplacements.yaml : copié tel quel (étape 4,
        point 6 : « ou copiez le fichier dans config/blueprints/automation/tab5/ »).
@ai_instruction Module pur (stdlib) : tests/test_installation_ha.py l'exécute dans un
      dossier temporaire. Les snippets/ ne sont pas copiés : HA ne les charge pas.

Usage :
    python tools/installation_ha/preparer_config.py --sortie ha-config
"""
from __future__ import annotations

import argparse
import shutil
import sys
import tempfile
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parent.parent
sys.path.insert(0, str(RACINE / "tools"))

from render_ha_config import HA_DIR, load_map, render  # noqa: E402

CARTE_CI = ICI / "placeholders_ci.yaml"
CONFIGURATION = ICI / "configuration.yaml"
DONNEES_TEST = ICI / "donnees_test.yaml"
BLUEPRINT = HA_DIR / "blueprints" / "automation" / "tab5" / "tab5_emplacements.yaml"

# Chemin du blueprint vu par HA (use_blueprint.path), relatif à blueprints/automation/.
CHEMIN_BLUEPRINT = "tab5/tab5_emplacements.yaml"

# Fichiers qu'écrit HA à sa première installation, à côté de configuration.yaml.
FICHIERS_VIDES = {"automations.yaml": "[]\n", "scripts.yaml": "", "scenes.yaml": ""}

# secrets.yaml d'une installation neuve, plus la ligne qu'exige packages/tab5_tv.yaml
# (son en-tête, @install) : sans elle, HA refuse TOUTE sa configuration (« Secret
# tab5_tv_app_url not defined », vu par ce job le 28/09/2026). Adresse factice : aucune
# TV derrière (port 9, « discard »).
SECRETS = (
    "# Use this file to store secrets like usernames and passwords.\n"
    "# Learn more at https://www.home-assistant.io/docs/configuration/secrets/\n"
    "some_password: welcome\n"
    "# packages/tab5_tv.yaml (@install) : URL de lancement des applications de la TV.\n"
    'tab5_tv_app_url: "http://127.0.0.1:9/api/v2/applications/{{ app_id }}"\n'
)


def preparer(sortie: Path, carte: Path = CARTE_CI) -> list[Path]:
    """Écrit le dossier config/ dans `sortie` (créé, doit être vide ou absent)."""
    if sortie.exists() and any(sortie.iterdir()):
        raise SystemExit(f"{sortie} n'est pas vide : un Home Assistant NEUF part d'un dossier vide.")
    sortie.mkdir(parents=True, exist_ok=True)
    ecrits: list[Path] = []

    shutil.copyfile(CONFIGURATION, sortie / "configuration.yaml")
    ecrits.append(sortie / "configuration.yaml")
    for nom, contenu in {**FICHIERS_VIDES, "secrets.yaml": SECRETS}.items():
        (sortie / nom).write_text(contenu, encoding="utf-8", newline="\n")
        ecrits.append(sortie / nom)
    (sortie / "themes").mkdir()

    correspondances = load_map(carte)
    if not correspondances:
        raise SystemExit(f"{carte} : aucune correspondance.")
    with tempfile.TemporaryDirectory() as tmp:
        render(correspondances, Path(tmp))
        for dossier in ("packages", "custom_templates"):
            shutil.copytree(Path(tmp) / dossier, sortie / dossier)
            ecrits.extend(sorted((sortie / dossier).iterdir()))

    shutil.copyfile(DONNEES_TEST, sortie / "packages" / "ci_donnees_test.yaml")
    ecrits.append(sortie / "packages" / "ci_donnees_test.yaml")

    cible = sortie / "blueprints" / "automation" / CHEMIN_BLUEPRINT
    cible.parent.mkdir(parents=True)
    shutil.copyfile(BLUEPRINT, cible)
    ecrits.append(cible)
    return ecrits


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--sortie", type=Path, required=True, help="dossier config/ à écrire (vide)")
    parser.add_argument("--carte", type=Path, default=CARTE_CI, help="placeholders (défaut : placeholders_ci.yaml)")
    args = parser.parse_args()
    for chemin in preparer(args.sortie, args.carte):
        print("  -", chemin.relative_to(args.sortie).as_posix())
    return 0


if __name__ == "__main__":
    sys.exit(main())
