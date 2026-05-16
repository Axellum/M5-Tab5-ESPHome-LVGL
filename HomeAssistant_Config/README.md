# Configuration Home Assistant pour Tab5 HMI

Ce dossier contient les fichiers de configuration nécessaires côté **Home Assistant** pour que votre M5Stack Tab5 puisse recevoir les données météorologiques, les alertes, le calendrier et gérer vos équipements.

## 🛠️ Prérequis

1. **Intégration Météo-France :** 
   Ce projet a été pensé pour fonctionner avec les données ultra-précises de l'intégration officielle Météo-France. Installez-la depuis la page des intégrations de Home Assistant.
   Vous obtiendrez des entités comme `weather.votre_ville`, `sensor.votre_ville_next_rain` et `sensor.XX_weather_alert` (alertes départementales).
   
2. **Google Calendar :**
   Pour la fonction "Planning", vous aurez besoin de l'intégration Google Calendar (`calendar.votre_email_gmail_com`).

## 📁 Contenu du dossier

* `automations_examples.yaml` : Contient l'immense automatisation "Push" qui synchronise dynamiquement l'écran sans le surcharger de requêtes. Elle met à jour la météo, la pluie dans l'heure, le calendrier et la climatisation.
* `scripts_examples.yaml` : Contient des scripts appelés *par* l'écran (ex: bouton de contrôle d'un volet roulant ou de la lumière).
* `template_sensors_examples.yaml` : **Très important !** Contient le "Template Sensor" qui génère la petite phrase météo affichée sur l'écran (ex: "Averses dans 10 mn"). À ajouter dans votre fichier `configuration.yaml` ou `template.yaml`.

## ⚙️ Comment utiliser ces fichiers ?

1. Copiez les blocs de code dont vous avez besoin dans vos propres fichiers `automations.yaml`, `scripts.yaml` et `template.yaml`.
2. **Rechercher et remplacer :** Assurez-vous de remplacer les valeurs génériques dans le code par vos propres entités Home Assistant :
   - `VOTRE_VILLE` : nom de votre ville issue de l'intégration Météo-France.
   - `VOTRE_DEPARTEMENT` : votre numéro de département (ex: `40`).
   - `VOTRE_CLIMATISATION` : l'entité de votre climatisation (`climate.votre_climatisation`).
   - `VOTRE_EMAIL_gmail_com` : votre entité de calendrier Google.
   - `VOTRE_VOLET` : vos propres entités pour les scripts.
3. Rechargez vos configurations YAML depuis Home Assistant ou redémarrez-le.
