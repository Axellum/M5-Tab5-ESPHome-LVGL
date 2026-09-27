# -*- coding: utf-8 -*-
"""tools/publication/pages.py — Page de flashage et manifestes de mise à jour (lot 6c, ADR-0022).

Le site GitHub Pages est reconstruit en entier à chaque publication, à partir des
fichiers des releases (pas des artefacts d'un run) : une pre-release ne remplace donc
jamais la version stable.

    <site>/index.html, …              la page (dossier web/)
    <site>/versions.json              ce que la page affiche
    <site>/stable/<écran>/            dernière release 3.x non « pre-release »
    <site>/beta/<écran>/              release 3.x la plus récente, pre-release ou non
        manifest.json                 lu par ESP Web Tools ET par l'entité de mise à jour
        tab5-ha-hmi-<écran>.factory.bin, .ota.bin

Les firmwares publiés lisent `<canal>/<écran>/manifest.json` (Tab5/publication-*.yaml) :
une tablette du canal bêta passe ainsi à la stable qui suit sa bêta. Les releases
d'avant la 3.0 n'ont pas de binaires : ignorées.

Usage (dans .github/workflows/publication.yml) :
    gh release list --json tagName,isPrerelease,isDraft,publishedAt > releases.json
    python tools/publication/pages.py choisir --releases releases.json   # stable=… beta=…
    python tools/publication/pages.py assembler --web web --assets assets \\
        --stable v3.0.0 --beta v3.1.0-rc.1 --sortie site
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path

ECRANS = ("st7123", "st7121", "ili9881c")
TAG = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.]+))?$")
MAJEURE_MINI = 3  # premières releases avec des binaires (lot 6c)


def choisir(releases: list[dict]) -> dict[str, str | None]:
    """Tags des canaux stable et bêta parmi les releases publiées (gh release list)."""
    candidates = []
    for r in releases:
        m = TAG.match(r.get("tagName", ""))
        if r.get("isDraft") or not m or int(m.group(1)) < MAJEURE_MINI:
            continue
        candidates.append(r)
    candidates.sort(key=lambda r: r["publishedAt"], reverse=True)
    stable = next((r["tagName"] for r in candidates if not r.get("isPrerelease")), None)
    beta = candidates[0]["tagName"] if candidates else None
    return {"stable": stable, "beta": beta}


def _canal(assets: Path, tag: str, dest: Path) -> dict:
    """Copie les manifestes et binaires d'une release ; renvoie ce qu'en dit versions.json."""
    source = assets / tag
    ecrans, version = [], None
    for ecran in ECRANS:
        manifeste_src = source / f"manifest-{ecran}.json"
        if not manifeste_src.is_file():
            continue
        manifeste = json.loads(manifeste_src.read_text(encoding="utf-8"))
        build = manifeste["builds"][0]
        fichiers = {build["parts"][0]["path"], build["ota"]["path"]}
        if any("/" in f or not (source / f).is_file() for f in fichiers):
            raise SystemExit(f"{tag}/{ecran} : binaire manquant ou chemin non local : {sorted(fichiers)}")
        cible = dest / ecran
        cible.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(manifeste_src, cible / "manifest.json")
        for f in fichiers:
            shutil.copyfile(source / f, cible / f)
        if version not in (None, manifeste["version"]):
            raise SystemExit(f"{tag} : versions différentes selon l'écran")
        version = manifeste["version"]
        ecrans.append(ecran)
    if not ecrans:
        raise SystemExit(f"{tag} : aucun manifest-<écran>.json dans {source}")
    return {"tag": tag, "version": version, "ecrans": ecrans}


def assembler(web: Path, assets: Path, stable: str | None, beta: str | None, sortie: Path) -> dict:
    """Écrit le site complet dans `sortie` ; renvoie le contenu de versions.json."""
    if sortie.exists():
        shutil.rmtree(sortie)
    shutil.copytree(web, sortie)
    versions = {canal: _canal(assets, tag, sortie / canal) if tag else None
                for canal, tag in (("stable", stable), ("beta", beta))}
    (sortie / "versions.json").write_text(json.dumps(versions, indent=2) + "\n", encoding="utf-8")
    return versions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sous = parser.add_subparsers(dest="action", required=True)
    p = sous.add_parser("choisir", help="affiche stable=<tag> et beta=<tag> (GITHUB_OUTPUT)")
    p.add_argument("--releases", type=Path, required=True)
    p = sous.add_parser("assembler", help="construit le site")
    p.add_argument("--web", type=Path, required=True)
    p.add_argument("--assets", type=Path, required=True, help="un sous-dossier par tag")
    p.add_argument("--stable", default="")
    p.add_argument("--beta", default="")
    p.add_argument("--sortie", type=Path, required=True)
    args = parser.parse_args()

    if args.action == "choisir":
        tags = choisir(json.loads(args.releases.read_text(encoding="utf-8")))
        for canal, tag in tags.items():
            print(f"{canal}={tag or ''}")
        return 0
    versions = assembler(args.web, args.assets, args.stable or None, args.beta or None, args.sortie)
    print(json.dumps(versions, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
