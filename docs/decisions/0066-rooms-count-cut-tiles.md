# ADR-0066: The number of rooms is chosen in Home Assistant (up to ten), and a room takes up to twenty elements by cutting tiles in two or in four

**Status:** Proposed (2026-10-18) — the contract of three lots (A: firmware, number of rooms; B: firmware, cut tiles; C: Home Assistant), not tried on a screen when written. Amends [ADR-0023](0023-rooms-generic-tiles.md).
**Date:** 2026-10-18

## Context

[ADR-0023](0023-rooms-generic-tiles.md) made the five bottom pages five rooms of five tiles: room `R` ↔ forecast page, tile `T` = visual position, keys `pR` and `tRT` with one digit each. PR #511 (2026-10-18) gave the HA mode its own roller (left = next room, right = previous, 5 → 1, empty rooms kept); the weather keeps its pages and its loop.

The author asked on 2026-10-18:

- to choose the number of rooms;
- for the rooms beyond the weather's pages, to show their sensors instead of weather tiles with nothing to say;
- more than five elements per room: at six, one tile cut in two; at seven, two tiles cut in two, and so on; beyond, a tile cut in four.

His three choices, the same day: rooms beyond the fifth get pages in weather mode too; the tiles cut are the last ones of the room's list (the user orders the list); a tile is cut in two **top / bottom**.

Five is written in many places (read in the code on 2026-10-18):

- `kPieces = 5`, `kTuiles = 5` (`Tab5/socle/tab5_geometrie.h`); the model kept in NVS (`Modele`, magic `TUI1`, 1 608 bytes: 5 room names + 25 `Def`, `Tab5/ecran/tab5_tuiles_priv.h`), in PSRAM;
- objects declared one by one: `lum_onglet_0..4`, `vol_onglet_0..4`, `pbar_0..4` (dots under the central card), the Maison popup (5 headers, 25 rows);
- the HA mode's room is derived from `forecast_page` (`kPieceDePage`), the page index of the weather;
- the navigation wheel takes 8 choices (`kRoueChoix`, `tab5_roue.h`): « Maison » and at most 7 rooms;
- keys read with one digit (`chiffre_0_4`, `tab5_tuiles.cpp`; `p` + digit < `kPieces`, `tab5_parse.cpp`), the adjustable tile's link `tRT` with digits 0-4 (`tab5_reglables.cpp`);
- Home Assistant: the blueprint generator (`PIECES = 5`, `tools/gen_blueprint_emplacements.py`), `range(5)` and « 5 entities per room » in the blueprint, a `range(7)` that mixes the five rooms with the row under the clock (5) and the Ok Nabu panel (6), the setup assistant (`MAX_PIECES = MAX_TUILES = 5`).

A whole card is 230 × 275 px on the Tab5 (130 px disc, 70 px icon, 32 px texts): it does not shrink as it is. The row under the clock ([ADR-0031](0031-row-under-the-clock.md), `tab5_rangee.cpp`) already draws an element at the largest of several sizes that fits (icons 70 / 45 / 32 px, texts 32 / 22 px).

## Decision

### Number of rooms (lot A, firmware; lot C, Home Assistant)

- **Up to ten rooms** (`kPiecesMax = 10`). The blueprint has a « Nombre de pièces » input (1 to 10, 5 by default) and sends it first in the definitions snapshot as a new entry **`pn|N`**. No `pn` (an older blueprint): 5. A room `R ≥ N` does not exist on screen — not in the roller, the dots, the popups, the wheel, the Maison popup — even if tiles were sent for it.
- Room keys do not change: `pR`, `cpR`, `crpR`, `cepR`, `R` = one digit `0`-`9`.
- **HA mode:** the roller of PR #511 over the `N` rooms (left = next, right = previous, `N − 1` → 0, empty rooms kept, « Aucun appareil »). One dot per room. The room shown is its own index, no longer derived from `forecast_page`; « Pièce n/N » counts `N`.
- **Weather mode:**
    - pages 0 to 4 do not change: hours and days, their loop and its `[AI-WARNING]` on the right side, rooms 0-4 on the pages of ADR-0023's table with their shoulders;
    - rooms 5 to `N − 1` get **one page each after days 10-14, on the left swipe**: home → days 5-9 → days 10-14 → room 5 → … → room `N − 1` → home. The right swipe does not change (home → next hours → hours 5-9 → home);
    - these pages show **the room's elements drawn as in HA mode** (cards and cut tiles), no weather, the room's name as title; the return to home after 25 s applies to them; the dots count 5 + (`N` − 5).
- **Everything that lists the rooms follows `N`:** the Lights and Shutters popups (one page per room), the Maison popup (at most five room columns per page, more pages with the swipe of [ADR-0046](0046-popups-a-pages.md)), the « Pièces » family of the navigation wheel (more than seven rooms: more choices or a second page, the wheel's look kept), the room climates (carousel, Temperature popup, history keys `p0` … `p9`), the adjustable tile's link (`tab5_reglables.cpp`).

### Elements per room (lot B, firmware; lot C, Home Assistant)

- A room's list in the blueprint keeps its order and takes **up to twenty elements** (`kTuilesMax = 20`). The element index `T` (0 … 19, the position in the list) is its identity everywhere: states, the key of `esphome.tab5_action`, popups, wheel, climate per tile. The drawing never is.
- **Keys:** `tRT`, `crRT`, `ceRT` — `R` one digit, `T` one character of `0123456789abcdefghij` (`T` = 0 … 19). Every key valid today (`t00` … `t44`) keeps its meaning.
- **Five places, cut by a pure function** in `Tab5/socle/` tested on a PC. With `n` elements:
    - `n ≤ 5`: `n` whole tiles, centred, as today;
    - `6 ≤ n ≤ 20`: the five places are each whole (one element), cut in two (two) or in four (four), with the fewest quarters, then the fewest halves. Places from left to right: whole ones, then halves, then quarters. Elements fill them in list order; inside a half, top then bottom; inside a quarter, top left, top right, bottom left, bottom right.

| `n` | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| whole | 4 | 3 | 2 | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 | 0 | 0 |
| halves | 1 | 2 | 3 | 4 | 5 | 3 | 4 | 2 | 3 | 1 | 2 | 0 | 1 | 0 | 0 |
| quarters | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 2 | 2 | 3 | 3 | 4 | 4 | 5 | 5 |

At 19 the last quarter stays empty.

- **A half** is the top or bottom band of the card (about 230 × 135 px on the Tab5): icon 45 px, value or state 32 px, name 22 px. **A quarter** (about 113 × 135 px): icon 32 px, value 22 px, the name only if it fits. Sizes chosen like the row under the clock (the largest that fits, `texte_ha_coupe()`), from the board's geometry (`tab5_geometrie.h`), so the Waveshare 7B (184 px tiles) follows. Same colours and roles as the whole card (`vue_def()`); an element that acts shows its state by the colour of its icon, as the disc does.
- **Every cell is its own touch target:** tap and long press do what they do on a whole tile of that type (`kGestes`, options included).
- **Weather mode's shoulders:** a place holding one whole element keeps them; a cut place has no shoulder and no device tap in weather mode.
- **Lists with one row per element** (Lights and Shutters popups, Maison popup) read the elements, not the places: every element can be reached; a room with more rows than a page holds gets more pages (ADR-0046). Nothing clipped.

### Memory

- The NVS model is sized on the maxima (10 × 20 `Def`, about 12 KB instead of 1.6 KB) and changes its magic each time its layout changes: the first start after the update shows the rooms again once Home Assistant sends its snapshot (at every connection). Lot A (or B, the first to merge) checks that the NVS partition has room for it; if not, one record per room. The model stays in PSRAM (`EXT_RAM_BSS_ATTR`); per-tile climate state is allocated only for `cli` elements.

### Home Assistant (lot C)

- Blueprint: « Nombre de pièces » (number, 1 to 10, help sentence), rooms 6 to 10 generated by `tools/gen_blueprint_emplacements.py` with the same inputs as rooms 1 to 5 (collapsed sections); `pn|N` first; rooms `≥ N` not sent; up to 20 elements per room with the keys above, in the states and in `esphome.tab5_action`; a longer list keeps its first twenty (the help sentence says so). The `range(7)` mixing rooms, row and panel is split.
- The setup assistant (`custom_components/tab5/assistant.py`): up to ten areas and twenty elements, and the « Nombre de pièces » it writes.

### Compatibility

- **Older firmware, new blueprint:** the keys it cannot read are ignored (`chiffre_0_4`, room digit `< 5`): the first five elements of the first five rooms, as today; `pn` ignored as an unknown entry (lot A checks it on the 3.9 parser and keeps a test).
- **New firmware, older blueprint:** no `pn`, so five rooms of at most five elements: today's screen.
- Only payload entries change, no service variable: lots A and C run `python tools/contrat_api.py --semver` and follow what it says.

### Lots

- **A** — firmware, number of rooms: `pn`, `kPiecesMax`, HA-mode room index, weather pages of rooms 5+, dots, popups, Maison pages, wheel, climates, adjustable tile link. Elements per room stay at five.
- **B** — firmware, cut tiles: `kTuilesMax`, `T` keys `a` … `j`, the layout function and its tests, cells drawn and touched, shoulders, rows of the popups. Rooms stay at five.
- **C** — Home Assistant: blueprint, generator, assistant, tests, docs.
- Each lot can be merged alone. A and B touch the same files (`tab5_tuiles.cpp`, `tab5_tuiles_priv.h`, the Maison popup): the second to merge rebases on the first and keeps both.

## Consequences

- Up to 10 × 20 = 200 elements. A crowded room is the user's choice; what the screen draws must stay « to the pixel » (author's preference): every lot that changes the display reads the off-device render, Tab5 and 1024 × 600, before it is merged.
- The weather's left swipe goes on to the extra rooms when `N > 5`: the `[AI-WARNING]` of `forecast_page_suivante()` is amended (author's decision of 2026-10-18), its right side is not.
- Not tried on a screen when written; the author's Tab5 is at M5Stack's repair service: the Waveshare 7B is the board of the tries.
