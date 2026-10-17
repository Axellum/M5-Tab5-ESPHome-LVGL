# ADR-0064: Screen off means a black panel, and a night window set from Home Assistant

**Status:** Accepted (2026-10-16; the panel sleep of the Waveshare 7B not yet seen on the board when written).
**Date:** 2026-10-16

## Context

The « anti-casse » audit of 2026-10-16 looked for what wears the hardware out. An IPS LCD does not burn in like an OLED, but a still image held for hours leaves image retention, and panel makers advise turning the screen off at quiet hours and not driving the backlight at full power. The firmware did the opposite:

- **« Screen off » kept the image.** Turning the `backlight` light off set the backlight to 0 and paused LVGL; the panel kept showing the last frame. On the Waveshare 7B the backlight is dimmed in analog (AP3032 feedback pin) and never goes fully dark: the frozen image stayed visible (seen by the author, and on the Home Assistant forum).
- **Nothing turned the screen off at night.** « Tab5 Extinction auto de l'écran » is « Jamais » by default, for good reasons (discussion #278: nothing changes for someone who picks nothing).
- **First start at 100 %**, the light's default.

ESPHome 2026.9.1 sends no DCS command after the panel's init, and `mipi_dsi::MipiDsi` is `final` with its command channel protected.

## Decision

- **A black veil, then DCS commands the panel supports** (`Tab5/ecran/tab5_dalle.cpp`, called by the light and by the `ota:` blocks):
    - off: a full-screen black object on LVGL's system layer, drawn at once (`lv_refr_now`), then « display off » (0x28) and « sleep in » (0x10) when the panel takes them, then `lvgl.pause`;
    - on: « sleep out » (0x11), 120 ms (DCS timing, script `tab5_dalle_rallumer`, nothing if the light went off meanwhile), « display on » (0x29), veil hidden, `lvgl.resume` redraws everything;
    - which commands: `carte_dalle_extinction` (0 veil only, 1 + display off, 2 + sleep in) per board and per Tab5 panel revision, passed as `-DTAB5_DALLE_EXTINCTION`. 7B (EK79007, separate GT911 touch): 2. The three Tab5 panels: 0 until a tablet of that revision has tried more (the author's is broken); ST7123 and ST7121 are touch-and-display chips, where « sleep in » would likely stop the touch that must wake the screen.
    - The command channel is read through an explicit template instantiation (access checks do not apply there), as PR #487 did: no copy or patch of the component.
- **The 7B also blacks out during a firmware update** (`carte_dalle_noire_ota`: the flash writes made the panel flicker), and wakes it if the update fails. The Tab5 has no such flicker: nothing changes there.
- **A night window chosen in Home Assistant**, held by the firmware: the « Tab5 · extinction de nuit » list (`packages/tab5_reglages.yaml`, `HH:MM-HH:MM` or « Aucune ») and its mirror « Tab5 · nuit de l'écran » drive the tablet's « Tab5 Nuit (écran éteint) » switch (« Tab5 — extinction de nuit », `packages/tab5_push.yaml`). While it is on, the existing 10 s auto-off check uses 1 min, whatever the select and the « battery only » switch say, with the same exceptions (alarm, voice, game, update); a screen turned off by the night comes back on when the night ends. The firmware applies it because it alone knows the last touch and those exceptions; Home Assistant only knows the clock and the user's choice.
- **Proposed by default without changing anyone's chosen behaviour:** a list never set reads 00:00-06:00, except when a presence sensor is chosen (it already turns off the screen of an empty room): then « Aucune ».
- **First start at 75 %** (`initial_state` of the light): a brightness already chosen is kept.

## Consequences

- An off screen is black on both boards, on every panel, whatever the backlight does; on the 7B the panel sleeps (to be confirmed on the board: glow at 0 %, wake on touch, after an OTA).
- Turning the screen on costs a full LVGL redraw (it already did after `lvgl.resume`) and, on a sleeping panel, 120 ms more before the image.
- Enabling sleep on a Tab5 panel is one substitution in its `ecran-*.yaml`, after a try on that revision; a panel that does not wake would stay black until a restart.
- Without Home Assistant there is no night (the switch starts off). Like the theme's night switch, the automation follows one tablet (`sensor.tab5_tablette`, ADR-0063).
- Rejected: changing the « Extinction auto » default (it would change every tablet already on « Jamais »); computing the night window on the tablet (a schedule to type at compile time, against « set from Home Assistant, with the mouse »); a pixel shift every hour (one full invalidation an hour, for a gain the night already gives).
