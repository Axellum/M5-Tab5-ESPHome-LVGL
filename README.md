# M5Stack Tab5 - "Sans les mains" (AI-Generated Dashboard)

<div align="center">
  <img src="https://img.shields.io/badge/ESPHome-Native-blue.svg" alt="ESPHome Native" />
  <img src="https://img.shields.io/badge/LVGL-C++_Interface-green.svg" alt="LVGL" />
  <img src="https://img.shields.io/badge/100%25-AI_Generated-orange.svg" alt="AI Generated" />
  <img src="https://img.shields.io/badge/License-MIT-purple.svg" alt="MIT License" />
</div>

<br>

*English version below / Version française ci-dessous*

---

## 🇺🇸 English Version

### What is this project?
This repository is the result of a personal experiment: **can a complete "noob" create a fully functional, high-performance smart home dashboard using only Artificial Intelligence?** 

The answer is yes! After hitting the limits of my old Nextion screen, I decided to try the [M5Stack Tab5](https://m5stack.com/) and let an AI (Antigravity / Gemini) handle all the coding. I didn't write a single line of code myself—just a lot of messy prompts! 

The result is a highly responsive, native ESPHome interface using **C++ and LVGL**, totally independent of heavy web browsers. I'm sharing it here so others can use it, learn from it, or just copy the parts they need for their own Home Assistant setup.

### Project Goals
* **Clock & Date:** A clean, easy-to-read main screen.
* **Weather & Rain Forecast:** Essential for me since I walk to work (relies on Météo-France data).
* **Schedule Integration:** Visual indicators for work days vs. rest days.
* **Smart Home Controls:** Simple buttons for shutters, AC, PC, and TV.
* **Sensors:** Live data from my phone, plants, etc.

### Key Features
* 🚀 **Zero Latency:** Everything runs natively on the ESP32-P4 processor. No slow web dashboards.
* 💾 **Optimized:** Uses vector fonts (`mdi_font_45`) instead of heavy images to save RAM.
* 🛡️ **Autonomous:** If Wi-Fi drops, the interface doesn't crash or show a browser error.
* 🤖 **100% AI Generated:** Proof that you don't need to be a senior developer to build cool stuff for Home Assistant.

### How to use it
1. **Clone/Download** this repository.
2. **Configuration:** Open `tab5-ha-hmi.yaml` and look at the `substitutions:` block at the top. Replace the generic names with your own Home Assistant entities.
3. **Home Assistant Side:** Check the `HomeAssistant_Config/` folder. It contains the automations and scripts you need to add to your Home Assistant so it can send data to the screen.
4. **Flash:** Use ESPHome to compile and flash via USB-C for the first time.

---

## 🇫🇷 Version Française

### L'histoire de ce projet
Ce dépôt est le résultat d'une expérience personnelle : **un "noob" en code peut-il créer une interface domotique complète et ultra-réactive en utilisant uniquement l'Intelligence Artificielle ?**

La réponse est oui ! Ayant atteint les limites de mon vieil écran Nextion, j'ai décidé de passer sur le [M5Stack Tab5](https://m5stack.com/) et de laisser une IA (Antigravity / Gemini) s'occuper de toute la programmation. Je n'ai pas tapé une seule ligne de code, juste des prompts souvent approximatifs avec une orthographe désastreuse ! ^^

Le résultat est une interface ESPHome native en **C++ et LVGL**, ultra fluide et sans les lenteurs d'un navigateur web. Je le partage ici pour la communauté : servez-vous, modifiez-le, ou copiez juste les morceaux qui vous intéressent pour votre propre Home Assistant.

### Objectifs de l'écran
* **Horloge et date :** Simple et lisible.
* **Prévisions météo et averses :** Indispensable car je vais bosser à pied (utilise l'intégration Météo-France).
* **Planning :** Illustration colorimétrique de mes jours de repos et des jours où je fais l'ouverture au travail.
* **Commandes diverses :** Volets, clim, ordi, TV.
* **Capteurs :** Humidité de mes plantes, état du téléphone, etc.

### Pourquoi cette approche ?
* 🚀 **Réactivité :** Le rendu est calculé directement par le processeur ESP32-P4. Fini les temps de chargement des pages web.
* 💾 **Optimisation :** L'IA a remplacé les lourdes images par des polices vectorielles (`mdi_font_45`) pour économiser la RAM.
* 🛡️ **Autonome :** Si le Wi-Fi coupe, l'écran ne plante pas sur une page d'erreur internet.
* 🤖 **100% IA :** La preuve qu'on n'a plus besoin d'être développeur pour créer des interfaces sympas sur Home Assistant.

### Comment l'utiliser ?
1. **Téléchargez** ce dépôt.
2. **Configuration :** Ouvrez `tab5-ha-hmi.yaml` et modifiez le bloc `substitutions:` tout en haut avec vos propres entités Home Assistant.
3. **Côté Home Assistant :** Regardez dans le dossier `HomeAssistant_Config/`. Il contient les automatisations et scripts à ajouter à votre Home Assistant pour qu'il puisse discuter avec l'écran.
4. **Flashage :** Utilisez ESPHome pour compiler et flasher l'écran via USB-C la première fois.

---
*Projet né sur le forum [Home Assistant Communauté Francophone (HACF)](https://forum.hacf.fr/). N'hésitez pas à proposer des améliorations !*
