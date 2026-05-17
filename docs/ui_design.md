# 🎨 UI Design & LVGL / Conception d'Interface & LVGL

*English version below / Version française ci-dessous*

---

## 🇫🇷 Version Française

### 1. Pourquoi LVGL Ntif ?
Plutôt que d'utiliser un tableau de bord web classique (qui est lourd, lent à charger sur un ESP32 et dépendant du réseau), ce projet utilise **LVGL** de manière native. 
L'interface est compilée en C++ directement dans la mémoire de l'écran. 
* **Avantage :** Vitesse d'affichage instantanée, zéro latence tactile.
* **Résilience :** Si le serveur Home Assistant ou le routeur Wi-Fi plante, l'écran ne montre pas un logo "Erreur 404". L'interface reste affichée et fonctionnelle (en mode hors-ligne).

### 2. Adieu les images PNG, bonjour les Vectorielles
Les écrans d'ancienne génération (comme Nextion) utilisaient des dizaines d'images (`.png` ou `.jpg`) très lourdes pour afficher un simple logo.
* Ce projet Tab5 utilise des **polices vectorielles (`mdi_font_45`)** via la librairie **Material Design Icons**.
* Mettre une icône de "canapé" ou un soleil prend 1 bit de mémoire au lieu de 150 Kilooctets.
* **Anti-Aliasing (`bpp: 1`) :** Nous forçons le rendu vectoriel en 1-bit pour accélérer drastiquement le rafraîchissement par le processeur.

### 3. Gestion Dynamique des Couleurs et du "Dimming"
LVGL est très puissant avec les couleurs.
* **Colorisation dynamique :** Un thermostat ou un calendrier de Home Assistant peut envoyer une balise texte (ex: `#FF0000 texte#`) qui sera nativement lue et colorisée par LVGL sans aucune ligne de code supplémentaire.
* **Dimming (Grisage) :** Au lieu de cacher un objet inactif, nous appliquons un masque de recolorisation (`lv_obj_set_style_img_recolor_opa` en C++). Un bouton désactivé devient simplement "grisé" tout en conservant la symétrie de l'interface graphique.

### 4. Transparence et "Margins"
* Une règle absolue sur les micro-contrôleurs : **Ne jamais demander à l'ESP32 de calculer des arrondis (radius) sur un objet transparent**. Cela détruit le CPU.
* Pour les calques invisibles, nous retirons toujours les marges (`pad_all: 0`, `border_width: 0`) pour empêcher LVGL de faire des calculs de collision inutiles.

---

## 🇺🇸 English Version

### 1. Why Native LVGL?
Instead of using a classic web dashboard (which is heavy, slow to load on an ESP32, and network-dependent), this project uses **LVGL** natively.
The interface is compiled in C++ directly into the screen's memory.
* **Advantage:** Instant display speed, zero touch latency.
* **Resilience:** If the Home Assistant server or Wi-Fi router crashes, the screen does not show a "404 Error" logo. The interface remains displayed and functional (in offline mode).

### 2. Goodbye PNG images, Hello Vectors
Older generation screens (like Nextion) used dozens of heavy images (`.png` or `.jpg`) to display a simple logo.
* This Tab5 project uses **vector fonts (`mdi_font_45`)** via the **Material Design Icons** library.
* Putting a "sofa" icon or a sun takes 1 bit of memory instead of 150 Kilobytes.
* **Anti-Aliasing (`bpp: 1`):** We force vector rendering to 1-bit to drastically speed up the processor's refresh rate.

### 3. Dynamic Color Management and "Dimming"
LVGL is very powerful with colors.
* **Dynamic colorization:** A Home Assistant thermostat or calendar can send a text tag (e.g., `#FF0000 text#`) which will be natively read and colored by LVGL without any additional lines of code.
* **Dimming:** Instead of hiding an inactive object, we apply a recoloring mask (`lv_obj_set_style_img_recolor_opa` in C++). A disabled button simply becomes "greyed out" while keeping the GUI symmetric.

### 4. Transparency and "Margins"
* An absolute rule on micro-controllers: **Never ask the ESP32 to calculate rounded corners (radius) on a transparent object**. This destroys the CPU.
* For invisible layers, we always remove margins (`pad_all: 0`, `border_width: 0`) to prevent LVGL from doing unnecessary collision calculations.
