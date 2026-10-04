#!/usr/bin/env python3
"""tools/gen_themes.py — catalogue des thèmes de l'écran (ADR-0029, lot 2).

[AI-CONTEXT] Source unique : Tab5/themes/<thème>.yaml (un fichier par thème : `nom`,
`ordre`, un mode `sombre` et un mode `clair`, chacun avec tous les rôles de
`struct Palette`), plus les rôles eux-mêmes (`struct Palette`, Tab5/tab5_tokens.h) et
les styles partagés (Tab5/tab5-styles.yaml). Écrit :

  (a) Tab5/tab5_tokens.h — THEMES[] (nom, palette sombre, palette claire) et
      THEME_COUNT, entre `// >>> themes` et `// <<< themes` ;
  (b) Tab5/tab5-themes.yaml — les options du select « Thème », entre `# >>> themes` et
      `# <<< themes`, dans l'ordre des `ordre:` (la tablette garde l'INDEX : un thème
      s'ajoute à la fin) ;
  (c) Tab5/tab5-themes.yaml — la repeinture des styles partagés après un changement
      de palette, entre `# >>> styles` et `# <<< styles` : chaque couleur de
      `style_definitions:` et du `theme:` (une lambda qui lit `UIColor.X`) y est
      reposée depuis la palette active, puis les objets de ce style sont rafraîchis
      (lv_obj_report_style_change, comme l'action lvgl.style.update).

    python tools/gen_themes.py          # réécrit les parties générées
    python tools/gen_themes.py --check  # exit 1 si l'une est périmée (n'écrit rien)

Un mode peut omettre le verre pré-mélangé (GLASS_HI_PAGE / GLASS_LO_PAGE : GLASS_HI /
GLASS_LO à 58 % sur BG ; GLASS_HI_MODAL / GLASS_LO_MODAL : à 88 % sur MODAL_SCRIM) :
il est calculé comme lv_color_mix(). `herite: <fichier>` reprend les deux modes d'un
autre thème avant d'appliquer ceux du fichier.

Fins de ligne : chaque fichier est réécrit avec les siennes ; `--check` compare en
normalisant. Rien d'autre que les parties générées ne change.
"""
from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
THEMES_DIR = REPO / "Tab5" / "themes"
TOKENS = REPO / "Tab5" / "tab5_tokens.h"
STYLES = REPO / "Tab5" / "tab5-styles.yaml"
FIRMWARE = REPO / "Tab5" / "tab5-themes.yaml"

MARQUES_CPP = ("// >>> themes", "// <<< themes")
MARQUES_OPTIONS = ("# >>> themes", "# <<< themes")
MARQUES_STYLES = ("# >>> styles", "# <<< styles")

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
RE_LAMBDA = re.compile(r"^return lv_color_hex\(UIColor\.([A-Z0-9_]+)\);$")
RE_NOM = re.compile(r"^[^\"\\\n]{1,24}$")


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


@dataclass
class Theme:
    fichier: str
    nom: str
    ordre: int
    modes: dict[str, dict[str, int]]


def _lire(chemin: Path) -> dict:
    with open(chemin, encoding="utf-8") as f:
        donnees = yaml.safe_load(f)
    if not isinstance(donnees, dict):
        raise ErreurTheme(f"{chemin.name} : un mapping YAML est attendu")
    return donnees


def charger(dossier: Path = THEMES_DIR, tokens: Path = TOKENS) -> list[Theme]:
    """Les thèmes, validés et complets (verre calculé), triés par `ordre`."""
    liste_roles = roles(tokens)
    connus = set(liste_roles)
    bruts = {p.stem: _lire(p) for p in sorted(dossier.glob("*.yaml"))}
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
        inconnues = set(brut) - {"nom", "ordre", "herite", *MODES}
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
        themes.append(Theme(stem, nom, ordre, modes))

    themes.sort(key=lambda t: t.ordre)
    ordres = [t.ordre for t in themes]
    if ordres != list(range(1, len(themes) + 1)):
        raise ErreurTheme(f"`ordre` doit aller de 1 à {len(themes)} sans trou ni doublon, lu {ordres}")
    noms = [t.nom for t in themes]
    if len(set(noms)) != len(noms):
        raise ErreurTheme(f"noms de thème en double : {noms}")
    return themes


def rendre_cpp(themes: list[Theme]) -> list[str]:
    largeur = max(len(r) for r in themes[0].modes["sombre"])
    lignes = ["inline constexpr Theme THEMES[] = {"]
    for t in themes:
        lignes.append(f'    {{"{t.nom}",  // Tab5/themes/{t.fichier}.yaml')
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


def _role_de(valeur, ou: str) -> str:
    if isinstance(valeur, dict) and valeur.get("__tag__") == "!lambda":
        m = RE_LAMBDA.match(str(valeur["valeur"]).strip())
        if m:
            return m.group(1)
    raise ErreurTheme(f"{ou} : la couleur doit être `!lambda 'return lv_color_hex(UIColor.X);'` "
                      f"pour suivre le thème, lu {valeur!r}")


def rendre_styles(styles: Path = STYLES, connus: set[str] | None = None) -> list[str]:
    """Actions ESPHome qui reposent chaque couleur des styles partagés et du thème."""
    lvgl = _styles_yaml(styles)["lvgl"]
    appels = []
    for style in lvgl.get("style_definitions", []):
        poses = 0
        for prop in PROPS_COULEUR:
            if prop in style:
                role = _role_de(style[prop], f"style {style['id']}.{prop}")
                if connus is not None and role not in connus:
                    raise ErreurTheme(f"style {style['id']}.{prop} : rôle inconnu UIColor.{role}")
                appels.append(f"lv_style_set_{prop}(id({style['id']}), lv_color_hex(UIColor.{role}));")
                poses += 1
        if poses:
            # Comme l'action lvgl.style.update : seuls les objets de ce style sont rafraîchis.
            appels.append(f"lv_obj_report_style_change(id({style['id']}));")
    lignes = ["- lambda: |-"] + [f"    {a}" for a in appels]
    theme = []
    for widget, props in (lvgl.get("theme") or {}).items():
        couleurs = [(p, _role_de(props[p], f"theme.{widget}.{p}")) for p in PROPS_COULEUR if p in props]
        if couleurs:
            theme.append(f"    {widget}:")
            theme += [f"      {p}: !lambda 'return lv_color_hex(UIColor.{r});'" for p, r in couleurs]
    if theme:
        lignes += ["- lvgl.theme.update:"] + theme
    return lignes


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
    tokens, _ = _lire_texte(TOKENS)
    attendu_tokens = _remplacer(tokens, MARQUES_CPP, rendre_cpp(themes), "tab5_tokens.h")
    firmware, _ = _lire_texte(FIRMWARE)
    attendu_fw = _remplacer(firmware, MARQUES_OPTIONS, [f'- "{t.nom}"' for t in themes], "tab5-themes.yaml")
    attendu_fw = _remplacer(attendu_fw, MARQUES_STYLES, rendre_styles(STYLES, set(roles())), "tab5-themes.yaml")
    return [(TOKENS, tokens, attendu_tokens), (FIRMWARE, firmware, attendu_fw)]


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
