# ADR-0007: `climate_popup.yaml` deliberately left un-factorized

**Status:** Accepted

## Context

`climate_popup.yaml` is, at 234 lines, the largest `ui_components/*.yaml` file and the most repetitive-looking one — a 3×3 grid of 9 buttons for HVAC mode/preset/swing/quiet/temperature. Every other repeated-widget pattern in this codebase (forecast cards, switch cards, light color presets) was factored into a parametrized C++ builder or an `!include`+`vars` template. A 2026-07-06 refactor pass (task #T164) did factor 6 of the 9 buttons this way (`climate_hvac_mode_btn.yaml`, `climate_preset_toggle_btn.yaml`).

## Decision

The remaining 3 buttons (`off`, `swing`, `quiet`) and the 2 +/- temperature buttons were deliberately left as individual, duplicated YAML rather than pushed into the same template pattern.

## Consequences

- `climate_popup.yaml` stays the one file in `ui_components/` that looks like it "should" be factored further and isn't — this is a known, revisited decision, not an oversight. Don't re-flag it as debt without reading this ADR first.
- Rationale: each of these buttons calls a genuinely different HA service with different lambda logic and different widget IDs referenced elsewhere for state highlighting — there is no purely cosmetic part separable from the action, unlike the 6 buttons that were successfully templated. Forcing a shared template here would mean parametrizing the service call itself, not just the button's appearance.
- The only path to further factoring, if ever pursued, is a C++ dispatch-by-index architecture (each grid button reports an index, a single C++ function maps index → service call) — noted as the "next step, more invasive" option in project history, not started.
