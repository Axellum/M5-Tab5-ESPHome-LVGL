# ADR-0009: Shared modal chrome + one compact title bar for every popup

**Status:** Accepted
**Date:** 2026-07-25
**Supersedes:** the v3 chrome shipped earlier the same day (`modal_header_brand.yaml` + `modal_close_btn.yaml`, both deleted here)

## Context

The nine popups (light, climate, pots, calendar + day detail, system console, TV remote, voice assistant) all reimplemented the same window frame with small divergences: three card sizes (1250×690, 1230×670, 1180×680), three scrim opacities (85 %, 72 %, 60 %), three ways of drawing a title (icon+title, centred title with a subtitle, fully inline duplicate), two close-cross sizes. Because every popup re-derived its own `y:` offsets, the title was never drawn at exactly the same place twice.

v3 introduced three `!include` templates but adoption was partial — the assistant duplicated the whole chrome inline, the TV remote skipped the title template, the day-detail sub-popup used a bare label — so the divergence survived. Axel also asked for a *compact* bar: as little dead space above the title as possible.

## Decision

1. **One include produces the whole title bar.** `Tab5/ui_components/modal_header.yaml` is a full-width container (`width: 100%`, `height: 52`) holding icon + title (left) and the close button (right). Vertical alignment is done by LVGL (`LEFT_MID` / `RIGHT_MID`), not by per-popup `y:` values — the bar cannot drift. It replaces `modal_header_brand.yaml` and `modal_close_btn.yaml`, both deleted.
2. **Compact geometry, mandatory for every popup**: 4 px above the line, 44 px line height, 4 px below → body starts at `y: 52` (was 80). Icon and close cross in `mdi_font_32`, title in `roboto_32_b`, close button 80×44, 10 px from the sides.
3. **One scrim.** `modal_scrim.yaml` takes a `scrim_opa` var — `85` for top-level popups, `60` for `cal_day_popup` (it stacks on an already-dimmed screen). No inline scrim anywhere; `style_modal_overlay` (72 %) is deleted.
4. **Geometry tokens.** `Tab5/tab5-ui-tokens.yaml` (an ESPHome package merging its `substitutions:`) owns `modal_card_w/h` (1250×690 for every popup) and `modal_body_y`. Window size is now a single knob. Note: `${...}` must be quoted inside YAML *flow* mappings (`y: "${modal_body_y}"`) — substitutions are applied after parsing, and a bare `{` breaks the flow parser.
5. **Header options stay siblings.** ESPHome vars are string substitutions — a widget subtree cannot be injected — so the calendar's month navigation and "Aujourd'hui" button remain siblings of the include, pinned to `y: 4, height: 44` (4 + 44/2 = 26 = 52/2, i.e. the bar's own axis).
6. **No subtitles.** The TV model line and the assistant's capability line are gone: one bar variant, no exceptions.

Rejected again: a single runtime modal instance whose centre is swapped — impossible with static ESPHome/LVGL YAML.

## Consequences

- Changing the cross, the scrim or the title alignment = editing one file. Changing every window's size = editing one substitution.
- `scripts/check_tab5_modal_chrome.py` (workspace root repo) reports any popup that reintroduces an inline scrim/cross, a hard-coded card size, or a card without the shared bar.
- The title dropped from `roboto_45_b` to `roboto_32_b`. This is a deliberate exception to the "don't touch my font sizes" rule of #T166 — it is the lever that makes the bar compact, and it was decided explicitly.
- 28 px of usable height recovered per popup. Bodies were re-laid out accordingly: calendar rows 82 + r*95 (cells 91 px tall instead of 84), console 2×2 grid re-centred at x 63/637 with 300 px cards, assistant and TV columns stretched to the new card height.
- `mdi_font_45` is no longer used by any chrome; `mdi_font_32` gained 4 glyphs (F024A, F0E17, F0141, F0142).
- Popups that need a *dynamic* title (calendar day detail) pass their existing label id as `title_id`; the C++ side (`cal_render_day_detail()`) is unchanged.
