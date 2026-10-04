"""Palettes et styles de rôle (thèmes, ADR-0029).

La palette (`struct Palette`, Tab5/tab5_tokens.h) est la seule source des couleurs de
l'interface. Trois listes doivent rester d'accord, et le compilateur n'en surveille
aucune : un champ omis dans une palette vaut 0x000000 sans un mot (initialiseurs
désignés), et un style de rôle qui lirait le mauvais champ compilerait aussi.
"""
from __future__ import annotations

import re
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
TOKENS = TAB5 / "tab5_tokens.h"
STYLES = TAB5 / "tab5-styles.yaml"
RE_LAMBDA = re.compile(r"^return lv_color_hex\(UIColor\.(\w+)\);$")
RE_ROLE = re.compile(r"^style_(text|bg|border|arc)_(\w+)$")


class _Chargeur(yaml.SafeLoader):
    pass


def _etiquette(loader, suffix, node):
    return loader.construct_scalar(node) if isinstance(node, yaml.ScalarNode) else None


_Chargeur.add_multi_constructor("!", _etiquette)


def _champs() -> list[str]:
    corps = re.search(r"struct Palette \{(.*?)\n\};", TOKENS.read_text(encoding="utf-8"), re.S).group(1)
    return re.findall(r"^\s*uint32_t (\w+);", corps, re.M)


def _palettes() -> dict[str, list[tuple[str, str]]]:
    texte = TOKENS.read_text(encoding="utf-8")
    return {
        nom: re.findall(r"^\s*\.(\w+)\s*=\s*(0x[0-9A-Fa-f]{6}),", corps, re.M)
        for nom, corps in re.findall(r"inline constexpr Palette (PALETTE_\w+) = \{(.*?)\n\};", texte, re.S)
    }


def _lvgl() -> dict:
    return yaml.load(STYLES.read_text(encoding="utf-8"), Loader=_Chargeur)["lvgl"]


def _champ_du_role(role: str) -> str:
    return {"dim": "TEXT_DIM", "soft": "TEXT_SOFT"}.get(role, role.upper())


def test_chaque_palette_donne_tous_les_roles_dans_l_ordre():
    champs = _champs()
    assert len(champs) > 40, "le motif ne lit plus struct Palette"
    assert len(set(champs)) == len(champs)
    palettes = _palettes()
    assert "PALETTE_SOMBRE" in palettes
    for nom, valeurs in palettes.items():
        assert [c for c, _ in valeurs] == champs, f"{nom} : rôle manquant, en trop ou hors de l'ordre de struct Palette"


def test_vigilance_officielle_dans_toutes_les_palettes():
    for nom, valeurs in _palettes().items():
        v = {c: x.upper() for c, x in valeurs}
        assert (v["ALERT_YELLOW"], v["ALERT_RED"]) == ("0XFFFF00", "0XFF0000"), f"{nom} : vigilance Météo-France modifiée"


def test_toute_couleur_des_styles_lit_la_palette():
    """Styles partagés, thème des widgets et fond : une lambda qui lit un champ de Palette."""
    champs = set(_champs())
    lvgl = _lvgl()

    def visite(noeud, ou):
        for cle, val in noeud.items():
            if cle.endswith("_color"):
                m = RE_LAMBDA.match(str(val).strip())
                assert m and m.group(1) in champs, f"{ou}.{cle} = {val!r} : lire `UIColor.X`, X champ de Palette"
            elif isinstance(val, dict):
                visite(val, f"{ou}.{cle}")

    for style in lvgl["style_definitions"]:
        visite(style, style["id"])
    visite(lvgl.get("theme") or {}, "theme")
    visite({"bg_color": lvgl["bg_color"]}, "lvgl")


def test_styles_de_role_une_seule_couleur_et_le_bon_champ():
    roles = {s["id"]: s for s in _lvgl()["style_definitions"] if RE_ROLE.match(s["id"])}
    assert len(roles) >= 30, "le motif ne trouve plus les styles de rôle"
    for sid, style in roles.items():
        prop, role = RE_ROLE.match(sid).groups()
        props = {k: v for k, v in style.items() if k != "id"}
        assert list(props) == [f"{prop}_color"], f"{sid} : une seule propriété, {prop}_color"
        m = RE_LAMBDA.match(str(props[f"{prop}_color"]).strip())
        assert m and m.group(1) == _champ_du_role(role), f"{sid} doit lire UIColor.{_champ_du_role(role)}"


def test_chaque_style_de_role_sert():
    """Pas de style mort : chaque style de rôle est posé par au moins un widget."""
    sources = [p for p in list(TAB5.glob("*.yaml")) + list((TAB5 / "ui_components").glob("*.yaml"))
               if p.name != "tab5-styles.yaml"]
    corpus = "\n".join(p.read_text(encoding="utf-8") for p in sources)
    for style in _lvgl()["style_definitions"]:
        if RE_ROLE.match(style["id"]):
            assert re.search(rf"\b{style['id']}\b", corpus), f"{style['id']} n'est posé par aucun widget"
