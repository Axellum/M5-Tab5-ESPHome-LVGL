# ADR-0005: Blocking boot delay before the display reset sequence

**Status:** Accepted (confirmed root cause, 2026-07-06)

## Context

See the full incident writeup in [`docs/troubleshooting.md`](../troubleshooting.md#black-screen-after-a-software-reboot-not-a-power-cycle). The display's `reset_pin` is wired through the `PI4IOE5V6408` I2C GPIO expander rather than a native GPIO. Right after boot, toggling the expander's output too early silently has no effect on the display, causing a black screen on some software reboots.

## Decision

A blocking `delay(1000)` was added in `on_boot: priority: 700` in `tab5-hardware.yaml`, immediately before the display reset sequence, giving the expander time to become reliably responsive.

## Consequences

- Adds a full second to every boot — a real, accepted cost, not an oversight.
- This code is marked `[AI-WARNING]` in `tab5-hardware.yaml` specifically because a fixed `delay()` at boot looks like exactly the kind of thing a "boot time optimization" pass would flag and remove. **It must not be removed without re-testing across multiple software reboots** — the root cause was only confirmed after 5 live tests with a human watching the physical screen; a shorter delay or a polling-based readiness check has not been validated.
- This superseded an earlier, weaker mitigation (leaving `logger: level: VERY_VERBOSE` on permanently, which correlated with fewer black-screen occurrences for unclear reasons). That workaround is no longer needed but is left as a historical note in case the root cause explanation turns out to be incomplete.
