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

Open `Tab5/user_entities.yaml` (gitignored — never committed, same pattern as `secrets.yaml`):

```yaml
# --- Voice assistant ---
entity_tab5_pipeline_select: select.your_tab5_assistant_pipeline

# --- System console ---
entity_primary_active: input_boolean.your_primary_active_flag
entity_push_automation: automation.your_tab5_push_automation
...
```

**Screen language:** French by default; add `tab5_langue: English` for English on the first boot. It can then be changed from Home Assistant (select « Langue »), see [translations](translations.md).

**Time zone:** Europe/Paris by default; add `tab5_fuseau: America/Montreal` (any tz name) for the clock and the alarm clock elsewhere.

**Tab5 revision:** if the display chip on your sticker is not the ST7123, add `tab5_ecran: st7121` or `tab5_ecran: ili9881c` to this file (see [Hardware revisions](hardware.md#hardware-revisions)). Leave it out for the ST7123.

Replace each value with your own entity IDs. **Your devices (lights, climate, plants, TV…) are not set here any more (since 3.0)**: you pick them in Home Assistant with the mouse, see Step 4 and [Adapt to your home](#adapt-to-your-home); old `entity_light_…` keys in an existing file are simply ignored. The entry point `tab5-ha-hmi.yaml` includes this file via `substitutions: !include Tab5/user_entities.yaml`. Two optional keys, `entity_tab5_satellite` and `entity_tab5_media_player`, only matter if you rename the device in Home Assistant: they hold the entity IDs HA derives from the device name (defaults in `Tab5/tab5-scripts.yaml`, commented example in the template).

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
6. **Choose your devices**: *Settings → Automations & scenes → Blueprints → Import blueprint*, paste
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`,
   then *Create automation* and pick an entity for each slot (all optional). Or copy the file into
   `config/blueprints/automation/tab5/`. One automation per tablet.

Start with `packages/tab5_push.yaml` (push automations, shared scripts, the scripts the Tab5 calls, the optional-zones answer) and `packages/tab5_meteo_sources.yaml` (weather, rain and warning sources, required since 2.2.0) — the others add optional features. See [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) for what each package does and the full placeholder list.

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

## Prérequis

- Une instance **Home Assistant** fonctionnelle (toute méthode d'installation)
- L'add-on **ESPHome** ou la CLI ESPHome standalone (`pip install esphome`)
- ESPHome version **≥ 2026.9.0** — imposée par le `min_version:` de `tab5-ha-hmi.yaml` : une version antérieure refuse de compiler. La 2026.7.0 a apporté la plateforme tactile `st7123` officielle (plus besoin d'`external_components`), l'audio zero-copy, le VAD et la PSRAM via SDIO ; le plancher est passé à 2026.8.1 le 26/08/2026 pour les correctifs API, assistant vocal et handler de crash que ce projet exerce tous les jours, puis à 2026.9.0 le 16/09/2026 parce que les mises à jour OTA sont chiffrées avec la clé API (`ota: encryption:` n'existe pas dans les versions antérieures — raisons dans le commentaire au-dessus de `min_version:`)
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

**Langue de l'écran :** le français par défaut ; ajoutez `tab5_langue: English` pour l'anglais au premier démarrage. Elle se change ensuite depuis Home Assistant (select « Langue »), voir [traductions](translations.md#version-française).

**Fuseau horaire :** Europe/Paris par défaut ; ajoutez `tab5_fuseau: America/Montreal` (n'importe quel nom de fuseau tz) pour l'horloge et le réveil ailleurs.

**Révision du Tab5 :** si la puce écran de votre autocollant n'est pas la ST7123, ajoutez `tab5_ecran: st7121` ou `tab5_ecran: ili9881c` dans ce fichier (voir [Révisions matérielles](hardware.md#révisions-matérielles)). Pour la ST7123, ne mettez rien.

Ouvrez `Tab5/user_entities.yaml` (gitignoré — ne jamais committer, même principe que `secrets.yaml`) et remplacez chaque valeur. **Vos appareils (lumières, clim, plantes, TV…) ne se règlent plus ici (depuis la 3.0)** : vous les choisissez dans Home Assistant, à la souris, voir l'étape 4 et [Adapter à sa maison](#adapter-à-sa-maison) ; les anciennes clés `entity_light_…` d'un fichier existant sont simplement ignorées. Le point d'entrée `tab5-ha-hmi.yaml` les charge via `substitutions: !include Tab5/user_entities.yaml`. Deux clés facultatives, `entity_tab5_satellite` et `entity_tab5_media_player`, ne servent que si vous renommez l'appareil dans Home Assistant : elles portent les identifiants qu'HA dérive du nom de la tablette (défauts dans `Tab5/tab5-scripts.yaml`, exemple commenté dans le modèle).

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
6. **Choisissez vos appareils** : *Paramètres → Automatisations et scènes → Blueprints → Importer un blueprint*, collez
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`,
   puis *Créer une automatisation* et choisissez une entité pour chaque emplacement (tous facultatifs). Ou copiez
   le fichier dans `config/blueprints/automation/tab5/`. Une automatisation par tablette.

Commencez par `packages/tab5_push.yaml` (automatisations de poussée, scripts partagés, scripts appelés par le Tab5, réponse des zones optionnelles) et `packages/tab5_meteo_sources.yaml` (sources de la météo, de la pluie et des vigilances, obligatoire depuis la 2.2.0) ; les autres ajoutent des fonctions optionnelles. Voir [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) pour le rôle de chaque package et la liste complète des placeholders.

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
