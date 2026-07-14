# UI Design & LVGL

## English · [Français](#version-française)

---

## Why native LVGL

The alternative to a native LVGL interface would be a web dashboard running in a browser embedded in the firmware (ESPHome's `esp32_s3_box` style, or a custom web server). That approach has real costs on a microcontroller:

- The WebSocket or HTTP stack competes with the audio stream for sockets
- The browser's layout engine re-renders on each update, triggering full repaints
- If the network is unreliable, the screen shows a loading spinner or a blank page

LVGL compiles into the firmware. The screen layout is computed once at boot. Updates are surgical: when a temperature value changes, only the label for that value is invalidated and redrawn. The rest of the screen is untouched. On a 400 MHz RISC-V core with 16 MB of PSRAM for the framebuffer, this runs at 60 FPS under normal workload.

An added side effect: if the Wi-Fi drops or Home Assistant restarts, the last known state stays on screen exactly as it was.

---

## Icons: vector fonts instead of images

Older touchscreen panels (Nextion, TFT_eSPI setups) typically use bitmap image files for icons. Each icon is a `.png` or `.jpg` embedded in flash, decoded at runtime.

This project uses **Material Design Icons** rendered as a TTF font (`materialdesignicons-webfont.ttf`). LVGL's font engine renders glyphs from the font file at the size specified in the style.

The difference in practice:
- A bitmap sun icon at 45 px might weigh 8 KB as a PNG, decoded to 8 KB in RAM at runtime
- The same icon as an MDI glyph at 45 px costs ~200 bytes in the font file, rendered on-the-fly by the font engine
- Scaling is free: the same glyph can render at 24 px, 45 px, or 72 px without separate assets

A second icon font (`IconeMeteo.ttf`) covers weather-specific symbols not present in the MDI set.

**Anti-aliasing note:** fonts are configured at `bpp: 1` (1 bit per pixel) rather than the default 4 bpp. This halves the font's RAM footprint and significantly reduces the CPU cost of rendering on each frame. At 45 px on a 1280 × 720 display, the visual difference between 1 bpp and 4 bpp is negligible at normal viewing distance.

---

## Global style system

LVGL allows two approaches for styling widgets:

1. **Inline styles** — set properties directly on each object: `lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN)`
2. **Style objects** — create a named `lv_style_t`, set properties on it once, and attach it to multiple objects

This project uses approach 2 exclusively, via `tab5-styles.yaml`. All style objects are declared globally and attached by name in `tab5-lvgl.yaml`.

Why this matters on an ESP32-P4 with 768 KB of internal SRAM:
- Each inline style property creates a local style record in the widget's internal data structure
- With 80+ widgets, that's 80+ separate style allocations, fragmented across PSRAM
- A shared style object, attached to 10 similar widgets, uses 1 allocation shared by all 10

In practice, moving from inline to global styles freed roughly 40 KB of PSRAM in this project.

---

## Dynamic color

LVGL supports inline color tags in label text strings. A label can receive a string like:

```
"#FF6B35 27°C#  Salon"
```

LVGL parses the hex color tag and renders "27°C" in orange, then "Salon" in the label's default color. No additional code required.

This is used for the calendar screen (event colors matching Google Calendar's event colors) and for temperature displays (blue below 18°C, green 18–24°C, orange above 24°C). The color logic runs on the Home Assistant side, embedded in the Jinja template that builds the payload string. The device just displays what it receives.

---

## Dimming inactive elements

When a button or control is unavailable (e.g., the climate control screen when the AC is off), the common approach is to hide the element with `lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)`. This works but leaves an empty hole in the layout.

This project uses **recolor opacity** instead:

```cpp
lv_obj_set_style_img_recolor(obj, lv_color_hex(0x808080), LV_PART_MAIN);
lv_obj_set_style_img_recolor_opa(obj, 180, LV_PART_MAIN);
```

This overlays a grey tint on the element without hiding it. The layout stays symmetric, and the visual feedback clearly communicates "inactive" without disrupting the screen geometry.

---

## CPU constraints to keep in mind

A few LVGL patterns that look innocent but are expensive on a microcontroller:

**Rounded corners on transparent objects.** If an object has `bg_opa: 0` (transparent background) and `radius > 0`, LVGL still computes the clipping mask for the rounded corners on every render pass. On a 400 MHz CPU rendering at 60 FPS, that's 24000 unnecessary computations per second per widget. Fix: set `radius: 0` on transparent containers, or remove `bg_opa: 0` if the object has no visual background.

**Unused padding.** Every non-zero padding value causes LVGL to include the widget in collision detection passes. Containers that are purely structural (used for positioning children, not for display) should have `pad_all: 0` and `border_width: 0`.

**Frequent full-screen invalidations.** Calling `lv_obj_invalidate(lv_scr_act())` forces a full repaint on the next tick. The correct pattern is to call `lv_obj_invalidate(specific_widget)` so only the changed area is redrawn.

---

---

## Version Française

---

## Pourquoi LVGL natif

L'alternative à une interface LVGL native serait un dashboard web tournant dans un navigateur embarqué dans le firmware. Cette approche a des coûts réels sur un microcontrôleur :

- La pile WebSocket ou HTTP entre en compétition avec le flux audio pour les sockets
- Le moteur de layout du navigateur re-rend à chaque mise à jour, déclenchant des repeintures complètes
- Si le réseau est instable, l'écran affiche un spinner de chargement ou une page blanche

LVGL est compilé dans le firmware. La mise en page de l'écran est calculée une fois au boot. Les mises à jour sont chirurgicales : quand une valeur de température change, seul le label pour cette valeur est invalidé et redessiné. Le reste de l'écran est inchangé. Sur un core RISC-V à 400 MHz avec 16 MB de PSRAM pour le framebuffer, ça tourne à 60 FPS sous charge normale.

Un effet de bord appréciable : si le Wi-Fi tombe ou que Home Assistant redémarre, le dernier état connu reste affiché exactement comme il était.

---

## Icônes : polices vectorielles au lieu d'images

Les anciens panneaux tactiles (Nextion, setups TFT_eSPI) utilisent typiquement des fichiers d'images bitmap pour les icônes. Chaque icône est un `.png` ou `.jpg` embarqué en flash, décodé à l'exécution.

Ce projet utilise **Material Design Icons** rendu comme police TTF (`materialdesignicons-webfont.ttf`). Le moteur de polices de LVGL rend les glyphes depuis le fichier de police à la taille spécifiée dans le style.

La différence en pratique :
- Une icône bitmap soleil à 45 px pèse ~8 KB en PNG, décodée en 8 KB en RAM à l'exécution
- Le même icône comme glyphe MDI à 45 px coûte ~200 octets dans le fichier de police, rendu à la volée par le moteur de polices
- Le scaling est gratuit : le même glyphe peut se rendre à 24 px, 45 px ou 72 px sans assets séparés

Une seconde police d'icônes (`IconeMeteo.ttf`) couvre les symboles météo spécifiques absents du set MDI.

**Note anti-aliasing :** les polices sont configurées à `bpp: 1` (1 bit par pixel) plutôt que les 4 bpp par défaut. Ça divise par deux l'empreinte RAM de la police et réduit significativement le coût CPU du rendu à chaque frame. À 45 px sur un affichage 1280 × 720, la différence visuelle entre 1 bpp et 4 bpp est négligeable à la distance de visionnage normale.

---

## Système de styles global

LVGL permet deux approches pour styliser les widgets :

1. **Styles inline** — définir les propriétés directement sur chaque objet
2. **Objets style** — créer un `lv_style_t` nommé, y définir les propriétés une fois, et l'attacher à plusieurs objets

Ce projet utilise exclusivement l'approche 2, via `tab5-styles.yaml`. Tous les objets style sont déclarés globalement et attachés par nom dans `tab5-lvgl.yaml`.

Pourquoi ça compte sur un ESP32-P4 avec 768 KB de SRAM interne :
- Chaque propriété de style inline crée un enregistrement de style local dans la structure de données interne du widget
- Avec 80+ widgets, c'est 80+ allocations de style séparées, fragmentées en PSRAM
- Un objet style partagé, attaché à 10 widgets similaires, utilise 1 allocation partagée par les 10

En pratique, le passage des styles inline aux styles globaux a libéré environ 40 KB de PSRAM dans ce projet.

---

## Couleur dynamique

LVGL supporte des tags de couleur inline dans les chaînes de texte des labels :

```
"#FF6B35 27°C#  Salon"
```

LVGL parse le tag de couleur hex et rend "27°C" en orange, puis "Salon" dans la couleur par défaut du label. Aucun code supplémentaire requis.

C'est utilisé pour l'écran calendrier (couleurs d'événements correspondant aux couleurs Google Calendar) et pour les affichages de température (bleu sous 18°C, vert 18–24°C, orange au-dessus). La logique de couleur tourne côté Home Assistant, embarquée dans le template Jinja qui construit la chaîne payload. L'appareil affiche simplement ce qu'il reçoit.

---

## Grisage des éléments inactifs

Quand un bouton ou contrôle est indisponible, l'approche commune est de le cacher avec `LV_OBJ_FLAG_HIDDEN`. Ça fonctionne mais laisse un trou dans la mise en page.

Ce projet utilise plutôt l'**opacité de recolorisation** :

```cpp
lv_obj_set_style_img_recolor(obj, lv_color_hex(0x808080), LV_PART_MAIN);
lv_obj_set_style_img_recolor_opa(obj, 180, LV_PART_MAIN);
```

Ça superpose une teinte grise sur l'élément sans le cacher. La mise en page reste symétrique, et le retour visuel communique clairement "inactif" sans perturber la géométrie de l'écran.

---

## Contraintes CPU à garder en tête

Quelques patterns LVGL qui semblent anodins mais sont coûteux sur un microcontrôleur :

**Coins arrondis sur objets transparents.** Si un objet a `bg_opa: 0` et `radius > 0`, LVGL calcule quand même le masque de clipping pour les coins arrondis à chaque passe de rendu. À 60 FPS, c'est 24000 calculs inutiles par seconde par widget. Correction : mettre `radius: 0` sur les conteneurs transparents.

**Padding inutilisé.** Toute valeur de padding non nulle fait inclure le widget dans les passes de détection de collision. Les conteneurs purement structurels devraient avoir `pad_all: 0` et `border_width: 0`.

**Invalidations fréquentes plein écran.** Appeler `lv_obj_invalidate(lv_scr_act())` force une repeinte complète au prochain tick. Le bon pattern est d'appeler `lv_obj_invalidate(widget_spécifique)` pour que seule la zone modifiée soit redessinée.
