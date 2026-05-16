# M5Stack Tab5 ESPHome Native HMI

<div align="center">
  <img src="https://img.shields.io/badge/ESPHome-Native-blue.svg" alt="ESPHome Native" />
  <img src="https://img.shields.io/badge/LVGL-C++_Interface-green.svg" alt="LVGL" />
  <img src="https://img.shields.io/badge/Architecture-ESP32--P4%20%2B%20C6-orange.svg" alt="ESP32-P4+C6" />
  <img src="https://img.shields.io/badge/License-MIT-purple.svg" alt="MIT License" />
</div>

<br>

*English version below / Version française ci-dessous*

---

## 🇺🇸 English Version

### What is this project?
This repository contains a **highly optimized, native ESPHome configuration** for the [M5Stack Tab5](https://m5stack.com/), utilizing the powerful ESP32-P4 and ESP32-C6 processors. 

Instead of relying on a slow Android kiosk mode or a heavy Web dashboard, this project uses **Native C++ and LVGL (Light and Versatile Graphics Library)** compiled directly into the ESP32 firmware. This creates a true, industrial-grade Human-Machine Interface (HMI) for Home Assistant.

### Key Features
* 🚀 **Zero Latency:** UI rendering is processed natively on the ESP32-P4. No HTML/JS network overhead.
* 💾 **RAM Optimization:** We removed heavy PNG images and replaced them with **vectorized C++ MDI fonts** (`mdi_font_45`) for weather and UI icons, layered dynamically via C++ lambdas.
* 🛡️ **Autonomous Mode:** Even if your Wi-Fi drops or Home Assistant restarts, the LVGL interface keeps running smoothly without throwing web errors.
* 🔊 **Audio Integration:** Full support for the internal DAC / Speaker.

### Installation

1. **Hardware Requirements:**
   * M5Stack Tab5
   * High-quality USB-C data cable for the first flash

2. **Configuration:**
   * Clone or download this repository.
   * Rename `secrets.example.yaml` (if provided) to `secrets.yaml` and add your Wi-Fi/API credentials.
   * Open `tab5-ha-hmi.yaml` and edit the `substitutions:` block at the very top to match your own Home Assistant entities (lights, sensors, media players).

3. **Flashing:**
   * Use the ESPHome Dashboard to compile and flash the firmware via USB-C for the first time. Subsequent updates can be done Over-The-Air (OTA).

---

## 🇫🇷 Version Française

### Qu'est-ce que ce projet ?
Ce dépôt contient une **configuration ESPHome native et hautement optimisée** pour le [M5Stack Tab5](https://m5stack.com/), exploitant la puissance des processeurs ESP32-P4 et ESP32-C6.

Plutôt que d'utiliser un mode kiosque Android ou un tableau de bord Web lourd, ce projet utilise du **C++ natif et LVGL (Light and Versatile Graphics Library)** compilés directement dans le firmware de l'ESP32. Cela permet d'obtenir un véritable terminal HMI (Human-Machine Interface) de qualité industrielle pour Home Assistant.

### Fonctionnalités Clés
* 🚀 **Latence Zéro :** Le rendu de l'interface graphique est traité nativement sur l'ESP32-P4. Aucun temps de chargement réseau lié au HTML/JS.
* 💾 **Optimisation de la RAM :** Suppression des images PNG lourdes au profit de **polices vectorielles C++ MDI** (`mdi_font_45`) pour la météo et les icônes de l'interface, superposées dynamiquement via des lambdas C++.
* 🛡️ **Mode Autonome :** Même en cas de coupure Wi-Fi ou de redémarrage de Home Assistant, l'interface LVGL continue de fonctionner fluidement sans afficher d'erreur web.
* 🔊 **Intégration Audio :** Prise en charge complète du DAC interne et du haut-parleur.

### Installation

1. **Matériel Requis :**
   * Un écran M5Stack Tab5.
   * Un câble de données USB-C de bonne qualité pour le premier flashage.

2. **Configuration :**
   * Téléchargez ou clonez ce dépôt.
   * Créez un fichier `secrets.yaml` et ajoutez vos identifiants Wi-Fi et Home Assistant.
   * Ouvrez `tab5-ha-hmi.yaml` et modifiez le bloc `substitutions:` tout en haut pour lier vos propres entités Home Assistant (lumières, capteurs, etc.).

3. **Flashage :**
   * Utilisez le Dashboard ESPHome pour compiler et flasher le firmware via USB-C la première fois. Les mises à jour suivantes se feront en mode OTA (Over-The-Air) via Wi-Fi.

---
**Note:** This project is built by passionate makers for the Home Assistant community. Pull Requests and optimizations are welcome!
