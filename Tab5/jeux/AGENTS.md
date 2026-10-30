# AGENTS.md — `Tab5/jeux/` (the eight game consoles)

Read with the root [`AGENTS.md`](../../AGENTS.md) and [`Tab5/AGENTS.md`](../AGENTS.md). Claude Code loads this file (through the `CLAUDE.md` next to it) only once it reads or edits a file of this folder.

The eight game consoles are documented separately in [`docs/arcade.md`](../../docs/arcade.md) — read it only when touching a game. Games read `PALETTE_SOMBRE.X` and stay dark (code rule 1), and they are the documented exception to the shared modal chrome (code rule 7).

## An optional module

The games are an optional module ([ADR-0065](../../docs/decisions/0065-games-optional-module.md)): `tab5_jeux` (`oui` on the Tab5, `non` by default on the 7B, which has no IMU) picks `Tab5/paquets/tab5-arcade.yaml` or its empty twin `tab5-arcade-absente.yaml`; the firmware must compile with and without them (`build`, `build-carte-7b`), and nothing outside the module may name a game page, script or namespace (`tests/test_module_jeux.py`).

Engine host tests (Go, chess, draughts) and their Python mirrors: [`tools/AGENTS.md`](../../tools/AGENTS.md).

## Toolbox — games (`Tab5/jeux/`)

- **Games**: `Tab5/jeux/game_common.h` (ADR-0014) — `clampf()`, `xorshift32_next()`, `mk_rect()` / `mk_label()`, `show()`, `set_bg()`, `set_border()`, `set_text_if()`, `set_text_color_if()`, `set_pressed_bg()`, `show_front()`, `hud_num()`, `NvsSlot<T>`, `timer_period_sync()`, `topn_insert()`, `tilt_calibrate()` / `tilt_smooth()`, `accel_delta_norm()` / `shake_fire()`, `game_mem_new()` / `game_mem_free()`, `ui_destroy()`, `SlotMenu<N>` / `SlotGeom`.
