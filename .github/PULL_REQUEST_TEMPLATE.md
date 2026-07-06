## What & why

<!-- One or two sentences: what changes, and why. Link an issue/task if relevant. -->

## Checklist

- [ ] `esphome compile tab5-ha-hmi.yaml` succeeds locally (`config_hash`: `______`)
- [ ] If this is a refactor with no intended behavior change: `config_hash` is identical before/after
- [ ] If a device is available and the change touches firmware behavior: tested via real OTA (device diagnostics checked afterward — `ha_api_status` on, uptime strictly increasing, no reboot) — otherwise noted as not tested and why
- [ ] If this touches code marked `[AI-WARNING]`: read the warning and `docs/decisions/`, explain below why the override is safe
- [ ] If this introduces a new non-obvious constraint: the relevant `[AI-CONTEXT]` block is added/updated
- [ ] If this changes the file/dependency structure: [`CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md) is updated
- [ ] [`CHANGELOG.md`](../CHANGELOG.md) has a new entry (skip for pure internal chores/docs typos)
- [ ] No hardcoded hex colors added, no `lv_obj_*` in a `sensor:`/`text_sensor:` lambda, no `static` for cross-handler state (see [`AGENTS.md`](../AGENTS.md#code-rules-full-detail-in-tab5readmemd))
- [ ] No secrets (`secrets.yaml`, real HA config under `HomeAssistant_Config/`) added or modified in this diff

## Notes for the reviewer

<!-- Anything non-obvious, tradeoffs made, follow-up left for later. -->
