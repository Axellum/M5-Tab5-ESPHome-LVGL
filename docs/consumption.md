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

## Waveshare 7B, at the USB input (2026-10-16)

The second board of the firmware ([Waveshare 7B](waveshare-7b.md), 7 inches, 1024×600) measured by the author, the board powered by a computer's USB port.

### How it was measured

- A **FNIRSI FNB58** USB tester between the supply and the board, read by the computer through its micro-USB « PC » port: 100 samples per second (about 37 new values per second, the others repeat), voltage and current to 0.01 mV / 0.01 mA. The power is measured **at the board's USB input**, so it includes its regulators; the Tab5 figures above are at its battery. The tester's accuracy is not given by its maker.
- The board is driven by its own API while the tester records ([`tools/campagne_mesures.py`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/tools/campagne_mesures.py), reader [`tools/fnb58.py`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/tools/fnb58.py)): 20 s per backlight level after 6 s to settle; for each of the 20 screens of the « Go to screen » select, the first 3 s (opening) then 15 s once open; three restarts with the « Restart » button, 50 s each. No measure falls within −5 s / +30 s of a minute that is a multiple of 5, when Home Assistant sends its grouped updates. Everything is put back at the end.
- Firmware `main` 46ad7f03, theme Ardoise douce in dark mode, full animations, Okay Nabu and the speaker on, Wi-Fi eco off, the home screen. One board, one run (156,988 samples). Inside a segment, the power varies by about ±0.08 W (median to 95th percentile): **a difference of a few hundredths of a watt between two segments means nothing**.

### Results

| Case | Average | Current at ~5 V |
|---|---|---|
| Home screen, backlight 189/255 (74 %, the author's setting) | 1.96 W | 393 mA |
| Backlight 100 % | 3.14 W | 645 mA |
| Backlight 75 % | 1.98 W | 397 mA |
| Backlight 50 % | 1.47 W | 293 mA |
| Backlight 25 % | 1.28 W | 252 mA |
| Backlight 10 % | 1.25 W | 246 mA |
| Backlight 5 % | 1.24 W | 246 mA |
| Backlight 1 % | 1.24 W | 245 mA |
| Backlight off | 1.24 W | 245 mA |
| Each of the 20 screens and popups, open (at 74 %) | 1.945 to 1.958 W | 390 to 393 mA |
| Opening a screen (first 3 s) | 1.957 to 1.987 W, peak 2.28 W | |
| A restart (50 s from the button) | 1.93 W on average, 26.7 mWh | |

A restart, second by second: the board stops about 2 s after the button, drops to about 1.1 W for 1.5 s (the chip resets, lowest sample 0.78 W), runs at about 1.8 W while it builds the screen, about 2.0 W once the Wi-Fi starts (around 7 s), and is back to 1.96 W after about 14 s; highest sample 2.57 W. Two restarts gave the same curve to ±0.02 W. The third one did not finish: the Wi-Fi chip did not come back, see [the 7B page](waveshare-7b.md).

### What it tells

1. **The last quarter of the backlight costs the most.** From 75 % to 100 % the board draws 1.16 W more; from off to 75 %, only 0.74 W. The light given out was not measured (no light meter): whether 100 % looks much brighter than 75 % is for the eye to judge.
2. **Below 25 %, nothing more to gain**: 25 % draws 0.04 W more than the backlight off. On the Tab5 the same floor started below 50 %.
3. **Drawing costs nothing measurable.** Every screen and popup, open, draws the same as the home screen (within 0.013 W, the noise); opening one adds at most 0.03 W over 3 s. Choosing a lighter screen for power saving makes no sense.
4. **A restart costs nothing either**: 1.93 W on average over the 50 s, less than a lit screen, because the backlight is off while the chip resets.
5. **The board's floor is 1.24 W** with the backlight off (processor, Wi-Fi through the ESP32-C6, microphone and speaker on): the same as the Tab5 at its battery (1.23 W), by chance or not.
6. **The USB voltage sags at full brightness**: 4.87 V at 645 mA through the tester and a computer port, against 5.06 V with the backlight off. Not a problem here, but a long or thin cable or a weak supply leaves less margin at 100 %.

### Tab5 and 7B side by side

Not the same measuring point (battery for the Tab5, USB input for the 7B), not the same screen size (5 and 7 inches): an order of magnitude, not a contest.

| | Tab5 (at the battery, 2026-10-10) | Waveshare 7B (at the USB input, 2026-10-16) |
|---|---|---|
| Backlight 100 % | 2.63 W | 3.14 W |
| Backlight 50 % | 1.74 W | 1.47 W |
| Screen off | 1.23 W | 1.24 W |
| No gain below | 50 % | 25 % |
| First image after a restart | about 21 s (before [#478](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/478); not measured since, the Tab5 is out of order) | 5.6-5.8 s (with #478) |

### Over a year, 7B

| State, all year | Power | kWh per year |
|---|---|---|
| Backlight 74 % | 1.96 W | 17.2 |
| Backlight 50 % | 1.47 W | 12.9 |
| Backlight off | 1.24 W | 10.9 |
| 74 % for 6 h, then off | 1.42 W | 12.4 |
| 50 % for 6 h, then off | 1.30 W | 11.4 |

USB supply losses not included (they were not measured).

### Settings and themes, 7B (night of 2026-10-16)

Same tester, same board, the night of the [speed measurements](performance.md#waveshare-7b-night-of-2026-10-16) (21:45 → 06:16, 3,065,400 samples, no gap): a measuring build of `main` 3293d4e1, backlight 189/255 (74 %), the home screen, 60 s per case (20 s per theme). The reference is Ardoise douce in dark mode, full animations, Okay Nabu and the speaker on, Eco Wi-Fi off.

| Case | Average | vs reference |
|---|---|---|
| Reference, start / end of the night | 1.956 / 1.957 W | |
| Each of the 21 themes, dark | 1.944 to 1.968 W | none |
| Each of the 21 themes, light | 1.995 to 2.013 W | about +0.05 W (+2.5 %) |
| Obsidienne, dark / light | 1.955 / 2.003 W | light +2.5 % |
| Animations « None » / « Essential » | 1.950 / 1.959 W | none |
| Power saving « Always » | 1.238 W | −37 % |
| Eco Wi-Fi « Always » | 1.781 W | −9 % |
| Okay Nabu off | 1.938 W | −1 % |
| Speaker off | 1.946 W | −0.5 % |
| Microphone and speaker off | 1.905 W | −2.6 % |
| A restart (10 restarts, 40 s each from the button) | 1.920 to 1.931 W | |
| The night, screen off (23:32 → 00:02) | 1.235 W | |
| The night, screen on (00:03 → 06:15) | 1.957 W | |

1. **Neither the theme nor the animations change the power.** The 21 dark themes are within 0.024 W of each other (the noise); light mode adds about 0.05 W, whatever the theme.
2. **Power saving « Always » brings the board to its floor**: 1.238 W, the same as the backlight at 1 % or off. On a home screen left alone, this mode lowers the brightness to the minimum after 30 s without a touch ([settings](notice/settings.md)). Whether the screen stayed readable was not checked.
3. **Eco Wi-Fi « Always » saves 0.18 W** (−9 %), about 1.5 kWh a year if left on all the time; answers may come a little later, which was not measured.
4. **The microphone and the speaker cost little**: 0.05 W for both.
5. **No drift over the night**: 12.79 Wh in 6 h 43 min (1.90 W on average, half an hour of it with the screen off); with the screen on, the hourly average stayed between 1.956 and 1.958 W.

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
| Économie d'énergie « Always » | 1,73 W | −0,90 W | 8,6 h |
| Écran éteint | 1,23 W | −1,40 W (−53 %) | 12,0 h |
| Thème sombre (Obsidienne), écran 100 % | 2,63 W | 0,00 W | 5,6 h |
| Même thème en mode clair | 2,65 W | +0,02 W | 5,6 h |
| Écran, Okay Nabu et haut-parleur coupés | 1,16 W | −1,47 W (−56 %) | 12,8 h |
| Référence rejouée (fin du passage) | 2,63 W | 0,00 W | 5,6 h |

La batterie est donnée pour 7,4 V × 2000 mAh = 14,8 Wh ([documentation M5Stack, relayée par ses revendeurs](https://www.switch-science.com/products/10378)) ; M5Stack annonce lui-même environ 6 heures à 50 % de luminosité avec le Wi-Fi et des tâches de fond. Les heures ci-dessus sont une capacité nominale divisée par une puissance : une borne haute, pas une autonomie mesurée. Le script a affiché 23,7 Wh de « capacité utilisable approximative » : ce chiffre vient de 7 points seulement de niveau de batterie (un niveau calculé d'après la tension, qui n'est pas linéaire pour un élément lithium) et dépasse la capacité nominale ; il ne faut pas s'y fier. Les heures qu'il a imprimées (9 h en référence) sont trop hautes.

## Ce qu'on en tire

1. **Le rétroéclairage est le seul grand levier.** 100 % → 50 % économise 0,89 W, un tiers du total. Éteindre l'écran en économise 1,4 W, plus de la moitié.
2. **Sous 50 %, il n'y a plus rien à gagner** : 50 % et 10 % donnent la même puissance (1,74 et 1,73 W). La cause n'est pas établie (un plancher du pilote de rétroéclairage est la première piste) ; une mesure à 75 % et 25 % trancherait.
3. **Le mode économie d'énergie, c'est en pratique son plafond à 50 %.** « Always » (plafond 50 %, 10 % après 30 s sans toucher, aucune animation, 30 images/s) consomme comme l'écran à 10 % : pas de gain mesurable des animations ni de la cadence.
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

## Waveshare 7B, à l'entrée USB (16/10/2026)

La deuxième carte du firmware ([Waveshare 7B](waveshare-7b.md), 7 pouces, 1024×600), mesurée par l'auteur, alimentée par un port USB d'ordinateur.

### Comment c'est mesuré

- Un testeur USB **FNIRSI FNB58** entre l'alimentation et la carte, lu par l'ordinateur par son port micro-USB « PC » : 100 échantillons par seconde (environ 37 valeurs nouvelles par seconde, les autres répètent), tension et courant au 0,01 mV / 0,01 mA près. La puissance est mesurée **à l'entrée USB de la carte**, régulateurs compris ; les chiffres du Tab5 ci-dessus sont pris à sa batterie. Le fabricant du testeur ne donne pas sa précision.
- La carte est pilotée par sa propre API pendant l'enregistrement ([`tools/campagne_mesures.py`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/tools/campagne_mesures.py), lecteur [`tools/fnb58.py`](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/tools/fnb58.py)) : 20 s par niveau de rétroéclairage après 6 s de stabilisation ; pour chacun des 20 écrans du select « Go to screen », les 3 premières secondes (ouverture) puis 15 s une fois ouvert ; trois redémarrages par le bouton « Restart », 50 s chacun. Aucune mesure ne tombe entre −5 s et +30 s autour d'une minute multiple de 5, quand Home Assistant envoie ses mises à jour groupées. Tout est remis en place à la fin.
- Firmware `main` 46ad7f03, thème Ardoise douce en mode sombre, animations complètes, Okay Nabu et haut-parleur actifs, Wi-Fi éco désactivé, écran d'accueil. Une carte, un passage (156 988 échantillons). Dans un segment, la puissance varie d'environ ±0,08 W (de la médiane au 95e centile) : **quelques centièmes de watt d'écart entre deux segments ne veulent rien dire**.

### Résultats

| Cas | Moyenne | Courant sous ~5 V |
|---|---|---|
| Accueil, rétroéclairage 189/255 (74 %, le réglage de l'auteur) | 1,96 W | 393 mA |
| Rétroéclairage 100 % | 3,14 W | 645 mA |
| Rétroéclairage 75 % | 1,98 W | 397 mA |
| Rétroéclairage 50 % | 1,47 W | 293 mA |
| Rétroéclairage 25 % | 1,28 W | 252 mA |
| Rétroéclairage 10 % | 1,25 W | 246 mA |
| Rétroéclairage 5 % | 1,24 W | 246 mA |
| Rétroéclairage 1 % | 1,24 W | 245 mA |
| Rétroéclairage éteint | 1,24 W | 245 mA |
| Chacun des 20 écrans et popups, ouvert (à 74 %) | 1,945 à 1,958 W | 390 à 393 mA |
| Ouverture d'un écran (3 premières secondes) | 1,957 à 1,987 W, pointe 2,28 W | |
| Un redémarrage (50 s depuis le bouton) | 1,93 W en moyenne, 26,7 mWh | |

Un redémarrage, seconde par seconde : la carte s'arrête environ 2 s après le bouton, descend vers 1,1 W pendant 1,5 s (la puce redémarre, plus bas échantillon 0,78 W), tourne vers 1,8 W pendant qu'elle construit l'écran, vers 2,0 W dès que le Wi-Fi démarre (vers 7 s), et retrouve 1,96 W après environ 14 s ; plus haut échantillon 2,57 W. Deux redémarrages ont donné la même courbe à ±0,02 W près. Le troisième n'a pas abouti : la puce Wi-Fi n'est pas revenue, voir [la page de la 7B](waveshare-7b.md).

### Ce qu'on en tire

1. **Le dernier quart du rétroéclairage coûte le plus.** De 75 % à 100 %, la carte consomme 1,16 W de plus ; d'éteint à 75 %, seulement 0,74 W. La lumière émise n'a pas été mesurée (pas de luxmètre) : à l'œil de dire si 100 % paraît beaucoup plus lumineux que 75 %.
2. **Sous 25 %, plus rien à gagner** : 25 % consomme 0,04 W de plus que le rétroéclairage éteint. Sur le Tab5, le même plancher commençait sous 50 %.
3. **Dessiner ne coûte rien de mesurable.** Chaque écran et chaque popup, ouvert, consomme comme l'accueil (à 0,013 W près, le bruit) ; en ouvrir un ajoute au plus 0,03 W sur 3 s. Choisir un écran plus léger pour économiser n'a pas de sens.
4. **Un redémarrage ne coûte rien non plus** : 1,93 W en moyenne sur les 50 s, moins qu'un écran allumé, parce que le rétroéclairage est éteint pendant que la puce redémarre.
5. **Le plancher de la carte est 1,24 W** rétroéclairage éteint (processeur, Wi-Fi par l'ESP32-C6, micro et haut-parleur actifs) : autant que le Tab5 à sa batterie (1,23 W), par hasard ou non.
6. **La tension USB baisse à pleine luminosité** : 4,87 V à 645 mA à travers le testeur et un port d'ordinateur, contre 5,06 V rétroéclairage éteint. Sans conséquence ici, mais un câble long ou fin ou une alimentation faible laisse moins de marge à 100 %.

### Tab5 et 7B côte à côte

Pas le même point de mesure (batterie pour le Tab5, entrée USB pour la 7B), pas la même taille d'écran (5 et 7 pouces) : un ordre de grandeur, pas un match.

| | Tab5 (à la batterie, 10/10/2026) | Waveshare 7B (à l'entrée USB, 16/10/2026) |
|---|---|---|
| Rétroéclairage 100 % | 2,63 W | 3,14 W |
| Rétroéclairage 50 % | 1,74 W | 1,47 W |
| Écran éteint | 1,23 W | 1,24 W |
| Plus de gain sous | 50 % | 25 % |
| Première image après un redémarrage | environ 21 s (avant [#478](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/478) ; pas remesuré depuis, le Tab5 est en panne) | 5,6-5,8 s (avec #478) |

### Sur une année, 7B

| État, toute l'année | Puissance | kWh par an |
|---|---|---|
| Rétroéclairage 74 % | 1,96 W | 17,2 |
| Rétroéclairage 50 % | 1,47 W | 12,9 |
| Rétroéclairage éteint | 1,24 W | 10,9 |
| 74 % pendant 6 h, puis éteint | 1,42 W | 12,4 |
| 50 % pendant 6 h, puis éteint | 1,30 W | 11,4 |

Pertes de l'alimentation USB non comprises (elles n'ont pas été mesurées).

### Réglages et thèmes, 7B (nuit du 16/10/2026)

Même testeur, même carte, la nuit des [mesures de vitesse](performance.md#waveshare-7b-nuit-du-16102026) (21 h 45 → 6 h 16, 3 065 400 échantillons, aucun trou) : un build de mesure de `main` 3293d4e1, rétroéclairage 189/255 (74 %), l'accueil, 60 s par cas (20 s par thème). La référence : Ardoise douce en sombre, animations complètes, Ok Nabu et le haut-parleur allumés, Wi-Fi éco coupé.

| Cas | Moyenne | Par rapport à la référence |
|---|---|---|
| Référence, début / fin de nuit | 1,956 / 1,957 W | |
| Chacun des 21 thèmes, sombre | 1,944 à 1,968 W | aucun |
| Chacun des 21 thèmes, clair | 1,995 à 2,013 W | environ +0,05 W (+2,5 %) |
| Obsidienne, sombre / clair | 1,955 / 2,003 W | clair +2,5 % |
| Animations « None » / « Essential » | 1,950 / 1,959 W | aucun |
| Économie d'énergie « Always » | 1,238 W | −37 % |
| Wi-Fi éco « Always » | 1,781 W | −9 % |
| Ok Nabu coupé | 1,938 W | −1 % |
| Haut-parleur coupé | 1,946 W | −0,5 % |
| Micro et haut-parleur coupés | 1,905 W | −2,6 % |
| Un redémarrage (10 redémarrages, 40 s chacun depuis le bouton) | 1,920 à 1,931 W | |
| La nuit, écran éteint (23 h 32 → 0 h 02) | 1,235 W | |
| La nuit, écran allumé (0 h 03 → 6 h 15) | 1,957 W | |

1. **Ni le thème ni les animations ne changent la puissance.** Les 21 thèmes sombres tiennent dans 0,024 W (le bruit) ; le mode clair ajoute environ 0,05 W, quel que soit le thème.
2. **L'économie d'énergie « Always » ramène la carte à son plancher** : 1,238 W, comme le rétroéclairage à 1 % ou éteint. Sur un accueil laissé seul, ce mode baisse la luminosité au minimum après 30 s sans toucher ([réglages](notice/settings.md)). L'écran restait-il lisible ? Pas vérifié.
3. **Le Wi-Fi éco « Always » économise 0,18 W** (−9 %), environ 1,5 kWh par an s'il reste toujours actif ; les réponses peuvent arriver un peu plus tard, ce qui n'est pas mesuré.
4. **Le micro et le haut-parleur coûtent peu** : 0,05 W pour les deux.
5. **Aucune dérive sur la nuit** : 12,79 Wh en 6 h 43 min (1,90 W en moyenne, dont une demi-heure écran éteint) ; écran allumé, la moyenne de chaque heure est restée entre 1,956 et 1,958 W.

## Quoi en faire

Réglages qui existent déjà : **Luminosité** (50 % est le bon compromis), **Extinction auto** (5 min) avec **Extinction auto sur batterie seulement** pour qu'elle ne joue que débranchée, **Économie d'énergie** (« On battery » est l'état d'origine), **Limite de charge** à 80 % pour une tablette toujours branchée, **Rallumer l'écran à « Okay Nabu »** et **d'une tape** pour le retrouver.

Inutile d'après la mesure : le thème sombre sur batterie (0 W), couper Okay Nabu (0,05 W et plus de voix).

Pistes qui changeraient quelque chose (non faites, à décider) :

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
