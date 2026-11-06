# -*- coding: utf-8 -*-
"""Site de documentation (ADR-0030) : docs/ → pages en/ et fr/ par tools/site/construire.py.

La construction complète (MkDocs en mode strict, ~3 s) est rejouée ici : un lien ou une
ancre cassés dans docs/ font échouer pytest, donc le job `python` de la PR, avant que le
déploiement du site ne s'arrête sur main.
"""
from __future__ import annotations

import html
import json
import posixpath
import re
from pathlib import Path

import pytest
import yaml

REPO = Path(__file__).resolve().parent.parent

import construire  # noqa: E402
import pages  # noqa: E402

# Fichiers de docs/ qui restent hors du site, exprès.
HORS_SITE = {
    "docs/INVENTAIRE_CONFIGS_TESTS.md": "inventaire interne des configurations de test",
    "docs/installation.md": "renvoi des anciens titres vers docs/installation/ (liens déjà publiés)",
    "docs/changelog/CHANGELOG-1.x.md": "historique des versions 1.x",
    "docs/changelog/CHANGELOG-2.x.md": "historique des versions 2.x",
    "docs/changelog/CHANGELOG-3.x.md": "historique des versions 3.x, de la 3.0.0 à l'avant-dernière mineure",
    "docs/press/forum_ha_en.md": "brouillon de message de forum",
    "docs/press/hackster.md": "brouillon d'article",
    "docs/press/hackster_paste_en.md": "brouillon d'article",
}


# Pages dont le <title> vient de `titre_onglet:` (menu.yml) sans nommer la Tab5.
TITRES_PROPRES = {"waveshare-7b/"}

# Les pages qui comptent : titre d'onglet et description écrits à la main (menu.yml, lot « SEO
# rapide » du 06/11/2026) au lieu du premier paragraphe (« Opens with… », « S'ouvre par… »).
PAGES_QUI_COMPTENT = (
    "docs/installation/README.md", "docs/installation/home-assistant-files.md", "docs/installation/flash.md",
    "docs/installation/wifi.md", "docs/installation/add-to-home-assistant.md", "docs/installation/sources.md",
    "docs/installation/devices.md", "docs/installation/dashboard.md",
    "docs/notice/README.md", "docs/notice/home.md", "docs/notice/lights.md", "docs/notice/climate.md",
    "docs/notice/energy.md", "docs/notice/voice.md", "docs/notice/weather.md", "docs/notice/music.md",
    "docs/notice/cameras.md", "docs/notice/alarm.md", "docs/notice/settings.md",
    "docs/voice_assistant.md", "docs/demo_mode.md", "docs/translations.md", "docs/troubleshooting.md",
    "docs/debugging.md", "docs/architecture.md", "docs/performance.md", "docs/consumption.md",
    "docs/related_projects.md", "docs/story.md",
)
JSON_LD = re.compile(r'<script type="application/ld\+json">(.*?)</script>', re.S)
# Un lien (<link> de l'en-tête, <a> du sélecteur de langue) qui annonce une version dans une langue.
LIEN_HREFLANG = re.compile(r"<(?:link|a)\b[^>]*\bhreflang=")


def _json_ld(texte: str) -> dict[str, dict]:
    """Les blocs JSON-LD de l'en-tête d'une page, par @type."""
    return {bloc["@type"]: bloc for bloc in (json.loads(b) for b in JSON_LD.findall(texte.partition("</head>")[0]))}


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
    assert en.cible("installation/updates.md#upgrading-from-31", source, False) ==         "installation/updates.md#upgrading-from-31"
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
    assert en.cible("installation/README.md#version-française", source, False) == construire.SITE + "fr/installation/"
    assert fr.cible("installation/README.md#version-française", source, False) == "installation/index.md"
    # HTML brut (non réécrit par MkDocs) : adresse relative à la page finale.
    assert en.cible("images/tab5_photo_home.jpg", source, True) == "../images/tab5_photo_home.jpg"
    assert en.cible("installation/README.md", source, True) == "../installation/"
    assert en.cible("../screens.md", "docs/installation/flash.md", True) == "../../screens/"
    # docs/README.md est la page « documentation/ » ; le README du dépôt, l'accueil.
    assert en.cible("README.md", source, True) == "../documentation/"
    assert en.cible("../README.md", source, True) == "../"
    assert en.cible("../README.md#note-on-ai", "docs/story.md", False) == "index.md#note-on-ai"
    assert en.cible("docs/notice/README.md", construire.ACCUEIL, False) == "notice/index.md"
    assert en.cible("docs/images/tab5_hero_4x3.jpg", construire.ACCUEIL, False) == "images/tab5_hero_4x3.jpg"


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


def test_alerte_github_devient_un_encart():
    """« > [!WARNING] » s'affiche en encadré sur GitHub ; sur le site, encart de Material
    titré dans la langue de la page. Dans un bloc de code, rien ne change."""
    texte = "> [!WARNING]\n> **Titre.** texte\n>\n> suite\n\nfin\n```\n> [!NOTE]\n```"
    assert construire.alertes(texte, "fr").split("\n") == [
        '!!! warning "Attention"', "    **Titre.** texte", "", "    suite", "", "fin", "```", "> [!NOTE]", "```"]
    assert construire.alertes("> [!TIP]\n> a", "en") == '!!! tip "Tip"\n    a'


def test_aucune_page_ne_renvoie_a_l_ancien_guide(plan):
    """docs/installation.md ne garde que des titres de renvoi, hors du site : une page publiée
    vise directement la page de docs/installation/ (sinon un aller-retour par GitHub)."""
    for page in plan.pages.values():
        texte = (REPO / page.source).read_text(encoding="utf-8")
        dossier = Path(page.source).parent
        for _, cible in construire.LIEN_MD.findall(texte):
            chemin = cible.strip("<>").split("#")[0]
            if chemin and not construire.ABSOLU.match(chemin):
                assert posixpath.normpath((dossier / chemin).as_posix()) != "docs/installation.md", \
                    f"{page.source} → {cible}"


def test_description_sans_markdown():
    texte = "# Titre\n\n> **Envie ?** [`docs/demo_mode.md`](demo_mode.md) montre *tout* sur `tab5_*.cpp`.\n"
    assert construire.description(texte) == "Envie ? docs/demo_mode.md montre tout sur tab5_*.cpp."


# --- Écrans et fonctions, coulisses des écrans, index du dépannage -------------------------

def _parties(source: str) -> dict[str, str]:
    texte = (REPO / source).read_text(encoding="utf-8").replace("\r\n", "\n")
    parties = construire.separer(texte)
    assert parties, source
    return parties


@pytest.mark.parametrize("source", ["docs/screens.md", "docs/screens_internals.md", "docs/troubleshooting.md"])
def test_autant_de_titres_en_anglais_qu_en_francais(source):
    """Les deux langues ont la même structure : une ancre de l'autre langue se traduit par
    rang (traduire_ancre), donc il faut autant de titres de chaque côté."""
    parties = _parties(source)
    assert len(construire._ancres(parties["en"])) == len(construire._ancres(parties["fr"])), source


def test_ecrans_lisibles_et_coulisses_sous_le_meme_titre():
    """screens.md est la page de l'utilisateur (environ 5 000 mots anglais ; elle en avait
    13 600) : le détail technique est dans screens_internals.md, rangé sous le titre de la
    section de screens.md qu'il détaille (une ancre citée sur l'une se retrouve sur l'autre)."""
    ecrans, coulisses = _parties("docs/screens.md"), _parties("docs/screens_internals.md")
    assert len(ecrans["en"].split()) <= 5500, "screens.md grossit : le détail va dans screens_internals.md"
    for langue in construire.LANGUES:
        ici, la = construire._ancres(ecrans[langue]), construire._ancres(coulisses[langue])
        inconnus = sorted(set(la) - set(ici))
        assert not inconnus, f"{langue} : titres des coulisses absents de screens.md : {inconnus}"


def test_ecrans_et_coulisses_ont_leur_titre_et_leur_description(plan):
    for source in ("docs/screens.md", "docs/screens_internals.md"):
        page = plan.pages[source]
        assert page.titre_onglet and page.resume, f"{source} : titre_onglet et description dans menu.yml"
        for langue in construire.LANGUES:
            assert len(page.titre_onglet[langue]) <= 65 and len(page.resume[langue]) <= 160, (source, langue)


def test_index_du_depannage_renvoie_a_chaque_incident():
    """Le tableau « symptôme → incident » en tête de troubleshooting.md, dans chaque langue :
    une ligne par incident, dans l'ordre de la page, vers une ancre qui existe."""
    parties = _parties("docs/troubleshooting.md")
    for langue in construire.LANGUES:
        ancres = construire._ancres(parties[langue])
        cibles = re.findall(r"^\| .+ \| \[.+\]\(#([^)]+)\) \|$", parties[langue], re.M)
        assert len(ancres) > 20, langue
        assert cibles == ancres[1:], f"{langue} : l'index doit citer chaque incident, une fois, dans l'ordre de la page"


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
def test_pages_referencables(site, plan, langue):
    """Langue, titre, description, adresse canonique, même page dans l'autre langue, image
    de partage publiée à la racine du site. Une page écrite dans une seule langue (les ADR en
    anglais, `arcade.md` en français) : son canonical est la page de sa langue d'origine, sur
    les deux sites, et elle n'a aucun hreflang."""
    par_adresse = {construire.url_page(p.source): p for p in plan.pages.values()}
    for page in _pages_html(site, langue):
        chemin = page.relative_to(site / langue).as_posix().removesuffix("index.html")
        texte = page.read_text(encoding="utf-8")
        voulue = par_adresse[chemin]
        origine = voulue.langue or langue
        assert f'<html lang="{langue}"' in texte, page
        titre = re.search(r"<title>([^<]+)</title>", texte)
        # Un `titre_onglet:` du menu nomme ce qu'on cherche (la carte 7B n'est pas une Tab5) ;
        # écrit à la main, il tient en 65 caractères et n'a plus le nom du site au bout.
        assert titre, page
        if voulue.titre_onglet:
            assert "Tab5" in titre.group(1) or chemin in TITRES_PROPRES, page
        else:
            assert "Home Assistant" in titre.group(1), page
            assert "M5Stack Tab5" in titre.group(1), page
        description = re.search(r'<meta name="description" content="([^"]+)"', texte)
        assert description and 40 <= len(description.group(1)) <= 300, page
        assert f'<link rel="canonical" href="{construire.SITE}{origine}/{chemin}">' in texte, page
        if voulue.langue:
            assert not LIEN_HREFLANG.search(texte), f"page d'une seule langue : aucun hreflang ({page})"
            # Le sélecteur de langue mène toujours à l'autre version, sans l'annoncer comme traduction.
            for autre in construire.LANGUES:
                assert f'href="{construire.SITE}{autre}/{chemin}"' in texte, f"sélecteur : {page}"
        else:
            for autre in construire.LANGUES:
                assert f'<link rel="alternate" hreflang="{autre}" href="{construire.SITE}{autre}/{chemin}">' in texte, page
                assert f'href="{construire.SITE}{autre}/{chemin}" hreflang="{autre}"' in texte, f"sélecteur : {page}"
        assert f'<meta property="og:image" content="{construire.SITE}{construire.IMAGE_PARTAGE}">' in texte, page
        assert f'<meta property="og:url" content="{construire.SITE}{origine}/{chemin}">' in texte, page
    assert construire.IMAGE_PARTAGE.removeprefix("images/") in pages.IMAGES


@pytest.mark.parametrize("langue", construire.LANGUES)
def test_pied_de_page_renvoie_au_site_de_l_auteur(site, langue):
    """Chaque page, 404 comprise, porte dans son pied le lien vers le site des projets de
    l'auteur, dans sa langue, en vrai lien (Material écrit `copyright` sans l'échapper)."""
    lien = f'<a href="{construire.SITE_AUTEUR[langue]}">Axel Duviella · auxfilsdesidees.fr</a>'
    pages_html = sorted((site / langue).rglob("*.html"))
    assert pages_html
    for page in pages_html:
        texte = page.read_text(encoding="utf-8")
        pied = re.search(r'<div class="md-copyright__highlight">(.*?)</div>', texte, re.S)
        assert pied and lien in pied.group(1), page


@pytest.mark.parametrize("langue", construire.LANGUES)
def test_titre_et_description_du_menu(site, plan, langue):
    """`titre_onglet:` et `description:` de menu.yml remplacent le <title>, og:title et la
    description tirés de la page (ce qu'affiche un moteur de recherche)."""
    vus = 0
    for page in plan.pages.values():
        if not (page.titre_onglet or page.resume):
            continue
        texte = (site / langue / construire.url_page(page.source) / "index.html").read_text(encoding="utf-8")
        texte = html.unescape(texte)
        if page.titre_onglet:
            assert f"<title>{page.titre_onglet[langue]}</title>" in texte, page.source
            assert f'<meta property="og:title" content="{page.titre_onglet[langue]}">' in texte, page.source
        if page.resume:
            assert f'<meta name="description" content="{page.resume[langue]}">' in texte, page.source
        vus += 1
    assert vus >= 2
    assert {construire.url_page(p.source) for p in plan.pages.values() if p.titre_onglet} >= TITRES_PROPRES


def test_titre_onglet_trop_long_refuse(tmp_path):
    menu = tmp_path / "menu.yml"
    menu.write_text(yaml.safe_dump([{"en": "Hardware", "fr": "Matériel", "page": "docs/hardware.md",
                                     "titre_onglet": {"en": "x" * 66, "fr": "y"}}]), encoding="utf-8")
    with pytest.raises(construire.ErreurSite, match="65"):
        construire.lire_menu(menu)


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


def test_liens_de_web_vers_la_documentation(site):
    """La racine et la page d'installation mènent à une page construite, et à une ancre qui
    y existe."""
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
    assert vus > 10


def test_accueil_du_site_est_le_readme(plan, site):
    """L'accueil des deux langues est le README du dépôt (un seul texte, sur GitHub et sur le
    site), avec son titre et la fiche JSON-LD du projet, sans ce qui ne sert que sur GitHub.
    La racine du site renvoie vers en/ ou fr/, et garde les deux liens sans JavaScript."""
    assert next(iter(plan.pages)) == construire.ACCUEIL, "premier du menu : l'accueil"
    assert construire.chemin_site(construire.ACCUEIL) == "index.md"
    assert construire.chemin_site(construire.SOMMAIRE) == "documentation.md"
    for langue in construire.LANGUES:
        texte = (site / langue / "index.html").read_text(encoding="utf-8")
        assert f"<title>{construire.TITRES_ACCUEIL[langue]}</title>" in texte
        assert '"@type": "SoftwareSourceCode"' in texte
        assert f'hreflang="x-default" href="{construire.SITE}"' in texte
        assert "img.shields.io" not in texte, "badges : hors-site"
    racine = (REPO / "web" / "index.html").read_text(encoding="utf-8")
    assert 'location.replace(code + "/"' in racine
    for langue in construire.LANGUES:
        assert f'href="{langue}/"' in racine
        assert f'hreflang="{langue}" href="{construire.SITE}{langue}/"' in racine


def test_assemblage_refuse_une_doc_qui_ecraserait_le_site(tmp_path):
    doc = tmp_path / "doc"
    for nom in ("en", "fr", "stable"):
        (doc / nom).mkdir(parents=True)
    (doc / "404.html").write_text("x", encoding="utf-8")
    with pytest.raises(SystemExit, match="stable"):
        pages.assembler(REPO / "web", tmp_path / "assets", None, None, tmp_path / "pages", None, doc)


# --- Moteurs de recherche : hreflang, plan du site ------------------------------------

def test_aucun_plan_du_site_demande_par_langue(site):
    """Material lisait en/<page>/sitemap.xml et fr/<page>/sitemap.xml à côté de chaque lien
    hreflang : des 404 sur chaque page, dans la console (audit Lighthouse du 27/10/2026). L'appel
    est retiré de son script ; ses deux autres lectures du plan (navigation instantanée,
    sélecteur de versions) restent éteintes."""
    for langue in construire.LANGUES:
        script = next((site / langue / "assets" / "javascripts").glob("bundle.*.min.js")).read_bytes()
        assert construire.APPEL_LANGUES_MATERIAL not in script, langue
        assert b"function ui({document$:e})" in script, "le reste du script de Material est intact"
    base = yaml.safe_load((REPO / "tools" / "site" / "mkdocs.yml").read_text(encoding="utf-8").replace(
        "!!python/object/apply:pymdownx.slugs.slugify", ""))
    assert "navigation.instant" not in base["theme"]["features"]
    assert "version" not in base.get("extra", {})


def test_script_de_material_inattendu_refuse(tmp_path):
    dossier = tmp_path / "assets" / "javascripts"
    dossier.mkdir(parents=True)
    (dossier / "bundle.0.min.js").write_bytes(b"autre version")
    with pytest.raises(construire.ErreurSite, match="APPEL_LANGUES_MATERIAL"):
        construire.sans_plan_par_langue(tmp_path)


def test_x_default_le_meme_dans_les_deux_langues(site):
    """Les deux versions d'une page annoncent le même x-default : la racine du site pour
    l'accueil (elle envoie vers en/ ou fr/ selon la langue du navigateur, web/index.html),
    sinon la page anglaise (le site est d'abord en anglais)."""
    uniques = {construire.url_page(p.source) for p in construire.lire_menu().pages.values() if p.langue}
    for page in _pages_html(site, "en"):
        relatif = page.relative_to(site / "en")
        chemin = relatif.as_posix().removesuffix("index.html")
        attendu = construire.SITE if chemin == "" else f"{construire.SITE}en/{chemin}"
        for langue in construire.LANGUES:
            texte = (site / langue / relatif).read_text(encoding="utf-8")
            if chemin in uniques:  # une seule langue : pas de hreflang, donc pas de x-default
                assert not LIEN_HREFLANG.search(texte), (langue, chemin)
                continue
            assert re.findall(r'hreflang="x-default" href="([^"]+)"', texte) == [attendu], (langue, chemin)
    racine = (REPO / "web" / "index.html").read_text(encoding="utf-8")
    assert re.findall(r'hreflang="x-default" href="([^"]+)"', racine) == [construire.SITE]


def test_pages_d_une_seule_langue_hors_du_plan_du_site_mais_en_ligne(site, plan, tmp_path):
    """Les ADR (anglais) sous fr/decisions/ et `arcade.md` (français) sous en/arcade/ sont le
    texte de l'original : même titre, rien à y indexer. Elles restent en ligne et accessibles
    (sélecteur de langue, menu), n'ont ni plan du site ni hreflang, et leur canonical mène à
    la page de la langue d'origine, qui seule est au plan."""
    uniques = [p for p in plan.pages.values() if p.langue]
    assert len(uniques) > 70 and {p.langue for p in uniques} == {"en", "fr"}
    sortie = tmp_path / "pages"
    pages.assembler(REPO / "web", tmp_path / "assets", None, None, sortie, REPO / "docs" / "images", site)
    locs = set(re.findall(r"<loc>([^<]+)</loc>", (sortie / "sitemap.xml").read_text(encoding="utf-8")))
    for p in uniques:
        adresse = construire.url_page(p.source)
        copie = "fr" if p.langue == "en" else "en"
        assert construire.SITE + f"{p.langue}/{adresse}" in locs, p.source
        assert construire.SITE + f"{copie}/{adresse}" not in locs, p.source
        assert (sortie / copie / adresse / "index.html").is_file(), f"{p.source} : la copie reste en ligne"
        texte = (sortie / copie / adresse / "index.html").read_text(encoding="utf-8")
        assert f'<link rel="canonical" href="{construire.SITE}{p.langue}/{adresse}">' in texte, p.source
    racine = [p for p in (REPO / "web").rglob("*.html") if not p.name.startswith("google") and p.name != "404.html"]
    assert len(locs) == 2 * len(plan.pages) - len(uniques) + len(racine)


def test_plan_du_site_ne_liste_que_les_adresses_canoniques(tmp_path):
    """Une page dont le canonical mène ailleurs n'est pas au plan (Google veut l'adresse préférée) ;
    une page sans canonical, ou qui est le sien, y est."""
    pages_html = {
        "a/index.html": f'<link rel="canonical" href="{pages.SITE}a/">',
        "b/index.html": f'<link rel="canonical" href="{pages.SITE}a/">',
        "c/index.html": "<html></html>",
    }
    for relatif, texte in pages_html.items():
        (tmp_path / relatif).parent.mkdir(parents=True, exist_ok=True)
        (tmp_path / relatif).write_text(texte, encoding="utf-8")
    locs = re.findall(r"<loc>([^<]+)</loc>", pages.plan_du_site(tmp_path))
    assert locs == [pages.SITE + "a/", pages.SITE + "c/"]


def test_fil_d_ariane_suit_le_menu(plan):
    """Chemin du menu, de l'accueil à la page, dans la langue du texte ; les sections sans page
    (« Install », « Under the hood ») n'y sont pas ; l'accueil n'en a pas (un seul élément)."""
    def noms(source: str, langue: str) -> list[str]:
        return [e["nom"] for e in construire.fil_d_ariane(plan, source, langue, construire.SITE)]
    assert noms("docs/notice/lights.md", "en") == ["Home", "User manual", "Lights"]
    assert noms("docs/notice/lights.md", "fr") == ["Accueil", "Notice d'utilisation", "Lumières"]
    assert noms("docs/architecture.md", "en") == ["Home", "Architecture"]
    assert noms("docs/installation/flash.md", "en") == ["Home", "Installation guide", "2. Install the firmware"]
    assert noms("docs/decisions/0001-push-only-zero-polling.md", "en")[:2] == ["Home", "Design decisions"]
    assert construire.fil_d_ariane(plan, construire.ACCUEIL, "en", construire.SITE) == []
    assert construire.fil_d_ariane(plan, "docs/README.md", "fr", construire.SITE) == [
        {"nom": "Accueil", "url": construire.SITE + "fr/"},
        {"nom": "Documentation", "url": construire.SITE + "fr/documentation/"}]
    # Un nom trop long (le « # » d'un ADR) est coupé à un mot.
    long = construire.fil_d_ariane(plan, "docs/decisions/0060-ai-server-actions.md", "en", construire.SITE)[-1]["nom"]
    assert len(long) <= construire.LIMITE_NOM_FIL and long.endswith("…")


@pytest.mark.parametrize("langue", construire.LANGUES)
def test_balisage_json_ld_et_og_locale(site, plan, langue):
    """BreadcrumbList sur toutes les pages sauf l'accueil, TechArticle sur les pages de doc
    (l'accueil garde SoftwareSourceCode), og:locale et og:locale:alternate. Tout décrit la page
    canonique : pour une copie, celle de la langue d'origine. Ni HowTo ni FAQPage : Google ne les
    affiche plus (journal des modifications de Google Search Central : HowTo retiré le 14/09/2023,
    FAQ le 07/05/2026)."""
    par_adresse = {construire.url_page(p.source): p for p in plan.pages.values()}
    for page in _pages_html(site, langue):
        chemin = page.relative_to(site / langue).as_posix().removesuffix("index.html")
        texte = page.read_text(encoding="utf-8")
        origine = par_adresse[chemin].langue or langue
        canonique = f"{construire.SITE}{origine}/{chemin}"
        blocs = _json_ld(texte)
        assert not {"HowTo", "FAQPage"} & set(blocs), page
        locales = re.findall(r'<meta property="og:locale" content="([^"]+)"', texte)
        assert locales == [construire.LOCALES[origine]], page
        alternates = re.findall(r'<meta property="og:locale:alternate" content="([^"]+)"', texte)
        autre = "fr" if origine == "en" else "en"
        assert alternates == ([] if par_adresse[chemin].langue else [construire.LOCALES[autre]]), page
        if chemin == "":
            assert set(blocs) == {"SoftwareSourceCode"}, "l'accueil : sa fiche, et pas de fil d'une seule étape"
            continue
        assert set(blocs) == {"TechArticle", "BreadcrumbList"}, page
        article = blocs["TechArticle"]
        assert article["url"] == article["mainEntityOfPage"] == canonique, page
        assert article["inLanguage"] == origine and 0 < len(article["headline"]) <= construire.LIMITE_TITRE_ARTICLE, page
        assert article["description"] and article["image"] == construire.SITE + construire.IMAGE_PARTAGE, page
        elements = blocs["BreadcrumbList"]["itemListElement"]
        assert len(elements) >= 2, f"Google veut deux éléments au moins ({page})"
        assert [e["position"] for e in elements] == list(range(1, len(elements) + 1)), page
        assert elements[0]["item"] == f"{construire.SITE}{origine}/", page
        assert elements[-1]["item"] == canonique, page
        assert all(e["name"] and e["item"].startswith(f"{construire.SITE}{origine}/") for e in elements), page


def test_pages_qui_comptent_ont_un_titre_et_une_description_ecrits(plan, site):
    """Les pages de PAGES_QUI_COMPTENT : titre d'onglet (≤ 65, vérifié à la lecture du menu) et
    description (≤ 160) écrits à la main dans les deux langues, qui nomment le produit, ne
    reprennent pas « Opens with… » / « S'ouvre par… », et ne se répètent pas d'une page à l'autre."""
    assert len(set(PAGES_QUI_COMPTENT)) == len(PAGES_QUI_COMPTENT) >= 25
    for langue in construire.LANGUES:
        titres, descriptions = [], []
        for source in PAGES_QUI_COMPTENT:
            page = plan.pages[source]
            assert page.titre_onglet and page.resume, f"{source} : titre_onglet et description à écrire"
            titre, resume = page.titre_onglet[langue], page.resume[langue]
            assert "Tab5" in titre, (source, titre)
            assert not resume.startswith(("Opens with", "S'ouvre par", "S’ouvre par")), (source, resume)
            assert 100 <= len(resume) <= 160, (source, len(resume))
            titres.append(titre)
            descriptions.append(resume)
        assert len(set(titres)) == len(titres) and len(set(descriptions)) == len(descriptions), langue
    assert len(plan.pages["docs/installation/dashboard.md"].titre_onglet["fr"]) <= 65  # 66 avec le nom du site au bout
    # Le titre construit est bien celui du menu, sans le nom du site.
    for source in PAGES_QUI_COMPTENT:
        for langue in construire.LANGUES:
            page_html = (site / langue / construire.url_page(source) / "index.html").read_text(encoding="utf-8")
            assert f"<title>{plan.pages[source].titre_onglet[langue]}</title>" in html.unescape(page_html), (source, langue)


def test_plan_du_site_date_chaque_page_de_son_source(site, plan, tmp_path):
    """<lastmod> = date du dernier commit du fichier source de la page (pages.py), pour
    toutes les pages ou aucune (clone superficiel de la CI : pas de faux) ; la table des
    sources n'est pas publiée, et le site est construit avec tout l'historique."""
    sources = json.loads((site / construire.SOURCES_DES_PAGES).read_text(encoding="utf-8"))
    assert construire.SOURCES_DES_PAGES == pages.SOURCES_DES_PAGES
    assert len(sources) == 2 * len(plan.pages) and set(sources.values()) == set(plan.pages)
    for adresse in sources:
        assert (site / adresse / "index.html").is_file(), adresse
    sortie = tmp_path / "pages"
    pages.assembler(REPO / "web", tmp_path / "assets", None, None, sortie, REPO / "docs" / "images", site)
    assert not (sortie / construire.SOURCES_DES_PAGES).exists()
    xml = (sortie / "sitemap.xml").read_text(encoding="utf-8")
    entrees = dict(re.findall(r"<loc>([^<]+)</loc>(?:\n    <lastmod>([^<]+)</lastmod>)?", xml))
    dates = pages.dates_de_commit({*sources.values(), "web/index.html"})
    if not dates:
        assert "<lastmod>" not in xml
    else:
        assert entrees[construire.SITE] == dates["web/index.html"]
        copies = {f"{'fr' if p.langue == 'en' else 'en'}/{construire.url_page(p.source)}" for p in plan.pages.values() if p.langue}
        for adresse, source in sources.items():
            if adresse in copies:  # hors du plan (canonical vers l'original), donc sans date
                assert construire.SITE + adresse not in entrees, adresse
                continue
            assert entrees[construire.SITE + adresse] == dates[source], adresse
        assert all(re.fullmatch(r"\d{4}-\d{2}-\d{2}", d) for d in entrees.values())
    flux = yaml.safe_load((REPO / ".github" / "workflows" / "site.yml").read_text(encoding="utf-8"))
    checkout = flux["jobs"]["pages"]["steps"][0]
    assert checkout["uses"].startswith("actions/checkout@") and checkout["with"]["fetch-depth"] == 0


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
