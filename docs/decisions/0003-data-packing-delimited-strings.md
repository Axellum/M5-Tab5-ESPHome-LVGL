# ADR-0003: Delimited-string payloads instead of one service call per data point

**Status:** Accepted

## Context

Several pushed datasets are naturally multi-valued: a 15-day forecast (4 fields × 15 days), an hourly rain/temperature chart (15 slots), calendar events (title + time + color × 4). Home Assistant could call one ESPHome API service per field per item, but that's dozens of separate network round-trips per update, competing with the I2S audio stream for the device's limited TCP sockets.

## Decision

HA-side automations serialize each multi-valued push into a single semicolon-delimited string (e.g. `"0;Soleil;27;14;1;Nuageux;24;12;..."`) and send it through one API service call. The device tokenizes it in a single C++ pass (`strtok_r`-based parsing in `tab5_custom.cpp`) and updates all affected widgets atomically.

## Consequences

- One network call instead of N — meaningfully reduces socket/connection pressure, which matters given the 8-connection cap on the ESPHome native API (see the "API connections exhausted" entry in [`docs/troubleshooting.md`](../troubleshooting.md)).
- The payload format is an implicit contract between the HA-side automation and the C++ parser — there's no schema validation beyond a length/OOM guard (buffers capped, e.g. 1024 bytes for the weather-alert payload after a 2026-07-06 fix). A field-count or delimiter mismatch on either side fails silently rather than raising a clear error; changing the payload shape requires updating both sides in the same change.
- Parsing buffers are fixed-size (`snprintf`/`strtok_r` with capped buffers, not `std::string` concatenation) specifically to avoid heap churn in a hot path — see code rule 4 in `Tab5/README.md`.
