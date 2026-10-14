# ADR-0062: Energy balance in kWh — the grid as a battery over the calendar month

**Status:** Proposed (2026-10-13, asked by the author for a user with monthly net metering; not yet tried on a tablet nor with a real solar installation)
**Date:** 2026-10-13

## Context

The « Bilan » page of the Energy popup ([ADR-0058](0058-energy-sun-forecast-balance.md)) needs the grid import or export meter and shows four cards of percentages and money: used at home, sold, bought, savings. A user of [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) has monthly net metering: what the home gives to the grid during a calendar month pays for what it takes during the same month. He has only two meters, the production and the home consumption, no import or export meter and no price. For him the page did not show, and the question it should answer is another one: « am I ahead of the grid this month, and where will I end? ».

## Decision

### The page

The page shows kWh first; money only appears when a price is given.

- **Hero card « Grid battery »** (left), always about the **calendar month** of the last known day:
  - the net of the month (produced − consumed), in the success colour when positive, the warning colour when negative, with « ahead of / behind the grid this month »;
  - a gauge centred on 0, its scale symmetric and rounded (`energie_rond()`);
  - the end-of-month projection at the pace of the 7 days before the last one (that day is not over): net + mean daily net × days left (`energie_mois()`);
  - produced, consumed, the savings when a price is given, and a ring « % of your use covered by the sun » (used at home / consumed).
- **Chart** (right), three buttons Hours / Days / Months, shared with the Production page:
  - **Days**: the days of the calendar month, the net per day as bars (surplus up, deficit down, one zero line), its running total as a smoothed line (`ui_courbe_lisse()`), today pointed with a bubble, the projection to the end of the month as dots;
  - **Hours**: today's hours, the running total of the day;
  - **Months**: the months of the calendar year, the running total from January.
  - Both scales share the zero; the shorter side is at least a quarter of the other (`energie_echelle()`).
- When import and export are estimated (below), the chart says « import / export estimated hour by hour ».

The page shows with the production and at least one of: home consumption, import meter, export meter.

### Contract: three fields at the end of the payload, three calendar views

`tab5_maj_energie_bilan(vue, debut, payload)` keeps its signature (the contract version does not change). The payload gets three fields **at the end**:

```
devise|vente|achat|gain|prod|conso|estime
```

| Field | Meaning |
|---|---|
| `prod` | kWh produced per slot, `;`-separated. Empty = the production series of `tab5_maj_energie_historique` of the same view (older HA). |
| `conso` | kWh consumed by the home per slot. Empty = no home meter: consumed = used at home + import. |
| `estime` | `1` = `vente` and `achat` are estimated by Home Assistant; otherwise measured. |

New `vue` values, calendar windows instead of sliding ones:

| `vue` | `debut` | Slots |
|---|---|---|
| `heures` | the day | 24 hours |
| `mois_civil` | the 1st of the month | one per day of the month (28 to 31) |
| `annee_civile` | January 1st | 12 months |

Slots to come are empty. The firmware still reads `jours` and `mois` (30 days and 12 months, sliding) from an older Home Assistant.

The pure calculations live in `Tab5/socle/tab5_parse.*` and are tested with g++ (`tools/test_parse.cpp`, `tools/test_tab5_socle.cpp`), fuzzed by `tools/fuzz/fuzz_parse.cpp`:

- `energie_bilan_lire()`, `energie_bilan_creneau()` (net = produced − consumed);
- `energie_cumul()`, `energie_mois()`, `energie_echelle()`, `energie_rond()` / `energie_pas()`, `energie_jour_plus()`.

The screen code (`Tab5/ecran/tab5_energie.cpp`) only draws.

### Home Assistant side (`packages/tab5_energie.yaml`, blueprint « Tab5 — emplacements »)

- New optional blueprint input **« Énergie consommée par la maison · Home energy consumption »** (energy sensor, the home meter of HA's Energy dashboard).
- **Estimate.** Without import or export meter, Home Assistant estimates both **hour by hour** from the production and the home consumption:
  - export = max(0, produced − consumed);
  - import = max(0, consumed − produced).

  Summing per hour, not per day, keeps the midday surplus and the night import of a balanced day. Hours come from the 5-minute statistics today and from the `hour` statistics for the past days of the month. An hour counts when the home meter has a value; a missing production is 0, which is the night. The year gets no estimate: it would need the `hour` statistics of the whole year (up to 8 760 rows a meter) in one template. Without meters, its import, export and savings stay empty and it shows produced, consumed and the net.
- **Pushes.** `mois_civil` on every first pass, since the hero card reads it whatever the view. `annee_civile` with the Months view. `heures` on each pass of the Hours view. One action inside a `repeat: for_each`, with `continue_on_error: true`.

## Compatibility

- **New firmware, older HA files**: the old payload (four fields, sliding views) is still read, and the page still needs an import or export meter. The hero card takes the days of the current month found in the 30-day sliding window, and the chart shows the sliding windows.
- **Older firmware, new HA files**: the old firmware refuses `mois_civil` and `annee_civile` (unknown view, the call is skipped by `continue_on_error`) and ignores the extra fields; the Bilan page stays as it was or hidden. **Update the tablet first, then the HA package.**

## Consequences

- One more blueprint input, more Jinja in one package (the hourly estimate), a page that is useful with two meters and no price.
- The four cards of ADR-0058 (used at home, sold, bought, savings as percentages) are gone; savings stay as one line of the hero card when a price is given.
- The projection is a straight line at the recent pace: a sunny week at the start of the month overestimates the end of a month that gets cloudier. It is labelled with its pace (« 7-day pace »).
- The author has no solar panels: the estimate and the page can only be checked by a user with a real installation; tests use simulated statistics.
