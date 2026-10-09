# ADR-0039: The home gestures — the clock in three touch areas, twelve gestures chosen in the blueprint

**Status:** Accepted (2026-10-09, asked for by the author; not tried on a tablet when written).
**Date:** 2026-10-09

## Context

Until now the clock was one invisible button over the whole clock tile (`btn_clock_calendar_zone`, 401 × 210): a tap opened the alarm clock, a long press the calendar. The three top buttons (house, gear, gamepad) had a fixed tap and, since 2026-10-07, a long press chosen in the blueprint (`appuis` key of `tab5_maj_emplacements`, one screen code per button, kept in NVS, « auto » = the old behaviour).

The author asked for more: the hours, the minutes and the date of the clock as three areas, and every tap and long press of the clock and of the top buttons chosen in the same section of the blueprint, with actions that are not screens (device mode, next device of the − / + tile, next line of the row under the clock, wake word on / off). A later lot will add « next line of Ok Nabu ».

Constraints: push-only and events-only ([ADR-0001](0001-push-only-zero-polling.md), [ADR-0025](0025-events-only.md)); one routine opens a screen (`tab5_ecran_ouvrir`, [ADR-0013](0013-single-registry-consoles-modals.md)); the HA ↔ firmware contract keeps working both ways between versions (`contrat/contrat.yaml`); one builder rather than N copies.

## Decision

- **Three touch areas** from one template (`ui_components/horloge_zone.yaml`), covering the clock tile with no gap and no overlap: HOURS (x 440..639, y 20..157), MINUTES (x 640..840, y 20..157), DATE (x 440..840, y 158..229). The vertical cut is the middle of the tile, inside the « : » in every theme; the horizontal cut is 40 px above the date's baseline (`police_theme.DATE_BASE`), under the digit frames in every theme. `tests/test_gestes.py` holds both.
- **Twelve gestures**, in a fixed order that is also the order of the new key: hours tap, hours long, minutes tap, minutes long, date tap, date long, house tap, house long, gear tap, gear long, gamepad tap, gamepad long (`enum Geste`, `tab5_zones.h`).
- **One list of codes** (`kCodesGestes`, `tab5_zones.cpp`): « rien », the screens of the « Aller à l'écran » select in its order, then the actions `mode_domo`, `appareil_suivant`, `rangee_suivante`, `ecoute`. The index is what NVS keeps, so a new code goes at the end (`nabu_suivant` next). The codes are read by HA and are never translated or renamed.
- **One script runs a gesture** (`tab5_geste`, `tab5-navigation.yaml`): `geste_cible()` turns the gesture into a screen (opened by `tab5_ecran_ouvrir`) or an action. The template buttons only call it with their gesture number.
- **« Auto » is the behaviour before the choice, except the clock**, which the author redrew: hours tap nothing, hours long alarm clock, minutes tap next device of the − / + tile, minutes long alarm clock, date tap next line of the row under the clock, date long calendar; house tap device mode, gear tap settings (Screen page), gamepad tap Arcade; the three long presses keep their « auto » screens (`kEcranAuto`).
- **Transport: a new key `gestes|c1|…|c12;` in `tab5_maj_emplacements`**, no new service variable, so `contrat/contrat.yaml` does not change. The blueprint still sends `appuis` (the three long presses) for a 3.7 firmware, which ignores `gestes`. The firmware resolves `gestes` first, then `appuis` for a button's long press, then « auto »; a payload with `appuis` and no `gestes` (a blueprint from before this change) puts every gesture back to « auto ». The choice is a new NVS preference (`gest`, magic « GST1 »); the `appuis` preference keeps its size and magic.
- **Icons say what the gestures do.** The mini icon of a top button still shows its long press. When its tap is changed, the button's main icon shows what the tap does (the window's header glyph, or the action's glyph); in « auto » it keeps its original icon. Code rule 9 covers both fonts (`MDI_CODE_TARGETS`: `code_glyphe`, `tap_glyphe`).
- **Next device of the − / + tile** (`reglables_suivant()`, [ADR-0033](0033-adjustable-tile.md)): the next row of the list, without unfolding it, kept in NVS like a pick in the list. To show which device is now set, the climate also gets its icon before its target (`clim_consigne_icone`, `mdi_font_45`, centred with the target between − and +; it fits with the widest target of every theme's date font, `tests/test_gestes.py`).

## Rejected

- **A new service variable** for the gestures: Home Assistant refuses a call with an extra or missing variable, so a firmware and a blueprint of different versions would stop pushing anything. A key of the existing payload is ignored by an older firmware.
- **Widening the `appuis` key to twelve fields**: a 3.7 firmware would read the first three fields as the buttons' long presses (the clock's gestures).
- **Two separate code lists** (screens for long presses, actions for taps): every gesture can take every code, and one list keeps the NVS index and the blueprint options in step with one test.
- **Areas following the digits of each theme**: the cut points are the same for the 21 themes (checked), and fixed areas need no code at theme change.

## Consequences

- The tap on the clock no longer opens the alarm clock in « auto »: it moved to the long press on the time. The user manual, `docs/screens.md` and the overview were rewritten.
- Firmware 3.8.0-rc.1 to rc.3 and 3.7 read only `appuis`: with them, the clock keeps one area and the taps are fixed.
- A new code is one row at the end of `kCodesGestes`, one value in `codes_gestes` and the options anchor of the blueprint, a branch in `tab5_geste` if it is an action, and its glyph in `mdi_font_26` and `mdi_font_70`.
