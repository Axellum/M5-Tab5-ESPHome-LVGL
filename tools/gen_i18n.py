#!/usr/bin/env python3
"""tools/gen_i18n.py — génère Tab5/tab5_i18n_data.h depuis Tab5/lang/*.yaml.

[AI-CONTEXT] Traduction de l'écran façon gettext : le texte FRANÇAIS du code est la
clé. `tr("Calendrier")` renvoie « Calendar » en anglais, et le texte d'origine si la
langue est le français ou si la traduction manque. Ajouter une langue = copier
Tab5/lang/en.yaml, changer `_langue`, `_code` et `_index` (le suivant, jamais un
index déjà pris : la tablette mémorise l'index du sélecteur), traduire, puis :

    python tools/gen_i18n.py           # réécrit Tab5/tab5_i18n_data.h
    python tools/gen_i18n.py --check   # exit 1 si le fichier généré n'est plus à jour

et ajouter le nom de la langue aux `options:` du select « Langue »
(Tab5/tab5-ha-controls.yaml), dans l'ordre des index. tests/test_i18n.py vérifie
le reste (clés utilisées, formats printf, glyphes des polices).

Format d'un fichier de langue : un mapping YAML plat `"texte français": "traduction"`.
Une clé `"contexte|texte"` sépare deux sens d'un même mot (`tr_ctx("jour", "Jeu")`
→ « Thu », alors qu'un « Jeu » sans contexte serait un jeu). Les clés qui commencent
par `_` sont des métadonnées.

Repli : une traduction vide ou absente prend le texte ANGLAIS, écrit ici dans la table
de la langue (commentaire « repli en »), puis le français si l'anglais ne l'a pas non
plus. Une langue récente peut donc rester partielle (sans `_statut: complet`) : ses
trous s'affichent en anglais plutôt qu'en français. Le repli est fait à la génération
et non dans tr() : aucun coût à l'exécution, et tab5_i18n.cpp ne change pas.
"""
from __future__ import annotations

import sys
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
LANG_DIR = REPO / "Tab5" / "lang"
OUT = REPO / "Tab5" / "tab5_i18n_data.h"
SEP = "|"
REPLI = "en"   # langue de repli des langues partielles (doit rester complète)


def load_languages(lang_dir: Path = LANG_DIR) -> list[dict]:
    langs = []
    for f in sorted(lang_dir.glob("*.yaml")):
        data = yaml.safe_load(f.read_text(encoding="utf-8")) or {}
        if not isinstance(data, dict):
            raise ValueError(f"{f.name} : un mapping YAML est attendu")
        meta = {k: data.pop(k) for k in list(data) if str(k).startswith("_")}
        for k in ("_langue", "_code", "_index"):
            if k not in meta:
                raise ValueError(f"{f.name} : métadonnée {k} manquante")
        entries = {}
        for k, v in data.items():
            if not isinstance(k, str) or not isinstance(v, (str, type(None))):
                raise ValueError(f"{f.name} : clé et traduction doivent être des chaînes ({k!r})")
            entries[k] = v if v else None
        langs.append({"file": f.name, "name": str(meta["_langue"]), "code": str(meta["_code"]),
                      "index": int(meta["_index"]), "entries": entries})
    langs.sort(key=lambda l: l["index"])
    if [l["index"] for l in langs] != list(range(len(langs))):
        raise ValueError("index des langues : 0, 1, 2… sans trou ni doublon (fr = 0)")
    if not langs or langs[0]["code"] != "fr":
        raise ValueError("fr.yaml (_index: 0) est la langue source")
    if langs[0]["entries"]:
        raise ValueError("fr.yaml ne contient que des métadonnées : le français est la clé")
    return langs


def split_key(k: str) -> tuple[str, str]:
    if SEP in k:
        ctx, txt = k.split(SEP, 1)
        return ctx, txt
    return "", k


def c_str(s: str | None) -> str:
    if s is None:
        return "nullptr"
    out = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ord(ch) < 0x20:
            out.append(f"\\{ord(ch):03o}")
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def render(langs: list[dict]) -> str:
    keys = sorted({k for l in langs[1:] for k in l["entries"]},
                  key=lambda k: tuple(p.encode("utf-8") for p in split_key(k)))
    lines = [
        "// GÉNÉRÉ par tools/gen_i18n.py depuis Tab5/lang/*.yaml — NE PAS MODIFIER À LA MAIN.",
        "// Clés = textes français (triés par octets UTF-8 : contexte puis texte, pour la",
        "// recherche dichotomique de tab5_i18n.cpp). Texte absent d'une langue = texte anglais",
        "// (« repli en ») ; nullptr = pas de traduction du tout → français.",
        "#pragma once",
        "#include <cstdint>",
        "",
        f"static const uint8_t kI18nLangCount = {len(langs)};",
        "static const char* const kI18nLangNames[] = {" + ", ".join(c_str(l["name"]) for l in langs) + "};",
        "static const char* const kI18nLangCodes[] = {" + ", ".join(c_str(l["code"]) for l in langs) + "};",
        f"static const uint16_t kI18nKeyCount = {len(keys)};",
        "",
        "static const char* const kI18nCtx[] = {",
    ]
    lines += [f"    {c_str(split_key(k)[0])}," for k in keys] or ["    nullptr,"]
    lines += ["};", "", "static const char* const kI18nKeys[] = {"]
    lines += [f"    {c_str(split_key(k)[1])}," for k in keys] or ["    nullptr,"]
    lines += ["};"]
    repli = next((l for l in langs[1:] if l["code"] == REPLI), None)
    for l in langs[1:]:
        lines += ["", f"// {l['name']} ({l['file']})", f"static const char* const kI18n_{l['code']}[] = {{"]
        for k in keys:
            v, note = l["entries"].get(k), ""
            if v is None and repli is not None and l is not repli:
                v = repli["entries"].get(k)
                note = f" (repli {REPLI})" if v is not None else ""
            lines.append(f"    {c_str(v)},  // {c_str(k)}{note}")
        if not keys:
            lines.append("    nullptr,")
        lines += ["};"]
    tables = ", ".join(["nullptr"] + [f"kI18n_{l['code']}" for l in langs[1:]])
    lines += ["", f"static const char* const* const kI18nTables[] = {{{tables}}};", ""]
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    langs = load_languages()
    text = render(langs)
    if "--check" in argv:
        actual = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
        if actual.replace("\r\n", "\n") != text:
            print("❌ Tab5/tab5_i18n_data.h n'est pas à jour : python tools/gen_i18n.py")
            return 1
        print(f"✅ tab5_i18n_data.h à jour ({len(langs)} langues)")
        return 0
    OUT.write_bytes(text.encode("utf-8"))
    print(f"écrit {OUT.relative_to(REPO)} : {len(langs)} langues")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
