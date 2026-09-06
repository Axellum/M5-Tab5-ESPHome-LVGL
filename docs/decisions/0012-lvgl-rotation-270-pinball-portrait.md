# ADR-0012: Landscape dashboard via `rotation: 270`, one console flips to portrait at runtime

**Status:** Accepted (2026-07-28, "Neon Apron")
**Date:** 2026-09-06 (written retroactively from the `[AI-CONTEXT]`/`[AI-WARNING]` blocks in `Tab5/tab5-styles.yaml`, `Tab5/pinball_game.h` and `Tab5/tab5-scripts.yaml`)

## Context

The Tab5 panel is natively **portrait 720×1280** (`display: mipi_dsi: dimensions: height: 1280, width: 720`). The dashboard is landscape, so the LVGL component is declared with `rotation: 270` in `tab5-styles.yaml`. ESPHome then rotates in software on every flush (rotation buffer + PPA client). The pinball console ("Neon Apron", replacing the deleted landscape "Flip Noir") only makes sense held upright, like an arcade cabinet.

## Decision

1. `rotation: 270` **stays** in the `lvgl:` block, and the block carries an explicit `id: tab5_lvgl`. ESPHome only compiles rotation support when the key is present in the config; without it `LvglComponent::set_rotation()` logs a warning and does nothing. The `id` is the only way to reach the component from a lambda.
2. `Pinball::open()` calls `set_rotation(0)` (LVGL becomes 720×1280) and `Pinball::close()` restores `set_rotation(270)`. The pointer is injected by `script.tab5_pinball_open`.
3. Touch needs nothing: `LVTouchListener` applies `rotate_coordinates()` using the *current* rotation on every indev read, so the native 720×1280 calibration in `tab5-hardware.yaml` is valid in both orientations.
4. `tab5_games_close_all` closes Pinball **last**, so the landscape restoration always has the last word if several closes are chained.
5. An "upside-down" toggle (0 ↔ 180) is a persisted game setting: there is no universal right way to hold the tablet.

## Consequences

- Portrait is *cheaper* than the dashboard: at `rotation: 0` the flush path takes ESPHome's `default:` branch with no software rotation and no PPA pass. The `lvgl took a long time` warnings seen at boot are the landscape cost, not a game bug.
- Removing the `rotation:` key to "simplify" would break the pinball silently (warning only) — hence the `[AI-WARNING]` on the key.
- Any future console that changes orientation must also close after Pinball in `tab5_games_close_all`, and must restore 270 itself; the dashboard never re-applies its rotation on its own.
- The game page is declared `skip: true` like every game page (ADR-0002 update): a stray swipe must never land on a portrait page whose rotation and timer are not set up.
