# ADR-0029: Themes — one C++ palette, role styles in the YAML, games stay dark

**Status:** Accepted (2026-10-04, lot 1 « fondation » asked by the author: no visible change). Lot 2 (2026-10-04): theme catalogue, light mode, live switch, Auto mode — see « Decision (lot 2) ».
**Date:** 2026-10-04

## Context

The author wants themes, with a dark and a light mode. Before this lot the interface could not change colour at run time, and not for a lack of tokens:

- **ESPHome writes a YAML colour into the generated C++ as a literal.** `text_color: color_text_dim` becomes `lv_color_make(148, 163, 184)` in `main.cpp`; the `color:` component is fixed at compile time. 483 colours were set that way, widget by widget (as local style properties), against 12 shared styles.
- **The C++ read `constexpr` tokens** (`UIColor::TEXT_DIM`, 282 uses): fine for one palette, impossible to switch.
- The colours were written twice, in `tab5-styles.yaml` (`color:`) and in `tab5_tokens.h`, with a comment asking to keep them « in sync by hand ».

LVGL 9.5 already has the right tool: a shared `lv_style_t` changed in place, then `lv_obj_report_style_change()`, repaints every widget that uses it. ESPHome 2026.9 wraps it (`lvgl.style.update`, `lvgl.theme.update`) and accepts a lambda wherever it accepts a colour, `style_definitions` and `theme:` included.

Three ways were weighed with the author:

- **A — one firmware per theme** (a substitution picks the hex values). No code change, but no switch from Home Assistant, no automatic day/night, two binaries to publish (ADR-0022).
- **B — recolour after creation by value** (walk every widget, map each dark value to its light one). Small diff, but two roles that share a value (INFO and TEMP_MIN are both sky-400) could never differ, and the games would be recoloured by accident. Rejected.
- **C — one palette, shared role styles, the C++ reading the active palette.** Chosen, in three lots: 1) foundation, the dark render unchanged; 2) the light palette and a « Thème » select, a change restarting the tablet like the language does; 3) optional, a live switch (the C++ colours through shared styles too) and an automatic day/night mode.

## Decision (lot 1)

- **`struct Palette`** (`Tab5/tab5_tokens.h`, still dependency-free): one field per colour role (background, glass, text, semantic, vigilance, climate, rain, weather icons), 55 roles. `PALETTE_SOMBRE` holds today's values, unchanged. Two roles with the same value stay two roles (INFO / TEMP_MIN, INACTIVE / BAR_INACTIVE): another theme may split them.
- **`UIColor` is the active palette**, a variable (`inline Palette UIColor = PALETTE_SOMBRE;`, constant-initialised, no static-init order issue). The C++ and the YAML lambdas read `UIColor.TEXT_DIM` instead of `UIColor::TEXT_DIM`. A `constexpr` table that must follow the theme keeps a pointer to member (`&Palette::TEXT_PRIMARY`, read through `UIColor.*field`) instead of a value: the weather icon table does.
- **The styles read the palette.** Every colour of `style_definitions`, of the `theme:` (labels) and the pages' background is a lambda `return lv_color_hex(UIColor.X);`, evaluated when LVGL creates the style. The interface colours left the `color:` section: only the games' and the arcade selector's remain.
- **33 role styles** (`style_<property>_<role>`: `style_text_dim`, `style_bg_accent`, `style_border_error`, `style_arc_info`…), one colour on one property each. A widget of the interface takes its colour through `styles:` instead of setting `text_color:` on itself; in a `styles:` list the role style comes **last**, so it wins over the styles before it, exactly like the local colour it replaces (a local property beats every shared style; among shared styles the last added wins). Templates that received a colour id now receive a role style (`modal_header.yaml` and `tv_transport_btn.yaml`: `icon_style`; `tv_app_btn.yaml`: `style`). A bulb's own colour (`light_popup` presets, `light_white_btn`) is data, not theme: it stays as it is.
- **The games stay dark** (ADR-0014): they read `PALETTE_SOMBRE.X`, never `UIColor.`, and keep their own palettes and their `color:` entries.
- **Guards.** `tools/check_tab5_code_rules.py` rule 8: outside the games, a LVGL colour property (`text_color:`, `bg_color:`…) is only a lambda or a template variable; a colour declared in `color:` is used by the games only; a game does not read `UIColor.`. `tests/test_themes.py`: every palette gives every role, in the order of `struct Palette` (an omitted field would be 0x000000 without a word from the compiler); the Météo-France vigilance colours are the official ones in every palette; every colour of the styles reads a field of `Palette`; each role style has one property and reads its own field; no role style is dead.

## Proof

- The migration of the 33 YAML files was checked tree against tree: the HEAD tree, transformed by the rule above, equals the new tree, file by file.
- The off-device render (ADR-0021) compares every screen of the pull request with the last run of `main`, in the seven languages: the dark render must be identical to the pixel.

## Decision (lot 2) — catalogue, live switch, Auto mode

The plan of option C was revised with the author on 2026-10-04: **the switch is live** (no restart, what C left for a lot 3), **an Auto mode** follows day and night, lot 3 becomes **fonts for a few themes** (display texts only, the seven languages covered) and lot 4 **the twelve themes**, by batches of four, chosen by the author on a render gallery. Themes stay compiled into the firmware (option A of the second discussion, against downloadable themes): no parser, no file system, every palette checked by the tests.

- **Catalogue.** One file per theme, `Tab5/themes/<name>.yaml`: `nom`, `ordre`, a `sombre:` and a `clair:` mode, each giving every role of `struct Palette` (`herite: <file>` starts from another theme; the glass pre-mixes are computed when omitted, like `lv_color_mix()`). `tools/gen_themes.py` writes `THEMES[]` (`struct Theme {nom, sombre, clair}`) into `tab5_tokens.h`, the options of the « Thème » select and the repaint of the shared styles into `tab5-themes.yaml`, between `>>>` / `<<<` marks; `--check` runs in pytest. `PALETTE_SOMBRE` is `THEMES[0].sombre`, the games' palette. Adding a theme is one YAML file and one command, no C++.
- **Entities** (`Tab5/tab5-themes.yaml`): the « Thème » select (its INDEX is kept, so a theme is only ever added at the end, `ordre:`), the « Clair ou sombre » select (Sombre, Clair, Auto) and the « Nuit (thème auto) » switch. In Auto the screen is light unless the switch is on; Home Assistant turns it on and off with `sun.sun` (automation « Tab5 — thème jour/nuit », `packages/tab5_push.yaml`; the switch is found through the `theme_nuit` attribute of `sensor.tab5_tablette`). Without Home Assistant, Auto keeps the last known state. Home Assistant reads the options, so they are never translated; on screen the mode is translated, the theme name is a proper noun written as in Home Assistant.
- **On the tablet:** the GESTION card of the system console gets a « Thème » row: one button moves to the next theme, the other to the next mode (`select.next`, the same path as Home Assistant). The four existing buttons are 68 px high instead of 86 to make room.
- **Live switch.** `theme_selectionner()` (`tab5_theme.cpp`) copies the chosen palette into `UIColor` and tells whether it changed (no repaint otherwise). `tab5_theme_repeindre` then (1) sets every colour of the shared styles again from `UIColor` and calls `lv_obj_report_style_change()` on each (what `lvgl.style.update` does), updates the label theme (`lvgl.theme.update`), the main page background and the calendar's C++ styles; (2) waits for `App.is_setup_complete()`; (3) calls `theme_rejouer_ui()`: every C++ unit repaints, from its last state, the colours it set itself as local properties (central card, vigilance, rain, tiles and light popup, measures, climate and plants, energy, zones, voice assistant, calendar detail, weather tiles), and the YAML repaints the icons its sensors colour. A unit that sets a palette colour itself must have such a replay (`[AI-CONTEXT]` of `tab5_theme.cpp`).
- **Boot order, found while building lot 2.** ESPHome 2026.9 creates the styles, pages and widgets in `main()`, *before* `App.setup()`; the selects and the switch restore their value during setup (HARDWARE priority). A saved light theme is therefore applied to an interface already built in the dark palette — exactly a live switch, hence step (2): a unit replays only once it has its widgets. The `on_boot` sequence is not touched. The games' pages keep the `bg_color` of `lvgl:`, evaluated at creation: they stay dark.
- **Twelve new roles** (67 in all, 268 bytes per palette), for colours that were still literals in the C++ or that a light palette must split: `TEXT_ON_ACCENT`, `ALERT_ORANGE` (the orange vigilance level had borrowed `WARNING`), `METEO_CLOUD`, and the stops of the temperature and humidity gradients (`TEMP_GRAD_*`, `HUM_GRAD_*`).
- **Contrast.** Every mode keeps the text at 7:1 at least, the secondary text at 4.5:1 and the semantic colours at 3:1 on the four card surfaces (`tests/test_themes.py`, WCAG). A `TEXT_ON_ACCENT` role (style `style_text_on_accent`) colours the text of filled accent buttons (« Tester », « Parler », « OK »): unchanged in dark, 4.5:1 at least in light. In a light palette, `couleur_lisible()` darkens a too-light tile colour towards black instead of lightening it, and a vigilance icon is drawn in the text colour on a pastille of its official colour.

### Proof (lot 2)

- The render task « clair » (`.github/workflows/rendu-host.yml`) captures every scene and screen in French twice: painted in dark then switched live to light just before the capture (`capturer.py --bascule Clair`), and after a cold start in light (`--puis-mode-theme Clair`, then a restart with the same preferences). The two series must be identical to the pixel, or the task fails: a difference means a colour that the switch does not repaint.
- The dark render of the seven languages stays identical to `main`, except the system console (GESTION card re-laid out).


- One source for the colours of the interface (the palette), and one shared style per role that a theme can repaint in one call.
- Lot 2 adds a palette: a struct instance, plus the glass pre-mix recomputed for its background (formula in `tab5_tokens.h`), and contrast choices (the vigilance yellow on a light background, `couleur_lisible()` darkening instead of lightening, the pressed opacity).
- What lot 2 still has to repaint on a switch without a restart (lot 3): the colours set by the C++ (local properties, set at each push), the calendar's C++ styles, and the pages' background (local). With a restart, nothing: everything is created from the active palette. Also for lot 3: `ui_text_color()` (`tab5_internal.h`) compares the new colour with the local property only, so the first push of a colour equal to the role style's sets a local property and the widget stops following its role style; compare the effective colour (`lv_obj_get_style_text_color`) instead.
- `UIColor.X` costs a load instead of an immediate; the palette is 220 bytes of RAM.
- Lot 2: each theme adds two palettes of 268 bytes to the flash (`THEMES[]`, `constexpr`); the active palette stays the only one in RAM. `ui_text_color()` was left as it is: the replays set the local colours again from the active palette, so a widget that left its role style still follows the theme.
