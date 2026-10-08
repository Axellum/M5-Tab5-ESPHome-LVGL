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

**Choosing your revision:** add one line to `Tab5/user_entities.yaml`, for example `tab5_ecran: st7121`. Without it, the firmware is built for the ST7123. What differs between revisions (display model, touch chip) lives in `Tab5/paquets/ecran-<revision>.yaml`; pins, calibration and behaviour are shared in `Tab5/paquets/tab5-hardware.yaml`.

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

The C6 has its own RAM (512 KB) and runs Espressif's ESP-Hosted firmware, not ours: our firmware, including the TCP/IP stack (lwIP), runs on the P4. ESPHome builds the P4 side of ESP-Hosted (2.12.12 with ESPHome 2026.9) but does not update the C6, which keeps its factory firmware unless someone reflashes it. The diagnostic sensor **Tab5 C6 Version** reports that version once per boot. The P4 resets the C6 through GPIO 15 on every boot, software reboots included (ESP-Hosted's default setting, `CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP`); the one time the C6 did not come back after an OTA: [Troubleshooting](troubleshooting.md#black-screen--device-off-the-network-after-an-ota--the-wi-fi-co-processor-never-came-up).

**Notes from another Tab5 project** ([discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), thanks to Jiuhai, who builds Tab5 devices on ESP-IDF without ESPHome). They come from reading the schematic and the source code, not from measurements, and the author has not checked the schematic:

- On the schematic, the C6 reset net `RF_C6_RST` goes through R4 to `SOC_EXTRF_RST`, which is GPIO 15 of the P4.
- **Firmware built with ESP-IDF, without ESPHome:** in ESP-Hosted's Kconfig (checked in 2.12.12), the reset pin `ESP_HOSTED_SDIO_GPIO_RESET_SLAVE` defaults to 15 only when the board preset `ESP32P4_TAB5_C6_BOARD` is selected, and to 54 on any other ESP32-P4: the reset pulse then goes to the wrong pin and the C6 is never actually reset (it happened in that project's first build). `grep GPIO_RESET_SLAVE sdkconfig` tells. ESPHome takes the pin from `reset_pin` (15 here). That preset also switches the reset to active-low, with a comment saying that otherwise the C6 never leaves reset on a Tab5; this firmware uses `active_high: true` and the C6 has come up on every boot of the author's tablet (about fifty since 2026-09-30). The two do not agree, and the author does not know why.
- At start-up, ESP-Hosted on the P4 compares its version with the C6's and warns `Version mismatch: Host […] > Co-proc […] ==> Upgrade co-proc to avoid RPC timeouts` (`compare_fw_version()`). That warning is not in an ESPHome firmware: ESP-IDF warnings are compiled out (see [Debugging](debugging.md#after-a-crash-or-a-wi-fi-outage-the-boot-and-outage-journal)). On the author's tablet (2.12.12 on the P4, 1.4.1 on the C6), no RPC timeout has been seen: none in the device journal since 2026-09-30, none in three boots read on the serial port on 2026-10-07.
- Updating the C6 is not simple: one Tab5 project finds that the factory C6 firmware offers no update path over SDIO and plans a one-time wired reflash first ([sslivins/arctic-controller#276](https://github.com/sslivins/arctic-controller/issues/276), open); on another P4 + C6 board, after an update of the hosted firmware to 2.12.8, the SDIO link failed and the C6 boot-looped, even after going back to 2.12.1 ([esphome/esphome#16692](https://github.com/esphome/esphome/issues/16692)). Neither project updates the C6.

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

Every pin the firmware configures, as written in its YAML. The pins are the same on the three revisions. "PI4IOE 0x43, P1" means pin 1 of the PI4IOE5V6408 I/O expander at address 0x43 on the internal I2C bus; there are two of them (0x43 and 0x44). The MIPI-DSI link itself has no pin in the YAML: the display model of `Tab5/paquets/ecran-<revision>.yaml` takes care of it. The last column names the component (its `id`, or the block that has none) and the key: `tests/test_doc_broches.py` checks every row, in both languages, against the YAML, and fails if a pin of the YAML is missing here.

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

The firmware declares a single I2C bus (`bsp_bus`), so every I2C chip it drives is on it: the two I/O expanders, the touch controller, the ES8388 and the ES7210, the BMI270 motion sensor (0x68, `Tab5/paquets/tab5-imu.yaml`), the RX8130CE clock (0x32) and the INA226 battery monitor (0x41; ESPHome's default address for it, 0x40, is the ES7210's).

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

The Tab5 is USB-C powered. Its consumption has never been measured: the 1.5 A figure given here before had no source. The author's tablet runs without a battery on a PC's USB port; the history kept by Home Assistant (since 2026-09-30) shows a single brownout reset, on 2026-10-06 right after an OTA, cause unknown. A charger of **5 V / 2 A** or more leaves some margin: advice, no longer a requirement. Without a battery, the tablet starts again by itself when USB-C power comes back, after a cable pulled while it runs as after a long-press power-off.

**Power path, as read on the schematic** (Jiuhai's notes in [discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), page 5 of the schematic, not checked by the author). Without a battery, the 5 V comes from USB-C or from the HVIN pin of the Ext.Port (pin 2): HVIN → D7 → FU4 (1 A polyfuse) → MP4560 buck converter (5.04 V) → MAX40200 ideal diode (1 A absolute maximum) → `SYS_5VBUS`; USB-C joins `SYS_5VBUS` through a second MAX40200; LPW5209 load switches then feed the screen, the P4, the C6, the speaker, the camera and the EXT 5V output. As far as that reading goes, every 5 V load shares one 1 A ideal diode: at peak load (full brightness, loud audio, camera, external sensors), a brownout seems more likely than damage. Whether it explains the brownouts above (on a PC's USB port) is not known. Jiuhai's devices run on 9 V through HVIN; the author has never powered the tablet that way. The measurements announced in the discussion (power-on from HVIN after a cut, input power at idle, at peak load and during an OTA, EXT 5V under load, twenty reboots and OTAs in a row, RTC drift) will be added here when they are published.

Backlight brightness is software-controlled via PWM (LEDC output on GPIO 22, `light: monochromatic`) and can be dimmed from Home Assistant to reduce power draw. Touching the screen while the backlight is off turns it back on (`touchscreen: on_release`). There is no ambient light sensor in this configuration.

**Battery.** The Tab5 takes an optional two-cell (2S) lithium battery. Before 3.6.0 the firmware never enabled its charger, and a user reported that the battery did not charge ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). From 3.6.0 (2026-10-04) up to the latest published version, it enables the charger at every boot (`charge_enable`, PI4IOE 0x44 P7), like M5Stack's M5Unified library, battery or not; the next version switches it on only when it finds a battery (below). Quick charge (P5) is kept off: the tablet stays plugged in, and standard charge asks less current from the USB charger. Three diagnostic entities reach Home Assistant: **Tab5 Batterie en charge** (charging status, P6, read every 10 s, a change published once it has lasted 30 s), **Tab5 Tension batterie** (voltage measured by the INA226, published on a 50 mV change or every 15 min) and **Tab5 Batterie** (level estimated from the voltage, 6.0 V = 0 %, 8.23 V = 100 %, the line of ESPHome's reference configuration; it reads too high while charging; "unknown" while no battery is detected). They are **disabled by default** in Home Assistant: with the battery fitted, enable them on the device page.

**Without a battery, with the charger on** (3.6.0 up to the latest published version), the charger charges into nothing. It says "charging", and the INA226 reads the charger or the USB, never a battery: 4.2 V on the tablet of a user whose fitted original battery reads about 7.2 V, with a charging status that follows reality (2026-10-05); on the author's tablet, which has none, 4.2 V and 8.39 V alternating every 1 to 3 minutes (evening of 2026-10-03), then a steady 5.71 V (2026-10-04), around 5.70 to 5.76 V on 2026-10-08. 8.39 V is therefore one of the values read without a battery, not a full battery. It also makes a faint continuous hiss, which stops when the charger is switched off ([troubleshooting](troubleshooting.md#faint-continuous-hiss-on-a-tablet-without-a-battery-2026-10-08)). Until the next version, "no battery" means a reading below 6.0 V in the last 10 minutes (since 2026-10-05, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): the alternating case climbs back above 6 V between two low readings, and the steady 5.7 V case stays below.

**Battery detection with the charger off** (next version). With the charger off, the INA226 reads the battery itself, or almost nothing: on the author's tablet, without a battery, **1.83 to 1.94 V** (four readings, 2026-10-08), with the charging status at "not charging". A 2S battery, even an empty one, reads more than 5 V. So, charger off, **3.0 V or more** means a battery, less means none. When the tablet looks:

- at boot, the charger stays on for about 30 s (it wakes up a battery in protection), then a probe switches it off for about 8 s and reads the voltage;
- charger on, with a battery: a probe every 10 minutes, and right after a reading below 6 V, to see whether the battery was taken out;
- charger off: every reading (every 60 s) decides, so a battery slid in while the tablet runs is seen within a minute;
- no battery found while the **Tab5 Batterie montée** switch says there is one: the charger is switched on for 30 s once an hour, then probed, in case the battery was so empty that its protection had cut it off. With the switch off (no battery declared), the charger is never switched on again.

Without a battery, the charger stays off: no more hiss, no more false "charging". A fourth diagnostic entity, **Tab5 Batterie détectée**, enabled by default, publishes the decision (unknown before the first reading). Measured so far: only the reading without a battery, on the author's tablet; with a battery, not measured yet.

**On the screen**, an icon closes the status bar (top left, after the alarm bell) once the device switch **Tab5 Batterie montée** is on; it is off by default and survives reboots. With no battery detected the icon is a **plug**, in the theme's text colour; with a battery its glyph follows the level (full above 80 %, half, low, "!" below 20 %, a bolt while charging) and its colour is the phone's scale (`get_battery_color()`: green, blue, amber, red); "?" in grey before the first reading. The icon reads the sensors on the tablet itself: it works even with the battery entities left disabled in Home Assistant.

**Charge limit** (next version). The device setting **Tab5 Limite de charge**: « 100 % » (the default) or « 80 % »: charging stops at 80 % and starts again at 70 %, after at least 10 minutes stopped. It is meant for a tablet that stays plugged in: a lithium battery kept below full lasts longer. The level comes from the voltage (the scale above, 6.0 V = 0 %, 8.23 V = 100 %), and the voltage is higher while charging than at rest, so charging stops a little before a real 80 %. Not measured yet: with the charger stopped and USB plugged in, does the tablet run from USB or from its battery? If it runs from its battery, the 80 % mode makes it go back and forth between 80 and 70 % instead of resting it.

**Power drawn** (next version). **Tab5 Consommation** (W) is the battery voltage × the battery current, while the battery is discharging; it is unknown on mains, since the USB current is not measured. On the tablet, the « Battery » line of the system console shows the level and that power on battery (for example « 78% · 2.4 W »), otherwise the level and the voltage. Not measured on a tablet yet. To measure it case by case (screen, microphone, speaker, theme, energy saving), the Home Assistant script « Tab5 — consumption test » (`packages/tab5_mesure_conso.yaml`, [described here](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/README.md#packagestab5_mesure_consoyaml--consumption-test)) switches on **Tab5 Mesure de consommation** (next version), which reads the battery every 2 s instead of every minute, for 2 h at most.

**Low battery alert** (next version). On battery, when the level drops below 20 %, then below 10 %, the tablet sends the `esphome.tab5_batterie_faible` event (level, threshold), once per threshold, re-armed at 30 % or when charging resumes. The « batterie faible » guard of the `tab5_health` package turns it into a persistent notification (the 10 % one replaces the 20 % one) and a phone notification.

**Battery current and energy saving** (2026-10-06). The INA226 also gives the battery current, published as **Tab5 Courant batterie** (diagnostic, disabled by default like the voltage): positive while the battery is discharging, that is while the tablet runs on it, negative while it charges. That sign comes from M5Stack's M5Unified library (`getBatteryCurrent`: the shunt is wired so that charging reads negative; ESPHome publishes the register as it is), and a user's tablet with its battery confirmed it on 2026-10-07 ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). Each reading (every 60 s) decides **Tab5 Sur batterie**: above 50 mA of discharge, on battery; below 20 mA, plugged in; never while charging (it switches back as soon as charging resumes) nor without a detected battery. The device setting **Tab5 Économie d'énergie** (« Sur batterie » by default) uses it: on battery, the brightness is capped at 50 % at the backlight output (Home Assistant keeps the brightness you chose), drops to the minimum (10 %) after 30 s without a touch and at 35 % of battery or less, the animations stop and LVGL redraws at most 30 times a second outside games. Thresholds: `Tab5/socle/tab5_economie.h`, tested on a PC by `tools/test_alarm_clock.cpp`; settings: [settings](installation/settings.md).

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

**Choisir sa révision :** ajoutez une ligne dans `Tab5/user_entities.yaml`, par exemple `tab5_ecran: st7121`. Sans elle, le firmware est compilé pour la ST7123. Ce qui change d'une révision à l'autre (modèle d'écran, puce tactile) est dans `Tab5/paquets/ecran-<révision>.yaml` ; les broches, la calibration et le comportement sont communs, dans `Tab5/paquets/tab5-hardware.yaml`.

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

Le C6 a sa propre RAM (512 Ko) et fait tourner le logiciel ESP-Hosted d'Espressif, pas le nôtre : notre firmware, pile TCP/IP (lwIP) comprise, tourne sur le P4. ESPHome compile la partie P4 d'ESP-Hosted (2.12.12 avec ESPHome 2026.9) mais ne met pas le C6 à jour : il garde son logiciel d'usine tant que personne ne le reflashe. Le capteur de diagnostic **Tab5 C6 Version** en donne la version, lue une fois par démarrage. Le P4 réinitialise le C6 par GPIO 15 à chaque démarrage, redémarrage logiciel compris (réglage par défaut d'ESP-Hosted, `CONFIG_ESP_HOSTED_SLAVE_RESET_ON_EVERY_HOST_BOOTUP`) ; la seule fois où le C6 n'est pas revenu après une OTA : [Incidents connus](troubleshooting.md#écran-noir--appareil-absent-du-réseau-après-une-ota--le-co-processeur-wifi-nest-pas-remonté).

**Notes d'un autre projet sur Tab5** ([discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), merci à Jiuhai, qui construit des appareils sur Tab5 avec ESP-IDF, sans ESPHome). Elles viennent de la lecture du schéma et du code source, pas de mesures, et l'auteur n'a pas vérifié le schéma :

- Sur le schéma, le signal de reset du C6, `RF_C6_RST`, passe par R4 jusqu'à `SOC_EXTRF_RST`, qui est le GPIO 15 du P4.
- **Firmware compilé avec ESP-IDF, sans ESPHome :** dans le Kconfig d'ESP-Hosted (vérifié en 2.12.12), la broche de reset `ESP_HOSTED_SDIO_GPIO_RESET_SLAVE` vaut 15 par défaut seulement si le préréglage de carte `ESP32P4_TAB5_C6_BOARD` est choisi, et 54 sur tout autre ESP32-P4 : l'impulsion de reset part alors sur la mauvaise broche et le C6 n'est jamais vraiment réinitialisé (c'est arrivé au premier build de ce projet). `grep GPIO_RESET_SLAVE sdkconfig` le dit. ESPHome prend la broche dans `reset_pin` (15 ici). Ce préréglage passe aussi le reset en actif bas, avec un commentaire qui dit que sinon le C6 ne sort jamais du reset sur un Tab5 ; ce firmware utilise `active_high: true` et le C6 est revenu à chaque démarrage de la tablette de l'auteur (une cinquantaine depuis le 30/09/2026). Les deux ne s'accordent pas, et l'auteur ne sait pas pourquoi.
- Au démarrage, ESP-Hosted sur le P4 compare sa version à celle du C6 et avertit `Version mismatch: Host […] > Co-proc […] ==> Upgrade co-proc to avoid RPC timeouts` (`compare_fw_version()`). Cet avertissement n'existe pas dans un firmware ESPHome : les avertissements d'ESP-IDF sont retirés à la compilation (voir [Diagnostiquer](debugging.md#après-un-plantage-ou-une-coupure-wi-fi--le-journal-des-démarrages)). Sur la tablette de l'auteur (2.12.12 sur le P4, 1.4.1 sur le C6), aucun délai RPC dépassé n'a été vu : aucun dans le journal de l'appareil depuis le 30/09/2026, aucun sur trois démarrages lus sur le port série le 07/10/2026.
- Mettre le C6 à jour n'est pas simple : un projet sur Tab5 constate que le logiciel d'usine du C6 n'offre aucun chemin de mise à jour par SDIO et prévoit d'abord une reprogrammation unique par câble ([sslivins/arctic-controller#276](https://github.com/sslivins/arctic-controller/issues/276), ouvert) ; sur une autre carte P4 + C6, après une mise à jour du logiciel hosted en 2.12.8, le lien SDIO a lâché et le C6 a redémarré en boucle, même après un retour en 2.12.1 ([esphome/esphome#16692](https://github.com/esphome/esphome/issues/16692)). Aucun des deux projets ne met le C6 à jour.

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

Toutes les broches que le firmware configure, telles qu'écrites dans son YAML. Elles sont les mêmes sur les trois révisions. « PI4IOE 0x43, P1 » veut dire la broche 1 de l'expandeur d'E/S PI4IOE5V6408 d'adresse 0x43 sur le bus I2C interne ; il y en a deux (0x43 et 0x44). La liaison MIPI-DSI elle-même n'a aucune broche dans le YAML : le modèle d'écran de `Tab5/paquets/ecran-<révision>.yaml` s'en charge. La dernière colonne nomme le composant (son `id`, ou le bloc qui n'en a pas) et la clé : `tests/test_doc_broches.py` compare chaque ligne au YAML, dans les deux langues, et échoue si une broche du YAML manque ici.

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

Le firmware ne déclare qu'un bus I2C (`bsp_bus`) : toutes les puces I2C qu'il pilote y sont, les deux expandeurs d'E/S, le contrôleur tactile, l'ES8388 et l'ES7210, le capteur de mouvement BMI270 (0x68, `Tab5/paquets/tab5-imu.yaml`), l'horloge RX8130CE (0x32) et le moniteur de batterie INA226 (0x41 ; l'adresse qu'ESPHome lui donne par défaut, 0x40, est celle de l'ES7210).

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

Le Tab5 est alimenté en USB-C. Sa consommation n'a jamais été mesurée : le chiffre de 1,5 A donné ici avant n'avait pas de source. La tablette de l'auteur tourne sans batterie sur un port USB de PC ; l'historique gardé par Home Assistant (depuis le 30/09/2026) montre un seul reset par sous-tension, le 06/10/2026 juste après une OTA, cause inconnue. Un chargeur de **5 V / 2 A** ou plus laisse de la marge : un conseil, plus une exigence. Sans batterie, la tablette redémarre seule au retour de l'alimentation USB-C, après un câble tiré en marche comme après une extinction par appui long.

**Chemin de l'alimentation, lu sur le schéma** (notes de Jiuhai dans la [discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), page 5 du schéma, non vérifiées par l'auteur). Sans batterie, le 5 V vient de l'USB-C ou de la broche HVIN de l'Ext.Port (broche 2) : HVIN → D7 → FU4 (fusible réarmable de 1 A) → convertisseur abaisseur MP4560 (5,04 V) → diode idéale MAX40200 (1 A au maximum absolu) → `SYS_5VBUS` ; l'USB-C rejoint `SYS_5VBUS` par une seconde MAX40200 ; des interrupteurs de charge LPW5209 alimentent ensuite l'écran, le P4, le C6, le haut-parleur, la caméra et la sortie EXT 5V. Selon cette lecture, toutes les charges en 5 V se partagent une seule diode idéale de 1 A : en pointe (luminosité au maximum, son fort, caméra, capteurs externes), une sous-tension semble plus probable qu'une casse. On ne sait pas si cela explique les sous-tensions ci-dessus (sur un port USB de PC). Les appareils de Jiuhai tournent en 9 V par HVIN ; l'auteur n'a jamais alimenté la tablette ainsi. Les mesures annoncées dans la discussion (démarrage par HVIN après une coupure, puissance absorbée au repos, en pointe et pendant une OTA, EXT 5V en charge, vingt redémarrages et OTA d'affilée, dérive de l'horloge) seront ajoutées ici quand elles seront publiées.

La luminosité du rétroéclairage est contrôlée logiciellement via PWM (sortie LEDC sur GPIO 22, `light: monochromatic`) et peut être réduite depuis Home Assistant. Toucher l'écran quand le rétroéclairage est éteint le rallume (`touchscreen: on_release`). Il n'y a pas de capteur de luminosité ambiante dans cette configuration.

**Batterie.** Le Tab5 accepte une batterie lithium à deux éléments (2S) en option. Avant la 3.6.0, le firmware n'activait jamais son chargeur, et un utilisateur a signalé que la batterie ne se chargeait pas ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). De la 3.6.0 (04/10/2026) à la dernière version publiée, il active le chargeur à chaque démarrage (`charge_enable`, PI4IOE 0x44 P7), comme la bibliothèque M5Unified de M5Stack, batterie ou pas ; la prochaine version ne l'allume que quand elle trouve une batterie (plus bas). La charge rapide (P5) reste à l'arrêt : la tablette reste branchée, et la charge standard demande moins de courant au chargeur USB. Trois entités de diagnostic arrivent dans Home Assistant : **Tab5 Batterie en charge** (état de charge, P6, lu toutes les 10 s, un changement publié quand il a tenu 30 s), **Tab5 Tension batterie** (tension mesurée par l'INA226, publiée quand elle bouge de 50 mV ou toutes les 15 min) et **Tab5 Batterie** (niveau estimé d'après la tension, 6,0 V = 0 %, 8,23 V = 100 %, la droite de la config de référence ESPHome ; il lit trop haut pendant la charge ; « inconnu » tant qu'aucune batterie n'est détectée). Elles sont **désactivées par défaut** dans Home Assistant ; avec la batterie montée, les activer sur la page de l'appareil.

**Sans batterie, chargeur allumé** (de la 3.6.0 à la dernière version publiée), le chargeur charge dans le vide. Il dit « en charge », et l'INA226 lit le chargeur ou l'USB, jamais une batterie : 4,2 V sur la tablette d'un utilisateur dont la batterie d'origine montée lit environ 7,2 V, avec un état de charge qui suit la réalité (05/10/2026) ; sur la tablette de l'auteur, qui n'en a pas, 4,2 V et 8,39 V en alternance toutes les 1 à 3 minutes (soir du 03/10/2026), puis 5,71 V stable (04/10/2026), vers 5,70 à 5,76 V le 08/10/2026. 8,39 V est donc une des valeurs lues sans batterie, pas une batterie pleine. Il fait aussi un léger souffle continu, qui s'arrête quand on coupe le chargeur ([dépannage](troubleshooting.md#léger-souffle-continu-sur-une-tablette-sans-batterie-08102026)). Jusqu'à la prochaine version, « pas de batterie » veut dire une lecture sous 6,0 V dans les 10 dernières minutes (depuis le 05/10/2026, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : le cas de l'alternance repasse au-dessus de 6 V entre deux lectures basses, et le cas des 5,7 V stables reste en dessous.

**Détection de la batterie chargeur coupé** (prochaine version). Chargeur coupé, l'INA226 lit la batterie elle-même, ou presque rien : sur la tablette de l'auteur, sans batterie, **1,83 à 1,94 V** (quatre lectures, 08/10/2026), avec l'état de charge à « pas en charge ». Une batterie 2S, même vide, lit plus de 5 V. Chargeur coupé, **3,0 V ou plus** veut donc dire une batterie, moins veut dire aucune. Quand la tablette regarde :

- au démarrage, le chargeur reste allumé environ 30 s (il réveille une batterie en protection), puis une sonde le coupe environ 8 s et lit la tension ;
- chargeur allumé, avec une batterie : une sonde toutes les 10 minutes, et tout de suite après une lecture sous 6 V, pour voir si la batterie a été retirée ;
- chargeur coupé : chaque lecture (toutes les 60 s) décide, donc une batterie glissée tablette allumée est vue dans la minute ;
- pas de batterie trouvée alors que l'interrupteur **Tab5 Batterie montée** dit qu'il y en a une : le chargeur est rallumé 30 s une fois par heure, puis une sonde, au cas où la batterie était si vide que sa protection l'avait coupée. Interrupteur éteint (aucune batterie déclarée), le chargeur n'est jamais rallumé.

Sans batterie, le chargeur reste coupé : plus de souffle, plus de faux « en charge ». Une quatrième entité de diagnostic, **Tab5 Batterie détectée**, activée par défaut, publie la décision (inconnue avant la première lecture). Mesuré à ce jour : seulement la lecture sans batterie, sur la tablette de l'auteur ; avec une batterie, pas encore mesuré.

**À l'écran**, une icône termine le bandeau d'état (en haut à gauche, après la cloche du réveil) quand l'interrupteur de l'appareil **Tab5 Batterie montée** est allumé ; il est éteint par défaut et survit aux redémarrages. Sans batterie détectée, l'icône est une **prise**, de la couleur du texte du thème ; avec une batterie, son glyphe suit le niveau (pleine au-dessus de 80 %, moitié, basse, « ! » sous 20 %, un éclair pendant la charge) et sa couleur est l'échelle du téléphone (`get_battery_color()` : vert, bleu, ambre, rouge) ; « ? » en gris avant la première lecture. L'icône lit les capteurs sur la tablette même : elle marche aussi avec les entités de la batterie laissées désactivées dans Home Assistant.

**Limite de charge** (prochaine version). Le réglage de l'appareil **Tab5 Limite de charge** : « 100 % » (le défaut) ou « 80 % » : la charge s'arrête à 80 % et reprend à 70 %, après au moins 10 minutes d'arrêt. C'est pour une tablette toujours branchée : une batterie lithium gardée sous le plein dure plus longtemps. Le niveau vient de la tension (l'échelle plus haut, 6,0 V = 0 %, 8,23 V = 100 %), et la tension est plus haute pendant la charge qu'au repos : la charge s'arrête donc un peu avant 80 % réels. Pas encore mesuré : chargeur arrêté et USB branché, la tablette tourne-t-elle sur l'USB ou sur sa batterie ? Si c'est sur sa batterie, le mode 80 % la fait aller et venir entre 80 et 70 % au lieu de la mettre au repos.

**Consommation** (prochaine version). **Tab5 Consommation** (W) est la tension × le courant de la batterie, quand la batterie se décharge ; elle est inconnue sur secteur, le courant de l'USB n'étant pas mesuré. Sur la tablette, la ligne « Batterie » de la console système montre le niveau et cette puissance sur batterie (par exemple « 78% · 2.4 W »), sinon le niveau et la tension. Pas encore mesurée sur une tablette. Pour la mesurer cas par cas (écran, micro, haut-parleur, thème, économie d'énergie), le script Home Assistant « Tab5 — consumption test » (`packages/tab5_mesure_conso.yaml`, [décrit ici](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/README.md#packagestab5_mesure_consoyaml--test-de-consommation)) allume **Tab5 Mesure de consommation** (prochaine version), qui lit la batterie toutes les 2 s au lieu d'une fois par minute, 2 h au plus.

**Alerte de batterie faible** (prochaine version). Sur batterie, quand le niveau passe sous 20 %, puis sous 10 %, la tablette envoie l'événement `esphome.tab5_batterie_faible` (niveau, seuil), une fois par seuil, ré-armé à 30 % ou quand la charge reprend. La garde « batterie faible » du package `tab5_health` le transforme en notification persistante (celle de 10 % remplace celle de 20 %) et en notification sur le téléphone.

**Courant de la batterie et économie d'énergie** (06/10/2026). L'INA226 donne aussi le courant de la batterie, publié en **Tab5 Courant batterie** (diagnostic, désactivé par défaut comme la tension) : positif quand la batterie se décharge, c'est-à-dire quand la tablette tourne sur elle, négatif quand elle charge. Ce sens vient de la bibliothèque M5Unified de M5Stack (`getBatteryCurrent` : le shunt est câblé pour que la charge se lise négative ; ESPHome publie le registre tel quel), et la tablette d'un utilisateur, avec sa batterie, l'a confirmé le 07/10/2026 ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). Chaque lecture (toutes les 60 s) décide **Tab5 Sur batterie** : au-dessus de 50 mA de décharge, sur batterie ; sous 20 mA, branchée ; jamais pendant la charge (elle repasse dès que la charge reprend) ni sans batterie détectée. Le réglage de l'appareil **Tab5 Économie d'énergie** (« Sur batterie » par défaut) s'en sert : sur batterie, la luminosité est plafonnée à 50 % à la sortie du rétroéclairage (Home Assistant garde la luminosité choisie), descend au plus bas (10 %) après 30 s sans toucher et à 35 % de batterie ou moins, les animations s'arrêtent et LVGL redessine 30 fois par seconde au plus hors des jeux. Seuils : `Tab5/socle/tab5_economie.h`, testés sur PC par `tools/test_alarm_clock.cpp` ; réglages : [réglages](installation/settings.md#version-française).
