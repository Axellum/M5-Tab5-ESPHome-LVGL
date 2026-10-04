# ADR-0029: Themes — one C++ palette, role styles in the YAML, games stay dark

**Status:** Accepted (2026-10-04, lot 1 « fondation » asked by the author: no visible change; the light palette and the theme selector are lot 2)
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

## Consequences

- One source for the colours of the interface (the palette), and one shared style per role that a theme can repaint in one call.
- Lot 2 adds a palette: a struct instance, plus the glass pre-mix recomputed for its background (formula in `tab5_tokens.h`), and contrast choices (the vigilance yellow on a light background, `couleur_lisible()` darkening instead of lightening, the pressed opacity).
- What lot 2 still has to repaint on a switch without a restart (lot 3): the colours set by the C++ (local properties, set at each push), the calendar's C++ styles, and the pages' background (local). With a restart, nothing: everything is created from the active palette. Also for lot 3: `ui_text_color()` (`tab5_internal.h`) compares the new colour with the local property only, so the first push of a colour equal to the role style's sets a local property and the widget stops following its role style; compare the effective colour (`lv_obj_get_style_text_color`) instead.
- `UIColor.X` costs a load instead of an immediate; the palette is 220 bytes of RAM.
