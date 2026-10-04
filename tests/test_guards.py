# -*- coding: utf-8 -*-
"""Les huit garde-fous de contenu, joués par pytest (donc par la CI) :

- cadre modal v4 (ADR-0009) sur chaque popup de Tab5/ui_components/ ;
- registre unique des consoles et des fenêtres modales (ADR-0013) : aucune
  liste recopiée dans un YAML, aucun jeu ni popup oublié ;
- règles de code (ADR-0006) : snprintf partout, aucun lv_* dans le contrat API
  ni dans le fichier matériel, aucun global orphelin, … et la règle 7 (icônes
  MDI), falsifiée sur une copie mutée du firmware (glyphe manquant, glyphe
  mort, icône C++ non rattachée) ;
- les 6 salles de « Fil d'Or » sont traversables et tout le loot atteignable ;
- les 10 niveaux de « Coureur d'Or » sont jouables jusqu'à la sortie ;
- les 8 niveaux d'« Arcanoïde » sont complets et finissables (aucune brique
  emmurée), et la banque de « Trial Poursuite » n'a ni entrée manquante ni
  question vide ou en double (une entrée oubliée compile en C++) ;
- les comptes de lignes de CARTOGRAPHIE_TAB5.md restent à 20 % du réel.

Chaque script reste lançable seul (`python tools/check_*.py`) ; ici on ne fait
que relire son verdict. Ils lisent le C++/YAML réel du dépôt : une salle ou une
map cassée fait échouer la suite avant tout flash."""
import os
import shutil
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from tools import (  # noqa: E402
    cartographie_counts,
    check_arkanoid_levels,
    check_lode_levels,
    check_marble_rooms,
    check_tab5_code_rules,
    check_tab5_modal_chrome,
    check_tab5_registry,
    check_trivia_questions,
)


def test_modal_chrome_adr_0009():
    assert check_tab5_modal_chrome.scan() == []


def test_registry_adr_0013():
    assert check_tab5_registry.scan() == []


def test_code_rules_adr_0006():
    assert check_tab5_code_rules.scan() == []


def _firmware_copy(tmp_path):
    """Copie des sources du firmware (sans TTF ni médias), mutable sans risque."""
    src = check_tab5_code_rules.TAB5
    dst = tmp_path / "Tab5"
    (dst / "ui_components").mkdir(parents=True)
    for pattern in ("*.yaml", "*.cpp", "*.h"):
        for path in src.glob(pattern):
            shutil.copy2(path, dst / path.name)
    for path in (src / "ui_components").glob("*.yaml"):
        shutil.copy2(path, dst / "ui_components" / path.name)
    entry = tmp_path / check_tab5_code_rules.ENTRY.name
    shutil.copy2(check_tab5_code_rules.ENTRY, entry)
    return dst, entry


def _edit_font(styles, font_id, old, new):
    """Remplace `old` par `new` dans l'entrée `font_id` de tab5-styles.yaml seulement."""
    text = styles.read_text(encoding="utf-8")
    start = text.index(f"id: {font_id}\n")
    end = text.find("- file:", start)
    section = text[start:end]
    assert old in section, f"{old!r} absent de {font_id}"
    styles.write_text(text[:start] + section.replace(old, new, 1) + text[end:], encoding="utf-8")


def test_mdi_rule_7_catches_missing_glyph(tmp_path):
    """Le bug réel du 25/09/2026 : la cloche barrée absente de mdi_font_45."""
    tab5, entry = _firmware_copy(tmp_path)
    assert check_tab5_code_rules.mdi_glyph_coverage(tab5, entry) == []
    _edit_font(tab5 / "tab5-styles.yaml", "mdi_font_45", '      - "\\U000F0023"  # alarm-off\n', "")
    problems = check_tab5_code_rules.mdi_glyph_coverage(tab5, entry)
    assert any("U+F0023 absente de `mdi_font_45`" in p for p in problems), problems


def test_mdi_rule_7_catches_dead_glyph(tmp_path):
    tab5, entry = _firmware_copy(tmp_path)
    _edit_font(tab5 / "tab5-styles.yaml", "mdi_font_70", "    glyphs:\n", '    glyphs:\n      - "\\U000F0026"\n')
    problems = check_tab5_code_rules.mdi_glyph_coverage(tab5, entry)
    assert any("`mdi_font_70` embarque 1 glyphe(s) jamais affiché(s) : U+F0026" in p for p in problems), problems


def test_mdi_rule_7_catches_untracked_cpp_icon(tmp_path):
    """Une icône posée depuis une fonction C++ inconnue ne passe pas en silence."""
    tab5, entry = _firmware_copy(tmp_path)
    with open(tab5 / "tab5_cards.cpp", "a", encoding="utf-8") as f:
        f.write('\nvoid nouvelle_icone(lv_obj_t* o) {\n    lv_label_set_text(o, "\\U000F0020");\n}\n')
    problems = check_tab5_code_rules.mdi_glyph_coverage(tab5, entry)
    assert any("nouvelle_icone" in p and "MDI_CODE_TARGETS" in p for p in problems), problems


def test_palette_rule_8_catches_frozen_colors(tmp_path):
    """Règle 8 (thèmes) : une couleur posée en dur sur un widget, une couleur de jeu dans
    l'interface, un jeu qui lit la palette active."""
    tab5, entry = _firmware_copy(tmp_path)
    assert check_tab5_code_rules.palette_colors(tab5, entry) == []
    popup = tab5 / "ui_components" / "alarm_popup.yaml"
    text = popup.read_text(encoding="utf-8")
    assert "styles: style_text_dim }" in text
    popup.write_text(text.replace("styles: style_text_dim }", "text_color: white }", 1), encoding="utf-8")
    with open(tab5 / "ui_components" / "climate_card.yaml", "a", encoding="utf-8") as f:
        f.write("\nessai:\n  - obj: { bg_color: color_marble_void }\n")
    with open(tab5 / "marble_game.cpp", "a", encoding="utf-8") as f:
        f.write("\nstatic uint32_t teinte() { return UIColor.TEXT_DIM; }\n")
    problems = check_tab5_code_rules.palette_colors(tab5, entry)
    assert any("alarm_popup.yaml" in p and "`text_color: white`" in p for p in problems), problems
    assert any("climate_card.yaml" in p and "`color_marble_void` est une couleur de jeu" in p for p in problems), problems
    assert any("marble_game.cpp" in p and "PALETTE_SOMBRE" in p for p in problems), problems


def test_marble_rooms_all_traversable(capsys):
    assert check_marble_rooms.main() == 0, capsys.readouterr().out


def test_lode_levels_all_playable(capsys):
    assert check_lode_levels.main() == 0, capsys.readouterr().out


def test_arkanoid_levels_complete_and_winnable():
    assert check_arkanoid_levels.scan() == []


def test_arkanoid_guard_catches_missing_value_and_walled_brick():
    """Falsifiabilité : une valeur oubliée, puis une brique emmurée par des indestructibles."""
    text = check_arkanoid_levels.GAME.read_text(encoding="utf-8")
    manque = text.replace("    {0,1,1,1,1,1,1,1,1,1,1,0},", "    {0,1,1,1,1,1,1,1,1,1,1},", 1)
    assert any("LVL2" in p and "valeurs" in p for p in check_arkanoid_levels.scan(manque))
    # LVL1, coin haut gauche : la brique (1, 1) a un indestructible à droite et un dessous.
    emmure = text.replace(
        "    {1,1,1,1,1,1,1,1,1,1,1,1},\n    {1,1,1,1,1,1,1,1,1,1,1,1},",
        "    {1,3,1,1,1,1,1,1,1,1,1,1},\n    {3,1,1,1,1,1,1,1,1,1,1,1},", 1)
    problems = check_arkanoid_levels.scan(emmure)
    assert any("LVL1" in p and "(1, 1)" in p for p in problems), problems


def test_trivia_bank_complete():
    assert check_trivia_questions.scan() == []


def test_trivia_guard_catches_missing_and_bad_entries():
    """Falsifiabilité : une question supprimée (le C++ compilerait une entrée nulle), un leurre identique."""
    text = check_trivia_questions.BANK.read_text(encoding="utf-8")
    ligne = '    {0,0,"Quelle est la capitale de la France ?","Paris","Lyon","Marseille","Bordeaux"},\n'
    assert ligne in text
    assert any("QUESTIONS_GEO : 119 questions" in p for p in check_trivia_questions.scan(text.replace(ligne, "", 1)))
    leurre = ligne.replace('"Lyon"', '"paris"')
    assert any("leurres" in p for p in check_trivia_questions.scan(text.replace(ligne, leurre, 1)))


def test_cartographie_line_counts():
    rows = cartographie_counts.scan(cartographie_counts.CARTO.read_text(encoding="utf-8"),
                                    cartographie_counts.tracked())
    assert len(rows) > 40, "le motif ne reconnaît plus les tableaux de la cartographie"
    assert cartographie_counts.drifts(rows) == [],         "lancer `python tools/cartographie_counts.py --write`"


def test_cartographie_drift_is_detected():
    """Falsifiabilité : un compte faux de 50 % doit être signalé."""
    assert cartographie_counts.drifts([(1, "x.cpp", 150, 100)]) != []
    assert cartographie_counts.drifts([(1, "x.cpp", 110, 100)]) == []
