# ADR-0037: A House popup — every room of the blueprint at once, one column per room, rows drawn and touched like the tiles

**Status:** Proposed (2026-10-07, asked for in discussion #278; not tried on a tablet yet)
**Date:** 2026-10-07

## Context

Since [ADR-0023](0023-rooms-generic-tiles.md) the bottom row shows one room at a time: five tiles, a swipe to the next room. In discussion #278 (husyildiz, 2026-10-07) the wish was to see the whole house at once, « like a Home Assistant dashboard ». The tablet already holds everything such a view needs: the 5 × 5 tile model, its icons, names, states and colours (`Vue`, `vue()` in `tab5_tuiles.cpp`), and the gestures of each type (`tuile_appui_piece()`, with the confirmation, « on only » and « read only » options).

Constraints: push-only and events-only ([ADR-0001](0001-push-only-zero-polling.md), [ADR-0025](0025-events-only.md)); one shared modal chrome ([ADR-0009](0009-modal-shell-header.md)); one modal registry ([ADR-0013](0013-single-registry-consoles-modals.md)); no hardcoded colour; one builder rather than N copies.

## Decision

- **A modal popup « Maison »**, shared chrome, 1250 × 690 card, registered as the last POPUP; the « Maison » option at the end of « Aller à l'écran » (index 12, the others keep theirs). Also opened, in HA mode only, by a tap on the central card's title (`btn_page_title_tap`, which had no tap until now; the same guard as the central card's long press: no drag, no gesture).
- **One column per room that has devices**, in the blueprint's order R = 0 → 4 (not the order of the pages, so the columns read like the blueprint), width (1250 − 24 − 12 (n − 1)) / n; a 40 px header with the room's name or « Pièce n »; « Aucun appareil » with no room.
- **One row per device**, 104 px, glass card: the HA-mode card's badge, icon, name and state line, painted by the same function (`peindre_vue_sur()`, factored out of `peindre_carte()`), cut with « … » at the column's width.
- **The tile's gestures, by the tile's function.** A row's tap and long press call `tuile_appui_piece()`, so nothing new reaches Home Assistant and the options behave as on the card; a « ⋯ » button repeats the long press on the types that have one (`tuile_gestes()`: not `cap`, `bin`, nor the `r` option). A climate has no long press on its tile; in the popup its long press and « ⋯ » open its climate popup, like its tap. The popup opened from a row comes over the House popup (`animate_popup_open()` moves it last), which stays open behind it.
- **« Éteindre les lumières »** in the title bar, shown when a room has a `lum` tile: for each such room, the light popup's « Tout éteindre » (`pR` / `eteindre`, `tuiles_piece_eteindre()`), no confirmation.
- **Widgets in YAML**, created once (5 headers, 25 rows from `maison_ligne.yaml`), LVGL objects in PSRAM; laid out and painted at each opening, a row repainted when its state arrives and the layout redone when `tab5_maj_tuiles` changes, only while the popup is shown. `tab5_maison.cpp` sends nothing itself.

## Rejected

- **A new LVGL page or a second home screen**: against the single page ([ADR-0002](0002-single-page-swipe-navigation.md)) and the « popups to the pixel » taste.
- **Rows created in C++**: the icon check (code rule 9, `MDI_CODE_TARGETS`) reads widget ids in the YAML; `reglables_ligne.yaml` already set the pattern.
- **A new event or blueprint input** (a « house » scene, a per-room « all off » list): the existing `pR` / `eteindre` already does it.

## Consequences

- The smallest text font is `roboto_22`; there is no 16 or 20 px Roboto. Header, name and state all use it, so with five rooms (≈ 235 px columns, 99 to 155 px of text) long states are cut. A smaller font would cost flash on every glyph set (not measured).
- About 185 more widgets in the firmware (flash for their setup code; the objects themselves go to PSRAM).
- The action wheel of ADR-0036, if merged, plugs into `tuile_appui_piece()`; the climate exception of `tuile_appui_maison()` must then follow it.
