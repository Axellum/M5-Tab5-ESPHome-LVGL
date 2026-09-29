# -*- coding: utf-8 -*-
"""Les comptes écrits dans la doc, comparés au code.

Constatés faux le 29/09/2026 (pendant la PR #259) : « douze packages » dans
docs/architecture.md (l'entrée en importe dix-neuf, et la liste recopiée en
oubliait quatre), « 18 services » dans la cartographie (19 dans
Tab5/tab5-api-logic.yaml), « 25 décisions d'architecture » sur le site et dans le
README (26 dans docs/decisions/). Là où un nombre reste écrit, ce test vérifie :

- les packages de `tab5-ha-hmi.yaml` : la liste recopiée dans docs/architecture.md
  (mêmes clés, mêmes fichiers, même ordre), leur nombre en toutes lettres dans le
  README (EN et FR) et en chiffres dans la cartographie ;
- les actions (`- service:`) de `Tab5/tab5-api-logic.yaml` : leur nombre dans la
  cartographie, et la table de Tab5/README.md, qui les liste chacune une fois ;
- les ADR de docs/decisions/ : leur nombre dans le README, le site et la cartographie.

Ajouté le même jour (schéma de la cartographie) : le schéma Mermaid n'avait pas de nœud
pour quatre packages de l'entrée et pas d'arête pour trois autres, annonçait 40
`ui_components` pour 45, docs/architecture.md « 23 » inclus directs pour 24 et n'avait
pas de section pour sept packages, et le README comptait « quatre » fichiers de plus de
500 lignes pour cinq. D'où :

- un nœud et une arête `ENTRY -->|packages:|` par package de l'entrée, dans son ordre ;
- une section « ### `fichier` » par package dans « Package roles » et « Rôles des packages » ;
- les fichiers de plus de 500 lignes nommés par le README (EN et FR) ;
- le nombre de `ui_components/*.yaml`, et de ceux que `tab5-lvgl.yaml` inclut lui-même.

Un texte qui n'a pas besoin du nombre l'omet (vue d'ensemble d'architecture.md) : il
ne se périme plus. Un motif qui ne trouve plus rien fait échouer le test : le texte a
changé, il faut adapter le motif, pas le laisser vérifier le vide."""
import pathlib
import re

import pytest

REPO = pathlib.Path(__file__).resolve().parents[1]
ENTREE = REPO / "tab5-ha-hmi.yaml"
API = REPO / "Tab5" / "tab5-api-logic.yaml"
DECISIONS = REPO / "docs" / "decisions"
ARCHITECTURE = REPO / "docs" / "architecture.md"
CARTOGRAPHIE = REPO / "CARTOGRAPHIE_TAB5.md"
README = REPO / "README.md"
README_TAB5 = REPO / "Tab5" / "README.md"
INVENTAIRE = REPO / "docs" / "INVENTAIRE_CONFIGS_TESTS.md"
SITE = REPO / "web" / "index.html"
LVGL = REPO / "Tab5" / "tab5-lvgl.yaml"
UI = REPO / "Tab5" / "ui_components"
GROS = 500   # « Most stay under 500 lines » (README)

_UNITES_EN = ("zero one two three four five six seven eight nine ten eleven twelve thirteen "
              "fourteen fifteen sixteen seventeen eighteen nineteen").split()
_UNITES_FR = ("zéro un deux trois quatre cinq six sept huit neuf dix onze douze treize "
              "quatorze quinze seize dix-sept dix-huit dix-neuf").split()
_DIZAINES_EN = "twenty thirty forty fifty".split()
_DIZAINES_FR = "vingt trente quarante cinquante".split()


def _lire(chemin):
    return chemin.read_text(encoding="utf-8")


def _en_lettres(n):
    """(anglais, français) de n < 60, comme dans une phrase : twenty-one, vingt et un."""
    assert 0 <= n < 60, f"{n} : étendre _en_lettres"
    if n < 20:
        return _UNITES_EN[n], _UNITES_FR[n]
    dizaine, unite = divmod(n, 10)
    en, fr = _DIZAINES_EN[dizaine - 2], _DIZAINES_FR[dizaine - 2]
    if unite == 0:
        return en, fr
    return f"{en}-{_UNITES_EN[unite]}", f"{fr} et un" if unite == 1 else f"{fr}-{_UNITES_FR[unite]}"


def _packages(texte):
    """[(clé, fichier)] du bloc `packages:` de premier niveau, dans l'ordre."""
    debuts = list(re.finditer(r"^packages:\n", texte, re.M))
    assert len(debuts) == 1, f"{len(debuts)} blocs `packages:` de premier niveau"
    fin = re.compile(r"^\S", re.M).search(texte, debuts[0].end())
    bloc = texte[debuts[0].end():fin.start() if fin else len(texte)]
    # Le chemin va jusqu'au commentaire ou à la fin de ligne : `ecran-${ tab5_ecran | … }.yaml`
    # contient des espaces.
    return re.findall(r"^  (\w+):\s*!include\s+(.+?)\s*(?:#.*)?$", bloc, re.M)


def _nom(fichier):
    """Nom d'un package dans la doc : `Tab5/ecran-${ tab5_ecran | … }.yaml` → `ecran-*.yaml`."""
    return re.sub(r"\$\{[^}]*\}", "*", fichier.rsplit("/", 1)[-1])


def _noms_des_packages():
    return [_nom(f) for _, f in _packages(_lire(ENTREE))]


def _schema():
    texte = _lire(CARTOGRAPHIE)
    debut = texte.index("```mermaid\n")
    return texte[debut:texte.index("\n```", debut + 1)]


def _services():
    return re.findall(r"^    - service: (\w+)", _lire(API), re.M)


def _adr():
    return sorted(DECISIONS.glob("[0-9][0-9][0-9][0-9]-*.md"))


def _nombres(chemin, motif):
    """Chaque nombre capturé par `motif` dans `chemin` ; au moins un."""
    trouves = [int(n) for n in re.findall(motif, _lire(chemin))]
    assert trouves, f"{chemin.name} : plus rien ne correspond à {motif!r}, adapter le motif"
    return trouves


def test_en_lettres():
    assert _en_lettres(12) == ("twelve", "douze")
    assert _en_lettres(19) == ("nineteen", "dix-neuf")
    assert _en_lettres(20) == ("twenty", "vingt")
    assert _en_lettres(21) == ("twenty-one", "vingt et un")
    assert _en_lettres(26) == ("twenty-six", "vingt-six")


# ─── Les packages de l'entrée ────────────────────────────────────────────────

def test_architecture_recopie_les_packages_de_l_entree():
    entree = _packages(_lire(ENTREE))
    assert len(entree) > 10, "le motif ne reconnaît plus le bloc `packages:` de l'entrée"
    assert _packages(_lire(ARCHITECTURE)) == entree, \
        "recopier le bloc `packages:` de tab5-ha-hmi.yaml dans docs/architecture.md, § 1"


def test_readme_nombre_de_packages():
    en, fr = _en_lettres(len(_packages(_lire(ENTREE))))
    texte = _lire(README)
    for motif, attendu in ((r"split across ([a-z-]+(?: et un)?) files by concern", en),
                           (r"découpée en ([a-zé-]+(?: et un)?) fichiers par domaine", fr)):
        trouve = re.search(motif, texte)
        assert trouve, f"README.md : plus rien ne correspond à {motif!r}, adapter le motif"
        assert trouve.group(1) == attendu, f"README.md : « {trouve.group(1)} », attendu « {attendu} »"


# Les motifs de la cartographie visent les phrases qui décrivent l'état actuel : son
# historique (« RÉSOLU … Architecture effective = 8 packages », juillet 2026) reste daté.
def test_cartographie_nombre_de_packages():
    n = len(_packages(_lire(ENTREE)))
    assert set(_nombres(CARTOGRAPHIE, r"YAML modulaire par domaine\*\* \((\d+) packages")) == {n}


def test_schema_un_noeud_et_une_arete_par_package():
    schema = _schema()
    debut = schema.index("subgraph PKG[")
    bloc = schema[debut:schema.index("\n    end\n", debut)]
    # Le libellé d'un nœud commence par le nom du fichier : `ID["nom.yaml<br/>…` ou `ID["nom.yaml (…`.
    noeuds = re.findall(r'^ +(\w+)\["([^\s"<]+)', bloc, re.M)
    assert [nom for _, nom in noeuds] == _noms_des_packages(), \
        "un nœud par package de tab5-ha-hmi.yaml, dans l'ordre du bloc `packages:`"
    aretes = re.findall(r"^ +ENTRY -->\|packages:\| (\w+)$", schema, re.M)
    assert aretes == [id_ for id_, _ in noeuds], "une arête `ENTRY -->|packages:|` par nœud du subgraph PKG"


def test_architecture_une_section_par_package():
    texte = _lire(ARCHITECTURE)
    for nom in _noms_des_packages():
        titres = re.findall(rf"^### `{re.escape(nom)}`$", texte, re.M)
        assert len(titres) == 2, f"docs/architecture.md : « ### `{nom}` » {len(titres)} fois, attendu EN + FR"


def test_readme_fichiers_de_plus_de_500_lignes():
    gros = set()
    for _, fichier in _packages(_lire(ENTREE)):
        for chemin in (REPO / fichier).parent.glob(_nom(fichier)):
            with chemin.open(encoding="utf-8") as f:
                if sum(1 for _ in f) > GROS:
                    gros.add(chemin.name)
    assert gros, "plus aucun package de plus de 500 lignes : réécrire la phrase du README"
    texte = _lire(README)
    for motif in (r"only the largest \(([^)]*)\) go beyond", r"seuls les plus gros \(([^)]*)\) dépassent"):
        trouve = re.search(motif, texte)
        assert trouve, f"README.md : plus rien ne correspond à {motif!r}, adapter le motif"
        assert set(re.findall(r"`([^`]+)`", trouve.group(1))) == gros


# ─── Les composants UI ───────────────────────────────────────────────────────

def _ui_total():
    return len(list(UI.glob("*.yaml")))


def _ui_directs():
    """Fichiers de ui_components/ que tab5-lvgl.yaml inclut lui-même (un fichier à `vars` compte une fois)."""
    return len(set(re.findall(r"ui_components/([\w.]+\.yaml)", _lire(LVGL))))


@pytest.mark.parametrize("chemin, motif, compte", [
    (CARTOGRAPHIE, r'subgraph UI\["ui_components/\*\.yaml \((\d+) fichiers', _ui_total),
    (CARTOGRAPHIE, r"ui_components/\*\.yaml \(\d+ fichiers, (\d+) inclus par tab5-lvgl", _ui_directs),
    (CARTOGRAPHIE, r"`ui_components/\*\.yaml` — (\d+) fichiers, dont", _ui_total),
    (CARTOGRAPHIE, r"fichiers, dont (\d+) inclus directement par `tab5-lvgl\.yaml`", _ui_directs),
    (ARCHITECTURE, r"`!include`s (\d+) `ui_components/\*\.yaml` files directly", _ui_directs),
    (ARCHITECTURE, r"for (\d+) component files in total", _ui_total),
    (ARCHITECTURE, r"`!include` directement (\d+) fichiers `ui_components", _ui_directs),
    (ARCHITECTURE, r"soit (\d+) fichiers de composants au total", _ui_total),
    (README, r"split into (\d+) reusable `ui_components", _ui_total),
    (README, r"découpée en (\d+) `ui_components/\*\.yaml` réutilisables", _ui_total),
    (README_TAB5, r"Les (\d+) composants et templates LVGL", _ui_total),
    (INVENTAIRE, r"\((\d+) composants UI dont", _ui_total),
    (INVENTAIRE, r"composants UI dont (\d+) inclus par", _ui_directs),
], ids=["schema-total", "schema-directs", "carto-total", "carto-directs", "archi-en-directs",
        "archi-en-total", "archi-fr-directs", "archi-fr-total", "readme-en", "readme-fr",
        "readme-tab5", "inventaire-total", "inventaire-directs"])
def test_nombre_de_composants_ui(chemin, motif, compte):
    assert _ui_directs() > 10, "le motif ne reconnaît plus les !include de tab5-lvgl.yaml"
    assert set(_nombres(chemin, motif)) == {compte()}


# ─── Les actions du contrat API ──────────────────────────────────────────────

@pytest.mark.parametrize("motif", [
    r"contrat HA, (\d+) services",                  # nœud API du schéma
    r"bloc `api: services:` \((\d+) services",      # ligne de tab5-api-logic.yaml
    r"table des services API \((\d+)\)",            # ligne de Tab5/README.md
], ids=["schema", "fichier", "readme-tab5"])
def test_cartographie_nombre_de_services(motif):
    assert set(_nombres(CARTOGRAPHIE, motif)) == {len(_services())}


@pytest.mark.parametrize("motif", [
    r"inclus par `tab5-lvgl\.yaml`, (\d+) services",   # en-tête de l'inventaire
    r"bloc `api: services:` \((\d+) services",         # ligne de tab5-api-logic.yaml
], ids=["entete", "fichier"])
def test_inventaire_nombre_de_services(motif):
    assert set(_nombres(INVENTAIRE, motif)) == {len(_services())}


def test_table_des_services_du_readme_tab5():
    texte = _lire(README_TAB5)
    debut = texte.index("## Services HA exposés")
    section = texte[debut:texte.index("\n## ", debut + 1)]
    lignes = re.findall(r"^\| `(\w+)` \|", section, re.M)
    services = _services()
    assert len(services) > 10, "le motif ne reconnaît plus les `- service:` du contrat"
    assert len(lignes) == len(set(lignes)), "une action listée deux fois"
    assert sorted(lignes) == sorted(services)


# ─── Les décisions d'architecture ────────────────────────────────────────────

@pytest.mark.parametrize("chemin, motif", [
    (README, r"\b(\d+) \[architecture decision records\]"),
    (README, r"\b(\d+) \[décisions d'architecture\]"),
    (SITE, r'lang="en">(\d+) architecture decision records'),
    (SITE, r'lang="fr">(\d+) décisions d\'architecture'),
    (CARTOGRAPHIE, r"\((\d+) ADR\)"),
], ids=["readme-en", "readme-fr", "site-en", "site-fr", "cartographie"])
def test_nombre_d_adr(chemin, motif):
    assert len(_adr()) > 20, "docs/decisions/ ne contient plus les ADR numérotés"
    assert set(_nombres(chemin, motif)) == {len(_adr())}
