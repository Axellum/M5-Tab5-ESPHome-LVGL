# ADR-0072: Settings of one screen — presence sensor, night screen off and music players per screen

**Status:** Accepted (2026-10-25).
**Date:** 2026-10-25

## Context

[ADR-0071](0071-screen-registry.md) made every package read each screen's own entities, and sorted the settings into home, screen and device layers with the author. Three settings were left as home-wide lists although each screen sits in its own room: the presence sensor that turns the screen on and off, the night screen-off window, and the music players of the Music popup (author's choice, 2026-10-25). With the Tab5 in one room and the Waveshare 7B in another, one presence sensor turned both on, and the 7B's Music popup listed the players of the other room.

Three ways were weighed:

- **Inputs of the blueprint**: the « Tab5 — emplacements » automation is home-wide (one per house, 6 000 lines of rooms); one automation per screen would copy every room.
- **Firmware entities**: a `text` per setting, typed by hand (no entity picker), a flash of every screen, and the firmware would name Home Assistant entities ([ADR-0025](0025-events-only.md) keeps it from doing so).
- **Lists in Home Assistant, per screen** (chosen): set with the mouse like the other « Tab5 · » lists, no firmware change.

## Decision

- **One screen at a time.** « Tab5 · screen to set » (`select.tab5_ecran_a_regler`) picks a screen (options « name · action prefix », unique per screen, [ADR-0063](0063-several-screens.md)); « Tab5 · this screen · presence sensor », « … · night screen off » and « … · music players » show and change that screen's value. Each offers « Same as home » first (the default): the screen follows the home list, so with one screen, or before anything is set, nothing changes. The dashboard ([ADR-0071](0071-screen-registry.md), one set of views per screen) shows them in each screen's Settings view, with a « Set this screen » button that picks the screen of that view.
- **Stored by a trigger-based template sensor.** `sensor.tab5_reglages_ecrans` (« Tab5 · screen settings ») keeps `{device_id: {presence | nuit | lecteurs: value}}` in its `reglages` attribute, updated by the `tab5_reglage_ecran` event (data `appareil`, `cle`, `valeur`; an empty value removes the key) that the lists fire. Home Assistant restores the state *and the attributes* of trigger-based template entities after a restart (Template integration documentation, read on 2026-10-25), so no `input_text` of 255 characters per screen is needed (six players' entity_ids do not fit in one for two screens). A device that is no longer a tablet is dropped at the next change; an unknown key or a foreign device changes nothing.
- **Read by one macro.** `tab5_reglage(appareil, cle)` (`custom_templates/tab5_tablette.jinja`) returns the screen's value or the home's; `tab5_presences(etat)`, `tab5_nuits(h)` and `tab5_lecteur_montre(appareil)` build on it.
- **Consumers.**
    - Presence (`tab5_push.yaml`): `sensor.tab5_presences_ecrans` lists the screens whose sensor says someone (`presents`) or no one (`absents`). A screen entering `presents` turns on; a screen entering `absents` starts `script.tab5_ecran_absence` (parallel, one run per screen), which turns it off after 15 min unless it leaves `absents` first (someone back, or sensor unavailable) — the 15 min of the former `for:` trigger, per screen. The phone stays the home's and serves every connected screen.
    - Night screen off: `sensor.tab5_nuits_ecrans` lists the screens inside their window, re-rendered every minute; « Tab5 — night screen off » fires on its change and turns each screen's switch on or off from that list.
    - Music players (`tab5_lecteur.yaml`): each screen has its list and its shown player — the chosen player if it is in that screen's list or in none (a tile's player), else the first one playing in its list, else its first. The push serves each screen with its own; a command from a screen (`esphome.tab5_lecteur`, now passed its `device_id`) indexes that screen's list. The « Tab5 · shown player » mirror and the music of the home ([ADR-0067](0067-hifi-music-assistant.md), Music Assistant rooms) keep the home list; a favourite playlist looks for its Apple TV in every list.
- **Kept.** The home lists, their memories and mirrors (`binary_sensor.tab5_presence`, `binary_sensor.tab5_nuit_de_l_ecran`) stay: they are the « Same as home » values and other automations may read them. The chat pipeline, calendars, phone, tracked sensors and fridges stay home-wide (ADR-0071's table).

## Consequences

- One screen, or several screens with nothing set: the same behaviour as before, apart from the presence trigger that now waits 15 min in a script instead of a `for:` (lost at a Home Assistant restart in both cases).
- Deploy `custom_templates/tab5_tablette.jinja` (and `tab5_dashboard.jinja`) and reload the custom templates before the packages. No firmware change, no contract change.
- A player opened from a tile on one screen is shown on another screen only if it is in that screen's list or in none.
