# ADR-0052: A setup assistant in the « Tab5 » integration writes the blueprint's automation from the Home Assistant areas

**Status:** Proposed (2026-10-10, asked for by the author; lot 1 of 2: rooms, summary, automation. Not tried on a real Home Assistant with real areas when written: CI only). Extends [ADR-0035](0035-hacs-integration-ha-files.md).
**Date:** 2026-10-10

## Context

Since [ADR-0035](0035-hacs-integration-ha-files.md) the HACS integration puts the Home Assistant files in place, but a new user still meets an empty screen: the devices are picked by hand in the automation of the blueprint « Tab5 — emplacements de l'écran » ([ADR-0019](0019-logical-slots-blueprint.md), rooms since [ADR-0023](0023-rooms-generic-tiles.md), room climate since [ADR-0040](0040-room-climate.md)), a long form. Home Assistant already knows most of the answer: its areas, the lights, covers and sensors in each. The author asked for « an assistant, pre-filled from what HA already contains; the user only clicks *Next*, everything stays editable ».

What was checked before choosing (Home Assistant 2026.8.3 code, read on 2026-10-10): `automations_with_blueprint(hass, path)` lists the automations of a blueprint; the automation editor always writes `automations.yaml` (`AUTOMATION_CONFIG_PATH`), with `yaml.dump` of the whole list (comments lost); a repair's fix flow is an ordinary data-entry flow (several steps, selectors) and finishing it removes the repair; area and entity selectors take `multiple`, `reorder` and a domain filter.

## Decision

- **Two doors, one flow** (`custom_components/tab5/assistant_flux.py`, a mixin):
  - **First installation**: once the files are active (the « version des fichiers HA » sensor reads the version, so the blueprint is loaded) and no automation of a `tab5_emplacements.yaml` blueprint exists, the integration raises the fixable repair **« configurer_pieces »** (« Configurer la tablette depuis vos pièces »); its fix flow *is* the assistant. Any automation of the blueprint, however made, removes it at the next start; the user can ignore it.
  - **Again later**: the options of the integration get a box « Lancer l'assistant de configuration ».
- **Steps**: *Rooms* (up to 5 areas, pre-picked), one *Room* page per picked area (name, tiles, temperature, humidity, climate — pre-filled), *Summary* (names, not entity IDs, and what will be written where). Nothing is written before the summary is submitted.
- **What is proposed** (`custom_components/tab5/assistant.py`, pure, tested by `tests/test_assistant_tab5.py`): an entity's area is its own, else its device's; never an entity of the tablet itself (device model `tab5-ha-hmi`), disabled, hidden, or of category config/diagnostic, nor one without a state. Areas are ranked by their controllable entities (light, cover, switch, fan, media_player, climate), then by name; an area with nothing to show is not offered (the empty areas of the onboarding). Tiles: up to 5, lights first, then covers, switches, fans, media players, by name in a domain; the form accepts every domain of the blueprint's tile selector (kept equal by the test). Temperature / humidity: the first `sensor` of that device class by name; climate: the first `climate`. The room's name: the area's. No area with anything to show: the assistant says so and stops — nothing invented.
- **Writing** (`assistant.situation`, then `ajouter` / `remplacer_pieces`, `ecrire_si_inchange`):
  - only into `automations.yaml`, and only if `configuration.yaml` includes it (`automation: !include automations.yaml`, the default of a new install) and `yaml.safe_load` reads it as a list (no `!secret`, `!include`…); otherwise **nothing is written** and a notification gives the YAML to paste;
  - **no automation of the blueprint**: one is appended (`use_blueprint: tab5/tab5_emplacements.yaml`, an id like the editor's, the text before it kept byte for byte when the list is in block style);
  - **one already there, in that file**: it changes only if the box « Remplacer les pièces de l'automatisation existante » (unticked) is ticked; then only its `piece_n_*` inputs are replaced, every other input stays; the file is rewritten like the editor does;
  - **several, or one loaded from elsewhere** (a package, another include): nothing is written;
  - before writing: the file read again (changed since the summary: nothing written), copied to `tab5_sauvegardes/automatisations/<date>_automations.yaml` (5 kept; the install backups ignore that folder), then written atomically; `automation.reload`; **the effect is checked** (the automation's entity, by its id, loaded and not `unavailable`) and a notification says it.
- **Languages**: French and English, like the rest of the integration (its `translations/` and `messages.py` have only these two).

## Rejected

- **Pre-filling from the existing automation** when one exists: the summary would compare nothing new, and the author's automation must not change without an explicit action; the box is the explicit action.
- **Writing through the automation editor's HTTP view** (`/api/config/automation/config/<id>`): an internal view of the `config` integration, not an API for integrations; and it would also write when `configuration.yaml` does not include `automations.yaml`.
- **A step of the « add integration » form**: at that moment the files (and the blueprint) are not installed yet, so the automation would not load; the repair comes once they are.
- **Guessing every input of the blueprint** in this lot: the home-wide inputs and the « Tab5 · … » lists are lot 2.

## Consequences

- The author's Home Assistant is not affected: it does not install the integration (Samba deployment), and its automation of the blueprint exists, so neither the repair nor a write would happen.
- `manifest.json` declares `after_dependencies: [automation]` (the integration imports `automations_with_blueprint`).
- A new tile domain in the blueprint must be added to `assistant.DOMAINES_TUILES` (the test compares).
- Tested in CI (`tools/installation_ha/verifier_integration.py`, step 1 bis, fresh Home Assistant 2026.9.4 and 2026.8.3): two areas with template entities, a generic thermostat and a hidden light; the repair followed like the interface, pre-filled values accepted → the automation written, loaded, the repair gone; run again from the options without the box → `automations.yaml` unchanged byte for byte; with the box and a new name → only the rooms changed, backup made. Not tested: the forms as rendered by the frontend, a real home.
