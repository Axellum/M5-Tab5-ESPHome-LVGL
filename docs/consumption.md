# Power consumption

## English · [Français](#version-française)

---

First real measurement of what the Tab5 draws, made on **2026-10-10** on a tablet with its battery by **husyildiz** (firmware 3.8.0-rc.4, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)), with the Home Assistant script « Tab5 — consumption test » (`packages/tab5_mesure_conso.yaml`). Thanks to them for the hour of patience.

## How it was measured

- Eleven cases of 5 minutes each (the first 45 s of each are not counted), the tablet unplugged and left alone, 47 to 51 readings per case.
- The power is the battery voltage × the battery current read by the INA226, so it is the power **at the battery**, not at the wall. A charger has losses of its own, not measured here.
- One tablet (display chip ST7121), one run, a battery between 92 and 100 %. The reference case, played again at the end, gave the same value (2.63 W): no drift. Inside a case the readings vary by about ±0.05 W: **differences smaller than that mean nothing**.

## Results

| Case | Average | vs reference | Full battery would last (14.8 Wh rating) |
|---|---|---|---|
| Reference: screen 100 %, Okay Nabu on, speaker on | 2.63 W | | 5.6 h |
| Okay Nabu (microphone) off | 2.58 W | −0.05 W | 5.7 h |
| Speaker amplifier off | 2.58 W | −0.05 W | 5.7 h |
| Screen 50 % | 1.74 W | −0.89 W (−34 %) | 8.5 h |
| Screen 10 % | 1.73 W | −0.90 W | 8.6 h |
| Energy saving « Always » | 1.73 W | −0.90 W | 8.6 h |
| Screen off | 1.23 W | −1.40 W (−53 %) | 12.0 h |
| Dark theme (Obsidienne), screen 100 % | 2.63 W | 0.00 W | 5.6 h |
| Same theme, light mode | 2.65 W | +0.02 W | 5.6 h |
| Screen, Okay Nabu and speaker all off | 1.16 W | −1.47 W (−56 %) | 12.8 h |
| Reference again (end of the run) | 2.63 W | 0.00 W | 5.6 h |

The battery's rating is 7.4 V × 2000 mAh = 14.8 Wh ([M5Stack documentation, via its distributors](https://www.switch-science.com/products/10378)); M5Stack itself gives about 6 hours at 50 % brightness with Wi-Fi and background tasks running. The hours above are a rating divided by a power: an upper bound, not a measured autonomy. The script printed 23.7 Wh as « rough usable capacity »: that figure comes from only 7 points of battery level (a level computed from the voltage, which is not linear for a lithium cell) and exceeds the rating, so it is not to be trusted. The hours it printed (9 h in the reference case) are too high.

## What it tells

1. **The backlight is the only big lever.** 100 % → 50 % saves 0.89 W, one third of the total. Switching the screen off saves 1.4 W, more than half.
2. **Below 50 % there is nothing more to gain**: 50 % and 10 % give the same power (1.74 and 1.73 W). The cause is not established (a floor of the backlight driver is the first guess); a measurement at 75 % and 25 % would tell.
3. **The energy saving mode is, in practice, its 50 % cap.** « Always » (50 % cap, 10 % after 30 s without a touch, no animation, 30 fps) draws the same as the screen at 10 %: no measurable gain from the animations or the frame rate.
4. **The theme changes nothing**: dark or light, 0.02 W. The screen is backlit, so a black pixel does not switch the light off (the panel type was not checked against its datasheet). A dark theme on battery is for taste, not for autonomy.
5. **Voice costs almost nothing.** The microphone (Okay Nabu) and the speaker amplifier each draw about 0.05 W, which is the noise of the measure. Switching Okay Nabu off to save power gives up the voice assistant for nothing.
6. **The rest of the tablet draws 1.2 W** with everything off (processor, Wi-Fi through the ESP32-C6, memory): the floor of the whole device as it runs today.

## Over a year

Power × 8.76 gives the kWh of a year of continuous running (at the battery, charger losses not included):

| State, all year | Power | kWh per year |
|---|---|---|
| Screen 100 %, voice on | 2.63 W | 23.0 |
| Screen 50 %, voice on | 1.74 W | 15.2 |
| Screen off, voice on | 1.23 W | 10.8 |

A day with the screen lit for **6 hours** and off the other 18 (an assumption, to adapt to your use; the voice stays on and wakes the screen):

| Profile | Average | kWh per year |
|---|---|---|
| Screen 100 % all day | 2.63 W | 23.0 |
| 100 % for 6 h, then off | 1.58 W | 13.8 |
| 50 % for 6 h, then off (the « eco » profile) | 1.36 W | 11.9 |

The eco profile uses about **half** of the always-lit reference. In money, multiply the kWh by your price: at 0.25 €/kWh (an example, not a quote), 11.9 kWh is about 3 € a year, and the reference about 5.75 €. The savings are real but small in euros: the point of the profile is the heat, the battery wear and the ease of living with a wall screen that dims itself.

## What to do with it

Settings that already exist: **Brightness** (50 % is the sweet spot), **Auto screen off** (5 min), **Power saving** (« On battery » is the default), **Charge limit** 80 % for a tablet that stays plugged in, **Wake the screen on « Okay Nabu »** and **with a tap** to get it back.

Not worth doing, according to the measure: dark theme on battery (0 W), switching Okay Nabu off (0.05 W and no voice).

Ideas that would change something (not done, to be decided):

- On battery, an automatic **screen off after 5 minutes**, today independent of the power source: it takes 1.74 W down to 1.23 W while nobody looks at the tablet.
- Measure the missing points: screen at 75 % and 25 % (why does nothing change under 50 %?), the Eco Wi-Fi, and the **wall power with a smart plug** (charger losses, with and without a battery).
- Make the consumption test script compute the autonomy from the battery's rating instead of from its own extrapolation.

## Against the usual solution (an old tablet)

This project has **not measured another tablet**: no figure is given here for one, and none found in a search was backed by a method. To compare, put a measuring plug (Shelly, Tapo P110, or the like) under the tablet for 24 hours with the screen lit, then compare with 1.7 to 2.6 W above (to compare fairly, add the Tab5's charger, also measured at the wall).

What can be said without a measure:

- The Tab5 runs a native program, not a browser: nothing to update on the tablet side except this firmware, no kiosk application, no Android version that stops being supported.
- Its battery is optional (it runs on USB alone, as the author's does), and the charge limit holds a plugged-in battery at 80 %. A swollen battery pushing out the screen of an old tablet left on its charger for years is a well-known complaint ([Home Assistant forum](https://community.home-assistant.io/t/tablet-as-wall-panel/734479), [SmartThings forum](https://community.smartthings.com/t/do-you-leave-your-smart-home-tablet-plugged-in-24-7/246866)).
- Push-only: the tablet never polls, so the Wi-Fi and the processor have little to do between two changes.

## Adapting it to your home, with or without an AI

Everything that depends on a home is set in Home Assistant, not in the firmware: rooms and tiles, climate, energy, cameras, tracked sensors, the shortcuts of the gestures. The firmware names no entity and calls no Home Assistant action ([ADR-0025](decisions/0025-events-only.md)): it sends events, and the packages map them to a whitelist. Seven screen languages, themes, popups, the settings above are all choices of the user. And the project is written to be changed by an AI: [`AGENTS.md`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/AGENTS.md) tells any agent what to read first, each file opens with an `[AI-CONTEXT]` header, and the tests refuse a change that breaks the contract with Home Assistant. This page itself comes from a measure taken by a user and analysed with Claude the same day.

---

## Version Française

---

Première vraie mesure de ce que consomme le Tab5, faite le **10/10/2026** sur une tablette avec sa batterie par **husyildiz** (firmware 3.8.0-rc.4, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)), avec le script Home Assistant « Tab5 — consumption test » (`packages/tab5_mesure_conso.yaml`). Merci pour l'heure de patience.

## Comment c'est mesuré

- Onze cas de 5 minutes (les 45 premières secondes de chacun ne comptent pas), tablette débranchée et laissée tranquille, 47 à 51 relevés par cas.
- La puissance est la tension de la batterie × son courant, lus par l'INA226 : c'est la puissance **à la batterie**, pas à la prise. Un chargeur a ses propres pertes, non mesurées ici.
- Une tablette (puce d'écran ST7121), un passage, une batterie entre 92 et 100 %. Le cas de référence, rejoué à la fin, redonne la même valeur (2,63 W) : pas de dérive. Dans un cas, les relevés varient d'environ ±0,05 W : **un écart plus petit ne veut rien dire**.

## Résultats

| Cas | Moyenne | Écart à la référence | Autonomie d'une batterie pleine (14,8 Wh nominaux) |
|---|---|---|---|
| Référence : écran 100 %, Okay Nabu, haut-parleur | 2,63 W | | 5,6 h |
| Okay Nabu (micro) coupé | 2,58 W | −0,05 W | 5,7 h |
| Ampli du haut-parleur coupé | 2,58 W | −0,05 W | 5,7 h |
| Écran à 50 % | 1,74 W | −0,89 W (−34 %) | 8,5 h |
| Écran à 10 % | 1,73 W | −0,90 W | 8,6 h |
| Économie d'énergie « Toujours » | 1,73 W | −0,90 W | 8,6 h |
| Écran éteint | 1,23 W | −1,40 W (−53 %) | 12,0 h |
| Thème sombre (Obsidienne), écran 100 % | 2,63 W | 0,00 W | 5,6 h |
| Même thème en mode clair | 2,65 W | +0,02 W | 5,6 h |
| Écran, Okay Nabu et haut-parleur coupés | 1,16 W | −1,47 W (−56 %) | 12,8 h |
| Référence rejouée (fin du passage) | 2,63 W | 0,00 W | 5,6 h |

La batterie est donnée pour 7,4 V × 2000 mAh = 14,8 Wh ([documentation M5Stack, relayée par ses revendeurs](https://www.switch-science.com/products/10378)) ; M5Stack annonce lui-même environ 6 heures à 50 % de luminosité avec le Wi-Fi et des tâches de fond. Les heures ci-dessus sont une capacité nominale divisée par une puissance : une borne haute, pas une autonomie mesurée. Le script a affiché 23,7 Wh de « capacité utilisable approximative » : ce chiffre vient de 7 points seulement de niveau de batterie (un niveau calculé d'après la tension, qui n'est pas linéaire pour un élément lithium) et dépasse la capacité nominale ; il ne faut pas s'y fier. Les heures qu'il a imprimées (9 h en référence) sont trop hautes.

## Ce qu'on en tire

1. **Le rétroéclairage est le seul grand levier.** 100 % → 50 % économise 0,89 W, un tiers du total. Éteindre l'écran en économise 1,4 W, plus de la moitié.
2. **Sous 50 %, il n'y a plus rien à gagner** : 50 % et 10 % donnent la même puissance (1,74 et 1,73 W). La cause n'est pas établie (un plancher du pilote de rétroéclairage est la première piste) ; une mesure à 75 % et 25 % trancherait.
3. **Le mode économie d'énergie, c'est en pratique son plafond à 50 %.** « Toujours » (plafond 50 %, 10 % après 30 s sans toucher, aucune animation, 30 images/s) consomme comme l'écran à 10 % : pas de gain mesurable des animations ni de la cadence.
4. **Le thème ne change rien** : sombre ou clair, 0,02 W. L'écran est rétroéclairé, un pixel noir n'éteint pas la lumière (le type de dalle n'a pas été vérifié sur sa fiche). Un thème sombre sur batterie est une affaire de goût, pas d'autonomie.
5. **La voix ne coûte presque rien.** Le micro (Okay Nabu) et l'ampli du haut-parleur consomment chacun environ 0,05 W, soit le bruit de la mesure. Couper Okay Nabu pour économiser renonce à l'assistant vocal pour rien.
6. **Le reste de la tablette consomme 1,2 W** avec tout coupé (processeur, Wi-Fi par l'ESP32-C6, mémoire) : le plancher de l'appareil tel qu'il tourne aujourd'hui.

## Sur une année

Puissance × 8,76 donne les kWh d'une année de fonctionnement continu (à la batterie, pertes du chargeur non comprises) :

| État, toute l'année | Puissance | kWh par an |
|---|---|---|
| Écran 100 %, voix active | 2,63 W | 23,0 |
| Écran 50 %, voix active | 1,74 W | 15,2 |
| Écran éteint, voix active | 1,23 W | 10,8 |

Une journée avec l'écran allumé **6 heures** et éteint les 18 autres (une hypothèse, à adapter à votre usage ; la voix reste active et rallume l'écran) :

| Profil | Moyenne | kWh par an |
|---|---|---|
| Écran à 100 % toute la journée | 2,63 W | 23,0 |
| 100 % pendant 6 h, puis éteint | 1,58 W | 13,8 |
| 50 % pendant 6 h, puis éteint (le profil « éco ») | 1,36 W | 11,9 |

Le profil éco consomme environ **moitié moins** que la référence toujours allumée. En euros, multipliez les kWh par votre tarif : à 0,25 €/kWh (un exemple, pas un tarif), 11,9 kWh font environ 3 € par an, et la référence environ 5,75 €. L'économie est réelle mais petite en euros : l'intérêt du profil, c'est la chaleur, l'usure de la batterie et le confort d'un écran mural qui baisse tout seul.

## Quoi en faire

Réglages qui existent déjà : **Luminosité** (50 % est le bon compromis), **Extinction auto** (5 min), **Économie d'énergie** (« Sur batterie » est l'état d'origine), **Limite de charge** à 80 % pour une tablette toujours branchée, **Rallumer l'écran à « Okay Nabu »** et **d'une tape** pour le retrouver.

Inutile d'après la mesure : le thème sombre sur batterie (0 W), couper Okay Nabu (0,05 W et plus de voix).

Pistes qui changeraient quelque chose (non faites, à décider) :

- Sur batterie, une **extinction automatique de l'écran après 5 minutes**, aujourd'hui indépendante de la source d'alimentation : elle fait passer de 1,74 W à 1,23 W tant que personne ne regarde la tablette.
- Mesurer les points manquants : écran à 75 % et 25 % (pourquoi rien ne change sous 50 % ?), le Wi-Fi éco, et la **puissance à la prise avec une prise mesurante** (pertes du chargeur, avec et sans batterie).
- Faire calculer l'autonomie au script de test d'après la capacité nominale de la batterie plutôt que d'après sa propre extrapolation.

## Face à la solution habituelle (une vieille tablette)

Ce projet **n'a mesuré aucune autre tablette** : aucun chiffre n'est donné ici pour l'une d'elles, et aucun de ceux trouvés en cherchant ne s'appuyait sur une méthode. Pour comparer, mettez une prise mesurante (Shelly, Tapo P110 ou autre) sous la tablette pendant 24 heures, écran allumé, puis comparez à 1,7 à 2,6 W ci-dessus (pour être juste, ajoutez le chargeur du Tab5, mesuré lui aussi à la prise).

Ce qu'on peut dire sans mesure :

- Le Tab5 fait tourner un programme natif, pas un navigateur : rien à mettre à jour côté tablette que ce firmware, pas d'application de kiosque, pas de version d'Android dont le support s'arrête.
- Sa batterie est facultative (il tourne sur l'USB seul, comme celui de l'auteur), et la limite de charge garde une batterie toujours branchée à 80 %. Une batterie gonflée qui repousse l'écran d'une vieille tablette laissée des années sur son chargeur est une plainte connue ([forum Home Assistant](https://community.home-assistant.io/t/tablet-as-wall-panel/734479), [forum SmartThings](https://community.smartthings.com/t/do-you-leave-your-smart-home-tablet-plugged-in-24-7/246866)).
- Tout est poussé : la tablette n'interroge jamais, donc le Wi-Fi et le processeur ont peu à faire entre deux changements.

## L'adapter à sa maison, avec ou sans IA

Tout ce qui dépend d'une maison se règle dans Home Assistant, pas dans le firmware : pièces et tuiles, clim, énergie, caméras, capteurs suivis, raccourcis des gestes. Le firmware ne nomme aucune entité et n'appelle aucune action de Home Assistant ([ADR-0025](decisions/0025-events-only.md)) : il émet des événements, et les packages les rattachent à une liste blanche. Sept langues d'écran, des thèmes, des popups, les réglages ci-dessus : autant de choix de l'utilisateur. Et le projet est écrit pour être modifié par une IA : [`AGENTS.md`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/AGENTS.md) dit à tout agent quoi lire d'abord, chaque fichier s'ouvre sur un en-tête `[AI-CONTEXT]`, et les tests refusent un changement qui casse le contrat avec Home Assistant. Cette page elle-même vient d'une mesure faite par un utilisateur et analysée avec Claude le jour même.
