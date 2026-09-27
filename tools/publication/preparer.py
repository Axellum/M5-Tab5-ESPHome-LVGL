# -*- coding: utf-8 -*-
"""tools/publication/preparer.py — Binaires d'une révision d'écran, prêts à publier (lot 6c).

`esphome/build-action` (complete-manifest) range sa sortie dans un dossier
`tab5-ha-hmi-esp32p4/` : `<nom>.factory.bin`, `<nom>.ota.bin` et un manifeste ESP Web
Tools dont les chemins sont ces noms. Les trois révisions d'écran portent les mêmes :
ce script les renomme par révision (les fichiers d'une release sont à plat) et réécrit le
manifeste en conséquence, en `manifest-<révision>.json`.

Contrôles, car une page de flashage fausse se voit trop tard :
- nom du projet et puce attendus, version = celle de la release ;
- MD5 et SHA-256 du manifeste = ceux des fichiers copiés ;
- `new_install_prompt_erase` : ESP Web Tools propose d'effacer la flash à une première
  installation (un Tab5 neuf porte la démo de M5Stack et sa NVS).

Usage (dans .github/workflows/publication.yml) :
    python tools/publication/preparer.py --source tab5-ha-hmi-esp32p4 --ecran st7121 \\
        --version 3.0.0 --sortie publie
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

PROJET = "axellum.tab5-ha-hmi"
PUCE = "ESP32-P4"
ECRANS = ("st7123", "st7121", "ili9881c")
BASE = "tab5-ha-hmi"


def _empreintes(fichier: Path) -> tuple[str, str]:
    donnees = fichier.read_bytes()
    return hashlib.md5(donnees).hexdigest(), hashlib.sha256(donnees).hexdigest()


def _unique(dossier: Path, motif: str) -> Path:
    trouves = sorted(dossier.glob(motif))
    if len(trouves) != 1:
        raise SystemExit(f"{dossier} : {len(trouves)} fichier(s) {motif}, 1 attendu")
    return trouves[0]


def preparer(source: Path, ecran: str, version: str, sortie: Path) -> Path:
    """Copie et renomme les binaires, écrit `manifest-<écran>.json` ; renvoie son chemin."""
    if ecran not in ECRANS:
        raise SystemExit(f"révision d'écran inconnue : {ecran} (attendu : {', '.join(ECRANS)})")
    manifeste = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    if manifeste.get("name") != PROJET:
        raise SystemExit(f"manifeste : name = {manifeste.get('name')!r}, {PROJET!r} attendu")
    if manifeste.get("version") != version:
        raise SystemExit(f"manifeste : version = {manifeste.get('version')!r}, {version!r} attendu")
    builds = manifeste.get("builds") or []
    if len(builds) != 1 or builds[0].get("chipFamily") != PUCE:
        raise SystemExit(f"manifeste : une seule build {PUCE} attendue, trouvé {builds!r}")

    sortie.mkdir(parents=True, exist_ok=True)
    usine = sortie / f"{BASE}-{ecran}.factory.bin"
    ota = sortie / f"{BASE}-{ecran}.ota.bin"
    shutil.copyfile(_unique(source, "*.factory.bin"), usine)
    shutil.copyfile(_unique(source, "*.ota.bin"), ota)

    build = builds[0]
    for partie, fichier in ((build["parts"][0], usine), (build["ota"], ota)):
        md5, sha256 = _empreintes(fichier)
        if partie.get("md5") != md5 or partie.get("sha256") != sha256:
            raise SystemExit(f"{fichier.name} : empreintes différentes de celles du manifeste")
        partie["path"] = fichier.name
    if len(build["parts"]) != 1 or build["parts"][0].get("offset") != 0:
        raise SystemExit("manifeste : une seule partie à l'offset 0 attendue (factory.bin)")
    manifeste["new_install_prompt_erase"] = True

    chemin = sortie / f"manifest-{ecran}.json"
    chemin.write_text(json.dumps(manifeste, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return chemin


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", type=Path, required=True, help="dossier de sortie de build-action")
    parser.add_argument("--ecran", required=True, choices=ECRANS)
    parser.add_argument("--version", required=True, help="version de la release (tag sans « v »)")
    parser.add_argument("--sortie", type=Path, required=True)
    args = parser.parse_args()
    chemin = preparer(args.source, args.ecran, args.version, args.sortie)
    print(f"écrit {chemin} et ses deux binaires")
    return 0


if __name__ == "__main__":
    sys.exit(main())
