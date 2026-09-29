# ADR-0026: The climate popup follows the device — its settings pushed by Home Assistant, the screen's commands translated by the blueprint

**Status:** Accepted (2026-09-29; not yet tried on the author's tablet)
**Date:** 2026-09-29

## Context

The climate popup was built for the author's Daikin Onecta, in °C: target bounded to 16-30 in the firmware, steps of 0.5, title « Climatisation Salon », and buttons named after that unit's modes — Éco is its preset `away`, Silence its fan mode `quiet`, Oscillation `swing` / `stop`, Brise `windnice`. The blueprint passed each value to Home Assistant as is. With another unit: a button the device does not know raises an HA error, a heat-only unit could not take a target while off (the blueprint forced `hvac_mode: cool`), a °F home saw 72 clamped to 30. And a toggle compared with one name only: a unit whose fan mode is `low` (or swing `on`, preset `eco`) got « quiet » at each tap and never left silence.

## Decision

- **The blueprint pushes the unit's settings** with the existing action `tab5_maj_emplacements`, new key `climr` (numbers with a decimal point; `|` → `/` and `;` → `,` in the name):

  ```
  climr|<min>|<max>|<step>|<unit>|<capabilities>|<name>
  ```

  `min`/`max` = `min_temp`/`max_temp` (16/30 without them); `step` = `target_temp_step`, else 1 in °F, 0.5 in °C; `unit` = `°C` or `°F`: the `temperature_unit` of a `weather.*` entity (it follows HA's unit system, as the climate attributes do), else `min_temp` ≥ 40 means °F; `name` = `friendly_name`. Sent just before `tab5_maj_clim` on (re)connection, automation reload, « MAJ Écran » and HA start, and when the climate changes one of `min_temp`, `max_temp`, `target_temp_step`, `*_modes`, `friendly_name` (or appears, or comes back from `unavailable`) — not at each degree of the room.
- **Capabilities** — one letter per screen button the unit can do (`tests/test_clim.py` compares this table, the blueprint and the firmware):

| Letter | Button | Present when |
|---|---|---|
| `c` | Froid | `cool` in `hvac_modes` |
| `h` | Chaud | `heat` in `hvac_modes` |
| `d` | Sec | `dry` in `hvac_modes` |
| `f` | Ventilation | `fan_only` in `hvac_modes` |
| `e` | Éco | `eco` or `away` in `preset_modes` |
| `b` | Boost | `boost` in `preset_modes` |
| `q` | Silence | `quiet`, `silence`, `Silence` or `low` in `fan_modes` |
| `s` | Oscillation | one of `swing`, `on`, `both`, `vertical`, `3d`, `horizontal` **and** one of `stop`, `off` in `swing_modes` |
| `w` | Brise | `windnice` in `swing_modes` |

- **The firmware adapts** (`tab5_cards.cpp`, no NVS): arc range `floor(min)`..`ceil(max)` then the target again, ± buttons by `step` within the bounds, target shown `%.1f` for a fractional step and `%.0f` otherwise, unit under the target and after the room temperature, the name as popup title (cut with « … »; « Climatisation », translated, without a name), missing buttons hidden (« Éteint » always stays), and the OPTIONS sections re-stacked: a section without a visible button disappears with its title, the others move up.
- **The screen keeps speaking « Daikin », the blueprint translates** to the first equivalent the unit knows, or sends nothing:

| Screen sends | Blueprint sends |
|---|---|
| target | bounded to `min_temp`/`max_temp`; if the unit is off, `hvac_mode` = first of `cool`, `heat`, `heat_cool`, `auto` it has (none: no `hvac_mode`) |
| mode `X` | `X` only if in `hvac_modes` |
| preset `away` / `none` / `boost` | `eco`, else `away` / `none`, else `home`, else `comfort` / `boost` — only if in `preset_modes` |
| fan `quiet` / `auto` | first of `quiet`, `silence`, `Silence`, `low` / `auto`, else `medium`, `mid`, `middle`, `high`, else the first mode that is not a silence |
| swing `stop` / `swing` / `windnice` | first of `stop`, `off` / first of `swing`, `on`, `both`, `vertical`, `3d`, `horizontal` / `windnice` if present |

- **Toggles** test the active state under all these names (`clim_eco_actif`, `clim_silence_actif`, `clim_oscillation_actif`), the same functions as the icon colouring: active → `none` / `auto` / `stop`, else `away` / `quiet` / `swing`.

## Compatibility

- **New blueprint, older firmware (3.0 to 3.2):** `climr` is an unknown key, ignored by `emplacements_appliquer()`; the commands are translated all the same (an older firmware sends the same values).
- **New firmware, older blueprint:** no `climr`, nothing changes: 16-30, 0.5, °C, every button, the YAML title; the old blueprint passes the values as before.
- **The author's Daikin** (attributes read in HA on 2026-09-29: `hvac_modes` off, fan_only, heat, cool, heat_cool, dry; `preset_modes` away, boost, none; `fan_modes` auto, quiet, 1-5; `swing_modes` stop, swing, windnice; 18-32, step 0.5) receives exactly the commands it received (`tests/test_clim.py`). What it sees change: arc 18-32 instead of 16-30, and its `friendly_name` as title.

## Consequences

- Modes without a button stay out of reach from the screen (`heat_cool`, `auto`, fan speeds, `sleep`…); a unit in `heat_cool` or `auto` lights no mode button (it used to light « Éteint »).
- The OPTIONS card keeps its glass and its « OPTIONS » title when the unit has none of the five options.
- `climr` travels with the slots, not in a new action: no firmware-version check in the blueprint (ADR-0023's `sw_version` gate is for missing actions, not keys).
