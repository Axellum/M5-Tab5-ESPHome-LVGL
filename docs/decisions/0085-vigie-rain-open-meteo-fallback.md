# ADR-0085: When Vigie gives no rain, Home Assistant asks Open-Meteo directly

**Status:** Accepted (2026-11-24, the author's choice: « Oui, ajoute-le »).
**Date:** 2026-11-24

## Context

Since [ADR-0084](0084-vigie-default-rain-source.md) the rain sensor of `packages/tab5_meteo_sources.yaml` reads Vigie, the author's open API, when the list is left on Météo-France without a Météo-France rain sensor, and whenever « Vigie » is picked. Vigie runs on one server of the author: when it does not answer, the rain card showed « no data » (`@-1,0`) until the next 5-minute call. Vigie itself does its own fallbacks (Météo-France gateway, its own Open-Meteo instance, the public API), but nothing covered Vigie being unreachable from the user's Home Assistant.

## Decision

- **After the call to Vigie, a second call to Open-Meteo directly** (`rest_command.tab5_pluie` with `source: Open-Meteo`, response `rep_repli`, `continue_on_error`) when, and only when, the effective source is Vigie and Vigie gave nothing usable: no answer (timeout, network error: the response variable is not set), a status other than 200, or neither a non-empty list of 5-min rates nor a non-empty list of 15-min amounts with a `server_utc`. The condition mirrors the reading of Vigie's answer; an empty 5-min list no longer hides the 15-min steps (the reading was made the same).
- **The series is then read from Open-Meteo's answer**, with the Open-Meteo reading that already exists (`src` / `rep` replaced at the top of `serie`): the same series as the Open-Meteo source.
- **The `source` attribute says which source was really read**: « Open-Meteo » when the fallback gave the series (new variable `source_lue`), the effective source otherwise. The state and the bars do not change their logic.
- **No fallback for the other sources** (Buienradar, DWD, Met.no, Open-Meteo, OpenWeatherMap): a user who picks one of them chose its data and its area.
- **The fresh-install CI counts a reading as Vigie's only if the `source` attribute says « Vigie »**: a reading from the fallback while Vigie is silent in Home Assistant's log is « not judged » and the message says that the fallback served; a fallback reading while Vigie answered is a failure (our reading of Vigie to check).

## Consequences

- Outside France, a silent Vigie costs nothing visible: Open-Meteo serves the same model. In France, the card falls back to the global model instead of Météo-France's nowcast for as long as Vigie is down.
- **Privacy**: when Vigie gives nothing, the home location rounded to 0.01° also goes to Open-Meteo, in the same 5 minutes. The documentation says so (`docs/installation/weather.md`).
- At most one more HTTP call every 5 minutes, only while Vigie fails; a silent Vigie and a silent Open-Meteo take up to twice the 15-second timeout before the card says « no data ».
- **Verified**: the condition and the series rendered with jinja2 (`tests/test_pluie_sans_meteo_france.py`: silent Vigie, 503, no data, empty lists, no time, HTML; the other sources never fall back; Open-Meteo down too), and in Home Assistant's own template engine with a real Open-Meteo answer (Tokyo, 2026-11-24). **Not verified**: a real Vigie outage on the author's Home Assistant (Vigie answered during the whole lot).
