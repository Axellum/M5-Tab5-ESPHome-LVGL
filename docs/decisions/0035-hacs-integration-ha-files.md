# ADR-0035: A « Tab5 » integration, installed by HACS, puts the Home Assistant files in place in one click

**Status:** Accepted (2026-10-07, asked for by a user — discussion #278 — and decided by the author; lot 1: the integration and its tests, lot 2: the release asset and the guide)
**Date:** 2026-10-07

## Context

Every release of the project has two halves:

- **the firmware**, which already updates from Home Assistant: its « Firmware » entity reads the published manifest, *Install* does the rest (ADR-0022);
- **the Home Assistant files** — packages, custom templates, the blueprint (ADR-0024) — which the user replaces **by hand**: download `tab5_home_assistant.zip`, unzip it on a PC, copy it into `config/` (Samba, File editor…), reload all the YAML, then install the firmware. In that order: a newer firmware asks the files for things they do not know yet (`docs/installation/updates.md`).

With release candidates every few days, that hand work is what users repeat the most. On 2026-10-07 the first outside user asked for « a one click solution: download the Home Assistant zip, replace the files, restart ».

What was checked before choosing (2026-10-02 and 2026-10-07, HACS code and documentation, Home Assistant 2026.9.4 code):

- HACS installs integrations, dashboards cards, themes, AppDaemon apps, python scripts and **one** Jinja template per repository. **Neither YAML packages nor blueprints.** So HACS alone cannot do it.
- A blueprint imported by its URL can be re-imported from the HA interface, but packages and templates cannot.
- An app (add-on) only exists on Home Assistant OS and Supervised, not on a container install.
- `homeassistant.reload_all` checks the configuration first, then calls every `reload` service, `reload_core_config` and `reload_custom_templates`; `automation.reload` empties the blueprint cache. Every domain the packages use (`automation`, `script`, `template`, `input_*`, `rest_command`) reloads without a restart; a domain HA has not loaded yet (`rest_command`, `template` on a new install) can be set up hot with `async_setup_component`.
- HACS with `zip_release` downloads one asset of the release and extracts it **at the root** of `custom_components/<domain>/`; it still wants `custom_components/<domain>/manifest.json` in the repository. Every new release tag shows as an update of the integration, and an updated integration needs a restart.

## Decision

- **A small custom integration, `tab5`, in this repository** (`custom_components/tab5/`, `hacs.json` at the root), added to HACS as a **custom repository**.
- **The HACS release asset carries the files.** Each release attaches `tab5_hacs.zip` (`tools/publication/archive_hacs.py`): the integration code at the root, its `manifest.json` set to the release version, and `fichiers/` = the same bytes as `tab5_home_assistant.zip` (`archive_ha.entrees`, one source), listed in `fichiers/MANIFESTE.json`.
  - Why not an integration that downloads the zip itself (the design noted on 2026-10-02): HACS shows every release of the repository as an update of the integration anyway, so the user would have had **two** updates for one release. Here the HACS update *is* the files update. The price is one restart per release, which the request already accepted.
- **At start, once per version**, the integration (`__init__.py`, file logic in `installation.py`):
  1. saves what it will change in `config/tab5_sauvegardes/<date>_<old version>/` (the 5 most recent are kept), then writes atomically: `packages/`, `custom_templates/`, the blueprint — also over a copy imported by its URL — and an optional package only if the user already copied it; a file it had put and that the release no longer ships is removed (into the backup). Nothing else in `config/` is touched. A backup identical to the previous one is not made again (reused instead). A Tab5 file already there that the integration did not put (copied by hand) and that differs is replaced too, and named in a persistent repair with the backup folder (amended 2026-10-08, audit HA-10);
  2. runs the configuration check; if the new files add an error **or a warning**, every file is put back byte for byte and a repair says so. In Home Assistant 2026.9 an invalid domain or package is only a warning of that check (`helpers/check_config.py`), yet that domain no longer loads: in the CI of 2026-10-07, an `input_boolean` whose `initial` is not a boolean gave `result: valid` with one warning, so the errors-only `async_check_ha_config_file` would have let it through. Files refused this way (version + fingerprint) are not tried again at the next starts, only by « Install the files of this version again now » or when HACS brings other files (amended 2026-10-08, audit HA-9);
  3. without the `packages:` line, stops there (a repair says so); otherwise reloads the custom templates, then the `input_*` helpers, sets up the missing domains (helpers, `rest_command` and `template` before `script` and `automation`), then calls `homeassistant.reload_all`: no second restart. In that order, because an automation reloaded first read a template or a helper not there yet (CI, 2026-10-07). Any exception of a reload (`rest_command.reload` raises a `KeyError` when its domain is gone, 2026.9) only means a restart is needed;
  4. **checks the effect**: the packages' sensor « Tab5 · version des fichiers HA » must read the new version; otherwise a repair says what is missing (the `packages:` line, or a restart — with a button);
  5. posts a notification (French or English, from the server language) naming any file edited by hand since the last install;
  6. if the option « Then update the tablet » is on (default, it was the request), installs the firmware **of the same version** as soon as the tablet's « Firmware » entity offers it — and only once the step 4 sensor reads that version. The tablet is found by its device model `tab5-ha-hmi`, like the packages do. The wait is only forgotten once the tablet reports that version installed: ESPHome's `update.install` only sends the command, so an update without effect after 15 minutes is started again, 3 times in all, then a repair says to install it by hand (`firmware.py`, amended 2026-10-08, audit HA-11).
- **The manual zip stays.** `tab5_home_assistant.zip` and the guide's manual steps are unchanged; the integration is an option. The author's own Home Assistant keeps its Samba deployment from the repository and does not install the integration.

## Consequences

- A user installs it once (HACS → custom repository → download → restart → add the « Tab5 » integration; the `packages:` line in `configuration.yaml` is still needed once), then each release is: Settings → Updates → Tab5 → Install → restart.
- Tests: `tests/test_integration_tab5.py` runs the file logic without Home Assistant (plan, backup, removal, byte-for-byte restore, optional packages, imported blueprint, user files untouched) and the asset layout; `.github/workflows/integration-hacs.yml` runs hassfest, the HACS validation, and `tools/installation_ha/verifier_integration.py` in a fresh Home Assistant container: first install without restart, update like HACS, rollback on a broken configuration, missing `packages:` line. Not tested there: the chained firmware update (no tablet in that container).
- A new domain in a package must be checked to set up hot (`test_etiquette_et_domaines` lists the current ones).
- HACS validation ignores « brands »: a custom integration has no entry in `home-assistant/brands`.
- Removing the integration leaves the files in place: they are the user's configuration from then on.
- HACS reads the latest full release (pre-releases only when beta versions are switched on for the repository; its code filters `prerelease` unless `show_beta`). The repository can therefore be added once a full release carries `custom_components/tab5/`: 3.7.0. Releases up to 3.7.0-rc.4 have no `tab5_hacs.zip` (`archive_hacs.py` exits with code 3 on those tags).
- `publication.yml` attaches the asset a few minutes after the release is published (job `home-assistant`). A HACS update clicked in between is expected to fail to download rather than install from the source code (HACS code, not observed yet); it works once the job is done.
