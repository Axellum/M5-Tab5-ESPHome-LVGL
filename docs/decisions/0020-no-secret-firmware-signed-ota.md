# ADR-0020: No secret in the firmware — Home Assistant provisions the API key, OTA images are signed

**Status:** Accepted (2026-09-27) — supersedes [ADR-0015](0015-ota-encrypted-with-api-key.md)
**Date:** 2026-09-27 (lot 6b of the « ouverture » audit)

## Context

Lot 6 aims at one firmware for everyone, flashed from a browser (lot 6c) and set up in Home Assistant with the mouse ([ADR-0019](0019-logical-slots-blueprint.md)). Until now every binary was personal: the Wi-Fi credentials, the fallback AP password and the API key were compiled in from `secrets.yaml`, and the OTA was encrypted with that API key (ADR-0015).

Checked in the ESPHome 2026.9.0 code installed on the dev PC and in Home Assistant 2026.9.3:

- `api: encryption:` without `key:` is valid. The device boots without a key and accepts the well-known all-zeros Noise key (and, until ESPHome 2027.2, plaintext). Home Assistant generates a key, sends it over that encrypted connection (`manager.py`, `_async_provision_key_over_noise`) and keeps it; the device saves it in NVS and refuses anything else from then on. The compiled key of a YAML build is never written to NVS.
- `provisioning: timeout:` bounds how long an unprovisioned device accepts that first contact. The window starts at boot, closes in RAM only (a reboot reopens it) and, once closed, the device refuses every connection and shuts down its fallback AP until it is rebooted.
- A keyless API cannot give its key to `ota: encryption:` (validation error). Without it, the OTA offers encryption once the key is provisioned but **still accepts a plain upload** (`ota_esphome.cpp`, « offered, plaintext accepted »). A shared OTA password would be the same for every user and readable in the published binary.
- `esp32: framework: advanced: signed_ota_verification:` works on the P4 (RSA-3072 or ECDSA-256, Secure Boot v2 signature block, no eFuse burnt). With `signing_key:` the build signs the app; ESP-IDF then refuses any OTA image whose signature block does not match the key of the running firmware. With `verification_keys:` (up to 3 public keys), images are signed outside the build.
- Wi-Fi credentials saved at run time (captive portal, Improv, `wifi.configure`) are keyed by the config hash when the YAML has credentials, and by a fixed slot when it has none: a build without credentials keeps them across OTAs, but a build with credentials cannot hand them over.
- `time: platform: homeassistant` without `timezone:` applies Home Assistant's time zone at every time sync (HA ≥ 2026.3). Nothing keeps it for the next boot: until HA answers, the clock uses the zone of the machine that compiled (UTC on GitHub runners).

## Decision

- **Wi-Fi**: no credentials in the YAML. They are set by `improv_serial` (USB, from the web flasher of lot 6c or ESPHome Web) or by the fallback AP's captive portal. The AP keeps its name and becomes **open**: a password written in a public repository protects nothing.
- **API**: `encryption: {}` without a key, plus `provisioning: timeout: 30min`. Adding the tablet in Home Assistant gives it its key.
- **OTA**: `ota: platform: esphome` without `encryption:`, and **signed images**: `signed_ota_verification` with `signing_key:` (RSA-3072). The private key is a local file outside git (`tab5_signature.pem` at the root, or the path in the `tab5_cle_signature` substitution), required to compile, like `secrets.yaml` was. Only a firmware signed with the same key can replace the running one over the network.
- **Time zone**: `time: platform: homeassistant`. The last zone received from HA is kept in NVS and restored at boot, so the alarm clock rings at the right local time even when HA is down after a power cut. The `tab5_fuseau` setting disappears.
- **Identity**: `esphome: project:` (`axellum.tab5-ha-hmi`), needed by the update manifest of lot 6c. `update: http_request` waits for lot 6c, where the manifest is published.
- `secrets.yaml` is no longer read by the firmware.

## Alternatives rejected

- **External signing with `verification_keys:`**: compiling would need no secret at all, but every local flash would need a separate signing step (`esphome run` would upload an unsigned image and fail). Worth reconsidering if the key ever has to be rotated.
- **Keeping compiled Wi-Fi credentials for people who compile**: two configurations to maintain, and the web-flashed firmware would differ from the compiled one.
- **Keeping ADR-0015 (OTA encrypted with a compiled API key)**: every binary would carry a key, which is exactly what a published firmware cannot do.

## Consequences

- **An existing tablet migrates once**: the new firmware is uploaded with the old API key (the running firmware refuses a plain upload), then the tablet opens its fallback AP to receive the Wi-Fi credentials, and Home Assistant asks to confirm that encryption was removed; it then provisions a new key by itself. Both steps must happen within 30 minutes of the tablet's boot, or after a reboot.
- **Losing the signing key** means no more OTA: the only way back is a USB flash. Keep a copy outside the PC. The CI compiles with a throwaway key; releases (lot 6c) are signed with the project key kept in the CI secrets.
- **Someone who compiles with their own key** can only flash their own builds over the network; the project's published firmware is for tablets flashed from the browser.
- **`esphome logs` no longer finds a key in the YAML.** `tools/tab5_logs.py` reads the key Home Assistant keeps (`.storage/core.config_entries`) or `TAB5_CLE_API`, and never prints it. The demo pusher does the same, and provisions its own key on a tablet that has never met Home Assistant (to give later to HA).
- **The open fallback AP** lets someone within Wi-Fi range change the network of a tablet that lost its own. They can neither flash it (signature) nor talk to its API (key); the worst case is a tablet cut off until its Wi-Fi is set again.
- Going back over the network needs a firmware still signed with the same key (for instance this lot reverted except the `signed_ota_verification` block); anything else goes through USB.
