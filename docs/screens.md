# Screens & Features

## English · [Français](#version-française)

---

This page describes what the Tab5 actually shows and does — verified against the firmware (`tab5-lvgl.yaml`, `ui_components/*.yaml`, `tab5_custom.cpp`) on 2026-07-06, re-checked 2026-07-14 (info panel, console button, swipe zones). The previous version of this page described a 6-tab, multi-screen navigation bar that no longer exists (and may never have shipped) — see [ADR-0002](decisions/0002-single-page-swipe-navigation.md) for why. If anything below stops matching the running firmware, the firmware is right — fix this page.

---

## Layout overview

There is a **single 1280×720 page** (`page_main`), not a tab-navigated set of screens. Three regions:

1. **Home area** — always visible: clock, indoor sensors, quick actions, compact climate card, plant moisture card.
2. **Central card** — a small area that automatically rotates between planning, rain forecast, weather alerts and an info panel (calendar recap / alert text).
3. **Bottom card region** — either the 5-card weather forecast, or 5 quick-action switch cards, whichever is currently selected.

Two overlays open fullscreen on top of this: the **climate popup** (tap the compact climate card) and the **light popup** (long-press a light shortcut). A **diagnostics console** opens via the console button (`btn_control_console`, top right) — not by swipe since the 14/07/2026 gesture rework.

---

## Home area

Always-visible content at the top of the screen:
- Current time and date
- Indoor temperature and humidity
- Microphone icon with pipeline state color (see Voice assistant below), and a mode toggle (Home Assistant agent vs. conversation/LLM pipeline)
- **Compact climate card** — current temperature (living room + greenhouse/serre sensors) and the target temperature with +/− buttons; tapping the target opens the climate popup (see Climate below)
- **Plant moisture card** — 4 slots for up to 5 BLE soil moisture sensors (see Plant moisture below)

---

## Central card — planning / rain / alerts / info

A single card rotates automatically every 8 seconds (`tab5-globals.yaml`, `interval: 8s`) between up to four panels. The rotation only runs on the default forecast window — swiping to another forecast window replaces the central card with a page-title overlay (`page_title_wrapper`) and pauses the rotation until you swipe back. Tapping a forecast card's temperature (see below) can also interrupt the rotation for a few seconds to show a specific day's schedule.

- **Planning** — always part of the rotation. Shows the day's schedule.
- **Rain forecast** — only rotated in if `has_rain` is true. A short-term rain graph: one data point every 5 minutes for the first 30 minutes, then every 10 minutes for the following 30 minutes (9 points total, 1-hour window), sourced from Météo-France via the `tab5_maj_pluie_1h` API service.
- **Weather alerts** — only rotated in if `has_alerts` is true. Shows one icon per active Météo-France vigilance type (wind, flooding, storms, etc.), each icon colored by its own severity: yellow (vigilance jaune), orange, or red (vigilance rouge) — the official Météo-France color codes, not to be changed.

**The date, not "a day", changes color with the current overall alert level:** independently of the rotation above, the date text under the clock in the home area (`lbl_date`, e.g. "Lun 06 Juil") is recolored every time an alert payload is received, based on the *overall* vigilance level for the day (green/default if none, pale yellow/orange/pale red for jaune/orange/rouge) — see `tab5-api-logic.yaml` in the `tab5_maj_alerte_meteo_france` service. This is separate from the per-type icon coloring in the alert panel above, which uses each alert type's own individual level rather than the overall one.

- **Info panel** — only rotated in if `has_info` is true. Shows either a 3-day calendar recap (multi-line, with inline color markup) or a Météo-France alert banner (single line, colored by severity), pushed by HA via the `tab5_maj_info_texte` service (`update_info_text_ui()`, `tab5_custom.cpp`).

If neither rain, alerts nor info are active, the rotation just keeps planning on screen.

**Temporary override:** tapping the max/min temperature on any of the 5 bottom forecast cards interrupts the rotation for **6 seconds** to show that specific day's opening-hours text in the central card, then automatically restores the previously active panel (`show_temporary_planning()`, `tab5_custom.cpp` — this used to be an ESPHome script in `tab5-scripts.yaml`, moved to C++ in the 12/07 reboot fix).

---

## Bottom card region

One `btn_control_ha` button (top right, house-shaped icon) toggles the entire bottom region between two independent views (`show_switches` global, `tab5-lvgl.yaml`). Toggling is stateful — leaving and returning to a view restores what was showing before.

### Weather view (default)

5 cards, navigated by **left/right swipe**, in 5 windows: 2 hourly + 3 daily (non-wrapping — swiping past the last window does not loop back to the first; see the [false positives note](troubleshooting.md#false-positives-worth-knowing-about-dont-fix-these-again) in `docs/troubleshooting.md`).

**Hourly windows (2):** the next 15 time slots, 5 per window. Each card shows a time label, a two-layer weather condition icon (`IconeMeteo.ttf`), a color-coded temperature, and rainfall in mm (or `-` if dry). There is no separate wind-speed reading — "windy" is one of the possible weather *condition* icons (alongside sun/cloud/rain/snow/fog), not a distinct data field.

**Daily windows (3):** a 15-day forecast, 5 days per window. Each card shows:
- Day name, color-coded (see Color coding below)
- Weather condition icon (same two-layer system)
- Max and min temperature, individually color-coded — **tapping this shows that day's schedule in the central card for 6 seconds** (see above)

**Each of the 5 daily cards also permanently doubles as a quick action button**, independent of the weather content shown on it: tapping the card's icon area triggers one specific Home Assistant action, always the same one per card position — card 1 (today): PC/TV toggle; card 2 (tomorrow): roller shutter; card 3: bedroom light; card 4: living room light; card 5: office LEDs. This works via an invisible button layered over the always-visible weather icon (`btn_j0_action`…`btn_j4_action`), so the weather data keeps showing normally — nothing swaps or disappears when you tap it. This is a separate mechanism from the `btn_control_ha` toggle above: that one replaces the *entire* row with dedicated switch cards; this one is a per-card shortcut that coexists with the weather display.

**Roller shutter card (card 2 / "tomorrow") has one extra control:** tapping its **title** area flips which direction the *next* tap on the action icon will send (open vs. close) and updates the small arrow icon in the card's top-right corner to match — this only changes the on-screen indicator, it does not move the shutter by itself. Tapping the **action icon** (the invisible button over the weather icon, same one as above) sends the actual command: if the shutter is currently moving, it sends `stop`; otherwise it sends `open` or `close` depending on the direction set by the title tap. The arrow icon always shows the *next possible action*, not the shutter's current position.

### Switches view

Toggled in by `btn_control_ha`. 5 dedicated cards, each with a visible icon, a state label (`Allumé`/`Éteint`/`Ouvert`/`Fermé`), and one action button:

| Card | Action |
|------|--------|
| PC Bureau | Calls `script.allumer_pc_tv` (PC + TV toggle) |
| Volet | Same shutter logic as the roller shutter card above: stop if moving, else open/close based on the last-set target direction |
| Chambre | Toggle bedroom light (`light.toggle`) |
| Salon | Toggle living room light (`light.toggle`) |
| LEDs Bureau | Calls `script.allumer_leds` (office LEDs) |

---

## Climate

Two levels of control, both driving the same Home Assistant `climate` entity:

- **Compact card** (always visible in the home area) — current temperature and target with +/− buttons.
- **Climate popup** (fullscreen, opened by tapping the compact card) — a 3×3 grid of 9 buttons: mode (Froid/Chaud/Éteint/Ventil./Sec/Oscill.) and preset (Éco/Boost/Silence), plus the arc thermostat and +/− controls. 6 of the 9 buttons are factorized templates (cool/heat/fan/dry, eco/boost); the remaining 3 (off/swing/quiet) and the +/− buttons are deliberately left as individual YAML — see [ADR-0007](decisions/0007-climate-popup-not-factorized.md).

The controls are dimmed (not hidden) when the AC is off, so the layout stays stable.

---

## Plant moisture card

Monitors up to 5 BLE soil moisture sensors, but only **4 slots are shown** (`sort_and_update_moisture_slots()`, `tab5_custom.cpp`). The sensors are sorted by moisture level (driest to wettest) each update, then mapped to slots as: driest, 2nd-driest, **the median-ranked sensor** (labeled `Moy:` — this shows that one sensor's raw reading, it is not a computed arithmetic average of all 5), and wettest. Because the mapping is by rank rather than by fixed sensor identity, *which* physical pot number appears in which slot changes over time as moisture levels shift — a photo taken today showing "Pot 2 / Pot 4 / Moy / Pot 3" is not a fixed layout.

Each slot shows the sensor's icon and moisture-level color (see Color coding below) plus its physical pot number (or `Moy:` for the median slot).

---

## Voice assistant

The microphone icon on the home screen is the visual interface for the voice assistant. It changes color to reflect the current pipeline state:

| Icon color | State | What's happening |
|-----------|-------|-----------------|
| **Dim grey** | Standby | Wake-word listening is disabled |
| **Grey** | Idle | Listening for "Ok Nabu" in the background |
| **Green** | Listening | Wake word detected — recording your speech |
| **Orange** | Processing | HA pipeline is running STT + intent recognition |
| **Blue** | Speaking | TTS response is playing through the speaker |
| **Red** | Error | Pipeline failed or command not understood |

**Wake-word toggle:** a button on the home screen activates or deactivates "Ok Nabu" wake-word detection. When off, you can still activate the voice assistant by tapping the microphone icon directly (push-to-talk).

**Mode selector:** a small button next to the microphone toggles between two modes:
- **Home Assistant mode** — commands go to the standard HA conversation agent
- **Conversation mode** — commands go to an LLM-backed pipeline for free-form conversation

The mode is saved across reboots via the HA `select` entity (`select.m5stack_tab5_home_assistant_hmi_assistant`).

---

## Console (diagnostics overlay)

Opened via the console button (`btn_control_console`, top right of the home area). A modal card showing live diagnostics — SRAM/PSRAM usage bars, max free block, flash size, uptime, Wi-Fi signal/SSID/IP, CPU temperature, loop time — plus a volume slider and a double-tap reboot button. It is **not** a log viewer (use `esphome logs` for payloads and events). See [`docs/debugging.md`](debugging.md) for a screenshot and more on using it to diagnose issues.

---

## Light popup — long press

Long-pressing the bedroom, living room, or office-LEDs daily-forecast card (instead of the short tap that just toggles the light) opens a fullscreen modal (960×520):
- Left column: **brightness arc** (0–255) — drag to adjust, calls `light.turn_on` with the new brightness in real time
- Right column: a 3×3 grid of 9 buttons — **8 color presets** (Blanc, Chaud, Rouge, Vert, Bleu, Rose, Orange, Cyan — each sends `light.turn_on` with the matching `color_name`) plus one **On/Off** button (`light.toggle`, not a color preset)
- Tapping the dark overlay or the × button closes the modal

The popup is context-aware: the entity it controls is set dynamically at open time (`current_light_entity` global), so the same popup component handles all three light entities without duplication.

---

## Color coding for readability

Color is used consistently as a primary information channel — to let you read state at a glance without reading labels.

**Temperatures:** mapped to a continuous scale via `get_temperature_color()` — blue (cold) → green (comfortable) → orange (warm) → red (hot). Applied identically to indoor sensors and forecast temperatures.

**Day names on daily forecast** (`refresh_daily_forecast()`, `tab5_custom.cpp`):
- Cyan — today
- Green — day off (non-Sunday)
- Amber — Sunday, day off
- Rose/Red — Sunday worked, **or** any day with a shift starting before 09:00 ("early")
- Dim slate — past day (overrides the above once the day has passed)

**Plant moisture (`get_humidity_color()`):**
- Red — ≤ 14% (very dry, needs watering)
- Gradient green → white-ish — 30–80%
- Blue — ≥ 80% (too wet)

**Microphone icon:** see Voice assistant above.

All color constants live in the `UIColor` namespace in `tab5_custom.h`, with YAML-side counterparts in `tab5-styles.yaml`.

---

## Roller shutter control

Two independent places control the same shutter, both calling `script.tab5_volet_action`:
- The **switches view**'s "Volet" card (see above)
- The **weather view**'s "tomorrow" forecast card, which has both the direction-flip (tap the title) and the action button (tap the icon) described above

A `volet_en_mouvement` global tracks whether the shutter is currently moving (a tap sends `stop`); when idle, `volet_target_open` tracks which direction the next tap will send (`open`/`close`). Both places read the same two globals, so they stay in sync with each other.

---


## Version Française

---

Cette page décrit ce que le Tab5 affiche et fait réellement — vérifié contre le firmware (`tab5-lvgl.yaml`, `ui_components/*.yaml`, `tab5_custom.cpp`) le 06/07/2026, re-vérifié le 14/07/2026 (panneau info, bouton console, zones de swipe). L'ancienne version de cette page décrivait une navigation par barre d'onglets à 6 écrans qui n'existe plus (et n'a peut-être jamais été livrée telle quelle) — voir [ADR-0002](decisions/0002-single-page-swipe-navigation.md). Si quelque chose ci-dessous ne correspond plus au firmware réel, c'est le firmware qui a raison — corrigez cette page.

---

## Vue d'ensemble de la mise en page

Il y a une **page unique 1280×720** (`page_main`), pas un jeu d'écrans navigués par onglets. Trois zones :

1. **Zone d'accueil** — toujours visible : horloge, capteurs intérieurs, actions rapides, carte clim compacte, carte humidité plantes.
2. **Carte centrale** — une petite zone qui alterne automatiquement entre planning, prévision de pluie, alertes météo et un panneau info (récap calendrier / texte d'alerte).
3. **Zone de cartes du bas** — soit les 5 cartes prévisions météo, soit 5 cartes d'action rapide, selon ce qui est sélectionné.

Deux overlays s'ouvrent en plein écran par-dessus : le **popup clim** (tap sur la carte clim compacte) et le **popup lumière** (appui long sur un raccourci lumière). Une **console diagnostics** s'ouvre via le bouton console (`btn_control_console`, en haut à droite) — plus par swipe depuis la refonte gestuelle du 14/07/2026.

---

## Zone d'accueil

Contenu toujours visible en haut de l'écran :
- Heure et date actuelles
- Température et humidité intérieure
- Icône microphone avec couleur d'état du pipeline (voir Assistant vocal ci-dessous), et un bouton de bascule de mode (agent Home Assistant vs pipeline conversation/LLM)
- **Carte clim compacte** — température actuelle (capteurs salon + serre) et température cible avec boutons +/− ; taper sur la cible ouvre le popup clim (voir Climatisation ci-dessous)
- **Carte humidité plantes** — 4 emplacements pour jusqu'à 5 capteurs BLE d'humidité du sol (voir Humidité des plantes ci-dessous)

---

## Carte centrale — planning / pluie / alertes / info

Une seule carte alterne automatiquement toutes les 8 secondes (`tab5-globals.yaml`, `interval: 8s`) entre jusqu'à quatre panneaux. La rotation ne tourne que sur la fenêtre prévisions par défaut — swiper vers une autre fenêtre remplace la carte centrale par un overlay de titre de page (`page_title_wrapper`) et met la rotation en pause. Taper sur la température d'une carte prévision (voir plus bas) peut aussi interrompre la rotation quelques secondes pour montrer le planning d'un jour précis.

- **Planning** — toujours dans la rotation. Affiche le planning du jour.
- **Prévision de pluie** — intégrée à la rotation seulement si `has_rain` est vrai. Un graphique de pluie à court terme : un point toutes les 5 minutes pour la première demi-heure, puis toutes les 10 minutes pour la demi-heure suivante (9 points au total, fenêtre d'1 heure), fourni par Météo-France via le service API `tab5_maj_pluie_1h`.
- **Alertes météo** — intégrée à la rotation seulement si `has_alerts` est vrai. Affiche une icône par type de vigilance Météo-France actif (vent, inondation, orages, etc.), chaque icône colorée selon sa propre sévérité : jaune (vigilance jaune), orange, ou rouge (vigilance rouge) — les codes couleur officiels Météo-France, à ne pas modifier.

**C'est la date, pas "un jour", qui prend la couleur du niveau d'alerte global en cours :** indépendamment de la rotation ci-dessus, le texte de la date sous l'horloge en zone d'accueil (`lbl_date`, ex. "Lun 06 Juil") est recoloré à chaque réception d'un payload d'alerte, selon le niveau de vigilance *global* du jour (vert/défaut si aucune, jaune pâle/orange/rouge pâle pour jaune/orange/rouge) — voir `tab5-api-logic.yaml` dans le service `tab5_maj_alerte_meteo_france`. C'est distinct de la coloration par icône du panneau d'alerte ci-dessus, qui utilise le niveau propre à chaque type d'alerte plutôt que le niveau global.

- **Panneau info** — intégré à la rotation seulement si `has_info` est vrai. Affiche soit un récap calendrier 3 jours (multi-lignes, avec balisage couleur inline), soit une bannière d'alerte Météo-France (une ligne, colorée selon la sévérité), poussé par HA via le service `tab5_maj_info_texte` (`update_info_text_ui()`, `tab5_custom.cpp`).

Si ni pluie, ni alerte, ni info ne sont actives, la rotation garde simplement le planning à l'écran.

**Bascule temporaire :** taper sur la température max/min de l'une des 5 cartes prévisions du bas interrompt la rotation pendant **6 secondes** pour afficher le texte des horaires de ce jour précis dans la carte centrale, puis restaure automatiquement le panneau qui était actif (`show_temporary_planning()`, `tab5_custom.cpp` — anciennement un script ESPHome de `tab5-scripts.yaml`, passé en C++ lors du fix reboot du 12/07).

---

## Zone de cartes du bas

Un bouton `btn_control_ha` (en haut à droite, icône en forme de maison) bascule toute la zone du bas entre deux vues indépendantes (global `show_switches`, `tab5-lvgl.yaml`). La bascule est étatée — quitter puis revenir à une vue restaure ce qui était affiché avant.

### Vue météo (par défaut)

5 cartes, navigables par **swipe gauche/droite**, sur 5 fenêtres : 2 horaires + 3 journalières (sans bouclage — swiper au-delà de la dernière fenêtre ne revient pas à la première ; voir la [note faux positifs](troubleshooting.md#false-positives-worth-knowing-about-dont-fix-these-again) dans `docs/troubleshooting.md`).

**Fenêtres horaires (2) :** les 15 prochaines tranches horaires, 5 par fenêtre. Chaque carte affiche une heure, une icône météo double couche (`IconeMeteo.ttf`), une température avec code couleur, et la pluie en mm (ou `-` si sec). Il n'y a pas de donnée de vitesse de vent séparée — "venteux" est l'une des icônes de *condition* météo possibles (à côté de soleil/nuage/pluie/neige/brouillard), pas un champ de donnée distinct.

**Fenêtres journalières (3) :** prévisions sur 15 jours, 5 jours par fenêtre. Chaque carte affiche :
- Nom du jour, avec code couleur (voir Coloration ci-dessous)
- Icône météo (même système double couche)
- Températures max et min, chacune avec code couleur — **taper dessus affiche le planning de ce jour dans la carte centrale pendant 6 secondes** (voir ci-dessus)

**Chacune des 5 cartes journalières double aussi en permanence comme bouton d'action rapide**, indépendamment du contenu météo affiché : taper sur la zone icône de la carte déclenche une action Home Assistant précise, toujours la même selon la position de la carte — carte 1 (aujourd'hui) : bascule PC/TV ; carte 2 (demain) : volet roulant ; carte 3 : lumière chambre ; carte 4 : lumière salon ; carte 5 : LEDs bureau. Ça fonctionne via un bouton invisible superposé à l'icône météo toujours visible (`btn_j0_action`…`btn_j4_action`), donc la donnée météo continue de s'afficher normalement — rien ne change ni ne disparaît visuellement quand on tape. C'est un mécanisme séparé de la bascule `btn_control_ha` ci-dessus : celle-ci remplace *toute* la rangée par des cartes switches dédiées ; celui-ci est un raccourci par carte qui coexiste avec l'affichage météo.

**La carte volet roulant (carte 2 / "demain") a un contrôle supplémentaire :** taper sur son **titre** inverse le sens que le *prochain* tap sur l'icône d'action enverra (ouvrir vs fermer) et met à jour la petite icône flèche en haut à droite de la carte en conséquence — ça change seulement l'indicateur affiché, ça ne fait pas bouger le volet en soi. Taper sur l'**icône d'action** (le bouton invisible sur l'icône météo, le même que ci-dessus) envoie la vraie commande : si le volet est en mouvement, ça envoie `stop` ; sinon ça envoie `open` ou `close` selon le sens réglé par le tap sur le titre. L'icône flèche montre toujours l'*action possible suivante*, pas la position actuelle du volet.

### Vue switches

Activée par `btn_control_ha`. 5 cartes dédiées, chacune avec une icône visible, un label d'état (`Allumé`/`Éteint`/`Ouvert`/`Fermé`), et un bouton d'action :

| Carte | Action |
|-------|--------|
| PC Bureau | Appelle `script.allumer_pc_tv` (bascule PC + TV) |
| Volet | Même logique que la carte volet roulant ci-dessus : stop si en mouvement, sinon ouvre/ferme selon le dernier sens réglé |
| Chambre | Bascule la lumière chambre (`light.toggle`) |
| Salon | Bascule la lumière salon (`light.toggle`) |
| LEDs Bureau | Appelle `script.allumer_leds` |

---

## Climatisation

Deux niveaux de contrôle, pilotant tous deux la même entité `climate` de Home Assistant :

- **Carte compacte** (toujours visible en zone d'accueil) — température actuelle et cible avec boutons +/−.
- **Popup clim** (plein écran, ouvert en tapant la carte compacte) — une grille 3×3 de 9 boutons : mode (Froid/Chaud/Éteint/Ventil./Sec/Oscill.) et preset (Éco/Boost/Silence), plus l'arc thermostat et les boutons +/−. 6 des 9 boutons sont des templates factorisés (froid/chaud/ventil/sec, éco/boost) ; les 3 restants (éteint/oscill/silence) et les boutons +/− sont volontairement laissés en YAML individuel — voir [ADR-0007](decisions/0007-climate-popup-not-factorized.md).

Les contrôles sont estompés (non cachés) quand le clim est éteint, pour garder la mise en page stable.

---

## Carte humidité des plantes

Surveille jusqu'à 5 capteurs BLE d'humidité du sol, mais seuls **4 emplacements sont affichés** (`sort_and_update_moisture_slots()`, `tab5_custom.cpp`). Les capteurs sont triés par niveau d'humidité (du plus sec au plus humide) à chaque mise à jour, puis mappés sur les emplacements ainsi : le plus sec, le 2e plus sec, **le capteur de rang médian** (étiqueté `Moy:` — ça affiche la lecture brute de ce capteur précis, ce n'est pas une moyenne arithmétique calculée sur les 5), et le plus humide. Comme le mapping se fait par rang plutôt que par identité fixe du capteur, *quel* numéro de pot physique apparaît dans quel emplacement change dans le temps selon l'évolution de l'humidité — une photo prise aujourd'hui montrant "Pot 2 / Pot 4 / Moy / Pot 3" n'est pas une disposition figée.

Chaque emplacement affiche l'icône du capteur et sa couleur de niveau d'humidité (voir Coloration ci-dessous) plus son numéro de pot physique (ou `Moy:` pour l'emplacement médian).

---

## Assistant vocal

L'icône microphone sur l'écran d'accueil est l'interface visuelle de l'assistant vocal. Elle change de couleur pour refléter l'état courant du pipeline :

| Couleur de l'icône | État | Ce qui se passe |
|-------------------|------|----------------|
| **Gris sombre** | En veille | Écoute wake-word désactivée |
| **Gris** | Repos | Écoute "Ok Nabu" en arrière-plan |
| **Vert** | Écoute | Wake word détecté — enregistrement en cours |
| **Orange** | Traitement | Pipeline HA exécute STT + reconnaissance d'intention |
| **Bleu** | Synthèse | Réponse TTS en lecture sur le haut-parleur |
| **Rouge** | Erreur | Pipeline échoué ou commande non comprise |

**Bascule wake-word :** un bouton sur l'écran d'accueil active ou désactive la détection "Ok Nabu". Quand désactivé, on peut toujours activer l'assistant en tapant directement sur l'icône microphone (push-to-talk).

**Sélecteur de mode :** un petit bouton à côté du microphone bascule entre deux modes :
- **Mode Home Assistant** — les commandes vont vers l'agent de conversation standard de HA
- **Mode Conversation** — les commandes vont vers un pipeline basé sur un LLM

Le mode est sauvegardé entre les redémarrages via l'entité HA `select` (`select.m5stack_tab5_home_assistant_hmi_assistant`).

---

## Console (overlay diagnostics)

Ouvert via le bouton console (`btn_control_console`, en haut à droite de la zone d'accueil). Une carte modale montrant les diagnostics en direct — barres SRAM/PSRAM, bloc max, taille flash, uptime, signal Wi-Fi/SSID/IP, température CPU, temps de boucle — plus un slider volume et un reboot par double-tap. Ce n'est **pas** un visualiseur de logs (utiliser `esphome logs` pour les payloads et événements). Voir [`docs/debugging.md`](debugging.md) pour une capture d'écran et plus de détails sur son usage en debug.

---

## Popup lumière — appui long

Un appui long sur la carte prévision journalière chambre, salon ou LEDs bureau (au lieu du tap court qui bascule juste la lumière) ouvre un modal plein écran (960×520) :
- Colonne gauche : **arc de luminosité** (0–255) — à glisser pour ajuster, appelle `light.turn_on` avec la nouvelle luminosité en temps réel
- Colonne droite : une grille 3×3 de 9 boutons — **8 couleurs prédéfinies** (Blanc, Chaud, Rouge, Vert, Bleu, Rose, Orange, Cyan — chacune envoie `light.turn_on` avec le `color_name` correspondant) plus un bouton **On/Off** (`light.toggle`, pas une couleur prédéfinie)
- Taper l'overlay sombre ou le bouton × ferme le modal

Le popup est contextuel : l'entité qu'il contrôle est définie dynamiquement à l'ouverture (globale `current_light_entity`), donc le même composant popup gère les trois entités lumière sans duplication.

---

## Coloration sémantique

La couleur est utilisée de façon systématique comme canal d'information primaire — pour lire l'état d'un coup d'œil sans lire les labels.

**Températures :** mappées sur une échelle continue via `get_temperature_color()` — bleu (froid) → vert (confortable) → orange (chaud) → rouge (très chaud). Appliqué identiquement aux capteurs intérieurs et aux températures des prévisions.

**Noms des jours en prévisions journalières** (`refresh_daily_forecast()`, `tab5_custom.cpp`) :
- Cyan — aujourd'hui
- Vert — jour de repos (hors dimanche)
- Ambre — dimanche, repos
- Rose/Rouge — dimanche travaillé, **ou** tout jour avec une prise de service avant 09:00 ("service tôt")
- Ardoise estompée — jour passé (prend le dessus sur les couleurs précédentes une fois le jour passé)

**Humidité des plantes (`get_humidity_color()`) :**
- Rouge — ≤ 14% (très sec, besoin d'arrosage)
- Dégradé vert → blanc cassé — 30–80%
- Bleu — ≥ 80% (trop humide)

**Icône microphone :** voir Assistant vocal ci-dessus.

Toutes les constantes de couleur vivent dans le namespace `UIColor` de `tab5_custom.h`, avec leurs équivalents YAML dans `tab5-styles.yaml`.

---

## Contrôle du volet roulant

Deux endroits indépendants contrôlent le même volet, appelant tous deux `script.tab5_volet_action` :
- La carte "Volet" de la **vue switches** (voir ci-dessus)
- La carte prévision "demain" de la **vue météo**, qui a à la fois l'inversion de sens (tap sur le titre) et le bouton d'action (tap sur l'icône) décrits ci-dessus

Une globale `volet_en_mouvement` suit si le volet est en mouvement (un tap envoie `stop`) ; à l'arrêt, `volet_target_open` suit quel sens le prochain tap enverra (`open`/`close`). Les deux endroits lisent les mêmes deux globales, donc ils restent synchronisés entre eux.
