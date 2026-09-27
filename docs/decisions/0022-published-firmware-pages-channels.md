# ADR-0022: Published firmware — signed in CI, a web flasher on GitHub Pages, stable and beta channels

**Status:** Accepted (2026-09-27)
**Date:** 2026-09-27 (lot 6c of the « ouverture » audit)

## Context

Since [ADR-0020](0020-no-secret-firmware-signed-ota.md) the firmware holds no secret and every build is the same for everyone, except for the display revision, which ESPHome chooses at compile time (three binaries: ST7123, ST7121, ILI9881C). Nobody could install it without a Python toolchain and a compile of about ten minutes.

Checked on 2026-09-27 (ESPHome 2026.9.0 installed on the dev PC, `esphome/build-action` v8.1.0, ESP Web Tools 10.4.0):

- **ESP Web Tools** flashes from Chrome or Edge (Web Serial), accepts `chipFamily: "ESP32-P4"`, offers Wi-Fi through Improv right after flashing (the firmware has `improv_serial`) and links to Home Assistant (`home_assistant_domain: esphome`). The page must be served over HTTPS, and the binaries with CORS headers: files attached to a GitHub release have none, GitHub Pages has them.
- **`update: platform: http_request`** reads the same manifest: `name`, `version`, and for the build whose `chipFamily` equals `ESPHOME_VARIANT` (« ESP32-P4 »), `ota.path` and `ota.md5`. A relative path is resolved from the manifest's folder. It needs `ota: platform: http_request`, which goes through the same OTA backend as `ota: esphome`: `signed_ota_verification` applies, so an image not signed with the project key is refused after download.
- **`esphome/build-action`** with `complete-manifest: true` writes the manifest (`name` = project name, `version` = project version, factory image at offset 0, OTA image with MD5 and SHA-256) and accepts `-s` substitutions.
- The existing `http_request` has `verify_ssl: false`: the HTTPS certificate of the download is not checked.
- Packages merge an `ota:` list with another list, but a dictionary `ota:` was **replaced** by the package's list (no `esphome` OTA left at all).

## Decision

- **Workflow `.github/workflows/publication.yml`**, run when a release is published (or by hand for a tag):
  - builds the three display revisions with ESPHome **pinned** (`ESPHOME_PUBLICATION`, the version tried on the tablet, never below `min_version`), signed with the **project key** from the `TAB5_CLE_SIGNATURE` secret. The key's SBv2 public digest is written in the workflow: if the secret gives another one, nothing is signed. The signature of each image is checked, then the key file is deleted;
  - attaches `tab5-ha-hmi-<revision>.factory.bin`, `.ota.bin` and `manifest-<revision>.json` to the release (`tools/publication/preparer.py`);
  - rebuilds the whole GitHub Pages site from the **files of the releases** (`tools/publication/pages.py`), never from one run: `stable/<revision>/` holds the latest 3.x release that is not a pre-release, `beta/<revision>/` the most recent 3.x release. The page comes from `web/` on the default branch.
- **Channels**: a pre-release is built for the beta channel, any other release for the stable one. A beta tablet therefore moves on to the stable that follows its beta; a stable tablet never sees a pre-release.
- **The update entity exists only in published firmware**: `tab5_publication` (`locale` by default, `stable` or `beta` in the workflow) selects `Tab5/publication-<value>.yaml`; the local one is empty. Someone who compiles with their own key would otherwise be offered updates their tablet refuses.
- **Version**: `project: version` is `${ tab5_version | default('3.0.0-dev') }`; the workflow passes the tag without its `v`, which the update entity compares with the manifest.
- `ota:` in `Tab5/tab5-hardware.yaml` becomes a list, so a published build keeps `esphome` and adds `http_request`.
- The three entity IDs the firmware still called with placeholders get defaults that exist everywhere (lot 6c-1): nothing is left to set for a published binary.

## Alternatives rejected

- **Signing on the PC and uploading by hand**: the key never leaves the PC, but every release depends on it being on, and nothing checks what is published. Axel chose the CI secret (2026-09-27).
- **Binaries only as release files**: no CORS, so no web flasher and no update from HA.
- **One site per run (artifacts of the current release)**: a pre-release would replace the stable manifests.
- **The update entity in every build**: a tablet signed with another key would be offered updates it refuses.

## Consequences

- **Axel sets the secret once** (`gh secret set TAB5_CLE_SIGNATURE < C:\Users\axell\.tab5\tab5_signature.pem`); an AI never types a key. Pages must be enabled with the « GitHub Actions » source.
- A **lost or leaked key**: the secret and the digest in the workflow change together, and published tablets can only be flashed again over USB (ADR-0020).
- Relaunching the workflow for a tag rebuilds and replaces its files (`--clobber`); relaunching it for any tag republishes the page with the current `web/`.
- The download is protected by the signature, not by HTTPS (`verify_ssl: false`).
- Only the ST7123 has been tried on a device; the page says so for the other two revisions.
