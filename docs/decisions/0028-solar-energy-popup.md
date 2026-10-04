# ADR-0028: An Energy popup for a solar installation — sensors picked in the blueprint, live values and production history pushed by HA while the popup is open

**Status:** Accepted (2026-10-04, option B chosen by the author; not yet tried on a tablet nor with a real solar installation)
**Date:** 2026-10-04

## Context

A user asked for his solar installation on the screen ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): what the panels produce, what the home uses, what goes to or comes from the grid, the battery, and the production over time. Until now a solar sensor could only sit in a room as a read-only `cap` tile (ADR-0023): one number, no history.

Three ways were weighed with the author:

- **A — more tile types**: a « solar » tile showing two or three numbers. No history, and the five-card room layout has no room for a flow.
- **B — a popup** opened from a tile, with the live values on top and the production as bars below (hours, days, months). Chosen.
- **C — a dedicated page** in the swipe carousel. Heavier (a page of its own in `tab5-lvgl.yaml`, its own idle and wake rules) for a feature most homes do not have.

Constraints that hold: push-only and events-only (ADR-0001, ADR-0025) — the firmware names no entity and calls no HA action; one shared modal chrome (ADR-0009); one registry of modals (ADR-0013); no hardcoded colour (tokens); public HA files without a real entity or placeholder (ADR-0024); the screen in seven languages.

## Decision

- **The sensors are picked in the blueprint** « Tab5 — emplacements », in an optional, collapsed section « Énergie · Energy »: solar power, solar energy produced (kWh, with statistics), grid power (+ import), grid export power (optional, for meters that give it apart: grid = import − export), home consumption (optional: computed as solar + grid − battery charge, never below 0), battery level, battery power (+ charge), battery temperature, and two « Invert » booleans (grid, battery). Left empty, nothing changes on the screen.
- **Entry points.** A `cap` tile whose entity is one of these sensors gets the new tile option **`e`**: it shows its value with `energie_formater()` (W / kW, kWh / MWh) and opens the popup on tap. The solar sensor gets the new palette icon `solaire` by default. « Aller à l'écran → Énergie » (HA select) opens it too. An older firmware ignores the unknown letter `e` (ADR-0023 parsing) and keeps a read-only tile.
- **Popup** (`ui_components/energie_popup.yaml`, `tab5_energie.cpp`, registered as « Énergie »): the shared chrome, then up to four cards (Solar, Home, Grid, Battery; a card whose sensors are all empty is hidden, the others spread over the width), and below, the chart: title « period · total », three view buttons (Hours, Days, Months) and the bars drawn in C++ (gold, the current slot in the accent colour, a reference line at the maximum). Without the produced-energy sensor, the cards alone fill the popup. Instant transitions; MDI glyphs declared in `mdi_font_45` (rule 9).
- **Request and answer.** The popup sends `esphome.tab5_energie` with `vue` (`heures`, `jours`, `mois`) when it opens and on each view button. The blueprint answers:
  - no sensor picked: `tab5_maj_energie` with an empty payload (`|||||||`) — the popup says « Aucun capteur d'énergie choisi »;
  - otherwise, if `script.tab5_energie` exists (package `packages/tab5_energie.yaml`): `script.turn_on` with the tablet's device, the view and the sensors. Without the package, nothing: the popup keeps « En attente de Home Assistant ».
- **Two new firmware actions.** `tab5_maj_energie(payload)`: `solar|home|grid|battery %|battery W|temperature|unit|day` (W, kWh; empty field = not picked, `nan` = picked without a value). `tab5_maj_energie_historique(vue, debut, valeurs)`: the view's production in kWh, `;`-separated (24 hours, 30 days or 12 months; empty = no data), `debut` = local date of the first slot. Every caller (blueprint, package, demo, off-device render, docs) is updated with them.
- **History from the recorder's statistics** (`recorder.get_statistics`, no new helper, no extra database write): the day = sum of the 5-minute `change` since local midnight + the part since the last 5-minute row (current state − last `state`); hours = the same rows by local hour; days = period `day` over 30 days, today replaced by the day; months = period `month` over 12 months, the current month completed by the 5-minute rows of the current hour and the partial part (just after the hour, the last full hour can miss a few minutes until the recorder compiles it). The energy sensor must have statistics (`state_class` `total_increasing` or `total`, as the HA Energy dashboard requires).
- **Refresh while open.** The script (`mode: restart`) pushes the history of the view asked for, then the live values again at each change of a picked sensor (at most every 5 s, at least every minute), the hour bars with them, as long as the tablet's own « Écran courant » sensor says « Énergie » — after a 12 s grace, since that sensor lags by up to 5 s — and for 15 minutes at most. HA pushes nothing while the popup is closed.

## Compatibility

- **New blueprint, older firmware:** the `e` option is ignored (read-only tile); `esphome.tab5_energie` is never sent, so the new branch never runs; the empty-section case sends nothing.
- **New firmware, older blueprint:** no tile gets `e`; « Aller à l'écran → Énergie » opens a popup that waits (« En attente de Home Assistant »).
- **Without the package** `tab5_energie.yaml`: same waiting popup; the blueprint checks `script.tab5_energie` exists before calling it.
- **Section left empty:** no tile changes, no icon changes, nothing pushed except the empty answer when the popup is opened by « Aller à l'écran ».

## Consequences

- One more popup, package, C++ unit (`tab5_energie.cpp`) and two actions; 21 actions in the contract. The demo pushes a solar home (room « Bureau », tile « Solaire ») and the off-device render captures the popup in its three views.
- Sign conventions are the user's to set (two « Invert » boxes, or two grid sensors); the project does not guess an integration's convention.
- With two tablets opening the popup at the same time, the last request wins (`mode: restart`); the other keeps its last values until its next request.
- The popup closes itself after the usual idle time, which ends the script's loop at the next check.
- `tests/test_energie.py` checks the payloads on both sides, renders the package's templates on simulated `get_statistics` answers against an independent Python computation, and checks the blueprint (empty section = nothing, filled = option `e` and icon `solaire`).
- Not verified on a real HA: `script.turn_on` restarting a `mode: restart` script, and the templates against real statistics (only the HA source and simulated answers were read).
