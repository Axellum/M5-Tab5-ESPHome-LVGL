# ADR-0010: Microphone and speaker share one I2S bus — every local sound relays the mic itself

**Status:** Accepted (confirmed on the device, 2026-08-05)
**Date:** 2026-09-06 (written retroactively from the incident and the `[I2S-BUS]` comments in `Tab5/tab5-alarm.yaml` / `tab5-hardware.yaml`)

## Context

On the Tab5 the ES7210 microphone ADC and the ES8388 speaker DAC hang off a **single** `i2s_audio` bus (`mic_bus`, GPIO27/29/30 in `tab5-hardware.yaml`). ESPHome protects that bus with one mutex (`i2s_audio.h`, `Mutex lock_`): the first component that starts holds it, the other one fails with `Parent bus is busy` and retries once per second. microWakeWord listens permanently when "Ok Nabu" is on — the user's default state — so the microphone normally owns the bus.

The `media_player` already handles this for TTS: `on_announcement` stops microWakeWord, `on_idle` restarts it. Anything that plays sound **outside** the media player does not get that relay. The alarm clock's `rtttl:` melody writes to `tab5_speaker` directly (deliberately: it must ring without Home Assistant), and on 2026-08-05 it was completely silent — one `Parent bus is busy` per second, speaker stuck in `STATE_STARTING`. Cutting the wake-word switch stopped the errors at the same second; with the mic off the melody played and the *microphone* failed instead. Mutual exclusion demonstrated in both directions.

## Decision

1. Any audio path outside the media player performs the mic ↔ speaker relay itself, per sound: `micro_wake_word.stop`, wait until `!microphone.is_capturing` (the mutex is only released in `stop_driver_()`, not when the action returns), a 250 ms breath (`is_capturing` drops in `STOPPING`, a few ms before the mutex is freed), play, wait until `!speaker.is_playing`, then `micro_wake_word.start`. This is what `tab5_alarm_ring_loop` and `tab5_alarm_preview` do.
2. The relay is **unconditional** — never "only if the mic is capturing": microWakeWord start/stop orders are deferred by one loop iteration (`pending_start_` / `pending_stop_`), so testing the mic state at that instant is a coin toss against a command already in flight.
3. `media_player.on_idle` does **not** restart microWakeWord while `alarm_ringing` is true: the `media_player.stop` issued at ring start would otherwise reopen the mic behind the loop's back and mute the melody for good.
4. A 10 s `interval:` reconciles the invariant "wake-word switch on ⇒ microWakeWord running" whenever nothing is playing (no ring, no assistant, no announcement, speaker idle) — a caller interrupted mid-relay (`tab5_alarm_preview` is `mode: restart`) would otherwise leave "Ok Nabu" dead until the next TTS.

## Consequences

- "Stop the alarm by voice" can only work during the **3 s silence window** between two melody passes: while the melody plays the microphone has no bus at all. Shortening that window means revisiting the voice stop.
- The ring loop is slower to start than a naive `rtttl.play` (stop + wait + 250 ms), accepted.
- The games' `sfx_*()` functions are stubs for the same reason: a local beep would need this whole relay. Any future local sound (game SFX, UI clicks) must go through it — there is no cheap "just play a sample" on this board while the wake word is armed.
- The speaker's `on_idle`-based restart of the mic is the only path that restores "Ok Nabu" after TTS; the alarm cleanup restores it explicitly in both directions (start if the switch is on, stop otherwise).
