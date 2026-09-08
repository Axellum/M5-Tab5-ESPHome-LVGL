# ADR-0013: One C++ registry lists the 8 consoles and the modal windows — no list is ever copied into YAML

**Status:** Accepted (2026-09-08)
**Date:** 2026-09-08 (audit of 2026-09-06, §4.1 item 5)

## Context

Two lists were maintained by hand in several places:

- **Which consoles exist.** The 8 game namespaces (`Marble`, `Arkanoid`, `Lode`, `Go`, `Trivia`, `Draughts`, `Chess`, `Pinball`) were enumerated in four YAML lambdas: `tab5_games_close_all` (close the running game), the 1 s interval of `tab5-scripts.yaml` (skip the auto-close while a game runs), `tab5-imu.yaml` twice (push the 3 axes to every game, and pick the 10/30 Hz poll rate from a *subset* of them), and the `tab5_screen_name` text sensor of `tab5-ha-controls.yaml` (name the game to HA). `Tab5/README.md` itself listed six steps to add a console, three of them being "add the namespace to this list too".
- **Which modal windows exist.** The 8 popups (+ the alarm ring layer and 3 sub-windows) were enumerated in three tables: `kPopups` in the auto-close interval, `kPopups` again in the "go to screen" select, `kScreens` in the text sensor, plus `kTargetOf` (select index → popup). The file header of `tab5-ha-controls.yaml` said a new modal "must be added to BOTH tables", and the auto-close comment said the same for its own list.

The project history already shows the failure mode: before `tab5_games_close_all` existed, Trivia was missing from 5 of the 8 selector cards. A 9th console or a 10th popup had five to seven places to update, none of which failed to compile when forgotten.

## Decision

1. **`Tab5/tab5_registry.h` / `.cpp`** is the single owner of both lists.
   - `GameRegistry::kGames[]` is a `constexpr`-style table `{name, is_open, close, on_imu, imu_fast}`. The game namespaces are plain C++, so the table lives entirely in C++. `any_open()`, `open_name()`, `close_all()`, `any_imu_fast_open()` and `dispatch_imu()` replace the four hand-written enumerations. "Neon Apron" stays **last**: its `close()` restores `rotation: 270` and must have the last word (ADR-0012).
   - `ModalRegistry` is a fixed array (16 slots, no heap) of `{lv_obj_t*, name, kind}` with three kinds: `POPUP` (named, faded out by `close_popup_if_open`, auto-closed after `UIIdle::POPUP_MS`), `SUBWINDOW` (never named, hidden flat together with the popups: calendar day detail, console confirmations), `LAYER` (named but **never** closed by the registry: the alarm ring screen). `visible_name()`, `any_popup_visible()`, `find(name)` and `close_all()` replace `kScreens`, both `kPopups` and `kTargetOf`.
2. LVGL widgets are only reachable through `id()` inside a YAML lambda, so the modal list is written **once**, in the `tab5_modal_registry_init` script of `tab5-scripts.yaml`. The script is idempotent and every reader executes it before reading; whichever runs first after boot fills the table, the others find it ready. This avoids touching the protected `on_boot` sequence of `tab5-ha-hmi.yaml`.
3. The "go to screen" select looks its target up **by option label** (`ModalRegistry::find(x)`), so the select option and the registered name must be identical. A mismatch logs a `WARN` instead of silently doing nothing.
4. `tools/check_tab5_registry.py`, run by `pytest` in CI, enforces the decision: every `*_game.h` is in `kGames` and vice-versa; no `X::is_open()`, `X::close()` or `X::on_imu(` in any YAML; the legacy table names are gone; every `style_modal_card` popup is registered; every select option has a registered label.

## Consequences

- Adding a console is one line in `kGames` (plus its page, open script and selector card). Adding a popup is one `ModalRegistry::add(...)` line (plus a select option if it should be reachable from HA). Forgetting either **fails the test suite** before anything is flashed.
- The IMU poll-rate subset is no longer a second list but a flag on each console (`imu_fast`); the reasoning (shake-only games stay at 10 Hz, the pinball nudge needs 30 Hz) sits next to the table.
- `any_popup_visible(lv_obj_t* const*, int)` disappears from `tab5_custom.h`: its only caller was one of the removed tables.
- The registration script runs a dozen pointer stores once per boot; the readers loop over at most 12 entries per second. Nothing measurable.
- Every lambda that used to spell out a list now depends on `tab5_registry.h`; a console header that changes its `on_imu` signature breaks the compile in one place, which is the point.
