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

## Limites

- Une tablette, une révision d'écran, une soirée.
- Pas mesurés : latence tactile, latence vocale (du mot d'activation à la réponse), consommation.
- L'essentiel du temps d'un redessin complet est du rendu logiciel sur un seul cœur ; ESPHome 2026.9 dessine avec un seul tampon, attend chaque envoi, et n'utilise l'accélérateur 2D du P4 que pour la rotation.
