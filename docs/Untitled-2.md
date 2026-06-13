RAPPORT D'AUDIT TECHNIQUE : ÉCRAN DOMOTIQUE M5STACK TAB5 V2 (ESPHome & LVGL)
Ce document présente un audit exhaustif et sans concession du projet d'écran domotique basé sur le microcontrôleur ESP32-P4 (M5Stack Tab5 V2) fonctionnant sous ESPHome et interfacé avec Home Assistant.

1. Architecture, Infrastructure et Organisation
A. Évaluation de l'infrastructure & Communication (ESP32 ↔ HA)
Forces :
L'intégration via l'API Native ESPHome (plutôt que MQTT) est le choix optimal. Elle offre une communication bidirectionnelle à faible latence et tire parti du protocole binaire natif d'ESPHome (ProtoBuf), beaucoup plus léger pour le processeur que le parsing JSON sur MQTT.
L'utilisation de services d'API bulk (tab5_maj_previsions_heures_bulk et tab5_maj_previsions_jours_bulk) est une excellente pratique industrielle. Envoyer les prévisions à 15 jours sous forme d'une chaîne sérialisée unique découpée en C++ évite de surcharger la pile réseau lwIP de l'ESP32 avec 45 à 60 appels d'API individuels, prévenant ainsi les déconnexions intempestives.
Faiblesses & Risques :
API Connexion Synchrone : Le script on_boot dans tab5-ha-hmi.yaml contient un bloc wait_until: api.connected: suivi de delay: 2s puis d'un envoi d'événement HA. Si Home Assistant est temporairement indisponible au démarrage de la tablette, le démarrage complet du micrologiciel et l'initialisation de certaines variables globales peuvent être bloqués ou retardés, gelant l'affichage sur les icônes par défaut.
Watchdog de l'API : Le paramètre reboot_timeout: 0s est défini sous api:. Cela empêche l'ESP32 de redémarrer automatiquement si la connexion à Home Assistant est perdue à long terme. Si cette désactivation a été choisie pour éviter les boucles de redémarrage la nuit (lorsque HA est mis à jour), elle présente le risque de laisser la tablette dans un état déconnecté permanent si le WiFi ou l'API ne se reconnectent pas correctement.
B. Organisation des fichiers
Forces :
Le découpage en packages YAML (7-YAML Architecture) dans le dossier 
Tab5
 (tab5-hardware.yaml, tab5-globals.yaml, tab5-sensors.yaml, etc.) est propre et respecte les bonnes pratiques de modularité.
Faiblesses critiques (Anti-patterns d'architecture) :
Viol du Principe de Séparation des Préoccupations (SoC) :
Le fichier 
tab5-sensors.yaml
 contient une quantité massive de logique de présentation (UI). Les triggers on_value des capteurs modifient directement les widgets LVGL (lv_label_set_text, lv_obj_set_style_text_color).
Impact : Si vous renommez ou modifiez un widget graphique dans tab5-lvgl.yaml, vous devez modifier la logique des capteurs dans tab5-sensors.yaml. C'est un couplage fort qui empêche la maintenance saine.
Formatage & Lignes Parasites :
Le fichier 
tab5-sensors.yaml
 contient des centaines de lignes vides inutiles (souvent deux ou trois lignes blanches consécutives entre chaque ligne de code). Cela rend le fichier inutilement long (1677 lignes) et pénible à lire.
Mélange de logique dans les globales :
tab5-globals.yaml
 contient un bloc interval: 5s qui manipule l'UI (lbl_planning_text). Cette logique de rotation d'affichage devrait être dans un script d'UI ou gérée directement par l'IHM.
C. Sécurité & Résilience
watchdog hardware : ESPHome configure par défaut un watchdog matériel (Task Watchdog Timer - TWDT) à 5 secondes. Si une lambda C++ exécute du code bloquant ou une boucle infinie (par exemple lors du parsing bulk), l'ESP32 redémarrera immédiatement.
Mémoire PSRAM vs SRAM :
L'ESP32-P4 dispose de 32 Mo de PSRAM externe (configurée via psram: mode: hex à 200MHz) et ~500 Ko de SRAM interne.
Par défaut, ESPHome place la structure de l'arbre d'objets LVGL et les buffers d'affichage en SRAM. Bien que l'option buffer_size: 100% force le buffer graphique en PSRAM (SPIRAM), l'allocation dynamique de texte dans les lambdas (char buf[32], std::string, etc.) s'effectue sur le tas (Heap) interne SRAM. Il est indispensable de surveiller la fragmentation de la SRAM interne.
Gestion des déconnexions Wi-Fi / API :
Si HA est déconnecté, les capteurs HA importés (platform: homeassistant) conservent leur dernier état ou passent à NaN (Not a Number). Le code actuel teste isnan(x) avant d'exécuter sprintf ou de mettre à jour l'UI. C'est robuste, mais le fait d'écrire -- °C sur l'écran en cas de perte de connexion devrait être harmonisé via une fonction centrale.
2. Qualité du Code (YAML, C++, LVGL)
A. Revue de code YAML
Incohérence Matérielle Critique (Haut-parleur) :
Dans 
tab5-hardware.yaml
, le composant speaker: est configuré avec channel: mono.
Rappel de la règle matérielle : La documentation de l'utilisateur stipule explicitement que les DACs configurés en Mono peuvent empêcher la création du composant ou provoquer l'extinction du haut-parleur. Il est recommandé de forcer channel: stereo.
Substitutions et Héritage :
Les entités Home Assistant sont correctement injectées via des substitutions (${entity_...}). C'est parfait pour la portabilité.
B. C++ et Lambdas (Redondance & Lisibilité)
Le projet souffre d'une duplication massive de code (anti-pattern Don't Repeat Yourself - DRY) et d'un manque de lisibilité dans les lambdas :

Gradient de couleur de température : La logique mathématique complexe de gradient à 5 paliers (bleu, vert, orange, rouge) est dupliquée à 5 endroits différents (au moins 3 fois dans tab5-sensors.yaml et 2 fois dans tab5-api-logic.yaml), parfois condensée sur une ligne unique de plus de 200 caractères (ex: tab5-sensors.yaml L.504) :
cpp

if (t <= -12) c_int = 0xFF0000; else if (t <= 0) { float r = floor((t + 12)/2.0)*2.0/12.0; c_int = (255<<16)|(0<<8)|(int)(255*r); } ...
Gradient d'humidité (Plantes) : La logique de couleur pour l'humidité est répétée à l'identique pour les capteurs moisture_1 à moisture_5 dans tab5-sensors.yaml (lignes 735-792, 846-903, 957-1014, 1068-1125, 1179-1236). C'est plus de 300 lignes de code dupliqué !
Fonctions locales redondantes : Les lambdas du calendrier et de la météo bulk recréent localement des lambdas apply_weather_icon et get_c (L.18, L.25, L.93, L.96, L.766, L.770 dans tab5-api-logic.yaml).
C. Interface Graphique (LVGL)
Arbre des objets : L'implémentation respecte le standard d'optimisation en utilisant radius: 0 et bg_opa: 0 sur les objets transparents pour désactiver le moteur de rendu 2D logiciel (CPU-saving).
Ghost clicks & Veille : Le standard d'extinction de l'écran est parfaitement implémenté :
Rétroéclairage éteint (light.turn_off) ➔ Pause de LVGL (lvgl.pause) pour bloquer la propagation des clics tactiles.
Dalle touchée (on_release du st7123) ➔ Rallumage rétroéclairage + Reprise de LVGL (lvgl.resume).
Rétrocompatibilité Gestures : Dans 
tab5-lvgl.yaml
, le code utilise bien les constantes modernes LV_DIR_TOP et LV_DIR_BOTTOM au lieu des anciennes macros dépréciées LV_DIR_UP/LV_DIR_DOWN.
3. Optimisation, Factorisation et Formatage
A. Séparation en 3 Couches Logicielles
Pour professionnaliser le projet, nous devons implémenter un découpage strict :

Logiciel / Matériel (Hardware & Drivers) : 
tab5-hardware.yaml
. Initialise les GPIOs, le bus I2C, l'écran MIPI-DSI, le tactile ST7123, le DAC audio ES8388.
Logique Métier & Communication (Sensors & APIs) : 
tab5-sensors.yaml
 et 
tab5-api-logic.yaml
. Reçoivent les données des capteurs locaux et de HA. Ils ne doivent pas manipuler directement l'UI. Ils appellent des fonctions de mise à jour ou mettent à jour des globales.
Présentation / IHM (UI & LVGL) : 
tab5-lvgl.yaml
 et les composants de ui_components/. Ils décrivent la structure géométrique et stylistique des écrans et s'abonnent aux variables ou fonctions d'IHM.
B. Proposition de factorisation extrême (C++)
Toute la logique métier de coloration et de formatage doit être centralisée dans 
tab5_custom.cpp
.

1. Factorisation de la température
Remplacer les calculs locaux par un appel à get_temperature_color(x) déjà déclarée dans le CPP.

YAML Avant :
yaml

- lambda: |-
    if(isnan(x)) { ... }
    else {
      ...
      float t = x; uint32_t c_int;
      if (t <= -12) c_int = 0xFF0000; ... // Formule dupliquée géante
      lv_obj_set_style_text_color(id(current_serre), lv_color_hex(c_int), LV_PART_MAIN);
    }
YAML Après :
yaml

- lambda: |-
    if(isnan(x)) {
      lv_label_set_text(id(current_serre), "-- °C");
      lv_obj_set_style_text_color(id(current_serre), lv_color_hex(0xA3A8B5), LV_PART_MAIN);
    } else {
      char buf[32];
      sprintf(buf, "%.1f °C", x);
      lv_label_set_text(id(current_serre), buf);
      lv_obj_set_style_text_color(id(current_serre), lv_color_hex(get_temperature_color(x)), LV_PART_MAIN);
    }
2. Factorisation de l'humidité des plantes (Gain : >250 lignes YAML)
Créer une fonction centrale d'UI pour les plantes dans 
tab5_custom.cpp
 :

cpp

void update_plant_moisture_ui(float x, lv_obj_t* icon_obj, lv_obj_t* val_obj, char* default_icon) {
    if (isnan(x)) {
        lv_label_set_text(val_obj, "--%");
        lv_obj_set_style_text_color(icon_obj, lv_color_hex(0x404552), LV_PART_MAIN);
        lv_obj_set_style_text_color(val_obj, lv_color_hex(0x404552), LV_PART_MAIN);
    } else {
        char buf[16];
        sprintf(buf, "%.0f%%", x);
        lv_label_set_text(val_obj, buf);
        
        uint32_t color = get_humidity_color(x);
        lv_obj_set_style_text_color(icon_obj, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_text_color(val_obj, lv_color_hex(color), LV_PART_MAIN);
    }
}
Dans 
tab5-sensors.yaml
, l'appel pour chaque plante se simplifie ainsi :

Plante 4 (Avant) : 50 lignes de code dupliqué.
Plante 4 (Après) :
yaml

  id: moisture_4
  entity_id: ${entity_plante_4}
  on_value:
    - lambda: 'update_plant_moisture_ui(x, id(icon_plant_4), id(val_plant_4));'
4. Plan d'Action Priorisé (Étape par Étape)
Mermaid diagram
Étape 1 : Corrections critiques matérielles & Nettoyage (Priorité Haute)
Modifier la configuration audio du composant speaker: dans 
tab5-hardware.yaml
 de channel: mono à channel: stereo pour respecter les contraintes du DAC ES8388.
Nettoyer les sauts de lignes doubles et triples parasites dans 
tab5-sensors.yaml
 pour restaurer un fichier lisible et aux normes industrielles.
Étape 2 : Déportation C++ & Factorisation des capteurs (Priorité Haute)
Ajouter la déclaration et définition de update_plant_moisture_ui(...) dans 
tab5_custom.h
 et 
tab5_custom.cpp
.
Remplacer les 5 immenses blocs redondants de calcul de gradient d'humidité par l'appel unitaire de 1 ligne : update_plant_moisture_ui(x, id(icon_plant_X), id(val_plant_X)); dans tab5-sensors.yaml.
Remplacer les gradients de température manuels par l'appel à get_temperature_color(x) pour les capteurs temp_serre, temp_salon et temp_chambre.
Étape 3 : Nettoyage de la logique d'API & bulk updates (Priorité Moyenne)
Nettoyer les services de bulk updates dans 
tab5-api-logic.yaml
 :
Supprimer les fonctions lambda C++ locales redondantes get_c définies à l'intérieur des triggers.
Remplacer les appels locaux à get_c(temp) par la fonction globale get_temperature_color(temp).
Étape 4 : Validation, Profiling & Benchmarking (Priorité Basse)
Nettoyer la logique de démarrage en mesurant le temps de boot. Si possible, retirer ou réduire le delay: 3s de on_boot dans 
tab5-ha-hmi.yaml
 pour éviter de bloquer l'event loop d'ESPHome au boot.
Exécuter un nettoyage du cache local avant compilation pour valider l'absence d'effets de bord :
powershell

esphome clean tab5-ha-hmi.yaml

gemini 3.5 hight