# Waveshare ESP32-P4 7B (experimental)

## English · [Français](#version-française)

---

The same firmware also runs on a second board, the **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B**: a 7-inch, 1024×600 touch screen built around the same chips as the Tab5 (an ESP32-P4, an ESP32-C6 for Wi-Fi, an ES7210 for the microphones). It is a cheaper and bigger screen, with less on board: no battery, no motion sensor, no clock chip.

**Status: experimental.** The author's board has been the daily screen of their home since 2026-10-14: display, touch, backlight, sound and Wi-Fi work, and Home Assistant sees it like a Tab5. One board only, though, and the games and one energy page are not adapted to its screen yet ([what is left](#what-is-not-done-yet)). If you try it, say how it went in [Discussions → Hardware compatibility](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/categories/hardware-compatibility): it helps a lot.

The choice of the board is one line at build time ([ADR-0061](decisions/0061-board-and-screen-size.md)); on the Tab5, nothing changes.

## What changes on screen

The home screen is the Tab5's layout brought to 1024×600: same places, same proportions, smaller. The large fonts are scaled by 0.82 (about 1024 / 1280 and 600 / 720), the small ones keep their size so they stay readable. The popups do the same: each keeps the Tab5's layout, smaller. Where a fixed-size text no longer fitted, it moves a little instead (the plant rows lose some margin, a rain amount drops its « mm » in a narrow weather column, the « On / Off » key of the remote wraps onto two lines); the camera pictures are asked at 720 × 405 instead of 960 × 540. The details are in [ADR-0061](decisions/0061-board-and-screen-size.md).

| Home screen | Lights | Energy |
|---|---|---|
| ![The home screen on the 7B at 1024×600, in French: clock, Ok Nabu frame, centre card and the five forecast cards](images/waveshare/7b-accueil.webp) | ![The Lights popup on the 7B, in French: bulbs, brightness ring and colours, all inside the screen](images/waveshare/7b-lumieres.webp) | ![The Energy popup on the 7B, Today page, in French: the sun's course, the production bars and the best window](images/waveshare/7b-energie.webp) |

These pictures come from the off-device render (the firmware running on a PC, [debugging](debugging.md)), which draws every screen at 1024×600 on each firmware change; they are the same pixels as on the board, without the touch. The render of this board is in French only.

What the board does not have, and what it means on screen:

- **No motion sensor (IMU):** the games played by tilting (Fil d'Or, Arcanoïde, Coureur d'Or, Neon Apron) get no input; the others work with touch.
- **No battery and no battery gauge:** the battery icon, the charging settings and the battery entities stay, without effect (« Tab5 Batterie » stays unknown). Energy saving « on battery » never triggers.
- **No clock chip:** the time comes from the network at each start. After a power cut, the alarm clock waits for Wi-Fi. (The board has a coin-cell holder for the P4's own clock; this firmware does not use it.)
- **No headphone jack detection, no switchable power rails:** their switches are kept so Home Assistant finds the same entities, and do nothing.

## Install it

There is no install page for this board yet: you build the firmware yourself. It is the [Build your own firmware](installation/build.md) page with three differences, and the [Home Assistant steps](installation/README.md) stay the same (files, adding the device, sources, the blueprint).

1. **ESPHome 2026.9.1 or newer.** 2026.9.0 crashes while starting the display of this board's ESP32-P4 revision 3 ([esphome#18984](https://github.com/esphome/esphome/pull/18984)); the build stops with a clear message under 2026.9.1. The Tab5 itself still builds from 2026.9.0.
2. **One line** in `Tab5/user_entities.yaml`: `tab5_carte: waveshare-7b` (or `-s tab5_carte waveshare-7b` on the command line).
3. **First flash over USB**: plug the USB-C port marked for the ESP32-P4 into the computer, then `esphome run tab5-ha-hmi.yaml`. Wi-Fi exactly as on the Tab5: the open **« Tab5 Fallback AP »** network with a phone, or Improv from [ESPHome Web](https://web.esphome.io). Later updates go over Wi-Fi. <!-- pragma: allowlist secret -->

Before the first flash, if you care about the factory demo, back up the flash with `esptool read-flash`: on the author's board, each read restarts the board, and a 4 KB block at 0x37C000 could not be read (filled with zeros in the backup).

The board announces itself in Home Assistant with the Tab5's name and model (`tab5-ha-hmi`): the packages and the blueprint find it with nothing to change. **Replacing a Tab5 in the same Home Assistant?** Disable the Tab5's ESPHome entry first. Otherwise Home Assistant keeps both, gives the new board's entities a `_2` suffix, and the pushes keep going to the old one.

## Problems met, and how they were solved

What happened on the author's board when it arrived (2026-10-14), in order. Everything here is already in the firmware: you should not meet these again.

| What we saw | Cause | Fix |
|---|---|---|
| Boot loop, then safe mode | ESPHome 2026.9.0 aborts while setting up the display clock of a P4 revision 3 (`mipi_dsi_ll_set_phy_pllref_clock_source`) | ESPHome 2026.9.1; the build of this board refuses an older ESPHome |
| No Wi-Fi, `sdmmc_card_init failed` | the reset of the ESP32-C6 was set active low | `esp32_hosted: active_high: true`, as on the Tab5 (same wiring) |
| About 30 « lcd.dsi underrun » per second (first trial firmware) | the PSRAM ran at its default speed | `psram: speed: 200MHz` |
| Touch almost dead: one touch recognised in 5 minutes, the brightness slider frozen | the panel is mounted upside down compared to the driver's default, and the touch controller was never reset (Waveshare's own code says its reset pin is not connected) | LVGL `rotation: 180`, the GT911 mirrored in X and Y, and its reset on GPIO 23, as in the other public configurations that work on this board: then every touch lands in the right place |
| The first sound was only a click | the microphone kept the I2S bus the speaker shares with it; the exact cause of the click was not established | the speaker at 48 kHz like the other configurations, the shared-bus rule of the Tab5 ([ADR-0010](decisions/0010-shared-i2s-bus-mic-speaker.md)): sound works since that evening |
| The board restarted every 15 minutes before Home Assistant knew it | ESPHome's default `reboot_timeout` while no API client is connected (first trial firmware) | the full firmware waits 60 minutes, like the Tab5 |

Two things learnt along the way:

- **The backlight is dimmed in analog, not chopped.** GPIO 32 does not switch the LED driver (AP3032) on and off: it goes through an RC filter (10 kΩ, 100 nF) into the driver's feedback pin, so the PWM becomes a steady voltage that shifts the LED current. A higher PWM frequency is smoother: the firmware uses 5 kHz, like Waveshare's code.
- **Start from the configurations that already work.** The three touch fixes were all in public ESPHome configurations of this board ([agillis/esphome-modular-lvgl-buttons](https://github.com/agillis/esphome-modular-lvgl-buttons), [Jordi1990/openamber](https://github.com/Jordi1990/openamber), the Home Assistant forum), not in the manufacturer's code. Reading them first would have saved an evening.

## What is not done yet

- **Not checked on the board:** the microphone and the wake word, the cameras popup (JPEG decoding), every game, a power cut without network.
- **Not adapted to 1024×600 yet:** the games (the next lot of [ADR-0061](decisions/0061-board-and-screen-size.md)), and the « Balance » page of the Energy popup, which came after the popups were adapted and runs past the bottom edge. For every other screen, the render counts on each change what runs past its area: about as much as on the Tab5 itself, mostly the shutters, alarm clock and cameras popups, which do the same there.
- Reported by others on this board, not seen here: a faint glow with the backlight at 0 %, a distorted speaker.
- Start-up takes about 20 s before the first image, like the Tab5.

## Hardware

| | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B |
|---|---|
| Main processor | ESP32-P4 (ESP32-P4NRW32), revision **v3.2** on the author's board, run at 360 MHz like the Tab5 (400 MHz is « experimental » for ESPHome) |
| Memory | 32 MB PSRAM (hex mode, 200 MHz), 32 MB flash |
| Display | 7 inches, 1024×600, landscape panel, EK79007 controller over MIPI-DSI (2 lanes), ESPHome model `WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B` |
| Touch | GT911, I2C at 0x5D, polled every 50 ms (no interrupt) |
| Backlight | AP3032 LED driver, analog dimming from GPIO 32 (LEDC, 5 kHz, inverted) |
| Sound out | ES8311 DAC (0x18), NS4150B amplifier enabled by GPIO 53, a two-pin speaker connector |
| Microphones | two, through an ES7210 ADC (0x40), 16 kHz / 16-bit like the Tab5 |
| Wi-Fi and Bluetooth | ESP32-C6-MINI-1 over SDIO (ESP-Hosted), reset on GPIO 54 |
| Not on this board | battery and gauge, motion sensor, clock chip, I/O expanders, headphone jack |
| Also on the board, not used | microSD slot, MIPI-CSI camera connector, USB-A, I2C / UART / CAN / RS485 connectors |

Everything the firmware sets for this board is in `Tab5/paquets/carte-waveshare-7b.yaml`, its screen size in `Tab5/paquets/geometrie-waveshare-7b.yaml`. What does not depend on the board (logger, OTA, energy saving, media player…) is copied from the Tab5's file, and `tests/test_carte_7b.py` keeps both equal.

### Pin map

Every pin the board's file configures. The display reset (GPIO 33) is set by ESPHome's display model, not in the YAML. As for the Tab5, `tests/test_doc_broches.py` checks every row, in both languages, against `carte-waveshare-7b.yaml`.

| Function | Pin | In the YAML |
|---|---|---|
| I2C bus, SDA (touch and codecs, 400 kHz) | GPIO 7 | `bsp_bus` · `sda` |
| I2C bus, SCL | GPIO 8 | `bsp_bus` · `scl` |
| Touch reset | GPIO 23 | `touch` · `reset_pin` |
| Backlight PWM (LEDC, 5 kHz, inverted) | GPIO 32 | `backlight_pwm` · `pin` |
| I2S word select (microphones and speaker) | GPIO 10 | `mic_bus` · `i2s_lrclk_pin` |
| I2S bit clock | GPIO 12 | `mic_bus` · `i2s_bclk_pin` |
| I2S master clock | GPIO 13 | `mic_bus` · `i2s_mclk_pin` |
| I2S data in, from the ES7210 (microphones) | GPIO 11 | `tab5_microphone` · `i2s_din_pin` |
| I2S data out, to the ES8311 (speaker) | GPIO 9 | `tab5_speaker` · `i2s_dout_pin` |
| Speaker amplifier enable | GPIO 53 | `speaker_enable` · `pin` |
| ESP32-C6 link (SDIO), clock | GPIO 18 | `esp32_hosted` · `clk_pin` |
| ESP32-C6 link (SDIO), command | GPIO 19 | `esp32_hosted` · `cmd_pin` |
| ESP32-C6 link (SDIO), data 0 | GPIO 14 | `esp32_hosted` · `d0_pin` |
| ESP32-C6 link (SDIO), data 1 | GPIO 15 | `esp32_hosted` · `d1_pin` |
| ESP32-C6 link (SDIO), data 2 | GPIO 16 | `esp32_hosted` · `d2_pin` |
| ESP32-C6 link (SDIO), data 3 | GPIO 17 | `esp32_hosted` · `d3_pin` |
| ESP32-C6 reset | GPIO 54 | `esp32_hosted` · `reset_pin` |

Sources: Waveshare's board support package and schematic, ESPHome's display model, and the public configurations named above. The pins of the C6 link are ESP-Hosted's defaults for the P4, which this board follows.

---

---

## Version Française

---

Le même firmware tourne aussi sur une deuxième carte, la **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** : un écran tactile de 7 pouces en 1024×600, construit autour des mêmes puces que le Tab5 (un ESP32-P4, un ESP32-C6 pour le Wi-Fi, un ES7210 pour les micros). Un écran moins cher et plus grand, avec moins de choses à bord : pas de batterie, pas de capteur de mouvement, pas de puce d'horloge.

**Statut : expérimental.** La carte de l'auteur est l'écran de tous les jours de sa maison depuis le 14/10/2026 : écran, tactile, rétroéclairage, son et Wi-Fi marchent, et Home Assistant la voit comme un Tab5. Une seule carte, cela dit, et les jeux et une page de l'énergie ne sont pas encore adaptés à son écran ([ce qui reste](#ce-qui-reste-à-faire)). Si vous l'essayez, racontez ce que ça a donné dans [Discussions → Hardware compatibility](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/categories/hardware-compatibility) : ça aide beaucoup.

Le choix de la carte tient en une ligne à la compilation ([ADR-0061](decisions/0061-board-and-screen-size.md)) ; sur le Tab5, rien ne change.

## Ce qui change à l'écran

L'accueil reprend la mise en page du Tab5, ramenée à 1024×600 : mêmes places, mêmes proportions, en plus petit. Les grosses polices sont réduites à 0,82 (à peu près 1024 / 1280 et 600 / 720), les petites gardent leur taille pour rester lisibles. Les popups font de même : chacun garde la mise en page du Tab5, en plus petit. Là où un texte de taille fixe ne tenait plus, il se décale un peu (les lignes des plantes perdent un peu de marge, une quantité de pluie perd son « mm » dans une colonne météo étroite, la touche « Marche / Arrêt » de la télécommande passe sur deux lignes) ; les images des caméras sont demandées en 720 × 405 au lieu de 960 × 540. Le détail est dans l'[ADR-0061](decisions/0061-board-and-screen-size.md).

| Accueil | Lumières | Énergie |
|---|---|---|
| ![L'accueil sur la 7B en 1024×600 : horloge, cadre Ok Nabu, carte centrale et les cinq cartes de prévisions](images/waveshare/7b-accueil.webp) | ![Le popup Lumières sur la 7B : ampoules, anneau de luminosité et couleurs, tout dans l'écran](images/waveshare/7b-lumieres.webp) | ![Le popup Énergie sur la 7B, page Aujourd'hui : la course du soleil, les barres de production et le meilleur créneau](images/waveshare/7b-energie.webp) |

Ces images viennent du rendu hors tablette (le firmware qui tourne sur un PC, [diagnostiquer](debugging.md#version-française)), qui dessine chaque écran en 1024×600 à chaque changement du firmware : ce sont les mêmes pixels que sur la carte, sans le tactile. Le rendu de cette carte n'existe qu'en français.

Ce que la carte n'a pas, et ce que ça donne à l'écran :

- **Pas de capteur de mouvement (IMU) :** les jeux qui se jouent en inclinant (Fil d'Or, Arcanoïde, Coureur d'Or, Neon Apron) ne reçoivent rien ; les autres se jouent au doigt.
- **Pas de batterie ni de jauge :** l'icône de batterie, les réglages de charge et les entités de batterie restent, sans effet (« Tab5 Batterie » reste inconnue). L'économie d'énergie « sur batterie » ne se déclenche jamais.
- **Pas de puce d'horloge :** l'heure vient du réseau à chaque démarrage. Après une coupure de courant, le réveil attend le Wi-Fi. (La carte a un support de pile pour l'horloge propre du P4 ; ce firmware ne s'en sert pas.)
- **Pas de détection de prise casque, pas d'alimentations commutables :** leurs interrupteurs sont gardés pour que Home Assistant retrouve les mêmes entités, et ne font rien.

## L'installer

Pas encore de page d'installation pour cette carte : on compile le firmware soi-même. C'est la page [Compiler son propre firmware](installation/build.md#version-française) avec trois différences, et les [étapes Home Assistant](installation/README.md#version-française) restent les mêmes (fichiers, ajout de l'appareil, sources, blueprint).

1. **ESPHome 2026.9.1 ou plus récent.** La 2026.9.0 plante au démarrage de l'écran de l'ESP32-P4 révision 3 de cette carte ([esphome#18984](https://github.com/esphome/esphome/pull/18984)) ; en dessous de 2026.9.1, la compilation s'arrête avec un message clair. Le Tab5, lui, se compile toujours à partir de 2026.9.0.
2. **Une ligne** dans `Tab5/user_entities.yaml` : `tab5_carte: waveshare-7b` (ou `-s tab5_carte waveshare-7b` en ligne de commande).
3. **Premier flash par l'USB** : branchez sur l'ordinateur le port USB-C de l'ESP32-P4, puis `esphome run tab5-ha-hmi.yaml`. Le Wi-Fi exactement comme sur le Tab5 : le réseau ouvert **« Tab5 Fallback AP »** avec un téléphone, ou Improv depuis [ESPHome Web](https://web.esphome.io). Les mises à jour suivantes passent par le Wi-Fi. <!-- pragma: allowlist secret -->

Avant le premier flash, si vous tenez à la démo d'usine, sauvegardez la flash avec `esptool read-flash` : sur la carte de l'auteur, chaque lecture redémarre la carte, et un bloc de 4 Ko à 0x37C000 n'a pas pu être lu (rempli de zéros dans la sauvegarde).

La carte s'annonce dans Home Assistant avec le nom et le modèle du Tab5 (`tab5-ha-hmi`) : les packages et le blueprint la trouvent sans rien changer. **Elle remplace un Tab5 dans le même Home Assistant ?** Désactivez d'abord l'entrée ESPHome du Tab5. Sinon Home Assistant garde les deux, ajoute un suffixe `_2` aux entités de la nouvelle carte, et les envois continuent de partir vers l'ancienne.

## Les problèmes rencontrés, et comment ils ont été réglés

Ce qui s'est passé sur la carte de l'auteur à son arrivée (14/10/2026), dans l'ordre. Tout est déjà dans le firmware : vous ne devriez pas les revoir.

| Ce qu'on a vu | Cause | Correction |
|---|---|---|
| Redémarrages en boucle, puis mode sans échec | ESPHome 2026.9.0 plante en réglant l'horloge de l'écran d'un P4 révision 3 (`mipi_dsi_ll_set_phy_pllref_clock_source`) | ESPHome 2026.9.1 ; la compilation de cette carte refuse un ESPHome plus ancien |
| Pas de Wi-Fi, `sdmmc_card_init failed` | le reset de l'ESP32-C6 était réglé actif bas | `esp32_hosted: active_high: true`, comme sur le Tab5 (même câblage) |
| Une trentaine de « lcd.dsi underrun » par seconde (premier firmware d'essai) | la PSRAM tournait à sa vitesse par défaut | `psram: speed: 200MHz` |
| Tactile presque mort : un appui reconnu en 5 minutes, le curseur de luminosité figé | la dalle est montée à l'envers par rapport au défaut du pilote, et le contrôleur tactile n'était jamais réinitialisé (le code de Waveshare dit que sa broche de reset n'est pas reliée) | `rotation: 180` dans LVGL, le GT911 inversé en X et en Y, et son reset sur GPIO 23, comme dans les autres configurations publiques qui marchent sur cette carte : depuis, chaque appui tombe au bon endroit |
| Le premier son n'était qu'un claquement | le micro gardait le bus I2S que le haut-parleur partage avec lui ; la cause exacte du claquement n'a pas été établie | le haut-parleur à 48 kHz comme les autres configurations, la règle du bus partagé du Tab5 ([ADR-0010](decisions/0010-shared-i2s-bus-mic-speaker.md)) : le son marche depuis ce soir-là |
| La carte redémarrait toutes les 15 minutes tant que Home Assistant ne la connaissait pas | le `reboot_timeout` par défaut d'ESPHome sans client de l'API (premier firmware d'essai) | le firmware complet attend 60 minutes, comme le Tab5 |

Deux choses apprises en route :

- **Le rétroéclairage se règle en analogique, pas en hachant.** GPIO 32 n'allume et n'éteint pas le pilote de LED (AP3032) : il passe par un filtre RC (10 kΩ, 100 nF) vers l'entrée de contre-réaction du pilote, et le PWM devient une tension continue qui décale le courant des LED. Plus la fréquence du PWM est haute, plus c'est lisse : le firmware est à 5 kHz, comme le code de Waveshare.
- **Partir des configurations qui marchent déjà.** Les trois corrections du tactile étaient toutes dans des configurations ESPHome publiques de cette carte ([agillis/esphome-modular-lvgl-buttons](https://github.com/agillis/esphome-modular-lvgl-buttons), [Jordi1990/openamber](https://github.com/Jordi1990/openamber), le forum Home Assistant), pas dans le code du fabricant. Les lire d'abord aurait épargné une soirée.

## Ce qui reste à faire

- **Pas vérifié sur la carte :** le micro et le mot d'activation, le popup Caméras (décodage JPEG), chacun des jeux, une coupure de courant sans réseau.
- **Pas encore adapté au 1024×600 :** les jeux (le prochain lot de l'[ADR-0061](decisions/0061-board-and-screen-size.md)), et la page « Bilan » du popup Énergie, arrivée après l'adaptation des popups, qui dépasse du bas. Pour tous les autres écrans, le rendu compte à chaque changement ce qui dépasse de sa zone : à peu près autant que sur le Tab5 lui-même, surtout dans les popups Volets, Réveil et Caméras, qui font pareil là-bas.
- Signalé par d'autres sur cette carte, pas vu ici : une faible lueur avec le rétroéclairage à 0 %, un haut-parleur qui sature.
- Le démarrage prend une vingtaine de secondes avant la première image, comme sur le Tab5.

## Matériel

| | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B |
|---|---|
| Processeur principal | ESP32-P4 (ESP32-P4NRW32), révision **v3.2** sur la carte de l'auteur, à 360 MHz comme le Tab5 (400 MHz est « expérimental » pour ESPHome) |
| Mémoire | 32 Mo de PSRAM (mode hex, 200 MHz), 32 Mo de flash |
| Écran | 7 pouces, 1024×600, dalle en paysage, contrôleur EK79007 en MIPI-DSI (2 voies), modèle ESPHome `WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B` |
| Tactile | GT911, I2C en 0x5D, interrogé toutes les 50 ms (sans interruption) |
| Rétroéclairage | pilote de LED AP3032, gradation analogique depuis GPIO 32 (LEDC, 5 kHz, inversé) |
| Son | DAC ES8311 (0x18), ampli NS4150B activé par GPIO 53, un connecteur deux broches pour le haut-parleur |
| Micros | deux, par un ADC ES7210 (0x40), 16 kHz / 16 bits comme le Tab5 |
| Wi-Fi et Bluetooth | ESP32-C6-MINI-1 en SDIO (ESP-Hosted), reset sur GPIO 54 |
| Absent de cette carte | batterie et jauge, capteur de mouvement, puce d'horloge, expandeurs d'E/S, prise casque |
| Présent, pas utilisé | lecteur microSD, connecteur de caméra MIPI-CSI, USB-A, connecteurs I2C / UART / CAN / RS485 |

Tout ce que le firmware règle pour cette carte est dans `Tab5/paquets/carte-waveshare-7b.yaml`, la taille de son écran dans `Tab5/paquets/geometrie-waveshare-7b.yaml`. Ce qui ne dépend pas de la carte (journal, OTA, économie d'énergie, lecteur…) est recopié du fichier du Tab5, et `tests/test_carte_7b.py` garde les deux égaux.

### Broches

Toutes les broches que le fichier de la carte configure. Le reset de l'écran (GPIO 33) est réglé par le modèle d'écran d'ESPHome, pas dans le YAML. Comme pour le Tab5, `tests/test_doc_broches.py` compare chaque ligne, dans les deux langues, à `carte-waveshare-7b.yaml`.

| Fonction | Broche | Dans le YAML |
|---|---|---|
| Bus I2C, SDA (tactile et codecs, 400 kHz) | GPIO 7 | `bsp_bus` · `sda` |
| Bus I2C, SCL | GPIO 8 | `bsp_bus` · `scl` |
| Reset du tactile | GPIO 23 | `touch` · `reset_pin` |
| PWM du rétroéclairage (LEDC, 5 kHz, inversé) | GPIO 32 | `backlight_pwm` · `pin` |
| Sélection de mot I2S (micros et haut-parleur) | GPIO 10 | `mic_bus` · `i2s_lrclk_pin` |
| Horloge bit I2S | GPIO 12 | `mic_bus` · `i2s_bclk_pin` |
| Horloge maître I2S | GPIO 13 | `mic_bus` · `i2s_mclk_pin` |
| Données I2S en entrée, depuis l'ES7210 (micros) | GPIO 11 | `tab5_microphone` · `i2s_din_pin` |
| Données I2S en sortie, vers l'ES8311 (haut-parleur) | GPIO 9 | `tab5_speaker` · `i2s_dout_pin` |
| Activation de l'ampli du haut-parleur | GPIO 53 | `speaker_enable` · `pin` |
| Liaison ESP32-C6 (SDIO), horloge | GPIO 18 | `esp32_hosted` · `clk_pin` |
| Liaison ESP32-C6 (SDIO), commande | GPIO 19 | `esp32_hosted` · `cmd_pin` |
| Liaison ESP32-C6 (SDIO), donnée 0 | GPIO 14 | `esp32_hosted` · `d0_pin` |
| Liaison ESP32-C6 (SDIO), donnée 1 | GPIO 15 | `esp32_hosted` · `d1_pin` |
| Liaison ESP32-C6 (SDIO), donnée 2 | GPIO 16 | `esp32_hosted` · `d2_pin` |
| Liaison ESP32-C6 (SDIO), donnée 3 | GPIO 17 | `esp32_hosted` · `d3_pin` |
| Reset de l'ESP32-C6 | GPIO 54 | `esp32_hosted` · `reset_pin` |

Sources : le paquet de support de la carte et le schéma de Waveshare, le modèle d'écran d'ESPHome et les configurations publiques citées plus haut. Les broches de la liaison du C6 sont celles qu'ESP-Hosted prend par défaut sur un P4, que cette carte suit.
