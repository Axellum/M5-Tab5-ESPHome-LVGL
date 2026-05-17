# ⚙️ Hardware & Wiring / Matériel & Câblage

*English version below / Version française ci-dessous*

---

## 🇫🇷 Version Française

### Le Coeur du Projet : M5Stack Tab5
Ce projet repose sur le [M5Stack Tab5](https://m5stack.com/), un écran tactile de 5 pouces alimenté par le processeur ESP32-P4 (ou S3 dans certaines variantes). 
L'ESP32-P4 est particulièrement adapté pour ce projet grâce à :
* **Sa puissance de calcul brute**, permettant de faire tourner LVGL nativement sans latence.
* **Sa PSRAM généreuse (16 MB)**, qui nous permet de stocker le buffer graphique (environ 1.8 MB) entièrement en mémoire pour un affichage "Zero-Lag" (sans effet de déchirement ou tearing).

### Gestion de l'Audio (DAC I2S)
Le Tab5 dispose d'une puce audio I2S pour restituer les alertes sonores ou de la musique depuis Home Assistant (via Media Player Pipeline).
* **Communication :** Le flux audio est décodé nativement par l'ESP32 sans avoir besoin de certificats SSL externes complexes, directement en passant par le réseau local vers le proxy Home Assistant.
* **Limitations Réseau :** Si l'interface LVGL effectue trop de requêtes simultanées en plus du lecteur I2S, le processeur peut manquer de sockets libres (`TCP Socket Starvation`). La configuration actuelle dans le code ESPHome ("Traffic Pacing") évite ce problème en limitant le flux de requêtes.

### Recommandations d'Alimentation
* Utilisez toujours le port USB-C intégré au Tab5 avec un chargeur de qualité (minimum 5V/2A) pour éviter les coupures de l'écran lors des pics de consommation (ex: Wi-Fi + Rétroéclairage à 100% + Audio).

---

## 🇺🇸 English Version

### The Core of the Project: M5Stack Tab5
This project relies on the [M5Stack Tab5](https://m5stack.com/), a 5-inch touch screen powered by the ESP32-P4 processor (or S3 in some variants).
The ESP32-P4 is particularly suited for this project because of:
* **Its raw computing power**, allowing LVGL to run natively without latency.
* **Its generous PSRAM (16 MB)**, which allows us to store the graphic buffer (around 1.8 MB) entirely in memory for a "Zero-Lag" display (no tearing).

### Audio Management (I2S DAC)
The Tab5 has an I2S audio chip to play sound alerts or music from Home Assistant (via Media Player Pipeline).
* **Communication:** The audio stream is natively decoded by the ESP32 without needing complex external SSL certificates, routing directly through the local network to the Home Assistant proxy.
* **Network Limitations:** If the LVGL interface makes too many simultaneous requests alongside the I2S player, the processor can run out of free sockets (`TCP Socket Starvation`). The current configuration in the ESPHome code ("Traffic Pacing") prevents this issue by throttling the request flow.

### Power Recommendations
* Always use the integrated USB-C port of the Tab5 with a high-quality charger (minimum 5V/2A) to prevent screen reboots during peak power consumption (e.g., Wi-Fi + 100% Backlight + Audio).
