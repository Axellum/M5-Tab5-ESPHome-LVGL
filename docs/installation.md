# Installation & Configuration

## English · [Français](#version-française)

---

> **Just want to try it first?** [`docs/demo_mode.md`](demo_mode.md) shows the full dashboard on a flashed device in a few minutes, with no Home Assistant install at all. Come back here when you're ready for the real install.

## Prerequisites

- A working **Home Assistant** instance (any installation method)
- The **ESPHome** add-on or standalone ESPHome CLI (`pip install esphome`)
- ESPHome version **≥ 2026.9.0** — enforced by `min_version:` in `tab5-ha-hmi.yaml`, so an older ESPHome refuses to compile. 2026.7.0 brought the official `st7123` touchscreen platform (no more `external_components`), zero-copy audio, VAD and PSRAM-over-SDIO; the floor was raised to 2026.8.1 on 2026-08-26 for the API, voice-assistant and crash-handler fixes this project exercises daily, then to 2026.9.0 on 2026-09-16 because OTA updates are encrypted with the API key (`ota: encryption:` does not exist in older releases — reasoning in the comment above `min_version:`)
- A M5Stack Tab5. The **ST7123** display chip (sticker on the back) is the one tested daily; the ST7121 and the original ILI9881C revisions compile but have never been run on a device — see [Hardware revisions](hardware.md#hardware-revisions), and Step 2 to pick yours

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

**Tab5 revision:** if the display chip on your sticker is not the ST7123, add `tab5_ecran: st7121` or `tab5_ecran: ili9881c` to this file (see [Hardware revisions](hardware.md#hardware-revisions)). Leave it out for the ST7123.

Replace each value with your own entity IDs. These substitutions propagate throughout all packages — you do not need to edit any other YAML file to adapt the project to your setup. The entry point `tab5-ha-hmi.yaml` includes this file via `substitutions: !include Tab5/user_entities.yaml`. Two optional keys, `entity_tab5_satellite` and `entity_tab5_media_player`, only matter if you rename the device in Home Assistant: they hold the entity IDs HA derives from the device name (defaults in `Tab5/tab5-scripts.yaml`, commented example in the template).

---

## Step 3 — Create your secrets file

Create a `secrets.yaml` file at the repository root (already in `.gitignore`):

```yaml
wifi_ssid: "YOUR_WIFI_NETWORK"
wifi_password: "YOUR_WIFI_PASSWORD"
wifi_ap_password: "FALLBACK_AP_PASSWORD"   # recovery access point when Wi-Fi is unreachable
api_encryption_key: "BASE64_32_BYTES_KEY"
```

There is no separate OTA password: since ESPHome 2026.9.0 the firmware encrypts OTA updates with `api_encryption_key` (`ota: encryption:` in `Tab5/tab5-hardware.yaml`), so this one key authenticates both Home Assistant and the uploader. The CLI reads it from `secrets.yaml`; a plaintext upload is refused.

To generate a valid `api_encryption_key`:

```bash
python3 -c "import secrets, base64; print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

---

## Step 4 — Set up the Home Assistant packages

Everything on the Home Assistant side is a **package** in `HomeAssistant_Config/packages/`:

1. Enable packages in `configuration.yaml`: `homeassistant: packages: !include_dir_named packages`.
2. Copy `HomeAssistant_Config/placeholders.example.yaml` to `placeholders.yaml` (gitignored) and fill in your real entity IDs (`VOTRE_VILLE`, `VOTRE_CLIMATISATION`, `VOTRE_PC`…).
3. Render: `python tools/render_ha_config.py` writes the deployable copies to `HomeAssistant_Config/rendered/`.
4. Copy `rendered/packages/*.yaml` into your HA `config/packages/`, and `rendered/custom_templates/` into `config/custom_templates/`.
5. Reload Automations, Scripts, Template entities, Input booleans and Input texts (or restart HA).

Start with `packages/tab5_push.yaml` (push automations, shared scripts, the scripts the Tab5 calls, rain sensor) — the others add optional features. See [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) for what each package does and the full placeholder list.

> These packages are exactly what runs on the author's Home Assistant (rendered with the author's own values) since 2026-09-26. There are no private versions and nothing to merge into `automations.yaml` or `scripts.yaml`.

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
3. The full push automation in `packages/tab5_push.yaml` queries `v1/vision/rain` and `v1/forecast` from Météo-France's API and formats the response into the semicolon-delimited payload the device expects

If you are outside France, the weather screen requires adaptation. The push automation will need to be rewritten to query your local weather integration and produce the same payload format. The payload format is documented in the package comments.

---

---

## Version Française

---

> **Envie de tester d'abord ?** [`docs/demo_mode.md`](demo_mode.md) montre le tableau de bord complet sur un appareil flashé en quelques minutes, sans aucune installation Home Assistant. Revenez ici quand vous êtes prêt pour l'installation réelle.

## Prérequis

- Une instance **Home Assistant** fonctionnelle (toute méthode d'installation)
- L'add-on **ESPHome** ou la CLI ESPHome standalone (`pip install esphome`)
- ESPHome version **≥ 2026.9.0** — imposée par le `min_version:` de `tab5-ha-hmi.yaml` : une version antérieure refuse de compiler. La 2026.7.0 a apporté la plateforme tactile `st7123` officielle (plus besoin d'`external_components`), l'audio zero-copy, le VAD et la PSRAM via SDIO ; le plancher est passé à 2026.8.1 le 26/08/2026 pour les correctifs API, assistant vocal et handler de crash que ce projet exerce tous les jours, puis à 2026.9.0 le 16/09/2026 parce que les mises à jour OTA sont chiffrées avec la clé API (`ota: encryption:` n'existe pas dans les versions antérieures — raisons dans le commentaire au-dessus de `min_version:`)
- Un M5Stack Tab5. La puce écran **ST7123** (autocollant au dos) est celle testée tous les jours ; les révisions ST7121 et ILI9881C d'origine compilent mais n'ont jamais tourné sur une tablette — voir [Révisions matérielles](hardware.md#révisions-matérielles), et l'étape 2 pour choisir la vôtre

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

**Révision du Tab5 :** si la puce écran de votre autocollant n'est pas la ST7123, ajoutez `tab5_ecran: st7121` ou `tab5_ecran: ili9881c` dans ce fichier (voir [Révisions matérielles](hardware.md#révisions-matérielles)). Pour la ST7123, ne mettez rien.

Ouvrez `Tab5/user_entities.yaml` (gitignoré — ne jamais committer, même principe que `secrets.yaml`) et remplacez chaque valeur. Ces substitutions se propagent dans tous les packages ; le point d'entrée `tab5-ha-hmi.yaml` les charge via `substitutions: !include Tab5/user_entities.yaml`. Deux clés facultatives, `entity_tab5_satellite` et `entity_tab5_media_player`, ne servent que si vous renommez l'appareil dans Home Assistant : elles portent les identifiants qu'HA dérive du nom de la tablette (défauts dans `Tab5/tab5-scripts.yaml`, exemple commenté dans le modèle).

---

## Étape 3 — Créer votre fichier secrets

Créez un fichier `secrets.yaml` à la racine du dépôt (déjà dans `.gitignore`) :

```yaml
wifi_ssid: "VOTRE_RESEAU_WIFI"
wifi_password: "VOTRE_MOT_DE_PASSE"
wifi_ap_password: "MOT_DE_PASSE_AP_SECOURS"   # point d'accès de secours si le Wi-Fi est injoignable
api_encryption_key: "CLE_BASE64_32_OCTETS"
```

Pas de mot de passe OTA séparé : depuis ESPHome 2026.9.0 le firmware chiffre les mises à jour OTA avec `api_encryption_key` (`ota: encryption:` dans `Tab5/tab5-hardware.yaml`), cette seule clé authentifie donc Home Assistant et l'outil qui flashe. Le CLI la lit dans `secrets.yaml` ; un envoi en clair est refusé.

Pour générer une `api_encryption_key` valide :

```bash
python3 -c "import secrets, base64; print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

---

## Étape 4 — Installer les packages Home Assistant

Tout le côté Home Assistant est en **packages**, dans `HomeAssistant_Config/packages/` :

1. Activez les packages dans `configuration.yaml` : `homeassistant: packages: !include_dir_named packages`.
2. Copiez `HomeAssistant_Config/placeholders.example.yaml` vers `placeholders.yaml` (gitignoré) et renseignez vos vrais entity IDs (`VOTRE_VILLE`, `VOTRE_CLIMATISATION`, `VOTRE_PC`…).
3. Rendez : `python tools/render_ha_config.py` écrit les copies déployables dans `HomeAssistant_Config/rendered/`.
4. Copiez `rendered/packages/*.yaml` dans le `config/packages/` de HA, et `rendered/custom_templates/` dans `config/custom_templates/`.
5. Rechargez Automatisations, Scripts, Entités de template, Entrées booléennes et Entrées de texte (ou redémarrez HA).

Commencez par `packages/tab5_push.yaml` (automatisations de poussée, scripts partagés, scripts appelés par le Tab5, capteur de pluie) ; les autres ajoutent des fonctions optionnelles. Voir [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) pour le rôle de chaque package et la liste complète des placeholders.

> Ces packages sont exactement ce qui tourne sur le Home Assistant de l'auteur (rendus avec ses valeurs) depuis le 26/09/2026. Il n'y a pas de version privée, ni rien à fusionner dans `automations.yaml` ou `scripts.yaml`.

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
3. L'automatisation de poussée complète de `packages/tab5_push.yaml` interroge `v1/vision/rain` et `v1/forecast` de l'API Météo-France et formate la réponse en payload délimité par des points-virgules attendu par l'appareil

Si vous êtes hors de France, l'écran météo nécessite une adaptation. L'automatisation push devra être réécrite pour interroger votre intégration météo locale et produire le même format de payload. Le format du payload est documenté dans les commentaires du package.
