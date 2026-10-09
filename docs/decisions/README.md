# Architecture Decision Records

Lightweight ADRs capturing *why* a non-obvious choice was made in this codebase — not just what the code does (the code and `Tab5/README.md`/`CARTOGRAPHIE_TAB5.md` already say that), but the reasoning and the alternatives that were rejected. Written retroactively from the project's development history so an agent or contributor doesn't have to re-derive (or worse, re-litigate) a decision that was already made deliberately.

Kept in English only, unlike the rest of the docs, to stay short — these are an internal reference for contributors and AI agents, not user-facing documentation.

Format: **Context / Decision / Consequences**. One page max. Add a new one whenever you make a call that isn't obvious from reading the code alone, especially if you expect a future audit (human or AI) to flag it as a "bug."

## Index

| # | Decision |
|---|----------|
| [0001](0001-push-only-zero-polling.md) | Push-only architecture, zero polling from the device |
| [0002](0002-single-page-swipe-navigation.md) | Single LVGL page + swipe navigation instead of a multi-page tab bar |
| [0003](0003-data-packing-delimited-strings.md) | Delimited-string payloads instead of one service call per data point |
| [0004](0004-no-alpha-png-prebaked-backgrounds.md) | No PNG alpha channel — pre-baked opaque backgrounds |
| [0005](0005-boot-delay-gpio-expander-reset.md) | Blocking boot delay before the display reset sequence |
| [0006](0006-centralize-lvgl-logic-in-cpp.md) | All non-trivial LVGL logic centralized in C++, never inline in sensor lambdas |
| [0007](0007-climate-popup-not-factorized.md) | `climate_popup.yaml` deliberately left un-factorized |
| [0008](0008-single-ha-instance.md) | Single Home Assistant instance (Freebox) instead of a Deck failover |
| [0009](0009-modal-shell-header.md) | Shared modal chrome + one compact title bar for every popup |
| [0010](0010-shared-i2s-bus-mic-speaker.md) | Microphone and speaker share one I2S bus — every local sound relays the mic itself |
| [0011](0011-api-reboot-timeout-60min.md) | `api: reboot_timeout: 60min` — anti-zombie net kept, HA outages no longer cycle the tablet |
| [0012](0012-lvgl-rotation-270-pinball-portrait.md) | Landscape dashboard via `rotation: 270`, one console flips to portrait at runtime |
| [0013](0013-single-registry-consoles-modals.md) | One C++ registry lists the 8 consoles and the modal windows — no list is ever copied into YAML |
| [0014](0014-game-common-helpers-local-palettes.md) | The 8 consoles share `game_common.h`; every console keeps its palette local; every engine has a Python mirror |
| [0015](0015-ota-encrypted-with-api-key.md) | ~~OTA encrypted with the API key — plain uploads refused, no OTA password~~ (superseded by 0020) |
| [0016](0016-ci-esphome-latest-canary.md) | CI compiles with ESPHome `latest` on purpose — a free upstream canary |
| [0017](0017-ha-placeholders-rendered-copies.md) | ~~Public HA files hold placeholders only — real IDs in `placeholders.yaml`, HA runs `rendered/`~~ (placeholders superseded by 0024; the leak guard stays) |
| [0018](0018-optional-zones-confirmed-by-ha.md) | Optional zones — a zone disappears only when HA confirms its entity does not exist |
| [0019](0019-logical-slots-blueprint.md) | Logical slots — the device knows slots, a Home Assistant blueprint maps them to entities |
| [0020](0020-no-secret-firmware-signed-ota.md) | No secret in the firmware — Home Assistant provisions the API key, OTA images are signed |
| [0021](0021-host-render-stubs.md) | The screen is rendered off the tablet — the `host` platform, stubbed hardware, no firmware change |
| [0022](0022-published-firmware-pages-channels.md) | Published firmware — signed in CI, a web flasher on GitHub Pages, stable and beta channels |
| [0023](0023-rooms-generic-tiles.md) | Rooms — each page of the five bottom tiles is a room of up to five devices, described by Home Assistant |
| [0024](0024-packages-without-placeholders.md) | Packages without placeholders — every home value is picked in Home Assistant, the tablet is detected by its model |
| [0025](0025-events-only.md) | Events only — the device never calls a Home Assistant action, one HA package maps its events to a whitelist |
| [0026](0026-climate-from-device.md) | The climate popup follows the device — settings pushed by HA (key `climr`), the screen's commands translated by the blueprint |
| [0027](0027-climate-per-tile.md) | Every climate tile opens the climate popup for its own unit — settings (`crRT`) and state (`ceRT`) per tile, one translation of the commands |
| [0028](0028-solar-energy-popup.md) | An Energy popup for a solar installation — sensors picked in the blueprint, live values and production history (recorder statistics) pushed by HA while the popup is open |
| [0029](0029-themes-palette.md) | Themes — one C++ palette (`struct Palette`, `UIColor` = the active one), role styles in the YAML instead of colours set on widgets, games stay dark |
| [0030](0030-documentation-site.md) | A documentation website built from `docs/` — MkDocs with Material, no plugin, the language split and the links done by `tools/site/construire.py` |
| [0031](0031-row-under-the-clock.md) | A row under the clock — up to three lines of four sensors plus the plants line, rotating with the central card, picked in the blueprint (`hLI`, `hp`, `hd`) |
| [0032](0032-temperature-history-popup.md) | A Temperature popup — the history of the two home-screen temperatures (24 h, 7 days, 30 days) and the forecast for the second one, pushed by HA from its recorder statistics while the popup is open |
| [0033](0033-adjustable-tile.md) | The − / + tile of the climate card adjusts a device chosen on the tablet — the climate, up to eight devices picked in the blueprint (`rN`), or the tablet's volume |
| [0034](0034-central-card-alerts.md) | Alerts of the central card — HA remembers each alert and its revision, a read alert comes back only when it changes, subscriptions and history live in HA |
| [0035](0035-hacs-integration-ha-files.md) | A « Tab5 » integration, installed by HACS, puts the Home Assistant files in place in one click — the release asset carries the files, backup, configuration check and rollback, then the firmware of the same version |
| [0036](0036-quick-action-wheel.md) | A long press on a light, a shutter or a climate opens a wheel of quick actions around the tile — two rings: its commands and their families, then the « Maison » and « Détails » links |
| [0037](0037-house-popup.md) | A House popup — every room of the blueprint at once, one column per room, rows drawn and touched like the tiles, « Éteindre les lumières » in the title bar |
| [0039](0039-gestes-accueil.md) | The home gestures — the clock in three touch areas (hours, minutes, date), the tap and long press of the clock and of the three top buttons chosen in the blueprint (`gestes` key), screens or actions |
| [0041](0041-ok-nabu-panel-scrolling.md) | The « Ok Nabu » panel takes lines like the row under the clock (`nLI`, `np`, `nd`), the hours tap shows its next line, and the row, the panel and the − / + tile scroll by choice (`defil` key) |
