# ADR-0036: A long press on a light, a shutter or a climate opens a wheel of quick actions

**Status:** Accepted (2026-10-07, asked for in a discussion and decided by the author; not yet tried on a tablet). *Updated the same day: two rings, the « Maison » and « Réglages » links, a hub on the tile and a new look, asked for by the author after the first version (see « Update » below).*
**Date:** 2026-10-07

## Context

Since the rooms ([ADR-0023](0023-rooms-generic-tiles.md)), a long press on a tile opens its popup: the room's lights, the drawn shutter, the climate. To dim a lamp to 50 % or to stop a shutter at mid-height, the user opens the popup, acts, then closes it: three gestures for the most frequent actions.

In [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278), a user suggested what Bubble Card's `sub_button_wheel` module does in a Home Assistant dashboard: « one click opens a mini popup, like a spring, or a circular popup », showing the device's main actions around it. The author asked for it on the tablet, with these choices (2026-10-07):

1. the long press of a `lum`, `vol` or `cli` tile opens the wheel instead of the popup; one of its buttons opens the popup of before; the tap does not change; `int`, `act` and `med` tiles do not change;
2. firmware only: the wheel sends the commands the tiles and their popups already send, nothing changes in Home Assistant.

Constraints that hold: push-only and events-only ([ADR-0001](0001-push-only-zero-polling.md), [ADR-0025](0025-events-only.md)): the firmware calls no Home Assistant action and no command is added to the contract; no hardcoded colour ([ADR-0029](0029-themes-palette.md)); instant transitions (the author's taste); every icon in its font's `glyphs:` (code rule 9); one registry of windows ([ADR-0013](0013-single-registry-consoles-modals.md)).

### Update (2026-10-07, same day)

The first version (one arc of four commands and « ⋯ » in front of a glass disc) worked, but the author found it short of choices and plain: the author asked for the most classic choices of each device, a link to the full popup, a link to the House popup ([ADR-0037](0037-house-popup.md)) except when the wheel is opened from it, a refined look (gradients, transparency), and whether a first wheel could open a second one inside the same wheel. Answer, accepted by the author: **two rings in one wheel**. The author also decided that a light without a dimmer gets a wheel (On, Off and the links), and that the wheel closes after each choice. The rule « fewer than three commands: no wheel » goes: one command is enough, the links make the rest.

## Decision

- **First ring, left to right**: « Maison » (the House popup, `Ecran::MAISON` through the single screen routine `tab5_ecran_ouvrir`; absent when the wheel is opened from a row of that popup), the tile's commands and its **families** (a button with a dot: touching it unfolds the second ring), then « Réglages » (the tile's full popup, `tuile_ouvrir_popup`, the former « ⋯ »). The two links are glass outlines with their word under them.
- **Second ring**: the choices of the touched family, centred on it, 110 px further out. Touching another family swaps them; touching the family again, the hub or the dimmed background folds them; folded, the next touch outside closes the wheel.

  | Tile | First ring (between the links) | Families → choices | Commands (`esphome.tab5_action`, key `tRT`) | No wheel |
  |---|---|---|---|---|
  | `lum` with `d` | Off (on) or On (off, or always with `o`) · Brightness ▸ · Whites ▸ and Colours ▸ with `c` | 10 · 25 · 50 · 75 · 100 % ; warm · cream · cold ; red · orange · gold · green · blue · purple | `allumer`, `eteindre`, `luminosite_pct`, `couleur` + colour name (the light popup's names and swatches) | with `k` |
  | `lum` without `d` | On · Off (not with `o`) · Whites ▸ and Colours ▸ with `c` | as above | `allumer`, `eteindre`, `couleur` | with `k` |
  | `vol` | Open · Stop · Close · Position ▸ when the shutter gives its position | 25 · 50 · 75 % | `ouvrir`, `arreter`, `fermer`, `position` (never without a known position) | with `k` |
  | `cli` | Off · Mode ▸ · Setpoint ▸ when known · Options ▸ when the unit has any | the modes it has (heat, cool, dry, fan only) ; its setpoint and two steps each side, within its bounds ; Eco, Boost, Quiet, Swing, Breeze | `eteindre`, `mode`, `consigne`, `preset`, `ventilation`, `oscillation` — the climate popup's commands and values, to `clim` with `m` (the blueprint's climate), else to `tRT` ([ADR-0027](0027-climate-per-tile.md)) | settings not received yet (`climr` with `m`, `crRT` without) |

  The climate's capabilities are the letters HA pushes with its settings ([ADR-0026](0026-climate-from-device.md)). There is no « auto » mode (no letter nor command carries it) and no fan-speed family: HA pushes no list of speeds, so the wheel offers the popup's Quiet toggle in Options instead. A tile with option `r` has no long press at all, as before. `k` keeps its former long press (a confirmed command must never be bypassed by one touch). The legacy mode (no rooms received) does not change.
- **The hub**, on the anchor: the card's icon in the state colour, its state line (« 60 % », « Ouvert », « 21.5 ° »), a 2 px rim in the state colour, and around it a thin arc gauge (brightness, shutter position, setpoint within the unit's bounds) open at the bottom, the device's name under it.
- **The current state** is marked: its button (lamp on or off, shutter fully open or closed, climate off; on the second ring the brightness, position, mode, setpoint and active options) is a glass tinted with the state colour, with a full rim and a glow — the wheel's only shadow (a shadow costs at each redraw, [performance](../performance.md)). An unfolded family is lightly tinted, its dot in the state colour. A colour is a swatch (lighter at the top, darker at the bottom, glass rim), never marked: the pushed state only gives the shown tint. Nothing optimistic: the wheel shows what HA pushed, and a pushed state repaints it open, the unfolded family kept.
- **Geometry** (`tab5_roue.cpp`, integer maths only): round buttons of 72 px; the first ring's centres on an arc of radius 180 px around the anchor, 30° apart, centred on the vertical above it; the second ring's on an arc of 290 px, 20° apart, centred on its family's angle, with its words 60 px further out. A button that would leave the screen (12 px margin) turns its fan by steps of 5° away from the edge, or towards the vertical when it would leave at the top or the bottom; an anchor too high for the second ring and its words (y < 364) puts the whole wheel below it. Under each ring, a glass band (a rounded arc, 12 px of glass around the buttons, a lighter core) — neutral under the first, tinted with the state colour under the second. Behind everything, the popups' veil colour at 60 %: the tiles stay visible. Buttons use the popups' shared glass (`style_clim_btn`), icons in a 36 px MDI font (`mdi_font_36`, its own glyph list), numbers in the bold UI font.
- **Anchor**: the pastille of the tile's card in HA mode (`sw_pastille_N`), the centre of the tile's button in weather mode, the pastille of a row in the House popup. The C++ entry point takes the anchor as a parameter, so another view can open the wheel of a tile around its own widget: `bool tuile_roue_ouvrir(int r, int t, lv_obj_t* ancre, bool depuis_maison = false)` (`tab5_internal.h`); false means no wheel for this tile, the caller then opens `tuile_ouvrir_popup(r, t)`.
- **Behaviour**: it opens, unfolds and folds at once; touching a command or a choice sends it and closes the wheel; a link closes it and opens its window; a swipe on it does nothing (no page change under it). It also closes after the popups' inactivity delay, when any popup opens (`animate_popup_open`), when the screen turns off, and when HA pushes new tile definitions. A theme change repaints it open (the rendering's « light » job compares a hot theme switch with a cold start).
- **Widgets**: `ui_components/roue_actions.yaml`, a full-screen veil holding the two bands, six `roue_choix.yaml` and six `roue_bouton.yaml` buttons, eight `roue_legende.yaml` words, the gauge, the hub and the name; included after the dashboard cards and before the popups; registered as a `SUBWINDOW` (closed with the popups), like the list of the − / + tile ([ADR-0033](0033-adjustable-tile.md)). Not a modal popup: no card, no header ([ADR-0009](0009-modal-shell-header.md) covers modal windows).

## Compatibility

- **Home Assistant**: nothing changes. The commands are those of the tiles and their popups; an older blueprint receives the same events.
- **Older firmware**: the long press opens the popup, as before.
- A `cli` tile without a wheel (the blueprint's climate before its settings `climr`) now opens the climate popup on a long press, as its tap does (before: nothing). A tile's own climate whose settings never arrived still does nothing, tap included ([ADR-0027](0027-climate-per-tile.md)).

## Consequences

- One C++ unit (`tab5_roue.cpp`), four UI files, one 36 px MDI font of 23 glyphs, one more window in `ModalRegistry`.
- The rendering (`tools/rendu/ecrans.py`) redoes the geometry (`roue_centres`, `roue_choix_centres`) to touch « Réglages » on the popup screens and a family on the wheel screens, and captures `roue-lampe` (brightness unfolded), `roue-lampe-couleurs`, `roue-volet` (positions), `roue-clim` (modes) and `maison-roue`; `tests/test_roue.py` compares both sides and holds the buttons, the choices, the commands and the closings.
- Reaching a popup takes a long press then « Réglages »: one touch more than before, for the less frequent settings (the room's other lights, the drawn shutter, the fan of a climate).
