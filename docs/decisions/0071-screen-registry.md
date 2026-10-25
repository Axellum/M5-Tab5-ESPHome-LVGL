# ADR-0071: A screen registry in Home Assistant — what reads one screen's entities reads each screen's

**Status:** Accepted (2026-10-25).
**Date:** 2026-10-25

## Context

[ADR-0063](0063-several-screens.md) made Home Assistant push the same display to every connected screen. It left one thing behind, written in its consequences: everything that reads the entities of *a* tablet went through `sensor.tab5_tablette` / `tab5_api()`, which follow a single one (the last « HA API Status » found that is `on`).

On 2026-10-25 the author ran the Tab5 and the Waveshare 7B at the same time (the 7B with `_2` entities). `sensor.tab5_tablette` followed the Tab5. The 7B:

- never turned on or off with presence, and the « alarm ringing » guard looked at the other screen;
- never switched its theme by the sun, never turned off at night;
- kept « Ok Nabu » listening in an empty home;
- had its restarts and disconnections unreported — and when `sensor.tab5_tablette` moved from one screen to the other, the « Tab5 · startup » mirror changed, which could raise a false « unexpected reboot »;
- had its alarm briefing spoken on the other screen, in the other screen's language.

The full-push guard had the same flaw and was fixed alone by #543 (`tab5_api(appareil)`).

## Decision

- **One place finds a screen's entities.** `custom_templates/tab5_tablette.jinja` gets three macros:
    - `tab5_cibles(appareil)`: the device_ids to serve, comma-separated — the tablet `appareil` when it is one (even offline), otherwise every connected tablet. Same rule as `tab5_destinataires()`, which now maps `tab5_prefixe()` over it (same prefixes as before).
    - `tab5_entite(appareil, role)`: the entity_id of a role (`api`, `ecran`, `reveil_en_cours`, `raison`, `demarrage`, `micro`, `satellite`, `rdv_avant`, `langue`, `theme_nuit`, `nuit_ecran`, `pipeline`) of that device, French and English names ([ADR-0070](0070-home-assistant-reads-english.md)) and the `_2` suffix. The patterns live only here; `sensor.tab5_tablette` reads them too.
    - `tab5_nom(appareil)`: its name in Home Assistant, for notifications.
- **A registry.** `sensor.tab5_ecrans` (« Tab5 · screens », `packages/tab5_reglages.yaml`): state = number of connected screens; attribute `ecrans` = one line per `tab5-ha-hmi` device (device_id, name, prefix, connected, since, version); attribute `hors_ligne` = the device_ids offline. A plain template sensor iterating `states.binary_sensor`: Home Assistant re-renders it on any change of that domain (rate-limited to once a second), so a screen that is added appears without a reload.
- **Each consumer loops or targets.**
    - Presence (`tab5_push.yaml`): `repeat: for_each` over the connected screens, each with its own backlight and its own « alarm ringing » guard.
    - Day / night theme, night screen off: on `esphome.tab5_connected`, the screen that connected; otherwise every connected screen. Each waits for its own link and switch (10 s) inside the loop, an `if` instead of a condition so a screen not ready does not stop the next.
    - Microphone when away (`tab5_micro_absence.yaml`): every connected screen's switch; the switches it turned off are kept in `input_text.tab5_micros_coupes_absence`, so only those are turned back on (a manual choice is still kept, per screen). The re-sync trigger is `esphome.tab5_connected` instead of the single-screen mirror.
    - Health (`tab5_health.yaml`): an unexpected restart is seen when a screen connects, from that screen's « Tab5 Uptime » (booted less than 5 min before; a device created less than 10 min ago is a new screen, not a restart); a disconnection of 2 min comes from the registry's `hors_ligne` attribute, one alert per screen, none at Home Assistant start (the attribute had no previous value: a screen left offline in Home Assistant does not alert at every restart); « HA files out of date » turns on if any screen is newer. Notification ids carry the device_id: one screen's alert does not replace the other's.
    - Alarm briefing: `tab5_evenements.yaml` passes the device_id of the ringing screen to `script.tab5_reveil_annonce` (now `parallel`), which speaks on that screen's satellite, in its language.
    - « Appointments: announce before » mirror: every connected screen's value, joined by « · » (« 15 » with one screen, as before).
    - Consumption test: an optional « Screen » field.
- **Kept on purpose.** `sensor.tab5_tablette` (users' own automations, older HA files, the installation check), `binary_sensor.tab5_connectee` and `sensor.tab5_demarrage` (the long-term statistics, « Tab5 · continuous uptime », still follow one screen) stay. No package reads `sensor.tab5_tablette` any more except those mirrors and the options of the chat-pipeline list (`tests/test_plusieurs_ecrans.py`).

- **A connecting screen is waited for and served alone** (added the same day, after restarting the 7B at 14:04). The tablet fires `esphome.tab5_connected` about 0.15 s before Home Assistant turns its « HA API Status » `on`. The pushes waited for « a tablet is connected » — true at once thanks to the other screen — and sent to the connected screens, so the 7B's full push went to the Tab5. `tab5_pret(appareil)` waits for that screen's link; `tab5_appareil_connexion(trigger)` names it; every push started by `tab5_connected` (full push and its scripts, appointments, player, music, tracked sensors, cold storage, AI server) waits for it and sends to it only, and the blueprint waits for it too. Other triggers still serve every connected screen.

## Home settings and screen settings

Sorted with the author on 2026-10-25. Nothing moves in this decision: a « screen » setting that is a home-wide list today stays one, applied to every screen.

| Setting | Layer | Why |
|---|---|---|
| Calendars, work keyword, phone, tracked sensors, fridges / freezers, weather sources | Home | The same for the whole household |
| Chat pipeline | Home | The pipelines are the home's (author's choice) |
| Presence sensor, night screen off, music players | Screen ([ADR-0072](0072-screen-settings.md), same day) | Each screen is in its own room (music players: author's choice) |
| Rooms, tiles, zones, cameras, wheels | Screen | Already possible: one blueprint automation per screen (`tablette` input) |
| Theme, light / dark, brightness, language, direction, alarm, « announce before », volume | Device | Firmware entities, already per screen |

## Consequences

- With one screen nothing changes: the same entities, the same calls, the same notifications (their id gains the device_id).
- With N screens each one gets its own presence, theme, night, microphone, health alerts and briefing, from one set of packages.
- Deploy `custom_templates/tab5_tablette.jinja` and reload the custom templates before the packages, as for every macro of that file. No firmware change, no contract change.
- The registry does not know the board (Tab5 or 7B): Home Assistant sees the same model for both and the firmware does not publish it. A firmware « Board » entity would add it.
