# ADR-0024: Packages without placeholders — every home value is picked in Home Assistant

**Status:** Accepted (2026-09-28) — supersedes the placeholder half of [ADR-0017](0017-ha-placeholders-rendered-copies.md); its leak guard (`--check`) stays.
**Date:** 2026-09-28

## Context

Since 3.0 the firmware installs from a browser, but the Home Assistant side still needed the repository and Python: copy `placeholders.example.yaml`, fill in about ten values (`VOTRE_VILLE`, `VOTRE_EMAIL_gmail_com`, `VOTRE_TV`…) plus the tablet's own entity IDs, render, copy `rendered/`. `packages/tab5_tv.yaml` also required a `tab5_tv_app_url` line in `secrets.yaml`: without it, Home Assistant rejected its **whole** configuration (seen by the « fresh HA » CI job). Calendars (`calendar.famille`, `calendar.anniversaires`, the public-holiday one) were written in the files. The weather source had already moved to a select in 4c-3.

## Decision

- **No placeholder, no `!secret`** in `packages/`, `custom_templates/`, `optionnel/`. The files install as they are; `render_ha_config.py` only copies, and `--check` now also fails on a leftover `VOTRE_…`.
- **A value of the home = a list in HA** (`packages/tab5_reglages.yaml`, `tab5_tv.yaml`, `optionnel/`): a trigger-based template select whose options are the matching entities plus « Aucun », the choice kept in an `input_text` (restored by HA). Trigger-based because iterating `states.binary_sensor` in a plain template re-renders on every sensor change. A default only when unambiguous (the only birthdays or holiday calendar, the only companion-app phone, the only Samsung TV, the Météo-France city); work calendar, appointments and presence are never guessed. « Aucun » = the feature stays off, without errors (every call targeting a choice is guarded: an empty target raises `NoEntitySpecifiedError`, which `continue_on_error` does not catch).
- **Detected, not chosen**: the tablet by its device model `tab5-ha-hmi` (`sensor.tab5_tablette`, entities by the end of their ID, which the firmware names); the providers' entities (`sensor.tab5_sources_meteo`: Météo-France sensors on the city's device, OpenWeatherMap weather, MeteoAlarm by its attribution).
- **Mirrors** for triggers (a `state:` trigger takes no computed entity ID): presence, phone, tablet link, boot time, appointment lead time, calendars.
- **TV address** is an `input_text` (or the `ip` of a router tracker with the TV's MAC); the REST command takes it as a variable.
- **The blind-shutter package moves to `optionnel/`**: while `script.tab5_volet_action` exists, the blueprint hands the shutter buttons to it, so shipping it by default would mute everyone else's shutter.
- **Releases attach `tab5_home_assistant.zip`** (`tools/publication/archive_ha.py`): `packages/`, `custom_templates/`, the blueprint and `tab5_optionnel/`, in `config/` layout. The « fresh HA » CI job installs exactly that list, then picks its sources with `select.select_option`.

## Consequences

- Install = unzip, one `packages:` line in `configuration.yaml`, restart, pick in the UI. Nothing in the repository describes one particular home any more.
- 24 more entities (26 with the optional shutter): lists, their `input_text` memories, the TV address, mirrors, the two detection sensors. Their entity IDs come from their names: `tests/test_installation_ha.py` checks that every `…tab5_…` entity a package reads is defined by one.
- Migration from ≤ 3.1: the lists start empty. « Tab5 · agenda de travail » must be set before the new automations run, or every day becomes a rest day for the alarm clock (reload input texts and templates, choose, then scripts and automations).
- The work calendar is chosen twice (blueprint slot `agenda_travail` for the planning zone, list for the hours) until the blueprint reads the list.
- `placeholders.yaml` survives only as the contributor's list of real values for `--check` (CI secret `HA_PLACEHOLDERS`).
