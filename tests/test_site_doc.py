# -*- coding: utf-8 -*-
"""Site de documentation (ADR-0030) : docs/ → pages en/ et fr/ par tools/site/construire.py.

La construction complète (MkDocs en mode strict, ~3 s) est rejouée ici : un lien ou une
ancre cassés dans docs/ font échouer pytest, donc le job `python` de la PR, avant que le
déploiement du site ne s'arrête sur main.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import pytest
import yaml

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools" / "site"))
sys.path.insert(0, str(REPO / "tools" / "publication"))

import construire  # noqa: E402
import pages  # noqa: E402

# Fichiers de docs/ qui restent hors du site, exprès.
HORS_SITE = {
    "docs/INVENTAIRE_CONFIGS_TESTS.md": "inventaire interne des configurations de test",
    "docs/changelog/CHANGELOG-1.x.md": "historique des versions 1.x",
    "docs/press/forum_ha_en.md": "brouillon de message de forum",
    "docs/press/hackster.md": "brouillon d'article",
    "docs/press/hackster_paste_en.md": "brouillon d'article",
}


@pytest.fixture(scope="module")
def plan():
    return construire.lire_menu()


@pytest.fixture(scope="module")
def site(tmp_path_factory):
    sortie = tmp_path_factory.mktemp("doc") / "site"
    construire.construire(sortie)
    return sortie


def _pages_html(site: Path, langue: str) -> list[Path]:
    return sorted(p for p in (site / langue).rglob("*.html") if p.name != "404.html")


# --- Le menu et les sources ----------------------------------------------------------

def test_chaque_doc_est_publiee_ou_ecartee_exprès(plan):
    docs = {p.relative_to(REPO).as_posix() for p in (REPO / "docs").rglob("*.md")}
    publiees = set(plan.pages)
    assert not (publiees & set(HORS_SITE)), "page à la fois publiée et écartée"
    oubliees = docs - publiees - set(HORS_SITE)
    assert not oubliees, f"ajouter au menu (tools/site/menu.yml) ou à HORS_SITE : {sorted(oubliees)}"
    assert set(HORS_SITE) <= docs, "HORS_SITE cite un fichier qui n'existe plus"


def test_chaque_page_a_ses_deux_langues(plan):
    for page in plan.pages.values():
        texte = (REPO / page.source).read_text(encoding="utf-8")
        parties = construire.separer(texte)
        if page.langue:
            assert parties is None, f"{page.source} : bilingue, retirer `langue:` du menu"
            continue
        assert parties, f"{page.source} : ni « ## Version Française » ni `langue:` dans le menu"
        assert parties["en"].startswith("# "), page.source
        assert "## English" not in parties["en"] and "## English" not in parties["fr"], page.source
        assert len(parties["fr"]) > 200, f"{page.source} : partie française vide ?"


def test_menu_sans_doublon_et_titres_dans_les_deux_langues(plan):
    for langue in construire.LANGUES:
        nav = construire.nav(plan, langue)
        assert nav and all(len(e) == 1 for e in nav)


# --- Liens ---------------------------------------------------------------------------

def test_liens_reecrits(plan):
    en = construire.Liens(plan, "en", construire.SITE)
    fr = construire.Liens(plan, "fr", construire.SITE)
    source = "docs/screens.md"
    # Une autre page publiée : chemin relatif entre pages du site, ancre gardée.
    assert en.cible("installation.md#ota-updates", source, False) == "installation.md#ota-updates"
    # Une page de docs/decisions/ depuis docs/ et l'inverse.
    assert en.cible("decisions/0002-single-page-swipe-navigation.md", source, False) == \
        "decisions/0002-single-page-swipe-navigation.md"
    assert en.cible("../troubleshooting.md", "docs/decisions/0005-boot-delay-gpio-expander-reset.md", False) == \
        "../troubleshooting.md"
    # Une image : copiée avec la page.
    assert en.cible("images/tab5_photo_home.jpg", source, False) == "images/tab5_photo_home.jpg"
    assert "docs/images/tab5_photo_home.jpg" in en.fichiers
    # Un fichier du dépôt hors du site : sur GitHub.
    assert en.cible("../Tab5/README.md", source, False) == construire.DEPOT + "blob/main/Tab5/README.md"
    assert en.cible("../Tab5", source, False) == construire.DEPOT + "tree/main/Tab5"
    # Absolu : inchangé.
    assert en.cible("https://esphome.io/", source, False) == "https://esphome.io/"
    # « Version française » : la page française, sur l'autre site depuis l'anglais.
    assert en.cible("#version-française", source, False) == construire.SITE + "fr/screens/"
    assert en.cible("installation.md#version-française", source, False) == construire.SITE + "fr/installation/"
    assert fr.cible("installation.md#version-française", source, False) == "installation.md"
    # HTML brut (non réécrit par MkDocs) : adresse relative à la page finale.
    assert en.cible("images/tab5_photo_home.jpg", source, True) == "../images/tab5_photo_home.jpg"
    assert en.cible("installation.md", source, True) == "../installation/"
    assert en.cible("README.md", source, True) == "../"


def test_ancre_de_l_autre_langue_traduite_par_rang():
    """La partie française de screens.md vise un titre anglais de troubleshooting.md : sur le
    site, la page française n'a que ses titres français, au même rang."""
    assert construire.traduire_ancre("docs/troubleshooting.md", "black-screen-after-a-software-reboot-not-a-power-cycle",
                                     "fr") == "écran-noir-après-un-reboot-logiciel-pas-une-coupure-dalimentation"
    assert construire.traduire_ancre("docs/troubleshooting.md", "faux-positifs-à-connaître-ne-pas-re-corriger",
                                     "en") == "false-positives-worth-knowing-about-dont-fix-these-again"
    # Déjà dans la bonne langue, ou inconnue : inchangée (le mode strict signale la seconde).
    assert construire.traduire_ancre("docs/troubleshooting.md", "nexiste-pas", "fr") == "nexiste-pas"


def test_lien_vers_un_fichier_absent_refuse(plan):
    liens = construire.Liens(plan, "en", construire.SITE)
    with pytest.raises(construire.ErreurSite, match="absent"):
        liens.cible("nexiste_pas.md", "docs/screens.md", False)


def test_bloc_hors_site_retire(plan):
    page = plan.pages["docs/README.md"]
    for langue in construire.LANGUES:
        texte = construire.contenu(page, langue)
        assert "hors-site" not in texte and "aussi un site" not in texte and "also a website" not in texte


def test_description_sans_markdown():
    texte = "# Titre\n\n> **Envie ?** [`docs/demo_mode.md`](demo_mode.md) montre *tout* sur `tab5_*.cpp`.\n"
    assert construire.description(texte) == "Envie ? docs/demo_mode.md montre tout sur tab5_*.cpp."


# --- Construction --------------------------------------------------------------------

def test_mode_strict_attrape_une_ancre_cassee(tmp_path, monkeypatch, plan):
    """Contre-épreuve : la construction échoue sur une ancre qui n'existe pas."""
    petit = construire.Plan(arbre=[{"en": "Doc", "fr": "Doc", "page": "docs/README.md"},
                                   {"en": "Demo", "fr": "Démo", "page": "docs/demo_mode.md"}],
                            pages={k: plan.pages[k] for k in ("docs/README.md", "docs/demo_mode.md")})
    monkeypatch.setattr(construire, "lire_menu", lambda: petit)
    reel = construire.contenu
    monkeypatch.setattr(construire, "contenu",
                        lambda page, langue: reel(page, langue) + "\n[x](demo_mode.md#ancre-qui-n-existe-pas)\n")
    with pytest.raises(construire.ErreurSite, match="ancre-qui-n-existe-pas"):
        construire.construire(tmp_path / "site")


def test_site_construit_dans_les_deux_langues(site, plan):
    assert (site / "404.html").is_file()
    for langue in construire.LANGUES:
        pages_html = _pages_html(site, langue)
        assert len(pages_html) == len(plan.pages), langue
        assert not (site / langue / "sitemap.xml").exists(), "un seul plan du site : celui de pages.py"


@pytest.mark.parametrize("langue", construire.LANGUES)
def test_pages_referencables(site, langue):
    """Langue, titre, description, adresse canonique, même page dans l'autre langue, image
    de partage publiée par la vitrine."""
    for page in _pages_html(site, langue):
        chemin = page.relative_to(site / langue).as_posix().removesuffix("index.html")
        texte = page.read_text(encoding="utf-8")
        assert f'<html lang="{langue}"' in texte, page
        titre = re.search(r"<title>([^<]+)</title>", texte)
        assert titre and "M5Stack Tab5" in titre.group(1) and "Home Assistant" in titre.group(1), page
        description = re.search(r'<meta name="description" content="([^"]+)"', texte)
        assert description and 40 <= len(description.group(1)) <= 300, page
        assert f'<link rel="canonical" href="{construire.SITE}{langue}/{chemin}">' in texte, page
        for autre in construire.LANGUES:
            assert f'<link rel="alternate" hreflang="{autre}" href="{construire.SITE}{autre}/{chemin}">' in texte, page
            assert f'href="{construire.SITE}{autre}/{chemin}" hreflang="{autre}"' in texte, f"sélecteur : {page}"
        assert f'<meta property="og:image" content="{construire.SITE}{construire.IMAGE_PARTAGE}">' in texte, page
    assert construire.IMAGE_PARTAGE.removeprefix("images/") in pages.IMAGES


@pytest.mark.parametrize("langue", construire.LANGUES)
def test_images_des_pages_presentes(site, langue):
    for page in _pages_html(site, langue):
        for src in re.findall(r'<img\b[^>]*\bsrc="([^"]+)"', page.read_text(encoding="utf-8")):
            if src.startswith(("http:", "https:", "data:")):
                continue
            assert (page.parent / src).resolve().is_file(), f"{page} : {src}"


def test_assemblage_avec_la_documentation(site, tmp_path):
    sortie = tmp_path / "pages"
    pages.assembler(REPO / "web", tmp_path / "assets", None, None, sortie, REPO / "docs" / "images", site)
    for chemin in ("index.html", "install/index.html", "en/index.html", "fr/installation/index.html", "404.html"):
        assert (sortie / chemin).is_file(), chemin
    plan_xml = (sortie / "sitemap.xml").read_text(encoding="utf-8")
    locs = re.findall(r"<loc>([^<]+)</loc>", plan_xml)
    assert pages.SITE + "fr/installation/" in locs and pages.SITE + "en/" in locs
    assert not any(url.endswith("404.html") for url in locs)
    for url in re.findall(r"<image:loc>([^<]+)</image:loc>", plan_xml):
        assert (sortie / url.removeprefix(pages.SITE)).is_file(), url


def test_liens_de_la_vitrine_vers_la_documentation(site):
    """La vitrine et la page d'installation mènent à une page construite, et à une ancre qui
    y existe ; les liens `data-doc` (basculés en fr/ par la vitrine) existent dans les deux
    langues."""
    from urllib.parse import unquote
    vus = 0
    for page in sorted((REPO / "web").rglob("*.html")):
        texte = page.read_text(encoding="utf-8")
        dossier = page.relative_to(REPO / "web").parent.as_posix()
        for href in re.findall(r'href="((?:\.\./)*(?:en|fr)/[^"]*)"', texte):
            chemin, _, ancre = href.partition("#")
            cible = site / Path(dossier, chemin) / "index.html"
            cible = cible.resolve()
            assert cible.is_file(), f"{page.name} : {href}"
            if ancre:
                assert f'id="{unquote(ancre)}"' in cible.read_text(encoding="utf-8"), f"{page.name} : {href}"
            vus += 1
        for doc in re.findall(r'data-doc="([^"]*)"', texte):
            for langue in construire.LANGUES:
                assert (site / langue / doc / "index.html").is_file(), f"{page.name} : data-doc {doc}"
    assert vus > 20


def test_assemblage_refuse_une_doc_qui_ecraserait_le_site(tmp_path):
    doc = tmp_path / "doc"
    for nom in ("en", "fr", "stable"):
        (doc / nom).mkdir(parents=True)
    (doc / "404.html").write_text("x", encoding="utf-8")
    with pytest.raises(SystemExit, match="stable"):
        pages.assembler(REPO / "web", tmp_path / "assets", None, None, tmp_path / "pages", None, doc)


# --- Dépendances et déploiement -----------------------------------------------------

def test_mkdocs_et_material_figes_avec_empreintes():
    exigences, courante = {}, None
    for ligne in (REPO / "tools" / "site" / "requirements.txt").read_text(encoding="utf-8").splitlines():
        if re.match(r"^[A-Za-z0-9]", ligne):
            courante = ligne.split()[0]
            assert re.match(r"^[A-Za-z0-9_.\-]+==[0-9][^=\s]*$", courante), f"version non figée « {courante} »"
            exigences[courante] = 0
        elif courante and "--hash=sha256:" in ligne:
            exigences[courante] += 1
    assert exigences and all(exigences.values()), [p for p, n in exigences.items() if not n]
    dev = (REPO / "requirements-dev.txt").read_text(encoding="utf-8")
    for paquet in ("mkdocs", "mkdocs-material"):
        version = next(p for p in exigences if p.split("==")[0] == paquet)
        assert re.search(rf"^{re.escape(version)}\b", dev, re.M), f"requirements-dev.txt : {version}"


def test_site_yml_construit_la_documentation():
    flux = yaml.safe_load((REPO / ".github" / "workflows" / "site.yml").read_text(encoding="utf-8"))
    etapes = flux["jobs"]["pages"]["steps"]
    texte = "\n".join(str(e.get("run", "")) for e in etapes)
    assert "--require-hashes --only-binary :all: -r tools/site/requirements.txt" in texte
    assert "python tools/site/construire.py --sortie doc" in texte
    assert "--doc doc" in texte
    rang = [e.get("name") for e in etapes]
    assert rang.index("Documentation") < rang.index("Site")
