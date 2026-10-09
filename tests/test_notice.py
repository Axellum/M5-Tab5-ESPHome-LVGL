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
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NOTICE = REPO / "docs" / "notice"
IMAGES = REPO / "docs" / "images" / "notice"
TAB5 = REPO / "Tab5"

from ecrans import ECRANS  # noqa: E402
from scenarios import SCENES  # noqa: E402  (tools/demo, chemin ajouté par ecrans)

FRANCAIS = "## Version Française"
CITATION = re.compile(r"images/notice/([a-z0-9-]+)-(fr|en)\.webp")
ANNOTE = "accueil-annote"

# Appui long → page qui le décrit. Clé : l'id du widget dans tab5-lvgl.yaml, le nom du
# fichier pour un composant de Tab5/ui_components/ (ses ids sont des modèles).
APPUIS_LONGS = {
    "btn_assist_trigger": "voice.md",
    # Les trois zones de l'horloge (heures, minutes, date) : un gabarit (09/10/2026, lot A).
    "horloge_zone.yaml": "home.md",
    # Les trois boutons du haut : un gabarit (08/10/2026, audit YML-4).
    "bouton_haut.yaml": "home.md",
    "btn_rangee": "plants.md",
    "climate_card.yaml": "temperature.md",
    "maison_ligne.yaml": "house.md",
    # Carte centrale : chaque panneau de l'accueil ouvre la roue de navigation (ADR-0042)
    # (alertes, pluie, planning, info, alertes HA : un gabarit depuis le 08/10/2026, YML-4),
    # comme le titre d'une page (prévisions, pièce du mode HA).
    "central_bouton.yaml": "home.md",
    "btn_page_title_tap": "home.md",
    "forecast_day_body.yaml": "tiles.md",
    "forecast_hour_card.yaml": "tiles.md",
    "switch_card.yaml": "tiles.md",
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
    # Climat de la pièce en mode HA (ADR-0040), décrit dans temperature.md.
    "temperature-piece": "temperature-serre montre la même fenêtre ; la température d'une pièce est décrite",
    # Humidité et pages du popup Température (ADR-0047), décrites dans temperature.md.
    "temperature-glisser": "temperature-serre montre la même fenêtre ; le glissement est décrit",
    "temperature-onglet": "temperature-serre montre la même fenêtre ; les onglets et l'humidité sont décrits ; "
                          "image à tirer du rendu de la PR",
    "climatisation-piece": "climatisation montre la même fenêtre ; la clim d'une pièce est décrite",
    # Images à tirer du rendu de la PR (tools/site/images_notice.py), puis citées.
    "appareil-scene": "appareil montre la même fenêtre ; la scène est décrite dans tiles.md",
    "console-batterie-en-charge": "console-batterie montre la même ligne ; l'éclair est décrit",
    "console-sans-batterie": "console-batterie montre la même ligne ; « Sur USB » est décrit",
    "roue-lampe": "roue d'actions rapides (ADR-0036), décrite dans tiles.md ; image à tirer du rendu",
    "roue-lampe-couleurs": "roue-lampe montre la même roue ; les couleurs sont décrites dans tiles.md",
    "roue-volet": "roue d'actions rapides (ADR-0036), décrite dans tiles.md ; image à tirer du rendu",
    "roue-clim": "roue d'actions rapides (ADR-0036), décrite dans tiles.md ; image à tirer du rendu",
    # Roue d'une clim par la température de la pièce (ADR-0048), décrite dans climate.md.
    "roue-clim-temperature": "roue d'une clim (ADR-0048), décrite dans climate.md ; image à tirer du rendu",
    "roue-clim-temperature-clims": "roue-clim-temperature montre la même roue ; « Clims » est décrit",
    "roue-clim-temperature-meteo": "roue-clim-temperature montre la même roue, sur la clim du blueprint",
    # Roue de navigation (ADR-0042), décrite dans home.md (carte centrale).
    "roue-navigation": "roue de navigation (ADR-0042), décrite dans home.md ; image à tirer du rendu",
    "roue-navigation-pieces": "roue-navigation montre la même roue ; les familles sont décrites dans home.md",
    "roue-navigation-appareils": "roue-navigation montre la même roue ; les familles sont décrites dans home.md",
    "roue-navigation-tablette": "roue-navigation montre la même roue ; les familles sont décrites dans home.md",
    "roue-navigation-bureau": "accueil-ha-piece-4 montre la même pièce ; le choix d'une pièce est décrit dans home.md",
    "aller-lumieres": "lumieres-chambre montre la même fenêtre ; l'ouverture par la roue est décrite dans home.md",
    "maison": "image à tirer du rendu de la PR du popup Maison, puis citer dans house.md",
    "maison-2-pieces": "maison montrera la même fenêtre ; deux colonnes plus larges, décrites dans house.md",
    "maison-par-le-titre": "maison montre la même fenêtre ; ce tap est décrit dans house.md",
    "maison-roue": "roue d'actions rapides (ADR-0036) devant le popup Maison, décrite dans house.md",
    # Réglages en quatre pages (08/10/2026) : images à tirer du rendu de la PR, puis citées.
    "reglages-apparence": "page Apparence des Réglages, décrite dans settings.md ; image à tirer du rendu",
    "reglages-batterie-en-charge": "page Batterie des Réglages, décrite dans settings.md ; image à tirer du rendu",
    "reglages-sans-batterie": "reglages-batterie-en-charge montre la même page ; « Pas de batterie détectée » "
                              "est décrit dans settings.md",
    "reglages-curseur": "reglages montre la même page ; le curseur qui ne change pas de page est décrit",
    # Carrousel des clims (ADR-0038, 09/10/2026) : décrit dans climate.md et home.md.
    "climatisation-par-la-piece": "climatisation montre la même fenêtre (une seule clim dans la démo)",
    "climatisation-carrousel": "carrousel des clims, décrit dans climate.md ; image à tirer du rendu de la PR",
    "climatisation-carrousel-page-2": "climatisation-carrousel montre le même carrousel, page suivante",
    "climatisation-carrousel-page-3": "climatisation-carrousel montre le même carrousel ; titre de la pièce décrit",
    "climatisation-carrousel-mode-ha": "climatisation-carrousel montre le même carrousel ; l'ouverture sur la "
                                       "pièce est décrite dans climate.md",
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
