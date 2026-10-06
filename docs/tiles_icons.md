# Tile icons

## English · [Français](#version-française)

---

Since the « rooms » version ([ADR-0023](decisions/0023-rooms-generic-tiles.md)), each device placed in a room is drawn with an icon from a fixed **palette**. The tablet can only draw the icons compiled into its fonts, so it cannot show any Material Design icon: the palette below is what it knows. Most icons have two shapes, one when the device is off, closed or idle, one when it is on, open, detected, present or unlocked; the colour follows the state as well.

## How the icon of a tile is chosen

The blueprint « Tab5 — emplacements » picks, for each tile, the first of:

1. the icon chosen in the blueprint's customisation (`personnalisation`, field `icone`);
2. the entity's own icon, if you set one in Home Assistant (entity settings → *Icon*);
3. the default of its device class (`cover.garage` → `garage`, `binary_sensor.door` → `porte`…);
4. the default of its domain (`light` → `ampoule`, `sensor` → `mesure`…).

A `mdi:` icon outside the palette falls back to the default of its domain. The icon Home Assistant draws by itself when you did not choose one (integration icons) cannot be read by a template: only an icon you set counts.

## Asking for an icon

Adding an icon is one line in [`Tab5/tuiles_icones.yaml`](../Tab5/tuiles_icones.yaml) and a new firmware release (the glyphs are compiled into the firmware, about 0.5 kB per size). If one is missing for your devices, open a [feature request](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/issues/new?template=feature_request.yml) with the `mdi:` name you use in Home Assistant and the device it stands for. Code points are checked against the `meta.json` of the same Material Design Icons version as the font (7.4.47) — see the header of `Tab5/tuiles_icones.yaml` and `tools/gen_tuiles_icones.py`.

## The palette

Generated from `Tab5/tuiles_icones.yaml` by `tools/gen_tuiles_icones.py` (do not edit the table by hand). « Off / on » are Material Design icon names; one name when the icon does not change.

<!-- >>> palette en (tools/gen_tuiles_icones.py) -->
| Code | Off / on | Stands for (`mdi:` icons) | Default for |
|---|---|---|---|
| `ampoule` | lightbulb / lightbulb-on | lightbulb, lightbulb-outline, lightbulb-on, lightbulb-on-outline, lightbulb-off, lightbulb-off-outline, lightbulb-variant, lightbulb-variant-outline, lightbulb-night, lightbulb-spot, lightbulb-group, lightbulb-group-outline, lightbulb-multiple, lightbulb-multiple-outline | type `lum`, `light`, `binary_sensor.light` |
| `plafonnier` | ceiling-light | ceiling-light, ceiling-light-outline, ceiling-light-multiple, ceiling-light-multiple-outline, light-recessed, ceiling-fan-light |  |
| `lampadaire` | floor-lamp | floor-lamp, floor-lamp-outline, floor-lamp-dual, floor-lamp-dual-outline, floor-lamp-torchiere, floor-lamp-torchiere-outline, floor-lamp-torchiere-variant, floor-lamp-torchiere-variant-outline |  |
| `lampe` | lamp | lamp, lamp-outline, lamps, lamps-outline |  |
| `lampe_bureau` | desk-lamp / desk-lamp-on | desk-lamp, desk-lamp-on, desk-lamp-off |  |
| `led` | led-strip-variant | led-strip-variant, led-strip-variant-off, led-strip, led-on, led-off, led-outline, led-variant-on, led-variant-off |  |
| `guirlande` | string-lights-off / string-lights | string-lights, string-lights-off |  |
| `applique` | wall-sconce | wall-sconce, wall-sconce-outline, wall-sconce-flat, wall-sconce-flat-outline, wall-sconce-flat-variant, wall-sconce-flat-variant-outline, wall-sconce-round, wall-sconce-round-outline, wall-sconce-round-variant, wall-sconce-round-variant-outline |  |
| `lustre` | chandelier | chandelier |  |
| `lit` | bed | bed, bed-outline, bed-empty, bed-double, bed-double-outline, bed-king, bed-queen, bed-single, bed-single-outline |  |
| `canape` | sofa | sofa, sofa-outline, sofa-single, sofa-single-outline |  |
| `prise` | power-plug-off / power-plug | power-plug, power-plug-off, power-plug-outline, power-plug-off-outline, power-socket, power-socket-eu, power-socket-fr, power-socket-de, power-socket-uk, power-socket-us, power-socket-it, power-socket-ch | `switch.outlet`, `binary_sensor.plug` |
| `interrupteur` | toggle-switch-variant-off / toggle-switch-variant | toggle-switch-variant, toggle-switch-variant-off, toggle-switch, toggle-switch-off, toggle-switch-outline, toggle-switch-off-outline, light-switch, light-switch-off, electric-switch, electric-switch-closed | type `int`, `switch`, `switch.switch`, `input_boolean`, `automation` |
| `ordinateur` | monitor | monitor, monitor-off, monitor-shimmer, desktop-classic, desktop-tower, desktop-tower-monitor, laptop, laptop-off |  |
| `tv` | television-off / television | television, television-off, television-classic, television-classic-off, television-box, television-ambient-light, cast, cast-off, cast-connected, cast-variant | type `med`, `media_player`, `media_player.tv` |
| `enceinte` | speaker-off / speaker | speaker, speaker-off, speaker-wireless, speaker-multiple, speaker-bluetooth, cast-audio, cast-audio-variant, soundbar, audio-video, audio-video-off | `media_player.speaker`, `media_player.receiver` |
| `console` | controller-off / controller | controller, controller-off, controller-classic, controller-classic-outline, gamepad, gamepad-variant, gamepad-variant-outline |  |
| `tablette` | tablet | tablet, tablet-dashboard, tablet-cellphone |  |
| `cafetiere` | coffee-maker | coffee-maker, coffee-maker-outline, coffee-maker-check, coffee-maker-check-outline, coffee, coffee-outline, coffee-off, coffee-off-outline, kettle, kettle-outline |  |
| `lave_linge` | washing-machine-off / washing-machine | washing-machine, washing-machine-off, washing-machine-alert, tumble-dryer, tumble-dryer-off |  |
| `lave_vaisselle` | dishwasher-off / dishwasher | dishwasher, dishwasher-off, dishwasher-alert |  |
| `aspirateur` | robot-vacuum-off / robot-vacuum | robot-vacuum, robot-vacuum-off, robot-vacuum-variant, robot-vacuum-variant-off, robot-vacuum-alert, vacuum, vacuum-outline |  |
| `ventilateur` | fan-off / fan | fan, fan-off, fan-auto, ceiling-fan, air-purifier, air-purifier-off | `fan` |
| `clim` | air-conditioner | air-conditioner, hvac, hvac-off, thermostat, thermostat-box, thermostat-auto, home-thermometer, home-thermometer-outline | type `cli`, `climate` |
| `radiateur` | radiator-off / radiator | radiator, radiator-off, radiator-disabled, heat-wave, fireplace, fireplace-off |  |
| `chauffe_eau` | water-boiler-off / water-boiler | water-boiler, water-boiler-off, water-boiler-auto |  |
| `pompe` | pump-off / pump | pump, pump-off, water-pump, water-pump-off |  |
| `arrosage` | sprinkler-variant | sprinkler, sprinkler-variant, watering-can, watering-can-outline |  |
| `vanne` | valve-closed / valve-open | valve, valve-open, valve-closed, pipe-valve | `valve`, `valve.water`, `valve.gas` |
| `humidificateur` | air-humidifier-off / air-humidifier | air-humidifier, air-humidifier-off | `humidifier` |
| `volet` | window-shutter / window-shutter-open | window-shutter, window-shutter-open, window-shutter-alert, window-shutter-auto, window-shutter-settings, window-shutter-cog | type `vol`, `cover`, `cover.shutter` |
| `rideau` | curtains-closed / curtains | curtains, curtains-closed | `cover.curtain` |
| `store` | blinds / blinds-open | blinds, blinds-open, blinds-horizontal, blinds-horizontal-closed, blinds-vertical, blinds-vertical-closed, roller-shade, roller-shade-closed, awning, awning-outline | `cover.blind`, `cover.shade`, `cover.awning` |
| `garage` | garage / garage-open | garage, garage-open, garage-variant, garage-open-variant | `cover.garage`, `binary_sensor.garage_door` |
| `portail` | gate / gate-open | gate, gate-open, gate-arrow-right | `cover.gate` |
| `porte` | door-closed / door-open | door, door-closed, door-open, door-sliding, door-sliding-open | `cover.door`, `binary_sensor.door`, `binary_sensor.opening` |
| `fenetre` | window-closed / window-open | window-closed, window-open, window-closed-variant, window-open-variant | `cover.window`, `binary_sensor.window` |
| `serrure` | lock / lock-open | lock, lock-open, lock-outline, lock-open-outline, lock-open-variant, lock-open-variant-outline, lock-smart | `lock`, `binary_sensor.lock` |
| `thermometre` | thermometer | thermometer, thermometer-lines, thermometer-auto | `sensor.temperature` |
| `humidite` | water-percent | water-percent, water-percent-alert, water-outline | `sensor.humidity`, `binary_sensor.moisture` |
| `batterie` | battery | battery, battery-outline, battery-off, battery-alert, battery-high, battery-medium, battery-low, battery-charging | `sensor.battery`, `binary_sensor.battery` |
| `mouvement` | motion-sensor-off / motion-sensor | motion-sensor, motion-sensor-off, run, run-fast, walk | `binary_sensor.motion`, `binary_sensor.moving` |
| `presence` | home-outline / home | home, home-outline, home-account, account, account-outline, account-off, account-off-outline | `person`, `device_tracker`, `binary_sensor.presence`, `binary_sensor.occupancy` |
| `energie` | flash | flash, flash-outline, flash-off, lightning-bolt, lightning-bolt-outline, meter-electric, meter-electric-outline, transmission-tower, home-lightning-bolt, home-lightning-bolt-outline | `sensor.power`, `sensor.energy`, `sensor.voltage`, `sensor.current`, `sensor.apparent_power`, `binary_sensor.power` |
| `solaire` | solar-power | solar-power, solar-power-variant, solar-panel |  |
| `plante` | flower | flower, flower-outline, sprout, sprout-outline, leaf, cactus | `sensor.moisture` |
| `co2` | molecule-co2 | molecule-co2, molecule-co | `sensor.carbon_dioxide` |
| `fumee` | smoke-detector / smoke-detector-alert | smoke-detector, smoke-detector-alert, smoke-detector-variant, smoke-detector-variant-alert, smoke-detector-off, fire-alert | `binary_sensor.smoke`, `binary_sensor.gas`, `binary_sensor.carbon_monoxide` |
| `mesure` | gauge | gauge, gauge-empty, gauge-full, eye, eye-outline, ray-vertex | type `cap`, `sensor`, `number`, `input_number` |
| `etat` | checkbox-blank-circle-outline / checkbox-marked-circle | checkbox-blank-circle-outline, checkbox-marked-circle, checkbox-marked-circle-outline, radiobox-marked | type `bin`, `binary_sensor` |
| `scene` | palette | palette, palette-outline | `scene` |
| `script` | script-text | script, script-text, script-text-outline, script-text-play, script-text-play-outline | `script` |
| `bouton` | gesture-tap-button | gesture-tap-button, gesture-tap, button-pointer, button-cursor | type `act`, `button`, `input_button` |
<!-- <<< palette en -->

---

## Version Française

Depuis la version « pièces » ([ADR-0023](decisions/0023-rooms-generic-tiles.md)), chaque appareil placé dans une pièce est dessiné avec une icône d'une **palette** fixe. La tablette ne sait dessiner que les icônes compilées dans ses polices : elle ne peut pas montrer n'importe quelle icône Material Design, la palette ci-dessous est ce qu'elle connaît. La plupart des icônes ont deux formes, l'une quand l'appareil est éteint, fermé ou au repos, l'autre quand il est allumé, ouvert, détecté, présent ou déverrouillé ; la couleur suit aussi l'état.

## Comment l'icône d'une tuile est choisie

Le blueprint « Tab5 — emplacements » prend, pour chaque tuile, la première de :

1. l'icône choisie dans la personnalisation du blueprint (`personnalisation`, champ `icone`) ;
2. l'icône propre de l'entité, si vous en avez choisi une dans Home Assistant (paramètres de l'entité → *Icône*) ;
3. le défaut de sa classe (`cover.garage` → `garage`, `binary_sensor.door` → `porte`…) ;
4. le défaut de son domaine (`light` → `ampoule`, `sensor` → `mesure`…).

Une icône `mdi:` hors de la palette retombe sur le défaut de son domaine. L'icône que Home Assistant dessine de lui-même quand vous n'en avez pas choisi (icônes des intégrations) ne se lit pas depuis un template : seule compte une icône que vous avez posée.

## Demander une icône

Ajouter une icône, c'est une ligne dans [`Tab5/tuiles_icones.yaml`](../Tab5/tuiles_icones.yaml) et une nouvelle version du firmware (les glyphes sont compilés dans le firmware, environ 0,5 Ko par taille). S'il en manque une pour vos appareils, ouvrez une [demande de fonctionnalité](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/issues/new?template=feature_request.yml) avec le nom `mdi:` que vous utilisez dans Home Assistant et l'appareil qu'il représente. Les points de code sont vérifiés contre le `meta.json` de la même version de Material Design Icons que la police (7.4.47) : voir l'en-tête de `Tab5/tuiles_icones.yaml` et `tools/gen_tuiles_icones.py`.

## La palette

Générée depuis `Tab5/tuiles_icones.yaml` par `tools/gen_tuiles_icones.py` (ne pas éditer le tableau à la main). « Éteint / allumé » sont des noms d'icônes Material Design ; un seul nom quand l'icône ne change pas.

<!-- >>> palette fr (tools/gen_tuiles_icones.py) -->
| Code | Éteint / allumé | Représente (icônes `mdi:`) | Défaut de |
|---|---|---|---|
| `ampoule` | lightbulb / lightbulb-on | lightbulb, lightbulb-outline, lightbulb-on, lightbulb-on-outline, lightbulb-off, lightbulb-off-outline, lightbulb-variant, lightbulb-variant-outline, lightbulb-night, lightbulb-spot, lightbulb-group, lightbulb-group-outline, lightbulb-multiple, lightbulb-multiple-outline | type `lum`, `light`, `binary_sensor.light` |
| `plafonnier` | ceiling-light | ceiling-light, ceiling-light-outline, ceiling-light-multiple, ceiling-light-multiple-outline, light-recessed, ceiling-fan-light |  |
| `lampadaire` | floor-lamp | floor-lamp, floor-lamp-outline, floor-lamp-dual, floor-lamp-dual-outline, floor-lamp-torchiere, floor-lamp-torchiere-outline, floor-lamp-torchiere-variant, floor-lamp-torchiere-variant-outline |  |
| `lampe` | lamp | lamp, lamp-outline, lamps, lamps-outline |  |
| `lampe_bureau` | desk-lamp / desk-lamp-on | desk-lamp, desk-lamp-on, desk-lamp-off |  |
| `led` | led-strip-variant | led-strip-variant, led-strip-variant-off, led-strip, led-on, led-off, led-outline, led-variant-on, led-variant-off |  |
| `guirlande` | string-lights-off / string-lights | string-lights, string-lights-off |  |
| `applique` | wall-sconce | wall-sconce, wall-sconce-outline, wall-sconce-flat, wall-sconce-flat-outline, wall-sconce-flat-variant, wall-sconce-flat-variant-outline, wall-sconce-round, wall-sconce-round-outline, wall-sconce-round-variant, wall-sconce-round-variant-outline |  |
| `lustre` | chandelier | chandelier |  |
| `lit` | bed | bed, bed-outline, bed-empty, bed-double, bed-double-outline, bed-king, bed-queen, bed-single, bed-single-outline |  |
| `canape` | sofa | sofa, sofa-outline, sofa-single, sofa-single-outline |  |
| `prise` | power-plug-off / power-plug | power-plug, power-plug-off, power-plug-outline, power-plug-off-outline, power-socket, power-socket-eu, power-socket-fr, power-socket-de, power-socket-uk, power-socket-us, power-socket-it, power-socket-ch | `switch.outlet`, `binary_sensor.plug` |
| `interrupteur` | toggle-switch-variant-off / toggle-switch-variant | toggle-switch-variant, toggle-switch-variant-off, toggle-switch, toggle-switch-off, toggle-switch-outline, toggle-switch-off-outline, light-switch, light-switch-off, electric-switch, electric-switch-closed | type `int`, `switch`, `switch.switch`, `input_boolean`, `automation` |
| `ordinateur` | monitor | monitor, monitor-off, monitor-shimmer, desktop-classic, desktop-tower, desktop-tower-monitor, laptop, laptop-off |  |
| `tv` | television-off / television | television, television-off, television-classic, television-classic-off, television-box, television-ambient-light, cast, cast-off, cast-connected, cast-variant | type `med`, `media_player`, `media_player.tv` |
| `enceinte` | speaker-off / speaker | speaker, speaker-off, speaker-wireless, speaker-multiple, speaker-bluetooth, cast-audio, cast-audio-variant, soundbar, audio-video, audio-video-off | `media_player.speaker`, `media_player.receiver` |
| `console` | controller-off / controller | controller, controller-off, controller-classic, controller-classic-outline, gamepad, gamepad-variant, gamepad-variant-outline |  |
| `tablette` | tablet | tablet, tablet-dashboard, tablet-cellphone |  |
| `cafetiere` | coffee-maker | coffee-maker, coffee-maker-outline, coffee-maker-check, coffee-maker-check-outline, coffee, coffee-outline, coffee-off, coffee-off-outline, kettle, kettle-outline |  |
| `lave_linge` | washing-machine-off / washing-machine | washing-machine, washing-machine-off, washing-machine-alert, tumble-dryer, tumble-dryer-off |  |
| `lave_vaisselle` | dishwasher-off / dishwasher | dishwasher, dishwasher-off, dishwasher-alert |  |
| `aspirateur` | robot-vacuum-off / robot-vacuum | robot-vacuum, robot-vacuum-off, robot-vacuum-variant, robot-vacuum-variant-off, robot-vacuum-alert, vacuum, vacuum-outline |  |
| `ventilateur` | fan-off / fan | fan, fan-off, fan-auto, ceiling-fan, air-purifier, air-purifier-off | `fan` |
| `clim` | air-conditioner | air-conditioner, hvac, hvac-off, thermostat, thermostat-box, thermostat-auto, home-thermometer, home-thermometer-outline | type `cli`, `climate` |
| `radiateur` | radiator-off / radiator | radiator, radiator-off, radiator-disabled, heat-wave, fireplace, fireplace-off |  |
| `chauffe_eau` | water-boiler-off / water-boiler | water-boiler, water-boiler-off, water-boiler-auto |  |
| `pompe` | pump-off / pump | pump, pump-off, water-pump, water-pump-off |  |
| `arrosage` | sprinkler-variant | sprinkler, sprinkler-variant, watering-can, watering-can-outline |  |
| `vanne` | valve-closed / valve-open | valve, valve-open, valve-closed, pipe-valve | `valve`, `valve.water`, `valve.gas` |
| `humidificateur` | air-humidifier-off / air-humidifier | air-humidifier, air-humidifier-off | `humidifier` |
| `volet` | window-shutter / window-shutter-open | window-shutter, window-shutter-open, window-shutter-alert, window-shutter-auto, window-shutter-settings, window-shutter-cog | type `vol`, `cover`, `cover.shutter` |
| `rideau` | curtains-closed / curtains | curtains, curtains-closed | `cover.curtain` |
| `store` | blinds / blinds-open | blinds, blinds-open, blinds-horizontal, blinds-horizontal-closed, blinds-vertical, blinds-vertical-closed, roller-shade, roller-shade-closed, awning, awning-outline | `cover.blind`, `cover.shade`, `cover.awning` |
| `garage` | garage / garage-open | garage, garage-open, garage-variant, garage-open-variant | `cover.garage`, `binary_sensor.garage_door` |
| `portail` | gate / gate-open | gate, gate-open, gate-arrow-right | `cover.gate` |
| `porte` | door-closed / door-open | door, door-closed, door-open, door-sliding, door-sliding-open | `cover.door`, `binary_sensor.door`, `binary_sensor.opening` |
| `fenetre` | window-closed / window-open | window-closed, window-open, window-closed-variant, window-open-variant | `cover.window`, `binary_sensor.window` |
| `serrure` | lock / lock-open | lock, lock-open, lock-outline, lock-open-outline, lock-open-variant, lock-open-variant-outline, lock-smart | `lock`, `binary_sensor.lock` |
| `thermometre` | thermometer | thermometer, thermometer-lines, thermometer-auto | `sensor.temperature` |
| `humidite` | water-percent | water-percent, water-percent-alert, water-outline | `sensor.humidity`, `binary_sensor.moisture` |
| `batterie` | battery | battery, battery-outline, battery-off, battery-alert, battery-high, battery-medium, battery-low, battery-charging | `sensor.battery`, `binary_sensor.battery` |
| `mouvement` | motion-sensor-off / motion-sensor | motion-sensor, motion-sensor-off, run, run-fast, walk | `binary_sensor.motion`, `binary_sensor.moving` |
| `presence` | home-outline / home | home, home-outline, home-account, account, account-outline, account-off, account-off-outline | `person`, `device_tracker`, `binary_sensor.presence`, `binary_sensor.occupancy` |
| `energie` | flash | flash, flash-outline, flash-off, lightning-bolt, lightning-bolt-outline, meter-electric, meter-electric-outline, transmission-tower, home-lightning-bolt, home-lightning-bolt-outline | `sensor.power`, `sensor.energy`, `sensor.voltage`, `sensor.current`, `sensor.apparent_power`, `binary_sensor.power` |
| `solaire` | solar-power | solar-power, solar-power-variant, solar-panel |  |
| `plante` | flower | flower, flower-outline, sprout, sprout-outline, leaf, cactus | `sensor.moisture` |
| `co2` | molecule-co2 | molecule-co2, molecule-co | `sensor.carbon_dioxide` |
| `fumee` | smoke-detector / smoke-detector-alert | smoke-detector, smoke-detector-alert, smoke-detector-variant, smoke-detector-variant-alert, smoke-detector-off, fire-alert | `binary_sensor.smoke`, `binary_sensor.gas`, `binary_sensor.carbon_monoxide` |
| `mesure` | gauge | gauge, gauge-empty, gauge-full, eye, eye-outline, ray-vertex | type `cap`, `sensor`, `number`, `input_number` |
| `etat` | checkbox-blank-circle-outline / checkbox-marked-circle | checkbox-blank-circle-outline, checkbox-marked-circle, checkbox-marked-circle-outline, radiobox-marked | type `bin`, `binary_sensor` |
| `scene` | palette | palette, palette-outline | `scene` |
| `script` | script-text | script, script-text, script-text-outline, script-text-play, script-text-play-outline | `script` |
| `bouton` | gesture-tap-button | gesture-tap-button, gesture-tap, button-pointer, button-cursor | type `act`, `button`, `input_button` |
<!-- <<< palette fr -->
