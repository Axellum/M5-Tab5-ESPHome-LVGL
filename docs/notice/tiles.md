# Bottom row and rooms

## English · [Français](#version-française)

---

The five cards at the bottom show the **weather** (by default) or your **devices**, room by room. The Home Assistant button, top right, switches from one to the other.

## Weather

The home page shows today and the next four days.

- **Swipe** left or right on the bottom row or the central card: swipe left, days 6 to 10, then 11 to 15, then back to the home page; swipe right, the next hours, five per page, over two pages, then back to the home page. The dots under the central card show the page; the central card gives its dates.
- After 25 s without a touch, the home page comes back by itself.
- **Tap a day's temperatures** (the day pages): that day's work schedule, in the central card, for 6 s.

![Swipe right: the next hours, five per card](../images/notice/accueil-previsions-heures-1-en.webp)

![A tap on Wednesday's temperatures: its schedule in the central card](../images/notice/accueil-planning-du-jour-en.webp)

**Your devices on the weather.** Each page of the weather is also a room. A card that holds a device shows it in its two top corners: on the left, its icon in the colour of its state; on the right, a bulb for a light, or for a shutter the arrow of its next move (pause while it moves). The large weather icon then works as the device's button: tap and long press as in the table below. To keep the weather alone, turn off the device switch « Tab5 Appareils sur la météo ».

## Your devices

![Device mode: the five devices of the first room](../images/notice/accueil-ha-piece-1-en.webp)

A tap on the Home Assistant button: each card shows a device of the room, with its icon, its name and its state (« 71 % », « Off », « Moving », « Offline » when Home Assistant cannot reach it…). The central card gives the room, « Room 1/5 » and its name.

- **Swipe**: the next or previous room that has devices. With a single room, nothing happens.
- Another tap on the Home Assistant button: back to the weather. Device mode never goes back by itself.

## Tap and long press, by device

On a device card, or on the weather icon of a card that holds a device:

| Device | Tap | Long press |
|---|---|---|
| Light | on / off | [lights window](lights.md) |
| Switch, plug, fan… | on / off | — |
| Shutter, valve | moving: stop; open: close; otherwise: open | [shutter window](shutters.md) |
| TV, media player | on / off | [TV remote](tv.md), for the TV picked in the blueprint |
| Scene, script, button | runs it; the state shows « OK » for a second | — |
| Climate | [climate window](climate.md), for this unit | — |
| Sensor of the « Énergie · Energy » section | [energy window](energy.md) | [energy window](energy.md) |
| Other sensor | — (it only shows its value) | — |

**Customised cards** (blueprint section « Personnaliser des tuiles », [step 6](../installation/devices.md)):

- *on only*: a tap turns the device on, never off;
- *confirm*: the first tap only asks, the state shows « Confirm? » and the icon turns amber; a second tap within 3 s sends. On a shutter, the long press then sends the other way (open or close), confirmed the same way, instead of opening the window;
- *read only*: the card shows the state and does nothing.

---

## Version Française

---

Les cinq cartes du bas montrent la **météo** (d'origine) ou vos **appareils**, pièce par pièce. Le bouton Home Assistant, en haut à droite, passe de l'une à l'autre.

## Météo

La page d'accueil montre aujourd'hui et les quatre jours suivants.

- **Glisser** vers la gauche ou la droite sur la rangée du bas ou la carte centrale : vers la gauche, les jours 6 à 10, puis 11 à 15, puis retour à la page d'accueil ; vers la droite, les heures qui viennent, cinq par page, sur deux pages, puis retour à la page d'accueil. Les points sous la carte centrale montrent la page ; la carte centrale donne ses dates.
- Après 25 s sans toucher, la page d'accueil revient seule.
- **Tap sur les températures d'un jour** (pages des jours) : le planning de travail de ce jour, dans la carte centrale, pendant 6 s.

![Glisser vers la droite : les heures qui viennent, cinq cartes](../images/notice/accueil-previsions-heures-1-fr.webp)

![Un tap sur les températures de mercredi : son planning dans la carte centrale](../images/notice/accueil-planning-du-jour-fr.webp)

**Vos appareils sur la météo.** Chaque page de la météo est aussi une pièce. Une carte qui porte un appareil le montre dans ses deux coins du haut : à gauche, son icône dans la couleur de son état ; à droite, une ampoule pour une lumière, ou pour un volet la flèche de son prochain mouvement (pause pendant qu'il bouge). La grande icône météo sert alors de bouton à l'appareil : tap et appui long comme dans le tableau plus bas. Pour garder la météo seule, éteignez l'interrupteur « Tab5 Appareils sur la météo » de l'appareil.

## Vos appareils

![Le mode appareils : les cinq appareils de la première pièce](../images/notice/accueil-ha-piece-1-fr.webp)

Un tap sur le bouton Home Assistant : chaque carte montre un appareil de la pièce, avec son icône, son nom et son état (« 71 % », « Éteint », « Mouvement », « Hors ligne » quand Home Assistant ne le joint pas…). La carte centrale donne la pièce, « Pièce 1/5 » et son nom.

- **Glisser** : la pièce suivante ou précédente qui a des appareils. Avec une seule pièce, rien ne se passe.
- Un nouveau tap sur le bouton Home Assistant : retour à la météo. Le mode appareils ne revient jamais seul à la météo.

## Tap et appui long, par appareil

Sur une carte d'appareil, ou sur l'icône météo d'une carte qui porte un appareil :

| Appareil | Tap | Appui long |
|---|---|---|
| Lumière | allumer / éteindre | [fenêtre des lumières](lights.md#version-française) |
| Interrupteur, prise, ventilateur… | allumer / éteindre | — |
| Volet, vanne | en mouvement : stop ; ouvert : fermer ; sinon : ouvrir | [fenêtre du volet](shutters.md#version-française) |
| TV, lecteur multimédia | allumer / éteindre | [télécommande TV](tv.md#version-française), pour la TV choisie dans le blueprint |
| Scène, script, bouton | le lance ; l'état affiche « OK » une seconde | — |
| Clim | [fenêtre de la clim](climate.md#version-française), pour cet appareil | — |
| Capteur de la section « Énergie · Energy » | [fenêtre de l'énergie](energy.md#version-française) | [fenêtre de l'énergie](energy.md#version-française) |
| Autre capteur | — (il montre seulement sa valeur) | — |

**Cartes personnalisées** (section « Personnaliser des tuiles » du blueprint, [étape 6](../installation/devices.md#version-française)) :

- *allumer seulement* : un tap allume l'appareil, jamais ne l'éteint ;
- *confirmer* : le premier tap demande seulement, l'état affiche « Confirmer ? » et l'icône passe en ambre ; un second tap dans les 3 s envoie. Sur un volet, l'appui long envoie alors l'autre sens (ouvrir ou fermer), confirmé de la même façon, au lieu d'ouvrir la fenêtre ;
- *lecture seule* : la carte montre l'état et ne fait rien.
