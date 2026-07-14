# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Dates are the day each pull request was merged into `main`.

## [Unreleased]

### 2026-07-14 — Central card info panel (`tab5_maj_info_texte`) + forecast swipe rework
- Implemented the `tab5_maj_info_texte` API service (empty stub since April): new `info_wrapper`/`lbl_info_text` 4th panel in the central card rotator, showing the 3-day calendar recap (recolor markup, `roboto_22`) or a Rouge/Orange weather-alert banner (`roboto_32_b`, colored by the `couleur` variable) sent by `automations_tab5.yaml` section 7. LVGL updates factored into `update_info_text_ui()` (per `tab5_custom.cpp` rule).
- `show_temporary_planning()` now restores the previously active panel (4-state static helper) and also hides the info panel during the 6 s temporary display.
- Forecast swipe rework: swipe zone limited to the central card band (`y >= 333`), console overlay now opened via `btn_control_console` only (no more up/down swipe); page title overlay (`page_title_wrapper`) shown on non-home forecast pages, day tiles titled "Lun 16" via SNTP on daily pages 2-3.
- UTF-8 accent fix: static strings use proper UTF-8 escapes (`\xC3\xA9` not Latin-1 `\xE9`); vigilance alert banner generated in firmware; `normalize_text_utf8()` for dynamic HA strings (Latin-1/mojibake); helpers `update_clock_date_ui()`, `update_rain_phrase_ui()`, `update_planning_text_ui()`.
- Correction of the 2026-07-12 note below: the service **is** called from HA (Tab5 automation section 7); its removal had already been reverted as a stub by the reboot fix.

### 2026-07-12 — Stabilite reboot 60s + reintegration progressive (#T220–#T226)
- Fix reboot ~60s : retrait `buffer_size` LVGL, planning au tap en C++ (`show_temporary_planning`), stub `tab5_maj_info_texte`.
- Garde visibilite capteurs console (#T222) : pas de MAJ LVGL si overlay masque.
- Durcissement ABI animation rotateur (#T225) : `anim_y_cb` statique.
- Migration complete `UIColor::` / tokens YAML (#T226) : API, meteo, console, popups ; hex restants limites aux presets couleur lumiere HA.

### 2026-07-12 — Entity substitutions split (user_entities.yaml)
- Home Assistant entity IDs moved out of `tab5-ha-hmi.yaml` into a local `Tab5/user_entities.yaml` (gitignored, same pattern as `secrets.yaml`).
- Added tracked template `Tab5/user_entities.example.yaml` with generic placeholder entity IDs for public repo and CI.
- CI workflow copies the example file before compile. `.gitignore` extended for `__pycache__/` / `*.pyc`.

### 2026-07-12 — Concours polish (docs & API cleanup)
- Refreshed `CARTOGRAPHIE_TAB5.md` §4 (resolved debt, current line counts).
- Removed unused stub API service `tab5_maj_info_texte` (never called from HA).
- Added `docs/images/gpio_pinout_table.png` and `push_only_architecture_diagram.png` to architecture/hardware docs.
- README CI badge, `CONTRIBUTING.md`, `docs/architecture.md` updated for `user_entities.yaml`.

### 2026-07-12 — Bug planning tuiles météo + console diagnostic
- Fixed wrong day index on min/max temperature tap (`(forecast_page_index - 2) * 5 + idx`).
- Added `get_day_planning_display_text()` with fallback when opening hours are empty.
- Console overlay: new header icon (`mdi-console-line`), layout rework (SRAM/PSRAM bars, aligned labels).
- Extended `UIColor::` in `tab5_custom.h` for weather icons, rain bars, alert date pastels.
- Migrated remaining hardcoded hex in `tab5_custom.cpp` and `tab5-api-logic.yaml`.
- Removed dead commented block in `tab5_maj_meteo_actuelle`.

## [1.0.0] — 2026-07-06 — first tagged release

This is the first version tagged in git. It was cut here rather than retroactively at the earlier "v1 stable" checkpoint (PR #6) because everything since has made the project strictly more stable and more complete: a confirmed (not just worked-around) fix for the black-screen-after-reboot bug, several rounds of factorization, technical-debt cleanup, and — in this same release — the addition of `AGENTS.md`, `docs/decisions/`, `docs/troubleshooting.md`, `docs/debugging.md`, and this changelog itself.

This is a personal, "100% AI-generated" project (see the README's "Note on AI"): stable and in daily use, but not aesthetically polished, not manually code-reviewed line-by-line, and with known open items — see [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md) §4 for the current, honestly-tracked technical debt. Tagging `1.0.0` here means "stable enough to be a reference point," not "finished" or "audited to a professional standard."

### 2026-07-06 — AI-agent documentation layer ([#22](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/22))
- Added `AGENTS.md` — entry-point instructions for AI coding agents (build/verify commands, read order, boundaries).
- Added `.github/PULL_REQUEST_TEMPLATE.md` — checklist covering compile verification, OTA testing, `[AI-WARNING]` review, and doc upkeep.
- Added `docs/troubleshooting.md` — symptom → root cause → fix log for incidents already diagnosed on this device.
- Added `docs/decisions/` — retroactive architecture decision records (push-only design, single-page navigation, data packing, boot delay, etc.).
- Added `docs/debugging.md` and the `[AI-DEBUG]` tag convention (alongside `[AI-CONTEXT]`/`[AI-WARNING]`) in `Tab5/README.md`.
- Fixed `docs/architecture.md`, which still described a multi-page tab-bar layout (`page_accueil`/`page_meteo`/.../`tab_bar`) that no longer matched the shipped firmware (single `page_main` + popups + swipe). Corrected in both language versions.

### 2026-07-06 — scripts & cartography ([#20](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/20), [#21](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/21))
- AI-CONTEXT headers and `continue_on_error` resilience added to the HA-side example scripts.
- Added `CARTOGRAPHIE_TAB5.md`, the full dependency-graph/file-inventory reference; cleaned up leftover IA scripts and drafts.

### 2026-07-06 — technical debt cleanup ([#15](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/15))
- Removed a tracked backup snapshot (`Tab5_backup_20260525/`, including committed `.pyc` files) and an orphaned `tab5-images.yaml`.
- Removed dead code (`my_components/st7123/binary_sensor/`, a write-only array) and hardcoded hex colors in `tab5_maj_clim`, replaced with `UIColor::` tokens.

### 2026-07-06 — black screen root cause confirmed ([#13](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/13))
- Confirmed root cause of the "black screen after software reboot" bug (the display reset pin runs through an I2C GPIO expander that needs a settle delay after boot) and applied the real fix, superseding the earlier `VERY_VERBOSE`-logging workaround.

### 2026-07-06 — climate popup factorization ([#12](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/12))
- Factorized 6 of the 9 climate popup grid buttons into parametrized templates (task #T164). The remaining 3 + the two temperature +/- buttons were deliberately left as-is — see [ADR-0007](docs/decisions/0007-climate-popup-not-factorized.md).

### 2026-07-06 — buffer fix & UI polish ([#10](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/10), [#11](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/11))
- Weather-alert payload buffer widened 512→1024 bytes to prevent silent truncation of long alert text; `strip_prefix` passed by reference.
- Pressed-state visual feedback added to "glass" buttons; central-panel carousel slowed 6s→8s.

### 2026-07-05 — dedup pass ([#7](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/7), [#8](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/8), [#9](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/9))
- Recovered an orphaned commit fixing the black-screen mitigation and a dead climate entity; deduplicated moisture/light-state sensors.
- Factorized `forecast_daily.yaml`, `switches_card.yaml`, and the light popup's 8 color preset buttons.

### 2026-07-05 — "v1 stable" checkpoint ([#6](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/6))
- Fixed a critical reboot crash (uninitialized `reinterpret_cast`), removed dead weather code, added volume slider debounce, hardened network boot (timeout, conditional heartbeat), fixed a phantom climate entity, and fully rewrote `Tab5/README.md` (the previous version described an unrelated, outdated project iteration). This was the project's own internal "v1 stable" milestone at the time, ahead of an actual git tag.

### Earlier — initial build-out ([#1](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/1)–[#5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/5))
- Initial audio/image asset libraries and ESPHome configuration (styles, climate card, OOM buffer guards).
- Systematic nullptr guards across API services (fixed a reboot loop caused by HA pushing data before LVGL widgets existed).
- Service API params switched to string+`atof`/`atoi` to tolerate non-numeric Jinja values.
- CI fixed by generating a dummy `secrets.yaml` before the ESPHome build.
- OTA reboot fix on ESP32-P4, automation examples updated.
