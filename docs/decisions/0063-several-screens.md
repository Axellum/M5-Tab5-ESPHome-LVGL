# ADR-0063: Several screens in one home — a name per screen, the same display pushed to each

**Status:** Accepted (2026-10-16; not yet run with two screens at once when written).
**Date:** 2026-10-16

## Context

Axel will run several screens at the same time (the Tab5 and the Waveshare 7B of [ADR-0061](0061-board-and-screen-size.md), maybe more), all showing the same thing. Two things stood in the way:

- **One device name for everyone.** The firmware set `name: tab5-ha-hmi`. Two ESPHome devices with the same name share an mDNS host name (`tab5-ha-hmi.local`), the same build folder and, in Home Assistant, the same action names: an ESPHome device's API services become `esphome.<device name with _>_<service>`.
- **Home Assistant pushed to one action name.** The packages called `esphome.tab5_ha_hmi_tab5_maj_…` written out; only the blueprint took the prefix from an input (`tablette`, `tab5_ha_hmi` by default). A second screen would have received nothing.

Home Assistant does not expose the ESPHome name of a device to templates: only its displayed name, its MAC addresses and the title of its config entry (checked on the author's Home Assistant). The prefix of the actions cannot be derived reliably from what Home Assistant knows.

## Decision

- **A name per screen, at compile time.** `tab5-ha-hmi.yaml` reads `name: ${ tab5_nom | default('tab5-ha-hmi') }` and `friendly_name: ${ tab5_nom_affiche | default('M5Stack Tab5 Home Assistant HMI') }` (`Tab5/user_entities.yaml`, or `-s tab5_nom salon-7b` on the command line). The default keeps the existing tablet as it is: same mDNS name, same entities, same actions. The device **model** (`name` of the `project:` block, `tab5-ha-hmi`) does not change: it is what Home Assistant recognises a tablet by.
- **The firmware says its own prefix.** A diagnostic text sensor, « Tab5 Préfixe des actions » (`Tab5/paquets/tab5-sensors-diagnostics.yaml`), publishes the name with `-` replaced by `_`, computed by Jinja at compile time (published shortly after boot, then every 24 h).
- **Home Assistant sends to each connected screen.** Two macros in `custom_templates/tab5_tablette.jinja`:
    - `tab5_prefixe(device_id)`: the state of that device's prefix sensor; `tab5_ha_hmi` for a firmware without it (older than this lot, so necessarily named `tab5-ha-hmi`) and while the sensor has no readable value: right after a boot the tablet connects, and asks for its full push, before it publishes its prefix, so the sensor is `unknown` or `unavailable` (on 2026-10-16 the push went to `esphome.unknown_…` and was lost); the default name is the right one for every screen that keeps it, and a renamed screen gets the next push;
    - `tab5_destinataires(appareil)`: the prefixes to send to, comma-separated (plain Jinja, no Home Assistant-only filter): only the tablet `appareil` when it is the `device_id` of a tablet (the event that asked for an answer: a calendar month, a day), otherwise every tablet whose « HA API Status » is `on`, without duplicates.
- **Each push is a loop.** Every `esphome.…_tab5_maj_…` call of the packages (`tab5_push`, `tab5_calendar`, `tab5_froid`, `tab5_lecteur`, `tab5_reveil`, `tab5_serveur_ia`, `tab5_suivi`) is wrapped in `repeat: for_each:` over `tab5_destinataires(...)`, with `tablette: "{{ repeat.item }}"` and `action: "esphome.{{ tablette }}_tab5_maj_…"`, `continue_on_error: true` kept: a screen that drops out does not stop the others. The two calendar scripts get an optional `appareil` field that `tab5_evenements.yaml` fills from the event's `device_id`, so the month a screen browses goes to that screen only.
- **The blueprint keeps its input.** `tablette` left at `tab5_ha_hmi` (its default) now means « every connected tablet » (the tablet that sent the event when the trigger is a `esphome.tab5_*` event); another value means that tablet alone, as before. Its 14 sends loop over `tablettes`.
- **The action names stay checkable.** `tools/contrat_api.py` already reads `esphome.{{ tablette }}_tab5_…`; `tests/commun.py` gets `envoi_tablettes()` so the tests find the action inside the loop.

## Consequences

- One firmware source, one set of packages and one blueprint drive N screens with the same display. A new screen = a new `tab5_nom`, compile, add it to Home Assistant: it receives the next pushes, and the full push on its connection like the first one.
- An existing installation changes nothing: no rename, the default prefix, and an older firmware is still reached (`tab5_prefixe` falls back to `tab5_ha_hmi`). The packages must be deployed with `custom_templates/tab5_tablette.jinja` reloaded first, as for every macro of that file.
- A push costs one call per connected screen; with one screen, the same single call.
- **Still one tablet:** what reads a tablet's own entities through `sensor.tab5_tablette` / `tab5_api()` follows a single one (the last connected): the screen on / off and alarm-ringing guards of the full push, the night switch of the theme, the Health view and its disconnection alerts, the restart reason, the version check, the wake-word / microphone package, the Assist pipeline of « Discu » and the consumption test. Each screen keeps its own settings (language, theme, alarms, volume) on the device: they are not mirrored. Making those per screen is a later lot if two screens need different behaviour. **Replaced on 2026-10-25 by [ADR-0071](0071-screen-registry.md):** a screen registry, and each of these reads every screen (the full-push guard first, by #543).
- Renaming a tablet already installed (from `tab5-ha-hmi` to another name) changes its mDNS name and its build folder: Home Assistant usually follows by its MAC, the command-line OTA must use the new name or the IP. Not recommended; give the new name to the new screen.
