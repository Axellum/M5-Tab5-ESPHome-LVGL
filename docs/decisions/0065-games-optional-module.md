# ADR-0065: The games are an optional module, left out by default on a board without an IMU

**Status:** Accepted (2026-10-18, the author's request: « on vire les jeux du P7, pas de gyro et peu d'intérêt »; not yet tried on a board when written).
**Date:** 2026-10-18

## Context

The eight consoles (ADR-0012 to ADR-0014, [`docs/arcade.md`](../arcade.md)) were compiled into every firmware. The Waveshare 7B (ADR-0061) has no motion sensor: four of the eight games are played by tilting (Fil d'Or, Arcanoïde, Coureur d'Or, Neon Apron) and get no input there, and none of them is laid out for 1024×600 — that was the next lot of the port. The author has little use for them on that screen.

The project had also talked several times about building the firmware as a core plus add-ons. The analysis of 2026-10-08 (outside this repository) found the games the easiest first module: no Home Assistant entity or event, the IMU already reached through `GameRegistry::dispatch_imu`, a single console table (ADR-0013). What tied them to the rest: their C++ files listed in the `includes:` of both root configurations, the console table inside `tab5_registry.cpp`, their 9 LVGL pages in `tab5-lvgl.yaml`, their colours and the chess font in `tab5-styles.yaml`, the list of their pages translated at boot, and the selector page read by the auto-return.

Facts checked before deciding: ESPHome 2026.9.0 (`config_helpers.merge_config`, `packages.merge_packages`) puts list items end to end, packages in their declaration order and the root file last. A package can therefore bring its own `includes:` and LVGL pages (the 7B's board package already brings `includes:`), and pages of a package declared after `tab5_lvgl` follow `page_main`, which stays index 0.

Measured on 2026-10-08: about 371 kB of code and constants for the games, the firmware at 41 % of its slot. Removing them makes nothing faster; the gain is a smaller, simpler 7B firmware, and the 1024×600 lot of the games no longer needed.

## Decision

- **`Tab5/paquets/tab5-arcade.yaml` is the games' module**: their C++ files (`esphome: includes:`, with the console table now in `Tab5/jeux/jeux_registre.cpp`), their colours and the chess font, their 9 pages, the opening scripts and `tab5_jeux_traduire` (translation of its pages at boot).
- **Picked by a substitution, `tab5_jeux`** (`oui` / `non`, in `Tab5/user_entities.yaml` or `-s tab5_jeux …`): `oui` by default on the Tab5, `non` on any other board (all of them lack an IMU today). Both root configurations use the same expression.
- **Without the module, `tab5-arcade-absente.yaml`** gives the core the same plugs, empty: the scripts `tab5_arcade_open` and `tab5_jeux_traduire`, and an empty console table (`Tab5/jeux/jeux_absents.cpp`). `GameRegistry::disponibles()` is then false and the Arcade is no screen of the tablet (`ecran_sans_zone`): no gesture, no wheel entry and no request from Home Assistant opens it.
- **The core names no game**: it goes through `GameRegistry` (`any_open()`, `close_all()`, `disponibles()`…) and those two scripts. `tab5_games_close_all` moves to `tab5-navigation.yaml` (the core calls it). The auto-return reads « not on `page_main` » instead of the selector's page.
- **The core declares the LVGL widget types it creates in C++.** ESPHome enables a widget type (`LV_USE_LINE`…) only when the configuration declares one. The curves of the core (`ui_ligne()`: Energy, Temperature, Fridges, Tracked sensors, Weather, AI server) had been relying on the pinball page's hidden `line:`; the first 7B build without games failed on `lv_line_create`. The core now has its own hidden `lvgl_line_probe` (`tab5-lvgl.yaml`), and a test checks every `lv_<widget>_create` of the core against the core's YAML.
- **Without games, the gamepad button's « auto » tap opens the audio player** (the Music popup, ADR-0050), its note shown on the button; its long press stays the TV remote (author's choice, 2026-10-18: a button that does nothing would look broken). Both can be changed in the blueprint, whose choice still wins.
- **Kept honest by the CI**: `build` compiles the Tab5 with the module, `build-carte-7b` the 7B without it; `tests/test_module_jeux.py` checks the shared expression, the defaults per board, the plugs of both variants, that the module compiles every file of `Tab5/jeux/` and that no file outside it names a game page, script, namespace or font. The off-device render of the 7B skips the Arcade and the consoles (`tools/rendu/vers_carte.py`, `jeux_compiles`).

## Consequences

- The 7B firmware has no game code, page, colour or chess font; a 7B owner who wants them sets `tab5_jeux: oui` (the tilt games still get no input).
- A ninth console goes into the module only (its files, page, colours, opening script, and its page in `tab5_jeux_traduire`); the core does not change.
- The pattern for a next module: one package and its `-absente` twin with the same plugs, chosen by a substitution, one compile with and one without in the CI. Each further module is a lot of its own, decided when it brings something (the analysis judged Assist, weather and the central card too entangled to leave the core).
- The published firmware of the Tab5 is unchanged in content (same games); its configuration hash changes (lists merged in another order).
- Rejected: hiding the Arcade on the 7B and still compiling it (dead code and pages, the render still counting the games that overflow 1024×600); a `#ifdef` in the core (a compile flag in every file instead of one plug); turning the whole firmware into modules at once (10 to 13 PRs for a gain only measured on the next feature).
