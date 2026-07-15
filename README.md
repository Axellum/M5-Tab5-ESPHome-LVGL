# M5Stack Tab5 — ESPHome HMI with LVGL

<div align="center">

[![ESPHome](https://img.shields.io/badge/ESPHome-≥2025.9.3-blue)](https://esphome.io)
[![Build](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/actions/workflows/esphome-tab5.yml/badge.svg)](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/actions/workflows/esphome-tab5.yml)
[![LVGL](https://img.shields.io/badge/LVGL-8.4-green)](https://lvgl.io)
[![Home Assistant](https://img.shields.io/badge/Home_Assistant-Push_Events-orange)](https://www.home-assistant.io)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](LICENSE)
[![Made with AI](https://img.shields.io/badge/Made_with-AI-purple)](docs/related_projects.md)

</div>

> *A personal project exploring what's possible when AI writes all the code. Built with Claude, Gemini (Antigravity), Deepseek, Minimax, and z.ai — not a single line typed by hand.*

---

## English · [Français](#version-française)

---

## What this is

A Home Assistant smart-home dashboard running natively on a **M5Stack Tab5 V2** (ESP32-P4), built with ESPHome and LVGL 8.4.

The interface is compiled in C++ and embedded in the device firmware. It does not run a web browser, does not poll for data, and does not depend on a live network connection to stay functional. When Home Assistant has something new to show, it pushes the update directly to the screen.

**Screens:**
| Main Dashboard | Light Control | Climate Control |
|:-:|:-:|:-:|
| ![Main](docs/images/tab5_photo_console_diag.jpg) | ![Light](docs/images/tab5_design_light.jpg) | ![Climate](docs/images/tab5_photo_climate_popup.jpg) |

*Main and Climate are real photos of the running device (2026-07-06). Light Control still shows the earlier design reference — no fresh photo of that popup yet.*

More real photos: the same bottom card region toggled to the switches view, and the diagnostics console (opened via the console button, see [`docs/debugging.md`](docs/debugging.md)).

| Domo view (switches↔weather toggle) | Diagnostics console |
|:-:|:-:|
| ![Weather](docs/images/tab5_photo_dashboard_switches.jpg) | ![Console](docs/images/tab5_photo_dashboard_weather.jpg) |

---

## What it does

A single 1280×720 page with six functional areas, all driven by Home Assistant push events (see [ADR-0002](docs/decisions/0002-single-page-swipe-navigation.md) — there is no multi-screen tab bar):

- **Home area** — time, indoor temp/humidity, quick-action buttons, microphone icon with pipeline state; the date recolors with the active weather-alert level
- **Weather** — **5-window swipeable forecast** in the bottom region: windows 1–2 show hourly weather for the next 15 time slots (time, temperature color-coded, rainfall in mm, condition icon); windows 3–5 show the **15-day daily forecast** (5 days/window) with color-coded day names, dual-layer condition icons, and max/min temperatures
- **Central rotating card** — cycles every 8 s between planning, short-term rain graph, Météo-France vigilance icons, and an info panel (3-day calendar recap or alert banner)
- **Climate** — compact card + fullscreen popup: arc thermostat, mode grid (cool / heat / off / fan / dry / swing) and presets (eco / boost / quiet); controls are dimmed (not hidden) when the AC is off
- **Plants** — soil moisture card for up to 5 BLE plant sensors, dynamically sorted, color-coded by level (red = dry, green = optimal, blue = too wet)
- **Console** — diagnostics overlay (RAM/PSRAM, Wi-Fi, uptime, loop time, volume slider, double-tap reboot), opened via its dedicated button

**Voice assistant** — runs `okay_nabu` wake-word detection locally on the ESP32-P4. The microphone icon changes color to show the pipeline state in real time: grey (idle) → green (listening) → orange (processing) → blue (speaking) → red (error). Wake-word detection can be toggled on/off from the UI; tapping the mic icon triggers push-to-talk. Two modes selectable from the UI: standard Home Assistant agent or a free-form LLM conversation pipeline.

**Roller shutters** — script buttons on the home screen send open/close/position commands to Home Assistant cover entities.

→ Full screen-by-screen description: [`docs/screens.md`](docs/screens.md)

---

## Key design decisions

- **Push-only, zero polling.** The device never requests state from Home Assistant. Automations on the HA side detect changes and push data to the screen via native ESPHome service calls. CPU stays near zero when nothing changes.
- **Modular YAML.** The ESPHome configuration is split across eight files by concern (hardware, diagnostics sensors, home-automation sensors, API logic, styles, UI, globals, scripts). Each file stays under ~600 lines and is independently readable.
- **Native LVGL, no web stack.** Rendering runs at 60 FPS directly in the ESP32-P4's PSRAM. Vector fonts (Material Design Icons) replace image files entirely.
- **Data packing.** Complex payloads (15-day forecast, hourly forecast, weather alerts) are serialized as delimited strings on the HA side and parsed in C++ on the device — one network call, zero subsequent requests.
- **Offline resilience.** All C++ lambdas check `api.connected()` and `has_state()` before touching the UI. If HA restarts, the last known state stays on screen.

---

## Voice assistant

The device runs a local wake-word model (`okay_nabu` via micro_wake_word / TensorFlow Lite) directly on the ESP32-P4. Audio is captured at 16 kHz / 16-bit over I2S and streamed to Home Assistant only after wake-word detection — nothing goes over the network before that.

Sound output goes through the ES8388 DAC chip (I2C + I2S). Boot sequencing is careful about startup order to avoid the hardware pop that happens if the amplifier enable line fires before the I2S clock is stable.

→ Full details: [`docs/voice_assistant.md`](docs/voice_assistant.md)

---

## Documentation

| Page | Contents |
|------|----------|
| [`AGENTS.md`](AGENTS.md) | Entry point for AI coding agents — read order, build/verify commands, boundaries |
| [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md) | Full dependency graph and file-by-file inventory, with known technical debt |
| [`docs/screens.md`](docs/screens.md) | Screen-by-screen feature description |
| [`docs/architecture.md`](docs/architecture.md) | Modular YAML structure, push paradigm, data packing, boot guards |
| [`docs/hardware.md`](docs/hardware.md) | ESP32-P4 specs, GPIO mapping, ES8388 DAC, PSRAM, power |
| [`docs/ui_design.md`](docs/ui_design.md) | LVGL rendering, vector fonts, dynamic color, CPU optimizations |
| [`docs/voice_assistant.md`](docs/voice_assistant.md) | Wake word pipeline, audio chain, visual feedback states |
| [`docs/installation.md`](docs/installation.md) | Prerequisites, `user_entities.yaml`, secrets, flash & OTA |
| [`docs/demo_mode.md`](docs/demo_mode.md) | Try it in minutes, no Home Assistant required |
| [`docs/troubleshooting.md`](docs/troubleshooting.md) | Symptom → root cause → fix log for incidents already diagnosed |
| [`docs/debugging.md`](docs/debugging.md) | How to observe/diagnose the device (logs, console overlay, marker technique) |
| [`docs/decisions/`](docs/decisions/README.md) | Architecture decision records — the "why" behind non-obvious choices |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | PR workflow, compile gate, files never to commit |
| [`CHANGELOG.md`](CHANGELOG.md) | Version history |
| [`HomeAssistant_Config/README.md`](HomeAssistant_Config/README.md) | HA automations, scripts, template sensors |
| [`Tab5/README.md`](Tab5/README.md) | ESPHome file-by-file description |
| [`docs/related_projects.md`](docs/related_projects.md) | Linked projects, AI experiment context |

---

## Quick start

```bash
# Clone the repo
git clone https://github.com/Axellum/M5-Tab5-ESPHome-LVGL.git

# Copy local config files (gitignored), then edit with your values
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
# Create secrets.yaml — see docs/installation.md

# Compile via ESPHome dashboard or CLI:
# esphome run tab5-ha-hmi.yaml
```

Full step-by-step: [`docs/installation.md`](docs/installation.md)

Just want to see it running before setting up Home Assistant? → [`docs/demo_mode.md`](docs/demo_mode.md) pushes synthetic data to a flashed device with a small standalone script — no HA install, nothing left to clean up.

---

## Repository layout

```
.
├── tab5-ha-hmi.yaml          # Entry point — includes user_entities + packages
├── Tab5/
│   ├── user_entities.example.yaml  # Public template (copy → user_entities.yaml)
│   ├── tab5-hardware.yaml    # Display (MIPI-DSI), touch, I2C, audio, OTA
│   ├── tab5-sensors-diagnostics.yaml  # System entities (Wi-Fi, power, uptime, RAM)
│   ├── tab5-sensors-domotique.yaml    # HA entities (plants, lights, temps, audio)
│   ├── tab5-api-logic.yaml   # HA service handlers + C++ lambdas
│   ├── tab5-styles.yaml      # Global LVGL style definitions
│   ├── tab5-lvgl.yaml        # UI layout — screens, widgets, icons
│   ├── tab5-globals.yaml     # Shared global variables
│   ├── tab5-scripts.yaml     # ESPHome script blocks
│   ├── tab5_custom.h         # C++ declarations
│   └── tab5_custom.cpp       # C++ implementations (parsers, helpers)
├── HomeAssistant_Config/     # Automations, scripts, template sensors for HA
└── docs/                     # Extended documentation
```

---

## Note on AI

This project is part of a personal exploration of what AI tools can produce when given full authorship of a technical project. The code, the architecture decisions, and most of this documentation were generated by AI (Claude, Gemini/Antigravity, Deepseek, Minimax, z.ai). The goal was never to produce a polished product — it was to learn, to see where AI helps and where it gets stuck, and to share what came out of it.

If something in the code is weird, it might be an AI quirk. If something works surprisingly well, same answer.

→ More context: [`docs/related_projects.md`](docs/related_projects.md)

---

---

## Version Française

---

## C'est quoi

Un tableau de bord domotique Home Assistant qui tourne nativement sur un **M5Stack Tab5 V2** (ESP32-P4), construit avec ESPHome et LVGL 8.4.

L'interface est compilée en C++ et embarquée dans le firmware de l'appareil. Elle ne fait pas tourner de navigateur web, ne poll pas les données, et ne dépend pas d'une connexion réseau active pour rester fonctionnelle. Quand Home Assistant a quelque chose de nouveau à afficher, il pousse directement la mise à jour vers l'écran.

---

## Choix de conception

- **Push uniquement, zéro polling.** L'appareil ne demande jamais son état à Home Assistant. Les automations côté HA détectent les changements et poussent les données vers l'écran via des appels de service ESPHome natifs. Le CPU reste proche de zéro quand rien ne change.
- **YAML modulaire.** La configuration ESPHome est découpée en huit fichiers par domaine (hardware, capteurs diagnostics, capteurs domotique, logique API, styles, UI, globales, scripts). Chaque fichier reste sous ~600 lignes et est lisible indépendamment.
- **LVGL natif, pas de stack web.** Le rendu tourne à 60 FPS directement dans la PSRAM de l'ESP32-P4. Les polices vectorielles (Material Design Icons) remplacent complètement les fichiers image.
- **Compression de données.** Les payloads complexes (prévisions 15 jours, prévisions horaires, alertes météo) sont sérialisés en chaînes délimitées côté HA et parsés en C++ sur l'appareil — un seul appel réseau, zéro requête suivante.
- **Résilience hors-ligne.** Toutes les lambdas C++ vérifient `api.connected()` et `has_state()` avant de toucher l'UI. Si HA redémarre, le dernier état connu reste affiché.

---

## Ce que ça fait

Une page unique 1280×720 avec six zones fonctionnelles, toutes alimentées par des événements push Home Assistant (voir [ADR-0002](docs/decisions/0002-single-page-swipe-navigation.md) — il n'y a pas de barre d'onglets multi-écrans) :

- **Zone d'accueil** — heure, temp/humidité intérieure, boutons d'action rapide, icône microphone avec état du pipeline ; la date se recolore selon le niveau d'alerte météo actif
- **Météo** — **prévisions par swipe en 5 fenêtres** dans la zone du bas : fenêtres 1–2 = météo horaire pour les 15 prochaines tranches (heure, température avec code couleur, pluie en mm, icône condition) ; fenêtres 3–5 = **prévisions journalières 15 jours** (5 jours/fenêtre) avec noms de jours en code couleur, icônes double couche, temp max/min
- **Carte centrale rotative** — alterne toutes les 8 s entre planning, graphe de pluie court terme, icônes de vigilance Météo-France, et un panneau info (récap calendrier 3 jours ou bannière d'alerte)
- **Clim** — carte compacte + popup plein écran : arc thermostat, grille de modes (froid / chaud / arrêt / ventilation / sec / oscillation) et presets (éco / boost / silence) ; les contrôles sont estompés (non cachés) quand la clim est éteinte
- **Plantes** — carte d'humidité du sol pour jusqu'à 5 capteurs BLE, triés dynamiquement, code couleur par niveau (rouge = sec, vert = optimal, bleu = trop humide)
- **Console** — overlay diagnostics (RAM/PSRAM, Wi-Fi, uptime, temps de boucle, slider volume, reboot double-tap), ouvert via son bouton dédié

**Assistant vocal** — fait tourner la détection wake-word `okay_nabu` localement sur l'ESP32-P4. L'icône microphone change de couleur pour montrer l'état du pipeline en temps réel : gris (repos) → vert (écoute) → orange (traitement) → bleu (synthèse) → rouge (erreur). La détection wake-word peut être activée/désactivée depuis l'UI ; taper sur l'icône micro déclenche le push-to-talk. Deux modes sélectionnables depuis l'UI : agent Home Assistant standard ou pipeline de conversation LLM libre.

**Volets roulants** — des boutons de script sur l'écran d'accueil envoient des commandes ouvrir/fermer/position aux entités cover de Home Assistant.

→ Description écran par écran : [`docs/screens.md`](docs/screens.md)

---

## Assistant vocal

L'appareil fait tourner un modèle de wake-word local (`okay_nabu` via micro_wake_word / TensorFlow Lite) directement sur l'ESP32-P4. L'audio est capturé en 16 kHz / 16-bit sur I2S et streamé vers Home Assistant uniquement après la détection du wake-word — rien ne passe sur le réseau avant ça.

La sortie sonore passe par le chip DAC ES8388 (I2C + I2S). Le séquencement au démarrage est soigneux pour éviter le pop hardware qui se produit si la ligne d'activation amplificateur passe avant que l'horloge I2S soit stable.

→ Détail complet : [`docs/voice_assistant.md`](docs/voice_assistant.md)

---

## Documentation

| Page | Contenu |
|------|---------|
| [`AGENTS.md`](AGENTS.md) | Point d'entrée pour les agents IA — ordre de lecture, commandes build/vérif, frontières (en anglais) |
| [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md) | Graphe de dépendances complet et inventaire fichier par fichier, dette technique connue |
| [`docs/screens.md`](docs/screens.md) | Description fonctionnelle écran par écran |
| [`docs/architecture.md`](docs/architecture.md) | Structure YAML modulaire, paradigme push, data packing, boot guards |
| [`docs/hardware.md`](docs/hardware.md) | Specs ESP32-P4, mapping GPIO, DAC ES8388, PSRAM, alimentation |
| [`docs/ui_design.md`](docs/ui_design.md) | Rendu LVGL, polices vectorielles, couleur dynamique, optimisations CPU |
| [`docs/voice_assistant.md`](docs/voice_assistant.md) | Pipeline wake-word, chaîne audio, états de retour visuel |
| [`docs/installation.md`](docs/installation.md) | Prérequis, `user_entities.yaml`, secrets, flash & OTA |
| [`docs/demo_mode.md`](docs/demo_mode.md) | Tester en quelques minutes, sans Home Assistant |
| [`docs/troubleshooting.md`](docs/troubleshooting.md) | Journal symptôme → cause racine → correctif des incidents déjà diagnostiqués |
| [`docs/debugging.md`](docs/debugging.md) | Comment observer/diagnostiquer l'appareil (logs, overlay console, technique des marqueurs) |
| [`docs/decisions/`](docs/decisions/README.md) | Décisions d'architecture (ADR) — le "pourquoi" des choix non-évidents (en anglais) |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Workflow PR, gate compile, fichiers à ne jamais committer |
| [`CHANGELOG.md`](CHANGELOG.md) | Historique des versions |
| [`HomeAssistant_Config/README.md`](HomeAssistant_Config/README.md) | Automations HA, scripts, template sensors |
| [`Tab5/README.md`](Tab5/README.md) | Description fichier par fichier ESPHome |
| [`docs/related_projects.md`](docs/related_projects.md) | Projets liés, contexte expérimentation IA |

---

## Note sur l'IA

Ce projet fait partie d'une exploration personnelle de ce que les outils IA peuvent produire quand on leur laisse la pleine paternité d'un projet technique. Le code, les décisions d'architecture et la plupart de cette documentation ont été générés par des IA (Claude, Gemini/Antigravity, Deepseek, Minimax, z.ai). Le but n'a jamais été de produire un produit fini — c'était d'apprendre, de voir où l'IA aide et où elle coince, et de partager ce qui en est sorti.

Si quelque chose dans le code est bizarre, c'est peut-être un quirk d'IA. Si quelque chose marche étonnamment bien, même réponse.

→ Plus de contexte : [`docs/related_projects.md`](docs/related_projects.md)
