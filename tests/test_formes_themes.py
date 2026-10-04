"""Formes et zones des thèmes (ADR-0029, lot 3) : tools/gen_themes.py.

Un thème change la géométrie et la matière des styles partagés (`formes:`), et peut
garder sombres le bandeau central et l'horloge en mode clair (`zones_sombres:`). Ces
tests font tourner le générateur sur un thème d'essai, dans un dossier temporaire : les
styles qui suivent un parent, les valeurs écrites comme ESPHome les compilerait, et les
refus (style non prévu, premier thème).
"""
from __future__ import annotations

import shutil
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

import gen_themes  # noqa: E402

ESSAI = """nom: Essai
ordre: 2
herite: ardoise
zones_sombres: [bandeau]
formes:
  style_meteo_card_page:
    radius: 24
    border_width: 2
    border_side: [TOP, LEFT]
    shadow_width: 10
    shadow_ofs_x: 5
    shadow_ofs_y: 5
    bg_grad_dir: NONE
    sombre: {shadow_color: 0x131517, bg_color: 0x363C44}
    clair: {shadow_color: GLASS_RIM, bg_color: 0xEFF2F6}
  style_bandeau_page:
    clair: {bg_color: 0x14171B, bg_grad_color: 0x1F2328}
  style_clim_btn:
    radius: 22
    border_opa: 100%
"""


@pytest.fixture
def dossier(tmp_path: Path) -> Path:
    shutil.copy(REPO / "Tab5" / "themes" / "ardoise.yaml", tmp_path / "ardoise.yaml")
    (tmp_path / "essai.yaml").write_text(ESSAI, encoding="utf-8")
    return tmp_path


def test_opacite_comme_esphome():
    # cv.percentage puis × 255.0, tronqué par static_cast<uint8_t> (lv_validation.py).
    assert [gen_themes.opacite(v, "x") for v in ("35%", "55%", "60%", "100%", "0%")] == [89, 140, 153, 255, 0]
    assert gen_themes.opacite(200, "x") == 200


def test_styles_qui_suivent_leur_parent(dossier: Path):
    themes = gen_themes.charger(dossier)
    essai = themes[1]
    assert essai.zones == ("bandeau",)
    compiles = gen_themes.styles_compiles()
    r = gen_themes.formes_resolues(essai, compiles)
    sombre, clair = r["sombre"], r["clair"]
    # L'horloge reprend tout des cartes météo (même définition dans tab5-styles.yaml).
    assert sombre["style_horloge_page"]["radius"] == ("nombre", 24)
    assert sombre["style_horloge_page"]["bg_color"] == ("couleur", 0x363C44)
    # Les onglets restent sans bordure (leur différence dans tab5-styles.yaml).
    assert "border_width" not in sombre["style_onglet_jour_page"]
    assert sombre["style_onglet_jour_page"]["shadow_width"] == ("nombre", 10)
    # style_meteo_card (sur un popup) : la forme, pas le fond pré-mélangé de la page.
    assert sombre["style_meteo_card"]["radius"] == ("nombre", 24)
    assert "bg_color" not in sombre["style_meteo_card"]
    # Le bandeau : ses couleurs à lui en clair, celles des cartes en sombre.
    assert clair["style_bandeau_page"]["bg_color"] == ("couleur", 0x14171B)
    assert sombre["style_bandeau_page"]["bg_color"] == ("couleur", 0x363C44)
    # Les boutons de la page suivent ceux des popups ; 100 % = 255.
    assert clair["style_clim_btn_page"] == {"radius": ("nombre", 22), "border_opa": ("nombre", 255)}
    # Un rôle reste un rôle (lu dans la palette du style au moment de poser).
    assert clair["style_meteo_card_page"]["shadow_color"] == ("role", "GLASS_RIM")


def test_tables_cpp_et_lambda(dossier: Path):
    themes = gen_themes.charger(dossier)
    cpp, yml = gen_themes.rendre_formes(themes)
    texte = "\n".join(cpp)
    assert "static constexpr int kStylesFormes = 9;" in texte
    assert "&UIBandeau" in texte and "&UIColor" in texte
    assert "LV_STYLE_SHADOW_OFFSET_X, FORME_NOMBRE, 5}" in texte
    assert "LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT" in texte
    assert "LV_GRAD_DIR_NONE" in texte
    # Ardoise : aucune forme (rangées vides) ; l'essai en a dans les deux modes.
    debuts = [l for l in cpp if l.startswith("static constexpr uint16_t kFormesDebut[]")][0]
    valeurs = [int(v) for v in debuts.split("{")[1].rstrip("};").split(",")]
    assert valeurs[0] == valeurs[1] == valeurs[2] == 0 and valeurs[2] < valeurs[3] < valeurs[4]
    # État compilé reposé avant les formes : une ombre absente de tab5-styles.yaml est retirée.
    assert "LV_STYLE_SHADOW_OFFSET_X, FORME_RETIRE, 0},  // style_meteo_card_page" in texte
    assert yml[0] == "- lambda: |-" and "theme_formes(styles, 9);" in yml[-1]


def test_premier_theme_sans_formes(dossier: Path):
    (dossier / "ardoise.yaml").write_text(
        (REPO / "Tab5" / "themes" / "ardoise.yaml").read_text(encoding="utf-8")
        + "\nformes:\n  style_glass_card: {radius: 4}\n", encoding="utf-8")
    with pytest.raises(gen_themes.ErreurTheme, match="premier thème"):
        gen_themes.charger(dossier)


def test_style_non_prevu_refuse(dossier: Path):
    (dossier / "essai.yaml").write_text(ESSAI + "  style_pill_accent: {radius: 4}\n", encoding="utf-8")
    with pytest.raises(gen_themes.ErreurTheme, match="non prévu"):
        gen_themes.charger(dossier)


def test_zone_inconnue_refusee(dossier: Path):
    (dossier / "essai.yaml").write_text(ESSAI.replace("[bandeau]", "[bandeau, meteo]"), encoding="utf-8")
    with pytest.raises(gen_themes.ErreurTheme, match="zones_sombres"):
        gen_themes.charger(dossier)


def test_drapeaux_des_zones_dans_themes(dossier: Path):
    lignes = gen_themes.rendre_cpp(gen_themes.charger(dossier))
    assert '    {"Ardoise", false, false,  // Tab5/themes/ardoise.yaml' in lignes
    assert '    {"Essai", true, false,  // Tab5/themes/essai.yaml' in lignes
