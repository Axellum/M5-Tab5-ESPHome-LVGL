# Performance

## English · [Français](#version-française)

---

Measured on the author's tablet (Tab5, ST7123 screen, ESP32-P4 revision v1.3 at 360 MHz). Newest campaign first; the older numbers are kept, dated, for comparison.

## Results

Latest campaign: measured on **2026-10-07** between 22:30 and 00:05, on the published **3.8.0-rc.1** and on measuring builds of the same code (commit `45a0c34`) that were never committed: they log every frame of 2 ms or more (duration, pixels, areas), and one of them also times each step of a theme switch and logs the free internal RAM and PSRAM every 60 s. Driven through the native API (`aioesphomeapi`), never in a minute where the blueprint pushes its 5-minute measurements; whole screen = backlight off then on; popups through the « Aller à l'écran » select; the number kept is the longest frame, median of the runs. Alerts were showing, so the rotating panel of the centre card was turning: idle numbers only compare at equal content, full frames do not depend on it.

### Whole screen, per theme (dark mode, 5 runs)

| Theme | Whole screen (ms) | Climate popup (ms) |
|---|---|---|
| Relief plat | 167.8 | 195.6 |
| Relief doux | 160.8 | 169.3 |
| Bonbon | 149.4 | 170.8 |
| Sorbet | 138.1 | 165.2 |
| Capsule | 135.8 | 154.7 |
| Ardoise | 135.6 | 162.3 |
| Ardoise douce | 134.9 | 163.5 |
| Bento | 134.8 | 160.3 |
| Craie et ardoise | 134.0 | 163.7 |
| Platine et or | 133.9 | 161.8 |
| Almanach | 133.7 | 163.3 |
| Obsidienne | 133.4 | 160.1 |
| Graphite | 133.1 | 158.7 |
| Ultraviolet | 132.6 | 162.5 |
| Terre cuite | 131.6 | 152.9 |
| Béton brut | 131.6 | 155.8 |
| Almanach imprimé | 131.4 | 158.7 |
| Signalisation | 130.7 | 160.1 |
| Néon calme | 130.1 | 161.4 |
| Pixel | 127.4 | 157.6 |
| Zen Sumi | 125.7 | 153.4 |

- 18 themes of 21 redraw the whole screen in **126 to 138 ms**. The three slower ones are the **shadows**: Relief plat and Relief doux are the only themes with real shadows (`shadow_width` in their `formes:`; 50 px around the popup window in Relief plat, hence its 196 ms climate popup), Bonbon has a 1 px shadow and the largest radii. Gradients alone cost almost nothing (Capsule, Sorbet: 0 to +3 ms from Ardoise).
- A whole screen is **~59 ms of sending** (fixed, see « Where the time goes » below) plus **~75 ms of drawing** in Ardoise; the shadows add +32 ms (Relief plat), +25 ms (Relief doux) and +14 ms (Bonbon) to the drawing.
- **Light mode** (3 runs): Ardoise 137.6, Relief doux 162.2, Relief plat 172.8, Bonbon 147.4, Capsule 139.1 ms — the same order as dark, within 0-5 ms.
- **Idle**, no theme is slower: the longest frame stays at 15-19 ms (the rotating panel). Shadows only cost when a large area is redrawn (screen turned back on, popup opened or closed).
- **LVGL's shadow cache** (`LV_DRAW_SW_SHADOW_CACHE_SIZE` at 80, 6.4 kB of internal RAM), A/B the same evening: Relief plat 169.4 ms against 167.8, Relief doux 159.5 against 160.8 — noise, **no gain**. The cache holds one entry, each card has a different shadow size and the screen is drawn in bands. Not adopted.

### Popups (theme Zen Sumi, dark, 4 runs)

| Popup | Opening (ms) |
|---|---|
| Alarm clock | 156.9 |
| Climate | 154.0 |
| Settings | 151.6 |
| Plants | 144.8 |
| System console | 143.1 |
| TV remote | 141.9 |
| Calendar | 141.3 |
| House | 137.1 |
| Voice assistant | 136.6 |
| Alerts | 127.8 |
| Energy | 106.8 |

Zen Sumi is the fastest theme: add ~10 ms for Ardoise and ~40 ms for Relief plat. Closing a popup redraws the whole screen: 126-130 ms. The « Tab5 Loop Time » sensor reaches 190-210 ms in the minute a popup opens: a full frame blocks the loop while it is drawn (software drawing on one core). Against 3.2.0 (below, single palette): climate popup 190 → 154-163 ms, console 167 → 143 ms, alarm clock 197 → 157 ms, TV remote 171 → 142 ms, calendar 126 → 141 ms (the only one up).

### Theme switch

Changing the theme or the light/dark mode, including the day/night switch of the Auto mode, **blocks the main loop 3.17 to 3.20 s, whatever the theme**; the new theme shows **3.7 to 3.9 s** after the request. Steps (identical within ±3 % at every switch):

| Step | Time |
|---|---|
| Colours of the 53 shared styles (53 × `lv_obj_report_style_change`) | 1.22 s |
| `lvgl.theme.update` (text colour of the labels) | 0.60 s |
| Shapes (12 styles: radii, borders, shadows, gradients) | 1.04 s |
| Display fonts (3 styles) | 0.12 s |
| Colours set by the C++ modules | 0.16 s |
| Alarm clock, assistant | 0.02 s |

The screen holds **1,593 LVGL objects**, and every `lv_obj_report_style_change()` walks them all and refreshes the whole subtree of each object that carries the style. It is the repaint code, not the theme. **An improvement is being studied** (not part of 3.8.0-rc.1).

### Boot

Restart from the « Redémarrage Système » button, 3 runs per theme. In Ardoise (the compiled palette, nothing to repaint) the first « Loop Time » published after the restart is 10,450 ms and the API is reachable again after 17.2-17.4 s; in another theme (Capsule, repainted at boot) 13,401 ms and 18.9-19.1 s. **Any theme other than Ardoise adds ~2.95 s of blocked loop at boot**, and the API comes back 1.7 s later — the same repaint as the theme switch. These times do not compare with the boot table of 2026-10-01 below (another time zero, another method).

### Resources

| Resource | Used | Free |
|---|---|---|
| Internal RAM (heap) | — | **262 kB** (lowest since boot 217 kB, largest block 172 kB); 249-290 kB over 7 days |
| Static RAM | 193 kB of 445 kB (43.4 %) | — |
| PSRAM | ~6.1 MB | **23.0 MB of 29.1 MB** (lowest 22.95 MB) |
| Flash (firmware slot, 7.75 MB) | 4,558,142 bytes (56.1 %), of which 1.08 MB of font and icon images, 452 kB of screen-building code (`setup()`) | **3.57 MB** |
| Main loop stack | — | **10.2 kB at the lowest** (7 days, unchanged since 3.2.0) |
| LVGL objects | 1,593 | — |

Since 3.2.0: free internal RAM 286.7 → 262 kB (−25 kB); firmware image 3.36 → 4.56 MB (+1.2 MB, mostly the display fonts of the themes). Nothing is tight.

### Hardware over 7 days (2026-09-30 → 10-07)

- ESP32-P4 temperature: 32.4 / 36.6 / 39.4 °C (min / mean / max, 976 readings).
- 3 brownout restarts; the tablet runs without a battery, on a computer's USB port. Cause not verified.

## Earlier results: 2026-09-28 (3.2.0)

Firmware **3.2.0** built exactly like the published one (ESPHome 2026.9.0), on **2026-09-28** between 20:35 and 22:52. Screen brightness 36/255, home page, weather mode, rooms defined, microphone off at the start (nobody home). A single palette: the themes came in 3.6.0.

| What | Measured | Runs |
|---|---|---|
| Longest frame of a minute, idle | 16.7-20.4 ms | 8 minutes |
| A changed value (tile, clock minute) | under 10 ms | diagnostic log |
| Rotating panel in the middle (every 8 s, 6-7 frames) | 13-21 ms per frame | diagnostic log |
| Whole screen redrawn (screen turned back on) | 133.2-134.0 ms | 3 |
| Opening the calendar | 126.2-127.2 ms | 2 |
| Opening the voice assistant | 162.1 ms | 1 |
| Opening the system console | 167.2-169.9 ms | 2 |
| Opening the plants | 170.4 ms | 1 |
| Opening the TV remote | 171.1 ms | 1 |
| Opening the climate popup | 190.2-190.4 ms | 2 |
| Opening the alarm clock | 197.4 ms | 1 |
| Restart → Home Assistant connected again | 16.4-17.0 s | 3 |
| Update over Wi-Fi (3.4 MB) | 12.5-13.7 s | 3 |
| Free internal RAM | 286.5-287.3 kB | whole run |
| Free stack of the main loop, lowest | 10,212 bytes | whole run |
| Firmware image | 3,358,098 bytes (41 % of the 7.75 MB slot) | build |

Compared with the last run of the same kind (2.x, 2026-09-26): the whole screen went from 163 to 133 ms, the calendar from 223 to 126 ms, the climate popup from 216 to 190 ms, the console from 191 to 167 ms. The idle frame depends on what is shown, not on the version: the rotating panel only turns when it has at least two things to show (schedule, rain, alerts…), and a firmware from 2026-09-27 measured the same evening gave the same 20 ms.

## Method

- **Longest frame**: the `Tab5 Draw Max` sensor (`Tab5/paquets/tab5-sensors-diagnostics.yaml`) keeps the longest frame of each minute, from `LV_EVENT_RENDER_START` to `LV_EVENT_REFR_READY` (drawing plus sending to the screen). It is `internal: true` in the published firmware; the measuring build only removes that line.
- **Actions** are sent from Home Assistant in the middle of a one-minute window (backlight off then on, « Aller à l'écran » select), never in a minute where the blueprint pushes its 5-minute measurements. Popups opened this way are not pressed: button feedback is not part of these numbers.
- **Where the time goes**: a local diagnostic build logs, for every frame of 8 ms or more, its duration and the rectangles LVGL redrew (`inv_areas` of the display at `LV_EVENT_RENDER_START`). That is how the rotating panel (a 1180 × 86 px band redrawn by its 190 ms slide and fade) was found to be the idle cost.
- **Restart**: button « Redémarrage système », then time until `ha_api_status` is `on` again in Home Assistant.
- **CPU load** (system console, 2026-10-06, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): for each core, the share of the last 2 seconds *not* spent in its FreeRTOS idle task. FreeRTOS counts each task's running time at every task switch once `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` is on (`Tab5/paquets/tab5-hardware.yaml`, esp_timer clock, 1 µs); `update_console_cpu_ui()` (`Tab5/ecran/tab5_console.cpp`) reads the two idle counters only while the console is open. An idle task that never yields would not have its counter updated: before each reading, an `esp_ipc` call to the other core makes its idle task yield. Cost of the option, estimated and not measured on the tablet: one esp_timer read per task switch, 12 bytes per task and a few bytes per queue. Not a profiler: it says how busy each core is, not which task.

## Where the time goes (2026-09-29)

A local diagnostic build timed, for every frame of 8 ms or more, the part spent sending it to the screen (LVGL `LV_EVENT_FLUSH_START` → `FLUSH_FINISH`: ESPHome rotates the frame with the P4's 2D accelerator, then copies it to the framebuffer) and the part spent drawing it.

| Frame | Sending | Drawing, 1 core | Drawing, 2 cores |
|---|---|---|---|
| Whole screen | 58.7 ms | 74.4 ms | 67.8 ms |
| Climate popup | 59 ms | 130.4 ms | 116.3 ms |
| System console | 59 ms | 107.5 ms | 98.0 ms |
| Calendar | 59 ms | 67.2 ms | 62.8 ms |
| Rotating panel (one frame) | 8.4 ms | 7.4 ms | 7.1 ms |
| Pinball (portrait, no rotation) | 1.2 ms | 9.2 ms | 8.7 ms |

- **Sending a whole screen costs a fixed ~59 ms** (1.8 MB rotated then copied in PSRAM), whatever the content.
- **Drawing on the P4's two cores** (`LV_USE_OS` FreeRTOS, 2 draw units, only through `esphome: platformio_options: build_flags` in ESPHome 2026.9) works, with no visible artefact, but saves only 4 to 11 % of the drawing: not adopted (LVGL 9.5 has known multi-thread drawing bugs, fixed in 9.6; 16 KB more internal RAM).
- **The main loop only waits for LVGL**: ESPHome's `runtime_stats` shows, idle, a longest iteration of 28 ms, all of it LVGL (one step of the rotating panel); API, Wi-Fi and sensors stay under 0.3 ms. The « Tab5 Loop Time » sensor adds the wait between iterations. The rotating panel only turns when it has two things to show — rain in the next hour is one of them —, which is why the idle numbers change with the weather.

### Rotating panel: only the text slides (2026-09-29)

Each panel of the rotator spans the whole card (its invisible 1180 px tap button), so sliding the panel redrew the whole band on every frame. Sliding its content instead (`transition_widgets()`, same 190 ms, 28 px animation) redraws only the text's width, except on the first and last frames (panel shown, panel hidden). Same tablet, same morning, a rotation between the schedule and rain in the next hour:

| Rotation (190 ms) | Frames | Time per rotation | Pixels redrawn |
|---|---|---|---|
| Whole panel slides | 6-7 | 107.8 ms | 604,000 |
| Only the text slides (714 px wide) | 7 | 93.4 ms (−13 %) | 475,000 |
| Only the text slides (616 px wide) | 7 | 86.4 ms (−20 %) | 447,000 |

The gain is in sending (proportional to the area); drawing a half-transparent text costs about as much as drawing the plain band.

## Boot, phase by phase (2026-10-01)

USB console read without resetting the chip (`tools/capture_serie.py` recipe, each line timestamped when received), restarts from the « Redémarrage Système » button in Home Assistant. Time zero: the tablet closes its API link (`Rebooting safely`), the moment Home Assistant marks it unavailable. Seconds, identical within 0.05 s from one restart to the next.

| Step | 3.3.2 (3 restarts) | Since 2026-10-01 (3) |
|---|---|---|
| Bootloader done, firmware loaded (3.3 MB) | 0.83 | 0.82 |
| ESP-IDF started (PSRAM, code copied to PSRAM) | 2.25 | 1.56 (no PSRAM test) |
| Screen built (LVGL objects, before `setup()`) | 3.44 | 2.76 |
| `setup()` done (1 s blocking wait included, display, audio) | 8.43 | 7.80 |
| First frame on screen (0.85 s of drawing) | 9.29 | 8.65 |
| Wi-Fi: connection starts | 11.03 (after a 1.7 s scan) | 7.71 (during `setup()`) |
| Wi-Fi connected (address obtained) | 14.85 | 10.55 (no ARP check) |
| `esphome.tab5_connected` received by Home Assistant | 18.2-18.4 | 12.0 |
| Full push received | 18.4-18.6 | 12.3 |

What is left: `setup()` (5 s, including the 1 s wait the screen needs after a software restart, `[AI-WARNING]` in `tab5-ha-hmi.yaml`), the Wi-Fi association through the ESP32-C6 (2.8 s), and Home Assistant's own reconnection (1.4 s after the Wi-Fi is up, not driven by the tablet).

### Boot timeline, read in Home Assistant (next version)

A tablet on a mains charger has no serial log. Two diagnostic entities give, for every boot, the time of fixed steps: **Tab5 Chronologie du démarrage** up to the end of ESPHome's `setup()`, **Tab5 Chronologie du démarrage (suite)** after it (two texts because Home Assistant keeps at most 255 characters in a state). Same format, `name=ms; …`, « - » for a step not reached (yet):

`ctor=…; objets=…; setup=…; bus=…; expandeur=…; avant1s=…; apres1s=…; p600=…; donnees=…; lvgl=…; wifiinit=…; reseau=…; ecoute=…; fin=…; i18n=…; zones=…`
`ordo=…; retro=…; dessin=…; image=…; tard=…; wifi=…; api=…; fin600=…; chargeur=…`

**Clock.** Every value is `esp_timer_get_time() / 1000`: ESP-IDF resets this hardware counter when it starts (`esp_timer_impl_early_init`, CORE init stage, before the C++ constructors), so neither the ROM nor the bootloader is counted (0.8 s in the table above). ESPHome's `millis()` is something else: the FreeRTOS tick count (`CONFIG_FREERTOS_HZ=1000`, `esp32/hal.cpp`), which starts with the scheduler. `ordo` is the time at which `millis()` was 0, measured once at `setup`: a value read with `millis()` (the first version of this entity, or a log line) is `value − ordo` on this scale. A tick lost during the boot would make `ordo` a little larger (not seen, not ruled out).

How ESPHome boots, to read the gaps: its `main()` builds every object (the ~1,800 LVGL widgets included) and then calls `App.setup()`, which runs each component's `setup()` by decreasing priority (registration order on a tie). The `on_boot` entries are components too, run at their own priority: the entries that only mark (end of the `on_boot` list in `tab5-ha-hmi.yaml`, priorities nothing else uses) therefore bound the priority bands. The difference between two consecutive marks is the time of the `setup()`s between them.

| Name | Where | What the gap before it holds |
|---|---|---|
| `ctor` | global C++ constructor in `Tab5/ecran/tab5_demarrage.cpp` (`do_global_ctors`, before the scheduler) | ESP-IDF's start: PSRAM, code copied to PSRAM. ESP-Hosted's own constructor (SDIO link to the C6) runs in the same phase, in no fixed order |
| `objets` | lambda of `style_repere_demarrage`, first style (`tab5-styles.yaml`), created right after LVGL's init | scheduler start, `app_main`, construction of the ESPHome components (no I/O) |
| `setup` | `on_boot` priority 2000, the first `setup()` | **construction of the LVGL widgets** (and of the remaining objects) |
| `bus` | `on_boot` 801 | I²C bus, 2.5 V LDO, preferences (1000), the two I/O expanders (900) |
| `expandeur` | USB power switch (`usb_5v_power`, expander 0x44 P3), hardware band (800) | first part of the 800 band: globals and selects restored from NVS, switches… in registration order |
| `avant1s` / `apres1s` | `on_boot` 700, before / after the blocking `delay(1000)` (`[AI-WARNING-CRITICAL]`, left as it is) | `avant1s`: rest of the 800 band and the backlight light (799); `apres1s`: the 1 s wait itself |
| `p600` | first action of `on_boot` 600 | nothing (registered before the display) |
| `donnees` | `on_boot` 500 | **the whole 600 band**: display (MIPI-DSI, reset through the expander), touch, ES7210, microphone, ES8388, INA226, IMU, sensors, and the start of `on_boot` 600 (`lvgl.resume`, backlight on). They share priority 600: no mark can split them without changing a priority |
| `lvgl` | `on_boot` 399 | 400 band, in this order: speaker (I²S), **LVGL's `setup()`**, media player |
| `wifiinit` | `on_boot` 249 | LVGL's deferred lambdas (395) and animations (380), network (300), **Wi-Fi `setup()`** (250), which goes through the C6 |
| `reseau` | `on_boot` 199 | SNTP (220), API, OTA, safe mode (200) |
| `ecoute` | `on_boot` 99 | micro_wake_word, voice assistant (100) |
| `fin` | first action of `on_boot` -100 | the rest down to -100 (debug component) |
| `i18n` / `zones` | after `i18n_apply_boot()` / after the `tab5_zones_apply` script, `on_boot` -100 | screen language; optional zones (tiles, climate, row, − / + tile). The end of `setup()` follows |
| `ordo` | computed at `setup` | — (see Clock) |
| `retro` | first non-zero write to the backlight PWM (`tab5-hardware.yaml`) | the backlight really comes on |
| `dessin` / `image` | `lvgl: on_draw_start` / first `on_draw_end` | first LVGL draw, end of the first frame sent to the screen |
| `tard` | end of `on_boot` -100 (its `delay: 2s`, then the pressed-scale helper and the clock pointers) | — |
| `wifi` / `api` | Wi-Fi connected / first API client (Home Assistant), first time | — |
| `fin600` | end of `on_boot` 600 (Home Assistant awaited, `esphome.tab5_connected`, `delay: 3s`, calendar prefetch) | — |
| `chargeur` | first time the battery charger is switched on; « - » when the previous boot saw no battery ([hardware](hardware.md), Battery) | — |

The marks of the first text never publish: both texts are published at the first frame, then at each later step (`tard`, `wifi`, `api`, `fin600`, `chargeur`), never during `setup()`; Home Assistant keeps one value per boot in its history. `retro`, `dessin` and `image` normally come after `fin` (ESPHome runs the loops only after `setup()`, unless a component makes `setup()` wait for it); compare them with `fin` to know. Marking a step costs one clock read and one memory write. Source: `Tab5/socle/tab5_demarrage.h`; wiring checked by `tests/test_chronologie_demarrage.py`.

## Limits

- One tablet, one screen revision, one evening per campaign.
- Not measured: touch latency, voice latency (wake word to answer). Power draw has its own page: [power consumption](consumption.md).
- Most of the time of a full-screen redraw is software rendering on one core; ESPHome 2026.9 draws with a single buffer and waits for each transfer, and uses the P4's 2D accelerator for rotation only.

---

## Version française

Mesuré sur la tablette de l'auteur (Tab5, écran ST7123, ESP32-P4 révision v1.3 à 360 MHz). La campagne la plus récente d'abord ; les anciens chiffres restent, datés, pour comparer.

## Résultats

Dernière campagne : mesurée le **07/10/2026** entre 22 h 30 et 0 h 05, sur la **3.8.0-rc.1** publiée et sur des builds de mesure du même code (commit `45a0c34`), jamais commités : ils écrivent au journal chaque image de 2 ms ou plus (durée, pixels, zones), et l'un d'eux chronomètre aussi chaque étape d'une bascule de thème et relève la RAM interne et la PSRAM libres toutes les 60 s. Pilotage par l'API native (`aioesphomeapi`), jamais dans une minute où le blueprint pousse ses mesures de 5 minutes ; écran entier = rétroéclairage éteint puis rallumé ; popups par le select « Aller à l'écran » ; chiffre retenu = image la plus longue, en médiane des passes. Des alertes étaient affichées, donc le panneau tournant de la carte centrale tournait : les chiffres au repos ne se comparent qu'à contenu égal, les images pleines n'en dépendent pas.

### Écran entier, par thème (mode sombre, 5 passes)

| Thème | Écran entier (ms) | Popup clim (ms) |
|---|---|---|
| Relief plat | 167,8 | 195,6 |
| Relief doux | 160,8 | 169,3 |
| Bonbon | 149,4 | 170,8 |
| Sorbet | 138,1 | 165,2 |
| Capsule | 135,8 | 154,7 |
| Ardoise | 135,6 | 162,3 |
| Ardoise douce | 134,9 | 163,5 |
| Bento | 134,8 | 160,3 |
| Craie et ardoise | 134,0 | 163,7 |
| Platine et or | 133,9 | 161,8 |
| Almanach | 133,7 | 163,3 |
| Obsidienne | 133,4 | 160,1 |
| Graphite | 133,1 | 158,7 |
| Ultraviolet | 132,6 | 162,5 |
| Terre cuite | 131,6 | 152,9 |
| Béton brut | 131,6 | 155,8 |
| Almanach imprimé | 131,4 | 158,7 |
| Signalisation | 130,7 | 160,1 |
| Néon calme | 130,1 | 161,4 |
| Pixel | 127,4 | 157,6 |
| Zen Sumi | 125,7 | 153,4 |

- 18 thèmes sur 21 redessinent l'écran entier en **126 à 138 ms**. Les trois plus lents, ce sont les **ombres** : Relief plat et Relief doux sont les seuls thèmes à vraies ombres (`shadow_width` dans leurs `formes:` ; 50 px autour de la fenêtre des popups dans Relief plat, d'où son popup clim à 196 ms), Bonbon a une ombre de 1 px et les plus grands rayons. Les dégradés seuls ne coûtent presque rien (Capsule, Sorbet : 0 à +3 ms d'Ardoise).
- Un écran entier = **~59 ms d'envoi** (fixes, voir « Où part le temps » plus bas) + **~75 ms de dessin** en Ardoise ; les ombres ajoutent +32 ms (Relief plat), +25 ms (Relief doux) et +14 ms (Bonbon) au dessin.
- **Mode clair** (3 passes) : Ardoise 137,6 ; Relief doux 162,2 ; Relief plat 172,8 ; Bonbon 147,4 ; Capsule 139,1 ms — le même classement qu'en sombre, à 0-5 ms près.
- **Au repos**, aucun thème ne ralentit : l'image la plus longue reste de 15 à 19 ms (panneau tournant). Les ombres ne coûtent que lorsqu'une grande surface est redessinée (écran rallumé, popup ouvert ou fermé).
- **Cache d'ombres de LVGL** (`LV_DRAW_SW_SHADOW_CACHE_SIZE` à 80, 6,4 Ko de RAM interne), A/B le même soir : Relief plat 169,4 ms contre 167,8, Relief doux 159,5 contre 160,8 — du bruit, **aucun gain**. Le cache n'a qu'une entrée, chaque carte a une taille d'ombre différente et l'écran est dessiné par bandes. Pas retenu.

### Popups (thème Zen Sumi, sombre, 4 passes)

| Popup | Ouverture (ms) |
|---|---|
| Réveil | 156,9 |
| Climatisation | 154,0 |
| Réglages | 151,6 |
| Plantes | 144,8 |
| Console système | 143,1 |
| Télécommande TV | 141,9 |
| Calendrier | 141,3 |
| Maison | 137,1 |
| Assistant vocal | 136,6 |
| Alertes | 127,8 |
| Énergie | 106,8 |

Zen Sumi est le thème le plus rapide : ajouter ~10 ms pour Ardoise et ~40 ms pour Relief plat. Fermer un popup redessine l'écran entier : 126-130 ms. Le capteur « Tab5 Loop Time » atteint 190 à 210 ms dans la minute où un popup s'ouvre : une image pleine bloque la boucle le temps de la dessiner (dessin logiciel sur un cœur). Par rapport à la 3.2.0 (plus bas, palette unique) : clim 190 → 154-163 ms, console 167 → 143 ms, réveil 197 → 157 ms, télécommande 171 → 142 ms, calendrier 126 → 141 ms (le seul en hausse).

### Bascule de thème

Changer de thème ou de mode clair/sombre, y compris la bascule jour/nuit du mode Auto, **bloque la boucle principale 3,17 à 3,20 s, quel que soit le thème** ; le nouveau thème apparaît **3,7 à 3,9 s** après la demande. Étapes (identiques à ±3 % à chaque bascule) :

| Étape | Durée |
|---|---|
| Couleurs des 53 styles partagés (53 × `lv_obj_report_style_change`) | 1,22 s |
| `lvgl.theme.update` (couleur du texte des labels) | 0,60 s |
| Formes (12 styles : rayons, bordures, ombres, dégradés) | 1,04 s |
| Polices d'affichage (3 styles) | 0,12 s |
| Couleurs posées par les modules C++ | 0,16 s |
| Réveil, assistant | 0,02 s |

L'écran compte **1 593 objets LVGL**, et chaque `lv_obj_report_style_change()` les parcourt tous et rafraîchit tout le sous-arbre de chaque objet qui porte le style. C'est le code de la repeinture, pas le thème. **Une amélioration est à l'étude** (hors 3.8.0-rc.1).

### Démarrage

Redémarrage par le bouton « Redémarrage Système », 3 passes par thème. En Ardoise (la palette compilée, rien à repeindre), la première « Loop Time » publiée après le redémarrage vaut 10 450 ms et l'API est de nouveau joignable après 17,2 à 17,4 s ; dans un autre thème (Capsule, repeint au démarrage), 13 401 ms et 18,9 à 19,1 s. **Tout thème autre qu'Ardoise ajoute ~2,95 s de boucle bloquée au démarrage**, et l'API revient 1,7 s plus tard — la même repeinture que la bascule. Ces durées ne se comparent pas au tableau du démarrage du 01/10/2026 plus bas (autre origine, autre méthode).

### Ressources

| Ressource | Pris | Libre |
|---|---|---|
| RAM interne (tas) | — | **262 Ko** (au plus bas depuis le démarrage 217 Ko, plus grand bloc 172 Ko) ; 249 à 290 Ko sur 7 jours |
| RAM statique | 193 Ko sur 445 Ko (43,4 %) | — |
| PSRAM | ~6,1 Mo | **23,0 Mo sur 29,1 Mo** (au plus bas 22,95 Mo) |
| Flash (emplacement du firmware, 7,75 Mo) | 4 558 142 octets (56,1 %), dont 1,08 Mo d'images de polices et d'icônes, 452 Ko de code de construction de l'écran (`setup()`) | **3,57 Mo** |
| Pile de la boucle principale | — | **10,2 Ko au plus bas** (7 jours, inchangé depuis la 3.2.0) |
| Objets LVGL | 1 593 | — |

Depuis la 3.2.0 : RAM interne libre 286,7 → 262 Ko (−25 Ko) ; image du firmware 3,36 → 4,56 Mo (+1,2 Mo, surtout les polices d'affichage des thèmes). Rien n'est tendu.

### Matériel sur 7 jours (30/09 → 07/10/2026)

- Température de l'ESP32-P4 : 32,4 / 36,6 / 39,4 °C (min / moyenne / max, 976 valeurs).
- 3 redémarrages par baisse de tension (brownout) ; la tablette tourne sans batterie, sur un port USB d'ordinateur. Cause non vérifiée.

## Résultats précédents : 28/09/2026 (3.2.0)

Firmware **3.2.0** compilé exactement comme le publié (ESPHome 2026.9.0), le **28/09/2026** entre 20 h 35 et 22 h 52. Luminosité 36/255, page d'accueil, mode météo, pièces définies, micro coupé au départ (personne à la maison). Une seule palette : les thèmes sont arrivés avec la 3.6.0.

| Quoi | Mesuré | Essais |
|---|---|---|
| Image la plus longue d'une minute, au repos | 16,7-20,4 ms | 8 minutes |
| Une valeur modifiée (tuile, minute de l'horloge) | moins de 10 ms | journal de diagnostic |
| Panneau tournant du centre (toutes les 8 s, 6-7 images) | 13-21 ms par image | journal de diagnostic |
| Écran entier redessiné (écran rallumé) | 133,2-134,0 ms | 3 |
| Ouverture du calendrier | 126,2-127,2 ms | 2 |
| Ouverture de l'assistant vocal | 162,1 ms | 1 |
| Ouverture de la console système | 167,2-169,9 ms | 2 |
| Ouverture des plantes | 170,4 ms | 1 |
| Ouverture de la télécommande TV | 171,1 ms | 1 |
| Ouverture de la clim | 190,2-190,4 ms | 2 |
| Ouverture du réveil | 197,4 ms | 1 |
| Redémarrage → Home Assistant reconnecté | 16,4-17,0 s | 3 |
| Mise à jour par le Wi-Fi (3,4 Mo) | 12,5-13,7 s | 3 |
| RAM interne libre | 286,5-287,3 Ko | toute la campagne |
| Pile libre de la boucle principale, au plus bas | 10 212 octets | toute la campagne |
| Image du firmware | 3 358 098 octets (41 % de l'emplacement de 7,75 Mo) | compilation |

Par rapport à la dernière campagne du même type (2.x, 26/09/2026) : l'écran entier passe de 163 à 133 ms, le calendrier de 223 à 126 ms, la clim de 216 à 190 ms, la console de 191 à 167 ms. L'image au repos dépend de ce qui est affiché, pas de la version : le panneau tournant ne tourne que s'il a au moins deux choses à montrer (planning, pluie, alertes…), et un firmware du 27/09/2026 mesuré le même soir donnait les mêmes 20 ms.

## Méthode

- **Image la plus longue** : le capteur `Tab5 Draw Max` (`Tab5/paquets/tab5-sensors-diagnostics.yaml`) garde l'image la plus longue de chaque minute, de `LV_EVENT_RENDER_START` à `LV_EVENT_REFR_READY` (dessin et envoi à l'écran). Il est `internal: true` dans le firmware publié ; le build de mesure retire seulement cette ligne.
- **Les actions** partent de Home Assistant au milieu d'une fenêtre d'une minute (rétroéclairage éteint puis rallumé, select « Aller à l'écran »), jamais dans une minute où le blueprint pousse ses mesures de 5 minutes. Les popups ouverts ainsi ne sont pas appuyés : l'effet d'appui des boutons n'est pas dans ces chiffres.
- **Où part le temps** : un build de diagnostic local écrit au journal, pour chaque image de 8 ms ou plus, sa durée et les rectangles que LVGL a redessinés (`inv_areas` du display à `LV_EVENT_RENDER_START`). C'est ainsi que le panneau tournant (un bandeau de 1180 × 86 px redessiné par son glissement et son fondu de 190 ms) a été identifié comme le coût au repos.
- **Redémarrage** : bouton « Redémarrage système », puis temps jusqu'au retour de `ha_api_status` à `on` dans Home Assistant.
- **Charge CPU** (console système, 06/10/2026, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : pour chaque cœur, la part des 2 dernières secondes passée *hors* de sa tâche inactive de FreeRTOS. FreeRTOS compte le temps de chaque tâche à chaque changement de tâche une fois `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` activée (`Tab5/paquets/tab5-hardware.yaml`, horloge esp_timer, 1 µs) ; `update_console_cpu_ui()` (`Tab5/ecran/tab5_console.cpp`) lit les deux compteurs de la tâche inactive seulement console ouverte. Une tâche inactive qui ne cède jamais la main ne verrait pas son compteur avancer : avant chaque lecture, un appel `esp_ipc` vers l'autre cœur la fait céder. Coût de l'option, estimé et non mesuré sur la tablette : une lecture d'esp_timer par changement de tâche, 12 octets par tâche et quelques octets par file. Pas un profileur : il dit combien chaque cœur travaille, pas quelle tâche.

## Où part le temps (29/09/2026)

Un build de diagnostic local a chronométré, pour chaque image de 8 ms ou plus, la part d'envoi à l'écran (`LV_EVENT_FLUSH_START` → `FLUSH_FINISH` de LVGL : ESPHome tourne l'image avec l'accélérateur 2D du P4, puis la copie dans le framebuffer) et la part de dessin.

| Image | Envoi | Dessin, 1 cœur | Dessin, 2 cœurs |
|---|---|---|---|
| Écran entier | 58,7 ms | 74,4 ms | 67,8 ms |
| Popup clim | 59 ms | 130,4 ms | 116,3 ms |
| Console système | 59 ms | 107,5 ms | 98,0 ms |
| Calendrier | 59 ms | 67,2 ms | 62,8 ms |
| Panneau tournant (une image) | 8,4 ms | 7,4 ms | 7,1 ms |
| Flipper (portrait, sans rotation) | 1,2 ms | 9,2 ms | 8,7 ms |

- **Envoyer un écran entier coûte ~59 ms fixes** (1,8 Mo tournés puis copiés en PSRAM), quel que soit le contenu.
- **Le dessin sur les deux cœurs du P4** (`LV_USE_OS` FreeRTOS, 2 unités de dessin, seulement par `esphome: platformio_options: build_flags` en ESPHome 2026.9) marche, sans défaut visible, mais ne gagne que 4 à 11 % du dessin : pas retenu (LVGL 9.5 a des bugs connus en dessin multi-cœur, corrigés en 9.6 ; 16 Ko de RAM interne en plus).
- **La boucle principale n'attend que LVGL** : `runtime_stats` d'ESPHome donne, au repos, un tour le plus long de 28 ms, entièrement LVGL (un pas du panneau tournant) ; API, Wi-Fi et capteurs restent sous 0,3 ms. Le capteur « Tab5 Loop Time » y ajoute l'attente entre deux tours. Le panneau tournant ne tourne que s'il a deux choses à montrer — la pluie dans l'heure en est une —, d'où des chiffres au repos qui changent avec la météo.

### Panneau tournant : seul le texte glisse (29/09/2026)

Chaque panneau du rotateur fait toute la largeur de la carte (son bouton invisible de 1180 px) : faire glisser le panneau redessinait le bandeau entier à chaque image. Faire glisser son contenu (`transition_widgets()`, même animation de 190 ms et 28 px) ne redessine plus que la largeur du texte, sauf à la première et à la dernière image (panneau affiché, panneau masqué). Même tablette, même matinée, une rotation entre le planning et la pluie dans l'heure :

| Rotation (190 ms) | Images | Temps par rotation | Pixels redessinés |
|---|---|---|---|
| Le panneau entier glisse | 6-7 | 107,8 ms | 604 000 |
| Seul le texte glisse (714 px de large) | 7 | 93,4 ms (−13 %) | 475 000 |
| Seul le texte glisse (616 px de large) | 7 | 86,4 ms (−20 %) | 447 000 |

Le gain vient de l'envoi (proportionnel à la surface) ; dessiner un texte à demi transparent coûte à peu près autant que le bandeau uni.

## Démarrage, phase par phase (01/10/2026)

Console USB lue sans réinitialiser la puce (recette de `tools/capture_serie.py`, chaque ligne horodatée à sa réception), redémarrages par le bouton « Redémarrage Système » de Home Assistant. Origine : la tablette ferme sa liaison API (`Rebooting safely`), au moment où Home Assistant la marque indisponible. En secondes, identiques à 0,05 s près d'un redémarrage à l'autre.

| Étape | 3.3.2 (3 redémarrages) | Depuis le 01/10/2026 (3) |
|---|---|---|
| Chargeur fini, firmware chargé (3,3 Mo) | 0,83 | 0,82 |
| ESP-IDF lancé (PSRAM, code copié en PSRAM) | 2,25 | 1,56 (sans test de la PSRAM) |
| Écran construit (objets LVGL, avant `setup()`) | 3,44 | 2,76 |
| `setup()` fini (attente bloquante de 1 s comprise, écran, audio) | 8,43 | 7,80 |
| Première image à l'écran (0,85 s de dessin) | 9,29 | 8,65 |
| Wi-Fi : début de la connexion | 11,03 (après 1,7 s de balayage) | 7,71 (pendant `setup()`) |
| Wi-Fi connecté (adresse obtenue) | 14,85 | 10,55 (sans vérification ARP) |
| `esphome.tab5_connected` reçu par Home Assistant | 18,2-18,4 | 12,0 |
| Poussée complète reçue | 18,4-18,6 | 12,3 |

Ce qui reste : `setup()` (5 s, dont l'attente de 1 s dont l'écran a besoin après un redémarrage logiciel, `[AI-WARNING]` dans `tab5-ha-hmi.yaml`), l'association Wi-Fi par l'ESP32-C6 (2,8 s) et la reconnexion de Home Assistant elle-même (1,4 s après le Wi-Fi, pas pilotée par la tablette).

### Chronologie du démarrage, lue dans Home Assistant (prochaine version)

Une tablette sur chargeur secteur n'a pas de journal série. Deux entités de diagnostic donnent, à chaque démarrage, l'instant d'étapes fixes : **Tab5 Chronologie du démarrage** jusqu'à la fin du `setup()` d'ESPHome, **Tab5 Chronologie du démarrage (suite)** après (deux textes parce que Home Assistant garde au plus 255 caractères dans un état). Même format, `nom=ms; …`, « - » pour une étape pas (encore) atteinte :

`ctor=…; objets=…; setup=…; bus=…; expandeur=…; avant1s=…; apres1s=…; p600=…; donnees=…; lvgl=…; wifiinit=…; reseau=…; ecoute=…; fin=…; i18n=…; zones=…`
`ordo=…; retro=…; dessin=…; image=…; tard=…; wifi=…; api=…; fin600=…; chargeur=…`

**Horloge.** Chaque valeur vaut `esp_timer_get_time() / 1000` : ESP-IDF remet ce compteur matériel à zéro à son lancement (`esp_timer_impl_early_init`, étape CORE, avant les constructeurs C++) ; ni la ROM ni le chargeur de démarrage ne sont comptés (0,8 s dans le tableau ci-dessus). Le `millis()` d'ESPHome est autre chose : le compte de ticks de FreeRTOS (`CONFIG_FREERTOS_HZ=1000`, `esp32/hal.cpp`), qui part avec l'ordonnanceur. `ordo` est l'instant où `millis()` valait 0, mesuré une fois à `setup` : une valeur lue avec `millis()` (la première version de cette entité, une ligne de journal) vaut `valeur − ordo` sur cette échelle. Un tick perdu pendant le démarrage agrandirait un peu `ordo` (pas vu, pas exclu).

Comment ESPHome démarre, pour lire les écarts : son `main()` construit tous les objets (les ~1 800 widgets LVGL compris), puis appelle `App.setup()`, qui lance le `setup()` de chaque composant par priorité décroissante (ordre d'enregistrement à égalité). Les entrées `on_boot` sont des composants elles aussi, lancées à leur priorité : les entrées qui ne font que marquer (fin de la liste `on_boot` de `tab5-ha-hmi.yaml`, à des priorités que rien d'autre n'a) bornent donc les bandes de priorité. L'écart entre deux marques qui se suivent est la durée des `setup()` d'entre elles.

| Nom | Où | Ce que contient l'écart qui le précède |
|---|---|---|
| `ctor` | constructeur C++ global de `Tab5/ecran/tab5_demarrage.cpp` (`do_global_ctors`, avant l'ordonnanceur) | le lancement d'ESP-IDF : PSRAM, code copié en PSRAM. Le constructeur d'ESP-Hosted (lien SDIO vers le C6) tourne dans la même phase, dans un ordre que rien ne fixe |
| `objets` | lambda de `style_repere_demarrage`, premier style (`tab5-styles.yaml`), créé juste après l'initialisation de LVGL | lancement de l'ordonnanceur, `app_main`, construction des composants ESPHome (sans entrée-sortie) |
| `setup` | `on_boot` priorité 2000, le premier `setup()` | **construction des widgets LVGL** (et des objets restants) |
| `bus` | `on_boot` 801 | bus I²C, LDO 2,5 V, préférences (1000), les deux expandeurs (900) |
| `expandeur` | interrupteur d'alimentation USB (`usb_5v_power`, expandeur 0x44 P3), bande matérielle (800) | début de la bande 800 : globals et selects relus en NVS, interrupteurs… dans l'ordre d'enregistrement |
| `avant1s` / `apres1s` | `on_boot` 700, avant / après le `delay(1000)` bloquant (`[AI-WARNING-CRITICAL]`, laissé tel quel) | `avant1s` : reste de la bande 800 et la lumière du rétroéclairage (799) ; `apres1s` : l'attente de 1 s elle-même |
| `p600` | première action de l'`on_boot` 600 | rien (enregistré avant l'écran) |
| `donnees` | `on_boot` 500 | **toute la bande 600** : écran (MIPI-DSI, reset par l'expandeur), tactile, ES7210, micro, ES8388, INA226, centrale inertielle, capteurs, et le début de l'`on_boot` 600 (`lvgl.resume`, rétroéclairage allumé). Même priorité 600 : aucune marque ne peut les séparer sans changer une priorité |
| `lvgl` | `on_boot` 399 | bande 400, dans cet ordre : haut-parleur (I²S), **`setup()` de LVGL**, lecteur |
| `wifiinit` | `on_boot` 249 | lambdas différées (395) et animations (380) de LVGL, réseau (300), **`setup()` du Wi-Fi** (250), qui passe par le C6 |
| `reseau` | `on_boot` 199 | SNTP (220), API, OTA, mode sans échec (200) |
| `ecoute` | `on_boot` 99 | micro_wake_word, assistant vocal (100) |
| `fin` | première action de l'`on_boot` -100 | le reste jusqu'à -100 (composant debug) |
| `i18n` / `zones` | après `i18n_apply_boot()` / après le script `tab5_zones_apply`, `on_boot` -100 | langue de l'écran ; zones optionnelles (tuiles, clim, rangée, tuile − / +). La fin de `setup()` suit |
| `ordo` | calculé à `setup` | — (voir Horloge) |
| `retro` | première écriture non nulle du PWM du rétroéclairage (`tab5-hardware.yaml`) | il s'allume vraiment |
| `dessin` / `image` | `lvgl: on_draw_start` / premier `on_draw_end` | premier dessin de LVGL, fin de la première image envoyée à l'écran |
| `tard` | fin de l'`on_boot` -100 (son `delay: 2s`, puis l'agrandissement au toucher et les pointeurs de l'horloge) | — |
| `wifi` / `api` | Wi-Fi connecté / premier client de l'API (Home Assistant), la première fois | — |
| `fin600` | fin de l'`on_boot` 600 (Home Assistant attendu, `esphome.tab5_connected`, `delay: 3s`, préchargement du calendrier) | — |
| `chargeur` | premier allumage du chargeur de la batterie ; « - » quand le démarrage précédent n'a vu aucune batterie ([matériel](hardware.md#version-française), Batterie) | — |

Les marques du premier texte ne publient jamais : les deux textes partent à la première image, puis à chaque étape suivante (`tard`, `wifi`, `api`, `fin600`, `chargeur`), jamais pendant `setup()` ; Home Assistant garde une valeur par démarrage dans son historique. `retro`, `dessin` et `image` viennent normalement après `fin` (ESPHome ne lance les boucles qu'après `setup()`, sauf si un composant fait attendre `setup()`) : les comparer à `fin` pour le savoir. Marquer une étape coûte une lecture d'horloge et une écriture en mémoire. Source : `Tab5/socle/tab5_demarrage.h` ; câblage relu par `tests/test_chronologie_demarrage.py`.

## Limites

- Une tablette, une révision d'écran, une soirée par campagne.
- Pas mesurées : latence tactile, latence vocale (du mot d'activation à la réponse). La consommation a sa page : [consommation](consumption.md#version-française).
- L'essentiel du temps d'un redessin complet est du rendu logiciel sur un seul cœur ; ESPHome 2026.9 dessine avec un seul tampon, attend chaque envoi, et n'utilise l'accélérateur 2D du P4 que pour la rotation.
