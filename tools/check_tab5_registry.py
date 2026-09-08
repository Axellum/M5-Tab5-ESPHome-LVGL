#!/usr/bin/env python3
"""Garde-fou du registre unique des consoles et des fenêtres modales (ADR-0013).

Depuis le 08/09/2026, la liste des 8 consoles vit dans `Tab5/tab5_registry.cpp`
(`GameRegistry::kGames`) et la liste des fenêtres modales dans le script
`tab5_modal_registry_init` de `Tab5/tab5-scripts.yaml`. Ce script vérifie que
personne ne recopie une liste ailleurs et qu'aucune console ni popup n'est
oubliée :

  1. chaque `Tab5/*_game.h` (namespace avec `bool is_open();`) figure dans
     `kGames`, et réciproquement ;
  2. aucun YAML de `Tab5/` n'interroge un jeu directement (`X::is_open()`),
     n'appelle `X::close()` hors du script d'ouverture du jeu, ni ne dispatche
     l'IMU jeu par jeu (`X::on_imu(`) : tout passe par `GameRegistry` ;
  3. les anciennes tables `kPopups` / `kScreens` / `kTargetOf` n'existent plus ;
  4. `ModalRegistry::add(` n'apparaît que dans `tab5-scripts.yaml` ;
  5. chaque popup à carte modale de `ui_components/` (fichier contenant
     `style_modal_card`, id racine = premier `id:`) est enregistré ;
  6. chaque option du select « Aller à l'écran » (hors « — » et « Accueil »)
     correspond exactement au libellé d'une fenêtre enregistrée — `find()` la
     retrouve par ce nom, un écart serait un no-op silencieux.

Usage : python tools/check_tab5_registry.py   (aussi lancé par `pytest`, tests/test_guards.py)
Sortie : 0 si tout est conforme, 1 sinon (liste des écarts sur stdout).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
UI = TAB5 / "ui_components"
REGISTRY_CPP = TAB5 / "tab5_registry.cpp"
SCRIPTS_YAML = TAB5 / "tab5-scripts.yaml"
HA_CONTROLS_YAML = TAB5 / "tab5-ha-controls.yaml"

RE_NAMESPACE = re.compile(r"^namespace (\w+) \{", re.M)
RE_ADD = re.compile(r'ModalRegistry::add\(\s*id\((\w+)\)\s*,\s*(nullptr|"([^"]*)")\s*,')
LEGACY_TABLES = ("kPopups", "kScreens", "kTargetOf")
SELECT_SKIP = {"—", "Accueil"}


def strip_comments(text: str) -> str:
    """Retire les lignes de commentaire YAML : une liste commentée n'en est pas une."""
    return "\n".join(l for l in text.splitlines() if not l.lstrip().startswith("#"))


def strip_cpp_comments(text: str) -> str:
    """Retire d'abord les `//` (un `/*` cité dans un commentaire de ligne ne doit
    pas avaler la suite du fichier), puis les blocs `/* … */`."""
    text = "\n".join(l.split("//", 1)[0] for l in text.splitlines())
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def games_from_headers(tab5: Path = TAB5) -> dict[str, str]:
    """{namespace: fichier} pour chaque en-tête de console (namespace + is_open)."""
    games: dict[str, str] = {}
    for h in sorted(tab5.glob("*_game.h")):
        text = strip_cpp_comments(h.read_text(encoding="utf-8"))
        if "bool is_open();" not in text:
            continue
        # Le premier `namespace X {` de colonne 0 qui n'est pas la déclaration
        # anticipée `namespace esphome { namespace font { class Font; } }` : les
        # namespaces internes (`Pal`, `Engine`) viennent après celui du jeu.
        for m in RE_NAMESPACE.finditer(text):
            if m.group(1) != "esphome":
                games[m.group(1)] = h.name
                break
    return games


def games_from_registry(cpp: Path = REGISTRY_CPP) -> set[str]:
    text = strip_cpp_comments(cpp.read_text(encoding="utf-8"))
    return set(re.findall(r"(\w+)::is_open\b", text))


def registered_modals(scripts: Path = SCRIPTS_YAML) -> dict[str, str | None]:
    """{id_lvgl: libellé ou None} des fenêtres enregistrées par le script d'init."""
    text = strip_comments(scripts.read_text(encoding="utf-8"))
    return {m.group(1): m.group(3) for m in RE_ADD.finditer(text)}


def modal_root_ids(ui_dir: Path = UI) -> dict[str, str]:
    """{id racine: fichier} des popups à carte modale."""
    roots: dict[str, str] = {}
    for path in sorted(ui_dir.glob("*.yaml")):
        text = strip_comments(path.read_text(encoding="utf-8"))
        if "style_modal_card" not in text:
            continue
        m = re.search(r"^\s*id:\s*(\w+)", text, re.M)
        if m:
            roots[m.group(1)] = path.name
    return roots


def select_options(ha_controls: Path = HA_CONTROLS_YAML) -> list[str]:
    text = strip_comments(ha_controls.read_text(encoding="utf-8"))
    m = re.search(r"id: tab5_goto_screen\n(.*?)\n\s*on_value:", text, re.S)
    if not m:
        return []
    return [o for o in re.findall(r'^\s*-\s*"([^"]+)"', m.group(1), re.M) if o not in SELECT_SKIP]


def scan(tab5: Path = TAB5) -> list[str]:
    problems: list[str] = []
    ui_dir = tab5 / "ui_components"
    registry_cpp = tab5 / "tab5_registry.cpp"
    scripts_yaml = tab5 / "tab5-scripts.yaml"
    ha_controls_yaml = tab5 / "tab5-ha-controls.yaml"

    for required in (registry_cpp, scripts_yaml, ha_controls_yaml):
        if not required.is_file():
            return [f"fichier introuvable : {required}"]

    # 1. en-têtes de jeux ↔ kGames
    headers = games_from_headers(tab5)
    in_registry = games_from_registry(registry_cpp)
    for ns in sorted(set(headers) - in_registry):
        problems.append(f"{headers[ns]} : namespace {ns} absent de GameRegistry::kGames ({registry_cpp.name})")
    for ns in sorted(in_registry - set(headers)):
        problems.append(f"{registry_cpp.name} : {ns} dans kGames sans en-tête *_game.h correspondant")

    # 2 + 3 + 4. aucune liste recopiée dans les YAML
    game_names = "|".join(sorted(headers)) or "NO_GAME"
    re_is_open = re.compile(rf"\b({game_names})::is_open\(")
    re_close = re.compile(rf"\b({game_names})::close\(")
    re_on_imu = re.compile(rf"\b({game_names})::on_imu\(")
    yaml_files = sorted(tab5.glob("*.yaml")) + sorted(ui_dir.glob("*.yaml"))
    for path in yaml_files:
        text = strip_comments(path.read_text(encoding="utf-8"))
        for m in re_is_open.finditer(text):
            problems.append(f"{path.name} : {m.group(0)} — interroger GameRegistry, pas le jeu")
        for m in re_close.finditer(text):
            problems.append(f"{path.name} : {m.group(0)} — fermer via GameRegistry::close_all()")
        for m in re_on_imu.finditer(text):
            problems.append(f"{path.name} : {m.group(0)} — dispatcher via GameRegistry::dispatch_imu()")
        for table in LEGACY_TABLES:
            if re.search(rf"\b{table}\b", text):
                problems.append(f"{path.name} : table {table} — remplacée par ModalRegistry")
        if "ModalRegistry::add(" in text and path.name != scripts_yaml.name:
            problems.append(f"{path.name} : ModalRegistry::add() hors de {scripts_yaml.name}")

    # 5. chaque popup à carte modale est enregistré
    registered = registered_modals(scripts_yaml)
    for root, fname in sorted(modal_root_ids(ui_dir).items()):
        if root not in registered:
            problems.append(f"{fname} : id(`{root}`) absent de tab5_modal_registry_init ({scripts_yaml.name})")

    # 6. options du select ↔ libellés enregistrés
    names = {n for n in registered.values() if n}
    for opt in select_options(ha_controls_yaml):
        if opt not in names:
            problems.append(
                f"{ha_controls_yaml.name} : option « {opt} » du select tab5_goto_screen sans fenêtre "
                f"enregistrée sous ce libellé (ModalRegistry::find() rendrait nullptr)"
            )

    return problems


def main() -> int:
    problems = scan()
    if problems:
        print("[KO] registre consoles/modales — écarts :")
        for p in problems:
            print("  -", p)
        return 1
    print("[OK] registre consoles/modales : listes uniques, rien d'oublié")
    return 0


if __name__ == "__main__":
    sys.exit(main())
