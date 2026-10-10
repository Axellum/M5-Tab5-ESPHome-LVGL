# ADR-0058: Energy popup in pages — power flow, the sun and the day's forecast learned from the home's own production, and a balance with export and savings

**Status:** Proposed (2026-10-10, the three lots asked by the author; not yet tried on a tablet nor with a real solar installation)
**Date:** 2026-10-10

## Context

The Energy popup (ADR-0028) shows four cards of numbers and the production in bars. The user who asked for it ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) uses Home Assistant's Energy dashboard: a flow between solar, home and grid, and the share of solar in the home. The author asked for more: the sun's course, sunrise and sunset, the hours of strong production learned from the home's own data (so the panels' orientation and the shade count without any setting), the forecast of the coming hours from the weather, and a balance — consumption against production, export to the grid, money saved.

## Decision

### Pages

The popup gets pages (`pages_brancher()`, ADR-0046), opened on the first one that has data:

1. **Flux** — solar, home, grid and battery as circles, joined by lines whose thickness follows the power; a ring on the home split between solar and grid. Static: no running animation (battery cost, author's taste). Needs the instant (`tab5_maj_energie`) with solar picked.
2. **Aujourd'hui** — the sun's arc from sunrise to sunset with the sun at its place (local time between `lever` and `coucher`), solar noon; the hours of the day: production (bars, from the hours series), forecast (outlined bars), the learned clear-sky curve (a line); the best window as a band; produced, forecast for the day, forecast for tomorrow. Needs `tab5_maj_energie_soleil`.
3. **Production** — the current page of ADR-0028 (cards and bars, Hours / Days / Months), unchanged.
4. **Bilan** — per hour, day or month (the same view as Production, the same buttons): production split into used at home / exported, consumption split into solar / grid, the self-consumption rate, the export, the savings, the total of the view. Needs `tab5_maj_energie_bilan`.

A page without its data is not shown; fewer than two pages = no tabs (the brick's rule).

### Two new firmware actions (pure addition: minor version of the contract)

`tab5_maj_energie_soleil(payload)` — one string, fields separated by `|`, lists by `;`:

```
lever|midi|coucher|prevu_jour|prevu_demain|source|creneau_debut|creneau_fin|prevu|clair
```

| Field | Meaning |
|---|---|
| `lever`, `midi`, `coucher` | Local `HH:MM` of today's sunrise, solar noon, sunset (from `sun.sun`). Empty = unknown (no `sun.sun`, polar day or night): no arc. |
| `prevu_jour` | Forecast production of the whole day, kWh. Empty = no forecast, `nan` = forecast picked without a value. |
| `prevu_demain` | Same for tomorrow. |
| `source` | `a` = learned curve × weather (this package); `e` = scaled to an external forecast sensor (Forecast.Solar, Solcast…); empty = none. Shown translated, never as is. |
| `creneau_debut`, `creneau_fin` | Best window of the rest of the day, whole local hours 0–24 (end excluded). Empty = none (night, nothing worth it). |
| `prevu` | 24 values, kWh forecast for each local hour of today, `;`-separated, empty = none. |
| `clair` | 24 values, kWh of the learned clear-sky curve for each local hour, `;`-separated, empty = not learned yet. |

`tab5_maj_energie_bilan(vue, debut, payload)` — `vue` and `debut` as `tab5_maj_energie_historique`; `payload`:

```
devise|vente|achat|gain
```

| Field | Meaning |
|---|---|
| `devise` | Currency symbol, at most 7 bytes (`€`, `$`, `CHF`…), empty = no price picked: no savings shown. |
| `vente` | kWh exported per slot, `;`-separated (24, 30 or 12 values, empty = no data). Empty field = export meter not picked. |
| `achat` | kWh imported per slot, same format. Empty field = import meter not picked. |
| `gain` | Money saved per slot in `devise`: used at home × purchase price + exported × resale price, `;`-separated. Empty = no price. |

Production per slot is the `tab5_maj_energie_historique` series of the same view; used at home = production − export (never below 0); consumption = used at home + import. The firmware computes the rate and the totals; Home Assistant computes the money (prices may change over time and stay on its side).

### Home Assistant side (`packages/tab5_energie.yaml`, blueprint « Tab5 — emplacements »)

- **Sun**: `sun.sun` (`next_rising`, `next_noon`, `next_setting`, read for today's date in local time).
- **Learned clear-sky curve**: the recorder's `hour` statistics of the production meters over the last 30 days; for each local hour, the second highest value (one spike does not set the curve). It carries the panels' orientation and tilt and the shade of the place. Recomputed at most once an hour.
- **Weather**: hourly forecast of the weather entity already used by the tablet (`sensor.tab5_sources_meteo`), `weather.get_forecasts` type `hourly`: forecast(h) = clair(h) × (1 − 0.75 × (cloud/100)^3.4) (Kasten–Czeplak); without `cloud_coverage`, a cloud share per condition (sunny 0, partlycloudy 50, cloudy 90, rainy/pouring/snowy/fog 100…). Without hourly forecast: forecast = clair.
- **External forecast** (optional blueprint inputs: today's and tomorrow's forecast in kWh): when picked, the learned hourly shape is scaled to that total and `source` = `e`.
- **Best window**: from the current hour to sunset, the 3 consecutive hours with the largest forecast sum; none if that sum is under 5 % of the day's learned maximum.
- **Balance**: optional blueprint inputs — import energy meter, export energy meter (the meters the HA Energy dashboard asks for), purchase price and resale price (number per kWh, or an entity that wins when picked), currency symbol (default `€`). Statistics read like the production (5-minute rows for hours, `day`, `month`).
- **Rate**: the sun payload on the first pass, then when the local hour changes; the balance with the history of the view asked for, and the hours again on each pass of the hours view. The hours series is pushed on every first pass whatever the view (the Aujourd'hui page needs it). Nothing while the popup is closed (ADR-0028 still holds).
- Every call of a new action has `continue_on_error: true`: an older firmware lacks the action and Home Assistant would stop the script there.

## Compatibility

- **Older firmware, new HA files**: the two new calls fail and are skipped (`continue_on_error`); the rest works as before.
- **New firmware, older HA files**: neither new action is called; the Flux page (instant only) and Production show; Aujourd'hui and Bilan stay hidden.
- **Blueprint inputs left empty**: no balance meter = no Bilan page; no price = no savings line; no production meter = no learned curve and no forecast.

## Consequences

- Two more actions (contract minor version), four pages in one popup, more blueprint inputs (all optional), more Jinja in one package.
- The learned curve needs some sunny days in the last 30; until then `clair` is empty and the forecast too, said on screen.
- The author has no solar panels: correctness of the learned curve and of the forecast can only be checked by a user with a real installation; tests use simulated statistics and forecasts.
