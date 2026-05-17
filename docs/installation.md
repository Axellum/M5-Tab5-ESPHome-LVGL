# 💻 Installation & Météo-France Setup / Installation & Configuration Météo-France

*English version below / Version française ci-dessous*

---

## 🇫🇷 Version Française

### 1. Prérequis
* Un serveur **Home Assistant** fonctionnel.
* L'add-on **ESPHome** installé sur votre Home Assistant (ou en local).
* L'intégration **Météo-France** installée et configurée dans Home Assistant (Requis pour le tableau de bord principal).

### 2. Configuration des Entités Home Assistant
Dans le fichier principal `tab5-ha-hmi.yaml`, tout en haut, vous trouverez un bloc `substitutions:`. Ce bloc est crucial car il fait l'abstraction entre le code de l'écran et **vos propres entités** Home Assistant.
Modifiez ces valeurs pour correspondre à vos noms d'entités (ex: vos lumières, votre media player, etc.).

### 3. Les Fichiers Secrets
Ne mettez **jamais** vos mots de passe en clair dans le code. 
Créez un fichier `secrets.yaml` à la racine de votre dossier ESPHome et ajoutez-y :
```yaml
wifi_ssid: "VOTRE_WIFI"
wifi_password: "VOTRE_MOT_DE_PASSE"
api_password: "MOT_DE_PASSE_API_ESPHOME"
```

### 4. Intégration Météo-France (Spécifique Tab5)
Le Tab5 est conçu pour afficher les prévisions de pluie dans l'heure avec une grande précision. Pour cela, vous devez configurer Home Assistant pour envoyer ces données spécifiques à l'écran.
1. Allez dans le dossier `HomeAssistant_Config/` de ce dépôt.
2. Vous y trouverez des scripts et des automatisations. Copiez-les dans vos propres fichiers `automations.yaml` et `scripts.yaml` de Home Assistant.
3. **Logique Météo :** Home Assistant interroge l'API Météo-France (`v1/vision/rain` et `v1/forecast`). Ensuite, l'automatisation "Pousse" (Push) massivement ces données vers l'écran Tab5 via un appel de service natif ESPHome (ex: `esphome.nom_device_tab5_maj_previsions_heures`). 
4. L'astuce réside dans la sérialisation : Home Assistant envoie une longue chaîne de caractères séparée par des `;` et c'est l'écran (en C++) qui découpe cette chaîne pour l'afficher instantanément.

### 5. Flashage
Connectez votre Tab5 en USB-C à votre PC/Serveur et compilez le projet via le tableau de bord ESPHome. Les mises à jour suivantes pourront se faire en OTA (Wi-Fi).

---

## 🇺🇸 English Version

### 1. Prerequisites
* A working **Home Assistant** server.
* The **ESPHome** add-on installed on your Home Assistant (or locally).
* The **Météo-France** integration installed and configured in Home Assistant (Required for the main dashboard).

### 2. Home Assistant Entities Configuration
In the main `tab5-ha-hmi.yaml` file, right at the top, you will find a `substitutions:` block. This block is critical because it abstracts the screen's code from **your own** Home Assistant entities.
Change these values to match your entity names (e.g., your lights, your media player, etc.).

### 3. Secrets File
**Never** put your passwords in plain text in the code.
Create a `secrets.yaml` file at the root of your ESPHome folder and add:
```yaml
wifi_ssid: "YOUR_WIFI"
wifi_password: "YOUR_PASSWORD"
api_password: "ESPHOME_API_PASSWORD"
```

### 4. Météo-France Integration (Tab5 Specific)
The Tab5 is designed to display rain forecasts within the hour with high precision. To do this, you must configure Home Assistant to send this specific data to the screen.
1. Go to the `HomeAssistant_Config/` folder of this repository.
2. You will find scripts and automations there. Copy them into your own `automations.yaml` and `scripts.yaml` Home Assistant files.
3. **Weather Logic:** Home Assistant queries the Météo-France API (`v1/vision/rain` and `v1/forecast`). Then, the automation massively "Pushes" this data to the Tab5 screen via a native ESPHome service call (e.g., `esphome.device_name_tab5_update_hourly_forecast`).
4. The trick is in the serialization: Home Assistant sends a long string separated by `;` and it's the screen (in C++) that splits this string to display it instantly.

### 5. Flashing
Connect your Tab5 via USB-C to your PC/Server and compile the project via the ESPHome dashboard. Subsequent updates can be done via OTA (Wi-Fi).
