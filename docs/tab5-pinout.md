# M5Stack Tab5 pinout: GPIO, I/O expanders and I2C addresses

## English · [Français](#version-française)

---

Every pin this firmware configures on the M5Stack Tab5, with the ESP32-P4 GPIO or the I/O expander pin behind it, the I2C bus and the address of each chip on it. **The pins on this page come from the firmware's YAML, not from a datasheet:** the project's CI compares every row with the YAML, in English and in French (`tests/test_doc_broches.py`), and fails if a pin the YAML configures is missing here or differs. The diagram is checked the same way. The pins are the same on the three display revisions ([hardware revisions](hardware.md#hardware-revisions)). For what each part is, see the [hardware reference](hardware.md); for the charger lines, the [battery page](tab5-battery.md).

![Wiring diagram of the M5Stack Tab5: the ESP32-P4 linked to the ESP32-C6 Wi-Fi co-processor over SDIO, to the display, and to the ES8388 and ES7210 codecs over I2S, with the I/O expanders, touch controller, motion sensor, clock and battery monitor on one I2C bus](images/m5stack-tab5-pinout-diagram.svg)

## Pin map

Every pin the firmware configures, as written in its YAML. "PI4IOE 0x43, P1" means pin 1 of the PI4IOE5V6408 I/O expander at address 0x43 on the internal I2C bus; there are two of them (0x43 and 0x44). The MIPI-DSI link itself has no pin in the YAML: the display model of `Tab5/paquets/ecran-<revision>.yaml` takes care of it. The last column names the component (its `id`, or the block that has none) and the key: `tests/test_doc_broches.py` checks every row, in both languages, against the YAML, and fails if a pin of the YAML is missing here.

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

## I2C bus and chip addresses

The firmware declares a single I2C bus (`bsp_bus`, GPIO 31 and 32, 400 kHz), so every I2C chip it drives is on it. The camera of the Tab5 shares these two lines too ([M5Stack's PinMap](https://docs.m5stack.com/en/core/Tab5)); this firmware does not use it. Four addresses are written in the YAML and checked by the same test; the others are the chips' own, as M5Stack's PinMap gives them (read on 2026-11-06).

| Chip | Address | Source |
|---|---|---|
| PI4IOE5V6408 I/O expander 1 | 0x43 | `pi4ioe1` · `address` |
| PI4IOE5V6408 I/O expander 2 | 0x44 | `pi4ioe2` · `address` |
| BMI270 motion sensor | 0x68 | `tab5_imu` · `address` |
| INA226 battery monitor | 0x41 | `ina226_batterie` · `address` (ESPHome's default for it, 0x40, is the ES7210's) |
| ES8388 audio codec (speaker) | 0x10 | M5Stack's PinMap, not written in the YAML |
| ES7210 audio ADC (microphone) | 0x40 | M5Stack's PinMap, not written in the YAML |
| RX8130CE real-time clock | 0x32 | M5Stack's PinMap, not written in the YAML |
| Touch controller, ST7123 or ST7121 (GT911 on the ILI9881C revision) | 0x55 (GT911: 0x14) | M5Stack's PinMap, not written in the YAML |

## Compared with M5Stack and ESPHome

On 2026-11-06 the pins of this table were compared with two other sources, read that day:

- **M5Stack's PinMap** ([docs.m5stack.com/en/core/Tab5](https://docs.m5stack.com/en/core/Tab5)): the same I2C pins (GPIO 31 and 32), touch interrupt (GPIO 23), backlight (GPIO 22), I2S pins of the two codecs (GPIO 27, 29, 30, 28 and 26) and ESP32-C6 link: SDIO data 0 to 3 on GPIO 11, 10, 9 and 8, command GPIO 13, clock GPIO 12, reset GPIO 15. The expanders agree too (0x43: antenna select P0, speaker amplifier P1, external 5 V P2, display reset P4, touch reset P5, headphone detect P7; 0x44: Wi-Fi power P0, USB power P3, quick charge P5, charging status P6, charge enable P7). M5Stack also lists a GPIO 14 wired to the C6's IO2, the camera reset on 0x43 P6 and a power-off pulse on 0x44 P4: this firmware configures none of them.
- **ESPHome's device page** ([the page](https://devices.esphome.io/devices/m5stack-tab5/), its `config.yaml` read in the [devices.esphome.io repository](https://github.com/esphome/devices.esphome.io/tree/main/src/docs/devices/M5Stack-Tab5)): the same SDIO, I2C, I2S, touch, backlight and expander pins.

This table was not compared with pinouts published elsewhere. If one disagrees with it, this table is what the firmware's YAML configures, and the CI compares it with the YAML at every change.

## Pins this firmware does not use

From M5Stack's PinMap (read on 2026-11-06), for readers who want the whole board. **Not in this firmware's YAML, so not checked by the CI.**

| Part | Pins (M5Stack's PinMap) |
|---|---|
| Camera SC2356 (MIPI-CSI) | I2C on GPIO 31 and 32 (shared), master clock GPIO 36, data on the dedicated CSI lanes; reset on the expander (0x43 P6) |
| microSD, SPI mode | MISO GPIO 39, CS GPIO 42, SCK GPIO 43, MOSI GPIO 44 |
| microSD, SDIO mode | DAT0 GPIO 39, DAT1 GPIO 40, DAT2 GPIO 41, DAT3 GPIO 42, CLK GPIO 43, CMD GPIO 44 |
| RS485 (SIT3088) | RX GPIO 21, TX GPIO 20, direction GPIO 34 |
| HY2.0-4P connector (PORT.A) | GND, 5 V, GPIO 53, GPIO 54 |
| ESP32-C6 | GPIO 14 wired to its IO2, besides the SDIO lines and the reset above |
| Display (MIPI-DSI) | dedicated lanes (clock, data 0 and 1), no GPIO; backlight on GPIO 22 |

The **M-Bus** connector on the back (30 pins, M5Stack's PinMap):

| Pins | Left | Right |
|---|---|---|
| 1 and 2 | GND | GPIO 16 |
| 3 and 4 | GND | GPIO 17 (PB_IN) |
| 5 and 6 | GND | RST (EN) |
| 7 and 8 | GPIO 18 (MOSI) | GPIO 45 |
| 9 and 10 | GPIO 19 (MISO) | GPIO 52 (PB_OUT) |
| 11 and 12 | GPIO 5 (SCK) | 3V3 |
| 13 and 14 | GPIO 38 (RXD0) | GPIO 37 (TXD0) |
| 15 and 16 | GPIO 7 (PC_RX) | GPIO 6 (PC_TX) |
| 17 and 18 | GPIO 31 (internal SDA) | GPIO 32 (internal SCL) |
| 19 and 20 | GPIO 3 | GPIO 4 |
| 21 and 22 | GPIO 2 | GPIO 48 |
| 23 and 24 | GPIO 47 | GPIO 35 |
| 25 and 26 | HVIN | GPIO 51 |
| 27 and 28 | HVIN | 5 V |
| 29 and 30 | HVIN | BAT |

---

## Version Française

---

Toutes les broches que ce firmware configure sur le M5Stack Tab5, avec le GPIO de l'ESP32-P4 ou la broche d'expandeur d'E/S derrière chacune, le bus I2C et l'adresse de chaque puce qui y est. **Les broches de cette page viennent du YAML du firmware, pas d'une fiche technique :** la CI du projet compare chaque ligne au YAML, en anglais et en français (`tests/test_doc_broches.py`), et échoue si une broche configurée par le YAML manque ici ou diffère. Le schéma est vérifié de la même façon. Les broches sont les mêmes sur les trois révisions d'écran ([révisions matérielles](hardware.md#révisions-matérielles)). Pour savoir ce qu'est chaque pièce, voir la [référence matérielle](hardware.md#version-française) ; pour les lignes du chargeur, la [page de la batterie](tab5-battery.md#version-française).

![Schéma de câblage du M5Stack Tab5 : l'ESP32-P4 relié au co-processeur Wi-Fi ESP32-C6 par SDIO, à l'écran, et aux codecs ES8388 et ES7210 par I2S, avec les expandeurs d'E/S, le contrôleur tactile, le capteur de mouvement, l'horloge et le moniteur de batterie sur un seul bus I2C](images/m5stack-tab5-pinout-diagram-fr.svg)

## Broches

Toutes les broches que le firmware configure, telles qu'écrites dans son YAML. « PI4IOE 0x43, P1 » veut dire la broche 1 de l'expandeur d'E/S PI4IOE5V6408 d'adresse 0x43 sur le bus I2C interne ; il y en a deux (0x43 et 0x44). La liaison MIPI-DSI elle-même n'a aucune broche dans le YAML : le modèle d'écran de `Tab5/paquets/ecran-<révision>.yaml` s'en charge. La dernière colonne nomme le composant (son `id`, ou le bloc qui n'en a pas) et la clé : `tests/test_doc_broches.py` compare chaque ligne au YAML, dans les deux langues, et échoue si une broche du YAML manque ici.

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

## Bus I2C et adresses des puces

Le firmware ne déclare qu'un bus I2C (`bsp_bus`, GPIO 31 et 32, 400 kHz) : toutes les puces I2C qu'il pilote y sont. La caméra du Tab5 partage aussi ces deux lignes ([brochage de M5Stack](https://docs.m5stack.com/en/core/Tab5)) ; ce firmware ne s'en sert pas. Quatre adresses sont écrites dans le YAML et vérifiées par le même test ; les autres sont celles des puces elles-mêmes, telles que le brochage de M5Stack les donne (lu le 06/11/2026).

| Puce | Adresse | Source |
|---|---|---|
| Expandeur d'E/S PI4IOE5V6408 n° 1 | 0x43 | `pi4ioe1` · `address` |
| Expandeur d'E/S PI4IOE5V6408 n° 2 | 0x44 | `pi4ioe2` · `address` |
| Capteur de mouvement BMI270 | 0x68 | `tab5_imu` · `address` |
| Moniteur de batterie INA226 | 0x41 | `ina226_batterie` · `address` (l'adresse par défaut d'ESPHome, 0x40, est celle de l'ES7210) |
| Codec audio ES8388 (haut-parleur) | 0x10 | brochage de M5Stack, pas écrite dans le YAML |
| ADC audio ES7210 (micro) | 0x40 | brochage de M5Stack, pas écrite dans le YAML |
| Horloge temps réel RX8130CE | 0x32 | brochage de M5Stack, pas écrite dans le YAML |
| Contrôleur tactile, ST7123 ou ST7121 (GT911 sur la révision ILI9881C) | 0x55 (GT911 : 0x14) | brochage de M5Stack, pas écrite dans le YAML |

## Comparé à M5Stack et à ESPHome

Le 06/11/2026, les broches de ce tableau ont été comparées à deux autres sources, lues ce jour-là :

- **Le brochage de M5Stack** ([docs.m5stack.com/en/core/Tab5](https://docs.m5stack.com/en/core/Tab5)) : les mêmes broches I2C (GPIO 31 et 32), d'interruption du tactile (GPIO 23), de rétroéclairage (GPIO 22), I2S des deux codecs (GPIO 27, 29, 30, 28 et 26) et de la liaison ESP32-C6 : données SDIO 0 à 3 sur GPIO 11, 10, 9 et 8, commande GPIO 13, horloge GPIO 12, reset GPIO 15. Les expandeurs s'accordent aussi (0x43 : choix d'antenne P0, ampli du haut-parleur P1, 5 V externe P2, reset de l'écran P4, reset du tactile P5, détection du casque P7 ; 0x44 : alimentation du Wi-Fi P0, alimentation USB P3, charge rapide P5, état de charge P6, activation de la charge P7). M5Stack liste en plus un GPIO 14 relié à l'IO2 du C6, le reset de la caméra sur 0x43 P6 et une impulsion d'extinction sur 0x44 P4 : ce firmware n'en configure aucun.
- **La page ESPHome de l'appareil** ([la page](https://devices.esphome.io/devices/m5stack-tab5/), son `config.yaml` lu dans le [dépôt devices.esphome.io](https://github.com/esphome/devices.esphome.io/tree/main/src/docs/devices/M5Stack-Tab5)) : les mêmes broches SDIO, I2C, I2S, tactile, rétroéclairage et expandeurs.

Ce tableau n'a pas été comparé aux brochages publiés ailleurs. Si l'un d'eux diffère, ce tableau est ce que le YAML du firmware configure, et la CI le compare au YAML à chaque changement.

## Broches que ce firmware n'utilise pas

D'après le brochage de M5Stack (lu le 06/11/2026), pour qui veut toute la carte. **Absentes du YAML de ce firmware, donc non vérifiées par la CI.**

| Pièce | Broches (brochage de M5Stack) |
|---|---|
| Caméra SC2356 (MIPI-CSI) | I2C sur GPIO 31 et 32 (partagé), horloge maître GPIO 36, données sur les voies CSI dédiées ; reset sur l'expandeur (0x43 P6) |
| microSD, mode SPI | MISO GPIO 39, CS GPIO 42, SCK GPIO 43, MOSI GPIO 44 |
| microSD, mode SDIO | DAT0 GPIO 39, DAT1 GPIO 40, DAT2 GPIO 41, DAT3 GPIO 42, CLK GPIO 43, CMD GPIO 44 |
| RS485 (SIT3088) | RX GPIO 21, TX GPIO 20, direction GPIO 34 |
| Connecteur HY2.0-4P (PORT.A) | GND, 5 V, GPIO 53, GPIO 54 |
| ESP32-C6 | GPIO 14 relié à son IO2, en plus des lignes SDIO et du reset ci-dessus |
| Écran (MIPI-DSI) | voies dédiées (horloge, données 0 et 1), pas de GPIO ; rétroéclairage sur GPIO 22 |

Le connecteur **M-Bus** au dos (30 broches, brochage de M5Stack) :

| Broches | Gauche | Droite |
|---|---|---|
| 1 et 2 | GND | GPIO 16 |
| 3 et 4 | GND | GPIO 17 (PB_IN) |
| 5 et 6 | GND | RST (EN) |
| 7 et 8 | GPIO 18 (MOSI) | GPIO 45 |
| 9 et 10 | GPIO 19 (MISO) | GPIO 52 (PB_OUT) |
| 11 et 12 | GPIO 5 (SCK) | 3V3 |
| 13 et 14 | GPIO 38 (RXD0) | GPIO 37 (TXD0) |
| 15 et 16 | GPIO 7 (PC_RX) | GPIO 6 (PC_TX) |
| 17 et 18 | GPIO 31 (SDA interne) | GPIO 32 (SCL interne) |
| 19 et 20 | GPIO 3 | GPIO 4 |
| 21 et 22 | GPIO 2 | GPIO 48 |
| 23 et 24 | GPIO 47 | GPIO 35 |
| 25 et 26 | HVIN | GPIO 51 |
| 27 et 28 | HVIN | 5 V |
| 29 et 30 | HVIN | BAT |
