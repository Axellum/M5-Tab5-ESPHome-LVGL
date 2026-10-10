# Adapt to your home

## English · [Français](#version-française)

---

The screen was first drawn around the author's home. Everything is chosen in Home Assistant, in the « Tab5 — emplacements » automation (the blueprint of [step 6](devices.md)): changing a device is an edit in HA's UI, no flash, no restart.

## Rooms (firmware 3.2 and later)

The five tiles at the bottom of the screen are **rooms** you fill yourself ([ADR-0023](../decisions/0023-rooms-generic-tiles.md)):

- **Up to 5 rooms of 5 devices**, one per page of the bottom row. Room 1 is the home page (today to day 4); rooms 2 and 3 are one and two swipes to the left (days 5-9, 10-14); rooms 4 and 5 one and two swipes to the right (next hours). In each room, pick the devices in the order of the tiles, left to right (they can be dragged); only the first five are used. A device may be in several rooms.
- **Devices that fit on a tile**: lights; switches, fans, humidifiers, input booleans and automations; covers and valves; media players; scenes, scripts and buttons; sensors and numbers (shown, not controlled); binary sensors, people, trackers and locks (shown); climate — a climate tile shows the room temperature, and a tap opens the climate popup for that unit (the one in the « Climatisation » input keeps the − / setpoint / + card of the home page).
- **Names and icons come from Home Assistant.** A room takes the name you type, otherwise the area its devices share, otherwise « Pièce n ». A tile takes the entity's name without the room's name (« Lampe du salon » in « Salon » becomes « Lampe »), and the icon chosen in the entity's settings, otherwise one for its kind.
- **Customise a tile** (folded section « Personnaliser des tuiles »): another name, another icon, or a behaviour — *on only* (never switched off from the screen), *confirm* (a second tap within 3 s), *read only*.
- The « HA » button shows the current page's room; a swipe goes to the next room that has devices.
- **A room's climate** (optional, [ADR-0040](../decisions/0040-room-climate.md)): three more fields in each room's section, all empty by default — « Température de la pièce · Room temperature » (a temperature sensor), « Humidité de la pièce · Room humidity » (a humidity sensor) and « Climatisation de la pièce · Room climate » (a climate). With a temperature, the climate card of the home page shows that room while you are on it in HA mode: its temperature on the left, its humidity on the right (or nothing), and the − / + tile adjusts its climate. Its temperature's long press opens its history. The humidity and the climate need the temperature. Both the firmware and the blueprint must have it; either one alone keeps the screen of before.
- A sensor's value is sent with the other measurements, every 5 minutes; the other devices are sent as soon as what the screen shows changes.
- **Room 1 left empty**: the home page keeps the 3.x setup (folded section « Ancien accueil (si la pièce 1 est vide) · Former home page (if room 1 is empty) »: PC or TV, shutter, three lights), with the PC tile's PC + TV behaviour. Nothing to redo after the update.
- **Firmware 3.0 or 3.1**: the blueprint reads the tablet's version and then only uses the 3.x setup; the rooms show up once the firmware is updated. The firmware and the blueprint can be updated in either order.

## Solar energy (optional)

A solar installation gets its own popup ([ADR-0028](../decisions/0028-solar-energy-popup.md)): solar, home, grid and battery live, and the production as bars per hour (today), per day (30 days) and per month (12 months).

1. In the « Tab5 — emplacements » automation, open the three folded sections **« Énergie : solaire · Energy: solar »**, **« Énergie : réseau et maison · Energy: grid and home »** and **« Énergie : batterie · Energy: battery »**, and pick what you have: solar power, solar energy produced (the kWh meter of HA's Energy dashboard: the day total and the bars come from its statistics), grid power, home consumption, battery level, power and temperature. Every field is optional. Grid power is positive when the home buys: tick « Invert » if your meter says the opposite, or give the export in its own field if your meter splits them. Battery power is positive when charging (same « Invert » box). Several inverters or panel strings (pv1, pv2…): put the first one in « Solar power » / « Solar energy produced » and the others in « Other solar power » / « Other solar energy produced »; they are added up (an inverter unavailable at night is skipped, a meter without data for a period counts as 0). Left empty, home consumption is computed (solar + grid − battery charge). Give the panels' **peak power** (kWp, all the panels) too, and an icon of the status strip, top left, shows the production as a share of it by its colour (grey panel at night); 0 = no icon.
2. Place one of these sensors in a room (the solar power, for instance): its tile shows the value and opens the popup on tap. The option « Énergie » of the tablet's « Aller à l'écran » select, in Home Assistant, opens it too.
3. The `tab5_energie.yaml` package (in the archive of [step 1](home-assistant-files.md)) pushes the values while the popup is open. Without it, the popup says « En attente de Home Assistant ».
4. **The day's forecast and the balance** (optional, [ADR-0058](../decisions/0058-energy-sun-forecast-balance.md)). With the solar energy meter picked, the « Today » page needs nothing more: the sun's times come from HA's `sun.sun`, the clear-sky curve is learned from your own production of the last 30 days (orientation and shade included; it appears after about three sunny days), and the forecast lowers it with the clouds of the hourly weather forecast the tablet already uses. If you have a forecast integration (Forecast.Solar, Solcast…), pick its kWh sensors in **« Prévision de production du jour »** and **« … de demain »**: the learned shape is scaled to them. For the « Balance » page, pick **« Énergie achetée au réseau »** and **« Énergie vendue au réseau »** (the grid meters of HA's Energy dashboard), and for the savings the **purchase and resale prices** per kWh (a number, or a price sensor that wins) and the **currency** (€ when empty).

![Energy popup of the Tab5, Days view: solar, home, grid and battery right now, and the production of the last 30 days (CI render, demo data)](../images/tab5_energie_en.png)

Section left empty: nothing changes on the screen.

## Temperature history

A long press on one of the two home-screen temperatures opens its history ([ADR-0032](../decisions/0032-temperature-history-popup.md)): 24 hours, 7 days or 30 days, from Home Assistant's long-term statistics, and the weather forecast for the second temperature.

1. The `tab5_historique.yaml` package (in the archive of [step 1](home-assistant-files.md)) sends the curve while the popup is open. Without it, the popup says « En attente de Home Assistant ».
2. If your second temperature is **outdoors**, tick « La seconde température est dehors · The second temperature is outdoors » in the « Tab5 — emplacements » automation (section « Températures · Temperatures »): the forecast then extends its curve. Unticked (a greenhouse), the forecast stays apart, as « Outdoors, forecast ».
3. The sensor needs statistics (a `state_class`, which thermometers have); the forecast is that of the weather entity picked for the tablet.

## Other zones

**What you don't have disappears**, with its buttons ([ADR-0018](../decisions/0018-optional-zones-confirmed-by-ha.md), [ADR-0019](../decisions/0019-logical-slots-blueprint.md)).

- **Remove a zone: leave its slot empty.** An empty slot, or an entity that doesn't exist, is absent. An entity that exists but is `unavailable` keeps its zone (« -- », « Hors ligne »). **Without the blueprint's automation, nothing disappears** (and nothing of your devices is shown).
- **A zone missing by mistake?** The tablet's diagnostic sensor « Zones masquées » lists what disappeared.
- A zone comes back by itself as soon as its entity sends a value.

| Zone | Blueprint input | Hidden when empty |
|---|---|---|
| TV | TV, and Télécommande de la TV for the remote keys (Autres télécommandes: up to three more pages, an Apple TV, a Freebox Player…) | TV button and remote; « HA » and « Sys » move one column right |
| Phone | Batterie du téléphone | Status icon |
| Room | Température de la pièce (and Humidité de la pièce) | Its temperature |
| Greenhouse | Seconde température (serre) | Its temperature; the icon becomes a carousel, its tap still switches the area left of the clock |
| Plants (0 to 5) | Pot 1 to 5: the moisture sensor; conductivity, light, temperature and battery are taken from the same device | Up to 4 plants: one slot each; 5: the « driest / median / wettest » summary. Popup cards, re-centred |
| Climate | Climatisation | − / setpoint / + and the popup |
| Work planning | Agenda de travail | Planning panel of the central card |
| Voice « Discussion » mode | none: the list « Tab5 · pipeline de discussion » set to « Aucun » (without the list, the zone stays) | Domo / Discu buttons of the home page and of the assistant popup; the tablet goes back to Domo |
| Lights (3.x setup) | Lumière 1 to 3 · Light 1 to 3 (« Ancien accueil » section) | Icons of tiles 3 to 5, card of the « HA » layer, rows of the Lights popup, « Tout éteindre » |
| PC (3.x setup) | PC (a switch turns it on; a presence tracker only shows it) | Status icon, « PC Bureau » card; the first tile too if there is no TV either |
| Shutter (3.x setup) | Volet (and the optional `volet_serre_tracking.yaml` package, `tab5_optionnel/` of the archive, for a shutter that doesn't report its travel) | Icons of tile 2, card of the « HA » layer |

The « 3.x setup » rows (blueprint section « Ancien accueil · Former home page ») are the home page of a 3.0 or 3.1 firmware, and of a 3.2 firmware while room 1 is empty. The planning hours and the alarm time come from the list « Tab5 · agenda de travail » ([step 5](sources.md)); leave the blueprint's « Agenda de travail » empty and it takes the same one.

## Limits

- **Icons**: the screen holds a limited palette ([tile icons](../tiles_icons.md)). An icon outside it shows the default of its kind; adding one means a line in `Tab5/tuiles_icones.yaml` and a new release.
- **Names**: the screen's fonts cover Latin alphabets only; other characters are dropped, and long names are cut.
- **Renaming an entity**: its tile follows at the tablet's next connection, or as soon as the automation is saved again.
- **Climate** ([ADR-0026](../decisions/0026-climate-from-device.md)): any brand. The blueprint sends the unit's bounds, step, unit (°C or °F, that of your weather entity) and modes; the popup takes its name as title, and a button the unit cannot do disappears. **Several units** ([ADR-0027](../decisions/0027-climate-per-tile.md)): put each one in a room; its tile opens the popup for it, with its own bounds, modes and name. A change made outside the screen (remote, app) shows in the popup at once for the setpoint and the mode, within 5 minutes for fan, swing, preset and room temperature. With a firmware older than the blueprint, such a tile only shows its temperature. The screen has buttons for cool, heat, dry, fan and off, Éco, Boost, Silence, Oscillation and Brise only: other modes (heat/cool, auto, fan speeds, sleep…) stay in Home Assistant (a unit in heat/cool or auto lights no mode button). With a blueprint older than the firmware, the popup stays as before (16-30 °C, steps of 0.5, every button).
- **A shutter followed by `volet_serre_tracking.yaml`** (it doesn't report its travel): keep it in the « Volet » input of the 3.x section too, even if it is in a room; its tile then shows the state the package keeps, and its commands go through the package's script.
- **One calendar per role**: work, appointments, birthdays, public holidays and school holidays are the five « Tab5 · agenda … » lists.
- **Spoken morning briefing**: in the screen's language (French, English, German, Dutch, Spanish, Italian, Turkish); only the French text has been reviewed.
- To see a smaller home without touching yours: [demo mode](../demo_mode.md#minimal-home-optional-zones), option `--maison-minimale`.

---

---

## Version Française

---

L'écran a d'abord été dessiné autour de la maison de l'auteur. Tout se choisit dans Home Assistant, dans l'automatisation « Tab5 — emplacements » (le blueprint de l'[étape 6](devices.md#version-française)) : changer d'appareil se fait dans l'interface de HA, ni flash ni redémarrage.

## Pièces (firmware 3.2 et plus)

Les cinq tuiles du bas de l'écran sont des **pièces** que vous remplissez vous-même ([ADR-0023](../decisions/0023-rooms-generic-tiles.md)) :

- **Jusqu'à 5 pièces de 5 appareils**, une par page de la rangée du bas. La pièce 1 est l'accueil (aujourd'hui à J+4) ; les pièces 2 et 3 sont à un et deux glissements vers la gauche (J+5 à J+9, J+10 à J+14) ; les pièces 4 et 5 à un et deux glissements vers la droite (prochaines heures). Dans chaque pièce, choisissez les appareils dans l'ordre des tuiles, de gauche à droite (ils se déplacent à la souris) ; seuls les cinq premiers servent. Un appareil peut être dans plusieurs pièces.
- **Ce qui trouve place sur une tuile** : lumières ; interrupteurs, ventilateurs, humidificateurs, entrées booléennes et automatisations ; volets et vannes ; lecteurs multimédia ; scènes, scripts et boutons ; capteurs et nombres (affichés, pas commandés) ; capteurs binaires, personnes, suivis de présence et serrures (affichés) ; climatisation — une tuile de clim montre la température de la pièce, et un appui ouvre le popup clim pour cet appareil (celui de l'entrée « Climatisation » garde la carte − / consigne / + de l'accueil).
- **Noms et icônes viennent de Home Assistant.** Une pièce prend le nom que vous saisissez, sinon l'aire que partagent ses appareils, sinon « Pièce n ». Une tuile prend le nom de l'entité sans celui de la pièce (« Lampe du salon » dans « Salon » devient « Lampe »), et l'icône choisie dans les réglages de l'entité, sinon celle de son genre.
- **Personnaliser une tuile** (section repliée « Personnaliser des tuiles ») : un autre nom, une autre icône, ou un comportement — *allumer seulement* (jamais éteint depuis l'écran), *confirmer* (un second appui dans les 3 s), *lecture seule*.
- Le bouton « HA » montre la pièce de la page affichée ; un glissement passe à la pièce suivante qui a des appareils.
- **Le climat d'une pièce** (facultatif, [ADR-0040](../decisions/0040-room-climate.md)) : trois champs de plus dans la section de chaque pièce, vides par défaut — « Température de la pièce · Room temperature » (une sonde de température), « Humidité de la pièce · Room humidity » (une sonde d'humidité) et « Climatisation de la pièce · Room climate » (une clim). Avec une température, la carte clim de l'accueil montre cette pièce quand vous êtes dessus en mode HA : sa température à gauche, son humidité à droite (ou rien), et la tuile − / + règle sa clim. L'appui long sur sa température ouvre son historique. L'humidité et la clim ont besoin de la température. Le firmware et le blueprint doivent l'avoir tous les deux ; l'un sans l'autre garde l'écran d'avant.
- La valeur d'un capteur part avec les autres mesures, toutes les 5 minutes ; les autres appareils partent dès que ce que montre l'écran change.
- **Pièce 1 laissée vide** : l'accueil garde le réglage 3.x (section repliée « Ancien accueil (si la pièce 1 est vide) · Former home page (if room 1 is empty) » : PC ou TV, volet, trois lumières), avec le comportement PC + TV de la tuile PC. Rien à refaire après la mise à jour.
- **Firmware 3.0 ou 3.1** : le blueprint lit la version de la tablette et n'utilise alors que le réglage 3.x ; les pièces apparaissent une fois le firmware mis à jour. Firmware et blueprint se mettent à jour dans n'importe quel ordre.

## Énergie solaire (facultatif)

Une installation solaire a son propre popup ([ADR-0028](../decisions/0028-solar-energy-popup.md)) : solaire, maison, réseau et batterie en direct, et la production en barres par heure (aujourd'hui), par jour (30 jours) et par mois (12 mois).

1. Dans l'automatisation « Tab5 — emplacements », ouvrez les trois sections repliées **« Énergie : solaire · Energy: solar »**, **« Énergie : réseau et maison · Energy: grid and home »** et **« Énergie : batterie · Energy: battery »**, et choisissez ce que vous avez : puissance solaire, énergie solaire produite (le compteur en kWh du tableau Énergie de HA : le total du jour et les barres viennent de ses statistiques), puissance du réseau, consommation de la maison, niveau, puissance et température de la batterie. Tous les champs sont facultatifs. La puissance du réseau est positive quand la maison achète : cochez « Inverser » si votre compteur dit l'inverse, ou donnez la vente dans son propre champ si votre compteur les sépare. La puissance de la batterie est positive en charge (même case « Inverser »). Plusieurs onduleurs ou chaînes de panneaux (pv1, pv2…) : mettez le premier dans « Puissance solaire » / « Énergie solaire produite » et les autres dans « Autres puissances solaires » / « Autres énergies solaires produites » ; ils sont additionnés (un onduleur indisponible la nuit est ignoré, un compteur sans donnée sur une période y compte pour 0). Laissée vide, la consommation de la maison est calculée (solaire + réseau − charge de la batterie). Donnez aussi la **puissance crête** des panneaux (kWc, tous les panneaux) : une icône du bandeau d'état, en haut à gauche, montre la production en part de cette crête par sa couleur (panneau gris la nuit) ; 0 = pas d'icône.
2. Placez l'un de ces capteurs dans une pièce (la puissance solaire, par exemple) : sa tuile montre la valeur et ouvre le popup au toucher. L'option « Énergie » de la liste « Aller à l'écran » de la tablette, dans Home Assistant, l'ouvre aussi.
3. Le package `tab5_energie.yaml` (dans l'archive de l'[étape 1](home-assistant-files.md#version-française)) pousse les valeurs tant que le popup est ouvert. Sans lui, le popup affiche « En attente de Home Assistant ».
4. **La prévision du jour et le bilan** (facultatif, [ADR-0058](../decisions/0058-energy-sun-forecast-balance.md)). Avec le compteur d'énergie solaire choisi, la page « Aujourd'hui » ne demande rien de plus : les heures du soleil viennent de `sun.sun` de HA, la courbe « ciel clair » est apprise sur votre propre production des 30 derniers jours (orientation et ombres comprises ; elle apparaît après environ trois jours de soleil), et la prévision l'abaisse selon les nuages de la météo heure par heure que la tablette utilise déjà. Si vous avez une intégration de prévision (Forecast.Solar, Solcast…), choisissez ses capteurs en kWh dans **« Prévision de production du jour »** et **« … de demain »** : la forme apprise est mise à leur échelle. Pour la page « Bilan », choisissez **« Énergie achetée au réseau »** et **« Énergie vendue au réseau »** (les compteurs du réseau du tableau Énergie de HA), et pour les gains les **prix d'achat et de revente** du kWh (un nombre, ou un capteur de prix qui l'emporte) et la **monnaie** (€ si vide).

![Popup Énergie du Tab5, vue Jours : solaire, maison, réseau et batterie en direct, et la production des 30 derniers jours (rendu de la CI, données de démonstration)](../images/tab5_energie.png)

Section laissée vide : rien ne change à l'écran.

## Historique des températures

Un appui long sur l'une des deux températures de l'accueil ouvre son historique ([ADR-0032](../decisions/0032-temperature-history-popup.md)) : 24 heures, 7 jours ou 30 jours, d'après les statistiques longue durée de Home Assistant, et la prévision de la météo pour la seconde température.

1. Le package `tab5_historique.yaml` (dans l'archive de l'[étape 1](home-assistant-files.md#version-française)) envoie la courbe tant que le popup est ouvert. Sans lui, le popup affiche « En attente de Home Assistant ».
2. Si votre seconde température est **dehors**, cochez « La seconde température est dehors · The second temperature is outdoors » dans l'automatisation « Tab5 — emplacements » (section « Températures · Temperatures ») : la prévision prolonge alors sa courbe. Décochée (une serre), la prévision reste à part, sous « Dehors, prévu ».
3. Le capteur doit avoir des statistiques (un `state_class`, comme les thermomètres) ; la prévision est celle de l'entité météo choisie pour la tablette.

## Autres zones

**Ce que vous n'avez pas disparaît**, avec ses boutons ([ADR-0018](../decisions/0018-optional-zones-confirmed-by-ha.md), [ADR-0019](../decisions/0019-logical-slots-blueprint.md)).

- **Retirer une zone : laissez son emplacement vide.** Un emplacement vide, ou une entité qui n'existe pas, est absent. Une entité qui existe mais est `unavailable` garde sa zone (« -- », « Hors ligne »). **Sans l'automatisation du blueprint, rien ne disparaît** (et aucun de vos appareils ne s'affiche).
- **Une zone manque par erreur ?** Le capteur de diagnostic « Zones masquées » de la tablette liste ce qui a disparu.
- Une zone revient d'elle-même dès que son entité envoie une valeur.

| Zone | Entrée du blueprint | Masqué quand elle est vide |
|---|---|---|
| TV | TV, et Télécommande de la TV pour les touches (Autres télécommandes : jusqu'à trois pages de plus, un Apple TV, un Freebox Player…) | Bouton TV et télécommande ; « HA » et « Sys » glissent d'une colonne |
| Téléphone | Batterie du téléphone | Icône d'état |
| Pièce | Température de la pièce (et Humidité de la pièce) | Sa température |
| Serre | Seconde température (serre) | Sa température ; l'icône devient un carrousel, son tap change toujours la zone à gauche de l'horloge |
| Pots (0 à 5) | Pot 1 à 5 : le capteur d'humidité ; conductivité, éclairement, température et batterie sont pris sur le même appareil | Jusqu'à 4 pots : un emplacement chacun ; à 5 : le résumé « plus secs / médiane / plus humide ». Cartes du popup, recentrées |
| Clim | Climatisation | − / consigne / + et le popup |
| Planning de travail | Agenda de travail | Panneau planning de la carte centrale |
| Mode vocal « Discussion » | aucune : la liste « Tab5 · pipeline de discussion » à « Aucun » (sans la liste, la zone reste) | Boutons Domo / Discu de l'accueil et du popup assistant ; la tablette repasse en Domo |
| Lumières (réglage 3.x) | Lumière 1 à 3 · Light 1 to 3 (section « Ancien accueil ») | Icônes des tuiles 3 à 5, carte du calque « HA », lignes du popup Lumières, « Tout éteindre » |
| PC (réglage 3.x) | PC (un interrupteur l'allume ; un suivi de présence l'affiche seulement) | Icône d'état, carte « PC Bureau » ; la première tuile aussi s'il n'y a pas non plus de TV |
| Volet (réglage 3.x) | Volet (et le package optionnel `volet_serre_tracking.yaml`, `tab5_optionnel/` de l'archive, pour un volet qui ne signale pas sa course) | Icônes de la tuile 2, carte du calque « HA » |

Les lignes « réglage 3.x » (section « Ancien accueil » du blueprint) sont l'accueil d'un firmware 3.0 ou 3.1, et d'un firmware 3.2 tant que la pièce 1 est vide. Les horaires du planning et l'heure du réveil viennent de la liste « Tab5 · agenda de travail » ([étape 5](sources.md#version-française)) ; laissez vide l'« Agenda de travail » du blueprint et il prend le même.

## Limites

- **Icônes** : l'écran en connaît une palette limitée ([icônes des tuiles](../tiles_icons.md#version-française)). Une icône hors palette montre celle de son genre ; en ajouter une demande une ligne dans `Tab5/tuiles_icones.yaml` et une nouvelle version.
- **Noms** : les polices de l'écran ne couvrent que les alphabets latins ; les autres caractères disparaissent, et un nom trop long est coupé.
- **Renommer une entité** : sa tuile suit à la prochaine connexion de la tablette, ou dès que l'automatisation est de nouveau enregistrée.
- **Clim** ([ADR-0026](../decisions/0026-climate-from-device.md)) : toutes marques. Le blueprint envoie les bornes, le pas, l'unité (°C ou °F, celle de votre entité météo) et les modes de l'appareil ; le popup prend son nom pour titre, et un bouton que l'appareil ne sait pas faire disparaît. **Plusieurs appareils** ([ADR-0027](../decisions/0027-climate-per-tile.md)) : placez chacun dans une pièce ; sa tuile ouvre le popup pour lui, avec ses bornes, ses modes et son nom. Un changement fait hors de l'écran (télécommande, application) se voit tout de suite dans le popup pour la consigne et le mode, en 5 minutes au plus pour la ventilation, l'oscillation, le préréglage et la température de la pièce. Avec un firmware plus ancien que le blueprint, une telle tuile montre seulement sa température. L'écran n'a de boutons que pour froid, chaud, sec, ventilation et arrêt, Éco, Boost, Silence, Oscillation et Brise : les autres modes (chaud/froid, auto, vitesses de ventilation, nuit…) restent dans Home Assistant (un appareil en chaud/froid ou auto n'allume aucun bouton de mode). Avec un blueprint plus ancien que le firmware, le popup reste comme avant (16-30 °C, pas de 0,5, tous les boutons).
- **Un volet suivi par `volet_serre_tracking.yaml`** (il ne signale pas sa course) : laissez-le aussi dans l'entrée « Volet » de la section 3.x, même s'il est dans une pièce ; sa tuile montre alors l'état que tient le package, et ses commandes passent par le script du package.
- **Un agenda par rôle** : travail, rendez-vous, anniversaires, jours fériés et vacances scolaires sont les cinq listes « Tab5 · agenda … ».
- **Briefing parlé du matin** : dans la langue de l'écran (français, anglais, allemand, néerlandais, espagnol, italien, turc) ; seul le texte français a été relu.
- Pour voir une maison plus petite sans toucher à la vôtre : [mode démo](../demo_mode.md#maison-minimale-zones-optionnelles), option `--maison-minimale`.
