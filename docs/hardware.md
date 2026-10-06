# Hardware Reference

## English · [Français](#version-française)

---

## Hardware revisions

M5Stack has shipped the Tab5 with three different display controllers ([M5Stack change log](https://docs.m5stack.com/en/core/Tab5)). Each one needs its own ESPHome display model, and they are not interchangeable ([ESPHome `mipi_dsi` documentation](https://esphome.io/components/display/mipi_dsi/)):

| Display chip | Units made | ESPHome model | `tab5_ecran:` | Status in this project |
|---|---|---|---|---|
| **ILI9881C** + separate **GT911** touch | 9 May 2025 → 14 October 2025 | `M5STACK-TAB5` | `ili9881c` | 🧪 **Compiles, untested** — built by the CI, never run on a device |
| **ST7123** (display and touch in one chip) | 14 October 2025 → 28 April 2026 | `M5STACK-TAB5-ST7123` (named `M5STACK-TAB5-V2` before ESPHome 2026.7) | `st7123` (default) | ✅ **Supported** — the author's device, in daily use |
| **ST7121** (display and touch in one chip) | from 28 April 2026 | `M5STACK-TAB5-ST7121` | `st7121` | ✅ **Runs on a user's unit** — built by the CI; another user has run this firmware since October 2026: display, touch and wake word ([Discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). The author has no ST7121 |

**How to tell which one you have:** the display chip is printed on the sticker on the back, just above the Espressif logo. ESPHome warns that a unit labelled "ST7123" may carry either an ST7123 or an ST7121: the only way to know is to try both.

**Choosing your revision:** add one line to `Tab5/user_entities.yaml`, for example `tab5_ecran: st7121`. Without it, the firmware is built for the ST7123. What differs between revisions (display model, touch chip) lives in `Tab5/ecran-<revision>.yaml`; pins, calibration and behaviour are shared in `Tab5/tab5-hardware.yaml`.

**What "untested" means:** the CI compiles the ILI9881C and ST7121 files whenever the display configuration changes, so they cannot silently break, but nobody has run this firmware on an ILI9881C yet.

- **ILI9881C:** display model and GT911 touch taken from the [ESPHome device page](https://devices.esphome.io/devices/m5stack-tab5/) for the original revision, with the same pins as the ST7123.
- **ST7121:** ESPHome has an official display model, but no ST7121 touch driver. According to comments in ESPHome's model code, M5Stack's factory firmware tells the two chips apart by reading the touch controller's firmware version, so they very likely share the same protocol: this project uses the `st7123` touch platform. In July 2026, while testing the ESPHome pull request that added the ST7121 model, another user [reported](https://github.com/esphome/esphome/issues/17471#issuecomment-4945713225) that this same pairing (`M5STACK-TAB5-ST7121` display, `st7123` touch) works on a real ST7121: display and touch, accurate across the whole screen, in landscape. Since October 2026, this firmware itself runs on another user's ST7121 ([Discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): display, touch and wake word work. If the screen works but touch does not on your unit, the touch driver is the first suspect.

**Good to know:** most Tab5 examples published so far target the original ILI9881C revision. In September 2026, the [ESPHome device page](https://devices.esphome.io/devices/m5stack-tab5/) still says only that one is supported, and [M5Stack's own Home Assistant HMI example](https://docs.m5stack.com/en/homeassistant/applications/dashboard/tab5_ha_hmi) does not support units made after 14 October 2025. This project runs on the ST7123 (the author's unit) and on the ST7121 (a user's unit), and builds for the ILI9881C.

**On an ST7121, two things are worth checking:** touch in all four corners in landscape, and opening then closing the Neon Apron pinball game, which switches the screen to portrait and back. The user quoted above rotated the image in the `display:` block; this firmware rotates it in LVGL (`rotation: 270`, changed on the fly by Neon Apron), a path nobody has reported running on an ST7121 yet. One [report](https://github.com/esphome/esphome/issues/18712) mentions a boot crash with LVGL rotation on an ST7121, but on a unit later returned for a display defect, so the link is uncertain. The same report shows that display/touch mirroring corrupts the image on an ST7121: this firmware does not use it.

Own an ST7121 or ILI9881C unit and willing to try? Say how it went in [Discussions → Hardware compatibility](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/categories/hardware-compatibility).

---

## M5Stack Tab5 V2

The Tab5 is a 5-inch touch-screen panel from M5Stack. It uses an **ESP32-P4** as the main application processor, with a separate **ESP32-C6** co-processor handling Wi-Fi and Bluetooth connectivity. This project runs on the **ST7123** revision, which this repository calls "Tab5 V2" (see [Hardware revisions](#hardware-revisions)).

### ESP32-P4 (main processor)

| Spec | Value |
|------|-------|
| Architecture | RISC-V, dual-core, 360 MHz on the Tab5 (chip revision v1.3, read at boot; 400 MHz needs a revision v3.x chip) |
| Internal SRAM | 768 KB |
| External PSRAM | 32 MB (hex mode, 16 data lines, 200 MHz) — measured on the device on 2026-09-26: `esp_psram_get_size()` = 32 768 KB, 29 567 KB available to the heap |
| Flash | 16 MB |
| Display interface | MIPI-DSI, 16-bit RGB565 |
| Touch controller | I2C (ST7123) |

The PSRAM is critical for this project. LVGL requires a framebuffer sized to the display resolution — at 1280 × 720 px in RGB565, that's ~1.8 MB just for the framebuffer. The ESP32-P4's internal SRAM alone would not be enough. With 32 MB of PSRAM, the framebuffer stays entirely in external memory.

### ESP32-C6 (co-processor)

Handles all radio communication: Wi-Fi 6 (802.11ax) and BLE 5. The main ESP32-P4 communicates with it over an SDIO bus (`esp32_hosted:` component, 20 MHz). From the ESPHome/LVGL code perspective, this is transparent — standard ESPHome Wi-Fi and BLE components work normally.

The C6 has its own RAM (512 KB) and runs Espressif's ESP-Hosted firmware, not ours: our firmware, including the TCP/IP stack (lwIP), runs on the P4. ESPHome builds the P4 side of ESP-Hosted (2.12.12 with ESPHome 2026.9) but does not update the C6, which keeps its factory firmware unless someone reflashes it. The diagnostic sensor **Tab5 C6 Version** reports that version once per boot.

### Real-time clock (RX8130CE)

Address 0x32 on the internal I2C bus (`bsp_bus`, GPIO31/32), backed by a 70 000 µF supercapacitor (M5Stack specification). The firmware reads it once at boot, so the clock and the alarm have the time before the network is up, and writes it back after every NTP sync. It stores UTC. The same bus also carries the INA226 battery monitor (0x41, see [Power](#power)).

---

## Display

- **Size:** 5 inches
- **Resolution:** 1280 × 720 px (M5Stack Tab5 V2 batch used in this project)
- **Interface:** MIPI-DSI 16-bit RGB565 (ESP32-P4 LCD peripheral)
- **Touch:** Capacitive multi-touch via ESPHome's official `st7123` I2C touchscreen platform on the ST7123 revision (since ESPHome 2026.7.0; the former custom `my_components/st7123` was removed on 2026-07-06) — `gt911` on the original ILI9881C revision, see [Hardware revisions](#hardware-revisions)

---

## Pin map

Every pin the firmware configures, as written in its YAML. The pins are the same on the three revisions. "PI4IOE 0x43, P1" means pin 1 of the PI4IOE5V6408 I/O expander at address 0x43 on the internal I2C bus; there are two of them (0x43 and 0x44). The MIPI-DSI link itself has no pin in the YAML: the display model of `Tab5/ecran-<revision>.yaml` takes care of it. The last column names the component (its `id`, or the block that has none) and the key: `tests/test_doc_broches.py` checks every row, in both languages, against the YAML, and fails if a pin of the YAML is missing here.

| Function | Pin | In the YAML |
|---|---|---|
| Internal I2C bus, SDA (400 kHz) | GPIO 31 | `bsp_bus` · `sda` |
| Internal I2C bus, SCL | GPIO 32 | `bsp_bus` · `scl` |
| Display reset | PI4IOE 0x43, P4 | `tab5_display` · `reset_pin` |
| Touch interrupt | GPIO 23 | `touch` · `interrupt_pin` |
| Touch reset | PI4IOE 0x43, P5 | `touch` · `reset_pin` |
| Backlight PWM (LEDC, 1 kHz) | GPIO 22 | `backlight_pwm` · `pin` |
| I2S bit clock (microphone and speaker) | GPIO 27 | `mic_bus` · `i2s_bclk_pin` |
| I2S word select (microphone and speaker) | GPIO 29 | `mic_bus` · `i2s_lrclk_pin` |
| I2S master clock (microphone and speaker) | GPIO 30 | `mic_bus` · `i2s_mclk_pin` |
| I2S data in, from the ES7210 (microphone) | GPIO 28 | `tab5_microphone` · `i2s_din_pin` |
| I2S data out, to the ES8388 (speaker) | GPIO 26 | `tab5_speaker` · `i2s_dout_pin` |
| Speaker amplifier enable | PI4IOE 0x43, P1 | `speaker_enable` · `pin` |
| Headphone jack detect | PI4IOE 0x43, P7 | `headphone_detect` · `pin` |
| Wi-Fi antenna, internal or external | PI4IOE 0x43, P0 | `wifi_antenna_int_ext` · `pin` |
| External 5 V power | PI4IOE 0x43, P2 | `external_5v_power` · `pin` |
| Wi-Fi power | PI4IOE 0x44, P0 | `wifi_power` · `pin` |
| USB power | PI4IOE 0x44, P3 | `usb_5v_power` · `pin` |
| Battery quick charge (active low, kept off) | PI4IOE 0x44, P5 | `quick_charge` · `pin` |
| Battery charging status | PI4IOE 0x44, P6 | `batterie_en_charge` · `pin` |
| Battery charge enable | PI4IOE 0x44, P7 | `charge_enable` · `pin` |
| ESP32-C6 link (SDIO), clock | GPIO 12 | `esp32_hosted` · `clk_pin` |
| ESP32-C6 link (SDIO), command | GPIO 13 | `esp32_hosted` · `cmd_pin` |
| ESP32-C6 link (SDIO), data 0 | GPIO 11 | `esp32_hosted` · `d0_pin` |
| ESP32-C6 link (SDIO), data 1 | GPIO 10 | `esp32_hosted` · `d1_pin` |
| ESP32-C6 link (SDIO), data 2 | GPIO 9 | `esp32_hosted` · `d2_pin` |
| ESP32-C6 link (SDIO), data 3 | GPIO 8 | `esp32_hosted` · `d3_pin` |
| ESP32-C6 reset | GPIO 15 | `esp32_hosted` · `reset_pin` |

The firmware declares a single I2C bus (`bsp_bus`), so every I2C chip it drives is on it: the two I/O expanders, the touch controller, the ES8388 and the ES7210, the BMI270 motion sensor (0x68, `Tab5/tab5-imu.yaml`), the RX8130CE clock (0x32) and the INA226 battery monitor (0x41; ESPHome's default address for it, 0x40, is the ES7210's).

---

## Audio — ES8388 DAC

The Tab5 integrates an **ES8388** audio codec chip, used here for the DAC path (speaker output) via ESPHome's native `audio_dac: es8388` platform. The microphone input goes through a separate **ES7210** ADC chip (`audio_adc: es7210`, 16 kHz / 16-bit), whose output the ESP32-P4 reads over I2S.

### Connections

Both chips are set up over the internal I2C bus and share **one** I2S bus (`mic_bus`): the three clocks are common, each direction has its own data line. Only one of the two can hold that bus at a time ([ADR-0010](decisions/0010-shared-i2s-bus-mic-speaker.md)). The speaker amplifier is switched by the first I/O expander (`speaker_enable`). Pins: [Pin map](#pin-map).

### Boot sequence issue

The ES8388 requires a specific initialization sequence over I2C to come out of reset and route audio correctly. If the amplifier enable line fires before the I2S clock is stable, a loud pop occurs through the speaker.

The ESPHome `on_boot` block addresses this by sequencing (`tab5-ha-hmi.yaml`, priority 600):
1. Turn on the backlight, wait 1 s
2. Set the media player volume
3. Only then enable the amplifier switch (`speaker_enable`)
4. Then wait for the HA API connection

This order ensures the I2S clock is stable before the amplifier opens the speaker path. Register-level initialization is handled by ESPHome's `es8388` platform itself — there are no manual `i2c.write_bytes` calls in the configuration anymore.

---

## Microphone — ES7210 ADC over I2S

The onboard microphone is captured by the **ES7210** ADC chip (`audio_adc: es7210`), which streams to the ESP32-P4 over I2S (`adc_type: external`), on the bus it shares with the speaker (pins: [Pin map](#pin-map)).

Capture parameters: **16 kHz, 16-bit mono**. This matches the input format expected by the `micro_wake_word` component and by Home Assistant's voice pipeline.

---

## Power

The Tab5 is USB-C powered. Peak consumption (Wi-Fi active + 100% backlight + audio playing) can exceed 1.5 A at 5V. A charger rated for at least **5V / 2A** is required to avoid brownout resets.

Backlight brightness is software-controlled via PWM (LEDC output on GPIO 22, `light: monochromatic`) and can be dimmed from Home Assistant to reduce power draw. Touching the screen while the backlight is off turns it back on (`touchscreen: on_release`). There is no ambient light sensor in this configuration.

**Battery.** The Tab5 takes an optional two-cell (2S) lithium battery. Since October 2026 the firmware enables its charger at boot (`charge_enable`, PI4IOE 0x44 P7), like M5Stack's M5Unified library; before that it never did, and a user reported that the battery did not charge ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). Quick charge (P5) is kept off: the tablet stays plugged in, and standard charge asks less current from the USB charger. Three diagnostic entities reach Home Assistant: **Tab5 Batterie en charge** (charging status, P6, read every 10 s, a change published once it has lasted 30 s), **Tab5 Tension batterie** (voltage measured by the INA226, published on a 50 mV change or every 15 min) and **Tab5 Batterie** (level estimated from the voltage, 6.0 V = 0 %, 8.23 V = 100 %, the line of ESPHome's reference configuration; it reads too high while charging; "unknown" while no battery is detected). Without a battery the charger still says "charging" and the INA226 reads the charger or the USB, never a steady battery voltage: 4.2 V on the tablet of a user whose fitted original battery reads about 7.2 V, with a charging status that follows reality (2026-10-05); on the author's tablet, which has none, 4.2 V and 8.39 V alternating every 1 to 3 minutes (evening of 2026-10-03), then a steady 5.71 V (2026-10-04). Those three entities are therefore **disabled by default** in Home Assistant: with the battery fitted, enable them on the device page. **Battery detection** (since 2026-10-05, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): a two-cell battery in working order never reads below about 6 V, so a reading below **6.0 V** within the last **10 minutes** (ten readings, one a minute) means no battery, and ten minutes without one, or none since boot, means a battery. The window is there for the alternating case, which climbs back above 6 V between two low readings. A fourth diagnostic entity, **Tab5 Batterie détectée**, enabled by default, publishes that decision (unknown before the first reading). Threshold and window: `kBatterieTensionMin` and `kBatterieFenetreMs` in `Tab5/tab5_core.h`, tested on a PC by `tools/test_alarm_clock.cpp`. **On the screen**, an icon closes the status bar (top left, after the alarm bell) once the device switch **Tab5 Batterie montée** is on; it is off by default and survives reboots. With no battery detected the icon is a **plug**, in the theme's text colour; with a battery its glyph follows the level (full above 80 %, half, low, "!" below 20 %, a bolt while charging) and its colour is the phone's scale (`get_battery_color()`: green, blue, amber, red); "?" in grey before the first reading. The icon reads the sensors on the tablet itself: it works even with the battery entities left disabled in Home Assistant. The detection rests on the readings above; the author's tablet has no battery to try it with.

**Battery current and energy saving** (2026-10-06). The INA226 also gives the battery current, published as **Tab5 Courant batterie** (diagnostic, disabled by default like the voltage): positive while the battery is discharging, that is while the tablet runs on it, negative while it charges. That sign comes from M5Stack's M5Unified library (`getBatteryCurrent`: the shunt is wired so that charging reads negative; ESPHome publishes the register as it is); no tablet has shown it yet. Each reading (every 60 s) decides **Tab5 Sur batterie**: above 50 mA of discharge, on battery; below 20 mA, plugged in; never while charging (it switches back as soon as charging resumes) nor without a detected battery. The device setting **Tab5 Économie d'énergie** (« Sur batterie » by default) uses it: on battery, the brightness is capped at 50 % at the backlight output (Home Assistant keeps the brightness you chose), drops to the minimum (10 %) after 30 s without a touch and at 35 % of battery or less, the animations stop and LVGL redraws at most 30 times a second outside games. Thresholds: `Tab5/tab5_economie.h`, tested on a PC by `tools/test_alarm_clock.cpp`; settings: [settings](installation/settings.md).

---

---

## Version Française

---

## Révisions matérielles

M5Stack a livré le Tab5 avec trois contrôleurs d'écran différents ([journal des versions M5Stack](https://docs.m5stack.com/en/core/Tab5)). Chacun demande son propre modèle d'affichage ESPHome, et ils ne sont pas interchangeables ([documentation ESPHome `mipi_dsi`](https://esphome.io/components/display/mipi_dsi/)) :

| Puce écran | Appareils fabriqués | Modèle ESPHome | `tab5_ecran:` | Statut dans ce projet |
|---|---|---|---|---|
| **ILI9881C** + tactile **GT911** séparé | du 9 mai 2025 au 14 octobre 2025 | `M5STACK-TAB5` | `ili9881c` | 🧪 **Compile, non testée** — compilée par la CI, jamais lancée sur une tablette |
| **ST7123** (écran et tactile dans une seule puce) | du 14 octobre 2025 au 28 avril 2026 | `M5STACK-TAB5-ST7123` (nommé `M5STACK-TAB5-V2` avant ESPHome 2026.7) | `st7123` (défaut) | ✅ **Prise en charge** — la tablette de l'auteur, utilisée tous les jours |
| **ST7121** (écran et tactile dans une seule puce) | depuis le 28 avril 2026 | `M5STACK-TAB5-ST7121` | `st7121` | ✅ **Tourne chez un utilisateur** — compilée par la CI ; un autre utilisateur fait tourner ce firmware depuis octobre 2026 : écran, tactile et mot d'activation ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). L'auteur n'a pas de ST7121 |

**Comment savoir lequel vous avez :** la puce écran est inscrite sur l'autocollant au dos, juste au-dessus du logo Espressif. ESPHome prévient qu'un appareil marqué « ST7123 » peut contenir une ST7123 ou une ST7121 : le seul moyen de savoir est d'essayer les deux.

**Choisir sa révision :** ajoutez une ligne dans `Tab5/user_entities.yaml`, par exemple `tab5_ecran: st7121`. Sans elle, le firmware est compilé pour la ST7123. Ce qui change d'une révision à l'autre (modèle d'écran, puce tactile) est dans `Tab5/ecran-<révision>.yaml` ; les broches, la calibration et le comportement sont communs, dans `Tab5/tab5-hardware.yaml`.

**Ce que « non testée » veut dire :** la CI compile les fichiers de l'ILI9881C et de la ST7121 à chaque changement de la configuration de l'écran, ils ne peuvent donc pas casser en silence, mais personne n'a encore lancé ce firmware sur une ILI9881C.

- **ILI9881C :** modèle d'écran et tactile GT911 repris de la [page ESPHome de l'appareil](https://devices.esphome.io/devices/m5stack-tab5/) pour la révision d'origine, avec les mêmes broches que la ST7123.
- **ST7121 :** ESPHome a un modèle d'écran officiel, mais pas de pilote tactile ST7121. D'après les commentaires du code du modèle dans ESPHome, le firmware d'usine de M5Stack distingue les deux puces en lisant la version du contrôleur tactile : elles partagent donc très probablement le même protocole, et ce projet utilise la plateforme tactile `st7123`. En juillet 2026, en testant la PR ESPHome qui ajoutait le modèle ST7121, un autre utilisateur a [signalé](https://github.com/esphome/esphome/issues/17471#issuecomment-4945713225) que ce même couple (écran `M5STACK-TAB5-ST7121`, tactile `st7123`) marche sur une vraie ST7121 : écran et tactile, précis sur tout l'écran, en paysage. Depuis octobre 2026, ce firmware lui-même tourne sur la ST7121 d'un autre utilisateur ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : écran, tactile et mot d'activation marchent. Si, chez vous, l'écran marche mais pas le tactile, le pilote tactile est le premier suspect.

**Bon à savoir :** la plupart des exemples Tab5 publiés jusqu'ici visent la révision d'origine ILI9881C. En septembre 2026, la [page ESPHome de l'appareil](https://devices.esphome.io/devices/m5stack-tab5/) n'indique encore qu'elle comme prise en charge, et [l'exemple d'IHM Home Assistant de M5Stack](https://docs.m5stack.com/en/homeassistant/applications/dashboard/tab5_ha_hmi) ne prend pas en charge les appareils fabriqués après le 14 octobre 2025. Ce projet tourne sur la ST7123 et compile pour les deux autres.

**Sur une ST7121, deux points méritent d'être vérifiés :** le tactile dans les quatre coins en paysage, et l'ouverture puis la fermeture du flipper Neon Apron, qui passe l'écran en portrait puis le remet en paysage. L'utilisateur cité plus haut tournait l'image dans le bloc `display:` ; ce firmware la tourne dans LVGL (`rotation: 270`, modifiée à la volée par Neon Apron), un chemin que personne n'a encore signalé sur une ST7121. Un [signalement](https://github.com/esphome/esphome/issues/18712) parle d'un plantage au démarrage avec la rotation LVGL sur une ST7121, mais sur un appareil renvoyé ensuite pour un défaut d'écran : le lien est incertain. Le même signalement montre que le miroir écran/tactile abîme l'image sur une ST7121 : ce firmware ne l'utilise pas.

Vous avez un Tab5 ST7121 ou ILI9881C et vous voulez bien essayer ? Racontez ce que ça a donné dans [Discussions → Hardware compatibility](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/categories/hardware-compatibility).

---

## M5Stack Tab5 V2

Le Tab5 est un panneau tactile de 5 pouces de M5Stack. Il utilise un **ESP32-P4** comme processeur applicatif principal, avec un co-processeur **ESP32-C6** séparé gérant la connectivité Wi-Fi et Bluetooth. Ce projet tourne sur la révision **ST7123**, que ce dépôt appelle « Tab5 V2 » (voir [Révisions matérielles](#révisions-matérielles)).

### ESP32-P4 (processeur principal)

| Spec | Valeur |
|------|--------|
| Architecture | RISC-V, dual-core, 360 MHz sur le Tab5 (puce révision v1.3, lue au démarrage ; 400 MHz demande une puce révision v3.x) |
| SRAM interne | 768 KB |
| PSRAM externe | 32 Mo (mode hex, 16 lignes de données, 200 MHz) — mesuré sur la tablette le 26/09/2026 : `esp_psram_get_size()` = 32 768 Ko, dont 29 567 Ko pour le tas |
| Flash | 16 MB |
| Interface affichage | MIPI-DSI, RGB565 16 bits |
| Contrôleur tactile | I2C (ST7123) |

La PSRAM est critique pour ce projet. LVGL nécessite un framebuffer dimensionné à la résolution de l'affichage — à 1280 × 720 px en RGB565, ça fait ~1,8 MB rien que pour le framebuffer. La SRAM interne de l'ESP32-P4 seule ne suffirait pas. Avec 32 Mo de PSRAM, le framebuffer reste entièrement en mémoire externe.

### ESP32-C6 (co-processeur)

Gère toute la communication radio : Wi-Fi 6 (802.11ax) et BLE 5. Le ESP32-P4 principal communique avec lui via un bus SDIO (composant `esp32_hosted:`, 20 MHz). Du point de vue du code ESPHome/LVGL, c'est transparent — les composants Wi-Fi et BLE standards d'ESPHome fonctionnent normalement.

Le C6 a sa propre RAM (512 Ko) et fait tourner le logiciel ESP-Hosted d'Espressif, pas le nôtre : notre firmware, pile TCP/IP (lwIP) comprise, tourne sur le P4. ESPHome compile la partie P4 d'ESP-Hosted (2.12.12 avec ESPHome 2026.9) mais ne met pas le C6 à jour : il garde son logiciel d'usine tant que personne ne le reflashe. Le capteur de diagnostic **Tab5 C6 Version** en donne la version, lue une fois par démarrage.

### Horloge temps réel (RX8130CE)

Adresse 0x32 sur le bus I2C interne (`bsp_bus`, GPIO31/32), sauvegardée par un supercondensateur de 70 000 µF (spécification M5Stack). Le firmware la lit une fois au démarrage, pour que l'horloge et le réveil aient l'heure avant le réseau, et la réécrit après chaque synchro NTP. Elle stocke l'heure UTC. Le même bus porte aussi le moniteur de batterie INA226 (0x41, voir [Alimentation](#alimentation)).

---

## Affichage

- **Taille :** 5 pouces
- **Résolution :** 1280 × 720 px (lot M5Stack Tab5 V2 utilisé dans ce projet)
- **Interface :** MIPI-DSI RGB565 16 bits (périphérique LCD de l'ESP32-P4)
- **Tactile :** Capacitif multi-touch via la plateforme tactile I2C officielle `st7123` d'ESPHome sur la révision ST7123 (depuis ESPHome 2026.7.0 ; l'ancien composant maison `my_components/st7123` a été retiré le 06/07/2026) — `gt911` sur la révision d'origine ILI9881C, voir [Révisions matérielles](#révisions-matérielles)

---

## Broches

Toutes les broches que le firmware configure, telles qu'écrites dans son YAML. Elles sont les mêmes sur les trois révisions. « PI4IOE 0x43, P1 » veut dire la broche 1 de l'expandeur d'E/S PI4IOE5V6408 d'adresse 0x43 sur le bus I2C interne ; il y en a deux (0x43 et 0x44). La liaison MIPI-DSI elle-même n'a aucune broche dans le YAML : le modèle d'écran de `Tab5/ecran-<révision>.yaml` s'en charge. La dernière colonne nomme le composant (son `id`, ou le bloc qui n'en a pas) et la clé : `tests/test_doc_broches.py` compare chaque ligne au YAML, dans les deux langues, et échoue si une broche du YAML manque ici.

| Fonction | Broche | Dans le YAML |
|---|---|---|
| Bus I2C interne, SDA (400 kHz) | GPIO 31 | `bsp_bus` · `sda` |
| Bus I2C interne, SCL | GPIO 32 | `bsp_bus` · `scl` |
| Reset de l'écran | PI4IOE 0x43, P4 | `tab5_display` · `reset_pin` |
| Interruption du tactile | GPIO 23 | `touch` · `interrupt_pin` |
| Reset du tactile | PI4IOE 0x43, P5 | `touch` · `reset_pin` |
| PWM du rétroéclairage (LEDC, 1 kHz) | GPIO 22 | `backlight_pwm` · `pin` |
| Horloge bit I2S (micro et haut-parleur) | GPIO 27 | `mic_bus` · `i2s_bclk_pin` |
| Sélection de mot I2S (micro et haut-parleur) | GPIO 29 | `mic_bus` · `i2s_lrclk_pin` |
| Horloge maître I2S (micro et haut-parleur) | GPIO 30 | `mic_bus` · `i2s_mclk_pin` |
| Données I2S entrantes, de l'ES7210 (micro) | GPIO 28 | `tab5_microphone` · `i2s_din_pin` |
| Données I2S sortantes, vers l'ES8388 (haut-parleur) | GPIO 26 | `tab5_speaker` · `i2s_dout_pin` |
| Activation de l'ampli du haut-parleur | PI4IOE 0x43, P1 | `speaker_enable` · `pin` |
| Détection de la prise casque | PI4IOE 0x43, P7 | `headphone_detect` · `pin` |
| Antenne Wi-Fi, interne ou externe | PI4IOE 0x43, P0 | `wifi_antenna_int_ext` · `pin` |
| Alimentation 5 V externe | PI4IOE 0x43, P2 | `external_5v_power` · `pin` |
| Alimentation du Wi-Fi | PI4IOE 0x44, P0 | `wifi_power` · `pin` |
| Alimentation USB | PI4IOE 0x44, P3 | `usb_5v_power` · `pin` |
| Charge rapide de la batterie (active à 0, laissée à l'arrêt) | PI4IOE 0x44, P5 | `quick_charge` · `pin` |
| État de charge de la batterie | PI4IOE 0x44, P6 | `batterie_en_charge` · `pin` |
| Activation de la charge de la batterie | PI4IOE 0x44, P7 | `charge_enable` · `pin` |
| Liaison ESP32-C6 (SDIO), horloge | GPIO 12 | `esp32_hosted` · `clk_pin` |
| Liaison ESP32-C6 (SDIO), commande | GPIO 13 | `esp32_hosted` · `cmd_pin` |
| Liaison ESP32-C6 (SDIO), données 0 | GPIO 11 | `esp32_hosted` · `d0_pin` |
| Liaison ESP32-C6 (SDIO), données 1 | GPIO 10 | `esp32_hosted` · `d1_pin` |
| Liaison ESP32-C6 (SDIO), données 2 | GPIO 9 | `esp32_hosted` · `d2_pin` |
| Liaison ESP32-C6 (SDIO), données 3 | GPIO 8 | `esp32_hosted` · `d3_pin` |
| Reset de l'ESP32-C6 | GPIO 15 | `esp32_hosted` · `reset_pin` |

Le firmware ne déclare qu'un bus I2C (`bsp_bus`) : toutes les puces I2C qu'il pilote y sont, les deux expandeurs d'E/S, le contrôleur tactile, l'ES8388 et l'ES7210, le capteur de mouvement BMI270 (0x68, `Tab5/tab5-imu.yaml`), l'horloge RX8130CE (0x32) et le moniteur de batterie INA226 (0x41 ; l'adresse qu'ESPHome lui donne par défaut, 0x40, est celle de l'ES7210).

---

## Audio — DAC ES8388

Le Tab5 intègre un codec audio **ES8388**, utilisé ici pour le chemin DAC (sortie haut-parleur) via la plateforme native `audio_dac: es8388` d'ESPHome. L'entrée microphone passe par un chip ADC séparé, l'**ES7210** (`audio_adc: es7210`, 16 kHz / 16 bits), dont l'ESP32-P4 lit la sortie en I2S.

### Connexions

Les deux puces se règlent par le bus I2C interne et partagent **un seul** bus I2S (`mic_bus`) : les trois horloges sont communes, chaque sens a sa ligne de données. Une seule des deux peut tenir ce bus à la fois ([ADR-0010](decisions/0010-shared-i2s-bus-mic-speaker.md)). L'ampli du haut-parleur est commandé par le premier expandeur d'E/S (`speaker_enable`). Les broches sont dans [Broches](#broches).

### Problème de séquence au boot

L'ES8388 nécessite une séquence d'initialisation spécifique sur I2C pour sortir du reset et router correctement l'audio. Si la ligne d'activation de l'amplificateur passe avant que l'horloge I2S soit stable, un fort pop se produit dans le haut-parleur.

Le bloc `on_boot` d'ESPHome règle ça en séquençant (`tab5-ha-hmi.yaml`, priorité 600) :
1. Allumer le rétroéclairage, attendre 1 s
2. Régler le volume du media player
3. Seulement alors activer le switch ampli (`speaker_enable`)
4. Puis attendre la connexion API HA

Cet ordre garantit que l'horloge I2S est stable avant que l'ampli ouvre le chemin vers le haut-parleur. L'initialisation des registres est prise en charge par la plateforme `es8388` d'ESPHome elle-même — il n'y a plus d'appel manuel `i2c.write_bytes` dans la configuration.

---

## Microphone — ADC ES7210 via I2S

Le microphone intégré est capturé par le chip ADC **ES7210** (`audio_adc: es7210`), qui streame vers l'ESP32-P4 en I2S (`adc_type: external`), sur le bus qu'il partage avec le haut-parleur (voir [Broches](#broches)).

Paramètres de capture : **16 kHz, 16-bit mono**. Correspond au format d'entrée attendu par le composant `micro_wake_word` et par le pipeline vocal de Home Assistant.

---

## Alimentation

Le Tab5 est alimenté en USB-C. La consommation en pointe (Wi-Fi actif + rétroéclairage 100% + audio en lecture) peut dépasser 1,5 A à 5V. Un chargeur d'au moins **5V / 2A** est nécessaire pour éviter les resets par sous-tension.

La luminosité du rétroéclairage est contrôlée logiciellement via PWM (sortie LEDC sur GPIO 22, `light: monochromatic`) et peut être réduite depuis Home Assistant. Toucher l'écran quand le rétroéclairage est éteint le rallume (`touchscreen: on_release`). Il n'y a pas de capteur de luminosité ambiante dans cette configuration.

**Batterie.** Le Tab5 accepte une batterie lithium à deux éléments (2S) en option. Depuis octobre 2026, le firmware active son chargeur au démarrage (`charge_enable`, PI4IOE 0x44 P7), comme la bibliothèque M5Unified de M5Stack ; avant, il ne le faisait jamais, et un utilisateur a signalé que la batterie ne se chargeait pas ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). La charge rapide (P5) reste à l'arrêt : la tablette reste branchée, et la charge standard demande moins de courant au chargeur USB. Trois entités de diagnostic arrivent dans Home Assistant : **Tab5 Batterie en charge** (état de charge, P6, lu toutes les 10 s, un changement publié quand il a tenu 30 s), **Tab5 Tension batterie** (tension mesurée par l'INA226, publiée quand elle bouge de 50 mV ou toutes les 15 min) et **Tab5 Batterie** (niveau estimé d'après la tension, 6,0 V = 0 %, 8,23 V = 100 %, la droite de la config de référence ESPHome ; il lit trop haut pendant la charge ; « inconnu » tant qu'aucune batterie n'est détectée). Sans batterie, le chargeur dit quand même « en charge » et l'INA226 lit le chargeur ou l'USB, jamais une tension de batterie stable : 4,2 V sur la tablette d'un utilisateur dont la batterie d'origine montée lit environ 7,2 V, avec un état de charge qui suit la réalité (05/10/2026) ; sur la tablette de l'auteur, qui n'en a pas, 4,2 V et 8,39 V en alternance toutes les 1 à 3 minutes (soir du 03/10/2026), puis 5,71 V stable (04/10/2026). Ces trois entités sont donc **désactivées par défaut** dans Home Assistant ; avec la batterie montée, les activer sur la page de l'appareil. **Détection de la batterie** (depuis le 05/10/2026, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : une batterie 2S en état de marche ne lit jamais sous 6 V environ ; une lecture sous **6,0 V** dans les **10 dernières minutes** (dix lectures, une par minute) veut donc dire pas de batterie, et dix minutes sans, ou aucune depuis le démarrage, une batterie. La fenêtre sert au cas de l'alternance, qui repasse au-dessus de 6 V entre deux lectures basses. Une quatrième entité de diagnostic, **Tab5 Batterie détectée**, activée par défaut, publie cette décision (inconnue avant la première lecture). Seuil et fenêtre : `kBatterieTensionMin` et `kBatterieFenetreMs` dans `Tab5/tab5_core.h`, testés sur PC par `tools/test_alarm_clock.cpp`. **À l'écran**, une icône termine le bandeau d'état (en haut à gauche, après la cloche du réveil) quand l'interrupteur de l'appareil **Tab5 Batterie montée** est allumé ; il est éteint par défaut et survit aux redémarrages. Sans batterie détectée, l'icône est une **prise**, de la couleur du texte du thème ; avec une batterie, son glyphe suit le niveau (pleine au-dessus de 80 %, moitié, basse, « ! » sous 20 %, un éclair pendant la charge) et sa couleur est l'échelle du téléphone (`get_battery_color()` : vert, bleu, ambre, rouge) ; « ? » en gris avant la première lecture. L'icône lit les capteurs sur la tablette même : elle marche aussi avec les entités de la batterie laissées désactivées dans Home Assistant. La détection repose sur les relevés ci-dessus ; la tablette de l'auteur n'a pas de batterie pour l'essayer.

**Courant de la batterie et économie d'énergie** (06/10/2026). L'INA226 donne aussi le courant de la batterie, publié en **Tab5 Courant batterie** (diagnostic, désactivé par défaut comme la tension) : positif quand la batterie se décharge, c'est-à-dire quand la tablette tourne sur elle, négatif quand elle charge. Ce sens vient de la bibliothèque M5Unified de M5Stack (`getBatteryCurrent` : le shunt est câblé pour que la charge se lise négative ; ESPHome publie le registre tel quel) ; aucune tablette ne l'a encore montré. Chaque lecture (toutes les 60 s) décide **Tab5 Sur batterie** : au-dessus de 50 mA de décharge, sur batterie ; sous 20 mA, branchée ; jamais pendant la charge (elle repasse dès que la charge reprend) ni sans batterie détectée. Le réglage de l'appareil **Tab5 Économie d'énergie** (« Sur batterie » par défaut) s'en sert : sur batterie, la luminosité est plafonnée à 50 % à la sortie du rétroéclairage (Home Assistant garde la luminosité choisie), descend au plus bas (10 %) après 30 s sans toucher et à 35 % de batterie ou moins, les animations s'arrêtent et LVGL redessine 30 fois par seconde au plus hors des jeux. Seuils : `Tab5/tab5_economie.h`, testés sur PC par `tools/test_alarm_clock.cpp` ; réglages : [réglages](installation/settings.md#version-française).
