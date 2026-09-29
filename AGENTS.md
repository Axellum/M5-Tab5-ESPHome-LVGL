# AGENTS.md — instructions for AI coding agents

This file is the entry point for any AI agent (Claude, Codex, Cursor, Copilot, Gemini CLI, or a human skimming fast) working on this repository. Keep it short; when it drifts from the code, the code wins — fix this file, don't work around it.

## What this project is

A Home Assistant dashboard running natively as ESP32-P4 firmware (ESPHome + LVGL 9.5 — not 8.x: check `lv_version.h` in the build tree before picking an API) on a M5Stack Tab5 V2. Push-only: the device never polls, Home Assistant automations push data via ESPHome API service calls. No web stack, no browser. See [`README.md`](README.md) for the full picture.

## Read this before touching anything

In order, before editing code or answering questions about architecture:

1. [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md) — full dependency graph, file-by-file inventory, and a verified list of known technical debt / dead code. Read this first instead of reverse-engineering the YAML tree from scratch.
2. [`Tab5/README.md`](Tab5/README.md) — file-by-file description of the ESPHome packages, the HA service contract table, the globals table, and **8 mandatory code rules**. The eight game consoles are documented separately in [`docs/arcade.md`](docs/arcade.md) — read it only when touching a game.
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
- OTA-flashing the real device is a deliberate, human-authorized action, not a default step of a coding task — only do it if explicitly asked. If you do: confirm afterward via the device's own diagnostic entities (`ha_api_status`, boot time on `Tab5 Uptime` unchanged after the flash's own reboot, no reboot) rather than assuming success.
- `esphome compile` (schema + C++ compile) is the correctness gate for the firmware itself. CI (`.github/workflows/esphome-tab5.yml`) runs on every PR and on pushes to `main`: a `python` job (pre-commit hooks, pytest, the real Go engine built with g++, demo dry-run) and the same compile with a throwaway signing key (`openssl genrsa`, no `secrets.yaml` since lot 6b) **only when `tab5-ha-hmi.yaml`, `Tab5/` (Markdown excluded) or the workflow change**. `python`, `build` and `build-min` are required checks; `build` stays present and reports "skipped" (= success) on docs-only changes; `workflow_dispatch` forces a compile. The compile deliberately uses ESPHome `latest` (free upstream canary, [ADR-0016](docs/decisions/0016-ci-esphome-latest-canary.md)) and keeps its ccache between runs; a `build-min` job compiles the same configuration with the ESPHome version read from `min_version:` in `tab5-ha-hmi.yaml` (required too since 2026-09-26). `main` publishes the `tab5-firmware` artifact.
- There is no unit test suite *for the HMI*, but three game engines have host tests (Go, chess, draughts), and six content guards read the real C++/YAML (modal chrome per ADR-0009, single registry per ADR-0013, code rules, Marble rooms traversable, Lode levels playable, `CARTOGRAPHIE_TAB5.md` line counts). Everything runs on a plain PC with no toolchain, in one `pytest` (config in `pyproject.toml`, deps in `requirements-dev.txt`):

```bash
pip install -r requirements-dev.txt # une fois : pytest, numpy (garde-fou Marble), aioesphomeapi, fonttools, pre-commit, yamllint
python -m pytest                    # tout : tests/ (outils + garde-fous) + tools/ (moteurs Go, échecs et dames)
pre-commit install                  # une fois : hooks yamllint / BOM / secrets / placeholders HA avant chaque commit
pre-commit run --all-files          # les mêmes garde-fous sur tout le dépôt (la CI les rejoue)
python tools/test_go_engine.py      # règles Go : capture, suicide, ko, territoire, score
python tools/test_chess_perft.py    # générateur d'échecs contre la suite perft standard
python tools/test_draughts_engine.py # générateur de dames (10×10 et 8×8) contre les perft de référence
python tools/render_ha_config.py --check   # ni identifiant HA réel ni placeholder dans un fichier public (local : lit placeholders.yaml)
python tools/check_tab5_modal_chrome.py    # ADR-0009 : chrome modal partagé sur chaque popup
python tools/check_marble_rooms.py         # les 6 salles de Fil d'Or restent traversables
python tools/check_lode_levels.py          # les 10 niveaux de Coureur d'Or restent jouables
python tools/check_tab5_registry.py        # ADR-0013 : registre unique consoles/modales
python tools/check_tab5_code_rules.py      # règles de code (snprintf, pas de lv_* dans le contrat, pas d'entité HA en dur…)
python tools/cartographie_counts.py        # comptes de lignes de la cartographie (--write pour les recalculer)
```

  All three are **Python mirrors** of the C++ (`go_engine.cpp`, `chess_ai.cpp`, `draughts_game.cpp`), not bindings: a change to the C++ must be mirrored there or the test stops proving anything. `tools/test_go_engine.cpp` is the same suite compiled against the real C++: the CI `python` job builds it with g++ and runs it on every PR (the dev box only has the RISC-V cross-compiler). `tools/test_alarm_clock.cpp` does the same for the **alarm engine** (`alarm_clock.cpp` + `tab5_core.cpp`, both free of ESPHome/LVGL): simulated clock through `tab5_time_source`, Europe/Paris DST, calendar anchor day. Keep those two files free of `esphome.h`/`lv_*` — the render lives in `alarm_render.*`.
- `tools/demo/demo_pusher.py --dry-run` validates the payloads of the **10 dashboard push services** against the firmware contract without any hardware — cheap check after touching `tab5-api-logic.yaml`. The 6 other services (`tab5_maj_alertes_ha_bulk`, `tab5_maj_calendrier_mois/_jour`, `tab5_maj_rdv_prochains`, `tab5_assist_reponse`, `tab5_maj_reponse_vocale`) are out of its scope by design (they need live HA entities, a calendar or a voice pipeline).

## Code rules (full detail in `Tab5/README.md`)

1. No hardcoded hex colors in YAML/lambdas — add a token to `UIColor::` (`Tab5/tab5_tokens.h`, included by `tab5_custom.h`).
2. `sensor:`/`text_sensor:` never touch `lv_obj_*` directly — always call a named C++ function of the C++ layer (`Tab5/tab5_*.cpp`, declared in `tab5_custom.h`).
3. No `static` inside a lambda for state shared across handlers — use a `globals:` entry instead.
4. No `std::string` by value or `to_string()` in a hot path (sliders, frequent `on_value`) — use `const std::string&` or a `snprintf` buffer.
5. Any widget/card repeated 3+ times goes through a parametrized C++ builder or `!include`+`vars` template, not copy-pasted YAML.
6. `esphome compile` must pass before committing.
7. Every modal popup reuses the shared chrome (`modal_scrim.yaml` + `modal_header.yaml`, ADR-0009) — games are the documented exception.
8. No hardcoded Home Assistant entity ID in firmware YAML — always a `user_entities.yaml` substitution (`${entity_…}`) or a `!lambda`. Since [ADR-0025](docs/decisions/0025-events-only.md) the firmware names none at all and **never calls a Home Assistant action** (no `homeassistant.service` / `homeassistant.action`): it emits `esphome.tab5_*` events, and `HomeAssistant_Config/packages/tab5_evenements.yaml` maps them to a whitelist of actions on the tablet's own entities (found by its device). A new request = an event + a branch there. Enforced by `tools/check_tab5_code_rules.py` and `tests/test_actions_ha.py`.
9. Every MDI icon shown on screen must be in the `glyphs:` list of its widget's `mdi_*` font (`Tab5/tab5-styles.yaml`), and every glyph listed there must be shown somewhere — a missing glyph renders blank with no build error. An icon set from C++ on a widget passed as a parameter needs its function in `MDI_CODE_TARGETS` (`tools/check_tab5_code_rules.py`, rule 7, run by `pytest`).

## Product preferences (the author's taste — keep them)

Not enforced by a test, but every change is judged against them on the real screen:

- **Instant transitions, no fade** on popups, pages and buttons. The only animation kept on purpose is the centre card rotation (short slide + fade, kept by the author on 2026-09-29 after the "text only" trial).
- **Popups to the pixel**: same glass everywhere, the whole useful area used, nothing overflowing or clipped.
- **French accents and "°" always**, on screen and in the voice answers; an MDI icon never shows as an empty rectangle (rule 9).
- **Voice and screen say short sentences**, never an entity ID, a code or a raw HA state.
- The screen must come back on by itself after an OTA or a reboot.
- **Single source**: one reused component or builder rather than N copies (rule 5).

## Boundaries — do not

- Do not read or write `secrets.yaml` or a signing key (`tab5_signature.pem`, any `*.pem`/`*.key`; repo root, gitignored). Since 3.0 (lot 6b, [ADR-0020](docs/decisions/0020-no-secret-firmware-signed-ota.md)) the firmware reads no `!secret` at all — a `secrets.yaml` left from 2.x is only used once, by `tools/migrer_vers_3.py`, to send the first 3.0 firmware with the old API key. Never add a `!secret` back (`tests/test_sans_secret.py` fails). The API key lives in Home Assistant: tools that talk to the tablet get it through `tools/tab5_cle_api.py` and never print it.
- Do not read or write `Tab5/user_entities.yaml` (gitignored — your real HA entity IDs). Edit `Tab5/user_entities.example.yaml` only when changing the public template or adding a new substitution key.
- The HA side has **one source**: the tracked packages. Since 2026-09-26 (lot 3 of the « ouverture » audit) Axel's Home Assistant runs `packages/tab5_push.yaml` (push automations, `tab5_push_*` scripts, device-called scripts, rain template, `is_primary_active`), the other packages and `optionnel/volet_serre_tracking.yaml` — as they are since 2026-09-28. These automations and scripts are read-only in the HA UI: change the package, deploy (Samba share of HA's `config/packages/`, `.bak` first, `check_config`, reload). Never edit them with `ha_config_set_automation` / `ha_config_set_script`. The former private copies (`automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`) and the three `*_examples*` files are gone; they used to drift (on 2026-09-26 the public shutter package had never run anywhere).
- Public HA files (`HomeAssistant_Config/packages/`, `optionnel/`, `snippets/`, `custom_templates/`) contain **no placeholder and no real value** ([ADR-0024](docs/decisions/0024-packages-without-placeholders.md), 2026-09-28): every home value is a « Tab5 · … » list picked in HA (`packages/tab5_reglages.yaml`), the tablet is detected by its device model (`sensor.tab5_tablette`), provider entities by `sensor.tab5_sources_meteo`. Never write an entity ID of a real home, a `VOTRE_…` placeholder or a `!secret` back. The gitignored `HomeAssistant_Config/placeholders.yaml` (template: `placeholders.example.yaml`, CI secret `HA_PLACEHOLDERS`) lists the author's real values: `python tools/render_ha_config.py --check` fails if one of them, or a placeholder, lands in a public file (it never prints the value — CI-safe). `render_ha_config.py` without `--check` only copies to `rendered/`. An entity read by a package must be defined by one (`tests/test_installation_ha.py`: template entity IDs come from their names). Releases attach `tab5_home_assistant.zip` (`tools/publication/archive_ha.py`); the blind-shutter package stays in `optionnel/` (while `script.tab5_volet_action` exists, the blueprint hands it the shutter).
- **Screen language (2026-09-27):** a new text on the screen is written in French and goes through `tr()` (C++, lambdas, games included) or is laid out by the YAML; a text kept in a table is marked `tr_noop("…")` and translated where shown, `tr(table[i])`; the literal stays on the same line as the call; add its line to `Tab5/lang/en.yaml` and run `python tools/gen_i18n.py` — `tests/test_i18n.py` fails otherwise. Never translate what Home Assistant reads (entity names, select options, text-sensor states, payload codes); a HA value shown on screen is translated at display time. See `docs/translations.md`.
- When you add or remove a variable on an `api: services:` entry in `tab5-api-logic.yaml`, you are changing a public contract with **three** callers, not one: the HA package (`HomeAssistant_Config/packages/tab5_push.yaml`, then deploy the rendered copy), the demo pusher (`tools/demo/`), and the service table in `Tab5/README.md`. Update all of them in the same PR.
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
