## What & why

<!-- One or two sentences: what changes, and why. Link an issue/task if relevant. -->

## Checklist

- [ ] `esphome compile tab5-ha-hmi.yaml` succeeds locally (`config_hash`: `______`)
- [ ] `python -m pytest` and `pre-commit run --all-files` pass locally (the CI `python` job replays both)
- [ ] If this changes a game engine (`Tab5/jeux/go_engine.cpp`, `Tab5/jeux/chess_ai.cpp`, the `Draughts::Engine` block of `Tab5/jeux/draughts_game.cpp`): its host test (`tools/test_*.cpp`, run by the CI `python` job) still passes, and the Python mirror of chess or draughts (`tools/test_chess_perft.py`, `tools/test_draughts_engine.py`) is updated in the same PR
- [ ] If this is a refactor with no intended behavior change: `config_hash` is identical before/after
- [ ] If a device is available and the change touches firmware behavior: tested via real OTA (device diagnostics checked afterward — `ha_api_status` on, boot time on `Tab5 Uptime` unchanged after the flash's own reboot, no reboot) — otherwise noted as not tested and why
- [ ] If this touches code marked `[AI-WARNING]`: read the warning and `docs/decisions/`, explain below why the override is safe
- [ ] If this introduces a new non-obvious constraint: the relevant `[AI-CONTEXT]` block is added/updated
- [ ] If this changes the file/dependency structure: [`CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md) is updated
- [ ] [`CHANGELOG.md`](../CHANGELOG.md) has a new entry (skip for pure internal chores/docs typos)
- [ ] No hardcoded hex colors added, no `lv_obj_*` in a `sensor:`/`text_sensor:` lambda, no `static` for cross-handler state (see [`AGENTS.md`](../AGENTS.md#code-rules-full-detail-in-tab5readmemd))
- [ ] No secrets (a key such as `tab5_signature.pem`, a `!secret` in the firmware, real HA config under `HomeAssistant_Config/`) added or modified in this diff

## Notes for the reviewer

<!-- Anything non-obvious, tradeoffs made, follow-up left for later. -->
