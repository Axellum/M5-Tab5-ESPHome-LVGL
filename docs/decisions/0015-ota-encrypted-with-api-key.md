# ADR-0015: OTA encrypted with the API key — plain uploads refused, no OTA password

**Status:** Accepted (2026-09-16)
**Date:** 2026-09-25 (written retroactively from the `ota:` comment block in `Tab5/tab5-hardware.yaml`, the `min_version` comment in `tab5-ha-hmi.yaml` and the 2026-09-16 CHANGELOG entry)

## Context

Until ESPHome 2026.8 the device accepted OTA uploads protected by `ota: password:`: a shared secret that authenticates the uploader, while the firmware image itself travels in clear over the LAN. ESPHome 2026.9.0 added Noise encryption of OTA with the key that already protects the native API (esphome #18489, #18979). The project already had `api: encryption: key`, so the device had two secrets guarding two doors.

The two mechanisms cannot be combined: `password:` together with `encryption:` fails config validation. The password path also costs ~3.5 KB of flash and 60 B of RAM (ESPHome 2026.9.0 warning).

## Decision

`ota: encryption:` with an empty block, which inherits `api: encryption: key`: **one key protects both the API and OTA**. `password:` is removed. Since the 2026-09-16 firmware, **a plain upload is refused**. `min_version: 2026.9.0` because the `encryption:` block does not exist before that version.

The migration took two OTAs on 2026-09-16: first 2026.9.0 without the block (the firmware then *offers* encryption but still accepts plain uploads, `config_hash` 0xe62c67fb), then the block itself (`config_hash` 0x137762d8).

## Consequences

- The machine that flashes must have `api_encryption_key` in its `secrets.yaml`. The ESPHome CLI reads it from the YAML and encrypts on its own; nothing is passed on the command line. An old `ota_password` line in `secrets.yaml` is harmless and ignored.
- Losing the API key means losing OTA as well: recovery is a USB flash. Keep the key backed up with the other secrets.
- The CI dummy `secrets.yaml` keeps a (fake) `api_encryption_key`, because `ota: encryption:` inherits it at compile time.
- Rolling back is possible: remove `encryption:`, put `password:` back, flash. The rollback OTA itself goes out encrypted, so the device accepts it. Going below ESPHome 2026.9.0 requires that rollback first.
- Do not re-add `password:` "for defence in depth": it does not validate alongside `encryption:`, and the key already authenticates the uploader.
