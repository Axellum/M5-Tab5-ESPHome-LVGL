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
- **HA mode:** the roller of PR #511 over the `N` rooms (left = next, right = previous, `N − 1` → 0, empty rooms kept, « Aucun appareil »). One dot per room. The room shown is its own index, no longer derived from `forecast_page`.
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

## Lot C — Home Assistant (2026-10-18)

What Home Assistant does since lot C (blueprint, generator, setup assistant), not tried with a firmware of lots A or B when written:

- **Blueprint.** `tools/gen_blueprint_emplacements.py` now writes the room inputs too (`PIECES = 10`): a « Pièces · Rooms » section with « Nombre de pièces » (`nombre_pieces`, number, 1 to 10, 5 by default, slider), then « Pièce 1 » to « Pièce 10 » with the same five inputs each (rooms 2 to 10 folded), the lists of variables that read them (`pieces`, `noms_saisis`, `pieces_climat_choix`) and the eight triggers of each room. The labels stay « français · english », like every input of the blueprint (a blueprint has no translations).
- **Payload.** `nombre_pieces` is clamped to 1 … 10 (5 when absent or unreadable). `tuiles` takes the first twenty elements of rooms `0 … N − 1` (`T` = `0123456789abcdefghij`), so a room `R ≥ N` has no definition, no state, no climate (`pieces_climat`) and no command: `tuiles` is still the whitelist of `esphome.tab5_action`, and « Pièce : tout éteindre » accepts `p0` … `p9`. The definitions start with `pn|N;`; the `range(7)` that mixed the rooms with the row (5) and the Ok Nabu panel (6) is now a list of groups, the rooms then `'h'` and `'n'`, in the same order. Triggers `piece_10…` read the room number up to the next `_` (it used to be the 7th character only). The − / + tile's `lien` prefers a tile key an older firmware reads (`t[0-4][0-4]`) when the entity is on several tiles.
- **Compatibility, proved** (`tests/test_nombre_de_pieces.py`): with « Nombre de pièces » left at 5 and five elements a room, every payload is the one of the blueprint before lot C (rendered by `fe80f1eb`, kept in the test) except `pn|5;` at the head of the definitions; with more elements or more rooms, the entries a 3.9 firmware reads (its one-digit keys `0`-`4`) are exactly the same as before. The same comparison ran in the author's Home Assistant (`POST /api/template`, read-only, on his real automation): identical but for `pn|5;`, and a synthetic room of 22 real entities gave `t60` … `t6j`. A home of 10 × 20 elements with 30-character names makes 10.6 KB of definitions, under `kPayloadMax` (16 KB); names are not capped by Home Assistant, so a home with much longer names could exceed it (lots A and B: same cap).
- **Setup assistant** ([ADR-0052](0052-setup-assistant-rooms.md), update of 2026-10-18): up to ten areas and twenty tiles, five tiles proposed a room, `nombre_pieces` written as the number of rooms picked.
- **Order of deployment.** Either order works: an older firmware ignores `pn` and the new keys, a new firmware with an older blueprint gets no `pn` (5 rooms of at most 5 elements).

## Lot A — firmware, number of rooms (2026-10-18)

What the firmware does since lot A, not tried on a screen when written (render only):

- **`pn|N`.** Read at the head of `tab5_maj_tuiles` (`pieces_nombre_lire()`, `Tab5/socle/tab5_core.cpp`, tested on a PC): absent, empty or unreadable → 5, clamped to 1 … 10. Arrays are sized on `kPiecesMax = 10` (`tab5_geometrie.h`); `pieces_nombre()` gives N (5 in legacy mode), and a room `R ≥ N` exists nowhere: `tuile_presente()` refuses it, its definitions are cleared from the snapshot, and the cards, dots, popups, wheel, climates and the adjustable tile's link all stop at N. Room digits `0` … `9` are read by `modele_ha::piece_index()` (the `tRT` and `pR` keys, `crR`/`ceR`, `pR` of the history).
- **Proof that 3.9 ignores the new keys** (`tests/test_tuiles_firmware.py`, `test_le_firmware_3_9_ignore_les_cles_des_dix_pieces`): reads `lire_entree()` and `chiffre_0_4()` of `v3.9.0-rc.5` from git and checks that `pn`, `p5` … `p9`, `t5x` … `t9x` and `t0a` … are read as neither a room, a tile nor a zone.
- **HA mode.** The room shown is `s_piece_ha`, independent of `forecast_page`; `forecast_page` still follows the room's page (`page_de_piece()`), so what reads « page 2 » keeps its meaning, and leaving HA mode shows that room's weather page (home when that page does not exist). The roll goes through the N rooms (`reglages_page_voisine()`), one dot per room; the central card shows the room's name only (the small « Pièce n/N » line above it was removed on 2026-10-18, author's request).
- **Weather mode.** Pages 0 … 4 unchanged; rooms 5 … N − 1 have pages 5 … N − 1 after days 10-14, on a left swipe (`meteo_page_suivante()`, pure, tested), drawn by the same function as HA mode (`peindre_cartes()`), with the room's title (a tap opens Maison); the 25 s return to home applies (it only looks for page 2). Only when « Tab5 Appareils sur la météo » is on: with it off, the weather keeps its five pages. 5 + (N − 5) dots.
- **Lists of rooms.** Lights and Shutters: ten tab buttons, one per room that has such a device (`pages_onglets()`). Maison: at most five columns a page, `pages_repartir()` spreads the rooms evenly (8 rooms → 2 pages of 4), ADR-0046 pages with « Pièces 1 à 4 » tabs. Navigation wheel, « Pièces » family: when Maison plus the occupied rooms exceed the eight choices, page 1 holds Maison, six rooms and « Suite », page 2 « Retour » and the rest (same look). Temperature popup: keys `p0` … `p9`, a window of seven tabs around the current one.
- **Memory.** NVS partition: 0x70000 (448 KB, `partitions.csv` that ESPHome writes in the build folder, read on 2026-10-18; the YAML sets no partition table). The model grows from 1 608 to 3 208 bytes (8 + 10 × 25 + 10 × 5 × 59) and its magic is `0x54550000 | sizeof(Modele)`: any layout change (lot B included) changes it, the old record is not read and the screen waits for Home Assistant's snapshot. With lot B: 12 060 bytes (8 + 10 × 25 + 10 × 20 × 59, plus 2 bytes of written tail padding), see « With lot A » below.
- **History keys on the Home Assistant side.** The lists of `packages/tab5_historique.yaml` and the blueprint gain `p5` … `p9` in the same PR (lot C had kept `p0` … `p4`: the blueprint did not start the history of rooms 6 to 10); `tests/test_piece_climat.py` keeps the firmware's twelve keys and Home Assistant's equal.
- **What follows the cards rather than HA mode** (review of the lot): on a weather room page, the tile wheel is anchored on the card's badge, Maison and the navigation wheel mark that room (`tuiles_piece_montree()`), the « Prévisions de … » notice is hidden and the room title comes back after a voice answer; the temperature zone and the climates keep following HA mode only (`tuiles_piece_mode_ha()`).

## Lot B as built (2026-10-18)

Choices made while writing lot B, within the decision above (not tried on a screen yet):

- **The layout is one pure function**, `disposition::disposer(presents, positions_fixes)` (`Tab5/socle/tab5_disposition.h`, tested for n = 0 … 20 by `tools/test_tab5_socle.cpp`): the present elements follow one another in index order, without gaps (an element missing from the list leaves no hole). **Weather mode up to five elements, all of index < 5, keeps element T on place T** (`positions_fixes`): a room with spaced tiles, and the legacy mode (its shutter on tile 1, `btn_j1_dir`), keep today's weather. HA mode centres the `utilisees` first places, as before.
- **A place is the touch target of its whole element only**: the YAML cards (`switch_card.yaml`) and weather buttons now call `tuile_appui(place)`, which finds that place's whole element (`element_de_place()`); the element index stays the identity everywhere else (states, keys, popups, wheel).
- **Cells are created in C++** on the HA layer (`Tab5/ecran/tab5_tuiles_cases.cpp`), four per place, the first time that place is cut: a house with five elements per room creates none. Background and border: the cards' shared style (`style_meteo_card_page`, which themes redraw); texts and icons: the active palette. No disc: a half has a 45 px icon, the name in 22 px and the value in 32 px (22 px if it does not fit), a quarter a 32 px icon, the value and the name when the cell is tall enough. Cells are equal, glued to the place's edges, at least `kAccCaseEcart` apart (`disposition::case_rect`).
- **Pages of the one-row-per-element popups**: Lights and Shutters, five rows per page, one name per room at the top (« Living room 2/3 » on the room shown; a single room of several pages: one name per page); « Open all » / « Close all » act on the whole room. House: five rows per column and per page, page names « 1–5 », « 6–10 »… between the title and « Éteindre les lumières », the swipe of the pages brick.
- **Memory**: the NVS magic is `"TU" + sizeof(Modele)`: any later layout (lot A's ten rooms) changes it by itself, whichever lot merges first. The model is read on the heap (6 KB here, ~12 KB with lot A; the loop task has 8 KB of stack). The climate table keeps one pointer per possible climate and allocates a case (≈ 180 bytes, PSRAM) only for a climate that HA has pushed.
- **With lot A** (merged after it, 2026-10-18): ten rooms of twenty elements, 10 × 20 `Def` in NVS (~12 KB, read on the heap) and 210 possible climates (pointer table ≈ 840 bytes in external BSS). The cards of a weather room page (rooms 6 and beyond) are drawn like HA mode's, cut places and cells included (`cartes_affichees()` rather than HA mode: cells, their touch, the wheel's anchor, `element_de_place()`). Lights and Shutters: one name per room (up to ten), « 2/3 » kept whole after it. House: a page is a group of at most five columns (lot A) × a slice of five rows (lot B), all of them swiped through; its tabs name the groups (« Pièces 1 à 4 », with « n/m » on the group shown when it has several slices) or, with a single group, the slices (« 1–5 », « 6–10 »), four tab buttons at most.
