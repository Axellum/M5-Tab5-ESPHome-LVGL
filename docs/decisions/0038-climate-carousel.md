# ADR-0038: The climate popup is a carousel — one page per climate the tablet knows, opened by a tap on the room's temperature

**Status:** Accepted (2026-10-09, asked for and decided by the author; not tried on a tablet when written)
**Date:** 2026-10-09

## Context

Since [ADR-0027](0027-climate-per-tile.md) the climate popup shows the « displayed climate »: the blueprint's (the home card, « Aller à l'écran → Climatisation », a tile with option m) or a `cli` tile's own unit (a tap on that tile). A home with several units had to find each unit's tile, on its room's page, to reach it. A tap on the living-room temperature of the climate card unfolded the list of the − / + tile ([ADR-0033](0033-adjustable-tile.md)); its long press opens the temperature history ([ADR-0032](0032-temperature-history-popup.md)).

The author asked (2026-10-09) for « a carousel to manage the climate when I tap the room's temperature (history on long press) », and chose a carousel of climates, each with its settings in compact rows. The list of the − / + tile must stay reachable. A later change will add a climate of its own to each room (the blueprint's « Pièce N » sections).

Constraints: one source rather than N copies (code rule 5); the shared modal chrome ([ADR-0009](0009-modal-shell-header.md)); one modal registry ([ADR-0013](0013-single-registry-consoles-modals.md)); instant transitions, no fade (the author's taste); the climate popup stays non-factorised per button ([ADR-0007](0007-climate-popup-not-factorized.md)).

## Decision

- **The carousel is the existing popup.** A page is the popup repainted for one climate (`clim_afficher_blueprint()` / `clim_afficher_tuile()`, the displayed climate of ADR-0027): title = its name, its own bounds, step and unit, the buttons it lacks hidden and the OPTIONS sections restacked (ADR-0026). No second popup, no new registry entry: « Climatisation » keeps its line. The popup is a carousel wherever it is opened from (card setpoint, tile, « Aller à l'écran »).
- **One list of climates**, `clims_enumerer()` (`tab5_clim.cpp`, declared in `tab5_internal.h`): the blueprint's first (unless Home Assistant declared it absent, zone `CLIM`), then every `cli` tile without option m whose settings (`crRT`) arrived, room by room, tile by tile. A unit whose non-empty name equals one already listed (the blueprint's unit also placed on a tile without m) gets no second page. The opening, the swipe and the dots read only this list; a room's own climate will be one line there and one case in `clim_ref_afficher()`. Each entry (`ClimRef`) carries its room.
- **Swipe left / right** = next / previous climate, looping (`reglages_page_voisine()`, as the Settings pages), instant. As in the Settings popup, `LV_OBJ_FLAG_GESTURE_BUBBLE` is removed from the popup so the gesture never reaches the forecast or room pages behind it, `lv_indev_wait_release()` keeps the lifted finger from pressing the button where the swipe started, and a drag on the arc stays a setpoint change.
- **Dots** under the three cards (the central card's: 16 × 4, the current one 30 wide and opaque, `pagination_afficher()`), one per climate, at most 8 (`kClimPastilles`); a ninth unit keeps its tile, with no page. One climate: no dots, no swipe.
- **Gestures of the home card.** A tap on the room's temperature (`btn_reglables_liste`) opens the carousel on the climate of the room shown in HA mode when it has one, otherwise on the first page (the blueprint's); with no climate at all, it still unfolds the − / + list. Its long press (history) is unchanged. The − / + list moves to a **long press on the value between − and +** (`btn_clim_target_click`, whose tap is unchanged).
- **A unit without a name** (the `crRT` name field empty) is titled with its tile's room, so pages stay distinct.

## Rejected

- **A second popup made of compact rows per climate**: a new layout to keep « to the pixel » in 21 themes next to the existing one, two places for every climate command (ADR-0007 already keeps one explicit handler per button). The existing three cards already hide what a unit lacks; a redesign into rows, if the author wants it after seeing the screen, can be done inside this popup.
- **Pages that slide with the finger**: an animation the author does not want on popups; the page changes at once.
- **Dynamic dots created in C++**: the central card's and the row's dots are YAML objects themed by role styles; eight fixed ones cost nothing and stay in the theme repaint.

## Consequences

- One more touch zone meaning: the − / + list is now a long press on the value (documented on the card and in `docs/screens.md`). The setpoint's tap and the room temperature's long press are unchanged.
- The popup no longer passes horizontal gestures to the page behind it (the Settings popup had that bug before the same fix; for the climate popup it is read from the code, not observed on the screen).
- `tests/test_carrousel_clim.py` holds the single list, the gesture, the dots and the card gestures; the off-device render captures the popup with one climate (`climatisation-par-la-piece`) and with three (`climatisation-carrousel*`, two tile units pushed then forgotten).
