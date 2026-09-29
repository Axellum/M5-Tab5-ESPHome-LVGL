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
SITE = REPO / "web" / "index.html"

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
    return re.findall(r"^  (\w+):\s*!include\s+(\S+)", bloc, re.M)


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


# ─── Les actions du contrat API ──────────────────────────────────────────────

@pytest.mark.parametrize("motif", [
    r"contrat HA, (\d+) services",                  # nœud API du schéma
    r"bloc `api: services:` \((\d+) services",      # ligne de tab5-api-logic.yaml
    r"table des services API \((\d+)\)",            # ligne de Tab5/README.md
], ids=["schema", "fichier", "readme-tab5"])
def test_cartographie_nombre_de_services(motif):
    assert set(_nombres(CARTOGRAPHIE, motif)) == {len(_services())}


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
