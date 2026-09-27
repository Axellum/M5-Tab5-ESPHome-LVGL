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
- **Captures** (`tools/rendu/capturer.py`, CI job `rendu-host.yml`): the render runs on the runner, the demo scenes are pushed through the real API as Home Assistant would, and `rendu_capture` writes one BMP per scene, in French then in English (the « Langue » select restarts the render, as it restarts the tablet). Date, time and time zone are frozen (`faketime`, `Europe/Paris`), so two runs give the same pixels (checked on 2026-09-27).
- **Comparison** with `docs/images/rendu/`, which are also the gallery of `docs/screens.md`: informative, a changed screen is reported with a diff image, not blocking (Axel's choice).

## Consequences

- A few firmware lines changed for portability, with the same behaviour on the tablet: `std::isnan` instead of `isnan` (`tab5_cards.cpp`, `tab5_forecast.cpp`, one lambda), and the dead Arduino branch of the memory card removed (`tab5_console.cpp`).
- A new hardware component used by the interface must get its stub, or the render stops compiling. The CI job says so on the pull request.
- What the render does not show: real touch, sound, the voice assistant, the downloaded image of an assistant answer (a black pixel here), timings and memory. It is a picture of the layout, not a device test.
- Updating a picture on purpose: `python tools/rendu/maj_references.py --run <id>`, then review and commit the PNGs.
