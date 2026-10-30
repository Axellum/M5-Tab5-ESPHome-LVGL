# AGENTS.md — `Tab5/` (the firmware)

Read with the root [`AGENTS.md`](../AGENTS.md). Claude Code loads this file (through the `CLAUDE.md` next to it) only once it reads or edits a file of this folder. File-by-file description, service contract table, globals table and the rules in full: [`Tab5/README.md`](README.md).

## Before editing

[`CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md): read this first instead of reverse-engineering the YAML tree from scratch. [`docs/decisions/`](../docs/decisions/README.md) explains the non-obvious choices (push-only, single-page UI, no hardcoded colors, etc.); [`docs/troubleshooting.md`](../docs/troubleshooting.md) the symptoms already diagnosed (black screen after reboot, missing weather/planning data, mic pipeline stuck, etc.).

## Layout of `Tab5/`

The first table of [`Tab5/README.md`](README.md) is the layout of `Tab5/` (since 2026-10-08): `socle/` (pure C++, built and tested on a PC, no ESPHome nor LVGL), `ecran/` (the LVGL layer, `tab5_custom.h`), `jeux/` (the eight consoles), `paquets/` (the ESPHome packages), `ui_components/` and `fonts/`; only `README.md`, `user_entities*.yaml` and `tuiles_icones.yaml` stay at its root (`tests/test_rangement.py`), with this `AGENTS.md` and its `CLAUDE.md`. ESPHome copies every `includes:` file flat into its build, so `#include "x.h"` never names a folder and a file name must stay unique across the folders.

Toolboxes (« boîte à outils »): [`socle/AGENTS.md`](socle/AGENTS.md), [`ecran/AGENTS.md`](ecran/AGENTS.md), [`jeux/AGENTS.md`](jeux/AGENTS.md). Parallel sessions kept rewriting helpers that already existed (audit of 2026-10-07: six ways to read a number from a payload, four brightness formulas, one of them visibly wrong): reuse before writing.

## The `[AI-CONTEXT]` / `[AI-WARNING]` / `[AI-DEBUG]` convention

Most files in `Tab5/` and every file in `Tab5/ui_components/` open with an `[AI-CONTEXT]` comment block: role, architectural constraints, and `@ai_instruction` notes for common edits.

- **`[AI-WARNING]`** marks code that looks like a bug or anti-pattern but is a deliberate, tested fix (e.g. the blocking `delay(1000)` in `on_boot` for the display reset GPIO expander, or the non-wrapping forecast pagination). **Read the warning before "fixing" it.** At least one of these was already reverted once after an LLM audit "corrected" it without reading the comment.
- **`[AI-DEBUG]`** marks a good observation point for diagnosing issues at runtime (a log line, a diagnostic entity, a technique that already worked). See [`docs/debugging.md`](../docs/debugging.md).

If you introduce a new non-obvious constraint, add or extend the relevant `[AI-CONTEXT]` block — don't leave the reasoning only in a commit message.

## Build, clean, flash

```bash
# Compile — must succeed before any commit. Toolchain is cached locally (about 1-3 min incremental, about 5 min after a clean).
python -m esphome compile tab5-ha-hmi.yaml
```

- If you modified a file included via `!include` (anything in `Tab5/ui_components/`), run `esphome clean` before the next `esphome run` — stale build cache is a known ESPHome trap.
- Compare the reported `config_hash` before/after a refactor that should be behavior-neutral — identical hash is the standard proof of "no functional change" used across this project's PR history.
- OTA-flashing the real device is a deliberate, human-authorized action, not a default step of a coding task — only do it if explicitly asked. If you do: confirm afterward via the device's own diagnostic entities (`ha_api_status`, boot time on `Tab5 Uptime` unchanged after the flash's own reboot, no reboot) rather than assuming success.
- The second board (Waveshare 7B) and CI: [`tools/AGENTS.md`](../tools/AGENTS.md).

## Code rules, in full (summary in the root `AGENTS.md`, detail in `Tab5/README.md`)

1. No hardcoded colour in YAML/lambdas — add a role to `struct Palette` (`Tab5/socle/tab5_tokens.h`, included by `tab5_custom.h`) with a value in every palette. C++ and lambdas read `UIColor.X` (the active palette); a widget takes its colour through a role style (`styles: style_text_dim`, `Tab5/paquets/tab5-styles.yaml`), never a `text_color:`/`bg_color:` set on it — ESPHome freezes those at compile time and a theme could not change them ([ADR-0029](../docs/decisions/0029-themes-palette.md), rule 8 of `tools/check_tab5_code_rules.py`). Games read `PALETTE_SOMBRE.X` and stay dark.
2. `sensor:`/`text_sensor:` never touch `lv_obj_*` directly — always call a named C++ function of the C++ layer (`Tab5/ecran/tab5_*.cpp`, declared in its module's header `tab5_<module>.h`, which the umbrella `tab5_custom.h` includes — one header per module since 2026-10-08).
3. No `static` inside a lambda for state shared across handlers — use a `globals:` entry instead.
4. No `std::string` by value or `to_string()` in a hot path (sliders, frequent `on_value`) — use `const std::string&` or a `snprintf` buffer.
5. Any widget/card repeated 3+ times goes through a parametrized C++ builder or `!include`+`vars` template, not copy-pasted YAML.
6. `esphome compile` must pass before committing.
7. Every modal popup reuses the shared chrome (`modal_scrim.yaml` + `modal_header.yaml`, ADR-0009) — games are the documented exception.
8. No hardcoded Home Assistant entity ID in firmware YAML — always a `user_entities.yaml` substitution (`${entity_…}`) or a `!lambda`. Since [ADR-0025](../docs/decisions/0025-events-only.md) the firmware names none at all and **never calls a Home Assistant action** (no `homeassistant.service` / `homeassistant.action`): it emits `esphome.tab5_*` events, and `HomeAssistant_Config/packages/tab5_evenements.yaml` maps them to a whitelist of actions on the tablet's own entities (found by its device). A new request = an event + a branch there. Enforced by `tools/check_tab5_code_rules.py` and `tests/test_actions_ha.py`.
9. Every MDI icon shown on screen must be in the `glyphs:` list of its widget's `mdi_*` font (`Tab5/paquets/tab5-styles.yaml`), and every glyph listed there must be shown somewhere — a missing glyph renders blank with no build error. An icon set from C++ on a widget passed as a parameter needs its function in `MDI_CODE_TARGETS` (`tools/check_tab5_code_rules.py`, rule 7, run by `pytest`).
10. Our C++ is called from a `lambda:` action, never from a field of a Home Assistant event nor from the value of an "apply" action (`select.set_index`, `rtttl.play`, `media_player.volume_set`, `globals.set`…): since ESPHome 2026.10.0 those values are emitted before the `includes:` and cannot see our functions, constants or C++ globals ([esphome/esphome#21303](https://github.com/esphome/esphome/issues/21303)). Such a value names only parameters, `id(…)` and the standard library; anything else goes through a `lambda:` or a script with parameters called from one. Enforced by `tests/test_includes_esphome.py`.

## Screen language

**Screen language (2026-09-27):** a new text on the screen is written in French and goes through `tr()` (C++, lambdas, games included) or is laid out by the YAML; a text kept in a table is marked `tr_noop("…")` and translated where shown, `tr(table[i])`; the literal stays on the same line as the call; add its line to `Tab5/lang/en.yaml` and run `python tools/gen_i18n.py` — `tests/test_i18n.py` fails otherwise. Never translate what Home Assistant reads (entity names, select options, text-sensor states, payload codes); a HA value shown on screen is translated at display time. Since 2026-10-24 ([ADR-0070](../docs/decisions/0070-home-assistant-reads-english.md)) the select options and text-sensor states HA reads are written in **English** for every screen language (never a `name:`, which stays as it is): a select is read and published by its index, never by its text, and a state computed with `tr()` goes through the `I18nPourHa` guard (`Tab5/socle/tab5_i18n.h`, `tests/test_ha_anglais.py`). See `docs/translations.md`.

## The API service contract

When you add or remove a variable on an `api: services:` entry in `tab5-api-logic.yaml`, you are changing a public contract with several callers, not one: the HA packages and the blueprint (`HomeAssistant_Config/`, then deploy), the snippets, the demo pusher (`tools/demo/`), the off-device render (`tools/rendu/ecrans.py`), and the service table in `Tab5/README.md`. Home Assistant refuses a call with a missing or an extra variable and stops the script there, so the following pushes never leave. Update all of them in the same PR; `tests/test_contrat.py` and `demo_pusher.py --dry-run` fail on a mismatch. The contract between versions is `contrat/contrat.yaml`: regenerate it (`python tools/contrat_api.py --write`), raise its `version:` as `--semver` asks (major: something an older HA file sends or reads disappears or becomes required; minor: pure addition), and state the update order in `[Unreleased]` of the CHANGELOG when `--matrice` says both ways do not work — `tests/test_contrat_versions.py` checks all three against the last published tags.

## Other boundaries of this folder

- Do not read or write `Tab5/user_entities.yaml` (gitignored — your real HA entity IDs). Edit `Tab5/user_entities.example.yaml` only when changing the public template or adding a new substitution key.
- Do not touch `Tab5/tts_library*/` (gitignored, experimental, predates the current voice pipeline, unused).
