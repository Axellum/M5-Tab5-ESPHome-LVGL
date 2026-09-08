# ADR-0014: The 8 consoles share `game_common.h`; every console keeps its palette local

**Status:** Accepted (2026-09-08)
**Date:** 2026-09-08 (audit of 2026-09-06, §4.2 items 21, 22 and 23)

## Context

Each console (`marble`, `arkanoid`, `pinball`, `lode`, `go`, `trivia`, `draughts`, `chess`) is an isolated sub-module: its own LVGL page, its own C++ namespace, no Home Assistant dependency. That isolation was paid for with copy-paste:

- `mk_rect()` / `mk_label()` were defined **eight times**, byte-identical; `show()`, `set_bg()`, `set_border()`, `set_text_if()` eight times with small drifts (null guards present or not, a default opacity here, an early return there); `clampf()` three times; the xorshift32 generator five times; and the NVS cycle (`make_preference<T>(key)` once, `load()` + magic check, `save()` + `sync()`) eight times with the same three statics.
- Two palette conventions coexisted: `Lode`, `Go`, `Trivia`, `Draughts` and `Chess` declare a local `namespace Pal` in their own header, while `Marble`, `Arkanoid` and `Pinball` had **76 tokens** (`UIColor::MARBLE_*`, `ARK_*`, `PIN_*`) inside `tab5_custom.h` — the shared HMI header — with a comment saying "used only by the Marble namespace, do not mix with the HMI".
- Go and chess had a Python mirror of their engine (`tools/test_go_engine.py`, `tools/test_chess_perft.py`); draughts had none, although its move generator (majority rule, flying kings, captured pieces staying on the board until the end of the sequence, no promotion mid-capture) is exactly the kind of code where rule bugs hide.

## Decision

1. **`Tab5/game_common.h`**, header-only, is the single home of the helpers shared by the consoles: `clampf()`, `xorshift32_next(uint32_t&)` (the *state* stays in each game — it is seeded per run and sometimes persisted), `mk_rect()`, `mk_label()`, `show()`, `set_bg()`, `set_border()`, `set_text_if()`, and `NvsSlot<T>` (key + magic, `load()` returns false on a missing/foreign save so the game applies *its* defaults, `save()` stamps the magic and syncs, `ready()` mirrors the former `g_pref_ready`). Everything is `static inline` or a template: one copy per translation unit, no "defined but not used" warning, no HMI dependency (`tab5_custom.h` is **not** included).
   The shared variants are the *supersets*: null guards everywhere, default opacity `LV_OPA_COVER`, and the pinball `show()` that returns early when the flag does not change (setting an already-set flag was a no-op anyway).
   The helpers live in `namespace GameCommon` with a `using namespace GameCommon;` at the end of the header, so games call them unqualified as before. A game may **redefine** a helper in its own namespace when it needs a different behaviour — `Go::set_bg()` also resets `bg_grad_dir` — and its definition wins for unqualified lookup. Defining the shared helpers in the *global* namespace instead would make such calls ambiguous: `lv_obj_t*` is a global-namespace type, so argument-dependent lookup would find the global helper too (that is exactly the first compile error of this lot).
2. **Palettes stay local**: `Marble::Pal`, `Arkanoid::Pal` and `Pinball::Pal` now live in `marble_game.h`, `arkanoid_game.h` and `pinball_game.h`, like the five other consoles; `tab5_custom.h` loses 99 lines and no longer knows any game. The three "mirror" colour tokens of `tab5-styles.yaml` (page background, playfield, HUD — needed by the LVGL YAML containers) stay, with their comments pointing at `<Game>::Pal::*`. Adding a console never touches a shared HMI file for a colour.
3. **Every engine gets a Python mirror**: `tools/test_draughts_engine.py` transliterates `Draughts::Engine` (same DR/DC tables, same generation order, same majority filter) and is checked against the reference perft values of both variants (international 10×10: 9, 81, 658, 4 265, 27 117, 167 140; English 8×8: 7, 49, 302, 1 469, 7 361, 36 768), plus targeted rule tests (majority rule, forward-only men in English, no promotion mid-capture, flying-king landings, a captured piece blocks and cannot be re-captured). It runs under `pytest`, so in CI. It also records the largest legal-move count seen (12), far below the C++ `MAX_MOVES = 96` that silently truncates.

## Consequences

- 754 lines removed from the eight consoles, 137 added in `game_common.h`; 3 palettes moved (96 tokens, 257 call sites renamed `UIColor::X_*` → `Pal::*`).
- A helper fixed once is fixed for the eight games; a ninth console starts from the header instead of copying a sibling. Rule 6 of "adding a 9th console" in `Tab5/README.md` says so.
- The mirror is a *transliteration*: a change to `gen_moves()` / `search_*_caps()` / `apply_move()` in `draughts_game.cpp` must be mirrored there, or the test stops proving anything (same contract as the Go and chess mirrors, `AGENTS.md`).
- Deliberately **not** done: the `lv_timer_create` / `lv_timer_delete` pair (two lines, one call each per game — an abstraction would hide more than it saves), and the per-game `static` state (141 to 240 statics per game, prototype-grade but harmless as long as one console runs at a time, which `GameRegistry::close_all()` guarantees — ADR-0013).
