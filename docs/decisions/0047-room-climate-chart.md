# ADR-0047: The Temperature popup draws the humidity with the temperature, and has one page per known temperature

**Status:** Proposed (2026-10-09, asked for by the author; not tried on a tablet when written).
**Date:** 2026-10-09

## Context

[ADR-0032](0032-temperature-history-popup.md) gave each home-screen temperature its history, and [ADR-0040](0040-room-climate.md) a room's own temperature and humidity in HA mode (`pR`). The humidity had no history (« the right side has no history »), and the popup showed one temperature: going from the living room to the bedroom meant closing it, changing room, and opening it again. The author asked for « a chart of the temperatures and humidity of the room sensors, if present », a popup « in several parts to switch from one room to another », « clear, very handy, readable, very nice », and optimised code.

Constraints: push-only and events-only ([ADR-0001](0001-push-only-zero-polling.md), [ADR-0025](0025-events-only.md)); no new action variable if it can be avoided (a variable added to `tab5_maj_historique` is a major version of the contract, `contrat/contrat.yaml`); an older firmware must keep working with the new package, and the other way round; colours from the palette ([ADR-0029](0029-themes-palette.md)); every payload read in `Tab5/socle/tab5_parse.*`, tested and fuzzed (lot F); popup chrome shared ([ADR-0009](0009-modal-shell-header.md)).

## Decision

- **Same action, longer fields.** `tab5_maj_historique` keeps its five variables. With a humidity sensor for the slot, HA adds a seventh header field (`…|exterieur|humidity`, the state in whole %, `nan` if unreadable) and three fields to each slot (`mean,min,max,h_mean,h_min,h_max`, whole %, `,,,h…` when only the humidity has a row). Without one, the payload is byte for byte the one of before. An older firmware reads six header fields and three slot fields: it ignores the rest.
- **Which humidity**: the living room's « Salon — humidité » for `salon`, the room's « Humidité de la pièce » for `pR` (ADR-0040), none for `serre`. The blueprint passes it as a new field `humidite` of `script.tab5_historique`; the script reads it from the same `recorder.get_statistics` call (two statistic ids) and rounds it to whole percents. A script field is not part of the tablet's contract.
- **Reading**: `historique_lire()` (`Tab5/socle/tab5_parse.*`), the former inline loop of `historique_recu()` moved as is, plus the humidity (`humidite_lire()`: 0 to 100, else « none »), stored as `uint8_t` (`0xFF` = none). Cases in `tools/test_parse.cpp`, the eleventh parser of `tools/fuzz/fuzz_parse.cpp` (selector `:`).
- **Drawing**: one chart, two scales. The humidity line (3 px, under the 4 px temperature line) uses `HUMIDITY_WET`, or `ACCENT_ALT` when a theme's accent is the same blue (Graphite: an RGB distance under 48); its % labels sit on the right, on the same grid lines as the degrees (step 1, 2, 5, 10, 20, 25 or 50 %, at least 4 % between lines, never past 100 %), and the degrees switch to the accent colour so that each scale has the colour of its line. The legend becomes « Température », « Minimum et maximum », « Humidité ». The fourth card, for a slot with humidity, shows the humidity now (its home-screen colour, `get_humidity_color()`) and its range over the period. Without humidity, nothing changes on screen.
- **Pages**: one tab per known temperature in the title bar, in this order: `salon` (if its zone is not absent), each `pR` with a declared temperature (`piece_climat_a_temperature()`), `serre` (if not absent); none with a single page. Same buttons as the Réglages pages (`historique_onglet.yaml`, 200 px at most, set to the right up to the close button), the shown one in the accent colour. A tap, or a left/right swipe anywhere on the popup (`LV_EVENT_GESTURE` on the popup, `GESTURE_BUBBLE` removed so `page_main` never sees it), shows the next or previous one in a loop, at the same view. The change is instant: the three views of every temperature already shown stay in PSRAM (`Memoire`, about 40 KB), painted at once, then replaced by HA's answer; a late answer for another page is stored under its key instead of dropped.
- **Entry**: a long press on a room's humidity (right side, HA mode) opens the same popup on that room (`accueil_historique_cle(true)` returns `pR` when the room has a humidity).

## Rejected

- **A new variable or action** (`humidite`, `tab5_maj_humidite`): a major version of the contract for every HA file that calls it, while longer fields are already ignored by older firmware.
- **Two stacked charts** (temperature above, humidity below): each gets half the height, the time axis twice, and the cards no room; one chart with two scales reads the link between heating and drying at a glance.
- **A separate humidity popup**: the author asked for the room's climate in one place.
- **Waiting for the shared « paged popup » brick** (lot `feat/popups-par-piece`): not published when this was written. The tabs and the swipe were written here with the Réglages' gesture code (`reglages_page_voisine()`) and button template. Since [ADR-0046](0046-popups-a-pages.md) the swipe is the brick's `pages_brancher()` and the active tab is painted by its `choix_peindre()`; the placement of the tabs stays here (it starts after the title, the brick at x = 318).
- **Keeping only the shown temperature's views** (ADR-0032's ~6 KB): every tab change would show « En attente de Home Assistant » for a moment; 40 KB of PSRAM is cheap.

## Consequences

- Update order: either side alone changes nothing visible. New package with an older firmware: the extra fields are ignored. New firmware with an older package: no humidity field, the popup of before, with its tabs. Both are needed for the humidity curve.
- The humidity sensor needs a `state_class` (`measurement`, as humidity sensors have) for its statistics; without, the popup shows the temperature alone.
- A tab change sends one more `esphome.tab5_historique` event; HA answers it like an opening.
- One more template (`historique_onglet.yaml`, seven includes), three texts (« HUMIDITÉ », « Humidité », « Entre %d et %d %% ») in seven languages.
