# Installation & Configuration

## English · [Français](#version-française)

---

> **Just want to try it first?** [`docs/demo_mode.md`](demo_mode.md) shows the full dashboard on a flashed device in a few minutes, with no Home Assistant install at all. Come back here when you're ready for the real install.

## Without compiling (install page)

Since 3.0, a ready-made, signed firmware installs from the browser. In this order:

1. **Home Assistant side first**: the archive `tab5_home_assistant.zip` of [Step 4](#step-4--set-up-the-home-assistant-packages), unzipped into Home Assistant's `config/` folder, one line in `configuration.yaml`, then your sources picked in Home Assistant with the mouse. No repository, no Python, nothing to fill in.
2. **Flash** from the [install page](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) (Chrome or Edge, a USB-C cable that carries data): your display revision, the Stable channel, *Connect and install*. On a new tablet, accept to erase it. In the port list, the tablet is « USB JTAG/serial debug unit »; if several ports have that name, unplug the tablet to see which one disappears.
   - « Failed to initialize… holding the BOOT button »: the Tab5 has no BOOT button. Hold its reset button about 2 s, until the internal green LED blinks fast (download mode), start again, and press reset once at the end to restart it.
   - The tablet already runs the same version: the page shows no *Install*. To start from scratch, « Erase User Data » (in red, at the bottom) erases everything, Wi-Fi, key and settings included, then installs again.
3. **Wi-Fi**: from the same window (*Connect to Wi-Fi*, over USB), or with a phone on the open « Tab5 Fallback AP » network.
4. **Add it to Home Assistant** within 30 minutes of its start: *Settings → Devices & services*, the ESPHome device is discovered, *Configure*. Home Assistant gives it its key. A tablet Home Assistant already knows gets a new key by itself, nothing to confirm (checked on 2026-09-28). Nothing else to allow: the tablet asks Home Assistant for everything through events ([ADR-0025](decisions/0025-events-only.md)).
   - Firmware 3.1 or older only: also tick « Allow the device to perform Home Assistant actions » (*ESPHome → Configure*), which voice, calendar and alarm clock need there.
5. **Your devices**: create the automation from the blueprint ([Step 4](#step-4--set-up-the-home-assistant-packages), item 5).

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

**Screen language:** French by default; add `tab5_langue: English` (or `Deutsch`, `Nederlands`, `Español`, `Italiano`, `Türkçe`) for another language on the first boot. It can then be changed from Home Assistant (select « Langue »), see [translations](translations.md).

**Time zone:** nothing to set since 3.0. The tablet takes Home Assistant's time zone and keeps the last one it received, so the alarm clock stays right when HA is down after a power cut. An old `tab5_fuseau` line is ignored.

**Tab5 revision:** if the display chip on your sticker is not the ST7123, add `tab5_ecran: st7121` or `tab5_ecran: ili9881c` to this file (see [Hardware revisions](hardware.md#hardware-revisions)). Leave it out for the ST7123.

**Your devices (lights, climate, plants, TV…) are not set here any more (since 3.0)**: you pick them in Home Assistant with the mouse, see Step 4 and [Adapt to your home](#adapt-to-your-home); old `entity_light_…` keys in an existing file are simply ignored. The entry point `tab5-ha-hmi.yaml` includes this file via `substitutions: !include Tab5/user_entities.yaml`. No entity key is left: Home Assistant finds the tablet's own entities (voice satellite, media player, pipeline select) and those of the « MAJ Écran » button by itself ([ADR-0025](decisions/0025-events-only.md)); old `entity_tab5_…`, `entity_primary_active` and `entity_push_automation` lines are ignored.

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

The whole Home Assistant side is one archive, **`tab5_home_assistant.zip`**, attached to the [releases](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases) (since 3.2.0). **Nothing to fill in**: every value of your home is picked afterwards in Home Assistant, with the mouse ([ADR-0024](decisions/0024-packages-without-placeholders.md)).

1. **Download** `tab5_home_assistant.zip` from the latest release and **unzip it into Home Assistant's `config/` folder**, the one holding `configuration.yaml` (Samba share, or the File editor / Studio Code Server add-on). It adds `packages/`, `custom_templates/` and `blueprints/automation/tab5/`; the `tab5_optionnel/` folder is not loaded (see below).
2. **One line in `configuration.yaml`: the only YAML you write** (skip it if it is already there):
   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```
   If `homeassistant:` already exists, add only the `packages:` line under it.
3. *Developer tools → YAML → Check configuration*, then **restart** Home Assistant. Later, replacing the same files only needs *Developer tools → YAML → All YAML configuration*.
4. **Pick your sources**: *Settings → Devices & services → Entities*, search **« Tab5 · »**, open a list and choose. Each list offers what your Home Assistant has:

   | List | What it drives | Chosen by default |
   |---|---|---|
   | Tab5 · source des prévisions | forecasts and current weather (any `weather.*`) | the Météo-France city, otherwise the first weather entity |
   | Tab5 · source de la pluie dans l'heure | rain card: Météo-France, OpenWeatherMap, Buienradar, DWD, Met.no, Open-Meteo or Aucune (none) | Météo-France; without the Météo-France integration, Open-Meteo is used |
   | Tab5 · source des vigilances | warning icons: Météo-France, MeteoAlarm, DWD, CAP Alerts or Aucune | Météo-France; without the Météo-France integration, no icons (as Aucune) |
   | Tab5 · agenda de travail | work events: planning, rest days and the **alarm time** (which events: see the keyword below) | nothing |
   | Tab5 · agenda des rendez-vous | calendar popup, appointment reminders, morning briefing | nothing |
   | Tab5 · agenda des anniversaires | birthdays in the calendar popup | the only calendar named « anniversaires » (or birthday…) |
   | Tab5 · agenda des jours fériés | public holidays in the calendar popup | the only public-holiday calendar (Holiday integration, or its name says so) |
   | Tab5 · agenda des vacances scolaires | school holidays in the calendar popup (every event of this calendar) | the only calendar whose name says so (« calendrier scolaire », « vacances scolaires », school…) |
   | Tab5 · téléphone | screen on when you come home, off when you leave | the only phone of the companion app |
   | Tab5 · capteur de présence | screen on at presence, off after 15 min without | nothing |
   | Tab5 · TV Samsung, Tab5 · adresse de la TV | app buttons of the TV popup (Samsung Tizen) | the only Samsung Smart TV; the address given by a router tracker when it reports one, otherwise type its IP |
   | Tab5 · pipeline de discussion | the Assist pipeline of the screen's « Discu » mode | nothing: the Domo / Discu buttons are then hidden |

   Each list has a two-language name, « français · english » (« Tab5 · agenda de travail · work calendar »), and keeps its entity id. Left on « Aucun », a feature simply stays off, without errors.

   - **Which events are work**: the text « Tab5 · mot des événements de travail · work event keyword » holds the words that make an event of the work calendar a work shift, comma-separated, in any case, looked for in the title. **Empty = every event of the work calendar**, for a calendar that holds only your shifts. The author's work calendar also holds his appointments: he types `Travail`.
   - **School holidays**: any calendar of yours. In France, the ministry publishes one ICS file per zone: *Settings → Devices & services → Add integration → Remote Calendar*, name « Calendrier scolaire » (the list then picks it by itself), URL `https://fr.ftp.opendatasoft.com/openscol/fr-en-calendrier-scolaire/Zone-A.ics` (`Zone-B.ics`, `Zone-C.ics` for the other zones; checked on 2026-09-29, until summer 2028). The weather providers' own entities (Météo-France rain and warning sensors, OpenWeatherMap, MeteoAlarm, DWD, CAP Alerts) and the tablet's entities (screen, alarm, microphone…) are found by themselves; the tablet by its device model, whatever you named it.
5. **Choose your devices**: *Settings → Automations & scenes → Blueprints*, « Tab5 — emplacements de l'écran · screen slots » (unzipped with the rest; or *Import blueprint* with
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`),
   then *Create automation* and pick an entity for each slot (all optional); its labels are in French and English. One automation per tablet. Its « Agenda de travail » can stay empty: it then takes the one of « Tab5 · agenda de travail ».

`tab5_optionnel/volet_serre_tracking.yaml` is only for a shutter that reports neither its position nor its travel (the author's Tuya motor): copy it into `packages/`, reload, and pick the shutter in « Tab5 · volet à course simulée ». It is not installed by default because, once present, it takes over the shutter buttons from the blueprint.

The same files are in the repository (`HomeAssistant_Config/packages/`, `custom_templates/`, `blueprints/`, `optionnel/`); see [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md) for what each package does.

> These packages are exactly what runs on the author's Home Assistant since 2026-09-26, with no per-home edit since 2026-09-28. There are no private versions and nothing to merge into `automations.yaml` or `scripts.yaml`.

**Coming from packages with placeholders (3.1.0 and earlier)?** Replace the files, then set each list to your old value: `VOTRE_EMAIL_gmail_com` → « Tab5 · agenda de travail », `calendar.famille` → rendez-vous, `calendar.anniversaires` and the public-holiday calendar (usually picked by default), `VOTRE_TELEPHONE` → téléphone, `VOTRE_CAPTEUR_PRESENCE` → capteur de présence, `VOTRE_TV` → TV Samsung, and the IP of the `tab5_tv_app_url` line of `secrets.yaml` → « Tab5 · adresse de la TV » (the line can then go). The weather choice is kept. Keep `volet_serre_tracking.yaml` in `config/packages/` if you use it, and pick your shutter in its list. **Set « Tab5 · agenda de travail » before the new automations run**: reload *Input texts* and *Template entities* first, choose, then reload *Scripts*, *Automations* and *REST commands*. Until it is set, every day counts as a rest day, and the alarm clock follows.

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
- Nothing else to allow. The tablet never calls a Home Assistant action: it sends events, which `packages/tab5_evenements.yaml` turns into a fixed list of actions, for a Tab5 only ([ADR-0025](decisions/0025-events-only.md)). The « Allow the device to perform Home Assistant actions » option stays unticked (firmware 3.1 or older still needs it, see [Upgrading from 3.1](#upgrading-from-31)).

---

## OTA updates

Once the tablet is on your network, later builds go over Wi-Fi:

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

The transfer is not encrypted any more (there is no key in the YAML); the tablet checks the signature and refuses a firmware signed by another key. Your builds are signed by your key at compile time, nothing else to do.

A tablet installed from the [web flasher](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) gets its updates from Home Assistant instead: its « Firmware » entity reads the published manifest every 6 hours, and « Install » downloads the image, which the tablet checks against the project key ([ADR-0022](decisions/0022-published-firmware-pages-channels.md)). A firmware you compile yourself has no such entity.

**The Home Assistant files do not update themselves**: replace them with those of `tab5_home_assistant.zip` from the same release (Step 4). The files of an archive know their version: when the tablet runs a newer major or minor release (X.Y; a patch release alone does not count), Home Assistant says so (notification « Tab5 : fichiers Home Assistant à mettre à jour », sensor « Tab5 · fichiers HA en retard »). Files copied from the repository have no version and are never compared.

**Logs:** `esphome logs` looks for the key in the YAML and no longer finds one. Use `python tools/tab5_logs.py --host 192.168.x.x --config-ha \\<ha-ip>\config`: it reads the key Home Assistant keeps (`.storage/core.config_entries`, or the `TAB5_CLE_API` variable) and never prints it.

---

## Upgrading from 3.2

Replace the Home Assistant files with those of the new `tab5_home_assistant.zip` (Step 4), then check three lists:

1. **Work calendar holding other events?** Type your word in « Tab5 · mot des événements de travail » (the author: `Travail`). Up to 3.2, only titles containing « Travail » counted; left empty, every event of the work calendar now counts as work.
2. **School holidays** no longer come from a table of the French Zone A: pick a calendar in « Tab5 · agenda des vacances scolaires » (Step 4).
3. **Discussion mode**: pick its pipeline in « Tab5 · pipeline de discussion ». Up to 3.2, the tablet asked for a pipeline named exactly « Discussion LLM ».

The lists keep their entity ids, so your dashboards and automations need nothing.

## Upgrading from 3.1

After 3.1 the tablet no longer calls Home Assistant actions: it sends events, which the new package `packages/tab5_evenements.yaml` turns into actions ([ADR-0025](decisions/0025-events-only.md)). In this order:

1. **Home Assistant files first**: replace them with those of `tab5_home_assistant.zip` (Step 4), which bring `tab5_evenements.yaml`, and set the « Tab5 · … » lists as told in « Coming from packages with placeholders » at the end of Step 4. A 3.1 tablet sends none of these events: the package just waits, nothing changes.
2. **Then the firmware** (« Firmware » entity, or your own build).
3. **Then untick** « Allow the device to perform Home Assistant actions » (*ESPHome → Configure*): the tablet no longer needs it, and without it Home Assistant refuses any action the device would ask for.

A new firmware **without** the package does not crash and logs nothing, but what it asks Home Assistant is lost: the calendar popup shows only its local grid (no work hours, holidays or appointments; a tapped day stays on « Chargement... »), nothing is spoken (appointments, alarm briefing, « Volet arrêté »), the Domotique / Discussion buttons no longer change the pipeline, a dismissed alert is hidden on the tablet only, until its next restart, and « MAJ Écran », « Recharger autos » and « Redémarrer HA » do nothing.

---

## Upgrading from 2.x

3.0 changes how the tablet is protected ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)). Once, in person (Steps 3 and 4 below must happen within 30 minutes of the tablet's start):

1. **Before flashing**, set up Home Assistant for 3.0: the blueprint of Step 4, item 5 (the tablet no longer knows your entities, [ADR-0019](decisions/0019-logical-slots-blueprint.md)).
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

**Forecasts and current weather** (hourly and daily pages, the humidity drop) come from any `weather.*` entity, picked in Home Assistant from the select « Tab5 · source des prévisions », which lists the weather entities you have (by default the Météo-France city, otherwise the first weather entity). The Tab5 asks each entity only for what it declares: daily forecasts, or else twice-daily ones (NWS) or hourly ones (free OpenWeatherMap) grouped by date; without hourly forecasts (Buienradar), the hourly page stays empty. Hours are shown in local time.

**Rain in the next hour and weather warnings** are optional. Their source is chosen **in Home Assistant**, with the two selects of `packages/tab5_meteo_sources.yaml`, without editing YAML:

| Card | Select « Tab5 · source … » | What it needs |
|---|---|---|
| Rain in the next hour (bars and sentence) | Météo-France | the Météo-France integration (France): `sensor.<city>_next_rain` |
| | OpenWeatherMap | the OpenWeatherMap integration in **v3.0** mode, which needs a One Call subscription (1,000 calls a day free; HA polls every 10 min). Found by itself: the chosen weather entity if it is OpenWeatherMap's, otherwise its first one |
| | Buienradar | nothing to install, no key: rain radar of the Netherlands and Belgium (also Luxembourg and the edges of France and Germany), every 5 min. Non-commercial use, source to credit |
| | DWD | nothing to install, no key: the DWD radar composite through [Bright Sky](https://brightsky.dev), Germany and neighbouring countries, every 5 min |
| | Met.no | nothing to install, no key: Nowcast radar of the Nordic countries (Norway, Sweden, Finland, Denmark), every 5 min |
| | Open-Meteo | nothing to install, no key, everywhere, but a **weather model**, not a radar, in 15-min steps (true 15-min data in central Europe and North America, interpolated elsewhere). Non-commercial use |
| | Aucune (none) | the rain card is hidden |
| Weather warnings | Météo-France | `sensor.<department>_weather_alert`, found by itself (the department of the Météo-France city) |
| | MeteoAlarm | the MeteoAlarm integration (YAML only, 39 European countries, one alert at a time), found by itself (its binary sensor « Information provided by MeteoAlarm ») |
| | DWD | Germany: the *Deutscher Wetterdienst (DWD) Weather Warnings* integration (built in, set up from the UI with the name or ID of your DWD warning cell, or a device tracker). All the warnings of the region, not the pre-warnings (« Vorabinformation ») |
| | CAP Alerts | the [CAP Alerts](https://github.com/seevee/cap_alerts) integration (HACS, custom repository), one entity per alert: MeteoAlarm (Europe, every alert of your region), NWS (United States), Environment Canada, and about 100 national services through the WMO |
| | Aucune (none) | no warning icons |

Buienradar, DWD, Met.no and Open-Meteo are queried by Home Assistant itself (`rest_command.tab5_pluie` in the package), every 5 min and only while chosen, at your home location (`zone.home`) rounded to 0.01° (about 1 km). Outside their area they answer « no data ». Open-Meteo is also used when the rain list is left on Météo-France (its default) and Home Assistant has no Météo-France rain sensor, for example outside France: the rain card then works with nothing to set. Choose « Aucune » to send nothing; add the Météo-France integration later and it takes over by itself.

With DWD and CAP Alerts, a warning counts while it is in force or starts within 24 hours; the overall level is the highest of them. Both are read by `custom_templates/tab5_vigilance.jinja`, which comes with the archive (Météo-France and MeteoAlarm work without it). The hazard slot comes from the DWD code, the MeteoAlarm hazard type, or the icon CAP Alerts gives the alert; a hazard with no slot (drought, air quality…) only raises the overall level.

Honest limits:
- OpenWeatherMap was tried on the author's installation on 2026-09-27 (forecasts and rain in the next hour, on a dry day); its daily forecast covers 8 days, so the last days of the 15-day pages stay empty. MeteoAlarm and the twice-daily grouping (NWS) were tested with simulated data only.
- Buienradar, DWD, Met.no and Open-Meteo were queried on 2026-09-29 (Amsterdam, Berlin, Oslo, south-west France; a dry day) and their answers read by the same templates in Home Assistant's engine; rain itself was simulated. Open-Meteo is also queried by the fresh-install CI.
- DWD was added to the author's Home Assistant on 2026-09-29 (Berlin, a day without warnings): entities found, dates read. The warnings themselves, and everything from CAP Alerts, were tested with simulated data in Home Assistant's template engine and in the fresh-install CI.
- Frost probability exists only at Météo-France. The snowflake icon reads `sensor.<city>_snow_chance` when it exists; otherwise it follows the current condition (snowy).

---

## Adapt to your home

The screen was first drawn around the author's home. Everything is chosen in Home Assistant, in the « Tab5 — emplacements » automation (the blueprint of Step 4): changing a device is an edit in HA's UI, no flash, no restart.

### Rooms (firmware 3.2 and later)

The five tiles at the bottom of the screen are **rooms** you fill yourself ([ADR-0023](decisions/0023-rooms-generic-tiles.md)):

- **Up to 5 rooms of 5 devices**, one per page of the bottom row. Room 1 is the home page (today to day 4); rooms 2 and 3 are one and two swipes to the left (days 5-9, 10-14); rooms 4 and 5 one and two swipes to the right (next hours). In each room, pick the devices in the order of the tiles, left to right (they can be dragged); only the first five are used. A device may be in several rooms.
- **Devices that fit on a tile**: lights; switches, fans, humidifiers, input booleans and automations; covers and valves; media players; scenes, scripts and buttons; sensors and numbers (shown, not controlled); binary sensors, people, trackers and locks (shown); climate — a climate tile shows the room temperature, and a tap opens the climate popup for that unit (the one in the « Climatisation » input keeps the − / setpoint / + card of the home page).
- **Names and icons come from Home Assistant.** A room takes the name you type, otherwise the area its devices share, otherwise « Pièce n ». A tile takes the entity's name without the room's name (« Lampe du salon » in « Salon » becomes « Lampe »), and the icon chosen in the entity's settings, otherwise one for its kind.
- **Customise a tile** (folded section « Personnaliser des tuiles »): another name, another icon, or a behaviour — *on only* (never switched off from the screen), *confirm* (a second tap within 3 s), *read only*.
- The « HA » button shows the current page's room; a swipe goes to the next room that has devices.
- A sensor's value is sent with the other measurements, every 5 minutes; the other devices are sent as soon as what the screen shows changes.
- **Room 1 left empty**: the home page keeps the 3.x setup (folded section « Tuiles de l'accueil (réglage 3.x) »: PC or TV, shutter, three lights), with the PC tile's PC + TV behaviour. Nothing to redo after the update.
- **Firmware 3.0 or 3.1**: the blueprint reads the tablet's version and then only uses the 3.x setup; the rooms show up once the firmware is updated. The firmware and the blueprint can be updated in either order.

### Other zones

**What you don't have disappears**, with its buttons ([ADR-0018](decisions/0018-optional-zones-confirmed-by-ha.md), [ADR-0019](decisions/0019-logical-slots-blueprint.md)).

- **Remove a zone: leave its slot empty.** An empty slot, or an entity that doesn't exist, is absent. An entity that exists but is `unavailable` keeps its zone (« -- », « Hors ligne »). **Without the blueprint's automation, nothing disappears** (and nothing of your devices is shown).
- **A zone missing by mistake?** The tablet's diagnostic sensor « Zones masquées » lists what disappeared.
- A zone comes back by itself as soon as its entity sends a value.

| Zone | Blueprint input | Hidden when empty |
|---|---|---|
| TV | TV, and Télécommande de la TV for the remote keys | TV button and remote; « HA » and « Sys » move one column right |
| Phone | Batterie du téléphone | Status icon |
| Room | Température de la pièce (and Humidité de la pièce) | Its temperature |
| Greenhouse | Seconde température (serre) | Its temperature; the icon becomes a gamepad, the arcade entrance stays |
| Plants (0 to 5) | Pot 1 to 5: the moisture sensor; conductivity, light, temperature and battery are taken from the same device | Up to 4 plants: one slot each; 5: the « driest / median / wettest » summary. Popup cards, re-centred |
| Climate | Climatisation | − / setpoint / + and the popup |
| Work planning | Agenda de travail | Planning panel of the central card |
| Voice « Discussion » mode | none: the list « Tab5 · pipeline de discussion » set to « Aucun » (without the list, the zone stays) | Domo / Discu buttons of the home page and of the assistant popup; the tablet goes back to Domo |
| Lights (3.x setup) | Lumière 1 to 3 | Icons of tiles 3 to 5, card of the « HA » layer, light-popup selector, « Tout éteindre » |
| PC (3.x setup) | PC (a switch turns it on; a presence tracker only shows it) | Status icon, « PC Bureau » card; the first tile too if there is no TV either |
| Shutter (3.x setup) | Volet (and the optional `volet_serre_tracking.yaml` package, `tab5_optionnel/` of the archive, for a shutter that doesn't report its travel) | Icons of tile 2, card of the « HA » layer |

The « 3.x setup » rows are the home page of a 3.0 or 3.1 firmware, and of a 3.2 firmware while room 1 is empty. The planning hours and the alarm time come from the list « Tab5 · agenda de travail » (Step 4); leave the blueprint's « Agenda de travail » empty and it takes the same one.

Limits:
- **Icons**: the screen holds a limited palette ([tile icons](tiles_icons.md)). An icon outside it shows the default of its kind; adding one means a line in `Tab5/tuiles_icones.yaml` and a new release.
- **Names**: the screen's fonts cover Latin alphabets only; other characters are dropped, and long names are cut.
- **Renaming an entity**: its tile follows at the tablet's next connection, or as soon as the automation is saved again.
- **Climate** ([ADR-0026](decisions/0026-climate-from-device.md)): any brand. The blueprint sends the unit's bounds, step, unit (°C or °F, that of your weather entity) and modes; the popup takes its name as title, and a button the unit cannot do disappears. **Several units** ([ADR-0027](decisions/0027-climate-per-tile.md)): put each one in a room; its tile opens the popup for it, with its own bounds, modes and name. A change made outside the screen (remote, app) shows in the popup at once for the setpoint and the mode, within 5 minutes for fan, swing, preset and room temperature. With a firmware older than the blueprint, such a tile only shows its temperature. The screen has buttons for cool, heat, dry, fan and off, Éco, Boost, Silence, Oscillation and Brise only: other modes (heat/cool, auto, fan speeds, sleep…) stay in Home Assistant (a unit in heat/cool or auto lights no mode button). With a blueprint older than the firmware, the popup stays as before (16-30 °C, steps of 0.5, every button).
- **A shutter followed by `volet_serre_tracking.yaml`** (it doesn't report its travel): keep it in the « Volet » input of the 3.x section too, even if it is in a room; its tile then shows the state the package keeps, and its commands go through the package's script.
- **One calendar per role**: work, appointments, birthdays, public holidays and school holidays are the five « Tab5 · agenda … » lists.
- **Spoken morning briefing**: in the screen's language (French, English, German, Dutch, Spanish, Italian, Turkish); only the French text has been reviewed.
- To see a smaller home without touching yours: [demo mode](demo_mode.md#minimal-home-optional-zones), option `--maison-minimale`.

---

---

## Version Française

---

> **Envie de tester d'abord ?** [`docs/demo_mode.md`](demo_mode.md) montre le tableau de bord complet sur un appareil flashé en quelques minutes, sans aucune installation Home Assistant. Revenez ici quand vous êtes prêt pour l'installation réelle.

## Sans compiler (page d'installation)

Depuis la 3.0, un firmware prêt à l'emploi et signé s'installe depuis le navigateur. Dans cet ordre :

1. **Home Assistant d'abord** : l'archive `tab5_home_assistant.zip` de l'[étape 4](#étape-4--installer-les-packages-home-assistant), décompressée dans le dossier `config/` de Home Assistant, une ligne dans `configuration.yaml`, puis vos sources choisies dans Home Assistant à la souris. Ni dépôt, ni Python, rien à remplir.
2. **Flasher** depuis la [page d'installation](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) (Chrome ou Edge, un câble USB-C qui transmet les données) : votre révision d'écran, le canal Stable, *Connecter et installer*. Sur une tablette neuve, acceptez de l'effacer. Dans la liste des ports, la tablette s'appelle « USB JTAG/serial debug unit » ; si plusieurs ports portent ce nom, débranchez la tablette pour voir lequel disparaît.
   - « Failed to initialize… holding the BOOT button » : le Tab5 n'a pas de bouton BOOT. Maintenez son bouton reset environ 2 s, jusqu'à ce que la LED verte interne clignote vite (mode téléchargement), recommencez, puis un appui court sur reset à la fin pour la redémarrer.
   - La tablette a déjà la même version : la page n'affiche pas *Install*. Pour repartir de zéro, « Erase User Data » (en rouge, en bas) efface tout, Wi-Fi, clé et réglages compris, puis réinstalle.
3. **Wi-Fi** : depuis la même fenêtre (*Connect to Wi-Fi*, par l'USB), ou avec un téléphone sur le réseau ouvert « Tab5 Fallback AP ».
4. **L'ajouter à Home Assistant** dans les 30 minutes qui suivent son démarrage : *Paramètres → Appareils et services*, l'appareil ESPHome est découvert, *Configurer*. Home Assistant lui donne sa clé. Une tablette que Home Assistant connaît déjà reçoit une nouvelle clé toute seule, rien à confirmer (vérifié le 28/09/2026). Rien d'autre à autoriser : la tablette demande tout à Home Assistant par des événements ([ADR-0025](decisions/0025-events-only.md)).
   - Firmware 3.1 ou plus ancien seulement : cochez aussi « Autoriser l'appareil à effectuer des actions Home Assistant » (*ESPHome → Configurer*), dont la voix, le calendrier et le réveil ont besoin sur ces versions.
5. **Vos appareils** : créez l'automatisation depuis le blueprint ([étape 4](#étape-4--installer-les-packages-home-assistant), point 5).

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

**Langue de l'écran :** le français par défaut ; ajoutez `tab5_langue: English` (ou `Deutsch`, `Nederlands`, `Español`, `Italiano`, `Türkçe`) pour une autre langue au premier démarrage. Elle se change ensuite depuis Home Assistant (select « Langue »), voir [traductions](translations.md#version-française).

**Fuseau horaire :** rien à régler depuis la 3.0. La tablette prend celui de Home Assistant et garde le dernier reçu : le réveil reste juste quand HA manque après une coupure de courant. Une ancienne ligne `tab5_fuseau` est ignorée.

**Révision du Tab5 :** si la puce écran de votre autocollant n'est pas la ST7123, ajoutez `tab5_ecran: st7121` ou `tab5_ecran: ili9881c` dans ce fichier (voir [Révisions matérielles](hardware.md#révisions-matérielles)). Pour la ST7123, ne mettez rien.

`Tab5/user_entities.yaml` est gitignoré (ne jamais le committer). Pour une installation standard, rien n'y est à remplacer : toutes les lignes sont facultatives. **Vos appareils (lumières, clim, plantes, TV…) ne se règlent plus ici (depuis la 3.0)** : vous les choisissez dans Home Assistant, à la souris, voir l'étape 4 et [Adapter à sa maison](#adapter-à-sa-maison) ; les anciennes clés `entity_light_…` d'un fichier existant sont simplement ignorées. Le point d'entrée `tab5-ha-hmi.yaml` les charge via `substitutions: !include Tab5/user_entities.yaml`. Il ne reste aucune clé d'entité : Home Assistant retrouve seul les entités de la tablette (satellite vocal, lecteur média, select de pipeline) et celles du bouton « MAJ Écran » ([ADR-0025](decisions/0025-events-only.md)) ; d'anciennes lignes `entity_tab5_…`, `entity_primary_active` et `entity_push_automation` sont ignorées.

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

Tout le côté Home Assistant tient dans une archive, **`tab5_home_assistant.zip`**, jointe aux [releases](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases) (depuis la 3.2.0). **Rien à remplir** : chaque valeur de votre maison se choisit ensuite dans Home Assistant, à la souris ([ADR-0024](decisions/0024-packages-without-placeholders.md)).

1. **Téléchargez** `tab5_home_assistant.zip` depuis la dernière release et **décompressez-la dans le dossier `config/` de Home Assistant**, celui de `configuration.yaml` (partage Samba, ou module File editor / Studio Code Server). Elle y ajoute `packages/`, `custom_templates/` et `blueprints/automation/tab5/` ; le dossier `tab5_optionnel/` n'est pas chargé (voir plus bas).
2. **Une ligne dans `configuration.yaml` : c'est la seule ligne de YAML à écrire** (rien à faire si elle y est déjà) :
   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```
   Si `homeassistant:` existe déjà, ajoutez seulement la ligne `packages:` dessous.
3. *Outils de développement → YAML → Vérifier la configuration*, puis **redémarrez** Home Assistant. Plus tard, remplacer ces mêmes fichiers demande seulement *Outils de développement → YAML → Toute la configuration YAML*.
4. **Choisissez vos sources** : *Paramètres → Appareils et services → Entités*, cherchez **« Tab5 · »**, ouvrez une liste et choisissez. Chaque liste propose ce que votre Home Assistant possède :

   | Liste | Ce qu'elle règle | Choix par défaut |
   |---|---|---|
   | Tab5 · source des prévisions | prévisions et météo du moment (n'importe quelle entité `weather.*`) | la ville Météo-France, sinon la première entité météo |
   | Tab5 · source de la pluie dans l'heure | carte pluie : Météo-France, OpenWeatherMap, Buienradar, DWD, Met.no, Open-Meteo ou Aucune | Météo-France ; sans l'intégration Météo-France, Open-Meteo prend le relais |
   | Tab5 · source des vigilances | icônes de vigilance : Météo-France, MeteoAlarm, DWD, CAP Alerts ou Aucune | Météo-France ; sans l'intégration Météo-France, aucune icône (comme Aucune) |
   | Tab5 · agenda de travail | événements de travail : planning, jours de repos et **heure du réveil** (lesquels : voir le mot plus bas) | rien |
   | Tab5 · agenda des rendez-vous | popup calendrier, rappels de rendez-vous, briefing du matin | rien |
   | Tab5 · agenda des anniversaires | anniversaires du popup calendrier | le seul agenda nommé « anniversaires » (ou birthday…) |
   | Tab5 · agenda des jours fériés | jours fériés du popup calendrier | le seul agenda de jours fériés (intégration Jours fériés, ou son nom le dit) |
   | Tab5 · agenda des vacances scolaires | vacances scolaires du popup calendrier (tous les événements de cet agenda) | le seul agenda dont le nom le dit (« calendrier scolaire », « vacances scolaires », school…) |
   | Tab5 · téléphone | écran allumé à votre retour, éteint à votre départ | le seul téléphone de l'application mobile |
   | Tab5 · capteur de présence | écran allumé à la présence, éteint après 15 min sans | rien |
   | Tab5 · TV Samsung, Tab5 · adresse de la TV | boutons d'applications du popup TV (Samsung Tizen) | la seule TV Samsung Smart TV ; l'adresse donnée par un suivi du routeur s'il la connaît, sinon tapez son IP |
   | Tab5 · pipeline de discussion | le pipeline Assist du mode « Discu » de l'écran | rien : les boutons Domo / Discu sont alors masqués |

   Chaque liste porte un nom en deux langues, « français · english » (« Tab5 · agenda de travail · work calendar »), et garde son identifiant d'entité. Laissée sur « Aucun », une fonction reste simplement éteinte, sans erreur.

   - **Quels événements sont du travail** : le texte « Tab5 · mot des événements de travail · work event keyword » contient les mots qui font d'un événement de l'agenda de travail un poste, séparés par des virgules, sans tenir compte des majuscules, cherchés dans le titre. **Vide = tous les événements de l'agenda de travail**, pour un agenda qui ne contient que vos postes. L'agenda de travail de l'auteur contient aussi ses rendez-vous : il y tape `Travail`.
   - **Vacances scolaires** : n'importe quel agenda. En France, le ministère publie un fichier ICS par zone : *Paramètres → Appareils et services → Ajouter une intégration → Remote Calendar*, nom « Calendrier scolaire » (la liste le prend alors seule), URL `https://fr.ftp.opendatasoft.com/openscol/fr-en-calendrier-scolaire/Zone-A.ics` (`Zone-B.ics`, `Zone-C.ics` pour les autres zones ; vérifié le 29/09/2026, jusqu'à l'été 2028). Les entités des fournisseurs météo (capteurs de pluie et de vigilance Météo-France, OpenWeatherMap, MeteoAlarm, DWD, CAP Alerts) et celles de la tablette (écran, réveil, micro…) sont trouvées seules ; la tablette par le modèle de son appareil, quel que soit le nom que vous lui avez donné.
5. **Choisissez vos appareils** : *Paramètres → Automatisations et scènes → Blueprints*, « Tab5 — emplacements de l'écran · screen slots » (décompressé avec le reste ; ou *Importer un blueprint* avec
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`),
   puis *Créer une automatisation* et choisissez une entité pour chaque emplacement (tous facultatifs) ; ses libellés sont en français et en anglais. Une automatisation par tablette. Son « Agenda de travail » peut rester vide : il prend alors celui de « Tab5 · agenda de travail ».

`tab5_optionnel/volet_serre_tracking.yaml` ne sert qu'à un volet qui ne signale ni sa position ni sa course (le moteur Tuya de l'auteur) : copiez-le dans `packages/`, rechargez, et choisissez le volet dans « Tab5 · volet à course simulée ». Il n'est pas installé par défaut car, une fois présent, il prend au blueprint les boutons du volet.

Les mêmes fichiers sont dans le dépôt (`HomeAssistant_Config/packages/`, `custom_templates/`, `blueprints/`, `optionnel/`) ; voir [`HomeAssistant_Config/README.md`](../HomeAssistant_Config/README.md#version-française) pour le rôle de chaque package.

> Ces packages sont exactement ce qui tourne sur le Home Assistant de l'auteur depuis le 26/09/2026, sans aucune retouche propre à sa maison depuis le 28/09/2026. Il n'y a pas de version privée, ni rien à fusionner dans `automations.yaml` ou `scripts.yaml`.

**Vous aviez les packages à placeholders (3.1.0 et avant) ?** Remplacez les fichiers, puis réglez chaque liste sur votre ancienne valeur : `VOTRE_EMAIL_gmail_com` → « Tab5 · agenda de travail », `calendar.famille` → rendez-vous, `calendar.anniversaires` et l'agenda des jours fériés (en général choisis par défaut), `VOTRE_TELEPHONE` → téléphone, `VOTRE_CAPTEUR_PRESENCE` → capteur de présence, `VOTRE_TV` → TV Samsung, et l'IP de la ligne `tab5_tv_app_url` de `secrets.yaml` → « Tab5 · adresse de la TV » (la ligne peut ensuite partir). Le choix météo est gardé. Gardez `volet_serre_tracking.yaml` dans `config/packages/` si vous l'utilisez, et choisissez votre volet dans sa liste. **Réglez « Tab5 · agenda de travail » avant que les nouvelles automatisations tournent** : rechargez d'abord *Entrées de texte* et *Entités de template*, choisissez, puis rechargez *Scripts*, *Automatisations* et *Commandes REST*. Tant qu'il n'est pas choisi, tous les jours comptent comme des jours de repos, et le réveil suit.

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
- Rien d'autre à autoriser. La tablette n'appelle jamais d'action de Home Assistant : elle envoie des événements, que `packages/tab5_evenements.yaml` traduit en une liste fixe d'actions, pour un Tab5 seulement ([ADR-0025](decisions/0025-events-only.md)). L'option « Autoriser l'appareil à effectuer des actions Home Assistant » reste décochée (un firmware 3.1 ou plus ancien en a encore besoin, voir [Passer d'une 3.1 à la suite](#passer-dune-31-à-la-suite)).

---

## Mises à jour OTA

Une fois la tablette sur votre réseau, les compilations suivantes passent par le Wi-Fi :

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

L'envoi n'est plus chiffré (il n'y a pas de clé dans le YAML) ; la tablette vérifie la signature et refuse un firmware signé par une autre clé. Vos compilations sont signées par votre clé, rien d'autre à faire.

Une tablette installée depuis le [flasheur web](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) reçoit plutôt ses mises à jour par Home Assistant : son entité « Firmware » lit le manifeste publié toutes les 6 h, et « Installer » télécharge l'image, que la tablette vérifie avec la clé du projet ([ADR-0022](decisions/0022-published-firmware-pages-channels.md)). Un firmware compilé soi-même n'a pas cette entité.

**Les fichiers Home Assistant ne se mettent pas à jour seuls** : remplacez-les par ceux de `tab5_home_assistant.zip` de la même release (étape 4). Les fichiers d'une archive connaissent leur version : quand la tablette tourne une release majeure ou mineure plus récente (X.Y ; une version corrective seule ne compte pas), Home Assistant le dit (notification « Tab5 : fichiers Home Assistant à mettre à jour », capteur « Tab5 · fichiers HA en retard »). Des fichiers copiés depuis le dépôt n'ont pas de version et ne sont jamais comparés.

**Journaux :** `esphome logs` cherche la clé dans le YAML et n'en trouve plus. Utilisez `python tools/tab5_logs.py --host 192.168.x.x --config-ha \\<ip-de-ha>\config` : il lit la clé que garde Home Assistant (`.storage/core.config_entries`, ou la variable `TAB5_CLE_API`) et ne l'affiche jamais.

---

## Passer d'une 3.2 à la suite

Remplacez les fichiers Home Assistant par ceux du nouveau `tab5_home_assistant.zip` (étape 4), puis vérifiez trois listes :

1. **Votre agenda de travail contient d'autres événements ?** Tapez votre mot dans « Tab5 · mot des événements de travail » (l'auteur : `Travail`). Jusqu'à la 3.2, seuls les titres contenant « Travail » comptaient ; laissé vide, tout l'agenda de travail compte désormais comme du travail.
2. **Les vacances scolaires** ne viennent plus d'une table de la zone A : choisissez un agenda dans « Tab5 · agenda des vacances scolaires » (étape 4).
3. **Mode Discussion** : choisissez son pipeline dans « Tab5 · pipeline de discussion ». Jusqu'à la 3.2, la tablette demandait un pipeline nommé exactement « Discussion LLM ».

Les listes gardent leurs identifiants d'entité : vos tableaux de bord et automatisations n'ont rien à changer.

## Passer d'une 3.1 à la suite

Après la 3.1, la tablette n'appelle plus d'action de Home Assistant : elle envoie des événements, que le nouveau package `packages/tab5_evenements.yaml` traduit en actions ([ADR-0025](decisions/0025-events-only.md)). Dans cet ordre :

1. **Les fichiers Home Assistant d'abord** : remplacez-les par ceux de `tab5_home_assistant.zip` (étape 4), qui apportent `tab5_evenements.yaml`, et réglez les listes « Tab5 · … » comme le dit « Vous aviez les packages à placeholders » à la fin de l'étape 4. Une tablette en 3.1 n'envoie aucun de ces événements : le package attend, rien ne change.
2. **Puis le firmware** (entité « Firmware », ou votre propre compilation).
3. **Puis décochez** « Autoriser l'appareil à effectuer des actions Home Assistant » (*ESPHome → Configurer*) : la tablette n'en a plus besoin, et sans elle Home Assistant refuse toute action que l'appareil demanderait.

Un firmware récent **sans** le package ne plante pas et n'écrit rien au journal, mais ce qu'il demande à Home Assistant se perd : le popup calendrier n'affiche que sa grille locale (ni horaires, ni fériés, ni rendez-vous ; un jour touché reste sur « Chargement... »), rien n'est dit (rendez-vous, briefing du réveil, « Volet arrêté »), les boutons Domotique / Discussion ne changent plus le pipeline, une alerte touchée n'est masquée que sur la tablette, jusqu'à son prochain redémarrage, et « MAJ Écran », « Recharger autos » et « Redémarrer HA » ne font rien.

---

## Passer à la 3.0

La 3.0 change la façon dont la tablette est protégée ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)). Une fois, sur place (les étapes 3 et 4 ci-dessous doivent tenir dans les 30 minutes qui suivent le démarrage de la tablette) :

1. **Avant de flasher**, préparez Home Assistant pour la 3.0 : le blueprint de l'étape 4, point 5 (la tablette ne connaît plus vos entités, [ADR-0019](decisions/0019-logical-slots-blueprint.md)).
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

Les **prévisions et la météo du moment** (pages horaires et journalières, goutte d'humidité) viennent de n'importe quelle entité `weather.*`, choisie dans Home Assistant avec la liste « Tab5 · source des prévisions », qui propose les entités météo présentes (par défaut la ville Météo-France, sinon la première entité météo). Le Tab5 ne demande à chaque entité que ce qu'elle déclare : les prévisions journalières, sinon les demi-journées (NWS) ou les horaires (OpenWeatherMap gratuit) regroupées par date ; sans prévisions horaires (Buienradar), la page horaire reste vide. Les heures sont affichées en heure locale.

La **pluie dans l'heure** et les **vigilances** sont facultatives. Leur source se choisit **dans Home Assistant**, avec les deux listes de `packages/tab5_meteo_sources.yaml`, sans toucher au YAML :

| Carte | Liste « Tab5 · source … » | Ce qu'il faut |
|---|---|---|
| Pluie dans l'heure (barres et phrase) | Météo-France | l'intégration Météo-France (France) : `sensor.<ville>_next_rain` |
| | OpenWeatherMap | l'intégration OpenWeatherMap en mode **v3.0**, qui demande l'abonnement One Call (1 000 appels par jour gratuits ; HA interroge toutes les 10 min). Trouvée seule : l'entité météo choisie si elle est d'OpenWeatherMap, sinon sa première |
| | Buienradar | rien à installer, sans clé : radar de pluie des Pays-Bas et de la Belgique (aussi le Luxembourg et les bords de la France et de l'Allemagne), toutes les 5 min. Usage non commercial, source à citer |
| | DWD | rien à installer, sans clé : composite radar du DWD par [Bright Sky](https://brightsky.dev), Allemagne et pays voisins, toutes les 5 min |
| | Met.no | rien à installer, sans clé : radar Nowcast des pays nordiques (Norvège, Suède, Finlande, Danemark), toutes les 5 min |
| | Open-Meteo | rien à installer, sans clé, partout, mais un **modèle météo** et non un radar, au pas de 15 min (vrai pas de 15 min en Europe centrale et en Amérique du Nord, interpolé ailleurs). Usage non commercial |
| | Aucune | la carte pluie est masquée |
| Vigilances | Météo-France | `sensor.<département>_weather_alert`, trouvé seul (le département de la ville Météo-France) |
| | MeteoAlarm | l'intégration MeteoAlarm (en YAML seulement, 39 pays européens, une alerte à la fois), trouvée seule (son capteur « Information provided by MeteoAlarm ») |
| | DWD | Allemagne : l'intégration *Deutscher Wetterdienst (DWD) Weather Warnings* (fournie avec HA, réglée dans l'interface avec le nom ou le numéro de votre cellule d'alerte DWD, ou un suivi d'appareil). Toutes les alertes de la région, pas les préavis (« Vorabinformation ») |
| | CAP Alerts | l'intégration [CAP Alerts](https://github.com/seevee/cap_alerts) (HACS, dépôt personnalisé), une entité par alerte : MeteoAlarm (Europe, toutes les alertes de votre région), NWS (États-Unis), Environnement Canada, et une centaine de services nationaux par l'OMM |
| | Aucune | pas d'icônes de vigilance |

Buienradar, DWD, Met.no et Open-Meteo sont interrogés par Home Assistant lui-même (`rest_command.tab5_pluie` du package), toutes les 5 min et seulement quand ils sont choisis, aux coordonnées du domicile (`zone.home`) arrondies à 0,01° (environ 1 km). Hors de leur zone, ils répondent « pas de données ». Open-Meteo sert aussi quand la liste de la pluie est restée sur Météo-France (son choix par défaut) et que Home Assistant n'a pas de capteur de pluie Météo-France, par exemple hors de France : la carte pluie marche alors sans rien régler. Choisissez « Aucune » pour ne rien envoyer ; l'intégration Météo-France ajoutée plus tard reprend la main toute seule.

Avec DWD et CAP Alerts, une alerte compte tant qu'elle est en cours ou si elle commence dans les 24 h ; le niveau global est la plus forte. Les deux sont lues par `custom_templates/tab5_vigilance.jinja`, fourni dans l'archive (Météo-France et MeteoAlarm marchent sans lui). La case du phénomène vient du code du DWD, du type de phénomène de MeteoAlarm ou de l'icône que CAP Alerts donne à l'alerte ; un phénomène sans case (sécheresse, qualité de l'air…) ne fait que monter le niveau global.

Limites, en toute franchise :
- OpenWeatherMap a été essayé sur l'installation de l'auteur le 27/09/2026 (prévisions et pluie dans l'heure, un jour sec) ; ses prévisions journalières couvrent 8 jours, les derniers jours des pages de 15 jours restent donc vides. MeteoAlarm et le regroupement des demi-journées (NWS) n'ont été testés qu'avec des données simulées.
- Buienradar, DWD, Met.no et Open-Meteo ont été interrogés le 29/09/2026 (Amsterdam, Berlin, Oslo, Landes ; un jour sec) et leurs réponses lues par les mêmes modèles dans le moteur de Home Assistant ; la pluie elle-même a été simulée. Open-Meteo est aussi interrogé par la CI d'installation à neuf.
- Le DWD a été ajouté au Home Assistant de l'auteur le 29/09/2026 (Berlin, un jour sans alerte) : entités trouvées, dates lues. Les alertes elles-mêmes, et tout CAP Alerts, n'ont été testés qu'avec des données simulées, dans le moteur de modèles de Home Assistant et dans la CI d'installation à neuf.
- La probabilité de gel n'existe que chez Météo-France. L'icône flocon lit `sensor.<ville>_snow_chance` quand il existe ; sinon, elle suit la condition du moment (neige).

---

## Adapter à sa maison

L'écran a d'abord été dessiné autour de la maison de l'auteur. Tout se choisit dans Home Assistant, dans l'automatisation « Tab5 — emplacements » (le blueprint de l'étape 4) : changer d'appareil se fait dans l'interface de HA, ni flash ni redémarrage.

### Pièces (firmware 3.2 et plus)

Les cinq tuiles du bas de l'écran sont des **pièces** que vous remplissez vous-même ([ADR-0023](decisions/0023-rooms-generic-tiles.md)) :

- **Jusqu'à 5 pièces de 5 appareils**, une par page de la rangée du bas. La pièce 1 est l'accueil (aujourd'hui à J+4) ; les pièces 2 et 3 sont à un et deux glissements vers la gauche (J+5 à J+9, J+10 à J+14) ; les pièces 4 et 5 à un et deux glissements vers la droite (prochaines heures). Dans chaque pièce, choisissez les appareils dans l'ordre des tuiles, de gauche à droite (ils se déplacent à la souris) ; seuls les cinq premiers servent. Un appareil peut être dans plusieurs pièces.
- **Ce qui trouve place sur une tuile** : lumières ; interrupteurs, ventilateurs, humidificateurs, entrées booléennes et automatisations ; volets et vannes ; lecteurs multimédia ; scènes, scripts et boutons ; capteurs et nombres (affichés, pas commandés) ; capteurs binaires, personnes, suivis de présence et serrures (affichés) ; climatisation — une tuile de clim montre la température de la pièce, et un appui ouvre le popup clim pour cet appareil (celui de l'entrée « Climatisation » garde la carte − / consigne / + de l'accueil).
- **Noms et icônes viennent de Home Assistant.** Une pièce prend le nom que vous saisissez, sinon l'aire que partagent ses appareils, sinon « Pièce n ». Une tuile prend le nom de l'entité sans celui de la pièce (« Lampe du salon » dans « Salon » devient « Lampe »), et l'icône choisie dans les réglages de l'entité, sinon celle de son genre.
- **Personnaliser une tuile** (section repliée « Personnaliser des tuiles ») : un autre nom, une autre icône, ou un comportement — *allumer seulement* (jamais éteint depuis l'écran), *confirmer* (un second appui dans les 3 s), *lecture seule*.
- Le bouton « HA » montre la pièce de la page affichée ; un glissement passe à la pièce suivante qui a des appareils.
- La valeur d'un capteur part avec les autres mesures, toutes les 5 minutes ; les autres appareils partent dès que ce que montre l'écran change.
- **Pièce 1 laissée vide** : l'accueil garde le réglage 3.x (section repliée « Tuiles de l'accueil (réglage 3.x) » : PC ou TV, volet, trois lumières), avec le comportement PC + TV de la tuile PC. Rien à refaire après la mise à jour.
- **Firmware 3.0 ou 3.1** : le blueprint lit la version de la tablette et n'utilise alors que le réglage 3.x ; les pièces apparaissent une fois le firmware mis à jour. Firmware et blueprint se mettent à jour dans n'importe quel ordre.

### Autres zones

**Ce que vous n'avez pas disparaît**, avec ses boutons ([ADR-0018](decisions/0018-optional-zones-confirmed-by-ha.md), [ADR-0019](decisions/0019-logical-slots-blueprint.md)).

- **Retirer une zone : laissez son emplacement vide.** Un emplacement vide, ou une entité qui n'existe pas, est absent. Une entité qui existe mais est `unavailable` garde sa zone (« -- », « Hors ligne »). **Sans l'automatisation du blueprint, rien ne disparaît** (et aucun de vos appareils ne s'affiche).
- **Une zone manque par erreur ?** Le capteur de diagnostic « Zones masquées » de la tablette liste ce qui a disparu.
- Une zone revient d'elle-même dès que son entité envoie une valeur.

| Zone | Entrée du blueprint | Masqué quand elle est vide |
|---|---|---|
| TV | TV, et Télécommande de la TV pour les touches | Bouton TV et télécommande ; « HA » et « Sys » glissent d'une colonne |
| Téléphone | Batterie du téléphone | Icône d'état |
| Pièce | Température de la pièce (et Humidité de la pièce) | Sa température |
| Serre | Seconde température (serre) | Sa température ; l'icône devient une manette, l'entrée de l'arcade reste |
| Pots (0 à 5) | Pot 1 à 5 : le capteur d'humidité ; conductivité, éclairement, température et batterie sont pris sur le même appareil | Jusqu'à 4 pots : un emplacement chacun ; à 5 : le résumé « plus secs / médiane / plus humide ». Cartes du popup, recentrées |
| Clim | Climatisation | − / consigne / + et le popup |
| Planning de travail | Agenda de travail | Panneau planning de la carte centrale |
| Mode vocal « Discussion » | aucune : la liste « Tab5 · pipeline de discussion » à « Aucun » (sans la liste, la zone reste) | Boutons Domo / Discu de l'accueil et du popup assistant ; la tablette repasse en Domo |
| Lumières (réglage 3.x) | Lumière 1 à 3 | Icônes des tuiles 3 à 5, carte du calque « HA », sélecteur du popup lumière, « Tout éteindre » |
| PC (réglage 3.x) | PC (un interrupteur l'allume ; un suivi de présence l'affiche seulement) | Icône d'état, carte « PC Bureau » ; la première tuile aussi s'il n'y a pas non plus de TV |
| Volet (réglage 3.x) | Volet (et le package optionnel `volet_serre_tracking.yaml`, `tab5_optionnel/` de l'archive, pour un volet qui ne signale pas sa course) | Icônes de la tuile 2, carte du calque « HA » |

Les lignes « réglage 3.x » sont l'accueil d'un firmware 3.0 ou 3.1, et d'un firmware 3.2 tant que la pièce 1 est vide. Les horaires du planning et l'heure du réveil viennent de la liste « Tab5 · agenda de travail » (étape 4) ; laissez vide l'« Agenda de travail » du blueprint et il prend le même.

Limites :
- **Icônes** : l'écran en connaît une palette limitée ([icônes des tuiles](tiles_icons.md#version-française)). Une icône hors palette montre celle de son genre ; en ajouter une demande une ligne dans `Tab5/tuiles_icones.yaml` et une nouvelle version.
- **Noms** : les polices de l'écran ne couvrent que les alphabets latins ; les autres caractères disparaissent, et un nom trop long est coupé.
- **Renommer une entité** : sa tuile suit à la prochaine connexion de la tablette, ou dès que l'automatisation est de nouveau enregistrée.
- **Clim** ([ADR-0026](decisions/0026-climate-from-device.md)) : toutes marques. Le blueprint envoie les bornes, le pas, l'unité (°C ou °F, celle de votre entité météo) et les modes de l'appareil ; le popup prend son nom pour titre, et un bouton que l'appareil ne sait pas faire disparaît. **Plusieurs appareils** ([ADR-0027](decisions/0027-climate-per-tile.md)) : placez chacun dans une pièce ; sa tuile ouvre le popup pour lui, avec ses bornes, ses modes et son nom. Un changement fait hors de l'écran (télécommande, application) se voit tout de suite dans le popup pour la consigne et le mode, en 5 minutes au plus pour la ventilation, l'oscillation, le préréglage et la température de la pièce. Avec un firmware plus ancien que le blueprint, une telle tuile montre seulement sa température. L'écran n'a de boutons que pour froid, chaud, sec, ventilation et arrêt, Éco, Boost, Silence, Oscillation et Brise : les autres modes (chaud/froid, auto, vitesses de ventilation, nuit…) restent dans Home Assistant (un appareil en chaud/froid ou auto n'allume aucun bouton de mode). Avec un blueprint plus ancien que le firmware, le popup reste comme avant (16-30 °C, pas de 0,5, tous les boutons).
- **Un volet suivi par `volet_serre_tracking.yaml`** (il ne signale pas sa course) : laissez-le aussi dans l'entrée « Volet » de la section 3.x, même s'il est dans une pièce ; sa tuile montre alors l'état que tient le package, et ses commandes passent par le script du package.
- **Un agenda par rôle** : travail, rendez-vous, anniversaires, jours fériés et vacances scolaires sont les cinq listes « Tab5 · agenda … ».
- **Briefing parlé du matin** : dans la langue de l'écran (français, anglais, allemand, néerlandais, espagnol, italien, turc) ; seul le texte français a été relu.
- Pour voir une maison plus petite sans toucher à la vôtre : [mode démo](demo_mode.md#maison-minimale-zones-optionnelles), option `--maison-minimale`.
