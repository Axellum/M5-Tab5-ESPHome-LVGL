# -*- coding: utf-8 -*-
"""Sens de l'écran choisi dans Home Assistant (ADR-0069, 19/10/2026) : ce qu'aucun compilateur
ne vérifie.

- la 7B a le select « Sens de l'écran » (Normal / Retourné, gardé en NVS, Normal d'abord), qui
  appelle `dalle_sens_choisir()` ; aucune autre carte ne l'a (le Tab5 reste à 270, compilé) ;
- la rotation à chaud n'existe que si `lvgl: rotation:` est dans la configuration (ESPHome ne
  compile la rotation logicielle qu'avec la clé) ;
- le select est restauré (bande 800) avant que la barre de chargement ne dessine (on_boot 500)
  et que LVGL ne démarre (400) : la première image sort dans le bon sens ;
- la barre et le flipper lisent la rotation choisie (`rotation_repos()`), plus la constante ;
- le firmware publié garde la rotation de repos de la carte : le select fait le reste.
"""
import re

import yaml
from tests.commun import REPO, ChargeurBalisesBrutes, ChargeurSansBalises, charger, geometrie, lire, source, sources

SELECT = "tab5_sens_ecran"


def _yaml(nom, chargeur=ChargeurBalisesBrutes):
    return yaml.load(source(nom).read_text(encoding="utf-8"), Loader=chargeur) or {}


def _select_7b():
    selects = [s for s in _yaml("carte-waveshare-7b.yaml").get("select", []) if s.get("id") == SELECT]
    assert len(selects) == 1, f"{SELECT} : {len(selects)} entrée(s) dans carte-waveshare-7b.yaml"
    return selects[0]


def test_select_de_la_7b():
    s = _select_7b()
    assert s["platform"] == "template" and s["name"] == "Sens de l'écran"
    assert s["entity_category"] == "config" and s["optimistic"] is True
    # Gardé en NVS d'un démarrage et d'une mise à jour à l'autre ; une carte neuve dans le sens
    # du fabricant (la rotation de repos de la carte).
    assert s["restore_value"] is True and s["initial_option"] == "Normal"
    # L'index est lu par le lambda (i == 1 : Upside down, « Retourné » avant le 24/10/2026,
    # ADR-0070) : l'ordre des options ne change pas.
    assert s["options"] == ["Normal", "Upside down"]
    lambdas = [a["lambda"] for a in s["on_value"] if "lambda" in a]
    assert lambdas == ["dalle_sens_choisir(id(tab5_lvgl), i == 1);"]


def test_aucune_autre_carte_n_a_le_select():
    """Le Tab5 garde sa rotation compilée (270) : ni select, ni appel, sa configuration résolue
    ne change pas."""
    for chemin in sources("*.yaml") + [REPO / "tab5-ha-hmi.yaml", REPO / "tab5-rendu-host.yaml"]:
        if chemin.name == "carte-waveshare-7b.yaml":
            continue
        texte = chemin.read_text(encoding="utf-8")
        assert SELECT not in texte and "dalle_sens_choisir" not in texte, chemin.name


def test_lvgl_compile_la_rotation():
    """`rotation:` présent = rotation logicielle compilée (ESPHome, LvglComponent::set_rotation)."""
    lvgl = charger("Tab5", "paquets", "tab5-styles.yaml", chargeur=ChargeurSansBalises)["lvgl"]
    assert lvgl["id"] == "tab5_lvgl" and lvgl["rotation"] == "${carte_rotation}"


def _priorite_on_boot(appel):
    racine = charger("tab5-ha-hmi.yaml", chargeur=ChargeurBalisesBrutes)
    trouvees = [e["priority"] for e in racine["esphome"]["on_boot"] if appel in yaml.safe_dump(e.get("then"))]
    assert len(trouvees) == 1, appel
    return trouvees[0]


def test_select_restaure_avant_la_barre_et_lvgl():
    # Select template : setup_priority::HARDWARE (800) dans ESPHome 2026.9.1 ; LVGL :
    # PROCESSOR (400). La barre lit rotation_repos() à l'ouverture.
    assert _priorite_on_boot("barre_demarrage_ouvrir") < 800
    assert "rotation_repos()" in lire("Tab5", "ecran", "tab5_barre_demarrage.cpp")


def test_rotation_lue_a_l_execution():
    """La constante kRotationRepos = la carte dans le sens du fabricant ; ce qui dessine ou
    remet la rotation lit le choix en vigueur."""
    for nom in ("tab5_barre_demarrage.cpp", "pinball_game.cpp"):
        texte = source(nom).read_text(encoding="utf-8")
        assert not re.search(r"\bkRotationRepos\b", texte), nom
        assert "rotation_repos()" in texte, nom
    dalle = lire("Tab5", "ecran", "tab5_dalle.cpp")
    assert "rotation_sens(kRotationRepos, retourne)" in dalle and "set_rotation(angle)" in dalle
    # Un sens choisi pendant une partie n'est que posé : le jeu qui tient sa rotation le dit,
    # et remet rotation_repos() en sortant.
    assert "!rotation_tenue()" in dalle
    assert "rotation_tenue() = portrait;" in source("pinball_game.cpp").read_text(encoding="utf-8")


def test_le_firmware_publie_garde_la_rotation_de_la_carte():
    """Une carte retournée tourne le firmware publié : la publication ne force aucune rotation,
    la 7B part de 180 et le select ajoute le demi-tour."""
    assert "carte_rotation" not in lire(".github", "workflows", "publication.yml")
    assert geometrie("waveshare-7b")["carte_rotation"] == "180"


def test_documente():
    for chemin in (("docs", "waveshare-7b.md"), ("docs", "installation", "settings.md")):
        texte = lire(*chemin)
        # EN et FR, sous le nom que Home Assistant montre (« Sens de l'écran » avant la
        # migration des entités, #540 ; le firmware garde ce nom-là).
        assert texte.count("Screen direction") >= 2, "/".join(chemin)
