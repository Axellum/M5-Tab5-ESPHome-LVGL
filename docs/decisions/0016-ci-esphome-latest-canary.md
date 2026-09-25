# ADR-0016: CI compiles with ESPHome `latest` on purpose — a free upstream canary

**Status:** Accepted (made explicit in the workflow on 2026-09-06, PR #102)
**Date:** 2026-09-25 (written retroactively from the header of `.github/workflows/esphome-tab5.yml` and the ESPHome floor raises in `tab5-ha-hmi.yaml`)

## Context

The CI `build` job uses `esphome/build-action` without a `version:` input, so it pulls the `ghcr.io/esphome/esphome:latest` image. The usual reflex, and a likely audit finding, is to pin the version for reproducible builds.

The device itself is flashed from the developer's PC with a known ESPHome version, and `tab5-ha-hmi.yaml` carries `min_version:` as the only hard constraint. What the project lacks is early warning: a new ESPHome release that breaks this configuration (renamed option, stricter validation, removed component) is otherwise discovered on the day someone upgrades locally and tries to flash.

## Decision

Keep `latest`. The CI is a free canary for the newest ESPHome: on 2026-08-25 it was already compiling with 2026.8.1, and on 2026-09-16 with 2026.9.0, each time before the project raised its own floor. `min_version:` stays the only version contract. The build-action major version (`@v8.1.0`) is pinned and updated by Dependabot. That is the action, not ESPHome.

## Consequences

- A red `build` on a PR that did not touch the firmware can be an upstream change, not a regression of the PR. Check the ESPHome version printed at the top of the build log (`INFO ESPHome 20xx.y.z`) before debugging the diff.
- Reproducing a CI build locally means using the same ESPHome version as the log, not the one installed on the PC.
- The build-action enforces its own minimum ESPHome version (2026.7.0 for v8.1.0). Raising the project's `min_version` above what the action supports would need an action upgrade first.
- If a release ever breaks the build for longer than the project can absorb, pin `version:` temporarily **with a dated comment** and remove the pin once fixed. A silent permanent pin would switch the canary off.
- Since 2026-09-25 the job keeps its ccache between runs. The cache is keyed by run and restored from the most recent one, and ccache hashes the compiler, so a new ESPHome image cannot produce stale objects. Only the hit rate drops for that run.
