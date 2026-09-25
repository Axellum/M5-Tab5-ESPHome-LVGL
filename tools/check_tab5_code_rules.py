#!/usr/bin/env python3
"""Règles de code du firmware Tab5, jouées à chaque `pytest` (audit du 06/09/2026,
§4.1 points 1, 4, 15 et §4.2 point 17 ; ADR-0006). Sept règles, toutes
falsifiables sur le dépôt réel :

  1. **`snprintf` partout** : aucun `sprintf(` brut dans `Tab5/*.cpp`, `*.h`,
     `*.yaml` ni `tab5-ha-hmi.yaml`. Un futur `%s` sur un buffer de 16 octets ne
     doit pas pouvoir déborder en silence.
  2. **Aucune logique LVGL dans `tab5-api-logic.yaml` ni `tab5-hardware.yaml`** :
     les services et les callbacks (voice_assistant, micro_wake_word, online_image)
     résolvent les `id()` et appellent `tab5_custom.cpp`. Dans le contrat API,
     seul `lv_obj_has_flag` (lecture pure, dans une condition) est toléré ; dans
     le fichier matériel, rien.
  3. **Aucun global orphelin** dans `tab5-globals.yaml` : chaque `- id:` du bloc
     `globals:` doit être lu ou écrit quelque part (`id(x)` dans une lambda,
     `id: x` dans une action `globals.set` / `globals.increment`…). Un global
     que personne ne référence est du code mort qui trompe le lecteur.
  4. **Aucune entité Home Assistant en dur** dans un YAML du firmware : toute
     valeur `entity_id:` littérale (`domaine.objet`) doit être une substitution
     de `user_entities.yaml` (`${entity_…}`) ou un `!lambda`. Les entités que la
     tablette expose elle-même (`assist_satellite.*`, `media_player.*`) sont
     dérivées de son nom dans HA : un renommage cassait l'interruption vocale et
     l'annonce des rendez-vous sans aucune erreur (audit §4.1 point 15).
  5. **Métadonnées sur chaque action du contrat API** (`tab5-api-logic.yaml`) :
     toute action porte une `description:`, et chaque variable la forme longue
     `type:` + `description:` + `example:` (ESPHome 2026.9.0, amont #18881). Ce
     sont elles que Home Assistant affiche dans « Outils de développement →
     Actions » : sans elles, un champ n'est qu'une case de texte sans indice sur
     le format du payload, toujours sérialisé à la main ici.
  6. **Glyphes de la date** (audit du 25/09/2026, lot 4) : `roboto_45` ne sert qu'à
     `lbl_date` et n'embarque plus que ses caractères (37 au lieu de 216 Latin-1,
     ≈ 59 Ko de flash). Chaque caractère des jours de `update_clock_date_ui()`
     (tab5_anim.cpp), des mois de `clock_month_short_utf8()` (tab5_text.cpp), des
     chiffres et du texte initial du label doit être dans sa liste de glyphes —
     sinon la lettre s'affiche vide, sans aucune erreur de compilation.
  7. **Icônes MDI couvertes, et rien de plus** (audit des polices du 25/09/2026) :
     chaque icône `\\U000Fxxxx` affichée doit figurer dans la police `mdi_*` du
     widget qui la porte, et chaque glyphe déclaré dans une police `mdi_*` doit
     être affiché quelque part. La police se lit sur le label (`text_font:`), sur
     le gabarit d'un `!include` (variable `${icon}`), sur le widget d'un
     `lv_label_set_text(id(x), …)`, ou via `MDI_CODE_TARGETS` quand le C++ reçoit
     le widget en paramètre. Une icône qu'on ne sait pas rattacher fait échouer la
     règle. Trouvé ainsi : la cloche barrée du popup réveil absente de `mdi_font_45`
     (icône vide réveil éteint) et 85 glyphes MDI jamais affichés (≈ 19 Ko).

Usage : python tools/check_tab5_code_rules.py   (aussi lancé par `pytest`, tests/test_guards.py)
Sortie : 0 si tout est conforme, 1 sinon (liste des écarts sur stdout).
"""

from __future__ import annotations

import fnmatch
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
ENTRY = REPO / "tab5-ha-hmi.yaml"
API_LOGIC = TAB5 / "tab5-api-logic.yaml"
HARDWARE = TAB5 / "tab5-hardware.yaml"
GLOBALS_YAML = TAB5 / "tab5-globals.yaml"

RE_SPRINTF = re.compile(r"(?<![A-Za-z_])sprintf\s*\(")
RE_LV_CALL = re.compile(r"\b(lv_[a-z0-9_]+)\s*\(")
# fichier → appels lv_* tolérés (lecture pure). Tout le reste est interdit.
LV_ALLOWED = {
    "tab5-api-logic.yaml": {"lv_obj_has_flag"},
    "tab5-hardware.yaml": set(),
    # Règle 2 (sensor:/text_sensor: sans lv_*), tenue depuis le 08/09/2026.
    "tab5-sensors-diagnostics.yaml": set(),
    "tab5-sensors-domotique.yaml": set(),
}
RE_GLOBAL_DEF = re.compile(r"^  - id: (\w+)\s*$", re.M)
# `entity_id: domaine.objet` littéral (clé `entity_id` ou `*_entity_id`). Une
# substitution `${…}`, un `!lambda` ou une liste de substitutions ne matchent pas.
RE_HA_ENTITY_LITERAL = re.compile(r"^\s*-?\s*\w*entity_id:\s*['\"]?([a-z_]+\.[A-Za-z0-9_]+)['\"]?\s*$")
RE_TOP_KEY = re.compile(r"^[a-z_]+:", re.M)
# Variable d'action en forme courte (`payload: string`) alors que la forme longue
# type/description/example est attendue. `type: string` ressemble lui-même à une
# forme courte, d'où l'exclusion du nom `type`.
RE_API_VAR_SHORTHAND = re.compile(r"^\s+(?!type:)([a-z0-9_]+): (?:string|int|float|bool)\s*$")


def strip_yaml_comments(text: str) -> str:
    return "\n".join(l for l in text.splitlines() if not l.lstrip().startswith("#"))


def strip_cpp_comments(text: str) -> str:
    text = "\n".join(l.split("//", 1)[0] for l in text.splitlines())
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def firmware_sources(tab5: Path = TAB5, entry: Path = ENTRY) -> list[Path]:
    files = sorted(tab5.glob("*.cpp")) + sorted(tab5.glob("*.h"))
    files += sorted(tab5.glob("*.yaml")) + sorted((tab5 / "ui_components").glob("*.yaml"))
    if entry.is_file():
        files.append(entry)
    return files


def globals_defined(globals_yaml: Path = GLOBALS_YAML) -> list[str]:
    """Les `- id:` du bloc `globals:` uniquement (le fichier porte aussi un interval)."""
    text = globals_yaml.read_text(encoding="utf-8")
    start = text.find("\nglobals:")
    if start < 0:
        return []
    rest = text[start + len("\nglobals:"):]
    m = RE_TOP_KEY.search(rest, 1)
    block = rest if m is None else rest[: m.start()]
    return RE_GLOBAL_DEF.findall(strip_yaml_comments(block))


def api_action_metadata(api_logic: Path = API_LOGIC) -> list[str]:
    """Règle 5 : chaque action du contrat API décrite, chaque variable en forme longue."""
    problems: list[str] = []
    text = strip_yaml_comments(api_logic.read_text(encoding="utf-8"))
    # Une action va de son `- service:` au suivant ; on ne lit que sa tête (avant
    # `then:`), pour ne pas confondre ses métadonnées avec le C++ de ses lambdas.
    for chunk in re.split(r"^\s+- service: ", text, flags=re.M)[1:]:
        name = chunk.splitlines()[0].strip()
        head = re.split(r"^\s+then:\s*$", chunk, maxsplit=1, flags=re.M)[0]
        # parts[0] = l'action elle-même, sans ses variables : sinon la description
        # d'une variable suffirait à faire passer l'action pour décrite.
        parts = re.split(r"^\s+variables:\s*$", head, maxsplit=1, flags=re.M)

        if not re.search(r"^\s+description: \S", parts[0], re.M):
            problems.append(
                f"{api_logic.name} : action `{name}` sans `description:` — c'est ce que "
                f"Home Assistant affiche dans « Outils de développement → Actions »"
            )

        if len(parts) != 2:
            continue
        lines = [l for l in parts[1].splitlines() if l.strip()]
        if not lines:
            continue

        # Indentation de premier niveau du bloc = celle des noms de variables ;
        # tout ce qui est plus indenté appartient à la variable en cours.
        var_indent = len(lines[0]) - len(lines[0].lstrip())
        found: dict[str, set[str]] = {}
        var: str | None = None
        for line in lines:
            if len(line) - len(line.lstrip()) > var_indent:
                if var is not None:
                    found[var].add(line.strip().split(":", 1)[0])
                continue
            short = RE_API_VAR_SHORTHAND.match(line)
            if short:
                problems.append(
                    f"{api_logic.name} : action `{name}`, variable `{short.group(1)}` en "
                    f"forme courte — passer à type/description/example"
                )
                var = None
                continue
            var = line.strip().rstrip(":")
            found[var] = set()

        for var, keys in found.items():
            missing = {"type", "description", "example"} - keys
            if missing:
                problems.append(
                    f"{api_logic.name} : action `{name}`, variable `{var}` sans "
                    f"{', '.join(sorted(missing))}"
                )

    return problems


RE_C_STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')


def _c_literals(block: str) -> list[str]:
    """Littéraux C d'un bloc, échappements \\xNN décodés (UTF-8). Chaque littéral est
    décodé seul : les tables coupent exprès « "D\\xC3\\xA9" "c" » pour que \\xA9 ne
    mange pas le « c » — la couverture des caractères n'a pas besoin de les recoller."""
    import codecs

    out = []
    for lit in RE_C_STRING.findall(block):
        raw = codecs.escape_decode(lit.encode("latin-1"))[0]
        out.append(raw.decode("utf-8"))
    return out


RE_YAML_QUOTED = r"'(?:[^']|'')*'|\"(?:[^\"\\]|\\.)*\""
RE_YAML_KEY = re.compile(r"^(\s*(?:-\s+)*)([A-Za-z_]\w*):")


def _yaml_scalar(raw: str) -> str:
    """Valeur d'un scalaire YAML entre guillemets (les doubles décodent \\U et \\u)."""
    raw = raw.strip()
    if raw.startswith("'"):
        return raw[1:-1].replace("''", "'")
    body = re.sub(
        r"\\U([0-9A-Fa-f]{8})|\\u([0-9A-Fa-f]{4})",
        lambda m: chr(int(m.group(1) or m.group(2), 16)),
        raw[1:-1],
    )
    return body.replace('\\"', '"').replace("\\\\", "\\")


def font_glyphs(styles: Path) -> dict[str, set[str]]:
    """Glyphes de chaque police du bloc `font:`. `glyphs:` peut être une chaîne, une
    ancre (`&latin1 '…'`, reprise par `*latin1`) ou une liste, un élément par ligne."""
    text = strip_yaml_comments(styles.read_text(encoding="utf-8"))
    m = re.search(r"^font:\s*$", text, re.M)
    if m is None:
        return {}
    rest = text[m.end():]
    end = RE_TOP_KEY.search(rest)
    block = rest if end is None else rest[: end.start()]
    anchors: dict[str, str] = {}
    fonts: dict[str, set[str]] = {}
    for entry in re.split(r"^  - (?=file:)", block, flags=re.M)[1:]:
        fid = re.search(r"^\s*id:\s*(\w+)", entry, re.M)
        if fid is None:
            continue
        chars = ""
        single = re.search(rf"^\s*glyphs:\s*(?:&(\w+)\s+)?({RE_YAML_QUOTED})\s*(?:#.*)?$", entry, re.M)
        alias = re.search(r"^\s*glyphs:\s*\*(\w+)", entry, re.M)
        listed = re.search(r"^(\s*)glyphs:\s*(?:#.*)?$", entry, re.M)
        if single:
            chars = _yaml_scalar(single.group(2))
            if single.group(1):
                anchors[single.group(1)] = chars
        elif alias:
            chars = anchors.get(alias.group(1), "")
        elif listed:
            for line in entry[listed.end():].splitlines()[1:]:
                item = re.match(rf"^\s+-\s*({RE_YAML_QUOTED})\s*(?:#.*)?$", line)
                if item is None:
                    if line.strip():
                        break
                    continue
                chars += _yaml_scalar(item.group(1))
        fonts[fid.group(1)] = set(chars)
    return fonts


def date_glyph_coverage(tab5: Path = TAB5) -> list[str]:
    anim = tab5 / "tab5_anim.cpp"
    text_cpp = tab5 / "tab5_text.cpp"
    styles = tab5 / "tab5-styles.yaml"
    lvgl = tab5 / "tab5-lvgl.yaml"
    for required in (anim, text_cpp, styles, lvgl):
        if not required.is_file():
            return [f"règle 6 : fichier introuvable : {required}"]

    m_days = re.search(r"static const char\* days\[\] = \{(.*?)\};", strip_cpp_comments(anim.read_text(encoding="utf-8")), re.S)
    m_months = re.search(
        r"clock_month_short_utf8\(int month\)\s*\{.*?months\[\] = \{(.*?)\};",
        strip_cpp_comments(text_cpp.read_text(encoding="utf-8")),
        re.S,
    )
    glyphs = font_glyphs(styles).get("roboto_45")
    m_initial = re.search(r"id: lbl_date, text: \"([^\"]*)\"", lvgl.read_text(encoding="utf-8"))
    if not (m_days and m_months and glyphs and m_initial):
        return ["règle 6 : table des jours, des mois, glyphes de roboto_45 ou texte initial de lbl_date introuvable"]

    needed = set(" 0123456789") | set(m_initial.group(1))
    for word in _c_literals(m_days.group(1)) + _c_literals(m_months.group(1)):
        needed |= set(word)
    missing = sorted(needed - glyphs)
    if missing:
        return [
            f"tab5-styles.yaml : roboto_45 sans glyphe pour {''.join(missing)!r} — lbl_date "
            f"l'afficherait vide (ajouter ces caractères à `glyphs:`)"
        ]
    return []


RE_MDI = re.compile(r"\\U000(F[0-9A-Fa-f]{4})")
RE_TEXT_FONT = re.compile(r"\btext_font:\s*[\"']?(\w+)")
RE_SET_TEXT_ID = re.compile(r"lv_label_set_text\(\s*id\((\w+)\)")
RE_INCLUDE_VARS = re.compile(r"!include\s*\{\s*file:\s*([\w./-]+)\s*,\s*vars:\s*\{(.*)\}\s*\}")
RE_CPP_FUNC = re.compile(r"^[A-Za-z_][\w:<>,*&\s]*?\b(\w+)\s*\([^;]*$")

# Icônes posées par du code sur un widget reçu en paramètre : la police ne se lit
# pas à l'appel. (fichier, fonction C++ ou `id:` YAML qui englobe la lambda) →
# widgets qui les affichent (motifs fnmatch sur les ids YAML). Une icône posée
# depuis une fonction absente d'ici fait échouer la règle 7 : l'ajouter.
MDI_CODE_TARGETS: dict[tuple[str, str], tuple[str, ...]] = {
    ("alarm_render.cpp", "alarm_render_settings"): ("icon_alarm_enable",),
    ("alarm_render.cpp", "alarm_ring_show"): ("icon_alarm_ring",),
    ("alarm_render.cpp", "alarm_render_status_icon"): ("icon_alarm_status",),
    ("tab5_calendar.cpp", "cal_detail_type_style"): ("cal_det_icon_*",),
    ("tab5_cards.cpp", "update_light_card_ui"): ("icon_card_light_j*",),
    ("tab5_console.cpp", "ui_sync_mute_icons"): ("icon_mute", "icon_assist_mute"),
    ("tab5_services.cpp", "update_volet_ui"): ("icon_card_shutter_arrow", "icon_card_shutter1"),
    ("tab5_services.cpp", "parse_and_update_vigilance"): ("alerte_slot_*",),
    ("tab5_services.cpp", "update_rain_predict_icon_ui"): ("icon_rain_predict",),
    ("tab5-sensors-domotique.yaml", "moisture_1"): ("icon_pot_s*",),
}


def _key_at(line: str) -> tuple[int, bool, str] | None:
    """(colonne de la clé, précédée d'un tiret de liste, nom) d'une ligne `clé:`."""
    m = RE_YAML_KEY.match(line)
    if m is None:
        return None
    return len(m.group(1)), "-" in m.group(1), m.group(2)


def _mapping_siblings(lines: list[str], i: int) -> dict[str, str]:
    """Clés sœurs (même mapping YAML en bloc) de la ligne i, avec leur valeur brute."""
    here = _key_at(lines[i])
    if here is None:
        return {}
    col, dashed, _ = here
    out: dict[str, str] = {}
    j = i
    while j >= 0:
        k = _key_at(lines[j])
        if k is not None:
            if k[0] < col:
                break
            if k[0] == col:
                out.setdefault(k[2], lines[j].split(":", 1)[1].strip())
                if k[1]:
                    break
        j -= 1
    for line in lines[i + 1:]:
        k = _key_at(line)
        if k is None:
            continue
        if k[0] < col or (k[0] == col and k[1]):
            break
        if k[0] == col:
            out.setdefault(k[2], line.split(":", 1)[1].strip())
    return out


def _yaml_lines(path: Path) -> list[str]:
    """Lignes d'un YAML, commentaires pleins vidés (numéros de ligne conservés)."""
    return ["" if l.lstrip().startswith("#") else l for l in path.read_text(encoding="utf-8").splitlines()]


def widget_font_map(sources: list[Path]) -> dict[str, str]:
    """id de widget → text_font, label en ligne (`{ id: x, …, text_font: f }`) ou en bloc."""
    out: dict[str, str] = {}
    for path in sources:
        if path.suffix != ".yaml":
            continue
        lines = _yaml_lines(path)
        for i, line in enumerate(lines):
            m = re.search(r"\bid:\s*[\"']?([\w${}]+)", line)
            if m is None:
                continue
            font = RE_TEXT_FONT.search(line)
            if font is None and "{" not in line:
                sib = _mapping_siblings(lines, i)
                if sib.get("id", "").strip("\"'") == m.group(1) and "text_font" in sib:
                    font = RE_TEXT_FONT.search(f"text_font: {sib['text_font']}")
            if font is not None:
                out[m.group(1)] = font.group(1)
    return out


def _template_font(template: Path, var: str) -> str | None:
    """Police sous laquelle un gabarit !include affiche `${var}`."""
    if not template.is_file():
        return None
    lines = _yaml_lines(template)
    for i, line in enumerate(lines):
        if "${" + var + "}" not in line:
            continue
        font = RE_TEXT_FONT.search(line)
        if font is None:
            font = RE_TEXT_FONT.search(f"text_font: {_mapping_siblings(lines, i).get('text_font', '')}")
        if font is not None:
            return font.group(1)
    return None


def _cpp_lines(path: Path) -> list[str]:
    """Lignes C++ sans commentaires, numéros de ligne conservés."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), path.read_text(encoding="utf-8"), flags=re.S)
    return [l.split("//", 1)[0] for l in text.splitlines()]


def mdi_glyph_coverage(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    """Règle 7 : chaque icône MDI affichée est dans la police de son widget, et
    chaque glyphe d'une police `mdi_*` est affiché quelque part."""
    styles = tab5 / "tab5-styles.yaml"
    if not styles.is_file():
        return [f"règle 7 : fichier introuvable : {styles}"]
    fonts = font_glyphs(styles)
    sources = [p for p in firmware_sources(tab5, entry) if p != styles]
    widgets = widget_font_map(sources)
    problems: list[str] = []
    used: dict[str, set[str]] = {f: set() for f in fonts}

    def use(where: str, cp: str, font: str | None, how: str) -> None:
        if font is None:
            problems.append(f"{where} : icône U+{cp} sans police identifiable ({how}) — "
                            f"poser text_font sur le label ou déclarer le widget dans MDI_CODE_TARGETS")
        elif font not in fonts:
            problems.append(f"{where} : icône U+{cp} affichée en `{font}`, police inconnue de tab5-styles.yaml")
        else:
            used[font].add(chr(int(cp, 16)))
            if chr(int(cp, 16)) not in fonts[font]:
                problems.append(f"{where} : icône U+{cp} absente de `{font}` — elle s'afficherait vide "
                                f"(l'ajouter aux glyphes de {font} dans tab5-styles.yaml)")

    def use_targets(where: str, cps: list[str], key: tuple[str, str]) -> None:
        patterns = MDI_CODE_TARGETS.get(key)
        if patterns is None:
            for cp in cps:
                use(where, cp, None, f"posée depuis `{key[1]}`, absente de MDI_CODE_TARGETS")
            return
        for pattern in patterns:
            matched = [w for w in widgets if fnmatch.fnmatchcase(w, pattern)]
            if not matched:
                problems.append(f"MDI_CODE_TARGETS : aucun label `{pattern}` avec text_font (entrée {key} périmée ?)")
            for w in matched:
                for cp in cps:
                    use(where, cp, widgets[w], f"widget {w}")

    for path in sources:
        where_base = path.name
        if path.suffix in (".cpp", ".h"):
            fn = ""
            for n, line in enumerate(_cpp_lines(path), 1):
                m = RE_CPP_FUNC.match(line)
                if m and not line.rstrip().endswith(";"):
                    fn = m.group(1)
                cps = RE_MDI.findall(line)
                if cps:
                    use_targets(f"{where_base}:{n}", cps, (path.name, fn))
            continue

        lines = _yaml_lines(path)
        for i, line in enumerate(lines):
            cps = RE_MDI.findall(line)
            if not cps:
                continue
            where = f"{where_base}:{i + 1}"
            font = RE_TEXT_FONT.search(line)
            inc = RE_INCLUDE_VARS.search(line)
            set_text = RE_SET_TEXT_ID.search(line)
            if font is not None:
                for cp in cps:
                    use(where, cp, font.group(1), "label")
            elif inc is not None:
                template = path.parent / inc.group(1)
                for var, value in re.findall(rf"(\w+):\s*({RE_YAML_QUOTED})", inc.group(2)):
                    for cp in RE_MDI.findall(value):
                        use(where, cp, _template_font(template, var), f"gabarit {inc.group(1)}, ${{{var}}}")
            elif set_text is not None:
                w = set_text.group(1)
                for cp in cps:
                    use(where, cp, widgets.get(w), f"widget {w}")
            elif _key_at(line) and _key_at(line)[2] == "text":
                sib = _mapping_siblings(lines, i)
                f = RE_TEXT_FONT.search(f"text_font: {sib.get('text_font', '')}")
                w = sib.get("id", "").strip("\"'")
                for cp in cps:
                    use(where, cp, f.group(1) if f else widgets.get(w), "label en bloc")
            else:
                # Lambda : rattachée à l'`id:` du composant qui l'englobe.
                indent = len(line) - len(line.lstrip())
                owner = ""
                for prev in reversed(lines[:i]):
                    m = re.match(r"^(\s*)(?:-\s+)?id:\s*(\w+)", prev)
                    if m and len(m.group(1)) < indent:
                        owner = m.group(2)
                        break
                use_targets(where, cps, (path.name, owner))

    for font, declared in sorted(fonts.items()):
        if not font.startswith("mdi_"):
            continue
        dead = sorted(declared - used[font])
        if dead:
            problems.append(
                f"tab5-styles.yaml : `{font}` embarque {len(dead)} glyphe(s) jamais affiché(s) : "
                + " ".join(f"U+{ord(c):05X}" for c in dead)
            )
    return problems


def scan(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    problems: list[str] = []
    api_logic = tab5 / "tab5-api-logic.yaml"
    hardware = tab5 / "tab5-hardware.yaml"
    sensors = (tab5 / "tab5-sensors-diagnostics.yaml", tab5 / "tab5-sensors-domotique.yaml")
    globals_yaml = tab5 / "tab5-globals.yaml"
    for required in (api_logic, hardware, *sensors, globals_yaml):
        if not required.is_file():
            return [f"fichier introuvable : {required}"]

    sources = firmware_sources(tab5, entry)

    # 1. sprintf brut
    for path in sources:
        text = path.read_text(encoding="utf-8")
        text = strip_yaml_comments(text) if path.suffix == ".yaml" else strip_cpp_comments(text)
        for lineno, line in enumerate(text.splitlines(), 1):
            if RE_SPRINTF.search(line):
                problems.append(f"{path.name}:{lineno} : sprintf brut — utiliser snprintf(buf, sizeof(buf), …)")

    # 2. lv_* dans le contrat API, le fichier matériel et les deux fichiers sensors
    for path in (api_logic, hardware, *sensors):
        allowed = LV_ALLOWED.get(path.name, set())
        text = strip_yaml_comments(path.read_text(encoding="utf-8"))
        for lineno, line in enumerate(text.splitlines(), 1):
            for m in RE_LV_CALL.finditer(line):
                if m.group(1) not in allowed:
                    problems.append(
                        f"{path.name}:{lineno} : {m.group(1)}() — logique LVGL interdite ici, "
                        f"la déplacer dans tab5_custom.cpp (ADR-0006)"
                    )

    # 3. globals orphelins
    corpus: list[tuple[Path, str]] = []
    for path in sources:
        text = path.read_text(encoding="utf-8")
        corpus.append((path, strip_yaml_comments(text) if path.suffix == ".yaml" else text))
    for name in globals_defined(globals_yaml):
        pattern = re.compile(rf"\bid\(\s*{re.escape(name)}\s*\)|\bid:\s*{re.escape(name)}\b")
        used = False
        for path, text in corpus:
            for line in text.splitlines():
                if path == globals_yaml and re.fullmatch(rf"  - id: {re.escape(name)}\s*", line):
                    continue  # la définition elle-même
                if pattern.search(line):
                    used = True
                    break
            if used:
                break
        if not used:
            problems.append(f"{globals_yaml.name} : global `{name}` défini mais jamais référencé (id({name}) / id: {name}) — code mort")

    # 4. entité HA en dur dans un YAML
    for path in sources:
        if path.suffix != ".yaml":
            continue
        text = strip_yaml_comments(path.read_text(encoding="utf-8"))
        for lineno, line in enumerate(text.splitlines(), 1):
            m = RE_HA_ENTITY_LITERAL.match(line)
            if m:
                problems.append(
                    f"{path.name}:{lineno} : entité HA en dur `{m.group(1)}` — passer par une "
                    f"substitution de user_entities.yaml (${{entity_…}})"
                )

    # 5. métadonnées des actions du contrat API
    problems += api_action_metadata(api_logic)

    # 6. glyphes de la date (roboto_45 réduite)
    problems += date_glyph_coverage(tab5)

    # 7. icônes MDI : couvertes par la police de leur widget, aucun glyphe mort
    problems += mdi_glyph_coverage(tab5, entry)

    return problems


def main() -> int:
    problems = scan()
    if problems:
        print("[KO] règles de code Tab5 — écarts :")
        for p in problems:
            print("  -", p)
        return 1
    print(
        "[OK] règles de code Tab5 : snprintf partout, api-logic et hardware sans LVGL, "
        "aucun global orphelin, aucune entité HA en dur, actions API décrites, "
        "glyphes de la date couverts, icônes MDI couvertes sans glyphe mort"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
