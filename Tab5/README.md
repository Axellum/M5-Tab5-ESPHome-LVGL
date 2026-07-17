# Tab5 ESPHome Files

## English · [Français](#version-française)

---

> ⚠️ This README was rewritten on 2026-07-05 to match the current codebase after the previous version was found describing an unrelated, outdated iteration of the project (different service names, a 6-page layout that no longer exists). If you find another mismatch, it's the code that's right — please fix this file, not the other way around.

This folder contains the ESPHome configuration packages and the C++ source files for the Tab5 firmware.

The entry point is `../tab5-ha-hmi.yaml` at the repository root. It loads `substitutions` from `Tab5/user_entities.yaml` (gitignored — copy `user_entities.example.yaml` and edit your HA entity IDs), declares the `on_boot` sequence, and the `packages:` import list for everything in this folder.

**Screen layout:** a single 1280×720 page (`page_main` in `tab5-lvgl.yaml`), not multiple pages. Navigation is by touch (clim/light/TV-remote popups, diagnostics console via the `btn_control_console` button, top right) and left/right swipes on the lower band of the screen (`y ≥ 333`) to cycle the 5 forecast pages (2 hourly + 3 daily windows). Since the 14/07/2026 swipe rework, the console is **not** opened by swipe anymore.

---

## `[AI-CONTEXT]` / `[AI-WARNING]` / `[AI-DEBUG]` convention (read this before editing)

Most files in this folder (and every file in `ui_components/`) start with a comment block tagged `[AI-CONTEXT]` — a short "system prompt" local to that file: its role, its architectural constraints, and explicit `@ai_instruction`s for common edits. Non-obvious decisions (a bug fix that looks removable, a duplication kept on purpose, a `!include` that must not be inlined) are documented **inside the file itself**, not only in the external knowledge base (`contexte_ia/` in the parent workspace) — a session that only has access to this repo (no cross-repo context) must still be able to find them.

A `[AI-WARNING]` (sometimes `[AI-WARNING-CRITICAL]`) marks something that looks like a bug/anti-pattern but is a deliberate, validated fix — e.g. the boot `delay(1000)` in `tab5-ha-hmi.yaml` (documented at length in the `logger:` block of `tab5-hardware.yaml`), or the pagination wrap logic in `handle_swipe_gesture()` (`tab5_custom.cpp`). **Read the warning before "fixing" it** — at least one of these was already reverted once after being "corrected" by an LLM audit that hadn't read it. See [`../docs/decisions/`](../docs/decisions/README.md) for the full reasoning behind each one.

A `[AI-DEBUG]` marks a good observation point when diagnosing a runtime issue — a log line worth watching, a diagnostics entity/overlay, or a technique already proven to work on this device (e.g. inserting a temporary marker directly into the real HA automation rather than reproducing its logic in an isolated test script, which can pass while masking the actual bug). See [`../docs/debugging.md`](../docs/debugging.md).

If you add a genuinely new architectural constraint or a non-obvious decision while editing a file, add or extend its `[AI-CONTEXT]` block rather than leaving the reasoning only in a commit message or an external doc.

---

## File descriptions

### `tab5-hardware.yaml`
Low-level hardware: display/touch buses, ES8388 DAC I2C init, speaker/mic I2S, PI4IOE5V6408 GPIO expander (Wi-Fi power/antenna switches), `ota:` (password-protected, see `secrets.yaml`). Also hosts the voice stack: `micro_wake_word` with **two models** — `okay_nabu` (always on when the wake-word switch is enabled) and `Stop` (armed only while the shutter moves, stops it locally) — and the `voice_assistant:` callbacks that drive the mic icon colors.

### `tab5-sensors-diagnostics.yaml`
System/network entities: the `wifi:` block, GPIO power switches (Wi-Fi, USB, external 5V, antenna select), HA API status, IP/SSID, uptime, Wi-Fi RSSI, core temperature, free RAM/loop time (`debug`), SNTP clock and the status-bar/console refresh `interval:`s.

### `tab5-sensors-domotique.yaml`
Home-automation entities pushed by HA over the ESPHome API: plant moisture (5×, dynamically sorted), light/PC state mirrors, phone battery, room & greenhouse temperature/humidity, audio (speaker amp, headphone jack, wake-word switch).

### `tab5-api-logic.yaml`
The `api: services:` block — the actual contract with Home Assistant. Each `tab5_maj_*` service receives a payload from an HA automation and calls into `tab5_custom.cpp` (via lambdas) to update the LVGL widgets. See the service table below.

### `tab5-globals.yaml`
All `globals:` (shared state read/written across files) + the 8s central-panel rotator (planning/rain/alerts/info — 4 panels, paused while off the default forecast window). See the globals table below.

### `tab5-scripts.yaml`
Reusable ESPHome `script:` blocks, grouped by family: **debounces** (`tab5_debounce_volume_set` 150 ms, `tab5_debounce_light_brightness` 200 ms, `tab5_debounce_clim_temp` 250 ms — one HA call per gesture instead of one per tick), **voice** (`tab5_vocal_arm_stop`/`tab5_vocal_disarm_stop` for the `Stop` wake word, `tab5_vocal_interrupt`/`tab5_vocal_interrupt_and_listen` for tap-to-interrupt, `tab5_assist_toggle`, `tab5_show_vocal_response`), **central rotator** (`tab5_central_rotator_auto`, `tab5_central_panel_next`, `tab5_dismiss_info_tap`, `tab5_dismiss_ha_alert_0…3`), **shutter** (`tab5_volet_end_movement`, `tab5_volet_stop_voice_feedback`) and **light popup** (`tab5_light_popup_show`). The temporary planning display moved to C++ (`show_temporary_planning()`, `tab5_custom.cpp`). Prefer adding a script here over duplicating a `delay` + action pattern inline.

### `tab5-styles.yaml`
All LVGL `style_definitions` (glassmorphism "Slate" theme) + font declarations (Roboto sizes, MDI icon sizes, weather icon font). Color tokens live in `UIColor::` (`tab5_custom.h`) — **never hardcode a hex color in a YAML lambda**, add a token instead.

### `tab5-lvgl.yaml`
The single-page layout: clock/date, status icons, quick-action buttons, climate card, moisture card, central rotating card, 5 forecast cards (daily/hourly), swipe gesture handling.

### `ui_components/*.yaml`
Included by `tab5-lvgl.yaml`: `climate_card.yaml`/`climate_popup.yaml` (near-fullscreen 1250×690 modal in 3 glass cards: stacked HVAC modes Froid/Chaud/Sec/Ventilation/Éteint, 320 px thermostat arc with optimistic target readout and a debounced `climate.set_temperature` + room temperature line, presets Éco/Boost + Silence + airflow Oscillation/Brise `windnice`), `light_popup.yaml` (near-fullscreen 1250×690 modal in 3 glass cards: Chambre/Salon/LEDs selector + On/Off + all-off, 320 px brightness arc with live % readout synced from the HA `brightness` attribute + 10/35/65/100 % shortcuts and a debounced `light.turn_on`, 3 named whites + 12 round color swatches; opened via `script.tab5_light_popup_show`), `console_sys.yaml` (4 glass cards: memory/network/system diagnostics, volume, and a management card — screen re-push, automation reload, HA restart and device reboot, the last two behind confirm overlays), `tv_remote_popup.yaml` (near-fullscreen 1230×670 Samsung remote: power/pad/volume/channels/playback row via `remote.send_command` on `${entity_tv_remote}`, opened by the TV button or a long-press on the PC card), `forecast_daily.yaml`/`forecast_hourly.yaml`, `moisture_sensors.yaml`, `switches_card.yaml`.

### `tab5_custom.h` / `tab5_custom.cpp`
All non-trivial C++ logic: `update_meteo_icon()`, `get_temperature_color()`/`get_humidity_color()`, `parse_and_update_heures_bulk()`/`parse_and_update_jours_bulk()`, `sort_and_update_moisture_slots()`, `transition_widgets()`. **Rule: sensors/services should only read HA state and call these C++ functions — never manipulate `lv_obj_*` directly from a `sensor:`/`text_sensor:` lambda** (keeps LVGL logic in one place, testable and greppable).

---

## Services HA exposés (`api: services:`)

| Service | Payload | Rôle |
|---|---|---|
| `tab5_maj_clim` | target, current, mode, preset, fan, swing (strings) | État climatisation (couleurs, cible, mode, presets) |
| `tab5_maj_volet_etat` | etat_physique (string) | État volet (ouvert/fermé/en mouvement) |
| `tab5_maj_planning` | ligne1, ligne2 (strings) | Texte planning affiché dans la carte centrale |
| `tab5_maj_alerte_meteo_france` | payload (string, 11 champs `\|`-delimited) | Alertes météo France (vent, inondation, orages...) + recoloration de la date |
| `tab5_maj_meteo_actuelle` | condition, temperature, humidite | Icône pluie prédictive + hygrométrie (l'ancienne grosse icône météo centrale a été retirée de l'UI) |
| `tab5_maj_probabilites` | uv, gel, neige (strings) | Bascule l'icône pluie prédictive en flocon si probabilité de neige ≥ 5 |
| `tab5_maj_pluie_1h` | index_5mn, intensite (strings) | Une barre du graphe pluie 1h (9 barres) ; met à jour `has_rain` |
| `tab5_maj_info_texte` | texte, couleur (strings) | 4ᵉ panneau du rotateur : alerte météo (Rouge/Orange) ou résumé santé HA 1 ligne — MAJ en attente, erreurs, indispos (`update_info_text_ui()`) |
| `tab5_maj_previsions_heures_bulk` | payload (string) | 5 cartes prévisions horaires |
| `tab5_maj_previsions_jours_bulk` | payload (string) | 5 cartes prévisions journalières (fenêtre glissante selon `forecast_page_index`) |
| `tab5_maj_reponse_vocale` | texte (string) | Affiche temporairement la réponse vocale dans le bandeau central (`tab5_show_vocal_response`) |
| `tab5_maj_alertes_ha_bulk` | payload (string) | Jusqu'à 4 bandeaux d'alertes/infos HA, un panneau du rotateur chacun, tap-to-dismiss local (`parse_and_update_ha_alerts_bulk()`) |

## Globals principaux (`tab5-globals.yaml`)

| Global | Type | Rôle |
|---|---|---|
| `boot_complete` | bool | true une fois le `on_boot` terminé |
| `conversation_mode` | bool | mode assistant vocal (persiste au reboot) |
| `forecast_page_index` | int (0-4) | page prévisions active — 0-1 horaire, 2-4 journalier |
| `clim_target_temp`, `clim_preset_mode`, `clim_fan_mode`, `clim_swing_mode` | float/string | état climatisation |
| `volet_target_open`, `volet_en_mouvement` | bool | état volet |
| `plan_ligne_1`, `plan_ligne_2` | string | texte planning brut |
| `has_alerts`, `has_rain`, `has_info`, `current_central_panel` | bool/int | rotateur carte centrale (8s) : planning / pluie / alertes météo / info + jusqu'à 4 bandeaux HA |
| `has_ha_alert_0…3`, `ha_alert_id_0…3` | bool/string | bandeaux alertes/infos HA (`tab5_maj_alertes_ha_bulk`) ; `tab5_dismissed_local` mémorise les ids masqués au tap |
| `current_light_entity` | string | entité lumière pilotée par le popup lumière (`tab5_light_popup_show`) |
| `va_stop_armed` | bool | modèle wake word « Stop » armé (volet en mouvement) |
| `system_volume`, `system_muted` | float/bool | volume haut-parleur |

## Règles de code à respecter (issues de l'audit du 05/07/2026)

1. **Pas de couleur en dur** (`0xFFAABB`) dans un YAML/lambda — ajouter un token dans `UIColor::` (`tab5_custom.h`) et l'utiliser partout.
2. **Les `sensor:`/`text_sensor:` ne manipulent pas LVGL directement** — ils appellent une fonction C++ dans `tab5_custom.cpp` (ex: `update_light_ui()`, pas de `lv_obj_set_style_*` inline).
3. **Pas de `static` dans une lambda pour de l'état partagé entre deux handlers différents** (`on_short_click`/`on_long_press`) — utiliser un `globals:` (cf. bug `reboot_armed` corrigé le 05/07 ; global retiré le 16/07 quand la console est passée aux overlays de confirmation).
4. **Pas de `std::string` par valeur ni de `to_string()` dans un hot-path** (sliders, `on_value` fréquents) — `const std::string&` ou buffer `snprintf` statique.
5. **Toute nouvelle carte/widget répété ≥3 fois** (météo, switches...) doit passer par une fonction C++ builder paramétrée plutôt qu'un copier-coller YAML (cf. refacto architecture en cours).
6. Avant de committer : `python -m esphome compile tab5-ha-hmi.yaml` doit réussir (toolchain déjà en cache localement, ~20-45s).

---

## Version Française

Ce dossier contient les packages de configuration ESPHome et les fichiers source C++ du firmware Tab5. Point d'entrée : `../tab5-ha-hmi.yaml`. Voir la section anglaise ci-dessus pour le détail par fichier, la table des services HA, la table des globals et les règles de code — écrit contre le code réel le 05/07/2026, re-vérifié ligne à ligne le 14/07/2026, puis le 17/07/2026 (12 services dont `tab5_maj_reponse_vocale`/`tab5_maj_alertes_ha_bulk`, popups v2, télécommande TV, wake word « Stop », scripts par familles).

---

## Fichiers de polices

| Fichier | Contenu |
|---------|---------|
| `materialdesignicons-webfont.ttf` | Material Design Icons — plusieurs tailles chargées séparément (26/32/45/56/60/70/120 px — l'id `mdi_font_80` charge en réalité du 70) |
| `IconeMeteo.ttf` | Police d'icônes météo personnalisée |

## Sous-répertoires

### `my_components/st7123/`
Composant ESPHome personnalisé pour le contrôleur tactile I2C ST7123 (certains lots Tab5 V2).

### `tts_library/`, `tts_library_v2/`
Fichiers audio TTS expérimentaux, antérieurs à l'intégration Voice HA. Non utilisés dans la config actuelle.
