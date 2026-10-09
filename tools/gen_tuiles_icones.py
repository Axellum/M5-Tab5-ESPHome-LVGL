#!/usr/bin/env python3
"""tools/gen_tuiles_icones.py — palette des icônes des tuiles de pièce (ADR-0023).

[AI-CONTEXT] Source unique : Tab5/tuiles_icones.yaml (code de palette, glyphe éteint /
allumé, noms `mdi:` représentés, défauts par type de tuile et par domaine HA). Écrit :

  (a) Tab5/socle/tab5_tuiles_icones.h — la table C++ et `tuile_icone(code, actif, type)` ;
  (b) Tab5/paquets/tab5-styles.yaml — les glyphes de la palette dans mdi_font_70, mdi_font_45 et
      mdi_font_32, entre `# >>> tuiles` et `# <<< tuiles` (sans ceux que la police liste
      déjà à la main au-dessus : ESPHome refuse un glyphe en double) ;
  (c) le blueprint tab5_emplacements.yaml — `icones_mdi` (« mdi:nom » → code) et
      `icones_defaut` (« domaine » ou « domaine.classe » → code), entre ses marqueurs ;
  (d) docs/tiles_icons.md — le tableau de la palette, en anglais et en français.

    python tools/gen_tuiles_icones.py                  # réécrit les parties générées
    python tools/gen_tuiles_icones.py --check          # exit 1 si l'une est périmée (n'écrit rien)
    python tools/gen_tuiles_icones.py --meta meta.json # vérifie noms ↔ points de code contre
                                                       # le meta.json de @mdi/svg (même version
                                                       # que le TTF, 7.4.47), n'écrit rien

Fins de ligne : chaque fichier est réécrit avec les siennes (CRLF dans un checkout
Windows, LF sous Linux) ; `--check` compare en normalisant, le résultat est le même
partout. Rien d'autre que les parties générées ne change.
Règle 7 (tools/check_tab5_code_rules.py) : la table est rattachée aux widgets qui
l'affichent par MDI_CODE_TARGETS[("tab5_tuiles_icones.h", "")] — d'où les trois polices.
"""
from __future__ import annotations

import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
SOURCE = REPO / "Tab5" / "tuiles_icones.yaml"
ENTETE = REPO / "Tab5" / "socle" / "tab5_tuiles_icones.h"
STYLES = REPO / "Tab5" / "paquets" / "tab5-styles.yaml"
BLUEPRINT = REPO / "HomeAssistant_Config" / "blueprints" / "automation" / "tab5" / "tab5_emplacements.yaml"
DOC = REPO / "docs" / "tiles_icons.md"
TTF = REPO / "Tab5" / "fonts" / "materialdesignicons-webfont.ttf"

# Polices des widgets qui affichent la palette : cartes du mode HA (icon_sw*, 70 px),
# lignes des popups Lumières et Volets (icon_light_sel_*, volet_ligne_*_icone, 45 px), épaules des tuiles (icon_card_*, 32 px).
POLICES = ("mdi_font_70", "mdi_font_45", "mdi_font_32")
TYPES = ("lum", "int", "vol", "med", "act", "cap", "bin", "cli")   # ADR-0023

MARQUES_POLICE = ("# >>> tuiles", "# <<< tuiles")
MARQUES_BLUEPRINT = ("# >>> icones (généré par tools/gen_tuiles_icones.py, ne pas éditer)", "# <<< icones")
ENTETES_DOC = {
    "en": "| Code | Off / on | Stands for (`mdi:` icons) | Default for |",
    "fr": "| Code | Éteint / allumé | Représente (icônes `mdi:`) | Défaut de |",
}

RE_CODE = re.compile(r"^[a-z][a-z0-9_]{0,14}$")      # ADR : [a-z0-9_]{1,15} ; lettre en tête
RE_GLYPHE = re.compile(r"^F([0-9A-F]{4}) ([a-z0-9]+(?:-[a-z0-9]+)*)$")
RE_NOM_MDI = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
RE_DOMAINE = re.compile(r"^[a-z_]+(?:\.[a-z_]+)?$")
RE_ITEM_GLYPHE = re.compile(r'^\s*-\s*"\\U000(F[0-9A-Fa-f]{4})"')
# Scalaires que YAML 1.1 (celui de HA) ne lirait pas comme une chaîne.
YAML_SPECIAUX = {"y", "n", "yes", "no", "on", "off", "true", "false", "null"}


class ErreurPalette(ValueError):
    pass


@dataclass(frozen=True)
class Glyphe:
    cp: int
    nom: str

    @property
    def c(self) -> str:
        return f"\\U{self.cp:08X}"


@dataclass
class Icone:
    code: str
    eteint: Glyphe
    allume: Glyphe
    mdi: list[str]
    types: list[str] = field(default_factory=list)
    domaines: list[str] = field(default_factory=list)


@dataclass
class Palette:
    icones: list[Icone]
    repli: str

    def par_code(self) -> dict[str, Icone]:
        return {i.code: i for i in self.icones}

    def glyphes(self) -> dict[int, tuple[str, list[str]]]:
        """point de code → (nom MDI, codes de palette qui l'utilisent), trié."""
        out: dict[int, tuple[str, list[str]]] = {}
        for i in self.icones:
            for g in (i.eteint, i.allume):
                nom, codes = out.setdefault(g.cp, (g.nom, []))
                if i.code not in codes:
                    codes.append(i.code)
        return dict(sorted(out.items()))

    def icones_mdi(self) -> dict[str, str]:
        return {f"mdi:{n}": i.code for i in self.icones for n in i.mdi}

    def icones_defaut(self) -> dict[str, str]:
        return dict(sorted((d, i.code) for i in self.icones for d in i.domaines))

    def defauts_types(self) -> dict[str, str]:
        par_type = {t: i.code for i in self.icones for t in i.types}
        return {t: par_type[t] for t in TYPES}


# --- Lecture et validation de la source --------------------------------------------------

def _glyphe(brut: object, ou: str) -> Glyphe:
    m = RE_GLYPHE.match(str(brut))
    if m is None:
        raise ErreurPalette(f"{ou} : glyphe « {brut} » — format attendu « FXXXX nom-mdi »")
    return Glyphe(int("F" + m.group(1), 16), m.group(2))


def _liste(entree: dict, cle: str, ou: str) -> list[str]:
    v = entree.get(cle) or []
    if not isinstance(v, list) or not all(isinstance(x, str) for x in v):
        raise ErreurPalette(f"{ou} : `{cle}` doit être une liste de chaînes")
    return v


def charger(source: Path = SOURCE) -> Palette:
    data = yaml.safe_load(source.read_text(encoding="utf-8")) or {}
    entrees = data.get("icones")
    if not isinstance(entrees, list) or not entrees:
        raise ErreurPalette(f"{source.name} : liste `icones:` absente ou vide")
    icones: list[Icone] = []
    vus_code: set[str] = set()
    vus_mdi: dict[str, str] = {}
    vus_type: dict[str, str] = {}
    vus_domaine: dict[str, str] = {}
    noms_cp: dict[int, str] = {}
    for n, e in enumerate(entrees, 1):
        if not isinstance(e, dict):
            raise ErreurPalette(f"{source.name} : entrée n° {n} n'est pas un mapping")
        code = str(e.get("code", ""))
        ou = f"{source.name} : `{code or n}`"
        inconnues = set(e) - {"code", "eteint", "allume", "mdi", "types", "domaines"}
        if inconnues:
            raise ErreurPalette(f"{ou} : clé(s) inconnue(s) {sorted(inconnues)}")
        if not RE_CODE.match(code) or code in YAML_SPECIAUX:
            raise ErreurPalette(f"{ou} : code invalide (a-z, 0-9, _ ; 15 caractères ; une lettre en tête)")
        if code in vus_code:
            raise ErreurPalette(f"{ou} : code en double")
        vus_code.add(code)
        eteint = _glyphe(e.get("eteint"), ou)
        allume = _glyphe(e["allume"], ou) if e.get("allume") is not None else eteint
        for g in (eteint, allume):
            if noms_cp.setdefault(g.cp, g.nom) != g.nom:
                raise ErreurPalette(f"{ou} : U+{g.cp:05X} nommé « {g.nom} » ici, « {noms_cp[g.cp]} » ailleurs")
        mdi = _liste(e, "mdi", ou)
        for nom in mdi:
            if not RE_NOM_MDI.match(nom):
                raise ErreurPalette(f"{ou} : nom MDI « {nom} » invalide (sans « mdi: », minuscules et tirets)")
            if nom in vus_mdi:
                raise ErreurPalette(f"{ou} : mdi:{nom} déjà représenté par `{vus_mdi[nom]}`")
            vus_mdi[nom] = code
        types = _liste(e, "types", ou)
        for t in types:
            if t not in TYPES:
                raise ErreurPalette(f"{ou} : type « {t} » inconnu (types : {' '.join(TYPES)})")
            if t in vus_type:
                raise ErreurPalette(f"{ou} : le type {t} a déjà pour défaut `{vus_type[t]}`")
            vus_type[t] = code
        domaines = _liste(e, "domaines", ou)
        for d in domaines:
            if not RE_DOMAINE.match(d):
                raise ErreurPalette(f"{ou} : domaine « {d} » invalide (domaine ou domaine.classe)")
            if d in vus_domaine:
                raise ErreurPalette(f"{ou} : {d} a déjà pour défaut `{vus_domaine[d]}`")
            vus_domaine[d] = code
        icones.append(Icone(code, eteint, allume, mdi, types, domaines))
    manquants = [t for t in TYPES if t not in vus_type]
    if manquants:
        raise ErreurPalette(f"{source.name} : aucun code par défaut pour le(s) type(s) {manquants}")
    repli = str(data.get("repli", ""))
    if repli not in vus_code:
        raise ErreurPalette(f"{source.name} : `repli: {repli}` n'est pas un code de la palette")
    return Palette(icones, repli)


# --- (a) En-tête C++ ------------------------------------------------------------------

def rendre_entete(pal: Palette) -> str:
    largeur = max(len(i.code) for i in pal.icones)
    lignes = [
        "/**",
        " * [AI-CONTEXT]",
        " * @file tab5_tuiles_icones.h",
        " * @role Palette des icônes des tuiles de pièce (ADR-0023) : code de palette → glyphe MDI,",
        " *       variante éteinte / allumée, icône par défaut de chaque type de tuile.",
        " * @architecture_constraint GÉNÉRÉ par tools/gen_tuiles_icones.py depuis",
        " *       Tab5/tuiles_icones.yaml : NE PAS MODIFIER À LA MAIN (--check échoue en CI).",
        " *       API figée : `tuile_icone(code, actif, type)`, `tuiles_icones::kPalette`,",
        " *       `tuiles_icones::defaut_du_type(type)`.",
        " *       Chaque glyphe est dans mdi_font_70, mdi_font_45 et mdi_font_32 (partie",
        " *       « tuiles » de tab5-styles.yaml, même générateur) ; la règle 7 le vérifie via",
        " *       MDI_CODE_TARGETS (tools/check_tab5_code_rules.py). La table doit rester la",
        " *       première chose du fichier qui porte des glyphes : un glyphe écrit dans une",
        " *       fonction serait rattaché à cette fonction par la règle 7.",
        " *       Aucune dépendance (ni ESPHome, ni LVGL) : que des tables constantes.",
        " */",
        "#pragma once",
        "#include <cstring>",
        "",
        "struct TuileIcone {",
        "    const char *code;    // code de palette envoyé par HA ([a-z0-9_]{1,15})",
        "    const char *eteint;  // glyphe UTF-8 quand l'appareil est éteint / fermé / inactif",
        "    const char *allume;  // glyphe UTF-8 quand il est allumé / ouvert / actif",
        "};",
        "",
        "namespace tuiles_icones {",
        "",
        f"// {len(pal.icones)} codes, {len(pal.glyphes())} glyphes distincts.",
        "inline constexpr TuileIcone kPalette[] = {",
    ]
    for i in pal.icones:
        nom = i.eteint.nom if i.allume == i.eteint else f"{i.eteint.nom} / {i.allume.nom}"
        entree = f'{{"{i.code}",'.ljust(largeur + 5)
        lignes.append(f'    {entree}"{i.eteint.c}", "{i.allume.c}"}},  // {nom}')
    lignes += [
        "};",
        "",
        "struct TuileDefaut {",
        "    const char *type;  // type de tuile (ADR-0023)",
        "    const char *code;  // code de palette montré quand HA n'en envoie pas",
        "};",
        "",
        "inline constexpr TuileDefaut kDefautsParType[] = {",
    ]
    lignes += [f'    {{"{t}", "{c}"}},' for t, c in pal.defauts_types().items()]
    lignes += [
        "};",
        "",
        "// Code montré quand ni le code reçu ni le type de la tuile ne sont connus.",
        f'inline constexpr const char *kRepli = "{pal.repli}";',
        "",
        "inline const TuileIcone *trouver(const char *code) {",
        "    if (code == nullptr || code[0] == '\\0') return nullptr;",
        "    for (const auto &ic : kPalette) {",
        "        if (std::strcmp(ic.code, code) == 0) return &ic;",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "// Icône par défaut d'un type de tuile (" + ", ".join(TYPES) + ") ; kRepli sinon.",
        "inline const char *defaut_du_type(const char *type) {",
        "    if (type != nullptr) {",
        "        for (const auto &d : kDefautsParType) {",
        "            if (std::strcmp(d.type, type) == 0) return d.code;",
        "        }",
        "    }",
        "    return kRepli;",
        "}",
        "",
        "}  // namespace tuiles_icones",
        "",
        "// Glyphe à afficher pour une tuile : le code de palette reçu de HA, sinon le défaut de son",
        "// type ; `actif` choisit la variante allumée. Ne renvoie jamais nullptr.",
        "inline const char *tuile_icone(const char *code, bool actif, const char *type) {",
        "    using namespace tuiles_icones;",
        "    const TuileIcone *ic = trouver(code);",
        "    if (ic == nullptr) ic = trouver(defaut_du_type(type));",
        "    if (ic == nullptr) ic = &kPalette[0];",
        "    return actif ? ic->allume : ic->eteint;",
        "}",
        "",
    ]
    return "\n".join(lignes)


# --- Remplacement entre marqueurs ----------------------------------------------------

def _marques(lignes: list[str], debut: str, fin: str, de: int, a: int, ou: str) -> tuple[int, int]:
    ia = [k for k in range(de, a) if lignes[k].strip() == debut]
    ib = [k for k in range(de, a) if lignes[k].strip() == fin]
    if len(ia) != 1 or len(ib) != 1 or ib[0] < ia[0]:
        raise ErreurPalette(f"{ou} : il faut exactement un « {debut} » suivi d'un « {fin} »")
    return ia[0], ib[0]


def _indentation(ligne: str) -> str:
    return ligne[: len(ligne) - len(ligne.lstrip())]


def _section_police(lignes: list[str], police: str) -> tuple[int, int]:
    """[début, fin) de l'entrée `id: police` du bloc `font:` de tab5-styles.yaml."""
    for k, l in enumerate(lignes):
        if re.fullmatch(rf"\s*id:\s*{re.escape(police)}\s*(#.*)?", l):
            deb = k
            while deb > 0 and not re.match(r"^\s*- file:", lignes[deb]):
                deb -= 1
            fin = k + 1
            while fin < len(lignes) and not re.match(r"^(\s*- file:|[A-Za-z_])", lignes[fin]):
                fin += 1
            return deb, fin
    raise ErreurPalette(f"tab5-styles.yaml : police `{police}` introuvable")


def rendre_styles(texte: str, pal: Palette) -> str:
    lignes = texte.split("\n")
    glyphes = pal.glyphes()
    for police in POLICES:
        deb, fin = _section_police(lignes, police)
        a, b = _marques(lignes, *MARQUES_POLICE, deb, fin, f"tab5-styles.yaml, {police} (poser les "
                        f"marqueurs à la fin de sa liste glyphs:)")
        deja = {int(m.group(1), 16) for k in [*range(deb, a), *range(b + 1, fin)]
                if (m := RE_ITEM_GLYPHE.match(lignes[k]))}
        ind = _indentation(lignes[a])
        corps = [f"{ind}# Palette des tuiles de pièce (ADR-0023), sauf les glyphes déjà listés au-dessus."]
        corps += [f'{ind}- "\\U{cp:08X}"  # {nom} ({", ".join(codes)})'
                  for cp, (nom, codes) in glyphes.items() if cp not in deja]
        lignes[a + 1:b] = corps
    return "\n".join(lignes)


def rendre_doc(texte: str, pal: Palette) -> str:
    """Tableau de la palette dans docs/tiles_icons.md, une fois par langue."""
    lignes = texte.split("\n")
    for langue, entete in ENTETES_DOC.items():
        a, b = _marques(lignes, *_marques_doc(langue), 0, len(lignes), f"docs/tiles_icons.md ({langue})")
        corps = [entete, "|---|---|---|---|"]
        for i in pal.icones:
            glyphes = i.eteint.nom if i.allume == i.eteint else f"{i.eteint.nom} / {i.allume.nom}"
            defaut = [f"type `{t}`" for t in i.types] + [f"`{d}`" for d in i.domaines]
            corps.append(f"| `{i.code}` | {glyphes} | {', '.join(i.mdi)} | {', '.join(defaut)} |")
        lignes[a + 1:b] = corps
    return "\n".join(lignes)


def _marques_doc(langue: str) -> tuple[str, str]:
    return (f"<!-- >>> palette {langue} (tools/gen_tuiles_icones.py) -->", f"<!-- <<< palette {langue} -->")


def _cle_yaml(cle: str) -> str:
    return f'"{cle}"' if ":" in cle else cle


def rendre_blueprint(texte: str, pal: Palette) -> str:
    lignes = texte.split("\n")
    a, b = _marques(lignes, *MARQUES_BLUEPRINT, 0, len(lignes), "blueprint tab5_emplacements.yaml "
                    "(poser les marqueurs au début de `variables:`)")
    ind = _indentation(lignes[a])
    corps = [
        f"{ind}# Source : Tab5/tuiles_icones.yaml ({len(pal.icones)} codes). L'icône d'une tuile : celle",
        f"{ind}# de la personnalisation, sinon l'attribut `icon` de l'entité, sinon sa classe, sinon",
        f"{ind}# son domaine ; un « mdi: » hors palette retombe sur le défaut de son domaine.",
        f"{ind}icones_mdi:",
    ]
    corps += [f"{ind}  {_cle_yaml(k)}: {v}" for k, v in pal.icones_mdi().items()]
    corps += [f"{ind}icones_defaut:"]
    corps += [f"{ind}  {_cle_yaml(k)}: {v}" for k, v in pal.icones_defaut().items()]
    lignes[a + 1:b] = corps
    return "\n".join(lignes)


# --- Vérifications contre la police et contre meta.json -------------------------------

def problemes_ttf(pal: Palette, ttf: Path = TTF) -> list[str]:
    """Chaque glyphe existe dans le TTF sous son nom ; chaque nom `mdi:` y existe.
    Hors ligne : les noms de glyphes du TTF sont ceux de MDI (sauf 17 doublons de
    dessin renommés, qu'on n'utilise pas)."""
    import logging

    from fontTools.ttLib import TTFont  # requirements-dev.txt

    # « 2 extra bytes in post.stringData array » : bourrage du TTF de svg2ttf, sans effet.
    logging.getLogger("fontTools").setLevel(logging.ERROR)
    cmap = TTFont(str(ttf)).getBestCmap()
    noms = set(cmap.values())
    out = []
    for cp, (nom, codes) in pal.glyphes().items():
        if cp not in cmap:
            out.append(f"U+{cp:05X} ({nom}, {codes}) absent de {ttf.name}")
        elif cmap[cp] != nom:
            out.append(f"U+{cp:05X} s'appelle « {cmap[cp]} » dans {ttf.name}, pas « {nom} » ({codes})")
    for k, code in pal.icones_mdi().items():
        if k[4:] not in noms:
            out.append(f"{k} ({code}) : nom inconnu de {ttf.name}")
    return out


def problemes_meta(pal: Palette, meta: list[dict]) -> list[str]:
    """Contre le meta.json officiel de @mdi/svg : nom ↔ point de code, noms canoniques
    non dépréciés."""
    par_nom = {m["name"]: m for m in meta}
    out = []
    for cp, (nom, codes) in pal.glyphes().items():
        m = par_nom.get(nom)
        if m is None or int(m["codepoint"], 16) != cp:
            attendu = f"U+{m['codepoint']}" if m else "nom inconnu"
            out.append(f"{nom} ({codes}) : U+{cp:05X} dans la palette, {attendu} dans meta.json")
    for k, code in pal.icones_mdi().items():
        m = par_nom.get(k[4:])
        if m is None:
            out.append(f"{k} ({code}) : absent de meta.json (alias ? prendre le nom canonique)")
        elif m.get("deprecated"):
            out.append(f"{k} ({code}) : déprécié dans MDI")
    return out


# --- Point d'entrée ---------------------------------------------------------------------

def _lire(chemin: Path) -> tuple[str, str]:
    """(texte en LF, fin de ligne du fichier) ; fichier absent → ('', LF)."""
    if not chemin.exists():
        return "", "\n"
    brut = chemin.read_bytes().decode("utf-8")
    return brut.replace("\r\n", "\n"), ("\r\n" if "\r\n" in brut else "\n")


def cibles(pal: Palette) -> list[tuple[Path, str, str]]:
    """(fichier, contenu actuel en LF, contenu attendu en LF) des quatre sorties."""
    out = []
    entete, _ = _lire(ENTETE)
    out.append((ENTETE, entete, rendre_entete(pal)))
    styles, _ = _lire(STYLES)
    out.append((STYLES, styles, rendre_styles(styles, pal)))
    bp, _ = _lire(BLUEPRINT)
    out.append((BLUEPRINT, bp, rendre_blueprint(bp, pal)))
    doc, _ = _lire(DOC)
    out.append((DOC, doc, rendre_doc(doc, pal)))
    return out


def main(argv: list[str]) -> int:
    try:
        pal = charger()
        if "--meta" in argv:
            k = argv.index("--meta")
            if k + 1 >= len(argv):
                print("--meta attend le chemin d'un meta.json de @mdi/svg")
                return 2
            meta = json.loads(Path(argv[k + 1]).read_text(encoding="utf-8"))
            problemes = problemes_meta(pal, meta)
            for p in problemes:
                print("❌", p)
            if not problemes:
                print(f"✅ {len(pal.glyphes())} glyphes et {len(pal.icones_mdi())} noms mdi: conformes à meta.json")
            return 1 if problemes else 0
        sorties = cibles(pal)
    except ErreurPalette as e:
        print(f"❌ {e}")
        return 1
    perimes = [p for p, actuel, attendu in sorties if actuel != attendu]
    resume = f"{len(pal.icones)} codes, {len(pal.glyphes())} glyphes, {len(pal.icones_mdi())} noms mdi:"
    if "--check" in argv:
        for p in perimes:
            print(f"❌ {p.relative_to(REPO).as_posix()} n'est pas à jour : python tools/gen_tuiles_icones.py")
        if not perimes:
            print(f"✅ palette des tuiles à jour ({resume})")
        return 1 if perimes else 0
    for p, _, attendu in sorties:
        if p in perimes:
            _, eol = _lire(p)
            p.write_bytes(attendu.replace("\n", eol).encode("utf-8"))
            print(f"écrit {p.relative_to(REPO).as_posix()}")
    print(f"palette : {resume}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
