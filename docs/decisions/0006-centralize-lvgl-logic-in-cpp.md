# ADR-0006: All non-trivial LVGL logic centralized in C++, never inline in sensor lambdas

**Status:** Accepted

## Context

ESPHome lets you write arbitrary C++ lambdas directly inline in `sensor:`/`text_sensor:`/`binary_sensor:` YAML blocks. It's tempting to just call `lv_obj_set_style_*`/`lv_label_set_text` right there when a sensor updates — it's fewer files to jump between for a one-off change.

## Decision

Sensors and services only read Home Assistant state and call a named function declared in `tab5_custom.h` / defined in `tab5_custom.cpp`. They never manipulate `lv_obj_*` directly, except for genuinely trivial 2-3 line cases (e.g. picking an icon color inline) that were judged not worth extracting.

## Consequences

- All non-trivial LVGL state logic is greppable and testable in one place (`tab5_custom.cpp`, ~520 lines) instead of scattered across `tab5-sensors.yaml`, `tab5-api-logic.yaml`, and 16 `ui_components/*.yaml` files.
- Every new "sensor reacts to X" feature requires touching two files (the YAML trigger + the C++ handler) instead of one — a small ongoing tax, accepted for the searchability/testability gain.
- This is enforced as code rule 2 in `Tab5/README.md` and checked by the `esphome compile` + manual review gate in the PR template, not by an automated lint — a lambda that violates this will still compile.
