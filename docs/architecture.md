# 🏗️ Architecture & Code Structure / Architecture & Structure du Code

*English version below / Version française ci-dessous*

---

## 🇫🇷 Version Française

### 1. La Philosophie Modulaire "7-YAML"
Pour éviter un fichier monolithique de 5000 lignes impossible à déboguer, ce projet utilise les **packages ESPHome** de manière stricte. 
Le fichier principal `tab5-ha-hmi.yaml` ne fait qu'importer d'autres fichiers logiques :
* `tab5-hardware.yaml` : Gestion de l'écran, du tactile, et du processeur (I2C, SPI).
* `tab5-sensors.yaml` : Capteurs physiques (température interne, luminosité).
* `tab5-api-logic.yaml` : Le cerveau. Contient les fonctions C++ et la réception des événements de Home Assistant.
* `tab5-styles.yaml` : Définition globale des styles LVGL (pour éviter les répétitions et économiser la RAM).
* `tab5-images.yaml` / `tab5-fonts.yaml` : Gestion des ressources visuelles.

### 2. Le Paradigme "Push vs Polling" (Zéro Polling)
Une règle absolue de ce projet : **l'écran ne demande jamais rien à Home Assistant**.
Les boucles de polling (`update_interval: 10s`) saturent le réseau et le processeur de l'écran. 
Au lieu de cela, nous utilisons le concept **Event-Driven (Push)** :
* L'écran est un récepteur passif (0% d'utilisation CPU au repos).
* Home Assistant écoute les changements d'état (ex: la lumière du salon s'allume).
* Home Assistant "Pousse" (Push) la nouvelle information directement à l'écran via l'API native d'ESPHome (`api: services:`).

### 3. Protection au Démarrage (Boot Lambdas)
Pendant le redémarrage de Home Assistant, l'écran pourrait recevoir des valeurs nulles (NaN) et crasher.
Toutes les fonctions C++ (Lambdas) du projet vérifient d'abord l'intégrité de la connexion avant de mettre à jour l'interface LVGL (ex: `.has_state()`), garantissant un système à toute épreuve, même en cas de coupure Wi-Fi.

### 4. L'Envoi Massif de Données (Data Packing)
Pour les composants complexes comme la météo ou l'agenda, envoyer 15 variables une par une ralentit la pile TCP/IP.
* **L'Astuce :** HA compile toutes les prévisions dans une chaîne de texte séparée par des `;` (ex: `0;Soleil;25;12;...`).
* **Réception :** Le code C++ de l'ESP32 utilise un parseur de chaîne (`String Tokenizer`) pour éclater cette donnée et remplir un tableau de variables instantanément (complexité O(1)).

---

## 🇺🇸 English Version

### 1. The "7-YAML" Modular Philosophy
To avoid a monolithic 5000-line file that is impossible to debug, this project strictly uses **ESPHome packages**.
The main file `tab5-ha-hmi.yaml` only imports other logical files:
* `tab5-hardware.yaml`: Screen, touch, and processor management (I2C, SPI).
* `tab5-sensors.yaml`: Physical sensors (internal temperature, brightness).
* `tab5-api-logic.yaml`: The brain. Contains C++ functions and the reception of Home Assistant events.
* `tab5-styles.yaml`: Global definition of LVGL styles (to avoid repetition and save RAM).
* `tab5-images.yaml` / `tab5-fonts.yaml`: Management of visual resources.

### 2. The "Push vs Polling" Paradigm (Zero Polling)
An absolute rule of this project: **the screen never asks Home Assistant for anything**.
Polling loops (`update_interval: 10s`) saturate the network and the screen's processor.
Instead, we use the **Event-Driven (Push)** concept:
* The screen is a passive receiver (0% CPU usage when idle).
* Home Assistant listens for state changes (e.g., the living room light turns on).
* Home Assistant "Pushes" the new information directly to the screen via the native ESPHome API (`api: services:`).

### 3. Boot Protection (Boot Lambdas)
During a Home Assistant reboot, the screen could receive null values (NaN) and crash.
All C++ functions (Lambdas) in the project first check connection integrity before updating the LVGL interface (e.g., `.has_state()`), guaranteeing a foolproof system, even in the event of a Wi-Fi dropout.

### 4. Massive Data Sending (Data Packing)
For complex components like weather or calendar, sending 15 variables one by one slows down the TCP/IP stack.
* **The Trick:** HA compiles all forecasts into a text string separated by `;` (e.g., `0;Sun;25;12;...`).
* **Reception:** The ESP32's C++ code uses a `String Tokenizer` to split this data and fill an array of variables instantly (O(1) complexity).
