# Directives Graphiques et Templates de Design LVGL Premium

Ce document répertorie les standards esthétiques et les modèles structurels (templates YAML ESPHome) requis pour concevoir des écrans tactiles premium pour le projet **Tab5 V2**.

---

## 🎨 1. Système de Design et Palette de Couleurs

Pour éviter les couleurs brutes par défaut (plain red/blue/green), utilisez les palettes harmonieuses suivantes basées sur un thème sombre (Dark Mode) haut de gamme.

### Palette de Couleurs
*   **Arrière-plan principal (Dark Slate)** : `#121820` (Profond, reposant pour les yeux)
*   **Cartes et Conteneurs (Glassmorphism)** : `#1E2633` avec opacité ou bordure fine `#2C3A4E`
*   **Couleur d'Accent Primaire (Neon Blue)** : `#00E5FF` (Utilisé pour les états actifs, valeurs clés)
*   **Couleur d'Accent Secondaire (Muted Teal)** : `#00B0FF`
*   **Alerte & Chauffage (Warm Amber)** : `#FF9100`
*   **Texte Principal** : `#FFFFFF` (Léger dégradé vers `#E0E0E0` pour les sous-titres)
*   **Texte Secondaire/Inactif** : `#7A8C9E`

### Typographie (Outfit / Inter)
*   **Titres d'écrans** : Taille 28px, Gras
*   **Valeurs principales (Température, etc.)** : Taille 36px ou 48px, Moyen/Gras
*   **Labels et contrôles** : Taille 16px ou 20px, Régulier
*   **Petits labels / Légendes** : Taille 12px, Régulier

---

## 🛠️ 2. Templates YAML ESPHome / LVGL

### A. Conteneur Style "Glassmorphism"
Idéal pour regrouper les contrôles d'une pièce.

```yaml
lvgl:
  - obj:
      id: card_salon
      width: 220
      height: 180
      x: 10
      y: 10
      style:
        bg_color: 0x1E2633
        border_color: 0x2C3A4E
        border_width: 1
        radius: 16
        pad_all: 12
        shadow_width: 8
        shadow_color: 0x0A0F15
        shadow_opa: 30
```

### B. Slider Premium pour l'Éclairage (Yeelight)
Un slider tactile fluide, de couleur Neon Blue, avec de larges marges d'appui.

```yaml
      - slider:
          id: salon_brightness
          width: 180
          height: 14
          align: BOTTOM_MID
          y: -15
          range_min: 0
          range_max: 100
          value: 75
          style:
            # Piste du slider (inactif)
            bg_color: 0x2C3A4E
            # Partie active (active)
            style_indic:
              bg_color: 0x00E5FF
            # Bouton de glissement (knob)
            style_knob:
              bg_color: 0xFFFFFF
              radius: 12
              width: 24
              height: 24
```

### C. Arc Circulaire (Température / Consommation)
Affiche une progression élégante en arc avec la valeur textuelle centrée.

```yaml
      - arc:
          id: temp_salon_arc
          width: 120
          height: 120
          align: CENTER
          range_min: 15
          range_max: 30
          value: 21
          rotation: 135
          bg_angle_start: 0
          bg_angle_end: 270
          style:
            arc_color: 0x2C3A4E
            arc_width: 10
            style_indic:
              arc_color: 0xFF9100 # Warm Amber
              arc_width: 10
      - label:
          text: "21.5°"
          align: CENTER
          style:
            text_font: Outfit_36
            text_color: 0xFFFFFF
```

---

## ✨ 3. Micro-Animations et Transitions

Pour rendre l'interface vivante :
*   **Transitions d'écrans** : Privilégier les fondus (`FADE_ON`) ou glissements doux (`SCROLL_TO`) plutôt que les changements instantanés.
*   **Hover/Active** : Réduire légèrement la luminosité des boutons au clic ou modifier la couleur de la bordure vers `#00E5FF` pour un feedback immédiat.
*   **Filtres de rafraîchissement** : Lisser les mises à jour des capteurs domotiques avec une transition amortie (filtres ESPHome `sliding_window_moving_average` ou `exponential_moving_average`).
