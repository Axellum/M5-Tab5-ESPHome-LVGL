# ADR-0011: `api: reboot_timeout: 60min` — keep the anti-zombie net, stop cycling on HA outages

**Status:** Accepted (2026-08-01)
**Date:** 2026-09-06 (written retroactively from the comment block in `Tab5/tab5-api-logic.yaml` and the incident in `docs/troubleshooting.md`)

## Context

ESPHome's `api: reboot_timeout:` reboots the device when **no API client has been connected** for that long; the counter restarts on every connection, even a brief one. The project shipped with the ESPHome default (15 min).

On 2026-07-31 → 08-01 the Home Assistant server took a long time to come up. The Tab5, Wi-Fi fine but with no API client, rebooted in a loop every ~15–20 min (timeout + boot + reconnection attempts, the counter restarting on each short-lived HA connection during HA's own startup). Nothing was broken on the device: each boot was clean, `safe_mode` said `Boot seems successful`. The safety net was simply doing what it was told.

An earlier audit item (F-04) had already ruled out `0s`: with no timeout at all, a frozen API stack leaves the device online but mute — a "zombie" that only a power cycle fixes, and that the push-only architecture (ADR-0001) cannot detect from the device side.

## Decision

`reboot_timeout: 60min`. An hour-long HA outage or maintenance window no longer cycles the tablet, which stays useful on its own in the meantime (clock, arcade, diagnostics console, last known weather); the anti-zombie net is kept, just with a longer fuse.

## Consequences

- A genuinely frozen API stack now costs up to one hour before the device recovers by itself, instead of fifteen minutes. Accepted: a frozen API has happened far less often than a slow HA start.
- Do **not** set `0s` without re-reading audit F-04, and do not go back to the default without re-reading the 08-01 incident — both directions have been tried.
- `wifi:` has its own, separate `reboot_timeout`; it is not involved in this decision and stays at its default (Wi-Fi was healthy throughout the incident).
- The HA-side health package (`HomeAssistant_Config/packages/tab5_health.yaml`, guard "API off for more than 2 min") is the fast detector; this timeout is the last resort.
