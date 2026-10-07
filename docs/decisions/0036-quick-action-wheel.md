# ADR-0036: A long press on a light, a shutter or a climate opens a wheel of quick actions

**Status:** Accepted (2026-10-07, asked for in a discussion and decided by the author; not yet tried on a tablet)
**Date:** 2026-10-07

## Context

Since the rooms ([ADR-0023](0023-rooms-generic-tiles.md)), a long press on a tile opens its popup: the room's lights, the drawn shutter, the climate. To dim a lamp to 50 % or to stop a shutter at mid-height, the user opens the popup, acts, then closes it: three gestures for the most frequent actions.

In [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278), a user suggested what Bubble Card's `sub_button_wheel` module does in a Home Assistant dashboard: « one click opens a mini popup, like a spring, or a circular popup », showing the device's main actions around it. The author asked for it on the tablet, with these choices (2026-10-07):

1. the long press of a `lum`, `vol` or `cli` tile opens the wheel instead of the popup; its last button, « ⋯ », opens the popup of before; the tap does not change; `int`, `act` and `med` tiles do not change;
2. firmware only: the wheel sends the commands the tiles and their popups already send, nothing changes in Home Assistant;
3. fewer than three actions: no wheel, the popup opens as before.

Constraints that hold: push-only and events-only ([ADR-0001](0001-push-only-zero-polling.md), [ADR-0025](0025-events-only.md)): the firmware calls no Home Assistant action and no command is added to the contract; no hardcoded colour ([ADR-0029](0029-themes-palette.md)); instant transitions (the author's taste); every icon in its font's `glyphs:` (code rule 9); one registry of windows ([ADR-0013](0013-single-registry-consoles-modals.md)).

## Decision

- **Buttons, left to right**, then « ⋯ » (the tile's popup, `tuile_ouvrir_popup`):

  | Tile | Buttons | Commands (`esphome.tab5_action`, key `tRT`) | No wheel |
  |---|---|---|---|
  | `lum` with `d` | Off (not with `o`) · 10 % · 50 % · 100 % | `eteindre`, `luminosite_pct` 10/50/100 (the light popup's shortcuts) | without `d`, with `k` |
  | `vol` | Open · Stop · Close · 50 % when the shutter gives its position | `ouvrir`, `arreter`, `fermer`, `position` 50 (the drawn shutter's command, never without a known position) | with `k` |
  | `cli` | Off · up to four modes the unit has, in the order heat, cool, dry, fan only | `eteindre`, `mode` `heat`/`cool`/`dry`/`fan_only` — the climate popup's commands, to `clim` with `m` (the blueprint's climate), else to `tRT` ([ADR-0027](0027-climate-per-tile.md)) | settings not received yet (`climr` with `m`, `crRT` without) |

  The capabilities are the letters HA pushes with the climate's settings ([ADR-0026](0026-climate-from-device.md)): `h`, `c`, `d`, `f`. There is no « auto » button: no letter nor command carries it, and adding one would change the contract. A tile with option `r` has no long press at all, as before. `k` keeps its former long press (a confirmed command must never be bypassed by one touch). The legacy mode (no rooms received) does not change.
- **The current state** is marked: its button takes the tile's state colour at 20 % behind its icon, drawn in that colour (lamp off, brightness rounded to the button's percentage, shutter fully open or closed, position 50, climate mode). Nothing optimistic: the tile and the wheel show what HA pushed.
- **Geometry** (`tab5_roue.cpp`, integer maths only): round buttons of 76 px, centres on an arc of radius 200 px around the anchor, 30° apart, the fan centred on the vertical (90°) above the anchor. A button that would leave the screen (12 px margin) turns the whole fan by steps of 5° away from the edge; an anchor too close to the top puts the arc below it. A glass disc (`style_glass_card` made round, no shadow, 2 px border in the state colour at 50 %) of radius 246 px sits behind, centred on the anchor. Buttons use the popups' shared glass (`style_clim_btn`, the shutter popup's Open / Stop / Close), icons in a 36 px MDI font (`mdi_font_36`, its own glyph list), percentages in the bold UI font; « ⋯ » in `TEXT_SOFT`.
- **Anchor**: the pastille of the tile's card in HA mode (`sw_pastille_N`), the centre of the tile's button in weather mode. The C++ entry point takes the anchor as a parameter, so another view can open the wheel of a tile around its own widget: `bool tuile_roue_ouvrir(int r, int t, lv_obj_t* ancre)` (`tab5_internal.h`); false means no wheel for this tile, the caller then opens `tuile_ouvrir_popup(r, t)`.
- **Behaviour**: it opens at once; touching a button sends its command and closes the wheel; « ⋯ » closes it and opens the popup; touching elsewhere closes it, and a swipe on it does nothing (no page change under it). It also closes after the popups' inactivity delay, when any popup opens (`animate_popup_open`), when the screen turns off, and when HA pushes new tile definitions. A theme change repaints it open (the rendering's « light » job compares a hot theme switch with a cold start).
- **Widgets**: `ui_components/roue_actions.yaml`, a full-screen transparent catcher holding the disc and six `roue_bouton.yaml` buttons, included after the dashboard cards and before the popups; registered as a `SUBWINDOW` (closed with the popups), like the list of the − / + tile ([ADR-0033](0033-adjustable-tile.md)). Not a modal popup: no scrim, no header ([ADR-0009](0009-modal-shell-header.md) covers modal windows).

## Compatibility

- **Home Assistant**: nothing changes. The commands are those of the tiles and their popups; an older blueprint receives the same events.
- **Older firmware**: the long press opens the popup, as before.
- A `cli` tile without a wheel (the blueprint's climate before its settings `climr`, fewer than three commands) now opens the climate popup on a long press, as its tap does (before: nothing). A tile's own climate whose settings never arrived still does nothing, tap included ([ADR-0027](0027-climate-per-tile.md)).

## Consequences

- One C++ unit (`tab5_roue.cpp`), two UI files, one 36 px MDI font of nine glyphs, one more window in `ModalRegistry`.
- The rendering (`tools/rendu/ecrans.py`) redoes the geometry (`roue_centres`) to touch « ⋯ » on the popup screens, and captures `roue-lampe`, `roue-volet` and `roue-clim`; `tests/test_roue.py` compares both sides and holds the buttons, the commands and the closings.
- Reaching a popup now takes a long press then « ⋯ »: one touch more than before, for the less frequent settings (colour, the room's other lights, the drawn shutter, the fan and swing of a climate).
