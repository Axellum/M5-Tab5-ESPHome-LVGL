# ADR-0051: The zone left of the clock shows a content chosen in the blueprint — the voice controls or a chart of the coming hours, the next one by a tap on the second temperature

**Status:** Accepted (2026-10-10, asked for by the author; not tried on a tablet when written).
**Date:** 2026-10-10

## Context

On the home page, the column left of the clock (x 20–425, above the « Ok Nabu » frame at y 236, [ADR-0041](0041-ok-nabu-panel-scrolling.md)) always showed the voice controls: the microphone (`icon_mic_status`, tap to talk, long press for the assistant popup) and the Domo / Discu buttons (hidden without a discussion pipeline, zone `discussion`).

The author asked (2026-10-10) to choose what this zone shows: the voice controls, an audio player, or a chart — « very beautiful, light » — and to handle it like the other zones (« Ok Nabu » panel, row under the clock, − / + tile): the next content by a tap on a given area of the screen (for now the second temperature, whose tap opened the Arcade), and the same action among the gesture choices.

A music player is being built at the same time by another lot (ADR-0050, popup and mini bar, not merged when this was written).

## Decision

- **Two contents now, a third reserved.** `ZoneGauche` (`Tab5/socle/tab5_parse.h`, pure C++): `VOCAL` (the screen from before), `GRAPHIQUE` (the chart below), `LECTEUR` (the compact audio player, reserved for a second lot built on ADR-0050's data). The firmware reads, keeps and **skips** `lecteur` as long as `disponible()` (`tab5_zone_gauche.cpp`) does not know it; the blueprint accepts the code (`codes_gauche`) but does not offer it yet, so nobody can pick a content that shows nothing. The second lot adds one case in `disponible()`, its widgets and the option; `tests/test_zone_gauche.py` keeps the options equal to what the screen can show.
- **The voice controls do not change.** The four widgets go into one transparent container, `zone_vocal`, at the corner of the screen, without padding or border: their screen coordinates, look, gestures and the `discussion` masking stay the same (the container is hidden or shown as a whole; the per-widget hidden flags are untouched).
- **The chart: the coming hours, from data already on the tablet.** `cal_heures_data` (15 slots, pushed by `tab5_maj_previsions_heures_bulk`; nothing new from Home Assistant, contract unchanged): the monotone curve of the temperatures (the Weather popup's own smoothing, moved to `ui_courbe_lisse()`, [ADR-0043](0043-weather-popup.md)) in the colour of their mean, a dot and its value at the warmest and the coldest hour (one only when flat), rain bars in mm in the rain colours, three hour labels (now, then every 4 slots) and the rain total on the same line. About 30 objects (`lv_line` + `lv_obj`: `lv_chart` is disabled), built at its first appearance, repainted only when new hourly forecasts arrive or the theme changes — at once if shown, at its next appearance otherwise. Colours from the palette (`UIColor`, no new role), fonts already compiled (`roboto_22`), no new screen text (« En attente de Home Assistant » exists). The card is `zone_graphique` (405 × 184 at x 20, y 42: 8 px under the status bar's ink, 10 px above the « Ok Nabu » frame), in the glass of the column's buttons (`style_clim_btn_page`); its sides keep 22 px for a pill-shaped frame (Capsule theme). A tap on it opens the Weather popup.
- **The next content: the second temperature's tap, and a gesture.** `btn_serre_games` (its id kept: docs, tests and the render cite it) now calls `zone_gauche_suivante()` on a tap; its long press (history, [ADR-0032](0032-temperature-history-popup.md)) does not change. Without a second sensor, the icon at its place was a gamepad (« the Arcade is here »); it is now `view-carousel` (U+F056C), the icon of the new gesture code `zone_gauche_suivante` (index 23, at the end of `kCodesGestes`: NVS keeps the index; `mdi_font_26`, `mdi_font_45`, `mdi_font_70`), selectable on any of the twelve home gestures ([ADR-0039](0039-gestes-accueil.md)). The Arcade stays one touch away: the tap of the gamepad button (« auto ») and « Jeux » in the navigation wheel ([ADR-0042](0042-navigation-wheel.md)).
- **Chosen in the blueprint, like the other zones.** A section « Zone à gauche de l'horloge » (`zone_gauche`): the content at start and the contents the tap goes through (a multiple select). One key of `tab5_maj_emplacements`, `gauche|start|c1|c2…;`, pushed with the gestures (all states: connection, reload, « MAJ Écran », HA start); an older firmware ignores it. Read by `zone_gauche_lire()` (`tab5_parse.cpp`; cases in `tools/test_parse.cpp`, called by the libFuzzer harness): an unknown start is `vocal`, unknown codes are left out, the start is always in the cycle (alone, the tap changes nothing). Without the key (a blueprint from before): `vocal` at start, `vocal` and `graphique` on tap — the screen from before until the first tap.
- **Kept in NVS** (« zgau », « ZGA1 »: current, start, cycle), so the content shown survives a restart. A new start from the blueprint shows at once; the same choices pushed again change nothing; a current content no longer offered falls back to the start. The order of the cycle is the enum's (vocal, chart, player).
- **Instant change, no animation** (the author's preference).

## Rejected

- **Building the player in this lot**: the other lot is writing the player (state, commands, popup) right now; a second player here would be a copy to merge later (rule 5). The compact player will read its data.
- **Leaving `lecteur` out of the enum until the second lot**: the NVS and the key would then need a second format; reserving it now costs one value and one code.
- **Offering `lecteur` in the blueprint already**: a choice that shows nothing on the screen.
- **A service variable or an event**: a key of `tab5_maj_emplacements` is what the gestures, `defil` and the solar icon already use; the contract between versions does not change (`contrat/contrat.yaml` untouched).
- **Rain probabilities**: `cal_heures_data` carries the quantity (mm) only; bars in mm, like the Weather popup.
- **Switching back to the voice controls during a voice session**: not asked; with the chart shown, the listening state is not visible on the home page (the assistant popup and the voice reply still are). Left to the author.

## Consequences

- The tap on the second temperature no longer opens the Arcade (`docs/arcade.md`, the user manual and `docs/screens.md` say so); the render opens the Arcade through the gamepad button (`tools/rendu/ecrans.py`) and captures the chart (`accueil-zone-graphique`, and in the Capsule theme).
- A new content: its value at the end of `ZoneGauche` and its code (`tab5_parse.h`), its case in `disponible()` and `zone_gauche_appliquer()`, its option in the blueprint; `tests/test_zone_gauche.py` compares.
- Not tried on a tablet when written: the author judges the look and the touch.
