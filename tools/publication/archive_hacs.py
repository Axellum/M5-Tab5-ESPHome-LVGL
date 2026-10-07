# -*- coding: utf-8 -*-
"""tools/publication/archive_hacs.py — `tab5_hacs.zip` : l'intégration « Tab5 » et les fichiers HA
d'une release, que HACS installe en un clic (ADR-0035).

HACS (hacs.json à la racine du dépôt : `zip_release`, `filename`) télécharge cet asset de la
release et le décompresse dans config/custom_components/tab5/ : les fichiers de
l'intégration sont donc à la RACINE du zip (un dossier custom_components/tab5/ dans le zip
donnerait custom_components/tab5/custom_components/tab5/). Contenu :

    __init__.py, const.py, …, translations/   custom_components/tab5/ du tag, tel quel
    manifest.json                              sa `version` = celle de la release (HA
                                               l'affiche ; le dépôt garde 0.0.0)
    fichiers/MANIFESTE.json                    la liste des fichiers HA de la release
    fichiers/packages/…, custom_templates/…,   les mêmes octets que tab5_home_assistant.zip
    blueprints/…, tab5_optionnel/…             (archive_ha.entrees : version posée)

L'intégration (custom_components/tab5/installation.py) ne pose que ce que liste
MANIFESTE.json. Un tag sans custom_components/tab5/ (avant l'ADR-0035) n'a pas d'archive
HACS : code de sortie 3, rien n'est écrit (comme archive_ha.py pour les placeholders).

Usage (dans .github/workflows/publication.yml et integration-hacs.yml) :
    python tools/publication/archive_hacs.py --version 3.8.0 --sortie publie-ha
    python tools/publication/archive_hacs.py --version 3.8.0 --sortie publie-ha \\
        --base tag/HomeAssistant_Config --integration tag/custom_components/tab5
"""
from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from archive_ha import HA_DIR, RACINE, entrees, placeholders, ecrire_zip  # noqa: E402

NOM = "tab5_hacs.zip"
INTEGRATION = RACINE / "custom_components" / "tab5"
FICHIERS = "fichiers"
MANIFESTE = "MANIFESTE.json"
# Jamais dans l'archive : caches Python, fichiers d'éditeur.
IGNORES = ("__pycache__", ".pyc", ".pyo", "~", ".swp")


def code_integration(dossier: Path, version: str) -> list[tuple[str, bytes]]:
    """(chemin dans le zip, contenu) des fichiers de l'intégration, manifest.json versionné."""
    resultat = []
    for f in sorted(dossier.rglob("*")):
        relatif = f.relative_to(dossier).as_posix()
        if not f.is_file() or any(i in relatif for i in IGNORES) or relatif.split("/")[0] == FICHIERS:
            continue
        donnees = f.read_bytes()
        if relatif == "manifest.json":
            manifeste = json.loads(donnees)
            manifeste["version"] = version
            donnees = (json.dumps(manifeste, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        resultat.append((relatif, donnees))
    if not any(c == "manifest.json" for c, _ in resultat):
        raise SystemExit(f"{dossier} : pas de manifest.json")
    return resultat


def contenu(version: str, base: Path = HA_DIR, integration: Path = INTEGRATION) -> list[tuple[str, bytes]]:
    """Tout le zip, dans l'ordre : le code, puis le manifeste et les fichiers HA."""
    ha = entrees(version, base)
    manifeste = {"version": version, "fichiers": [chemin for chemin, _ in ha]}
    return (code_integration(integration, version)
            + [(f"{FICHIERS}/{MANIFESTE}",
                (json.dumps(manifeste, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))]
            + [(f"{FICHIERS}/{chemin}", donnees) for chemin, donnees in ha])


def construire(version: str, sortie: Path, base: Path = HA_DIR, integration: Path = INTEGRATION) -> Path:
    """Écrit `sortie/tab5_hacs.zip` ; renvoie son chemin."""
    tout = contenu(version, base, integration)
    sortie.mkdir(parents=True, exist_ok=True)
    archive = sortie / NOM
    ecrire_zip(archive, tout)
    return archive


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--version", required=True, help="version de la release (ex. 3.8.0)")
    parser.add_argument("--sortie", type=Path, required=True, help="dossier où écrire l'archive")
    parser.add_argument("--base", type=Path, default=HA_DIR,
                        help="dossier HomeAssistant_Config à archiver (défaut : celui du dépôt)")
    parser.add_argument("--integration", type=Path, default=INTEGRATION,
                        help="dossier custom_components/tab5 (défaut : celui du dépôt)")
    args = parser.parse_args()
    if not (args.integration / "manifest.json").is_file():
        print(f"::warning::{args.integration} absent : version d'avant l'intégration HACS, pas d'archive HACS.")
        return 3
    if restes := placeholders(args.base):
        print(f"::warning::{len(restes)} fichier(s) avec des placeholders : pas d'archive HACS.")
        return 3
    archive = construire(args.version, args.sortie, args.base, args.integration)
    with zipfile.ZipFile(archive) as z:
        for nom in z.namelist():
            print("  -", nom)
    print(f"{archive} ({archive.stat().st_size} octets)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
