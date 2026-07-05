# Tab5 ESPHome Files

## English · [Français](#version-française)

---

This folder contains the ESPHome configuration packages and the C++ source files for the Tab5 firmware.

The entry point is `../tab5-ha-hmi.yaml` at the repository root. That file imports everything here via `!include`.

---

## File descriptions

### `tab5-ha-hmi.yaml` *(root, not in this folder)*
Entry point. Declares the `substitutions` block (your entity IDs), the `on_boot` sequence, and the `packages:` import list. The only file to edit when adapting the project to a different Home Assistant setup.

---

### `tab5-hardware.yaml`
Low-level hardware configuration. Everything that touches physical pins lives here.

- **Display:** SPI bus, RGB framebuffer configuration, backlight PWM
- **Touch:** I2C controller, touch calibration
- **I2C buses:** two buses — one for the display/touch, one for the ES8388 DAC
- **ES8388 DAC:** I2C register initialization sequence on boot (`i2c.write_bytes` to register `0x04`), I2S output bus
- **Microphone:** I2S input bus at 16 kHz / 16-bit mono
- **Ambient light sensor:** ADC reading for auto-brightness
- **Speaker enable:** GPIO switch for the amplifier

**What to change:** usually nothing, unless you are adapting this to a different hardware variant. GPIO numbers match Tab5 V2.

---

### `tab5-sensors.yaml`
All sensor entities exposed to Home Assistant over the ESPHome API.

Includes:
- Internal ESP32-P4 temperature (°C)
- Ambient light level (lux)
- Wi-Fi signal strength (dBm) and uptime
- Plant moisture sensors (BLE, up to 5 devices)
- Room temperature and humidity (linked to HA entities via `homeassistant` sensor platform)

**What to change:** BLE sensor MAC addresses if you use plant monitors, and the `homeassistant` sensor entity IDs (these are in the substitutions block of the root file, not here directly).

---

### `tab5-api-logic.yaml`
The API layer. This is where Home Assistant connects to the screen.

Two types of things live here:

**`api: services:`** — ESPHome service endpoints that HA calls to push data. Each service has a named payload parameter (typically a semicolon-delimited string) and a `lambda:` block that calls a C++ function to parse and apply it.

Examples of declared services:
- `tab5_update_meteo_7j` — 7-day weather forecast (7 × icon + max/min temp + condition code)
- `tab5_update_pluie_heure` — hourly rain chart (60 values, 0–100)
- `tab5_update_calendrier` — calendar events (4 slots × title + time + color tag)
- `tab5_update_clim` — climate state (mode, target temp, current temp)
- `tab5_update_plantes` — plant moisture values (5 sensors)
- `tab5_update_alerte_meteo` — weather alert text + severity level

**`lambda:` blocks** — C++ logic for state management, conditional UI updates, and anything that requires branching logic not expressible cleanly in YAML action sequences.

**What to change:** nothing structurally, but if you add a new screen or data source, new service endpoints are declared here.

---

### `tab5-styles.yaml`
Global LVGL style definitions. All style objects are declared here and referenced by ID in `tab5-lvgl.yaml`.

Style categories:
- `style_panel_base` — transparent container, no border, no padding
- `style_card` — dark rounded card for content panels
- `style_text_title` — large white bold text
- `style_text_value` — large accent-color numeric value
- `style_text_dim` — small grey secondary text
- `style_btn_normal` / `style_btn_pressed` / `style_btn_active` — button states
- `style_arc_indicator` — colored fill for thermostat arc
- `style_progress_moisture` — plant moisture bar style
- Color constants via `UIColor` namespace in C++

**What to change:** color palette (hex values in the C++ header) to match your preferred theme.

---

### `tab5-globals.yaml`
Global variables shared across all packages.

| Variable | Type | Description |
|----------|------|-------------|
| `boot_complete` | `bool` | Gate: true after HA API connects and boot sequence finishes |
| `conversation_mode` | `bool` | false = HA mode, true = conversation/LLM mode |
| `voice_state` | `int` | Current pipeline state (0=idle, 1=listening, 2=processing, 3=speaking, 4=error) |
| `last_temp_salon` | `float` | Cached last temperature value (prevents NaN display on reconnect) |
| `clim_target_temp` | `float` | Cached climate target temperature |
| `clim_mode` | `std::string` | Cached climate mode string |

**What to change:** if you add new cached state variables, declare them here with explicit types and initial values.

---

### `tab5-lvgl.yaml`
The complete UI layout. All screens, all widgets, all icons.

Screen structure:
```
main_screen
├── tab_bar              ← bottom navigation bar (6 tabs)
├── page_accueil         ← home: time, date, room temp/humidity, alerts, mic icon, mode button
├── page_meteo           ← weather: 7-day forecast cards + hourly rain bar chart
├── page_clim            ← climate: arc thermostat, mode selector, room temp
├── page_plantes         ← plants: 5 moisture gauges + temperature
├── page_console         ← debug: scrollable log output
└── page_planning        ← calendar: 4 event slots with color tags
```

All widgets reference style IDs from `tab5-styles.yaml`. No inline style properties.

**What to change:** widget positions (`x`, `y`, `width`, `height`) if you want to adjust layout. Icon glyphs (MDI Unicode code points) if you want different icons.

---

### `tab5-scripts.yaml`
Short reusable ESPHome script blocks. Called from lambdas in `tab5-api-logic.yaml` or directly from HA automations.

Currently minimal — used mainly for brightness adjustment sequences and reconnection handling.

---

### `tab5_custom.h`
C++ header. Declares all functions used from YAML `lambda:` blocks.

Functions declared here must have their implementations in `tab5_custom.cpp`. The header is included first by ESPHome's build system.

---

### `tab5_custom.cpp`
C++ implementation file. The bulk of the non-trivial logic.

Main function groups:
- **String tokenizers** — `parse_meteo_7j()`, `parse_pluie_heure()`, `parse_calendrier()` — split semicolon-delimited payloads into typed arrays, then update LVGL widgets
- **LVGL update helpers** — safe wrappers that check `boot_complete` before calling any `lv_*` function
- **Color mappers** — `condition_to_color()`, `moisture_to_color()`, `temp_to_color()` — map sensor values to LVGL `lv_color_hex()` values
- **Voice state machine** — `set_voice_state(int state)` — updates the mic icon color and status text

---

## Font files

| File | Contents |
|------|----------|
| `materialdesignicons-webfont.ttf` | Material Design Icons (v7) — 7000+ icons for all UI elements |
| `IconeMeteo.ttf` | Custom weather icon font — covers condition symbols not in MDI |

These are referenced in `tab5-styles.yaml` via ESPHome's `font:` component with specific Unicode ranges to limit flash usage.

---

## Subdirectories

### `my_components/st7123/`
A custom ESPHome component for the ST7123 display controller variant found in some Tab5 V2 batches. Only needed if the standard `ili9xxx` or `st7789` components produce incorrect output on your unit.

### `tts_library/` and `tts_library_v2/`
Experimental TTS audio files (MP3/WAV) for local offline TTS — an earlier approach before the Home Assistant Voice pipeline integration was stable. Kept for reference. Not used in the current main configuration.

### `ui_components/`
Experimental reusable LVGL component definitions. Work in progress.

---

---

## Version Française

---

Ce dossier contient les packages de configuration ESPHome et les fichiers source C++ pour le firmware du Tab5.

Le point d'entrée est `../tab5-ha-hmi.yaml` à la racine du dépôt. Ce fichier importe tout ce qui se trouve ici via `!include`.

---

## Description des fichiers

### `tab5-hardware.yaml`
Configuration matérielle bas niveau. Tout ce qui touche les broches physiques est ici.

- **Affichage :** bus SPI, configuration framebuffer RGB, PWM rétroéclairage
- **Tactile :** contrôleur I2C, calibration
- **Bus I2C :** deux bus — un pour l'affichage/tactile, un pour le DAC ES8388
- **DAC ES8388 :** séquence d'initialisation registres I2C au boot, bus I2S sortie
- **Microphone :** bus I2S entrée à 16 kHz / 16-bit mono
- **Capteur de luminosité ambiante :** lecture ADC pour auto-luminosité
- **Activation haut-parleur :** switch GPIO pour l'amplificateur

**Ce qu'il faut changer :** généralement rien, sauf adaptation à un autre variant hardware. Les numéros GPIO correspondent au Tab5 V2.

---

### `tab5-sensors.yaml`
Toutes les entités capteurs exposées à Home Assistant via l'API ESPHome.

**Ce qu'il faut changer :** adresses MAC BLE des capteurs plantes, et les entity IDs des capteurs `homeassistant` (définis dans le bloc substitutions du fichier racine).

---

### `tab5-api-logic.yaml`
La couche API. C'est ici que Home Assistant se connecte à l'écran.

**Services ESPHome (`api: services:`)** — les endpoints que HA appelle pour pousser des données. Chaque service a un paramètre payload nommé (typiquement une chaîne délimitée par des points-virgules) et un bloc `lambda:` qui appelle une fonction C++ pour la parser et l'appliquer.

**Blocs `lambda:`** — logique C++ pour la gestion d'état, les mises à jour UI conditionnelles, et tout ce qui nécessite une logique de branchement non exprimable proprement en séquences d'actions YAML.

**Ce qu'il faut changer :** rien structurellement, mais si vous ajoutez un nouvel écran ou source de données, les nouveaux endpoints de service se déclarent ici.

---

### `tab5-styles.yaml`
Définitions globales de styles LVGL. Tous les objets style sont déclarés ici et référencés par ID dans `tab5-lvgl.yaml`.

**Ce qu'il faut changer :** la palette de couleurs (valeurs hex dans le header C++) pour correspondre à votre thème préféré.

---

### `tab5-globals.yaml`
Variables globales partagées entre tous les packages. Incluent le booléen de gate `boot_complete`, le mode assistant `conversation_mode`, l'état du pipeline vocal `voice_state`, et les snapshots de valeurs courantes.

**Ce qu'il faut changer :** si vous ajoutez de nouvelles variables d'état cachées, déclarez-les ici avec des types explicites et des valeurs initiales.

---

### `tab5-lvgl.yaml`
La mise en page UI complète. Tous les écrans, tous les widgets, toutes les icônes. Structure en 6 pages (accueil, météo, clim, plantes, console, planning). Toutes les références de style pointent vers des IDs définis dans `tab5-styles.yaml`.

**Ce qu'il faut changer :** positions des widgets si vous ajustez la mise en page. Code points Unicode MDI pour les icônes.

---

### `tab5_custom.h` / `tab5_custom.cpp`
Header et implémentation C++. Contiennent les tokenizers de chaînes, les helpers de mise à jour LVGL sécurisés (avec vérification `boot_complete`), les mappeurs de couleurs, et la machine d'états du pipeline vocal.

---

## Fichiers de polices

| Fichier | Contenu |
|---------|---------|
| `materialdesignicons-webfont.ttf` | Material Design Icons (v7) — 7000+ icônes |
| `IconeMeteo.ttf` | Police d'icônes météo personnalisée |

---

## Sous-répertoires

### `my_components/st7123/`
Composant ESPHome personnalisé pour le contrôleur d'affichage ST7123 trouvé dans certains lots Tab5 V2. Nécessaire uniquement si les composants standard produisent un rendu incorrect sur votre unité.

### `tts_library/` et `tts_library_v2/`
Fichiers audio TTS expérimentaux pour une approche TTS hors-ligne — antérieure à l'intégration stable du pipeline Voice HA. Conservés pour référence, non utilisés dans la configuration principale actuelle.

### `ui_components/`
Définitions de composants LVGL réutilisables expérimentales. Travail en cours.
