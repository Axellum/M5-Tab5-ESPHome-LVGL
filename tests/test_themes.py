"""Palettes, catalogue des thèmes et styles de rôle (thèmes, ADR-0029).

La palette (`struct Palette`, Tab5/tab5_tokens.h) est la seule source des couleurs de
l'interface. Trois listes doivent rester d'accord, et le compilateur n'en surveille
aucune : un champ omis dans une palette vaut 0x000000 sans un mot (initialiseurs
désignés), et un style de rôle qui lirait le mauvais champ compilerait aussi.

Lot 2 : les palettes viennent de Tab5/themes/<thème>.yaml, écrites dans THEMES[] par
tools/gen_themes.py (avec les options du select « Thème » et la repeinture des styles,
Tab5/tab5-themes.yaml) ; chaque mode doit rester lisible (contrastes minimaux).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
TOKENS = TAB5 / "tab5_tokens.h"
THEMES_DATA = TAB5 / "tab5_themes_data.h"
STYLES = TAB5 / "tab5-styles.yaml"
THEMES_YAML = TAB5 / "tab5-themes.yaml"
sys.path.insert(0, str(REPO / "tools"))

import gen_themes  # noqa: E402
RE_LAMBDA = re.compile(r"^return lv_color_hex\(UIColor\.(\w+)\);$")
# Lot 3 : le bandeau central et l'horloge lisent la palette de leur zone.
RE_LAMBDA_ZONE = re.compile(r"^return lv_color_hex\((UIColor|UIBandeau|UIHorloge)\.(\w+)\);$")
RE_ROLE = re.compile(r"^style_(text|bg|border|arc)_(\w+)$")
RE_ZONE = re.compile(r"^style_(bandeau|horloge)_(text|bg|border|arc)_(\w+)$")


class _Chargeur(yaml.SafeLoader):
    pass


def _etiquette(loader, suffix, node):
    return loader.construct_scalar(node) if isinstance(node, yaml.ScalarNode) else None


_Chargeur.add_multi_constructor("!", _etiquette)


def _champs() -> list[str]:
    corps = re.search(r"struct Palette \{(.*?)\n\};", TOKENS.read_text(encoding="utf-8"), re.S).group(1)
    return re.findall(r"^\s*uint32_t (\w+);", corps, re.M)


def _valeurs(texte: str) -> list[tuple[str, str]]:
    return re.findall(r"^\s*\.(\w+)\s*=\s*(0x[0-9A-Fa-f]{6}),", texte, re.M)


def _palettes() -> dict[str, list[tuple[str, str]]]:
    """Chaque palette écrite dans THEMES[] (tab5_themes_data.h), « Thème (mode) » → rôles.
    Le premier thème reprend PALETTE_SOMBRE (tab5_tokens.h) pour son mode sombre."""
    sombre = re.search(r"inline constexpr Palette PALETTE_SOMBRE = \{\n(.*?)\n\};",
                       TOKENS.read_text(encoding="utf-8").replace("\r\n", "\n"), re.S).group(1)
    texte = THEMES_DATA.read_text(encoding="utf-8").replace("\r\n", "\n")
    bloc = re.search(r"inline constexpr Theme THEMES\[\] = \{(.*?)\n\};", texte, re.S).group(1)
    palettes = {}
    for nom, corps in re.findall(r'\{"([^"]+)",[^\n]*\n(.*?)\n     \}\},', bloc, re.S):
        if re.match(r"\s*PALETTE_SOMBRE,", corps):
            palettes[f"{nom} (sombre)"] = _valeurs(sombre)
        for mode, valeurs in re.findall(r"\{  // (sombre|clair)\n(.*?)(?:\n     \}|\Z)", corps, re.S):
            palettes[f"{nom} ({mode})"] = _valeurs(valeurs)
    return palettes


def _lvgl() -> dict:
    return yaml.load(STYLES.read_text(encoding="utf-8"), Loader=_Chargeur)["lvgl"]


def _champ_du_role(role: str) -> str:
    return {"dim": "TEXT_DIM", "soft": "TEXT_SOFT", "on_accent": "TEXT_ON_ACCENT"}.get(role, role.upper())


def test_chaque_palette_donne_tous_les_roles_dans_l_ordre():
    champs = _champs()
    assert len(champs) > 40, "le motif ne lit plus struct Palette"
    assert len(set(champs)) == len(champs)
    palettes = _palettes()
    assert len(palettes) == 2 * len(gen_themes.charger()), "le motif ne lit plus THEMES[]"
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
                m = RE_LAMBDA_ZONE.match(str(val).strip())
                assert m and m.group(2) in champs, (
                    f"{ou}.{cle} = {val!r} : lire `UIColor.X` (ou UIBandeau / UIHorloge), X champ de Palette")
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


def test_styles_de_zone_lisent_la_palette_de_leur_zone():
    """style_bandeau_<prop>_<rôle> lit UIBandeau, style_horloge_<prop>_<rôle> UIHorloge ;
    les cartes du bandeau et de l'horloge aussi. Une couleur d'UIColor dans le bandeau
    resterait claire sur le bandeau sombre d'un thème à `zones_sombres:`."""
    palettes = {"bandeau": "UIBandeau", "horloge": "UIHorloge"}
    styles = {s["id"]: s for s in _lvgl()["style_definitions"]}
    zones = {sid: s for sid, s in styles.items() if RE_ZONE.match(sid)}
    assert len(zones) >= 6, "le motif ne trouve plus les styles de zone"
    for sid, style in zones.items():
        zone, prop, role = RE_ZONE.match(sid).groups()
        props = {k: v for k, v in style.items() if k != "id"}
        assert list(props) == [f"{prop}_color"], f"{sid} : une seule propriété, {prop}_color"
        m = RE_LAMBDA_ZONE.match(str(props[f"{prop}_color"]).strip())
        assert m and m.groups() == (palettes[zone], _champ_du_role(role)), \
            f"{sid} doit lire {palettes[zone]}.{_champ_du_role(role)}"
    for sid, zone in (("style_bandeau_page", "bandeau"), ("style_horloge_page", "horloge")):
        lues = {RE_LAMBDA_ZONE.match(str(v).strip()).group(1) for k, v in styles[sid].items() if k.endswith("_color")}
        assert lues == {palettes[zone]}, f"{sid} doit lire {palettes[zone]}, lu {lues}"


def test_chaque_style_de_role_sert():
    """Pas de style mort : chaque style de rôle est posé par au moins un widget."""
    sources = [p for p in list(TAB5.glob("*.yaml")) + list((TAB5 / "ui_components").glob("*.yaml"))
               if p.name != "tab5-styles.yaml"]
    # Sans les commentaires : un style cité seulement dans un commentaire n'est posé nulle part.
    corpus = "\n".join(l for p in sources for l in p.read_text(encoding="utf-8").splitlines()
                       if not l.lstrip().startswith("#"))
    for style in _lvgl()["style_definitions"]:
        if RE_ROLE.match(style["id"]) or RE_ZONE.match(style["id"]) or style["id"].startswith("style_police_"):
            assert re.search(rf"\b{style['id']}\b", corpus), f"{style['id']} n'est posé par aucun widget"


def test_catalogue_a_jour():
    """THEMES[], les options du select « Thème » et la repeinture des styles suivent
    Tab5/themes/ et tab5-styles.yaml (`python tools/gen_themes.py`)."""
    assert gen_themes.main(["--check"]) == 0


# Propriétés de couleur sans drapeau de mise en page, de zone de dessin ni de calque dans
# LVGL 9.5 (table des drapeaux de src/misc/lv_style.c : 0, ou INHERITABLE pour text_color).
# Les opacités d'ombre et de contour n'y sont pas : elles changent la zone de dessin.
COULEURS_SANS_GEOMETRIE = {"bg_color", "bg_grad_color", "border_color", "outline_color", "shadow_color",
                           "text_color", "arc_color", "line_color", "image_recolor"}


def test_bascule_rapide_ne_coupe_le_rafraichissement_que_pour_des_couleurs():
    """tab5_theme_repeindre repose les couleurs rafraîchissement des styles coupé, puis
    redessine l'écran une fois (3,2 s → ~0,2 s, mesuré le 07/10/2026). Une propriété de
    taille, de place ou de zone de dessin posée pendant la coupure resterait sans effet :
    le bloc généré ne doit y changer que des couleurs, et rallumer puis redessiner."""
    assert set(gen_themes.PROPS_COULEUR) <= COULEURS_SANS_GEOMETRIE
    texte = THEMES_YAML.read_text(encoding="utf-8")
    bloc = re.search(r"# >>> styles[^\n]*\n(.*?)# <<< styles", texte, re.S).group(1)
    assert bloc.count("lv_obj_enable_style_refresh(false);") == 1
    assert bloc.count("lv_obj_enable_style_refresh(true);") == 1
    coupe, apres = bloc.split("lv_obj_enable_style_refresh(false);", 1)[1].split(
        "lv_obj_enable_style_refresh(true);", 1)
    assert "lv_obj_invalidate(lv_screen_active());" in apres.split("- lambda:", 1)[0], \
        "l'écran doit être redessiné juste après la coupure"
    poses = re.findall(r"lv_style_set_(\w+)\(", coupe)
    assert poses, "aucune couleur reposée"
    assert set(poses) <= COULEURS_SANS_GEOMETRIE, sorted(set(poses) - COULEURS_SANS_GEOMETRIE)
    # Rien d'autre que des couleurs, et rien d'asynchrone (un delay ou un wait_until
    # laisserait LVGL dessiner, rafraîchissement coupé) : seules actions admises, une lambda
    # (lv_style_set_* seulement) et lvgl.theme.update (couleurs seulement).
    for ligne in coupe.splitlines():
        ligne = ligne.strip()
        if not ligne or ligne.startswith("//") or ligne.startswith("lv_style_set_"):
            continue
        if ligne.startswith("- "):
            assert ligne in ("- lambda: |-", "- lvgl.theme.update:"), f"pendant la coupure : {ligne}"
            continue
        if re.fullmatch(r"\w+:", ligne):  # widget de lvgl.theme.update
            continue
        prop = re.match(r"(\w+): !lambda 'return lv_color_hex\(\w+\.\w+\);'$", ligne)
        assert prop and prop.group(1) in COULEURS_SANS_GEOMETRIE, f"pendant la coupure : {ligne}"


def test_premier_theme_sombre_est_la_palette_des_jeux():
    texte = TOKENS.read_text(encoding="utf-8")
    assert "inline constexpr Palette PALETTE_SOMBRE = {" in texte
    assert "inline Palette UIColor = PALETTE_SOMBRE;" in texte, "l'écran naît dans la palette sombre"
    premier = re.search(r"inline constexpr Theme THEMES\[\] = \{\r?\n[^\n]*\n([^\n]*)",
                        THEMES_DATA.read_text(encoding="utf-8")).group(1)
    assert premier.strip().startswith("PALETTE_SOMBRE,"), "THEMES[0].sombre doit reprendre PALETTE_SOMBRE"
    assert dict(_palettes()["Ardoise (sombre)"]) == {
        r: f"0x{v:06X}" for r, v in gen_themes.charger()[0].modes["sombre"].items()}


def test_catalogue_hors_des_jetons():
    """Les ~2 900 lignes du catalogue vivent dans tab5_themes_data.h (audit du 07/10/2026,
    DO-3) : tab5_tokens.h, inclus par presque toutes les unités et par les jeux, n'en garde
    rien ; seuls tab5_theme.cpp et tab5_reglages.cpp l'incluent, et les deux configurations
    (tablette, rendu hors tablette) le copient dans le build (`includes:`)."""
    jetons = TOKENS.read_text(encoding="utf-8")
    assert "THEMES[]" not in re.sub(r"//[^\n]*", "", jetons), "THEMES[] est revenu dans tab5_tokens.h"
    assert len(jetons.splitlines()) < 400, "tab5_tokens.h regrossit : les tables vont dans tab5_themes_data.h"
    inclus = sorted(p.name for p in list(TAB5.glob("*.cpp")) + list(TAB5.glob("*.h"))
                    if '#include "tab5_themes_data.h"' in p.read_text(encoding="utf-8"))
    assert inclus == ["tab5_reglages.cpp", "tab5_theme.cpp"], inclus
    for config in ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"):
        texte = (REPO / config).read_text(encoding="utf-8")
        assert re.search(r"^\s+- Tab5/tab5_themes_data\.h\s*$", texte, re.M), f"{config} : includes:"


def test_tables_du_theme_liees_a_theme_count():
    """DO-13 (audit du 07/10/2026) : les tables de tab5_theme.cpp lues par index de thème
    sont liées à THEME_COUNT par des static_assert générés ; une table périmée ne compile
    plus au lieu d'être lue hors de ses bornes au premier changement de thème."""
    texte = (TAB5 / "tab5_theme.cpp").read_text(encoding="utf-8")
    bloc = re.search(r"// >>> formes[^\n]*\n(.*?)// <<< formes", texte, re.S).group(1)
    assert "static_assert(sizeof(kPolices) / sizeof(kPolices[0]) == THEME_COUNT" in bloc
    assert "static_assert(sizeof(kFormesDebut) / sizeof(kFormesDebut[0]) == 2 * THEME_COUNT + 1" in bloc
    assert "static_assert(kFormesDebut[2 * THEME_COUNT] + 1 == sizeof(kFormes) / sizeof(kFormes[0])" in bloc
    assert f"static_assert(THEME_COUNT == {len(gen_themes.charger())}," in THEMES_DATA.read_text(encoding="utf-8")


def test_options_du_select_dans_l_ordre_des_themes():
    """La tablette garde l'INDEX du thème choisi : l'ordre des options ne bouge jamais."""
    texte = THEMES_YAML.read_text(encoding="utf-8")
    bloc = re.search(r"# >>> themes[^\n]*\n(.*?)# <<< themes", texte, re.S).group(1)
    options = re.findall(r'- "([^"]+)"', bloc)
    assert options == [t.nom for t in gen_themes.charger()]


def test_theme_par_defaut_existe():
    """initial_option du select « Thème » : une option du catalogue (sinon ESPHome refuse
    la configuration, et on ne le verrait qu'à la compilation)."""
    texte = (REPO / "Tab5" / "tab5-themes.yaml").read_text(encoding="utf-8")
    bloc = re.split(r"id: tab5_theme\r?\n", texte, maxsplit=1)[1].split("on_value:", 1)[0]
    m = re.search(r'initial_option: "([^"]+)"', bloc)
    assert m and m.group(1) in [t.nom for t in gen_themes.charger()]


def _luminance(c: int) -> float:
    def canal(v: int) -> float:
        v = v / 255
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * canal((c >> 16) & 0xFF) + 0.7152 * canal((c >> 8) & 0xFF) + 0.0722 * canal(c & 0xFF)


def _contraste(a: int, b: int) -> float:
    claire, sombre = sorted((_luminance(a), _luminance(b)), reverse=True)
    return (claire + 0.05) / (sombre + 0.05)


# Minimums WCAG sur les surfaces des cartes (tableau de bord et popups) : texte courant
# 7:1 (AAA), texte secondaire 4,5:1 (AA), couleurs sémantiques (icônes, gros chiffres)
# 3:1 (contenu non textuel, 1.4.11).
SURFACES = ("GLASS_HI_PAGE", "GLASS_LO_PAGE", "GLASS_HI_MODAL", "GLASS_LO_MODAL")
MINIMUMS = {
    "TEXT_PRIMARY": 7.0, "TEXT_SOFT": 7.0, "TEXT_DIM": 4.5,
    "ACCENT": 3.0, "SUCCESS": 3.0, "WARNING": 3.0, "ERROR": 3.0, "INFO": 3.0, "GOLD": 3.0,
    "TEMP_MAX": 3.0, "TEMP_MIN": 3.0, "EARLY": 3.0,
}


def test_chaque_mode_reste_lisible():
    trop_faibles = []
    for theme in gen_themes.charger():
        for mode, p in theme.modes.items():
            for role, minimum in MINIMUMS.items():
                for surface in SURFACES:
                    r = _contraste(p[role], p[surface])
                    if r < minimum:
                        trop_faibles.append(f"{theme.nom} ({mode}) : {role} sur {surface} = {r:.2f} < {minimum}")
    assert not trop_faibles, "\n".join(trop_faibles)


def test_console_lisible_dans_chaque_mode():
    """Retour d'Axel (05/10/2026) : en clair, la console système gardait les valeurs
    blanches et les libellés gris de la console sombre sur le verre clair du popup, et ses
    encadrés de confirmation restaient noirs sous un texte foncé. Libellés 4,5:1 et valeurs
    7:1 sur le verre des popups en clair ; dans les deux modes, le texte des encadrés
    (TEXT_SOFT, TEXT_DIM) se lit sur CONSOLE_BG."""
    trop_faibles = []
    for theme in gen_themes.charger():
        for mode, p in theme.modes.items():
            exigences = [("TEXT_SOFT", "CONSOLE_BG", 7.0), ("TEXT_DIM", "CONSOLE_BG", 4.5)]
            if mode == "clair":
                exigences += [(r, s, m) for s in ("GLASS_HI_MODAL", "GLASS_LO_MODAL")
                              for r, m in (("CONSOLE_LABEL", 4.5), ("CONSOLE_VALUE", 7.0))]
            for role, surface, minimum in exigences:
                r = _contraste(p[role], p[surface])
                if r < minimum:
                    trop_faibles.append(f"{theme.nom} ({mode}) : {role} sur {surface} = {r:.2f} < {minimum}")
    assert not trop_faibles, "\n".join(trop_faibles)


def test_renvoi_de_role_pris_dans_le_theme_qui_herite():
    """`CONSOLE_VALUE: TEXT_PRIMARY` (ardoise.yaml, clair) : chaque thème clair y met son
    propre TEXT_PRIMARY, pas celui d'Ardoise."""
    for theme in gen_themes.charger():
        p = theme.modes["clair"]
        assert p["CONSOLE_VALUE"] == p["TEXT_PRIMARY"] and p["CONSOLE_LABEL"] == p["TEXT_DIM"], theme.nom
        assert p["CONSOLE_BG"] == p["GLASS_HI_MODAL"], theme.nom


def test_texte_sur_l_accent_lisible_en_clair():
    """Les pastilles accent pleines (Tester, Parler, OK) : en sombre, le texte d'avant ce
    rôle (choix d'origine de l'écran) ; en clair, 4,5:1 au moins."""
    for theme in gen_themes.charger():
        p = theme.modes["clair"]
        assert _contraste(p["TEXT_ON_ACCENT"], p["ACCENT"]) >= 4.5, theme.nom
