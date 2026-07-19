# Hackster — texte prêt à coller (EN)

> Copié / synchronisé le 2026-07-19. Case **Made with AI** : cocher.
> Médias à uploader : voir § Cover media en bas + `docs/images/`.

---

## Title

Tab5 Voice HMI — Push-Only Home Assistant Dashboard on ESP32-P4

## Subtitle

A 60 FPS native LVGL dashboard + local voice satellite on the M5Stack Tab5 V2 — every line of code written by AI, steered by a human.

## What it does

A single-page LVGL dashboard on the M5Stack Tab5 V2, driven only by Home Assistant push events (no polling):

- **Home** — clock/date, indoor temps, plant moisture strip, work planning bar, compact climate control, mic icon with live Assist pipeline state, quick actions (PC, shutter, lights)
- **Weather** — 5 swipeable slides: hourly forecast (15 slots) + 15-day daily forecast, Météo-France vigilance recoloring the date, 1-hour rain graph on the rotating card
- **Glass popups** — climate (arc thermostat, modes, presets, Daikin airflow), lights (brightness arc, whites + 12 color swatches), full-screen Samsung TV remote, BLE plants detail, system console (memory / network / uptime / HA management)
- **Voice** — on-device `okay_nabu` wake word on the ESP32-P4; mic color = pipeline state (grey → green → orange → blue → red); tap-to-talk and tap-to-interrupt; optional free-form LLM conversation via vromvrom-engine; second on-device wake word **Stop** to halt a moving shutter
- **Push-only** — the tablet never requests state; HA automations pack forecasts/events into delimited strings and call native ESPHome services

---

## Story (main body — paste below)

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

That's the "why." The "how" is where AI comes in: a few months ago I kept hearing that AI could now write real code, and I wanted to see it for myself — not on a toy example, but on this. So I set the challenge: build the whole thing — firmware, automations, even most of this write-up — with AI writing **every single line**. My role stayed modest: set the goals, test on the real device, reject what didn't work, and steer.

Today the Tab5 sits on my desk and does all of this:

- A **weather station** — 15-day and hourly Météo-France forecasts you flick through with a swipe, vigilance alerts recoloring the date, a 1-hour rain graph.
- A **home control panel** — lights, roller shutter, climate, TV remote, plant moisture — with full-screen "glass" popups.
- A **rotating info card** — my work planning, rain, weather alerts, and up to 4 live Home Assistant alert banners you dismiss with a tap.
- A **local voice satellite** — "Okay Nabu" wake word running *on the device*, local STT/TTS, and a second on-device wake word ("Stop") that halts the shutter while it moves.
- An optional **multi-agent engine** for real conversations — weather, calendar, web questions — routed locally first.

Here's what it actually looks like day to day 👇

**[Embed YouTube]** https://www.youtube.com/watch?v=ygNhgtMffu4

**[Upload image/GIF]** `docs/images/tab5_ui_tour.webp` (or `tab5_ui_tour.gif`) — animated UI overview, 4:3

Wanna find out how it works? Read along. Spoiler: this is **not** a web dashboard in a kiosk browser — there is no browser, no polling, and (from my side) not a single hand-typed line of code.

---

### The big idea: push, don't poll

Most DIY wall panels run a browser, reload dashboards, and hammer the server with requests. This project turns that upside down:

- The UI is **LVGL 8.4 compiled into the ESP32-P4 firmware**. Layout is computed once at boot; updates only redraw the label that changed. It renders at 60 FPS from a ~1.8 MB framebuffer in PSRAM, over a MIPI-DSI bus with DMA.
- The Tab5 **never asks Home Assistant for anything**. HA automations detect changes and **push** data to the device through 12 native ESPHome services (`tab5_maj_*`). When nothing changes, the network is silent and the CPU is nearly idle.
- Complex payloads (a 15-day forecast!) are **packed into one delimited string** on the HA side and parsed by C++ on the device — one network call instead of dozens.
- If Wi-Fi drops or HA restarts, the screen just keeps showing the last known state. No spinner, no blank page, ever.

**[Upload]** `docs/images/push_only_architecture_diagram.png`

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

**[Upload]** `docs/images/gpio_pinout_table.png`

Just one heads-up: several critical pins (display reset, amplifier enable) are **not** native ESP32 GPIOs — they go through I2C GPIO expanders. It matters more than it sounds; it cost me my longest debugging night (Lessons, below).

### Step 2: Flash the firmware

```bash
git clone https://github.com/Axellum/M5-Tab5-ESPHome-LVGL.git
cd M5-Tab5-ESPHome-LVGL
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml   # map YOUR entity IDs
# create secrets.yaml (Wi-Fi + API key) — see docs/installation.md
esphome run tab5-ha-hmi.yaml                                  # USB first time, OTA after
```

The firmware is **modular YAML**: 8 packages split by concern (hardware, sensors, API contract, styles, layout, globals, scripts) plus 19 reusable UI components and two C++ files holding all the non-trivial logic. Every file opens with an `[AI-CONTEXT]` header — a local "system prompt" so any AI (or human) editing it knows the constraints. CI compiles every PR.

**Try it with zero Home Assistant:** flash, then run the demo pusher — it feeds synthetic weather/planning/climate/plant data over the native API:

```bash
pip install -r tools/demo/requirements.txt
python tools/demo/demo_pusher.py --host <tab5-ip> --key <api_encryption_key>
```

Full walkthrough: [`installation.md`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/docs/installation.md) · [`demo_mode.md`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/docs/demo_mode.md)

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

**[Photo]** `tab5_photo_home.jpg` — Accueil météo / main dashboard (live)

**#Screen 2: Weather at a flick.** The bottom band holds 5 swipeable windows: 2 of hourly forecast (15 slots) and 3 of 15-day daily forecast. Day names are color-coded against my *work planning* (cyan = today, green = day off, red = early shift — my favorite touch). Each daily card secretly doubles as a quick-action button: tap the icon area and it toggles the PC, the shutter, a light…

**#Screen 3: Domotics strip / quick actions.** Tap the house button and the forecast band flips to the Domo view — PC, shutter, bedroom/living-room lights and LED strip: status at a glance, one tap to toggle.

**[Photo]** `tab5_photo_domo.jpg` — Domo / switches view

**#Screen 4: The rotating central card.** Every 8 s it cycles: planning → 1-hour rain graph → vigilance icons → info panel → up to **4 Home Assistant alert banners** pushed live (updates pending, errors, unavailable devices). Tap a banner to dismiss it — the id is remembered locally, so a re-push of the same alert stays hidden until HA sends a genuinely new one.

**#Screen 5: Climate popup.** Tap the climate card and a near-fullscreen glass panel opens: stacked mode buttons, a 320 px arc thermostat whose target updates *optimistically* (one debounced `climate.set_temperature` per gesture — not one per tick!), presets, and airflow including the Daikin "Brise" (`windnice`) mode that the official remote buries three menus deep.

**[Photo]** `tab5_photo_climate_popup_v2.jpg`

**#Screen 6: Light popup.** Long-press a light card: a 3-light selector (switch rooms without closing), a brightness arc with the live % in the center — synced from HA's `brightness` attribute, debounced on drag — shortcuts 10/35/65/100 %, 3 named whites and 12 round color swatches.

**[Photo]** `tab5_photo_light_popup_v2.jpg`

**#Screen 7: Plants detail.** Full plant cards for up to 5 BLE soil sensors — moisture with status color, fertility, light, temperature, battery; offline pots stay visible as "Hors ligne".

**[Photo]** `tab5_photo_plants.jpg` — Mes Plantes

**#Screen 8: TV remote.** A full-screen Samsung remote — power, pad, volume, playback row. The Tab5 has no IR hardware: every key is a `remote.send_command` service call, HA's Samsung integration does the talking.

**[Photo]** `tab5_photo_tv_remote.jpg`

**#Screen 9: The system console.** Four glass cards: memory (SRAM/PSRAM bars), network (+ HA link state), system (uptime, CPU temp, volume with live %), and a **management card** — re-push the whole screen, reload automations, restart HA or reboot the tablet, the destructive ones behind Cancel/Confirm overlays.

**[Photo]** `tab5_photo_console_v2.jpg`

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

The parts no datasheet warned me about — kept in the repo's [`troubleshooting.md`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/docs/troubleshooting.md) so nobody debugs them twice:

- **Black screen after every soft reboot.** The display's reset pin goes through the I2C GPIO expander, which needs a settle delay after boot — reset it too early and it's a silent no-op. One blocking `delay(1000)` at the right boot priority, confirmed over 5 live reboots. It looks like a beginner mistake in the code; it's the fix.
- **PNG icons with alpha crash the P4.** Weather icons are now font glyphs (MDI + a custom weather TTF) at `bpp: 1` — ~200 bytes per icon instead of 8 KB, scaling for free, and the crash is gone.
- **The device "randomly" refused connections.** ESPHome's native API has 8 slots; my own forgotten `esphome logs` terminals were eating them all.
- **French accents turned to mojibake** the day HA pushed Latin-1 into UTF-8 labels — fixed with an explicit normalization pass on every dynamic string.
- **One shared style object instead of 80 inline styles** freed ~40 KB of PSRAM. On a microcontroller, CSS hygiene is memory management.

### A note on this project

This is a personal project. The goal was never to build a product — it was to explore what AI tools can actually do when given a real hardware target, real constraints, and real debugging cycles.

Everything in this repository — the ESPHome YAML, the C++ code, the Home Assistant automations, and most of this documentation — was generated by AI:

- Claude (Anthropic) — architecture decisions, C++ implementation, debugging
- Gemini / Antigravity (Google) — orchestration, documentation, code review
- DeepSeek — code generation, optimization passes
- MiniMax and Z.ai — various generation tasks
- Cursor — recent editing / packaging passes

The role of the human in the loop was to define what the project should do, to test what came out, to say "this is wrong" or "this is close but not quite", and to keep pushing. No code was written by hand.

This is not a claim about AI capability or human obsolescence. It's just what the experiment looked like in practice. Some things worked better than expected. Some things required many iterations to get right. A few things still don't work correctly. That's the honest version.

If any part of this is useful to you — as a starting point, as a reference, or as a "what not to do" — that's good enough.

### Done!

That's Tab5 Voice HMI — release [v1.0.5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v1.0.5), in daily use on my desk, and fully reproducible from the repo. If you'd like to see another screen on it (or want to argue about push vs. poll), tell me in the comments.

Until next time.

---

## Cover media (ordre d’upload Hackster)

1. **YouTube** — https://www.youtube.com/watch?v=ygNhgtMffu4
2. **Hero animé 4:3** — `docs/images/tab5_ui_tour_hq.webp` (ou `.gif` / `.webp` compact)
3. **Still hero** — `docs/images/tab5_hero_4x3.jpg` (si le champ exige une image fixe)
4. Photos live (2026-07-19) : `tab5_photo_home.jpg`, `tab5_photo_domo.jpg`, `tab5_photo_climate_popup_v2.jpg`, `tab5_photo_light_popup_v2.jpg`, `tab5_photo_plants.jpg`, `tab5_photo_tv_remote.jpg`, `tab5_photo_console_v2.jpg`
5. `push_only_architecture_diagram.png` · `gpio_pinout_table.png`

## Captions gallery (EN)

| Media | Caption |
|-------|---------|
| tab5_ui_tour*.webp/gif | Animated UI tour — home, domotics, plants, climate, lights, TV, console |
| tab5_photo_home.jpg | Main dashboard — weather, voice mic, climate strip |
| tab5_photo_domo.jpg | Quick actions — PC, shutters, room lights |
| tab5_photo_climate_popup_v2.jpg | Climate popup — modes, optimistic arc, Daikin Brise |
| tab5_photo_light_popup_v2.jpg | Light popup — selector, live-% arc, color swatches |
| tab5_photo_plants.jpg | BLE plant sensors — moisture, fertility, light, battery |
| tab5_photo_tv_remote.jpg | Full-screen Samsung TV remote (HA `remote.send_command`) |
| tab5_photo_console_v2.jpg | System console — memory, network, uptime, HA management |
| push_only_architecture_diagram.png | Push-only data flow (no polling) |
| gpio_pinout_table.png | GPIO / audio map used by the firmware |
