# Home screen

## English · [Français](#version-française)

---

The top of the screen never changes: voice, clock, buttons, temperatures, climate and the row under the clock. The numbers are those of the [overview](README.md).

## Voice: Domo, microphone, Discu, Ok Nabu (1 to 4)

![Home screen in conversation mode: the robot button is lit](../images/notice/accueil-mode-discussion-en.webp)

- **Domo** (house) and **Discu** (robot): the two voice modes. A tap picks one; the lit button is the current mode. Domo sends what you say to Home Assistant's assistant, which runs commands (« turn on the living room »). Discu sends it to your conversation assistant, for questions and chat. How to set them up: [the two voice modes](../installation/settings.md#voice-assistant-the-two-modes).
- **Microphone**: a tap listens right away, without the wake word. While the tablet answers, a tap cuts the answer and listens again. A **long press** opens the [voice assistant window](voice.md). Its colour gives the state: grey, waiting; green, listening; orange, thinking; blue, answering; red, error.
- **Ok Nabu: ON / OFF**: a tap turns the wake word on or off. Off, the tablet only listens when you tap the microphone.

Without a conversation assistant (« Tab5 · pipeline de discussion » set to « Aucun »), Domo and Discu are not shown and the tablet stays in Domo mode.

## Clock and date (5)

- **Tap**: the [alarm clock](alarm.md).
- **Long press**: the [calendar](calendar.md).

The date changes colour with the day's weather warnings (yellow, orange, red). In the status row, top left, the bell of the alarm is green when it will ring, amber when it is on but no day qualifies, struck through when it is off.

## The three buttons, top right (6 to 8)

- **Home Assistant** (house): a tap shows your **devices** in the bottom row instead of the weather; tap again for the weather. It is lit while it shows the devices. Not shown when no room has a device. Details: [bottom row and rooms](tiles.md). A **long press** opens the [Energy window](energy.md) when the solar production shows in the status row, top left: a small solar panel then marks the button, in its top-right corner.
- **Gear**: a tap opens the [settings](settings.md) (brightness, screen off, theme, language…); a **long press**, the [system console](console.md).
- **Gamepad**: a tap opens the [Arcade](arcade.md); a **long press**, the [TV remote](tv.md) when a TV is picked in the blueprint: a small screen then marks the button, in its top-right corner. Without a TV, the button stays and its long press does nothing.

**Another screen on a long press.** What these three long presses open is chosen in the « Tab5 — emplacements » blueprint, section « Boutons du haut · Top buttons »: Automatic (what is described above), Nothing, or a screen — voice assistant, calendar, alarm clock, climate, plants, TV remote, system console, Energy, settings, alerts, Arcade. The taps do not change. A small icon in the button's top-right corner shows the chosen screen (the one of its window's title). A screen your home lacks (no climate, no plant, no TV, no solar production for Energy) does not open, and the icon is not shown. If the gear no longer opens the console, the tablet's « Aller à l'écran » entity in Home Assistant still does.

## Temperatures and climate (9, 10)

- The two temperatures: the room (sofa) and a second sensor (greenhouse). A **tap on the second one** opens the [Arcade](arcade.md); without a second sensor, a gamepad stands in its place and does the same.
- A **long press on either temperature** opens its [history](temperature.md): 24 hours, 7 or 30 days, and the weather forecast for the second one.
- **Target temperature**: a tap opens the [climate window](climate.md).
- **−** and **+**: one step down or up. The new target shows at once; quick taps add up and leave as one command when you stop.
- **Another device for − and +**: a tap on the first temperature (the room) unfolds a list — the climate, the devices chosen in the « Tab5 — emplacements » blueprint (section « Tuile − / + · − / + tile »: a TV or soundbar volume, a light's brightness, a thermostat, a water heater, a humidifier, a fan's speed, a shutter's position, a number), then the tablet's volume. Tap one: − and + now adjust it, its icon and value show between them, and it stays chosen, even after a restart. A tap elsewhere closes the list. A tap on the value opens the device's window when it has one (a light or a shutter placed in a room, the TV's remote), otherwise nothing.

## Row under the clock (11)

Up to three lines of four sensors, plus the plants line, chosen in the « Tab5 — emplacements » blueprint (section « Sous l'horloge · Under the clock »). With two lines or more they take turns, just before the central card changes (every 32 seconds by default, set in the blueprint), and small dashes under the row show which line is on.

- **The plants line**: four icons, one per pot, coloured by soil moisture (red: to water, green: fine, blue: too wet). A **long press** opens the [plant details](plants.md).
- **A sensor line**: an icon per device, and the value of each sensor, coloured by what it measures (temperatures on the screen's scale, humidity, battery, gold for power and energy). A switch, a light or a detector shows its state; it is not controlled from here.
- **Tap**: the next line at once.

Without plant sensors and without a sensor line, the area stays empty.

## Central card (12)

It changes every 8 seconds between the day's schedule, the rain in the next hour, the weather warnings, a message and the alerts sent by Home Assistant.

![Central card showing the rain of the next hour and an orange warning](../images/notice/2-pluie-alerte-orange-en.webp)

- **Tap** on the schedule, the rain or the warnings: the next panel at once.
- **Tap** on a message or an alert: it is dismissed and does not come back, until Home Assistant sends a new one.
- **Long press**, whatever it shows: the [alerts window](alerts.md), the 20 latest alerts with when they appeared, were read and ended.

It also shows, for a moment, the tablet's spoken answer, a day's schedule (when you tap that day's temperatures, see [bottom row](tiles.md)), the dates of another forecast page, or the name of the room in device mode.

---

## Version Française

---

Le haut de l'écran ne change jamais : voix, horloge, boutons, températures, clim et rangée sous l'horloge. Les numéros sont ceux de la [vue d'ensemble](README.md#version-française).

## Voix : Domo, micro, Discu, Ok Nabu (1 à 4)

![L'écran d'accueil en mode discussion : le bouton du robot est allumé](../images/notice/accueil-mode-discussion-fr.webp)

- **Domo** (maison) et **Discu** (robot) : les deux modes vocaux. Un tap en choisit un ; le bouton allumé est le mode en cours. Domo envoie ce que vous dites à l'assistant de Home Assistant, qui exécute les commandes (« allume le salon »). Discu l'envoie à votre assistant de discussion, pour les questions et la conversation. Pour les régler : [les deux modes vocaux](../installation/settings.md#assistant-vocal--les-deux-modes).
- **Micro** : un tap écoute tout de suite, sans le mot de réveil. Pendant que la tablette répond, un tap coupe la réponse et réécoute. Un **appui long** ouvre la [fenêtre de l'assistant vocal](voice.md#version-française). Sa couleur donne l'état : gris, en attente ; vert, écoute ; orange, réflexion ; bleu, réponse ; rouge, erreur.
- **Ok Nabu : ON / OFF** : un tap active ou coupe le mot de réveil. Coupé, la tablette n'écoute que si vous touchez le micro.

Sans assistant de discussion (« Tab5 · pipeline de discussion » sur « Aucun »), Domo et Discu ne s'affichent pas et la tablette reste en mode Domo.

## Horloge et date (5)

- **Tap** : le [réveil](alarm.md#version-française).
- **Appui long** : le [calendrier](calendar.md#version-française).

La date change de couleur avec les vigilances météo du jour (jaune, orange, rouge). Dans la ligne d'état, en haut à gauche, la cloche du réveil est verte quand il sonnera, ambre quand il est activé mais qu'aucun jour ne convient, barrée quand il est éteint.

## Les trois boutons, en haut à droite (6 à 8)

- **Home Assistant** (maison) : un tap montre vos **appareils** dans la rangée du bas au lieu de la météo ; un nouveau tap revient à la météo. Il est allumé tant qu'il montre les appareils. Absent quand aucune pièce n'a d'appareil. Le détail : [rangée du bas et pièces](tiles.md#version-française). Un **appui long** ouvre la [fenêtre Énergie](energy.md#version-française) quand la production solaire s'affiche dans la ligne d'état, en haut à gauche : un petit panneau solaire marque alors le bouton, dans son coin en haut à droite.
- **Engrenage** : un tap ouvre les [réglages](settings.md#version-française) (luminosité, extinction, thème, langue…) ; un **appui long**, la [console système](console.md#version-française).
- **Manette** : un tap ouvre l'[Arcade](arcade.md#version-française) ; un **appui long**, la [télécommande TV](tv.md#version-française) quand une TV est choisie dans le blueprint : un petit écran marque alors le bouton, dans son coin en haut à droite. Sans TV, le bouton reste et son appui long ne fait rien.

**Un autre écran à l'appui long.** Ce qu'ouvrent ces trois appuis longs se choisit dans le blueprint « Tab5 — emplacements », section « Boutons du haut · Top buttons » : Automatique (ce qui est décrit ci-dessus), Rien, ou un écran — assistant vocal, calendrier, réveil, clim, plantes, télécommande TV, console système, Énergie, réglages, alertes, Arcade. Les taps ne changent pas. Une petite icône dans le coin en haut à droite du bouton montre l'écran choisi (celle du titre de sa fenêtre). Un écran absent de la maison (pas de clim, pas de plante, pas de TV, pas de production solaire pour Énergie) ne s'ouvre pas, et l'icône ne s'affiche pas. Si l'engrenage n'ouvre plus la console, l'entité « Aller à l'écran » de la tablette dans Home Assistant l'ouvre toujours.

## Températures et clim (9, 10)

- Les deux températures : la pièce (canapé) et une seconde sonde (serre). Un **tap sur la seconde** ouvre l'[Arcade](arcade.md#version-française) ; sans seconde sonde, une manette prend sa place et fait de même.
- Un **appui long sur l'une des deux températures** ouvre son [historique](temperature.md#version-française) : 24 heures, 7 ou 30 jours, et la prévision de la météo pour la seconde.
- **Consigne** : un tap ouvre la [fenêtre de la clim](climate.md#version-française).
- **−** et **+** : un pas de moins ou de plus. La nouvelle consigne s'affiche tout de suite ; des taps rapides s'additionnent et partent en une seule commande quand vous vous arrêtez.
- **Un autre appareil pour − et +** : un tap sur la première température (la pièce) déroule une liste — la clim, les appareils choisis dans le blueprint « Tab5 — emplacements » (section « Tuile − / + · − / + tile » : volume d'une TV ou d'une barre de son, luminosité d'une lampe, un thermostat, un chauffe-eau, un humidificateur, la vitesse d'un ventilateur, la position d'un volet, un nombre), puis le volume de la tablette. Un tap en choisit un : − et + le règlent désormais, son icône et sa valeur s'affichent entre les deux, et il reste choisi, même après un redémarrage. Un tap ailleurs ferme la liste. Un tap sur la valeur ouvre la fenêtre de l'appareil quand il en a une (une lampe ou un volet placé dans une pièce, la télécommande de la TV), sinon rien.

## Rangée sous l'horloge (11)

Jusqu'à trois lignes de quatre capteurs, plus la ligne des plantes, choisies dans le blueprint « Tab5 — emplacements » (section « Sous l'horloge · Under the clock »). À partir de deux lignes, elles se relaient juste avant que la carte centrale change (toutes les 32 secondes d'origine, réglable dans le blueprint), et de petits tirets sous la rangée montrent la ligne affichée.

- **La ligne des plantes** : quatre icônes, une par pot, colorées selon l'humidité de la terre (rouge : à arroser, vert : bien, bleu : trop humide). Un **appui long** ouvre le [détail des plantes](plants.md#version-française).
- **Une ligne de capteurs** : une icône par appareil, et la valeur de chaque capteur, colorée selon ce qu'il mesure (températures sur l'échelle de l'écran, humidité, batterie, or pour la puissance et l'énergie). Un interrupteur, une lampe ou un détecteur montre son état ; il ne se commande pas d'ici.
- **Tap** : la ligne suivante, tout de suite.

Sans capteurs de plantes ni ligne de capteurs, la zone reste vide.

## Carte centrale (12)

Elle passe toutes les 8 secondes du planning du jour à la pluie de l'heure qui vient, aux vigilances météo, à un message et aux alertes envoyées par Home Assistant.

![La carte centrale montre la pluie de l'heure qui vient et une vigilance orange](../images/notice/2-pluie-alerte-orange-fr.webp)

- **Tap** sur le planning, la pluie ou les vigilances : le panneau suivant, tout de suite.
- **Tap** sur un message ou une alerte : il est écarté et ne revient pas, jusqu'à ce que Home Assistant en envoie un nouveau.
- **Appui long**, quoi qu'elle montre : la [fenêtre des alertes](alerts.md#version-française), les 20 dernières alertes avec l'heure où elles sont apparues, ont été lues et se sont terminées.

Elle montre aussi, un moment, la réponse parlée de la tablette, le planning d'un jour (quand vous touchez les températures de ce jour, voir la [rangée du bas](tiles.md#version-française)), les dates d'une autre page de prévisions, ou le nom de la pièce en mode appareils.
