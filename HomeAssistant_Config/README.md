# Configuration Home Assistant pour Tab5 HMI / Home Assistant Configuration for Tab5 HMI

*(English translation below)*

Ce dossier contient les fichiers de configuration nécessaires côté **Home Assistant** pour que votre M5Stack Tab5 puisse recevoir les données météorologiques, les alertes, le calendrier et gérer vos équipements.

## 🇫🇷 🛠️ Prérequis

1. **Intégration Météo-France :** 
   Ce projet a été pensé pour fonctionner avec les données ultra-précises de l'intégration officielle Météo-France. Installez-la depuis la page des intégrations de Home Assistant.
   Vous obtiendrez des entités comme `weather.votre_ville`, `sensor.votre_ville_next_rain` et `sensor.XX_weather_alert` (alertes départementales).
   
2. **Google Calendar :**
   Pour la fonction "Planning", vous aurez besoin de l'intégration Google Calendar (`calendar.votre_email_gmail_com`).

## 🇫🇷 📁 Contenu du dossier

* `automations_examples.yaml` : Contient l'immense automatisation "Push" qui synchronise dynamiquement l'écran sans le surcharger de requêtes. Elle met à jour la météo, la pluie dans l'heure, le calendrier et la climatisation.
* `scripts_examples.yaml` : Contient des scripts appelés *par* l'écran (ex: bouton de contrôle d'un volet roulant ou de la lumière).
* `template_sensors_examples.yaml` : **Très important !** Contient le "Template Sensor" qui génère la petite phrase météo affichée sur l'écran (ex: "Averses dans 10 mn"). À ajouter dans votre fichier `configuration.yaml` ou `template.yaml`.

## 🇫🇷 ⚙️ Comment utiliser ces fichiers ?

1. Copiez les blocs de code dont vous avez besoin dans vos propres fichiers `automations.yaml`, `scripts.yaml` et `template.yaml`.
2. **Rechercher et remplacer :** Assurez-vous de remplacer les valeurs génériques dans le code par vos propres entités Home Assistant :
   - `VOTRE_VILLE` : nom de votre ville issue de l'intégration Météo-France.
   - `VOTRE_DEPARTEMENT` : votre numéro de département (ex: `40`).
   - `VOTRE_CLIMATISATION` : l'entité de votre climatisation (`climate.votre_climatisation`).
   - `VOTRE_EMAIL_gmail_com` : votre entité de calendrier Google.
   - `VOTRE_VOLET` : vos propres entités pour les scripts.
3. Rechargez vos configurations YAML depuis Home Assistant ou redémarrez-le.

---

## 🇬🇧 🛠️ Prerequisites

1. **Météo-France Integration :** 
   This project is designed to work with the highly accurate data from the official Météo-France integration (for French users). Install it from the Home Assistant integrations page.
   You will get entities like `weather.your_city`, `sensor.your_city_next_rain` and `sensor.XX_weather_alert`. *(Note: If you are outside France, you will need to adapt the automation sensors to your own local weather integration).*
   
2. **Google Calendar :**
   For the "Planning" feature, you will need the Google Calendar integration (`calendar.your_email_gmail_com`).

## 🇬🇧 📁 Folder Contents

* `automations_examples.yaml` : Contains the large "Push" automation that dynamically synchronizes the screen without overloading it with requests. It updates weather, rain forecasts, calendar, and climate.
* `scripts_examples.yaml` : Contains scripts called *by* the screen (e.g., controlling a roller shutter or lights).
* `template_sensors_examples.yaml` : **Very important!** Contains the "Template Sensor" that generates the short weather sentence displayed on the screen (e.g., "Showers in 10 min"). Add this to your `configuration.yaml` or `template.yaml` file.

## 🇬🇧 ⚙️ How to use these files?

1. Copy the necessary code blocks into your own `automations.yaml`, `scripts.yaml`, and `template.yaml` files.
2. **Find and Replace:** Make sure to replace the generic values in the code with your own Home Assistant entities:
   - `VOTRE_VILLE` : your city name from the Météo-France integration.
   - `VOTRE_DEPARTEMENT` : your department number (e.g., `40`).
   - `VOTRE_CLIMATISATION` : your climate entity (`climate.your_climate`).
   - `VOTRE_EMAIL_gmail_com` : your Google calendar entity.
   - `VOTRE_VOLET` : your own entities for the scripts.
3. Reload your YAML configurations from Home Assistant or restart it.
