# -*- coding: utf-8 -*-
"""tools/publication/archive_ha.py — L'archive Home Assistant d'une release : `tab5_home_assistant.zip`.

Depuis le 28/09/2026 (ADR-0024), les fichiers Home Assistant du dépôt n'ont plus de
placeholder : ils s'installent tels quels. Cette archive les rassemble dans l'arborescence
du dossier `config/` de Home Assistant, pour une installation sans dépôt ni Python :
décompresser dans `config/`, ajouter la ligne des packages à configuration.yaml,
redémarrer, puis choisir ses sources dans les listes « Tab5 · … » (docs/installation.md).

    packages/*.yaml                                 (HomeAssistant_Config/packages/)
    custom_templates/*.jinja                        (HomeAssistant_Config/custom_templates/)
    blueprints/automation/tab5/tab5_emplacements.yaml
    tab5_optionnel/*.yaml                           (HomeAssistant_Config/optionnel/ : à copier
                                                     dans packages/ seulement au besoin)
    LISEZMOI-Tab5.txt                               (ces étapes, en français et en anglais)

Version : la seule ligne des packages qui finit par le marqueur `# >>> version de l'archive`
(capteur « Tab5 · version des fichiers HA », packages/tab5_health.yaml) prend la version de la
release à la place de « dépôt » : HA compare ainsi ses fichiers au firmware de la tablette et
prévient s'ils sont plus anciens (garde (f)). Un ancien tag, sans marqueur, s'archive tel quel.

Contrôles : un fichier qui contient encore un placeholder (`VOTRE_…`, versions ≤ 3.1) ne
s'installe pas tel quel → code de sortie 3 et rien n'est écrit (le workflow n'envoie
alors pas d'archive, sans échouer : cas d'un ancien tag republié). L'archive est
reproductible : ordre et dates des entrées fixes.

Usage (dans .github/workflows/publication.yml) :
    python tools/publication/archive_ha.py --version 3.2.0 --sortie publie-ha
    python tools/publication/archive_ha.py --version 3.2.0 --sortie publie-ha --base <tag>/HomeAssistant_Config
"""
from __future__ import annotations

import argparse
import re
import sys
import zipfile
from pathlib import Path

RACINE = Path(__file__).resolve().parent.parent.parent
HA_DIR = RACINE / "HomeAssistant_Config"
NOM = "tab5_home_assistant.zip"

# (dossier source relatif à HomeAssistant_Config/, motif, dossier dans l'archive)
CONTENU = (
    ("packages", "*.yaml", "packages"),
    ("custom_templates", "*.jinja", "custom_templates"),
    ("blueprints/automation/tab5", "*.yaml", "blueprints/automation/tab5"),
    ("optionnel", "*.yaml", "tab5_optionnel"),
)
LISEZMOI = "LISEZMOI-Tab5.txt"
PLACEHOLDER = re.compile(r"VOTRE_[A-Z]")
# Valeur de la version des fichiers (voir le docstring), remplacée par celle de la release.
# Fin de ligne CRLF tolérée : un checkout Windows (core.autocrlf) en a.
MARQUEUR_VERSION = re.compile(r"\"dépôt\"(?=  # >>> version de l'archive\r?$)", re.M)
# Date fixe des entrées (zip reproductible) : 1er janvier 2026.
DATE = (2026, 1, 1, 0, 0, 0)

TEXTE_LISEZMOI = """\
M5Stack Tab5 — Home Assistant, version {version}
https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/docs/installation.md

FRANÇAIS
1. Décompressez cette archive dans le dossier config/ de Home Assistant (celui de
   configuration.yaml) : packages/, custom_templates/ et blueprints/ s'y ajoutent.
2. Dans configuration.yaml, la SEULE ligne de YAML à écrire (si elle n'y est pas déjà) :
     homeassistant:
       packages: !include_dir_named packages
3. Outils de développement → YAML → « Vérifier la configuration », puis redémarrez
   Home Assistant.
4. Paramètres → Appareils et services → Entités, cherchez « Tab5 · » et choisissez
   vos sources : prévisions, pluie, vigilances, agenda de travail, rendez-vous,
   anniversaires, jours fériés, téléphone, capteur de présence, TV Samsung (et son
   adresse). Ce qui n'est pas choisi reste simplement absent de l'écran.
5. Vos appareils (lumières, clim, TV…) : Paramètres → Automatisations et scènes →
   Blueprints →
   « Tab5 — emplacements de l'écran · screen slots » → Créer une automatisation.
6. Le tableau de bord de la tablette (facultatif) : Paramètres → Tableaux de bord →
   « Ajouter un tableau de bord » → « Nouveau tableau de bord à partir de zéro »,
   titre « Tab5 ». Outils de développement → Modèle : remplacez le contenu par
     {{% from 'tab5_dashboard.jinja' import tab5_dashboard %}}{{{{ tab5_dashboard() }}}}
   et copiez le résultat. Tableau de bord Tab5 → crayon → ⋮ → « Éditeur de
   configuration brute » : remplacez tout par ce résultat, enregistrez.
tab5_optionnel/ : un volet qui ne signale pas sa course ? Copiez
volet_serre_tracking.yaml dans packages/, puis choisissez-le dans
« Tab5 · volet à course simulée ». Sinon, ignorez ce dossier.

ENGLISH
1. Unzip this archive into Home Assistant's config/ folder (the one holding
   configuration.yaml): packages/, custom_templates/ and blueprints/ are added.
2. In configuration.yaml, the ONLY YAML line to write (unless it is already there):
     homeassistant:
       packages: !include_dir_named packages
3. Developer tools → YAML → "Check configuration", then restart Home Assistant.
4. Settings → Devices & services → Entities, search "Tab5 ·" and pick your sources:
   forecasts, rain, warnings, work calendar, appointments, birthdays, public
   holidays, phone, presence sensor, Samsung TV (and its address). Anything left
   unset simply stays off the screen.
5. Your devices (lights, climate, TV…): Settings → Automations & scenes →
   Blueprints → "Tab5 — emplacements de l'écran · screen slots" → Create automation.
6. The tablet's dashboard (optional): Settings → Dashboards → "Add dashboard" →
   "New dashboard from scratch", title "Tab5". Developer tools → Template: replace
   the content with
     {{% from 'tab5_dashboard.jinja' import tab5_dashboard %}}{{{{ tab5_dashboard() }}}}
   and copy the result. Tab5 dashboard → pencil → ⋮ → "Raw configuration editor":
   replace everything with that result, save.
tab5_optionnel/: a shutter that doesn't report its travel? Copy
volet_serre_tracking.yaml into packages/, then pick it in
"Tab5 · volet à course simulée". Otherwise ignore this folder.
"""


def fichiers(base: Path = HA_DIR) -> list[tuple[Path, str]]:
    """(fichier source, chemin dans l'archive), dans l'ordre de l'archive."""
    liste: list[tuple[Path, str]] = []
    for dossier, motif, cible in CONTENU:
        for source in sorted((base / dossier).glob(motif)):
            liste.append((source, f"{cible}/{source.name}"))
    return liste


def placeholders(base: Path = HA_DIR) -> list[str]:
    """Chemins (dans l'archive) des fichiers qui contiennent encore un placeholder.

    Le blueprint n'est pas lu : il n'en a jamais eu (il s'importe tel quel depuis le
    lot 6a), et sa description peut citer un ancien nom pour mémoire."""
    return [chemin for source, chemin in fichiers(base)
            if not chemin.startswith("blueprints/")
            and PLACEHOLDER.search(source.read_text(encoding="utf-8"))]


def construire(version: str, sortie: Path, base: Path = HA_DIR) -> Path:
    """Écrit `sortie/tab5_home_assistant.zip` ; renvoie son chemin."""
    if not re.fullmatch(r"\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?", version):
        raise SystemExit(f"version {version!r} : X.Y.Z[-suffixe] attendu")
    liste = fichiers(base)
    if not any(chemin.startswith("packages/") for _, chemin in liste):
        raise SystemExit(f"{base} : aucun package")
    sortie.mkdir(parents=True, exist_ok=True)
    archive = sortie / NOM
    entrees, marques = [], 0
    for source, chemin in liste:
        donnees = source.read_bytes()
        if chemin.startswith("packages/"):
            texte, n = MARQUEUR_VERSION.subn(f'"{version}"', donnees.decode("utf-8"))
            if n:
                donnees, marques = texte.encode("utf-8"), marques + n
        entrees.append((chemin, donnees))
    if marques > 1:
        raise SystemExit(f"{marques} lignes portent la version des fichiers : une seule attendue")
    entrees.append((LISEZMOI, TEXTE_LISEZMOI.format(version=version).encode("utf-8")))
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for chemin, donnees in entrees:
            info = zipfile.ZipInfo(chemin, date_time=DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, donnees)
    return archive


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--version", required=True, help="version de la release (ex. 3.2.0)")
    parser.add_argument("--sortie", type=Path, required=True, help="dossier où écrire l'archive")
    parser.add_argument("--base", type=Path, default=HA_DIR,
                        help="dossier HomeAssistant_Config à archiver (défaut : celui du dépôt)")
    args = parser.parse_args()
    restes = placeholders(args.base)
    if restes:
        print(f"::warning::{len(restes)} fichier(s) avec des placeholders ({', '.join(restes[:5])}) : "
              "version d'avant l'ADR-0024, pas d'archive Home Assistant.")
        return 3
    archive = construire(args.version, args.sortie, args.base)
    with zipfile.ZipFile(archive) as z:
        for nom in z.namelist():
            print("  -", nom)
    print(f"{archive} ({archive.stat().st_size} octets)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
