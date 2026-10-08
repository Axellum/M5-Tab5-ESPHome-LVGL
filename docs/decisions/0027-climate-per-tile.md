# ADR-0027: Every climate tile opens the climate popup for its own unit — settings and state per tile, one translation of the commands

**Status:** Accepted (2026-09-29; not yet tried on the author's tablet)
**Date:** 2026-09-29

## Context

Since [ADR-0023](0023-rooms-generic-tiles.md), a `cli` tile opens the climate popup only with option `m` (« this climate is the blueprint's »); any other climate tile shows its room temperature and does nothing on tap. [ADR-0026](0026-climate-from-device.md) made the popup follow the blueprint's unit (key `climr`) and the blueprint translate the screen's commands to the unit's own modes. A home with several units could drive one of them from the screen.

The popup's state lived in globals (`clim_target_temp`, `clim_hvac_mode`, `clim_preset_mode`, `clim_fan_mode`, `clim_swing_mode`) that the compact card of the home page reads too (target colour, − / +), and its commands were sent with `emplacement: clim`.

## Decision

- **Settings per tile** — key `crRT|min|max|step|unit|capabilities|name` of `tab5_maj_emplacements`: the fields and rules of `climr`, computed by the same template (`reglages_clims`, entity → settings, from which `climr` is taken too). Pushed for every `cli` tile **without** `m`, after the definitions (connection, automation reload, « MAJ Écran », HA start, zones request, a tile entity appearing), and when the unit's settings may have changed: it comes back from `unavailable`, or one of `min_temp`, `max_temp`, `target_temp_step`, `*_modes`, `friendly_name` differs at a state or target change (`reglages_changes`, shared with `climr`).
- **State per tile** — key `ceRT|target|room|mode|preset|fan|swing`: the fields of `tab5_maj_clim`, in its order and with its defaults (`none`, `auto`, `stop`); an unknown number is `nan` (the popup shows « -- », − / + do nothing, the arc picks one). Pushed with the tile (every time its `tRT` is): after the definitions, at a mode change (trigger `piece_n`), at a target change (new trigger `piece_n_consigne`, attribute `temperature`, which only climates carry among the tile domains), and with the slow measurements (5 minutes, `last_updated`) for everything else.
  - *Why not at every change*: the room temperature moves by tenths and must not wake the automation (ADR-0019, 2026-09-28 update); `current_temperature` has no trigger.
  - *Why the target at once*: the popup's − / + count from the target it shows; a target 5 minutes old would send a wrong value. Fan, swing and preset buttons send the mode they ask for (not a relative step): a stale state costs at most one tap, and it catches up within 5 minutes.
  - One payload per pass, in this order: `crRT…`, `tRT…`, `ceRT…`; only at protocol 2 (firmware 3.2 or later).
- **Keys, not a new action.** A missing action is an error `continue_on_error` does not catch (ADR-0023); an unknown key is ignored by `emplacements_appliquer()`. So no firmware-version check beyond the tiles' own protocol.
- **Firmware** (`tab5_clim.cpp`): a table of 25 cells (settings + state), allocated in PSRAM at the first `cr` / `ce` key — nothing before, nothing in internal RAM; mode strings kept to 15 bytes. The popup shows the **displayed climate**: the blueprint's (default) or a tile's. A tap on a `cli` tile without `m` (and without `r`) opens the popup on its climate once its settings were received; before that, nothing, as before. With `m`, the blueprint's. The compact card and « Aller à l'écran → Climatisation » open the blueprint's. Closing with the × or the overlay brings the displayed climate back to the blueprint's; a popup closed another way (idle return home) is brought back at the next push from HA, before that push is stored.
- **Separate states.** The blueprint's climate keeps its globals: `tab5_maj_clim` writes them and the compact card reads them, whatever the popup shows. The popup's gestures (`clim_popup_*`) write the displayed climate (the globals, or the tile's cell); `clim_recolorer()` (the former YAML script `tab5_clim_recolor`) colours the card's target from the blueprint's mode and the popup from the displayed climate. A `tab5_maj_clim` arriving while the popup shows a tile updates the card only; a `ceRT` updates the popup only if it shows that tile, never the card.
- **Commands.** The popup's commands go with `emplacement` = `clim` or `tRT` (`clim_affichee_cle()`), same `action` / `valeur` as before. A tile's target goes through its own debounce (`tab5_debounce_clim_tuile`), key and value captured at the gesture: a popup closed within the 250 ms still sends it to that unit. The blueprint's branches « Clim : … » now act on `clim_cible` (the blueprint's climate for `clim`; the tile's entity for a `tRT` whose entity is a climate) with `clim_commande` (as is for `clim`; for a tile, after its custom behaviour — read only: nothing; on only: no `eteindre`). One translation for both; the whitelist is unchanged: only an entity a tile holds.

## Compatibility

- **New blueprint, older firmware:** `crRT` / `ceRT` are unknown keys, ignored; the tile still does nothing on tap. A 3.0/3.1 firmware (protocol 1) receives neither.
- **New firmware, older blueprint:** no `cr`, the tile does nothing on tap; the blueprint's popup is unchanged.
- **The blueprint's climate:** same `climr`, same `tab5_maj_clim`, same commands. Rendered with the previous blueprint (`main` @ ee6ee00) and this one, for 5 units (the author's Daikin, a Midea-like, a °F heat-only unit, one without attributes, one with other mode names) × 8 states × 26 screen commands × 2 homes (alone, and with a climate tile next to it): 2080 commands, 0 difference; 400 pushes of `climr` + `tab5_maj_clim` (connection, HA start, state changes), 0 difference.

## Consequences

- A home with several units drives each one from its tile; the compact card stays the blueprint's unit.
- For a tile's unit, fan / swing / preset changes made outside the screen reach an open popup within 5 minutes (the blueprint's unit still gets every change at once, through its own trigger).
- Five more triggers in the blueprint (`piece_n_consigne`).
- The popup's handlers call C++ (`clim_popup_*`, `clim_affichee_*`), and the colouring moved from YAML to C++ (ADR-0006); ADR-0007 still holds (one explicit YAML handler per button).
- A tile redefined (another entity at that place) forgets its climate until the blueprint resends it in the same pass; a popup open on it closes.
- `tests/test_clim.py` checks the keys on both sides, the order of the `ce` fields, the pushes at each trigger and the per-tile commands against the blueprint's translation; the « HA neuf » CI job adds the demo `climate.heatpump` to room 3 and checks its `cr` / `ce`.
