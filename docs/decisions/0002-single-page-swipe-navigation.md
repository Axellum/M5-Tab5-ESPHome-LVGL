# ADR-0002: Single LVGL page + swipe navigation instead of a multi-page tab bar

**Status:** Accepted (supersedes an earlier multi-page design — see note below)

## Context

An early iteration of this project (and an early draft of `docs/architecture.md`, corrected in the same change that added this ADR) used a classic multi-page layout: a bottom tab bar switching between `page_accueil` / `page_meteo` / `page_clim` / `page_plantes` / `page_console` / `page_planning`. That draft never matched the shipped firmware.

## Decision

The shipped UI is a single 1280×720 `page_main`. Home content (clock, indoor temp, quick actions, climate card, moisture card) is always visible at once. Secondary interactions (climate detail, light control, diagnostics console) are fullscreen overlays (`climate_popup`, `light_popup`, `console_sys`) opened by tapping the relevant card or button, not separate pages reached via a tab bar. A central card rotates automatically between planning/rain/alerts every 8 seconds (a 4th "info" panel — calendar recap / alert banner — was added 2026-07-14). A bottom card region shows either the switches card or the forecast card at any given time — a dedicated toggle button (`btn_control_ha`, top right) flips a `show_switches` global and swaps which one is visible (`LV_OBJ_FLAG_HIDDEN`, not a separate page); when in forecast mode, that region additionally cycles through 5 forecast windows (2 hourly + 3 daily) via left/right swipe (since 2026-07-14 the swipe zone is limited to the lower band, `y ≥ 333`, and the console opens via its own button rather than an up/down swipe).

## Consequences

- No tab bar means no screen real estate spent on navigation chrome — the whole 1280×720 is dashboard content.
- Anything not on the home screen is reached by touch/swipe gesture rather than a persistent nav element, which is less discoverable but matches the "glanceable dashboard" goal (most information should be visible without interaction).
- Documentation describing this project **must** stay in sync with this — the multi-page draft was a real, confirmed source of confusion for docs readers (and for AI agents skimming `docs/architecture.md` before this fix). Any new doc describing screen structure should reference this ADR.
