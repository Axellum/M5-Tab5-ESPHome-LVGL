import os

# --- LOGS_APPRENTISSAGE.md ---
logs_path = 'LOGS_APPRENTISSAGE.md'
logs_addition = """
### 7. Résolution Complète des Bugs Graphiques (ESP-P4 / LVGL / Météo France)

#### 7.1 Bug d'Inversion Endianness (Byte-Swap DMA)
- **Le problème :** Lors de l'envoi d'une image native à l'ESP32-P4 en LVGL, le matériel inverse les octets du RGB565 (Endianness inverse), provoquant des teintes Magenta / Bleu très désagréables.
- **La solution :** Les images ne peuvent pas entrer "normalement". Il faut traiter le rendu des images PNG via un script local en amont (`convert_svg_qt.py`) et basculer informatiquement les octets *Big Endian* vers *Little Endian* pour que la puce le ré-inverse "correctement" au runtime.

#### 7.2 Panic Hardware sur la Transparence Alpha (RGBA)
- **Le problème :** L'utilisation de données Alpha (RGBA) fait crasher la mémoire PSRAM du P4 sur des accès à des variables `.rodata` (StoreProhibited Panic).
- **La solution :** Canal Alpha interdit ! Le contournement passe par LVGL **Chroma_Key** : Le pixel `x:0, y:0` de chaque image sert de référence colorimétrique cible "transparente". De plus, l'imagerie doit être régénérée et fusionnée numériquement avec la couleur exacte du fond cible.

#### 7.3 Les Boîtes LVGL Blanches Par Défaut (Obj)
- **Le problème :** Tout container de structure (ex: type `FLEX`) dans ESPHome nécessite un bloc enfant `- obj:`. Par défaut, le thème LVGL assigne un fond de couleur `#FFFFFF` (blanc) très visible sur les thèmes sombres.
- **La solution :** Fixation globale. Création d'un style invisible `transparent_style` (`bg_opa: TRANSP` et `border_width: 0`), injecté sur tous les objets structuraux du fichier yaml.

#### 7.4 Auto-Scroll Involontaire (Overflow LVGL)
- **Le problème :** Augmenter aléatoirement les polices dans des cases contraintes "réveille" le mécanisme d'ascenseur tactile de LVGL (`scrollbar_mode: AUTO`).
- **La solution :** Augmentation des largeurs invisibles (margins/width) et désactivation coercitive du défilement via `scrollbar_mode: "OFF"`.

#### 7.5 Gestion Artisanale et Optimisée des SVG
- **La solution retenue :** Plutôt que de développer des algorithmes "auto-crop" lourd dans Qt, il s'est avéré plus qualitatif que l'humain modifie/recadre directement le fichier source **SVG**. Le script l'ingère nativement via Qt et l'étire purement dans son cadre `250x250` sans aucune déformation.
"""

if os.path.exists(logs_path):
    with open(logs_path, 'a', encoding='utf-8') as f:
        f.write(logs_addition)

# --- MON_INSTALLATION.md ---
inst_path = 'MON_INSTALLATION.md'
inst_addition = """
## 🎨 M5Stack Tab5 : Design Actuel & Directives LVGL
- **Thème Sombre Contrasté :**
  - Fond global d'écran (`color_bg`) : Gris clair mat `#323642`.
  - Fond des cartes prévisionnelles (`color_card_bg`) : Gris abysse `#23262E`.
- **Typographie Mobile :**
  - Police `gfonts://Roboto` avec épaisseur forcée (`weight: 700` ou Bold).
  - Tailles optimisées : `36px` et `60px` omniprésentes, `120px` pour l'horloge centrale.
- **Architecture des Assets PNG :**
  - Les SVG Météo France, recadrés à la main sans marges, sont fixés dans un rendu vectoriel 1:1 de *250x250 pixels*.
  - Pour tromper le bug de canal Alpha, les icônes sont générées via script Qt en deux versions imbriquées : `icons_2_png_main/` (transparence fusionnée écran) et `icons_2_png_card/` (transparence fusionnée carte).
- **Proportions Modulaires :**
  - Pluie horaire : `height: 60px` / `width: 20px`.
  - Conteneurs `obj` : Application obligatoire du profil LVGL `transparent_style` (bg_opa=0) pour retirer le fond blanc par défaut.
"""

if os.path.exists(inst_path):
    with open(inst_path, 'a', encoding='utf-8') as f:
        f.write(inst_addition)

print("Mise à jour des fichiers Markdown effectuée avec succès.")
