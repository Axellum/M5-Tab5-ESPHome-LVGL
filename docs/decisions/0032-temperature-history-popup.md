# ADR-0032: A Temperature popup — the history of the two home-screen temperatures, and the forecast for the second one, pushed by HA from its statistics while the popup is open

**Status:** Accepted (2026-10-06, asked for and decided by the author; not yet tried on a tablet)
**Date:** 2026-10-06

## Context

The home screen shows two temperatures: the room's and a second sensor (a greenhouse at the author's home; outdoors for most other homes). The author asked for a history in a popup on a long press of the room's, and, for the second one, a history **and** the forecast, both as graphs, « without taking too many resources ». He then decided (2026-10-06):

1. the forecast is shown on the second temperature's popup; it extends the curve when that temperature is outdoors, and stays apart under the name « Dehors, prévu » (outdoors, forecast) otherwise — a blueprint checkbox says which;
2. three views: 24 hours, 7 days, 30 days.

Constraints that hold: push-only and events-only (ADR-0001, ADR-0025), the firmware names no entity (ADR-0019); the shared modal chrome (ADR-0009) and the single registry (ADR-0013); no hardcoded colour (ADR-0029); public HA files without a real entity (ADR-0024). The build has `LV_USE_CHART 0` and `LV_USE_CANVAS 0`; `LV_USE_LINE 1`.

## Decision

- **Same model as the Energy popup (ADR-0028).** The tablet stores nothing in NVS. A long press on a temperature runs `tab5_historique_ouvrir` (`cle` = `salon` or `serre`); opening and each view button emit `esphome.tab5_historique` (`cle`, `vue` = `jour`, `semaine` or `mois`). The blueprint « Tab5 — emplacements », which knows the sensor of each slot, starts `script.tab5_historique` (package `tab5_historique.yaml`) with that sensor and the new checkbox `serre_exterieure`. The script pushes **once** `tab5_maj_historique` (`cle`, `vue`, `entete`, `mesures`, `previsions`); nothing is pushed while the popup is closed.
- **Data: HA's long-term statistics.** `recorder.get_statistics`, types mean / min / max: period `hour` for 24 h (24 full hours + the current one) and 7 days (3-hour slots aligned on 0 h, 3 h…: 56 + the current one; mean of the hourly means, min of the mins, max of the maxes), period `day` for 30 days (30 days + today). Hourly statistics are kept forever by HA, so the three views work as soon as the sensor has a `state_class`; no helper, no extra database write.
- **Forecast, second temperature only:** `weather.get_forecasts` on the weather entity of `sensor.tab5_meteo` (the one the tablet already uses): hourly for 24 h (24 points) and 7 days (every 3 h over 72 h) when the entity provides it, daily (or twice daily) otherwise and for 30 days (tomorrow, 3 days or 7 days); a daily point sits at local noon and carries its min and max. 48 points at most.
- **Wall-clock minutes.** Slots and points are placed in minutes from the first slot, counted on the local clock (timestamp difference plus the difference of UTC offsets): across a DST change a slot may hold one hour more or less, the axis and the labels never shift. The header gives the first slot as a naive local `AAAA-MM-JJTHH:MM`; the firmware computes its labels from it with civil-date arithmetic, no time zone.
- **Drawing without a chart widget** (`tab5_historique.cpp`): a `lv_line` for the means (ending on the current value, with a dot), plain `lv_obj` bars from min to max behind it, a second `lv_line` in gold for the forecast on a tinted background after a « Maintenant » line, gold min-max bars for daily forecast points, 1-2-5 degree grid (at most 5 intervals), a time axis at 3 h / 6 h / 12 h / 1 day… (10 labels at most), a legend. LVGL 9.5 dashes only horizontal and vertical lines, so the forecast is told apart by its colour and background. `lv_line_set_points()` does not copy its points: they live in the PSRAM block with the series.
- **Memory.** One block in PSRAM (internal RAM as a fallback), allocated on the first opening, ~6 KB: the three views of the temperature shown and the two point arrays. Opening the other temperature forgets them. Widgets are created once.
- **Gestures.** A long press on the room's temperature (the invisible zone `btn_reglables_liste`, whose tap unfolds the list of the − / + tile, [ADR-0033](0033-adjustable-tile.md)) or on the second one (`btn_serre_games`, whose tap still opens the Arcade: LVGL sends no short click after a long press). A slot without a sensor (zone absent) does nothing.
- **Shared view buttons.** `energie_vue_btn.yaml` becomes `vue_btn.yaml` with a `prefixe` var, used by both popups.

## Compatibility

- **New blueprint, older firmware:** the event never comes; the new input has a default (`false`).
- **New firmware, older blueprint or no package:** the popup opens and says « En attente de Home Assistant ».
- **Sensor without statistics** (no `state_class`): empty `mesures`, the popup says « Aucun historique »; the « Now » card still shows the current value.
- **No weather entity:** no forecast, the fourth card shows « -- ».

## Consequences

- One more package (twenty-three), C++ unit, service (twenty-two) and two UI files (`historique_popup.yaml`, the `historique_carte.yaml` template included four times), one more HA package and one blueprint input.
- `tests/test_historique.py` renders the package's real templates on simulated `get_statistics` and `get_forecasts` answers (both DST changes, a missing hour, a failed call) against an independent Python computation, checks the blueprint branch, the demo and the firmware's limits; the templates were also rendered by the author's Home Assistant on its real statistics and Météo-France forecast (2026-10-06).
- The demo answers the event; the off-device render pushes the answer itself (five new screens).
