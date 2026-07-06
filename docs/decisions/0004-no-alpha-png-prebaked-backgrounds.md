# ADR-0004: No PNG alpha channel — pre-baked opaque backgrounds

**Status:** Accepted

## Context

The UI's glassmorphism look would normally use semi-transparent PNGs (icons/cards with an alpha channel) composited over the background at render time — the standard approach on any GPU-backed UI toolkit.

## Decision

PNG assets are pre-baked against the actual background color (`#23262E`) at build/export time and shipped without an alpha channel; the red/blue channel swap needed for the display's pixel format is also done at export time, not at runtime.

## Consequences

- Avoids a firmware crash: rendering true-alpha PNGs was found to crash the firmware on the ESP32-P4 (MIPI-DSI display path), not just a performance concern — this is a hard constraint, not an optimization choice.
- Changing the background color or a card's position now requires re-exporting affected image assets rather than just adjusting an alpha blend at runtime. In practice this pushed the project further toward vector rendering (Material Design Icon fonts, `bpp:1`) instead of raster images wherever possible, which sidesteps the problem entirely for anything that can be an icon rather than a photo/illustration.
