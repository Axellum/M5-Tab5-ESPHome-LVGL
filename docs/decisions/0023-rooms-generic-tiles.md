# ADR-0023: Rooms — each page of the five bottom tiles is a room of up to five devices, described by Home Assistant

**Status:** Proposed (2026-09-28, contract for the « pièces » lots; not tried on a tablet yet)
**Date:** 2026-09-28

## Context

Since 3.0 ([ADR-0019](0019-logical-slots-blueprint.md)) the user picks the entities in a blueprint, but the screen still draws the author's home: five fixed places (PC/TV, shutter, three lights), fixed icons (bed, sofa, LED strip), fixed titles (« PC Bureau », « Chambre », « Salon », « LEDs ») and one colour for « on ». A lamp named « Cuisine » shows a bed and the title « Chambre ». More than three lights, or a device of another kind, has no place.

The bottom row already has five pages (hours 5-9, hours 0-4, days 0-4, days 5-9, days 10-14: `forecast_page_index` 0..4, home = 2), drawn by the same five tile objects (`g_day_slots`, `g_hour_slots`), and the « HA » layer (`layer_switches`, five `sw_card`) is drawn over them. The central card has a title panel (`page_title_wrapper`: small line + main line) used for the forecast pages. Checked on 2026-09-28 in the code: swiping while the « HA » layer is shown changes the forecast page and shows the forecast layer again under the cards (a bug), and « Go to screen → Home » does not leave the « HA » layer.

Checked in Home Assistant's selector documentation (2026-09-28): an `entity` selector with `multiple: true` has a `reorder` option; an `icon` selector returns `mdi:…`; an `object` selector takes `fields` (each with any selector) and `multiple: true`. A template cannot read the icon HA's frontend shows (integration icon translations), only the `icon` attribute, which is set when the user picks an icon in the entity's settings.

## Decision

### Rooms and pages

- **A room is a page of the bottom row**: up to 5 rooms × 5 tiles = 25 tiles. Room `R` ↔ forecast page, in the order a swipe reaches them from home:

  | Room `R` | Page | Weather it carries | Reached by |
  |---|---|---|---|
  | 0 | 2 | days 0-4 (home) | — |
  | 1 | 3 | days 5-9 | swipe left |
  | 2 | 4 | days 10-14 | swipe left twice |
  | 3 | 1 | next 5 hours | swipe right |
  | 4 | 0 | hours 5-9 | swipe right twice |

- **Tile `T` is the visual position**, 0 = left … 4 = right, on every page (on the hourly pages, tile `T` is the object `h(4−T)`).
- Keys: room `pR`, tile `tRT` (`t00` … `t44`).

### What HA sends: definitions, then states

**Definitions** — new API action `tab5_maj_tuiles(payload)`, a full snapshot (whatever is not listed is empty), sent on (re)connection, on automation reload and on the zones request, before the states:

```
payload    := entry (';' entry)* [';']
entry      := 'p' R '|' name
            | 't' R T '|' type '|' icon '|' options '|' complement '|' name
type       := lum | int | vol | med | act | cap | bin | cli
icon       := a palette code ([a-z0-9_]{1,15}), '' = the type's default
options    := letters among d c o k r t m e, '' = none
complement := cap: unit (≤ 7 bytes: '°C', '%', 'W', 'kWh'…) ; bin: device class ('door', 'motion', 'presence'…) ; else ''
name       := display text; the firmware keeps ≤ 24 bytes (cut on a character boundary) and drops the characters its fonts lack
```

In every text field `|` becomes `/` and `;` becomes `,` (as today).

| Type | HA domains | Tap | Long press |
|---|---|---|---|
| `lum` | `light` | `basculer` | light popup (the room's `lum` tiles) |
| `int` | `switch`, `input_boolean`, `fan`, `humidifier`, `automation` | `basculer` (`allumer` only, with `o`) | device popup (update 2026-10-06, below) |
| `vol` | `cover`, `valve` | moving → `arreter` (pause); else the chosen direction (`ouvrir` / `fermer`, see below) | shutter popup (update 2026-10-05, below); with `k`, the other one of `ouvrir`/`fermer` |
| `med` | `media_player` | `basculer` | TV remote, with `t`; else the device popup (update 2026-10-06) |
| `act` | `scene`, `script`, `button`, `input_button` | `lancer` | device popup (update 2026-10-06) |
| `cap` | `sensor`, `number`, `input_number` | — (read only) | — |
| `bin` | `binary_sensor`, `device_tracker`, `person`, `lock` | — (read only) | — |
| `cli` | `climate` | climate popup: the blueprint's with `m`, else its own once HA sent its settings ([ADR-0027](0027-climate-per-tile.md)) | — |

Options: `d` dimmable (brightness), `c` colour, `o` on only (never switched off from the screen), `k` confirm (a second tap within 3 s sends; the state line asks for it), `r` read only (no tap at all), `t` this media player is the blueprint's TV (remote), `m` this climate is the blueprint's climate (popup; without `m`, the tile's own climate, [ADR-0027](0027-climate-per-tile.md)), `e` this sensor is one of the blueprint's « Énergie » section (a `cap` whose tap opens the Energy popup, [ADR-0028](0028-solar-energy-popup.md); an older firmware ignores the letter and keeps a read-only tile).

**States** — the existing `tab5_maj_emplacements(payload)`, new keys with a fourth field:

```
't' R T '|' state '|' value '|' colour
state  := the HA state as is (on, off, open, closing, playing, 21.4, home, cool, unavailable…)
value  := lum: brightness 0-255 ; vol: current_position 0-100 ; cap: the number ; cli: current_temperature ; else nan
colour := lum when on and the light reports rgb_color: 'RRGGBB' ; else ''
```

All tiles after the definitions; one tile when its entity changes (state, brightness, rgb_color, position); `cap` tiles are batched with the slow measurements (5 minutes, ADR-0019 update). A 3.x firmware ignores unknown keys (`tab5_custom.h`, `emplacements_appliquer`).

### What the tablet sends

`esphome.tab5_action` as today, `emplacement` = `tRT` or `pR`:

| `action` | `valeur` | For |
|---|---|---|
| `basculer`, `allumer`, `eteindre` | '' | `lum`, `int`, `med` |
| `ouvrir`, `fermer`, `arreter` | '' | `vol` |
| `position` | 0-100 | `vol` (shutter popup: the drawn shutter, on release) |
| `lancer` | '' | `act` |
| `luminosite` / `luminosite_pct` / `couleur` | 0-255 / 10-100 / colour name | `lum` (popup) |
| `pR` + `eteindre` | '' | every `lum` tile of room `R` (popup « Tout éteindre ») |

HA dispatches by the **domain of the tile's entity**, and acts only on entities placed in a tile (the blueprint is the whitelist).

### Screen

- **Weather mode** (as today): on **every** page, a tile holding a device shows it in its « shoulders » — left: the device's icon coloured by its state; right: a bulb (`lum`) or the arrow of the next move (`vol`), nothing for the other types — and its invisible action button (tap / long press above). A page without devices looks as today.
- **HA mode** (button « HA »): the five cards show the current page's room: icon (palette, 70 px), name (title tab, cut with « … »), state line (translated by the tablet), colour by type and state. Empty tiles are hidden and the others centred (the zones formula, ADR-0018). The central card shows the title panel: small line « Pièce n/N », main line the room's name (« Pièce n » if it has none); the rotator does not own the card in HA mode.
- **Swipe in HA mode** goes to the next / previous page in the weather order, empty rooms included (small line « Pièce n/5 », main line « Aucun appareil »); the forecast layers stay hidden; the pagination dots follow. Entering HA mode on a page without devices jumps to the nearest room that has some. *(Update 2026-09-28: empty rooms used to be skipped; with a single configured room the swipe did nothing, which read as broken on the author's tablet.)*
- Leaving HA mode shows the weather of the current page. The « HA » button gets a blue border while HA mode is on, like the voice « Domo » button; its icon keeps showing whether Home Assistant is connected. « Aller à l'écran → Accueil » leaves HA mode. No device at all: the « HA » button is hidden.
- State lines (`tr()`): `lum` « 60 % » (dimmable, on) / « Allumé » / « Éteint » ; `int` « Allumé » / « Éteint » ; `vol` « Mouvement » / « 45 % » (partly open) / « Ouvert » / « Fermé » ; `med` « Lecture » / « Pause » / « Allumé » / « Éteint » ; `act` « Lancer », then « OK » for 1 s ; `cap` the value and its unit ; `bin` by device class (« Ouvert »/« Fermé », « Détecté »/« Rien », « Présent »/« Absent », « Verrouillé »/« Ouvert », else « Actif »/« Inactif ») ; `cli` the room temperature. `unavailable`/`unknown`: « Hors ligne », greyed.
- **Shutter direction** (update 2026-09-28, as in 3.1): each `vol` tile keeps a chosen direction; touching the tile's title (weather tab or HA card name) flips it, the right shoulder shows it (arrow up / down, pause while moving) and the HA card's state line says « Ouvrir » / « Fermer » for 2 s. At the end of a course the direction flips by itself (closed → open, fully open → close); stopped half-way (position 1-99, or -1 for the simulated shutter's « Partiel ») it stays.
- Colours are `UIColor` tokens (ADR-0004), except a light's own colour, lightened when too dark for the background.

### Compatibility

- **New firmware, 3.x blueprint** (the firmware is updated from HA before the blueprint is re-imported): until a first `tab5_maj_tuiles` has been received (flag kept in NVS), room 0 is built from the 3.x slots — `t00` PC/TV, `t01` shutter, `t02..t04` `lumiere_1..3`, with the 3.x names, icons and behaviours (PC/TV shoulder, remote on long press, 3-lamp popup) — and the tiles send the 3.x commands (`pc / basculer`, `lumiere_1 / basculer`, `volet / ouvrir`…). The screen looks as in 3.1, only redrawn by the new code.
- **New blueprint, 3.x firmware**: the blueprint reads the tablet's `sw_version` (`device_attr(…, 'sw_version')`, the project version since 3.0); below `3.2.0` it never calls `tab5_maj_tuiles` (a missing action is an error `continue_on_error` does not catch) and pushes the 3.x keys only. The firmware's default version becomes `3.2.0-dev`, so a self-built firmware is recognised.
- The 3.x inputs (`lumiere_1..3`, `pc`, `volet`) stay in the blueprint, in a folded section; while room 1 (`R` 0) is empty in the new inputs, it is built from them, and the PC tile keeps its PC + TV behaviour.

### Persistence

The definitions (rooms, types, icons, options, names) are kept in NVS when they change (magic `TUI1`, written only if different), and loaded where the zones are loaded (ADR-0018), so the screen draws the rooms before HA answers; the states are not kept (greyed « -- » until the first push).

### Icons: one palette, generated

`Tab5/tuiles_icones.yaml` lists the palette (code, glyph when off / on, the `mdi:` names it stands for, default for which domain / device class). `tools/gen_tuiles_icones.py` writes the C++ table `Tab5/tab5_tuiles_icones.h` (`tuile_icone(code, active, type)`), the glyph lists of the fonts the tiles use (between `# >>> tuiles` / `# <<< tuiles` markers in `Tab5/tab5-styles.yaml`) and the two maps of the blueprint (`icones_mdi`: `mdi:` name → code; `icones_defaut`: domain / device class → code), between markers; `--check` fails when a generated part is stale. The blueprint picks: the icon chosen in the blueprint's customisation, else the entity's `icon` attribute, else its device class, else its domain; a `mdi:` name outside the palette falls back to the default of its domain.

### Blueprint inputs

Per room `n` = 1..5 (section « Pièce n — … », room 1 open, the others folded): `piece_n_nom` (text, optional: empty = the area shared by its devices, else « Pièce n ») and `piece_n_tuiles` (entity, `multiple`, `reorder`, filtered on the domains above; the first five are used, in order). One optional `personnalisation` (object, `multiple`: `entite`, `nom`, `icone`, `comportement` among normal / on only / confirm / read only). The other inputs (TV and remote for the « TV » button, phone, temperatures, plants, climate, planning, tablet name) are unchanged. A tile's name: the customised one, else the entity's `friendly_name` without the room's name, else its `friendly_name`.

## Consequences

- The screen adapts to what the user picks and to how many: 0 to 25 devices, grouped by room, named and drawn as in HA.
- The palette is finite (a few dozen icons, about 200-600 bytes each per font size): an icon outside it shows its domain's default. Adding one = one line in `Tab5/tuiles_icones.yaml` and a release.
- More text comes from HA: names outside Latin-1 lose their unsupported characters (the fonts are Latin-1 + cp1252).
- The light popup lists the room's lights (up to 5) instead of three fixed ones; « Tout éteindre » acts on the room.
- Tests: the firmware's keys, types, options and commands against the blueprint's (`tests/test_emplacements.py`, new tests for the tiles and the palette), the demo scenes and the host rendering capture every room in HA mode.
- Released as a minor version (3.2.0): both directions stay compatible (above).

## Update — 2026-09-29: every climate tile has its popup

A `cli` tile without `m` used to do nothing on tap. [ADR-0027](0027-climate-per-tile.md): the blueprint pushes its unit's settings (`crRT`) and state (`ceRT`) with the tiles, and the tap opens the climate popup on that unit; its commands go with `emplacement: tRT`, translated by the same « Clim : … » branches as the blueprint's climate.

## Update — 2026-10-05: devices on the forecast become optional

A user found the shoulders above the weather icons unnecessary ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). A device setting, the switch **Tab5 Appareils sur la météo** (`tab5-ha-controls.yaml`, `entity_category: config`, `RESTORE_DEFAULT_ON`), chooses. On (the default), nothing changes. Off, weather mode draws every page as a page without devices: no shoulders, no invisible action button, no shutter-direction flip by the tile's title, no 3.x shutter-direction button. HA mode does not read the setting: the « HA » button still shows the rooms and their cards, and a card's title still flips its shutter. The switch takes effect at once (`tuiles_appareils_meteo()`, `tab5_tuiles.cpp`) and survives reboots; the themes (ADR-0029) repaint through the same path, so they keep it. The setting is the tablet's, not the blueprint's: HA keeps pushing the states either way.

## Update — 2026-10-05: the shutter popup (long press)

Asked in [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) (« the level of the blinds should be in a popup window, and the buttons too »). The long press of a `vol` tile used to send the other one of `ouvrir` / `fermer`, with nothing on screen saying where the shutter stood.

- **Gesture.** The long press of a `vol` tile (weather shoulders or HA-mode card) opens the **shutter popup** on that tile. With option `k` it keeps the old long press (the other direction, confirmed by a second press): the popup never bypasses a confirmation. Option `r`: nothing, as before. Legacy mode (3.x shutter): unchanged. The tap and the title flip are unchanged.
- **Popup** (`ui_components/volet_popup.yaml`, `tab5_tuiles.cpp`, registered as « Volet », shared chrome of [ADR-0009](0009-modal-shell-header.md)): title = the tile's name; left card, the position in large digits (« 45 % ») and the state in words below; right card, **Ouvrir / Stop / Fermer**, which send the tile's own `ouvrir` / `arreter` / `fermer`. A 0-100 % slider shows only when the position is known (a value 0-100 with an online state): it sends **`position`** with the value **on release only**, never while dragging. Unknown position (`nan`: a shutter that does not report one; `-1`: the simulated shutter's « Partiel ») shows the state alone, in words: « Ouvert », « Fermé », « Partiel » (stopped half-way), « En mouvement », « Hors ligne ». The popup follows its tile's pushes while it is open (never under the finger).
- **Home Assistant.** The blueprint's « Tuile : position » branch calls `cover.set_cover_position` / `valve.set_valve_position` on the entity of **that tile only** (the same whitelist as the other tile commands, behind the same origin guard), when the entity can set a position (`supported_features` bit 4, `SET_POSITION`), with the value clamped to 0-100. The simulated shutter of `optionnel/volet_serre_tracking.yaml` has no real position: its states now push `nan` at both ends of the course instead of 100 / 0 (the arrow flips the same way), so the tablet gives it no slider, and the branch skips it.
- **Compatibility.** An older blueprint has no branch for `position`: the slider then does nothing (the buttons still work). An older firmware never sends it.

## Update — 2026-10-06: a drawn shutter instead of the slider

Asked in [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) (« maybe like the HA animation, not a basic slider; buttons can be like HA dashboard buttons »).

- **Drawn shutter.** The slider of the left card is replaced by a window (`volet_cadre` → `volet_fenetre` → `volet_tablier`, `volet_popup.yaml`) whose slatted curtain sits at `y = −position × 456 / 100` and is clipped by the window: at 100 % it is back in the box, at 0 % it covers the glass. The 12 slats are built once in C++ (`lames_construire()`, one shared `lv_style_t` coloured from the active palette, so a theme repaints them). The large « 45 % » and the state in words move to the right of the drawing.
- **Same contract.** Dragging the window (up = open, down = close, relative to where the finger landed) moves the curtain and the number; the release sends the same **`position`** 0-100 on the same path (`esphome.tab5_action`, tile key `tRT`, the blueprint's « Tuile : position » branch). The sent position stays drawn until Home Assistant pushes the tile's next state (a theme or definitions repaint keeps it). Nothing is sent while dragging, nor for a touch under 12 px, nor when the position is unknown at press or at release. No API service or variable changes.
- **Follows Home Assistant.** Each pushed position redraws the curtain at once — that is the « animation »: the drawing goes down as the real shutter does, push after push. No interpolation nor fade (the author's preference: instant transitions). Never under the finger.
- **Unknown position** (`nan`, `-1`, offline, nothing received): no drag, no number; the drawing shows the state — `open` up, `closed` down, anything else half-way with faded slats.
- **Buttons.** Ouvrir / Stop / Fermer keep the shared glass button (`style_clim_btn`, theme shapes and press effect); their icon sits in a round badge tinted with `TEXT_SOFT` at 14 %, like a Home Assistant tile.

## Update — 2026-10-06: the device popup (long press)

Asked in [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) (« buttons can have pop up screen like ha dashboard »). The long press of an `int` or `act` tile, or of a `med` tile without `t`, used to do nothing.

- **Gesture.** That long press (weather shoulders or HA-mode card) opens the **device popup** on that tile, the « more info » window of a Home Assistant dashboard. A `med` with `t` keeps the TV remote; option `r`: nothing, as before (no tap at all); legacy mode: unchanged. The tap is unchanged.
- **Popup** (`ui_components/appareil_popup.yaml`, `tab5_tuiles.cpp`, registered as « Appareil », shared chrome of [ADR-0009](0009-modal-shell-header.md)): title = the tile's name; left card, the tile's icon in a round badge of its state's colour, the state in words (the HA-mode card's line; « Prêt » / « En cours » for an `act`), the room, and the tile's options (`o`, `k`) in words; right card, one large vertical switch (filled at the top and coloured when on, at the bottom and grey when off, filled for an `act`) and, under it, what a press does (« Allumer », « Éteindre », « Lancer »).
- **Command.** The large switch runs **the tile's own tap**, through the same function (`tuile_appui_piece`): `basculer` (`allumer` with `o`) or `lancer`, the same confirmation with `k` (the first press arms the tile's 3 s timer; the tile and the popup both ask « Confirmer ? »; a second press sends), the same « OK » after `lancer`. No new command, no change to the contract or to Home Assistant.
- **Data.** Only what HA already pushes for the tiles (definition and state). The time of the last change, the attributes, the history are not pushed: not shown.

## Update — 2026-10-06: HA-mode cards drawn like Home Assistant's « tile » card

Same discussion (« buttons can be like ha dashboard buttons »). The five HA-mode cards lose their title and state tabs and look like the vertical « tile » card of a Home Assistant dashboard: the icon (palette, 70 px) at full strength in a round badge of the same colour at 20 % (`sw_pastille_N`, coloured by `peindre_carte` from the colour the card already used), the name under it, the state line under the name in its colour. Same widgets otherwise (`icon_swN`, `lbl_swN_title`, `lbl_swN_state`, the 130 × 130 invisible button now over the badge, the name's frame still the target of the shutter-direction tap), same colours and words, same 230 × 275 cards and centring. The weather tiles and their shoulders are unchanged.
