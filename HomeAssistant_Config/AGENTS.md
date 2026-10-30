# AGENTS.md — `HomeAssistant_Config/` (the Home Assistant side)

Read with the root [`AGENTS.md`](../AGENTS.md). Claude Code loads this file (through the `CLAUDE.md` next to it) only once it reads or edits a file of this folder. The packages themselves are described in [`HomeAssistant_Config/README.md`](README.md).

## One source: the tracked packages

The HA side has **one source**: the tracked packages. Since 2026-09-26 (lot 3 of the « ouverture » audit) Axel's Home Assistant runs `packages/tab5_push.yaml` (push automations, `tab5_push_*` scripts, device-called scripts, rain template), the other packages and `optionnel/volet_serre_tracking.yaml` — as they are since 2026-09-28. These automations and scripts are read-only in the HA UI: change the package, deploy (Samba share of HA's `config/packages/`, `.bak` first, `check_config`, reload). Never edit them with `ha_config_set_automation` / `ha_config_set_script`. The former private copies (`automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`) and the three `*_examples*` files are gone; they used to drift (on 2026-09-26 the public shutter package had never run anywhere).

## No placeholder, no real value

Public HA files (`HomeAssistant_Config/packages/`, `optionnel/`, `snippets/`, `custom_templates/`) contain **no placeholder and no real value** ([ADR-0024](../docs/decisions/0024-packages-without-placeholders.md), 2026-09-28): every home value is a « Tab5 · … » list picked in HA (`packages/tab5_reglages.yaml`), the tablet is detected by its device model (`sensor.tab5_tablette`), provider entities by `sensor.tab5_sources_meteo`. Never write an entity ID of a real home, a `VOTRE_…` placeholder or a `!secret` back. The gitignored `HomeAssistant_Config/placeholders.yaml` (template: `placeholders.example.yaml`, CI secret `HA_PLACEHOLDERS`) lists the author's real values: `python tools/render_ha_config.py --check` fails if one of them, or a placeholder, lands in a public file (it never prints the value — CI-safe). `render_ha_config.py` without `--check` only copies to `rendered/`. An entity read by a package must be defined by one (`tests/test_installation_ha.py`: template entity IDs come from their names). Releases attach `tab5_home_assistant.zip` (`tools/publication/archive_ha.py`); the blind-shutter package stays in `optionnel/` (the shutter picked in its « Tab5 · shutter with simulated travel » list gets its script instead of the blueprint's commands; on « Aucun », nothing changes).

## Events from the tablet

The firmware never calls a Home Assistant action (code rule 8, [`Tab5/AGENTS.md`](../Tab5/AGENTS.md)): it emits `esphome.tab5_*` events, and `packages/tab5_evenements.yaml` maps them to a whitelist of actions on the tablet's own entities. A new request = an event + a branch there. A change to an `api: services:` variable is a contract change: see « The API service contract » in [`Tab5/AGENTS.md`](../Tab5/AGENTS.md).
