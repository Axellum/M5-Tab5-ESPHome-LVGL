# Step 6 — Your devices

## English · [Français](#version-française)

---

Your devices — lights, shutters, climate, plants, TV, sensors — are picked in **one automation**, made from the blueprint « Tab5 — emplacements de l'écran · screen slots » that step 1 added ([ADR-0019](../decisions/0019-logical-slots-blueprint.md)). The tablet itself names no device: changing one is an edit in Home Assistant, no flash, no restart.

## Create the automation

1. *Settings → Automations & scenes → Blueprints*, « Tab5 — emplacements de l'écran · screen slots ».
   Not in the list? *Import blueprint*, with
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`
2. *Create automation*.
3. Fill the sections you need (every field is optional; the labels are in French and English), then *Save*.

![The screen slots automation in the Home Assistant editor: the blueprint, its description and its sections, Room 1 open, Rooms 2 to 5 and Customise tiles folded](../images/installation/ha_blueprint_en.png)

**One automation per tablet.** As soon as it is saved, the tablet receives your devices.

**It worked if** the tiles at the bottom of the screen show your devices. The tablet's diagnostic sensor « Zones masquées » lists what is hidden: a slot you left empty, or an entity that does not exist.

## The sections

| Section | What you pick | More |
|---|---|---|
| Pièce 1 — accueil · Room 1 — home | a name and up to five devices: the tiles of the home page, left to right | [rooms](adapt-to-your-home.md#rooms-firmware-32-and-later) |
| Pièce 2 to 5 · Room 2 to 5 (folded) | the rooms one or two swipes away | [rooms](adapt-to-your-home.md#rooms-firmware-32-and-later) |
| Personnaliser des tuiles · Customise tiles (folded) | another name, icon or behaviour for a tile (on only, confirm, read only) | [tile icons](../tiles_icons.md) |
| TV, téléphone · TV, phone | the TV, its remote, the phone battery | [other zones](adapt-to-your-home.md#other-zones) |
| Températures · Temperatures | room temperature and humidity, a second temperature (greenhouse) | |
| Climatisation · Climate | the climate unit of the home card | [climate, any brand](adapt-to-your-home.md#limits) |
| Plantes · Plants | up to five moisture sensors | |
| Sous l'horloge · Under the clock (folded) | up to three lines of four sensors under the clock (temperatures, humidity, production, batteries, detectors, switches: shown, not controlled), the place of the plants line, the time per line | [user manual, home screen](../notice/home.md) |
| Planning de travail · Work schedule | empty: the calendar of the « Tab5 · agenda de travail » list ([step 5](sources.md)) | |
| Météo · Weather | empty: the weather lists of [step 5](sources.md). Filled, it writes its choice into them and wins over them; with several tablets, fill it in one automation only | [weather providers](weather.md) |
| Énergie · Energy (folded) | solar, grid, home and battery | [solar energy](adapt-to-your-home.md#solar-energy-optional) |
| Tuiles de l'accueil (réglage 3.x) · Home tiles (3.x setup) (folded) | used while room 1 is empty, and by a 3.0 or 3.1 firmware | [rooms](adapt-to-your-home.md#rooms-firmware-32-and-later) |
| Avancé · Advanced (folded) | the tablet's name in ESPHome: change it only if you renamed the device | |

**What you don't have disappears**, with its buttons: leave its slot empty ([other zones](adapt-to-your-home.md#other-zones)).

How to use the tiles on the screen (tap, long press, swipe): [user manual, bottom row](../notice/tiles.md).

**Next (optional): [step 7, a dashboard](dashboard.md).**

---

## Version Française

---

Vos appareils — lumières, volets, clim, plantes, TV, capteurs — se choisissent dans **une automatisation**, créée depuis le blueprint « Tab5 — emplacements de l'écran · screen slots » que l'étape 1 a ajouté ([ADR-0019](../decisions/0019-logical-slots-blueprint.md)). La tablette ne nomme aucun appareil : en changer se fait dans Home Assistant, ni flash ni redémarrage.

## Créer l'automatisation

1. *Paramètres → Automatisations et scènes → Blueprints*, « Tab5 — emplacements de l'écran · screen slots ».
   Absent de la liste ? *Importer un blueprint*, avec
   `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`
2. *Créer une automatisation*.
3. Remplissez les sections utiles (tous les champs sont facultatifs ; les libellés sont en français et en anglais), puis *Enregistrer*.

![L'automatisation des emplacements dans l'éditeur de Home Assistant : le blueprint, sa description et ses sections, Pièce 1 ouverte, Pièces 2 à 5 et Personnaliser des tuiles repliées](../images/installation/ha_blueprint_fr.png)

**Une automatisation par tablette.** Dès qu'elle est enregistrée, la tablette reçoit vos appareils.

**C'est bon si** les tuiles du bas de l'écran montrent vos appareils. Le capteur de diagnostic « Zones masquées » de la tablette liste ce qui est masqué : un emplacement laissé vide, ou une entité qui n'existe pas.

## Les sections

| Section | Ce que vous choisissez | Plus |
|---|---|---|
| Pièce 1 — accueil · Room 1 — home | un nom et jusqu'à cinq appareils : les tuiles de l'accueil, de gauche à droite | [pièces](adapt-to-your-home.md#pièces-firmware-32-et-plus) |
| Pièce 2 à 5 · Room 2 to 5 (repliées) | les pièces à un ou deux glissements | [pièces](adapt-to-your-home.md#pièces-firmware-32-et-plus) |
| Personnaliser des tuiles · Customise tiles (repliée) | un autre nom, une autre icône ou un comportement pour une tuile (allumer seulement, confirmer, lecture seule) | [icônes des tuiles](../tiles_icons.md#version-française) |
| TV, téléphone · TV, phone | la TV, sa télécommande, la batterie du téléphone | [autres zones](adapt-to-your-home.md#autres-zones) |
| Températures · Temperatures | température et humidité de la pièce, une seconde température (serre) | |
| Climatisation · Climate | la clim de la carte de l'accueil | [clim, toutes marques](adapt-to-your-home.md#limites) |
| Plantes · Plants | jusqu'à cinq capteurs d'humidité | |
| Sous l'horloge · Under the clock (repliée) | jusqu'à trois lignes de quatre capteurs sous l'horloge (températures, humidités, production, batteries, détecteurs, interrupteurs : montrés, pas commandés), la place de la ligne des plantes, la durée d'une ligne | [notice, écran d'accueil](../notice/home.md#version-française) |
| Planning de travail · Work schedule | vide : l'agenda de la liste « Tab5 · agenda de travail » ([étape 5](sources.md#version-française)) | |
| Météo · Weather | vide : les listes météo de l'[étape 5](sources.md#version-française). Remplie, elle écrit son choix dans ces listes et prime sur elles ; avec plusieurs tablettes, remplissez-la dans une seule automatisation | [fournisseurs météo](weather.md#version-française) |
| Énergie · Energy (repliée) | solaire, réseau, maison et batterie | [énergie solaire](adapt-to-your-home.md#énergie-solaire-facultatif) |
| Tuiles de l'accueil (réglage 3.x) · Home tiles (3.x setup) (repliée) | sert tant que la pièce 1 est vide, et à un firmware 3.0 ou 3.1 | [pièces](adapt-to-your-home.md#pièces-firmware-32-et-plus) |
| Avancé · Advanced (repliée) | le nom de la tablette dans ESPHome : ne le changez que si vous avez renommé l'appareil | |

**Ce que vous n'avez pas disparaît**, avec ses boutons : laissez son emplacement vide ([autres zones](adapt-to-your-home.md#autres-zones)).

Se servir des tuiles à l'écran (appui, appui long, glissement) : [notice, rangée du bas](../notice/tiles.md#version-française).

**Ensuite (facultatif) : [étape 7, un tableau de bord](dashboard.md#version-française).**
