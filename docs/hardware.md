# M5Stack Tab5 hardware: display revisions, specs and power

## English · [Français](#version-française)

---

## Tab5 at a glance

What the M5Stack Tab5 has, and which parts this firmware uses. The specifications come from [M5Stack's Tab5 page](https://docs.m5stack.com/en/core/Tab5) and [ESPHome's device page](https://devices.esphome.io/devices/m5stack-tab5/), both read on 2026-11-06; the « This firmware » column comes from this repository's YAML. Anything the sources do not say is left out. Pins: [Tab5 pinout](tab5-pinout.md). Battery and power: [Tab5 battery](tab5-battery.md).

| Part | What the Tab5 has | This firmware | Source |
|---|---|---|---|
| **Display** | 5-inch IPS TFT, 1280×720, MIPI-DSI. The controller depends on the production date: ILI9881C with a separate GT911 touch chip (until 2025-10-14), ST7123 (2025-10-14 to 2026-04-28), ST7121 (from 2026-04-28), the last two with display and touch in one chip | **Used**, the three revisions ([below](#hardware-revisions)). Backlight: PWM on GPIO 22 | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Processor** | ESP32-P4NRW32: RISC-V dual core at 360 MHz, plus one low-power core at 40 MHz | **Used**, at 360 MHz ([details](#esp32-p4-main-processor)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5), [ESPHome](https://devices.esphome.io/devices/m5stack-tab5/) |
| **Memory** | 16 MB flash, 32 MB PSRAM | **Used**: the YAML configures the PSRAM in hex mode at 200 MHz; measured on the device on 2026-09-26: 32 768 KB, 29 567 KB available to the heap | [M5Stack](https://docs.m5stack.com/en/core/Tab5), [ESPHome](https://devices.esphome.io/devices/m5stack-tab5/) |
| **Wireless** | ESP32-C6-MINI-1U module: Wi-Fi 6 on 2.4 GHz, Thread and Zigbee, linked to the P4 over SDIO. Built-in 3D antenna, or an external MMCX antenna (M5Stack's page is not consistent on the connector: two MMCX ports in the specifications table, one in the feature list, « SMA » in the pin map) | **Wi-Fi only**, through ESP-Hosted ([details](#esp32-c6-co-processor)). The antenna select pin is declared, not exposed to Home Assistant. No Bluetooth, Thread or Zigbee in the YAML | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Audio** | ES8388 codec, ES7210 echo-cancelling front end with a dual-microphone array, 1 W / 8 Ω speaker (NS4150B amplifier), 3.5 mm headphone jack | **Used**: speaker and microphone (16 kHz mono) on one shared I2S bus, headphone detect pin read. No echo cancellation configured ([audio](#audio--es8388-dac), [microphone](#microphone--es7210-adc-over-i2s)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5), [ESPHome](https://devices.esphome.io/devices/m5stack-tab5/) |
| **Sensors** | BMI270 six-axis motion sensor (accelerometer and gyroscope, interrupt wake-up), INA226 power monitor. No ambient light sensor listed | **Used**: BMI270 for the games and tap to wake (its interrupt line is not used), INA226 for the battery | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Clock** | RX8130CE real-time clock with a 70 000 µF / 3.3 V supercapacitor and timed-interrupt wake-up | **Used**: read at boot, written back after every NTP sync, UTC; the interrupt is not used ([details](#real-time-clock-rx8130ce)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Battery** | None in the standard Tab5 (SKU C145). The Tab5 Kit (K145) has a removable NP-F550: 7.4 V, 2000 mAh (14.8 Wh), IP2326 charger | **Used**: charger control, level, safety locks ([Tab5 battery](tab5-battery.md)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Power input** | USB-C. A HVIN input on the M-Bus connector, 6 to 24 V (M5Stack lists that range with the RS485 port) | **USB-C used**: 1.2 W with the screen off to 2.6 W at full brightness, measured ([power consumption](consumption.md)). HVIN never tried by the author | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Camera** | SC2356, 2 MP (1600×1200), MIPI-CSI | Not used | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **microSD** | Slot, SPI or SDIO mode | Not used | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **USB** | USB-A host, USB-C 2.0 OTG | **USB-C used** to flash, read the log and set up Wi-Fi from the browser. USB-A: its 5 V switch is declared, the host function is not used | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **RS485** | SIT3088 with a switchable 120 Ω terminator | Not used | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Expansion** | HY2.0-4P (PORT.A), M-Bus on the back, Ext.Port 2.54-10P, RS485 HC1.25-6P, USB MX1.25-4P; stamp pads for Cat.M, NB-IoT or LoRaWAN modules | Not used. Their 5 V output is switched on (external 5 V power) | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Button** | One: single click to switch on, double click to switch off, hold until the green LED blinks for download mode | — | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Size and weight** | 128.0 × 80.0 × 12.0 mm, 120.9 g (Tab5 Kit: 26.7 mm thick, 217.6 g). 1/4″-20 tripod nut on the side. Operating temperature 0 to 40 °C | — | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |

Notes from another Tab5 project, on the reset of the C6 and on its version, are at the [bottom of this page](#notes-from-another-tab5-project).

---

## Hardware revisions

M5Stack has shipped the Tab5 with three different display controllers ([M5Stack change log](https://docs.m5stack.com/en/core/Tab5)). Each one needs its own ESPHome display model, and they are not interchangeable ([ESPHome `mipi_dsi` documentation](https://esphome.io/components/display/mipi_dsi/)):

| Display chip | Units made | ESPHome model | `tab5_ecran:` | Status in this project |
|---|---|---|---|---|
| **ILI9881C** + separate **GT911** touch | 9 May 2025 → 14 October 2025 | `M5STACK-TAB5` | `ili9881c` | 🧪 **Compiles, untested** — built by the CI, never run on a device |
| **ST7123** (display and touch in one chip) | 14 October 2025 → 28 April 2026 | `M5STACK-TAB5-ST7123` (named `M5STACK-TAB5-V2` before ESPHome 2026.7) | `st7123` (default) | ✅ **Supported** — the author's device, in daily use |
| **ST7121** (display and touch in one chip) | from 28 April 2026 | `M5STACK-TAB5-ST7121` | `st7121` | ✅ **Runs on a user's unit** — built by the CI; another user has run this firmware since October 2026: display, touch and wake word ([Discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). The author has no ST7121 |

**How to tell which one you have:** the display chip is printed on the sticker on the back, just above the Espressif logo. ESPHome warns that a unit labelled "ST7123" may carry either an ST7123 or an ST7121: the only way to know is to try both.

**Choosing your revision:** add one line to `Tab5/user_entities.yaml`, for example `tab5_ecran: st7121`. Without it, the firmware is built for the ST7123. What differs between revisions (display model, touch chip) lives in `Tab5/paquets/ecran-<revision>.yaml`; pins, calibration and behaviour are shared in `Tab5/paquets/tab5-hardware.yaml`.

**What "untested" means:** the CI compiles the ILI9881C and ST7121 files whenever the display configuration changes, so they cannot silently break, but nobody has run this firmware on an ILI9881C yet.

- **ILI9881C:** display model and GT911 touch taken from the [ESPHome device page](https://devices.esphome.io/devices/m5stack-tab5/) for the original revision, with the same pins as the ST7123.
- **ST7121:** ESPHome has an official display model, but no ST7121 touch driver. According to comments in ESPHome's model code, M5Stack's factory firmware tells the two chips apart by reading the touch controller's firmware version, so they very likely share the same protocol: this project uses the `st7123` touch platform. In July 2026, while testing the ESPHome pull request that added the ST7121 model, another user [reported](https://github.com/esphome/esphome/issues/17471#issuecomment-4945713225) that this same pairing (`M5STACK-TAB5-ST7121` display, `st7123` touch) works on a real ST7121: display and touch, accurate across the whole screen, in landscape. Since October 2026, this firmware itself runs on another user's ST7121 ([Discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): display, touch and wake word work. If the screen works but touch does not on your unit, the touch driver is the first suspect.

**Good to know:** most Tab5 examples published so far target the original ILI9881C revision. In September 2026, the [ESPHome device page](https://devices.esphome.io/devices/m5stack-tab5/) still says only that one is supported, and [M5Stack's own Home Assistant HMI example](https://docs.m5stack.com/en/homeassistant/applications/dashboard/tab5_ha_hmi) does not support units made after 14 October 2025. This project runs on the ST7123 (the author's unit) and on the ST7121 (a user's unit), and builds for the ILI9881C.

**On an ST7121, two things are worth checking:** touch in all four corners in landscape, and opening then closing the Neon Apron pinball game, which switches the screen to portrait and back. The user quoted above rotated the image in the `display:` block; this firmware rotates it in LVGL (`rotation: 270`, changed on the fly by Neon Apron), a path nobody has reported running on an ST7121 yet. One [report](https://github.com/esphome/esphome/issues/18712) mentions a boot crash with LVGL rotation on an ST7121, but on a unit later returned for a display defect, so the link is uncertain. The same report shows that display/touch mirroring corrupts the image on an ST7121: this firmware does not use it.

Own an ST7121 or ILI9881C unit and willing to try? Say how it went in [Discussions → Hardware compatibility](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/categories/hardware-compatibility).

---

## Waveshare ESP32-P4 7B board

✅ **Supported: runs on the author's board since 2026-10-14.** The firmware can also be built for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (7-inch, 1024×600, the same ESP32-P4 and ESP32-C6 as the Tab5): add `tab5_carte: waveshare-7b` to `Tab5/user_entities.yaml` and build with ESPHome 2026.9.1 or newer ([ADR-0061](decisions/0061-board-and-screen-size.md)). The home screen and the popups follow its screen size; it has no motion sensor, no clock chip and no battery gauge (a battery connector, which this firmware does not use yet). Its hardware, pins, install steps and the problems met when it arrived have their own page: [Waveshare ESP32-P4 7B](waveshare-7b.md). The rest of this page is about the Tab5.

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

The C6 has its own RAM (512 KB) and runs Espressif's ESP-Hosted firmware, not ours: our firmware, including the TCP/IP stack (lwIP), runs on the P4. ESPHome builds the P4 side of ESP-Hosted (2.12.12 with ESPHome 2026.9) but does not update the C6, which keeps its factory firmware unless someone reflashes it. The diagnostic sensor **C6 Version** reports that version once per boot. The P4 resets the C6 through GPIO 15 on every boot, software reboots included (ESP-Hosted's default setting, `CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP`); the one time the C6 did not come back after an OTA: [Troubleshooting](troubleshooting.md#black-screen--device-off-the-network-after-an-ota--the-wi-fi-co-processor-never-came-up).

Notes from another Tab5 project, on the reset pin of the C6 and on the version check between the P4 and the C6, are at the [bottom of this page](#notes-from-another-tab5-project).

### Real-time clock (RX8130CE)

Address 0x32 on the internal I2C bus (`bsp_bus`, GPIO31/32), backed by a 70 000 µF supercapacitor (M5Stack specification). The firmware reads it once at boot, so the clock and the alarm have the time before the network is up, and writes it back after every NTP sync. It stores UTC. The same bus also carries the INA226 battery monitor (0x41, see [Tab5 battery](tab5-battery.md)).

---

## Display

- **Size:** 5 inches
- **Resolution:** 1280 × 720 px (M5Stack Tab5 V2 batch used in this project)
- **Interface:** MIPI-DSI 16-bit RGB565 (ESP32-P4 LCD peripheral)
- **Touch:** Capacitive multi-touch via ESPHome's official `st7123` I2C touchscreen platform on the ST7123 revision (since ESPHome 2026.7.0; the former custom `my_components/st7123` was removed on 2026-07-06) — `gt911` on the original ILI9881C revision, see [Hardware revisions](#hardware-revisions)

---

## Pin map

The table of every pin the firmware configures (GPIO of the ESP32-P4 and pins of the two I/O expanders), the I2C bus with the address of each chip, and a wiring diagram have their own page: [M5Stack Tab5 pinout](tab5-pinout.md#pin-map). The pins come from the firmware's YAML, and the CI checks them against it, in English and in French.

---

## Audio — ES8388 DAC

The Tab5 integrates an **ES8388** audio codec chip, used here for the DAC path (speaker output) via ESPHome's native `audio_dac: es8388` platform. The microphone input goes through a separate **ES7210** ADC chip (`audio_adc: es7210`, 16 kHz / 16-bit), whose output the ESP32-P4 reads over I2S.

### Connections

Both chips are set up over the internal I2C bus and share **one** I2S bus (`mic_bus`): the three clocks are common, each direction has its own data line. Only one of the two can hold that bus at a time ([ADR-0010](decisions/0010-shared-i2s-bus-mic-speaker.md)). The speaker amplifier is switched by the first I/O expander (`speaker_enable`). Pins: [Pin map](tab5-pinout.md#pin-map).

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

The onboard microphone is captured by the **ES7210** ADC chip (`audio_adc: es7210`), which streams to the ESP32-P4 over I2S (`adc_type: external`), on the bus it shares with the speaker (pins: [Pin map](tab5-pinout.md#pin-map)).

Capture parameters: **16 kHz, 16-bit mono**. This matches the input format expected by the `micro_wake_word` component and by Home Assistant's voice pipeline.

---

## Power

The power supply, the battery detection, the charger and its safety locks, the battery level, the charge limit, the energy saving and the measurements behind them have their own page: [Tab5 battery and power](tab5-battery.md). In short: the Tab5 is USB-C powered (measured 1.2 W with the screen off, 1.7 W at 50 %, 2.6 W at 100 %, [power consumption](consumption.md)); the battery is optional (NP-F550, two cells); without one, the charger stays off.

---

## Notes from another Tab5 project

**Notes from another Tab5 project** ([discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), thanks to Jiuhai, who builds Tab5 devices on ESP-IDF without ESPHome). They come from reading the schematic and the source code, not from measurements, and the author has not checked the schematic:

- On the schematic, the C6 reset net `RF_C6_RST` goes through R4 to `SOC_EXTRF_RST`, which is GPIO 15 of the P4.
- **Firmware built with ESP-IDF, without ESPHome:** in ESP-Hosted's Kconfig (checked in 2.12.12), the reset pin `ESP_HOSTED_SDIO_GPIO_RESET_SLAVE` defaults to 15 only when the board preset `ESP32P4_TAB5_C6_BOARD` is selected, and to 54 on any other ESP32-P4: the reset pulse then goes to the wrong pin and the C6 is never actually reset (it happened in that project's first build). `grep GPIO_RESET_SLAVE sdkconfig` tells. ESPHome takes the pin from `reset_pin` (15 here). That preset also switches the reset to active-low, with a comment saying that otherwise the C6 never leaves reset on a Tab5; this firmware uses `active_high: true` and the C6 has come up on every boot of the author's tablet (about fifty since 2026-09-30). The two do not agree, and the author does not know why.
- At start-up, ESP-Hosted on the P4 compares its version with the C6's and warns `Version mismatch: Host […] > Co-proc […] ==> Upgrade co-proc to avoid RPC timeouts` (`compare_fw_version()`). That warning is not in an ESPHome firmware: ESP-IDF warnings are compiled out (see [Debugging](debugging.md#after-a-crash-or-a-wi-fi-outage-the-boot-and-outage-journal)). On the author's tablet (2.12.12 on the P4, 1.4.1 on the C6), no RPC timeout has been seen: none in the device journal since 2026-09-30, none in three boots read on the serial port on 2026-10-07.
- Updating the C6 is not simple: one Tab5 project finds that the factory C6 firmware offers no update path over SDIO and plans a one-time wired reflash first ([sslivins/arctic-controller#276](https://github.com/sslivins/arctic-controller/issues/276), open); on another P4 + C6 board, after an update of the hosted firmware to 2.12.8, the SDIO link failed and the C6 boot-looped, even after going back to 2.12.1 ([esphome/esphome#16692](https://github.com/esphome/esphome/issues/16692)). Neither project updates the C6.

---

## Version Française

---

## Le Tab5 en un coup d'œil

Ce que le M5Stack Tab5 a, et les parties dont ce firmware se sert. Les caractéristiques viennent de la [page Tab5 de M5Stack](https://docs.m5stack.com/en/core/Tab5) et de la [page ESPHome de l'appareil](https://devices.esphome.io/devices/m5stack-tab5/), toutes deux lues le 06/11/2026 ; la colonne « Ce firmware » vient du YAML de ce dépôt. Ce que les sources ne disent pas est laissé de côté. Broches : [Brochage du Tab5](tab5-pinout.md#version-française). Batterie et alimentation : [Batterie du Tab5](tab5-battery.md#version-française).

| Partie | Ce que le Tab5 a | Ce firmware | Source |
|---|---|---|---|
| **Écran** | IPS TFT de 5 pouces, 1280×720, MIPI-DSI. Le contrôleur dépend de la date de fabrication : ILI9881C avec une puce tactile GT911 séparée (jusqu'au 14/10/2025), ST7123 (du 14/10/2025 au 28/04/2026), ST7121 (depuis le 28/04/2026), les deux dernières avec écran et tactile dans une seule puce | **Utilisé**, les trois révisions ([ci-dessous](#révisions-matérielles)). Rétroéclairage : PWM sur le GPIO 22 | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Processeur** | ESP32-P4NRW32 : RISC-V double cœur à 360 MHz, plus un cœur basse consommation à 40 MHz | **Utilisé**, à 360 MHz ([détails](#esp32-p4-processeur-principal)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5), [ESPHome](https://devices.esphome.io/devices/m5stack-tab5/) |
| **Mémoire** | 16 Mo de flash, 32 Mo de PSRAM | **Utilisée** : le YAML configure la PSRAM en mode hex à 200 MHz ; mesuré sur la tablette le 26/09/2026 : 32 768 Ko, dont 29 567 Ko pour le tas | [M5Stack](https://docs.m5stack.com/en/core/Tab5), [ESPHome](https://devices.esphome.io/devices/m5stack-tab5/) |
| **Sans fil** | Module ESP32-C6-MINI-1U : Wi-Fi 6 en 2,4 GHz, Thread et Zigbee, relié au P4 par SDIO. Antenne 3D intégrée, ou antenne MMCX externe (la page de M5Stack n'est pas cohérente sur le connecteur : deux ports MMCX dans le tableau des caractéristiques, un seul dans la liste des fonctions, « SMA » dans le brochage) | **Wi-Fi seulement**, par ESP-Hosted ([détails](#esp32-c6-co-processeur)). La broche de choix d'antenne est déclarée, pas exposée à Home Assistant. Ni Bluetooth, ni Thread, ni Zigbee dans le YAML | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Audio** | Codec ES8388, circuit d'entrée ES7210 avec annulation d'écho et réseau de deux microphones, haut-parleur de 1 W / 8 Ω (amplificateur NS4150B), prise casque 3,5 mm | **Utilisé** : haut-parleur et microphone (16 kHz mono) sur un seul bus I2S partagé, broche de détection du casque lue. Aucune annulation d'écho configurée ([audio](#audio--dac-es8388), [microphone](#microphone--adc-es7210-via-i2s)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5), [ESPHome](https://devices.esphome.io/devices/m5stack-tab5/) |
| **Capteurs** | BMI270, capteur de mouvement six axes (accéléromètre et gyroscope, réveil par interruption), INA226, moniteur de puissance. Aucun capteur de lumière ambiante indiqué | **Utilisés** : BMI270 pour les jeux et le toucher qui réveille (sa ligne d'interruption n'est pas utilisée), INA226 pour la batterie | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Horloge** | Horloge temps réel RX8130CE avec un supercondensateur de 70 000 µF / 3,3 V et réveil par interruption minutée | **Utilisée** : lue au démarrage, réécrite après chaque synchronisation NTP, en UTC ; l'interruption n'est pas utilisée ([détails](#horloge-temps-réel-rx8130ce)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Batterie** | Aucune dans le Tab5 standard (référence C145). Le Tab5 Kit (K145) a une NP-F550 amovible : 7,4 V, 2000 mAh (14,8 Wh), chargeur IP2326 | **Utilisée** : commande du chargeur, niveau, verrous de sécurité ([Batterie du Tab5](tab5-battery.md#version-française)) | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Entrée d'alimentation** | USB-C. Une entrée HVIN sur le connecteur M-Bus, de 6 à 24 V (M5Stack donne cette plage avec le port RS485) | **USB-C utilisé** : 1,2 W écran éteint, jusqu'à 2,6 W à pleine luminosité, mesurés ([consommation](consumption.md#version-française)). HVIN jamais essayée par l'auteur | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Caméra** | SC2356, 2 Mpx (1600×1200), MIPI-CSI | Non utilisée | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **microSD** | Lecteur, mode SPI ou SDIO | Non utilisé | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **USB** | USB-A hôte, USB-C 2.0 OTG | **USB-C utilisé** pour flasher, lire le journal et régler le Wi-Fi depuis le navigateur. USB-A : son interrupteur 5 V est déclaré, la fonction d'hôte n'est pas utilisée | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **RS485** | SIT3088 avec une résistance de terminaison de 120 Ω commutable | Non utilisé | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Extension** | HY2.0-4P (PORT.A), M-Bus au dos, Ext.Port 2.54-10P, RS485 HC1.25-6P, USB MX1.25-4P ; plages à souder pour des modules Cat.M, NB-IoT ou LoRaWAN | Non utilisée. Leur sortie 5 V est activée (alimentation 5 V externe) | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Bouton** | Un seul : un clic pour allumer, un double clic pour éteindre, un appui maintenu jusqu'au clignotement de la LED verte pour le mode téléchargement | — | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |
| **Taille et poids** | 128,0 × 80,0 × 12,0 mm, 120,9 g (Tab5 Kit : 26,7 mm d'épaisseur, 217,6 g). Écrou de trépied 1/4″-20 sur le côté. Température de fonctionnement de 0 à 40 °C | — | [M5Stack](https://docs.m5stack.com/en/core/Tab5) |

Les notes d'un autre projet sur Tab5, sur le reset du C6 et sur sa version, sont [en bas de cette page](#notes-dun-autre-projet-sur-tab5).

---

## Révisions matérielles

M5Stack a livré le Tab5 avec trois contrôleurs d'écran différents ([journal des versions M5Stack](https://docs.m5stack.com/en/core/Tab5)). Chacun demande son propre modèle d'affichage ESPHome, et ils ne sont pas interchangeables ([documentation ESPHome `mipi_dsi`](https://esphome.io/components/display/mipi_dsi/)) :

| Puce écran | Appareils fabriqués | Modèle ESPHome | `tab5_ecran:` | Statut dans ce projet |
|---|---|---|---|---|
| **ILI9881C** + tactile **GT911** séparé | du 9 mai 2025 au 14 octobre 2025 | `M5STACK-TAB5` | `ili9881c` | 🧪 **Compile, non testée** — compilée par la CI, jamais lancée sur une tablette |
| **ST7123** (écran et tactile dans une seule puce) | du 14 octobre 2025 au 28 avril 2026 | `M5STACK-TAB5-ST7123` (nommé `M5STACK-TAB5-V2` avant ESPHome 2026.7) | `st7123` (défaut) | ✅ **Prise en charge** — la tablette de l'auteur, utilisée tous les jours |
| **ST7121** (écran et tactile dans une seule puce) | depuis le 28 avril 2026 | `M5STACK-TAB5-ST7121` | `st7121` | ✅ **Tourne chez un utilisateur** — compilée par la CI ; un autre utilisateur fait tourner ce firmware depuis octobre 2026 : écran, tactile et mot d'activation ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). L'auteur n'a pas de ST7121 |

**Comment savoir lequel vous avez :** la puce écran est inscrite sur l'autocollant au dos, juste au-dessus du logo Espressif. ESPHome prévient qu'un appareil marqué « ST7123 » peut contenir une ST7123 ou une ST7121 : le seul moyen de savoir est d'essayer les deux.

**Choisir sa révision :** ajoutez une ligne dans `Tab5/user_entities.yaml`, par exemple `tab5_ecran: st7121`. Sans elle, le firmware est compilé pour la ST7123. Ce qui change d'une révision à l'autre (modèle d'écran, puce tactile) est dans `Tab5/paquets/ecran-<révision>.yaml` ; les broches, la calibration et le comportement sont communs, dans `Tab5/paquets/tab5-hardware.yaml`.

**Ce que « non testée » veut dire :** la CI compile les fichiers de l'ILI9881C et de la ST7121 à chaque changement de la configuration de l'écran, ils ne peuvent donc pas casser en silence, mais personne n'a encore lancé ce firmware sur une ILI9881C.

- **ILI9881C :** modèle d'écran et tactile GT911 repris de la [page ESPHome de l'appareil](https://devices.esphome.io/devices/m5stack-tab5/) pour la révision d'origine, avec les mêmes broches que la ST7123.
- **ST7121 :** ESPHome a un modèle d'écran officiel, mais pas de pilote tactile ST7121. D'après les commentaires du code du modèle dans ESPHome, le firmware d'usine de M5Stack distingue les deux puces en lisant la version du contrôleur tactile : elles partagent donc très probablement le même protocole, et ce projet utilise la plateforme tactile `st7123`. En juillet 2026, en testant la PR ESPHome qui ajoutait le modèle ST7121, un autre utilisateur a [signalé](https://github.com/esphome/esphome/issues/17471#issuecomment-4945713225) que ce même couple (écran `M5STACK-TAB5-ST7121`, tactile `st7123`) marche sur une vraie ST7121 : écran et tactile, précis sur tout l'écran, en paysage. Depuis octobre 2026, ce firmware lui-même tourne sur la ST7121 d'un autre utilisateur ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : écran, tactile et mot d'activation marchent. Si, chez vous, l'écran marche mais pas le tactile, le pilote tactile est le premier suspect.

**Bon à savoir :** la plupart des exemples Tab5 publiés jusqu'ici visent la révision d'origine ILI9881C. En septembre 2026, la [page ESPHome de l'appareil](https://devices.esphome.io/devices/m5stack-tab5/) n'indique encore qu'elle comme prise en charge, et [l'exemple d'IHM Home Assistant de M5Stack](https://docs.m5stack.com/en/homeassistant/applications/dashboard/tab5_ha_hmi) ne prend pas en charge les appareils fabriqués après le 14 octobre 2025. Ce projet tourne sur la ST7123 et compile pour les deux autres.

**Sur une ST7121, deux points méritent d'être vérifiés :** le tactile dans les quatre coins en paysage, et l'ouverture puis la fermeture du flipper Neon Apron, qui passe l'écran en portrait puis le remet en paysage. L'utilisateur cité plus haut tournait l'image dans le bloc `display:` ; ce firmware la tourne dans LVGL (`rotation: 270`, modifiée à la volée par Neon Apron), un chemin que personne n'a encore signalé sur une ST7121. Un [signalement](https://github.com/esphome/esphome/issues/18712) parle d'un plantage au démarrage avec la rotation LVGL sur une ST7121, mais sur un appareil renvoyé ensuite pour un défaut d'écran : le lien est incertain. Le même signalement montre que le miroir écran/tactile abîme l'image sur une ST7121 : ce firmware ne l'utilise pas.

Vous avez un Tab5 ST7121 ou ILI9881C et vous voulez bien essayer ? Racontez ce que ça a donné dans [Discussions → Hardware compatibility](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/categories/hardware-compatibility).

---

## Carte Waveshare ESP32-P4 7B

✅ **Prise en charge : tourne sur la carte de l'auteur depuis le 14/10/2026.** Le firmware peut aussi être compilé pour la Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (7 pouces, 1024×600, les mêmes ESP32-P4 et ESP32-C6 que le Tab5) : ajoutez `tab5_carte: waveshare-7b` dans `Tab5/user_entities.yaml` et compilez avec ESPHome 2026.9.1 ou plus récent ([ADR-0061](decisions/0061-board-and-screen-size.md)). L'accueil et les popups suivent la taille de son écran ; elle n'a ni capteur de mouvement, ni puce d'horloge, ni jauge de batterie (un connecteur de batterie, dont ce firmware ne se sert pas encore). Son matériel, ses broches, son installation et les problèmes rencontrés à son arrivée ont leur page : [Waveshare ESP32-P4 7B](waveshare-7b.md#version-française). Le reste de cette page parle du Tab5.

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

Le C6 a sa propre RAM (512 Ko) et fait tourner le logiciel ESP-Hosted d'Espressif, pas le nôtre : notre firmware, pile TCP/IP (lwIP) comprise, tourne sur le P4. ESPHome compile la partie P4 d'ESP-Hosted (2.12.12 avec ESPHome 2026.9) mais ne met pas le C6 à jour : il garde son logiciel d'usine tant que personne ne le reflashe. Le capteur de diagnostic **C6 Version** en donne la version, lue une fois par démarrage. Le P4 réinitialise le C6 par GPIO 15 à chaque démarrage, redémarrage logiciel compris (réglage par défaut d'ESP-Hosted, `CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP`) ; la seule fois où le C6 n'est pas revenu après une OTA : [Incidents connus](troubleshooting.md#écran-noir--appareil-absent-du-réseau-après-une-ota--le-co-processeur-wifi-nest-pas-remonté).

Les notes d'un autre projet sur Tab5, sur la broche de reset du C6 et sur le contrôle de version entre le P4 et le C6, sont [en bas de cette page](#notes-dun-autre-projet-sur-tab5).

### Horloge temps réel (RX8130CE)

Adresse 0x32 sur le bus I2C interne (`bsp_bus`, GPIO31/32), sauvegardée par un supercondensateur de 70 000 µF (spécification M5Stack). Le firmware la lit une fois au démarrage, pour que l'horloge et le réveil aient l'heure avant le réseau, et la réécrit après chaque synchro NTP. Elle stocke l'heure UTC. Le même bus porte aussi le moniteur de batterie INA226 (0x41, voir [Batterie du Tab5](tab5-battery.md#version-française)).

---

## Affichage

- **Taille :** 5 pouces
- **Résolution :** 1280 × 720 px (lot M5Stack Tab5 V2 utilisé dans ce projet)
- **Interface :** MIPI-DSI RGB565 16 bits (périphérique LCD de l'ESP32-P4)
- **Tactile :** Capacitif multi-touch via la plateforme tactile I2C officielle `st7123` d'ESPHome sur la révision ST7123 (depuis ESPHome 2026.7.0 ; l'ancien composant maison `my_components/st7123` a été retiré le 06/07/2026) — `gt911` sur la révision d'origine ILI9881C, voir [Révisions matérielles](#révisions-matérielles)

---

## Broches

Le tableau de toutes les broches que le firmware configure (GPIO de l'ESP32-P4 et broches des deux expandeurs d'E/S), le bus I2C avec l'adresse de chaque puce, et un schéma de câblage ont leur page : [Brochage du M5Stack Tab5](tab5-pinout.md#version-française). Les broches viennent du YAML du firmware, et la CI les compare à lui, en anglais et en français.

---

## Audio — DAC ES8388

Le Tab5 intègre un codec audio **ES8388**, utilisé ici pour le chemin DAC (sortie haut-parleur) via la plateforme native `audio_dac: es8388` d'ESPHome. L'entrée microphone passe par un chip ADC séparé, l'**ES7210** (`audio_adc: es7210`, 16 kHz / 16 bits), dont l'ESP32-P4 lit la sortie en I2S.

### Connexions

Les deux puces se règlent par le bus I2C interne et partagent **un seul** bus I2S (`mic_bus`) : les trois horloges sont communes, chaque sens a sa ligne de données. Une seule des deux peut tenir ce bus à la fois ([ADR-0010](decisions/0010-shared-i2s-bus-mic-speaker.md)). L'ampli du haut-parleur est commandé par le premier expandeur d'E/S (`speaker_enable`). Les broches sont dans [Broches](tab5-pinout.md#version-française).

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

Le microphone intégré est capturé par le chip ADC **ES7210** (`audio_adc: es7210`), qui streame vers l'ESP32-P4 en I2S (`adc_type: external`), sur le bus qu'il partage avec le haut-parleur (voir [Broches](tab5-pinout.md#version-française)).

Paramètres de capture : **16 kHz, 16-bit mono**. Correspond au format d'entrée attendu par le composant `micro_wake_word` et par le pipeline vocal de Home Assistant.

---

## Alimentation

L'alimentation, la détection de la batterie, le chargeur et ses verrous de sécurité, le niveau de la batterie, la limite de charge, l'économie d'énergie et les mesures derrière tout cela ont leur page : [Batterie et alimentation du Tab5](tab5-battery.md#version-française). En bref : le Tab5 est alimenté en USB-C (mesuré : 1,2 W écran éteint, 1,7 W à 50 %, 2,6 W à 100 %, [consommation](consumption.md#version-française)) ; la batterie est facultative (NP-F550, deux éléments) ; sans elle, le chargeur reste coupé.

---

## Notes d'un autre projet sur Tab5

**Notes d'un autre projet sur Tab5** ([discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), merci à Jiuhai, qui construit des appareils sur Tab5 avec ESP-IDF, sans ESPHome). Elles viennent de la lecture du schéma et du code source, pas de mesures, et l'auteur n'a pas vérifié le schéma :

- Sur le schéma, le signal de reset du C6, `RF_C6_RST`, passe par R4 jusqu'à `SOC_EXTRF_RST`, qui est le GPIO 15 du P4.
- **Firmware compilé avec ESP-IDF, sans ESPHome :** dans le Kconfig d'ESP-Hosted (vérifié en 2.12.12), la broche de reset `ESP_HOSTED_SDIO_GPIO_RESET_SLAVE` vaut 15 par défaut seulement si le préréglage de carte `ESP32P4_TAB5_C6_BOARD` est choisi, et 54 sur tout autre ESP32-P4 : l'impulsion de reset part alors sur la mauvaise broche et le C6 n'est jamais vraiment réinitialisé (c'est arrivé au premier build de ce projet). `grep GPIO_RESET_SLAVE sdkconfig` le dit. ESPHome prend la broche dans `reset_pin` (15 ici). Ce préréglage passe aussi le reset en actif bas, avec un commentaire qui dit que sinon le C6 ne sort jamais du reset sur un Tab5 ; ce firmware utilise `active_high: true` et le C6 est revenu à chaque démarrage de la tablette de l'auteur (une cinquantaine depuis le 30/09/2026). Les deux ne s'accordent pas, et l'auteur ne sait pas pourquoi.
- Au démarrage, ESP-Hosted sur le P4 compare sa version à celle du C6 et avertit `Version mismatch: Host […] > Co-proc […] ==> Upgrade co-proc to avoid RPC timeouts` (`compare_fw_version()`). Cet avertissement n'existe pas dans un firmware ESPHome : les avertissements d'ESP-IDF sont retirés à la compilation (voir [Diagnostiquer](debugging.md#après-un-plantage-ou-une-coupure-wi-fi--le-journal-des-démarrages)). Sur la tablette de l'auteur (2.12.12 sur le P4, 1.4.1 sur le C6), aucun délai RPC dépassé n'a été vu : aucun dans le journal de l'appareil depuis le 30/09/2026, aucun sur trois démarrages lus sur le port série le 07/10/2026.
- Mettre le C6 à jour n'est pas simple : un projet sur Tab5 constate que le logiciel d'usine du C6 n'offre aucun chemin de mise à jour par SDIO et prévoit d'abord une reprogrammation unique par câble ([sslivins/arctic-controller#276](https://github.com/sslivins/arctic-controller/issues/276), ouvert) ; sur une autre carte P4 + C6, après une mise à jour du logiciel hosted en 2.12.8, le lien SDIO a lâché et le C6 a redémarré en boucle, même après un retour en 2.12.1 ([esphome/esphome#16692](https://github.com/esphome/esphome/issues/16692)). Aucun des deux projets ne met le C6 à jour.
