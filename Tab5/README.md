# Tab5 ESPHome Files

## English · [Français](#version-française)

---

> ⚠️ This README was rewritten on 2026-07-05 to match the current codebase after the previous version was found describing an unrelated, outdated iteration of the project (different service names, a 6-page layout that no longer exists). If you find another mismatch, it's the code that's right — please fix this file, not the other way around.

This folder contains the ESPHome configuration packages and the C++ source files for the Tab5 firmware.

The entry point is `../tab5-ha-hmi.yaml` at the repository root. It declares `substitutions` (your HA entity IDs), the `on_boot` sequence, `ota:`/`api:` config, and the `packages:` import list for everything in this folder.

**Screen layout:** a single 1280×720 page (`page_main` in `tab5-lvgl.yaml`), not multiple pages. Navigation is by touch (clim/light popups, console) and swipe gestures (top = console, bottom = close console, left/right on the bottom half = cycle 5 forecast pages: 2 hourly + 3 daily windows).

---

## File descriptions

### `tab5-hardware.yaml`
Low-level hardware: display/touch buses, ES8388 DAC I2C init, speaker/mic I2S, PI4IOE5V6408 GPIO expander (Wi-Fi power/antenna switches), `ota:` (password-protected, see `secrets.yaml`).

### `tab5-sensors.yaml`
All `sensor`/`text_sensor`/`binary_sensor`/`switch` entities exposed over the ESPHome API: system diagnostics (RAM/PSRAM/uptime/Wi-Fi), plant moisture (5×), light/PC/volet state mirrors, volume slider wiring.

### `tab5-api-logic.yaml`
The `api: services:` block — the actual contract with Home Assistant. Each `tab5_maj_*` service receives a payload from an HA automation and calls into `tab5_custom.cpp` (via lambdas) to update the LVGL widgets. See the service table below.

### `tab5-globals.yaml`
All `globals:` (shared state read/written across files) + the 6s central-panel rotator (planning/rain/alerts). See the globals table below.

### `tab5-scripts.yaml`
Reusable ESPHome `script:` blocks (debounced actions, timed sequences). Small and growing — prefer adding a script here over duplicating a `delay` + action pattern inline.

### `tab5-styles.yaml`
All LVGL `style_definitions` (glassmorphism "Slate" theme) + font declarations (Roboto sizes, MDI icon sizes, weather icon font). Color tokens live in `UIColor::` (`tab5_custom.h`) — **never hardcode a hex color in a YAML lambda**, add a token instead.

### `tab5-lvgl.yaml`
The single-page layout: clock/date, status icons, quick-action buttons, climate card, moisture card, central rotating card, 5 forecast cards (daily/hourly), swipe gesture handling.

### `ui_components/*.yaml`
Included by `tab5-lvgl.yaml`: `climate_card.yaml`/`climate_popup.yaml`, `light_popup.yaml`, `console_sys.yaml` (diagnostics + reboot), `forecast_daily.yaml`/`forecast_hourly.yaml`, `moisture_sensors.yaml`, `switches_card.yaml`.

### `tab5_custom.h` / `tab5_custom.cpp`
All non-trivial C++ logic: `update_meteo_icon()`, `get_temperature_color()`/`get_humidity_color()`, `parse_and_update_heures_bulk()`/`parse_and_update_jours_bulk()`, `sort_and_update_moisture_slots()`, `transition_widgets()`. **Rule: sensors/services should only read HA state and call these C++ functions — never manipulate `lv_obj_*` directly from a `sensor:`/`text_sensor:` lambda** (keeps LVGL logic in one place, testable and greppable).

---

## Services HA exposés (`api: services:`)

| Service | Payload | Rôle |
|---|---|---|
| `tab5_maj_clim` | target/current/mode (strings) | État climatisation (couleurs, cible, mode) |
| `tab5_maj_volet_etat` | string | État volet (ouvert/fermé/en mouvement) |
| `tab5_maj_planning` | ligne1, ligne2 (strings) | Texte planning affiché dans la carte centrale |
| `tab5_maj_alerte_meteo_france` | payload (string, `\|`-delimited) | Alertes météo France (vent, inondation, orages...) |
| `tab5_maj_meteo_actuelle` | condition, temperature, humidite | Icône pluie prédictive + hygrométrie (l'ancienne grosse icône météo centrale a été retirée de l'UI) |
| `tab5_maj_pluie_1h` | — | Prévision de pluie à 1h |
| `tab5_maj_previsions_heures_bulk` | payload (string) | 5 cartes prévisions horaires |
| `tab5_maj_previsions_jours_bulk` | payload (string) | 5 cartes prévisions journalières (fenêtre glissante selon `forecast_page_index`) |

## Globals principaux (`tab5-globals.yaml`)

| Global | Type | Rôle |
|---|---|---|
| `boot_complete` | bool | true une fois le `on_boot` terminé |
| `conversation_mode` | bool | mode assistant vocal (persiste au reboot) |
| `reboot_armed` | bool | armement double-tap du bouton reboot (console) |
| `forecast_page_index` | int (0-4) | page prévisions active — 0-1 horaire, 2-4 journalier |
| `clim_target_temp`, `clim_preset_mode`, `clim_fan_mode`, `clim_swing_mode` | float/string | état climatisation |
| `volet_target_open`, `volet_en_mouvement` | bool | état volet |
| `plan_ligne_1`, `plan_ligne_2` | string | texte planning brut |
| `has_alerts`, `has_rain`, `current_central_panel` | bool/int | rotateur carte centrale (6s) |
| `system_volume`, `system_muted` | float/bool | volume haut-parleur |

## Règles de code à respecter (issues de l'audit du 05/07/2026)

1. **Pas de couleur en dur** (`0xFFAABB`) dans un YAML/lambda — ajouter un token dans `UIColor::` (`tab5_custom.h`) et l'utiliser partout.
2. **Les `sensor:`/`text_sensor:` ne manipulent pas LVGL directement** — ils appellent une fonction C++ dans `tab5_custom.cpp` (ex: `update_light_ui()`, pas de `lv_obj_set_style_*` inline).
3. **Pas de `static` dans une lambda pour de l'état partagé entre deux handlers différents** (`on_short_click`/`on_long_press`) — utiliser un `globals:` (cf. bug `reboot_armed` corrigé le 05/07).
4. **Pas de `std::string` par valeur ni de `to_string()` dans un hot-path** (sliders, `on_value` fréquents) — `const std::string&` ou buffer `snprintf` statique.
5. **Toute nouvelle carte/widget répété ≥3 fois** (météo, switches...) doit passer par une fonction C++ builder paramétrée plutôt qu'un copier-coller YAML (cf. refacto architecture en cours).
6. Avant de committer : `python -m esphome compile tab5-ha-hmi.yaml` doit réussir (toolchain déjà en cache localement, ~20-45s).

---

## Version Française

Ce dossier contient les packages de configuration ESPHome et les fichiers source C++ du firmware Tab5. Point d'entrée : `../tab5-ha-hmi.yaml`. Voir la section anglaise ci-dessus pour le détail par fichier, la table des services HA, la table des globals et les règles de code — tout est vérifié contre le code réel au 05/07/2026 (l'ancienne version de ce README décrivait un projet différent, obsolète).

---

## Fichiers de polices

| Fichier | Contenu |
|---------|---------|
| `materialdesignicons-webfont.ttf` | Material Design Icons — plusieurs tailles chargées séparément (26/32/45/56/60/80/120px) |
| `IconeMeteo.ttf` | Police d'icônes météo personnalisée |

## Sous-répertoires

### `my_components/st7123/`
Composant ESPHome personnalisé pour le contrôleur d'affichage ST7123 (certains lots Tab5 V2).

### `tts_library/`, `tts_library_v2/`
Fichiers audio TTS expérimentaux, antérieurs à l'intégration Voice HA. Non utilisés dans la config actuelle.
