# ADR-0045: Three settings — charging mode, animations, eco Wi-Fi

**Status:** Accepted (2026-10-09, asked for by the author; not tried on a tablet when written: the effect of the quick charge and of the Wi-Fi power save is not measured).
**Date:** 2026-10-09

## Context

The author asked for three choices in the Settings popup and in Home Assistant:

- **Charging mode** « Classique / Rapide ». The battery charger of the Tab5 has a quick-charge input, `nCHG_QC_EN`, wired to pin P5 of the second I/O expander (`quick_charge`, `tab5-sensors-diagnostics.yaml`, active low, kept off since the first firmware). The charger itself (`CHG_EN`, P7) has been driven since 2026-10-08 by one interval, `chargeur_pas()` (`tab5_batterie.h`): off without a battery, off during the 2 s probe, paused at 80 % when the « 80 % » limit is chosen.
- **Animations** « Complètes / Essentielles / Aucune ». Since 2026-10-06 the energy saving mode cut every animation (`animations_reduites(true)`); there was no way to cut them without it.
- **Eco Wi-Fi** « Jamais / Sur batterie / Toujours ». The firmware sets `power_save_mode: NONE`: the radio listens all the time, for the lowest latency of the pushes from Home Assistant and of the voice stream.

Constraints: the defaults keep the behaviour of the previous firmware; no new service variable (`contrat/contrat.yaml` unchanged); the logic is pure and tested on a PC (`tab5_batterie.h`, `tab5_economie.h`, `tools/test_alarm_clock.cpp`); one decision point per setting; the screen pages are not redesigned (another lot may refactor them).

## Sources read

- **Quick charge.** M5Unified, `src/utility/Power_Class.inl`, `setChargeCurrent()` for the Tab5 (commit `b926d64`, 2026-10-02): 0 mA = P7 low, P5 high; « 500 mA » = P7 high, P5 high (quick charge off); « 1000 mA » = P7 high, P5 low (quick charge on). The same library writes `OUT_SET` = `0b10000001` at start (P7 high, P5 low). **The 500 mA and 1000 mA figures are M5Unified's names; nobody measured them on this tablet.**
- **Wi-Fi power save.** ESPHome 2026.9.0 (`wifi_component.h`, `wifi_component_esp_idf.cpp`): `set_power_save_mode()` is public and only stores the mode; the private `wifi_apply_power_save_()` calls `esp_wifi_set_ps()` at every `WIFI_EVENT_STA_START`. LIGHT = `WIFI_PS_MIN_MODEM`, HIGH = `WIFI_PS_MAX_MODEM`. `request_high_performance()` exists only with `USE_WIFI_RUNTIME_POWER_SAVE`, which this build does not define.
- **ESP-Hosted.** In the build (esp_hosted 2.12.12), `esp_wifi_set_ps()` on the P4 is `esp_wifi_remote_set_ps()` → `rpc_wifi_set_ps()`, a synchronous request with a 5 s timeout (`DEFAULT_RPC_RSP_TIMEOUT`); the C6 calls its own `esp_wifi_set_ps()` (`slave_wifi_std.c`). The C6 of the author's tablet reports 1.4.1 (`Tab5 C6 version`, read in Home Assistant). **Whether that C6 software honours `WIFI_PS_MIN_MODEM` is not verified.** A serial capture of three boots (2026-10-07) shows no RPC error for the `NONE` request ESPHome already sends at every station start.

## Decision

- **Charging mode.** A select « Tab5 Mode de charge » (Classique by default, kept in NVS, index = `enum ModeCharge`). `charge_rapide_voulue(mode, chargeur_allume)` (`tab5_batterie.h`) allows the quick charge only while the charger is on; it is applied by the same 1 s interval that drives `CHG_EN`, right after it. So « Rapide » has no effect without a battery, during a probe or in the 80 % pause. `quick_charge` keeps `ALWAYS_OFF` at boot. `tests/test_batterie.py` checks that nothing else switches it.
- **Animations.** A select « Tab5 Animations » (Complètes by default, kept in NVS, index = `enum ChoixAnimations`), in `tab5-economie.yaml`. **One decision point:** `economie_decider()` returns the level, `animations_effectives(choix, economie_active)`; **energy saving on forces « Aucune »** (what it did before, so the energy saving mode loses nothing). `tab5_anim.cpp` receives it through `animations_niveau()`:
  - « Complètes »: everything as before.
  - « Essentielles »: only the rotation of the central card (`transition_widgets()`, which also carries the row under the clock in step with it). Page swipes, alert entrance, cross-fades, icon roll-in and the clock roller are placed at their final state.
  - « Aucune »: everything instant, the central card included.

  Popups and pages already open with no fade (product preference); this setting does not touch them.
- **Eco Wi-Fi.** A select « Tab5 Wi-Fi éco » (Jamais by default, kept in NVS), same options and same « on battery » rule as « Tab5 Économie d'énergie » (`economie_active()`), but **never during a stream**: voice assistant running or listening continuously, media player playing or announcing, OTA in progress (`wifi_eco_voulu()`, `tab5_economie.h`). A script run every second and on a change of the select calls `wifi_eco_appliquer()` (`tab5_console.cpp`):
  - it stores the mode in ESPHome (`set_power_save_mode(LIGHT / NONE)`), so ESPHome applies it again by itself after a reconnection;
  - while connected, it sends `esp_wifi_set_ps(WIFI_PS_MIN_MODEM / WIFI_PS_NONE)` **once per change of the decision**, success or not, and logs the result (tag `tab5.wifi`);
  - only the light mode (`MIN_MODEM`) is offered: the smallest step away from the current behaviour.
- **Settings popup.** Three rows, without a new page: Animations next to Night on the Appearance page; Charging mode next to Charge limit, and Eco Wi-Fi under Energy saving, on the Battery page. The rows share `reglages_rangee.yaml` (new optional `x`) and `reglages_choix_btn.yaml`. To make room, the explanatory sentence under the charge limit (« 80 %: charging stops at 80 % and resumes at 70 % ») was removed from the screen; it stays on the Home Assistant dashboard.
- **Home Assistant dashboard.** The three selects get a tile with a short subtitle (`tab5_dashboard.jinja`), as the other settings.

## Trade-offs of the eco Wi-Fi

- **Latency (not verified).** In `WIFI_PS_MIN_MODEM` the station wakes at every DTIM beacon of the access point. A frame from Home Assistant can wait up to one DTIM interval, which the router sets (not read on the author's router, not measured here). A push, a tap's event answer or a ping can feel slower. Nothing was measured on this tablet, neither the latency nor the current saved.
- **Voice and sound.** The decision is « off » while the voice pipeline or the media player runs, so the audio streams are not throttled. A stream that starts while eco is on waits at most one second (the script period) plus one RPC before the radio is back to full listening.
- **API connections.** The ESPHome API has 8 connection slots. Power save does not close connections, but a slower answer to keep-alive pings could make a client time out sooner on a poor network (not observed, not verified).
- **The RPC to the C6 is synchronous** (up to 5 s if the C6 does not answer): one request per decision change, never in a loop, never while disconnected.
- **Default « Jamais ».** Nothing changes for someone who does not choose it; the author can try « Toujours » and judge on the screen and in the logs (`tab5.wifi`).

## Rejected

- **A « Tab5 Charge rapide » switch.** The author asked for a mode; a select also leaves room for another mode at the end of the list without changing the NVS index.
- **Quick charge on whenever the user chose it, charger on or off.** P5 means nothing with the charger off, and a single place (the charger interval) keeps the two pins coherent.
- **Energy saving keeping the chosen animation level.** It would make the energy saving mode weaker than before; the user who wants animations on battery can choose « Jamais » for energy saving.
- **A fifth page in Settings.** The pages are being reworked in another lot; three rows fit in the existing pages.
- **`WIFI_PS_MAX_MODEM`** (ESPHome's HIGH) and a `listen_interval`: a bigger step from the current behaviour, with nothing measured to justify it.

## Consequences

- Three new config entities in Home Assistant: `select.*_tab5_mode_de_charge`, `select.*_tab5_animations`, `select.*_tab5_wi_fi_eco`.
- `animations_reduites(bool)` is replaced by `animations_niveau(ChoixAnimations)`; `DecisionEconomie::animations` replaces `animations_reduites`.
- To check on the tablet: the « Charge rapide allumee / arretee » and « Wi-Fi eco actif / coupe » log lines (tags `tab5.batterie`, `tab5.wifi`), and a refusal from the C6 in a `W` line. The current drawn and the latency stay to be measured.
