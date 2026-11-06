# M5Stack Tab5 battery and power in Home Assistant: charger, level and safety

## English · [Français](#version-française)

---

The Tab5 takes an optional two-cell (2S) lithium battery. This page says what the firmware does with it, with and without one: detection, charger, safety locks, level, charge limit, energy saving, alerts, and the measurements behind each number. Everything below is in the stable release [3.9.0](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.9.0) (2026-10-25) unless a version says otherwise: « since 3.8.0 » means the 3.8.0 release and every later one, « up to 3.8.1 » means the earlier releases only. The last section lists what each release changed. The pins used by the charger are in the [Tab5 pinout](tab5-pinout.md); the rest of the hardware is in the [hardware reference](hardware.md).

## At a glance

| Topic | Without a battery | With a battery | Since | Setting or entity |
|---|---|---|---|---|
| **Detection** | the charger is switched off and the voltage read with it off is under 5.5 V | 5.5 V or more with the charger off | reading under 6.0 V over 10 minutes in 3.7.0; charger-off reading in 3.8.0; 5.5 V in 3.8.1 | diagnostic **Battery detected** (on by default), switch **Battery fitted** (off by default) |
| **Charger** | stays off: no hiss, no false « charging » | switched on only after a reading that saw a battery; waking a battery in protection is a last resort | off without a battery since 3.8.0; reading before any switching on since 3.9.0 | diagnostic **Battery charging** |
| **Safety locks** | none needed (charger off) | thermal lock (CPU 55 °C), electrical lock (above 8.45 V, or no current for 30 s); two cut-offs within an hour lock the charger until the next restart | thermal 3.8.1; kept in flash, electrical and visible in 3.9.0 | diagnostic **Charger safety**, event `esphome.tab5_securite_chargeur` |
| **Level** | « unknown » | estimated from the voltage on a discharge curve measured on a Tab5 | straight line since 3.6.0; measured curve since 3.9.0 | diagnostics **Battery**, **Battery voltage**, **Battery current** (current since 3.7.0), disabled by default |
| **Charge limit** | – | 100 % (default) or 80 %: stops at 80 %, starts again at 70 % | 3.8.0 | device setting **Charge limit** |
| **Charge mode** | – | Classic (default) or Fast (the charger's quick-charge input) | 3.8.0 | device setting **Charge mode** |
| **Energy saving** | never | on battery: brightness capped at 50 %, minimum after 30 s without a touch and at 35 % of battery or less, animations off, 30 frames per second | 3.7.0; 60 % return threshold in 3.9.0 | device setting **Power saving** (« On battery » by default), diagnostic **On battery** |
| **Eco Wi-Fi** | only with « Always » | light Wi-Fi power save on battery (or always) | 3.8.0 | device setting **WiFi power saving** (« Never » by default) |
| **Low battery alert** | – | at 20 % then 10 %, on battery | 3.8.0 | event `esphome.tab5_batterie_faible` |
| **Power draw** | unknown (the USB current is not measured) | watts, while the battery is discharging | 3.8.0 | **Power draw**, **Power measurement** |
| **Status bar icon** | a plug | a battery whose glyph and colour follow the level | 3.6.0; plug in 3.7.0 | switch **Battery fitted** |

## What M5Stack says about the battery

From [M5Stack's Tab5 page](https://docs.m5stack.com/en/core/Tab5) (read on 2026-11-06):

- The battery is a removable **NP-F550**, 7.4 V, 2000 mAh (14.8 Wh), charged by an **IP2326** and monitored by an **INA226**. The standard Tab5 (SKU C145) comes **without** a battery; the Tab5 Kit (SKU K145) includes it.
- Battery life: about **6 hours** from full (8.23 V) to the shutdown threshold (6.0 V), with the screen at 50 %, Wi-Fi always on and background tasks running. For comparison, the discharge measured below (screen off, about 1.2 W) lasted 13 h 23 min.
- Charging: the Tab5 **charges only once it is switched on and initialised**, not when it is off. Below 6 V the battery enters protection mode: M5Stack's advice is to take the battery out, reset the device, put the battery back and charge through the USB-C port; the charger then trickle-charges for a few minutes until the voltage is back above 6 V.
- Power supply: before disconnecting the power or changing the battery, shut the device down; if the power is cut directly, wait 5 seconds before switching on again, or the IMU may fail to initialise.
- Operating temperature 0 to 40 °C.

## Power supply

The Tab5 is USB-C powered. Measured on a tablet with its battery on 2026-10-10: **2.6 W** with the screen at 100 %, **1.7 W** at 50 %, **1.2 W** with the screen off ([power consumption](consumption.md)); the 1.5 A figure given here before had no source. The author's tablet runs without a battery on a PC's USB port; the history kept by Home Assistant (since 2026-09-30) shows a single brownout reset, on 2026-10-06 right after an OTA, cause unknown. A charger of **5 V / 2 A** or more leaves some margin: advice, no longer a requirement. Without a battery, the tablet starts again by itself when USB-C power comes back, after a cable pulled while it runs as after a long-press power-off.

**Power path, as read on the schematic** (Jiuhai's notes in [discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), page 5 of the schematic, not checked by the author). Without a battery, the 5 V comes from USB-C or from the HVIN pin of the Ext.Port (pin 2): HVIN → D7 → FU4 (1 A polyfuse) → MP4560 buck converter (5.04 V) → MAX40200 ideal diode (1 A absolute maximum) → `SYS_5VBUS`; USB-C joins `SYS_5VBUS` through a second MAX40200; LPW5209 load switches then feed the screen, the P4, the C6, the speaker, the camera and the EXT 5V output. As far as that reading goes, every 5 V load shares one 1 A ideal diode: at peak load (full brightness, loud audio, camera, external sensors), a brownout seems more likely than damage. Whether it explains the brownouts above (on a PC's USB port) is not known. Jiuhai's devices run on 9 V through HVIN; the author has never powered the tablet that way. The measurements announced in the discussion (power-on from HVIN after a cut, input power at idle, at peak load and during an OTA, EXT 5V under load, twenty reboots and OTAs in a row, RTC drift) will be added here when they are published.

**Backlight.** Brightness is software-controlled via PWM (LEDC output on GPIO 22, `light: monochromatic`) and can be dimmed from Home Assistant to reduce power draw. Touching the screen while the backlight is off turns it back on (`touchscreen: on_release`). There is no ambient light sensor in this configuration.

## Battery detection

The INA226 reads the battery itself, or the charger, or the USB: the firmware has to decide from the voltage whether a battery is there. A wrong decision means a charger charging into nothing.

**Charger on, no battery (3.6.0 and 3.7.0).** The charger charges into nothing. It says « charging », and the INA226 reads the charger or the USB, never a battery: 4.2 V on the tablet of a user whose fitted original battery reads about 7.2 V, with a charging status that follows reality (2026-10-05); on the author's tablet, which has none, 4.2 V and 8.39 V alternating every 1 to 3 minutes (evening of 2026-10-03), then a steady 5.71 V (2026-10-04), around 5.70 to 5.76 V on 2026-10-08. 8.39 V is therefore one of the values read without a battery, not a full battery. It also makes a faint continuous hiss, which stops when the charger is switched off ([troubleshooting](troubleshooting.md#faint-continuous-hiss-on-a-tablet-without-a-battery-2026-10-08)). In 3.7.0, « no battery » means a reading below 6.0 V in the last 10 minutes (rule written on 2026-10-05, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)): the alternating case climbs back above 6 V between two low readings, and the steady 5.7 V case stays below.

**Charger off (since 3.8.0).** With the charger off, the INA226 reads the battery itself, or almost nothing: on the author's tablet, without a battery, **1.83 to 1.94 V** (four readings, 2026-10-08), with the charging status at « not charging ». A 2S battery, even an empty one, reads more than 5 V. So, charger off, **5.5 V or more** means a battery, less means none (3.0 V in 3.8.0: a residual 3.29 V read without a battery on 2026-10-11 passed it, and the charger then charged into nothing; 5.5 V since 3.8.1; a 2S battery never drops below about 6 V, its protection cuts off first). When the tablet looks:

- **at boot** (since 3.9.0), the charger stays off until a probe has read the voltage with the charger off (about 8 s after the end of `setup()`); it is switched on only if that reading sees a battery (5.5 V or more). The 30 s wake-up of a battery in protection only follows a probe that saw nothing, when the previous boot had seen a battery (kept in flash, `tab5_batterie_memoire`: unknown, none, seen), and **never after an abnormal restart** (crash, brownout). Up to 3.8.1, that wake-up came first, at every boot until something was decided (3.8.0 and 3.8.1 only skipped it when the previous boot had seen no battery): 30 s of charging into nothing on the first boot of a tablet without a battery (the C145 model is sold without one), and at every turn of a restart loop. With the INA226 silent (presence unknown), the charger stays off. **Risk:** a new or completely flat battery reads below 5.5 V, is seen as absent and is not woken up: switch **Battery fitted** on, which wakes it up at once (30 s, since 2026-10-10), then once an hour while it is not seen;
- **charger on, with a battery:** a probe every 10 minutes, and right after a reading below 6 V, to see whether the battery was taken out;
- **charger off:** every reading (every 60 s) decides, so a battery slid in while the tablet runs is seen within a minute;
- **no battery found while the Battery fitted switch says there is one:** the charger is switched on for 30 s once an hour (and at once when the switch is turned on), then probed, in case the battery was so empty that its protection had cut it off. With the switch off (no battery declared), the charger is never switched on again.

Without a battery, the charger stays off: no more hiss, no more false « charging ». The diagnostic entity **Battery detected**, enabled by default, publishes the decision (unknown before the first reading). Measured so far: only the reading without a battery, on the author's tablet; with a battery, not measured yet.

## The charger and its safety locks

**Charger.** Before 3.6.0 the firmware never enabled its charger, and a user reported that the battery did not charge ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). In 3.6.0 and 3.7.0 it enabled the charger at every boot (`charge_enable`, PI4IOE 0x44 P7), like M5Stack's M5Unified library, battery or not. Since 3.8.0 it no longer does so blindly, and since 3.9.0 it switches it on only when it has found a battery ([battery detection](#battery-detection)). Quick charge (P5) is kept off by default: the tablet stays plugged in, and standard charge asks less current from the USB charger (the Fast charge mode, [below](#charge-mode), switches it on).

**Safety locks.** The author's Tab5 died on 2026-10-11, most likely from charging into nothing (charger on without a battery, 7.7 V read, the P4 going from 61 to 70 °C in 4 minutes; not confirmed, it is at M5Stack's repair service). The firmware cuts the charger off by itself:

- **thermal** (since 3.8.1): CPU at 55 °C or more, or +4 °C within 60 s above 48 °C → charger off at least 15 minutes, then until a temperature **read** at 44 °C or less. Since 3.9.0 it is kept in flash (a restart, caused by the heat or not, finds it set) and an unreadable temperature no longer lifts it;
- **electrical** (since 3.9.0), charger on: a voltage above **8.45 V** (more than a full 2S battery) or **no charging current** (below 20 mA either way) for 30 s → charger off and a probe at once (before: up to 10 minutes until the next probe). No current while the probe reads a full battery (95 % or more) means charging has finished: nothing wrong, the current check stays quiet until the level drops below 90 %. **Two cut-offs within an hour lock the charger until the next restart.** Not measured with a battery: the voltage read at the end of a charge, and the current of a full battery;
- each lock is visible (since 3.9.0; in 3.8.1 the thermal lock had neither of the two): the diagnostic entity **Charger safety** (a problem, on while a lock is set) and the `esphome.tab5_securite_chargeur` event (`cause`: thermique, tension or courant; `mesure`; `verrouille`: oui when locked until restart), which the « sécurité du chargeur » guard of the `tab5_health` package turns into a persistent notification and a phone notification.

## Battery level and discharge curve

Three diagnostic entities reach Home Assistant (since 3.6.0): **Battery charging** (charging status, PI4IOE 0x44 P6, read every 10 s, a change published once it has lasted 30 s), **Battery voltage** (voltage measured by the INA226, published on a 50 mV change or every 15 min) and **Battery** (level estimated from the voltage; it reads too high while charging; « unknown » while no battery is detected). **Battery current** (since 3.7.0) comes with the [energy saving](#energy-saving-on-battery). They are **disabled by default** in Home Assistant: with the battery fitted, enable them on the device page.

Since 3.9.0 the level follows a discharge curve measured on a Tab5: 6.6 V = 0 %, 7.0 V = 2 %, 7.4 V = 7 %, 7.6 V = 18 %, 7.8 V = 47 %, 8.0 V = 62 %, 8.2 V = 97 %, 8.35 V = 100 %. It was measured on the original battery of a user's tablet (husyildiz, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278), 2026-10-15): two full discharges at a constant load, screen off (about 0.17 A, 1.2 W), read from Home Assistant's history; the remaining capacity is the time left until the battery's protection cut off (13 h 23 min). The second night crossed every voltage within 5 minutes of the first one. Up to 3.8.1 it was a straight line, 6.0 V = 0 %, 8.23 V = 100 % (ESPHome's reference configuration, and the figures M5Stack gives for its battery life): a lithium battery stays long on a plateau and then collapses, so the level dropped from 100 to about 65 % in 11 hours, then to 0 % in about one hour; 3.9.0-rc.2 used the open-circuit table of another battery (Samsung EB535151VU, from the Linux kernel), better but still flat for hours, then 58 → 7 % in two hours. Limits: one battery measured; with the screen on, the voltage sags a little more and the level reads a little low; on the plateau from 8.00 to 8.05 V (62 → 81 %), the voltage published to the nearest 50 mV can make the level jump by about 20 %.

## Charge limit

Since 3.8.0. The device setting **Charge limit**: « 100 % » (the default) or « 80 % »: charging stops at 80 % and starts again at 70 %, after at least 10 minutes stopped. It is meant for a tablet that stays plugged in: a lithium battery kept below full lasts longer. The level comes from the voltage (the discharge curve above), and the voltage is higher while charging than at rest, so charging stops a little before a real 80 %: with the measured curve, it stops at about 8.05 V read while charging and starts again at about 8.02 V read with the charger off (7.78 V and 7.56 V with the former straight line); both are on the plateau, and the 10-minute minimum pause keeps it from going back and forth (not measured on a tablet). Not measured yet: with the charger stopped and USB plugged in, does the tablet run from USB or from its battery? If it runs from its battery, the 80 % mode makes it go back and forth between 80 and 70 % instead of resting it.

## Charge mode

Since 3.8.0 ([ADR-0045](decisions/0045-charging-animations-eco-wifi.md)). The device setting **Charge mode**: « Classic » (the default, quick charge off as before) or « Fast »: the charger's quick-charge input (`nCHG_QC_EN`, P5) is switched on, only while the charger itself is on — never without a battery, during a probe or in the 80 % pause. M5Stack's M5Unified library calls these two states 500 mA and 1000 mA (`setChargeCurrent`); **neither current was measured on this tablet**, nor the heat or the load on the USB charger.

## Energy saving on battery

Since 3.7.0. The INA226 also gives the battery current, published as **Battery current** (diagnostic, disabled by default like the voltage): positive while the battery is discharging, that is while the tablet runs on it, negative while it charges. That sign comes from M5Stack's M5Unified library (`getBatteryCurrent`: the shunt is wired so that charging reads negative; ESPHome publishes the register as it is), and a user's tablet with its battery confirmed it on 2026-10-07 ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). Each reading (every 60 s) decides **On battery**: above 50 mA of discharge, on battery; below 20 mA, plugged in; never while charging (it switches back as soon as charging resumes) nor without a detected battery. The device setting **Power saving** (« On battery » by default) uses it: on battery, the brightness is capped at 50 % at the backlight output (Home Assistant keeps the brightness you chose), drops to the minimum (10 %) after 30 s without a touch and at 35 % of battery or less (until the level climbs back to 60 % since 3.9.0; 40 % up to 3.8.1, only 23 mV above 35 % on the measured discharge curve, against 0.26 V for 60 %), the animations stop (whatever the « Animations » setting) and LVGL redraws at most 30 times a second outside games. Thresholds: `Tab5/socle/tab5_economie.h`, tested on a PC by `tools/test_alarm_clock.cpp`; settings: [settings](installation/settings.md).

## Eco Wi-Fi

Since 3.8.0 ([ADR-0045](decisions/0045-charging-animations-eco-wifi.md)). The device setting **WiFi power saving**: « Never » (the default: the radio always listens, `power_save_mode: NONE`), « On battery » or « Always », with the same « on battery » as energy saving. On, the Wi-Fi uses the light power save (`WIFI_PS_MIN_MODEM`), sent to the C6 through ESP-Hosted, and goes back to full listening during voice, sound and updates. **Not measured**: the current saved, and the added latency of a push or a ping (up to one DTIM interval of the router). Whether the C6 software (1.4.1 on the author's tablet) honours the request is not verified either; the `tab5.wifi` log line says whether it was accepted.

## Low battery alert

Since 3.8.0. On battery, when the level drops below 20 %, then below 10 %, the tablet sends the `esphome.tab5_batterie_faible` event (level, threshold), once per threshold, re-armed at 30 % or when charging resumes. The « batterie faible » guard of the `tab5_health` package turns it into a persistent notification (the 10 % one replaces the 20 % one) and a phone notification.

## On the screen

An icon closes the status bar (top left, after the alarm bell) once the device switch **Battery fitted** is on (since 3.6.0); it is off by default and survives reboots. With no battery detected the icon is a **plug** (since 3.7.0), in the theme's text colour; with a battery its glyph follows the level (full above 80 %, half, low, « ! » below 20 %, a bolt while charging) and its colour is the phone's scale (`get_battery_color()`: green, blue, amber, red); « ? » in grey before the first reading. The icon reads the sensors on the tablet itself: it works even with the battery entities left disabled in Home Assistant.

## Power draw and measurements

Since 3.8.0. **Power draw** (W) is the battery voltage × the battery current, while the battery is discharging; it is unknown on mains, since the USB current is not measured. On the tablet, the « Battery » line of the system console shows the level and that power on battery (for example « 78% · 2.4 W »), otherwise the level and the voltage. Measured on a tablet on 2026-10-10 ([power consumption](consumption.md)). To measure it case by case (screen, microphone, speaker, theme, energy saving), the Home Assistant script « Tab5 — consumption test » (`packages/tab5_mesure_conso.yaml`, [described here](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/README.md#packagestab5_mesure_consoyaml--consumption-test)) switches on **Power measurement**, which reads the battery every 2 s instead of every minute, for 2 h at most.

## What the firmware cannot do for you

Fit only the 2S battery made for the Tab5 (NP-F550 type, 7.4 V nominal), **never a single-cell (1S) battery**. Prefer a **5 V / 2 A charger to a PC's USB port**: a brownout was seen on a PC's port right after the brightness went to 100 % (2026-10-08). HVIN: 6 to 24 V only. M5Stack gives 0 to 40 °C ambient for the Tab5: not in the sun nor against a heat source.

## Versions: what changed when

Dates are those of the GitHub releases, and each row was read in the code of the release's tag.

| Version | Date | Battery and power |
|---|---|---|
| 3.6.0 | 2026-10-05 | the charger is enabled at every boot (before: never); diagnostics Battery charging, Battery voltage and Battery (straight-line level); switch Battery fitted and the status bar icon |
| 3.7.0 | 2026-10-07 | diagnostic Battery detected (a reading under 6.0 V over 10 minutes means no battery) and the plug icon; Battery current, Power saving and On battery |
| 3.8.0 | 2026-10-10 | charger off without a battery, battery looked for with the charger off (threshold 3.0 V), memory of the previous boot; Charge limit, Charge mode, WiFi power saving, low battery alert, Power draw and Power measurement; Battery fitted wakes a battery at once |
| 3.8.1 | 2026-10-11 | presence threshold raised to 5.5 V; thermal lock |
| 3.9.0 | 2026-10-25 | reading before any switching on of the charger; thermal lock kept in flash; electrical lock and the Charger safety entity and event; discharge curve measured on a Tab5; energy-saving return threshold at 60 %; the probe no longer interrupts the 80 % charge limit |

---

## Version Française

---

Le Tab5 accepte une batterie lithium à deux éléments (2S) en option. Cette page dit ce que fait le firmware avec elle, avec et sans : détection, chargeur, verrous de sécurité, niveau, limite de charge, économie d'énergie, alertes, et les mesures derrière chaque chiffre. Tout ce qui suit est dans la version stable [3.9.0](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.9.0) (25/10/2026) sauf quand une version est nommée : « depuis la 3.8.0 » veut dire la 3.8.0 et toutes les suivantes, « jusqu'à la 3.8.1 » les versions précédentes seulement. La dernière section liste ce que chaque version a changé. Les broches du chargeur sont dans le [brochage du Tab5](tab5-pinout.md#version-française) ; le reste du matériel est dans la [référence matérielle](hardware.md#version-française).

## En un coup d'œil

| Sujet | Sans batterie | Avec une batterie | Depuis | Réglage ou entité |
|---|---|---|---|---|
| **Détection** | le chargeur est coupé et la tension lue chargeur coupé est sous 5,5 V | 5,5 V ou plus chargeur coupé | lecture sous 6,0 V sur 10 minutes en 3.7.0 ; lecture chargeur coupé en 3.8.0 ; 5,5 V en 3.8.1 | diagnostic **Battery detected** (activé par défaut), interrupteur **Battery fitted** (éteint par défaut) |
| **Chargeur** | reste coupé : plus de souffle, plus de faux « en charge » | allumé seulement après une lecture qui a vu une batterie ; réveiller une batterie en protection est un dernier recours | coupé sans batterie depuis la 3.8.0 ; lecture avant tout allumage depuis la 3.9.0 | diagnostic **Battery charging** |
| **Verrous de sécurité** | inutiles (chargeur coupé) | verrou thermique (CPU à 55 °C), verrou électrique (plus de 8,45 V, ou aucun courant pendant 30 s) ; deux coupures en moins d'une heure verrouillent le chargeur jusqu'au redémarrage suivant | thermique 3.8.1 ; gardé en flash, électrique et visible en 3.9.0 | diagnostic **Charger safety**, événement `esphome.tab5_securite_chargeur` |
| **Niveau** | « inconnu » | estimé d'après la tension sur une courbe de décharge mesurée sur un Tab5 | droite depuis la 3.6.0 ; courbe mesurée depuis la 3.9.0 | diagnostics **Battery**, **Battery voltage**, **Battery current** (courant depuis la 3.7.0), désactivés par défaut |
| **Limite de charge** | – | 100 % (défaut) ou 80 % : arrêt à 80 %, reprise à 70 % | 3.8.0 | réglage de l'appareil **Charge limit** |
| **Mode de charge** | – | Classic (défaut) ou Fast (l'entrée de charge rapide du chargeur) | 3.8.0 | réglage de l'appareil **Charge mode** |
| **Économie d'énergie** | jamais | sur batterie : luminosité plafonnée à 50 %, minimum après 30 s sans toucher et à 35 % de batterie ou moins, animations arrêtées, 30 images par seconde | 3.7.0 ; seuil de retour à 60 % en 3.9.0 | réglage de l'appareil **Power saving** (« On battery » par défaut), diagnostic **On battery** |
| **Wi-Fi éco** | seulement avec « Always » | économie légère du Wi-Fi sur batterie (ou toujours) | 3.8.0 | réglage de l'appareil **WiFi power saving** (« Never » par défaut) |
| **Alerte de batterie faible** | – | à 20 % puis 10 %, sur batterie | 3.8.0 | événement `esphome.tab5_batterie_faible` |
| **Consommation** | inconnue (le courant de l'USB n'est pas mesuré) | en watts, quand la batterie se décharge | 3.8.0 | **Power draw**, **Power measurement** |
| **Icône du bandeau** | une prise | une batterie dont le glyphe et la couleur suivent le niveau | 3.6.0 ; prise en 3.7.0 | interrupteur **Battery fitted** |

## Ce que dit M5Stack de la batterie

D'après [la page du Tab5 chez M5Stack](https://docs.m5stack.com/en/core/Tab5) (lue le 06/11/2026) :

- La batterie est une **NP-F550** amovible, 7,4 V, 2000 mAh (14,8 Wh), chargée par un **IP2326** et surveillée par un **INA226**. Le Tab5 standard (référence C145) est vendu **sans** batterie ; le Tab5 Kit (K145) l'inclut.
- Autonomie : environ **6 heures** de la pleine charge (8,23 V) au seuil d'arrêt (6,0 V), écran à 50 %, Wi-Fi toujours actif et tâches de fond en marche. En comparaison, la décharge mesurée plus bas (écran éteint, environ 1,2 W) a duré 13 h 23.
- Charge : le Tab5 **ne se charge qu'une fois allumé et initialisé**, pas quand il est éteint. Sous 6 V, la batterie passe en mode protection : M5Stack conseille de sortir la batterie, de réinitialiser l'appareil, de remettre la batterie et de charger par le port USB-C ; le chargeur charge alors à petit courant pendant quelques minutes, jusqu'à ce que la tension repasse au-dessus de 6 V.
- Alimentation : avant de débrancher l'alimentation ou de changer la batterie, éteindre l'appareil ; si l'alimentation est coupée directement, attendre 5 secondes avant de rallumer, sinon l'IMU peut ne pas s'initialiser.
- Température de fonctionnement : 0 à 40 °C.

## Alimentation

Le Tab5 est alimenté en USB-C. Mesurée le 10/10/2026 sur une tablette avec sa batterie : **2,6 W** écran à 100 %, **1,7 W** à 50 %, **1,2 W** écran éteint ([consommation](consumption.md#version-française)) ; le chiffre de 1,5 A donné ici avant n'avait pas de source. La tablette de l'auteur tourne sans batterie sur un port USB de PC ; l'historique gardé par Home Assistant (depuis le 30/09/2026) montre un seul reset par sous-tension, le 06/10/2026 juste après une OTA, cause inconnue. Un chargeur de **5 V / 2 A** ou plus laisse de la marge : un conseil, plus une exigence. Sans batterie, la tablette redémarre seule au retour de l'alimentation USB-C, après un câble tiré en marche comme après une extinction par appui long.

**Chemin de l'alimentation, lu sur le schéma** (notes de Jiuhai dans la [discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369), page 5 du schéma, non vérifiées par l'auteur). Sans batterie, le 5 V vient de l'USB-C ou de la broche HVIN de l'Ext.Port (broche 2) : HVIN → D7 → FU4 (fusible réarmable de 1 A) → convertisseur abaisseur MP4560 (5,04 V) → diode idéale MAX40200 (1 A au maximum absolu) → `SYS_5VBUS` ; l'USB-C rejoint `SYS_5VBUS` par une seconde MAX40200 ; des interrupteurs de charge LPW5209 alimentent ensuite l'écran, le P4, le C6, le haut-parleur, la caméra et la sortie EXT 5V. Selon cette lecture, toutes les charges en 5 V se partagent une seule diode idéale de 1 A : en pointe (luminosité au maximum, son fort, caméra, capteurs externes), une sous-tension semble plus probable qu'une casse. On ne sait pas si cela explique les sous-tensions ci-dessus (sur un port USB de PC). Les appareils de Jiuhai tournent en 9 V par HVIN ; l'auteur n'a jamais alimenté la tablette ainsi. Les mesures annoncées dans la discussion (démarrage par HVIN après une coupure, puissance absorbée au repos, en pointe et pendant une OTA, EXT 5V en charge, vingt redémarrages et OTA d'affilée, dérive de l'horloge) seront ajoutées ici quand elles seront publiées.

**Rétroéclairage.** La luminosité est contrôlée logiciellement via PWM (sortie LEDC sur GPIO 22, `light: monochromatic`) et peut être réduite depuis Home Assistant. Toucher l'écran quand le rétroéclairage est éteint le rallume (`touchscreen: on_release`). Il n'y a pas de capteur de luminosité ambiante dans cette configuration.

## Détection de la batterie

L'INA226 lit la batterie elle-même, ou le chargeur, ou l'USB : le firmware doit décider d'après la tension s'il y a une batterie. Une mauvaise décision, c'est un chargeur qui charge dans le vide.

**Chargeur allumé, sans batterie (3.6.0 et 3.7.0).** Le chargeur charge dans le vide. Il dit « en charge », et l'INA226 lit le chargeur ou l'USB, jamais une batterie : 4,2 V sur la tablette d'un utilisateur dont la batterie d'origine montée lit environ 7,2 V, avec un état de charge qui suit la réalité (05/10/2026) ; sur la tablette de l'auteur, qui n'en a pas, 4,2 V et 8,39 V en alternance toutes les 1 à 3 minutes (soir du 03/10/2026), puis 5,71 V stable (04/10/2026), vers 5,70 à 5,76 V le 08/10/2026. 8,39 V est donc une des valeurs lues sans batterie, pas une batterie pleine. Il fait aussi un léger souffle continu, qui s'arrête quand on coupe le chargeur ([dépannage](troubleshooting.md#léger-souffle-continu-sur-une-tablette-sans-batterie-08102026)). En 3.7.0, « pas de batterie » veut dire une lecture sous 6,0 V dans les 10 dernières minutes (règle écrite le 05/10/2026, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : le cas de l'alternance repasse au-dessus de 6 V entre deux lectures basses, et le cas des 5,7 V stables reste en dessous.

**Chargeur coupé (depuis la 3.8.0).** Chargeur coupé, l'INA226 lit la batterie elle-même, ou presque rien : sur la tablette de l'auteur, sans batterie, **1,83 à 1,94 V** (quatre lectures, 08/10/2026), avec l'état de charge à « pas en charge ». Une batterie 2S, même vide, lit plus de 5 V. Chargeur coupé, **5,5 V ou plus** veut donc dire une batterie, moins veut dire aucune (3,0 V en 3.8.0 : un résidu de 3,29 V lu sans batterie le 11/10/2026 le passait, et le chargeur chargeait alors dans le vide ; 5,5 V depuis la 3.8.1 ; une batterie 2S ne descend jamais sous 6 V environ, sa protection coupe avant). Quand la tablette regarde :

- **au démarrage** (depuis la 3.9.0), le chargeur reste coupé jusqu'à ce qu'une sonde ait lu la tension chargeur coupé (environ 8 s après la fin de `setup()`) ; il n'est allumé que si cette lecture voit une batterie (5,5 V ou plus). Le réveil de 30 s d'une batterie en protection ne suit qu'une sonde qui n'a rien vu, si le démarrage précédent avait vu une batterie (gardé en flash, `tab5_batterie_memoire` : inconnue, aucune, vue), et **jamais après un redémarrage anormal** (plantage, baisse de tension). Jusqu'à la 3.8.1, ce réveil venait d'abord, à chaque démarrage tant que rien n'était décidé (la 3.8.0 et la 3.8.1 ne le sautaient que si le démarrage précédent n'avait vu aucune batterie) : 30 s de charge dans le vide au premier démarrage d'une tablette sans batterie (le modèle C145 est vendu sans), et à chaque tour d'une boucle de redémarrages. INA226 muet (présence inconnue) : le chargeur reste coupé. **Risque :** une batterie neuve ou totalement plate lit moins de 5,5 V, est vue absente et n'est pas réveillée : allumer **Battery fitted**, qui la réveille tout de suite (30 s, depuis le 10/10/2026), puis une fois par heure tant qu'elle n'est pas vue ;
- **chargeur allumé, avec une batterie :** une sonde toutes les 10 minutes, et tout de suite après une lecture sous 6 V, pour voir si la batterie a été retirée ;
- **chargeur coupé :** chaque lecture (toutes les 60 s) décide, donc une batterie glissée tablette allumée est vue dans la minute ;
- **pas de batterie trouvée alors que l'interrupteur Battery fitted dit qu'il y en a une :** le chargeur est rallumé 30 s une fois par heure (et tout de suite quand on allume l'interrupteur), puis une sonde, au cas où la batterie était si vide que sa protection l'avait coupée. Interrupteur éteint (aucune batterie déclarée), le chargeur n'est jamais rallumé.

Sans batterie, le chargeur reste coupé : plus de souffle, plus de faux « en charge ». L'entité de diagnostic **Battery detected**, activée par défaut, publie la décision (inconnue avant la première lecture). Mesuré à ce jour : seulement la lecture sans batterie, sur la tablette de l'auteur ; avec une batterie, pas encore mesuré.

## Le chargeur et ses verrous de sécurité

**Chargeur.** Avant la 3.6.0, le firmware n'activait jamais son chargeur, et un utilisateur a signalé que la batterie ne se chargeait pas ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). En 3.6.0 et 3.7.0, il activait le chargeur à chaque démarrage (`charge_enable`, PI4IOE 0x44 P7), comme la bibliothèque M5Unified de M5Stack, batterie ou pas. Depuis la 3.8.0 il ne le fait plus à l'aveugle, et depuis la 3.9.0 il ne l'allume que quand il a trouvé une batterie ([détection de la batterie](#détection-de-la-batterie)). La charge rapide (P5) reste à l'arrêt par défaut : la tablette reste branchée, et la charge standard demande moins de courant au chargeur USB (le mode de charge Fast, [plus bas](#mode-de-charge), l'allume).

**Verrous de sécurité.** La Tab5 de l'auteur est morte le 11/10/2026, très probablement d'une charge dans le vide (chargeur allumé sans batterie, 7,7 V lus, le P4 passé de 61 à 70 °C en 4 minutes ; pas confirmé, elle est au SAV de M5Stack). Le firmware coupe le chargeur de lui-même :

- **thermique** (depuis la 3.8.1) : CPU à 55 °C ou plus, ou +4 °C en 60 s au-dessus de 48 °C → chargeur coupé au moins 15 minutes, puis jusqu'à une température **lue** à 44 °C ou moins. Depuis la 3.9.0, il est gardé en flash (un redémarrage, provoqué par la chaleur ou non, le retrouve posé) et une température illisible ne le lève plus ;
- **électrique** (depuis la 3.9.0), chargeur allumé : une tension au-dessus de **8,45 V** (plus qu'une batterie 2S pleine) ou **aucun courant de charge** (moins de 20 mA dans un sens ou dans l'autre) pendant 30 s → chargeur coupé et sonde tout de suite (avant : jusqu'à 10 minutes avant la sonde suivante). Pas de courant alors que la sonde lit une batterie pleine (95 % ou plus) veut dire charge terminée : rien d'anormal, le contrôle du courant se tait jusqu'à ce que le niveau repasse sous 90 %. **Deux coupures en moins d'une heure verrouillent le chargeur jusqu'au redémarrage suivant.** Pas mesuré avec une batterie : la tension lue en fin de charge, et le courant d'une batterie pleine ;
- chaque verrou se voit (depuis la 3.9.0 ; en 3.8.1, le verrou thermique n'avait ni l'un ni l'autre) : l'entité de diagnostic **Charger safety** (problème, allumée tant qu'un verrou est posé) et l'événement `esphome.tab5_securite_chargeur` (`cause` : thermique, tension ou courant ; `mesure` ; `verrouille` : oui quand il est bloqué jusqu'au redémarrage), que la garde « sécurité du chargeur » du package `tab5_health` change en notification persistante et sur le téléphone.

## Niveau de la batterie et courbe de décharge

Trois entités de diagnostic arrivent dans Home Assistant (depuis la 3.6.0) : **Battery charging** (état de charge, PI4IOE 0x44 P6, lu toutes les 10 s, un changement publié quand il a tenu 30 s), **Battery voltage** (tension mesurée par l'INA226, publiée quand elle bouge de 50 mV ou toutes les 15 min) et **Battery** (niveau estimé d'après la tension ; il lit trop haut pendant la charge ; « inconnu » tant qu'aucune batterie n'est détectée). **Battery current** (depuis la 3.7.0) vient avec l'[économie d'énergie](#économie-dénergie-sur-batterie). Elles sont **désactivées par défaut** dans Home Assistant ; avec la batterie montée, les activer sur la page de l'appareil.

Depuis la 3.9.0, le niveau suit une courbe de décharge mesurée sur un Tab5 : 6,6 V = 0 %, 7,0 V = 2 %, 7,4 V = 7 %, 7,6 V = 18 %, 7,8 V = 47 %, 8,0 V = 62 %, 8,2 V = 97 %, 8,35 V = 100 %. Elle a été mesurée sur la batterie d'origine de la tablette d'un utilisateur (husyildiz, [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278), 15/10/2026) : deux décharges complètes à charge constante, écran éteint (0,17 A environ, 1,2 W), lues dans l'historique de Home Assistant ; la capacité restante est le temps restant jusqu'à la coupure de la protection de la batterie (13 h 23). La seconde nuit a franchi chaque tension à moins de 5 minutes de la première. Jusqu'à la 3.8.1, c'était une droite, 6,0 V = 0 %, 8,23 V = 100 % (config de référence ESPHome, et les chiffres que M5Stack donne pour l'autonomie de sa batterie) : une batterie lithium reste longtemps sur un plateau puis s'effondre, et le niveau descendait de 100 à 65 % environ en 11 heures, puis à 0 % en une heure environ ; la 3.9.0-rc.2 prenait la table à vide d'une autre batterie (Samsung EB535151VU, noyau Linux), mieux mais encore plate des heures, puis 58 → 7 % en deux heures. Limites : une seule batterie mesurée ; écran allumé, la tension baisse un peu plus et le niveau lit un peu bas ; sur le plateau de 8,00 à 8,05 V (62 → 81 %), la tension publiée à 50 mV près peut faire sauter le niveau d'environ 20 %.

## Limite de charge

Depuis la 3.8.0. Le réglage de l'appareil **Charge limit** : « 100 % » (le défaut) ou « 80 % » : la charge s'arrête à 80 % et reprend à 70 %, après au moins 10 minutes d'arrêt. C'est pour une tablette toujours branchée : une batterie lithium gardée sous le plein dure plus longtemps. Le niveau vient de la tension (la courbe de décharge plus haut), et la tension est plus haute pendant la charge qu'au repos : la charge s'arrête donc un peu avant 80 % réels. Avec la courbe mesurée, elle s'arrête vers 8,05 V lus en charge et reprend vers 8,02 V lus chargeur coupé (7,78 V et 7,56 V avec l'ancienne droite) ; les deux sont sur le plateau, et la pause d'au moins 10 minutes évite le va-et-vient (pas mesuré sur une tablette). Pas encore mesuré : chargeur arrêté et USB branché, la tablette tourne-t-elle sur l'USB ou sur sa batterie ? Si c'est sur sa batterie, le mode 80 % la fait aller et venir entre 80 et 70 % au lieu de la mettre au repos.

## Mode de charge

Depuis la 3.8.0 ([ADR-0045](decisions/0045-charging-animations-eco-wifi.md)). Le réglage de l'appareil **Charge mode** : « Classic » (le défaut, charge rapide à l'arrêt comme avant) ou « Fast » : l'entrée de charge rapide du chargeur (`nCHG_QC_EN`, P5) est allumée, seulement quand le chargeur lui-même l'est — jamais sans batterie, pendant une sonde ou en pause à 80 %. La bibliothèque M5Unified de M5Stack appelle ces deux états 500 mA et 1000 mA (`setChargeCurrent`) ; **aucun de ces courants n'a été mesuré sur cette tablette**, ni la chauffe, ni la charge sur le chargeur USB.

## Économie d'énergie sur batterie

Depuis la 3.7.0. L'INA226 donne aussi le courant de la batterie, publié en **Battery current** (diagnostic, désactivé par défaut comme la tension) : positif quand la batterie se décharge, c'est-à-dire quand la tablette tourne sur elle, négatif quand elle charge. Ce sens vient de la bibliothèque M5Unified de M5Stack (`getBatteryCurrent` : le shunt est câblé pour que la charge se lise négative ; ESPHome publie le registre tel quel), et la tablette d'un utilisateur, avec sa batterie, l'a confirmé le 07/10/2026 ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)). Chaque lecture (toutes les 60 s) décide **On battery** : au-dessus de 50 mA de décharge, sur batterie ; sous 20 mA, branchée ; jamais pendant la charge (elle repasse dès que la charge reprend) ni sans batterie détectée. Le réglage de l'appareil **Power saving** (« On battery » par défaut) s'en sert : sur batterie, la luminosité est plafonnée à 50 % à la sortie du rétroéclairage (Home Assistant garde la luminosité choisie), descend au plus bas (10 %) après 30 s sans toucher et à 35 % de batterie ou moins (jusqu'à ce que le niveau remonte à 60 % depuis la 3.9.0 ; 40 % jusqu'à la 3.8.1, à 23 mV seulement de 35 % sur la courbe de décharge mesurée, contre 0,26 V pour 60 %), les animations s'arrêtent (quel que soit le réglage « Animations ») et LVGL redessine 30 fois par seconde au plus hors des jeux. Seuils : `Tab5/socle/tab5_economie.h`, testés sur PC par `tools/test_alarm_clock.cpp` ; réglages : [réglages](installation/settings.md#version-française).

## Wi-Fi éco

Depuis la 3.8.0 ([ADR-0045](decisions/0045-charging-animations-eco-wifi.md)). Le réglage de l'appareil **WiFi power saving** : « Never » (le défaut : la radio écoute en permanence, `power_save_mode: NONE`), « On battery » ou « Always », avec le même « sur batterie » que l'économie d'énergie. Actif, le Wi-Fi passe en économie légère (`WIFI_PS_MIN_MODEM`), envoyée au C6 par ESP-Hosted, et revient à l'écoute complète pendant la voix, un son ou une mise à jour. **Pas mesuré** : le courant gagné, et la latence ajoutée à un push ou un ping (jusqu'à un intervalle DTIM du routeur). Que le logiciel du C6 (1.4.1 sur la tablette de l'auteur) applique la demande n'est pas vérifié non plus ; la ligne de journal `tab5.wifi` dit si elle a été acceptée.

## Alerte de batterie faible

Depuis la 3.8.0. Sur batterie, quand le niveau passe sous 20 %, puis sous 10 %, la tablette envoie l'événement `esphome.tab5_batterie_faible` (niveau, seuil), une fois par seuil, ré-armé à 30 % ou quand la charge reprend. La garde « batterie faible » du package `tab5_health` le transforme en notification persistante (celle de 10 % remplace celle de 20 %) et en notification sur le téléphone.

## À l'écran

Une icône termine le bandeau d'état (en haut à gauche, après la cloche du réveil) quand l'interrupteur de l'appareil **Battery fitted** est allumé (depuis la 3.6.0) ; il est éteint par défaut et survit aux redémarrages. Sans batterie détectée, l'icône est une **prise** (depuis la 3.7.0), de la couleur du texte du thème ; avec une batterie, son glyphe suit le niveau (pleine au-dessus de 80 %, moitié, basse, « ! » sous 20 %, un éclair pendant la charge) et sa couleur est l'échelle du téléphone (`get_battery_color()` : vert, bleu, ambre, rouge) ; « ? » en gris avant la première lecture. L'icône lit les capteurs sur la tablette même : elle marche aussi avec les entités de la batterie laissées désactivées dans Home Assistant.

## Consommation et mesures

Depuis la 3.8.0. **Power draw** (W) est la tension × le courant de la batterie, quand la batterie se décharge ; elle est inconnue sur secteur, le courant de l'USB n'étant pas mesuré. Sur la tablette, la ligne « Batterie » de la console système montre le niveau et cette puissance sur batterie (par exemple « 78% · 2.4 W »), sinon le niveau et la tension. Mesurée sur une tablette le 10/10/2026 ([consommation](consumption.md#version-française)). Pour la mesurer cas par cas (écran, micro, haut-parleur, thème, économie d'énergie), le script Home Assistant « Tab5 — consumption test » (`packages/tab5_mesure_conso.yaml`, [décrit ici](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/main/HomeAssistant_Config/README.md#packagestab5_mesure_consoyaml--test-de-consommation)) allume **Power measurement**, qui lit la batterie toutes les 2 s au lieu d'une fois par minute, 2 h au plus.

## Ce que le firmware ne peut pas faire pour vous

Ne monter que la batterie 2S prévue pour le Tab5 (type NP-F550, 7,4 V nominal), **jamais une batterie à un seul élément (1S)**. Préférer un **chargeur 5 V / 2 A à un port USB de PC** : une sous-tension a été vue sur le port d'un PC juste après le passage de la luminosité à 100 % (08/10/2026). HVIN : 6 à 24 V seulement. M5Stack donne 0 à 40 °C ambiants pour le Tab5 : ni au soleil ni contre une source de chaleur.

## Versions : ce qui a changé quand

Les dates sont celles des versions publiées sur GitHub, et chaque ligne a été relue dans le code de l'étiquette (tag) de la version.

| Version | Date | Batterie et alimentation |
|---|---|---|
| 3.6.0 | 05/10/2026 | le chargeur est activé à chaque démarrage (avant : jamais) ; diagnostics Battery charging, Battery voltage et Battery (niveau en droite) ; interrupteur Battery fitted et icône du bandeau |
| 3.7.0 | 07/10/2026 | diagnostic Battery detected (une lecture sous 6,0 V sur 10 minutes veut dire pas de batterie) et icône prise ; Battery current, Power saving et On battery |
| 3.8.0 | 10/10/2026 | chargeur coupé sans batterie, batterie cherchée chargeur coupé (seuil 3,0 V), mémoire du démarrage précédent ; Charge limit, Charge mode, WiFi power saving, alerte de batterie faible, Power draw et Power measurement ; Battery fitted réveille une batterie tout de suite |
| 3.8.1 | 11/10/2026 | seuil de présence relevé à 5,5 V ; verrou thermique |
| 3.9.0 | 25/10/2026 | lecture avant tout allumage du chargeur ; verrou thermique gardé en flash ; verrou électrique, entité et événement Charger safety ; courbe de décharge mesurée sur un Tab5 ; seuil de retour de l'économie d'énergie à 60 % ; la sonde n'interrompt plus la limite de charge à 80 % |
