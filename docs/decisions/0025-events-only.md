# ADR-0025: Events only — the device never calls a Home Assistant action

**Status:** Accepted (2026-09-28; not yet tried on the author's tablet)
**Date:** 2026-09-28

## Context

After lot 6a ([ADR-0019](0019-logical-slots-blueprint.md)) the firmware still made 13 `homeassistant.service` calls: spoken briefing and appointment announcements, calendar month and day requests, dismissed alerts, voice interruption, pipeline choice, and the three system-console buttons (« MAJ Écran », « Recharger autos », « Redémarrer HA »). Each one needs the ESPHome option « Allow the device to perform Home Assistant actions »: one more installation step, easy to miss (without it HA only logs a rejection and raises a repair issue), and an option that lets the device call **any** action of Home Assistant. Five of the calls also named the tablet's own entities (satellite, media player, pipeline select) or the push automation through `entity_*` substitutions, wrong as soon as the device or the automation is renamed.

Device events need no such option: HA's ESPHome `manager.py` fires any `esphome.*` event with the device's `device_id` added (checked on the `dev` branch, 2026-09-28).

## Decision

- **The firmware emits events only.** Each former call becomes a `homeassistant.event` `esphome.tab5_<name>` with the data it needs: `tab5_reveil_annonce`, `tab5_annonce` (message), `tab5_calendrier_mois` (annee, mois), `tab5_calendrier_jour` (date), `tab5_alerte_lue` (alert_id), `tab5_voix_stop`, `tab5_mode_assistant` (option), `tab5_maj_ecran`, `tab5_recharger_automatisations`, `tab5_redemarrage_ha_confirme`. The `entity_*` substitutions are gone.
- **One HA package translates them**, `packages/tab5_evenements.yaml`: one automation, `mode: parallel`, one branch per event, each calling a **fixed** action from a whitelist (`script.turn_on` of the project's scripts, `assist_satellite.announce`, `media_player.media_stop`, `select.select_option`, `input_boolean.turn_on`, `automation.trigger`, `automation.reload`, `homeassistant.restart`). No action or entity name is ever taken from the event.
- **Only a Tab5 is heard**: the event's `device_id` must be a device of model `tab5-ha-hmi` (the firmware's `project:` block), as the blueprint and the pushes already find it. The tablet's entities are found with `device_entities()` of that device; the push automation by its `id`.
- **The confirmation screen stays on the tablet.** `homeassistant.restart` is reachable only from `tab5_redemarrage_ha_confirme`, emitted only by the « Confirmer » button of the confirmation overlay.
- `tests/test_actions_ha.py` checks it: no `homeassistant.service`/`action` in the firmware, every emitted event has a consumer and every consumed `esphome.tab5_*` event is emitted (the `tab5_alarm_*` events are left to the user's automations), the whitelist, the model guard, restart on confirmation only. The « HA neuf » CI job installs without the option and drives two requests end to end (calendar, « MAJ Écran »).

## Consequences

- One installation step less; the option can be unticked, which closes « any action » to the device.
- **Order of update**: package first (idle with a 3.1 tablet, which emits none of these events), then firmware, then untick the option. A new firmware without the package loses its requests silently (no crash, no log): calendar grid only, no announcements, console buttons inert — documented in [`docs/installation/updates.md`](../installation/updates.md#upgrading-from-31).
- Pipeline: the option is selected only if the tablet's pipeline select offers it (no more error at each boot for a missing « Discussion LLM »). Since HA 2025.10 there are two pipeline selects; the first by entity id is the one the firmware used to target.
- **Known limit**: `manager.py` merges the device's data **after** `device_id`, so another ESPHome device already added to HA could send its own `device_id` field and pass for the tablet. The firmware never sends that field. This is still far narrower than the option it replaces.
- Some scripts called by the package still name the satellite themselves (`tab5_reveil_annonce` in `tab5_reveil.yaml`); passing the tablet's satellite to them is a possible follow-up.
