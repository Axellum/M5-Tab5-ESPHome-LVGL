# -*- coding: utf-8 -*-
"""Tests de tools/render_ha_config.py : copie des fichiers publics, et vérification
qu'aucune valeur réelle ni aucun placeholder ne traîne dans un fichier public
(ADR-0017, puis ADR-0024 : plus de placeholder, tout se choisit dans Home Assistant)."""


from tools.render_ha_config import check, load_map, main, placeholders, public_files, render  # noqa: E402


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


def test_render_copies_same_tree_byte_for_byte(tmp_path):
    base = _ha_tree(tmp_path)
    (base / "optionnel").mkdir()
    (base / "packages" / "p.yaml").write_bytes(b"entity_id: weather.x\r\n")
    (base / "optionnel" / "o.yaml").write_text("x: 1\n", encoding="utf-8")
    (base / "snippets" / "s.yaml").write_text("x: light.y\n", encoding="utf-8")
    (base / "README.md").write_text("pas copié", encoding="utf-8")
    out = tmp_path / "rendered"
    written = render(out, base)
    assert {p.relative_to(out).as_posix() for p in written} == {
        "packages/p.yaml", "optionnel/o.yaml", "snippets/s.yaml"}
    assert (out / "packages" / "p.yaml").read_bytes() == b"entity_id: weather.x\r\n"


def test_check_reports_key_name_never_value(tmp_path):
    base = _ha_tree(tmp_path)
    (base / "packages" / "p.yaml").write_text(
        "ok: weather.x\nfuite: weather.ma_ville_reelle\n", encoding="utf-8")
    findings = check({"MA_VILLE": "ma_ville_reelle"}, base)
    assert findings == [("packages/p.yaml", 2, "MA_VILLE")]
    # La valeur réelle ne doit apparaître nulle part dans ce que renvoie check().
    assert "ma_ville_reelle" not in repr(findings)


def test_check_ignores_short_or_identity_values(tmp_path):
    base = _ha_tree(tmp_path)
    (base / "packages" / "p.yaml").write_text("dept: 40\nville: MA_VILLE\n", encoding="utf-8")
    assert check({"MON_DEPARTEMENT": "40", "MA_VILLE": "MA_VILLE"}, base) == []


def test_check_catches_derived_identifiers(tmp_path):
    base = _ha_tree(tmp_path)
    # Entité DÉRIVÉE de la valeur réelle (sensor.<ville>_next_rain) : doit être
    # détectée comme l'entité de base — d'où une recherche en sous-chaîne.
    (base / "packages" / "p.yaml").write_text("x: sensor.ma_ville_reelle_next_rain" + chr(10), encoding="utf-8")
    assert check({"MA_VILLE": "ma_ville_reelle"}, base) == [("packages/p.yaml", 1, "MA_VILLE")]


def test_placeholders_found_in_public_files(tmp_path):
    base = _ha_tree(tmp_path)
    (base / "packages" / "p.yaml").write_text("a: weather.x\nb: calendar.VOTRE_EMAIL_gmail_com\n", encoding="utf-8")
    assert placeholders(base) == [("packages/p.yaml", 2)]


def test_check_fails_on_placeholder_even_without_map(tmp_path, monkeypatch, capsys):
    """Un fork sans placeholders.yaml : la fuite de valeurs n'est pas vérifiable, mais un
    placeholder restant l'est toujours (un fichier public doit marcher sans être rempli)."""
    import tools.render_ha_config as outil

    base = _ha_tree(tmp_path)
    (base / "packages" / "p.yaml").write_text("x: media_player.VOTRE_TV\n", encoding="utf-8")
    monkeypatch.setattr(outil, "HA_DIR", base)
    assert main(["--check", "--map", str(tmp_path / "absent.yaml")]) == 1
    assert "PLACEHOLDER" in capsys.readouterr().out


def test_depot_sans_placeholder():
    """ADR-0024 : aucun fichier Home Assistant public (packages, optionnels, snippets,
    custom_templates) ne contient de placeholder `VOTRE_…`."""
    assert public_files(), "aucun fichier public trouvé"
    assert placeholders() == []
