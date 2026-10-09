# ADR-0047: A tap on the room's temperature opens the wheel of a climate; its « Détails » is the carousel

**Status:** Proposed (2026-10-09, asked for by the author; not tried on a tablet when written). Amends the gestures of [ADR-0038](0038-climate-carousel.md).
**Date:** 2026-10-09

## Context

[ADR-0038](0038-climate-carousel.md) made a tap on the room's temperature (`btn_reglables_liste`, climate card of the home screen) open the climate popup as a carousel, one page per climate. The author had meant something else: « je parlais d'une roue comme tu as faite en bas, super classe et pratique, pas une page » — the quick-action wheel of the tiles ([ADR-0036](0036-quick-action-wheel.md)), opened by the temperature. The carousel stays: it is the full climate window, with a swipe from one unit (or room) to the next.

Constraints: one wheel, one code for a climate's buttons (code rule 5); no new command and no change of the API contract (the commands of [ADR-0025](0025-events-only.md), [ADR-0027](0027-climate-per-tile.md) and [ADR-0040](0040-room-climate.md), at the places `clim`, `tRT` and `cpR`); at most six buttons on the first ring (`kRoueBoutons`); every icon in its font's `glyphs:` (code rule 9); instant transitions.

## Decision

- **The tap opens the wheel of the climate the carousel opened on** (`clim_temperature_ouvrir()`, `tab5_clim.cpp`): the room shown in HA mode — its own climate first, else the first of its tiles —, otherwise the first of `clims_enumerer()` (the blueprint's). The choice is `clim_ref_choisir()`, the code the carousel used. The wheel is not anchored on the temperature: it is too high (centre y 164), the rings would go below it, folded and pivoted over the − / + tile and the centre card (first render of 2026-10-09). The hub shows the climate, so it sits on a low anchor instead (`clim_ancre_basse()`: a 1 px object without style nor touch, created once on the top layer, so `roue_ouvrir()` keeps its signature and its geometry), below the touched zone with x brought into [480, 800], at y 460: the full fan opens above it, both rings and their words without any pivot nor any word pushed back into the screen, six families of six choices and 150 px words included (`disposer()` and `mot_recul()` hold from y 365 to 477 and x 479 to 801; `tests/test_roue_clim.py` measures it again).
- **One wheel code**: `clim_roue_ouvrir(const ClimRef&, ancre)` (`tab5_tuiles_roue.cpp`) is the tile wheel with no tile (`RoueTuile::r = -1`) and a target climate (`RoueTuile::clim`, the `ClimRef` convention: `r < 0` the blueprint's, `t < 0` a room's, else the tile `tRT`). A `cli` tile sets that target from `clim_cible()`. The climate's buttons — Off, Mode ▸, Setpoint ▸, Options ▸, from what Home Assistant pushed — are built by one lambda, `composer_clim`, called by both. The `clim_*` readers of `tab5_clim.cpp` (`clim_capacites_connues`, `clim_roue_consignes`, `clim_roue_bascules`, `clim_roue_jauge`) now also read a room's climate (`t < 0`); `clim_mode_connu()` and `clim_tete()` give the mode, the hub (name, state line and colour of a `cli` tile, gauge of the setpoint) and the setpoint text.
- **Links**: « Détails » opens the carousel on that climate (`clim_carrousel_ouvrir_sur()`). The first slot is « Maison » with one climate; with two or more, it becomes the family **« Clims ▸ »** (icon air-conditioner, F001B): its second ring shows the climates of `clims_enumerer()` (at most six, the window that holds the wheel's one), each with its setpoint (or the power icon when off) and its name under it, the wheel's one marked. A touch reopens the wheel on that climate, at the same place; a climate without settings opens the carousel on it. Seven buttons would not fit, and « Maison » is the tiles' link, reachable from any tile.
- **Unchanged**: with no settings received for the chosen climate (the blueprint's before `climr`), the tap opens the carousel as before; with no climate at all, it unfolds the − / + list; the long press opens the history; the carousel, its swipe and its dots do not change; a `cli` tile's wheel sends and marks exactly what it did.
- **Repaint**: a pushed setting or state of any climate (`climr`, `tab5_maj_clim`, `crRT`/`ceRT`, `crpR`/`cepR`), or a forgotten climate, repaints the open wheel when it targets a climate (`roue_clim_changee()`), or closes it when the climate has no wheel any more. Before, a tile's climate wheel only followed its tile's state.

## Rejected

- **A second wheel code for a climate without a tile** (a copy of the `cli` branch): two places to keep equal for every family, against code rule 5.
- **A seventh button**: the widgets, the geometry and the off-device render are built for six; « Clims ▸ » takes the place of « Maison » only when there is a choice to make.
- **Dropping the carousel**: the author keeps it as the climate « page », reached by « Détails ».

## Consequences

- One more icon in `mdi_font_36` (24 glyphs), one more `RoueIcone` and `RoueAction`.
- `clim_carrousel_ouvrir()` is replaced by `clim_ref_choisir()` + `clim_carrousel_ouvrir_sur()` and the card's entry point `clim_temperature_ouvrir(lv_obj_t*)`.
- The off-device render captures `roue-clim-temperature` (HA mode, the Bureau's own climate), `roue-clim-temperature-clims` (« Clims ▸ » unfolded) and `roue-clim-temperature-meteo` (weather mode, the blueprint's climate after `climr`); `climatisation-carrousel-mode-ha` now reaches the carousel through the wheel's « Détails ». `tests/test_roue_clim.py` holds the rest.
