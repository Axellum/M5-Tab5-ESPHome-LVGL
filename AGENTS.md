# AGENTS.md — instructions for AI coding agents

This file is the entry point for any AI agent (Claude, Codex, Cursor, Copilot, Gemini CLI, or a human skimming fast) working on this repository. Keep it short; when it drifts from the code, the code wins — fix this file, don't work around it.

## What this project is

A Home Assistant dashboard running natively as ESP32-P4 firmware (ESPHome + LVGL 8.4) on a M5Stack Tab5 V2. Push-only: the device never polls, Home Assistant automations push data via ESPHome API service calls. No web stack, no browser. See [`README.md`](README.md) for the full picture.

## Read this before touching anything

In order, before editing code or answering questions about architecture:

1. [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md) — full dependency graph, file-by-file inventory, and a verified list of known technical debt / dead code. Read this first instead of reverse-engineering the YAML tree from scratch.
2. [`Tab5/README.md`](Tab5/README.md) — file-by-file description of the ESPHome packages, the HA service contract table, the globals table, and **6 mandatory code rules**.
3. [`docs/decisions/`](docs/decisions/README.md) — why non-obvious architectural choices were made (push-only, single-page UI, no hardcoded colors, etc.). Check here before "fixing" something that looks wrong.
4. [`docs/troubleshooting.md`](docs/troubleshooting.md) — incidents already diagnosed on this exact device. Check here before re-diagnosing a symptom that looks familiar (black screen after reboot, missing weather/planning data, mic pipeline stuck, etc.).
5. The target file itself, including its `[AI-CONTEXT]` header (see below).

## The `[AI-CONTEXT]` / `[AI-WARNING]` / `[AI-DEBUG]` convention

Most files in `Tab5/` and every file in `Tab5/ui_components/` open with an `[AI-CONTEXT]` comment block: role, architectural constraints, and `@ai_instruction` notes for common edits.

- **`[AI-WARNING]`** marks code that looks like a bug or anti-pattern but is a deliberate, tested fix (e.g. the blocking `delay(1000)` in `on_boot` for the display reset GPIO expander, or the non-wrapping forecast pagination). **Read the warning before "fixing" it.** At least one of these was already reverted once after an LLM audit "corrected" it without reading the comment.
- **`[AI-DEBUG]`** marks a good observation point for diagnosing issues at runtime (a log line, a diagnostic entity, a technique that already worked). See [`docs/debugging.md`](docs/debugging.md).

If you introduce a new non-obvious constraint, add or extend the relevant `[AI-CONTEXT]` block — don't leave the reasoning only in a commit message.

## Build & verify

```bash
# Compile — must succeed before any commit. Toolchain is cached locally (~20-45s).
python -m esphome compile tab5-ha-hmi.yaml
```

- If you modified a file included via `!include` (anything in `Tab5/ui_components/`), run `esphome clean` before the next `esphome run` — stale build cache is a known ESPHome trap.
- Compare the reported `config_hash` before/after a refactor that should be behavior-neutral — identical hash is the standard proof of "no functional change" used across this project's PR history.
- OTA-flashing the real device is a deliberate, human-authorized action, not a default step of a coding task — only do it if explicitly asked. If you do: confirm afterward via the device's own diagnostic entities (`ha_api_status`, uptime strictly increasing, no reboot) rather than assuming success.
- There is no unit test suite; `esphome compile` (schema + C++ compile) is the correctness gate. CI (`.github/workflows/esphome-tab5.yml`) runs the same compile with a dummy `secrets.yaml` on every push/PR.

## Code rules (full detail in `Tab5/README.md`)

1. No hardcoded hex colors in YAML/lambdas — add a token to `UIColor::` (`Tab5/tab5_custom.h`).
2. `sensor:`/`text_sensor:` never touch `lv_obj_*` directly — always call a named C++ function in `tab5_custom.cpp`.
3. No `static` inside a lambda for state shared across handlers — use a `globals:` entry instead.
4. No `std::string` by value or `to_string()` in a hot path (sliders, frequent `on_value`) — use `const std::string&` or a `snprintf` buffer.
5. Any widget/card repeated 3+ times goes through a parametrized C++ builder or `!include`+`vars` template, not copy-pasted YAML.
6. `esphome compile` must pass before committing.

## Boundaries — do not

- Do not read or write `secrets.yaml` / `Tab5/secrets.yaml` (gitignored, never tracked — verified against full git history).
- Do not read or write `Tab5/user_entities.yaml` (gitignored — your real HA entity IDs). Edit `Tab5/user_entities.example.yaml` only when changing the public template or adding a new substitution key.
- Do not confuse the gitignored real HA config (`HomeAssistant_Config/automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml` — Axel's actual production files) with the tracked `*_examples.yaml*` placeholders. If you change logic in one, mirror the change in the other.
- Do not leave more than a couple of ESPHome CLI processes running against the device at once — the API only has 8 connection slots; a past session's leaked `esphome` processes silently starved the real device of connections.
- Do not "clean up" code flagged `[AI-WARNING]` without reading the warning and checking `docs/decisions/`.
- Do not touch `Tab5/tts_library*/` (gitignored, experimental, predates the current voice pipeline, unused).

## Workflow

- Branch + PR against `main`, never push directly. Use [`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md)'s checklist.
- Add an entry to [`CHANGELOG.md`](CHANGELOG.md) for user-visible or architectural changes.
- If a change alters the file/dependency structure described in `CARTOGRAPHIE_TAB5.md`, update that file in the same PR.
- Commit messages and code comments in this repo are a mix of French and English (the author is French, the repo is public/bilingual) — match the existing style of the file you're editing rather than converting it wholesale.

## Full documentation map

See the table in [`README.md`](README.md#documentation) for the complete list of `docs/*.md` files (architecture, hardware, UI design, voice assistant, installation, screens).
