# v1.1.0 — Arcade, ESPHome 2026.7, and a much faster dashboard

> Draft for the `v1.1.0` tag. Tag **after** PR #82 is merged, then paste this as the GitHub release body and delete this file (or keep it out of the tag — it is a working note, not documentation).

`v1.0.5` was tagged on 2026-07-17. **99 commits and 32 merged pull requests later**, it no longer describes what this firmware does. The headline is the Arcade — but the release that matters day-to-day is the ESPHome 2026.7 migration and the performance pass.

---

## Arcade — 8 game consoles

The Tab5's screen is a 1280×720 MIPI-DSI panel driven by an ESP32-P4 at 60 FPS. This release finds out what that actually buys you: **8 complete games, ~18 700 lines of C++**, all rendered with native LVGL widgets — no sprites, no image assets, no framebuffer blitting.

| # | Console | What it is |
|---|---------|-----------|
| 1 | **Fil d'Or** | Marble roguelite, 6 rooms, Dark Souls-style meta-progression — steered by tilting the tablet (BMI270) |
| 2 | **Arcanoïde** | Breakout, 8 levels, power-ups, combo scoring |
| 3 | **Neon Apron** | Neon pinball, 3 balls — **rotates the whole screen to portrait 720×1280** and back |
| 4 | **Coureur d'Or** | Lode Runner (1983), 10 named levels, 5 speed tiers, dig & climb |
| 5 | **Go Tab** | Go 9×9 / 13×13 / 19×19, Chinese scoring, komi 6.5, 4 AI levels |
| 6 | **Trial Poursuite** | Trivia — real 42-space wheel + 6 spokes, 1–6 teams, **720 French questions** in flash |
| 7 | **Dames Tab** | International draughts 10×10 (8×8 checkers option), 4 AI levels |
| 8 | **Roi Noir** | Full FIDE chess, 5 AI levels, **perft-validated** move generator |

Every console: its own LVGL page, all content built in C++, an `lv_timer` created on open and destroyed on close (**zero CPU when no game runs**), a pre-allocated widget pool (**zero heap allocation in the game loop**), NVS persistence, and **no Home Assistant or network dependency at all** — they work with the Wi-Fi off.

Opened by tapping the greenhouse temperature on the dashboard.

**On the AI angle:** these are first-pass, AI-generated prototypes, kept deliberately honest about that in the docs. They exist to probe how far code generation goes on embedded hardware, not to ship retail games. Two of them are backed by host test suites you can run on a plain PC with no toolchain:

```bash
python tools/test_go_engine.py && python tools/test_chess_perft.py
```

The chess generator is checked against the standard perft suite; the Go engine against capture, suicide, ko, eyes, handicap, territory and dead-stone scoring.

### Two problems worth reading about

**The Go engine used ~5 KB of stack per AI recursion level** (2.2 KB in `count_liberties` alone) and crashed the device. Rewritten with zero large stack arrays — every scratch buffer is a module static, since the engine runs single-threaded inside LVGL and never needs to be reentrant.

**The old pinball drew its walls and flippers as LVGL rectangles re-rotated every frame** via `transform_rotation` — ugly and expensive. "Neon Apron" replaces it entirely: `lv_arc` and tight-bounding-box `lv_line` built once, vertical gradients for volume, and only the ball, flippers, flashes and plunger ever move. It also flips LVGL to portrait on open and restores landscape on close — which turned out to be *faster*, because at rotation 0 ESPHome's flush path skips software rotation entirely.

---

## ESPHome 2026.7 — `min_version` is now enforced

**This release will not compile on an older ESPHome.** `tab5-ha-hmi.yaml` declares `min_version: 2026.7.0`.

- **`st7123` is now an official ESPHome touchscreen platform.** The bundled `external_components` shim and the whole `Tab5/my_components/` directory are gone.
- **Audio reworked**: zero-copy path, VAD, noise suppression, auto-gain at 15 dBFS, mic gain 24 dB with DC-offset correction.
- **PSRAM over SDIO** (`esp32_hosted: use_psram`), and `micro_wake_word` moved its task stack into PSRAM.

---

## Faster, smoother, more responsive

A dedicated performance pass, plus targeted work:

- **Calendar popup opens instantly.** Stale-while-revalidate prefetch: the month cache survives closing (10 min TTL), the current month and M+1 are pre-fetched at boot and on HA reconnect, adjacent months are pre-fetched on each render, and distant months are evicted (~4.5 KB ceiling).
- **Popups open and close with no fade** — immediate show/hide, pressed feedback without an LVGL transition.
- **Shorter animations**, plus rolling-digit transitions for the clock and weather icons.
- **Optimistic climate feedback**: the target temperature updates on tap, with a debounced push to HA rather than one call per tick.
- **Lode Runner** got a per-actor render cache and a 5-tier adjustable speed.

---

## New on the dashboard

- **Voice assistant popup** — the STT transcription and the LLM reply rendered as **Markdown** (aligned tables, bold, code), with an optional image downloaded on demand. Left column holds the settings: brain selector (Domotique ↔ Discussion), Ok Nabu toggle, mute, volume, and a persisted A-/A/A+ text size.
- **Monthly calendar popup** — long-press the clock. A 7×6 Monday-first grid computed **locally** from SNTP, with work hours inside the cells and colour markers for public holidays, school holidays, appointments and birthdays. HA enriches each month on demand; tap a day for a detail sub-popup.
- **Plant details popup** — long-press the moisture slots: one glass card per BLE sensor with soil moisture, watering status, fertility (EC), light, temperature and battery.
- **HA alert queue** — up to 4 banners pushed live into the central rotator, each with tap-to-dismiss and a local dismiss list so a re-push of the same id stays hidden.

---

## Fixes

20 fix commits. Two of them came from **Cursor Bugbot reviews** (9 issues on PR #68, 5 more on race conditions, a stale offset and a move counter). Also: a full placement audit of Fil d'Or (11 defects), mid-turn state preserved when reopening Trial Poursuite, `esphome::millis()` used consistently across the games, the HA crossfade made robust against forecast swipes, and the clock now paints on the first NTP sync instead of waiting.

---

## Documentation

`v1.1.0` ships after a full documentation audit against the real code (PR #82). Notably: **demo mode was broken for 14 days** — an argument added to a service on 16/07 was never propagated to `tools/demo/demo_pusher.py`, which raised `KeyError` on every scene. Fixed, with a runtime guard that now names the offending service and argument instead of crashing.

Also corrected: the installation docs advertised ESPHome ≥ 2025.9.3 (following them, you could not compile), pointed at three gitignored files absent from any clone, and `docs/screens.md` still listed a game that had been deleted. All 8 games now have a technical section in [`Tab5/README.md`](Tab5/README.md).

---

## Upgrading

Nothing to do beyond a normal OTA — no breaking change to the Home Assistant service contract, no new required entity.

**But check your ESPHome version first:** below 2026.7.0 the build stops on `min_version`.

If you had a local `Tab5/my_components/` from an earlier clone, delete it — it is dead weight now that `st7123` is official.

---

**Full changelog:** [`CHANGELOG.md`](CHANGELOG.md) · **Compare:** `v1.0.5...v1.1.0`
