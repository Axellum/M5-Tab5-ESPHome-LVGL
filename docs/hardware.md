# Hardware Reference

## English · [Français](#version-française)

---

## M5Stack Tab5 V2

The Tab5 is a 5-inch touch-screen panel from M5Stack. The V2 revision uses an **ESP32-P4** as the main application processor, with a separate **ESP32-C6** co-processor handling Wi-Fi and Bluetooth connectivity.

### ESP32-P4 (main processor)

| Spec | Value |
|------|-------|
| Architecture | RISC-V, dual-core, up to 400 MHz |
| Internal SRAM | 768 KB |
| External PSRAM | 16 MB (OCT-SPI) |
| Flash | 16 MB |
| Display interface | RGB565 parallel |
| Touch controller | I2C |

The PSRAM is critical for this project. LVGL requires a framebuffer sized to the display resolution — at 1024 × 600 px in RGB565, that's 1.2 MB just for the framebuffer. The ESP32-P4's internal SRAM alone would not be enough. With 16 MB of PSRAM, the framebuffer stays entirely in external memory, and LVGL can operate at 60 FPS without tearing.

### ESP32-C6 (co-processor)

Handles all radio communication: Wi-Fi 6 (802.11ax) and BLE 5. The main ESP32-P4 communicates with it over a UART bridge. From the ESPHome/LVGL code perspective, this is transparent — standard ESPHome Wi-Fi and BLE components work normally.

---

## Display

- **Size:** 5 inches
- **Resolution:** 1024 × 600 px
- **Interface:** 16-bit RGB parallel (connected to the ESP32-P4's LCD peripheral)
- **Touch:** Capacitive, multi-touch, connected via I2C

The display driver is initialized by ESPHome's built-in `ili9xxx` component (or equivalent ST7xxx depending on the batch). No custom driver code is needed.

---

## Audio — ES8388 DAC

The Tab5 integrates an **ES8388** audio codec chip. It handles both ADC (microphone input) and DAC (speaker output), though in this project only the DAC path is used — the microphone is captured directly by the ESP32-P4 via I2S.

### Connections

| Signal | GPIO |
|--------|------|
| I2C SDA (DAC control) | GPIO 31 |
| I2C SCL (DAC control) | GPIO 32 |
| I2S BCLK (audio clock) | GPIO 26 (shared) |
| I2S LRCLK (word select) | GPIO 29 (shared) |
| I2S DOUT (data to DAC) | GPIO 26 |
| Amplifier enable | GPIO (software-controlled switch) |

### Boot sequence issue

The ES8388 requires a specific initialization sequence over I2C to come out of reset and route audio correctly. If the amplifier enable line fires before the I2S clock is stable, a loud pop occurs through the speaker.

The ESPHome `on_boot` block addresses this by sequencing:
1. Set media player volume
2. Wait for HA API connection
3. Only then enable the amplifier switch

This order ensures the I2S bus is active before the amplifier opens the speaker path.

### I2C register initialization

The chip also requires writing to register `0x04` on boot to properly set the input/output routing. This is handled in `tab5-hardware.yaml` via `i2c.write_bytes` on boot.

---

## Microphone — I2S PDM

The onboard microphone is a digital PDM microphone connected to the ESP32-P4 via I2S.

| Signal | GPIO |
|--------|------|
| I2S DIN (data from mic) | GPIO 28 |
| I2S BCLK | GPIO 27 |
| I2S LRCLK | GPIO 29 |

Capture parameters: **16 kHz, 16-bit mono**. This matches the input format expected by the `micro_wake_word` component and by Home Assistant's voice pipeline.

---

## Power

The Tab5 is USB-C powered. Peak consumption (Wi-Fi active + 100% backlight + audio playing) can exceed 1.5 A at 5V. A charger rated for at least **5V / 2A** is required to avoid brownout resets.

Backlight brightness is software-controlled via PWM and can be dimmed to reduce power draw. The project includes an ambient light sensor reading used to adjust brightness automatically.

---

---

## Version Française

---

## M5Stack Tab5 V2

Le Tab5 est un panneau tactile de 5 pouces de M5Stack. La révision V2 utilise un **ESP32-P4** comme processeur applicatif principal, avec un co-processeur **ESP32-C6** séparé gérant la connectivité Wi-Fi et Bluetooth.

### ESP32-P4 (processeur principal)

| Spec | Valeur |
|------|--------|
| Architecture | RISC-V, dual-core, jusqu'à 400 MHz |
| SRAM interne | 768 KB |
| PSRAM externe | 16 MB (OCT-SPI) |
| Flash | 16 MB |
| Interface affichage | RGB565 parallèle |
| Contrôleur tactile | I2C |

La PSRAM est critique pour ce projet. LVGL nécessite un framebuffer dimensionné à la résolution de l'affichage — à 1024 × 600 px en RGB565, ça fait 1,2 MB rien que pour le framebuffer. La SRAM interne de l'ESP32-P4 seule ne suffirait pas. Avec 16 MB de PSRAM, le framebuffer reste entièrement en mémoire externe, et LVGL peut fonctionner à 60 FPS sans tearing.

### ESP32-C6 (co-processeur)

Gère toute la communication radio : Wi-Fi 6 (802.11ax) et BLE 5. Le ESP32-P4 principal communique avec lui via un pont UART. Du point de vue du code ESPHome/LVGL, c'est transparent — les composants Wi-Fi et BLE standards d'ESPHome fonctionnent normalement.

---

## Affichage

- **Taille :** 5 pouces
- **Résolution :** 1024 × 600 px
- **Interface :** RGB parallèle 16 bits (connecté au périphérique LCD de l'ESP32-P4)
- **Tactile :** Capacitif, multi-touch, connecté via I2C

---

## Audio — DAC ES8388

Le Tab5 intègre un codec audio **ES8388**. Il gère l'ADC (entrée microphone) et le DAC (sortie haut-parleur), bien que dans ce projet seul le chemin DAC soit utilisé — le microphone est capturé directement par l'ESP32-P4 via I2S.

### Connexions

| Signal | GPIO |
|--------|------|
| I2C SDA (contrôle DAC) | GPIO 31 |
| I2C SCL (contrôle DAC) | GPIO 32 |
| I2S BCLK (horloge audio) | GPIO 26 (partagé) |
| I2S LRCLK (word select) | GPIO 29 (partagé) |
| I2S DOUT (données vers DAC) | GPIO 26 |
| Activation ampli | GPIO (switch logiciel) |

### Problème de séquence au boot

L'ES8388 nécessite une séquence d'initialisation spécifique sur I2C pour sortir du reset et router correctement l'audio. Si la ligne d'activation de l'amplificateur passe avant que l'horloge I2S soit stable, un fort pop se produit dans le haut-parleur.

Le bloc `on_boot` d'ESPHome règle ça en séquençant :
1. Régler le volume du media player
2. Attendre la connexion API HA
3. Seulement alors activer le switch ampli

Cet ordre garantit que le bus I2S est actif avant que l'ampli ouvre le chemin vers le haut-parleur.

### Initialisation des registres I2C

Le chip nécessite aussi d'écrire dans le registre `0x04` au boot pour correctement configurer le routage entrée/sortie. C'est géré dans `tab5-hardware.yaml` via `i2c.write_bytes` au démarrage.

---

## Microphone — I2S PDM

Le microphone intégré est un microphone numérique PDM connecté à l'ESP32-P4 via I2S.

| Signal | GPIO |
|--------|------|
| I2S DIN (données du micro) | GPIO 28 |
| I2S BCLK | GPIO 27 |
| I2S LRCLK | GPIO 29 |

Paramètres de capture : **16 kHz, 16-bit mono**. Correspond au format d'entrée attendu par le composant `micro_wake_word` et par le pipeline vocal de Home Assistant.

---

## Alimentation

Le Tab5 est alimenté en USB-C. La consommation en pointe (Wi-Fi actif + rétroéclairage 100% + audio en lecture) peut dépasser 1,5 A à 5V. Un chargeur d'au moins **5V / 2A** est nécessaire pour éviter les resets par sous-tension.

La luminosité du rétroéclairage est contrôlée logiciellement via PWM et peut être réduite. Le projet inclut une lecture du capteur de luminosité ambiante pour ajuster la luminosité automatiquement.
