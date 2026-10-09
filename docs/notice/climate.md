# Climate

## English · [Français](#version-française)

---

**Opens with** a tap on the target temperature of the home screen, or on a climate card ([bottom row](tiles.md); a long press shows its [quick actions](tiles.md#quick-actions), whose **Details** opens this window): that card's unit, with its name as title. When the window closes, the home screen goes back to the climate picked in the blueprint. The **Details** of the quick actions opened by the room temperature (below) opens it too, on that unit.

**Several units**: the window has one page per climate unit (the one picked in the blueprint, then those placed on room cards). Swipe left or right for the next or previous one; the dots at the bottom show which one. With a single unit, there are no dots.

![The climate window: modes, target 20.0 with the arc, options](../images/notice/climatisation-en.webp)

**Mode** (left): **Cool**, **Heat**, **Dry**, **Fan**, **Off**. The icon of the current mode is coloured.

**Temperature** (centre)

- Drag the arc, or tap **−** and **+**: the target shows at once and leaves as one command when you stop.
- **Room**, at the bottom: the room temperature.

**Options** (right), each a tap to turn on, another to turn off:

- **Eco** and **Boost**: the presets;
- **Quiet**: the quiet fan;
- **Swing** and **Breeze**: the airflow, one or the other.

The window follows your unit: its temperature range and step, and only the buttons it can do (an options group with none left disappears). While the unit is off, the controls stay in place, dimmed. Modes without a button here (heat/cool, auto, fan speeds…) stay in Home Assistant.

### Quick actions from the room temperature

A tap on the room temperature of the home screen opens the same wheel as a climate card's [quick actions](tiles.md#quick-actions), on that temperature: the unit of the room shown in HA mode (the room's own climate first), otherwise the climate picked in the blueprint. In the middle, the unit: the room temperature, a gauge of its target and its name. Around it: **Off**, **Mode**, **Target** and **Options** (the unit's own, as on a card), and **Details**, this window on that unit. With several units, **Climates** takes the place of **Home**, on the left: it unfolds every unit (its target, or the off icon, and its name; the current one glows) and a tap moves the wheel to that one. Until Home Assistant has sent the unit's settings, the tap opens this window directly. A long press on the temperature still opens its [history](temperature.md).

---

## Version Française

---

**S'ouvre par** un tap sur la consigne de l'écran d'accueil, ou sur une carte de clim ([rangée du bas](tiles.md#version-française) ; un appui long montre ses [actions rapides](tiles.md#actions-rapides), dont **Détails** ouvre cette fenêtre) : l'appareil de cette carte, avec son nom pour titre. À la fermeture de la fenêtre, l'accueil revient à la clim choisie dans le blueprint. Le **Détails** des actions rapides ouvertes par la température de la pièce (plus bas) l'ouvre aussi, sur cette clim.

**Plusieurs clims** : la fenêtre a une page par clim (celle choisie dans le blueprint, puis celles posées sur des cartes de pièce). Glissez à gauche ou à droite pour la suivante ou la précédente ; les pastilles du bas disent laquelle. Avec une seule clim, pas de pastilles.

![La fenêtre de la clim : modes, consigne 20.0 avec l'arc, options](../images/notice/climatisation-fr.webp)

**Mode** (à gauche) : **Froid**, **Chaud**, **Sec**, **Ventilation**, **Éteint**. L'icône du mode en cours est en couleur.

**Température** (au centre)

- Faites glisser l'arc, ou touchez **−** et **+** : la consigne s'affiche tout de suite et part en une seule commande quand vous vous arrêtez.
- **Pièce**, en bas : la température de la pièce.

**Options** (à droite), chacune un tap pour l'activer, un autre pour l'arrêter :

- **Éco** et **Boost** : les préréglages ;
- **Silence** : la ventilation silencieuse ;
- **Oscillation** et **Brise** : le flux d'air, l'un ou l'autre.

La fenêtre suit votre appareil : sa plage de températures et son pas, et seulement les boutons qu'il sait faire (un groupe d'options qui n'en garde aucun disparaît). Appareil éteint, les commandes restent en place, atténuées. Les modes sans bouton ici (chaud/froid, auto, vitesses de ventilation…) restent dans Home Assistant.

### Actions rapides par la température de la pièce

Un tap sur la température de la pièce de l'accueil ouvre, sur cette température, la même roue que les [actions rapides](tiles.md#actions-rapides) d'une carte de clim : la clim de la pièce affichée en mode HA (d'abord la clim propre de la pièce), sinon celle choisie dans le blueprint. Au centre, l'appareil : la température de la pièce, une jauge de sa consigne et son nom. Autour : **Éteindre**, **Mode**, **Consigne** et **Options** (celles de l'appareil, comme sur une carte), et **Détails**, cette fenêtre sur cette clim. Avec plusieurs clims, **Clims** prend la place de **Maison**, à gauche : il déplie toutes les clims (leur consigne, ou l'icône éteinte, et leur nom ; celle de la roue brille) et un tap passe la roue sur celle-là. Tant que Home Assistant n'a pas envoyé les réglages de l'appareil, le tap ouvre directement cette fenêtre. Un appui long sur la température ouvre toujours son [historique](temperature.md#version-française).
