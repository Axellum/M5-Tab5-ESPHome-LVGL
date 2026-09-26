# Home Assistant Configuration for Tab5

## English · [Français](#version-française)

---

This folder contains the Home Assistant side of the Tab5 integration: automations that push data to the device, scripts triggered by the device, template sensors and helpers. Everything is a Home Assistant **package** (`packages/`), with placeholder entity IDs.

> **One source (2026-09-26).** The author's Home Assistant runs the *rendered* copies of these very packages: there are no private versions anymore. Until that day, three hand-merged example files (`automations_examples.yaml.example`, `scripts_examples.yaml`, `template_sensors_examples.yaml`) were derived from private files, and they drifted: the shutter package had never run anywhere, and `script.allumer_pc_tv`, called by the firmware, existed in no public file.

> **Install = packages.** Enable `homeassistant: packages: !include_dir_named packages` in `configuration.yaml`, then copy the rendered packages into `config/packages/` (see [Adapting to your setup](#adapting-to-your-setup)). Nothing to merge into `automations.yaml` or `scripts.yaml`. Automations and scripts defined in a package are read-only in the HA UI: change the file, render, deploy.

---

## Files

### `packages/tab5_push.yaml`
The push automations, the scripts they share, the scripts the Tab5 calls, the rain template sensor and the `is_primary_active` guard. They push data to the Tab5 via native ESPHome service calls; blocks sent from more than one automation live once in the `tab5_push_*` scripts. This is the package to start from.

What it pushes:
- **Daily forecast (15 days):** every 10 min, on calendar changes and on (re)connection — serializes 15 × (index, day label, condition, min, max, weekend/holiday flags, work hours) into a `|`/`;`-delimited string sent to `tab5_maj_previsions_jours_bulk`
- **Hourly forecast (10 slots):** two chunks of 5 through `tab5_maj_previsions_heures_bulk` (the screen has two hourly pages)
- **Short-term rain chart:** on `sensor.*_next_rain` state change — **9** bars in **one** call (`tab5_maj_pluie_1h_bulk`, payload `idx|intensity;…`, index 0–8 = 0/5/10/…/55 min) built from Météo-France's `v1/vision/rain` data
- **Current weather / probabilities:** `tab5_maj_meteo_actuelle` (condition, temperature, humidity) and `tab5_maj_probabilites` (UV, frost, snow) — when they change (`tab5_ha_hmi_meteo_push`) and on (re)connection, script `tab5_push_meteo`
- **Climate state:** dedicated fast-path automation `tab5_ha_hmi_clim_push` (no delay, `mode: restart`) and on (re)connection — `tab5_maj_clim` (target, current, mode, preset, fan, swing), script `tab5_push_clim`
- **Shutter state:** `tab5_maj_volet_etat` when the helpers change (`tab5_volet_updater`) and on (re)connection, script `tab5_push_volet` — also arms the device-local “Stop” wake word while the shutter moves
- **Info banner:** `tab5_maj_info_texte` (text, colour, dismiss id) — 3-day calendar recap or a weather-alert banner
- **Météo-France vigilance:** `tab5_maj_alerte_meteo_france` — a single 11-field `|`-delimited payload
- **HA alert queue:** `tab5_maj_alertes_ha_bulk` — up to 4 banners in the central rotator (see `packages/tab5_alerts.yaml`)

Also in the package, not a push: **`tab5_screen_presence_wifi`** switches the screen backlight on when the presence sensor detects someone (or the phone comes home) if it is off, and off after **15 min** without presence (or when the phone leaves) if it is on and the alarm is not ringing. With the screen off the firmware pauses LVGL; a touch or a tap on the panel wakes it.

Room temperatures, humidity, light states and plant moisture do **not** go through these services: they are “mirror” entities (`platform: homeassistant` in `Tab5/tab5-sensors-domotique.yaml`), which HA syncs automatically — nothing to write on the HA side.

**No periodic re-push of unchanged state (2026-09-26):** current weather, probabilities, climate and shutter used to be re-sent every 10 min on top of their on-change pushes (576 calls a day, each one repainted by the device). The full push now sends them only on (re)connection and when `input_boolean.is_primary_active` comes back `on`.

**Traffic pacing:** the automation uses `delay: 1s` between each push block and `delay: 150ms` within forecast loops. This prevents multiple large payloads from overwhelming the ESP32-P4's TCP socket buffer simultaneously with the active I2S audio stream.

---

**Scripts.** Called **by** the Tab5 (from a `homeassistant.service:` in `Tab5/tab5-api-logic.yaml` or an LVGL `on_short_click:`), not the other way round: `allumer_leds`, and `allumer_pc_tv` for the « PC Bureau » button (TV on → turn it off; otherwise turn the PC and the TV on). Simple pass-through: it keeps the ESPHome code thin and the logic on the HA side where it belongs.

Also the **push scripts** `tab5_push_alertes` (sections 1, 7 and 7b: Météo-France vigilance, info banner, HA alert rotator — updates, `problem` sensors and the unavailable count are read once per run), `tab5_push_meteo`, `tab5_push_clim` and `tab5_push_volet`. These are called *by the automations*, not by the Tab5: each block exists once instead of being copied into the full push and into its on-change automation.

**Template sensor.** `Phrase Prochaine Pluie` turns the Météo-France next-rain forecast into a short sentence the Tab5 shows (`"Pluie dans 10 min"`, `"Pas de pluie prévue"`, `"Averses possibles"`). It runs on the HA side rather than on the device to keep the C++ code simple.

**Guard `input_boolean.is_primary_active`.** Every push is conditioned on it; `force_primary_active_on_boot` turns it back on when HA starts, and `packages/tab5_health.yaml` warns if it stays off for 5 min. It is a leftover of a former two-instance setup ([ADR-0008](../docs/decisions/0008-single-ha-instance.md)): on a single Home Assistant it simply stays on.

The assistant-reply example (engine → assistant popup) moved to `snippets/tab5_assist_reponse_exemple.yaml`: inside a package it would have been active for everyone.

---

### `packages/tab5_health.yaml`
Health-monitoring package: five guard automations that alert when the push pipeline silently degrades. Because the Tab5 is push-only (see `docs/decisions/0001-push-only-zero-polling.md`), a stale screen raises no error on its own — these automations are the HA-side safety net.

What it watches:
- **`input_boolean.is_primary_active` OFF for more than 5 min** — this boolean gates every push automation; stuck OFF means the screen silently freezes (a real incident, see `docs/troubleshooting.md`)
- **A new boot time on `Tab5 Uptime`** (a timestamp, published once per boot since 26/09/2026) — unexpected device reboot (brownout, firmware crash, power cut); a plain Wi-Fi drop without reboot comes back with the same boot time and does *not* trigger it
- **`HA API Status` off/unavailable for more than 2 min** — device unreachable, every push fails during the outage
- **A Tab5 automation logs « Error rendering »** — a push action failed to render its template and `continue_on_error` skipped it silently (real incident, 18/09/2026: Météo-France dropped `templow` from the 15th day). Requires `system_log: fire_event: true` in `configuration.yaml` (restart needed) — without it the guard loads but never fires. Exclude `system_log_event` from the recorder. At most one notification per hour while the error repeats
- **The boot and outage journal sent by the Tab5** (`esphome.tab5_journal`, since 26/09/2026) — the firmware (`Tab5/tab5_journal.cpp`) keeps its errors, and its warnings while HA is not connected, in memory that survives software resets and crashes (plus an NVS copy once Wi-Fi has been missing for 90 s, for power cuts), and sends them when HA reconnects for: an abnormal reset or a crash report (ESPHome's: PC and backtrace of both cores), Wi-Fi missing for 90 s or more (the Wi-Fi co-processor link, or the router), a boot that never reached HA, an error (ESPHome or ESP-IDF, including the ESP-Hosted driver) after HA connected, or HA reached more than 90 s after boot. Lines logged before the first HA connection are context only: a normal boot sends nothing, and neither does HA being away while Wi-Fi is up. Persistent notification every time; phone push only when `grave` (abnormal reset, crash, or Wi-Fi missing 90 s). No `system_log` prerequisite

Design notes:
- The guards notify through one script, `script.tab5_health_notify` (persistent notification + `notify.notify`), so the channels are adapted in a single place; each channel carries `continue_on_error: true` so one failing channel doesn't block the other
- No template uses raw `now()` — detection relies on trigger `for:` windows and `trigger.from_state` / `trigger.to_state`
- Numeric comparisons use `| float(0)` defaults (boot safety)

It's a self-contained HA *package*; enable packages in `configuration.yaml` first:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Then adapt the entity names at the top of the file (`notify.notify`, the `tab5_ha_hmi` entity prefix, and uncomment the `input_boolean` block if the helper doesn't exist in your setup).

---

### `packages/tab5_calendar.yaml`
Backend of the firmware's **calendar popup** (long press on the clock). Two scripts called *by the device* (`homeassistant.service:`), both `mode: restart`:

- **`tab5_calendrier_mois`** (`annee`, `mois`) — reads the work / public-holidays / family / birthdays calendars over the requested month and pushes back `esphome.<device>_tab5_maj_calendrier_mois`: a 62-hex-char string (2 per day — bits: work / public holiday / school holiday / appointment / birthday) plus 31 `|`-separated work-hour fields and a `details` field (day-detail lines, `~`-separated — required by the firmware since the 25/07/2026 schema, sent empty here)
- **`tab5_calendrier_jour`** (`date`) — builds the day-detail lines (`type|text;...`, max 6) and pushes `esphome.<device>_tab5_maj_calendrier_jour`

School holidays come from a **static Zone A table** (Bordeaux academy) verified against data.education.gouv.fr — edit it for your zone, and extend it once the next school year is published (see the `@ai_warning` in the file). The Google public-holidays calendar mixes real holidays with civil observances, hence the `feries_connus` whitelist. Same package install as above.

The four Jinja macros shared by its templates (`ev_start`, `ev_end`, `ev_summary`, `couvre` — one normalisation of a `calendar.get_events` event, all-day or timed) live in `custom_templates/tab5_calendar.jinja`. Deploy that file to HA's `config/custom_templates/` and call `homeassistant.reload_custom_templates` (or restart) **before** loading the package: without it both scripts fail at their import line.

---

### `packages/tab5_reveil.yaml`
What Home Assistant adds to the firmware's **alarm clock** — and nothing more. **The alarm itself does not depend on this file**: the device computes its ring time from the SNTP clock and the work hours it already caches, and rings a locally synthesised melody. Stop HA and the alarm still goes off; only the spoken briefing and the appointment reminders go missing. Never move the decision to ring in here.

- **`tab5_rdv_prochains`** — pushes the next 24 h of *timed* appointments to `esphome.<device>_tab5_maj_rdv_prochains` as `epoch|title~epoch|title~…` (8 max). Work events are excluded: their hours already drive the alarm time, and they are not appointments. **The device runs the countdown itself**, so an HA outage between the push and the deadline misses nothing.
- **`tab5_reveil_annonce`** — the spoken morning briefing (time, today's shift, next appointment, temperature), called *by the firmware* when `switch.tab5_alarm_tts` is on, and only on the first ring — not on snoozes.
- **automation `tab5_rdv_push`** — keeps the list fresh: every 5 min, on calendar changes, on `esphome.tab5_connected` (otherwise the list stays empty after a device reboot), and when the lead time changes.

Edit the two calendar entity IDs at the top of each `calendar.get_events` call to match yours. Same package install as above.

---

### `packages/tab5_alerts.yaml`
Backend of the **HA alert queue** — panels 4 to 7 of the central rotating card. Provides the `input_text.tab5_alerts_dismissed` helper (the dismiss list), the `tab5_dismiss_alert` script the device calls when you tap a banner or the info panel, the `sensor.tab5_unavailable_count` counter and a nightly cleanup of stale ids. The `tab5_maj_alertes_ha_bulk` payload itself (max 4 banners, already-dismissed ids filtered out) is built by the `tab5_push_alertes` script.

After a dismiss, the refresh comes from the light push automation (`tab5_ha_hmi_alerts_push` in `packages/tab5_push.yaml`): it triggers on `input_text.tab5_alerts_dismissed` and re-pushes sections 1, 7 and 7b filtered by the dismiss list. The dismiss script no longer triggers the full push automation (it did until 2026-09-08 — a second, heavy push for nothing). Removed on 2026-09-26 for lack of callers: the `tab5_dismiss_info_panel` script and the automation listening to `esphome.tab5_alert_dismiss`, an event the firmware never fires.

Tapping a banner on screen removes it immediately and stores its id here, so a re-push of the same id stays hidden until HA sends a new one. `snippets/tab5_alerts_dismissed_input_text.yaml` is the same helper on its own, if you prefer declaring it in your existing `input_text:` block instead of loading the whole package.

---

### `packages/volet_serre_tracking.yaml`
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

Replace these placeholders throughout the files:

| Placeholder | What to replace with |
|-------------|---------------------|
| `VOTRE_VILLE` | Your city entity from Météo-France (`weather.your_city`) |
| `VOTRE_DEPARTEMENT` | Your department number for weather alerts (e.g., `40`) |
| `VOTRE_CLIMATISATION` | Your climate entity (`climate.your_ac_unit`) |
| `VOTRE_EMAIL_gmail_com` | Your Google Calendar entity (`calendar.your_email_gmail_com`) |
| `VOTRE_VOLET` | Your roller shutter / cover entity |
| `VOTRE_TV` | Your Samsung TV (`media_player.<…>` **and** `remote.<…>`, package `tab5_tv`) |
| `VOTRE_LEDS` | The light toggled by `script.allumer_leds` |
| `VOTRE_PC` | The switch that turns your PC on (`switch.<…>`, `script.allumer_pc_tv`) |
| `VOTRE_TELEPHONE` / `VOTRE_CAPTEUR_PRESENCE` | Phone tracker and presence sensor of the screen on/off automation |
| `tab5-ha-hmi` | Your ESPHome device name (as configured in `tab5-ha-hmi.yaml`) |

**Don't search-and-replace by hand — render.** Copy `placeholders.example.yaml` to `placeholders.yaml` (gitignored), fill in your real entity IDs, then:

```bash
python tools/render_ha_config.py          # writes HomeAssistant_Config/rendered/ — the deployable copies
python tools/render_ha_config.py --check  # fails if a real ID leaked into a tracked file (never prints the value)
```

Deploy **from `rendered/`** to your HA `config/` (`packages/` and `custom_templates/`; `snippets/` are copy-paste fragments, not loaded by HA). The tracked files stay placeholder-only, so a PR never carries a real entity ID and a `git pull` never overwrites your values.

After deploying:

1. Reload the custom templates (`homeassistant.reload_custom_templates`) — `packages/tab5_calendar.yaml` imports `custom_templates/tab5_calendar.jinja`
2. In Home Assistant, **Developer Tools → YAML**: reload **Automations**, **Scripts**, **Template entities**, **Input booleans** and **Input texts** (or restart HA)
3. The Tab5 should receive its first push within a few seconds of connecting to the API

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

Ce dossier contient le côté Home Assistant de l'intégration Tab5 : automatisations qui poussent des données vers l'appareil, scripts déclenchés par l'appareil, capteurs de template et helpers. Tout est en **packages** Home Assistant (`packages/`), avec des identifiants d'entités placeholder.

> **Une seule source (26/09/2026).** Le Home Assistant de l'auteur fait tourner les copies *rendues* de ces mêmes packages : il n'y a plus de version privée. Jusqu'à ce jour, trois fichiers d'exemples à fusionner à la main (`automations_examples.yaml.example`, `scripts_examples.yaml`, `template_sensors_examples.yaml`) étaient tirés de fichiers privés, et ils dérivaient : le package du volet n'avait jamais tourné nulle part, et `script.allumer_pc_tv`, appelé par le firmware, n'existait dans aucun fichier public.

> **Installer = des packages.** Activez `homeassistant: packages: !include_dir_named packages` dans `configuration.yaml`, puis copiez les packages rendus dans `config/packages/` (voir [Adapter à votre setup](#adapter-à-votre-setup)). Rien à fusionner dans `automations.yaml` ni `scripts.yaml`. Les automatisations et scripts d'un package sont en lecture seule dans l'interface HA : modifier le fichier, rendre, déployer.

---

## Fichiers

### `packages/tab5_push.yaml`
Les automatisations de poussée, les scripts qu'elles partagent, les scripts appelés par le Tab5, le capteur de template de la pluie et le garde-fou `is_primary_active`. Elles poussent les données vers le Tab5 via des appels de service ESPHome natifs ; les blocs envoyés par plusieurs automatisations n'existent qu'une fois, dans les scripts `tab5_push_*`. C'est le package par lequel commencer.

Ce qu'elle pousse :
- **Prévisions journalières (15 jours) :** toutes les 10 min, au changement du calendrier et à la (re)connexion — sérialise 15 × (index, libellé jour, condition, min, max, drapeaux week-end/férié, heures de travail) en chaîne délimitée `|`/`;` vers `tab5_maj_previsions_jours_bulk`
- **Prévisions horaires (10 créneaux) :** deux chunks de 5 via `tab5_maj_previsions_heures_bulk` (l'écran a deux pages horaires)
- **Graphe de pluie court terme :** sur changement de `sensor.*_next_rain` — **9** barres en **un** appel (`tab5_maj_pluie_1h_bulk`, payload `idx|intensité;…`, index 0–8 = 0/5/10/…/55 min) construites depuis `v1/vision/rain` de Météo-France
- **Météo actuelle / probabilités :** `tab5_maj_meteo_actuelle` (condition, température, humidité) et `tab5_maj_probabilites` (UV, gel, neige) — au changement (`tab5_ha_hmi_meteo_push`) et à la (re)connexion, script `tab5_push_meteo`
- **État climatisation :** automation dédiée à faible latence `tab5_ha_hmi_clim_push` (sans delay, `mode: restart`) et à la (re)connexion — `tab5_maj_clim` (cible, actuelle, mode, preset, ventilation, oscillation), script `tab5_push_clim`
- **État volet :** `tab5_maj_volet_etat` au changement des helpers (`tab5_volet_updater`) et à la (re)connexion, script `tab5_push_volet` — arme aussi le wake word local « Stop » pendant le mouvement
- **Bandeau info :** `tab5_maj_info_texte` (texte, couleur, id de dismiss) — récap calendrier 3 jours ou bannière d'alerte météo
- **Vigilance Météo-France :** `tab5_maj_alerte_meteo_france` — un seul payload à 11 champs délimités `|`
- **File d'alertes HA :** `tab5_maj_alertes_ha_bulk` — jusqu'à 4 bandeaux dans le rotateur central (voir `packages/tab5_alerts.yaml`)

Aussi dans le package, hors poussée : **`tab5_screen_presence_wifi`** allume l'écran quand le capteur de présence détecte quelqu'un (ou au retour du téléphone) s'il est éteint, et l'éteint après **15 min** sans présence (ou au départ du téléphone) s'il est allumé et que le réveil ne sonne pas. Écran éteint, le firmware met LVGL en pause ; un toucher ou une tape sur la dalle le rallume.

Les températures/humidités des pièces, les états de lumière et l'humidité des plantes ne passent **pas** par ces services : ce sont des entités « miroir » (`platform: homeassistant` dans `Tab5/tab5-sensors-domotique.yaml`), synchronisées automatiquement par HA — rien à écrire côté HA.

**Plus de renvoi périodique d'un état inchangé (26/09/2026) :** météo actuelle, probabilités, clim et volet repartaient toutes les 10 min en plus de leurs poussées au changement (576 appels par jour, chacun repeint par l'appareil). La poussée complète ne les envoie plus qu'à la (re)connexion et au retour à `on` de `input_boolean.is_primary_active`.

**Traffic pacing :** l'automatisation utilise `delay: 1s` entre chaque bloc push et `delay: 150ms` dans les boucles de prévisions. Cela empêche plusieurs gros payloads de saturer le buffer de sockets TCP de l'ESP32-P4 simultanément avec le flux audio I2S actif.

---

**Scripts.** Appelés **par** le Tab5 (depuis un `homeassistant.service:` de `Tab5/tab5-api-logic.yaml` ou un `on_short_click:` LVGL), et pas l'inverse : `allumer_leds`, et `allumer_pc_tv` pour le bouton « PC Bureau » (TV allumée → on l'éteint ; sinon on allume le PC et la TV). Pass-through simple : ça garde le code ESPHome léger et la logique côté HA où est sa place.

Il contient aussi les **scripts de poussée** `tab5_push_alertes` (sections 1, 7 et 7b : vigilance Météo-France, bandeau info, rotateur d'alertes HA — MAJ, capteurs `problem` et compte d'indisponibles relevés une fois par passage), `tab5_push_meteo`, `tab5_push_clim` et `tab5_push_volet`. Ceux-là sont appelés *par les automatisations*, pas par le Tab5 : chaque bloc n'existe qu'une fois au lieu d'être recopié dans la poussée complète et dans son automatisation au changement.

**Capteur de template.** `Phrase Prochaine Pluie` transforme la prévision de pluie Météo-France en phrase courte affichée par le Tab5 (`"Pluie dans 10 min"`, `"Pas de pluie prévue"`, `"Averses possibles"`). Côté HA plutôt que sur l'appareil, pour garder le code C++ simple.

**Garde-fou `input_boolean.is_primary_active`.** Toutes les poussées en dépendent ; `force_primary_active_on_boot` le remet à `on` au démarrage de HA, et `packages/tab5_health.yaml` prévient s'il reste à `off` 5 min. C'est un reste d'une ancienne installation à deux instances ([ADR-0008](../docs/decisions/0008-single-ha-instance.md)) : avec un seul Home Assistant, il reste simplement à `on`.

L'exemple de réponse de l'assistant (moteur → popup Assistant) est passé dans `snippets/tab5_assist_reponse_exemple.yaml` : dans un package, il aurait été actif chez tout le monde.

---

### `packages/tab5_health.yaml`
Package de surveillance santé : cinq automations de garde qui alertent quand le pipeline de push se dégrade silencieusement. Le Tab5 étant push-only (voir `docs/decisions/0001-push-only-zero-polling.md`), un écran figé ne lève aucune erreur par lui-même — ces automations sont le filet de sécurité côté HA.

Ce qui est surveillé :
- **`input_boolean.is_primary_active` OFF depuis plus de 5 min** — ce booléen conditionne toutes les automations de push ; bloqué sur OFF, l'écran se fige silencieusement (incident réel, voir `docs/troubleshooting.md`)
- **Une nouvelle heure de démarrage sur `Tab5 Uptime`** (un horodatage, publié une fois par démarrage depuis le 26/09/2026) — reboot inattendu de l'appareil (brownout, crash firmware, coupure d'alimentation) ; une simple coupure Wi-Fi sans reboot revient avec la même heure de démarrage et ne déclenche *pas*
- **`HA API Status` off/unavailable depuis plus de 2 min** — appareil injoignable, toutes les poussées échouent pendant la coupure
- **Une automation Tab5 journalise « Error rendering »** — une action de poussée n'a pas pu rendre son template et `continue_on_error` l'a sautée en silence (incident réel du 18/09/2026 : Météo-France a retiré `templow` du 15ᵉ jour). Exige `system_log: fire_event: true` dans `configuration.yaml` (redémarrage nécessaire) — sans lui la garde est chargée mais ne se déclenche jamais. Exclure `system_log_event` du recorder. Au plus une notification par heure tant que l'erreur se répète
- **Le journal des démarrages et des coupures envoyé par le Tab5** (`esphome.tab5_journal`, depuis le 26/09/2026) — le firmware (`Tab5/tab5_journal.cpp`) garde ses erreurs, et ses avertissements tant que HA n'est pas connecté, dans une mémoire qui survit aux redémarrages logiciels et aux plantages (plus une copie NVS quand le Wi-Fi manque depuis 90 s, pour les coupures de courant), et les envoie à la reconnexion de HA pour : un reset anormal ou un rapport de plantage (celui d'ESPHome : PC et pile d'appels des deux cœurs), un Wi-Fi absent 90 s ou plus (lien du co-processeur Wi-Fi, ou routeur), un démarrage qui n'a jamais joint HA, une erreur (ESPHome ou ESP-IDF, pilote ESP-Hosted compris) après la connexion à HA, ou HA joint plus de 90 s après le démarrage. Les lignes d'avant la première connexion à HA ne sont que du contexte : un démarrage normal n'envoie rien, une absence de HA avec Wi-Fi présent non plus. Notification persistante à chaque fois ; téléphone seulement si `grave` (reset anormal, plantage, ou Wi-Fi absent 90 s). Aucun prérequis `system_log`

Notes de conception :
- Les gardes notifient via un seul script, `script.tab5_health_notify` (notification persistante + `notify.notify`) : les canaux s'adaptent à un seul endroit ; chaque canal porte `continue_on_error: true`, un canal en échec ne bloque pas l'autre
- Aucun template n'utilise `now()` brut — la détection repose sur les fenêtres `for:` des déclencheurs et sur `trigger.from_state` / `trigger.to_state`
- Les comparaisons numériques utilisent des défauts `| float(0)` (sécurité au boot)

C'est un *package* HA autonome ; activez d'abord les packages dans `configuration.yaml` :

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Puis adaptez les noms d'entités en tête de fichier (`notify.notify`, le préfixe d'entité `tab5_ha_hmi`, et décommentez le bloc `input_boolean` si le helper n'existe pas chez vous).

---

### `packages/tab5_calendar.yaml`
Backend du **popup calendrier** du firmware (appui long sur l'horloge). Deux scripts appelés *par l'appareil* (`homeassistant.service:`), tous deux `mode: restart` :

- **`tab5_calendrier_mois`** (`annee`, `mois`) — lit les calendriers boulot / jours fériés / famille / anniversaires sur le mois demandé et repousse `esphome.<device>_tab5_maj_calendrier_mois` : chaîne de 62 hex (2 par jour — bits : travail / férié / vacances scolaires / RDV / anniversaire) + 31 champs d'heures de travail séparés par `|` + un champ `details` (lignes de détail jour séparées par `~` — exigé par le firmware depuis le schéma du 25/07/2026, envoyé vide ici)
- **`tab5_calendrier_jour`** (`date`) — construit les lignes de détail du jour (`type|texte;...`, max 6) et pousse `esphome.<device>_tab5_maj_calendrier_jour`

Les vacances scolaires viennent d'une **table statique Zone A** (académie de Bordeaux) vérifiée sur data.education.gouv.fr — adaptez-la à votre zone, et complétez-la à la publication de l'année scolaire suivante (voir l'`@ai_warning` dans le fichier). Le calendrier Google des jours fériés mélange vrais fériés et fêtes civiles, d'où la liste blanche `feries_connus`. Même installation package que ci-dessus.

Les quatre macros Jinja partagées par ses templates (`ev_start`, `ev_end`, `ev_summary`, `couvre` — une seule normalisation d'un événement `calendar.get_events`, journée entière ou horodaté) vivent dans `custom_templates/tab5_calendar.jinja`. Déployez ce fichier dans le `config/custom_templates/` de HA et appelez `homeassistant.reload_custom_templates` (ou redémarrez) **avant** de charger le package : sans lui, les deux scripts échouent à leur ligne d'import.

---

### `packages/tab5_reveil.yaml`
Ce que Home Assistant apporte au **réveil** du firmware — et rien de plus. **Le réveil lui-même ne dépend pas de ce fichier** : l'appareil calcule son heure depuis l'horloge SNTP et les horaires de travail qu'il garde déjà en cache, et sonne une mélodie synthétisée localement. Arrêtez HA, le réveil sonne quand même ; seuls le briefing parlé et les rappels de rendez-vous manquent. Ne jamais déplacer ici la décision de sonner.

- **`tab5_rdv_prochains`** — pousse les rendez-vous *horodatés* des 24 prochaines heures vers `esphome.<device>_tab5_maj_rdv_prochains`, au format `epoch|titre~epoch|titre~…` (8 maximum). Les événements « Travail » sont exclus : leurs horaires servent déjà à calculer l'heure de réveil, et ce ne sont pas des rendez-vous. **C'est l'appareil qui tient le compte à rebours**, donc une coupure HA entre la poussée et l'échéance ne fait rien rater.
- **`tab5_reveil_annonce`** — le briefing parlé du matin (heure, horaires du jour, prochain rendez-vous, température), appelé *par le firmware* quand `switch.tab5_alarm_tts` est actif, et uniquement au premier déclenchement — pas aux répétitions.
- **automation `tab5_rdv_push`** — entretient la liste : toutes les 5 min, sur changement de calendrier, sur `esphome.tab5_connected` (sinon la liste reste vide après un redémarrage de la tablette), et quand le délai d'annonce change.

Adaptez les deux IDs de calendrier en tête de chaque `calendar.get_events` aux vôtres. Même installation package que ci-dessus.

---

### `packages/tab5_alerts.yaml`
Backend de la **file d'alertes HA** — panneaux 4 à 7 de la carte centrale rotative. Fournit le helper `input_text.tab5_alerts_dismissed` (liste de dismiss), le script `tab5_dismiss_alert` que l'appareil appelle au tap sur un bandeau ou sur le panneau info, le compteur `sensor.tab5_unavailable_count` et une purge nocturne des ids périmés. Le payload `tab5_maj_alertes_ha_bulk` lui-même (4 bandeaux max, ids déjà masqués filtrés) est construit par le script `tab5_push_alertes`.

Après un acquittement, le rafraîchissement vient de l'automation « push léger » (`tab5_ha_hmi_alerts_push` dans `packages/tab5_push.yaml`) : elle se déclenche sur `input_text.tab5_alerts_dismissed` et repousse les sections 1, 7 et 7b filtrées par la liste. Le script d'acquittement ne déclenche plus l'automation de push complète (il le faisait jusqu'au 08/09/2026 — un second push, lourd, pour rien). Retirés le 26/09/2026 faute d'appelant : le script `tab5_dismiss_info_panel` et l'automation qui écoutait `esphome.tab5_alert_dismiss`, un événement que le firmware n'émet jamais.

Un tap sur un bandeau le retire tout de suite et mémorise son id ici : un re-push du même id reste masqué tant que HA n'envoie pas un id différent. `snippets/tab5_alerts_dismissed_input_text.yaml` contient le helper seul, si vous préférez le déclarer dans votre bloc `input_text:` existant plutôt que charger tout le package.

---

### `packages/volet_serre_tracking.yaml`
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

Remplacez ces placeholders dans les fichiers :

| Placeholder | Par quoi le remplacer |
|-------------|----------------------|
| `VOTRE_VILLE` | Votre entité ville Météo-France (`weather.votre_ville`) |
| `VOTRE_DEPARTEMENT` | Votre numéro de département pour les alertes (ex: `40`) |
| `VOTRE_CLIMATISATION` | Votre entité climate (`climate.votre_clim`) |
| `VOTRE_EMAIL_gmail_com` | Votre entité Google Calendar (`calendar.votre_email_gmail_com`) |
| `VOTRE_VOLET` | Votre entité volet roulant / cover |
| `VOTRE_TV` | Votre TV Samsung (`media_player.<…>` **et** `remote.<…>`, package `tab5_tv`) |
| `VOTRE_LEDS` | La lumière basculée par `script.allumer_leds` |
| `VOTRE_PC` | L'interrupteur qui allume votre PC (`switch.<…>`, `script.allumer_pc_tv`) |
| `VOTRE_TELEPHONE` / `VOTRE_CAPTEUR_PRESENCE` | Tracker du téléphone et capteur de présence de l'automation d'allumage écran |
| `tab5-ha-hmi` | Le nom de votre appareil ESPHome |

**Ne pas chercher-remplacer à la main — rendre.** Copiez `placeholders.example.yaml` vers `placeholders.yaml` (gitignoré), renseignez vos vrais entity IDs, puis :

```bash
python tools/render_ha_config.py          # écrit HomeAssistant_Config/rendered/ — les copies déployables
python tools/render_ha_config.py --check  # échoue si un ID réel est retombé dans un fichier suivi (n'affiche jamais la valeur)
```

Déployez **depuis `rendered/`** vers le `config/` de HA (`packages/` et `custom_templates/` ; les `snippets/` sont des fragments à copier, pas chargés par HA). Les fichiers suivis ne contiennent que des placeholders : une PR n'embarque jamais un ID réel, et un `git pull` n'écrase jamais vos valeurs.

Après déploiement : rechargez les templates personnalisés (`homeassistant.reload_custom_templates`, `packages/tab5_calendar.yaml` importe `custom_templates/tab5_calendar.jinja`), puis dans HA, **Outils de développement → YAML** : rechargez **Automatisations**, **Scripts**, **Entités de template**, **Entrées booléennes** et **Entrées de texte** (ou redémarrez HA). Le Tab5 devrait recevoir son premier push en quelques secondes après connexion à l'API.

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
