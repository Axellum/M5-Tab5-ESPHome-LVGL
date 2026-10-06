# ADR-0031: A row under the clock — up to three lines of four sensors plus the plants line, rotating with the central card, picked in the blueprint

**Status:** Accepted (2026-10-06, asked for and decided by the author; not yet tried on a tablet)
**Date:** 2026-10-06

## Context

Under the clock of the home page, a strip of 401 × 70 px showed the plant pots (up to four moisture gauges, `moisture_sensors.yaml`) and nothing else. A home without plants had an empty space there, and a home with other things to watch (temperatures, humidity, solar production, a battery, presence detectors, a few switches) had no place for them outside a room page.

The author asked for that strip to take other sensors too, up to three lines that rotate « a bit like the central card », depending on what is configured; clean, easy to set up, adaptable, and good looking. He then decided (2026-10-06):

1. switches on the strip are shown, not controlled;
2. the strip uses the central card's animation and changes slightly **before** it, so the screen changes from top to bottom;
3. the duration is set in the blueprint, in turns of the central card (8 s): the central card keeps its 8 s and the strip changes every N turns, 0.2 s ahead;
4. the plants are a line of their own, whose place (first, second, third, hidden) is chosen.

Constraints that hold: push-only and events-only (ADR-0001, ADR-0025); the firmware names no entity; one source for the tile translation (ADR-0023: type, icon, options, complement, name); no hardcoded colour (themes, ADR-0029); public HA files without a real entity (ADR-0024); an older firmware or an older blueprint must keep working.

## Decision

- **Name: « rangée » (row), keys `h…`.** « Bandeau » already names the status bar; the strip's keys start with `h` (for *horloge*, clock) so they never collide with `b…` or `t…`.
- **Picked in the blueprint** « Tab5 — emplacements », in a collapsed section « Sous l'horloge · Under the clock »: three entity lists (four at most each, reorderable; the tile domains without scenes, scripts and buttons), the place of the plants line (`0`, `1`, `2` or hidden) and the time per line (8 to 120 s, step 8, default 32). The entities are a variable of their own (`rangee`), **not** part of `tuiles`: the screen's command whitelist (« Commande d'un bouton de l'écran ») never sees them, so the strip can show a switch without being able to switch it.
- **Same translation as the tiles.** In `tab5_maj_tuiles`, after the rooms: `hp|place` (0 to 2, `-` hidden), `hd|seconds`, and `hLI|type|icon|options|complement|name|class` for element I of line L — the six fields of a tile computed by the same loop of the blueprint, plus the device class (the value's colour). States go with the tiles' in `tab5_maj_emplacements`, `hLI|state|value|colour`, pushed on every all-states push, when the state changes (triggers `rangee_n` / `rangee_n_sortie`, state only) and with the slow measurements (`cap`, `cli`) every 5 minutes.
- **Firmware model.** `tab5_tuiles.cpp` keeps the strip next to the rooms but in its **own** NVS record (`ModeleRangee`, magic `RAN1`), so the rooms' record keeps its size. Each `tab5_maj_tuiles` payload is a full snapshot: without `h` keys (older blueprint) the strip is the plants alone, first, 32 s.
- **Drawing** (`tab5_rangee.cpp`, `ui_components/rangee.yaml`, package `tab5-rangee.yaml`): lines in the blueprint's order, empty lines skipped, the plants line inserted at its place when the tablet has pots (`zones_pots_presents()`); three lines at most, so with the plants a third sensor line waits until they are hidden. Two panels of four elements (icon, then the value for a sensor or a climate) alternate with `transition_widgets()`, the central card's animation. The layout picks the largest size that fits 401 px: the date's font (45 px, the theme's) with 45 px icons, then 32 px bold, then 22 px, then icon above value; past that the value is cut with « … ». Icons only (no sensor on the line): the 70 px icons of the pots. Small dots under the strip show the current line when there are two or more.
- **Colours by measurement, on the strip only.** A value in ° (or class `temperature`; °F brought back to °C) takes the screen's temperature scale and is written « 21.4 ° »; humidity and moisture the humidity scale; battery the battery scale; power, energy or the `solaire` icon gold, written with `energie_formater()` (W / kW, kWh / MWh). The room tiles keep their colours: changing them was proposed and left to the author.
- **Rotation.** `tab5_central_rotator_auto` waits 7.8 s, calls `rangee_tour()` (screen on, no popup open), waits 0.2 s, then moves the central card as before. `rangee_tour()` counts turns and shows the next line every `round(seconds / 8)` turns (1 to 15). A tap on the strip shows the next line at once (and restarts the count); a long press on the plants line opens « Mes Plantes ».
- **The off-device render** stops the strip with the central card's rotator (`rendu_panneau` calls `rangee_recaler()`: first line, count at zero), so every capture is reproducible.

## Compatibility

- **New blueprint, older firmware** (3.6.0 and before): the `h` keys are unknown and ignored (`lire_entree`, `tuiles_etat_recu`), the strip shows the plants as before.
- **New firmware, older blueprint:** no `h` key, so the plants line alone, first; the screen is the one of 3.6.0.
- **Section left empty:** `hp|0;hd|32;` only — the defaults of the firmware; the screen is unchanged.
- **No pots and no sensor line:** the strip and its touch zone are hidden.

## Consequences

- One more package (twenty-two), C++ unit and two UI files (`rangee.yaml`, the `rangee_element.yaml` template included eight times). `moisture_sensors.yaml` moves inside the strip; `btn_pots_detail_zone` becomes `btn_rangee`.
- The strip is display only. A popup « Capteurs » opened by a long press on a sensor line (names and values of the line) is the next step (lot 2); a special « Énergie » line drawn as a flow is a possible lot 3.
- `tests/test_rangee.py` holds the timing (7.8 s + 0.2 s = one 8 s turn), the keys on both sides, the whitelist exclusion, the demo and the render; `tests/test_installation_ha.py` and the « Installation dans un HA neuf » job check the definitions and states in a real Home Assistant.
