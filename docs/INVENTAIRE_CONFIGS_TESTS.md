# Inventaire des configurations YAML et des suites de tests

> **[AI-CONTEXT] RÔLE DE CE FICHIER**
> Ce document consigne l'emplacement précis des fichiers de configuration YAML
> (ESPHome et Home Assistant) ainsi que l'arborescence des suites de tests
> unitaires et d'intégration existantes dans le dépôt `00ProjetTab/`.
> Il sert de référence pour toute tâche d'inspection, de maintenance ou
> d'extension du projet. Les chemins sont relatifs à la racine du dépôt
> `H:\AuxFilsDesIdees\00ProjetTab`.

`Généré le 2026-08-01` · Sources vérifiées directement dans l'arborescence du dépôt.

---

## 1. Configurations YAML ESPHome

### 1.1 Point d'entrée

| Fichier | Emplacement | Rôle |
|---|---|---|
| `tab5-ha-hmi.yaml` | Racine du dépôt | Point d'entrée ESPHome : `substitutions:`, `packages:`, `esphome: includes:`, séquence `on_boot:`. |

### 1.2 Packages ESPHome (`Tab5/*.yaml`)

| Fichier | Emplacement | Rôle |
|---|---|---|
| `tab5-ui-tokens.yaml` | `Tab5/` | Tokens dimensionnels (modal_card_w/h, modal_body_y). |
| `tab5-hardware.yaml` | `Tab5/` | Bas niveau : display MIPI-DSI, tactile ST7123, DAC/ADC audio, expander GPIO, esp32_hosted, wake words, OTA. |
| `tab5-sensors-diagnostics.yaml` | `Tab5/` | WiFi, alimentation GPIO, statut API HA, uptime, RAM, loop time, horloge SNTP. |
| `tab5-sensors-domotique.yaml` | `Tab5/` | Miroirs d'entités HA : plantes, lumières, PC, températures, batterie, audio. |
| `tab5-api-logic.yaml` | `Tab5/` | Contrat API HA↔Tab5 : bloc `api: services:` (14 services). |
| `tab5-styles.yaml` | `Tab5/` | Thème "Dark Mode Slate" : tokens `color:`, déclarations `font:`, `lvgl: style_definitions:`. |
| `tab5-globals.yaml` | `Tab5/` | État partagé (`globals:`) + rotateur carte centrale (interval 8s). |
| `tab5-scripts.yaml` | `Tab5/` | Scripts ESPHome : debounces, vocal, rotateur, volet, popups, jeux. |
| `tab5-lvgl.yaml` | `Tab5/` | Layout complet : page unique 1280×720, swipe prévisions, console, popups. |
| `tab5-imu.yaml` | `Tab5/` | BMI270 IMU : `motion:`, poll adaptatif 10/30Hz, tap-to-wake. |
| `tab5-ha-controls.yaml` | `Tab5/` | Number volume, text_sensor écran courant, select aller-à, button recharger calendrier. |
| `tab5-alarm.yaml` | `Tab5/` | Réveil : rtttl, ~20 entités HA, machine d'état sonnerie, tick 1s. |

### 1.3 Composants UI (`Tab5/ui_components/*.yaml`)

33 fichiers, dont 21 inclus directement par `tab5-lvgl.yaml`. Exemples :

| Fichier | Emplacement | Rôle |
|---|---|---|
| `climate_card.yaml` | `Tab5/ui_components/` | Carte clim compacte. |
| `climate_popup.yaml` | `Tab5/ui_components/` | Popup clim plein écran. |
| `forecast_daily.yaml` | `Tab5/ui_components/` | 5 cartes prévisions journalières. |
| `forecast_hourly.yaml` | `Tab5/ui_components/` | 5 cartes prévisions horaires. |
| `switches_card.yaml` | `Tab5/ui_components/` | Cartes switches (PC, volet, lumières). |
| `console_sys.yaml` | `Tab5/ui_components/` | Console Système en 4 cartes. |
| `light_popup.yaml` | `Tab5/ui_components/` | Popup contrôle lumière. |
| `tv_remote_popup.yaml` | `Tab5/ui_components/` | Popup télécommande TV Samsung. |
| `moisture_sensors.yaml` | `Tab5/ui_components/` | 4 slots UI humidité plantes. |
| `pots_popup.yaml` | `Tab5/ui_components/` | Popup détails plantes. |
| `calendar_popup.yaml` | `Tab5/ui_components/` | Popup calendrier mensuel. |
| `assistant_popup.yaml` | `Tab5/ui_components/` | Popup assistant vocal. |
| `modal_scrim.yaml` | `Tab5/ui_components/` | Voile d'assombrissement (chrome partagé). |
| `modal_header.yaml` | `Tab5/ui_components/` | Barre de titre 52px (chrome partagé). |
| `game_selector.yaml` | `Tab5/ui_components/` | Sélecteur Arcade (grille 4×2). |
| `marble_game.yaml` … `draughts_game.yaml` | `Tab5/ui_components/` | 8 consoles de jeu (pages LVGL dédiées). |

### 1.4 Fichiers de substitution et secrets

| Fichier | Emplacement | Statut |
|---|---|---|
| `Tab5/user_entities.yaml` | `Tab5/` | **Gitignoré** — entités HA réelles d'Axel. |
| `Tab5/user_entities.example.yaml` | `Tab5/` | Modèle public des substitutions. |
| `secrets.yaml` | Racine | **Gitignoré** — secrets ESPHome. |
| `Tab5/secrets.yaml` | `Tab5/` | **Gitignoré** — secrets ESPHome (variante). |

---

## 2. Configurations YAML Home Assistant (`HomeAssistant_Config/`)

### 2.1 Fichiers de production (gitignorés)

| Fichier | Emplacement | Rôle |
|---|---|---|
| `automations_tab5.yaml` | `HomeAssistant_Config/` | Automation push principale (météo, pluie, clim, planning, alertes, plantes). |
| `scripts_tab5.yaml` | `HomeAssistant_Config/` | Scripts déclenchés par le Tab5. |
| `template_sensors_meteo_tab5.yaml` | `HomeAssistant_Config/` | Pré-traitement Météo-France côté HA. |

### 2.2 Fichiers publics (trackés)

| Fichier | Emplacement | Rôle |
|---|---|---|
| `automations_examples.yaml.example` | `HomeAssistant_Config/` | Placeholder générique des automations. |
| `scripts_examples.yaml` | `HomeAssistant_Config/` | Placeholder générique des scripts. |
| `template_sensors_examples.yaml` | `HomeAssistant_Config/` | Placeholder générique des template sensors. |
| `packages/tab5_alerts.yaml` | `HomeAssistant_Config/packages/` | Package alertes HA. |
| `packages/tab5_calendar.yaml` | `HomeAssistant_Config/packages/` | Package calendrier HA. |
| `packages/tab5_health.yaml` | `HomeAssistant_Config/packages/` | Package santé HA. |
| `packages/tab5_reveil.yaml` | `HomeAssistant_Config/packages/` | Package réveil HA. |
| `packages/tab5_tv.yaml` | `HomeAssistant_Config/packages/` | Package TV HA. |
| `packages/volet_serre_tracking.yaml` | `HomeAssistant_Config/packages/` | Package suivi volet/serre. |
| `snippets/tab5_alerts_dismissed_input_text.yaml` | `HomeAssistant_Config/snippets/` | Snippet input_text alertes. |

---

## 3. Suites de tests unitaires et d'intégration

### 3.1 Tests pytest (`tests/`)

| Fichier | Emplacement | Type | Cible |
|---|---|---|---|
| `test_verifier_secrets_config.py` | `tests/` | Unitaire | Détection de secrets en clair dans les YAML (`tools/verifier_secrets_config.py`). |
| `__init__.py` | `tests/` | — | Marqueur de package. |

### 3.2 Tests moteurs de jeux (`tools/`)

| Fichier | Emplacement | Type | Cible |
|---|---|---|---|
| `test_go_engine.py` | `tools/` | Unitaire (miroir Python) | Règles Go : capture, suicide, ko, territoire, score. |
| `test_chess_perft.py` | `tools/` | Unitaire (miroir Python) | Générateur d'échecs contre la suite perft standard. |
| `test_go_engine.cpp` | `tools/` | Unitaire (C++ hôte) | Même suite compilée contre le vrai `go_engine.cpp`. |

### 3.3 Outils de validation (intégration)

| Fichier | Emplacement | Type | Rôle |
|---|---|---|---|
| `tools/demo/demo_pusher.py` | `tools/demo/` | Intégration (dry-run) | Valide chaque payload push contre le contrat firmware. |
| `tools/verifier_secrets_config.py` | `tools/` | Outil | Analyse les YAML pour détecter des secrets en clair. |

### 3.4 Commandes de lancement

```bash
# Tests pytest (dossier tests/)
pytest tests/

# Tests moteurs de jeux (miroirs Python)
python tools/test_go_engine.py
python tools/test_chess_perft.py

# Validation des payloads push (dry-run, sans matériel)
python tools/demo/demo_pusher.py --dry-run
```

---

## 4. Résumé de l'arborescence des tests

```
00ProjetTab/
├── tests/
│   ├── __init__.py
│   └── test_verifier_secrets_config.py
├── tools/
│   ├── demo/
│   │   ├── demo_pusher.py
│   │   ├── requirements.txt
│   │   └── scenarios.py
│   ├── test_go_engine.py
│   ├── test_go_engine.cpp
│   ├── test_chess_perft.py
│   ├── make_chess_font.py
│   └── verifier_secrets_config.py
└── .github/workflows/
    └── esphome-tab5.yml   (CI : compile ESPHome + dummy secrets.yaml)
```

---

## 5. Notes importantes

- **Pas de suite de tests unitaires pour la HMI** : la logique LVGL (`tab5_custom.cpp`) n'a pas de tests hôte. Seuls les moteurs de jeux (Go, échecs) disposent de tests exécutables sur PC.
- **Les tests Go/échecs sont des miroirs Python** du C++ : toute modification du C++ doit être reflétée dans le miroir Python, sinon le test ne prouve plus rien.
- **CI GitHub Actions** (`.github/workflows/esphome-tab5.yml`) : génère un `secrets.yaml` factice et compile via `esphome/build-action@v8.0.0` à chaque push/PR.
- **Fichiers gitignorés** : `secrets.yaml`, `Tab5/secrets.yaml`, `Tab5/user_entities.yaml`, `HomeAssistant_Config/automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`, `Tab5/tts_library*/`, `archives/`.
