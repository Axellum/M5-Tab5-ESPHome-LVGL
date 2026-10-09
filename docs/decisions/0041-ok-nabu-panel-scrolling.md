# ADR-0041: The « Ok Nabu » panel takes lines like the row under the clock, and three zones scroll by choice

**Status:** Accepted (2026-10-09, asked for by the author; not tried on a tablet when written).
**Date:** 2026-10-09

## Context

On the home page, the « Ok Nabu » frame (`btn_ok_nabu`, 405 × 90 at x 20, y 218, under the Domo and Discu buttons) showed one thing: « Ok Nabu: ON / OFF », and a tap turned wake word listening on or off. The row under the clock ([ADR-0031](0031-row-under-the-clock.md)) already showed up to three lines of four sensors picked in the blueprint, rotating with the central card.

The author asked for:

1. the same lines in the « Ok Nabu » frame, up to three, picked in the blueprint with the same mechanism and format as the row (one source, code rule 5); the listening line stays possible and comes first by default; a tap keeps switching listening, at least on the listening line;
2. the hours tap of the clock ([ADR-0039](0039-gestes-accueil.md)) to show the next line of the frame, as its « auto »;
3. a choice of scrolling for the row, the frame and the − / + tile ([ADR-0033](0033-adjustable-tile.md)): « auto » (in step with the central card) or « fixed » (only by a gesture); defaults: row auto, frame fixed, − / + tile fixed; in auto, a gesture is not overwritten at once;
4. no new service variable (the contract between versions does not change), choices kept in NVS;
5. the quality of the row: icon and value, centred, nothing overflowing in the 21 themes, page dots when there is more than one line, the same transition.

## Decision

- **Two zones with lines, one model, one drawing.** The row's model (`ModeleRangee`, `tab5_tuiles.cpp`) and its drawing (`tab5_rangee.cpp`) take a zone index (`RangeeZone`: `RANGEE_HORLOGE`, `RANGEE_NABU`, `tab5_internal.h`). Each zone has its key letter (`h`, `n`), its NVS record (the row keeps « rang » / « RAN1 », so a tablet already set up keeps its row; the frame gets « nabu » / « NAB1 »), its widgets (`RangeeUI`: `g_rangee_ui`, `g_nabu_ui`) and a `Defileur` (order of lines, current line, turn count). Its « special » line is the plants under the clock and the listening line in the frame.
- **Same keys, letter `n`.** `tab5_maj_tuiles` carries `np|place;` (listening line, 0 to 2, `-` hidden), `nd|seconds;` and `nLI|type|icon|options|complement|name|class;`, exactly like `hp`, `hd`, `hLI`; `tab5_maj_emplacements` carries the states `nLI|state|value|colour;`. Without any `n` key (a blueprint from before this change, or no line picked) the frame shows the listening line alone: the screen from before.
- **Same widgets, from templates.** The two sensor panels of each zone come from one template (`ui_components/rangee_panneau.yaml`, four `rangee_element.yaml` each); the page dots of both zones from another (`ui_components/rangee_pastilles.yaml`). In the frame, the panels are 401 × 88, centred in the 405 × 90 button (`pad_all: 0`), and not clickable: the touch stays on the button. The listening label sits in its own panel (`nabu_ecoute`). The frame is hidden when it has no line at all (listening hidden and no sensor line).
- **Tap on the frame.** On the listening line, it switches listening as before. On a sensor line, it does **nothing**: the lines are display only, like the row, and switching the microphone from a line that does not show its state would be a blind action. The « Écoute « Ok Nabu » » gesture of ADR-0039 still switches it from anywhere, and so does the tablet's switch in Home Assistant.
- **Hours tap.** A new gesture code `nabu_suivant` (index 17, at the end of `kCodesGestes`: NVS keeps the index), glyph `microphone-message` (U+F050A) in `mdi_font_26` and `mdi_font_70`. It is the « auto » of the hours tap; with the frame from before (listening alone) it does nothing, which is what « auto » did there.
- **Room in the frame.** The frame of some themes is a pill (Capsule: radius 45, 4 px border). Elements keep 14 px from the panel's sides in the frame (`kMargeCadre`, 8 px under the clock): every element box stays inside the inner rounded border in the 21 themes, both modes (`tests/test_nabu.py` computes it from `tab5_theme.cpp`).
- **Page dots** under the frame, at the height of the row's (y 319: between the bottom of the frame, y 308, and the central card, y 333), centred on its 405 px; hidden under two lines.
- **Scrolling by choice: one key `defil|row|frame|tile|seconds;`** in `tab5_maj_emplacements`, sent with the gestures (all states pushed: connection, reload, « MAJ Écran », HA start). Each zone is `auto` or `fixe` (anything else: its default); `seconds` is how long a device of the − / + tile stays in auto, rounded to the 8 s turn of the central card. Defaults (no key, or a blueprint from before): row auto, frame fixed, tile fixed — the screen from before. A payload carrying gestures but no `defil` (a blueprint from before this change) puts the defaults back. Kept in NVS (« defl », « DEF1 »), written only when it changes.
- **One clock for the three.** `rangee_tour()` (called by the central card's rotator 0.2 s before it turns, screen on, no popup) counts a turn for each zone in auto and for the − / + tile. A gesture (tap on the row, `rangee_suivante`, `nabu_suivant`, a pick or a step on the tile) resets its count, so the line or device the user chose stays a whole duration. The frame's duration is its `nd` key; the tile's, the 4th field of `defil`. `kTourCentralS` has one definition (`tab5_internal.h`).
- **The − / + tile in auto** goes through the climate and the blueprint's devices, not the tablet's volume (picked by hand). It never writes NVS (a write every turn would wear the flash; at start the tile comes back to the last device picked by hand) and never moves while the list is open or a value waits to be sent. It changes the device at once (the tile's own drawing), with no slide.

## Rejected

- **A separate model for the frame**: two copies of the same parser, NVS record, drawing and rotation; the next fix would land in one of them only (rule 5).
- **A tap on a sensor line that still switches listening**: the user would switch the microphone without seeing its state. A tap that shows the next line was also possible; the author's request kept the tap for listening, and the next line has its gesture (hours tap).
- **One NVS record for both zones**: the row's record would change size and every tablet set up with ADR-0031 would lose its row until the next push.
- **New service variables for the choices**: Home Assistant refuses a call with an extra or missing variable, so a firmware and a blueprint of different versions would stop pushing anything; a key of an existing payload is ignored by an older firmware.
- **Rotating the − / + tile with a slide**: the tile has no second panel to slide to, and its value is the one the user adjusts; an instant change keeps it simple and in line with the author's « instant transitions » preference.

## Consequences

- Firmware 3.7 and older ignore `n` keys and `defil`: the frame stays « Ok Nabu: ON / OFF » and the row keeps rotating.
- A third zone with lines = a value of `RangeeZone`, its key letter and NVS key (`tab5_tuiles.cpp`), its widgets from the two templates, its `Defileur` and `Defilement` value, its section in the blueprint and its triggers (`tools/gen_blueprint_emplacements.py`).
- The off-device render captures the frame with one and three lines, in the default theme, the theme with the widest date font (Almanach imprimé) and the pill theme (Capsule).
