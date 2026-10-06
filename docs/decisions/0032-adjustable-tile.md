# ADR-0032: The − / + tile of the climate card adjusts a device chosen on the tablet — the climate, up to eight devices picked in the blueprint, or the tablet's volume

**Status:** Accepted (2026-10-06, asked for and decided by the author; not yet tried on a tablet)
**Date:** 2026-10-06

## Context

The climate card of the home page (top right, `climate_card.yaml`) shows the living-room and greenhouse temperatures and, under them, a glass tile « − / setpoint / + » that adjusts the blueprint's climate; touching the setpoint opens the climate popup. A home without a climate had nothing there, and a home with other things it adjusts often (the TV or soundbar volume, a light's brightness, a radiator, a water heater, a fan) had to open a room page or a popup for each.

The author asked for the − / + buttons to adjust other devices too — « the TV, soundbar or tablet volume, a light's brightness, various thermostats, or other devices I don't think of » — keeping the climate popup; the choice stays fixed, and the list unfolds when touching the living-room icon or temperature. The author then decided (2026-10-06):

1. the chosen device stays until the next choice, even after a reboot;
2. the devices of the list are picked in the blueprint, in a section of their own; the blueprint's climate comes first and the tablet's volume always last;
3. touching the value between − and + opens the device's popup when it has one (climate → climate popup, a light or a shutter → its popup, the TV → the remote), otherwise nothing.

Constraints that hold: push-only and events-only (ADR-0001, ADR-0025): the firmware names no entity and calls no Home Assistant action; the blueprint only commands entities of its own lists (whitelist, origin guard); one source for icons (the tiles' palette, ADR-0023); no hardcoded colour (themes, ADR-0029); public HA files without a real entity (ADR-0024); an older firmware or an older blueprint keeps working.

## Decision

- **Name: « tuile − / + » (− / + tile), keys `rN`.** The keys of the list start with `r` (*réglable*, adjustable), N from 0 to 7: no collision with `t…`, `h…`, `b…`, `cr…`.
- **Picked in the blueprint** « Tab5 — emplacements », in a collapsed section « Tuile − / + · − / + tile »: one entity list (`reglables`, reorderable), domains `media_player`, `light`, `climate`, `water_heater`, `humidifier`, `fan`, `cover`, `valve`, `number`, `input_number`. The variable `reglables` keeps the first eight that exist, have a type, are not the blueprint's climate (already first), are not duplicates and are not « read only » in the tiles' customisation. It is the **whitelist** of the commands of an `rN` key.
- **Types** (`types_reglables` in the blueprint, `kTypes` in `tab5_reglables.cpp`; `tests/test_reglables.py` compares):

  | Type | Domains | Value | − / + |
  |---|---|---|---|
  | `son` | media_player | volume in % | 5 % |
  | `lum` | light | brightness in % (0 = off) | 10 %, or 100 without dimmer |
  | `cli` | climate | setpoint | the device's step, °C/°F as for the climate (ADR-0026) |
  | `eau` | water_heater | setpoint | the device's step, else 0.5 |
  | `hum` | humidifier | target humidity in % | 5 % |
  | `ven` | fan | speed in % (0 = off) | the device's step if ≥ 5, else 10 |
  | `vol` | cover, valve | position in % | 10 % with a settable position, else 100 (closed / open) |
  | `nbr` | number, input_number | the value | its step, min, max and unit |

- **Definitions** with the tiles' in `tab5_maj_tuiles` (protocol 2): `rN|type|icon|options|link|min|max|step|unit|name`. Icon: the tiles' palette, chosen the same way (customisation, `icon` attribute, device class, domain). Option `t`: the blueprint's TV (its value opens the remote). Link: the first tile carrying the same entity (`tRT`), whose popup the value opens. Unit: 7 bytes at most; `|` and `;` are replaced in the unit and the name, as for the tiles.
- **States** in `tab5_maj_emplacements`: `rN|state|value`, the value in its definition's unit (`nan` when Home Assistant has none: a player switched off, a shutter moving without position). Pushed with all states, and for one device when its state or the attribute carrying its value changes (triggers `reglable`, `reglable_volume`, `reglable_luminosite`, `reglable_consigne`, `reglable_humidite`, `reglable_vitesse`, `reglable_position`; never all attributes: a player changes them with every song). A device that appears redefines everything.
- **Commands.** The tablet sends `esphome.tab5_action` with `rN` / `regler` / value, or `rN` / `consigne` / value for a climate (it then takes the existing branch « Clim : consigne », with its bounds and its mode when off). The branch « Tuile − / + : régler » bounds the value to the definition and picks the action by domain: `media_player.volume_set`, `light.turn_on` (`brightness_pct`) or `turn_off` at 0, `fan.set_percentage` or `turn_off` at 0, `humidifier.set_humidity`, `water_heater.set_temperature`, `cover/valve.set_*_position`, otherwise open above 50 and close below (the shutter of the simulated-travel package through its script), `number/input_number.set_value`. An `rN` key out of the list, another command or a value that is not a number command nothing.
- **Firmware** (`tab5_reglables.cpp`, package `tab5-reglables.yaml`, `ui_components/reglables_liste.yaml` and its `reglables_ligne.yaml` template): definitions in their own NVS record (`REG1`), so the tile shows the right device at boot; the choice in another (`RGC1`), recognised by type and name (FNV-1a; two devices of the same type and name are told apart by their rank), so reordering the blueprint's list keeps the device; a vanished choice falls back to the first entry. The list: the climate (when present), the blueprint's devices, the tablet. Climate chosen: − / +, the setpoint and its popup take **exactly their former path** (`clim_target_temp`, `tab5_debounce_clim_temp`). Another device: `clim_target` is hidden, `reglable_rangee` shows its icon and value; − / + move the value on the grid of its step, at once (optimistic), and one command leaves 250 ms after the last touch (`tab5_debounce_reglable`, as the climate); a state received during a gesture keeps the gesture's value. The tablet's volume goes through `tab5_volume_apply` (the single entry point of the volume), step 5 %.
- **The list** is a full-screen transparent catcher with a glass panel (520 px, right edge on the card's, top on its top), one 52 px row per device: icon, name, value; the chosen row in accent. Touching a row chooses it and closes the list; touching elsewhere closes it; it closes after the popups' inactivity delay. It is not a modal popup (no scrim, no header): registered as a `SUBWINDOW` (closed with the popups), `style_glass_card`, not `style_modal_card`. It is the 17th window of `ModalRegistry` (`MAX` = 24).
- **The tile's visibility** does not change: shown with a climate or at least one device of the blueprint; the tablet's volume alone does not show it.

## Compatibility

- **New blueprint, older firmware** (3.6.0 and before): the `r` keys are unknown and ignored; the tile adjusts the climate as before. The new section only adds keys.
- **New firmware, older blueprint:** no `r` key, so the list holds the climate and the tablet; with the climate chosen (default) the screen is the one of 3.6.0.
- **Section left empty:** no `r` key, same as above.
- **Protocol 1** (firmware older than 3.2): neither definitions nor `r` states are sent.

## Consequences

- One more package (twenty-three), C++ unit and two UI files. `climate_card.yaml` gains the touch zone `btn_reglables_liste` on the living-room row and `reglable_rangee` between − and +. The tiles' palette gains the `tablette` code (`mdi:tablet`).
- The touch zone on the living-room temperature is new: before, touching it did nothing.
- `tests/test_reglables.py` holds the types on both sides, the whitelist, the bounds, the definitions, the states, the triggers, the command per domain, the card and the list; the « Installation dans un HA neuf » job loads the blueprint in a real Home Assistant.
