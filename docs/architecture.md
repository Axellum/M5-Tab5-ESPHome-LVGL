# Architecture & Code Structure

## English · [Français](#version-française)

---

## Overview

The ESPHome configuration is split into seven YAML packages imported by a single entry-point file. This avoids a monolithic file that becomes impossible to navigate once you're past 1000 lines. Each package has a clearly defined responsibility and can be edited, tested, or replaced in isolation.

---

## 1. Entry point: `tab5-ha-hmi.yaml`

The root file does three things:

1. **Declares substitutions** — a single block at the top where you map the project's generic variable names (`entity_light_salon`, `entity_climate_salon`, etc.) to your actual Home Assistant entity IDs. This is the only file you need to edit to adapt the project to a different home.

2. **Defines the boot sequence** — the `on_boot` block handles the startup order carefully: backlight on → media player volume set → amplifier enable (in that order, to avoid the ES8388 pop) → wait for HA API → fire a `tab5_connected` event on the HA event bus → start wake-word detection if enabled.

3. **Imports all packages** via `!include`.

```yaml
packages:
  tab5_hardware:   !include Tab5/tab5-hardware.yaml
  tab5_sensors:    !include Tab5/tab5-sensors.yaml
  tab5_api_logic:  !include Tab5/tab5-api-logic.yaml
  tab5_styles:     !include Tab5/tab5-styles.yaml
  tab5_globals:    !include Tab5/tab5-globals.yaml
  tab5_scripts:    !include Tab5/tab5-scripts.yaml
  tab5_lvgl:       !include Tab5/tab5-lvgl.yaml
```

---

## 2. Package roles

### `tab5-hardware.yaml`
Low-level hardware configuration:
- Display driver (SPI), touch controller (I2C)
- I2C buses, SPI bus, UART
- ES8388 DAC initialization (I2C register writes)
- Backlight PWM, ambient light sensor
- I2S bus for microphone and speaker

→ Details: [`docs/hardware.md`](hardware.md)

---

### `tab5-sensors.yaml`
All sensor entities exposed to Home Assistant:
- Internal ESP32-P4 temperature
- Ambient light level (for auto-brightness)
- Plant moisture sensors (up to 5 BLE plant monitors)
- Room temperature & humidity sensors
- Uptime, Wi-Fi signal strength

This file only declares sensors. No UI logic lives here.

---

### `tab5-api-logic.yaml`
The most important package. Two things live here:

**ESPHome API service handlers** — these are the endpoints Home Assistant calls to push data to the screen. Each handler receives a payload, validates it, then calls a C++ function in `tab5_custom.cpp` to parse and apply it.

```yaml
api:
  services:
    - service: tab5_update_meteo_7j   # 7-day forecast push
      variables:
        payload: string
      then:
        - lambda: |-
            parse_meteo_7j(payload.c_str());
```

**C++ lambdas** — for logic that doesn't fit cleanly in YAML (state machine transitions, string parsing, conditional LVGL updates).

The rule enforced throughout: before any `lv_*` call, check `boot_complete` and `api.connected()`. This prevents NaN values from crashing the UI during HA restarts.

---

### `tab5-styles.yaml`
Global LVGL style definitions, declared once and referenced by ID everywhere in `tab5-lvgl.yaml`.

The rationale: LVGL allocates a style object per widget if you define styles inline. With 80+ widgets on screen, that's 80+ style allocations in PSRAM. Declaring styles globally and attaching them by reference uses a handful of allocations for the whole screen.

Styles defined here cover: base panel, card backgrounds, text variants (title, value, dim, small), button states (normal, pressed, active), progress bar fill, and icon color variants.

---

### `tab5-globals.yaml`
Shared global variables accessible from any package:
- `boot_complete` (bool) — gate for all UI updates
- `conversation_mode` (bool) — toggles between Home Assistant pipeline and chat pipeline
- Current state snapshots (last known temperature values, climate mode, etc.)

Variables here are typed and initialized. Uninitialized globals on ESP32 are undefined behavior.

---

### `tab5-lvgl.yaml`
The UI layout. Declares the single page, panels, labels, buttons, arcs, and icons, plus swipe gesture handling. Includes the 16 `ui_components/*.yaml` files (climate card/popup, light popup, console overlay, forecast cards, moisture gauges, switches card).

**There is a single LVGL page, not a multi-page tab-bar layout.** Everything lives on one 1280×720 `page_main`:
```
page_main (1280×720, single page)
├── home content always visible   (clock, indoor temp/humidity, quick actions,
│                                   climate card, moisture card)
├── central rotating card          (planning / rain / alerts — auto-cycles every 8s,
│                                   tab5-globals.yaml `interval:`)
├── bottom card region — one of two, toggled by `btn_control_ha` (house icon, top right):
│   ├── switches card   (`layer_switches` — PC/volet/light switches, 5 tabs)
│   └── forecast card   (`layer_forecast_daily` / `layer_forecast_hourly` — weather, 5 tabs)
├── climate_popup (fullscreen overlay, opened by tapping the climate card)
├── light_popup   (fullscreen overlay, opened by tapping a light switch card)
└── console_sys   (diagnostics overlay, opened by swipe from the top)
```

Navigation is by touch (opening/closing the climate/light popups and the console, and toggling the bottom card region between switches and weather) and by swipe gesture, handled in C++ (`handle_swipe_gesture()` in `tab5_custom.cpp`):
- swipe down from the top → open the console overlay
- swipe up from the bottom half → close the console overlay
- swipe left/right on the bottom half → cycle through the 5 forecast pages (2 hourly windows + 3 daily windows, non-wrapping 0↔4) — only when the bottom region is in forecast mode; the switches card doesn't paginate via swipe

The `show_switches` global (`tab5-globals.yaml`) tracks which of the two is currently visible; the other is hidden via `LV_OBJ_FLAG_HIDDEN` rather than removed, so the toggle button (`tab5-lvgl.yaml`, `btn_control_ha`) just flips which layer is shown/hidden — see the `[AI-CONTEXT]` header in `ui_components/switches_card.yaml` for the source-level note.

All style references point to IDs defined in `tab5-styles.yaml`. No inline style properties.

→ Full file-by-file inventory and dependency graph: [`../CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md)

---

### `tab5-scripts.yaml`
Short ESPHome script blocks for reusable multi-step actions called from lambdas or HA. Keeps `tab5-api-logic.yaml` from becoming cluttered with repeated patterns.

---

## 3. C++ layer: `tab5_custom.h` / `tab5_custom.cpp`

The `.h` file declares all functions used from YAML lambdas. The `.cpp` file implements them.

Main responsibilities:
- **String tokenizer** — splits semicolon-delimited payload strings into arrays. Used for every multi-value push (weather, calendar, rain chart).
- **LVGL helpers** — wrappers around `lv_label_set_text`, `lv_arc_set_value`, `lv_bar_set_value` that include the boot-guard check.
- **Color logic** — maps weather condition codes or plant moisture levels to LVGL color values.
- **Voice state machine** — updates the microphone icon color based on the pipeline state (idle → listening → processing → speaking → error).

---

## 4. The Push paradigm

The device never initiates a network request. All data flow goes in one direction:

```
Home Assistant                       Tab5 (ESP32-P4)
──────────────                       ───────────────
State change detected
  → automation triggered
    → service call: esphome.tab5_update_meteo_7j(payload)
      → API handler receives payload ──────────────────────→
                                       parse_meteo_7j() runs
                                       LVGL labels updated
```

**Traffic pacing on the HA side:** the push automation inserts a 1-second delay between each service block, and 150 ms between items within forecast loops. This prevents the ESP32's TCP stack from running out of sockets when multiple large payloads arrive simultaneously alongside the I2S audio stream.

---

## 5. Data packing

For the 7-day weather forecast, 7 × 4 data points (icon, max temp, min temp, condition code) would be 28 separate service calls. Instead, HA builds one string:

```
"0;Soleil;27;14;1;Nuageux;24;12;2;Pluie;19;11;..."
```

The C++ tokenizer splits on `;` in a single pass — O(n) on string length, not O(n) on call count. The LVGL update then happens once, atomically, without intermediate redraws.

Same pattern applies to: hourly rain chart (60 values), calendar events (title + time + color tag × 4 slots).

---

---

## Version Française

---

## Vue d'ensemble

La configuration ESPHome est découpée en sept packages YAML importés par un fichier d'entrée unique. Cela évite un fichier monolithique qui devient impossible à naviguer au-delà de 1000 lignes. Chaque package a une responsabilité clairement définie et peut être édité, testé, ou remplacé de façon isolée.

---

## 1. Point d'entrée : `tab5-ha-hmi.yaml`

Le fichier racine fait trois choses :

1. **Déclare les substitutions** — un bloc unique en haut où on mappe les noms de variables génériques du projet (`entity_light_salon`, `entity_climate_salon`, etc.) vers les vrais entity IDs de Home Assistant. C'est le seul fichier à éditer pour adapter le projet à une autre maison.

2. **Définit la séquence de boot** — le bloc `on_boot` gère l'ordre de démarrage soigneusement : rétroéclairage → volume media player → activation ampli (dans cet ordre, pour éviter le pop ES8388) → attente de l'API HA → envoi d'un événement `tab5_connected` sur le bus HA → démarrage de la détection wake-word si activée.

3. **Importe tous les packages** via `!include`.

---

## 2. Rôles des packages

### `tab5-hardware.yaml`
Configuration matérielle bas niveau :
- Driver affichage (SPI), contrôleur tactile (I2C)
- Bus I2C, bus SPI, UART
- Initialisation DAC ES8388 (écritures registres I2C)
- PWM rétroéclairage, capteur luminosité ambiante
- Bus I2S pour microphone et haut-parleur

→ Détails : [`docs/hardware.md`](hardware.md)

---

### `tab5-sensors.yaml`
Toutes les entités capteurs exposées à Home Assistant :
- Température interne ESP32-P4
- Niveau de luminosité ambiante (pour auto-luminosité)
- Capteurs humidité plantes (jusqu'à 5 moniteurs BLE)
- Température & humidité des pièces
- Uptime, signal Wi-Fi

Ce fichier ne déclare que des capteurs. Aucune logique UI ici.

---

### `tab5-api-logic.yaml`
Le package le plus important. Deux choses y vivent :

**Gestionnaires de services API ESPHome** — ce sont les endpoints que Home Assistant appelle pour pousser des données vers l'écran. Chaque gestionnaire reçoit un payload, le valide, puis appelle une fonction C++ dans `tab5_custom.cpp` pour le parser et l'appliquer.

**Lambdas C++** — pour la logique qui ne rentre pas proprement en YAML (transitions de machine d'états, parsing de chaînes, mises à jour LVGL conditionnelles).

La règle appliquée partout : avant tout appel `lv_*`, vérifier `boot_complete` et `api.connected()`. Cela empêche les valeurs NaN de crasher l'UI pendant les redémarrages HA.

---

### `tab5-styles.yaml`
Définitions globales de styles LVGL, déclarées une fois et référencées par ID partout dans `tab5-lvgl.yaml`.

Le rationnel : LVGL alloue un objet style par widget si on définit les styles inline. Avec 80+ widgets à l'écran, ça fait 80+ allocations en PSRAM. Déclarer les styles globalement et les attacher par référence n'utilise qu'une poignée d'allocations pour tout l'écran.

---

### `tab5-globals.yaml`
Variables globales partagées accessibles depuis tous les packages :
- `boot_complete` (bool) — verrou pour toutes les mises à jour UI
- `conversation_mode` (bool) — bascule entre le pipeline Home Assistant et le pipeline chat
- Snapshots d'état courant (dernières valeurs de température connues, mode clim, etc.)

Les variables ici sont typées et initialisées. Les globales non initialisées sur ESP32 sont un comportement indéfini.

---

### `tab5-lvgl.yaml`
La mise en page UI. Déclare la page unique, panneaux, labels, boutons, arcs et icônes, ainsi que la gestion des gestes swipe. Inclut les 16 fichiers `ui_components/*.yaml` (carte/popup clim, popup lumière, overlay console, cartes prévisions, jauges humidité, carte switches).

**Il y a une seule page LVGL, pas une navigation multi-pages par onglets.** Tout tient sur un `page_main` unique en 1280×720 :
```
page_main (1280×720, page unique)
├── contenu accueil toujours visible   (horloge, temp/humidité intérieure,
│                                        actions rapides, carte clim, carte
│                                        humidité)
├── carte centrale rotative            (planning / pluie / alertes — cycle
│                                        auto toutes les 8s, `interval:` de
│                                        tab5-globals.yaml)
├── zone carte du bas — l'une des deux, basculée par `btn_control_ha` (icône maison, en haut à droite) :
│   ├── carte switches   (`layer_switches` — switches PC/volet/lumières, 5 onglets)
│   └── carte prévisions (`layer_forecast_daily` / `layer_forecast_hourly` — météo, 5 onglets)
├── climate_popup (overlay plein écran, ouvert au tap sur la carte clim)
├── light_popup   (overlay plein écran, ouvert au tap sur une carte switch lumière)
└── console_sys   (overlay diagnostics, ouvert par swipe depuis le haut)
```

La navigation se fait au tactile (ouverture/fermeture des popups clim/lumière et de la console, et bascule de la zone du bas entre switches et météo) et par geste swipe, géré en C++ (`handle_swipe_gesture()` dans `tab5_custom.cpp`) :
- swipe vers le bas depuis le haut → ouvre l'overlay console
- swipe vers le haut depuis la moitié basse → ferme l'overlay console
- swipe gauche/droite sur la moitié basse → cycle les 5 pages de prévisions (2 fenêtres horaires + 3 fenêtres journalières, sans bouclage 0↔4) — uniquement quand la zone du bas est en mode météo ; la carte switches ne se pagine pas au swipe

Le global `show_switches` (`tab5-globals.yaml`) suit laquelle des deux est actuellement visible ; l'autre est cachée via `LV_OBJ_FLAG_HIDDEN` plutôt que retirée, donc le bouton de bascule (`tab5-lvgl.yaml`, `btn_control_ha`) ne fait que basculer quel layer est affiché/caché — voir le bloc `[AI-CONTEXT]` de `ui_components/switches_card.yaml` pour la note au niveau du code source.

Toutes les références de style pointent vers des IDs définis dans `tab5-styles.yaml`. Aucune propriété de style inline.

→ Inventaire fichier par fichier et graphe de dépendances complet : [`../CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md)

---

## 3. Couche C++ : `tab5_custom.h` / `tab5_custom.cpp`

Le `.h` déclare toutes les fonctions utilisées depuis les lambdas YAML. Le `.cpp` les implémente.

Responsabilités principales :
- **Tokenizer de chaînes** — découpe les chaînes de payload délimitées par des points-virgules en tableaux. Utilisé pour chaque push multi-valeurs (météo, calendrier, graphique pluie).
- **Helpers LVGL** — wrappers autour de `lv_label_set_text`, `lv_arc_set_value`, `lv_bar_set_value` qui incluent la vérification boot-guard.
- **Logique couleur** — mappe les codes de condition météo ou les niveaux d'humidité des plantes en valeurs de couleur LVGL.
- **Machine d'états vocale** — met à jour la couleur de l'icône microphone selon l'état du pipeline (veille → écoute → traitement → synthèse → erreur).

---

## 4. Le paradigme Push

L'appareil n'initie jamais de requête réseau. Tout le flux de données va dans un seul sens :

```
Home Assistant                       Tab5 (ESP32-P4)
──────────────                       ───────────────
Changement d'état détecté
  → automatisation déclenchée
    → appel service : esphome.tab5_update_meteo_7j(payload)
      → gestionnaire API reçoit payload ────────────────────→
                                           parse_meteo_7j() s'exécute
                                           labels LVGL mis à jour
```

**Traffic pacing côté HA :** l'automatisation de push insère un délai de 1 seconde entre chaque bloc de service, et 150 ms entre les éléments dans les boucles de prévisions. Cela empêche la pile TCP de l'ESP32 de manquer de sockets quand plusieurs payloads larges arrivent simultanément avec le flux audio I2S.

---

## 5. Compression de données

Pour les prévisions météo 7 jours, 7 × 4 points de données (icône, temp max, temp min, code condition) représenteraient 28 appels de service séparés. À la place, HA construit une chaîne :

```
"0;Soleil;27;14;1;Nuageux;24;12;2;Pluie;19;11;..."
```

Le tokenizer C++ découpe sur `;` en un seul passage — O(n) sur la longueur de chaîne, pas O(n) sur le nombre d'appels. La mise à jour LVGL se fait ensuite une seule fois, de façon atomique, sans redraws intermédiaires.

Même schéma pour : graphique pluie horaire (60 valeurs), événements calendrier (titre + heure + tag couleur × 4 slots).
