# ADR-0021: The screen is rendered off the tablet — the `host` platform, stubbed hardware, no firmware change

**Status:** Accepted (2026-09-27)
**Date:** 2026-09-27 (lot 7 of the « ouverture » audit)

## Context

Every screen change was checked by flashing the tablet and looking at it. The README pictures aged, English overflows were only seen on the device, and a pull request could change the dashboard without anyone noticing.

ESPHome 2026.9.0 can compile a configuration for the `host` platform (a Linux or macOS program) and has a `snapshot` display: LVGL draws into memory and `snapshot.take` writes a BMP. What stood in the way:

- the interface packages call hardware that does not exist on `host`: `voice_assistant`, `micro_wake_word`, `speaker`, `microphone` (both pull the ESP32-only `audio` component), `rtttl`, `online_image`, `http_request`, the I/O expander pins;
- the C++ includes ESP-IDF headers (`esp_heap_caps.h`, `esp_attr.h`, `esp_system.h`, ESP-Hosted, FreeRTOS);
- the interface is initialised by `on_boot` in `tab5-ha-hmi.yaml`, a protected sequence mixed with hardware steps (1 s expander delay, backlight, amplifier, waiting for HA).

## Decision

- **A second entry point, `tab5-rendu-host.yaml`**, loads the same interface packages as the tablet but not the hardware ones (`tab5-hardware.yaml`, `ecran-*.yaml`, `tab5-sensors-diagnostics.yaml`, `tab5-imu.yaml`).
- **Stubs, never the firmware**:
  - `Tab5/rendu/composants/` holds external components named like the ESPHome ones they replace (`voice_assistant`, `micro_wake_word`, `speaker`, `microphone`, `rtttl`, `online_image`, `http_request`). They accept the real configuration, keep the ids and the few C++ methods the lambdas read, and register the same actions and conditions as no-ops (`rendu_muet`). `media_player` stays the real one, with a silent platform;
  - `Tab5/rendu/bouchons.yaml` swaps hardware entries with `!extend` / `!remove` and provides the diagnostic ids the system console reads;
  - `Tab5/rendu/hote/` and `Tab5/rendu/freertos/` replace the ESP-IDF headers for `host` only (copied into the render build by `includes:`).
- **The interface part of `on_boot` is copied**, not moved: the protected sequence stays untouched. `tests/test_rendu_host.py` requires every copied lambda to exist verbatim in `tab5-ha-hmi.yaml`.
- **Captures** (`tools/rendu/capturer.py`, CI job `rendu-host.yml`): the render runs on the runner, the demo scenes are pushed through the real API as Home Assistant would, and `rendu_capture` writes one BMP per scene, in French then in English (the « Langue » select restarts the render, as it restarts the tablet). Date, time and time zone are frozen (`faketime` with a stopped clock, `Europe/Paris`; a running clock crossed a minute during the ~64 s of the three scenes), and the central card's rotator (a new panel every 8 s) is stopped on one panel per scene before the capture (`rendu_panneau`): without it, a capture could fall into the 190 ms cross-fade, or on another panel depending on how many turns had passed (seen on 2026-09-27 on the English rain scene).
- **Comparison** with `docs/images/rendu/`, which are also the gallery of `docs/screens.md`: informative, a changed screen is reported with a diff image, not blocking (Axel's choice).

## Consequences

- A few firmware lines changed for portability, with the same behaviour on the tablet: `std::isnan` instead of `isnan` (`tab5_cards.cpp`, `tab5_forecast.cpp`, one lambda), and the dead Arduino branch of the memory card removed (`tab5_console.cpp`).
- A new hardware component used by the interface must get its stub, or the render stops compiling. The CI job says so on the pull request.
- What the render does not show: real touch, sound, the voice assistant, the downloaded image of an assistant answer (a black pixel here), timings and memory. It is a picture of the layout, not a device test.
- Updating a picture on purpose: `python tools/rendu/maj_references.py --run <id>`, then review and commit the PNGs.

## Amendment (2026-09-27, evening): every screen, four languages

The first version only captured the three demo scenes of the dashboard, in French and English. Popups, sub-windows and games were still checked on the device, and German and Dutch not at all.

- **A virtual finger.** `Tab5/rendu/rendu_doigt.h` adds an LVGL pointer input device to the render only; the actions `rendu_toucher` (press at x, y for a given time: a tap, or a long press from 1 s) and `rendu_glisser` (a 300 ms swipe) drive it through the API. Coordinates are LVGL's logical ones, i.e. those of the PNG captures: ESPHome hands LVGL the rotated resolution and only rotates its own touchscreen's points. Screens open exactly as on the panel: same handlers (`on_click`, `on_long_press`, gestures), same hit testing, including widgets created in C++ by the games.
- **A plan of screens**, `tools/rendu/ecrans.py` (about 80): dashboard variants (forecast pages, switches layer, vigilance and HA alert panels, day planning, voice answer, conversation mode), every popup and sub-window (alarm and ringing, assistant and an answer, calendar and a day, lights, climate, plants, TV remote, system console and both confirmations), the Arcade selector, and for each of the 8 games its hub, menus, a game in progress, pause and end screens. Popups get the data HA would push (calendar month, next appointments, HA alerts). Each screen starts from the dashboard; « Aller à l'écran » → Accueil closes everything afterwards. A game that was started is abandoned first: a game left in progress would be saved and add a « Resume » line to the Go and chess menus, shifting every following tap. `capturer.py` reports a capture identical to an earlier one: the tap missed (stale coordinates after a layout change). `tests/test_rendu_ecrans.py` checks names, bounds, select options and API actions.
- **One CI job per language**, in parallel (French, English, German, Dutch), each with fresh preferences: game statistics and leaderboards are the same in every language, and the run takes about as long as one language.
- **References without growing the repository.** About 330 PNGs per run would add 12 MB or more at each accepted change. Only the gallery (the demo scenes, French and English) stays in `docs/images/rendu/`; a pull request is compared with the captures of the last successful run on `main` (artifact kept 90 days on `main`). Accepting a screen change needs no commit: once merged, `main` becomes the reference.
- Not deterministic, so not captured: states reached through randomness (trivia dice and categories, chess promotion, a lost Arkanoid ball) and anything that needs the IMU.

