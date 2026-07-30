# Installation & Configuration

## English · [Français](#version-française)

---

> **Just want to try it first?** [`docs/demo_mode.md`](demo_mode.md) shows the full dashboard on a flashed device in a few minutes, with no Home Assistant install at all. Come back here when you're ready for the real install.

## Prerequisites

- A working **Home Assistant** instance (any installation method)
- The **ESPHome** add-on or standalone ESPHome CLI (`pip install esphome`)
- ESPHome version **≥ 2026.7.0** — enforced by `min_version:` in `tab5-ha-hmi.yaml`, so an older ESPHome refuses to compile. This release is what brings the official `st7123` touchscreen platform (no more `external_components`), zero-copy audio, VAD, and PSRAM-over-SDIO
- A M5Stack Tab5 V2 (ESP32-P4 variant)

Optional but used by the default configuration:
- **Météo-France** integration (for weather data — replace with your own weather integration if outside France)
- **Google Calendar** integration (for the planning screen)
- A configured **Home Assistant Voice pipeline** (for voice assistant features)

---

## Step 1 — Clone and locate the entry point

```bash
git clone https://github.com/Axellum/M5-Tab5-ESPHome-LVGL.git
cd M5-Tab5-ESPHome-LVGL
```

The main file is `tab5-ha-hmi.yaml` at the repository root. All other YAML files in `Tab5/` are included as packages by this entry point.

---

## Step 2 — Create your entity substitutions file

Copy the example file and edit it with your Home Assistant entity IDs:

```bash
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
```

Open `Tab5/user_entities.yaml` (gitignored — never committed, same pattern as `secrets.yaml`):

```yaml
entity_tracker_pc: device_tracker.your_pc
entity_phone_battery: sensor.your_phone_battery

# --- Lights ---
entity_light_chambre: light.your_bedroom_light
entity_light_salon: light.your_living_room_light
entity_light_bureau: light.your_office_light

# --- Climate ---
entity_climate_salon: climate.your_living_room_climate

# --- Voice assistant ---
entity_tab5_pipeline_select: select.your_tab5_assistant_pipeline

# --- Temperature & Humidity ---
entity_temp_salon: sensor.your_living_room_temperature
entity_hum_salon: sensor.your_living_room_humidity
...
```

Replace each value with your own entity IDs. These substitutions propagate throughout all packages — you do not need to edit any other YAML file to adapt the project to your setup. The entry point `tab5-ha-hmi.yaml` includes this file via `substitutions: !include Tab5/user_entities.yaml`.

---

## Step 3 — Create your secrets file

Create a `secrets.yaml` file at the repository root (already in `.gitignore`):

```yaml
wifi_ssid: "YOUR_WIFI_NETWORK"
wifi_password: "YOUR_WIFI_PASSWORD"
api_encryption_key: "BASE64_32_BYTES_KEY"
ota_password: "YOUR_OTA_PASSWORD"
```

To generate a valid `api_encryption_key`:

```bash
python3 -c "import secrets, base64; print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

---

## Step 4 — Set up Home Assistant automations

Copy the files from `HomeAssistant_Config/` into your Home Assistant configuration:

| File | Where to add it |
|------|-----------------|
| `automations_examples.yaml.example` | Merge into your `automations.yaml` (or include it from your `automations:` section) |
| `scripts_examples.yaml` | Include in your `scripts:` section |
| `template_sensors_examples.yaml` | Include in your `template:` section in `configuration.yaml` |
| `packages/*.yaml` | Self-contained HA *packages* — enable `homeassistant: packages: !include_dir_named packages` first |

Then search-and-replace the placeholder entity names (`VOTRE_VILLE`, `VOTRE_CLIMATISATION`…) in those files. See [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) for the full list.

> The `*_examples*` files are the versioned, genuinely shipped ones. The author's own production files (`automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`) are gitignored and **not** part of a clone — if a doc mentions them, it is talking about the private originals these examples are generated from.

---

## Step 5 — First flash (USB)

Connect the Tab5 to your computer via USB-C. Then:

```bash
# Via CLI
esphome run tab5-ha-hmi.yaml

# Or via ESPHome Dashboard
# Add the device, point it at tab5-ha-hmi.yaml, click Install
```

The first flash must be done over USB. After that, all updates can be done via OTA over Wi-Fi (the device will appear in your ESPHome dashboard once it connects).

---

## OTA updates

After the initial flash, the device registers with ESPHome's OTA server. Subsequent compilations can be pushed wirelessly:

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

Or just click **Install → Wirelessly** in the ESPHome dashboard.

---

## Météo-France specifics

The weather screen is built around Météo-France's data structure. If you are in France:

1. Install the **Météo-France** integration from the HA integrations page
2. You will get entities: `weather.your_city`, `sensor.your_city_next_rain`, `sensor.XX_weather_alert`
3. The automation in `automations_examples.yaml.example` queries `v1/vision/rain` and `v1/forecast` from Météo-France's API and formats the response into the semicolon-delimited payload the device expects

If you are outside France, the weather screen requires adaptation. The push automation will need to be rewritten to query your local weather integration and produce the same payload format. The payload format is documented in the automation file comments.

---

---

## Version Française

---

> **Envie de tester d'abord ?** [`docs/demo_mode.md`](demo_mode.md) montre le tableau de bord complet sur un appareil flashé en quelques minutes, sans aucune installation Home Assistant. Revenez ici quand vous êtes prêt pour l'installation réelle.

## Prérequis

- Une instance **Home Assistant** fonctionnelle (toute méthode d'installation)
- L'add-on **ESPHome** ou la CLI ESPHome standalone (`pip install esphome`)
- ESPHome version **≥ 2026.7.0** — imposée par le `min_version:` de `tab5-ha-hmi.yaml` : une version antérieure refuse de compiler. C'est cette version qui apporte la plateforme tactile `st7123` officielle (plus besoin d'`external_components`), l'audio zero-copy, le VAD et la PSRAM via SDIO
- Un M5Stack Tab5 V2 (variante ESP32-P4)

Optionnel mais utilisé par la configuration par défaut :
- Intégration **Météo-France** (pour les données météo — remplacez par votre propre intégration si vous êtes hors de France)
- Intégration **Google Calendar** (pour l'écran planning)
- Un **pipeline Voice Home Assistant** configuré (pour les fonctions assistant vocal)

---

## Étape 1 — Cloner et localiser le point d'entrée

```bash
git clone https://github.com/Axellum/M5-Tab5-ESPHome-LVGL.git
cd M5-Tab5-ESPHome-LVGL
```

Le fichier principal est `tab5-ha-hmi.yaml` à la racine du dépôt. Tous les autres fichiers YAML dans `Tab5/` sont inclus comme packages par ce point d'entrée.

---

## Étape 2 — Créer votre fichier d'entités HA

Copiez le modèle puis adaptez-le à vos entity IDs Home Assistant :

```bash
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
```

Ouvrez `Tab5/user_entities.yaml` (gitignoré — ne jamais committer, même principe que `secrets.yaml`) et remplacez chaque valeur. Ces substitutions se propagent dans tous les packages ; le point d'entrée `tab5-ha-hmi.yaml` les charge via `substitutions: !include Tab5/user_entities.yaml`.

---

## Étape 3 — Créer votre fichier secrets

Créez un fichier `secrets.yaml` à la racine du dépôt (déjà dans `.gitignore`) :

```yaml
wifi_ssid: "VOTRE_RESEAU_WIFI"
wifi_password: "VOTRE_MOT_DE_PASSE"
api_encryption_key: "CLE_BASE64_32_OCTETS"
ota_password: "VOTRE_MOT_DE_PASSE_OTA"
```

Pour générer une `api_encryption_key` valide :

```bash
python3 -c "import secrets, base64; print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

---

## Étape 4 — Configurer les automations Home Assistant

Copiez les fichiers de `HomeAssistant_Config/` dans votre configuration Home Assistant :

| Fichier | Où l'ajouter |
|---------|-------------|
| `automations_examples.yaml.example` | Fusionner avec votre `automations.yaml` (ou l'inclure depuis votre section `automations:`) |
| `scripts_examples.yaml` | Inclure dans votre section `scripts:` |
| `template_sensors_examples.yaml` | Inclure dans votre section `template:` dans `configuration.yaml` |
| `packages/*.yaml` | *Packages* HA autonomes — activez d'abord `homeassistant: packages: !include_dir_named packages` |

Puis recherchez-remplacez les noms d'entités placeholder (`VOTRE_VILLE`, `VOTRE_CLIMATISATION`…) dans ces fichiers. Voir [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) pour la liste complète.

> Les fichiers `*_examples*` sont les seuls réellement versionnés et livrés. Les fichiers de production de l'auteur (`automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`) sont gitignorés et **absents** d'un clone — quand une doc les mentionne, elle parle des originaux privés dont ces exemples sont dérivés.

---

## Étape 5 — Premier flash (USB)

Connectez le Tab5 à votre ordinateur via USB-C. Ensuite :

```bash
# Via CLI
esphome run tab5-ha-hmi.yaml

# Ou via le Dashboard ESPHome
# Ajoutez l'appareil, pointez-le vers tab5-ha-hmi.yaml, cliquez Installer
```

Le premier flash doit se faire en USB. Ensuite, toutes les mises à jour peuvent se faire en OTA via Wi-Fi.

---

## Mises à jour OTA

Après le flash initial, l'appareil s'enregistre auprès du serveur OTA d'ESPHome. Les compilations suivantes peuvent être poussées sans fil :

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

Ou cliquez simplement **Installer → Sans fil** dans le dashboard ESPHome.

---

## Spécificités Météo-France

L'écran météo est construit autour de la structure de données de Météo-France. Si vous êtes en France :

1. Installez l'intégration **Météo-France** depuis la page des intégrations HA
2. Vous obtiendrez des entités : `weather.votre_ville`, `sensor.votre_ville_next_rain`, `sensor.XX_alerte_meteo`
3. L'automatisation dans `automations_examples.yaml.example` interroge `v1/vision/rain` et `v1/forecast` de l'API Météo-France et formate la réponse en payload délimité par des points-virgules attendu par l'appareil

Si vous êtes hors de France, l'écran météo nécessite une adaptation. L'automatisation push devra être réécrite pour interroger votre intégration météo locale et produire le même format de payload. Le format du payload est documenté dans les commentaires du fichier d'automatisation.
