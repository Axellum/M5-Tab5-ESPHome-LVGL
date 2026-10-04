#!/usr/bin/env python3
"""tools/police_theme.py — mesure les polices d'affichage des thèmes (ADR-0029, lot 3).

[AI-CONTEXT] Un thème peut changer la police de trois textes d'affichage : l'heure
(horloge à rouleaux et écran de sonnerie), la date sous l'horloge et les titres (en-tête
des popups, titre de la carte centrale). Le reste du texte reste en Roboto : les libellés
ont été calés en Roboto dans 7 langues.

Ce script lit les familles citées par `Tab5/themes/*.yaml` (bloc `polices:`), télécharge
chaque fichier comme ESPHome (API CSS2 de Google Fonts, format truetype), relève ses
métriques avec fontTools et calcule, pour chaque rôle, la taille et la position :

  - horloge : la plus grande taille (≤ 130 px, celle de Roboto) dont chaque chiffre tient
    dans le cadre de 75 × 104 du rouleau, centré, avec 2 px d'air en haut et en bas, et
    dont le « : » tient entre les deux groupes ; le label est remonté pour centrer
    l'encre dans le cadre ; le « : » est centré entre les heures et les minutes ;
  - date : la plus grande taille (≤ 45 px) dont la date la plus large, dans les 7 langues,
    tient dans la tuile de l'horloge avec 16 px de marge de chaque côté ;
  - titre : la plus grande taille (≤ 32 px) pour laquelle le plus long des titres de
    popup, dans les 7 langues, n'est pas plus large que le plus long en Roboto 32.

Un caractère absent du fichier (lettres turques de Fredoka, symboles rares de Jersey 10)
est dessiné par la Roboto du même rôle : tab5_theme.cpp la pose en repli (`fallback`
de lv_font_t) sur chaque police de thème. La couverture de chaque fichier est relevée
sur un jeu fixe (UNIVERS : Latin-1, Latin étendu A, ponctuation typographique), et
tools/gen_themes.py ne demande à ESPHome que les glyphes présents (ESPHome refuse un
glyphe absent). Les chiffres et le « : » de l'horloge doivent exister dans la police.

Il écrit `Tab5/themes/_polices.yaml` : les métriques des chiffres et du « : » (pour que
pytest refasse la géométrie de l'horloge hors ligne, tests/test_polices_themes.py), les
caractères absents et les tailles retenues. Roboto 700 y figure comme référence : ses
valeurs doivent redonner la géométrie actuelle (130 px, y -23, « : » à 181 ; date 45 ;
titres 32). tools/gen_themes.py lit ce fichier. À relancer après l'ajout d'une police à
un thème (pytest le signale) ou d'un titre de popup nettement plus long.

    python tools/police_theme.py   # réseau : télécharge les polices absentes du cache

Cache : $ESPHOME_DATA_DIR/tab5_polices (ou ~/.cache/tab5_polices).
"""
from __future__ import annotations

import hashlib
import math
import os
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
THEMES_DIR = TAB5 / "themes"
SORTIE = THEMES_DIR / "_polices.yaml"
LANG_DIR = TAB5 / "lang"
REFERENCE = "Roboto@700"

# Géométrie de la tuile horloge (tab5-lvgl.yaml, clock_roller.yaml ; tests/test_horloge.py).
CADRE_L, CADRE_H = 75, 104
CADRE_Y = 33
MARGE_MIN = 2
X_H1, X_M10 = 102, 222          # x des rouleaux h1 et m10 : le « : » vit entre les deux
TUILE_UTILE = 399               # 401 - 2 × bordure
MARGE_DATE = 16
MAX_TAILLE = {"horloge": 130, "date": 45, "titre": 32}
MIN_TAILLE = {"horloge": 80, "date": 30, "titre": 22}
CHIFFRES = "0123456789"
# Jeu sur lequel la couverture de chaque fichier est relevée : tout texte d'affichage
# (dates et titres dans les 7 langues) doit s'y trouver (tools/gen_themes.py le vérifie).
UNIVERS = ("".join(chr(c) for c in range(0x20, 0x7F)) + "".join(chr(c) for c in range(0xA0, 0x180))
           + "‘’‚“”„–—…€•")
# Titres posés par le C++ (nom d'une pièce, jour du calendrier…) : lettres et chiffres
# courants en plus des titres connus, pour qu'ils restent dans la police du thème.
TITRE_BASE = ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !'(),-./:?%°’«»–—…"
              "àâäçéèêëîïôöùûüÿœæÀÂÄÇÉÈÊËÎÏÔÖÙÛÜŸŒÆ")


def cache_dir() -> Path:
    base = os.environ.get("ESPHOME_DATA_DIR")
    d = Path(base) / "tab5_polices" if base else Path.home() / ".cache" / "tab5_polices"
    d.mkdir(parents=True, exist_ok=True)
    return d


def _lire_url(url: str, timeout: int) -> bytes:
    # Même User-Agent qu'ESPHome (external_files.py) : Google Fonts sert un fichier
    # différent selon le client, et c'est celui d'ESPHome qui est compilé.
    try:
        from esphome.const import __version__ as version
    except ImportError:
        version = "2026.9.0"
    req = urllib.request.Request(url, headers={"User-agent": f"ESPHome/{version} (https://esphome.io)"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def telecharger(famille: str, graisse: int) -> Path:
    """Le fichier TTF de Google Fonts, comme ESPHome (_gfonts_css_url, format truetype)."""
    chemin = cache_dir() / f"{famille.replace(' ', '+')}@{graisse}.ttf"
    if chemin.exists():
        return chemin
    url = (f"https://fonts.googleapis.com/css2?family={urllib.parse.quote(famille)}"
           f":ital,wght@0,{graisse}")
    css = _lire_url(url, 30).decode("utf-8")
    m = re.search(r"src:\s+url\((.+?)\)\s+format\('truetype'\);", css)
    if m is None:
        raise SystemExit(f"{famille}@{graisse} : pas de fichier truetype dans la réponse de Google Fonts")
    chemin.write_bytes(_lire_url(m.group(1), 60))
    return chemin


# --- Textes à couvrir ---------------------------------------------------------

def _check_rules():
    import importlib.util
    spec = importlib.util.spec_from_file_location("regles", REPO / "tools" / "check_tab5_code_rules.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def langues() -> list[dict]:
    out = []
    for p in sorted(LANG_DIR.glob("*.yaml")):
        with open(p, encoding="utf-8") as f:
            out.append(yaml.safe_load(f) or {})
    return out


def dates() -> list[str]:
    """Toutes les dates possibles sous l'horloge, en français et dans chaque langue."""
    regles = _check_rules()
    src = regles.strip_cpp_comments((TAB5 / "tab5_core.cpp").read_text(encoding="utf-8"))
    jours = regles._c_literals(re.search(r"fr_day_short_utf8\(int wday\)\s*\{.*?days\[\] = \{(.*?)\};",
                                         src, re.S).group(1))
    mois = re.search(r"clock_month_short_utf8\(int month\)\s*\{.*?months\[\] = \{(.*?)\};", src, re.S).group(1)
    # Les tables coupent « "D\xC3\xA9" "c" » : on recolle les littéraux adjacents.
    mois = [m.replace("\n", "") for m in regles._c_literals(re.sub(r'"\s+"', "", mois))]
    modele = "{jour_court} {quantieme} {mois_court}"
    out = []
    for trad in [{}] + langues():
        forme = trad.get(modele, modele)
        for j in jours:
            for m in mois:
                for q in ("00", "08", "28", "30"):
                    out.append(forme.replace("{jour_court}", trad.get(j, j))
                               .replace("{quantieme}", q).replace("{mois_court}", trad.get(m, m)))
    return out


def titres() -> list[str]:
    """Titres des popups (en-tête partagé modal_header.yaml), en français et traduits."""
    francais = set()
    for p in sorted((TAB5 / "ui_components").glob("*.yaml")):
        for m in re.finditer(r"file: modal_header\.yaml, vars: \{[^}]*?title: \"([^\"]+)\"",
                             p.read_text(encoding="utf-8")):
            francais.add(m.group(1))
    out = set(francais)
    for trad in langues():
        out |= {trad.get(t, t) for t in francais}
    return sorted(out)


def jeux_par_role(liste_dates: list[str] | None = None, liste_titres: list[str] | None = None) -> dict[str, str]:
    """Caractères que chaque rôle peut afficher (avant de retirer ceux absents d'une police)."""
    liste_dates = dates() if liste_dates is None else liste_dates
    liste_titres = titres() if liste_titres is None else liste_titres
    return {
        "horloge": CHIFFRES + ":",
        "date": "".join(sorted(set("".join(liste_dates)))),
        "titre": "".join(sorted(set("".join(liste_titres)) | set(TITRE_BASE))),
    }


# --- Métriques ---------------------------------------------------------------

def mesurer(chemin: Path, caracteres: str) -> dict:
    from fontTools.pens.boundsPen import BoundsPen
    from fontTools.ttLib import TTFont

    f = TTFont(chemin)
    cmap = f.getBestCmap()
    jeu = f.getGlyphSet()
    hmtx = f["hmtx"]
    glyphes, manquants = {}, []
    for c in sorted(set(caracteres)):
        nom = cmap.get(ord(c))
        if nom is None:
            manquants.append(c)
            continue
        avance = hmtx[nom][0]
        stylo = BoundsPen(jeu)
        jeu[nom].draw(stylo)
        b = stylo.bounds or (0, 0, 0, 0)
        glyphes[c] = [avance, round(b[0]), round(b[2]), round(b[1]), round(b[3])]
    return {
        "unites_em": f["head"].unitsPerEm,
        "ascendante": f["hhea"].ascent,
        "glyphes": glyphes,
        "manquants": "".join(manquants),
        "sha256": hashlib.sha256(chemin.read_bytes()).hexdigest()[:16],
    }


# --- Calculs (repris tels quels par tests/test_polices_themes.py) -------------------

def geometrie_horloge(m: dict, taille: int) -> dict | None:
    """Arrondis de FreeType, ceux d'ESPHome (tests/test_horloge.py) : ascendante et haut de
    l'encre au pixel supérieur, bas de l'encre au pixel inférieur, avance au plus proche.
    None si un chiffre ou le « : » ne tient pas."""
    e = taille / m["unites_em"]
    asc = math.ceil(m["ascendante"] * e)
    g = m["glyphes"]
    haut = min(asc - math.ceil(g[c][4] * e) for c in CHIFFRES)
    bas = max(asc - math.floor(g[c][3] * e) for c in CHIFFRES)
    if bas - haut > CADRE_H - 2 * MARGE_MIN:
        return None
    for c in CHIFFRES:
        avance = round(g[c][0] * e)
        x0 = (CADRE_L - avance) // 2          # label centré (TOP_MID) dans le cadre
        if x0 + math.floor(g[c][1] * e) < 0 or x0 + math.ceil(g[c][2] * e) > CADRE_L:
            return None
    fente = X_M10 - (X_H1 + CADRE_L)          # entre les heures et les minutes
    avance_dp = round(g[":"][0] * e)
    if avance_dp > fente:
        return None
    y = round((CADRE_H - (haut + bas)) / 2)
    return {"taille": taille, "y": y, "x_deux_points": X_H1 + CADRE_L + (fente - avance_dp + 1) // 2,
            "y_deux_points": CADRE_Y + y}


def largeur(m: dict, texte: str, taille: int, ref: dict | None = None) -> int:
    """Somme des avances arrondies ; un caractère absent compte avec celle de Roboto
    (`ref`), qui le dessinera (repli de la police)."""
    total = 0
    for c in texte:
        if c in m["glyphes"]:
            total += round(m["glyphes"][c][0] * taille / m["unites_em"])
        else:
            total += round(ref["glyphes"][c][0] * taille / ref["unites_em"])
    return total


def tailles(m: dict, ref: dict, liste_dates: list[str], liste_titres: list[str]) -> dict:
    out = {}
    for t in range(MAX_TAILLE["horloge"], MIN_TAILLE["horloge"] - 1, -1):
        geo = geometrie_horloge(m, t)
        if geo:
            out["horloge"] = geo
            break
    for t in range(MAX_TAILLE["date"], MIN_TAILLE["date"] - 1, -1):
        if max(largeur(m, d, t, ref) for d in liste_dates) <= TUILE_UTILE - 2 * MARGE_DATE:
            out["date"] = {"taille": t}
            break
    plus_long = max(largeur(ref, s, MAX_TAILLE["titre"]) for s in liste_titres)
    for t in range(MAX_TAILLE["titre"], MIN_TAILLE["titre"] - 1, -1):
        if max(largeur(m, s, t, ref) for s in liste_titres) <= plus_long:
            out["titre"] = {"taille": t}
            break
    return out


def familles_citees() -> list[str]:
    out = {REFERENCE}
    for p in sorted(THEMES_DIR.glob("*.yaml")):
        if p.name.startswith("_"):
            continue
        with open(p, encoding="utf-8") as f:
            polices = (yaml.safe_load(f) or {}).get("polices") or {}
        out |= {str(v) for v in polices.values()}
    return sorted(out, key=lambda x: (x != REFERENCE, x))


def main() -> int:
    liste_dates, liste_titres = dates(), titres()
    hors_univers = sorted(set("".join(jeux_par_role(liste_dates, liste_titres).values())) - set(UNIVERS))
    if hors_univers:
        print(f"[KO] caractères d'affichage hors du jeu mesuré (UNIVERS) : {''.join(hors_univers)!r}")
        return 1
    mesures = {}
    for cle in familles_citees():
        famille, graisse = cle.rsplit("@", 1)
        mesures[cle] = mesurer(telecharger(famille, int(graisse)), UNIVERS)
    ref = mesures[REFERENCE]
    sortie = {}
    for cle, m in mesures.items():
        if set(CHIFFRES + ":") & set(m["manquants"]):
            print(f"[KO] {cle} : chiffres ou « : » absents ({m['manquants']!r}), pas d'horloge possible")
            return 1
        r = tailles(m, ref, liste_dates, liste_titres)
        sortie[cle] = {
            "sha256": m["sha256"], "unites_em": m["unites_em"], "ascendante": m["ascendante"],
            # Absents du fichier (dans UNIVERS) : dessinés par la Roboto du même rôle.
            "manquants": m["manquants"],
            **r,
            "chiffres": {c: m["glyphes"][c] for c in CHIFFRES + ":"},
        }
        print(f"[OK] {cle} : " + ", ".join(f"{k} {v['taille']}" for k, v in r.items())
              + (f" ; absents : {m['manquants']!r}" if m["manquants"] else ""))
    entete = ("# Écrit par tools/police_theme.py — ne pas modifier à la main (ADR-0029, lot 3).\n"
              "# Métriques en unités de police (fontTools, fichier de Google Fonts) ; tailles et\n"
              "# positions en px. chiffres : caractère → [avance, xMin, xMax, yMin, yMax].\n"
              "# tests/test_polices_themes.py refait la géométrie de l'horloge depuis ces valeurs.\n")
    texte = yaml.safe_dump(sortie, allow_unicode=True, sort_keys=False, width=110,
                           default_flow_style=None)
    SORTIE.write_text(entete + texte, encoding="utf-8", newline="\n")
    print(f"{len(sortie)} police(s) dans {SORTIE.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
