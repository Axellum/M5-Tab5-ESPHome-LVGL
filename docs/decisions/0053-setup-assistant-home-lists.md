# ADR-0053: The setup assistant also proposes the home inputs of the blueprint and the « Tab5 · … » lists

**Status:** Proposed (2026-10-10, asked for by the author; lot 2 of 2 of the setup assistant. Not tried on a real Home Assistant with real devices when written: CI only). Extends [ADR-0052](0052-setup-assistant-rooms.md).
**Date:** 2026-10-10

## Context

[ADR-0052](0052-setup-assistant-rooms.md) gave the « Tab5 » integration an assistant that writes the rooms of the blueprint's automation. Two other forms remain for a new user: the inputs of the same blueprint that do not belong to a room (TV, phone battery, the temperature at the top of the home screen, a second temperature, climate, pots, forecast weather, the tablet's ESPHome name), and the « Tab5 · … » lists of `packages/tab5_reglages.yaml` (work, appointments, birthdays, public holidays and school holidays calendars, tracked phone, presence sensor, chat pipeline — [ADR-0024](0024-packages-without-placeholders.md)). The package already guesses four of these lists by itself when a single candidate exists (birthdays, public holidays, school holidays, phone: `auto_*` variables); it never guesses the other four (work, appointments, presence, pipeline). The author asked that the assistant pre-fill « only when there is no ambiguity » and, for the four lists the package never guesses, offer a plausible choice that the user confirms.

## Decision

- **Two more pages** between the last *Room* page and the *Summary* (`assistant_flux.py`): *Home* and *Calendars and people*. Nothing is written or set before the summary is submitted.
- **Home** (`assistant.CHAMPS_MAISON`, `proposer_maison`, pure): a field is pre-filled only when Home Assistant has **one** answer, among entities that are not the tablet's, disabled, hidden, of category config/diagnostic, or without a state:
  - `tv`: the only `media_player` of device class `tv`; `tv_telecommande`: the only `remote` of that TV's device;
  - `telephone`: the only battery sensor of the `mobile_app` integration;
  - `salon_temperature` / `salon_humidite`: those of room 1 (the first room is the one shown at start);
  - `serre_temperature`: the only temperature whose name or entity ID says « outdoor » (`exterieur`, `exterior`, `outdoor`, `outside`, `dehors`, `außen`, `buiten`, `esterno`/`esterna`, `dış`), then `serre_exterieure` ticked;
  - `clim`: the only `climate`; `pot_1`…`pot_5`: the soil moisture sensors when there are five or fewer, by name; `meteo_previsions`: the only `weather`;
  - `tablette`: the ESPHome name of the only tablet device (`device_name` of its ESPHome config entry, « - » → « _ »), omitted when it is the blueprint's default.
  When an automation of the blueprint already exists, its own values come first: the page shows what is there, the proposals only fill what is empty.
- **Calendars and people** (`assistant.LISTES`, `proposer_liste`, pure): one drop-down per « Tab5 · … » select that Home Assistant has (the page is skipped when the package is not loaded), with the select's own options (names shown, values unchanged). A choice already made stays. A list still on « Aucun »:
  - guessed by the package (birthdays, holidays, school holidays, phone): the same rules as the package, kept equal by `tests/test_assistant_tab5.py` (patterns compared with the YAML); since the select already shows the package's single candidate as its state, the assistant in practice changes nothing there;
  - never guessed by the package: work / appointments = the only calendar whose entity ID or name says so (« travail », « work », « Arbeit »… / « rendez-vous », « appointment », « Termin »…), not one the package files elsewhere, not one matching both; presence = the only `binary_sensor` of class `occupancy` or `presence` (not `motion`, too noisy to turn a screen off); pipeline = Home Assistant's preferred Assist pipeline, when the list offers it. Marked « (suggested) » in the summary;
  - never an entity of the Tab5 packages themselves (template entities whose `unique_id` starts with `tab5_`, a convention the test checks): `binary_sensor.tab5_presence`, class `occupancy`, mirrors the presence list itself and was proposed for it by the first CI run — a loop. The same rule applies to the *Room* and *Home* pages.
- **Applying** (`_appliquer`, `_regler_listes`): the automation is written as in ADR-0052, now with the home inputs (`fusionner`: on update, the rooms are all replaced, a home input only when the page gives a value; an emptied field keeps its value, an unticked « outdoors » box is written `false`); then each changed list through `select.select_option`, exactly like the user, and **checked** (the select's state is the chosen option); the notification lists what was set and what was not. The lists are set with or without the « update » box: that box is about the automation only, and the summary says which lists change.
- **Seven languages**, those of the screen (asked for by the author): `translations/{de,nl,es,it,tr}.json` join `fr` and `en` for the whole integration, and `messages.py` / `assistant.py` keep one table per language (`LANGUES`, `langue_de`: the server language, any other → English). Translated with Claude from the French, not reviewed by native speakers; the Home Assistant menu names they quote are not checked in each language. The tests keep the seven files and tables equal in keys, `{…}` parameters and backquoted code. The words that make the assistant propose an entity gain Spanish, Italian and Turkish (`exterior`, `esterna`, `dış`, `mesai`, `randevu`).
- `manifest.json`: `after_dependencies` gains `assist_pipeline` (preferred pipeline read through `async_get_pipeline`, in a `try`: no proposal when it fails).

## Rejected

- **Proposing the other inputs** (bottom rows, Ok Nabu panel, gestures, energy, the − / + tile, rain and weather alerts, legacy home screen): their answer is not in Home Assistant's registries (a taste, or a provider choice already made by `sensor.tab5_sources_meteo`); the user sets them in the automation.
- **The « music players » list** (several choices, [ADR-0050](0050-music-player.md)) and the work event keyword: a multi-select or free text, not a single entity to guess.
- **Writing the `input_text.tab5_choix_*` memories directly**: `select.select_option` runs the package's own action, so the assistant cannot drift from it if the memory format changes.
- **Picking the « best » candidate** when several match (the biggest calendar, the most recent phone): a wrong guess on a public install is worse than an empty field the user fills in once.
- **Translating the lists' « Aucun »**: it is the value Home Assistant stores and the package reads; only its label in the drop-down is translated.

## Consequences

- The author's Home Assistant is still not affected (it does not install the integration).
- A new home input of the blueprint to propose = one line in `CHAMPS_MAISON` and a rule in `proposer_maison` (the test compares natures and selectors with the blueprint); a new list = one line in `LISTES` (the test checks the select exists in the package and whether the package guesses it).
- Tested in CI (`tools/installation_ha/verifier_integration.py`, step 1 bis): an outdoor temperature without an area and a local calendar « Travail CI » added; the *Home* page proposes exactly room 1's sensors, the outdoor temperature (box ticked), the only climate and the only weather; the *Calendars* page proposes the calendar for the work list and leaves the rest on « Aucun »; after submitting, the automation has rooms and home inputs and `select.tab5_agenda_de_travail` reads the calendar; run again, the choice is kept; on update, the *Home* page shows the existing values and an unticked box is written `false`. Not tested: the forms as rendered by the frontend, a real home with phones, TVs or several calendars.
