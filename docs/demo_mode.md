# Demo Mode — Try It Without Home Assistant

## English · [Français](#version-française)

---

## Why this exists

The full install (see [`installation.md`](installation/README.md)) assumes you already run Home Assistant with a weather integration, Google Calendar, a climate entity, BLE plant sensors... That's a lot to set up just to see whether the dashboard is worth the effort.

The firmware is **push-only** ([ADR-0001](decisions/0001-push-only-zero-polling.md)): it never asks Home Assistant for anything, it only reacts to ESPHome native API calls. That means anything that can speak the ESPHome API protocol can drive the screen — including a small script that isn't Home Assistant at all.

`tools/demo/demo_pusher.py` does exactly that: it connects to your flashed Tab5 with [`aioesphomeapi`](https://github.com/esphome/aioesphomeapi) (the same library Home Assistant's own ESPHome integration uses) and pushes rotating synthetic scenes — weather, forecast, climate, planning, plant moisture, weather alerts. No Home Assistant install, no entity mapping, no account created anywhere. Stop the script with `Ctrl+C` and there is nothing left to clean up except the flashed device itself.

## What it does *not* touch

Nothing in `Tab5/paquets/*.yaml`, the C++ of `Tab5/socle|ecran|jeux/`, `Tab5/user_entities.yaml`, or `HomeAssistant_Config/` is modified by demo mode. It is a standalone script that talks to the same API surface real automations use — purely additive.

## Steps

1. **Flash normally**, but leave `Tab5/user_entities.example.yaml` as-is (copy it to `Tab5/user_entities.yaml` unmodified — you don't need real Home Assistant entities behind these placeholder IDs for the demo). You still need the signing key and the Wi-Fi of [`installation.md`](installation/README.md) steps 3 and 5.
2. **Note the device's IP** once it's on your Wi-Fi (your router, or the page of « Tab5 Fallback AP » right after you picked the network).
3. **Install the one dependency** and run the script from your PC (same Wi-Fi network as the device):
   ```bash
   pip install -r tools/demo/requirements.txt
   python tools/demo/demo_pusher.py --host <device-ip>
   ```
   **Encryption key** (3.0, [ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)): a tablet never added to Home Assistant has no key yet. The script gives it one, as HA would, within 30 minutes of the tablet's start (otherwise restart it), and keeps it in `tools/demo/cle_demo.txt` (gitignored): **that is the key to give Home Assistant** when you add the tablet later. On a tablet HA already knows, pass HA's key: `--cle`, the `TAB5_CLE_API` variable, or `--config-ha <HA config folder>` to read it where HA keeps it.
4. **Watch the screen.** Every ~20 seconds it cycles between three scenes (sunny day, rainy day with a weather alert, a rest day with a plant that needs watering), driving nine dashboard push services, the optional-zones answer (`tab5_maj_zones`) and the home's slots (`tab5_maj_emplacements`: lights, temperatures, PC, TV, phone, plants), as the Home Assistant blueprint would (lot 6a) — and, on a 3.2 firmware, the rooms of the bottom tiles (`tab5_maj_tuiles`, see [Rooms](#rooms-32-firmware)), and the solar installation of the Energy popup (`tab5_maj_energie` and `tab5_maj_energie_historique`, [ADR-0028](decisions/0028-solar-energy-popup.md)). The remaining services are out of scope by design — they belong to features a demo can't fake (`tab5_maj_reponse_vocale` / `tab5_assist_reponse` need a voice pipeline, `tab5_maj_calendrier_mois` / `_jour` and `tab5_maj_rdv_prochains` need a real calendar, `tab5_maj_alertes_ha_bulk` needs live HA entities, `tab5_maj_alertes_historique` the alerts memory of Home Assistant, `tab5_maj_lecteur` real media players, `tab5_maj_cameras` real cameras, `tab5_maj_suivi` the statistics of real sensors, `tab5_maj_froid` those of real fridge and freezer thermometers), and `tab5_maj_planning` is obsolete since 2026-09-08 (the tablet derives the planning banner from the daily forecast; kept for compatibility, it will be removed in a future major version).
5. **Stop with `Ctrl+C`.** Nothing persists outside the device, except `tools/demo/cle_demo.txt` for a tablet the demo gave its key to.

Want to check, without any hardware or dependency at all, that every call has exactly the variables the firmware declares (read in `Tab5/paquets/tab5-api-logic.yaml`) and every payload its format (it prints them, and fails on a mismatch):
```bash
python tools/demo/demo_pusher.py --dry-run
```

## Minimal home (optional zones)

A zone whose Home Assistant entity doesn't exist disappears from the screen ([ADR-0018](decisions/0018-optional-zones-confirmed-by-ha.md)). To see what a smaller home looks like:
```bash
python tools/demo/demo_pusher.py --host <device-ip> --maison-minimale
```
The script then behaves like a Home Assistant without a climate unit, TV, phone, third light, greenhouse sensor, plants 3 to 5, shutter or work calendar: it pushes nothing for those slots, answers the tablet's `esphome.tab5_zones` request with their keys, and pushes nothing for the climate and the shutter. What stays: the PC, two lights, the living-room temperature and two plants. The greenhouse spot shows a carousel icon (its tap still switches the area left of the clock). On a 3.2 firmware, the rooms come down to one, without a name (« Pièce 1 » on screen): the PC and the two lamps, where they were in 3.x.

**Restart the tablet when switching from a full demo to this one**: a zone that has already received data stays on screen (data always wins). The other way round needs nothing: a full demo answers "nothing missing" as soon as it connects.

## Rooms (3.2 firmware)

Since 3.2 ([ADR-0023](decisions/0023-rooms-generic-tiles.md)), each page of the five bottom tiles is a room of up to five devices that Home Assistant describes. When the tablet has the `tab5_maj_tuiles` action, the demo pushes its own home, as the blueprint does: the definitions (name, type, icon, options of each tile), then the states, after the slots in `tab5_maj_emplacements` (`tRT|state|value|colour` keys).

| Room | Page (reached from home by) | Devices |
|---|---|---|
| Salon | home, days 0-4 | TV (off; long press: remote), shutter opening, dimmable orange colour lamp, ceiling light, a scene |
| Entrée | days 5-9 (swipe left) | front door (closed), corridor motion (detected), a read-only wall light, a script that asks for a second tap, an offline plug |
| Chambre d'amis à l'étage | days 10-14 (swipe left twice) | bedside lamp, the climate (same mode and temperature as the climate card of the scene), presence (away): three spaced tiles, side by side in HA mode. The name is longer than the 24 bytes the tablet keeps |
| Bureau | next 5 hours (swipe right) | the PC (on, never switched off from the screen) and the solar production (1.45 kW; tap: the Energy popup): two tiles, centred in HA mode |
| Jardin | hours 5-9 (swipe right twice) | awning 60 % open, soil moisture, a speaker playing, a dark indigo string light (its name is cut), outdoor temperature |

What to look at: in weather mode, each page shows its room's devices in the « shoulders » of its tiles; the **HA** button shows the room's cards, and a swipe goes from room to room. On a 3.x firmware (no `tab5_maj_tuiles`), the demo pushes the 3.x slots only, and the screen keeps its five fixed tiles.

## Interactive mode (light/climate buttons)

By default, the script also logs when you tap a light, climate, or shutter control on screen (confirms the touch path works), via ESPHome's `subscribe_home_assistant_states_and_services` hook. On a 3.2 firmware, a tile's command is logged with its room and name, e.g. `t02 (Salon › Lampe d'ambiance, lum) : basculer`. Disable it with `--no-interactive`.

**Known limitation**: the light popup targets `id(current_light_slot)`, an internal firmware global set by a long-press on a card — it isn't observable over the native API protocol, so the script cannot mirror the exact on-screen light state back after a tap. It logs the button press (proof the touchscreen works) but does not fake a state change. This is a deliberate scope limit, not a bug.

## Reference: what gets pushed

| Service | Demo behavior |
|---|---|
| `tab5_maj_meteo_actuelle`, `_probabilites`, `_previsions_heures_bulk`, `_previsions_jours_bulk` | Full 15-hour / 15-day forecast per scene |
| `tab5_maj_alerte_meteo_france` | 11-field vigilance payload; the rainy scene triggers an Orange alert banner |
| `tab5_maj_pluie_1h_bulk` | 9-bar short-term rain chart, one call (`idx|intensity;…`) |
| `tab5_maj_clim`, `_volet_etat`, `_info_texte` | Climate, shutter and info-banner cards (`_info_texte` takes 3 args: `texte`, `couleur`, `meteo_id`). Rain and banner use the lot 4c codes (`@level,start`, `@ha|…`), so the tablet writes them in its own language. The planning banner is derived by the tablet from the daily push, as with Home Assistant |
| `tab5_maj_zones` | Optional zones: empty answer (full home), or the minimal home's keys with `--maison-minimale` |
| `tab5_maj_emplacements` (lot 6a) | The home's slots, as the blueprint pushes them: lights, room temp/humidity, phone battery, PC, TV, 5 plants with their details (one deliberately low, to show the dynamic sort). On a 3.2 firmware, followed by the states of the rooms' tiles (`tRT` keys). The screen's commands (`esphome.tab5_action`) are only logged |
| `tab5_maj_tuiles` (3.2, ADR-0023) | The rooms: 5 rooms, 20 tiles (one room of 3 with `--maison-minimale`), a full snapshot sent before the states. Only when the tablet has this action |
| `tab5_maj_energie`, `_energie_historique` (ADR-0028) | A solar installation: 1.45 kW produced, 620 W for the home, 430 W sold, battery at 64 % and charging; production per hour (today), per day (30 days) and per month (12 months), pushed at every scene and on every request of the tablet (`esphome.tab5_energie`). Only when the tablet has these actions |
| `tab5_maj_historique` (ADR-0032) | The Temperature popup: the curve of the room (around 21 °C) or of the greenhouse (up to 28 °C in the afternoon) over 24 h, 7 days or 30 days, ending on the home-screen value, and, for the greenhouse, the outdoor forecast. Only on a request of the tablet (`esphome.tab5_historique`, a long press on a temperature): the popup ignores an answer it did not ask for. Only when the tablet has this action |

Source of the exact payload contract: `Tab5/paquets/tab5-api-logic.yaml` and `Tab5/ecran/tab5_services.cpp` / `tab5_forecast.cpp` (parsing rules, field counts, buffer limits) — see comments in `tools/demo/scenarios.py` for the specifics.

---

---

## Version Française

---

## Pourquoi ce mode existe

L'installation complète (voir [`installation.md`](installation/README.md)) suppose que vous avez déjà Home Assistant avec une intégration météo, Google Calendar, une entité climatisation, des capteurs BLE plantes... Beaucoup de travail juste pour voir si le tableau de bord vaut le coup.

Le firmware est **push-only** ([ADR-0001](decisions/0001-push-only-zero-polling.md)) : il ne demande jamais rien à Home Assistant, il réagit seulement aux appels de l'API native ESPHome. N'importe quel client qui parle ce protocole peut donc piloter l'écran — y compris un petit script qui n'est pas du tout Home Assistant.

`tools/demo/demo_pusher.py` fait exactement ça : il se connecte à votre Tab5 flashé via [`aioesphomeapi`](https://github.com/esphome/aioesphomeapi) (la même librairie que l'intégration ESPHome de Home Assistant) et pousse des scènes synthétiques qui tournent — météo, prévisions, clim, planning, humidité des plantes, alertes météo. Aucune installation Home Assistant, aucun mappage d'entités, aucun compte créé nulle part. Arrêtez le script avec `Ctrl+C` et il ne reste rien à nettoyer à part l'appareil flashé lui-même.

## Ce que ça ne touche pas

Rien dans `Tab5/paquets/*.yaml`, le C++ de `Tab5/socle|ecran|jeux/`, `Tab5/user_entities.yaml` ou `HomeAssistant_Config/` n'est modifié par le mode démo. C'est un script autonome qui parle la même API que les vraies automations — purement additif.

## Étapes

1. **Flashez normalement**, mais laissez `Tab5/user_entities.example.yaml` tel quel (copiez-le vers `Tab5/user_entities.yaml` sans le modifier — pas besoin de vraies entités Home Assistant derrière ces IDs placeholder pour la démo). Il vous faut quand même la clé de signature et le Wi-Fi des étapes 3 et 5 d'[`installation.md`](installation/README.md#version-française).
2. **Notez l'IP de l'appareil** une fois connecté au Wi-Fi (votre routeur, ou la page de « Tab5 Fallback AP » juste après le choix du réseau).
3. **Installez l'unique dépendance** et lancez le script depuis votre PC (même réseau Wi-Fi que l'appareil) :
   ```bash
   pip install -r tools/demo/requirements.txt
   python tools/demo/demo_pusher.py --host <ip-appareil>
   ```
   **Clé de chiffrement** (3.0, [ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)) : une tablette jamais ajoutée à Home Assistant n'a pas encore de clé. Le script lui en donne une, comme HA le ferait, dans les 30 minutes qui suivent son démarrage (sinon redémarrez-la), et la garde dans `tools/demo/cle_demo.txt` (gitignoré) : **c'est la clé à donner à Home Assistant** quand vous y ajouterez la tablette. Sur une tablette que HA connaît déjà, passez la clé de HA : `--cle`, la variable `TAB5_CLE_API`, ou `--config-ha <dossier de configuration de HA>` pour la lire là où HA la garde.
4. **Regardez l'écran.** Toutes les ~20 secondes, il alterne entre trois scènes (journée ensoleillée, jour de pluie avec alerte météo, jour de repos avec une plante à arroser), qui pilotent neuf services de push du dashboard, la réponse des zones optionnelles (`tab5_maj_zones`) et les emplacements de la maison (`tab5_maj_emplacements` : lumières, températures, PC, TV, téléphone, plantes), comme le ferait le blueprint Home Assistant (lot 6a) — et, avec un firmware 3.2, les pièces des tuiles du bas (`tab5_maj_tuiles`, voir [Pièces](#pièces-firmware-32)), et l'installation solaire du popup Énergie (`tab5_maj_energie` et `tab5_maj_energie_historique`, [ADR-0028](decisions/0028-solar-energy-popup.md)). Les services restants sont hors périmètre par choix : ils relèvent de fonctions qu'une démo ne peut pas simuler (`tab5_maj_reponse_vocale` / `tab5_assist_reponse` demandent un pipeline vocal, `tab5_maj_calendrier_mois` / `_jour` et `tab5_maj_rdv_prochains` un vrai calendrier, `tab5_maj_alertes_ha_bulk` des entités HA vivantes, `tab5_maj_alertes_historique` la mémoire des alertes de Home Assistant, `tab5_maj_lecteur` de vrais lecteurs multimédias, `tab5_maj_cameras` de vraies caméras, `tab5_maj_suivi` les statistiques de vrais capteurs, `tab5_maj_froid` celles de vrais thermomètres de réfrigérateur et de congélateur), et `tab5_maj_planning` est obsolète depuis le 08/09/2026 (la tablette dérive le bandeau planning des prévisions journalières ; gardé pour compatibilité, il sera retiré dans une future version majeure).
5. **Arrêtez avec `Ctrl+C`.** Rien ne persiste en dehors de l'appareil, sauf `tools/demo/cle_demo.txt` pour une tablette à qui la démo a donné sa clé.

Pour vérifier, sans matériel ni dépendance du tout, que chaque appel a exactement les variables déclarées par le firmware (lues dans `Tab5/paquets/tab5-api-logic.yaml`) et chaque payload son format (il les affiche, et échoue sur un écart) :
```bash
python tools/demo/demo_pusher.py --dry-run
```

## Maison minimale (zones optionnelles)

Une zone dont l'entité Home Assistant n'existe pas disparaît de l'écran ([ADR-0018](decisions/0018-optional-zones-confirmed-by-ha.md)). Pour voir à quoi ressemble une maison plus petite :
```bash
python tools/demo/demo_pusher.py --host <ip-appareil> --maison-minimale
```
Le script se comporte alors comme un Home Assistant sans clim, TV, téléphone, troisième lumière, capteur de serre, pots 3 à 5, volet ni agenda de travail :
- il ne pousse rien pour ces emplacements ;
- il répond à la demande `esphome.tab5_zones` de la tablette avec leurs clés ;
- il ne pousse rien pour la clim ni le volet.

Il reste le PC, deux lumières, la température du salon et deux pots. À la place de la serre, une icône de carrousel : son tap change toujours la zone à gauche de l'horloge. Avec un firmware 3.2, il ne reste qu'une pièce, sans nom (« Pièce 1 » à l'écran) : le PC et les deux lampes, à leur place de la 3.x.

**Redémarrez la tablette en passant d'une démo complète à celle-ci** : une zone qui a déjà reçu une donnée reste à l'écran (la donnée l'emporte). Dans l'autre sens, rien à faire : une démo complète répond « rien ne manque » dès sa connexion.

## Pièces (firmware 3.2)

Depuis la 3.2 ([ADR-0023](decisions/0023-rooms-generic-tiles.md)), chaque page des cinq tuiles du bas est une pièce de cinq appareils au plus, que Home Assistant décrit. Quand la tablette a l'action `tab5_maj_tuiles`, la démo pousse sa propre maison, comme le blueprint : les définitions (nom, type, icône, options de chaque tuile), puis les états, à la suite des emplacements dans `tab5_maj_emplacements` (clés `tRT|état|valeur|couleur`).

| Pièce | Page (atteinte depuis l'accueil par) | Appareils |
|---|---|---|
| Salon | l'accueil, jours 0-4 | TV (éteinte ; appui long : télécommande), volet qui s'ouvre, lampe couleur orange à variateur, plafonnier, une scène |
| Entrée | jours 5-9 (glisser à gauche) | porte d'entrée (fermée), mouvement du couloir (détecté), une applique en lecture seule, un script qui demande un second appui, une prise hors ligne |
| Chambre d'amis à l'étage | jours 10-14 (deux fois à gauche) | chevet, la clim (même mode et même température que la carte clim de la scène), présence (absent) : trois tuiles espacées, côte à côte en mode HA. Le nom dépasse les 24 octets que garde la tablette |
| Bureau | 5 prochaines heures (glisser à droite) | le PC (allumé, jamais éteint depuis l'écran) et la production solaire (1,45 kW ; au toucher : le popup Énergie) : deux tuiles, centrées en mode HA |
| Jardin | heures 5-9 (deux fois à droite) | store ouvert à 60 %, humidité du sol, une enceinte en lecture, une guirlande indigo foncé (son nom est coupé), température extérieure |

À regarder : en mode météo, chaque page montre les appareils de sa pièce dans les « épaules » de ses tuiles ; le bouton **HA** montre les cartes de la pièce, et un glissement passe de pièce en pièce. Avec un firmware 3.x (sans `tab5_maj_tuiles`), la démo ne pousse que les emplacements 3.x, et l'écran garde ses cinq tuiles fixes.

## Mode interactif (boutons lumière/clim)

Par défaut, le script loggue aussi quand vous appuyez sur un contrôle lumière/clim/volet à l'écran (confirme que le tactile fonctionne), via le hook `subscribe_home_assistant_states_and_services` d'ESPHome. Avec un firmware 3.2, la commande d'une tuile est journalisée avec sa pièce et son nom, par exemple `t02 (Salon › Lampe d'ambiance, lum) : basculer`. Désactivez avec `--no-interactive`.

**Limitation connue** : le popup lumière cible `id(current_light_slot)`, un global interne au firmware réglé par un appui long sur une carte — invisible depuis le protocole natif. Le script loggue l'appui (preuve que le tactile fonctionne) mais ne simule pas de changement d'état à l'écran. C'est une limite de périmètre assumée, pas un bug.

## Référence : ce qui est poussé

| Service | Comportement démo |
|---|---|
| `tab5_maj_meteo_actuelle`, `_probabilites`, `_previsions_heures_bulk`, `_previsions_jours_bulk` | Prévisions complètes 15h / 15 jours par scène |
| `tab5_maj_alerte_meteo_france` | Payload vigilance à 11 champs ; la scène pluie déclenche une bannière d'alerte Orange |
| `tab5_maj_pluie_1h_bulk` | Graphe de pluie court terme à 9 barres, un seul appel (`idx|intensité;…`) |
| `tab5_maj_clim`, `_volet_etat`, `_info_texte` | Cartes clim et volet, bandeau info (`_info_texte` prend 3 arguments : `texte`, `couleur`, `meteo_id`). Pluie et bandeau passent par les codes du lot 4c (`@niveau,début`, `@ha|…`) : la tablette les écrit dans sa langue. Le bandeau planning est dérivé par la tablette de la poussée des jours, comme avec Home Assistant |
| `tab5_maj_zones` | Zones optionnelles : réponse vide (maison complète), ou les clés de la maison minimale avec `--maison-minimale` |
| `tab5_maj_emplacements` (lot 6a) | Les emplacements de la maison, comme le blueprint les pousse : lumières, temp/humidité de la pièce, batterie du téléphone, PC, TV, 5 pots avec leurs détails (un volontairement bas, pour montrer le tri dynamique). Avec un firmware 3.2, suivis des états des tuiles des pièces (clés `tRT`). Les commandes de l'écran (`esphome.tab5_action`) sont seulement journalisées |
| `tab5_maj_tuiles` (3.2, ADR-0023) | Les pièces : 5 pièces, 20 tuiles (une pièce de 3 avec `--maison-minimale`), instantané complet envoyé avant les états. Seulement si la tablette a cette action |
| `tab5_maj_energie`, `_energie_historique` (ADR-0028) | Une installation solaire : 1,45 kW produits, 620 W pour la maison, 430 W vendus, batterie à 64 % qui charge ; production par heure (aujourd'hui), par jour (30 jours) et par mois (12 mois), poussée à chaque scène et à chaque demande de la tablette (`esphome.tab5_energie`). Seulement si la tablette a ces actions |
| `tab5_maj_historique` (ADR-0032) | Le popup Température : la courbe de la pièce (autour de 21 °C) ou de la serre (jusqu'à 28 °C l'après-midi) sur 24 h, 7 jours ou 30 jours, finie sur la valeur de l'accueil, et, pour la serre, la prévision de dehors. Seulement à la demande de la tablette (`esphome.tab5_historique`, un appui long sur une température) : le popup ignore une réponse qu'il n'a pas demandée. Seulement si la tablette a cette action |

Source du contrat exact des payloads : `Tab5/paquets/tab5-api-logic.yaml` et `Tab5/ecran/tab5_services.cpp` / `tab5_forecast.cpp` (règles de parsing, nombre de champs, limites de buffer) — voir les commentaires de `tools/demo/scenarios.py` pour le détail.
