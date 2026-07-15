# Demo Mode — Try It Without Home Assistant

## English · [Français](#version-française)

---

## Why this exists

The full install (see [`installation.md`](installation.md)) assumes you already run Home Assistant with a weather integration, Google Calendar, a climate entity, BLE plant sensors... That's a lot to set up just to see whether the dashboard is worth the effort.

The firmware is **push-only** ([ADR-0001](decisions/0001-push-only-zero-polling.md)): it never asks Home Assistant for anything, it only reacts to ESPHome native API calls. That means anything that can speak the ESPHome API protocol can drive the screen — including a small script that isn't Home Assistant at all.

`tools/demo/demo_pusher.py` does exactly that: it connects to your flashed Tab5 with [`aioesphomeapi`](https://github.com/esphome/aioesphomeapi) (the same library Home Assistant's own ESPHome integration uses) and pushes rotating synthetic scenes — weather, forecast, climate, planning, plant moisture, weather alerts. No Home Assistant install, no entity mapping, no account created anywhere. Stop the script with `Ctrl+C` and there is nothing left to clean up except the flashed device itself.

## What it does *not* touch

Nothing in `Tab5/*.yaml`, `tab5_custom.cpp/.h`, `Tab5/user_entities.yaml`, `secrets.yaml`, or `HomeAssistant_Config/` is modified by demo mode. It is a standalone script that talks to the same API surface real automations use — purely additive.

## Steps

1. **Flash normally**, but leave `Tab5/user_entities.example.yaml` as-is (copy it to `Tab5/user_entities.yaml` unmodified — you don't need real Home Assistant entities behind these placeholder IDs for the demo). You still need a `secrets.yaml` with your own Wi-Fi credentials and a generated `api_encryption_key` (see [`installation.md`](installation.md) step 3) — that part is required by ESPHome itself, demo mode doesn't change it.
2. **Note the device's IP** once it's on your Wi-Fi (ESPHome dashboard, your router, or `esphome logs tab5-ha-hmi.yaml`).
3. **Install the one dependency** and run the script from your PC (same Wi-Fi network as the device):
   ```bash
   pip install -r tools/demo/requirements.txt
   python tools/demo/demo_pusher.py --host <device-ip> --key <api_encryption_key>
   ```
   If `secrets.yaml` exists at the repo root, `--key` can be omitted — the script reads it directly.
4. **Watch the screen.** Every ~20 seconds it cycles between three scenes (sunny day, rainy day with a weather alert, a rest day with a plant that needs watering) covering all ten `tab5_maj_*` push services plus the plant/light/temperature "mirror" entities.
5. **Stop with `Ctrl+C`.** Nothing persists anywhere outside the device.

Want to check the exact payloads without any hardware or dependency at all:
```bash
python tools/demo/demo_pusher.py --dry-run
```

## Interactive mode (light/climate buttons)

By default, the script also logs when you tap a light, climate, or shutter control on screen (confirms the touch path works), via ESPHome's `subscribe_home_assistant_states_and_services` hook. Disable it with `--no-interactive`.

**Known limitation**: the light popup targets `id(current_light_entity)`, an internal firmware global set by a long-press on a card — it isn't observable over the native API protocol, so the script cannot mirror the exact on-screen light state back after a tap. It logs the button press (proof the touchscreen works) but does not fake a state change. This is a deliberate scope limit, not a bug.

## Reference: what gets pushed

| Service | Demo behavior |
|---|---|
| `tab5_maj_meteo_actuelle`, `_probabilites`, `_previsions_heures_bulk`, `_previsions_jours_bulk` | Full 15-hour / 15-day forecast per scene |
| `tab5_maj_alerte_meteo_france` | 11-field vigilance payload; the rainy scene triggers an Orange alert banner |
| `tab5_maj_pluie_1h` | 9-bar short-term rain chart |
| `tab5_maj_clim`, `_volet_etat`, `_planning`, `_info_texte` | Climate, shutter, and planning cards |
| 13 mirror entities (`platform: homeassistant` in `tab5-sensors-domotique.yaml`) | Lights, room temp/humidity, phone battery, PC tracker, 5 plant moisture sensors (one deliberately low, to show the dynamic sort) |

Source of the exact payload contract: `Tab5/tab5-api-logic.yaml` and `Tab5/tab5_custom.cpp` (parsing rules, field counts, buffer limits) — see comments in `tools/demo/scenarios.py` for the specifics.

---

---

## Version Française

---

## Pourquoi ce mode existe

L'installation complète (voir [`installation.md`](installation.md)) suppose que vous avez déjà Home Assistant avec une intégration météo, Google Calendar, une entité climatisation, des capteurs BLE plantes... Beaucoup de travail juste pour voir si le tableau de bord vaut le coup.

Le firmware est **push-only** ([ADR-0001](decisions/0001-push-only-zero-polling.md)) : il ne demande jamais rien à Home Assistant, il réagit seulement aux appels de l'API native ESPHome. N'importe quel client qui parle ce protocole peut donc piloter l'écran — y compris un petit script qui n'est pas du tout Home Assistant.

`tools/demo/demo_pusher.py` fait exactement ça : il se connecte à votre Tab5 flashé via [`aioesphomeapi`](https://github.com/esphome/aioesphomeapi) (la même librairie que l'intégration ESPHome de Home Assistant) et pousse des scènes synthétiques qui tournent — météo, prévisions, clim, planning, humidité des plantes, alertes météo. Aucune installation Home Assistant, aucun mappage d'entités, aucun compte créé nulle part. Arrêtez le script avec `Ctrl+C` et il ne reste rien à nettoyer à part l'appareil flashé lui-même.

## Ce que ça ne touche pas

Rien dans `Tab5/*.yaml`, `tab5_custom.cpp/.h`, `Tab5/user_entities.yaml`, `secrets.yaml`, ou `HomeAssistant_Config/` n'est modifié par le mode démo. C'est un script autonome qui parle la même API que les vraies automations — purement additif.

## Étapes

1. **Flashez normalement**, mais laissez `Tab5/user_entities.example.yaml` tel quel (copiez-le vers `Tab5/user_entities.yaml` sans le modifier — pas besoin de vraies entités Home Assistant derrière ces IDs placeholder pour la démo). Il vous faut quand même un `secrets.yaml` avec votre Wi-Fi et une `api_encryption_key` générée (voir [`installation.md`](installation.md) étape 3) — ça, c'est exigé par ESPHome lui-même, le mode démo n'y change rien.
2. **Notez l'IP de l'appareil** une fois connecté au Wi-Fi (dashboard ESPHome, votre routeur, ou `esphome logs tab5-ha-hmi.yaml`).
3. **Installez l'unique dépendance** et lancez le script depuis votre PC (même réseau Wi-Fi que l'appareil) :
   ```bash
   pip install -r tools/demo/requirements.txt
   python tools/demo/demo_pusher.py --host <ip-appareil> --key <api_encryption_key>
   ```
   Si `secrets.yaml` existe à la racine du repo, `--key` peut être omis — le script le lit directement.
4. **Regardez l'écran.** Toutes les ~20 secondes, il alterne entre trois scènes (journée ensoleillée, jour de pluie avec alerte météo, jour de repos avec une plante à arroser) qui couvrent les dix services de push `tab5_maj_*` plus les entités "miroir" (plantes, lumières, températures).
5. **Arrêtez avec `Ctrl+C`.** Rien ne persiste nulle part en dehors de l'appareil.

Pour vérifier les payloads exacts sans matériel ni dépendance du tout :
```bash
python tools/demo/demo_pusher.py --dry-run
```

## Mode interactif (boutons lumière/clim)

Par défaut, le script loggue aussi quand vous appuyez sur un contrôle lumière/clim/volet à l'écran (confirme que le tactile fonctionne), via le hook `subscribe_home_assistant_states_and_services` d'ESPHome. Désactivez avec `--no-interactive`.

**Limitation connue** : le popup lumière cible `id(current_light_entity)`, un global interne au firmware réglé par un appui long sur une carte — invisible depuis le protocole natif. Le script loggue l'appui (preuve que le tactile fonctionne) mais ne simule pas de changement d'état à l'écran. C'est une limite de périmètre assumée, pas un bug.

## Référence : ce qui est poussé

| Service | Comportement démo |
|---|---|
| `tab5_maj_meteo_actuelle`, `_probabilites`, `_previsions_heures_bulk`, `_previsions_jours_bulk` | Prévisions complètes 15h / 15 jours par scène |
| `tab5_maj_alerte_meteo_france` | Payload vigilance à 11 champs ; la scène pluie déclenche une bannière d'alerte Orange |
| `tab5_maj_pluie_1h` | Graphe de pluie court terme à 9 barres |
| `tab5_maj_clim`, `_volet_etat`, `_planning`, `_info_texte` | Cartes clim, volet et planning |
| 13 entités miroir (`platform: homeassistant` dans `tab5-sensors-domotique.yaml`) | Lumières, temp/humidité pièces, batterie téléphone, tracker PC, 5 capteurs d'humidité plantes (un volontairement bas, pour montrer le tri dynamique) |

Source du contrat exact des payloads : `Tab5/tab5-api-logic.yaml` et `Tab5/tab5_custom.cpp` (règles de parsing, nombre de champs, limites de buffer) — voir les commentaires de `tools/demo/scenarios.py` pour le détail.
