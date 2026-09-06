# -*- coding: utf-8 -*-
"""Tests de tools/render_ha_config.py : rendu placeholder -> valeur, et
vérification qu'aucune valeur réelle ne traîne dans un fichier public."""
import os
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from tools.render_ha_config import check, load_map, render, render_text  # noqa: E402


def _ha_tree(tmp_path):
    base = tmp_path / "HomeAssistant_Config"
    (base / "packages").mkdir(parents=True)
    (base / "snippets").mkdir()
    return base


def test_load_map_ignores_comments_and_quotes(tmp_path):
    f = tmp_path / "placeholders.yaml"
    f.write_text(
        "# commentaire\n"
        "VOTRE_VILLE: \"ma_ville\"  # commentaire de fin\n"
        "number.m5stack_x_avant: number.salon_m5stack_x_avant\n"
        "\n",
        encoding="utf-8",
    )
    m = load_map(f)
    assert m == {
        "VOTRE_VILLE": "ma_ville",
        "number.m5stack_x_avant": "number.salon_m5stack_x_avant",
    }


def test_load_map_missing_file_is_empty(tmp_path):
    assert load_map(tmp_path / "absent.yaml") == {}


def test_render_text_longest_key_first():
    m = {"VOTRE_TV": "tv_reelle", "media_player.VOTRE_TV": "media_player.autre"}
    # La clé longue doit être appliquée avant la courte, sinon on obtiendrait
    # media_player.tv_reelle.
    assert render_text("media_player.VOTRE_TV / remote.VOTRE_TV", m) == \
        "media_player.autre / remote.tv_reelle"


def test_render_writes_same_tree(tmp_path):
    base = _ha_tree(tmp_path)
    (base / "packages" / "p.yaml").write_text("entity_id: weather.VOTRE_VILLE\n", encoding="utf-8")
    (base / "scripts_examples.yaml").write_text("x: light.VOTRE_LEDS\n", encoding="utf-8")
    out = tmp_path / "rendered"
    written = render({"VOTRE_VILLE": "ma_ville", "VOTRE_LEDS": "sonoff_1"}, out, base)
    assert {p.relative_to(out).as_posix() for p in written} == {"packages/p.yaml", "scripts_examples.yaml"}
    assert (out / "packages" / "p.yaml").read_text(encoding="utf-8") == "entity_id: weather.ma_ville\n"
    assert (out / "scripts_examples.yaml").read_text(encoding="utf-8") == "x: light.sonoff_1\n"


def test_check_reports_placeholder_name_never_value(tmp_path):
    base = _ha_tree(tmp_path)
    (base / "packages" / "p.yaml").write_text(
        "ok: weather.VOTRE_VILLE\nfuite: weather.ma_ville_reelle\n", encoding="utf-8")
    findings = check({"VOTRE_VILLE": "ma_ville_reelle"}, base)
    assert findings == [("packages/p.yaml", 2, "VOTRE_VILLE")]
    # La valeur réelle ne doit apparaître nulle part dans ce que renvoie check().
    assert "ma_ville_reelle" not in repr(findings)


def test_check_ignores_short_or_identity_values(tmp_path):
    base = _ha_tree(tmp_path)
    (base / "packages" / "p.yaml").write_text("dept: 40\nville: VOTRE_VILLE\n", encoding="utf-8")
    assert check({"VOTRE_DEPARTEMENT": "40", "VOTRE_VILLE": "VOTRE_VILLE"}, base) == []


def test_check_catches_derived_identifiers(tmp_path):
    base = _ha_tree(tmp_path)
    # Entité DÉRIVÉE de la valeur réelle (sensor.<ville>_next_rain) : doit être
    # détectée comme l'entité de base — d'où une recherche en sous-chaîne.
    (base / "packages" / "p.yaml").write_text("x: sensor.ma_ville_reelle_next_rain" + chr(10), encoding="utf-8")
    assert check({"VOTRE_VILLE": "ma_ville_reelle"}, base) == [("packages/p.yaml", 1, "VOTRE_VILLE")]
