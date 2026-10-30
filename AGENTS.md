# AGENTS.md — instructions for AI coding agents

This file is the entry point for any AI agent (Claude, Codex, Cursor, Copilot, Gemini CLI, or a human skimming fast) working on this repository. Keep it short; when it drifts from the code, the code wins — fix this file, don't work around it.

The detail lives next to its code, in one `AGENTS.md` per folder (a `CLAUDE.md` beside it imports it: Claude Code loads it only once it reads or edits a file there): [`Tab5/`](Tab5/AGENTS.md) (layout, code rules in full, screen language, API contract), [`Tab5/socle/`](Tab5/socle/AGENTS.md) and [`Tab5/ecran/`](Tab5/ecran/AGENTS.md) (toolboxes), [`Tab5/jeux/`](Tab5/jeux/AGENTS.md) (games), [`HomeAssistant_Config/`](HomeAssistant_Config/AGENTS.md) (HA side), [`tools/`](tools/AGENTS.md) (CI, tests, demo, 7B, site).

## What this project is

A Home Assistant dashboard running natively as ESP32-P4 firmware (ESPHome + LVGL 9.5 — not 8.x: check `lv_version.h` in the build tree before picking an API) on a M5Stack Tab5 V2. Push-only: the device never polls, Home Assistant automations push data via ESPHome API service calls. No web stack, no browser. See [`README.md`](README.md) for the full picture.

## Read this before touching anything

In order, before editing code or answering questions about architecture:

1. [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md) — full dependency graph, file-by-file inventory, and a verified list of known technical debt / dead code.
2. [`Tab5/README.md`](Tab5/README.md) — file-by-file description of the ESPHome packages, the HA service contract table, the globals table, and **10 mandatory code rules**. Layout of `Tab5/`: [`Tab5/AGENTS.md`](Tab5/AGENTS.md). The games: [`docs/arcade.md`](docs/arcade.md), only when touching a game.
3. [`docs/decisions/`](docs/decisions/README.md) — why non-obvious architectural choices were made. Check here before "fixing" something that looks wrong.
4. [`docs/troubleshooting.md`](docs/troubleshooting.md) — incidents already diagnosed on this exact device. Check here before re-diagnosing a symptom that looks familiar.
5. The target file itself, including its `[AI-CONTEXT]` header (see below), and the `AGENTS.md` of its folder.

## The `[AI-CONTEXT]` / `[AI-WARNING]` / `[AI-DEBUG]` convention

Most firmware files open with an `[AI-CONTEXT]` block (role, constraints, `@ai_instruction` notes). **`[AI-WARNING]`** marks a deliberate, tested fix that looks like a bug: **read the warning before "fixing" it**. **`[AI-DEBUG]`** marks a good runtime observation point ([`docs/debugging.md`](docs/debugging.md)). A new non-obvious constraint goes into the `[AI-CONTEXT]` block, not only into a commit message. In full, with examples: [`Tab5/AGENTS.md`](Tab5/AGENTS.md).

## Build & verify

```bash
python -m esphome compile tab5-ha-hmi.yaml                             # must pass before any commit
python -m esphome -s tab5_carte waveshare-7b compile tab5-ha-hmi.yaml  # Waveshare 7B, ESPHome 2026.9.1 at least
pip install -r requirements-dev.txt                                    # once
python -m pytest                                                       # every test and guard
pre-commit run --all-files                                             # the hooks CI replays
```

- A file of `Tab5/ui_components/` changed: `esphome clean` before the next `esphome run`. A behaviour-neutral refactor: same `config_hash` before/after. Detail: [`Tab5/AGENTS.md`](Tab5/AGENTS.md).
- **OTA-flashing the real device only if explicitly asked**, then prove it by the device's own entities ([`Tab5/AGENTS.md`](Tab5/AGENTS.md)).
- `python`, `build` and `build-min` are required checks; `build` reports "skipped" (= success) on docs-only changes. CI, every guard, the host engine tests, the demo dry-run and the 7B: [`tools/AGENTS.md`](tools/AGENTS.md).

## Code rules (full detail in `Tab5/README.md`)

In full: [`Tab5/AGENTS.md`](Tab5/AGENTS.md).

1. No hardcoded colour: a `struct Palette` role (`Tab5/socle/tab5_tokens.h`), `UIColor.X`, role styles; games `PALETTE_SOMBRE.X`.
2. `sensor:`/`text_sensor:` never touch `lv_obj_*` — call a named C++ function of `Tab5/ecran/`.
3. No `static` inside a lambda for state shared across handlers — a `globals:` entry.
4. No `std::string` by value or `to_string()` in a hot path — `const std::string&` or `snprintf`.
5. A widget/card repeated 3+ times goes through a C++ builder or an `!include`+`vars` template.
6. `esphome compile` must pass before committing.
7. Every modal popup reuses the shared chrome (`modal_scrim.yaml` + `modal_header.yaml`, ADR-0009); games excepted.
8. No Home Assistant entity ID and no HA action in the firmware: `esphome.tab5_*` events only (ADR-0025).
9. Every MDI icon shown is in its font's `glyphs:`, every glyph listed is shown (`MDI_CODE_TARGETS`).
10. Our C++ is called from a `lambda:` action, never from an event field or an "apply" action value (ESPHome 2026.10).

## Toolbox (« boîte à outils »): reuse before writing

Before writing a helper, look in the toolbox of [`Tab5/socle/`](Tab5/socle/AGENTS.md) (pure), [`Tab5/ecran/`](Tab5/ecran/AGENTS.md) (LVGL) or [`Tab5/jeux/`](Tab5/jeux/AGENTS.md), and `grep` the name; when you add a shared helper, add its line there.

## Product preferences (the author's taste — keep them)

Not enforced by a test, but judged on the real screen:

- **Instant transitions, no fade** on popups, pages and buttons. The only animation kept on purpose is the centre card rotation (short slide + fade, [`Tab5/ecran/AGENTS.md`](Tab5/ecran/AGENTS.md)).
- **Popups to the pixel**: same glass everywhere, the whole useful area used, nothing overflowing or clipped.
- **French accents and "°" always**, on screen and in the voice answers; an MDI icon never shows as an empty rectangle (rule 9).
- **Voice and screen say short sentences**, never an entity ID, a code or a raw HA state.
- The screen must come back on by itself after an OTA or a reboot.
- **Single source**: one reused component or builder rather than N copies (rule 5).
- **Set from Home Assistant, with the mouse**: a new choice goes into the blueprint or a « Tab5 · » list, with a help sentence, rather than a substitution to compile.
- **Wheels and pages**: a quick action goes on a wheel ([ADR-0036](docs/decisions/0036-quick-action-wheel.md)), a popup with several views gets pages swiped left / right ([ADR-0046](docs/decisions/0046-popups-a-pages.md)).
- **Prettier first**: a visible improvement wins over a small performance gain; measure before trading one for the other.

## Boundaries — do not

- Do not read or write `secrets.yaml` or a signing key (`tab5_signature.pem`, any `*.pem`/`*.key`; repo root, gitignored); never add a `!secret` back ([`tools/AGENTS.md`](tools/AGENTS.md)).
- Do not read or write `Tab5/user_entities.yaml` (gitignored — your real HA entity IDs); edit `Tab5/user_entities.example.yaml` instead.
- HA side: change the tracked package and deploy, never `ha_config_set_*`; no placeholder nor real value in a public HA file ([`HomeAssistant_Config/AGENTS.md`](HomeAssistant_Config/AGENTS.md)).
- A new screen text is written in French through `tr()`; what Home Assistant reads stays in English ([`Tab5/AGENTS.md`](Tab5/AGENTS.md)).
- A variable added to or removed from an `api: services:` entry changes a public contract: update every caller in the same PR ([`Tab5/AGENTS.md`](Tab5/AGENTS.md)).
- At most a couple of ESPHome CLI processes against the device at once: its API has 8 connection slots ([`tools/AGENTS.md`](tools/AGENTS.md)).
- Do not "clean up" code flagged `[AI-WARNING]` without reading the warning and checking `docs/decisions/`.
- Do not touch `Tab5/tts_library*/` (gitignored, experimental, unused).

## Workflow

- Branch + PR against `main`, never push directly. Use [`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md)'s checklist.
- User-visible or architectural change: a fragment `changelog.d/<branch>.md`, never `CHANGELOG.md` itself ([format](changelog.d/README.md)). A new ADR: number from `python tools/prochain_adr.py`.
- If a change alters the file/dependency structure described in `CARTOGRAPHIE_TAB5.md`, update that file in the same PR (counts in conflict after merging `main`: `python tools/cartographie_counts.py --write`).
- Commit messages and code comments mix French and English (the author is French, the repo is public/bilingual) — match the style of the file you edit.
- Documentation by task: [`docs/README.md`](docs/README.md); the site's menu and `HORS_SITE`: [`tools/AGENTS.md`](tools/AGENTS.md).
