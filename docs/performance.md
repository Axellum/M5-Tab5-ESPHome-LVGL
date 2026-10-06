# Performance

## English · [Français](#version-française)

---

Measured on the author's tablet (Tab5, ST7123 screen, ESP32-P4 revision v1.3 at 360 MHz), firmware **3.2.0** built exactly like the published one (ESPHome 2026.9.0), on **2026-09-28** between 20:35 and 22:52. Screen brightness 36/255, home page, weather mode, rooms defined, microphone off at the start (nobody home).

## Results

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

- **Longest frame**: the `Tab5 Draw Max` sensor (`Tab5/tab5-sensors-diagnostics.yaml`) keeps the longest frame of each minute, from `LV_EVENT_RENDER_START` to `LV_EVENT_REFR_READY` (drawing plus sending to the screen). It is `internal: true` in the published firmware; the measuring build only removes that line.
- **Actions** are sent from Home Assistant in the middle of a one-minute window (backlight off then on, « Aller à l'écran » select), never in a minute where the blueprint pushes its 5-minute measurements. Popups opened this way are not pressed: button feedback is not part of these numbers.
- **Where the time goes**: a local diagnostic build logs, for every frame of 8 ms or more, its duration and the rectangles LVGL redrew (`inv_areas` of the display at `LV_EVENT_RENDER_START`). That is how the rotating panel (a 1180 × 86 px band redrawn by its 190 ms slide and fade) was found to be the idle cost.
- **Restart**: button « Redémarrage système », then time until `ha_api_status` is `on` again in Home Assistant.
- **CPU load** (system console, 2026-10-06, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): for each core, the share of the last 2 seconds *not* spent in its FreeRTOS idle task. FreeRTOS counts each task's running time at every task switch once `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` is on (`Tab5/tab5-hardware.yaml`, esp_timer clock, 1 µs); `update_console_cpu_ui()` (`Tab5/tab5_console.cpp`) reads the two idle counters only while the console is open. An idle task that never yields would not have its counter updated: before each reading, an `esp_ipc` call to the other core makes its idle task yield. Cost of the option, estimated and not measured on the tablet: one esp_timer read per task switch, 12 bytes per task and a few bytes per queue. Not a profiler: it says how busy each core is, not which task.

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

## Limits

- One tablet, one screen revision, one evening.
- Not measured: touch latency, voice latency (wake word to answer), power draw.
- Most of the time of a full-screen redraw is software rendering on one core; ESPHome 2026.9 draws with a single buffer and waits for each transfer, and uses the P4's 2D accelerator for rotation only.

---

## Version française

Mesuré sur la tablette de l'auteur (Tab5, écran ST7123, ESP32-P4 révision v1.3 à 360 MHz), firmware **3.2.0** compilé exactement comme le publié (ESPHome 2026.9.0), le **28/09/2026** entre 20 h 35 et 22 h 52. Luminosité 36/255, page d'accueil, mode météo, pièces définies, micro coupé au départ (personne à la maison).

## Résultats

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

- **Image la plus longue** : le capteur `Tab5 Draw Max` (`Tab5/tab5-sensors-diagnostics.yaml`) garde l'image la plus longue de chaque minute, de `LV_EVENT_RENDER_START` à `LV_EVENT_REFR_READY` (dessin et envoi à l'écran). Il est `internal: true` dans le firmware publié ; le build de mesure retire seulement cette ligne.
- **Les actions** partent de Home Assistant au milieu d'une fenêtre d'une minute (rétroéclairage éteint puis rallumé, select « Aller à l'écran »), jamais dans une minute où le blueprint pousse ses mesures de 5 minutes. Les popups ouverts ainsi ne sont pas appuyés : l'effet d'appui des boutons n'est pas dans ces chiffres.
- **Où part le temps** : un build de diagnostic local écrit au journal, pour chaque image de 8 ms ou plus, sa durée et les rectangles que LVGL a redessinés (`inv_areas` du display à `LV_EVENT_RENDER_START`). C'est ainsi que le panneau tournant (un bandeau de 1180 × 86 px redessiné par son glissement et son fondu de 190 ms) a été identifié comme le coût au repos.
- **Redémarrage** : bouton « Redémarrage système », puis temps jusqu'au retour de `ha_api_status` à `on` dans Home Assistant.
- **Charge CPU** (console système, 06/10/2026, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : pour chaque cœur, la part des 2 dernières secondes passée *hors* de sa tâche inactive de FreeRTOS. FreeRTOS compte le temps de chaque tâche à chaque changement de tâche une fois `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` activée (`Tab5/tab5-hardware.yaml`, horloge esp_timer, 1 µs) ; `update_console_cpu_ui()` (`Tab5/tab5_console.cpp`) lit les deux compteurs de la tâche inactive seulement console ouverte. Une tâche inactive qui ne cède jamais la main ne verrait pas son compteur avancer : avant chaque lecture, un appel `esp_ipc` vers l'autre cœur la fait céder. Coût de l'option, estimé et non mesuré sur la tablette : une lecture d'esp_timer par changement de tâche, 12 octets par tâche et quelques octets par file. Pas un profileur : il dit combien chaque cœur travaille, pas quelle tâche.

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

## Limites

- Une tablette, une révision d'écran, une soirée.
- Pas mesurés : latence tactile, latence vocale (du mot d'activation à la réponse), consommation.
- L'essentiel du temps d'un redessin complet est du rendu logiciel sur un seul cœur ; ESPHome 2026.9 dessine avec un seul tampon, attend chaque envoi, et n'utilise l'accélérateur 2D du P4 que pour la rotation.
