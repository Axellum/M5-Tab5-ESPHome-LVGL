# ADR-0018: Optional zones — a zone disappears only when Home Assistant confirms its entity does not exist

**Status:** Accepted (2026-09-27) — updated by [ADR-0019](0019-logical-slots-blueprint.md) (lot 6a): the tablet now sends only the slot keys, and the « Tab5 — emplacements » blueprint answers (an empty slot is an absent zone); commenting a line in `user_entities.yaml` no longer applies.
**Date:** 2026-09-27 (lot 5 of the « ouverture » audit)

## Context

The screen was built around the author's home: 3 lights, a climate unit, a greenhouse shutter, a TV, 5 plant sensors. Someone without a climate unit saw a fake one at 20.0 °C (HA's `| float(20)` on a missing entity), a lamp with no entity looked "off", 1 to 3 plant sensors were repeated across the 4 slots, and every button called entities that do not exist.

Removing zones at compile time was rejected: all widgets live in `tab5-lvgl.yaml` and are cross-referenced (the 5 weather tiles also carry the device buttons), and the generic firmware planned next (lot 6) cannot depend on a per-home build.

Hiding a zone because its entity stayed silent was also rejected. Checked in HA's ESPHome integration (`manager.py`): a missing entity sends nothing, but so does an entity **created after the device subscribed**. State-changed events with no `old_state` are not forwarded, and HA's ESPHome connection starts during bootstrap, in parallel with the other integrations. After an HA restart, a lamp that never changes could stay silent for hours. The device cannot re-request a state either: ESPHome sends its subscription list once per connection.

## Decision

- **HA decides, the device asks.** Once per connection, on the first forecast push (proof that HA automations run, which they don't during HA startup), the device fires `esphome.tab5_zones` with `key=entity` pairs for the entities it tracks. The `tab5_zones_reponse` automation (`packages/tab5_push.yaml`) answers `tab5_maj_zones` with the keys whose entity does not exist (`states[e] is none`: an `unavailable` entity exists). It adds the zones only HA knows about: `clim`, `volet` (entity or package missing), `planning` (no work calendar).
- **Data always wins.** Any state received makes its zone reappear (`zone_vue()`), whatever HA said.
- **No answer, no change.** Without the package, nothing is ever hidden: the pre-lot-5 behaviour.
- **Removing a zone = commenting its line** in `Tab5/user_entities.yaml`. `Tab5/tab5-zones.yaml` gives every `entity_…` a default that exists in no HA (`*.tab5_zone_absente`); package substitutions are overridden by the user file (checked on ESPHome 2026.9.0).
- The hidden set is kept in NVS and applied in `on_boot` (-100) before the first frame, so a small home does not flash the missing zones at every boot.

## Consequences

- The keys (`lumiere_1` … `planning`) are a contract written in four places: the `Zone` enum, `kCles`, the request string and the HA automation. `tests/test_zones.py` compares them.
- A typo in an entity ID now hides its zone instead of showing a fake state. The « Zones masquées » diagnostic sensor lists what disappeared.
- More than 3 lights, or a different device per tile, is out of reach: the tiles are 5 fixed places. That is lot 6 (logical slots), which will replace the detection with a mapping pushed by HA.
