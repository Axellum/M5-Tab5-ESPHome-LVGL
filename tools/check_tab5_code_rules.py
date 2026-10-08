#!/usr/bin/env python3
"""Règles de code du firmware Tab5, jouées à chaque `pytest` (audit du 06/09/2026,
§4.1 points 1, 4, 15 et §4.2 point 17 ; ADR-0006 ; lot L6 de l'audit du 07/10/2026
pour les règles 9 à 14). Quatorze règles, toutes falsifiables sur le dépôt réel
(numérotation propre à ce script, distincte des règles d'AGENTS.md) :

  1. **`snprintf` partout** : aucun `sprintf(` brut dans `Tab5/*.cpp`, `*.h`,
     `*.yaml` ni `tab5-ha-hmi.yaml`. Un futur `%s` sur un buffer de 16 octets ne
     doit pas pouvoir déborder en silence.
  2. **Aucune logique LVGL dans `tab5-api-logic.yaml`, `tab5-hardware.yaml` ni la
     pile vocale de `tab5-assist.yaml`** (tout ce qui précède son `script:`) :
     les services et les callbacks (voice_assistant, micro_wake_word, online_image)
     résolvent les `id()` et appellent `tab5_custom.cpp`. Dans le contrat API,
     seul `lv_obj_has_flag` (lecture pure, dans une condition) est toléré ; dans
     le fichier matériel et la pile vocale, rien.
  3. **Aucun global orphelin** dans `tab5-globals.yaml` : chaque `- id:` du bloc
     `globals:` doit être lu ou écrit quelque part (`id(x)` dans une lambda,
     `id: x` dans une action `globals.set` / `globals.increment`…). Un global
     que personne ne référence est du code mort qui trompe le lecteur.
  4. **Aucune entité Home Assistant en dur** dans un YAML du firmware : toute
     valeur `entity_id:` littérale (`domaine.objet`) doit être une substitution
     de `user_entities.yaml` (`${entity_…}`) ou un `!lambda`. Les entités que la
     tablette expose elle-même (`assist_satellite.*`, `media_player.*`) sont
     dérivées de son nom dans HA : un renommage cassait l'interruption vocale et
     l'annonce des rendez-vous sans aucune erreur (audit §4.1 point 15). Depuis
     l'ADR-0025, le firmware n'en nomme plus aucune : il émet des événements et HA
     retrouve ces entités par l'appareil (tests/test_actions_ha.py).
  5. **Métadonnées sur chaque action du contrat API** (`tab5-api-logic.yaml`) :
     toute action porte une `description:`, et chaque variable la forme longue
     `type:` + `description:` + `example:` (ESPHome 2026.9.0, amont #18881). Ce
     sont elles que Home Assistant affiche dans « Outils de développement →
     Actions » : sans elles, un champ n'est qu'une case de texte sans indice sur
     le format du payload, toujours sérialisé à la main ici.
  6. **Glyphes de la date** (audit du 25/09/2026, lot 4) : la police de `lbl_date`
     (lue sur son `text_font:`, `roboto_45_b` depuis l'essai D8 du 26/09/2026) doit
     couvrir ses caractères. Chaque caractère des jours de `update_clock_date_ui()`
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
  8. **Couleurs de l'interface par la palette** (lot 1 des thèmes, 04/10/2026,
     ADR-0029) : ESPHome écrit une couleur YAML en dur dans main.cpp, un thème ne
     pourrait pas la changer. Hors jeux, une propriété couleur LVGL (`text_color:`,
     `bg_color:`…) ne vaut qu'une lambda (qui lit `UIColor.X`) ou une variable de
     gabarit : un widget prend sa couleur par un style de rôle (`styles:
     style_text_dim`). Les couleurs déclarées dans `color:` (celles des jeux) ne
     servent qu'aux jeux, et les jeux ne lisent pas la palette active (`UIColor.`) :
     ils restent sombres (`PALETTE_SOMBRE.X`). Hors jeux, ni `PALETTE_SOMBRE` (sauf
     tab5_tokens.h qui la définit) ni couleur littérale (`lv_color_hex(0x…)`,
     `lv_color_make(…)`) dans le code : ce serait contourner le thème.
  9. **Pas de logique LVGL dans un YAML** (YML-8) : la règle 2 étendue à tous les YAML
     du firmware. Les appels `lv_*` du 08/10/2026 sont listés fichier par fichier
     (`LV_YAML_PLAFONDS`, plafonds exacts) : aucun nouveau. `lv_color_hex()` reste
     libre ; `tab5-themes.yaml` (généré, pose les styles) est hors règle.
 10. **Pas de `static` modifiable dans une lambda YAML** (règle 3 d'AGENTS.md) : un état
     partagé entre handlers va dans un `globals:`. Les cas d'un seul handler sont
     listés (`STATIC_LAMBDA_PERMIS`).
 11. **Pas de copie de chaîne dans un chemin chaud** (règle 4 d'AGENTS.md, partie sûre) :
     ni `std::string` par valeur ni `to_string()` dans un `on_value:` / `on_change:`,
     aucun paramètre `std::string` par valeur dans un en-tête de Tab5/.
 12. **Chaque fonction de `tab5_custom.h` a un appelant hors de son fichier** (CPP-4) :
     une lambda YAML (firmware ou tablette virtuelle) ou une autre unité C++.
     Exceptions du 08/10/2026 : `PUBLIQUES_SANS_APPELANT`.
 13. **Conventions du nouveau code** (Tab5/README.md) : `nullptr` jamais `NULL` ; hors
     jeux, tag de journal `tab5.<module>`, y compris celui donné à `payload_refuse()` /
     `payload_trop_long()` (seuls relais d'un tag reçu : `TAGS_RELAYES`).
 14. **Accents des textes de l'écran** : aucun mot de `MOTS_SANS_ACCENT` (« Ecran »,
     « Temperature », « Etat »…) dans un texte de `tr()` ou du YAML des écrans.
  La règle 5 d'AGENTS.md (widget répété 3 fois → builder ou gabarit) n'est pas
  vérifiée : « le même widget » ne se reconnaît pas sûrement dans le YAML.

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
# Règle 8 : sources des jeux (palette sombre fixe) et propriétés couleur de LVGL.
RE_GAME_SOURCE = re.compile(
    r"(_game\.(yaml|cpp|h)|^game_common\.h|^game_selector\.yaml|^arcade_card\.yaml|^tab5-arcade\.yaml)$"
)
LV_COLOR_PROPS = ("text_color", "bg_color", "bg_grad_color", "border_color", "outline_color",
                  "shadow_color", "arc_color", "line_color", "image_recolor")
RE_LV_COLOR_PROP = re.compile(r"(?<![\w.])(" + "|".join(LV_COLOR_PROPS) + r"):[ \t]*([^,}\s][^,}]*)")
RE_YAML_COLOR_DECL = re.compile(r"^  - id: (color_\w+)\s*$", re.M)
# Contournements du thème hors jeux : une couleur littérale dans du code, la palette sombre fixe.
RE_LV_COLOR_LITERAL = re.compile(r"\blv_color_(?:hex\s*\(\s*0x|hex3\s*\(|make\s*\(\s*\d)")


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
    core = tab5 / "tab5_core.cpp"      # fr_day_short_utf8 + clock_month_short_utf8 (lot 8d)
    styles = tab5 / "tab5-styles.yaml"
    lvgl = tab5 / "tab5-lvgl.yaml"
    for required in (core, styles, lvgl):
        if not required.is_file():
            return [f"règle 6 : fichier introuvable : {required}"]

    core_src = strip_cpp_comments(core.read_text(encoding="utf-8"))
    m_days = re.search(r"fr_day_short_utf8\(int wday\)\s*\{.*?days\[\] = \{(.*?)\};", core_src, re.S)
    m_months = re.search(r"clock_month_short_utf8\(int month\)\s*\{.*?months\[\] = \{(.*?)\};", core_src, re.S)
    m_initial = re.search(r"id: lbl_date, text: \"([^\"]*)\"([^}\n]*)", lvgl.read_text(encoding="utf-8"))
    font = "?"
    if m_initial:
        # Police posée sur le label, ou par un de ses styles (style_police_date depuis
        # les thèmes, lot 3 : celle de l'état compilé ; un thème qui la change dessine
        # les glyphes absents de sa police avec elle, tab5_theme.cpp).
        m_font = re.search(r"text_font: (\w+)", m_initial.group(2))
        if m_font:
            font = m_font.group(1)
        else:
            m_styles = re.search(r"styles: \[?([\w, ]+)\]?", m_initial.group(2))
            texte_styles = styles.read_text(encoding="utf-8")
            for sid in (m_styles.group(1).replace(" ", "").split(",") if m_styles else []):
                m_def = re.search(rf"- id: {sid}\n(?:[ \t]+\w+:.*\n)*?[ \t]+text_font: (\w+)", texte_styles)
                if m_def:
                    font = m_def.group(1)
    glyphs = font_glyphs(styles).get(font)
    if not (m_days and m_months and glyphs and m_initial):
        return [f"règle 6 : table des jours, des mois, glyphes de {font} ou texte initial / police de lbl_date introuvable"]

    needed = set(" 0123456789") | set(m_initial.group(1))
    for word in _c_literals(m_days.group(1)) + _c_literals(m_months.group(1)):
        needed |= set(word)
    missing = sorted(needed - glyphs)
    if missing:
        return [
            f"tab5-styles.yaml : {font} sans glyphe pour {''.join(missing)!r} — lbl_date "
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
    ("tab5_console.cpp", "ui_sync_mute_icon"): ("icon_assist_mute",),
    # Popup Énergie (ADR-0028) : icônes des quatre cartes (energie_carte.yaml, mdi_font_45).
    ("tab5_energie.cpp", "glyphe_carte"): ("energie_icone_*",),
    # Pièces (ADR-0023) : icônes 3.1 du mode héritage (cartes du mode HA, épaules gauches
    # de l'accueil), ampoule et flèche du volet sur les épaules droites de toutes les tuiles.
    # Le popup Maison (ADR-0037) montre la carte de chaque tuile, héritage compris (maison_icone_*, mdi_font_32).
    ("tab5_tuiles.cpp", "heritage_glyphe_carte"): ("icon_sw?", "maison_icone_*"),
    ("tab5_tuiles.cpp", "heritage_glyphe_selecteur"): ("icon_light_sel_*",),
    ("tab5_tuiles.cpp", "heritage_glyphe_epaule"): (
        "icon_card_pc", "icon_card_shutter1", "icon_card_lit_j2", "icon_card_salon_j3", "icon_card_led_j4"),
    ("tab5_tuiles.cpp", "glyphe_ampoule"): (
        "icon_card_droite_j0", "icon_card_shutter_arrow", "icon_card_light_j*", "icon_card_h*_d"),
    ("tab5_tuiles.cpp", "glyphe_fleche"): (
        "icon_card_droite_j0", "icon_card_shutter_arrow", "icon_card_light_j*", "icon_card_h*_d"),
    # Popup d'un appareil (06/10/2026) : icône du grand bouton (marche / arrêt, lecture).
    ("tab5_tuiles_popups.cpp", "glyphe_commande"): ("appareil_commande_icone",),
    # Roue d'actions rapides (ADR-0036) : icônes de ses deux anneaux (roue_bouton.yaml,
    # roue_choix.yaml, mdi_font_36).
    ("tab5_roue.cpp", "glyphe_roue"): ("roue_bouton_*_icone", "roue_choix_*_icone"),
    ("tab5_services.cpp", "parse_and_update_vigilance"): ("alerte_slot_*",),
    ("tab5_services.cpp", "update_rain_predict_icon_ui"): ("icon_rain_predict",),
    ("tab5_zones.cpp", "zones_apply_ui"): ("icon_serre",),
    # Batterie de la tablette dans le bandeau d'état (04/10/2026) et sur la ligne
    # « Batterie » de la console système (06/10/2026, mdi_font_32).
    ("tab5_zones.cpp", "batterie_glyphe"): ("icon_batterie", "lbl_sys_batterie_icone"),
    # Production solaire dans le bandeau d'état (04/10/2026).
    ("tab5_zones.cpp", "solaire_glyphe"): ("icon_solaire",),
    # Mini icônes des trois boutons du haut (07/10/2026) : l'écran qu'ouvre leur appui long.
    ("tab5_zones.cpp", "mini_glyphe"): ("icon_mini_*",),
    # Palette des tuiles de pièce (ADR-0023) : table au niveau du fichier, d'où la fonction vide.
    # Ses glyphes s'affichent sur les cartes du mode HA (icon_sw*, mdi_font_70), dans les
    # épaules des tuiles (icon_card_*, mdi_font_32) et dans le sélecteur du popup lumière
    # (icon_light_sel_*, mdi_font_45) : tools/gen_tuiles_icones.py les écrit dans ces trois
    # polices (POLICES), à garder d'accord avec cette ligne. La rangée sous l'horloge
    # (ADR-0031, rangee_icone_*, déclarés en mdi_font_32) passe ses icônes en 45 ou 70 px
    # selon la ligne (tab5_rangee.cpp) : les mêmes trois polices. Le popup d'un appareil
    # (appareil_icone, mdi_font_70, 06/10/2026) montre l'icône de sa tuile comme les cartes.
    # La tuile − / + (ADR-0033, tab5_reglables.cpp) montre la même palette en mdi_font_45 :
    # reglable_icone sur la carte clim, reglable_ligne_*_icone dans sa liste.
    # Le popup Maison (ADR-0037) : la même palette en mdi_font_32, maison_icone_* (une ligne par tuile).
    # Le moyeu de la roue d'actions rapides (ADR-0036) : l'icône de la tuile en mdi_font_45.
    ("tab5_tuiles_icones.h", ""): (
        "icon_sw?",
        "icon_card_*",
        "icon_light_sel_*",
        "rangee_icone_*",
        "appareil_icone",
        "reglable_icone",
        "reglable_ligne_*_icone",
        "maison_icone_*",
        "roue_moyeu_icone",
    ),
    # Icônes des pots sur la ligne des plantes (script tab5_pots_maj, 08/10/2026).
    ("tab5_rangee.cpp", "pots_humidite_maj"): ("icon_pot_s*",),
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


def _widget_fonts(lines: list[str]) -> list[tuple[str, str]]:
    """(id brut, text_font) des labels d'un YAML, en ligne (`{ id: x, …, text_font: f }`) ou en bloc."""
    out: list[tuple[str, str]] = []
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
            out.append((m.group(1), font.group(1)))
    return out


RE_VAR_SIMPLE = re.compile(r"(\w+):\s*(\"[^\"]*\"|'[^']*'|[\w.-]+)")


def widget_font_map(sources: list[Path]) -> dict[str, str]:
    """id de widget → text_font. Un gabarit `!include { file, vars }` est aussi déplié avec
    les vars de chaque inclusion (`id: "icon_sw${n}"` + `n: "0"` → `icon_sw0`), pour que
    MDI_CODE_TARGETS nomme les ids réels (08/10/2026, cartes du mode HA et tuiles météo)."""
    out: dict[str, str] = {}
    for path in sources:
        if path.suffix != ".yaml":
            continue
        lines = _yaml_lines(path)
        for wid, font in _widget_fonts(lines):
            out[wid] = font
        for line in lines:
            inc = RE_INCLUDE_VARS.search(line)
            if inc is None:
                continue
            template = path.parent / inc.group(1)
            if not template.is_file():
                continue
            valeurs = {k: v.strip("\"'") for k, v in RE_VAR_SIMPLE.findall(inc.group(2))}
            for wid, font in _widget_fonts(_yaml_lines(template)):
                if "${" not in wid:
                    continue
                reel = re.sub(r"\$\{(\w+)\}", lambda m: valeurs.get(m.group(1), m.group(0)), wid)
                if "${" not in reel:
                    out[reel] = font
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


def palette_colors(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    """Règle 8 : les couleurs de l'interface passent par la palette (`Palette`, tab5_tokens.h)."""
    problems: list[str] = []
    declared = set(RE_YAML_COLOR_DECL.findall((tab5 / "tab5-styles.yaml").read_text(encoding="utf-8")))
    for path in firmware_sources(tab5, entry):
        game = bool(RE_GAME_SOURCE.search(path.name))
        text = path.read_text(encoding="utf-8")
        if not game:
            code = strip_cpp_comments(strip_yaml_comments(text) if path.suffix == ".yaml" else text)
            # tab5_themes_data.h (généré) : THEMES[0].sombre reprend PALETTE_SOMBRE.
            if path.name not in ("tab5_tokens.h", "tab5_themes_data.h") and "PALETTE_SOMBRE" in code:
                problems.append(
                    f"{path.name} : `PALETTE_SOMBRE` hors jeux — l'interface lit la palette active "
                    f"(`UIColor.X`), sinon le thème ne la change pas (ADR-0029)"
                )
            for m in RE_LV_COLOR_LITERAL.finditer(code):
                problems.append(
                    f"{path.name} : `{m.group(0)}…` — couleur littérale, ajouter un rôle à "
                    f"`struct Palette` et lire `UIColor.X` (ADR-0029)"
                )
        if path.suffix != ".yaml":
            if game and "UIColor." in strip_cpp_comments(text):
                problems.append(
                    f"{path.name} : un jeu lit la palette active (`UIColor.`) — il reste sombre, "
                    f"lire `PALETTE_SOMBRE.X` (ADR-0029)"
                )
            continue
        if game:
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            if line.lstrip().startswith("#") or RE_YAML_COLOR_DECL.match(line):
                continue
            for m in RE_LV_COLOR_PROP.finditer(line):
                value = m.group(2).strip().strip("\"'")
                if not value.startswith(("!lambda", "${")):
                    problems.append(
                        f"{path.name}:{lineno} : `{m.group(1)}: {value}` — couleur figée à la compilation, "
                        f"prendre un style de rôle (`styles: style_text_dim`, tab5-styles.yaml) ou lire "
                        f"`UIColor.X` dans une lambda (ADR-0029)"
                    )
            for color in re.findall(r"\bcolor_\w+", line):
                if color in declared:
                    problems.append(
                        f"{path.name}:{lineno} : `{color}` est une couleur de jeu (figée) — l'interface "
                        f"lit la palette (ADR-0029)"
                    )
    return problems


# ─── Règles 9 à 14 : garde-fous du lot L6 (audit du 07/10/2026, §4.2 et §4.4) ─────────
# Chacune empêche les NOUVEAUX écarts sans forcer à déplacer le code existant : les cas
# légitimes ou hérités sont listés ici, un par un, avec leur raison. Une liste sert de
# plafond exact : un appel de plus échoue (« nouvel écart »), un appel de moins aussi
# (« abaisser le plafond »), pour que la liste ne garde jamais un cas disparu.


def _plafonds(compte: dict[str, int], plafonds: dict[str, int], ou: str, liste: str,
              trop: str, lignes: dict[str, int] | None = None) -> list[str]:
    """Écarts entre un décompte et ses plafonds (exacts) ; `lignes` : première ligne de chaque clé."""
    problems = []
    for cle, n in sorted(compte.items()):
        p = plafonds.get(cle, 0)
        if n > p:
            ligne = f":{lignes[cle]}" if lignes and cle in lignes else ""
            problems.append(f"{ou}{ligne} : {cle} ×{n} (toléré ×{p}) — {trop}")
    for cle, p in sorted(plafonds.items()):
        n = compte.get(cle, 0)
        if n < p:
            problems.append(f"{liste} : {ou} n'a plus que {n} {cle} (plafond {p}) — abaisser le plafond")
    return problems


# Règle 9 (YML-8) : pas de logique LVGL dans une lambda YAML. La règle 2 ne visait que le
# contrat API, le matériel et les deux fichiers sensors ; celle-ci couvre tous les YAML
# du firmware. Le YAML pose les widgets ; ce qui calcule ou modifie l'écran passe par une
# fonction C++ nommée (tab5_custom.h), plus facile à tester, à partager et à relire.
# Toléré partout : lv_color_hex(), conversion d'une couleur de la palette dans une
# propriété `!lambda` (la couleur littérale, elle, est refusée par la règle 8).
LV_YAML_PURS = {"lv_color_hex"}
# Fichiers hors règle 9 : leur rôle est de poser des styles LVGL.
LV_YAML_EXEMPTS = {
    # Généré par tools/gen_themes.py (bloc `# >>> styles`) : repeint les styles partagés
    # au changement de thème (ADR-0029). tests/test_themes.py le tient.
    "tab5-themes.yaml",
}
# Appels existants le 08/10/2026, par fichier : plafonds exacts. Ne pas en ajouter :
# écrire une fonction C++ et l'appeler depuis la lambda.
LV_YAML_PLAFONDS: dict[str, dict[str, int]] = {
    # Popup Assistant : libellé « Ok Nabu : ON/OFF » et sa couleur, au basculement.
    "tab5-assist.yaml": {"lv_label_set_text": 1, "lv_obj_set_style_text_color": 1},
    # Calendrier : fermeture du détail du jour (4 gestes).
    "tab5-calendar.yaml": {"lv_obj_add_flag": 4},
    # Ouverture d'un écran depuis HA : popup déjà affiché ? (lecture seule).
    "tab5-navigation.yaml": {"lv_obj_has_flag": 1},
    # Geste de balayage de la page : point et direction lus sur l'entrée LVGL
    # (le traitement est dans handle_swipe_gesture(), tab5_central.cpp).
    "tab5-lvgl.yaml": {"lv_indev_active": 1, "lv_indev_get_point": 1, "lv_indev_get_gesture_dir": 1},
    # Valeur de l'arc de luminosité lue pour l'envoi (la console au premier plan est partie
    # le 08/10/2026 : c'est une page des Réglages).
    "tab5-scripts.yaml": {"lv_arc_get_value": 1},
    # Page Système des Réglages : les deux écrans de confirmation (afficher, masquer,
    # premier plan) et l'état « Redémarrage... » de HA. Candidat à un `console_confirmer()`
    # en C++. Voile et croix ne les masquent plus ici (reglages_fermer, 08/10/2026).
    "console_sys.yaml": {"lv_obj_add_flag": 6, "lv_obj_remove_flag": 2, "lv_obj_move_to_index": 2,
                         "lv_label_set_text": 1, "lv_obj_set_style_text_color": 1},
    # Popup lumière : boutons de niveau (arc + libellé) et libellé du pourcentage.
    "light_level_btn.yaml": {"lv_arc_set_value": 1, "lv_label_set_text": 1},
    "light_popup.yaml": {"lv_label_set_text": 1},
    # on_boot du point d'entrée (intouchable sans accord d'Axel) : l'écran actif est
    # passé à apply_pressed_scale_to_tree().
    "tab5-ha-hmi.yaml": {"lv_screen_active": 1},
}


def lvgl_yaml(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    """Règle 9 : aucun nouvel appel lv_* dans les lambdas YAML du firmware."""
    problems: list[str] = []
    vus = set()
    for path in firmware_sources(tab5, entry):
        if path.suffix != ".yaml" or path.name in LV_ALLOWED or path.name in LV_YAML_EXEMPTS:
            continue
        vus.add(path.name)
        compte: dict[str, int] = {}
        premiere: dict[str, int] = {}
        for lineno, line in enumerate(_yaml_lines(path), 1):
            for m in RE_LV_CALL.finditer(line):
                fn = m.group(1)
                if fn in LV_YAML_PURS:
                    continue
                compte[f"{fn}()"] = compte.get(f"{fn}()", 0) + 1
                premiere.setdefault(f"{fn}()", lineno)
        plafonds = {f"{k}()": v for k, v in LV_YAML_PLAFONDS.get(path.name, {}).items()}
        problems += _plafonds(compte, plafonds, path.name, "LV_YAML_PLAFONDS",
                              "logique LVGL dans le YAML : la mettre dans une fonction C++ "
                              "déclarée dans tab5_custom.h et l'appeler (ADR-0006)", premiere)
    problems += [f"LV_YAML_PLAFONDS : `{nom}` n'est plus un YAML du firmware — retirer l'entrée"
                 for nom in sorted(set(LV_YAML_PLAFONDS) - vus)]
    return problems


# Règle 10 (règle 3 d'AGENTS.md) : pas de `static` modifiable dans une lambda YAML. Une
# variable `static` d'une lambda n'est visible que d'elle : partager un état entre deux
# handlers (bug `reboot_armed` du 05/07/2026) demande un `globals:`. Les cas ci-dessous
# gardent un état propre à UN seul handler (vérifié le 08/10/2026) : plafonds exacts.
# Les constantes (`static const`, `static constexpr`) restent libres.
RE_STATIC_MUTABLE = re.compile(r"\bstatic\s+(?!const\b|constexpr\b)[\w:]+(?:<[^<>]*>)?[\s*&]+(\w+)\s*[=;({\[]")
STATIC_LAMBDA_PERMIS: dict[str, dict[str, int]] = {
    # « Prochain réveil » et « Prochain rendez-vous » : dernière valeur publiée (pas de
    # republication identique à HA), une par text_sensor.
    "tab5-alarm.yaml": {"last": 2},
    # « Écran courant » : dernière valeur publiée.
    "tab5-navigation.yaml": {"last": 1},
    # Version du C6 : nombre d'essais de lecture (3 au plus).
    "tab5-sensors-diagnostics.yaml": {"essais": 1},
}


def static_lambdas(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    """Règle 10 : aucun nouveau `static` modifiable dans une lambda YAML."""
    problems: list[str] = []
    vus = set()
    for path in firmware_sources(tab5, entry):
        if path.suffix != ".yaml":
            continue
        vus.add(path.name)
        compte: dict[str, int] = {}
        premiere: dict[str, int] = {}
        for lineno, line in enumerate(_yaml_lines(path), 1):
            for m in RE_STATIC_MUTABLE.finditer(line.split("//", 1)[0]):
                compte[m.group(1)] = compte.get(m.group(1), 0) + 1
                premiere.setdefault(m.group(1), lineno)
        problems += _plafonds(compte, STATIC_LAMBDA_PERMIS.get(path.name, {}), path.name,
                              "STATIC_LAMBDA_PERMIS",
                              "`static` modifiable dans une lambda : un état partagé entre "
                              "handlers va dans un `globals:` (règle 3 d'AGENTS.md)", premiere)
    problems += [f"STATIC_LAMBDA_PERMIS : `{nom}` n'est plus un YAML du firmware — retirer l'entrée"
                 for nom in sorted(set(STATIC_LAMBDA_PERMIS) - vus)]
    return problems


# Règle 11 (règle 4 d'AGENTS.md, partie vérifiable) : pas de copie de chaîne dans un
# chemin chaud. Deux formes sûres à détecter :
#  - dans un `on_value:` / `on_change:` YAML (capteur, curseur : appelé à chaque valeur),
#    ni `std::string` par valeur ni `to_string()` ; `const std::string &` reste permis ;
#  - aucune fonction déclarée dans un en-tête de Tab5/ ne prend un `std::string` par
#    valeur (copie à chaque appel) : `const std::string&`, ou `const char*` + longueur.
# Non vérifié : un appel fréquent hors de ces blocs (script appelé par un interval…),
# laissé à la relecture.
RE_HOT_KEY = re.compile(r"^(\s*)(?:-\s+)?(on_value|on_change):")
RE_STRING_COPIE = re.compile(r"\bto_string\s*\(|(?<!const )\bstd::string\b(?!\s*&)")
RE_PARAM_STRING_VALEUR = re.compile(r"[(,]\s*(?:const\s+)?std::string\s+\w+\s*[,)=]")


def chemins_chauds(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    """Règle 11 : ni copie de std::string dans un on_value/on_change, ni paramètre par valeur."""
    problems: list[str] = []
    for path in firmware_sources(tab5, entry):
        if path.suffix == ".h":
            for lineno, line in enumerate(_cpp_lines(path), 1):
                if RE_PARAM_STRING_VALEUR.search(line):
                    problems.append(f"{path.name}:{lineno} : `std::string` par valeur en paramètre — "
                                    f"`const std::string&` (règle 4 d'AGENTS.md)")
            continue
        if path.suffix != ".yaml":
            continue
        lines = _yaml_lines(path)
        for i, line in enumerate(lines):
            m = RE_HOT_KEY.match(line)
            if m is None:
                continue
            indent = len(m.group(1))
            for j in range(i + 1, len(lines)):
                s = lines[j]
                if s.strip() and len(s) - len(s.lstrip()) <= indent:
                    break
                if RE_STRING_COPIE.search(s.split("//", 1)[0]):
                    problems.append(f"{path.name}:{j + 1} : copie de chaîne dans un `{m.group(2)}:` "
                                    f"(appelé à chaque valeur) — `const std::string&` ou buffer "
                                    f"snprintf (règle 4 d'AGENTS.md)")
    return problems


# Règle 12 (CPP-4 / YML-11) : chaque fonction déclarée dans tab5_custom.h a un appelant
# hors de son fichier : une lambda YAML (firmware, ou tablette virtuelle du rendu hors
# tablette : tab5-rendu-host.yaml, Tab5/rendu/), ou une autre unité C++. Une fonction
# appelée par son seul fichier n'a rien à faire dans l'en-tête public : `static` (ou
# namespace anonyme) dans son unité, ou tab5_internal.h si une autre unité la veut.
# Un gabarit `!include` qui appelle `${var}_suite(` (vue_btn.yaml : `${prefixe}_choisir_vue`)
# compte pour chaque valeur passée à `var:` par ses includes (energie, historique).
# Exceptions du 08/10/2026 : sans appelant hors de leur fichier. Ne pas en ajouter ; les
# sortir de l'en-tête quand un lot touche déjà leur fichier (aucun déplacement forcé).
# (Six autres, appelées seulement par une autre unité C++, passent la règle mais iraient
# mieux dans tab5_internal.h : animate_crossfade_layers, update_central_forecast_page_ui,
# moisture_slots_refresh, zone_tuile_absente, zones_pots_presents, central_planning_set_off.)
PUBLIQUES_SANS_APPELANT = {
    "update_meteo_icon",        # tab5_forecast.cpp : dessin d'une tuile météo
    "clim_eco_actif",           # tab5_clim.cpp : état des boutons de la clim (lus aussi par tests/test_clim.py)
    "clim_silence_actif",
    "clim_oscillation_actif",
    "clim_preset_actif",
    "solaire_present",          # tab5_zones.cpp : production solaire connue
    "ecran_disponible",         # tab5_zones.cpp : écran disponible pour un appui long
    "boutons_haut_apply_ui",    # tab5_zones.cpp : mini icônes des trois boutons du haut
    "tuile_titre_appui",        # tab5_tuiles.cpp : appui sur le titre de la carte centrale
}
RE_CPP_DEF = re.compile(r"^[A-Za-z_][\w:<>,\s*&]*?[\s*&](\w+)\s*\([^;{}]*\)\s*(?:const\s*)?\{", re.M)
RE_CPP_PROTO = re.compile(r"^[A-Za-z_][\w:<>,\s*&]*?[\s*&](\w+)\s*\([^;{}]*\)\s*(?:const\s*)?;", re.M)
RE_GABARIT_APPEL = re.compile(r"\$\{(\w+)\}(\w*)\s*\(")


def fonctions_publiques(header: Path) -> list[str]:
    """Fonctions déclarées dans `header` hors struct/enum (espaces de noms compris)."""
    text = strip_cpp_comments(header.read_text(encoding="utf-8"))
    text = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
    text = "\n".join(l for l in text.splitlines() if not l.lstrip().startswith("#"))
    noms: list[str] = []
    pile: list[bool] = []  # True = espace de noms
    stmt = ""
    for ch in text:
        if ch == "{":
            pile.append(bool(re.search(r"\bnamespace\s+\w*\s*$", " ".join(stmt.split()))))
            stmt = ""
        elif ch == "}":
            if pile:
                pile.pop()
            stmt = ""
        elif ch == ";":
            s = " ".join(stmt.split())
            if all(pile) and not s.startswith(("using", "typedef", "extern ")):
                m = re.match(r"^(?:inline\s+|static\s+|constexpr\s+)*[A-Za-z_][\w:<>,\s*&]*?[\s*&](\w+)\s*\(.*\)\s*(?:const)?$", s)
                if m:
                    noms.append(m.group(1))
            stmt = ""
        else:
            stmt += ch
    return noms


def appelants_publics(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    """Règle 12 : chaque fonction de tab5_custom.h a un appelant hors de son fichier."""
    header = tab5 / "tab5_custom.h"
    if not header.is_file():
        return [f"règle 12 : fichier introuvable : {header}"]
    noms = fonctions_publiques(header)
    if len(noms) < 100:
        return [f"règle 12 : {len(noms)} fonctions lues dans tab5_custom.h, le motif ne les reconnaît plus"]
    sources = firmware_sources(tab5, entry)
    yamls = [p for p in sources if p.suffix == ".yaml"]
    yamls += sorted((tab5 / "rendu").glob("*.yaml")) + [p for p in [tab5.parent / "tab5-rendu-host.yaml"] if p.is_file()]
    texte_yaml = "\n".join(strip_yaml_comments(p.read_text(encoding="utf-8")) for p in yamls)
    # Appels par gabarit : `${var}_suite(` → valeur_suite( pour chaque `var: valeur` passée.
    for var, suite in set(RE_GABARIT_APPEL.findall(texte_yaml)):
        for valeur in set(re.findall(rf"\b{var}:\s*[\"']?(\w+)", texte_yaml)):
            texte_yaml += f"\n{valeur}{suite}("
    cpp: dict[str, str] = {}
    definis: dict[str, set[str]] = {}
    for p in sources:
        if p.suffix not in (".cpp", ".h") or p == header:
            continue
        t = strip_cpp_comments(p.read_text(encoding="utf-8"))
        definis[p.name] = set(RE_CPP_DEF.findall(t))
        # Les têtes de définition et les déclarations ne sont pas des appels.
        cpp[p.name] = RE_CPP_PROTO.sub("", RE_CPP_DEF.sub("{", t))
    problems: list[str] = []
    for nom in sorted(set(noms)):
        appel = re.compile(rf"(?<![\w.>]){re.escape(nom)}\s*\(")
        if appel.search(texte_yaml):
            appele = True
        else:
            appele = any(appel.search(t) for f, t in cpp.items() if nom not in definis[f])
        if not appele and nom not in PUBLIQUES_SANS_APPELANT:
            ou = ", ".join(sorted(f for f, d in definis.items() if nom in d)) or "?"
            problems.append(f"tab5_custom.h : `{nom}()` ({ou}) n'est appelée ni par un YAML ni par une "
                            f"autre unité — `static` dans son fichier, ou tab5_internal.h (CPP-4)")
        elif appele and nom in PUBLIQUES_SANS_APPELANT:
            problems.append(f"PUBLIQUES_SANS_APPELANT : `{nom}()` a maintenant un appelant — retirer l'exception")
    problems += [f"PUBLIQUES_SANS_APPELANT : `{nom}()` n'est plus déclarée dans tab5_custom.h — retirer l'exception"
                 for nom in sorted(PUBLIQUES_SANS_APPELANT - set(noms))]
    return problems


# Règle 13 (conventions du nouveau code, Tab5/README.md) : dans le C++ de Tab5/, `nullptr`
# et jamais `NULL` ; hors jeux, un journal porte le tag `tab5.<module>` (`ESP_LOGW("tab5.clim",
# …)`) et non « TAB5 », pour filtrer un module dans les journaux. Les jeux (et leurs moteurs)
# gardent leur nom court (« chess », « lode »…), comme leurs pages.
RE_NULL = re.compile(r"\bNULL\b")
RE_ESP_LOG = re.compile(r"\bESP_LOG[EWIDVC]+\s*\(\s*(\"[^\"]*\"|\w+)")
RE_TAG_CONST = re.compile(r"\b(?:const|constexpr)\s+char\s*(?:\*\s*(?:const\s+)?)?(\w+)\s*(?:\[\])?\s*=\s*\"([^\"]*)\"")
RE_TAG_TAB5 = re.compile(r"^tab5\.[a-z0-9_]+$")
RE_JEU_CPP = re.compile(r"(_game|_ai|_engine|^game_common|^trivia_questions)\.(cpp|h)$")
# Exceptions datées (vide depuis la fin du lot L5, 08/10/2026 : tab5_central.cpp et
# tab5_zones.cpp sont passés à tab5.central / tab5.zones). Nombre exact de tags permis.
TAGS_TAB5_TEMPORAIRES: dict[str, int] = {}
# Journal d'un payload refusé (lot L5) : ces fonctions écrivent sous le tag que l'appelant
# leur donne (paramètre `tag`) ; chaque appel doit alors passer un « tab5.<module> » littéral.
TAGS_RELAYES: dict[str, int] = {"tab5_text.cpp": 2}
RE_TAG_RELAYE = re.compile(r"\bpayload_(?:refuse|trop_long)\s*\(\s*(\"[^\"]*\"|[^,\s)]+)")


def conventions_cpp(tab5: Path = TAB5, entry: Path = ENTRY) -> list[str]:
    """Règle 13 : `nullptr` partout, tag de journal `tab5.<module>` hors jeux."""
    problems: list[str] = []
    vus = set()
    for path in firmware_sources(tab5, entry):
        if path.suffix not in (".cpp", ".h"):
            continue
        lines = _cpp_lines(path)
        texte = "\n".join(lines)
        for lineno, line in enumerate(lines, 1):
            if RE_NULL.search(re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)):
                problems.append(f"{path.name}:{lineno} : `NULL` — écrire `nullptr` (conventions, Tab5/README.md)")
        if RE_JEU_CPP.search(path.name):
            continue
        vus.add(path.name)
        tags = dict(RE_TAG_CONST.findall(texte))
        hors: list[int] = []
        relayes = 0
        for lineno, line in enumerate(lines, 1):
            for m in RE_ESP_LOG.finditer(line):
                arg = m.group(1)
                if arg == "tag" and path.name in TAGS_RELAYES:
                    relayes += 1
                    continue
                tag = arg[1:-1] if arg.startswith('"') else tags.get(arg)
                if tag is None or not RE_TAG_TAB5.match(tag):
                    hors.append(lineno)
            for m in RE_TAG_RELAYE.finditer(line):
                arg = m.group(1)
                if line.lstrip().startswith(("bool ", "void ")):
                    continue  # déclaration ou définition, pas un appel
                tag = arg[1:-1] if arg.startswith('"') else tags.get(arg)
                if tag is None or not RE_TAG_TAB5.match(tag):
                    hors.append(lineno)
        if relayes != TAGS_RELAYES.get(path.name, 0):
            problems.append(f"TAGS_RELAYES : {path.name} relaie {relayes} tag(s) reçu(s) "
                            f"(attendu {TAGS_RELAYES.get(path.name, 0)}) — mettre la table à jour")
        permis = TAGS_TAB5_TEMPORAIRES.get(path.name, 0)
        if len(hors) > permis:
            problems += [f"{path.name}:{n} : tag de journal hors `tab5.<module>` — "
                         f"`ESP_LOGW(\"tab5.{path.stem.removeprefix('tab5_')}\", …)` (conventions, Tab5/README.md)"
                         for n in hors]
        elif len(hors) < permis:
            problems.append(f"TAGS_TAB5_TEMPORAIRES : {path.name} n'a plus que {len(hors)} tag(s) hors "
                            f"convention (plafond {permis}) — abaisser ou retirer l'entrée")
    problems += [f"TAGS_TAB5_TEMPORAIRES : `{nom}` n'est plus une unité du firmware — retirer l'entrée"
                 for nom in sorted(set(TAGS_TAB5_TEMPORAIRES) - vus)]
    return problems


# Règle 14 : accents des textes de l'écran (« French accents always », AGENTS.md). Les
# textes vus sont ceux de tools/i18n_keys.py : littéraux de tr() / tr_ctx() / tr_fill() /
# tr_noop() (C++ et lambdas, jeux compris) et textes posés par le YAML des écrans. Pas les
# identifiants, les noms d'entités, les options lues par HA ni les clés de payload : ils ne
# passent pas par tr(). Pour tr_ctx(), seul le texte compte (le contexte est un code).
# Liste FERMÉE de mots courants, sans accent = toujours une faute en français (vérifiée
# sans faux positif sur les 977 textes du 08/10/2026). Exclu exprès : « annule » (« Annule
# le dernier coup » est juste), « a » seul (verbe avoir), « connecte » / « active »
# (formes verbales justes). Un mot ajouté doit rester faux dans tous ses emplois.
MOTS_SANS_ACCENT = (
    "journees?", "redemarrages?", "redemarrer", "redemarree?s?", "reflexions?", "temperatures?",
    "etes?", "previsions?", "precipitations?", "reglages?", "regler", "parametres?", "securite",
    "electricite", "energies?", "deja", "equipements?", "ecrans?", "etats?", "arretee?s?",
    "annulees?", "annules", "recue?s?", "fenetres?", "pieces?", "telecommandes?", "meteo",
    "themes?", "systemes?", "categories?", "dernieres?", "premieres?", "deconnectee?s?",
    "desactivee?s?", "cree", "creee?s?", "delais?", "durees?", "generale?s?", "numeros?",
    "periodes?", "bientot", "controles?", "frequences?", "debut", "evenements?", "ecoute",
    "reponses?", "precedente?s?", "details?", "mise a jour",
)
RE_SANS_ACCENT = re.compile(r"(?<!\w)(" + "|".join(MOTS_SANS_ACCENT) + r")(?!\w)", re.I)


def _i18n_keys():
    outils = str(REPO / "tools")
    if outils not in sys.path:
        sys.path.insert(0, outils)
    import i18n_keys
    return i18n_keys


def textes_ecran(tab5: Path = TAB5, entry: Path = ENTRY) -> dict[str, list[str]]:
    """Texte affiché → emplacements (tr*() et textes YAML des écrans)."""
    k = _i18n_keys()
    fichiers = [p for p in firmware_sources(tab5, entry)
                if p.name not in ("tab5_i18n_data.h", "tab5_i18n.cpp", "tab5_i18n.h")]
    textes: dict[str, list[str]] = {}
    for source in (k.cles_tr(fichiers, tab5.parent), k.textes_yaml(fichiers, tab5.parent)):
        for t, ou in source.items():
            textes.setdefault(t, []).extend(ou)
    return textes


def accents_ecran(textes: dict[str, list[str]]) -> list[str]:
    """Règle 14 : aucun mot courant écrit sans son accent dans un texte de l'écran."""
    problems = []
    for cle, ou in sorted(textes.items()):
        texte = cle.split("|", 1)[1] if "|" in cle else cle
        for m in RE_SANS_ACCENT.finditer(texte):
            problems.append(f"{ou[0]} : « {m.group(1)} » sans accent dans {texte[:60]!r} "
                            f"(accents toujours, AGENTS.md ; MOTS_SANS_ACCENT)")
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

    # 2 bis. La pile vocale, sortie de tab5-hardware.yaml vers tab5-assist.yaml au
    # lot 8c (25/09/2026), garde la même règle : tout ce qui précède `script:`
    # (micro_wake_word, voice_assistant, image de la réponse) passe par
    # assist_set_pipeline_state() / assist_image_state_ui(), jamais par lv_*.
    # Les scripts du popup Assistant, eux, ont le droit de toucher leurs widgets.
    assist = tab5 / "tab5-assist.yaml"
    if assist.is_file():
        text = strip_yaml_comments(assist.read_text(encoding="utf-8"))
        pile = text.split("\nscript:", 1)[0]
        for lineno, line in enumerate(pile.splitlines(), 1):
            for m in RE_LV_CALL.finditer(line):
                problems.append(
                    f"tab5-assist.yaml:{lineno} : {m.group(1)}() — la pile vocale ne touche pas "
                    f"LVGL, passer par assist_set_pipeline_state() (ADR-0006)"
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

    # 6. glyphes de la date (police de lbl_date)
    problems += date_glyph_coverage(tab5)

    # 7. icônes MDI : couvertes par la police de leur widget, aucun glyphe mort
    problems += mdi_glyph_coverage(tab5, entry)

    # 8. couleurs de l'interface par la palette (thèmes)
    problems += palette_colors(tab5, entry)

    # 9 à 14. garde-fous du lot L6 (audit du 07/10/2026)
    problems += lvgl_yaml(tab5, entry)
    problems += static_lambdas(tab5, entry)
    problems += chemins_chauds(tab5, entry)
    problems += appelants_publics(tab5, entry)
    problems += conventions_cpp(tab5, entry)
    problems += accents_ecran(textes_ecran(tab5, entry))

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
        "glyphes de la date couverts, icônes MDI couvertes sans glyphe mort, "
        "couleurs de l'interface par la palette"
        ", aucun nouvel appel LVGL ni `static` dans le YAML, pas de copie de chaîne dans un chemin chaud"
        ", fonctions publiques appelées"
        ", nullptr et tags tab5.<module>"
        ", accents des textes de l'écran"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
