#!/usr/bin/env python3
"""tools/gen_themes.py — catalogue des thèmes de l'écran (ADR-0029, lots 2 et 3).

[AI-CONTEXT] Source unique : Tab5/themes/<thème>.yaml (un fichier par thème : `nom`,
`ordre`, un mode `sombre` et un mode `clair`, chacun avec tous les rôles de
`struct Palette` ; en option `zones_sombres:`, `formes:` et `polices:`), plus les rôles
eux-mêmes (`struct Palette`, Tab5/tab5_tokens.h), les styles partagés
(Tab5/tab5-styles.yaml) et les polices mesurées (Tab5/themes/_polices.yaml, écrit par
tools/police_theme.py). Écrit :

  (a) Tab5/tab5_tokens.h — THEMES[] (nom, zones sombres, palette sombre, palette
      claire) et THEME_COUNT, entre `// >>> themes` et `// <<< themes` ;
  (b) Tab5/tab5-themes.yaml — les options du select « Thème », entre `# >>> themes` et
      `# <<< themes`, dans l'ordre des `ordre:` (la tablette garde l'INDEX : un thème
      s'ajoute à la fin) ;
  (c) Tab5/tab5-themes.yaml — la repeinture des styles partagés après un changement
      de thème, entre `# >>> styles` et `# <<< styles` : chaque couleur de
      `style_definitions:` et du `theme:` (une lambda qui lit `UIColor.X`, ou
      `UIBandeau.X` / `UIHorloge.X` dans le bandeau central et l'horloge) y est reposée
      depuis la palette active, puis les objets de ce style sont rafraîchis
      (lv_obj_report_style_change, comme l'action lvgl.style.update) ; suivent l'appel
      des formes (theme_formes) et des polices (theme_polices) ;
  (d) Tab5/tab5-themes.yaml — les polices des thèmes (bloc `font:`), entre
      `# >>> polices` et `# <<< polices` ;
  (e) Tab5/tab5_theme.cpp — les tables des formes et des polices, entre `// >>> formes`
      et `// <<< formes`.

    python tools/gen_themes.py          # réécrit les parties générées
    python tools/gen_themes.py --check  # exit 1 si l'une est périmée (n'écrit rien)

Palettes. Un mode peut omettre le verre pré-mélangé (GLASS_HI_PAGE / GLASS_LO_PAGE :
GLASS_HI / GLASS_LO à 58 % sur BG ; GLASS_HI_MODAL / GLASS_LO_MODAL : à 88 % sur
MODAL_SCRIM) : il est calculé comme lv_color_mix(). `herite: <fichier>` reprend les
deux palettes d'un autre thème avant d'appliquer celles du fichier (les palettes
seulement : ni formes, ni polices, ni zones).

Zones sombres. `zones_sombres: [bandeau, horloge]` : en mode clair, le bandeau central
et/ou l'horloge gardent la palette SOMBRE du thème (UIBandeau, UIHorloge) ; leur fond
se donne dans `formes:` (style_bandeau_page, style_horloge_page), en clair.

Formes. `formes: {<style>: {<propriété>: valeur, sombre: {…}, clair: {…}}}` change la
géométrie et la matière d'un style partagé de STYLES_FORMES : rayon, bordure (largeur,
côtés, opacité, couleur), dégradé (couleurs, direction, arrêts), ombre (largeur,
décalage, étalement, couleur, opacité), contour. Une couleur est 0xRRGGBB ou le nom
d'un rôle de la palette. Un style de SUIT reprend les formes de son parent, sauf ce
qui le distingue de lui dans tab5-styles.yaml (ex. les onglets restent sans bordure).
Le premier thème (`ordre: 1`) est l'état compilé de tab5-styles.yaml : ni formes, ni
polices, ni zones.

Polices. `polices: {horloge: "Famille@graisse", date: …, titre: …}` (Google Fonts) :
l'heure (rouleaux et sonnerie), la date sous l'horloge, les titres (en-têtes des
popups, titre de la carte centrale). Taille et position viennent de _polices.yaml ;
les glyphes absents du fichier sont dessinés par la Roboto du même rôle (repli).

Fins de ligne : chaque fichier est réécrit avec les siennes ; `--check` compare en
normalisant. Rien d'autre que les parties générées ne change.
"""
from __future__ import annotations

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
THEMES_DIR = REPO / "Tab5" / "themes"
TOKENS = REPO / "Tab5" / "tab5_tokens.h"
STYLES = REPO / "Tab5" / "tab5-styles.yaml"
FIRMWARE = REPO / "Tab5" / "tab5-themes.yaml"
THEME_CPP = REPO / "Tab5" / "tab5_theme.cpp"
POLICES = THEMES_DIR / "_polices.yaml"

MARQUES_CPP = ("// >>> themes", "// <<< themes")
MARQUES_OPTIONS = ("# >>> themes", "# <<< themes")
MARQUES_STYLES = ("# >>> styles", "# <<< styles")
MARQUES_POLICES = ("# >>> polices", "# <<< polices")
MARQUES_FORMES = ("// >>> formes", "// <<< formes")

# Verre pré-mélangé : rôle → (verre, fond, opacité sur 255), comme les commentaires de
# tab5-styles.yaml (58 % = 147, 88 % = 224).
DERIVES = {
    "GLASS_HI_PAGE": ("GLASS_HI", "BG", 147),
    "GLASS_LO_PAGE": ("GLASS_LO", "BG", 147),
    "GLASS_HI_MODAL": ("GLASS_HI", "MODAL_SCRIM", 224),
    "GLASS_LO_MODAL": ("GLASS_LO", "MODAL_SCRIM", 224),
}
MODES = ("sombre", "clair")
# Propriétés couleur de LVGL qu'un style peut porter (lv_style_set_<prop>).
PROPS_COULEUR = ("bg_color", "bg_grad_color", "border_color", "outline_color", "shadow_color",
                 "text_color", "arc_color", "line_color", "image_recolor")
# Palettes qu'une couleur de style peut lire : l'interface, et les deux zones qu'un
# thème clair peut garder sombres (Tab5/tab5_tokens.h).
PALETTES = ("UIColor", "UIBandeau", "UIHorloge")
ZONES = {"bandeau": "UIBandeau", "horloge": "UIHorloge"}
RE_LAMBDA = re.compile(r"^return lv_color_hex\((UIColor|UIBandeau|UIHorloge)\.([A-Z0-9_]+)\);$")
RE_NOM = re.compile(r"^[^\"\\\n]{1,24}$")
RE_POLICE = re.compile(r"^[A-Za-z0-9 ]+@[1-9]00$")

# Styles qu'un thème peut redessiner (`formes:`), et ceux qui suivent un parent.
STYLES_FORMES = (
    "style_meteo_card_page", "style_horloge_page", "style_bandeau_page", "style_clim_carte_page",
    "style_onglet_jour_page", "style_onglet_temp_page", "style_meteo_card",
    "style_clim_btn", "style_clim_btn_page",
    "style_modal_card", "style_modal_card_verre", "style_glass_card",
)
SUIT = {
    "style_horloge_page": "style_meteo_card_page",
    "style_bandeau_page": "style_meteo_card_page",
    "style_clim_carte_page": "style_meteo_card_page",
    "style_onglet_jour_page": "style_meteo_card_page",
    "style_onglet_temp_page": "style_meteo_card_page",
    "style_meteo_card": "style_meteo_card_page",
    "style_clim_btn_page": "style_clim_btn",
    "style_modal_card_verre": "style_modal_card",
}
# Propriété → genre de valeur. Les noms sont ceux d'ESPHome (style_definitions).
PROPS_FORME = {
    "radius": "nombre", "border_width": "nombre", "border_side": "cotes", "border_opa": "opacite",
    "border_color": "couleur", "bg_color": "couleur", "bg_grad_color": "couleur", "bg_grad_dir": "degrade",
    "bg_main_stop": "arret", "bg_grad_stop": "arret", "shadow_width": "nombre", "shadow_ofs_x": "decalage",
    "shadow_ofs_y": "decalage", "shadow_spread": "decalage", "shadow_color": "couleur", "shadow_opa": "opacite",
    "outline_width": "nombre", "outline_pad": "nombre", "outline_color": "couleur", "outline_opa": "opacite",
}
LV_PROP = {"shadow_ofs_x": "LV_STYLE_SHADOW_OFFSET_X", "shadow_ofs_y": "LV_STYLE_SHADOW_OFFSET_Y"}
COTES = {"NONE": 0, "BOTTOM": 1, "TOP": 2, "LEFT": 4, "RIGHT": 8, "FULL": 15}
DEGRADES = ("NONE", "VER", "HOR")

# Polices : rôles, Roboto compilée de chaque rôle (tab5-styles.yaml), style partagé.
ROLES_POLICE = ("horloge", "date", "titre")
ROBOTO = {"horloge": "roboto_130_b", "date": "roboto_45_b", "titre": "roboto_32_b"}
STYLES_POLICE = {"horloge": "style_police_horloge", "date": "style_police_date", "titre": "style_police_titre"}
REFERENCE_POLICE = "Roboto@700"
# Labels de l'horloge que theme_polices() place (rouleaux puis « : »), tab5-lvgl.yaml.
LABELS_HORLOGE = tuple(f"lbl_time_{d}_{ab}" for d in ("h10", "h1", "m10", "m1") for ab in ("a", "b")) \
    + ("lbl_time_colon",)


class ErreurTheme(Exception):
    pass


def roles(tokens: Path = TOKENS) -> list[str]:
    """Rôles de `struct Palette`, dans l'ordre de la structure."""
    texte = tokens.read_text(encoding="utf-8")
    m = re.search(r"struct Palette \{(.*?)\n\};", texte, re.S)
    if not m:
        raise ErreurTheme("struct Palette introuvable dans tab5_tokens.h")
    return re.findall(r"^\s*uint32_t\s+([A-Z0-9_]+);", m.group(1), re.M)


def melange(c1: int, c2: int, opa: int) -> int:
    """lv_color_mix(c1, c2, opa) de LVGL 9 : canal par canal, arrondi, division par 255."""
    out = 0
    for decalage in (16, 8, 0):
        a, b = (c1 >> decalage) & 0xFF, (c2 >> decalage) & 0xFF
        v = ((a * opa + b * (255 - opa) + 128) * 0x8081) >> 23
        out |= v << decalage
    return out


def opacite(valeur, ou: str) -> int:
    """« 35% » comme ESPHome (cv.percentage puis × 255.0, tronqué par static_cast<uint8_t>),
    ou un entier 0..255."""
    if isinstance(valeur, str) and valeur.strip().endswith("%"):
        pct = float(valeur.strip()[:-1].rstrip()) / 100.0
        if not 0 <= pct <= 1:
            raise ErreurTheme(f"{ou} : opacité hors de 0..100 %")
        return int(pct * 255.0)
    if isinstance(valeur, bool) or not isinstance(valeur, int) or not 0 <= valeur <= 255:
        raise ErreurTheme(f"{ou} : opacité « NN% » ou 0..255 attendue, lu {valeur!r}")
    return valeur


@dataclass
class Theme:
    fichier: str
    nom: str
    ordre: int
    modes: dict[str, dict[str, int]]
    zones: tuple[str, ...] = ()
    # mode → style → propriété → (genre, valeur), formes du fichier seulement (sans SUIT).
    formes: dict[str, dict[str, dict[str, tuple]]] = field(default_factory=dict)
    polices: dict[str, str] = field(default_factory=dict)


def _lire(chemin: Path) -> dict:
    with open(chemin, encoding="utf-8") as f:
        donnees = yaml.safe_load(f)
    if not isinstance(donnees, dict):
        raise ErreurTheme(f"{chemin.name} : un mapping YAML est attendu")
    return donnees


def _valeur_forme(prop: str, valeur, connus: set[str], ou: str) -> tuple:
    genre = PROPS_FORME[prop]
    if genre == "couleur":
        if isinstance(valeur, str):
            if valeur not in connus:
                raise ErreurTheme(f"{ou} : rôle inconnu `{valeur}` (struct Palette)")
            return ("role", valeur)
        if isinstance(valeur, bool) or not isinstance(valeur, int) or not 0 <= valeur <= 0xFFFFFF:
            raise ErreurTheme(f"{ou} : couleur 0xRRGGBB ou nom de rôle attendu, lu {valeur!r}")
        return ("couleur", valeur)
    if genre == "opacite":
        return ("nombre", opacite(valeur, ou))
    if genre == "cotes":
        noms = [valeur] if isinstance(valeur, str) else valeur
        if not isinstance(noms, list) or not noms or any(not isinstance(n, str) or n.upper() not in COTES
                                                         for n in noms):
            raise ErreurTheme(f"{ou} : côtés parmi {sorted(COTES)} attendus, lu {valeur!r}")
        masque = 0
        for n in noms:
            masque |= COTES[n.upper()]
        return ("cotes", masque)
    if genre == "degrade":
        if not isinstance(valeur, str) or valeur.upper() not in DEGRADES:
            raise ErreurTheme(f"{ou} : direction parmi {DEGRADES} attendue, lu {valeur!r}")
        return ("degrade", valeur.upper())
    if isinstance(valeur, bool) or not isinstance(valeur, int):
        raise ErreurTheme(f"{ou} : entier attendu, lu {valeur!r}")
    bornes = {"nombre": (0, 999), "arret": (0, 255), "decalage": (-100, 100)}[genre]
    if not bornes[0] <= valeur <= bornes[1]:
        raise ErreurTheme(f"{ou} : valeur hors de {bornes[0]}..{bornes[1]}, lu {valeur}")
    return ("nombre", valeur)


def _formes_de(stem: str, brut, connus: set[str]) -> dict[str, dict[str, dict[str, tuple]]]:
    out = {m: {} for m in MODES}
    if brut is None:
        return out
    if not isinstance(brut, dict):
        raise ErreurTheme(f"{stem}.yaml : `formes` doit être un mapping style → propriétés")
    for style, props in brut.items():
        if style not in STYLES_FORMES:
            raise ErreurTheme(f"{stem}.yaml, formes : style `{style}` non prévu (STYLES_FORMES de gen_themes.py)")
        if not isinstance(props, dict):
            raise ErreurTheme(f"{stem}.yaml, formes.{style} : mapping attendu")
        communs = {k: v for k, v in props.items() if k not in MODES}
        for mode in MODES:
            propres = props.get(mode) or {}
            if not isinstance(propres, dict):
                raise ErreurTheme(f"{stem}.yaml, formes.{style}.{mode} : mapping attendu")
            fusion = {**communs, **propres}
            for prop, valeur in fusion.items():
                if prop not in PROPS_FORME:
                    raise ErreurTheme(f"{stem}.yaml, formes.{style} : propriété `{prop}` non prévue "
                                      f"({', '.join(PROPS_FORME)})")
                out[mode].setdefault(style, {})[prop] = _valeur_forme(
                    prop, valeur, connus, f"{stem}.yaml, formes.{style}.{prop} ({mode})")
    return out


def charger(dossier: Path = THEMES_DIR, tokens: Path = TOKENS) -> list[Theme]:
    """Les thèmes, validés et complets (verre calculé), triés par `ordre`. Les fichiers
    dont le nom commence par « _ » ne sont pas des thèmes (_polices.yaml)."""
    liste_roles = roles(tokens)
    connus = set(liste_roles)
    bruts = {p.stem: _lire(p) for p in sorted(dossier.glob("*.yaml")) if not p.name.startswith("_")}
    if not bruts:
        raise ErreurTheme(f"aucun thème dans {dossier}")

    def modes_de(stem: str, pile: tuple[str, ...] = ()) -> dict[str, dict[str, int]]:
        if stem in pile:
            raise ErreurTheme(f"{stem}.yaml : héritage circulaire ({' -> '.join(pile + (stem,))})")
        if stem not in bruts:
            raise ErreurTheme(f"{pile[-1]}.yaml : `herite: {stem}` — thème introuvable")
        brut = bruts[stem]
        base = modes_de(brut["herite"], pile + (stem,)) if brut.get("herite") else {m: {} for m in MODES}
        modes = {}
        for mode in MODES:
            propres = brut.get(mode) or {}
            if not isinstance(propres, dict):
                raise ErreurTheme(f"{stem}.yaml : `{mode}` doit être un mapping rôle → couleur")
            for role, valeur in propres.items():
                if role not in connus:
                    raise ErreurTheme(f"{stem}.yaml, {mode} : rôle inconnu `{role}` (struct Palette)")
                if isinstance(valeur, bool) or not isinstance(valeur, int) or not 0 <= valeur <= 0xFFFFFF:
                    raise ErreurTheme(f"{stem}.yaml, {mode}.{role} : couleur 0xRRGGBB attendue, lu {valeur!r}")
            modes[mode] = {**base[mode], **propres}
        return modes

    themes = []
    for stem, brut in bruts.items():
        inconnues = set(brut) - {"nom", "ordre", "herite", "zones_sombres", "formes", "polices", *MODES}
        if inconnues:
            raise ErreurTheme(f"{stem}.yaml : clé(s) inconnue(s) {sorted(inconnues)}")
        nom, ordre = brut.get("nom"), brut.get("ordre")
        if not isinstance(nom, str) or not RE_NOM.match(nom):
            raise ErreurTheme(f"{stem}.yaml : `nom` (1 à 24 caractères, sans guillemet) attendu")
        if isinstance(ordre, bool) or not isinstance(ordre, int) or ordre < 1:
            raise ErreurTheme(f"{stem}.yaml : `ordre` (entier à partir de 1) attendu")
        modes = modes_de(stem)
        for mode in MODES:
            valeurs = modes[mode]
            for role, (verre, fond, opa) in DERIVES.items():
                if role not in valeurs and verre in valeurs and fond in valeurs:
                    valeurs[role] = melange(valeurs[verre], valeurs[fond], opa)
            manquants = [r for r in liste_roles if r not in valeurs]
            if manquants:
                raise ErreurTheme(f"{stem}.yaml, {mode} : rôle(s) manquant(s) {manquants}")
            modes[mode] = {r: valeurs[r] for r in liste_roles}
        zones = brut.get("zones_sombres") or []
        if not isinstance(zones, list) or any(z not in ZONES for z in zones) or len(set(zones)) != len(zones):
            raise ErreurTheme(f"{stem}.yaml : `zones_sombres` = liste parmi {sorted(ZONES)}, lu {zones!r}")
        polices = brut.get("polices") or {}
        if not isinstance(polices, dict) or any(r not in ROLES_POLICE for r in polices) \
                or any(not isinstance(v, str) or not RE_POLICE.match(v) for v in polices.values()):
            raise ErreurTheme(f"{stem}.yaml : `polices` = {{horloge|date|titre: \"Famille@graisse\"}}, "
                              f"lu {polices!r}")
        themes.append(Theme(stem, nom, ordre, modes, tuple(z for z in ZONES if z in zones),
                            _formes_de(stem, brut.get("formes"), connus), dict(polices)))

    themes.sort(key=lambda t: t.ordre)
    ordres = [t.ordre for t in themes]
    if ordres != list(range(1, len(themes) + 1)):
        raise ErreurTheme(f"`ordre` doit aller de 1 à {len(themes)} sans trou ni doublon, lu {ordres}")
    noms = [t.nom for t in themes]
    if len(set(noms)) != len(noms):
        raise ErreurTheme(f"noms de thème en double : {noms}")
    premier = themes[0]
    if premier.zones or any(premier.formes[m] for m in MODES) or premier.polices:
        raise ErreurTheme(f"{premier.fichier}.yaml : le premier thème est l'état compilé de "
                          "tab5-styles.yaml (ni zones_sombres, ni formes, ni polices)")
    return themes


def rendre_cpp(themes: list[Theme]) -> list[str]:
    largeur = max(len(r) for r in themes[0].modes["sombre"])
    lignes = ["inline constexpr Theme THEMES[] = {"]
    for t in themes:
        drapeaux = ", ".join("true" if z in t.zones else "false" for z in ZONES)
        lignes.append(f'    {{"{t.nom}", {drapeaux},  // Tab5/themes/{t.fichier}.yaml')
        for mode in MODES:
            lignes.append(f"     {{  // {mode}")
            for role, valeur in t.modes[mode].items():
                lignes.append(f"      .{role:<{largeur}} = 0x{valeur:06X},")
            lignes.append("     }," if mode == "sombre" else "     }},")
    lignes.append("};")
    lignes.append("inline constexpr int THEME_COUNT = static_cast<int>(sizeof(THEMES) / sizeof(THEMES[0]));")
    return lignes


def _styles_yaml(styles: Path = STYLES) -> dict:
    class Chargeur(yaml.SafeLoader):
        pass

    def tag(loader, _suffixe, noeud):
        if isinstance(noeud, yaml.ScalarNode):
            return {"__tag__": noeud.tag, "valeur": loader.construct_scalar(noeud)}
        return None

    Chargeur.add_multi_constructor("!", tag)
    with open(styles, encoding="utf-8") as f:
        return yaml.load(f, Loader=Chargeur)


def _palette_role(valeur, ou: str) -> tuple[str, str]:
    if isinstance(valeur, dict) and valeur.get("__tag__") == "!lambda":
        m = RE_LAMBDA.match(str(valeur["valeur"]).strip())
        if m:
            return m.group(1), m.group(2)
    raise ErreurTheme(f"{ou} : la couleur doit être `!lambda 'return lv_color_hex(UIColor.X);'` "
                      f"(ou UIBandeau / UIHorloge) pour suivre le thème, lu {valeur!r}")


def _role_de(valeur, ou: str) -> str:
    return _palette_role(valeur, ou)[1]


def rendre_styles(styles: Path = STYLES, connus: set[str] | None = None) -> list[str]:
    """Actions ESPHome qui reposent chaque couleur des styles partagés et du thème."""
    lvgl = _styles_yaml(styles)["lvgl"]
    appels = []
    for style in lvgl.get("style_definitions", []):
        poses = 0
        for prop in PROPS_COULEUR:
            if prop in style:
                palette, role = _palette_role(style[prop], f"style {style['id']}.{prop}")
                if connus is not None and role not in connus:
                    raise ErreurTheme(f"style {style['id']}.{prop} : rôle inconnu {palette}.{role}")
                appels.append(f"lv_style_set_{prop}(id({style['id']}), lv_color_hex({palette}.{role}));")
                poses += 1
        if poses:
            # Comme l'action lvgl.style.update : seuls les objets de ce style sont rafraîchis.
            appels.append(f"lv_obj_report_style_change(id({style['id']}));")
    lignes = ["- lambda: |-"] + [f"    {a}" for a in appels]
    theme = []
    for widget, props in (lvgl.get("theme") or {}).items():
        couleurs = [(p, _palette_role(props[p], f"theme.{widget}.{p}")) for p in PROPS_COULEUR if p in props]
        if couleurs:
            theme.append(f"    {widget}:")
            theme += [f"      {p}: !lambda 'return lv_color_hex({pal}.{r});'" for p, (pal, r) in couleurs]
    if theme:
        lignes += ["- lvgl.theme.update:"] + theme
    return lignes


# --- Formes ------------------------------------------------------------------

def styles_compiles(styles: Path = STYLES) -> dict[str, dict[str, tuple]]:
    """Les propriétés de forme de chaque style de STYLES_FORMES dans tab5-styles.yaml,
    au format des formes : (genre, valeur) ; une couleur = ("role", X), sa palette à part."""
    defs = {s["id"]: s for s in _styles_yaml(styles)["lvgl"].get("style_definitions", [])}
    out = {}
    for sid in STYLES_FORMES:
        if sid not in defs:
            raise ErreurTheme(f"tab5-styles.yaml : style `{sid}` (STYLES_FORMES) introuvable")
        props = {}
        for prop in PROPS_FORME:
            if prop not in defs[sid]:
                continue
            ou = f"tab5-styles.yaml, {sid}.{prop}"
            if PROPS_FORME[prop] == "couleur":
                props[prop] = ("role", _role_de(defs[sid][prop], ou))
            else:
                props[prop] = _valeur_forme(prop, defs[sid][prop], set(), ou)
        out[sid] = props
    return out


def palette_des_styles(styles: Path = STYLES) -> dict[str, str]:
    """Palette que lisent les couleurs de chaque style de STYLES_FORMES (une seule)."""
    defs = {s["id"]: s for s in _styles_yaml(styles)["lvgl"].get("style_definitions", [])}
    out = {}
    for sid in STYLES_FORMES:
        palettes = {_palette_role(defs[sid][p], f"{sid}.{p}")[0] for p in PROPS_COULEUR if p in defs[sid]}
        if len(palettes) > 1:
            raise ErreurTheme(f"tab5-styles.yaml, {sid} : couleurs lues dans plusieurs palettes {sorted(palettes)}")
        out[sid] = palettes.pop() if palettes else "UIColor"
    return out


def formes_resolues(theme: Theme, compiles: dict[str, dict[str, tuple]]) -> dict[str, dict[str, dict[str, tuple]]]:
    """mode → style → propriétés à poser par-dessus l'état compilé : les formes du fichier,
    plus celles qu'un style de SUIT reprend de son parent (sauf ce que tab5-styles.yaml lui
    donne de différent), moins ce qui égale déjà l'état compilé."""
    out = {}
    for mode in MODES:
        directes = theme.formes.get(mode, {})
        effectives: dict[str, dict[str, tuple]] = {}
        for sid in STYLES_FORMES:  # les parents viennent avant leurs enfants
            base = {}
            parent = SUIT.get(sid)
            if parent:
                distincts = {p for p in set(compiles[sid]) | set(compiles[parent])
                             if compiles[sid].get(p) != compiles[parent].get(p)}
                base = {p: v for p, v in effectives.get(parent, {}).items() if p not in distincts}
            fusion = {**base, **directes.get(sid, {})}
            if fusion:
                effectives[sid] = fusion
        out[mode] = {sid: {p: v for p, v in props.items() if compiles[sid].get(p) != v}
                     for sid, props in effectives.items()}
        out[mode] = {sid: props for sid, props in out[mode].items() if props}
    return out


def _cpp_valeur(genre: str, valeur, roles_idx: dict[str, int]) -> tuple[str, str]:
    if genre == "role":
        return "FORME_ROLE", f"{roles_idx[valeur]}"
    if genre == "couleur":
        return "FORME_COULEUR", f"0x{valeur:06X}"
    if genre == "cotes":
        noms = [n for n in ("BOTTOM", "TOP", "LEFT", "RIGHT") if valeur & COTES[n]]
        expr = "LV_BORDER_SIDE_FULL" if valeur == 15 else (
            "LV_BORDER_SIDE_NONE" if not noms else " | ".join(f"LV_BORDER_SIDE_{n}" for n in noms))
        return "FORME_NOMBRE", expr
    if genre == "degrade":
        return "FORME_NOMBRE", f"LV_GRAD_DIR_{valeur}"
    return "FORME_NOMBRE", str(valeur)


def rendre_formes(themes: list[Theme], styles: Path = STYLES) -> tuple[list[str], list[str]]:
    """(tables C++ de tab5_theme.cpp, lambda YAML qui passe les styles) des formes."""
    compiles = styles_compiles(styles)
    palettes = palette_des_styles(styles)
    resolues = [formes_resolues(t, compiles) for t in themes]
    touches: dict[str, set[str]] = {}
    for r in resolues:
        for mode in MODES:
            for sid, props in r[mode].items():
                touches.setdefault(sid, set()).update(props)
    utilises = [s for s in STYLES_FORMES if s in touches]
    idx_style = {s: i for i, s in enumerate(utilises)}
    roles_utilises = sorted({v[1] for sid in utilises for p in touches[sid]
                             for v in [compiles[sid].get(p)] if v and v[0] == "role"}
                            | {v[1] for r in resolues for m in MODES for props in r[m].values()
                               for v in props.values() if v[0] == "role"})
    roles_idx = {r: i for i, r in enumerate(roles_utilises)}

    def ligne(sid: str, prop: str, valeur) -> str:
        lv = LV_PROP.get(prop, f"LV_STYLE_{prop.upper()}")
        if valeur is None:
            return f"    {{{idx_style[sid]}, {lv}, FORME_RETIRE, 0}},  // {sid}"
        genre, v = valeur
        type_, expr = _cpp_valeur(genre, v, roles_idx)
        return f"    {{{idx_style[sid]}, {lv}, {type_}, {expr}}},  // {sid}"

    c = ["// Styles qu'un thème redessine, dans l'ordre du tableau que passe tab5_theme_repeindre",
         "// (tab5-themes.yaml), et la palette où leurs rôles se lisent.",
         f"static constexpr int kStylesFormes = {len(utilises)};"]
    c.append("static const Palette* const kPaletteStyle[] = {"
             + ", ".join(f"&{palettes[s]}" for s in utilises) + ("" if utilises else "nullptr") + "};")
    c.append("static constexpr uint32_t Palette::* kRolesFormes[] = {"
             + ", ".join(f"&Palette::{r}" for r in roles_utilises) + ("" if roles_utilises else "nullptr") + "};")
    defaut = [ligne(sid, p, compiles[sid].get(p)) for sid in utilises for p in PROPS_FORME if p in touches[sid]]
    c.append("// État compilé (tab5-styles.yaml) de chaque propriété qu'un thème change, reposé avant")
    c.append("// les formes du thème choisi.")
    c.append(f"static constexpr int kNbFormesDefaut = {len(defaut)};")
    c.append("static constexpr Forme kFormesDefaut[] = {")
    c += defaut + ["    {0, 0, FORME_RETIRE, 0},  // (fin)", "};"]
    c.append("// Formes de chaque thème, mode sombre puis clair : kFormes[kFormesDebut[2 t + clair] ..")
    c.append("// kFormesDebut[2 t + clair + 1][.")
    corps, debuts = [], [0]
    for t, r in zip(themes, resolues):
        for mode in MODES:
            for sid in utilises:
                for p in PROPS_FORME:
                    if p in r[mode].get(sid, {}):
                        corps.append(ligne(sid, p, r[mode][sid][p]) + f" {t.fichier} ({mode})")
            debuts.append(len(corps))
    c.append(f"static constexpr uint16_t kFormesDebut[] = {{{', '.join(map(str, debuts))}}};")
    c.append("static constexpr Forme kFormes[] = {")
    c += corps + ["    {0, 0, FORME_RETIRE, 0},  // (fin)", "};"]
    yml = []
    if utilises:
        yml = ["- lambda: |-",
               "    // Formes du thème (`formes:` de Tab5/themes/<thème>.yaml), par-dessus ses couleurs.",
               "    lv_style_t* const styles[] = {" + ", ".join(f"id({s})" for s in utilises) + "};",
               f"    theme_formes(styles, {len(utilises)});"]
    return c, yml


# --- Polices -----------------------------------------------------------------

def _police_theme():
    import importlib.util
    spec = importlib.util.spec_from_file_location("police_theme", REPO / "tools" / "police_theme.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def lire_polices(chemin: Path = POLICES) -> dict:
    if not chemin.exists():
        raise ErreurTheme(f"{chemin.name} introuvable : lancer `python tools/police_theme.py`")
    with open(chemin, encoding="utf-8") as f:
        return yaml.safe_load(f) or {}


def _slug(texte: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", texte.lower()).strip("_")


def rendre_polices(themes: list[Theme], mesures: dict | None = None, jeux: dict[str, str] | None = None
                   ) -> tuple[list[str], list[str], list[str]]:
    """(bloc `font:` YAML, lambda YAML de theme_polices, table C++ kPolices)."""
    cites = sorted({v for t in themes for v in t.polices.values()})
    mesures = lire_polices() if mesures is None else mesures
    if cites:
        if jeux is None:
            pt = _police_theme()
            jeux = pt.jeux_par_role()
            hors = sorted(set("".join(jeux.values())) - set(pt.UNIVERS))
            if hors:
                raise ErreurTheme(f"caractères d'affichage hors du jeu mesuré (UNIVERS, tools/police_theme.py) : "
                                  f"{''.join(hors)!r}")
    # Polices compilées : (famille@graisse, taille) → (id, glyphes) ; les 3 Roboto d'abord.
    ids: list[str] = [ROBOTO[r] for r in ROLES_POLICE]
    fontes: dict[tuple[str, int], dict] = {}
    table = []
    for t in themes:
        rangee = {}
        geo = {"y": None}
        for role in ROLES_POLICE:
            cle = t.polices.get(role)
            if cle is None:
                rangee[role] = ROLES_POLICE.index(role)
                continue
            if cle not in mesures:
                raise ErreurTheme(f"{t.fichier}.yaml : police `{cle}` absente de _polices.yaml "
                                  "(lancer `python tools/police_theme.py`)")
            m = mesures[cle]
            if role not in m:
                raise ErreurTheme(f"{t.fichier}.yaml : `{cle}` ne tient pas dans le rôle {role} (_polices.yaml)")
            taille = m[role]["taille"]
            famille, graisse = cle.rsplit("@", 1)
            if cle == REFERENCE_POLICE and taille == {"horloge": 130, "date": 45, "titre": 32}[role]:
                rangee[role] = ROLES_POLICE.index(role)
            else:
                f = fontes.setdefault((cle, taille), {"id": f"police_{_slug(famille)}_{graisse}_{taille}",
                                                     "famille": famille, "graisse": int(graisse),
                                                     "taille": taille, "glyphes": set()})
                f["glyphes"] |= set(jeux[role]) - set(m.get("manquants", ""))
                if f["id"] not in ids:
                    ids.append(f["id"])
                rangee[role] = ids.index(f["id"])
            if role == "horloge":
                geo = m["horloge"]
        if geo["y"] is None:
            if "horloge" not in mesures.get(REFERENCE_POLICE, {}):
                raise ErreurTheme(f"{REFERENCE_POLICE} absente de _polices.yaml (lancer `python tools/police_theme.py`)")
            geo = mesures[REFERENCE_POLICE]["horloge"]
        table.append((t, rangee, geo))
    font_yaml = []
    if fontes:
        font_yaml = ["font:"]
        for f in sorted(fontes.values(), key=lambda x: x["id"]):
            glyphes = "".join(sorted(f["glyphes"])).replace("'", "''")
            font_yaml += [f"  - file: gfonts://{f['famille']}@{f['graisse']}",
                          f"    id: {f['id']}",
                          f"    size: {f['taille']}",
                          "    bpp: 2",
                          f"    glyphs: '{glyphes}'"]
    lambda_yaml = [
        "- lambda: |-",
        "    // Polices d'affichage du thème (`polices:`), géométrie de l'horloge comprise.",
        "    esphome::font::Font* const polices[] = {" + ", ".join(f"id({i})" for i in ids) + "};",
        "    lv_obj_t* const horloge[] = {" + ", ".join(f"id({i})" for i in LABELS_HORLOGE) + "};",
        "    theme_polices(id(" + "), id(".join(STYLES_POLICE[r] for r in ROLES_POLICE) + f"), polices, {len(ids)}, horloge);",
    ]
    cpp = ["// Polices de chaque thème : index dans le tableau `polices` que passe",
           "// tab5_theme_repeindre (0-2 = les Roboto compilées), puis la géométrie de l'horloge",
           "// (y des labels des rouleaux, position du « : »), tools/police_theme.py.",
           f"static constexpr int kNbPolices = {len(ids)};",
           "static constexpr PolicesTheme kPolices[] = {"]
    for t, rangee, geo in table:
        cpp.append(f"    {{{rangee['horloge']}, {rangee['date']}, {rangee['titre']}, {geo['y']}, "
                   f"{geo['x_deux_points']}, {geo['y_deux_points']}}},  // {t.fichier}")
    cpp.append("};")
    return font_yaml, lambda_yaml, cpp


# --- Écriture ----------------------------------------------------------------

def _remplacer(texte: str, marques: tuple[str, str], contenu: list[str], ou: str) -> str:
    """Remplace les lignes entre les deux marques (lignes de la marque comprises gardées),
    à l'indentation de la marque ouvrante."""
    lignes = texte.split("\n")
    debut = [i for i, l in enumerate(lignes) if l.strip().startswith(marques[0])]
    fin = [i for i, l in enumerate(lignes) if l.strip().startswith(marques[1])]
    if len(debut) != 1 or len(fin) != 1 or fin[0] < debut[0]:
        raise ErreurTheme(f"{ou} : marques « {marques[0]} » / « {marques[1]} » introuvables ou en double")
    indent = lignes[debut[0]][: len(lignes[debut[0]]) - len(lignes[debut[0]].lstrip())]
    return "\n".join(lignes[: debut[0] + 1] + [indent + l if l else l for l in contenu] + lignes[fin[0]:])


def _lire_texte(chemin: Path) -> tuple[str, str]:
    brut = chemin.read_bytes().decode("utf-8")
    return brut.replace("\r\n", "\n"), ("\r\n" if "\r\n" in brut else "\n")


def cibles(themes: list[Theme]) -> list[tuple[Path, str, str]]:
    """(fichier, texte actuel normalisé, texte attendu normalisé) pour chaque cible."""
    formes_cpp, formes_yaml = rendre_formes(themes)
    font_yaml, polices_yaml, polices_cpp = rendre_polices(themes)
    tokens, _ = _lire_texte(TOKENS)
    attendu_tokens = _remplacer(tokens, MARQUES_CPP, rendre_cpp(themes), "tab5_tokens.h")
    firmware, _ = _lire_texte(FIRMWARE)
    attendu_fw = _remplacer(firmware, MARQUES_OPTIONS, [f'- "{t.nom}"' for t in themes], "tab5-themes.yaml")
    attendu_fw = _remplacer(attendu_fw, MARQUES_STYLES,
                            rendre_styles(STYLES, set(roles())) + formes_yaml + polices_yaml, "tab5-themes.yaml")
    attendu_fw = _remplacer(attendu_fw, MARQUES_POLICES, font_yaml, "tab5-themes.yaml")
    cpp, _ = _lire_texte(THEME_CPP)
    attendu_cpp = _remplacer(cpp, MARQUES_FORMES, formes_cpp + polices_cpp, "tab5_theme.cpp")
    return [(TOKENS, tokens, attendu_tokens), (FIRMWARE, firmware, attendu_fw), (THEME_CPP, cpp, attendu_cpp)]


def main(argv: list[str]) -> int:
    verifier = "--check" in argv
    try:
        themes = charger()
        resultats = cibles(themes)
    except ErreurTheme as e:
        print(f"[KO] thèmes : {e}")
        return 1
    perimes = []
    for chemin, actuel, attendu in resultats:
        if actuel == attendu:
            continue
        perimes.append(chemin)
        if not verifier:
            _, fin = _lire_texte(chemin)
            chemin.write_bytes(attendu.replace("\n", fin).encode("utf-8"))
    noms = ", ".join(f"{t.nom} ({t.fichier}.yaml)" for t in themes)
    if verifier and perimes:
        print("[KO] thèmes : partie(s) générée(s) périmée(s) dans "
              + ", ".join(p.name for p in perimes) + " — lancer `python tools/gen_themes.py`")
        return 1
    etat = "à jour" if not perimes else "réécrit : " + ", ".join(p.name for p in perimes)
    print(f"[OK] thèmes ({len(themes)}) : {noms} — {etat}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
