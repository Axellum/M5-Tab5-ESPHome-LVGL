# Home Assistant Configuration for Tab5

## English · [Français](#version-française)

---

This folder contains the Home Assistant side of the Tab5 integration: automations that push data to the device, scripts triggered by the device, template sensors and helpers. Everything is a Home Assistant **package** (`packages/`), with **no placeholder** since 2026-09-28 ([ADR-0024](../docs/decisions/0024-packages-without-placeholders.md)): every value of your home is picked in Home Assistant (the « Tab5 · … » lists), and the tablet is found by its device model.

> **One source (2026-09-26).** The author's Home Assistant runs these very packages (as they are since 2026-09-28): there are no private versions anymore. Until that day, three hand-merged example files (`automations_examples.yaml.example`, `scripts_examples.yaml`, `template_sensors_examples.yaml`) were derived from private files, and they drifted: the shutter package had never run anywhere, and `script.allumer_pc_tv`, called by the firmware, existed in no public file.

> **Install = one archive.** Unzip `tab5_home_assistant.zip` (attached to each release) into HA's `config/`, enable `homeassistant: packages: !include_dir_named packages` in `configuration.yaml` (the only YAML line to write), restart, then pick your sources (see [Adapting to your setup](#adapting-to-your-setup)). Nothing to merge into `automations.yaml` or `scripts.yaml`. Automations and scripts defined in a package are read-only in the HA UI: change the file, deploy.

---

## Files

### `packages/tab5_reglages.yaml` — your choices (since 2026-09-28)
What used to be placeholders, picked in HA's UI (*Settings → Devices & services → Entities*, search « Tab5 · »):
- **Lists** (template selects, the choice kept in an `input_text.tab5_choix_*` that HA restores): « Tab5 · agenda de travail », « … des rendez-vous », « … des anniversaires », « … des jours fériés », « … des vacances scolaires », « Tab5 · téléphone », « Tab5 · capteur de présence », « Tab5 · pipeline de discussion ». Their names are in French and English (« Tab5 · agenda de travail · work calendar »); `default_entity_id` keeps their entity IDs. State = the chosen entity (or pipeline), or « Aucun ». A default is picked only when there is no doubt (the only birthdays calendar, the only public-holiday calendar, the only school-holiday calendar, the only phone of the companion app); work calendar, appointments, presence sensor and chat pipeline are never guessed. The text « Tab5 · mot des événements de travail » is typed by hand. The lists are trigger-based (start, template reload, entity added or renamed, choice changed): iterating `states.binary_sensor` in a plain template would re-render it on every sensor change of the house.
- **`sensor.tab5_tablette`**: the tablet, found like the `tab5_connectee` guard (the « HA API Status » sensor of an ESPHome device whose model is `tab5-ha-hmi`); its entities (screen, alarm ringing, restart reason, uptime, wake-word switch, voice satellite, appointment lead time) in attributes, whatever the device is called (part of the author's entities carry a `salon_` prefix).
- **Mirrors** for the automations' triggers (a `state:` trigger needs an entity ID written in YAML): `binary_sensor.tab5_presence`, `sensor.tab5_telephone_suivi`, `binary_sensor.tab5_connectee`, `sensor.tab5_demarrage`, `sensor.tab5_rendez_vous_annoncer_avant`, `sensor.tab5_agendas`. Unavailable while nothing is chosen: nothing fires.

### `packages/tab5_push.yaml`
The push automations, the scripts they share, the scripts the Tab5 calls, the optional-zones answer and the `is_primary_active` guard. They push data to the Tab5 via native ESPHome service calls; blocks sent from more than one automation live once in the `tab5_push_*` scripts. This is the package to start from.

What it pushes:
- **Daily forecast (15 days):** every 10 min, on calendar changes and on (re)connection — serializes 15 × (index, day label, condition, min, max, weekend/holiday flags, work hours) into a `|`/`;`-delimited string sent to `tab5_maj_previsions_jours_bulk`
- **Hourly forecast (10 slots):** two chunks of 5 through `tab5_maj_previsions_heures_bulk` (the screen has two hourly pages)
- **Short-term rain chart:** on a change of `sensor.tab5_pluie_dans_l_heure` (packages/tab5_meteo_sources.yaml) — **9** bars in **one** call (`tab5_maj_pluie_1h_bulk`, payload `idx|intensity;…`, index 0–8 = 0/5/10/…/55 min, intensity = Météo-France label or level 0–4), from the source chosen in HA: Météo-France, OpenWeatherMap or none
- **Current weather / probabilities:** `tab5_maj_meteo_actuelle` (condition, temperature, humidity) and `tab5_maj_probabilites` (UV, frost, snow) — when they change (`tab5_ha_hmi_meteo_push`) and on (re)connection, script `tab5_push_meteo`
- **Climate state:** pushed by the blueprint `tab5_emplacements.yaml` since 3.0 (`tab5_maj_clim`: target, current, mode, preset, fan, swing), on each change and on (re)connection
- **Shutter state:** `tab5_maj_volet_etat` when the helpers change (`tab5_volet_updater`) and on (re)connection, script `tab5_push_volet` — also arms the device-local “Stop” wake word while the shutter moves
- **Info banner:** `tab5_maj_info_texte` (text, colour, dismiss id) — the `@ha|…` code (updates, errors, unavailable entities, weather-warning banner), written by the Tab5 in its language
- **Weather warnings:** `tab5_maj_alerte_meteo_france` (historical name) — one `|`-delimited payload: rain code, overall level, then 11 hazards (the 9 Météo-France ones, fog, forest fire), from `sensor.tab5_vigilance` (Météo-France, MeteoAlarm, DWD, CAP Alerts or none)
- **HA alert queue:** `tab5_maj_alertes_ha_bulk` — up to 4 banners in the central rotator (see `packages/tab5_alerts.yaml`)

Also in the package, not a push: **`tab5_screen_presence_wifi`** switches the screen backlight on when the presence sensor detects someone (or the phone comes home) if it is off, and off after **15 min** without presence (or when the phone leaves) if it is on and the alarm is not ringing. With the screen off the firmware pauses LVGL; a touch or a tap on the panel wakes it.

Room temperatures, lights, PC, TV, phone and plants do **not** go through this package: since 3.0 the blueprint `tab5_emplacements.yaml` pushes them (`tab5_maj_emplacements`). The tablet no longer subscribes to any entity of your home.

**No periodic re-push of unchanged state (2026-09-26):** current weather, probabilities, climate and shutter used to be re-sent every 10 min on top of their on-change pushes (576 calls a day, each one repainted by the device). The full push now sends them only on (re)connection, when Home Assistant starts (the tablet often reconnects before automations are active, and its `esphome.tab5_connected` is then lost) and when `input_boolean.is_primary_active` comes back `on`.

**Traffic pacing:** the automation uses `delay: 1s` between each push block and `delay: 150ms` within forecast loops. This prevents multiple large payloads from overwhelming the ESP32-P4's TCP socket buffer simultaneously with the active I2S audio stream.

---

**Scripts.** Since 3.0 the Tab5 calls no script for your devices: its commands go to the blueprint (see below). The blueprint still calls `script.tab5_volet_action` (shutter package) and `script.tab5_tv_app` (TV package); the calendar, alarm and dismiss scripts are started by `packages/tab5_evenements.yaml` when the tablet asks (see below). The tablet itself calls no action at all ([ADR-0025](../docs/decisions/0025-events-only.md)).

Also the **push scripts** `tab5_push_alertes` (sections 1, 7 and 7b: Météo-France vigilance, info banner, HA alert rotator — updates, `problem` sensors and the unavailable count are read once per run), `tab5_push_meteo` and `tab5_push_volet`. These are called *by the automations*, not by the Tab5: each block exists once instead of being copied into the full push and into its on-change automation.

**Template sensor** (now in `packages/tab5_meteo_sources.yaml`, see below). `Tab5 Pluie dans l'heure` turns the next-rain forecast into a **code**, `@level,start` (level -1 no data, 0 dry, 1 to 4 light to very heavy, 5 unknown intensity; start = UTC epoch of the rain, 0 if it is already raining). Since lot 4c (2026-09-27), the Tab5 writes the sentence itself, in its own language (« Averses dans 12 mn » / “Showers in 12 min”), and counts the minutes down on its own clock: the sensor only changes with the Météo-France data, no longer every minute. The info banner and the HA alert rotator are sent as codes too (`@ha|…`, `@maj:`, `@indispo:`). **Deploy this package after the lot 4c firmware**: an older firmware would show the codes as they are.

**Optional zones (lot 5, [ADR-0018](../docs/decisions/0018-optional-zones-confirmed-by-ha.md)).** Since 3.0 the blueprint answers the tablet's zones request: an empty slot, or an entity that doesn't exist, disappears from the screen. The pushes of this package follow the same rule: `tab5_push_volet` sends nothing without a shutter. See [Adapt to your home](../docs/installation.md#adapt-to-your-home).

**Guard `input_boolean.is_primary_active`.** Every push is conditioned on it; `force_primary_active_on_boot` turns it back on when HA starts, and `packages/tab5_health.yaml` warns if it stays off for 5 min. It is a leftover of a former two-instance setup ([ADR-0008](../docs/decisions/0008-single-ha-instance.md)): on a single Home Assistant it simply stays on.

The assistant-reply example (engine → assistant popup) moved to `snippets/tab5_assist_reponse_exemple.yaml`: inside a package it would have been active for everyone.

---

### `blueprints/automation/tab5/tab5_emplacements.yaml` — pick your devices (since 3.0)
The Tab5 knows no entity of your home any more ([ADR-0019](../docs/decisions/0019-logical-slots-blueprint.md)): this **blueprint** maps its slots to your entities. Import it (*Settings → Automations & scenes → Blueprints → Import blueprint*, then paste its GitHub URL) or copy it into `config/blueprints/automation/tab5/`, then create **one automation per tablet**. All inputs are optional:
- **rooms 1 to 5** (firmware 3.2 and later, [ADR-0023](../docs/decisions/0023-rooms-generic-tiles.md)): a name and up to five devices each, in the order of the tiles — one room per page of the bottom row (room 1 = home page). Lights, switches, covers and valves, media players, scenes/scripts/buttons, sensors, binary sensors, people and locks, climate. An optional customisation per device: name, icon, behaviour (on only, confirm, read only);
- TV and its remote, phone battery, room temperature and humidity, a second temperature (greenhouse), climate, plants 1-5 (the moisture sensor; conductivity, light, temperature and battery are taken from the same device), work calendar;
- the **3.x setup** of the home page (folded): lights 1-3, PC, shutter. A 3.0/3.1 firmware only uses these; a 3.2 firmware too while room 1 is empty.

The automation:
- reads the tablet's version (`sw_version` of the device, found by its model): from 3.2.0 it describes the rooms with `tab5_maj_tuiles` (`pR|name;tRT|type|icon|options|complement|name;…`) on (re)connection, on automation reload, when Home Assistant starts and on the zones request, then pushes each tile's state (`tRT|state|value|colour`, in `tab5_maj_emplacements`). Below 3.2.0, or with an unreadable version, it never calls `tab5_maj_tuiles` (a missing action is an error `continue_on_error` does not catch);
- pushes the 3.x slots with `tab5_maj_emplacements` (`key|state|value;…`): all of them on (re)connection and when Home Assistant starts, lights, PC and TV on each visible change, measurements (and sensors placed in a room) grouped every 5 minutes; the climate with `tab5_maj_clim`, and a shutter that reports its travel with `tab5_maj_volet_etat`;
- runs the screen's commands (`esphome.tab5_action` events: toggle, brightness, colour, setpoint, HVAC mode, TV keys and apps, shutter…): a 3.x slot on the chosen entity, a tile (`tRT`) or a room (`pR`, « Tout éteindre ») according to the domain of the tile's entity — never on an entity that no tile holds. Events need no « allow the device to perform Home Assistant actions » option;
- answers the zones request (`esphome.tab5_zones`, [ADR-0018](../docs/decisions/0018-optional-zones-confirmed-by-ha.md)): an empty slot, or an entity that doesn't exist, disappears from the screen.

No placeholder: the file is generic. Changing a device is an edit of the automation in HA's UI — no flash, no restart. The block between `# >>> icones` and `# <<< icones` (the icon palette) is written by `tools/gen_tuiles_icones.py`.

### `packages/tab5_evenements.yaml` — the tablet's requests (events only)
Since [ADR-0025](../docs/decisions/0025-events-only.md) the firmware never calls a Home Assistant action: the « Allow the device to perform Home Assistant actions » option is no longer needed. The tablet sends `esphome.tab5_*` events, and this package's single automation (`tab5_evenements`, `mode: parallel`) turns each one into a **fixed** action, for a device of model `tab5-ha-hmi` only, on that tablet's own entities (found with `device_entities`, no entity to configure):

| Event (data) | Action |
|---|---|
| `tab5_reveil_annonce` | `script.tab5_reveil_annonce` (below) |
| `tab5_annonce` (`message`) | `assist_satellite.announce` on the tablet's satellite (appointments, « Volet arrêté ») |
| `tab5_calendrier_mois` (`annee`, `mois`), `tab5_calendrier_jour` (`date`) | `script.tab5_calendrier_mois` / `_jour` (below) |
| `tab5_alerte_lue` (`alert_id`) | `script.tab5_dismiss_alert` (below) |
| `tab5_voix_stop` | `media_player.media_stop` on the tablet's player |
| `tab5_mode_assistant` (`option`) | `select.select_option` on the tablet's pipeline select, only if the option exists |
| `tab5_maj_ecran` | « MAJ Écran »: `input_boolean.is_primary_active` on, then `automation.trigger` of the full push (found by its id, `tab5_ha_hmi_updater`) |
| `tab5_recharger_automatisations` | `automation.reload` |
| `tab5_redemarrage_ha_confirme` | `homeassistant.restart` — sent only by « Confirmer » on the tablet's confirmation screen |

No action name or entity comes from the event itself. A missing script (package not installed) or option is skipped silently. **Deploy it before a firmware newer than 3.1**; with 3.1 or older (which still calls actions) it simply waits. See [Upgrading from 3.1](../docs/installation.md#upgrading-from-31). No placeholder: the file is generic.

### `packages/tab5_meteo_sources.yaml`
Weather adapters (lot 4c-2, 2026-09-27). Two selects pick the source **in Home Assistant, without YAML**: « Tab5 · source de la pluie dans l'heure » (Météo-France / OpenWeatherMap / Buienradar / DWD / Met.no / Open-Meteo / Aucune) and « Tab5 · source des vigilances » (Météo-France / MeteoAlarm / DWD / CAP Alerts / Aucune). Two normalized sensors turn any source into what the Tab5 reads, and the pushes only read them:
- `sensor.tab5_pluie_dans_l_heure`: state = rain code `@level,start`, attribute `barres` = the 9 bars. It is a trigger-based template: it runs `openweathermap.get_minute_forecast` (minute series in mm/h, read from the integration's cache — no extra API call) or, since 2026-09-29, `rest_command.tab5_pluie` of the same package for four keyless services queried at `zone.home` rounded to 0.01° (radars Buienradar, DWD through Bright Sky, Met.no Nowcast; the Open-Meteo model in 15-min steps), only while chosen. Every answer is turned into one series (start, length, mm/h) that the state and bars read. Thresholds: < 0.1 dry, < 2.5 light, < 7.6 moderate, < 50 heavy, then very heavy;
- `sensor.tab5_vigilance`: state = overall level (Vert / Jaune / Orange / Rouge), attribute `phenomenes` = 11 levels. MeteoAlarm codes are mapped to the Tab5 slots (wind, snow-ice, thunderstorms, fog, heat, cold, coastal, forest fire, avalanches, rain, flooding); it exposes one alert at a time. DWD (2026-09-29: every warning of the region, pre-warnings left out by their `urgency`, since the entity IDs follow HA's language) and CAP Alerts (HACS, one entity per alert; slot from the MeteoAlarm hazard type or the icon the integration derives) go through the macro `tab5_vigilance` of `custom_templates/tab5_vigilance.jinja`, imported inside their branch only: warnings in force or starting within 24 h, overall level = the highest.

- **Forecast source** (lot 4c-3): the select « Tab5 · source des prévisions » lists the `weather.*` entities (choice kept in `input_text.tab5_meteo_previsions`; empty or gone: the Météo-France city, otherwise the first weather entity). The providers' entities are found by `sensor.tab5_sources_meteo` (trigger-based): the Météo-France sensors on the same device as the city (`_next_rain`, `_weather_alert`, `_uv`, `_freeze_chance`, `_snow_chance`), the OpenWeatherMap weather, the MeteoAlarm binary sensor (by its attribution), the DWD and CAP Alerts sensors (attributes `dwd` and `cap`: the integration's sensors, otherwise recognized by their attributes). `sensor.tab5_meteo` gives its condition (state) and `temperature`, `humidite`, `uv`, `gel`, `neige`, plus what the entity supports (`type_jours` = daily, twice_daily or hourly; `heures_ok`). The full push asks only for supported types and groups twice-daily or hourly forecasts by date.

OpenWeatherMap (forecasts and rain) was tried on the author's installation on 2026-09-27; the MeteoAlarm branch and the twice-daily/hourly grouping were tested with simulated data in Home Assistant's template engine only. DWD was added to the author's installation on 2026-09-29 (a day without warnings); its warnings and CAP Alerts were tested with simulated data (template engine, fresh-install CI). Setup: [weather providers](../docs/installation.md#weather-providers).

### `packages/tab5_health.yaml`
Health-monitoring package: six guard automations that alert when the push pipeline silently degrades. Because the Tab5 is push-only (see `docs/decisions/0001-push-only-zero-polling.md`), a stale screen raises no error on its own — these automations are the HA-side safety net.

What it watches:
- **`input_boolean.is_primary_active` OFF for more than 5 min** — this boolean gates every push automation; stuck OFF means the screen silently freezes (a real incident, see `docs/troubleshooting.md`)
- **A new boot time on `Tab5 Uptime`** (a timestamp, published once per boot since 26/09/2026) — unexpected device reboot (brownout, firmware crash, power cut); a plain Wi-Fi drop without reboot comes back with the same boot time and does *not* trigger it
- **`HA API Status` off/unavailable for more than 2 min** — device unreachable, every push fails during the outage
- **A Tab5 automation logs « Error rendering »** — a push action failed to render its template and `continue_on_error` skipped it silently (real incident, 18/09/2026: Météo-France dropped `templow` from the 15th day). Requires `system_log: fire_event: true` in `configuration.yaml` (restart needed) — without it the guard loads but never fires. Exclude `system_log_event` from the recorder. At most one notification per hour while the error repeats
- **The boot and outage journal sent by the Tab5** (`esphome.tab5_journal`, since 26/09/2026) — the firmware (`Tab5/tab5_journal.cpp`) keeps its errors, and its warnings while HA is not connected, in memory that survives software resets and crashes (plus an NVS copy once Wi-Fi has been missing for 90 s, for power cuts), and sends them when HA reconnects for: an abnormal reset or a crash report (ESPHome's: PC and backtrace of both cores), Wi-Fi missing for 90 s or more (the Wi-Fi co-processor link, or the router), a boot that never reached HA, an error (ESPHome or ESP-IDF, including the ESP-Hosted driver) after HA connected, or HA reached more than 90 s after boot. Lines logged before the first HA connection are context only: a normal boot sends nothing, and neither does HA being away while Wi-Fi is up. Persistent notification every time; phone push only when `grave` (abnormal reset, crash, or Wi-Fi missing 90 s). No `system_log` prerequisite
- **Home Assistant files older than the firmware** (since 2026-09-29) — `sensor.tab5_version_des_fichiers_ha` holds the release of the archive these files came from (« dépôt » when copied from the repository: never compared); `binary_sensor.tab5_fichiers_ha_en_retard` turns on when the tablet runs a newer X.Y (major or minor: a patch release alone does not count, it may ask for a single file). Persistent notification only, removed once the versions meet

Design notes:
- The guards notify through one script, `script.tab5_health_notify` (persistent notification + `notify.notify`), so the channels are adapted in a single place; each channel carries `continue_on_error: true` so one failing channel doesn't block the other
- No template uses raw `now()` — detection relies on trigger `for:` windows and `trigger.from_state` / `trigger.to_state`
- Numeric comparisons use `| float(0)` defaults (boot safety)

It's a self-contained HA *package*; enable packages in `configuration.yaml` first:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Only `notify.notify` may need adapting (the notification channel). The tablet's entities are no longer named in the file: the guards read the mirrors `sensor.tab5_demarrage` and `binary_sensor.tab5_connectee` (`packages/tab5_reglages.yaml`); the arrival of a new tablet is not taken for a reboot.

---

### `packages/tab5_calendar.yaml`
Backend of the firmware's **calendar popup** (long press on the clock). Two scripts requested *by the device* (events `esphome.tab5_calendrier_mois` / `_jour`, started by `packages/tab5_evenements.yaml`), both `mode: queued` (`max: 10`; until 2026-09-28 `restart`, which let only one of the three requests the tablet sends in a row — the shown month and its neighbours — get an answer):

- **`tab5_calendrier_mois`** (`annee`, `mois`) — reads the four calendars chosen in the « Tab5 · agenda … » lists (work, public holidays, appointments, birthdays; one left on « Aucun » is skipped) over the requested month and pushes back `esphome.<device>_tab5_maj_calendrier_mois`: a 62-hex-char string (2 per day — bits: work / public holiday / school holiday / appointment / birthday) plus 31 `|`-separated work-hour fields and a `details` field (day-detail lines, `~`-separated — required by the firmware since the 25/07/2026 schema, sent empty here)
- **`tab5_calendrier_jour`** (`date`) — builds the day-detail lines (`type|text;...`, max 6) and pushes `esphome.<device>_tab5_maj_calendrier_jour`

School holidays come from the calendar chosen in « Tab5 · agenda des vacances scolaires » (every event of it; in France, the ministry's ICS file of your zone through the Remote Calendar integration, see [docs/installation.md](../docs/installation.md#step-4--set-up-the-home-assistant-packages)); until 2026-09-29 they came from a static Zone A table. Work events are those whose title holds a word of « Tab5 · mot des événements de travail » (any case; empty = all of the work calendar). The Google public-holidays calendar mixes real holidays with civil observances, hence the `feries_connus` whitelist; a calendar of the Holiday integration holds only public holidays, all taken. Same package install as above.

The four Jinja macros shared by its templates (`ev_start`, `ev_end`, `ev_summary`, `couvre` — one normalisation of a `calendar.get_events` event, all-day or timed) live in `custom_templates/tab5_calendar.jinja`. Deploy that file to HA's `config/custom_templates/` and call `homeassistant.reload_custom_templates` (or restart) **before** loading the package: without it both scripts fail at their import line.

---

### `packages/tab5_reveil.yaml`
What Home Assistant adds to the firmware's **alarm clock** — and nothing more. **The alarm itself does not depend on this file**: the device computes its ring time from the SNTP clock and the work hours it already caches, and rings a locally synthesised melody. Stop HA and the alarm still goes off; only the spoken briefing and the appointment reminders go missing. Never move the decision to ring in here.

- **`tab5_rdv_prochains`** — pushes the next 24 h of *timed* appointments to `esphome.<device>_tab5_maj_rdv_prochains` as `epoch|title~epoch|title~…` (8 max). Work events are excluded: their hours already drive the alarm time, and they are not appointments. **The device runs the countdown itself**, so an HA outage between the push and the deadline misses nothing.
- **`tab5_reveil_annonce`** — the spoken morning briefing (time, today's shift, next appointment, temperature) in the screen's language (select « Langue »: French, English, German, Dutch, Spanish or Italian), requested *by the firmware* (event `esphome.tab5_reveil_annonce`, through `packages/tab5_evenements.yaml`) when `switch.tab5_alarm_tts` is on, and only on the first ring — not on snoozes.
- **automation `tab5_rdv_push`** — keeps the list fresh: every 5 min, on calendar changes, on `esphome.tab5_connected` (otherwise the list stays empty after a device reboot), and when the lead time changes.

The calendars are the lists « Tab5 · agenda des rendez-vous » and « Tab5 · agenda de travail »; the satellite that speaks is the detected tablet's. Nothing to edit. **The alarm time comes from the work calendar** (pushed with the daily forecast): left on « Aucun », every day is a rest day. Same package install as above.

---

### `packages/tab5_alerts.yaml`
Backend of the **HA alert queue** — panels 4 to 7 of the central rotating card. Provides the `input_text.tab5_alerts_dismissed` helper (the dismiss list), the `tab5_dismiss_alert` script the device asks for (event `esphome.tab5_alerte_lue`, through `packages/tab5_evenements.yaml`) when you tap a banner or the info panel, the `sensor.tab5_unavailable_count` counter and a nightly cleanup of stale ids. The `tab5_maj_alertes_ha_bulk` payload itself (max 4 banners, already-dismissed ids filtered out) is built by the `tab5_push_alertes` script.

After a dismiss, the refresh comes from the light push automation (`tab5_ha_hmi_alerts_push` in `packages/tab5_push.yaml`): it triggers on `input_text.tab5_alerts_dismissed` and re-pushes sections 1, 7 and 7b filtered by the dismiss list. The dismiss script no longer triggers the full push automation (it did until 2026-09-08 — a second, heavy push for nothing). Removed on 2026-09-26 for lack of callers: the `tab5_dismiss_info_panel` script and the automation listening to `esphome.tab5_alert_dismiss`, an event the firmware never fires.

Tapping a banner on screen removes it immediately and stores its id here, so a re-push of the same id stays hidden until HA sends a new one. `snippets/tab5_alerts_dismissed_input_text.yaml` is the same helper on its own, if you prefer declaring it in your existing `input_text:` block instead of loading the whole package.

---

### `optionnel/volet_serre_tracking.yaml` — optional
**Not installed by default** (`tab5_optionnel/` in the release archive): copy it into `config/packages/` only for a shutter that reports neither position nor travel, then pick it in « Tab5 · volet à course simulée ». While `script.tab5_volet_action` exists, the blueprint hands the shutter buttons to it instead of the shutter chosen in the automation.

Everything for a roller shutter whose motor reports **no position and no end-stop** (typical cheap Tuya module): the two helpers (an `input_boolean` armed for the measured travel time, an `input_text` carrying the label shown on screen), the central script `tab5_volet_action` called by the Tab5, `tab5_volet_updater`, which pushes the label to `tab5_maj_volet_etat`, and `volet_serre_track_direct_cover`, which updates the helpers when the shutter is commanded some other way (HA UI, sunrise/sunset automation, another integration) so the screen follows.

Adapt the `26 s` travel delay to your own shutter (it appears in the script and in the direct-cover automation).

### `packages/tab5_micro_absence.yaml`
Turns the Tab5 wake word (« Ok Nabu ») **off when nobody is home** and back on when someone returns. It listens 24/7 otherwise (10 ms frames, model + voice activity detection: an estimated 5-15 % of a core plus the I2S bus and the microphone ADC), for nothing when the flat is empty.

- Presence is `zone.home` (number of tracked people at home): no personal entity ID in the file. To follow a single person, replace it with a condition on your `person.*`.
- Off after **10 min** of empty home (a GPS glitch does nothing), on again **as soon as** someone is back.
- A manual choice is kept: the microphone is only switched back on if this automation switched it off (`input_boolean.tab5_micro_coupe_absence`).
- A departure or return missed while the tablet was offline is caught up when it reconnects or when HA restarts.
- The alarm clock still arms its voice « Stop » while ringing, even with the wake word off (firmware side, nothing to do).

After deploying: reload **Input booleans** and **Automations**.

---

## Adapting to your setup

**Nothing to edit in the files** ([ADR-0024](../docs/decisions/0024-packages-without-placeholders.md)). After installing ([docs/installation.md, Step 4](../docs/installation.md#step-4--set-up-the-home-assistant-packages)), pick your sources in HA (*Settings → Devices & services → Entities*, search « Tab5 · »):

| List | Package | What it drives |
|------|---------|----------------|
| Tab5 · source des prévisions | `tab5_meteo_sources` | forecasts and current weather (any `weather.*`) |
| Tab5 · source de la pluie dans l'heure / des vigilances | `tab5_meteo_sources` | Météo-France, OpenWeatherMap, Buienradar, DWD, Met.no, Open-Meteo / MeteoAlarm, DWD, CAP Alerts, or Aucune |
| Tab5 · agenda de travail | `tab5_reglages` | planning, rest days, alarm time; its other events are appointments |
| Tab5 · mot des événements de travail (text) | `tab5_reglages` | which events of the work calendar are work (empty = all) |
| Tab5 · agenda des rendez-vous / des anniversaires / des jours fériés / des vacances scolaires | `tab5_reglages` | calendar popup, reminders, morning briefing |
| Tab5 · pipeline de discussion | `tab5_reglages` | the Assist pipeline of the « Discu » mode (« Aucun » hides its buttons) |
| Tab5 · téléphone, Tab5 · capteur de présence | `tab5_reglages` | screen on / off |
| Tab5 · TV Samsung, Tab5 · adresse de la TV | `tab5_tv` | the TV popup's app buttons (Tizen REST API on port 8001) |
| Tab5 · volet à course simulée | `optionnel/volet_serre_tracking` | the shutter whose travel is simulated |

The action names (`esphome.tab5_ha_hmi_…`) still follow the ESPHome device name `tab5-ha-hmi` set by the firmware.

**Contributors.** `python tools/render_ha_config.py` now only copies the public files into `HomeAssistant_Config/rendered/`. `--check` is still the leak guard (pre-commit, and CI with the `HA_PLACEHOLDERS` secret): it fails if a value listed in your gitignored `placeholders.yaml` (your real entity IDs, one `name: value` per line, template `placeholders.example.yaml`) or a `VOTRE_…` placeholder appears in a public file, and never prints the value. The release archive is built by `tools/publication/archive_ha.py`.

After replacing the files on an existing install: **Developer Tools → YAML**, reload the custom templates, **Input texts**, **Template entities**, then **Scripts**, **Automations** and **REST commands** (or restart HA). The Tab5 receives its first push within a few seconds of connecting to the API.

---

## How the push works (quick summary)

```
HA state change (e.g., the weather entity updates)
  ↓
Automation trigger fires
  ↓
HA calls: action: esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk
          data:
            payload: "0|Auj 30|sunny|16.0|28.0|0|0|0|09h00 - 17h30;1|Ven 31|..."
  ↓
ESPHome receives the service call (Tab5/tab5-api-logic.yaml)
  ↓
C++ parse_and_update_jours_bulk() runs, updates the LVGL labels in one pass
```

The service name seen by HA is `esphome.<device_name>_<service>` — with the stock `name: tab5-ha-hmi`, `tab5_maj_previsions_jours_bulk` becomes `esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk`. Rename the device and every call in the automation has to follow.

The device never polls. It only receives. When nothing changes in HA, the device uses near-zero CPU.

---

---

## Version Française

---

Ce dossier contient le côté Home Assistant de l'intégration Tab5 : automatisations qui poussent des données vers l'appareil, scripts déclenchés par l'appareil, capteurs de template et helpers. Tout est en **packages** Home Assistant (`packages/`), **sans placeholder** depuis le 28/09/2026 ([ADR-0024](../docs/decisions/0024-packages-without-placeholders.md)) : chaque valeur de votre maison se choisit dans Home Assistant (les listes « Tab5 · … »), et la tablette est trouvée par le modèle de son appareil.

> **Une seule source (26/09/2026).** Le Home Assistant de l'auteur fait tourner ces mêmes packages (tels quels depuis le 28/09/2026) : il n'y a plus de version privée. Jusqu'à ce jour, trois fichiers d'exemples à fusionner à la main (`automations_examples.yaml.example`, `scripts_examples.yaml`, `template_sensors_examples.yaml`) étaient tirés de fichiers privés, et ils dérivaient : le package du volet n'avait jamais tourné nulle part, et `script.allumer_pc_tv`, appelé par le firmware, n'existait dans aucun fichier public.

> **Installer = une archive.** Décompressez `tab5_home_assistant.zip` (jointe à chaque release) dans le `config/` de HA, activez `homeassistant: packages: !include_dir_named packages` dans `configuration.yaml` (la seule ligne de YAML à écrire), redémarrez, puis choisissez vos sources (voir [Adapter à votre setup](#adapter-à-votre-setup)). Rien à fusionner dans `automations.yaml` ni `scripts.yaml`. Les automatisations et scripts d'un package sont en lecture seule dans l'interface HA : modifier le fichier, déployer.

---

## Fichiers

### `packages/tab5_reglages.yaml` — vos choix (depuis le 28/09/2026)
Ce qui était des placeholders, choisi dans l'interface de HA (*Paramètres → Appareils et services → Entités*, chercher « Tab5 · ») :
- **Listes** (template selects, choix gardé dans un `input_text.tab5_choix_*` que HA restaure) : « Tab5 · agenda de travail », « … des rendez-vous », « … des anniversaires », « … des jours fériés », « … des vacances scolaires », « Tab5 · téléphone », « Tab5 · capteur de présence », « Tab5 · pipeline de discussion ». Leurs noms sont en français et en anglais (« Tab5 · agenda de travail · work calendar ») ; `default_entity_id` garde leurs entity_id. État = l'entité (ou le pipeline) choisie, ou « Aucun ». Un choix par défaut n'est pris que sans doute possible (le seul agenda d'anniversaires, le seul agenda de jours fériés, le seul agenda de vacances scolaires, le seul téléphone de l'application mobile) ; agenda de travail, rendez-vous, capteur de présence et pipeline de discussion ne sont jamais devinés. Le texte « Tab5 · mot des événements de travail » se tape à la main. Les listes sont à déclencheurs (démarrage, rechargement des modèles, entité ajoutée ou renommée, choix changé) : parcourir `states.binary_sensor` dans un modèle ordinaire le ferait recalculer à chaque changement d'un capteur de la maison.
- **`sensor.tab5_tablette`** : la tablette, trouvée comme par la garde « tablette connectée » (le capteur « HA API Status » d'un appareil ESPHome de modèle `tab5-ha-hmi`) ; ses entités (écran, réveil en cours, raison du redémarrage, uptime, micro, satellite vocal, délai d'annonce des rendez-vous) en attributs, quel que soit le nom de l'appareil (une partie de celles de l'auteur ont un préfixe `salon_`).
- **Miroirs** pour les déclencheurs des automatisations (un déclencheur `state:` veut un entity_id écrit dans le YAML) : `binary_sensor.tab5_presence`, `sensor.tab5_telephone_suivi`, `binary_sensor.tab5_connectee`, `sensor.tab5_demarrage`, `sensor.tab5_rendez_vous_annoncer_avant`, `sensor.tab5_agendas`. Indisponibles tant que rien n'est choisi : rien ne se déclenche.

### `packages/tab5_push.yaml`
Les automatisations de poussée, les scripts qu'elles partagent, les scripts appelés par le Tab5, la réponse des zones optionnelles et le garde-fou `is_primary_active`. Elles poussent les données vers le Tab5 via des appels de service ESPHome natifs ; les blocs envoyés par plusieurs automatisations n'existent qu'une fois, dans les scripts `tab5_push_*`. C'est le package par lequel commencer.

Ce qu'elle pousse :
- **Prévisions journalières (15 jours) :** toutes les 10 min, au changement du calendrier et à la (re)connexion — sérialise 15 × (index, libellé jour, condition, min, max, drapeaux week-end/férié, heures de travail) en chaîne délimitée `|`/`;` vers `tab5_maj_previsions_jours_bulk`
- **Prévisions horaires (10 créneaux) :** deux chunks de 5 via `tab5_maj_previsions_heures_bulk` (l'écran a deux pages horaires)
- **Graphe de pluie court terme :** sur changement de `sensor.tab5_pluie_dans_l_heure` (packages/tab5_meteo_sources.yaml) — **9** barres en **un** appel (`tab5_maj_pluie_1h_bulk`, payload `idx|intensité;…`, index 0–8 = 0/5/10/…/55 min, intensité = libellé Météo-France ou niveau 0–4), depuis la source choisie dans HA : Météo-France, OpenWeatherMap ou aucune
- **Météo actuelle / probabilités :** `tab5_maj_meteo_actuelle` (condition, température, humidité) et `tab5_maj_probabilites` (UV, gel, neige) — au changement (`tab5_ha_hmi_meteo_push`) et à la (re)connexion, script `tab5_push_meteo`
- **État climatisation :** poussé par le blueprint `tab5_emplacements.yaml` depuis la 3.0 (`tab5_maj_clim` : cible, actuelle, mode, preset, ventilation, oscillation), à chaque changement et à la (re)connexion
- **État volet :** `tab5_maj_volet_etat` au changement des helpers (`tab5_volet_updater`) et à la (re)connexion, script `tab5_push_volet` — arme aussi le wake word local « Stop » pendant le mouvement
- **Bandeau info :** `tab5_maj_info_texte` (texte, couleur, id de dismiss) — le code `@ha|…` (mises à jour, erreurs, entités indisponibles, bannière de vigilance), écrit par le Tab5 dans sa langue
- **Vigilances :** `tab5_maj_alerte_meteo_france` (nom historique) — un seul payload délimité `|` : code de pluie, niveau global, puis 11 phénomènes (les 9 de Météo-France, brouillard, feux de forêt), depuis `sensor.tab5_vigilance` (Météo-France, MeteoAlarm, DWD, CAP Alerts ou aucune)
- **File d'alertes HA :** `tab5_maj_alertes_ha_bulk` — jusqu'à 4 bandeaux dans le rotateur central (voir `packages/tab5_alerts.yaml`)

Aussi dans le package, hors poussée : **`tab5_screen_presence_wifi`** allume l'écran quand le capteur de présence détecte quelqu'un (ou au retour du téléphone) s'il est éteint, et l'éteint après **15 min** sans présence (ou au départ du téléphone) s'il est allumé et que le réveil ne sonne pas. Écran éteint, le firmware met LVGL en pause ; un toucher ou une tape sur la dalle le rallume.

Les températures, les lumières, le PC, la TV, le téléphone et les plantes ne passent **pas** par ce package : depuis la 3.0, le blueprint `tab5_emplacements.yaml` les pousse (`tab5_maj_emplacements`). La tablette ne s'abonne plus à aucune entité de votre maison.

**Plus de renvoi périodique d'un état inchangé (26/09/2026) :** météo actuelle, probabilités, clim et volet repartaient toutes les 10 min en plus de leurs poussées au changement (576 appels par jour, chacun repeint par l'appareil). La poussée complète ne les envoie plus qu'à la (re)connexion, au démarrage de Home Assistant (la tablette se reconnecte souvent avant que les automatisations soient actives, et son `esphome.tab5_connected` est alors perdu) et au retour à `on` de `input_boolean.is_primary_active`.

**Traffic pacing :** l'automatisation utilise `delay: 1s` entre chaque bloc push et `delay: 150ms` dans les boucles de prévisions. Cela empêche plusieurs gros payloads de saturer le buffer de sockets TCP de l'ESP32-P4 simultanément avec le flux audio I2S actif.

---

**Scripts.** Depuis la 3.0, le Tab5 n'appelle plus de script pour vos appareils : ses commandes vont au blueprint (voir plus bas). Le blueprint appelle encore `script.tab5_volet_action` (package du volet) et `script.tab5_tv_app` (package TV) ; les scripts d'agenda, de réveil et d'acquittement sont lancés par `packages/tab5_evenements.yaml` quand la tablette le demande (voir plus bas). La tablette elle-même n'appelle plus aucune action ([ADR-0025](../docs/decisions/0025-events-only.md)).

Il contient aussi les **scripts de poussée** `tab5_push_alertes` (sections 1, 7 et 7b : vigilance Météo-France, bandeau info, rotateur d'alertes HA — MAJ, capteurs `problem` et compte d'indisponibles relevés une fois par passage), `tab5_push_meteo` et `tab5_push_volet`. Ceux-là sont appelés *par les automatisations*, pas par le Tab5 : chaque bloc n'existe qu'une fois au lieu d'être recopié dans la poussée complète et dans son automatisation au changement.

**Capteur de template** (désormais dans `packages/tab5_meteo_sources.yaml`, voir plus bas). `Tab5 Pluie dans l'heure` transforme la prévision de pluie en **code**, `@niveau,début` (niveau -1 pas de données, 0 sec, 1 à 4 faible à très forte, 5 intensité inconnue ; début = epoch UTC de la pluie, 0 s'il pleut déjà). Depuis le lot 4c (27/09/2026), le Tab5 écrit lui-même la phrase, dans sa langue (« Averses dans 12 mn » / “Showers in 12 min”), et décompte les minutes avec sa propre horloge : le capteur ne change plus qu'avec les données Météo-France, plus à chaque minute. Le bandeau info et le rotateur d'alertes HA partent aussi en codes (`@ha|…`, `@maj:`, `@indispo:`). **Déployer ce package après le firmware du lot 4c** : un firmware plus ancien afficherait les codes tels quels.

**Zones optionnelles (lot 5, [ADR-0018](../docs/decisions/0018-optional-zones-confirmed-by-ha.md)).** Depuis la 3.0, c'est le blueprint qui répond à la demande des zones de la tablette : un emplacement vide, ou une entité qui n'existe pas, disparaît de l'écran. Les poussées de ce package suivent la même règle : `tab5_push_volet` n'envoie rien sans volet. Voir [Adapter à sa maison](../docs/installation.md#adapter-à-sa-maison).

**Garde-fou `input_boolean.is_primary_active`.** Toutes les poussées en dépendent ; `force_primary_active_on_boot` le remet à `on` au démarrage de HA, et `packages/tab5_health.yaml` prévient s'il reste à `off` 5 min. C'est un reste d'une ancienne installation à deux instances ([ADR-0008](../docs/decisions/0008-single-ha-instance.md)) : avec un seul Home Assistant, il reste simplement à `on`.

L'exemple de réponse de l'assistant (moteur → popup Assistant) est passé dans `snippets/tab5_assist_reponse_exemple.yaml` : dans un package, il aurait été actif chez tout le monde.

---

### `blueprints/automation/tab5/tab5_emplacements.yaml` — choisir ses appareils (depuis la 3.0)
Le Tab5 ne connaît plus aucune entité de votre maison ([ADR-0019](../docs/decisions/0019-logical-slots-blueprint.md)) : ce **blueprint** relie ses emplacements à vos entités. Importez-le (*Paramètres → Automatisations et scènes → Blueprints → Importer un blueprint*, puis collez son URL GitHub) ou copiez-le dans `config/blueprints/automation/tab5/`, puis créez **une automatisation par tablette**. Toutes les entrées sont facultatives :
- **pièces 1 à 5** (firmware 3.2 et plus, [ADR-0023](../docs/decisions/0023-rooms-generic-tiles.md)) : un nom et jusqu'à cinq appareils chacune, dans l'ordre des tuiles — une pièce par page de la rangée du bas (pièce 1 = accueil). Lumières, interrupteurs, volets et vannes, lecteurs, scènes/scripts/boutons, capteurs, capteurs binaires, personnes et serrures, clim. Une personnalisation facultative par appareil : nom, icône, comportement (allumer seulement, confirmer, lecture seule) ;
- TV et sa télécommande, batterie du téléphone, température et humidité de la pièce, seconde température (serre), clim, pots 1 à 5 (le capteur d'humidité ; conductivité, éclairement, température et batterie sont pris sur le même appareil), agenda de travail ;
- le **réglage 3.x** de l'accueil (replié) : lumières 1 à 3, PC, volet. Un firmware 3.0/3.1 n'utilise que lui ; un firmware 3.2 aussi, tant que la pièce 1 est vide.

L'automatisation :
- lit la version de la tablette (`sw_version` de l'appareil, trouvé par son modèle) : à partir de 3.2.0, elle décrit les pièces par `tab5_maj_tuiles` (`pR|nom;tRT|type|icône|options|complément|nom;…`) à la (re)connexion, au rechargement des automatisations, au démarrage de Home Assistant et à la demande des zones, puis pousse l'état de chaque tuile (`tRT|état|valeur|couleur`, dans `tab5_maj_emplacements`). En dessous de 3.2.0, ou si la version est illisible, elle n'appelle jamais `tab5_maj_tuiles` (une action absente est une erreur que `continue_on_error` n'attrape pas) ;
- pousse les emplacements 3.x par `tab5_maj_emplacements` (`clé|état|valeur;…`) : tous à la (re)connexion et au démarrage de Home Assistant, lumières, PC et TV à chaque changement visible, les mesures (et les capteurs placés dans une pièce) groupées toutes les 5 minutes ; la clim par `tab5_maj_clim`, et un volet qui signale sa course par `tab5_maj_volet_etat` ;
- exécute les commandes de l'écran (événements `esphome.tab5_action` : bascule, luminosité, couleur, consigne, mode, touches et applications de la TV, volet…) : un emplacement 3.x sur l'entité choisie, une tuile (`tRT`) ou une pièce (`pR`, « Tout éteindre ») selon le domaine de l'entité de la tuile — jamais sur une entité qu'aucune tuile ne porte. Un événement n'exige pas l'option « autoriser l'appareil à effectuer des actions Home Assistant » ;
- répond à la demande des zones (`esphome.tab5_zones`, [ADR-0018](../docs/decisions/0018-optional-zones-confirmed-by-ha.md)) : un emplacement vide, ou une entité qui n'existe pas, disparaît de l'écran.

Aucun placeholder : le fichier est générique. Changer d'appareil = modifier l'automatisation dans l'interface de HA, ni flash ni redémarrage. Le bloc entre `# >>> icones` et `# <<< icones` (la palette des icônes) est écrit par `tools/gen_tuiles_icones.py`.

### `packages/tab5_evenements.yaml` — les demandes de la tablette (événements seulement)
Depuis l'[ADR-0025](../docs/decisions/0025-events-only.md), le firmware n'appelle plus aucune action de Home Assistant : l'option « Autoriser l'appareil à effectuer des actions Home Assistant » n'est plus nécessaire. La tablette envoie des événements `esphome.tab5_*`, et l'unique automatisation de ce package (`tab5_evenements`, `mode: parallel`) traduit chacun en une action **fixe**, pour un appareil de modèle `tab5-ha-hmi` seulement, sur les entités de cette tablette (trouvées par `device_entities`, aucune entité à régler) :

| Événement (données) | Action |
|---|---|
| `tab5_reveil_annonce` | `script.tab5_reveil_annonce` (plus bas) |
| `tab5_annonce` (`message`) | `assist_satellite.announce` sur le satellite de la tablette (rendez-vous, « Volet arrêté ») |
| `tab5_calendrier_mois` (`annee`, `mois`), `tab5_calendrier_jour` (`date`) | `script.tab5_calendrier_mois` / `_jour` (plus bas) |
| `tab5_alerte_lue` (`alert_id`) | `script.tab5_dismiss_alert` (plus bas) |
| `tab5_voix_stop` | `media_player.media_stop` sur le lecteur de la tablette |
| `tab5_mode_assistant` (`option`) | `select.select_option` sur le select de pipeline de la tablette, seulement si l'option existe |
| `tab5_maj_ecran` | « MAJ Écran » : `input_boolean.is_primary_active` à on, puis `automation.trigger` de la poussée complète (trouvée par son id, `tab5_ha_hmi_updater`) |
| `tab5_recharger_automatisations` | `automation.reload` |
| `tab5_redemarrage_ha_confirme` | `homeassistant.restart` — envoyé seulement par « Confirmer » de l'écran de confirmation de la tablette |

Aucun nom d'action ni d'entité ne vient de l'événement. Un script absent (package non installé) ou une option absente sont ignorés sans bruit. **À déployer avant un firmware plus récent que la 3.1** ; avec une 3.1 ou plus ancienne (qui appelle encore les actions), il attend simplement. Voir [Passer d'une 3.1 à la suite](../docs/installation.md#passer-dune-31-à-la-suite). Aucun placeholder : le fichier est générique.

### `packages/tab5_meteo_sources.yaml`
Adaptateurs météo (lot 4c-2, 27/09/2026). Deux listes choisissent la source **dans Home Assistant, sans YAML** : « Tab5 · source de la pluie dans l'heure » (Météo-France / OpenWeatherMap / Buienradar / DWD / Met.no / Open-Meteo / Aucune) et « Tab5 · source des vigilances » (Météo-France / MeteoAlarm / DWD / CAP Alerts / Aucune). Deux capteurs normalisés ramènent n'importe quelle source à ce que lit le Tab5, et les poussées ne lisent qu'eux :
- `sensor.tab5_pluie_dans_l_heure` : état = code de pluie `@niveau,début`, attribut `barres` = les 9 barres. Capteur à déclencheurs : il lance `openweathermap.get_minute_forecast` (série à la minute en mm/h, lue dans le cache de l'intégration, sans appel d'API en plus) ou, depuis le 29/09/2026, `rest_command.tab5_pluie` du même package pour quatre services sans clé interrogés à `zone.home` arrondi à 0,01° (radars Buienradar, DWD par Bright Sky, Met.no Nowcast ; modèle Open-Meteo au pas de 15 min), seulement quand ils sont choisis. Chaque réponse devient une même série (début, durée, mm/h) que lisent l'état et les barres. Seuils : < 0,1 sec, < 2,5 faible, < 7,6 modérée, < 50 forte, au-delà très forte ;
- `sensor.tab5_vigilance` : état = niveau global (Vert / Jaune / Orange / Rouge), attribut `phenomenes` = 11 niveaux. Les codes MeteoAlarm sont rangés dans les cases du Tab5 (vent, neige-verglas, orages, brouillard, canicule, grand froid, submersion, feux de forêt, avalanches, pluie, inondation) ; il n'expose qu'une alerte à la fois. Le DWD (29/09/2026 : toutes les alertes de la région, préavis écartés par leur `urgency`, car les identifiants suivent la langue de HA) et CAP Alerts (HACS, une entité par alerte ; case tirée du type de phénomène MeteoAlarm ou de l'icône que déduit l'intégration) passent par la macro `tab5_vigilance` de `custom_templates/tab5_vigilance.jinja`, importée dans leur seule branche : alertes en cours ou qui commencent dans les 24 h, niveau global = la plus forte.

- **Source des prévisions** (lot 4c-3) : la liste « Tab5 · source des prévisions » propose les entités `weather.*` (choix gardé dans `input_text.tab5_meteo_previsions` ; vide ou disparue : la ville Météo-France, sinon la première entité météo). Les entités des fournisseurs sont trouvées par `sensor.tab5_sources_meteo` (à déclencheurs) : les capteurs Météo-France du même appareil que la ville (`_next_rain`, `_weather_alert`, `_uv`, `_freeze_chance`, `_snow_chance`), la météo OpenWeatherMap, le binary_sensor MeteoAlarm (par son attribution), les capteurs DWD et CAP Alerts (attributs `dwd` et `cap` : ceux de l'intégration, sinon reconnus à leurs attributs). `sensor.tab5_meteo` donne sa condition (état) et `temperature`, `humidite`, `uv`, `gel`, `neige`, ainsi que ce que l'entité sait fournir (`type_jours` = daily, twice_daily ou hourly ; `heures_ok`). La poussée complète ne demande que les types gérés et regroupe par date les demi-journées ou les heures.

OpenWeatherMap (prévisions et pluie) a été essayé sur l'installation de l'auteur le 27/09/2026 ; la branche MeteoAlarm et le regroupement des demi-journées et des heures n'ont été testés qu'avec des données simulées dans le moteur de modèles de Home Assistant. Le DWD a été ajouté à l'installation de l'auteur le 29/09/2026 (un jour sans alerte) ; ses alertes et CAP Alerts ont été testés avec des données simulées (moteur de modèles, CI d'installation à neuf). Installation : [fournisseurs météo](../docs/installation.md#fournisseurs-météo).

### `packages/tab5_health.yaml`
Package de surveillance santé : six automations de garde qui alertent quand le pipeline de push se dégrade silencieusement. Le Tab5 étant push-only (voir `docs/decisions/0001-push-only-zero-polling.md`), un écran figé ne lève aucune erreur par lui-même — ces automations sont le filet de sécurité côté HA.

Ce qui est surveillé :
- **`input_boolean.is_primary_active` OFF depuis plus de 5 min** — ce booléen conditionne toutes les automations de push ; bloqué sur OFF, l'écran se fige silencieusement (incident réel, voir `docs/troubleshooting.md`)
- **Une nouvelle heure de démarrage sur `Tab5 Uptime`** (un horodatage, publié une fois par démarrage depuis le 26/09/2026) — reboot inattendu de l'appareil (brownout, crash firmware, coupure d'alimentation) ; une simple coupure Wi-Fi sans reboot revient avec la même heure de démarrage et ne déclenche *pas* ; un redémarrage demandé non plus (depuis le 27/09/2026 : mise à jour, bouton « Redémarrage Système », changement de langue, reset par l'USB, lus dans `Tab5 Raison du redémarrage`), et la notification donne la raison
- **`HA API Status` off/unavailable depuis plus de 2 min** — appareil injoignable, toutes les poussées échouent pendant la coupure
- **Une automation Tab5 journalise « Error rendering »** — une action de poussée n'a pas pu rendre son template et `continue_on_error` l'a sautée en silence (incident réel du 18/09/2026 : Météo-France a retiré `templow` du 15ᵉ jour). Exige `system_log: fire_event: true` dans `configuration.yaml` (redémarrage nécessaire) — sans lui la garde est chargée mais ne se déclenche jamais. Exclure `system_log_event` du recorder. Au plus une notification par heure tant que l'erreur se répète
- **Le journal des démarrages et des coupures envoyé par le Tab5** (`esphome.tab5_journal`, depuis le 26/09/2026) — le firmware (`Tab5/tab5_journal.cpp`) garde ses erreurs, et ses avertissements tant que HA n'est pas connecté, dans une mémoire qui survit aux redémarrages logiciels et aux plantages (plus une copie NVS quand le Wi-Fi manque depuis 90 s, pour les coupures de courant), et les envoie à la reconnexion de HA pour : un reset anormal ou un rapport de plantage (celui d'ESPHome : PC et pile d'appels des deux cœurs), un Wi-Fi absent 90 s ou plus (lien du co-processeur Wi-Fi, ou routeur), un démarrage qui n'a jamais joint HA, une erreur (ESPHome ou ESP-IDF, pilote ESP-Hosted compris) après la connexion à HA, ou HA joint plus de 90 s après le démarrage. Les lignes d'avant la première connexion à HA ne sont que du contexte : un démarrage normal n'envoie rien, une absence de HA avec Wi-Fi présent non plus. Notification persistante à chaque fois ; téléphone seulement si `grave` (reset anormal, plantage, ou Wi-Fi absent 90 s). Aucun prérequis `system_log`
- **Des fichiers Home Assistant plus anciens que le firmware** (depuis le 29/09/2026) — `sensor.tab5_version_des_fichiers_ha` porte la release de l'archive d'où viennent ces fichiers (« dépôt » s'ils sont copiés depuis le dépôt : jamais comparés) ; `binary_sensor.tab5_fichiers_ha_en_retard` s'allume quand la tablette tourne une version X.Y plus récente (majeure ou mineure : une version corrective seule ne compte pas, elle peut ne demander qu'un fichier). Notification persistante seulement, retirée quand les versions se rejoignent

Notes de conception :
- Les gardes notifient via un seul script, `script.tab5_health_notify` (notification persistante + `notify.notify`) : les canaux s'adaptent à un seul endroit ; chaque canal porte `continue_on_error: true`, un canal en échec ne bloque pas l'autre
- Aucun template n'utilise `now()` brut — la détection repose sur les fenêtres `for:` des déclencheurs et sur `trigger.from_state` / `trigger.to_state`
- Les comparaisons numériques utilisent des défauts `| float(0)` (sécurité au boot)

C'est un *package* HA autonome ; activez d'abord les packages dans `configuration.yaml` :

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Seul `notify.notify` peut demander à être adapté (le canal de notification). Les entités de la tablette ne sont plus nommées dans le fichier : les gardes lisent les miroirs `sensor.tab5_demarrage` et `binary_sensor.tab5_connectee` (`packages/tab5_reglages.yaml`) ; l'arrivée d'une nouvelle tablette n'est pas prise pour un redémarrage.

---

### `packages/tab5_calendar.yaml`
Backend du **popup calendrier** du firmware (appui long sur l'horloge). Deux scripts demandés *par l'appareil* (événements `esphome.tab5_calendrier_mois` / `_jour`, lancés par `packages/tab5_evenements.yaml`), tous deux `mode: queued` (`max: 10` ; `restart` jusqu'au 28/09/2026, qui ne laissait aboutir qu'une des trois demandes que la tablette envoie d'affilée — le mois affiché et ses voisins) :

- **`tab5_calendrier_mois`** (`annee`, `mois`) — lit les quatre agendas choisis dans les listes « Tab5 · agenda … » (travail, jours fériés, rendez-vous, anniversaires ; un agenda laissé sur « Aucun » est sauté) sur le mois demandé et repousse `esphome.<device>_tab5_maj_calendrier_mois` : chaîne de 62 hex (2 par jour — bits : travail / férié / vacances scolaires / RDV / anniversaire) + 31 champs d'heures de travail séparés par `|` + un champ `details` (lignes de détail jour séparées par `~` — exigé par le firmware depuis le schéma du 25/07/2026, envoyé vide ici)
- **`tab5_calendrier_jour`** (`date`) — construit les lignes de détail du jour (`type|texte;...`, max 6) et pousse `esphome.<device>_tab5_maj_calendrier_jour`

Les vacances scolaires viennent de l'agenda choisi dans « Tab5 · agenda des vacances scolaires » (tous ses événements ; en France, le fichier ICS du ministère pour votre zone par l'intégration Remote Calendar, voir [docs/installation.md](../docs/installation.md#étape-4--installer-les-packages-home-assistant)) ; jusqu'au 29/09/2026, d'une table fixe de la zone A. Les événements de travail sont ceux dont le titre contient un mot de « Tab5 · mot des événements de travail » (sans casse ; vide = tout l'agenda de travail). Le calendrier Google des jours fériés mélange vrais fériés et fêtes civiles, d'où la liste blanche `feries_connus` ; un agenda de l'intégration Jours fériés ne contient que des jours fériés, tous retenus. Même installation package que ci-dessus.

Les quatre macros Jinja partagées par ses templates (`ev_start`, `ev_end`, `ev_summary`, `couvre` — une seule normalisation d'un événement `calendar.get_events`, journée entière ou horodaté) vivent dans `custom_templates/tab5_calendar.jinja`. Déployez ce fichier dans le `config/custom_templates/` de HA et appelez `homeassistant.reload_custom_templates` (ou redémarrez) **avant** de charger le package : sans lui, les deux scripts échouent à leur ligne d'import.

---

### `packages/tab5_reveil.yaml`
Ce que Home Assistant apporte au **réveil** du firmware — et rien de plus. **Le réveil lui-même ne dépend pas de ce fichier** : l'appareil calcule son heure depuis l'horloge SNTP et les horaires de travail qu'il garde déjà en cache, et sonne une mélodie synthétisée localement. Arrêtez HA, le réveil sonne quand même ; seuls le briefing parlé et les rappels de rendez-vous manquent. Ne jamais déplacer ici la décision de sonner.

- **`tab5_rdv_prochains`** — pousse les rendez-vous *horodatés* des 24 prochaines heures vers `esphome.<device>_tab5_maj_rdv_prochains`, au format `epoch|titre~epoch|titre~…` (8 maximum). Les événements de travail (mot de « Tab5 · mot des événements de travail ») sont exclus : leurs horaires servent déjà à calculer l'heure de réveil, et ce ne sont pas des rendez-vous. **C'est l'appareil qui tient le compte à rebours**, donc une coupure HA entre la poussée et l'échéance ne fait rien rater.
- **`tab5_reveil_annonce`** — le briefing parlé du matin (heure, horaires du jour, prochain rendez-vous, température) dans la langue de l'écran (select « Langue » : français, anglais, allemand, néerlandais, espagnol ou italien), demandé *par le firmware* (événement `esphome.tab5_reveil_annonce`, via `packages/tab5_evenements.yaml`) quand `switch.tab5_alarm_tts` est actif, et uniquement au premier déclenchement — pas aux répétitions.
- **automation `tab5_rdv_push`** — entretient la liste : toutes les 5 min, sur changement de calendrier, sur `esphome.tab5_connected` (sinon la liste reste vide après un redémarrage de la tablette), et quand le délai d'annonce change.

Les agendas sont les listes « Tab5 · agenda des rendez-vous » et « Tab5 · agenda de travail » ; le satellite qui parle est celui de la tablette détectée. Rien à modifier. **L'heure du réveil vient de l'agenda de travail** (poussé avec les prévisions jours) : laissé sur « Aucun », tous les jours sont des jours de repos. Même installation package que ci-dessus.

---

### `packages/tab5_alerts.yaml`
Backend de la **file d'alertes HA** — panneaux 4 à 7 de la carte centrale rotative. Fournit le helper `input_text.tab5_alerts_dismissed` (liste de dismiss), le script `tab5_dismiss_alert` que l'appareil demande (événement `esphome.tab5_alerte_lue`, via `packages/tab5_evenements.yaml`) au tap sur un bandeau ou sur le panneau info, le compteur `sensor.tab5_unavailable_count` et une purge nocturne des ids périmés. Le payload `tab5_maj_alertes_ha_bulk` lui-même (4 bandeaux max, ids déjà masqués filtrés) est construit par le script `tab5_push_alertes`.

Après un acquittement, le rafraîchissement vient de l'automation « push léger » (`tab5_ha_hmi_alerts_push` dans `packages/tab5_push.yaml`) : elle se déclenche sur `input_text.tab5_alerts_dismissed` et repousse les sections 1, 7 et 7b filtrées par la liste. Le script d'acquittement ne déclenche plus l'automation de push complète (il le faisait jusqu'au 08/09/2026 — un second push, lourd, pour rien). Retirés le 26/09/2026 faute d'appelant : le script `tab5_dismiss_info_panel` et l'automation qui écoutait `esphome.tab5_alert_dismiss`, un événement que le firmware n'émet jamais.

Un tap sur un bandeau le retire tout de suite et mémorise son id ici : un re-push du même id reste masqué tant que HA n'envoie pas un id différent. `snippets/tab5_alerts_dismissed_input_text.yaml` contient le helper seul, si vous préférez le déclarer dans votre bloc `input_text:` existant plutôt que charger tout le package.

---

### `optionnel/volet_serre_tracking.yaml` — optionnel
**Pas installé par défaut** (`tab5_optionnel/` dans l'archive de la release) : copiez-le dans `config/packages/` seulement pour un volet qui ne signale ni sa position ni sa course, puis choisissez-le dans « Tab5 · volet à course simulée ». Tant que `script.tab5_volet_action` existe, le blueprint lui confie les boutons du volet au lieu du volet choisi dans l'automatisation.

Tout ce qu'il faut pour un volet dont le moteur ne renvoie **ni position ni fin de course** (module Tuya bas de gamme typique) : les deux helpers (un `input_boolean` armé pendant la durée de course mesurée, un `input_text` qui porte le libellé affiché à l'écran), le script central `tab5_volet_action` appelé par le Tab5, `tab5_volet_updater`, qui pousse le libellé vers `tab5_maj_volet_etat`, et `volet_serre_track_direct_cover`, qui met les helpers à jour quand le volet est commandé autrement (interface HA, automatisation lever/coucher, autre intégration) pour que l'écran suive.

Adaptez le délai de course de `26 s` à votre volet (il figure dans le script et dans l'automatisation de suivi direct).

### `packages/tab5_micro_absence.yaml`
Coupe le mot d'activation du Tab5 (« Ok Nabu ») **quand personne n'est à la maison**, et le rallume au retour. Sinon il écoute 24 h/24 (trames de 10 ms, modèle + détection de voix : 5 à 15 % d'un cœur plus le bus I2S et l'ADC du micro, estimation), pour rien quand l'appartement est vide.

- Présence = `zone.home` (nombre de personnes suivies à la maison) : aucun identifiant personnel dans le fichier. Pour ne suivre qu'une personne, remplacer par une condition sur votre `person.*`.
- Coupure après **10 min** de maison vide (un saut du GPS ne fait rien), rallumage **dès** le retour.
- Un choix manuel est respecté : le micro n'est rallumé que si c'est cette automation qui l'a coupé (`input_boolean.tab5_micro_coupe_absence`).
- Un départ ou un retour manqué pendant que la tablette était hors ligne est rattrapé à sa reconnexion ou au redémarrage de HA.
- Le réveil arme quand même son « Stop » vocal pendant la sonnerie, micro coupé ou non (côté firmware, rien à faire).

Après le déploiement : recharger **Entrées booléennes** et **Automatisations**.

---

## Adapter à votre setup

**Rien à modifier dans les fichiers** ([ADR-0024](../docs/decisions/0024-packages-without-placeholders.md)). Après l'installation ([docs/installation.md, étape 4](../docs/installation.md#étape-4--installer-les-packages-home-assistant)), choisissez vos sources dans HA (*Paramètres → Appareils et services → Entités*, chercher « Tab5 · ») :

| Liste | Package | Ce qu'elle règle |
|-------|---------|------------------|
| Tab5 · source des prévisions | `tab5_meteo_sources` | prévisions et météo du moment (n'importe quelle `weather.*`) |
| Tab5 · source de la pluie dans l'heure / des vigilances | `tab5_meteo_sources` | Météo-France, OpenWeatherMap, Buienradar, DWD, Met.no, Open-Meteo / MeteoAlarm, DWD, CAP Alerts, ou Aucune |
| Tab5 · agenda de travail | `tab5_reglages` | planning, jours de repos, heure du réveil ; ses autres événements sont des rendez-vous |
| Tab5 · mot des événements de travail (texte) | `tab5_reglages` | quels événements de l'agenda de travail sont du travail (vide = tous) |
| Tab5 · agenda des rendez-vous / des anniversaires / des jours fériés / des vacances scolaires | `tab5_reglages` | popup calendrier, rappels, briefing du matin |
| Tab5 · pipeline de discussion | `tab5_reglages` | le pipeline Assist du mode « Discu » (« Aucun » masque ses boutons) |
| Tab5 · téléphone, Tab5 · capteur de présence | `tab5_reglages` | allumage / extinction de l'écran |
| Tab5 · TV Samsung, Tab5 · adresse de la TV | `tab5_tv` | boutons d'applications du popup TV (API REST Tizen, port 8001) |
| Tab5 · volet à course simulée | `optionnel/volet_serre_tracking` | le volet dont la course est simulée |

Les noms d'actions (`esphome.tab5_ha_hmi_…`) suivent toujours le nom d'appareil ESPHome `tab5-ha-hmi` donné par le firmware.

**Contributeurs.** `python tools/render_ha_config.py` ne fait plus que copier les fichiers publics dans `HomeAssistant_Config/rendered/`. `--check` reste le garde-fou de fuite (pre-commit, et la CI avec le secret `HA_PLACEHOLDERS`) : il échoue si une valeur de votre `placeholders.yaml` gitignoré (vos identifiants réels, une ligne `nom: valeur`, modèle `placeholders.example.yaml`) ou un placeholder `VOTRE_…` apparaît dans un fichier public, sans jamais afficher la valeur. L'archive des releases est construite par `tools/publication/archive_ha.py`.

Après avoir remplacé les fichiers sur une installation existante : **Outils de développement → YAML**, rechargez les templates personnalisés, **Entrées de texte**, **Entités de template**, puis **Scripts**, **Automatisations** et **Commandes REST** (ou redémarrez HA). Le Tab5 reçoit son premier push quelques secondes après sa connexion à l'API.

---

## Comment fonctionne le push (résumé rapide)

```
Changement d'état HA (ex: l'entité météo se met à jour)
  ↓
Déclencheur d'automatisation se déclenche
  ↓
HA appelle : action: esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk
             data:
               payload: "0|Auj 30|sunny|16.0|28.0|0|0|0|09h00 - 17h30;1|Ven 31|..."
  ↓
ESPHome reçoit l'appel de service (Tab5/tab5-api-logic.yaml)
  ↓
La fonction C++ parse_and_update_jours_bulk() s'exécute et met à jour tous les labels LVGL en une passe
```

Le nom vu par HA est `esphome.<nom_appareil>_<service>` — avec le `name: tab5-ha-hmi` livré, `tab5_maj_previsions_jours_bulk` devient `esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk`. Renommez l'appareil et tous les appels de l'automatisation doivent suivre.

L'appareil ne poll jamais. Il reçoit seulement. Quand rien ne change dans HA, l'appareil utilise un CPU quasi nul.
