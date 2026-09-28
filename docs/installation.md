# Installation & Configuration

## English · [Français](#version-française)

---

> **Just want to try it first?** [`docs/demo_mode.md`](demo_mode.md) shows the full dashboard on a flashed device in a few minutes, with no Home Assistant install at all. Come back here when you're ready for the real install.

## Without compiling (install page)

Since 3.0, a ready-made, signed firmware installs from the browser. In this order:

1. **Home Assistant side first**: the packages and the blueprint of [Step 4](#step-4--set-up-the-home-assistant-packages). For now this step still needs the repository and Python, once, to fill in your city, calendar and so on.
2. **Flash** from the [install page](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) (Chrome or Edge, a USB-C cable that carries data): your display revision, the Stable channel, *Connect and install*. On a new tablet, accept to erase it. In the port list, the tablet is « USB JTAG/serial debug unit »; if several ports have that name, unplug the tablet to see which one disappears.
   - « Failed to initialize… holding the BOOT button »: the Tab5 has no BOOT button. Hold its reset button about 2 s, until the internal green LED blinks fast (download mode), start again, and press reset once at the end to restart it.
   - The tablet already runs the same version: the page shows no *Install*. To start from scratch, « Erase User Data » (in red, at the bottom) erases everything, Wi-Fi, key and settings included, then installs again.
3. **Wi-Fi**: from the same window (*Connect to Wi-Fi*, over USB), or with a phone on the open « Tab5 Fallback AP » network.
4. **Add it to Home Assistant** within 30 minutes of its start: *Settings → Devices & services*, the ESPHome device is discovered, *Configure*. Home Assistant gives it its key. A tablet Home Assistant already knows gets a new key by itself, nothing to confirm (checked on 2026-09-28).
5. **Allow Home Assistant actions**: *ESPHome → Configure*, tick « Allow the device to perform Home Assistant actions ». Voice, calendar and alarm clock need them.
6. **Your devices**: create the automation from the blueprint ([Step 4](#step-4--set-up-the-home-assistant-packages), item 6).

Updates then show up in Home Assistant (« Firmware » entity), on the channel you installed; to switch channels, install again from the page without erasing. Over the air, the tablet only accepts a firmware signed with the project key: to switch to your own builds (your own key), flash once over USB.

The steps below are for building your own firmware; Steps 4 and 6 are for everyone.

## Prerequisites

- A working **Home Assistant** instance, **2026.8 or newer** (it gives the tablet its encryption key, see Step 6), any installation method
- To build your own firmware only: the **ESPHome** add-on or standalone ESPHome CLI (`pip install esphome`)
- ESPHome version **≥ 2026.9.0** — enforced by `min_version:` in `tab5-ha-hmi.yaml`, so an older ESPHome refuses to compile. 2026.7.0 brought the official `st7123` touchscreen platform (no more `external_components`), zero-copy audio, VAD and PSRAM-over-SDIO; the floor was raised to 2026.8.1 on 2026-08-26 for the API, voice-assistant and crash-handler fixes this project exercises daily, then to 2026.9.0 on 2026-09-16; the pairing window, the key given by Home Assistant and signed firmware (3.0) are checked on it (reasoning in the comment above `min_version:`)
- A M5Stack Tab5. The **ST7123** display chip (sticker on the back) is the one tested daily; the ST7121 and the original ILI9881C revisions compile but have never been run on a device — see [Hardware revisions](hardware.md#hardware-revisions), and Step 2 to pick yours

Optional but used by the default configuration:
- A **weather integration** with hourly and daily forecasts (Météo-France, Met.no, OpenWeatherMap…); see [Weather providers](#weather-providers)
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

## Step 2 — Create your build settings file

Copy the example file:

```bash
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
```

`Tab5/user_entities.yaml` is gitignored (never committed). For a standard install there is nothing to replace in it: every line is optional.

**Screen language:** French by default; add `tab5_langue: English` (or `Deutsch`, `Nederlands`) for another language on the first boot. It can then be changed from Home Assistant (select « Langue »), see [translations](translations.md).

**Time zone:** nothing to set since 3.0. The tablet takes Home Assistant's time zone and keeps the last one it received, so the alarm clock stays right when HA is down after a power cut. An old `tab5_fuseau` line is ignored.

**Tab5 revision:** if the display chip on your sticker is not the ST7123, add `tab5_ecran: st7121` or `tab5_ecran: ili9881c` to this file (see [Hardware revisions](hardware.md#hardware-revisions)). Leave it out for the ST7123.

**Your devices (lights, climate, plants, TV…) are not set here any more (since 3.0)**: you pick them in Home Assistant with the mouse, see Step 4 and [Adapt to your home](#adapt-to-your-home); old `entity_light_…` keys in an existing file are simply ignored. The entry point `tab5-ha-hmi.yaml` includes this file via `substitutions: !include Tab5/user_entities.yaml`. The remaining entity keys are commented out in the template, with their defaults in `Tab5/tab5-scripts.yaml`:
- `entity_tab5_satellite`, `entity_tab5_media_player` and `entity_tab5_pipeline_select` (Domotique / Discussion buttons) hold the entity IDs Home Assistant derives from the device name: set them only if you rename the tablet in HA;
- `entity_primary_active` and `entity_push_automation` (« MAJ Écran » button of the system console) are the names created by `packages/tab5_push.yaml`: set them only if you changed that package.

---

## Step 3 — Create your firmware signing key

Since 3.0 the firmware holds **no secret**: no Wi-Fi password, no API key ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)). What protects the tablet is a signature: over the network, it only accepts a firmware signed with the same key as the one it runs. Create this private key once, at the repository root:

```bash
python -m espsecure generate-signing-key --version 2 --scheme rsa3072 tab5_signature.pem
```

`espsecure` comes with ESPHome. The file is gitignored; to keep it elsewhere, set its path in `Tab5/user_entities.yaml` (`tab5_cle_signature: …`). **Keep a copy outside your computer**: without it, the tablet can only be updated over USB.

No `secrets.yaml` any more: one left from 2.x is simply not read (see [Upgrading from 2.x](#upgrading-from-2x)).

---

## Step 4 — Set up the Home Assistant packages

Everything on the Home Assistant side is a **package** in `HomeAssistant_Config/packages/`:

1. Enable packages in `configuration.yaml`: `homeassistant: packages: !include_dir_named packages`.
2. Copy `HomeAssistant_Config/placeholders.example.yaml` to `placeholders.yaml` (gitignored) and fill in your real entity IDs (`VOTRE_VILLE`, `VOTRE_DEPARTEMENT`, `VOTRE_EMAIL_gmail_com`…).
3. Render: `python tools/render_ha_config.py` writes the deployable copies to `HomeAssistant_Config/rendered/`.
4. Copy `rendered/packages/*.yaml` into your HA `config/packages/`, and `rendered/custom_templates/` into `config/custom_templates/`.
5. Reload Automations, Scripts, Template entities, Input booleans and Input texts (or restart HA).
6. **Choose your devices**: *Settings → Automations & scenes → Blueprints → Import blueprint*, paste
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`,
   then *Create automation* and pick an entity for each slot (all optional). Or copy the file into
   `config/blueprints/automation/tab5/`. One automation per tablet.

Start with `packages/tab5_push.yaml` (push automations, shared scripts, the scripts the Tab5 calls, the optional-zones answer) and `packages/tab5_meteo_sources.yaml` (weather, rain and warning sources, required since 2.2.0) — the others add optional features. See [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) for what each package does and the full placeholder list.

> These packages are exactly what runs on the author's Home Assistant (rendered with the author's own values) since 2026-09-26. There are no private versions and nothing to merge into `automations.yaml` or `scripts.yaml`.

---

## Step 5 — First flash (USB) and Wi-Fi

Connect the Tab5 to your computer via USB-C. Then:

```bash
esphome run tab5-ha-hmi.yaml
```

The tablet has no Wi-Fi network yet. Give it yours, either way:
- **over USB**, right after the flash: the project's [web flasher](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) (Chrome or Edge), *Connect and install*, then its Wi-Fi button, or [ESPHome Web](https://web.esphome.io), *Connect*, then *Configure Wi-Fi*. Both use Improv and work with a firmware you built yourself. Closing that window restarts the tablet once: that is normal;
- **without a cable**: join the open **« Tab5 Fallback AP »** network with a phone; a page opens (otherwise go to `http://192.168.4.1`) to pick your network. <!-- pragma: allowlist secret -->

The network is kept across updates. The fallback AP comes back whenever the tablet loses its Wi-Fi for a minute, to set a new one.

---

## Step 6 — Add the tablet to Home Assistant

**Within 30 minutes of the tablet's start** (its pairing window): *Settings → Devices & services*, the tablet shows up as discovered (ESPHome). *Configure*, then *Submit*. Home Assistant creates the encryption key, gives it to the tablet and keeps it: nothing to copy.

- Window missed? Restart the tablet: it reopens for 30 minutes. Once it has its key, the window never opens again.
- Then, in the device's options (*ESPHome → Configure*), tick **« Allow the device to perform Home Assistant actions »**: voice, calendar and alarm clock use them.

---

## OTA updates

Once the tablet is on your network, later builds go over Wi-Fi:

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

The transfer is not encrypted any more (there is no key in the YAML); the tablet checks the signature and refuses a firmware signed by another key. Your builds are signed by your key at compile time, nothing else to do.

A tablet installed from the [web flasher](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) gets its updates from Home Assistant instead: its « Firmware » entity reads the published manifest every 6 hours, and « Install » downloads the image, which the tablet checks against the project key ([ADR-0022](decisions/0022-published-firmware-pages-channels.md)). A firmware you compile yourself has no such entity.

**Logs:** `esphome logs` looks for the key in the YAML and no longer finds one. Use `python tools/tab5_logs.py --host 192.168.x.x --config-ha \\<ha-ip>\config`: it reads the key Home Assistant keeps (`.storage/core.config_entries`, or the `TAB5_CLE_API` variable) and never prints it.

---

## Upgrading from 2.x

3.0 changes how the tablet is protected ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)). Once, in person (Steps 3 and 4 below must happen within 30 minutes of the tablet's start):

1. **Before flashing**, set up Home Assistant for 3.0: the blueprint of Step 4, item 6 (the tablet no longer knows your entities, [ADR-0019](decisions/0019-logical-slots-blueprint.md)).
2. **Signing key** (Step 3), then compile: `esphome compile tab5-ha-hmi.yaml`.
3. **Flash.** The 2.x firmware refuses a plain upload, so this one goes out encrypted with your old key (`api_encryption_key` of your `secrets.yaml`, never printed):
   ```bash
   python tools/migrer_vers_3.py --host 192.168.x.x --port COM3
   ```
   `--port` is the tablet's USB port (`COM…` on Windows, `/dev/ttyACM0` on Linux; `python -m serial.tools.list_ports -v` lists them, the tablet's serial number is its MAC address). Or over USB only: `esphome upload tab5-ha-hmi.yaml --device COM3`.
4. **Wi-Fi.** 2.x had the credentials compiled in, 3.0 does not. With `--port`, the script gives them back right after the restart, over USB (Improv, `wifi_ssid` and `wifi_password` of the same `secrets.yaml`, never printed): no phone, no access point. Without it, the tablet starts without network and opens « Tab5 Fallback AP »: join it and pick your network, or use the web flasher's Wi-Fi button over USB (Step 5).
5. **Home Assistant** notifies that the tablet « disabled transport encryption » (*Settings → Devices & services*, re-authentication): confirm. HA then gives it a new key by itself. The entities, automations and history stay the same.

Then `secrets.yaml` can go (keep the old key only if you may flash 2.x again), and `tab5_fuseau` is ignored.

---

## Weather providers

The screen does not depend on one weather service (since lot 4c, 2026-09-27).

**Forecasts and current weather** (hourly and daily pages, the humidity drop) come from any `weather.*` entity, picked in Home Assistant from the select « Tab5 · source des prévisions », which lists the weather entities you have (`VOTRE_VILLE` is only the default). The Tab5 asks each entity only for what it declares: daily forecasts, or else twice-daily ones (NWS) or hourly ones (free OpenWeatherMap) grouped by date; without hourly forecasts (Buienradar), the hourly page stays empty. Hours are shown in local time.

**Rain in the next hour and weather warnings** are optional. Their source is chosen **in Home Assistant**, with the two selects of `packages/tab5_meteo_sources.yaml`, without editing YAML:

| Card | Select « Tab5 · source … » | What it needs |
|---|---|---|
| Rain in the next hour (bars and sentence) | Météo-France | the Météo-France integration (France): `sensor.<city>_next_rain` |
| | OpenWeatherMap | the OpenWeatherMap integration in **v3.0** mode, which needs a One Call subscription (1,000 calls a day free; HA polls every 10 min). Set `VOTRE_METEO_OWM` to its full entity id (`weather.openweathermap` by default) |
| | Aucune (none) | the rain card is hidden |
| Weather warnings | Météo-France | `sensor.<department>_weather_alert` (set `VOTRE_DEPARTEMENT`) |
| | MeteoAlarm | the MeteoAlarm integration (YAML only, 39 European countries, one alert at a time). Set `VOTRE_METEOALARM` to its full entity id (`binary_sensor.meteoalarm` by default) |
| | Aucune (none) | no warning icons |

Honest limits:
- OpenWeatherMap was tried on the author's installation on 2026-09-27 (forecasts and rain in the next hour, on a dry day); its daily forecast covers 8 days, so the last days of the 15-day pages stay empty. MeteoAlarm and the twice-daily grouping (NWS) were tested with simulated data only.
- Frost probability exists only at Météo-France. The snowflake icon reads `sensor.<city>_snow_chance` when it exists; otherwise it follows the current condition (snowy).
- Other warning sources (DWD, Environment Canada, NWS Alerts…) are not wired yet.

---

## Adapt to your home

The screen was drawn around the author's home: three lights, a climate unit, a greenhouse shutter, a TV, five plant sensors. **What you don't have disappears**, with its buttons ([ADR-0018](decisions/0018-optional-zones-confirmed-by-ha.md), [ADR-0019](decisions/0019-logical-slots-blueprint.md)).

- **Your devices are chosen in Home Assistant**, in the « Tab5 — emplacements » automation (the blueprint of Step 4). Changing one is an edit in HA's UI: no flash, no restart.
- **Remove a zone: leave its slot empty.** An empty slot, or an entity that doesn't exist, is absent. An entity that exists but is `unavailable` keeps its zone (« -- », « Hors ligne »). **Without the blueprint's automation, nothing disappears** (and nothing of your devices is shown).
- **A zone missing by mistake?** The tablet's diagnostic sensor « Zones masquées » lists what disappeared.
- A zone comes back by itself as soon as its entity sends a value.

| Zone | Blueprint input | Hidden when empty |
|---|---|---|
| Lights (up to 3) | Lumière 1 to 3 | Icons of tiles 3 to 5, card of the « HA » layer, light-popup selector, « Tout éteindre » |
| PC | PC (a switch turns it on; a presence tracker only shows it) | Status icon, « PC Bureau » card; the first tile too if there is no TV either |
| TV | TV, and Télécommande de la TV for the remote keys | TV button and remote; « HA » and « Sys » move one column right |
| Phone | Batterie du téléphone | Status icon |
| Room | Température de la pièce (and Humidité de la pièce) | Its temperature |
| Greenhouse | Seconde température (serre) | Its temperature; the icon becomes a gamepad, the arcade entrance stays |
| Plants (0 to 5) | Pot 1 to 5: the moisture sensor; conductivity, light, temperature and battery are taken from the same device | Up to 4 plants: one slot each; 5: the « driest / median / wettest » summary. Popup cards, re-centred |
| Climate | Climatisation | − / setpoint / + and the popup |
| Shutter | Volet (and the `volet_serre_tracking.yaml` package for a shutter that doesn't report its travel) | Icons of tile 2, card of the « HA » layer |
| Work planning | Agenda de travail | Planning panel of the central card |

The planning hours themselves still come from the `tab5_push.yaml` package (`VOTRE_EMAIL_gmail_com`): pick the same calendar in both.

Limits:
- **More than 3 lights, or another device on a tile**: not yet. The tiles are 5 fixed places (PC/TV, shutter, three lights).
- **Calendars written in the packages**: `calendar.famille`, `calendar.anniversaires` and the French public-holiday calendar (`tab5_reveil.yaml`, `tab5_calendar.yaml`) are to be edited by hand, see [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md).
- To see a smaller home without touching yours: [demo mode](demo_mode.md#minimal-home-optional-zones), option `--maison-minimale`.

---

---

## Version Française

---

> **Envie de tester d'abord ?** [`docs/demo_mode.md`](demo_mode.md) montre le tableau de bord complet sur un appareil flashé en quelques minutes, sans aucune installation Home Assistant. Revenez ici quand vous êtes prêt pour l'installation réelle.

## Sans compiler (page d'installation)

Depuis la 3.0, un firmware prêt à l'emploi et signé s'installe depuis le navigateur. Dans cet ordre :

1. **Home Assistant d'abord** : les packages et le blueprint de l'[étape 4](#étape-4--installer-les-packages-home-assistant). Pour l'instant, cette étape demande encore le dépôt et Python, une fois, pour renseigner votre ville, votre agenda, etc.
2. **Flasher** depuis la [page d'installation](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) (Chrome ou Edge, un câble USB-C qui transmet les données) : votre révision d'écran, le canal Stable, *Connecter et installer*. Sur une tablette neuve, acceptez de l'effacer. Dans la liste des ports, la tablette s'appelle « USB JTAG/serial debug unit » ; si plusieurs ports portent ce nom, débranchez la tablette pour voir lequel disparaît.
   - « Failed to initialize… holding the BOOT button » : le Tab5 n'a pas de bouton BOOT. Maintenez son bouton reset environ 2 s, jusqu'à ce que la LED verte interne clignote vite (mode téléchargement), recommencez, puis un appui court sur reset à la fin pour la redémarrer.
   - La tablette a déjà la même version : la page n'affiche pas *Install*. Pour repartir de zéro, « Erase User Data » (en rouge, en bas) efface tout, Wi-Fi, clé et réglages compris, puis réinstalle.
3. **Wi-Fi** : depuis la même fenêtre (*Connect to Wi-Fi*, par l'USB), ou avec un téléphone sur le réseau ouvert « Tab5 Fallback AP ».
4. **L'ajouter à Home Assistant** dans les 30 minutes qui suivent son démarrage : *Paramètres → Appareils et services*, l'appareil ESPHome est découvert, *Configurer*. Home Assistant lui donne sa clé. Une tablette que Home Assistant connaît déjà reçoit une nouvelle clé toute seule, rien à confirmer (vérifié le 28/09/2026).
5. **Autoriser les actions Home Assistant** : *ESPHome → Configurer*, cochez l'option qui autorise l'appareil à effectuer des actions Home Assistant. La voix, le calendrier et le réveil en ont besoin.
6. **Vos appareils** : créez l'automatisation depuis le blueprint ([étape 4](#étape-4--installer-les-packages-home-assistant), point 6).

Les mises à jour arrivent ensuite dans Home Assistant (entité « Firmware »), sur le canal installé ; pour changer de canal, réinstallez depuis la page sans effacer. Par le réseau, la tablette n'accepte qu'un firmware signé par la clé du projet : pour passer à vos propres compilations (votre clé), flashez une fois par USB.

Les étapes suivantes servent à compiler son propre firmware ; les étapes 4 et 6 concernent tout le monde.

## Prérequis

- Une instance **Home Assistant** fonctionnelle, **2026.8 ou plus récente** (c'est elle qui donne sa clé de chiffrement à la tablette, voir l'étape 6), toute méthode d'installation
- Pour compiler son propre firmware seulement : l'add-on **ESPHome** ou la CLI ESPHome standalone (`pip install esphome`)
- ESPHome version **≥ 2026.9.0** — imposée par le `min_version:` de `tab5-ha-hmi.yaml` : une version antérieure refuse de compiler. La 2026.7.0 a apporté la plateforme tactile `st7123` officielle (plus besoin d'`external_components`), l'audio zero-copy, le VAD et la PSRAM via SDIO ; le plancher est passé à 2026.8.1 le 26/08/2026 pour les correctifs API, assistant vocal et handler de crash que ce projet exerce tous les jours, puis à 2026.9.0 le 16/09/2026 ; la fenêtre d'appairage, la clé donnée par Home Assistant et les firmwares signés (3.0) y sont vérifiés (raisons dans le commentaire au-dessus de `min_version:`)
- Un M5Stack Tab5. La puce écran **ST7123** (autocollant au dos) est celle testée tous les jours ; les révisions ST7121 et ILI9881C d'origine compilent mais n'ont jamais tourné sur une tablette — voir [Révisions matérielles](hardware.md#révisions-matérielles), et l'étape 2 pour choisir la vôtre

Optionnel mais utilisé par la configuration par défaut :
- Une **intégration météo** avec prévisions horaires et journalières (Météo-France, Met.no, OpenWeatherMap…) ; voir [Fournisseurs météo](#fournisseurs-météo)
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

## Étape 2 — Créer votre fichier de réglages de compilation

Copiez le modèle :

```bash
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
```

**Langue de l'écran :** le français par défaut ; ajoutez `tab5_langue: English` (ou `Deutsch`, `Nederlands`) pour une autre langue au premier démarrage. Elle se change ensuite depuis Home Assistant (select « Langue »), voir [traductions](translations.md#version-française).

**Fuseau horaire :** rien à régler depuis la 3.0. La tablette prend celui de Home Assistant et garde le dernier reçu : le réveil reste juste quand HA manque après une coupure de courant. Une ancienne ligne `tab5_fuseau` est ignorée.

**Révision du Tab5 :** si la puce écran de votre autocollant n'est pas la ST7123, ajoutez `tab5_ecran: st7121` ou `tab5_ecran: ili9881c` dans ce fichier (voir [Révisions matérielles](hardware.md#révisions-matérielles)). Pour la ST7123, ne mettez rien.

`Tab5/user_entities.yaml` est gitignoré (ne jamais le committer). Pour une installation standard, rien n'y est à remplacer : toutes les lignes sont facultatives. **Vos appareils (lumières, clim, plantes, TV…) ne se règlent plus ici (depuis la 3.0)** : vous les choisissez dans Home Assistant, à la souris, voir l'étape 4 et [Adapter à sa maison](#adapter-à-sa-maison) ; les anciennes clés `entity_light_…` d'un fichier existant sont simplement ignorées. Le point d'entrée `tab5-ha-hmi.yaml` les charge via `substitutions: !include Tab5/user_entities.yaml`. Les clés d'entités qui restent sont commentées dans le modèle, avec leurs défauts dans `Tab5/tab5-scripts.yaml` :
- `entity_tab5_satellite`, `entity_tab5_media_player` et `entity_tab5_pipeline_select` (boutons Domotique / Discussion) portent les identifiants qu'HA dérive du nom de la tablette : à régler seulement si vous la renommez dans HA ;
- `entity_primary_active` et `entity_push_automation` (bouton « MAJ Écran » de la console système) sont les noms que crée `packages/tab5_push.yaml` : à régler seulement si vous avez modifié ce package.

---

## Étape 3 — Créer votre clé de signature du firmware

Depuis la 3.0, le firmware ne contient **aucun secret** : ni mot de passe Wi-Fi, ni clé API ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)). Ce qui protège la tablette, c'est une signature : par le réseau, elle n'accepte qu'un firmware signé par la même clé que celui qu'elle fait tourner. Créez cette clé privée une fois, à la racine du dépôt :

```bash
python -m espsecure generate-signing-key --version 2 --scheme rsa3072 tab5_signature.pem
```

`espsecure` est installé avec ESPHome. Le fichier est gitignoré ; pour le ranger ailleurs, donnez son chemin dans `Tab5/user_entities.yaml` (`tab5_cle_signature: …`). **Gardez-en une copie hors de votre ordinateur** : sans elle, la tablette ne se met plus à jour que par USB.

Plus de `secrets.yaml` : celui d'une 2.x n'est simplement plus lu (voir [Passer à la 3.0](#passer-à-la-30)).

---

## Étape 4 — Installer les packages Home Assistant

Tout le côté Home Assistant est en **packages**, dans `HomeAssistant_Config/packages/` :

1. Activez les packages dans `configuration.yaml` : `homeassistant: packages: !include_dir_named packages`.
2. Copiez `HomeAssistant_Config/placeholders.example.yaml` vers `placeholders.yaml` (gitignoré) et renseignez vos vrais entity IDs (`VOTRE_VILLE`, `VOTRE_DEPARTEMENT`, `VOTRE_EMAIL_gmail_com`…).
3. Rendez : `python tools/render_ha_config.py` écrit les copies déployables dans `HomeAssistant_Config/rendered/`.
4. Copiez `rendered/packages/*.yaml` dans le `config/packages/` de HA, et `rendered/custom_templates/` dans `config/custom_templates/`.
5. Rechargez Automatisations, Scripts, Entités de template, Entrées booléennes et Entrées de texte (ou redémarrez HA).
6. **Choisissez vos appareils** : *Paramètres → Automatisations et scènes → Blueprints → Importer un blueprint*, collez
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`,
   puis *Créer une automatisation* et choisissez une entité pour chaque emplacement (tous facultatifs). Ou copiez
   le fichier dans `config/blueprints/automation/tab5/`. Une automatisation par tablette.

Commencez par `packages/tab5_push.yaml` (automatisations de poussée, scripts partagés, scripts appelés par le Tab5, réponse des zones optionnelles) et `packages/tab5_meteo_sources.yaml` (sources de la météo, de la pluie et des vigilances, obligatoire depuis la 2.2.0) ; les autres ajoutent des fonctions optionnelles. Voir [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) pour le rôle de chaque package et la liste complète des placeholders.

> Ces packages sont exactement ce qui tourne sur le Home Assistant de l'auteur (rendus avec ses valeurs) depuis le 26/09/2026. Il n'y a pas de version privée, ni rien à fusionner dans `automations.yaml` ou `scripts.yaml`.

---

## Étape 5 — Premier flash (USB) et Wi-Fi

Connectez le Tab5 à votre ordinateur via USB-C. Ensuite :

```bash
esphome run tab5-ha-hmi.yaml
```

La tablette n'a pas encore de réseau Wi-Fi. Donnez-lui le vôtre, au choix :
- **par l'USB**, juste après le flash : le [flasheur web](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) du projet (Chrome ou Edge), *Connecter et installer*, puis son bouton Wi-Fi, ou [ESPHome Web](https://web.esphome.io), *Connect*, puis *Configure Wi-Fi*. Les deux passent par Improv et marchent avec un firmware compilé soi-même. Fermer cette fenêtre redémarre la tablette une fois : c'est normal ;
- **sans câble** : connectez un téléphone au réseau ouvert **« Tab5 Fallback AP »** ; une page s'ouvre (sinon allez sur `http://192.168.4.1`) pour choisir votre réseau. <!-- pragma: allowlist secret -->

Le réseau est gardé d'une mise à jour à l'autre. L'AP de secours revient dès que la tablette perd son Wi-Fi une minute, pour en donner un autre.

---

## Étape 6 — Ajouter la tablette à Home Assistant

**Dans les 30 minutes qui suivent le démarrage de la tablette** (sa fenêtre d'appairage) : *Paramètres → Appareils et services*, la tablette apparaît comme découverte (ESPHome). *Configurer*, puis *Valider*. Home Assistant crée la clé de chiffrement, la donne à la tablette et la garde : rien à recopier.

- Fenêtre ratée ? Redémarrez la tablette : elle se rouvre pour 30 minutes. Une fois la clé reçue, elle ne s'ouvre plus.
- Ensuite, dans les options de l'appareil (*ESPHome → Configurer*), cochez **« Autoriser l'appareil à effectuer des actions Home Assistant »** : la voix, l'agenda et le réveil s'en servent.

---

## Mises à jour OTA

Une fois la tablette sur votre réseau, les compilations suivantes passent par le Wi-Fi :

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

L'envoi n'est plus chiffré (il n'y a pas de clé dans le YAML) ; la tablette vérifie la signature et refuse un firmware signé par une autre clé. Vos compilations sont signées par votre clé, rien d'autre à faire.

Une tablette installée depuis le [flasheur web](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) reçoit plutôt ses mises à jour par Home Assistant : son entité « Firmware » lit le manifeste publié toutes les 6 h, et « Installer » télécharge l'image, que la tablette vérifie avec la clé du projet ([ADR-0022](decisions/0022-published-firmware-pages-channels.md)). Un firmware compilé soi-même n'a pas cette entité.

**Journaux :** `esphome logs` cherche la clé dans le YAML et n'en trouve plus. Utilisez `python tools/tab5_logs.py --host 192.168.x.x --config-ha \\<ip-de-ha>\config` : il lit la clé que garde Home Assistant (`.storage/core.config_entries`, ou la variable `TAB5_CLE_API`) et ne l'affiche jamais.

---

## Passer à la 3.0

La 3.0 change la façon dont la tablette est protégée ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)). Une fois, sur place (les étapes 3 et 4 ci-dessous doivent tenir dans les 30 minutes qui suivent le démarrage de la tablette) :

1. **Avant de flasher**, préparez Home Assistant pour la 3.0 : le blueprint de l'étape 4, point 6 (la tablette ne connaît plus vos entités, [ADR-0019](decisions/0019-logical-slots-blueprint.md)).
2. **Clé de signature** (étape 3), puis compilez : `esphome compile tab5-ha-hmi.yaml`.
3. **Flashez.** Le firmware 2.x refuse un envoi en clair : celui-ci part donc chiffré avec votre ancienne clé (`api_encryption_key` de votre `secrets.yaml`, jamais affichée) :
   ```bash
   python tools/migrer_vers_3.py --host 192.168.x.x --port COM3
   ```
   `--port` est le port USB de la tablette (`COM…` sous Windows, `/dev/ttyACM0` sous Linux ; `python -m serial.tools.list_ports -v` les liste, le numéro de série de la tablette est son adresse MAC). Ou seulement par USB : `esphome upload tab5-ha-hmi.yaml --device COM3`.
4. **Wi-Fi.** La 2.x avait les identifiants compilés, la 3.0 non. Avec `--port`, le script les lui redonne juste après le redémarrage, par l'USB (Improv, `wifi_ssid` et `wifi_password` du même `secrets.yaml`, jamais affichés) : ni téléphone ni point d'accès. Sans lui, la tablette démarre sans réseau et ouvre « Tab5 Fallback AP » : connectez-vous-y et choisissez votre réseau, ou utilisez le bouton Wi-Fi du flasheur web par l'USB (étape 5).
5. **Home Assistant** signale que la tablette « a désactivé le chiffrement du transport » (*Paramètres → Appareils et services*, réauthentification) : confirmez. HA lui donne ensuite une nouvelle clé tout seul. Les entités, les automatisations et l'historique restent les mêmes.

Ensuite, `secrets.yaml` peut partir (gardez l'ancienne clé seulement si vous risquez de reflasher une 2.x), et `tab5_fuseau` est ignoré.

---

## Fournisseurs météo

L'écran ne dépend plus d'un seul service météo (lot 4c, 27/09/2026).

Les **prévisions et la météo du moment** (pages horaires et journalières, goutte d'humidité) viennent de n'importe quelle entité `weather.*`, choisie dans Home Assistant avec la liste « Tab5 · source des prévisions », qui propose les entités météo présentes (`VOTRE_VILLE` n'est que le choix par défaut). Le Tab5 ne demande à chaque entité que ce qu'elle déclare : les prévisions journalières, sinon les demi-journées (NWS) ou les horaires (OpenWeatherMap gratuit) regroupées par date ; sans prévisions horaires (Buienradar), la page horaire reste vide. Les heures sont affichées en heure locale.

La **pluie dans l'heure** et les **vigilances** sont facultatives. Leur source se choisit **dans Home Assistant**, avec les deux listes de `packages/tab5_meteo_sources.yaml`, sans toucher au YAML :

| Carte | Liste « Tab5 · source … » | Ce qu'il faut |
|---|---|---|
| Pluie dans l'heure (barres et phrase) | Météo-France | l'intégration Météo-France (France) : `sensor.<ville>_next_rain` |
| | OpenWeatherMap | l'intégration OpenWeatherMap en mode **v3.0**, qui demande l'abonnement One Call (1 000 appels par jour gratuits ; HA interroge toutes les 10 min). Réglez `VOTRE_METEO_OWM` sur l'entity_id complet (`weather.openweathermap` par défaut) |
| | Aucune | la carte pluie est masquée |
| Vigilances | Météo-France | `sensor.<département>_weather_alert` (réglez `VOTRE_DEPARTEMENT`) |
| | MeteoAlarm | l'intégration MeteoAlarm (en YAML seulement, 39 pays européens, une alerte à la fois). Réglez `VOTRE_METEOALARM` sur l'entity_id complet (`binary_sensor.meteoalarm` par défaut) |
| | Aucune | pas d'icônes de vigilance |

Limites, en toute franchise :
- OpenWeatherMap a été essayé sur l'installation de l'auteur le 27/09/2026 (prévisions et pluie dans l'heure, un jour sec) ; ses prévisions journalières couvrent 8 jours, les derniers jours des pages de 15 jours restent donc vides. MeteoAlarm et le regroupement des demi-journées (NWS) n'ont été testés qu'avec des données simulées.
- La probabilité de gel n'existe que chez Météo-France. L'icône flocon lit `sensor.<ville>_snow_chance` quand il existe ; sinon, elle suit la condition du moment (neige).
- Les autres sources d'alertes (DWD, Environment Canada, NWS Alerts…) ne sont pas encore branchées.

---

## Adapter à sa maison

L'écran a été dessiné autour de la maison de l'auteur : trois lumières, une clim, un volet de serre, une TV, cinq capteurs de plantes. **Ce que vous n'avez pas disparaît**, avec ses boutons ([ADR-0018](decisions/0018-optional-zones-confirmed-by-ha.md), [ADR-0019](decisions/0019-logical-slots-blueprint.md)).

- **Vos appareils se choisissent dans Home Assistant**, dans l'automatisation « Tab5 — emplacements » (le blueprint de l'étape 4). En changer se fait dans l'interface de HA : ni flash, ni redémarrage.
- **Retirer une zone : laissez son emplacement vide.** Un emplacement vide, ou une entité qui n'existe pas, est absent. Une entité qui existe mais est `unavailable` garde sa zone (« -- », « Hors ligne »). **Sans l'automatisation du blueprint, rien ne disparaît** (et aucun de vos appareils ne s'affiche).
- **Une zone manque par erreur ?** Le capteur de diagnostic « Zones masquées » de la tablette liste ce qui a disparu.
- Une zone revient d'elle-même dès que son entité envoie une valeur.

| Zone | Entrée du blueprint | Masqué quand elle est vide |
|---|---|---|
| Lumières (jusqu'à 3) | Lumière 1 à 3 | Icônes des tuiles 3 à 5, carte du calque « HA », sélecteur du popup lumière, « Tout éteindre » |
| PC | PC (un interrupteur l'allume ; un suivi de présence l'affiche seulement) | Icône d'état, carte « PC Bureau » ; la première tuile aussi s'il n'y a pas non plus de TV |
| TV | TV, et Télécommande de la TV pour les touches | Bouton TV et télécommande ; « HA » et « Sys » glissent d'une colonne |
| Téléphone | Batterie du téléphone | Icône d'état |
| Pièce | Température de la pièce (et Humidité de la pièce) | Sa température |
| Serre | Seconde température (serre) | Sa température ; l'icône devient une manette, l'entrée de l'arcade reste |
| Pots (0 à 5) | Pot 1 à 5 : le capteur d'humidité ; conductivité, éclairement, température et batterie sont pris sur le même appareil | Jusqu'à 4 pots : un emplacement chacun ; à 5 : le résumé « plus secs / médiane / plus humide ». Cartes du popup, recentrées |
| Clim | Climatisation | − / consigne / + et le popup |
| Volet | Volet (et le package `volet_serre_tracking.yaml` pour un volet qui ne signale pas sa course) | Icônes de la tuile 2, carte du calque « HA » |
| Planning de travail | Agenda de travail | Panneau planning de la carte centrale |

Les horaires du planning viennent encore du package `tab5_push.yaml` (`VOTRE_EMAIL_gmail_com`) : choisissez le même agenda des deux côtés.

Limites :
- **Plus de 3 lumières, ou un autre appareil sur une tuile** : pas encore. Les tuiles sont 5 places fixes (PC/TV, volet, trois lumières).
- **Agendas écrits dans les packages** : `calendar.famille`, `calendar.anniversaires` et l'agenda des jours fériés français (`tab5_reveil.yaml`, `tab5_calendar.yaml`) se corrigent à la main, voir [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md#version-française).
- Pour voir une maison plus petite sans toucher à la vôtre : [mode démo](demo_mode.md#maison-minimale-zones-optionnelles), option `--maison-minimale`.
