#!/usr/bin/env python3
"""Règles de code du firmware Tab5, jouées à chaque `pytest` (audit du 06/09/2026,
§4.1 points 1, 4, 15 et §4.2 point 17 ; ADR-0006). Cinq règles, toutes
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

Usage : python tools/check_tab5_code_rules.py   (aussi lancé par `pytest`, tests/test_guards.py)
Sortie : 0 si tout est conforme, 1 sinon (liste des écarts sur stdout).
"""

from __future__ import annotations

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
        "aucun global orphelin, aucune entité HA en dur, actions API décrites"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
