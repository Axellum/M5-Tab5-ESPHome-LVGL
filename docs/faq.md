# Frequently asked questions: M5Stack Tab5 and Waveshare 7B Home Assistant screen

## English · [Français](#version-française)

---

Short answers about the Home Assistant wall screen that runs on the M5Stack Tab5 and the Waveshare ESP32-P4 7B. Each one points to the page that has the detail. Not here? Look at the [known incidents](troubleshooting.md), or ask in the [Discussions](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions).

## The project

### What is this project?

A Home Assistant wall screen that runs natively on the M5Stack Tab5 (ESP32-P4), as firmware written with ESPHome and LVGL. There is no browser and no polling: Home Assistant pushes what changed, and the screen redraws only that ([ADR-0001](decisions/0001-push-only-zero-polling.md)). It is the author's everyday screen, shared in case it is useful to someone. The [architecture](architecture.md) page explains how it is built.

### What can it show, and how do I use it?

One page with the weather (rain in the next hour, a 15-day forecast), a central card with the schedule and your alerts, rooms, climate, lights, shutters, plants, a TV remote, voice, a calendar and an alarm clock. Energy, music, cameras and eight offline games come on top. Windows open with a tap, a long press or a swipe: the [user manual](notice/README.md) shows where everything is, one page per part of the screen.

### Which screens does it run on?

On the M5Stack Tab5 (1280×720). Its display chip is printed on the sticker on the back, just above the Espressif logo: the ST7123 is the author's first tablet and is supported, the ST7121 is the author's second tablet (since 21 October 2026) and another user's, and the ILI9881C compiles but has never been tried ([install the firmware](installation/flash.md)). It also runs on the 7-inch Waveshare ESP32-P4 7B (1024×600), which is supported ([Waveshare 7B](waveshare-7b.md)).

### Who made it, and can I reuse it?

It is a personal project: the author is more the architect than the writer, and the code is written with AI tools and tried on real screens one at a time. It is shared under the MIT licence, in case it is useful to someone. [The story](story.md) tells how it came about.

## Installing

### What do I need before I start?

A Tab5 (or a Waveshare 7B) and the name of its display chip, a computer with Chrome or Edge and a USB-C cable that carries data, and Home Assistant 2026.8 or newer with a way to copy files into its `config/` folder. You also need a 2.4 GHz Wi-Fi network and a USB-C power source. A weather integration, your calendars and a voice assistant are optional ([what you need](installation/README.md#what-you-need)).

### How do I install it? Do I have to compile?

No: since 3.0 a ready-made, signed firmware installs from the browser, from the [install page](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/). The [guide](installation/README.md#the-seven-steps) has seven steps, 1 to 6 needed and 7 optional: the Home Assistant files, the firmware over USB, the Wi-Fi, adding the tablet to Home Assistant, your sources, your devices, and a dashboard. To change the firmware yourself, [build your own](installation/build.md) instead of step 2.

### How do I give the tablet my Wi-Fi?

The firmware holds no Wi-Fi password, so you give it yours in one of two ways: over USB right after the install, from the install page (*Connect to Wi-Fi*), or with a phone on the open « Tab5 Fallback AP » network. That network also comes back whenever the tablet loses its Wi-Fi for a minute, to give it another one. The tablet joins 2.4 GHz networks only ([step 3, Wi-Fi](installation/wifi.md)).

### Is there a password or a key in the firmware?

No: since 3.0 the firmware holds no secret. The Wi-Fi comes from the install page or the fallback network, the API key from Home Assistant, and over the air the tablet only accepts a firmware signed with the project key. The fallback network is open on purpose: someone within range can change the network of a tablet that lost its own, but can neither flash it nor talk to its API ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md)).

### Home Assistant does not find the tablet, or nothing shows up: what do I check?

A tablet that Home Assistant has not met yet accepts its first contact only during the 30 minutes after its start: check that it has its Wi-Fi, and restart it to open the window again. If no « Tab5 · » entity appears, check that the three folders went straight into `config/`, that the `packages:` line is in `configuration.yaml` and that Home Assistant was restarted. A clock with no weather, rooms or tiles is normal until steps 5 and 6 are done ([nothing shows up?](installation/README.md#first-installation-nothing-shows-up)).

### How do I update?

In this order: the Home Assistant files first, then the firmware. The firmware comes from Home Assistant: the tablet's « Firmware » entity reads the published manifest every 6 hours, and *Install* downloads the image, which the tablet checks against the project key. The Home Assistant files update in one click with the HACS integration, otherwise by hand ([updates](installation/updates.md)).

## Home Assistant and your home

### Does it work without Home Assistant?

Not for your home: the tablet gets what it shows from the Home Assistant files, and without them nothing of your home reaches the screen. If Home Assistant goes down, the last known state stays on screen and the clock, the alarm clock, the games and the diagnostics console stay usable. The tablet restarts by itself only after a full hour without any Home Assistant connection ([ADR-0011](decisions/0011-api-reboot-timeout-60min.md)).

### Can I try it without Home Assistant?

Yes, on a flashed tablet: the [demo mode](demo_mode.md) runs a small script on your computer that pushes demo data (weather, forecast, climate, plants, weather alerts) the way Home Assistant would. There is no Home Assistant to install and no account to create; stop the script with `Ctrl+C` and nothing is left to clean up except the flashed device. The voice, the calendar, the music players and the cameras need a real Home Assistant, so the demo does not show them.

### How do I choose my rooms, lights and sensors?

In Home Assistant, with the mouse, and without flashing again. Your sources (weather, calendars, phone, music players, tracked sensors) are the « Tab5 · » lists ([your sources](installation/sources.md)); your home (rooms, tiles, climate, temperatures, energy, cameras, what each zone shows and what each gesture does) is one automation made from the blueprint ([your devices](installation/devices.md)). Every field is optional, and what you don't have disappears from the screen ([adapt to your home](installation/adapt-to-your-home.md)).

### Where does the weather come from?

You pick the source in Home Assistant. For the rain in the next hour: Météo-France in France, the Buienradar, DWD or Met.no radars, OpenWeatherMap, or Open-Meteo, which works everywhere with no key. Weather warnings come from Météo-France, MeteoAlarm, the DWD or CAP Alerts; apart from Météo-France, they were only tried with simulated data ([weather providers](installation/weather.md)).

## On the screen

### Which languages does the screen speak?

French, English, German, Dutch, Spanish, Italian and Turkish, picked with the « Language » select in Home Assistant: the tablet restarts and keeps the choice. Every language was written by an AI, and the author checked the French only. The quiz questions of one game stay in French ([choosing the language](translations.md#choosing-the-language)).

### Can I change the colours?

Yes: there are 21 themes, light or dark, picked from the tablet's device page in Home Assistant, from the dashboard or from the tablet's own Settings window. The screen repaints at once, without a restart. « Auto » turns light by day and dark at night, following the sun at your Home Assistant's location ([theme, light or dark](installation/settings.md#theme-light-or-dark)).

### How does the voice assistant work?

Say « Okay Nabu », or tap the microphone, then speak: the tablet detects the wake word itself, then sends your voice to the voice assistant of Home Assistant (speech-to-text, conversation agent, text-to-speech). Two modes pick who answers: Domo for Home Assistant commands, Discu for a conversation assistant. A tap on the microphone cuts a reply and listens again ([voice](notice/voice.md), [voice pipeline](voice_assistant.md)).

### Is the tablet always listening?

It listens all the time for the wake word, on the tablet itself, and no audio is sent over the network during that idle listening. Audio leaves the tablet only after « Okay Nabu » or a tap on the microphone. The wake word can be switched off on the home screen (« Ok Nabu: ON / OFF »): a tap on the microphone then does the listening ([wake-word detection](voice_assistant.md#wake-word-detection)).

## The hardware

### How is it powered, and does it need a battery?

The Tab5 is powered through USB-C. M5Stack gives no input rating: a 5 V / 2 A charger is advice, not a requirement, and the author's tablet runs without a battery on a PC's USB port. The Tab5 takes an optional two-cell (2S) lithium battery ([power](hardware.md#power)).

### What is different on the Waveshare 7B?

It is a cheaper and bigger 7-inch screen (1024×600) built around the same ESP32-P4 and ESP32-C6, with less on board: no motion sensor, so the eight games are left out by default, no clock chip, so the time comes from the network at each start, and no battery gauge. The home screen is the Tab5's layout brought to 1024×600. Its firmware installs from the same install page since 3.9.0 ([what changes on screen](waveshare-7b.md#what-changes-on-screen)).

---

## Version Française

---

Des réponses courtes sur l'écran mural Home Assistant qui tourne sur le M5Stack Tab5 et sur la Waveshare ESP32-P4 7B. Chacune renvoie à la page qui détaille. Pas ici ? Regardez les [incidents connus](troubleshooting.md#version-française), ou posez la question dans les [Discussions](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions).

## Le projet

### Qu'est-ce que ce projet ?

Un écran mural Home Assistant qui tourne en natif sur le M5Stack Tab5 (ESP32-P4), sous forme d'un firmware écrit avec ESPHome et LVGL. Pas de navigateur et pas d'interrogation répétée : Home Assistant envoie ce qui a changé, et l'écran ne redessine que cela ([ADR-0001](decisions/0001-push-only-zero-polling.md), en anglais). C'est l'écran de tous les jours de l'auteur, partagé au cas où il servirait à quelqu'un. La page [architecture](architecture.md#version-française) explique comment il est construit.

### Que montre-t-il, et comment s'en sert-on ?

Une seule page avec la météo (la pluie dans l'heure, des prévisions sur 15 jours), une carte centrale avec le planning et vos alertes, des pièces, la clim, les lumières, les volets, les plantes, une télécommande TV, la voix, un calendrier et un réveil. S'y ajoutent l'énergie, la musique, les caméras et huit jeux hors ligne. Les fenêtres s'ouvrent par un tap, un appui long ou un glissement : le [mode d'emploi](notice/README.md#version-française) montre où est chaque chose, une page par partie de l'écran.

### Sur quels écrans tourne-t-il ?

Sur le M5Stack Tab5 (1280×720). La puce d'écran est imprimée sur l'étiquette au dos, juste au-dessus du logo Espressif : la ST7123 est la première tablette de l'auteur et est prise en charge, la ST7121 est sa deuxième tablette (depuis le 21/10/2026) et celle d'un autre utilisateur, et l'ILI9881C compile mais n'a jamais été essayée ([installer le firmware](installation/flash.md#version-française)). Il tourne aussi sur la Waveshare ESP32-P4 7B de 7 pouces (1024×600), prise en charge ([Waveshare 7B](waveshare-7b.md#version-française)).

### Qui l'a fait, et puis-je le réutiliser ?

C'est un projet personnel : l'auteur est plus l'architecte que le rédacteur, et le code est écrit avec des outils d'IA et essayé sur de vrais écrans un par un. Il est partagé sous licence MIT, au cas où il servirait à quelqu'un. [L'histoire](story.md#version-française) raconte comment il est né.

## L'installation

### Que me faut-il avant de commencer ?

Un Tab5 (ou une Waveshare 7B) et le nom de sa puce d'écran, un ordinateur avec Chrome ou Edge et un câble USB-C qui transporte des données, et Home Assistant 2026.8 ou plus récent avec un moyen de copier des fichiers dans son dossier `config/`. Il faut aussi un réseau Wi-Fi en 2,4 GHz et une source d'alimentation USB-C. Une intégration météo, vos agendas et un assistant vocal sont facultatifs ([ce qu'il faut](installation/README.md#ce-quil-faut)).

### Comment l'installer ? Faut-il compiler ?

Non : depuis la 3.0, un firmware prêt à l'emploi et signé s'installe depuis le navigateur, par la [page d'installation](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/). Le [guide](installation/README.md#les-sept-étapes) compte sept étapes, de 1 à 6 nécessaires et la 7 facultative : les fichiers Home Assistant, le firmware par l'USB, le Wi-Fi, l'ajout de la tablette à Home Assistant, vos sources, vos appareils, et un tableau de bord. Pour modifier le firmware vous-même, [compilez le vôtre](installation/build.md#version-française) à la place de l'étape 2.

### Comment donner mon Wi-Fi à la tablette ?

Le firmware ne contient aucun mot de passe Wi-Fi : vous lui donnez le vôtre de deux façons, par l'USB juste après l'installation, depuis la page d'installation (*Connect to Wi-Fi*), ou avec un téléphone sur le réseau ouvert « Tab5 Fallback AP ». Ce réseau revient aussi dès que la tablette perd son Wi-Fi une minute, pour lui en donner un autre. La tablette ne rejoint que des réseaux en 2,4 GHz ([étape 3, Wi-Fi](installation/wifi.md#version-française)).

### Y a-t-il un mot de passe ou une clé dans le firmware ?

Non : depuis la 3.0, le firmware ne contient aucun secret. Le Wi-Fi vient de la page d'installation ou du réseau de secours, la clé de l'API est donnée par Home Assistant, et une mise à jour par le Wi-Fi n'est acceptée que si le firmware est signé avec la clé du projet. Le réseau de secours est ouvert exprès : quelqu'un à portée peut changer le réseau d'une tablette qui a perdu le sien, mais ne peut ni la flasher ni parler à son API ([ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md), en anglais).

### Home Assistant ne trouve pas la tablette, ou rien ne s'affiche : que vérifier ?

Une tablette que Home Assistant n'a pas encore rencontrée n'accepte son premier contact que pendant les 30 minutes qui suivent son démarrage : vérifiez qu'elle a son Wi-Fi, et redémarrez-la pour rouvrir la fenêtre. Si aucune entité « Tab5 · » n'apparaît, vérifiez que les trois dossiers sont allés directement dans `config/`, que la ligne `packages:` est dans `configuration.yaml` et que Home Assistant a été redémarré. Une horloge sans météo, pièces ni tuiles est normale tant que les étapes 5 et 6 ne sont pas faites ([rien ne s'affiche ?](installation/README.md#première-installation--rien-napparaît-)).

### Comment la mettre à jour ?

Dans cet ordre : les fichiers Home Assistant d'abord, puis le firmware. Le firmware vient de Home Assistant : l'entité « Firmware » de la tablette lit le manifeste publié toutes les 6 heures, et *Install* télécharge l'image, que la tablette vérifie avec la clé du projet. Les fichiers Home Assistant se mettent à jour en un clic avec l'intégration HACS, sinon à la main ([mises à jour](installation/updates.md#version-française)).

## Home Assistant et votre maison

### Marche-t-il sans Home Assistant ?

Pas pour votre maison : la tablette reçoit ce qu'elle montre des fichiers Home Assistant, et sans eux rien de votre maison n'arrive à l'écran. Si Home Assistant tombe, le dernier état connu reste affiché et l'horloge, le réveil, les jeux et la console de diagnostic restent utilisables. La tablette ne redémarre d'elle-même qu'après une heure entière sans aucune connexion à Home Assistant ([ADR-0011](decisions/0011-api-reboot-timeout-60min.md), en anglais).

### Puis-je l'essayer sans Home Assistant ?

Oui, sur une tablette flashée : le [mode démo](demo_mode.md#version-française) lance sur votre ordinateur un petit script qui envoie des données de démonstration (météo, prévisions, clim, plantes, alertes météo) comme le ferait Home Assistant. Pas de Home Assistant à installer ni de compte à créer ; arrêtez le script avec `Ctrl+C` et il ne reste rien à nettoyer, sauf l'appareil flashé. La voix, le calendrier, les lecteurs de musique et les caméras demandent un vrai Home Assistant : la démo ne les montre pas.

### Comment choisir mes pièces, mes lumières et mes capteurs ?

Dans Home Assistant, à la souris, sans reflasher. Vos sources (météo, agendas, téléphone, lecteurs de musique, capteurs suivis) sont les listes « Tab5 · » ([vos sources](installation/sources.md#version-française)) ; votre maison (pièces, tuiles, clim, températures, énergie, caméras, ce que montre chaque zone et ce que fait chaque geste) est une automatisation faite à partir du blueprint ([vos appareils](installation/devices.md#version-française)). Chaque champ est facultatif, et ce que vous n'avez pas disparaît de l'écran ([adapter à votre maison](installation/adapt-to-your-home.md#version-française)).

### D'où vient la météo ?

Vous choisissez la source dans Home Assistant. Pour la pluie dans l'heure : Météo-France en France, les radars Buienradar, DWD ou Met.no, OpenWeatherMap, ou Open-Meteo, qui marche partout sans clé. Les vigilances viennent de Météo-France, de MeteoAlarm, du DWD ou de CAP Alerts ; hors Météo-France, elles n'ont été essayées qu'avec des données simulées ([fournisseurs météo](installation/weather.md#version-française)).

## À l'écran

### Dans quelles langues parle l'écran ?

Français, anglais, allemand, néerlandais, espagnol, italien et turc, choisis avec la liste « Language » dans Home Assistant : la tablette redémarre et garde le choix. Chaque langue a été écrite par une IA, et l'auteur n'a relu que le français. Les questions du quiz d'un jeu restent en français ([choisir la langue](translations.md#choisir-la-langue)).

### Puis-je changer les couleurs ?

Oui : il y a 21 thèmes, clairs ou sombres, choisis depuis la page de l'appareil de la tablette dans Home Assistant, depuis le tableau de bord ou depuis la fenêtre Réglages de la tablette. L'écran se repeint tout de suite, sans redémarrage. « Auto » passe en clair le jour et en sombre la nuit, selon le soleil à l'emplacement de votre Home Assistant ([thème, clair ou sombre](installation/settings.md#thème-clair-ou-sombre)).

### Comment marche l'assistant vocal ?

Dites « Okay Nabu », ou touchez le micro, puis parlez : la tablette détecte elle-même le mot de réveil, puis envoie votre voix à l'assistant vocal de Home Assistant (reconnaissance de la parole, agent de conversation, synthèse vocale). Deux modes choisissent qui répond : Domo pour les commandes de Home Assistant, Discu pour un assistant de conversation. Un tap sur le micro coupe une réponse et réécoute ([voix](notice/voice.md#version-française), [pipeline vocal](voice_assistant.md#version-française)).

### La tablette écoute-t-elle en permanence ?

Elle guette le mot de réveil en permanence, sur la tablette elle-même, et aucun son n'est envoyé sur le réseau pendant cette écoute au repos. Le son ne quitte la tablette qu'après « Okay Nabu » ou un tap sur le micro. Le mot de réveil se coupe sur l'écran d'accueil (« Ok Nabu : ON / OFF ») : un tap sur le micro fait alors l'écoute ([détection du mot de réveil](voice_assistant.md#détection-du-wake-word)).

## Le matériel

### Comment est-elle alimentée, et faut-il une batterie ?

Le Tab5 est alimenté par l'USB-C. M5Stack ne donne aucune valeur d'entrée : un chargeur de 5 V / 2 A est un conseil, pas une exigence, et la tablette de l'auteur fonctionne sans batterie sur le port USB d'un PC. Le Tab5 accepte une batterie lithium à deux éléments (2S) en option ([alimentation](hardware.md#alimentation)).

### Qu'est-ce qui change sur la Waveshare 7B ?

C'est un écran de 7 pouces (1024×600) moins cher et plus grand, construit autour du même ESP32-P4 et du même ESP32-C6, avec moins de choses à bord : pas de capteur de mouvement, donc les huit jeux sont écartés par défaut, pas de puce d'horloge, donc l'heure vient du réseau à chaque démarrage, et pas de jauge de batterie. L'écran d'accueil est la disposition du Tab5 ramenée à 1024×600. Son firmware s'installe depuis la même page d'installation depuis la 3.9.0 ([ce qui change à l'écran](waveshare-7b.md#ce-qui-change-à-lécran)).
