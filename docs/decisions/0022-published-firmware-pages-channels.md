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

- **Axel sets the secret once**, in the `publication` environment since 2026-10-01 (`gh secret set TAB5_CLE_SIGNATURE --env publication < C:\Users\axell\.tab5\tab5_signature.pem`, from `cmd`: PowerShell has no `<`); an AI never types a key. Pages must be enabled with the « GitHub Actions » source.
- A **lost or leaked key**: the secret and the digest in the workflow change together, and published tablets can only be flashed again over USB (ADR-0020).
- Relaunching the workflow for a tag rebuilds and replaces its files (`--clobber`); relaunching it for any tag republishes the page with the current `web/`.
- The download is protected by the signature, not by HTTPS (`verify_ssl: false`).
- Only the ST7123 has been tried on a device; the page says so for the other two revisions.

## Amendment (2026-09-28): a showcase page, the installer under `install/`, a site deployed on its own

The site was only the installer, and only `publication.yml` deployed it: fixing the page meant compiling the three firmwares again and replacing the release files. Search engines did not index the project's pictures either. Checked on 2026-09-28 (`curl`): github.com serves the images of a README as `/<owner>/<repo>/raw/main/…`, and its `robots.txt` has `Disallow: /*/raw/` for every crawler; `axellum.github.io` has no `robots.txt`, so the Pages site is the one place where they can be indexed.

- **`web/index.html` is a showcase page** (French and English, photos with captions and alt texts, the host renders of ADR-0021, `canonical`, Open Graph, JSON-LD `SoftwareSourceCode`); **the installer moves to `web/install/index.html`** and reads `../versions.json` and `../<channel>/<revision>/manifest.json`. The channel folders stay at the root of the site: published firmwares keep reading their updates at the same address.
- **Pictures under a descriptive name**: `tools/publication/pages.py` copies the files of `docs/images/` listed in `IMAGES` to `images/` (`m5stack-tab5-…`); the repository keeps its own names (README, docs, press kit). It also writes `sitemap.xml`: every page, with the images it shows.
- **`.github/workflows/site.yml` deploys the site without compiling anything**: called by `publication.yml` once the files are attached to a release, on every push to `main` that touches `web/`, `docs/images/` or `pages.py`, or by hand. Relaunching `publication.yml` for a tag still rebuilds and replaces its files; it is no longer needed to update the page.
- **A channel only serves a release whose files are attached** (#221 and its follow-up): `site.yml` lists the releases through the REST API (`gh api …/releases`, which gives their files and their upload state; `gh release list` does not), and `pages.py choisir` skips a release without the ST7123 manifest, or with a manifest but not yet its two binaries; the previous release is served meanwhile. `publication.yml` attaches the files after compiling (v3.1.0: published 11:29 UTC, files 11:41), and a push to `main` in between failed with « no assets to download » (2026-09-28). Another missing revision does not skip a release: one added to `ECRANS` later has no files in the older releases.
- **One Pages deployment per commit**: `actions/deploy-pages` gives the commit as `pages_build_version`, and the API rejects a version that is not a commit (404, tried in #231). On 2026-09-28, v3.2.0-rc.1 targeted the commit a push to `main` had just deployed: its deployment reported success, and the site kept serving the push's build (beta channel still on 3.1.0). `site.yml` now fails before deploying a commit that already has a successful deployment; publish the release on a new commit, or deploy from a new commit of `main`.
- `tests/test_publication.py` checks that every picture a page shows is listed in `IMAGES` and exists, that each page has a title, a description, its canonical address, a sharing image and alt texts, and that `site.yml` compiles nothing.

## Amendment (2026-10-01): the key in a protected environment

The audit of 2026-09-30 (S2) found the signing key in a repository secret: a workflow of any branch could read it.

- **The `firmware` job of `publication.yml` runs in the `publication` environment**, the only place that holds `TAB5_CLE_SIGNATURE`. The environment deploys only from `main` (a run started by hand) and from tags `v*` (a published release), and waits for Axel's approval (« Review deployments »), administrators included. The repository secret is removed.
- **Tags `v*` can no longer be deleted or moved** (repository ruleset « Tags de version immuables », checked on 2026-10-01 with a throwaway tag: creation accepted, force-push and deletion refused). A release built from the wrong commit gets a new version number; relaunching the workflow for the same tag still works.
- `tests/test_publication.py` fails if another job or another workflow reads the key.
- A release now waits for one approval in Actions before anything is compiled.
