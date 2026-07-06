# ADR-0001: Push-only architecture, zero polling from the device

**Status:** Accepted

## Context

The device needs to display near-real-time Home Assistant state (weather, climate, calendar, plant moisture, light/switch state) on screen. The obvious approach is for the device to poll HA's API on an interval and refresh widgets. An ESP32-P4 has plenty of headroom to do this.

## Decision

The device never requests its own state. All data flow is one-directional: Home Assistant automations detect state changes and push them to the device via native ESPHome API `services:` calls (`tab5_maj_*`). The firmware is purely reactive.

## Consequences

- CPU/network usage stays near zero when nothing changes — no wasted polling cycles, no unnecessary Wi-Fi radio wake-ups.
- The device has no way to "catch up" on missed state on its own; if an automation fails to fire (see [`docs/troubleshooting.md`](../troubleshooting.md) — the `is_primary_active` incident), the screen silently goes stale with no error surfaced. This tradeoff was accepted; the mitigation is `continue_on_error: true` + guard automations on the HA side, not a pull fallback on the device side.
- Adding a new piece of displayed state always means adding both an HA-side push automation *and* a device-side `api: services:` handler — there is no single place to "just read a sensor."
