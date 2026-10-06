# -*- coding: utf-8 -*-
"""Notice d'utilisation (docs/notice/) : chaque geste du firmware et chaque fenêtre du
rendu y sont, dans les deux langues.

Un appui long ajouté au YAML, un geste de plus ou une nouvelle fenêtre dans le rendu
(tools/rendu/ecrans.py) fait échouer ce test tant que la notice ne les décrit pas, ou
qu'ils ne sont pas écartés ici avec leur raison. Les images de la notice viennent des
captures du rendu (tools/site/images_notice.py) : une page ne peut citer qu'une image
présente, et aucune image ne reste sans page.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NOTICE = REPO / "docs" / "notice"
IMAGES = REPO / "docs" / "images" / "notice"
TAB5 = REPO / "Tab5"
sys.path.insert(0, str(REPO / "tools" / "rendu"))

from ecrans import ECRANS  # noqa: E402
from scenarios import SCENES  # noqa: E402  (tools/demo, chemin ajouté par ecrans)

FRANCAIS = "## Version Française"
CITATION = re.compile(r"images/notice/([a-z0-9-]+)-(fr|en)\.webp")
ANNOTE = "accueil-annote"

# Appui long → page qui le décrit. Clé : l'id du widget dans tab5-lvgl.yaml, le nom du
# fichier pour un composant de Tab5/ui_components/ (ses ids sont des modèles).
APPUIS_LONGS = {
    "btn_assist_trigger": "voice.md",
    "btn_clock_calendar_zone": "calendar.md",
    "btn_control_console": "home.md",
    "btn_control_ha": "home.md",
    "btn_control_tv": "home.md",
    "btn_rangee": "plants.md",
    "climate_card.yaml": "temperature.md",
    "forecast_daily.yaml": "tiles.md",
    "forecast_hour_card.yaml": "tiles.md",
    "switches_card.yaml": "tiles.md",
}

# Fenêtres du rendu que la notice ne montre pas, exprès.
NON_MONTREES = {
    "assistant": "assistant-reponse montre la même fenêtre, avec une réponse",
    "lumieres-salon": "lumieres-chambre montre la même fenêtre, lumière allumée",
    "energie-heures": "energie-jours montre la même fenêtre ; les vues sont décrites",
    "energie-mois": "energie-jours montre la même fenêtre ; les vues sont décrites",
    "console-confirmer-redemarrage-ha": "même confirmation que console-confirmer-reboot",
    "temperature-salon": "temperature-serre montre la même fenêtre, avec la prévision en plus",
    "temperature-serre-semaine": "temperature-serre montre la même fenêtre ; les vues sont décrites",
    "temperature-serre-mois": "temperature-serre montre la même fenêtre ; les vues sont décrites",
    # Images à tirer du rendu de la PR (tools/site/images_notice.py), puis citées.
}


def _pages() -> list[Path]:
    return sorted(NOTICE.glob("*.md"))


def _moities(page: Path) -> tuple[str, str]:
    texte = page.read_text(encoding="utf-8")
    assert texte.count(FRANCAIS) == 1, f"{page.name} : une seule « {FRANCAIS} »"
    anglais, francais = texte.split(FRANCAIS)
    return anglais, francais


def _citees() -> set[str]:
    return {m.group(1) for page in _pages()
            for m in CITATION.finditer(page.read_text(encoding="utf-8"))}


def _fenetres() -> set[str]:
    """Écrans du rendu qui ne sont ni une variante de l'accueil, ni un jeu, ni une scène."""
    return {e.nom for e in ECRANS
            if not e.nom.startswith(("accueil-", "jeu-")) and not e.nom[0].isdigit()}


def test_chaque_page_a_ses_deux_langues_et_les_memes_images():
    assert len(_pages()) >= 14
    for page in _pages():
        anglais, francais = _moities(page)
        en = [m.groups() for m in CITATION.finditer(anglais)]
        fr = [m.groups() for m in CITATION.finditer(francais)]
        assert all(langue == "en" for _, langue in en), f"{page.name} : image française en anglais"
        assert all(langue == "fr" for _, langue in fr), f"{page.name} : image anglaise en français"
        assert [e for e, _ in en] == [e for e, _ in fr], f"{page.name} : pas les mêmes images"


def test_images_citees_presentes_et_aucune_orpheline():
    presentes = {p.name for p in IMAGES.glob("*.webp")}
    attendues = {f"{e}-{langue}.webp" for e in _citees() for langue in ("en", "fr")}
    assert attendues - presentes == set(), (
        f"lancer tools/site/images_notice.py : {sorted(attendues - presentes)}")
    assert presentes - attendues == set(), f"image sans page : {sorted(presentes - attendues)}"


def test_images_tirees_du_rendu():
    noms = {e.nom for e in ECRANS}
    for ecran in _citees() - {ANNOTE}:
        if ecran[0].isdigit():
            assert 1 <= int(ecran.split("-")[0]) <= len(SCENES), ecran
        else:
            assert ecran in noms, f"{ecran} : pas un écran de tools/rendu/ecrans.py"


def test_chaque_fenetre_du_rendu_est_montree():
    fenetres = _fenetres()
    assert set(NON_MONTREES) <= fenetres, "NON_MONTREES cite un écran qui n'existe plus"
    manquantes = fenetres - _citees() - set(NON_MONTREES)
    assert not manquantes, f"montrer dans docs/notice/ ou écarter dans NON_MONTREES : {sorted(manquantes)}"


def _proprietaire(fichier: Path, lignes: list[str], n: int) -> str:
    """Le widget d'un `on_long_press:` : l'`id:` frère le plus proche au-dessus (même
    retrait) dans tab5-lvgl.yaml, le fichier pour un composant."""
    if fichier.parent.name == "ui_components":
        return fichier.name
    retrait = len(lignes[n]) - len(lignes[n].lstrip())
    for ligne in reversed(lignes[:n]):
        m = re.match(rf"^ {{{retrait}}}id: (\S+)", ligne)
        if m:
            return m.group(1)
    raise AssertionError(f"{fichier.name}:{n + 1} : appui long sans id")


def test_chaque_appui_long_est_decrit():
    trouves = {}
    for fichier in sorted(TAB5.rglob("*.yaml")):
        lignes = fichier.read_text(encoding="utf-8").splitlines()
        for n, ligne in enumerate(lignes):
            if ligne.lstrip().startswith("on_long_press:"):
                trouves[_proprietaire(fichier, lignes, n)] = fichier.name
    assert set(trouves) == set(APPUIS_LONGS), (
        f"décrire dans docs/notice/ puis ajouter à APPUIS_LONGS : {sorted(set(trouves) - set(APPUIS_LONGS))} ; "
        f"plus dans le firmware : {sorted(set(APPUIS_LONGS) - set(trouves))}")
    for page in set(APPUIS_LONGS.values()):
        anglais, francais = _moities(NOTICE / page)
        assert "long press" in anglais.lower() and "appui long" in francais.lower(), page
    # Un appui long en C++ (un jeu) passerait sous ce test : il n'y en a pas.
    cpp = [f.name for f in TAB5.rglob("*.cpp") if "LV_EVENT_LONG_PRESSED" in f.read_text(encoding="utf-8")]
    assert not cpp, f"appui long en C++, à décrire dans docs/notice/ : {cpp}"


def test_le_glissement_est_decrit():
    gestes = [f.name for f in TAB5.rglob("*.yaml") for ligne in f.read_text(encoding="utf-8").splitlines()
              if ligne.lstrip().startswith("on_gesture:")]
    assert gestes == ["tab5-lvgl.yaml"], f"un geste de plus, à décrire dans docs/notice/ : {gestes}"
    anglais, francais = _moities(NOTICE / "tiles.md")
    assert "**Swipe**" in anglais and "**Glisser**" in francais


def test_legende_de_l_accueil_annote():
    """Les numéros de la légende (README) sont ceux posés sur l'image (REPERES)."""
    source = (REPO / "tools" / "site" / "images_notice.py").read_text(encoding="utf-8")
    bloc = source[source.index("REPERES = {"):]
    bloc = bloc[:bloc.index("\n}\n")]
    reperes = [int(n) for n in re.findall(r"^\s+(\d+): \(", bloc, re.M)]
    assert reperes == list(range(1, len(reperes) + 1))
    for moitie in _moities(NOTICE / "README.md"):
        legende = [int(n) for n in re.findall(r"^\| (\d+) \|", moitie, re.M)]
        assert legende == reperes, "légende de docs/notice/README.md ≠ REPERES de images_notice.py"
