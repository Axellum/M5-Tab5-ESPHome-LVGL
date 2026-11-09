# ADR-0078: The consumption of the day, stacked, on the « Aujourd'hui » page of the Energy popup

**Status:** Proposed (2026-11-09; draft, being written).
**Date:** 2026-11-09

## Context

Lot 4 of the author's plan for the electric cars (sketch B2). The « Aujourd'hui » page ([ADR-0058](0058-energy-sun-forecast-balance.md)) shows the sun's arc, the production hour by hour (solid bars), the forecast (outlined bars), the learned clear-sky curve and the best window; its right column gives the produced, forecast and tomorrow's energy. **It shows no consumption at all** (nothing in ADR-0058 nor in the code: the page reads the hours series of the production, the sun payload, and nothing else). The cars of [ADR-0076](0076-cars-on-the-flux-page.md) and the analysis of [ADR-0074](0074-car-charges-analysis.md) make the question « did the cars charge from the sun? » answerable; this page is where it is answered for the day.

(Details to be completed with the implementation.)
