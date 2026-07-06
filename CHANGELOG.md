# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Dates are the day each pull request was merged into `main`.

## [Unreleased]

### Added
- `AGENTS.md` — entry-point instructions for AI coding agents (build/verify commands, read order, boundaries).
- `.github/PULL_REQUEST_TEMPLATE.md` — checklist covering compile verification, OTA testing, `[AI-WARNING]` review, and doc upkeep.
- `docs/troubleshooting.md` — symptom → root cause → fix log for incidents already diagnosed on this device.
- `docs/decisions/` — retroactive architecture decision records (push-only design, single-page navigation, data packing, boot delay, etc.).
- `docs/debugging.md` and the `[AI-DEBUG]` tag convention (alongside `[AI-CONTEXT]`/`[AI-WARNING]`) in `Tab5/README.md`.
### Fixed
- `docs/architecture.md` described a multi-page tab-bar layout (`page_accueil`/`page_meteo`/.../`tab_bar`) that no longer matches the shipped firmware (single `page_main` + popups + swipe). Corrected in both language versions.

## [2026-07-06]
- `docs(ai)`: AI-CONTEXT headers + `continue_on_error` resilience added to the HA-side example scripts ([#20](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/20)); cartography integration and IA-scripts cleanup ([#21](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/21)).
- `chore`: deep cleanup of technical debt identified by the new cartography — removed a tracked backup snapshot, an orphaned `tab5-images.yaml`, dead `st7123` button code, and hardcoded hex colors in `tab5_maj_clim` ([#15](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/15)).
- `fix`: confirmed root cause of the "black screen after software reboot" bug — GPIO expander reset timing — and applied the boot-delay fix ([#13](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/13)).
- `refactor`: factorized 6 of the 9 climate popup grid buttons into parametrized templates (task #T164, [#12](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/12)).
- `fix`: weather-alert payload buffer widened 512→1024 bytes to prevent silent truncation; `strip_prefix` passed by reference (task #T165, [#11](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/11)).
- `feat`: pressed-state visual feedback added to "glass" buttons; central-panel carousel slowed 6s→8s (task #T166, [#10](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/10)).

## [2026-07-05]
- `refactor`: factorized the light popup's 8 color preset buttons ([#9](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/9)).
- `refactor`: factorized `forecast_daily.yaml` and `switches_card.yaml` (task #T164, [#8](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/8)).
- `fix`: black-screen-after-reboot mitigation, dead climate entity fix, sensor deduplication ([#7](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/7)).

## [1.0.0] — 2026-07-05 — "v1 stable"
- `fix(tab5)`: critical audit fixes and network robustness hardening, documentation brought up to date ([#6](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/6)). This PR fixed a critical reboot crash (uninitialized `reinterpret_cast`), removed dead weather code, added volume slider debounce, hardened network boot (timeout, conditional heartbeat), fixed a phantom climate entity, and fully rewrote `Tab5/README.md` (the previous version described an unrelated, outdated project iteration). Treated as the first stable checkpoint of the project — not tagged in git at the time; see the note below.

## [0.x] — pre-1.0
- `fix(tab5)`: OTA reboot fix on ESP32-P4, automation examples updated ([#5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/5)).
- `fix(ci)`: generate a dummy `secrets.yaml` before the ESPHome CI build, unblocking CI ([#4](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/4)).
- `fix(hmi)`: service API params switched to string+`atof`/`atoi` to tolerate non-numeric Jinja values ([#3](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/3)).
- `fix(hmi)`: systematic nullptr guards added across API services, fixing a reboot loop caused by HA pushing data before LVGL widgets existed ([#2](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/2)).
- `feat(tab5)`: initial audio/image asset libraries and ESPHome configuration (styles, climate card, OOM buffer guards) ([#1](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/1)).

---

**Note on the `1.0.0` label:** no git tag was created at the time PR #6 merged. The version above is assigned retroactively from this repo's own PR history to give this changelog a stable reference point. If you want an actual `git tag v1.0.0` pointing at `aa97858` (the PR #6 merge commit) for GitHub Releases, that's a manual step for the repo owner, not done as part of adding this file.
