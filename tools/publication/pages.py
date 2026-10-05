# -*- coding: utf-8 -*-
"""tools/publication/pages.py — Site GitHub Pages : vitrine, page de flashage et manifestes
de mise à jour (lot 6c, ADR-0022).

Le site est reconstruit en entier à chaque déploiement (.github/workflows/site.yml), à
partir des fichiers des releases (pas des artefacts d'un run) : une pre-release ne
remplace donc jamais la version stable.

    <site>/index.html                 la vitrine (dossier web/)
    <site>/install/index.html         la page de flashage
    <site>/images/                    images de docs/images/ sous un nom parlant (IMAGES)
    <site>/sitemap.xml                pages et images, pour les moteurs de recherche
    <site>/versions.json              ce que la page de flashage affiche
    <site>/stable/<écran>/            dernière release 3.x non « pre-release »
    <site>/beta/<écran>/              release 3.x la plus récente, pre-release ou non
        manifest.json                 lu par ESP Web Tools ET par l'entité de mise à jour
        tab5-ha-hmi-<écran>.factory.bin, .ota.bin

Les firmwares publiés lisent `<canal>/<écran>/manifest.json` (Tab5/publication-*.yaml) :
une tablette du canal bêta passe ainsi à la stable qui suit sa bêta. Les releases
d'avant la 3.0 n'ont pas de binaires : ignorées.

Les images sont servies par le site et non par github.com, dont le robots.txt interdit
`/*/raw/` : c'est là que passent les images d'un README.

Usage (dans .github/workflows/site.yml) :
    gh release list --json tagName,isPrerelease,isDraft,publishedAt > releases.json
    python tools/publication/pages.py choisir --releases releases.json   # stable=… beta=…
    python tools/publication/pages.py assembler --web web --assets assets --images docs/images \\
        --stable v3.0.0 --beta v3.1.0-rc.1 --sortie site
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path
from xml.sax.saxutils import escape

ECRANS = ("st7123", "st7121", "ili9881c")
TAG = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.]+))?$")
MAJEURE_MINI = 3  # premières releases avec des binaires (lot 6c)
SITE = "https://axellum.github.io/M5-Tab5-ESPHome-LVGL/"

# Nom publié (site/images/) → fichier de docs/images/. Un nom parlant aide les moteurs
# de recherche ; les fichiers du dépôt gardent le leur (README, docs, kit de presse).
IMAGES = {
    "m5stack-tab5-home-assistant-screen-card.jpg": "tab5_social_preview.jpg",
    "m5stack-tab5-home-assistant-wall-screen.jpg": "tab5_hero_4x3.jpg",
    "m5stack-tab5-home-assistant-ui-tour.webp": "tab5_ui_tour_hq.webp",
    "m5stack-tab5-home-assistant-device-buttons.jpg": "tab5_photo_domo.jpg",
    "m5stack-tab5-plant-sensors-soil-moisture.jpg": "tab5_photo_plants.jpg",
    "m5stack-tab5-home-assistant-climate-control.jpg": "tab5_photo_climate_popup_v2.jpg",
    "m5stack-tab5-home-assistant-light-control.jpg": "tab5_photo_light_popup_v2.jpg",
    "m5stack-tab5-samsung-tv-remote.jpg": "tab5_photo_tv_remote.jpg",
    "m5stack-tab5-esphome-diagnostics-console.jpg": "tab5_photo_console_v2.jpg",
    "m5stack-tab5-calendar-work-hours.jpg": "tab5_photo_calendar.jpg",
    "m5stack-tab5-voice-assistant-reply.jpg": "tab5_photo_assistant_popup.jpg",
    "m5stack-tab5-arcade-games-lvgl.jpg": "tab5_photo_arcade_selector.jpg",
    "m5stack-tab5-themes-light-dark.jpg": "tab5_themes.jpg",
    "m5stack-tab5-home-assistant-dashboard.png": "ha_tableau_tab5.png",
    "m5stack-tab5-chess-game-esp32-p4.jpg": "tab5_photo_chess.jpg",
    "m5stack-tab5-lode-runner-game.jpg": "tab5_photo_lode_runner.jpg",
    "m5stack-tab5-breakout-game-tilt.jpg": "tab5_photo_arkanoid.jpg",
    # Recadrages de rendus de la CI (scène « pluie + vigilance orange »), propres au site :
    # hors de docs/images/rendu/, que tools/rendu/maj_references.py vide à chaque mise à jour.
    "m5stack-tab5-rain-next-hour-fr.png": "site/pluie-dans-l-heure.png",
    "m5stack-tab5-rain-next-hour-en.png": "site/pluie-dans-l-heure-en.png",
    "m5stack-tab5-weather-warnings-fr.png": "site/vigilances.png",
    "m5stack-tab5-weather-warnings-en.png": "site/vigilances-en.png",
    "m5stack-tab5-lvgl-screen-sunny-day-en.png": "rendu/1-journee-ensoleillee-en.png",
    "m5stack-tab5-lvgl-screen-sunny-day-fr.png": "rendu/1-journee-ensoleillee.png",
    "m5stack-tab5-lvgl-screen-rain-warning-en.png": "rendu/2-pluie-alerte-orange-en.png",
    "m5stack-tab5-lvgl-screen-rain-warning-fr.png": "rendu/2-pluie-alerte-orange.png",
    "m5stack-tab5-lvgl-screen-day-off-plants-en.png": "rendu/3-jour-de-repos-plantes-a-surveiller-en.png",
    "m5stack-tab5-lvgl-screen-day-off-plants-fr.png": "rendu/3-jour-de-repos-plantes-a-surveiller.png",
}
IMAGE_DE_PAGE = re.compile(r'<img\b[^>]*\bsrc="((?:\.\./)*images/[^"]+)"')


def fichiers_joints(fichiers: set[str]) -> bool:
    """Le site peut servir la release : le manifeste de l'écran de référence (ECRANS[0], le
    seul essayé sur une tablette) et, pour chaque manifeste, ses deux binaires (noms de
    preparer.py). gh release upload envoie les fichiers en parallèle : un manifeste peut
    être joint avant ses binaires, et _canal() échoue alors. Un autre écran absent ne
    l'écarte pas : une révision ajoutée à ECRANS après une release n'a pas de fichiers
    dans celle-ci, et les exiger tous viderait les canaux jusqu'à la release suivante."""
    ecrans = [e for e in ECRANS if f"manifest-{e}.json" in fichiers]
    return ECRANS[0] in ecrans and all(
        f"tab5-ha-hmi-{e}.{image}.bin" in fichiers for e in ecrans for image in ("factory", "ota"))


def choisir(releases: list[dict]) -> dict[str, str | None]:
    """Tags des canaux stable et bêta parmi les releases publiées (API des releases).

    Une release dont les fichiers ne sont pas encore joints (fichiers_joints) est ignorée :
    juste après sa création, publication.yml compile encore ses binaires (~12 min). Un déploiement
    du site lancé entre-temps (push sur main, 28/09/2026 : #219 mergée 4 min après la
    création de v3.1.0) la prenait pour la stable et échouait sur « no assets to
    download ». `assets` absent : pas de filtre (liste déjà vérifiée)."""
    candidates = []
    for r in releases:
        m = TAG.match(r.get("tagName", ""))
        if r.get("isDraft") or not m or int(m.group(1)) < MAJEURE_MINI:
            continue
        if "assets" in r and not fichiers_joints(set(r["assets"])):
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


def _images(source: Path, dest: Path) -> None:
    """Copie les images de docs/images/ sous leur nom publié."""
    dest.mkdir(parents=True, exist_ok=True)
    for nom, fichier in IMAGES.items():
        if not (source / fichier).is_file():
            raise SystemExit(f"image absente : {source / fichier}")
        shutil.copyfile(source / fichier, dest / nom)


def plan_du_site(site: Path) -> str:
    """sitemap.xml : une entrée par page HTML, avec les images qu'elle affiche."""
    lignes = ['<?xml version="1.0" encoding="UTF-8"?>',
              '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"'
              ' xmlns:image="http://www.google.com/schemas/sitemap-image/1.1">']
    for page in sorted(site.rglob("*.html")):
        relatif = page.relative_to(site).as_posix()
        if relatif.rsplit("/", 1)[-1].startswith("google"):  # fichier de vérification de Search Console
            continue
        dossier = relatif.removesuffix("index.html")
        lignes.append(f"  <url><loc>{escape(SITE + dossier)}</loc>")
        vues = []
        for src in IMAGE_DE_PAGE.findall(page.read_text(encoding="utf-8")):
            image = SITE + "images/" + src.rsplit("images/", 1)[1]
            if image not in vues:
                vues.append(image)
                lignes.append(f"    <image:image><image:loc>{escape(image)}</image:loc></image:image>")
        lignes.append("  </url>")
    lignes.append("</urlset>")
    return "\n".join(lignes) + "\n"


def assembler(web: Path, assets: Path, stable: str | None, beta: str | None, sortie: Path,
              images: Path | None = None) -> dict:
    """Écrit le site complet dans `sortie` ; renvoie le contenu de versions.json."""
    if sortie.exists():
        shutil.rmtree(sortie)
    shutil.copytree(web, sortie)
    if images is not None:
        _images(images, sortie / "images")
    versions = {canal: _canal(assets, tag, sortie / canal) if tag else None
                for canal, tag in (("stable", stable), ("beta", beta))}
    (sortie / "versions.json").write_text(json.dumps(versions, indent=2) + "\n", encoding="utf-8")
    (sortie / "sitemap.xml").write_text(plan_du_site(sortie), encoding="utf-8")
    return versions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sous = parser.add_subparsers(dest="action", required=True)
    p = sous.add_parser("choisir", help="affiche stable=<tag> et beta=<tag> (GITHUB_OUTPUT)")
    p.add_argument("--releases", type=Path, required=True)
    p = sous.add_parser("assembler", help="construit le site")
    p.add_argument("--web", type=Path, required=True)
    p.add_argument("--assets", type=Path, required=True, help="un sous-dossier par tag")
    p.add_argument("--images", type=Path, help="docs/images (images de la vitrine)")
    p.add_argument("--stable", default="")
    p.add_argument("--beta", default="")
    p.add_argument("--sortie", type=Path, required=True)
    args = parser.parse_args()

    if args.action == "choisir":
        tags = choisir(json.loads(args.releases.read_text(encoding="utf-8")))
        for canal, tag in tags.items():
            print(f"{canal}={tag or ''}")
        return 0
    versions = assembler(args.web, args.assets, args.stable or None, args.beta or None, args.sortie,
                         args.images)
    print(json.dumps(versions, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
