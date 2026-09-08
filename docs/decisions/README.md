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
