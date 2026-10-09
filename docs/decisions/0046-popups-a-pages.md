# ADR-0046: Popups with pages — one shared brick; the Lights and Shutters popups show the whole house, one page per room

**Status:** Accepted (2026-10-09, asked for by the author; not tried on a tablet when written).
**Date:** 2026-10-09

## Context

Since [ADR-0023](0023-rooms-generic-tiles.md) the light popup showed the lights of **one** room (the room of the tile that opened it, up to five), and the shutter popup showed **one** shutter. To reach the lights of another room, one had to close the popup, swipe the bottom row to that room and long-press again. Neither popup could be opened from Home Assistant's « Aller à l'écran » on its own: they needed a tile.

Meanwhile three popups grew pages of their own, each with its own code: the Settings popup (four pages, names at the top, a left / right swipe), the climate carousel ([ADR-0038](0038-climate-carousel.md), a swipe and page dots) and, from the author's request of 2026-10-09, these two. Code rule 5 (one builder rather than N copies) asks for one brick.

Constraints: push-only and events-only ([ADR-0001](0001-push-only-zero-polling.md), [ADR-0025](0025-events-only.md): nothing new reaches Home Assistant); one shared modal chrome ([ADR-0009](0009-modal-shell-header.md)); one modal registry ([ADR-0013](0013-single-registry-consoles-modals.md)); no hardcoded colour ([ADR-0029](0029-themes-palette.md)); instant transitions; every MDI glyph in its font (code rule 9).

## Decision

- **One brick for every popup with pages**, `tab5_pages.cpp` (declared in `tab5_internal.h`). The popup keeps its own page and tells the brick how to count, read and show it (`PagesPopup` {popup, `nombre()`, `courante()`, `afficher(i)`}); the brick keeps nothing.
  - `pages_brancher()`: the left / right swipe anywhere on the popup changes page (left = next, right = previous, wrapping, `reglages_page_voisine()`). The gesture stops at the popup (`LV_OBJ_FLAG_GESTURE_BUBBLE` removed), otherwise LVGL would also swipe the forecast or the room behind it. A swipe started on a slider or an arc is that control's drag, not a page. The finger's release after a swipe triggers nothing (`lv_indev_wait_release()`).
  - `pages_onglets()`: the page names on the title line, next to the ×, right-aligned, 200 px each at most (four pages land exactly on the Settings names; five share 830 px, 158 px each), the shown page in the accent colour (`choix_peindre()`, the same recipe as the active option of a Settings row). Names cut with « … » to fit.
  - **Fewer than two pages: no names, no swipe.**
  - The Settings popup uses it since this ADR (its gesture code moved there, behaviour unchanged).
- **Lights and Shutters: every light / shutter of the house, one page per room** that has one, in the blueprint's room order. A room with none has no page. A row = one tile of that room, drawn by the HA-mode card's function (`tuile_peindre_ligne()`, `piece_ligne.yaml`, five rows at most like a room): its badge runs the tile's own tap (`tuile_appui_maison(r, t, false)`: same command, same `k` confirmation), a long press on the row opens the tile's quick-action wheel around its badge ([ADR-0036](0036-quick-action-wheel.md)), a tap on the row selects it (the arc and colours, or the drawn shutter, then control it). The state shared by both popups is one template, `PopupPieces` (on top of `PopupTuile`: rooms that have such tiles, the page, its tiles, the selected one), filled by `pieces_calculer()`.
- **Opening.** From a tile (long press, then « Détails »): on that tile's room, that tile selected. Without a tile — the screens `Ecran::LUMIERES` / `Ecran::VOLET` of the navigation wheel ([ADR-0042](0042-navigation-wheel.md)): « Aller à l'écran » → « Lumières » / « Volet », the wheel, a home gesture `lumieres` / `volet` —: on the room shown on the home page, else the first room that has one (`tuiles_ecran_ouvrir()`, whose test is the pages' own, `est_lumiere()` / `est_volet()`). With no light (shutter) at all, the screen is « absent » like a screen whose zone is hidden. No screen, code or select option of its own.
- **Lights popup**: the former On/Off button goes (every row's badge does it); « Tout éteindre » stays, for the room shown. **Shutters popup**: the left card lists the shutters of the room with « Tout ouvrir » / « Tout fermer » (each shutter of the page that the popup lists gets its own `ouvrir` / `fermer`, as many `esphome.tab5_action` events as shutters — no new event); the right card is the former popup for the selected shutter (drawn shutter, its name, position, state, Ouvrir / Stop / Fermer). A shutter tile with option `k` (confirm) or `r` (read only) is not listed, as it never opened this popup; the 3.x shutter of legacy mode neither.

## Rejected

- **A new LVGL page per room, or one popup per room**: against the single page ([ADR-0002](0002-single-page-swipe-navigation.md)) and twice the widgets.
- **Page dots instead of names**: with up to five rooms, a name tells where a tap goes; dots stay for the climate carousel, where pages have no short name.
- **An animated page change**: the author's taste is instant transitions (AGENTS.md).
- **A « house » event for « Tout ouvrir / Tout fermer »**: the existing per-tile `ouvrir` / `fermer` already does it, with the tile's own entity; a new event would need a new branch in `tab5_evenements.yaml` and a deployment.
- **Keeping the On/Off button**: it controlled only the selected light, which the badge of its row now does in place.

## Consequences

- No change to the API contract nor to the events, nor to the select or the gesture codes (those of ADR-0042); the blueprint's label of the `volet` gesture says « Volets » (same value).
- More widgets: 10 room names, 5 shutter rows and 2 buttons (the light rows existed); one MDI glyph fewer in `mdi_font_56` (the On/Off icon).
- The climate carousel can move onto the brick later (its dots and its gesture are its own today).
- Not tried on a tablet when written: the swipe on the drawn shutter (its frame keeps the gesture, so it never changes page), the 158 px room names with five rooms and the rows' long press need a look on the real screen.
