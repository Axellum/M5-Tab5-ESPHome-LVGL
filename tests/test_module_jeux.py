# -*- coding: utf-8 -*-
"""Le module des jeux (ADR-0065, 18/10/2026) : ce que la compilation ne dit qu'à moitié.

- les deux racines (tab5-ha-hmi.yaml, tab5-rendu-host.yaml) choisissent le module par la même
  expression : `tab5_jeux` (oui / non), par défaut oui sur le Tab5, non sur les autres cartes
  (sans IMU) ;
- tab5-arcade.yaml (le module) et tab5-arcade-absente.yaml donnent au socle les mêmes prises
  (scripts) ; le module compile chaque fichier de Tab5/jeux/ sauf la table vide, l'absent
  seulement elle ; ni l'une ni l'autre racine ne compile un fichier des jeux elle-même ;
- hors du module (Tab5/jeux/, tab5-arcade.yaml et les composants de ses pages), aucun fichier
  du firmware ne nomme une page, un script d'ouverture, un namespace ou la police d'un jeu :
  sinon le firmware sans jeux (build-carte-7b en CI) ne compile plus.
"""
import re

import check_tab5_registry
import jinja2
import pytest
from tests.commun import CARTES, REPO, charger, lire, sources

RACINES = ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml")
MODULE = ("Tab5", "paquets", "tab5-arcade.yaml")
ABSENT = ("Tab5", "paquets", "tab5-arcade-absente.yaml")
RE_CHOIX = re.compile(r"^  tab5_arcade: !include Tab5/paquets/tab5-arcade(\$\{[^}]*\})\.yaml$", re.M)
# Composants de ui_components/ que seules les pages du module incluent.
COMPOSANTS_DU_MODULE = re.compile(r"^(game_selector|arcade_card|\w+_game)\.yaml$")


def _expression(racine):
    trouve = RE_CHOIX.findall(lire(racine))
    assert len(trouve) == 1, f"{racine} : le package tab5_arcade ne choisit plus le module par une expression"
    return trouve[0]


def _fichier(expression, **contexte):
    """Le package choisi, comme ESPHome l'évalue (Jinja, variables absentes = non définies)."""
    corps = expression[2:-1].strip()
    return "tab5-arcade" + jinja2.Environment().compile_expression(corps)(**contexte) + ".yaml"


def test_les_deux_racines_choisissent_pareil():
    assert _expression(RACINES[0]) == _expression(RACINES[1])


@pytest.mark.parametrize("contexte, attendu", [
    ({}, "tab5-arcade.yaml"),
    ({"tab5_carte": "tab5"}, "tab5-arcade.yaml"),
    ({"tab5_carte": "waveshare-7b"}, "tab5-arcade-absente.yaml"),
    ({"tab5_carte": "Waveshare-7B"}, "tab5-arcade-absente.yaml"),
    ({"tab5_carte": "waveshare-7b", "tab5_jeux": "oui"}, "tab5-arcade.yaml"),
    ({"tab5_carte": "waveshare-7b", "tab5_jeux": "OUI"}, "tab5-arcade.yaml"),
    ({"tab5_jeux": "non"}, "tab5-arcade-absente.yaml"),
    ({"tab5_carte": "tab5", "tab5_jeux": "non"}, "tab5-arcade-absente.yaml"),
])
def test_jeux_par_defaut_selon_la_carte(contexte, attendu):
    assert _fichier(_expression(RACINES[0]), **contexte) == attendu


def test_chaque_carte_a_son_defaut():
    """Une carte de plus (CARTES) : oui seulement si elle a une IMU, comme le Tab5 (ADR-0065)."""
    for carte in CARTES:
        attendu = "tab5-arcade.yaml" if carte == "tab5" else "tab5-arcade-absente.yaml"
        assert _fichier(_expression(RACINES[0]), tab5_carte=carte) == attendu, carte


def test_le_rendu_sait_quelle_carte_a_les_jeux():
    """tools/rendu/capturer.py saute l'Arcade et les consoles d'une carte sans jeux."""
    import vers_carte
    for carte in CARTES:
        attendu = _fichier(_expression(RACINES[1]), tab5_carte=carte) == "tab5-arcade.yaml"
        assert vers_carte.jeux_compiles(carte) == attendu, carte


def test_sans_jeux_la_manette_ouvre_le_lecteur():
    """Choix d'Axel (18/10/2026) : sans jeux, le tap « auto » de la manette ouvre le lecteur
    audio (popup Musique), son appui long reste la télécommande ; le blueprint les change."""
    zones = lire("Tab5", "ecran", "tab5_zones.cpp")
    assert 'if (g == GESTE_MANETTE_COURT && !GameRegistry::disponibles()) return "musique";' in zones
    assert '{"musique", Ecran::MUSIQUE, GesteAction::ECRAN}' in zones
    assert re.search(r"kEcranAuto\[BOUTON_HAUT_NB\] = \{[^}]*, Ecran::TV\};", zones)


def _scripts(chemin):
    return {s["id"] for s in charger(*chemin).get("script", [])}


def _includes(chemin):
    return charger(*chemin)["esphome"]["includes"]


def test_memes_prises_avec_et_sans_jeux():
    absent = _scripts(ABSENT)
    assert absent == {"tab5_arcade_open", "tab5_jeux_traduire"}, absent
    assert absent <= _scripts(MODULE)


def test_le_module_compile_tous_les_jeux_et_l_absent_aucun():
    jeux = sorted(f"Tab5/jeux/{p.name}" for p in (REPO / "Tab5" / "jeux").iterdir()
                  if p.suffix in (".h", ".cpp") and p.name != "jeux_absents.cpp")
    assert "Tab5/jeux/jeux_registre.cpp" in jeux
    assert sorted(_includes(MODULE)) == jeux
    assert _includes(ABSENT) == ["Tab5/jeux/jeux_absents.cpp"]
    for chemin in (MODULE, ABSENT):
        assert all((REPO / f).is_file() for f in _includes(chemin)), chemin


@pytest.mark.parametrize("racine", RACINES)
def test_les_racines_ne_compilent_aucun_jeu(racine):
    # Texte et non YAML : tab5-rendu-host.yaml fusionne ses substitutions par `<<: !include`.
    assert re.findall(r"^ +- (Tab5/jeux/\S+)", lire(racine), re.M) == []


def _sans_commentaires(texte, suffixe):
    """Sans les commentaires YAML (`#`) ni C++ (ceux des lambdas aussi) : un nom de jeu
    écrit en commentaire ne se compile pas."""
    if suffixe == ".yaml":
        texte = "\n".join(l for l in texte.splitlines() if not l.lstrip().startswith("#"))
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", texte, flags=re.S)


def _du_socle(chemin):
    return "jeux" not in chemin.parts and chemin.name != MODULE[-1] and not COMPOSANTS_DU_MODULE.match(chemin.name)


def test_le_socle_declare_les_widgets_qu_il_cree():
    """ESPHome n'active un type de widget LVGL (`LV_USE_LINE`…) que si la config en déclare
    un : un widget créé en C++ par le socle doit être déclaré par un YAML du socle, pas
    seulement par une page de jeu (le 18/10/2026, `lv_line_create` ne compilait plus sans
    l'amorce du flipper : d'où `lvgl_line_probe`, tab5-lvgl.yaml)."""
    tab5 = REPO / "Tab5"
    cpp = [p for p in sources("*.h", "*.cpp") if _du_socle(p)]
    crees = set()
    for p in cpp:
        crees |= set(re.findall(r"\blv_([a-z]+)_create\(", _sans_commentaires(p.read_text(encoding="utf-8"), p.suffix)))
    crees -= {"obj", "timer", "indev", "display", "anim", "style", "group", "event"}
    assert "line" in crees, crees
    yaml_socle = [p for p in sources("*.yaml") + sorted((tab5 / "ui_components").glob("*.yaml")) if _du_socle(p)]
    declares = set()
    for p in yaml_socle:
        declares |= set(re.findall(r"^\s*- ([a-z]+):", _sans_commentaires(p.read_text(encoding="utf-8"), ".yaml"), re.M))
    assert crees <= declares, f"widgets créés en C++ sans déclaration dans le socle : {sorted(crees - declares)}"


def test_le_socle_ne_nomme_aucun_jeu():
    module = charger(*MODULE)
    pages = [p["id"] for p in module["lvgl"]["pages"]]
    assert "page_arcade" in pages and len(pages) == 9, pages
    ouvertures = sorted(s for s in _scripts(MODULE) if s != "tab5_arcade_open" and s.endswith("_open"))
    assert len(ouvertures) == 8, ouvertures
    espaces = sorted(check_tab5_registry.games_from_headers())
    assert len(espaces) == 8, espaces
    polices = [f["id"] for f in module["font"]]
    noms = re.compile(r"\b(" + "|".join(pages + ouvertures + polices) + r")\b|\b(" + "|".join(espaces) + r")::")

    du_module = {REPO.joinpath(*MODULE)}
    fautifs = []
    tab5 = REPO / "Tab5"
    autres = sorted((tab5 / "ui_components").glob("*.yaml")) + sorted(
        p for p in (tab5 / "rendu").rglob("*") if p.suffix in (".yaml", ".h", ".cpp"))
    for chemin in sources("*.yaml", "*.h", "*.cpp") + autres + [REPO / r for r in RACINES]:
        if chemin in du_module or "jeux" in chemin.parts or COMPOSANTS_DU_MODULE.match(chemin.name):
            continue
        texte = _sans_commentaires(chemin.read_text(encoding="utf-8"), chemin.suffix)
        fautifs += [f"{chemin.relative_to(REPO)} : {m.group(0)}" for m in noms.finditer(texte)]
    assert fautifs == [], "le firmware sans jeux ne compilerait plus (ADR-0065) : " + "; ".join(fautifs)
