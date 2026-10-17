# ADR-0005: Blocking boot delay before the display reset sequence

**Status:** Accepted (confirmed root cause, 2026-07-06); amended 2026-10-17: the length of the wait comes from the board

## Context

See the full incident writeup in [`docs/troubleshooting.md`](../troubleshooting.md#black-screen-after-a-software-reboot-not-a-power-cycle). The display's `reset_pin` is wired through the `PI4IOE5V6408` I2C GPIO expander rather than a native GPIO. Right after boot, toggling the expander's output too early silently has no effect on the display, causing a black screen on some software reboots.

## Decision

A blocking `delay(1000)` was added in `on_boot: priority: 700` in `tab5-hardware.yaml`, immediately before the display reset sequence, giving the expander time to become reliably responsive.

## Consequences

- Adds a full second to every boot — a real, accepted cost, not an oversight.
- This code is marked `[AI-WARNING]` in `tab5-hardware.yaml` specifically because a fixed `delay()` at boot looks like exactly the kind of thing a "boot time optimization" pass would flag and remove. **It must not be removed without re-testing across multiple software reboots** — the root cause was only confirmed after 5 live tests with a human watching the physical screen; a shorter delay or a polling-based readiness check has not been validated.
- This superseded an earlier, weaker mitigation (leaving `logger: level: VERY_VERBOSE` on permanently, which correlated with fewer black-screen occurrences for unclear reasons). That workaround is no longer needed but is left as a historical note in case the root cause explanation turns out to be incomplete.

## Amendment (2026-10-17): one wait per board

The wait lives in `tab5-ha-hmi.yaml` (`on_boot: priority: 700`), shared by every board since [ADR-0061](0061-board-and-screen-size.md). Its length is now a substitution of the board package, `carte_attente_ecran_ms`:

- **Tab5: 1000** (`Tab5/paquets/carte-tab5.yaml`), unchanged. Its resolved configuration is the same as before except for the new substitution (the lambda still reads `delay(1000);`, for all three panel revisions), and the generated C++ is identical but for one comment line.
- **Waveshare 7B: 0** (`Tab5/paquets/carte-waveshare-7b.yaml`). It has no I/O expander: the reset of its EK79007 panel is GPIO33, a pin of the P4 itself, driven by ESPHome's `mipi_dsi` driver with its own timing (5 ms high, 5 ms low, then 120 ms before « sleep out »). The 7B used to pay the second at every boot (boot timeline: `avant1s` → `apres1s` = 1,000 ms). Neither of the two public ESPHome configurations that run on this board (agillis/esphome-modular-lvgl-buttons, Jordi1990/openamber) waits before the display. Tried on 2026-10-17: 10 restarts out of 10 from the « Redémarrage Système » button, every step after the wait 1 s earlier (first image 4.78-4.84 s instead of 5.84-5.90 s), and the screen back on with its picture after a restart, seen by the author.

The `[AI-WARNING-CRITICAL]` stays: the Tab5 value must not go down without the re-test described above. A new board with its reset pin behind an expander starts from 1000.
