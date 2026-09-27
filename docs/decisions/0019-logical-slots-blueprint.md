# ADR-0019: Logical slots — the device knows slots, a Home Assistant blueprint maps them to entities

**Status:** Accepted (2026-09-27, tried on the author's tablet)
**Date:** 2026-09-27 (lot 6a of the « ouverture » audit)

## Context

The firmware is customised at compile time: `Tab5/user_entities.yaml` feeds 37 `platform: homeassistant` subscriptions (lights, PC, TV, phone, temperatures, 5 plants × 5 values) and about 40 `homeassistant.action` calls that name entities (light toggle and popup, climate, TV remote and apps, PC/TV, LEDs, shutter). Changing a lamp means editing YAML and flashing, and no binary can be published: every user compiles their own. Lot 6 aims at one generic firmware, flashed from a browser, configured in Home Assistant with the mouse.

Checked in ESPHome 2026.9.0: a `homeassistant` sensor's `entity_id` is fixed at build time (`homeassistant/__init__.py`), and the C++ `subscribe_home_assistant_state()` only reaches HA when HA subscribes, so a runtime subscription (EspControl's approach) needs a reboot after every change and the « allow the device to perform Home Assistant actions » option. Device events (`esphome.*`) need no such option (HA `manager.py`); actions pushed by HA to the device have no count limit, accept `string[]`, 32 KiB per message.

## Decision

- **The device speaks in slots, never in entities.** Keys extend those of ADR-0018: `lumiere_1..3`, `pc`, `tv`, `telephone`, `salon` (+ `salon_hum`), `serre`, `pot_1..5` (+ `_ec`, `_lux`, `_temp`, `_bat`), `clim`, `volet`, `planning`.
- **HA pushes slot states** with one action, `tab5_maj_emplacements(payload)`, `key|state|value;…` (ADR-0003): all slots on (re)connection, one slot on each change. `state` is the HA state as is (`on`, `off`, `home`, `unavailable`…); `value` is the number the screen shows (brightness 0-255, %, °C, µS/cm, lx), `nan` when there is none. Climate and shutter keep their dedicated actions (`tab5_maj_clim`, `tab5_maj_volet_etat`): they carry several fields and already exist.
- **The device sends every command as an event**, `esphome.tab5_action` with `emplacement`, `action`, `valeur` (e.g. `lumiere_1 / basculer`, `lumiere_1 / luminosite / 128`, `clim / consigne / 21.5`, `tv / touche / KEY_UP`, `volet / ouvrir`). HA dispatches it to the mapped entity. The screen no longer holds any entity ID for these zones.
- **A blueprint maps slots to entities**: one automation per tablet, one entity selector per slot (filtered by domain), grouped in collapsible sections, all optional. It pushes states, dispatches commands, and answers `esphome.tab5_zones` (ADR-0018): an empty input is an absent zone. Changing an entity is an edit in HA's UI: no flash, no reboot.
- **Kept as they are in 6a**: the calls that don't name a home entity (voice satellite and media player of the tablet itself, calendar and alarm scripts, dismissed alerts, system console). They still need the actions option; converting them is a later step.

## Consequences

- `user_entities.yaml` loses its `entity_*` keys for these zones; the pushes of `tab5_push.yaml` stay (weather, calendar, alerts). Breaking change: released as 3.0.0 with 6b (firmware without secrets) and 6c (web flasher).
- Latency: a state change goes through an HA automation instead of a direct subscription (one more hop, same as climate and shutter today).
- A shutter that doesn't report its travel (the author's) keeps `volet_serre_tracking.yaml`, which simulates it; a cover reporting `opening`/`closing` is pushed by the blueprint directly.
- The blueprint is the contract's second half: its keys are tested against the firmware's, as ADR-0018's are (`tests/test_zones.py`).
- The blueprint needs the 3.0 firmware: with an older one, `esphome.tab5_ha_hmi_tab5_maj_emplacements` doesn't exist, and `continue_on_error` does not catch a missing action (HA logs an error at each change).
- Weather, calendars and presence still come from packages with placeholders. Folding them into the blueprint (true no-YAML install) is a decision for later in lot 6.
