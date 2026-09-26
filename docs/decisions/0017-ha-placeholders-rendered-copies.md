# ADR-0017: Public HA files hold placeholders only — real IDs live in `placeholders.yaml`, HA runs `rendered/`

**Status:** Accepted (2026-09-06)
**Date:** 2026-09-25 (written retroactively from `tools/render_ha_config.py`, `AGENTS.md` and the 2026-09-06 CHANGELOG entry)

## Context

The repository is public, and `HomeAssistant_Config/` publishes working HA packages, snippets, Jinja templates and example automations. On 2026-09-06 an audit found real identifiers in those tracked files: the Gmail handle of the work calendar, the town of the weather entity, the TV, a lamp, and an engine IP in a comment. Checked over Samba, the packages running on the HA server were **byte-for-byte** the tracked files. Anonymising the repository by hand would therefore have changed what HA loads, or left two diverging copies.

## Decision

A one-way pipeline: **public → rendered**.

- Tracked files contain placeholders only (`VOTRE_VILLE`, `calendar.VOTRE_EMAIL_gmail_com`, `media_player.VOTRE_TV`…). They are the files reviewed in PRs.
- Real values live in `HomeAssistant_Config/placeholders.yaml`, which is gitignored (template: `placeholders.example.yaml`). It is a flat `placeholder: value` list, parsed without a YAML dependency because keys contain dots.
- `python tools/render_ha_config.py` writes the deployable copies to `HomeAssistant_Config/rendered/` (also gitignored), replacing the longest placeholders first. **HA is deployed from `rendered/`, never from the tracked files.**
- `--check` fails if any real value (6+ characters) appears in a public file. It prints the placeholder name, file and line, **never the value**, so it is safe in public CI logs. It runs in pre-commit and in the CI `python` job, where `placeholders.yaml` comes from the `HA_PLACEHOLDERS` repository secret (without it the step emits a warning instead of passing silently).

## Consequences

- Never edit `rendered/`: the next render overwrites it. Edit the tracked file, render, deploy.
- A new real identifier needs a new placeholder in the tracked file, a line in `placeholders.yaml`, a line in `placeholders.example.yaml`, and an update of the `HA_PLACEHOLDERS` secret. Otherwise the CI cannot catch its leak.
- ~~Production-only HA files (`automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`) stay gitignored and outside this pipeline. They are mirrored by hand into the `*_examples*` files, which drifts easily (see `AGENTS.md`).~~ **Superseded on 2026-09-26** (lot 3 of the « ouverture » audit): those files and the three `*_examples*` files were replaced by `packages/tab5_push.yaml`, and the pipeline now covers *all* of the Tab5's HA side. A comparison that day showed the drift was real: the shutter package had never run in production, and `script.allumer_pc_tv`, called by the firmware, existed in no public file.
- The firmware side follows the same idea through a different file: HA entity IDs used by the device come from `Tab5/user_entities.yaml` (gitignored) via `${entity_…}` substitutions, enforced by code rule 8.
