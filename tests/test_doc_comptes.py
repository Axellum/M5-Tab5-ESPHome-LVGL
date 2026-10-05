# -*- coding: utf-8 -*-
"""Les comptes écrits dans la doc, comparés au code.

Constatés faux le 29/09/2026 (pendant la PR #259) : « douze packages » dans
docs/architecture.md (l'entrée en importe dix-neuf, et la liste recopiée en
oubliait quatre), « 18 services » dans la cartographie (19 dans
Tab5/tab5-api-logic.yaml), « 25 décisions d'architecture » sur le site et dans le
README (26 dans docs/decisions/). Là où un nombre reste écrit, ce test vérifie :

- les packages de `tab5-ha-hmi.yaml` : la liste recopiée dans docs/architecture.md
  (mêmes clés, mêmes fichiers, même ordre), leur nombre en toutes lettres dans ses
  « Key design decisions » (EN et FR ; dans le README jusqu'au 05/10/2026) et en
  chiffres dans la cartographie ;
- les actions (`- service:`) de `Tab5/tab5-api-logic.yaml` : leur nombre dans la
  cartographie, et la table de Tab5/README.md, qui les liste chacune une fois ;
- les ADR de docs/decisions/ : leur nombre dans le README et la cartographie.

Ajouté le même jour (schéma de la cartographie) : le schéma Mermaid n'avait pas de nœud
pour quatre packages de l'entrée et pas d'arête pour trois autres, annonçait 40
`ui_components` pour 45, docs/architecture.md « 23 » inclus directs pour 24 et n'avait
pas de section pour sept packages, et le README comptait « quatre » fichiers de plus de
500 lignes pour cinq. D'où :

- un nœud et une arête `ENTRY -->|packages:|` par package de l'entrée, dans son ordre ;
- une section « ### `fichier` » par package dans « Package roles » et « Rôles des packages » ;
- les fichiers de plus de 500 lignes nommés par docs/architecture.md (EN et FR) ;
- le nombre de `ui_components/*.yaml`, et de ceux que `tab5-lvgl.yaml` inclut lui-même.

Ajouté le 30/09/2026 (lot D de l'audit, contrat HA ↔ tablette) : le nombre de champs
de la vigilance, les services appelés ou non par la démo (AGENTS.md, docs/demo_mode.md)
et la table des événements de Tab5/README.md (données émises, consommateurs).

Ajouté le 02/10/2026 (après le turc) : le nombre de langues de Tab5/lang/ dans le README
(EN et FR) et sur la racine du site.

Le 05/10/2026, la vitrine (web/index.html) est devenue un renvoi vers l'accueil de la
documentation, qui est le README : ses comptes ne sont plus vérifiés qu'une fois.

Un texte qui n'a pas besoin du nombre l'omet (vue d'ensemble d'architecture.md) : il
ne se périme plus. Un motif qui ne trouve plus rien fait échouer le test : le texte a
changé, il faut adapter le motif, pas le laisser vérifier le vide."""
import pathlib
import re
import sys

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
GROS = 500   # « Most stay under 500 lines » (docs/architecture.md)

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


def test_choix_de_conception_nombre_de_packages():
    en, fr = _en_lettres(len(_packages(_lire(ENTREE))))
    texte = _lire(ARCHITECTURE)
    for motif, attendu in ((r"split across ([a-z-]+(?: et un)?) files by concern", en),
                           (r"découpée en ([a-zé-]+(?: et un)?) fichiers par domaine", fr)):
        trouve = re.search(motif, texte)
        assert trouve, f"architecture.md : plus rien ne correspond à {motif!r}, adapter le motif"
        assert trouve.group(1) == attendu, f"architecture.md : « {trouve.group(1)} », attendu « {attendu} »"


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


def test_choix_de_conception_fichiers_de_plus_de_500_lignes():
    gros = set()
    for _, fichier in _packages(_lire(ENTREE)):
        for chemin in (REPO / fichier).parent.glob(_nom(fichier)):
            with chemin.open(encoding="utf-8") as f:
                if sum(1 for _ in f) > GROS:
                    gros.add(chemin.name)
    assert gros, "plus aucun package de plus de 500 lignes : réécrire la phrase de docs/architecture.md"
    texte = _lire(ARCHITECTURE)
    for motif in (r"only the largest \(([^)]*)\) go beyond", r"seuls les plus gros \(([^)]*)\) dépassent"):
        trouve = re.search(motif, texte)
        assert trouve, f"architecture.md : plus rien ne correspond à {motif!r}, adapter le motif"
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
    (ARCHITECTURE, r"split into (\d+) reusable `ui_components", _ui_total),
    (ARCHITECTURE, r"découpée en (\d+) `ui_components/\*\.yaml` réutilisables", _ui_total),
    (README_TAB5, r"Les (\d+) composants et templates LVGL", _ui_total),
    (INVENTAIRE, r"\((\d+) composants UI dont", _ui_total),
    (INVENTAIRE, r"composants UI dont (\d+) inclus par", _ui_directs),
], ids=["schema-total", "schema-directs", "carto-total", "carto-directs", "archi-en-directs",
        "archi-en-total", "archi-fr-directs", "archi-fr-total", "archi-en-choix", "archi-fr-choix",
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
    (CARTOGRAPHIE, r"\((\d+) ADR\)"),
], ids=["readme-en", "readme-fr", "cartographie"])
def test_nombre_d_adr(chemin, motif):
    assert len(_adr()) > 20, "docs/decisions/ ne contient plus les ADR numérotés"
    assert set(_nombres(chemin, motif)) == {len(_adr())}


# ─── Les langues de l'écran (02/10/2026) ─────────────────────────────────────
# Constaté faux ce jour-là, après l'ajout du turc : « Six languages / Six langues » sur
# le site au-dessus d'une liste de sept. Le nombre se compte dans Tab5/lang/ ; écrit en
# chiffres ou en lettres, avec ou sans majuscule.

def _langues():
    return sorted((REPO / "Tab5" / "lang").glob("*.yaml"))


def _nombres_ecrits(chemin, motif):
    """Comme _nombres, mais « 7 », « seven », « Sept », « Twenty-one », « Vingt et un »…
    valent tous un nombre (jusqu'à 59, _en_lettres)."""
    trouves = re.findall(motif, _lire(chemin))
    assert trouves, f"{chemin.name} : plus rien ne correspond à {motif!r}, adapter le motif"
    en_lettres = {}
    for n in range(60):
        for mot in _en_lettres(n):
            en_lettres[mot] = n
    nombres = []
    for mot in trouves:
        mot = mot.lower()
        if mot.isdigit():
            nombres.append(int(mot))
        else:
            assert mot in en_lettres, f"{chemin.name} : nombre inconnu {mot!r}"
            nombres.append(en_lettres[mot])
    return nombres


@pytest.mark.parametrize("chemin, motif", [
    (README, r"in (\w+) languages\."),
    (README, r"\*\*(\w+) languages, down to the details"),
    (README, r"en (\w+) langues\."),
    (README, r"\*\*(\w+) langues, jusque dans les détails"),
    (SITE, r'in (\w+) languages\.">'),
], ids=["readme-en-accroche", "readme-en-pourquoi", "readme-fr-accroche", "readme-fr-pourquoi",
        "site-apercu"])
def test_nombre_de_langues(chemin, motif):
    assert len(_langues()) > 5, "Tab5/lang/ ne contient plus les fichiers de langue"
    assert set(_nombres_ecrits(chemin, motif)) == {len(_langues())}


# ─── Les thèmes de l'écran (05/10/2026) ──────────────────────────────────────
# Écrit dans le README, docs/screens.md et docs/installation/settings.md pour la 3.6.0 :
# le nombre se compte dans Tab5/themes/ (un fichier par thème ; `_polices.yaml`, généré,
# n'en est pas un).

def _themes():
    return sorted(f for f in (REPO / "Tab5" / "themes").glob("*.yaml") if not f.name.startswith("_"))


# Un nombre en lettres peut avoir un trait d'union (« Twenty-one », « Dix-huit ») ou
# « et un » (« Vingt et un ») : `(\w+)` n'en lirait que la fin (« one », « un »).
@pytest.mark.parametrize("chemin, motif", [
    (README, r"([\w-]+) themes, light or dark"),
    (README, r"([\w-]+(?: et un)?) thèmes, clairs ou sombres"),
    (REPO / "docs" / "screens.md", r"([\w-]+) themes, each with a dark and a light mode"),
    (REPO / "docs" / "screens.md", r"([\w-]+(?: et un)?) thèmes, chacun en sombre et en clair"),
    (REPO / "docs" / "installation" / "settings.md", r"\| Thème \| (\d+) themes"),
    (REPO / "docs" / "installation" / "settings.md", r"\| Thème \| (\d+) thèmes"),
], ids=["readme-en", "readme-fr", "ecrans-en", "ecrans-fr", "installation-en", "installation-fr"])
def test_nombre_de_themes(chemin, motif):
    assert len(_themes()) > 10, "Tab5/themes/ ne contient plus les fichiers de thème"
    assert set(_nombres_ecrits(chemin, motif)) == {len(_themes())}


# ─── Le contrat HA ↔ tablette (audit du 30/09/2026, lot D) ───────────────────
# Constatés faux ce jour-là : « 11 champs » de vigilance (le parseur en lit jusqu'à 13),
# « 10 dashboard push services » et « 6 other services » dans AGENTS.md (la démo en
# appelle 12, et 7 non, dont tab5_maj_planning oublié), tab5_maj_rdv_prochains et
# tab5_maj_planning absents des services hors démo de docs/demo_mode.md, et dans la table
# des événements de Tab5/README.md, le blueprint (tab5_maj_ecran) et tab5_reglages.yaml
# (tab5_connected) absents des consommateurs. Champs et appels eux-mêmes :
# tests/test_contrat.py.

AGENTS = REPO / "AGENTS.md"
DEMO_MODE = REPO / "docs" / "demo_mode.md"
SERVICES_CPP = REPO / "Tab5" / "tab5_services.cpp"
sys.path.insert(0, str(REPO / "tools" / "demo"))

import demo_pusher  # noqa: E402
import scenarios  # noqa: E402
from tests.test_actions_ha import _consommateurs  # noqa: E402
from tests.test_contrat import champs_emis  # noqa: E402


def _vigilance_min_max():
    """(champs de la forme Météo-France, champs lus au plus par parse_and_update_vigilance)."""
    corps = _lire(SERVICES_CPP).split("bool parse_and_update_vigilance(", 1)[1].split("\n}\n", 1)[0]
    lus = re.search(r"const char\* fields\[(\d+)\];", corps)
    assert lus, "parse_and_update_vigilance : plus de `const char* fields[N]`, adapter le motif"
    tampon = re.search(r"char buf\[(\d+)\];", corps)
    assert tampon and int(tampon.group(1)) == scenarios.ALERTE_BUF_OCTETS, \
        "tools/demo/scenarios.py : ALERTE_BUF_OCTETS = le tampon de parse_and_update_vigilance"
    return len(scenarios.ALERTE_CHAMPS), int(lus.group(1))


@pytest.mark.parametrize("chemin, motif", [
    (README_TAB5, r"`tab5_maj_alerte_meteo_france` \| payload \(string, (\d+) à (\d+) champs"),
    (ARCHITECTURE, r"`tab5_maj_alerte_meteo_france`, (\d+) to (\d+) `\|`-delimited fields"),
    (ARCHITECTURE, r"`tab5_maj_alerte_meteo_france`, (\d+) à (\d+) champs délimités"),
    (API, r"// (\d+) à (\d+) champs « \| »"),
], ids=["readme-tab5", "archi-en", "archi-fr", "api-logic"])
def test_champs_de_vigilance(chemin, motif):
    trouves = re.findall(motif, _lire(chemin))
    assert trouves, f"{chemin.name} : plus rien ne correspond à {motif!r}, adapter le motif"
    assert {(int(a), int(b)) for a, b in trouves} == {_vigilance_min_max()}


def _demo():
    """Actions appelées par la démo (tests/test_demo.py le vérifie en la jouant)."""
    return set(demo_pusher.SERVICES_ATTENDUS) | {demo_pusher.SERVICE_TUILES,
                                                 demo_pusher.SERVICE_ENERGIE,
                                                 demo_pusher.SERVICE_ENERGIE_HISTORIQUE}


def _noms(texte):
    """Noms d'actions entre accents graves ; « `_jour` » reprend le préfixe du précédent."""
    noms = []
    for nom in re.findall(r"`(\w+)`", texte):
        if nom.startswith("_") and noms:
            nom = noms[-1].rsplit("_", 1)[0] + nom
        noms.append(nom)
    return [n for n in noms if n.startswith("tab5_")]


def test_agents_services_de_la_demo():
    texte = _lire(AGENTS)
    appeles = re.search(r"the \*\*(\d+) push services\*\* the demo calls", texte)
    autres = re.search(r"The (\d+) other services \(([^)]*)\) are out of its scope", texte)
    assert appeles and autres, "AGENTS.md : phrase du --dry-run changée, adapter les motifs"
    hors_demo = set(_services()) - _demo()
    assert int(appeles.group(1)) == len(_demo())
    assert int(autres.group(1)) == len(hors_demo)
    assert sorted(_noms(autres.group(2))) == sorted(hors_demo)


@pytest.mark.parametrize("motif_nombre, motif_restants, langue", [
    (r"driving ([a-z]+) dashboard push services",
     r"The remaining services are out of scope by design (.*?)\n", 0),
    (r"qui pilotent ([a-zé]+) services de push du dashboard",
     r"Les services restants sont hors périmètre par choix (.*?)\n", 1),
], ids=["en", "fr"])
def test_demo_mode_services(motif_nombre, motif_restants, langue):
    """« nine dashboard push services » + zones + emplacements + tuiles + énergie, nommées à part ;
    les services restants, tous nommés."""
    texte = _lire(DEMO_MODE)
    nombre, restants = re.search(motif_nombre, texte), re.search(motif_restants, texte)
    assert nombre and restants, "docs/demo_mode.md : phrase changée, adapter les motifs"
    a_part = {"tab5_maj_zones", "tab5_maj_emplacements", demo_pusher.SERVICE_TUILES,
              demo_pusher.SERVICE_ENERGIE, demo_pusher.SERVICE_ENERGIE_HISTORIQUE}
    assert nombre.group(1) == _en_lettres(len(_demo() - a_part))[langue]
    assert sorted(set(_noms(restants.group(1)))) == sorted(set(_services()) - _demo())


def _table_des_evenements():
    """{événement: (données, consommateurs)} de la table de Tab5/README.md."""
    texte = _lire(README_TAB5)
    debut = texte.index("## Événements émis vers HA")
    section = texte[debut:texte.index("\n## ", debut + 1)]
    lignes = {}
    for evenements, donnees, _, consommateurs in re.findall(
            r"^\| (`tab5_[^|]+) \| ([^|]*) \| ([^|]*) \| ([^|]*) \|$", section, re.M):
        noms = re.findall(r"`(tab5_\w+)`", evenements)
        # « `tab5_alarm_start` / `_stop` / … » : même préfixe.
        noms += [noms[0].rsplit("_", 1)[0] + s for s in re.findall(r"`(_\w+)`", evenements)]
        for nom in noms:
            lignes["esphome." + nom] = (donnees, consommateurs)
    return lignes


def test_table_des_evenements_du_readme_tab5():
    table = _table_des_evenements()
    emis = champs_emis()
    assert len(emis) > 15 and sorted(table) == sorted(emis), "une ligne par événement émis"
    consommateurs = _consommateurs()
    for evt, (donnees, qui) in sorted(table.items()):
        # Données : « a, b », « option (`preferred` / …) » ou « — ».
        champs = {c.strip().split(" ")[0] for c in donnees.split(",")} - {"—"}
        assert champs == set(emis[evt]), f"{evt} : « {donnees} », le firmware émet {sorted(emis[evt])}"
        for fichier in consommateurs.get(evt, []):
            nom = "blueprint" if "/blueprints/" in fichier else f"`{fichier.rsplit('/', 1)[1]}`"
            assert nom in qui, f"{evt} : {nom} l'écoute, absent de la colonne « Consommateur » ({qui})"
