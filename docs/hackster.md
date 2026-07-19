# Hackster.io / M5Stack Global Innovation Contest 2026 — Project Draft

> **How to use this file:** Copy sections into the Hackster.io project editor. Primary language is **English** (jury-facing). The structure mirrors a full Hackster tutorial (story → numbered steps → schematics → code), with `📷` markers where images/galleries should be uploaded.
> **Repos:** [M5-Tab5-ESPHome-LVGL](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL) · [vromvrom-engine](https://github.com/Axellum/vromvrom-engine)
> **Firmware release:** [v1.0.5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v1.0.5) (July 2026)
> **Demo video:** https://www.youtube.com/watch?v=ygNhgtMffu4
> **⚠️ TODO before publishing:** re-shoot photos of the **climate popup v2**, **light popup v2**, **TV remote** and **console v2** (current `tab5_photo_climate_popup.jpg` shows the old 3×3 popup, photos date from 06/07).

---

## Project header (Hackster metadata)

- **Title:** Tab5 Voice HMI — Push-Only Home Assistant Dashboard on ESP32-P4
- **Subtitle:** A 60 FPS native LVGL dashboard + local voice satellite on the M5Stack Tab5 V2 — every line of code written by AI, steered by a human.
- **Difficulty:** Intermediate
- **Type:** Full instructions provided
- **Time:** ~4 hours (dashboard) · a weekend (voice + engine)
- **License:** MIT

### Things used in this project

**Hardware components**

| Item | Qty | Note |
|------|-----|------|
| M5Stack Tab5 V2 (ESP32-P4) | ×1 | The star — **contest-eligible M5Stack controller**. 5" 1280×720 MIPI-DSI touch, ES8388/ES7210 audio, ESP32-C6 co-processor. No soldering, no 3D printing: it's a finished device |
| Home Assistant server | ×1 | Any: Raspberry Pi, VM, NUC… |
| Zigbee coordinator (e.g. SLZB-06MU) | ×1 | Optional — lights, covers, sensors |
| BLE soil moisture sensors | ×1–5 | Optional — plant cards |
| Daikin (or any HA `climate` entity) | ×1 | Optional — climate popup |
| Samsung TV (HA integration) | ×1 | Optional — TV remote popup |
| Steam Deck / any LAN Linux box | ×1 | Optional — hosts the multi-agent engine for Discussion mode |

**Software apps and online services**

- [ESPHome](https://esphome.io) ≥ 2025.9.3 (firmware framework)
- [LVGL](https://lvgl.io) 8.4 (UI, compiled into the firmware)
- [Home Assistant](https://www.home-assistant.io) + Wyoming Whisper (STT) + Wyoming Piper (TTS)
- [vromvrom-engine](https://github.com/Axellum/vromvrom-engine) (optional, multi-agent voice routing)

**Hand tools and fabrication machines**

- None. A USB-C cable for the first flash — that's the whole workshop.

---

## Story (EN — paste as main text)

A few months ago, I kept hearing that AI could now write real code. I wanted to see it for myself — not on a toy example, but on something I'd actually live with every day. My old Nextion weather screen (ESPHome + Météo-France) was showing its age, so I set the challenge: replace it with a **full Home Assistant control panel** on an **M5Stack Tab5 V2**, and let AI write **every single line** — firmware, automations, even most of this documentation. My role: set goals, test on the real device, reject bad ideas, and steer.

### Why a screen, though?

After five years with the Nextion, I wanted to give my mostly-weather screen a facelift, keeping at least the same core goals:

- Be a clock.
- Warn me at a glance if rain is coming within the hour — do I leave 15 minutes early so I don't show up soaked at the office? Do I grab an umbrella?
- Show the forecast for the next few days, so I have a conversation topic for the rare occasions I decide to be social.
- Do all that on reasonable power, always on (well, whenever I'm actually in front of it), with total freedom over layout, design and logic — not just the stock Home Assistant dashboard — with all the upsides that come with it... and the downsides.

Then home-automation ambitions crept in:

- A direct readout of the soil moisture in my plant pots / veggie patch.
- Turn the TV and PC on without lifting my butt off the chair.
- Control the three living-room spotlights and the bedroom light.

Then, little by little...

- Control my AC.
- Operate the roller shutter — again, without getting up.
- Add "Ok Nabu" wake-word support, so I don't even have to lean over to grab the screen anymore 🙂
- Polish the voice assistant: a conversation mode, a choice of LLM (local or not), and home commands that are as fast as possible and actually understand me.
- Display my work schedule so I can read my shift hours at a glance.
- Get a network remote for my TV — it can always come in handy. (Funny story: Claude Fable 5 completely blew me away on that one — it built the whole thing in two prompts flat. Naturally, I then let it loose redoing *every single popup* in the project… at the cost of roughly 50% of my 5-hour usage cap per popup, on the Pro plan. Worth it.)

All of that with these design goals in mind. My very first sessions with Gemini, rewriting the old Nextion code, were honestly humbling — it rethought how the data got sent and cut the codebase to a third of its size. So this time I wanted lightness and much better optimization than anything I'd hand-rolled myself:

- No images anywhere — as light and optimized for the tablet as it gets.
- Data pushes on the Home Assistant side as gentle as possible (my HA runs on a Freebox box, so I have to stay lean).
- A fast boot with no display lag — something that just *feels* smooth.

I also aimed for a modern-feeling interface (I'm in my fifties — don't expect miracles): no separate pages, just popups, everything reachable via a button, a long-press, or a swipe. Heavy color-coding gives at-a-glance readability from a few meters away, even though the screen is really meant to be read from under a meter for the fine print. And I tried to pack the maximum info/controls onto something that stays reasonably clean — yes, I know, "clean" is the part I'm worst at. End goal: a screen that looks decent to the eye, even though deep down I'm way more about function than form.

Today the Tab5 sits on my desk and does all of this:

- A **weather station** — 15-day and hourly Météo-France forecasts you flick through with a swipe, vigilance alerts recoloring the date, a 1-hour rain graph.
- A **home control panel** — lights, roller shutter, climate, TV remote, plant moisture — with full-screen "glass" popups.
- A **rotating info card** — my work planning, rain, weather alerts, and up to 4 live Home Assistant alert banners you dismiss with a tap.
- A **local voice satellite** — "Okay Nabu" wake word running *on the device*, local STT/TTS, and a second on-device wake word ("Stop") that halts the shutter while it moves.
- An optional **multi-agent engine** for real conversations — weather, calendar, web questions — routed locally first.

Don't believe a compiled C++ dashboard can feel this alive? Watch the demo 👇

📷 **[YouTube embed — https://www.youtube.com/watch?v=ygNhgtMffu4]**

📷 **[GIF — `m5stack_tab5_demo.gif` — animated UI overview]**

Wanna find out how it works? Read along. Spoiler: this is **not** a web dashboard in a kiosk browser — there is no browser, no polling, and (from my side) not a single hand-typed line of code.

---

### The big idea: push, don't poll

Most DIY wall panels run a browser, reload dashboards, and hammer the server with requests. This project turns that upside down:

- The UI is **LVGL 8.4 compiled into the ESP32-P4 firmware**. Layout is computed once at boot; updates only redraw the label that changed. It renders at 60 FPS from a ~1.8 MB framebuffer in PSRAM, over a MIPI-DSI bus with DMA.
- The Tab5 **never asks Home Assistant for anything**. HA automations detect changes and **push** data to the device through 12 native ESPHome services (`tab5_maj_*`). When nothing changes, the network is silent and the CPU is nearly idle.
- Complex payloads (a 15-day forecast!) are **packed into one delimited string** on the HA side and parsed by C++ on the device — one network call instead of dozens.
- If Wi-Fi drops or HA restarts, the screen just keeps showing the last known state. No spinner, no blank page, ever.

📷 **[Image — `push_only_architecture_diagram.png` — the push-only data flow]**

### What you need to prepare

**Minimum demo:** an M5Stack Tab5 V2, a Wi-Fi network, and a PC with ESPHome for the first USB flash. That's it — there's even a demo mode that fakes all the data without any Home Assistant (see Step 2).

**Full install (as in the video):** add Home Assistant, Wyoming Whisper/Piper for local voice, and whatever entities you want on screen (lights, cover, climate, TV, plant sensors). Everything is optional and mapped in one config file.

---

### Step 1: Meet the hardware

The Tab5 V2 packs everything this project needs into one finished device — no soldering, no enclosure to print:

| Inside the Tab5 | Role in this project |
|------|----------------|
| ESP32-P4 (dual-core RISC-V 400 MHz) | Runs LVGL + wake-word inference |
| 5" 1280×720 MIPI-DSI + ST7123 touch | The dashboard |
| ES7210 ADC + onboard mic | Voice capture (16 kHz / 16-bit I2S) |
| ES8388 DAC + speaker | TTS playback |
| ESP32-C6 co-processor (SDIO) | Wi-Fi 6 |
| PI4IOE5V6408 I2C GPIO expanders | Power rails, display reset… and one great war story (see "Lessons") |

📷 **[Image — `gpio_pinout_table.png` — GPIO map used by the firmware]**

Just one heads-up: several critical pins (display reset, amplifier enable) are **not** native ESP32 GPIOs — they go through I2C GPIO expanders. It matters more than it sounds; it cost me my longest debugging night (Lessons, below).

### Step 2: Flash the firmware

```bash
git clone https://github.com/Axellum/M5-Tab5-ESPHome-LVGL.git
cd M5-Tab5-ESPHome-LVGL
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml   # map YOUR entity IDs
# create secrets.yaml (Wi-Fi + API key) — see docs/installation.md
esphome run tab5-ha-hmi.yaml                                  # USB first time, OTA after
```

The firmware is **modular YAML**: 8 packages split by concern (hardware, sensors, API contract, styles, layout, globals, scripts) plus 17 reusable UI components and two C++ files holding all the non-trivial logic. Every file opens with an `[AI-CONTEXT]` header — a local "system prompt" so any AI (or human) editing it knows the constraints. CI compiles every PR.

**Try it with zero Home Assistant:** flash, then run the demo pusher — it feeds synthetic weather/planning/climate/plant data over the native API:

```bash
pip install -r tools/demo/requirements.txt
python tools/demo/demo_pusher.py --host <tab5-ip> --key <api_encryption_key>
```

Full walkthrough: [`installation.md`](installation.md) · [`demo_mode.md`](demo_mode.md)

### Step 3: Wire up Home Assistant

Copy the packages from `HomeAssistant_Config/` into your HA instance. They implement the push side: automations that watch your entities and call the Tab5's 12 `tab5_maj_*` services (forecast bulks, climate, shutter, planning, rain graph, Météo-France alerts, HA alert banners, voice replies).

A taste of the "data packing" trick — the whole 15-day forecast travels as one string, built by a Jinja template and parsed in C++ on the device:

```
"Lun 21;soleil;28;16|Mar 22;nuageux;24;15|..."
```

Just two heads-ups learned the hard way: put `continue_on_error: true` on every push action (one malformed payload otherwise silently kills the *rest* of the automation), and never pass a raw Jinja `now()` to `calendar.get_events` — format it with `strftime`, or the schema rejects it without a word.

### Step 4: The screens, one by one

Everything lives on a **single 1280×720 page** — no tab bar, no page switching. Popups overlay it, swipes paginate the forecast band.

**#Screen 1: The main dashboard.** Clock and date (the date recolors with the Météo-France vigilance level), indoor temperature/humidity, plant moisture (5 BLE sensors auto-sorted driest-first), quick actions, the compact climate card, and the mic icon showing the voice pipeline state in real time.

📷 **[Photo — `tab5_photo_console_diag.jpg` — main dashboard, live]**

**#Screen 2: Weather at a flick.** The bottom band holds 5 swipeable windows: 2 of hourly forecast (15 slots) and 3 of 15-day daily forecast. Day names are color-coded against my *work planning* (cyan = today, green = day off, red = early shift — my favorite touch). Each daily card secretly doubles as a quick-action button: tap the icon area and it toggles the PC, the shutter, a light…

📷 **[Photo — `tab5_photo_dashboard_switches.jpg` — quick actions / switches view]**

**#Screen 3: The rotating central card.** Every 8 s it cycles: planning → 1-hour rain graph → vigilance icons → info panel → up to **4 Home Assistant alert banners** pushed live (updates pending, errors, unavailable devices). Tap a banner to dismiss it — the id is remembered locally, so a re-push of the same alert stays hidden until HA sends a genuinely new one.

**#Screen 4: Climate popup.** Tap the climate card and a near-fullscreen glass panel opens: stacked mode buttons, a 320 px arc thermostat whose target updates *optimistically* (one debounced `climate.set_temperature` per gesture — not one per tick!), presets, and airflow including the Daikin "Brise" (`windnice`) mode that the official remote buries three menus deep.

📷 **[Photo TODO — climate popup v2]**

**#Screen 5: Light popup.** Long-press a light card: a 3-light selector (switch rooms without closing), a brightness arc with the live % in the center — synced from HA's `brightness` attribute, debounced on drag — shortcuts 10/35/65/100 %, 3 named whites and 12 round color swatches.

📷 **[Photo TODO — light popup v2]**

**#Screen 6: TV remote.** A full-screen Samsung remote — power, pad, volume, channels, playback row. The Tab5 has no IR hardware: every key is a `remote.send_command` service call, HA's Samsung integration does the talking.

📷 **[Photo TODO — TV remote popup]**

**#Screen 7: The system console.** Four glass cards: memory (SRAM/PSRAM bars), network (+ HA link state), system (uptime, CPU temp, volume with live %), and a **management card** — re-push the whole screen, reload automations, restart HA or reboot the tablet, the destructive ones behind Cancel/Confirm overlays.

📷 **[Photo TODO — console v2]**

### Step 5: Give it a voice

The Tab5 is a Home Assistant **Assist Satellite**. Idle listening never leaves the device:

| Step | Where | What |
|------|--------|------|
| 1. Wake word | Tab5, on-device | `okay_nabu` (microWakeWord / TFLite) |
| 2. STT | Home Assistant | Wyoming Whisper, local |
| 3. Intent / reply | HA agent → optional engine | HA commands: deterministic match. Chat: light LLM path |
| 4. TTS | Home Assistant | Wyoming Piper, local |
| 5. Playback | Tab5 | ES8388 + speaker |

The mic icon is the whole state machine: grey (idle) → green (listening) → orange (thinking) → blue (speaking) → red (didn't get it).

Two details I'm particularly happy with:

- **A second on-device wake word: "Stop".** It's only armed while the roller shutter is actually moving, and it halts it instantly — no "Okay Nabu", no round-trip, no cloud. (Fun fact: ESPHome only auto-enables the *first* declared wake-word model, and the detected string is `"Stop"` with a capital S. Both cost me an afternoon.)
- **Tap-to-interrupt.** While the assistant speaks, the pipeline owns the microphone, so barging in by voice is impossible — but tapping the mic icon stops the reply and immediately re-opens listening.

### Step 6 (optional): The multi-agent engine

Out of the box, voice goes to the standard HA agent. Flip the on-screen toggle (🏠 → 🤖) and it routes to **[vromvrom-engine](https://github.com/Axellum/vromvrom-engine)** instead — my Python multi-agent orchestrator (running on a Steam Deck!): deterministic matching for short home commands, a light LLM path for multi-turn chat, and specialist routes (web search, calendar) when the question needs them. Work-in-progress, shared for context — the dashboard works fully without it.

### Lessons from the trenches

The parts no datasheet warned me about — kept in the repo's [`troubleshooting.md`](troubleshooting.md) so nobody debugs them twice:

- **Black screen after every soft reboot.** The display's reset pin goes through the I2C GPIO expander, which needs a settle delay after boot — reset it too early and it's a silent no-op. One blocking `delay(1000)` at the right boot priority, confirmed over 5 live reboots. It looks like a beginner mistake in the code; it's the fix.
- **PNG icons with alpha crash the P4.** Weather icons are now font glyphs (MDI + a custom weather TTF) at `bpp: 1` — ~200 bytes per icon instead of 8 KB, scaling for free, and the crash is gone.
- **The device "randomly" refused connections.** ESPHome's native API has 8 slots; my own forgotten `esphome logs` terminals were eating them all.
- **French accents turned to mojibake** the day HA pushed Latin-1 into UTF-8 labels — fixed with an explicit normalization pass on every dynamic string.
- **One shared style object instead of 80 inline styles** freed ~40 KB of PSRAM. On a microcontroller, CSS hygiene is memory management.

### About the "100 % AI-built" part

Right, the "why" is out of the way — from here I'll hand you over to the rest of this write-up, written by AI and far more qualified than I am to talk about code, architecture, and hardware in detail. There are almost certainly a few rough edges left; the project keeps evolving regularly.

Firmware, automations, C++, and most of the docs were produced by AI tools (Antigravity/Gemini, DeepSeek, MiniMax, Z.ai, Claude, Cursor). I am more the **architect than the author**: I set the goals, tested every OTA on the real device, rejected what didn't work, and made the calls. The repo keeps the traces honest — `[AI-CONTEXT]` headers in every file, an `AGENTS.md` for the next AI, ADRs for the non-obvious choices, and a changelog where the AI documents its own mistakes.

If something in the code looks weird, it might be an AI quirk. If something works surprisingly well — same answer.

### Done!

That's Tab5 Voice HMI — release [v1.0.5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v1.0.5), in daily use on my desk, and fully reproducible from the repo. If you'd like to see another screen on it (or want to argue about push vs. poll), tell me in the comments.

Until next time.

---

## Schematics (Hackster "Schematics" section)

- **Push-only architecture** — `docs/images/push_only_architecture_diagram.png`
- **GPIO & audio map** — `docs/images/gpio_pinout_table.png` (details: [`hardware.md`](hardware.md))

## Code (Hackster "Code" section)

| Component | Location |
|-----------|----------|
| ESPHome entry | `tab5-ha-hmi.yaml` |
| UI + logic | `Tab5/*.yaml`, `Tab5/tab5_custom.cpp` |
| HA automations | `HomeAssistant_Config/` |
| Demo pusher | `tools/demo/demo_pusher.py` |
| Engine (optional) | [github.com/Axellum/vromvrom-engine](https://github.com/Axellum/vromvrom-engine) |

License: MIT · Firmware release: [v1.0.5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v1.0.5)

---

## Cover media (upload order on Hackster)

1. **YouTube embed** — `https://www.youtube.com/watch?v=ygNhgtMffu4` (primary hero)
2. **GIF** — `docs/images/m5stack_tab5_demo.gif` (animated UI overview)
3. **Photos** (from `docs/images/` — ⚠️ re-shoot the popup/console ones, see TODO at top):
   - `tab5_photo_console_diag.jpg` — main dashboard
   - `tab5_photo_dashboard_switches.jpg` — quick actions
   - *(new)* climate popup v2, light popup v2, TV remote, console v2
   - `push_only_architecture_diagram.png` — architecture
4. **Bonus gallery** — `tab5_design_main.jpg` / `tab5_design_climate.jpg` / `tab5_design_light.jpg` (June design — nice "how it evolved" before/after)

## Gallery captions (for Hackster photo uploads)

| File | Caption (EN) |
|------|----------------|
| `tab5_photo_console_diag.jpg` | Main dashboard — weather forecast, voice mic, climate strip |
| `tab5_photo_dashboard_switches.jpg` | Quick actions — PC, shutters, room lights |
| *(new photo)* | Climate popup v2 — stacked modes, optimistic arc, Daikin "Brise" |
| *(new photo)* | Light popup v2 — selector, live-% arc, 12 color swatches |
| *(new photo)* | Full-screen Samsung TV remote |
| *(new photo)* | System console v2 — diagnostics + HA management |
| `m5stack_tab5_demo.gif` | Animated UI overview |
| `push_only_architecture_diagram.png` | Push-only data flow (no polling) |
| `tab5_design_*.jpg` | The first design iteration (June) — before the glass theme |

---

## Contest alignment (internal checklist)

| Criterion | Angle |
|-----------|--------|
| **Creativity & originality** | Physical push-only HMI + local multi-agent voice routing — not a cloud voice assistant in a box; "100 % AI-built" narrative |
| **Functionality & execution** | Live demo: wake word → HA command → discussion → touch popup → TV remote → "Stop" on a moving shutter |
| **Documentation & presentation** | Public GitHub (release v1.0.5) + this tutorial-style page + video + GIF + photos + honest lessons-learned |
| **Impact & utility** | Private, local-first smart home — no mandatory Alexa/Google cloud |
| **M5Stack integration** | Tab5 V2 as primary controller: display, touch, audio, dual wake words, LVGL firmware |

**Submission links:** [M5Stack Global Innovation Contest 2026](https://m5stack.com/global-innovation-contest-2026) · Hackster.io project page · Google form (official)

---

## Version Française — pourquoi un écran ?

**Bref, pourquoi un écran ?**

Après 5 ans avec le Nextion, je souhaitais donner un coup de jeune à mon écran plutôt axé météo, en gardant a minima les mêmes objectifs :

- faire office d'horloge ;
- voir au premier coup d'œil si des averses sont prévues dans l'heure : je pars 15 min en avance pour ne pas arriver trempé au boulot ? Je prévois le parapluie ?
- avoir la prévision météo sur quelques jours, histoire d'avoir un sujet de conversation si je décide de me sociabiliser ;
- le tout pour une consommation raisonnable et toujours allumé (enfin, quand je suis devant), avec une liberté totale sur les positionnements, designs et logiques — pas juste l'affichage HA standard — avec les avantages... et les inconvénients que ça implique.

Avec un esprit domotique plus poussé : un retour direct de l'humidité de mes pots / de mon potager, allumer la TV et l'ordi sans bouger mes fesses de ma chaise, et gérer les trois spots du salon et la lumière de la chambre.

Puis, petit à petit :

- gérer ma clim ;
- avoir la main sur mon volet roulant, toujours sans me lever ;
- intégrer « Ok Nabu », plus besoin de me pencher pour attraper l'écran :) ;
- peaufiner l'intégration de l'assistant vocal : mode conversation, choix du LLM (local ou pas), domotique la plus rapide possible et qui me comprend ;
- afficher mon planning avec une lisibilité rapide de mes heures d'embauche ;
- avoir une télécommande réseau pour ma TV, ça peut toujours dépanner (pour la petite histoire, Claude Fable 5 m'a bluffé sur ce coup : il m'a fait ça en 2 prompts, du coup je l'ai laissé reprendre tous les popups, au prix de 50 % de ma limite des 5h par popup, sur le forfait Pro...).

Le tout avec, en termes de conception, les objectifs suivants. Vu que mes premières sessions avec Gemini sur le code du Nextion m'ont littéralement humilié — il a révolutionné l'envoi des données et divisé le code par trois — je voulais cette fois de la légèreté et de bien meilleures optimisations que ce que j'avais fait à la main :

- pas d'images, le plus léger et optimisé possible pour la tablette ;
- une gestion des envois de données côté HA robuste et la plus douce possible (mon Home Assistant tourne sur une Freebox, je reste léger) ;
- un démarrage rapide, pas de lenteur d'affichage, quelque chose de fluide, quoi.

J'ai aussi essayé d'avoir une interface moderne (j'ai la cinquantaine, ne m'en demandez pas trop) : pas de pages, mais des popups, tout accessible directement depuis l'écran d'accueil par bouton, toucher long ou swipe. Beaucoup de code couleur pour une lisibilité même à quelques mètres, tout en ayant un écran pensé pour être lu à moins d'un mètre si on veut voir toutes les données correctement. Et j'ai essayé de caser un maximum d'infos et de commandes sur une interface relativement épurée — oui, je sais, le plus dur pour moi. Objectif final : un écran à peu près correct visuellement, même si je reste plus axé pratique dans l'absolu.

De fil en aiguille, j'ai complété l'écran avec un assistant vocal, puis par un « moteur » pour gérer en local la partie domotique vocale, et en semi-local ou cloud la partie conversations. Le moteur est un projet en cours (lui aussi 🙂) où j'ai posé beaucoup (trop) de choses pour expérimenter et mieux comprendre comment marchent les LLMs : RAG, notations, gestion multi-LLM, MCP, et j'en passe. Je le partage donc surtout dans un but informatif : je l'utilise de façon fonctionnelle pour l'écran, mais pas encore pour le codage ni pour tout ce que j'ai voulu y implémenter — qui fonctionne plus ou moins bien.

Dans mon périple, j'ai commencé avec Antigravity, puis je l'ai aidé par différents modèles (DeepSeek, MiniMax, Z.ai). Ensuite, j'ai testé Claude, qui a fait lui aussi beaucoup de travail, puis Cursor récemment. Bref, je vous partage le projet de mon écran, fait pour mon usage quotidien, dont je suis plus **l'architecte que le créateur** — issu de mes débuts d'aventure dans le monde de l'IA. L'écran tourne tous les jours sur mon bureau.

Sur ce, je vous laisse avec le reste de la description du projet faite par l'IA, bien plus pertinente que moi pour l'aspect code, architecture et matériel. Il y a certainement des coquilles ; le projet évolue encore régulièrement.

---

## Version Française — titre & résumé Hackster

**Titre :** Tab5 — Tableau de bord HA push-only + assistant vocal local

**Résumé :** Interface LVGL native sur M5Stack Tab5 V2 (ESP32-P4), pilotée par événements Home Assistant (zéro polling), avec satellite vocal Assist (double wake word local dont « Stop » volet), popups clim/lumières/TV plein écran, et moteur multi-agents optionnel pour la conversation.

**Vidéo :** https://www.youtube.com/watch?v=ygNhgtMffu4
