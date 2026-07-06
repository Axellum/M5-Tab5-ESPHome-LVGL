# Cartographie intégrale — Projet M5Stack Tab5 V2 (HMI ESPHome/LVGL)

> **[AI-CONTEXT] PRÉSENTATION ET RÔLE DE CE FICHIER**
> Ce fichier est la cartographie officielle du projet Tab5. Il a été créé **spécifiquement pour guider les agents IA** (Claude, Gemini, etc.) dans leur compréhension de l'architecture du firmware.
> Au lieu de lire et d'analyser à l'aveugle les dizaines de fichiers YAML et C++, **l'IA doit lire cette cartographie en premier**. Elle y trouvera l'arbre des dépendances (7-YAML), la répartition des rôles entre le YAML et le C++, ainsi que l'historique des bugs résolus et de la dette technique. Cela évite les hallucinations et le temps perdu en rétro-ingénierie.

`Généré le 2026-07-06` · Sources vérifiées directement dans le code (`00ProjetTab/`), croisées avec `Tab5/README.md` (réécrit le 05/07/2026 contre le firmware réel), `contexte_ia/04_Projets/etat_tab5.md` et `contexte_ia/02_Hardware/rules_esphome.md`. Aucun fait ci-dessous n'est tiré d'une supposition — chaque ligne cite le fichier source lu.

Repo Git distinct : `Axellum/M5-Tab5-ESPHome-LVGL` (dossier local `00ProjetTab/`), branche `main`.

---

## 1. Vue d'ensemble en une phrase

Un tableau de bord domotique 60 FPS + satellite vocal local tournant **entièrement en firmware C++/LVGL** sur un M5Stack Tab5 V2 (ESP32-P4), architecture **YAML modulaire par domaine** (7 packages + `ui_components/`), **push-only** depuis Home Assistant (zéro polling), avec toute la logique non-triviale centralisée dans deux fichiers C++ (`tab5_custom.h/.cpp`).

---

## 2. Diagramme Mermaid — arbre des dépendances

```mermaid
graph TD
    ENTRY["tab5-ha-hmi.yaml<br/>(point d'entrée, 146 lignes)<br/>substitutions + on_boot + api:/ota: + packages:"]

    subgraph PKG["Packages ESPHome (Tab5/*.yaml)"]
        HW["tab5-hardware.yaml<br/>306 lignes<br/>display/touch/i2c/audio/esp32_hosted"]
        SENS["tab5-sensors.yaml<br/>670 lignes (LE PLUS GROS)<br/>wifi:/switch:/sensor:/text_sensor:/select:/time:"]
        API["tab5-api-logic.yaml<br/>466 lignes<br/>api: services: (contrat HA)"]
        STY["tab5-styles.yaml<br/>304 lignes<br/>color:/font:/lvgl: style_definitions"]
        GLOB["tab5-globals.yaml<br/>129 lignes<br/>globals: + rotateur carte centrale (8s)"]
        SCR["tab5-scripts.yaml<br/>29 lignes<br/>script: (debounce, séquences)"]
        LVGL["tab5-lvgl.yaml<br/>407 lignes<br/>page_main + gestes swipe"]
    end

    subgraph UI["ui_components/*.yaml (16 fichiers, inclus par tab5-lvgl.yaml)"]
        MOIST["moisture_sensors.yaml (51L)"]
        CLIMCARD["climate_card.yaml (95L)"]
        CLIMPOP["climate_popup.yaml (234L)<br/>NON factorisé, grille 3×3"]
        CLIMBTN["climate_hvac_mode_btn.yaml (17L)<br/>climate_preset_toggle_btn.yaml (17L)<br/>templates paramétrés !include+vars"]
        FDAILY["forecast_daily.yaml (276L)"]
        FDTAB["forecast_day_title_tab.yaml (10L)<br/>forecast_day_temp_tab.yaml (26L)<br/>templates paramétrés"]
        FHOUR["forecast_hourly.yaml (19L)<br/>forecast_hour_card.yaml (70L)"]
        SWCARD["switches_card.yaml (189L)"]
        SWTAB["switch_card_title_tab.yaml (8L)<br/>switch_card_state_tab.yaml (8L)"]
        CONSOLE["console_sys.yaml (221L)<br/>diagnostics + reboot double-tap"]
        LIGHTPOP["light_popup.yaml (141L)"]
        LIGHTBTN["light_color_preset_btn.yaml (26L)"]
    end

    subgraph CPP["C++ (esphome: includes:)"]
        HFILE["tab5_custom.h (128L)<br/>déclarations, structs, UIColor::, MeteoIcon::"]
        CFILE["tab5_custom.cpp (520L)<br/>toute la logique LVGL non-triviale"]
    end

    subgraph HWCOMP["Composants matériels (natifs + custom)"]
        MIPIDSI["display: mipi_dsi<br/>M5STACK-TAB5-V2, 1280×720, 16bit RGB565"]
        ST7123TOUCH["my_components/st7123/touchscreen/<br/>st7123_touchscreen.cpp/.h<br/>(composant custom I2C, ACTIF)"]
        ST7123BTN["my_components/st7123/binary_sensor/<br/>st7123_button.cpp/.h<br/>(composant custom, JAMAIS INSTANCIÉ)"]
        PI4IOE["pi4ioe5v6408 ×2 (pi4ioe1/pi4ioe2)<br/>GPIO expander I2C — reset écran/tactile, alim WiFi/ampli"]
        ES8388["audio_dac: es8388 (sortie haut-parleur)"]
        ES7210["audio_adc: es7210 (entrée micro)"]
        HOSTED["esp32_hosted (co-proc esp32c6, WiFi via SPI)"]
        MWW["micro_wake_word (okay_nabu, TFLite local)"]
        VA["voice_assistant (pipeline HA)"]
    end

    subgraph HASIDE["Côté Home Assistant (HomeAssistant_Config/, gitignoré=config réelle)"]
        AUTO["automations_tab5.yaml (468L, gitignoré)<br/>push météo/clim/planning/alertes/plantes"]
        SCRHA["scripts_tab5.yaml (100L, gitignoré)"]
        TPLHA["template_sensors_meteo_tab5.yaml (49L, gitignoré)"]
    end

    ENTRY -->|packages:| HW
    ENTRY -->|packages:| SENS
    ENTRY -->|packages:| API
    ENTRY -->|packages:| STY
    ENTRY -->|packages:| GLOB
    ENTRY -->|packages:| SCR
    ENTRY -->|packages:| LVGL
    ENTRY -->|includes:| HFILE
    ENTRY -->|includes:| CFILE

    HW -->|external_components:<br/>path local| ST7123TOUCH
    HW -.->|composant déclaré<br/>mais platform jamais utilisée| ST7123BTN
    HW --> MIPIDSI
    HW --> PI4IOE
    HW --> ES8388
    HW --> ES7210
    HW --> HOSTED
    HW --> MWW
    HW --> VA
    ST7123TOUCH -->|reset_pin via| PI4IOE
    MIPIDSI -->|reset_pin via| PI4IOE

    LVGL --> MOIST
    LVGL --> CLIMCARD
    LVGL --> FDAILY
    LVGL --> FHOUR
    LVGL --> SWCARD
    LVGL --> CONSOLE
    LVGL --> CLIMPOP
    LVGL --> LIGHTPOP

    CLIMPOP -->|!include+vars| CLIMBTN
    FDAILY -->|!include+vars| FDTAB
    FHOUR -->|!include+vars| FHOUR
    SWCARD -->|!include+vars| SWTAB
    LIGHTPOP -->|!include+vars| LIGHTBTN

    LVGL -->|lambdas appellent| CFILE
    API -->|lambdas appellent| CFILE
    SENS -->|lambdas appellent| CFILE
    UI -->|lambdas appellent| CFILE
    GLOB -->|lambda transition_widgets| CFILE

    AUTO -->|homeassistant.service<br/>tab5_maj_*| API
    API -->|expose api: services:| AUTO
```

---

## 3. Inventaire strict des fichiers

### 3.1 Point d'entrée

| Fichier | Rôle exact | Gère | Dépend de |
|---|---|---|---|
| `tab5-ha-hmi.yaml` (146L) | Point d'entrée ESPHome. Bloc `substitutions:` (entités HA à adapter), séquence `on_boot:` en 2 priorités (700 puis 600), `packages:` qui importe les 7 fichiers `Tab5/*.yaml`, `esphome: includes:` pour le C++. | Boot, substitutions utilisateur, orchestration des packages | `Tab5/tab5_custom.h/.cpp`, tous les `Tab5/*.yaml` |

Point notable vérifié dans le code : le délai bloquant `on_boot:priority:700: lambda: delay(1000);` est la **cause racine confirmée** (06/07/2026, 5 tests OTA avec Axel présent) du bug historique « écran noir après reboot logiciel » — le `reset_pin` de l'écran passe par le GPIO expander I2C `PI4IOE5V6408`, qui a besoin de temps pour se stabiliser après boot avant que le reset ait un effet fiable. Documenté en détail dans `tab5-hardware.yaml:33-69`.

### 3.2 Packages ESPHome (`Tab5/*.yaml`)

| Fichier | Lignes | Rôle exact | Gère | Dépend de |
|---|---|---|---|---|
| `tab5-hardware.yaml` | 306 | Bas niveau : bus display/touch, init I2C DAC ES8388, I2S haut-parleur/micro, expander GPIO PI4IOE5V6408, `esp32_hosted` (co-proc WiFi ESP32-C6 via SPI), `micro_wake_word`/`voice_assistant`, `ota:` | Hardware, audio, wake-word, OTA | `external_components: my_components/st7123` |
| `tab5-sensors.yaml` | 670 (**le plus gros fichier du projet**) | `wifi:`, tous les `sensor:`/`text_sensor:`/`binary_sensor:`/`switch:`/`select:` exposés à l'API : diagnostics système (RAM/PSRAM/uptime/WiFi), humidité 5 plantes (triées dynamiquement), miroirs d'état lumière/PC/volet, wiring volume | Réseau WiFi, capteurs physiques et miroirs d'entités HA | `tab5_custom.h` (fonctions `get_temperature_color`, `get_humidity_color`, `sort_and_update_moisture_slots`, `update_light_card_ui`) |
| `tab5-api-logic.yaml` | 466 | Le contrat réel avec HA : bloc `api: services:`. Chaque service `tab5_maj_*` reçoit un payload d'une automation HA et appelle une fonction `tab5_custom.cpp` via lambda | Contrat API HA↔Tab5 (clim, volet, planning, alertes météo France, prévisions bulk, pluie 1h) | `tab5_custom.h/.cpp`, IDs LVGL définis dans `tab5-lvgl.yaml`/`ui_components/*.yaml` |
| `tab5-styles.yaml` | 304 | Thème "Dark Mode Slate" (glassmorphism) : tokens `color:`, déclarations `font:` (Roboto + MDI + police météo custom), `lvgl: style_definitions:` | Palette visuelle, typographie, styles réutilisables | Polices `Tab5/materialdesignicons-webfont.ttf`, `Tab5/IconeMeteo.ttf` |
| `tab5-globals.yaml` | 129 | Tout l'état partagé entre fichiers (`globals:`) + l'`interval: 8s` qui fait tourner la carte centrale (planning/pluie/alertes) | État global partagé, rotateur carte centrale | `tab5_custom.cpp` (`transition_widgets()`) |
| `tab5-scripts.yaml` | 29 | Scripts ESPHome réutilisables : affichage temporaire planning, debounce slider volume (150ms) | Séquences temporisées | `globals:` (`system_volume`, `is_showing_temp_planning`) |
| `tab5-lvgl.yaml` | 407 | Layout complet : page unique 1280×720 (`page_main`), gestion des gestes swipe (haut=console, bas=fermer, gauche/droite=pagination prévisions 0-4), boutons statut/mode vocal/horloge, carte centrale, pagination | Layout racine, navigation gestuelle | Tous les `ui_components/*.yaml`, `tab5_custom.cpp` (`refresh_daily_forecast`, `refresh_hourly_forecast`) |

### 3.3 C++ core

| Fichier | Lignes | Rôle exact | Fonctions clés |
|---|---|---|---|
| `tab5_custom.h` | 128 | Déclarations, structs (`DayForecastData`, `HourForecastData`, `WeatherHourSlot`, `WeatherDaySlot`, `MoistureSlotUI`), namespace `MeteoIcon::` (codes UTF-8 police météo), namespace `UIColor::` (palette sémantique — **miroir exact des tokens `color:` YAML, à garder synchro manuellement**) | — |
| `tab5_custom.cpp` | 520 | Toute la logique LVGL non-triviale, gardée contre les `lv_obj_t*` nuls (LVGL pas encore initialisé) | `update_meteo_icon()` (icônes météo double-couche), `get_humidity_color()`/`get_temperature_color()` (gradients colorimétriques continus), `parse_and_update_heures_bulk()`/`parse_and_update_jours_bulk()` (parsing `strtok_r` in-place, garde OOM à 2048 octets), `refresh_daily_forecast()`/`refresh_hourly_forecast()`, `update_light_card_ui()` (factorisée #T164, ex-triplée), `sort_and_update_moisture_slots()` (tri bubble 5→4 slots), `transition_widgets()` (animation glissement+fondu 450ms) |

**Règle d'architecture vérifiée et respectée dans le code** (`Tab5/README.md:44`) : les `sensor:`/`text_sensor:` YAML ne manipulent jamais `lv_obj_*` directement — ils appellent toujours une fonction `tab5_custom.cpp`. Confirmé par lecture de `tab5-sensors.yaml` (tous les `on_value:` appellent une fonction C++ nommée, sauf les cas triviaux de couleur d'icône à 2-3 lignes qui restent inline).

### 3.4 Composants UI (`ui_components/*.yaml`, 16 fichiers inclus par `tab5-lvgl.yaml`)

| Fichier | Lignes | Rôle | Statut factorisation |
|---|---|---|---|
| `climate_card.yaml` | 95 | Carte clim compacte (dashboard principal) | — |
| `climate_popup.yaml` | 234 | Popup fullscreen clim, grille 3×3 (9 boutons mode/preset) | **Partiellement factorisé** (#T164, 06/07) : 6/9 boutons via `climate_hvac_mode_btn.yaml`/`climate_preset_toggle_btn.yaml`. Les 3 restants (`off`, `swing`, `quiet`) + les 2 boutons +/- température volontairement non factorisés (service HA différent par bouton, décision explicite documentée dans `etat_tab5.md`) |
| `climate_hvac_mode_btn.yaml` | 17 | Template paramétré (`!include`+`vars`) pour bouton mode HVAC | Template réutilisé 4× |
| `climate_preset_toggle_btn.yaml` | 17 | Template paramétré pour bouton preset (eco/boost) | Template réutilisé 2× |
| `forecast_daily.yaml` | 276 | 5 cartes prévisions journalières (fenêtre glissante sur 15 jours) | Onglets titre/température factorisés via `forecast_day_title_tab.yaml`/`forecast_day_temp_tab.yaml` ; le "corps sombre + action" par carte reste dupliqué 5× (actions HA différentes par carte, cf. §4) |
| `forecast_day_title_tab.yaml` | 10 | Template onglet titre jour | Réutilisé 5× |
| `forecast_day_temp_tab.yaml` | 26 | Template onglet température jour | Réutilisé 5× |
| `forecast_hourly.yaml` | 19 | Conteneur 5 cartes prévisions horaires | `!include`+`vars`, 365→19 lignes après factorisation (historique PR #7) |
| `forecast_hour_card.yaml` | 70 | Template carte horaire individuelle | Réutilisé 5× |
| `switches_card.yaml` | 189 | Cartes switches (PC, volet, lumières) | Onglets titre/état factorisés via `switch_card_title_tab.yaml`/`switch_card_state_tab.yaml` ; actions par carte laissées en clair (logique différente par carte) |
| `switch_card_title_tab.yaml` | 8 | Template onglet titre switch | Réutilisé 3× |
| `switch_card_state_tab.yaml` | 8 | Template onglet état switch | Réutilisé 3× |
| `console_sys.yaml` | 221 | Overlay diagnostic (SRAM/PSRAM/fragmentation/IP/SSID/loop time), reboot par double-tap | — |
| `light_popup.yaml` | 141 | Popup contrôle lumière (couleur, luminosité) | Presets couleur factorisés via `light_color_preset_btn.yaml` (8 instances) |
| `light_color_preset_btn.yaml` | 26 | Template preset couleur lumière | Réutilisé 8× |
| `moisture_sensors.yaml` | 51 | 4 slots UI humidité plantes (tri dynamique sur 5 capteurs BLE) | — |

### 3.5 Composant matériel custom (`my_components/st7123/`)

| Fichier | Lignes | Rôle | Statut |
|---|---|---|---|
| `st7123/__init__.py` | 6 | Déclaration du namespace ESPHome + dépendance `i2c` | Actif |
| `st7123/touchscreen/st7123_touchscreen.cpp/.h` | 100 + 59 | Pilote tactile custom pour le contrôleur I2C ST7123 (jusqu'à 10 points de touche simultanés, registres `REG_GET_TOUCH_INFO`/`REG_GET_TOUCH`) | **Utilisé** — instancié dans `tab5-hardware.yaml:121` (`touchscreen: platform: st7123`) |
| `st7123/binary_sensor/st7123_button.cpp/.h` | 27 + 28 | Pilote pour un bouton physique lié au même contrôleur | **CODE MORT confirmé par grep** — aucun `binary_sensor: platform: st7123` nulle part dans le YAML actif (`tab5-hardware.yaml`, `tab5-sensors.yaml`). Seule la version obsolète `Tab5_backup_20260525/tab5-hardware.yaml` le référence |

### 3.6 Côté Home Assistant (`HomeAssistant_Config/`)

Tous ces fichiers sont **gitignorés** (`.gitignore:20-23`) — ce sont les vrais fichiers de prod d'Axel, non versionnés dans le repo public. Seuls les `*_examples.yaml*` (placeholders génériques) sont trackés.

| Fichier | Lignes | Rôle |
|---|---|---|
| `automations_tab5.yaml` | 468 (gitignoré) | Automation push principale : météo 7j, pluie horaire, températures/humidité, clim, planning Google Calendar, alertes Météo-France, humidité plantes. Pacing `delay: 1s` entre blocs, `150ms` dans les boucles |
| `scripts_tab5.yaml` | 100 (gitignoré) | Scripts déclenchés **par** le Tab5 (bouton physique → action HA) |
| `template_sensors_meteo_tab5.yaml` | 49 (gitignoré) | Pré-traitement Météo-France côté HA (phrase météo courte) avant envoi au device |

### 3.7 CI/CD et documentation

| Fichier | Rôle |
|---|---|
| `.github/workflows/esphome-tab5.yml` | CI GitHub Actions : génère un `secrets.yaml` factice, compile via `esphome/build-action@v7.3.0`, upload le firmware en artifact |
| `README.md` (racine) | Doc utilisateur bilingue EN/FR, à jour, décrit les 6 écrans et les choix d'architecture |
| `Tab5/README.md` | **Réécrit le 05/07/2026**, description fichier-par-fichier + table des services API + table des globals + 6 règles de code — vérifié ligne à ligne contre le firmware réel le jour de l'écriture |
| `docs/*.md` (9 fichiers) | `architecture.md`, `hardware.md`, `ui_design.md`, `voice_assistant.md`, `installation.md`, `screens.md`, `related_projects.md`, `LVGL_PREMIUM_TEMPLATES.md`, plus 3 rapports d'audit LLM (`analyse_esphome_auto.md`, `analyse_globale_tab5_v2.md`, `audit_rapport_final.md`, `Audit Exhaustif...md`) |

---

## 4. Points de friction / dette technique

Classés par impact décroissant. Chaque point est vérifié contre le code ou la documentation d'état — aucun n'est une supposition.

### 4.1 Dette de repository (poids mort versionné)

- **`Tab5_backup_20260525/` est trackée en Git** (31 fichiers, 1,5 Mo) — un snapshot complet de l'ancienne architecture (avant factorisation #T164), y compris des fichiers `.pyc` compilés (`__pycache__/__init__.cpython-313.pyc`) qui n'ont strictement rien à faire dans un dépôt Git. Ce dossier référence même l'ancienne version obsolète du composant `st7123` (avec `binary_sensor` actif). Candidat de suppression immédiate — c'est un backup, pas une release taggée.
- **4 fichiers cruft à la racine, non gitignorés au moment de l'écriture** : `config_dump.yaml`, `esphome_live_logs.txt` sont dans `.gitignore` mais `home-assistant_2026-06-07T*.log` (2 fichiers) matchent le pattern `home-assistant_*.log` du `.gitignore` — à vérifier qu'ils ne sont pas déjà trackés depuis avant l'ajout de la règle (`git ls-files` n'en a pas remonté, donc priorité basse, mais à surveiller).
- **`archives/` (4,2 Mo, gitignoré)** : dossier de travail avec des images scramblées (`Png_card_scrambled/`, `Png_main_scrambled/`) et anciens YAML Nextion-like — hors Git donc pas un problème de repo, mais pollue le contexte si un outil d'IA scanne le dossier localement sans respecter le `.gitignore`.

### 4.2 Code mort confirmé

- **`my_components/st7123/binary_sensor/st7123_button.cpp/.h` n'est jamais instancié.** Vérifié par `grep -r "st7123" *.yaml` : aucun `binary_sensor: platform: st7123` dans la config active. Le composant existe, compile probablement (fait partie du `external_components:` local), mais ne sert à rien dans le firmware actuellement flashé. Soit un bouton physique prévu puis abandonné, soit un reliquat de portage.
- **Doublon `cal_jour_nom[15]`/`cal_heures[15]` dans `tab5_custom.cpp`** : `parse_and_update_jours_bulk()` remplit à la fois les nouveaux tableaux `cal_jours_data[]`/`cal_heures_data[]` (structs) ET les anciens tableaux legacy `cal_jour_nom[]`/`cal_heures[]`, commentés explicitement `// Rétrocompatibilité`. Aucun appelant de `cal_jour_nom`/`cal_heures` n'a été trouvé en dehors de `refresh_daily_forecast()` (qui les utilise pour le toggle calendrier `cal_toggled[]`) — à vérifier si les deux structures sont réellement encore nécessaires ou si c'est une v1→v2 jamais nettoyée.
- **Blocs commentés dans `tab5-api-logic.yaml`** (`tab5_maj_clim`, lignes ~185-224) : plusieurs `lv_obj_set_style_text_color(id(icon_clim_cool)...)` etc. sont laissés en commentaire (widgets `icon_clim_*` de l'ancienne carte compacte, remplacés par `popup_icon_clim_*`). Ce sont des vestiges d'une UI antérieure, à nettoyer une fois confirmé qu'`icon_clim_cool` etc. n'existent plus dans `tab5-lvgl.yaml`/`climate_card.yaml`.
- **`tab5_maj_info_texte` (service API, `tab5-api-logic.yaml:419-431`) a un corps de lambda vide** avec le commentaire `// Conteneur conserve, on laisse la methode vide en attendant d'y mettre les alertes` — service API exposé côté HA mais no-op côté firmware.

### 4.3 Violations documentées des propres règles du projet

Le fichier `Tab5/README.md` liste 6 règles issues de l'audit du 05/07/2026. Vérification croisée :

- **Règle "pas de couleur en dur"** : `tab5-api-logic.yaml` contient de nombreuses couleurs hexadécimales en dur dans le service `tab5_maj_clim` (`c_cool_active = 0x4D94FF`, `c_heat_active = 0xFF4D4D`, etc., lignes 157-171) — définies localement dans la lambda au lieu d'utiliser des tokens `UIColor::`. C'est exactement le pattern que la règle 1 du README interdit. Friction confirmée, pas juste théorique.
- **`climate_popup.yaml` a `pressed: bg_opa: 30%` répété sur ~10 boutons individuels** — découverte documentée dans `etat_tab5.md` (06/07) : `pressed:` ne peut *pas* être mis dans un `style_definitions:` partagé (rejeté par le validateur ESPHome). C'est une contrainte du framework, pas un choix de code — mais ça reste 10 répétitions identiques qui grossiront à chaque nouveau bouton "verre".
- **`rules_esphome.md` §1 impose une architecture "7-YAML"** (`hardware, sensors, api-logic, styles, images, globals, main`) — le fichier `tab5-images.yaml` existe encore (4 lignes) mais le README confirme qu'il est vide de facto (« Plus d'images! image: !include supprimé au profit des polices C++ vectorielles LVGL », `tab5-ha-hmi.yaml:136`). Fichier fantôme gardé pour coller au nombre "7" historique — à assumer explicitement ou supprimer.

### 4.4 Fichiers volumineux (candidats de découpe)

Le README revendique "chaque fichier reste sous ~600 lignes". Vérifié :

- **`tab5-sensors.yaml` (670 lignes) dépasse déjà ce seuil.** Mélange 5 domaines différents dans un seul fichier : `wifi:`, `switch:` (6 entités hardware + 1 template wake-word), `binary_sensor:` (2), `text_sensor:` (4, dont 3 quasi-identiques pour les 3 lumières), `sensor:` (11, dont 5 quasi-identiques pour les plantes via ancre YAML), `select:`, `time:`, deux blocs `interval:`. Candidat naturel de scission en `tab5-sensors-diagnostics.yaml` (RAM/uptime/wifi) + `tab5-sensors-domotique.yaml` (lumières/plantes/temp) si le refacto #T164 se poursuit.
- **`tab5_custom.cpp` (520 lignes)** commence à mélanger plusieurs responsabilités indépendantes (parsing bulk, rendu météo, tri plantes, animations, cartes lumière) dans un seul fichier sans découpage en unités de compilation — pas bloquant à ce stade mais à surveiller si le fichier continue de grossir.
- **`climate_popup.yaml` (234 lignes) reste le plus gros `ui_component` non factorisable** selon la décision documentée dans `etat_tab5.md` — chaque bouton a un service HA + logique lambda propres. C'est un choix assumé et argumenté, pas un oubli, mais reste le point chaud si un refacto plus profond (dispatch C++ par index évoqué dans "Prochaine étape" de `etat_tab5.md`) est un jour entrepris.

### 4.5 Risques déjà identifiés mais volontairement non corrigés (traçabilité, pas nouveauté)

Pour éviter de re-signaler du bruit déjà tranché par Axel (cf. `etat_tab5.md`) :
- `atoi`/`atof` sans validation (~13 sites) : réel, sans symptôme rapporté, laissé tel quel sur décision explicite (P3, sweep disproportionné).
- Garde-fou `logger: level: INFO` + délai bloquant 1s à `on_boot:priority:700` : ce n'est pas de la dette, c'est un vrai correctif vérifié expérimentalement 3/3 — à ne pas "nettoyer" sans retester à fond (le README et `tab5-hardware.yaml` le disent explicitement en gros commentaire).
- Non-wrap intentionnel de la pagination prévisions (0↔4) : déjà "corrigé à tort" une fois suite à un faux positif d'audit LLM, puis reverté — ne pas re-signaler comme bug.

---

## 5. Ce que la cartographie ne couvre pas

- Le contenu réel de `HomeAssistant_Config/automations_tab5.yaml` (gitignoré, config privée d'Axel) — seule sa description dans `HomeAssistant_Config/README.md` a pu être vérifiée, pas le payload exact envoyé aujourd'hui.
- Les rapports d'audit LLM existants dans `docs/` (`analyse_esphome_auto.md`, `audit_rapport_final.md`, etc.) n'ont pas été relus en détail ici — `etat_tab5.md` indique qu'ils ont déjà été synthétisés en tâches (#T161-#T169) le 05/07/2026, dont plusieurs traitées depuis. Les recopier ici aurait dupliqué un travail déjà fait.
