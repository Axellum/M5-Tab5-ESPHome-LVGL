# Shutters

## English · [Français](#version-française)

---

**Opens with** a long press on a shutter or a valve (its card in device mode, or the weather icon of a card that holds it), then **Details** among its [quick actions](tiles.md#quick-actions): on its room, with that shutter selected. Also with « Aller à l'écran » → *Volets* from Home Assistant, or a [home page gesture](home.md) set to *Shutters*: on the room shown on the home page, or the first one that has shutters. A tap on a shutter's card stops it while it moves, closes it when it is open, opens it otherwise ([bottom row](tiles.md)).

![The shutter window: the drawn shutter at 45 %, moving, Open, Stop and Close](../images/notice/volet-en.webp)

**One page per room**: every shutter of the house, room by room, like the [lights](lights.md). The names of the rooms are at the top, next to the ×: **swipe left or right** (except on the drawn shutter) or **tap a room's name**. One room only: no names, no swipe.

**Shutters** (left)

- The shutters of the room, up to five, each with its icon, its name and its state, like its card. The selected one is framed: the right side shows it. A tap on another one: the window now shows it.
- **Tap the round icon** of a shutter: what a tap on its card does (stop, close or open).
- **Long press** a shutter: its [quick actions](tiles.md#quick-actions), around its icon.
- **Open all**, **Close all**: every shutter of the room shown.

**Position** (right), for the selected shutter

- A drawn shutter: its slats come down from the top as the shutter closes.
- **Drag it** up or down with a finger, from anywhere on the window, and **lift your finger**: the shutter goes to that position. The drawing and the number follow your finger; nothing leaves while you drag, and a simple touch sends nothing.
- To its right, its name, the position in large digits and the state in words: « Open », « Closed », « Partly open », « Moving », « Offline ».
- The drawing follows the real shutter while it moves, never under your finger.
- **Open**, **Stop**, **Close**, at the bottom right.

![The shutter dragged down with a finger, from 45 % to 13 %](../images/notice/volet-glisse-en.webp)

A shutter that does not report its position cannot be dragged: the drawing shows its state (open at the top, closed at the bottom, half-way with faded slats otherwise) and the words, without a number:

![The window of a shutter without position: half-way, faded, and the state](../images/notice/volet-sans-position-en.webp)

A shutter card set to *confirm* or *read-only* in the blueprint is not in this window: its long press sends the other way, open or close, after a second press ([customised cards](tiles.md#tap-and-long-press-by-device)).

---

## Version Française

---

**S'ouvre par** un appui long sur un volet ou une vanne (sa carte en mode appareils, ou l'icône météo d'une carte qui le porte), puis **Détails** parmi ses [actions rapides](tiles.md#actions-rapides) : sur sa pièce, ce volet choisi. Aussi par « Aller à l'écran » → *Volets* depuis Home Assistant, ou un [geste de l'accueil](home.md#version-française) réglé sur *Volets* : sur la pièce affichée à l'accueil, ou la première qui a des volets. Un tap sur la carte d'un volet l'arrête s'il bouge, le ferme s'il est ouvert, l'ouvre sinon ([rangée du bas](tiles.md#version-française)).

![La fenêtre du volet : le volet dessiné à 45 %, en mouvement, Ouvrir, Stop et Fermer](../images/notice/volet-fr.webp)

**Une page par pièce** : tous les volets de la maison, pièce par pièce, comme les [lumières](lights.md#version-française). Les noms des pièces sont en haut, à côté de la × : **glissez à gauche ou à droite** (sauf sur le volet dessiné) ou **touchez le nom d'une pièce**. Une seule pièce : ni noms ni glissement.

**Volets** (à gauche)

- Les volets de la pièce, cinq au plus, chacun avec son icône, son nom et son état, comme sa carte. Celui qui est choisi est encadré : la partie droite le montre. Un tap sur un autre : la fenêtre le montre désormais.
- **Touchez l'icône ronde** d'un volet : ce que fait un tap sur sa carte (arrêter, fermer ou ouvrir).
- **Appui long** sur un volet : ses [actions rapides](tiles.md#actions-rapides), autour de son icône.
- **Tout ouvrir**, **Tout fermer** : tous les volets de la pièce affichée.

**Position** (à droite), du volet choisi

- Un volet dessiné : ses lames descendent depuis le haut quand le volet se ferme.
- **Faites-le glisser** du doigt vers le haut ou le bas, depuis n'importe où sur la fenêtre, et **levez le doigt** : le volet va à cette position. Le dessin et le nombre suivent le doigt ; rien ne part pendant le glissement, et un simple toucher n'envoie rien.
- À sa droite, son nom, la position en grands chiffres et l'état en mots : « Ouvert », « Fermé », « Partiel », « En mouvement », « Hors ligne ».
- Le dessin suit le vrai volet pendant qu'il bouge, jamais sous votre doigt.
- **Ouvrir**, **Stop**, **Fermer**, en bas à droite.

![Le volet tiré du doigt vers le bas, de 45 % à 13 %](../images/notice/volet-glisse-fr.webp)

Un volet qui ne donne pas sa position ne se fait pas glisser : le dessin montre son état (ouvert en haut, fermé en bas, sinon à mi-hauteur en lames estompées) et les mots, sans nombre :

![La fenêtre d'un volet sans position : à mi-hauteur, estompé, et l'état](../images/notice/volet-sans-position-fr.webp)

Une carte de volet réglée sur *confirmer* ou *lecture seule* dans le blueprint n'est pas dans cette fenêtre : son appui long envoie l'autre sens, ouvrir ou fermer, après un second appui ([cartes personnalisées](tiles.md#tap-et-appui-long-par-appareil)).
