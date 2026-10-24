# ADR-0070: Home Assistant reads the tablet in English: menu options and text states, whatever the screen language

**Status:** Proposed (2026-10-24, the author's decision; built and tested off-device, not flashed on a tablet when written).
**Date:** 2026-10-24

## Context

Users who do not read French want to see the tablet in English in Home Assistant: husyildiz (Turkish, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) and michapixel (Home Assistant forum), on 2026-10-24. Until then the rule was « never translate what Home Assistant reads » (AGENTS.md, [`docs/translations.md`](../translations.md), [ADR-0029](0029-themes-palette.md) for the themes): entity names, select options and text states stayed in French in every screen language, because renaming them would break the history and the automations.

Two different things hide behind that rule:

- **The entity `name:`.** In ESPHome the name gives the entity's key, its unique ID in Home Assistant (`MAC/0/type/Name`, aioesphomeapi) and the slot where the tablet keeps its value (`fnv1_hash_object_id(name)`). Renaming it would lose the tablet's settings and make Home Assistant drop the old registry entries. It never changes; the translation of names belongs to Home Assistant's registry (the Tab5 integration, a separate change).
- **The text of a select option and of a text state.** A template select restores its choice by **index** (ESPHome `template_select.cpp`, `setup_with_restore`: `pref.load(&index)`), not by text. Changing the text of an option without changing the order keeps the stored choice.

## Decision

- **Every select option of the firmware is English**, in every screen language, in the same order: « Sens de l'écran » (Normal, Upside down), « Réveil : mode » (Fixed time, Work days, Before shift start), « Réveil : sonnerie » (Gentle, Classic, Insistent, Chime), « Réveil : jours » (Every day, Monday-Friday, Monday-Saturday, Weekend, Custom), « Tab5 Économie d'énergie » and « Tab5 Wi-Fi éco » (Never, On battery, Always), « Tab5 Animations » (Full, Essential, None), « Tab5 Extinction auto de l'écran » (Never, 1 min…), « Tab5 Mode de charge » (Classic, Fast), « Clair ou sombre » (Dark, Light, Auto), « Thème » (the `nom_ha` of each `Tab5/themes/*.yaml`: Slate, Soft relief…), « Aller à l'écran » (—, Home, Voice assistant, Calendar, Alarm, Air conditioning, Plants, TV remote, System console, Energy, Settings, Alerts, House, Lights, Shutter, Temperature, Weather, Music, Cameras, Tracking, Fridges, AI server). The English words are those of `Tab5/lang/en.yaml` when the screen already had them, so the screen in English and Home Assistant say the same thing (`tests/test_reglages.py` checks it for the settings' buttons). « Langue » keeps each language's native name; « 100 % / 80 % », « LINE1… » and « Internal / External » were already language-free.
- **The firmware reads and publishes a select by its index only.** The alarm days select is published with `publish_state(index)`; the French preset and melody names of `alarm_clock.cpp` are gone. The screen keeps its own French labels (`label_text` of the popups' buttons, translated by `tr()`); a theme keeps its screen name (`nom`) next to its HA name (`nom_ha`), the screen still shows `nom`.
- **Text states Home Assistant reads are English**: « Écran courant » (Home, the registry labels — the same as the « Aller à l'écran » options —, Arcade, Game · <console>; a console name is a proper name), « Zones masquées » (none, or the zone keys), « Tab5 C6 Version » (Unknown — not `unknown`, Home Assistant's own « no value » state). « Prochain réveil » was translated into the screen language by `tr()`: it is now computed under the `I18nPourHa` guard (`Tab5/socle/tab5_i18n.h`), which turns `tr()` to English for the time of the computation, so Home Assistant reads « Tomorrow 07:00 » while the alarm popup keeps the screen language. « Prochain rendez-vous » carries only a time and the event's own title: nothing to change.
- **Home Assistant files accept the old and the new text** while a firmware of either side may answer: `packages/tab5_push.yaml` (popup Alerts open: `is_state` with `['Alerts', 'Alertes']`, checked against the author's Home Assistant with a read-only template on 2026-10-24), `packages/tab5_energie.yaml` (`states(ecran) not in ['Energy', 'Énergie']`), `packages/tab5_mesure_conso.yaml` (an option chosen by its index in the select's own `options`). The off-device tools follow the firmware of their commit (`tools/rendu/`, `tools/installation_ha/`); the measuring tools run against any tablet and pick options by index (`tools/mesure_conso.py`, `tools/campagne_mesures.py`).
- `tests/test_ha_anglais.py` keeps it: no select option with a French letter (except « Langue »), no `current_option()` in the firmware, the guard around « Prochain réveil », and both texts accepted on the HA side.

## Rejected

- **Renaming the entities in the firmware** (`name:` in English): the tablet would lose every setting kept in NVS and Home Assistant would make new entities, the history orphaned.
- **Options in the screen language** (one set per language): the options are written once at compile time and read by automations; a change of the screen language would break them.
- **Reordering or trimming an option list** to tidy it: the tablet keeps the index, a moved option is a changed choice.

## Consequences

- After the update a choice made before stays (same index), and Home Assistant shows the English text. An automation of the user that writes or tests an old French text (`select.select_option` with « Accueil », a condition on « Écran courant » = « Énergie »…) is to be updated; one that uses the index, or the Tab5 packages, keeps working with both firmwares.
- Update order: **the Home Assistant files first** (they accept both texts), then the firmware. The other way round, the Home Assistant files of before would miss three things with the new firmware: the Alerts history is no longer pushed again while the popup is open (« Écran courant » = « Alerts », not « Alertes »), the Energy popup stops being refreshed after its 12 s grace (« Energy », not « Énergie »), and the consumption test stops on its first « Jamais ».
- API contract (`contrat/contrat.yaml`) unchanged: no action, variable or event changes.
- The theme name differs between the screen (Relief doux) and Home Assistant (Soft relief) until the screen translates the theme names too (not done: 21 names in six languages, and ADR-0029 calls them proper names).
- Supersedes the « options never translated » parts of [ADR-0029](0029-themes-palette.md), [ADR-0045](0045-charging-animations-eco-wifi.md) and [ADR-0069](0069-screen-direction-from-home-assistant.md) (option names only; their decisions stand).
- Not checked on a tablet when written: the restored choices after the update, the English states in Home Assistant, the dashboard.
