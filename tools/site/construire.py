# -*- coding: utf-8 -*-
"""tools/site/construire.py — Documentation du site GitHub Pages, en anglais et en français (ADR-0030).

Les fichiers de docs/ restent la seule source : lus tels quels sur GitHub, ils deviennent
aussi les pages du site, sous en/ et fr/. Le README du dépôt en est la page d'accueil (la
racine du site renvoie vers en/ ou fr/). Ce script prépare les pages de chaque langue,
puis MkDocs (thème Material, sans autre plugin que sa recherche) les met en page :

  1. menu : tools/site/menu.yml, titres anglais et français côte à côte ;
  2. langues : un fichier bilingue est coupé à « ## Version Française » (l'anglais avant,
     le français après) ; un fichier d'une seule langue (`langue:` dans le menu) est repris
     sur l'autre site avec un encart qui le dit ;
  3. liens : une page publiée → sa page sur le site, avec son ancre ; une image de docs/ →
     copiée avec la page ; tout autre fichier du dépôt → github.com (blob/main). Un lien
     vers un fichier absent fait échouer la construction ;
  4. alertes de GitHub (« > [!WARNING] ») → encarts de Material, titrés dans la langue ;
     description de la page (premier paragraphe), pour les moteurs de recherche ;
  5. mkdocs.yml de la langue, qui hérite de tools/site/mkdocs.yml, puis construction en
     mode strict : un lien ou une ancre cassés font échouer.

Le travail est ici plutôt que dans des plugins : Material for MkDocs est en maintenance
depuis novembre 2025 ; un autre générateur qui lit mkdocs.yml n'aura qu'à mettre en page du
Markdown ordinaire.

Usage :
    pip install -r tools/site/requirements.txt
    python tools/site/construire.py --sortie build/doc        # build/doc/en, build/doc/fr, build/doc/404.html
    python tools/site/construire.py --sortie build/doc --adresse http://localhost:8766/   # aperçu local
"""
from __future__ import annotations

import argparse
import posixpath
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from urllib.parse import quote, unquote

import yaml

RACINE = Path(__file__).resolve().parents[2]
ICI = Path(__file__).resolve().parent
SITE = "https://axellum.github.io/M5-Tab5-ESPHome-LVGL/"
DEPOT = "https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/"
LANGUES = ("en", "fr")
NOMS_LANGUES = {"en": "English", "fr": "Français"}
# Page d'accueil du site : le README du dépôt, le même texte sur GitHub et sur le site
# (la racine du site, web/index.html, renvoie vers en/ ou fr/).
# docs/README.md (le sommaire de la documentation) devient la page « documentation/ ».
ACCUEIL = "README.md"
SOMMAIRE = "docs/README.md"
# <title> des pages d'accueil (les autres : « titre de la page - nom du site »).
TITRES_ACCUEIL = {
    "en": "M5Stack Tab5 Home Assistant wall screen — ESPHome and LVGL firmware",
    "fr": "M5Stack Tab5, écran mural Home Assistant — firmware ESPHome et LVGL",
}
# Image de partage du site (pages.py la publie sous ce nom).
IMAGE_PARTAGE = "images/m5stack-tab5-home-assistant-screen-card.jpg"

MARQUE_FR = re.compile(r"^## Version [Ff]ran[çc]aise[ \t]*\r?$", re.M)
ENTETE_EN = re.compile(r"^## English\b[^\n]*\n", re.M)
# Ancre de la partie française d'un fichier bilingue sur GitHub.
ANCRE_FR = re.compile(r"^version-fran(ç|c|%C3%A7)aise$", re.I)
FICHIERS_COPIES = {".png", ".jpg", ".jpeg", ".webp", ".svg", ".gif", ".mp4", ".webm"}
CLOTURE = re.compile(r"^\s*(```|~~~)")
CODE_EN_LIGNE = re.compile(r"(`+)(?:(?!\1).)+?\1")
LIEN_MD = re.compile(r"(\]\()(<[^>\n]*>|[^)\s]+)")
LIEN_HTML = re.compile(r'(\b(?:src|href)=")([^"]+)(")')
ABSOLU = re.compile(r"^(?:[a-z][a-z0-9+.-]*:|//)", re.I)
# Bloc lu sur GitHub seulement (« ces pages sont aussi un site… »).
HORS_SITE = re.compile(r"^<!-- hors-site -->\n.*?^<!-- /hors-site -->\n", re.M | re.S)

DESCRIPTIONS = {
    "en": "Documentation of the M5Stack Tab5 Home Assistant wall screen: install, use, set up "
          "and understand the ESPHome and LVGL firmware.",
    "fr": "Documentation de l'écran mural Home Assistant pour la M5Stack Tab5 : installer, "
          "utiliser, régler et comprendre le firmware ESPHome et LVGL.",
}
ENCART = {
    # (langue du site, langue de la page) → encart en tête de page
    ("en", "fr"): '!!! note "Page in French"\n    This page is only written in French.\n',
    ("fr", "en"): '!!! note "Page en anglais"\n    Cette page n\'existe qu\'en anglais.\n',
}
# Alerte de GitHub (« > [!WARNING] » puis des lignes « > … ») → encart de Material, titré
# dans la langue du site : un encadré sur GitHub comme sur le site.
ALERTE = re.compile(r"^> \[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\][ \t]*$")
ALERTES = {
    "NOTE": ("note", {"en": "Note", "fr": "Note"}),
    "TIP": ("tip", {"en": "Tip", "fr": "Astuce"}),
    "IMPORTANT": ("info", {"en": "Important", "fr": "Important"}),
    "WARNING": ("warning", {"en": "Warning", "fr": "Attention"}),
    "CAUTION": ("danger", {"en": "Caution", "fr": "Prudence"}),
}
THEME = {
    "en": ("Switch to dark mode", "Switch to light mode"),
    "fr": ("Passer en mode sombre", "Passer en mode clair"),
}
PIED = {
    "en": "MIT licence · shared in case it is useful · made with the help of AI",
    "fr": "Licence MIT · partagé au cas où ça serve · fait avec l'aide de l'IA",
}


class ErreurSite(Exception):
    """Source du site incohérente (lien vers un fichier absent, page en double…)."""


@dataclass
class Page:
    source: str               # chemin dans le dépôt, « docs/installation.md »
    titres: dict              # {"en": …, "fr": …} : titres du menu
    langue: str | None = None  # None = bilingue ; "en" / "fr" = une seule langue
    h1_fr: str | None = None   # titre de la page française, s'il diffère de celui du menu


@dataclass
class Plan:
    """Le menu lu dans menu.yml : l'arbre, et les pages par fichier source."""
    arbre: list
    pages: dict = field(default_factory=dict)


# --- Menu --------------------------------------------------------------------------

def _titre_h1(chemin: Path) -> str:
    m = re.search(r"^# (.+)$", chemin.read_text(encoding="utf-8"), re.M)
    if not m:
        raise ErreurSite(f"{chemin} : pas de titre « # »")
    return m.group(1).strip()


def lire_menu(fichier: Path = ICI / "menu.yml") -> Plan:
    plan = Plan(arbre=yaml.safe_load(fichier.read_text(encoding="utf-8")))

    def ajouter(source: str, titres: dict, langue: str | None, h1_fr: str | None = None) -> None:
        if source in plan.pages:
            raise ErreurSite(f"{source} : deux fois dans le menu")
        if not (RACINE / source).is_file():
            raise ErreurSite(f"{source} : fichier absent")
        if not source.startswith("docs/") and source != ACCUEIL:
            raise ErreurSite(f"{source} : seules les pages de docs/ et le README sont publiés")
        plan.pages[source] = Page(source, titres, langue, h1_fr)

    def parcourir(entrees: list) -> None:
        for e in entrees:
            if not all(e.get(lg) for lg in LANGUES):
                raise ErreurSite(f"entrée du menu sans titre anglais ou français : {e}")
            if "page" in e:
                ajouter(e["page"], {lg: e[lg] for lg in LANGUES}, e.get("langue"), e.get("h1_fr"))
            if "dossier" in e:
                for f in sorted((RACINE / e["dossier"]).glob("*.md")):
                    if f.name != "README.md":
                        titre = _titre_h1(f)
                        ajouter(f.relative_to(RACINE).as_posix(), {lg: titre for lg in LANGUES}, e.get("langue"))
            if "pages" in e:
                parcourir(e["pages"])

    parcourir(plan.arbre)
    return plan


def chemin_site(source: str) -> str:
    """docs/installation.md → installation.md ; README.md (le dépôt) → index.md ;
    docs/README.md → documentation.md ; docs/installation/README.md → installation/index.md."""
    if source == ACCUEIL:
        return "index.md"
    if source == SOMMAIRE:
        return "documentation.md"
    relatif = posixpath.relpath(source, "docs")
    dossier, nom = posixpath.split(relatif)
    if nom == "README.md":
        nom = "index.md"
    return posixpath.join(dossier, nom) if dossier else nom


def url_page(source: str) -> str:
    """Adresse d'une page sous la racine de sa langue (MkDocs, use_directory_urls)."""
    chemin = chemin_site(source).removesuffix(".md")
    if chemin == "index":
        return ""
    if chemin.endswith("/index"):
        return chemin.removesuffix("index")
    return chemin + "/"


def nav(plan: Plan, langue: str) -> list:
    def entree(e: dict):
        titre = e[langue]
        if "lien" in e:
            return {titre: e["lien"]}
        if "pages" in e or "dossier" in e:
            enfants = []
            if "page" in e:
                enfants.append(chemin_site(e["page"]))
            if "dossier" in e:
                for f in sorted((RACINE / e["dossier"]).glob("*.md")):
                    if f.name != "README.md":
                        source = f.relative_to(RACINE).as_posix()
                        enfants.append({plan.pages[source].titres[langue]: chemin_site(source)})
            enfants += [entree(x) for x in e.get("pages", [])]
            return {titre: enfants}
        return {titre: chemin_site(e["page"])}
    return [entree(e) for e in plan.arbre]


# --- Langues -----------------------------------------------------------------------

def _rognure(lignes: list[str]) -> list[str]:
    """Retire lignes vides et filets « --- » au début et à la fin."""
    while lignes and lignes[0].strip() in ("", "---"):
        lignes.pop(0)
    while lignes and lignes[-1].strip() in ("", "---"):
        lignes.pop()
    return lignes


def separer(texte: str) -> dict[str, str] | None:
    """Partie anglaise et partie française d'un fichier bilingue (None : une seule langue).

    L'anglais garde le « # » du fichier, sans la ligne « ## English · [Français](…) » ; le
    français n'a pas de « # » : construire_langue() lui met le titre français du menu."""
    m = MARQUE_FR.search(texte)
    if not m:
        return None
    en = ENTETE_EN.sub("", texte[:m.start()], count=1).splitlines()
    titre = [en.pop(0)] if en and en[0].startswith("# ") else []
    en = titre + [""] + _rognure(en) if titre else _rognure(en)
    fr = _rognure(texte[m.end():].splitlines())
    return {"en": "\n".join(en) + "\n", "fr": "\n".join(fr) + "\n"}


# --- Ancres ------------------------------------------------------------------------

TITRE = re.compile(r"^(#{2,6}) +(.+?) *#*$")
META = re.compile(r"^\*\*[^*]{1,30}:\*\*")


def _slug(texte: str) -> str:
    """Ancre d'un titre, comme `toc` (pymdownx.slugs, accents gardés) et comme GitHub."""
    from pymdownx.slugs import slugify
    texte = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", texte)   # [lien](…) → lien
    texte = re.sub(r"<[^>]+>", "", texte).replace("`", "")
    texte = re.sub(r"(\*\*|__|\*|_)(\S.*?\S|\S)\1", r"\2", texte)
    return slugify(case="lower")(texte, "-")


def _ancres(texte: str) -> list[str]:
    ancres, dans_bloc = [], False
    for ligne in texte.splitlines():
        if CLOTURE.match(ligne):
            dans_bloc = not dans_bloc
        elif not dans_bloc and (m := TITRE.match(ligne)):
            ancres.append(_slug(m.group(2)))
    return ancres


_ANCRES: dict[str, dict[str, list[str]]] = {}


def traduire_ancre(source: str, ancre: str, langue: str) -> str:
    """Ancre d'un titre de l'autre langue → le titre au même rang dans la page de `langue`.

    Sur GitHub, une partie française qui vise « #black-screen-… » tombe sur le titre anglais
    du même fichier ; sur le site, la page française n'a que ses titres français. Les deux
    parties d'un fichier bilingue ont leurs titres dans le même ordre : on prend le même rang.
    Une ancre introuvable reste telle quelle (le mode strict la signale)."""
    if source not in _ANCRES:
        texte = HORS_SITE.sub("", (RACINE / source).read_text(encoding="utf-8").replace("\r\n", "\n"))
        parties = separer(texte)
        _ANCRES[source] = {lg: _ancres(parties[lg]) for lg in LANGUES} if parties else {}
    par_langue = _ANCRES[source]
    if not par_langue or ancre in par_langue[langue]:
        return ancre
    autre = par_langue["fr" if langue == "en" else "en"]
    if ancre in autre and len(autre) == len(par_langue[langue]):
        return par_langue[langue][autre.index(ancre)]
    return ancre


# --- Liens -------------------------------------------------------------------------

def _hors_code(texte: str, remplacer) -> str:
    """Applique `remplacer(ligne)` hors des blocs de code et des `code` en ligne."""
    sortie, dans_bloc = [], False
    for ligne in texte.split("\n"):
        if CLOTURE.match(ligne):
            dans_bloc = not dans_bloc
            sortie.append(ligne)
            continue
        if dans_bloc:
            sortie.append(ligne)
            continue
        morceaux, debut = [], 0
        for m in CODE_EN_LIGNE.finditer(ligne):
            morceaux.append(remplacer(ligne[debut:m.start()]))
            morceaux.append(m.group(0))
            debut = m.end()
        morceaux.append(remplacer(ligne[debut:]))
        sortie.append("".join(morceaux))
    return "\n".join(sortie)


@dataclass
class Liens:
    """Réécrit les liens d'une page ; garde la liste des images à copier."""
    plan: Plan
    langue: str
    adresse: str
    fichiers: set = field(default_factory=set)

    def cible(self, url: str, source: str, html: bool) -> str:
        if url.startswith("<") and url.endswith(">"):
            return "<" + self.cible(url[1:-1], source, html) + ">"
        if ABSOLU.match(url):
            return url
        chemin, _, ancre = url.partition("#")
        if not chemin:
            # « [Français](#version-française) » : la même page dans l'autre langue.
            if ANCRE_FR.match(ancre):
                return self.adresse + "fr/" + url_page(source)
            return "#" + traduire_ancre(source, unquote(ancre), self.langue) if ancre else url
        cible = posixpath.normpath(posixpath.join(posixpath.dirname(source), unquote(chemin)))
        if cible.startswith("../"):
            raise ErreurSite(f"{source} : lien hors du dépôt : {url}")
        page_courante = chemin_site(source)
        if cible in self.plan.pages:
            if ANCRE_FR.match(ancre):
                # Partie française d'une autre page : sa page française.
                if self.langue == "en":
                    return self.adresse + "fr/" + url_page(cible)
                ancre = ""
            ancre = traduire_ancre(cible, unquote(ancre), self.langue) if ancre else ""
            suffixe = "#" + ancre if ancre else ""
            if html:  # le HTML brut n'est pas réécrit par MkDocs : adresse finale
                return posixpath.relpath(url_page(cible) or ".", url_page(source) or ".") + "/" + suffixe
            return posixpath.relpath(chemin_site(cible), posixpath.dirname(page_courante) or ".") + suffixe
        if not (RACINE / cible).exists():
            raise ErreurSite(f"{source} : lien vers un fichier absent : {url}")
        if cible.startswith("docs/") and posixpath.splitext(cible)[1].lower() in FICHIERS_COPIES:
            self.fichiers.add(cible)
            depuis = (url_page(source) or ".") if html else (posixpath.dirname(page_courante) or ".")
            return posixpath.relpath(chemin_site(cible), depuis) + ("#" + ancre if ancre else "")
        genre = "tree" if (RACINE / cible).is_dir() else "blob"
        return f"{DEPOT}{genre}/main/{quote(cible)}" + ("#" + ancre if ancre else "")

    def page(self, texte: str, source: str) -> str:
        def remplacer(morceau: str) -> str:
            morceau = LIEN_MD.sub(lambda m: m.group(1) + self.cible(m.group(2), source, False), morceau)
            return LIEN_HTML.sub(lambda m: m.group(1) + self.cible(m.group(2), source, True) + m.group(3), morceau)
        return _hors_code(texte, remplacer)


# --- Description -------------------------------------------------------------------

def _texte_simple(texte: str) -> str:
    """Markdown d'un paragraphe → texte : liens, images, emphase et `code` retirés."""
    texte = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", texte)
    texte = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", texte)
    codes = []
    texte = CODE_EN_LIGNE.sub(lambda m: codes.append(m.group(0).strip("`")) or f"\x00{len(codes) - 1}\x00", texte)
    texte = re.sub(r"\*\*|__|(?<!\w)[*_](?=\S)|(?<=\S)[*_](?!\w)", "", texte)
    texte = re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], texte)
    return re.sub(r"\s+", " ", texte).strip()


def description(texte: str, limite: int = 160, minimum: int = 60) -> str:
    """Premier paragraphe de texte d'au moins `minimum` caractères (ni titre, ni tableau, ni
    image, ni code, ni encart) ; à défaut le plus long. « **Status:** Accepted » d'un ADR
    est trop court : on prend la suite."""
    paragraphes, courant, dans_bloc = [], [], False
    for ligne in texte.splitlines() + [""]:
        brute = ligne.strip()
        if CLOTURE.match(ligne):
            dans_bloc = not dans_bloc
            continue
        if dans_bloc or ligne.startswith("    "):  # code, ou corps d'un encart « !!! »
            continue
        if not brute or brute.startswith(("#", "---", "|", "!", "<")):
            # Un bloc « **Status:** … / **Date:** … » (ADR) est une fiche, pas un résumé.
            if courant and not all(META.match(l) for l in courant):
                paragraphes.append(_texte_simple(" ".join(courant)))
            courant = []
            continue
        courant.append(brute.lstrip("> ").strip())
    texte = next((p for p in paragraphes if len(p) >= minimum), max(paragraphes, key=len, default=""))
    if len(texte) > limite:
        texte = texte[:limite].rsplit(" ", 1)[0].rstrip(",;:—-") + "…"
    return texte


# --- Construction ------------------------------------------------------------------

def contenu(page: Page, langue: str) -> str:
    """Texte Markdown d'une page dans une langue (avant réécriture des liens)."""
    texte = (RACINE / page.source).read_text(encoding="utf-8").replace("\r\n", "\n")
    texte = HORS_SITE.sub("", texte)
    if page.langue:
        if separer(texte):
            raise ErreurSite(f"{page.source} : marqué `langue: {page.langue}` mais bilingue")
        encart = ENCART.get((langue, page.langue))
        if encart:
            lignes = texte.split("\n")
            rang = next((i for i, l in enumerate(lignes) if l.startswith("# ")), -1)
            lignes[rang + 1:rang + 1] = ["", encart]
            texte = "\n".join(lignes)
        return alertes(texte, langue)
    parties = separer(texte)
    if not parties:
        raise ErreurSite(f"{page.source} : ni « ## Version Française » ni `langue:` dans le menu")
    if langue == "fr":
        return alertes(f"# {page.h1_fr or page.titres['fr']}\n\n" + parties["fr"], langue)
    return alertes(parties["en"], langue)


def alertes(texte: str, langue: str) -> str:
    """« > [!WARNING] » (alerte de GitHub) → « !!! warning "Attention" » (encart de Material)."""
    sortie, lignes, i, dans_bloc = [], texte.split("\n"), 0, False
    while i < len(lignes):
        ligne = lignes[i]
        i += 1
        if CLOTURE.match(ligne):
            dans_bloc = not dans_bloc
        m = None if dans_bloc else ALERTE.match(ligne)
        if not m:
            sortie.append(ligne)
            continue
        genre, titres = ALERTES[m.group(1)]
        sortie.append(f'!!! {genre} "{titres[langue]}"')
        while i < len(lignes) and lignes[i].startswith(">"):
            corps = lignes[i][1:].removeprefix(" ")
            sortie.append(f"    {corps}" if corps else "")
            i += 1
    return "\n".join(sortie)


def preparer_langue(plan: Plan, langue: str, sources: Path, adresse: str) -> set[str]:
    """Écrit les pages Markdown de la langue dans `sources` ; renvoie les fichiers copiés."""
    liens = Liens(plan, langue, adresse)
    for page in plan.pages.values():
        texte = liens.page(contenu(page, langue), page.source)
        tete = yaml.safe_dump({"description": description(texte) or DESCRIPTIONS[langue]},
                              allow_unicode=True, width=1000)
        cible = sources / chemin_site(page.source)
        cible.parent.mkdir(parents=True, exist_ok=True)
        cible.write_text(f"---\n{tete}---\n\n{texte}", encoding="utf-8")
    for fichier in liens.fichiers:
        cible = sources / chemin_site(fichier)
        cible.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(RACINE / fichier, cible)
    shutil.copytree(ICI / "assets", sources / "assets", dirs_exist_ok=True)
    return liens.fichiers


def configuration(plan: Plan, langue: str, sources: Path, sortie: Path, adresse: str) -> dict:
    sombre, clair = THEME[langue]
    return {
        "INHERIT": (ICI / "mkdocs.yml").as_posix(),
        "site_url": adresse + langue + "/",
        "site_description": DESCRIPTIONS[langue],
        "docs_dir": sources.as_posix(),
        "site_dir": sortie.as_posix(),
        "copyright": PIED[langue],
        "theme": {
            "language": langue,
            "custom_dir": (ICI / "theme").as_posix(),
            "palette": [
                {"media": "(prefers-color-scheme: light)", "scheme": "default", "primary": "custom",
                 "accent": "custom", "toggle": {"icon": "material/weather-night", "name": sombre}},
                {"media": "(prefers-color-scheme: dark)", "scheme": "slate", "primary": "custom",
                 "accent": "custom", "toggle": {"icon": "material/weather-sunny", "name": clair}},
            ],
        },
        "plugins": [{"search": {"lang": [langue]}}],
        "nav": nav(plan, langue),
        "extra": {
            "titre_accueil": TITRES_ACCUEIL[langue],
            "depot": DEPOT,
            "racine": adresse,
            "image_partage": SITE + IMAGE_PARTAGE,
            "alternate": [{"name": NOMS_LANGUES[lg], "link": adresse + lg + "/", "lang": lg} for lg in LANGUES],
        },
    }


def construire(sortie: Path, adresse: str = SITE, sources: Path | None = None, strict: bool = True) -> None:
    """Construit en/ et fr/ dans `sortie`, plus 404.html à la racine (celui de GitHub Pages)."""
    plan = lire_menu()
    if sortie.exists():
        shutil.rmtree(sortie)
    with tempfile.TemporaryDirectory() as tmp:
        base = sources or Path(tmp)
        for langue in LANGUES:
            src = base / langue
            if src.exists():
                shutil.rmtree(src)
            preparer_langue(plan, langue, src, adresse)
            fichier = base / f"mkdocs-{langue}.yml"
            fichier.write_text(yaml.safe_dump(configuration(plan, langue, src, sortie / langue, adresse),
                                              allow_unicode=True, sort_keys=False), encoding="utf-8")
            # Pas de --quiet : il coupe les avertissements avant que --strict ne les compte.
            commande = [sys.executable, "-m", "mkdocs", "build", "-f", str(fichier)]
            if strict:
                commande.append("--strict")
            resultat = subprocess.run(commande, capture_output=True, text=True, encoding="utf-8")
            avertissements = [l for l in (resultat.stdout + resultat.stderr).splitlines()
                              if l.startswith(("WARNING", "ERROR"))]
            if resultat.returncode or avertissements:
                raise ErreurSite(f"mkdocs ({langue}) :\n" + "\n".join(avertissements or [resultat.stderr]))
            # Un seul plan du site, celui de pages.py, pour tout le site.
            for nom in ("sitemap.xml", "sitemap.xml.gz"):
                (sortie / langue / nom).unlink(missing_ok=True)
    # GitHub Pages sert le 404.html de la racine : celui de l'anglais (adresses absolues).
    shutil.copyfile(sortie / "en" / "404.html", sortie / "404.html")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sortie", type=Path, required=True, help="dossier du site construit (vidé d'abord)")
    parser.add_argument("--adresse", default=SITE, help="adresse du site (aperçu local : http://localhost:<port>/)")
    parser.add_argument("--sources", type=Path, help="garde ici les pages préparées et les mkdocs.yml")
    args = parser.parse_args()
    adresse = args.adresse if args.adresse.endswith("/") else args.adresse + "/"
    try:
        construire(args.sortie.resolve(), adresse, args.sources.resolve() if args.sources else None)
    except ErreurSite as e:
        print(f"ERREUR : {e}", file=sys.stderr)
        return 1
    print(f"Site de documentation : {args.sortie / 'en'} et {args.sortie / 'fr'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
