# ADR-0084: Vigie is the default rain source when Home Assistant has no Météo-France, everywhere

**Status:** Accepted (2026-11-24, the author's choice: « Oui, Vigie par défaut »).
**Date:** 2026-11-24

## Context

The list « Tab5 · next-hour rain source » starts on Météo-France, so a fresh install picks it. Since 2026-10-02 (discussion #278, a user in Turkey with an empty rain card) the rain sensor of `packages/tab5_meteo_sources.yaml` falls back on its own when that list is left on Météo-France and Home Assistant has no Météo-France rain sensor: until now it called the public Open-Meteo API directly (15-min steps of its model).

Since 2026-11-22 (#636) the list also offers « Vigie », the open API of the same author (`https://vigie.auxfilsdesidees.fr/v1/point`). Vigie answers everywhere: in mainland France with Météo-France (the PIAF nowcast at the GPS point, twelve 5-min rates, `rain_nowcast.rate_5m_mm_h`), elsewhere with the Open-Meteo model (`rain_nowcast.mm_per_step`, four 15-min amounts in mm; Vigie reads it from its own Open-Meteo instance first and the public API as a fallback, Vigie's ADR 0039). The Tab5 side only read the 5-min rates: outside France the « Vigie » choice showed « no data ».

The fresh-install CI tested the fallback through Open-Meteo directly, and GitHub's runners could not reach `api.open-meteo.com` several times (2026-11-10, 11-23, 11-24: TLS handshake timeouts; #638 stopped turning that red).

## Decision

- **The « Vigie » source reads both shapes**: the twelve 5-min rates when Vigie gives them (France), otherwise the four 15-min amounts, turned into mm/h (× 4) and dated like the Open-Meteo source: from the start of the current quarter hour of Vigie's `server_utc`, each value being the rain of the 15 minutes **before** its time (Open-Meteo's own convention, checked live on 2026-11-24). For the same rain, the series — so the state and the bars — is the same as with the Open-Meteo source (`tests/test_pluie_sans_meteo_france.py`).
- **Vigie replaces Open-Meteo as the default effective source**: the list left on Météo-France without a Météo-France rain sensor now gives `source = 'Vigie'`; the sensor's `source` attribute says « Vigie ». The list itself does not change (no new option, no migration), and Météo-France still wins as soon as its integration is added.
- **Open-Meteo stays in the list**, for whoever prefers to call it directly; « None » still sends nothing.
- **The fresh-install CI tests the rain for real through Vigie, then Open-Meteo** (`tools/installation_ha/verifier_installation.py`, same retries and same « not judged » rule as #638 for each host).

## Consequences

- In France without the Météo-France integration, the rain card gets Météo-France's PIAF at the GPS point (5-min steps) instead of a global model: better data with nothing to set. Elsewhere it is the same Open-Meteo model as before, through Vigie.
- **Privacy**: the home location, rounded to 0.01° (about 1 km), goes to the author's server every 5 minutes instead of Open-Meteo's. The documentation says so (`docs/installation/weather.md`); « None » or another source avoids it.
- **One more dependency on the author's own server**: when Vigie does not answer, the card shows « no data » (state `@-1,0`), as it did when Open-Meteo did not answer. There is no fallback to Open-Meteo directly on the Tab5 side; Vigie does its own fallbacks (gateway, self-hosted Open-Meteo, public API).
- Vigie documents `mm_per_step` as « the next hour », while the values are Open-Meteo's sums of the preceding 15 minutes from the current quarter hour: the Tab5 dates them as Open-Meteo does. To be fixed in Vigie's documentation, not here. In France without PIAF, the 15-min steps may come from Vigie's Météo-France gateway; their dating there was not verified.
- **Not verified on the real screen with rain outside France**: the 15-min branch was read on a live Vigie answer (Tokyo, 2026-11-24) and the rain itself was simulated.
