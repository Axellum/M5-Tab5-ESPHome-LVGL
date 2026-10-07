# Troubleshooting — incidents already diagnosed

## English · [Français](#version-française)

---

This is a log of real symptoms hit on this exact device/firmware, with the confirmed root cause and fix. Check here **before** re-diagnosing something that looks familiar — several of these took hours of live debugging with the device on a desk. If you're an AI agent auditing this code, several of these look like bugs at first glance; they are documented here precisely because they were already investigated and closed.

Format: **Symptom → Root cause → Fix**. Entries are chronological, most recent last within each topic isn't guaranteed — read the whole thing, it's short.

---

### Black screen after a software reboot (not a power cycle)

**Symptom:** device reboots (OTA, crash, `api.reboot`), display stays black afterward. A power cycle (unplug/replug) fixes it; a soft reboot alone does not, or does so unreliably.

**Root cause:** the display's `reset_pin` is wired through the `PI4IOE5V6408` I2C GPIO expander, not a native ESP32 GPIO. Right after boot, the expander itself needs a short settle time before it reliably drives its output pins — toggling `reset_pin` too early is a no-op from the display's point of view.

**Fix:** a blocking `delay(1000)` was added in `on_boot: priority: 700` in `tab5-hardware.yaml`, right before the display reset sequence. Confirmed root cause on 2026-07-06 after 5 live reboot tests with the developer watching the screen. This is marked `[AI-WARNING]` in the file — **do not remove this delay** to "optimize boot time" without re-testing across several reboots; a previous permissive `logger: level: VERY_VERBOSE` workaround (kept for weeks as a correlation-only mitigation) was replaced by this confirmed fix.

---

### Weather / planning / climate data missing, but the device is online

**Symptom:** the device shows a healthy Wi-Fi/API connection (diagnostics entities look fine, uptime climbing), but the weather, planning, and forecast cards never update — while the clock and a couple of directly-mirrored sensors keep working, which makes the failure look partial and confusing.

**Root cause:** the push automations on the Home Assistant side are all gated behind one shared `condition: state` on a boolean helper (a "which HA instance is the active one" flag, relevant only in a dual-instance setup). That helper was stuck `off`; a guard automation meant to force it back `on` at every HA boot was itself disabled, so even a full HA restart didn't self-heal.

**Fix:** turn the helper back on, re-enable the guard automation, manually trigger the push automation once. **If your setup pushes conditionally on a shared state flag, make sure the automation that's supposed to re-arm it on boot is actually enabled** — an automation that silently no-ops because its own guard is off is easy to miss for a long time (in this case, ~3 hours before being noticed).

**Since 3.8** the flag (`input_boolean.is_primary_active`), the automation that re-armed it and its health guard are gone: no push depends on a shared flag any more. This incident can no longer happen with the current packages; if the screen still looks frozen, use « MAJ Écran » and check that the push automations are on.

---

### STT/TTS broken, dozens of "entity already exists — ignoring" log lines

**Symptom:** voice pipeline reports a missing/broken speech-to-text provider despite the underlying service responding correctly when tested directly. Home Assistant logs show many `does not generate unique IDs ... already exists - ignoring` warnings across automation/sensor/climate/calendar entities.

**Root cause:** a `remote_homeassistant`-style integration was mirroring an entire second Home Assistant instance's entities into this one. ID collisions silently shadowed the real STT/TTS entities the voice pipeline depended on. This only reproduces in a **dual Home Assistant instance** setup — not relevant if you run a single HA instance.

**Fix:** disable the mirroring integration, full HA restart. If you don't run two HA instances feeding into each other, this can't happen to you.

---

### Touch "not working" but the touchscreen is actually detecting presses fine

**Symptom:** tapping the screen does nothing; looks like a dead touch controller.

**Root cause:** Home Assistant was rejecting the device's service calls (`Service call ... rejected; ... enable this functionality in the options flow`) — the touch events were reaching HA, HA just refused to act on them.

**Fix:** enable "Allow service calls" in the ESPHome integration's options flow for this device in Home Assistant. Check this before assuming a hardware/firmware touch bug.

**Since [ADR-0025](decisions/0025-events-only.md) (after 3.1)** the firmware calls no action at all, so this rejection cannot happen any more: the screen's commands and requests are events. The same symptom on a recent firmware means their consumer is missing — the blueprint « Tab5 — emplacements » for the tiles, `packages/tab5_evenements.yaml` for the calendar, announcements and system console.

---

### A calendar/weather push silently breaks the rest of the same automation

**Symptom:** after fixing the two issues above, planning/weather/climate were *still* not showing up on screen — a distinct bug from the ones above.

**Root cause:** the automation called `calendar.get_events` with `start_date_time: "{{ now() }}"` — a raw Jinja `now()` datetime object including microseconds, which Home Assistant's schema silently rejected. Because the calendar call was one step among several sequential actions in the same automation, the validation failure aborted everything after it — including unrelated weather and forecast pushes further down the same automation.

**Fix:** format the datetime explicitly: `"{{ now().strftime('%Y-%m-%d %H:%M:%S') }}"`. More broadly: add `continue_on_error: true` on individual push actions (calendar, each `weather.get_forecasts` call, each screen-push service call) so one bad payload doesn't take down unrelated pushes later in the same automation, plus `is defined` guards on any template variable consumed downstream.

---

### Daily weather icons gone, hourly forecast fine, automation reports success (2026-09-18)

**Symptom:** the five daily forecast cards lost their weather icons (and kept stale data after a reboot) while the hourly cards kept updating. Home Assistant had all the data: the weather entity was fine and `weather.get_forecasts` returned 15 daily and 67 hourly entries. The push automation showed no error.

**Root cause:** the `tab5_maj_previsions_jours_bulk` payload template looped over `range(15)` and read `fcasts[i].templow` directly. Météo-France stopped sending `templow` for the 15th day (D+14): the key is simply absent from that dict. In Jinja, `dict.missing_key` raises `UndefinedError` **before** `| float(0)` can apply, so the payload was never rendered and the service never called. The hourly cards use another call (`previsions_heures_bulk`), which is why they kept working. `continue_on_error: true` on the action is what hid it: HA only logged `Error rendering data template: UndefinedError: 'dict object' has no attribute 'templow'`.

**Fix:** guarded access everywhere a forecast attribute is read: `fcasts[i].get('templow') | float(0)`, same for `condition`, `temperature` and (hourly) `precipitation`. Applied to production on 2026-09-18 and mirrored into the public example on 2026-09-25 (now `packages/tab5_push.yaml`, the single source since 2026-09-26).

**How to spot the next one:**
- Read the automation **trace** and compare the list of services actually called with the expected ones. A missing `…_bulk` call stands out even though the run is "successful".
- Guard (d) of `HomeAssistant_Config/packages/tab5_health.yaml` (`tab5_health_template_error`) raises an alert on `Error rendering data template`. It needs `system_log: fire_event: true` in `configuration.yaml`, otherwise it loads but never fires.
- To force a full push after a fix, use the **MAJ Écran** button of the device's system console (`automation.trigger` skips the conditions) or *Run actions* in HA. Firing `esphome.tab5_connected` by hand no longer works since 2026-09-25: that trigger only pushes when the API link came up less than 3 min ago.

---

### ESPHome `pressed:` style rejected when placed in a shared `style_definitions`

**Symptom:** `esphome compile` fails with `[pressed] is an invalid option for [style_definitions]` after trying to centralize a button's pressed-state styling.

**Root cause:** confirmed by reading ESPHome's own LVGL component source (`defines.py` / `widgets/__init__.py` / `styles.py`) rather than guessing — the `pressed:` state key is only valid on the widget itself, not inside a reusable `style_definitions:` block.

**Fix:** keep the pressed properties out of `style_definitions`, but share them anyway: put them in a normal style and reference it from the widget with `pressed: { styles: style_x }`. ESPHome accepts that form on a widget and generates `lv_obj_add_style(obj, style_x, LV_STATE_PRESSED)` (verified in `schemas.py` / `widgets/__init__.py` of ESPHome 2026.9, 2026-09-26). The earlier advice "repeat the block on every button, it cannot be shared" was wrong.

**Related:** press/release no longer animate. The default LVGL theme used to fade every button over 80 ms, plus a 70 ms delay on release. `-DLV_THEME_DEFAULT_TRANSITION_TIME=0` (`build_flags` in `tab5-ha-hmi.yaml`) removes those transitions (a `CONFIG_LV_*` line in the sdkconfig has no effect: ESPHome builds LVGL with `LV_KCONFIG_IGNORE`; the first attempt, on 2026-09-26, went there and never applied), and the project's own pressed style (`tab5_anim.cpp`, 94 % scale) is applied instantly.

---

### ESPHome API connections exhausted (device drops connections under otherwise normal use)

**Symptom:** device intermittently fails to accept new API connections (from HA, from `esphome logs`, from OTA) for no obvious reason.

**Root cause:** the ESPHome native API only allows a small, fixed number of simultaneous connections (8). Leftover `esphome` CLI processes from earlier debugging sessions (e.g. `esphome logs` left running in a forgotten terminal) each hold one connection open indefinitely.

**Fix:** close CLI sessions when you're done with them; if connections seem exhausted, check for and kill orphaned local `esphome` processes before assuming a device-side bug.

---

### Device reboots on its own every ~15–20 min while Home Assistant is unreachable

**Symptom:** Home Assistant is down (or slow to start), the Tab5 is still on Wi-Fi and shows its last known state, yet it reboots by itself roughly every 15–20 minutes. Nothing in the logs looks like a crash: each boot is clean and `safe_mode` reports `Boot seems successful`.

**Root cause:** not a crash — `api: reboot_timeout:` in `tab5-api-logic.yaml`. ESPHome reboots the device when **no API client has been connected** for that long, and the counter restarts on every connection, even a brief one. With the original 15 min, an HA server that takes a while to come up produces a reboot cycle slightly longer than the timeout itself (timeout + boot + reconnection attempts). The `wifi:` component has its own separate `reboot_timeout`, which is *not* involved here since Wi-Fi was fine.

**Fix:** raised to `60min` on 2026-08-01 — an hour-long HA outage or maintenance window no longer cycles the tablet, which stays useful without HA (clock, arcade, diagnostics console, last known weather), while the anti-"zombie" safety net from audit F-04 is kept. Do **not** set it to `0s` without re-reading that audit: a frozen API stack would then leave the device online but mute until someone power-cycles it.

---

### Black screen + device off the network after an OTA — the Wi-Fi co-processor never came up

**Symptom:** an OTA reports `OTA successful`, the device reboots, and then: black screen, no Home Assistant entities, no API (`esphome logs --device <ip>` times out on port 6053). Rebooting it again changes nothing. Confusingly, the IP still answers `ping` — that is a *different* device that picked up the lease, exactly like the 2026-08-01 DHCP mix-up.

**Root cause:** the ESP32-P4 has no radio of its own; Wi-Fi comes from the ESP32-C6 co-processor over SDIO (`esp32_hosted`). That link failed to come up after the OTA's *software* reboot. The P4 does reset the C6 through GPIO 15 (`esp32_hosted` · `reset_pin`) on every boot, software reboots included: that is ESP-Hosted's default setting (`CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP=y`, active-high reset, 1,500 ms, read on 2026-10-07 in the `sdkconfig` of an ESPHome 2026.9 build). Cause reviewed on 2026-10-07: this page used to say that a software reboot does not reset the C6, which is very probably wrong for the 2026-08-05 build too. That build came from ESPHome 2026.7 (exact release not recorded); releases 2026.7.0 to 2026.7.3 ask for esp_hosted 2.12.9 without choosing when to reset the C6, and esp_hosted 2.12.9 has the same default. `reset_pin: GPIO15` has been in `Tab5/tab5-hardware.yaml` since the first commit. So the OTA reboot very probably did send its reset pulse to the C6, and that did not clear the fault; only cutting the power did. The exact cause is unknown. Serial console (USB, COM port) makes it unambiguous in seconds:

```
[I][esp-idf:000]: E (93883) H_API: ESP-Hosted link not yet up
[W][wifi_esp32:300]: esp_wifi_set_mode failed: ESP_FAIL
[I][wifi:852]: Starting fallback AP
```

Those three lines repeat ~50 times per second. The black screen is a *consequence*, not the fault: that hot retry loop starves the rest of the firmware, LVGL included. Do not go looking at the display stack.

**Fix:** a **full power cycle** — unplug USB-C for ~15 s, then plug it back in: without a battery, the tablet starts again by itself. On the author's tablet, which has no battery, unplugging was enough: the long-press was not needed. With a battery fitted, unplugging does not cut the power: also switch the tablet off with a long-press. A soft reboot will not do it. Confirmed on 2026-08-05: same binary, black screen after the OTA reboot, then Wi-Fi + API + display all healthy after the power cycle, `safe_mode: Boot seems successful`. The firmware was not at fault (that OTA only changed audio settings).

**How often:** a single documented occurrence (2026-08-05). Since 2026-09-30, when the history kept by Home Assistant begins, the author's tablet has restarted about fifty times, most of them OTAs, and Wi-Fi came back every time (count of 2026-10-07). The line `ESP-Hosted link not yet up` shows up once on every normal boot and is harmless: at 9,984 ms in a serial capture of 2026-10-07 (Wi-Fi connected ~17 s after the reset), at +6.9 s and +11.9 s in Home Assistant's log. The fault was that line repeated ~50 times per second.

**Not tried (a guess):** since a reset pulse was very probably not enough, cutting the C6's power in software and restoring it (`wifi_power`, PI4IOE 0x44, P0, which the firmware only switches on at boot) might recover from the fault without pulling the cable. Nothing proves it.

**Diagnostic trap, learned the same day:** opening *and closing* the USB serial port to read logs can reset the chip. An unexplained reboot right at the end of an `esphome logs --device COM<n>` session is the log session itself, not an instability. Once the device is back on Wi-Fi, watch it through its Home Assistant diagnostic entities instead.

---

### `Tab5 Uptime` stuck at `unavailable` after the update to the boot-time firmware (2026-09-26)

**Symptom:** after flashing a firmware from 2026-09-26 or later over an older one, every entity comes back except `Tab5 Uptime`, which stays `unavailable`. The Home Assistant log shows `ValueError: Sensor sensor.…_tab5_uptime has a unit of measurement and thus indicating it has a numeric value; however, it has the non-numeric device class: timestamp`.

**Root cause:** Home Assistant, not the firmware. `Tab5 Uptime` changed from seconds (`s`) to a boot timestamp without a unit, under the same name, so the same entity. On reconnection, HA's ESPHome integration updates the existing entity in place and only copies the unit when the new one is not empty (`homeassistant/components/esphome/sensor.py`, `_on_static_info_update`): the old `s` sticks, and HA refuses to write a timestamp state that carries a unit.

**Fix:** reload the ESPHome integration of the device once (Settings → Devices & services → ESPHome → the device → ⋮ → Reload), or restart Home Assistant. The entity is rebuilt from the device's description, without a unit. Done on 2026-09-26 at 21:39.

---

### Crash alert after a press on the power button (2026-10-06)

**Symptom:** a short press on the power button reboots the tablet in about 10 s, then Home Assistant sends « Tab5 : journal du démarrage (plantage (chien de garde)) » to the phone. The journal has only the usual boot lines (ESP-Hosted « not yet up », touch polling, Wi-Fi associating), no `esp32.crash` line. Meanwhile `Tab5 Raison du redémarrage` shows `Reboot request from esphome.ota`, although the last update was hours earlier. Reported in discussion #278 (tablet with a battery, 3.7.0-rc.2), reproduced the same day on a tablet powered by USB.

**Root cause:** not a crash. The firmware does not handle the power button: the hardware resets the chip, and `esp_reset_reason()` reads `ESP_RST_WDT`, a reason the journal counted as a crash. ESPHome's `debug` component, for that reason, shows the source stored by the last *requested* reboot (`components/debug/debug_esp32.cpp`), which it never clears: a stale text.

**Fix (firmware after 3.7.0-rc.3, and `packages/tab5_health.yaml`):** a watchdog reset with no crash report is now a normal boot: « bouton d'alimentation ou chien de garde RTC (rst 0x..) » in the journal, no event sent; `Power button or RTC watchdog (rst 0x..)` in the entity, which the « reboot inattendu » guard lets through. Update both the firmware and the HA files: with only the new firmware, the old guard would alert on the new text. A watchdog reset with a crash report still alerts. Details in [`debugging.md`](debugging.md).

---

### Adding Tab5 to HACS: « Repository not found », or an empty box (2026-10-07)

**Symptom:** adding the repository to HACS, by the « Open Tab5 in HACS » link or by *Custom repositories*, fails with « Repository not found » (« Dépôt introuvable » in French). Or the box that HACS opens through the link shows a title only (« Confirm? »), with no text.

**Root cause:** HACS checks a repository on its latest full release, even with beta versions switched on, and only the releases from 3.7.0 on carry the integration: before 3.7.0 was published (7 October 2026), HACS refused this repository (tried that day with 3.6.0 as the latest full release). The empty box comes from HACS: on a first load through the link, its texts are not loaded yet (seen with HACS 2.0.5).

**Fix:** « Repository not found »: check on the [releases page](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases) that the release marked *Latest* is 3.7.0 or later, and, when adding it by hand, that the type is **Integration**; then try again. Empty box: reload the page, the box comes back with its texts, *Add* on the right. Without HACS, the archive copied by hand works as before ([steps 1 to 3](installation/home-assistant-files.md#1-download-and-unzip)).

---

### Tab5 integration (HACS): a message in Repairs

**Symptom:** after an update of the « Tab5 — fichiers HA · HA files » integration and a restart, *Settings → Repairs* shows a message whose title starts with « Tab5 ».

**Root cause:** at start, the integration puts the files of its version in place, checks the configuration, reloads the YAML, then checks that the sensor « Tab5 · version des fichiers HA » shows the new version ([updates](installation/updates.md#home-assistant-files)). When a step does not end as expected, it says so in Repairs.

**Fix**, by message:

- **« Tab5: no Home Assistant files in the integration »**: the integration was copied from the repository, which holds its code but not the files. Install it through HACS ([with HACS](installation/home-assistant-files.md#with-hacs)), which downloads `tab5_hacs.zip` from the release, then restart.
- **« Tab5: Home Assistant does not load the packages »**: the files are in `config/packages/`, but `configuration.yaml` has no `packages:` line. Add it ([step 2](installation/home-assistant-files.md#2-one-line-in-configurationyaml)), check the configuration, then restart.
- **« Tab5: restart Home Assistant to finish »**: the files are in place, but a reload was not enough (a part that only loads at start, a reload that failed, or a configuration that already had an error). Open the message and submit: the integration checks the configuration, then restarts Home Assistant. If it answers that the configuration has an error, fix it first (*Developer tools → YAML → Check configuration*). With « Then update the tablet » ticked, the firmware of this version starts after this restart, once the tablet's « Firmware » entity offers it.
- **« Tab5: the files X were not installed »**: the configuration check found a new error or warning with the new files, or writing them failed. The previous files were put back: nothing changed, and the firmware of this version is not started. The reason is in the log (*Settings → System → Logs*, search « tab5 »). If it is in one of your own files, fix it; otherwise please report it ([issues](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/issues)). The integration tries again at each start of Home Assistant.

To go back to older files by hand: [putting older files back](installation/updates.md#putting-older-files-back-by-hand).

---

### False positives worth knowing about (don't "fix" these again)

- **Forecast pagination "wrap-around"**: the 5 forecast pages (indices 0–4) intentionally do **not** wrap from 4 back to 0 on a further right-swipe. This was already "corrected" once by an LLM audit that assumed non-wrapping was a bug, then reverted. See [`docs/decisions/`](decisions/README.md).
- **A cover entity showing `unknown` in Home Assistant**: this can be real on the HA side while being irrelevant to the Tab5 firmware, if the corresponding UI card drives its own internal state via globals + a script rather than reading that entity's state directly. Check what the specific `ui_components/*.yaml` card actually binds to before assuming the firmware is affected.
- **`Tab5 Uptime` about a minute away from the actual reboot**: not a clock problem. An ESPHome sensor state is a 32-bit float, which rounds a Unix timestamp to the nearest multiple of 128 s (±64 s). The health guard in `packages/tab5_health.yaml` allows 5 min for it.
- **Touch "overlap" between two adjacent buttons flagged by a static audit**: verify the actual pixel geometry (`x`/`y`/`width`/`height`) before trusting a reported overlap — a past report was off by roughly a dozen pixels and wasn't a real overlap.

---

---

## Version Française

---

Journal d'incidents réels rencontrés sur ce firmware précis, avec la cause racine confirmée et le correctif. À consulter **avant** de re-diagnostiquer quelque chose qui semble familier — plusieurs de ces cas ont demandé des heures de debug en direct avec l'appareil sur le bureau.

Format : **Symptôme → Cause racine → Correctif**.

### Écran noir après un reboot logiciel (pas une coupure d'alimentation)

**Symptôme :** après un reboot (OTA, crash, `api.reboot`), l'écran reste noir. Un cycle d'alimentation (débranché/rebranché) corrige le problème ; un reboot logiciel seul non, ou pas de façon fiable.

**Cause racine :** le `reset_pin` de l'écran passe par l'expander GPIO I2C `PI4IOE5V6408`, pas un GPIO natif ESP32. Juste après le boot, l'expander a lui-même besoin d'un court délai de stabilisation avant de piloter ses sorties de façon fiable — actionner `reset_pin` trop tôt est un no-op du point de vue de l'écran.

**Correctif :** un `delay(1000)` bloquant a été ajouté dans `on_boot: priority: 700` (`tab5-hardware.yaml`), juste avant la séquence de reset écran. Cause racine confirmée le 06/07/2026 après 5 tests de reboot en direct. Marqué `[AI-WARNING]` dans le fichier — **ne pas retirer ce délai** sans re-tester sur plusieurs reboots.

### Météo / planning / clim absents alors que l'appareil est en ligne

**Symptôme :** l'appareil affiche une connexion Wi-Fi/API saine, mais les cartes météo/planning/prévisions ne se mettent jamais à jour.

**Cause racine :** les automations de push côté HA sont toutes conditionnées par un flag booléen partagé (pertinent uniquement dans un setup à double instance HA). Ce flag était bloqué à `off` ; l'automation de garde-fou censée le reforcer à `on` à chaque boot HA était elle-même désactivée.

**Correctif :** réactiver le flag et l'automation de garde-fou, déclencher manuellement le push une fois. **Si votre setup pousse conditionnellement sur un flag d'état partagé, vérifiez que l'automation censée le réarmer au boot est bien active.**

**Depuis la 3.8**, le flag (`input_boolean.is_primary_active`), l'automatisation qui le réarmait et sa garde de santé sont retirés : plus aucune poussée ne dépend d'un flag partagé. Cet incident ne peut plus arriver avec les packages actuels ; si l'écran paraît encore figé, « MAJ Écran », puis vérifier que les automatisations de poussée sont actives.

### STT/TTS cassé, dizaines de logs "entity already exists — ignoring"

**Symptôme :** pipeline vocal en échec malgré un service STT sain testé directement. Logs HA pleins d'avertissements de collision d'ID.

**Cause racine :** une intégration type `remote_homeassistant` dupliquait toute une seconde instance HA dans celle-ci — collisions d'ID masquant les vraies entités STT/TTS. Ne se reproduit que dans un setup à **double instance HA**.

**Correctif :** désactiver l'intégration de duplication, redémarrage HA complet.

### Tactile "mort" alors que le contrôleur détecte bien les appuis

**Symptôme :** taper sur l'écran ne fait rien.

**Cause racine :** HA rejetait les appels de service de l'appareil (`Service call ... rejected`).

**Correctif :** cocher "Autoriser les appels de service" dans l'options flow de l'intégration ESPHome pour cet appareil.

**Depuis l'[ADR-0025](decisions/0025-events-only.md) (après la 3.1)**, le firmware n'appelle plus aucune action : ce refus ne peut plus arriver, les commandes et demandes de l'écran sont des événements. Le même symptôme sur un firmware récent veut dire que leur consommateur manque — le blueprint « Tab5 — emplacements » pour les tuiles, `packages/tab5_evenements.yaml` pour le calendrier, les annonces et la console système.

### Un push calendrier/météo casse silencieusement le reste de la même automation

**Cause racine :** `calendar.get_events` avec `start_date_time: "{{ now() }}"` (datetime brut avec microsecondes) — échec de validation silencieux qui interrompait toute la suite de l'automation.

**Correctif :** `"{{ now().strftime('%Y-%m-%d %H:%M:%S') }}"`, plus `continue_on_error: true` sur chaque action de push + gardes `is defined`.

### Icônes météo des jours disparues, horaire intact, automation « réussie » (18/09/2026)

**Symptôme :** les 5 cartes journalières perdent leurs icônes (et gardent des données périmées après un reboot) alors que l'horaire se met à jour. HA a toutes les données, l'automation de push ne signale aucune erreur.

**Cause racine :** le template du payload `tab5_maj_previsions_jours_bulk` lisait `fcasts[i].templow` en accès direct sur 15 jours. Météo-France ne fournit plus `templow` pour j+14 : en Jinja, `dict.cle_absente` lève `UndefinedError` **avant** que `| float(0)` ne s'applique, le payload n'est jamais rendu et le service jamais appelé. `continue_on_error: true` masquait la panne (seul le journal HA disait « Error rendering data template »).

**Correctif :** accès protégé `fcasts[i].get('templow') | float(0)` (idem `condition`, `temperature`, `precipitation`), en prod le 18/09 et dans l'exemple public le 25/09. Pour repérer la suivante : la **trace** de l'automation (un appel `…_bulk` manquant saute aux yeux), la garde (d) de `packages/tab5_health.yaml` (exige `system_log: fire_event: true`). Pour forcer un push complet : bouton **MAJ Écran** de la console système ou *Exécuter les actions* dans HA — l'événement `esphome.tab5_connected` lancé à la main ne pousse plus rien depuis le 25/09 (réservé à une liaison API de moins de 3 min).

---

### `pressed:` LVGL refusé dans un `style_definitions` partagé

**Cause racine :** limitation confirmée dans le code source du composant LVGL d'ESPHome — `pressed:` n'est valide que sur le widget lui-même.

**Correctif :** le partager sur le widget par `pressed: { styles: style_x }` (version anglaise ci-dessus ; l'ancien conseil « le répéter sur chaque bouton » était faux). Un bouton verre cliquable de rayon 18 n'a besoin de rien : `apply_pressed_scale_to_tree()` (`tab5_anim.cpp`) lui pose l'appui, et les `pressed:` qui le répétaient sont retirés depuis le 01/10/2026.

### Connexions API ESPHome épuisées

**Cause racine :** l'API native n'autorise que 8 connexions simultanées ; des process `esphome` CLI orphelins (sessions de debug oubliées) en occupent chacun une indéfiniment.

**Correctif :** fermer les sessions CLI une fois terminées ; vérifier les process `esphome` orphelins avant de soupçonner l'appareil.

### L'appareil redémarre tout seul toutes les ~15-20 min quand Home Assistant est injoignable

**Symptôme :** HA est tombé (ou met longtemps à démarrer), le Tab5 est toujours sur le WiFi et affiche son dernier état connu, mais il redémarre seul toutes les 15-20 minutes environ. Aucun plantage dans les logs : chaque boot est propre et `safe_mode` annonce `Boot seems successful`.

**Cause racine :** ce n'est pas un crash — c'est `api: reboot_timeout:` dans `tab5-api-logic.yaml`. ESPHome redémarre l'appareil quand **aucun client API n'est connecté** pendant cette durée, et le compteur repart à zéro à chaque connexion, même brève. Avec les 15 min d'origine, un serveur HA long à monter produit un cycle de reboots un peu plus long que le timeout lui-même (timeout + boot + tentatives de reconnexion). Le composant `wifi:` a son propre `reboot_timeout`, qui n'est *pas* en cause ici puisque le WiFi fonctionnait.

**Correctif :** porté à `60min` le 01/08/2026 — une panne ou une maintenance HA d'une heure ne fait plus cycler la tablette, qui reste utile sans HA (horloge, arcade, console diag, dernière météo affichée), tout en gardant le filet anti-« zombie » de l'audit F-04. **Ne pas** mettre `0s` sans relire cet audit : une pile API figée laisserait alors l'appareil en ligne mais muet jusqu'à une coupure d'alimentation manuelle.

### Écran noir + appareil absent du réseau après une OTA — le co-processeur WiFi n'est pas remonté

**Symptôme :** une OTA annonce `OTA successful`, l'appareil redémarre, et ensuite : écran noir, aucune entité côté Home Assistant, pas d'API (`esphome logs --device <ip>` part en timeout sur le port 6053). Le rebooter à nouveau ne change rien. Trompeur : l'IP répond toujours au `ping` — c'est un *autre* appareil qui a récupéré le bail, exactement comme la confusion DHCP du 01/08/2026.

**Cause racine :** l'ESP32-P4 n'a pas de radio à lui ; le WiFi vient du co-processeur ESP32-C6 en SDIO (`esp32_hosted`). Ce lien n'est pas remonté après le reboot *logiciel* de l'OTA. Le P4 réinitialise pourtant le C6 par GPIO 15 (`esp32_hosted` · `reset_pin`) à chaque démarrage, redémarrage logiciel compris : c'est le réglage par défaut d'ESP-Hosted (`CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP=y`, reset actif haut, 1 500 ms, lu le 07/10/2026 dans le `sdkconfig` d'un build ESPHome 2026.9). Cause revue le 07/10/2026 : cette page disait qu'un reboot logiciel ne réinitialise pas le C6, ce qui est très probablement faux pour le build du 05/08/2026 aussi. Il venait d'ESPHome 2026.7 (version exacte non notée) ; les versions 2026.7.0 à 2026.7.3 demandent esp_hosted 2.12.9 sans choisir quand réinitialiser le C6, et le réglage par défaut d'esp_hosted 2.12.9 est le même. `reset_pin: GPIO15` est dans `Tab5/tab5-hardware.yaml` depuis le premier commit. Le reboot de l'OTA a donc très probablement envoyé son impulsion de reset au C6 sans effacer la panne ; seule la coupure d'alimentation l'a fait. La cause exacte est inconnue. La console série (USB, port COM) tranche en quelques secondes :

```
[I][esp-idf:000]: E (93883) H_API: ESP-Hosted link not yet up
[W][wifi_esp32:300]: esp_wifi_set_mode failed: ESP_FAIL
[I][wifi:852]: Starting fallback AP
```

Ces trois lignes se répètent ~50 fois par seconde. L'écran noir est une *conséquence*, pas la panne : cette boucle chaude de reconnexion affame le reste du firmware, LVGL compris. Ne pas partir fouiller la pile d'affichage.

**Correctif :** une **vraie coupure d'alimentation** — débrancher l'USB-C pendant ~15 s, puis le rebrancher : sans batterie, la tablette redémarre seule. Sur la tablette de l'auteur, qui n'a pas de batterie, débrancher a suffi : l'appui long n'était pas nécessaire. Avec une batterie montée, débrancher ne coupe pas l'alimentation : éteindre aussi la tablette par un appui long. Un reboot logiciel ne suffit pas. Confirmé le 05/08/2026 : même binaire, écran noir après le reboot d'OTA, puis WiFi + API + dalle tous sains après la coupure, `safe_mode: Boot seems successful`. Le firmware n'était pas en cause (cette OTA ne changeait que des réglages audio).

**Fréquence :** une seule occurrence documentée (05/08/2026). Depuis le 30/09/2026, début de l'historique gardé par Home Assistant, la tablette de l'auteur a redémarré une cinquantaine de fois, des OTA pour la plupart, et le WiFi est revenu à chaque fois (relevé du 07/10/2026). La ligne `ESP-Hosted link not yet up` apparaît une fois à chaque démarrage normal et est sans conséquence : à 9 984 ms sur une capture série du 07/10/2026 (WiFi connecté ~17 s après le reset), à +6,9 s et +11,9 s dans le journal de Home Assistant. La panne, c'était cette ligne répétée ~50 fois par seconde.

**Pas essayé (une supposition) :** puisqu'une impulsion de reset n'a très probablement pas suffi, couper puis remettre l'alimentation du C6 par logiciel (`wifi_power`, PI4IOE 0x44, P0, que le firmware ne fait qu'allumer au démarrage) pourrait rattraper la panne sans débrancher le câble. Rien ne le prouve.

**Piège de diagnostic, appris le même jour :** ouvrir *et refermer* le port série USB pour lire les logs peut réinitialiser la puce. Un reboot inexpliqué pile à la fin d'une session `esphome logs --device COM<n>`, c'est la session de logs elle-même, pas une instabilité. Une fois l'appareil revenu sur le WiFi, le surveiller via ses entités de diagnostic Home Assistant.

### `Tab5 Uptime` bloqué à `unavailable` après la mise à jour vers le firmware à heure de démarrage (26/09/2026)

**Symptôme :** après le flash d'un firmware du 26/09/2026 ou plus récent par-dessus un plus ancien, toutes les entités reviennent sauf `Tab5 Uptime`, qui reste `unavailable`. Le journal de Home Assistant montre `ValueError: Sensor sensor.…_tab5_uptime has a unit of measurement and thus indicating it has a numeric value; however, it has the non-numeric device class: timestamp`.

**Cause racine :** Home Assistant, pas le firmware. `Tab5 Uptime` est passé de secondes (`s`) à une heure de démarrage sans unité, sous le même nom, donc la même entité. À la reconnexion, l'intégration ESPHome de HA met à jour l'entité existante sur place et ne recopie l'unité que si la nouvelle n'est pas vide (`homeassistant/components/esphome/sensor.py`, `_on_static_info_update`) : l'ancien `s` reste, et HA refuse d'écrire un état horodaté qui porte une unité.

**Correctif :** recharger une fois l'intégration ESPHome de l'appareil (Paramètres → Appareils et services → ESPHome → l'appareil → ⋮ → Recharger), ou redémarrer Home Assistant. L'entité est reconstruite depuis la description de l'appareil, sans unité. Fait le 26/09/2026 à 21:39.

### Alerte de plantage après un appui sur le bouton d'alimentation (06/10/2026)

**Symptôme :** un appui court sur le bouton d'alimentation redémarre la tablette en ~10 s, puis Home Assistant envoie « Tab5 : journal du démarrage (plantage (chien de garde)) » sur le téléphone. Le journal ne contient que les lignes habituelles du démarrage (ESP-Hosted « not yet up », tactile, Wi-Fi qui s'associe), aucune ligne `esp32.crash`. En même temps, `Tab5 Raison du redémarrage` affiche `Reboot request from esphome.ota`, alors que la dernière mise à jour date de plusieurs heures. Signalé dans la discussion #278 (tablette avec batterie, 3.7.0-rc.2), reproduit le même jour sur une tablette alimentée par l'USB.

**Cause racine :** pas un plantage. Le firmware ne gère pas le bouton d'alimentation : c'est le matériel qui réinitialise la puce, et `esp_reset_reason()` lit `ESP_RST_WDT`, une raison que le journal comptait comme un plantage. Pour cette raison, le composant `debug` d'ESPHome affiche la source enregistrée par le dernier redémarrage *demandé* (`components/debug/debug_esp32.cpp`), qu'il n'efface jamais : un texte périmé.

**Correctif (firmware après la 3.7.0-rc.3, et `packages/tab5_health.yaml`) :** un reset du chien de garde sans rapport de plantage est un démarrage normal : « bouton d'alimentation ou chien de garde RTC (rst 0x..) » dans le journal, aucun événement envoyé ; `Power button or RTC watchdog (rst 0x..)` dans l'entité, que la garde « reboot inattendu » laisse passer. Mettre à jour le firmware ET les fichiers HA : avec le nouveau firmware seul, l'ancienne garde alerterait sur le nouveau texte. Un chien de garde avec rapport de plantage alerte toujours. Détail dans [`debugging.md`](debugging.md#version-française).

### Ajout de Tab5 dans HACS : « Dépôt introuvable », ou une boîte vide (07/10/2026)

**Symptôme :** l'ajout du dépôt dans HACS, par le lien « Ouvrir Tab5 dans HACS » ou par *Custom repositories*, échoue avec « Dépôt introuvable » (« Repository not found » en anglais). Ou la boîte que HACS ouvre par le lien n'affiche qu'un titre (« Confirmer ? »), sans texte.

**Cause racine :** HACS vérifie un dépôt sur sa dernière release complète, même avec les versions bêta activées, et seules les releases depuis la 3.7.0 portent l'intégration : avant la publication de la 3.7.0 (7 octobre 2026), HACS refusait ce dépôt (essayé ce jour-là, avec la 3.6.0 comme dernière release complète). La boîte vide vient de HACS : au premier chargement par le lien, ses textes ne sont pas encore chargés (vu avec HACS 2.0.5).

**Correctif :** « Dépôt introuvable » : vérifier sur la [page des releases](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases) que la release marquée *Latest* est la 3.7.0 ou plus récente et, pour un ajout à la main, que le type est **Integration** ; puis réessayer. Boîte vide : recharger la page, la boîte revient avec ses textes, *Ajouter* à droite. Sans HACS, l'archive copiée à la main marche comme avant ([étapes 1 à 3](installation/home-assistant-files.md#1-télécharger-et-décompresser)).

### Intégration Tab5 (HACS) : un message dans Réparations

**Symptôme :** après une mise à jour de l'intégration « Tab5 — fichiers HA · HA files » et un redémarrage, *Paramètres → Réparations* montre un message dont le titre commence par « Tab5 ».

**Cause racine :** au démarrage, l'intégration pose les fichiers de sa version, vérifie la configuration, recharge le YAML, puis vérifie que le capteur « Tab5 · version des fichiers HA » donne la nouvelle version ([mises à jour](installation/updates.md#fichiers-home-assistant)). Quand une étape ne finit pas comme prévu, elle le dit dans Réparations.

**Correctif**, selon le message :

- **« Tab5 : aucun fichier Home Assistant dans l'intégration »** : l'intégration a été copiée depuis le dépôt, qui a son code mais pas les fichiers. Installez-la par HACS ([avec HACS](installation/home-assistant-files.md#avec-hacs)), qui télécharge `tab5_hacs.zip` de la release, puis redémarrez.
- **« Tab5 : Home Assistant ne charge pas les packages »** : les fichiers sont dans `config/packages/`, mais `configuration.yaml` n'a pas de ligne `packages:`. Ajoutez-la ([étape 2](installation/home-assistant-files.md#2-une-ligne-dans-configurationyaml)), vérifiez la configuration, puis redémarrez.
- **« Tab5 : redémarrez Home Assistant pour finir »** : les fichiers sont en place, mais un rechargement n'a pas suffi (une partie qui ne se charge qu'au démarrage, un rechargement qui a échoué, ou une configuration qui avait déjà une erreur). Ouvrez le message et validez : l'intégration vérifie la configuration, puis redémarre Home Assistant. Si elle répond que la configuration a une erreur, corrigez-la d'abord (*Outils de développement → YAML → Vérifier la configuration*). Avec « Mettre ensuite la tablette à jour » coché, le firmware de cette version part après ce redémarrage, dès que l'entité « Firmware » de la tablette le propose.
- **« Tab5 : les fichiers X n'ont pas été installés »** : la vérification de la configuration a trouvé une nouvelle erreur ou un nouvel avertissement avec les nouveaux fichiers, ou leur écriture a échoué. Les fichiers précédents ont été remis : rien n'a changé, et le firmware de cette version n'est pas lancé. La raison est dans le journal (*Paramètres → Système → Journaux*, cherchez « tab5 »). Si elle est dans un de vos propres fichiers, corrigez-le ; sinon, merci de la signaler ([issues](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/issues)). L'intégration réessaie à chaque démarrage de Home Assistant.

Pour revenir à la main à des fichiers plus anciens : [remettre des fichiers plus anciens](installation/updates.md#remettre-à-la-main-des-fichiers-plus-anciens).

### Faux positifs à connaître (ne pas re-"corriger")

- **Pagination prévisions sans bouclage** : intentionnel (0↔4), déjà "corrigé à tort" une fois par un audit LLM puis reverté. Voir [`docs/decisions/`](decisions/README.md).
- **Entité `cover` à `unknown` côté HA** : peut être réel côté HA sans affecter le firmware si la carte concernée pilote son propre état via des globals + un script plutôt que de lire cette entité.
- **`Tab5 Uptime` décalé d'environ une minute par rapport au vrai reboot** : pas un problème d'horloge. L'état d'un capteur ESPHome est un float 32 bits, qui arrondit un horodatage Unix au multiple de 128 s le plus proche (±64 s). La garde de `packages/tab5_health.yaml` prévoit 5 min de marge pour cela.
- **"Chevauchement" tactile signalé par un audit statique** : vérifier la géométrie pixel réelle avant de faire confiance au rapport — un cas signalé était décalé d'une douzaine de pixels, pas un vrai chevauchement.
