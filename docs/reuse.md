# Start your own ESPHome screen from this project

## English · [Français](#version-française)

---

You want a Home Assistant dashboard on an ESP32 touch screen with ESPHome and LVGL, but not this exact tablet. This page says what you can take from this project, how a second board was added to it, and what only holds on the M5Stack Tab5 and the ESP32-P4. Everything below was built and run on two ESP32-P4 boards (the M5Stack Tab5 and the Waveshare ESP32-P4 7B); nothing was tried on another chip.

### What you can reuse as it is

**The code rules.** Ten numbered rules, each born from a real bug or a slow screen, most of them checked by a test (`tools/check_tab5_code_rules.py`, run by `pytest`). Seven of them hold for any ESPHome + LVGL firmware; the full text is in [`Tab5/AGENTS.md`](../Tab5/AGENTS.md):

| Rule | Why it matters on any ESPHome + LVGL screen |
|---|---|
| 1. No colour set on a widget, a role style instead | ESPHome freezes a `text_color:` / `bg_color:` at compile time: a theme can only change colours that come from a shared style |
| 2. A `sensor:` never touches `lv_obj_*`, it calls a named C++ function | the drawing logic stays in one place and can be tested ([ADR-0006](decisions/0006-centralize-lvgl-logic-in-cpp.md)) |
| 3. No `static` inside a lambda for shared state | each lambda is a separate function: use `globals:` |
| 4. No `std::string` by value in a hot path | sliders and frequent updates allocate on every call otherwise |
| 5. A widget repeated three times or more goes through a builder or an `!include` with `vars` | one fix instead of N copies |
| 9. Every MDI icon shown is in its font's `glyphs:`, every glyph listed is shown | a missing glyph draws an empty box, with no build error |
| 10. Our C++ is called from a `lambda:` action, never from an event field or an "apply" action value | since ESPHome 2026.10.0 those values are written before the `includes:` and do not see your C++ ([esphome/esphome#21303](https://github.com/esphome/esphome/issues/21303)) |

The three others are this project's own choices: the shared popup chrome (rule 7, [ADR-0009](decisions/0009-modal-shell-header.md)), no Home Assistant entity or action in the firmware, only events (rule 8, [ADR-0025](decisions/0025-events-only.md)), and `esphome compile` before every commit (rule 6). They are worth copying, but your project can live without them.

**The design decisions that do not depend on the board** ([all of them](decisions/README.md)):

- [ADR-0001](decisions/0001-push-only-zero-polling.md): Home Assistant pushes what changed, the screen never polls.
- [ADR-0003](decisions/0003-data-packing-delimited-strings.md): one service call carries a whole list as a delimited string, instead of one call per value.
- [ADR-0004](decisions/0004-no-alpha-png-prebaked-backgrounds.md): no PNG with transparency, backgrounds baked opaque.
- [ADR-0019](decisions/0019-logical-slots-blueprint.md): the screen knows slots, a Home Assistant blueprint maps them to your entities; changing a light needs no flash.
- [ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md): no secret in the firmware, Home Assistant provides the API key, updates are signed.
- [ADR-0021](decisions/0021-host-render-stubs.md): the screen is drawn on a PC (ESPHome's `host` platform, stubbed hardware), so every change is seen without the device.
- [ADR-0025](decisions/0025-events-only.md): the screen sends events; one Home Assistant package maps them to a list of allowed actions.
- [ADR-0029](decisions/0029-themes-palette.md): one C++ palette and role styles, so themes change at run time.
- [ADR-0046](decisions/0046-popups-a-pages.md): one shared brick for popups with pages swiped left and right.
- [ADR-0063](decisions/0063-several-screens.md): several screens in one home, each with its own name.

**The troubleshooting log.** [Known incidents](troubleshooting.md) labels each of its 20 incidents: 11 apply to any ESPHome + LVGL screen (Home Assistant side, ESPHome's API, the LVGL schema, the build), 9 are specific to the Tab5 or the ESP32-P4. Read the first kind before you debug your own screen.

**The way to code with an AI.** This project was written with AI agents, and the method is in the files:

- an [`AGENTS.md`](../AGENTS.md) at the root (read first, rules, boundaries), one per folder for its detail, and a `CLAUDE.md` beside each that only imports it, so every tool reads the same text;
- an `[AI-CONTEXT]` block at the top of each firmware file (role, constraints), `[AI-WARNING]` on a deliberate fix that looks like a bug, so that an audit does not "correct" it back, `[AI-DEBUG]` on a good observation point ([debugging](debugging.md));
- the decisions in [`docs/decisions/`](decisions/README.md) and the incidents in [troubleshooting](troubleshooting.md), so that a question already settled is not asked again;
- tests that read the real YAML and C++ (code rules, popup chrome, line counts of [`CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md)), so a rule written in `AGENTS.md` is also checked.

### How a second screen was added: the Waveshare 7B

The Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (7 inches, 1024×600) runs the same firmware as the Tab5 (5 inches, 1280×720) since 2026-10-14. How it was done, in [ADR-0061](decisions/0061-board-and-screen-size.md):

1. **One substitution picks the board**: `tab5_carte` (`tab5` by default, `waveshare-7b`) loads `Tab5/paquets/carte-<board>.yaml`, the hardware of that board, and `geometrie-<board>.yaml`, its screen size, panel size, LVGL rotation and font scale. Building for the 7B: `python -m esphome -s tab5_carte waveshare-7b compile tab5-ha-hmi.yaml`.
2. **The size is written once**: the layout tokens derive from it, and the C++ gets it through build flags (`-DTAB5_ECRAN_L`, `-DTAB5_ECRAN_H`). A test checks the C++ against the tokens for every board.
3. **Hardware the new board lacks keeps its ids, as stubs** (battery gauge, clock chip, power switches): the other packages do not change, and Home Assistant finds the same entities.
4. **The layout is the Tab5's, scaled**: every position `x × L / 1280`, `y × H / 720`; large fonts scaled by 0.82, small ones kept readable; a few texts moved where they no longer fitted.
5. **Then the board itself**: panel mounted upside down, ESPHome 2026.9.1 at least, PSRAM at 200 MHz.

The ADRs that followed are what a board can change: [ADR-0005](decisions/0005-boot-delay-gpio-expander-reset.md) (the boot delay comes from the board), [ADR-0064](decisions/0064-screen-really-off-night.md) (how each panel really turns off), [ADR-0065](decisions/0065-games-optional-module.md) (games as an optional module, off on a board without a motion sensor, `tab5_jeux`), [ADR-0069](decisions/0069-screen-direction-from-home-assistant.md) (screen direction set from Home Assistant). The board page: [Waveshare 7B](waveshare-7b.md); side by side: [Tab5 or 7B](tab5-vs-waveshare-7b.md).

For a third ESP32-P4 board, the same path: a `carte-<board>.yaml` and a `geometrie-<board>.yaml`, stubs for what is missing, then the off-device render at its size. Not tried.

### What only holds on the ESP32-P4 and the Tab5

- **The display link**: both boards drive a MIPI-DSI panel; ESPHome's [`mipi_dsi`](https://esphome.io/components/display/mipi_dsi/) page describes it as the interface « available in the ESP32-P4 ».
- **Wi-Fi through a co-processor**: the ESP32-P4 has no radio of its own (« ESP32-P4 does not include built-in Wi-Fi or Bluetooth », [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html)); Wi-Fi comes from an ESP32-C6 over SDIO, ESPHome's [`esp32_hosted`](https://esphome.io/components/esp32_hosted/). Two incidents of the troubleshooting log come from that link.
- **The hardware JPEG decoder** of the P4 ([ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/jpeg.html)) decodes the camera pictures.
- **The rotation**: the Tab5's panel is portrait, turned by LVGL (`rotation: 270`); on the ESP32-P4 that software rotation uses the PPA, the P4's 2D accelerator ([ESPHome `lvgl`](https://esphome.io/components/lvgl/)).
- **The Tab5's own hardware**: the display reset behind an I/O expander (1 s at boot), the battery charger and gauge, the motion sensor of the tilt games, the three display revisions ([hardware](hardware.md), [pinout](tab5-pinout.md), [battery](tab5-battery.md)).
- **This project's Home Assistant side**: the « Tab5 » integration for HACS, the blueprint and the packages are written for this firmware's contract.

### And an ESP32-S3?

Not tried in this project: nothing here was built or run on an ESP32-S3. What official sources say, read on 2026-11-19:

| Point | What the source says | Source |
|---|---|---|
| LVGL in ESPHome | needs « an ESP32 or RP2040 »; PSRAM « generally recommended », especially for large colour displays | [esphome.io/components/lvgl](https://esphome.io/components/lvgl/) |
| ESP32-S3 in ESPHome | listed among the supported ESP32 variants | [esphome.io/components/esp32](https://esphome.io/components/esp32/) |
| Parallel RGB panels | `mipi_rgb` « requires an ESP32 (usually an ESP32-S3…) », with PSRAM | [esphome.io/components/display/mipi_rgb](https://esphome.io/components/display/mipi_rgb/) |
| Wi-Fi | the ESP32-S3 has « integrated 2.4 GHz, 802.11 b/g/n Wi-Fi »: no co-processor, no `esp32_hosted` | [espressif.com, ESP32-S3](https://www.espressif.com/en/products/socs/esp32-s3) |
| MIPI-DSI on the ESP32-S3 | none: in ESP-IDF v5.5.5, `SOC_MIPI_DSI_SUPPORTED` is declared for the ESP32-P4 and absent for the ESP32-S3, which declares `SOC_LCD_I80_SUPPORTED` and `SOC_LCD_RGB_SUPPORTED` (read on 2026-11-19) | [`soc_caps.h`, ESP32-S3](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32s3/include/soc/soc_caps.h), [`soc_caps.h`, ESP32-P4](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32p4/include/soc/soc_caps.h) |
| Hardware JPEG decoder on the ESP32-S3 | none: in ESP-IDF v5.5.5, `SOC_JPEG_CODEC_SUPPORTED` is declared for the ESP32-P4 and absent for the ESP32-S3 (read on 2026-11-19) | [`soc_caps.h`, ESP32-S3](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32s3/include/soc/soc_caps.h), [`soc_caps.h`, ESP32-P4](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32p4/include/soc/soc_caps.h) |
| Voice (`micro_wake_word`, `voice_assistant`) on the ESP32-S3 | not verified (the component pages name no chip) | [micro_wake_word](https://esphome.io/components/micro_wake_word/), [voice_assistant](https://esphome.io/components/voice_assistant/) |

So on an ESP32-S3 the board package (display, touch, Wi-Fi) would be written from scratch; the rules, the design decisions, the Home Assistant side and the method above do not depend on the chip. Whether this firmware's screens, fonts and memory fit an ESP32-S3: not verified.

---

## Version Française

---

Vous voulez un tableau de bord Home Assistant sur un écran tactile ESP32 avec ESPHome et LVGL, mais pas cette tablette précise. Cette page dit ce que vous pouvez prendre dans ce projet, comment une deuxième carte y a été ajoutée, et ce qui ne vaut que pour le M5Stack Tab5 et l'ESP32-P4. Tout ce qui suit a été construit et a tourné sur deux cartes ESP32-P4 (le M5Stack Tab5 et la Waveshare ESP32-P4 7B) ; rien n'a été essayé sur une autre puce.

### Ce qui se réutilise tel quel

**Les règles de code.** Dix règles numérotées, chacune née d'un vrai bug ou d'un écran lent, la plupart vérifiées par un test (`tools/check_tab5_code_rules.py`, lancé par `pytest`). Sept valent pour tout firmware ESPHome + LVGL ; le texte complet est dans [`Tab5/AGENTS.md`](../Tab5/AGENTS.md) (en anglais) :

| Règle | Pourquoi elle compte sur tout écran ESPHome + LVGL |
|---|---|
| 1. Aucune couleur posée sur un widget, un style de rôle à la place | ESPHome fige un `text_color:` / `bg_color:` à la compilation : un thème ne peut changer que les couleurs venues d'un style partagé |
| 2. Un `sensor:` ne touche jamais `lv_obj_*`, il appelle une fonction C++ nommée | la logique de dessin reste à un seul endroit et se teste ([ADR-0006](decisions/0006-centralize-lvgl-logic-in-cpp.md)) |
| 3. Pas de `static` dans une lambda pour un état partagé | chaque lambda est une fonction à part : utiliser `globals:` |
| 4. Pas de `std::string` par valeur dans un chemin chaud | sinon les curseurs et les mises à jour fréquentes allouent à chaque appel |
| 5. Un widget répété trois fois ou plus passe par un constructeur ou un `!include` avec `vars` | une correction au lieu de N copies |
| 9. Chaque icône MDI affichée est dans les `glyphs:` de sa police, chaque glyphe listé est affiché | un glyphe manquant dessine une case vide, sans erreur de compilation |
| 10. Notre C++ est appelé depuis une action `lambda:`, jamais depuis un champ d'événement ou la valeur d'une action « apply » | depuis ESPHome 2026.10.0 ces valeurs sont écrites avant les `includes:` et ne voient pas votre C++ ([esphome/esphome#21303](https://github.com/esphome/esphome/issues/21303)) |

Les trois autres sont des choix propres à ce projet : le cadre partagé des popups (règle 7, [ADR-0009](decisions/0009-modal-shell-header.md)), aucune entité ni action Home Assistant dans le firmware, seulement des événements (règle 8, [ADR-0025](decisions/0025-events-only.md)), et `esphome compile` avant chaque commit (règle 6). Ils valent la peine d'être copiés, mais votre projet peut s'en passer.

**Les décisions d'architecture qui ne dépendent pas de la carte** ([toutes](decisions/README.md), en anglais) :

- [ADR-0001](decisions/0001-push-only-zero-polling.md) : Home Assistant pousse ce qui a changé, l'écran n'interroge jamais.
- [ADR-0003](decisions/0003-data-packing-delimited-strings.md) : un appel de service porte toute une liste en chaîne délimitée, au lieu d'un appel par valeur.
- [ADR-0004](decisions/0004-no-alpha-png-prebaked-backgrounds.md) : pas de PNG avec transparence, des fonds précalculés opaques.
- [ADR-0019](decisions/0019-logical-slots-blueprint.md) : l'écran connaît des emplacements, un blueprint Home Assistant les relie à vos entités ; changer une lumière ne demande aucun flash.
- [ADR-0020](decisions/0020-no-secret-firmware-signed-ota.md) : aucun secret dans le firmware, Home Assistant fournit la clé d'API, les mises à jour sont signées.
- [ADR-0021](decisions/0021-host-render-stubs.md) : l'écran est dessiné sur un PC (plateforme `host` d'ESPHome, matériel simulé), chaque changement se voit sans l'appareil.
- [ADR-0025](decisions/0025-events-only.md) : l'écran envoie des événements ; un package Home Assistant les relie à une liste d'actions permises.
- [ADR-0029](decisions/0029-themes-palette.md) : une palette C++ et des styles de rôle, donc des thèmes qui changent à l'exécution.
- [ADR-0046](decisions/0046-popups-a-pages.md) : une brique partagée pour les popups à pages qu'on fait glisser à gauche et à droite.
- [ADR-0063](decisions/0063-several-screens.md) : plusieurs écrans dans une maison, chacun avec son nom.

**Le journal de dépannage.** Les [incidents connus](troubleshooting.md#version-française) étiquettent chacun de leurs 20 incidents : 11 valent pour tout écran ESPHome + LVGL (côté Home Assistant, API d'ESPHome, schéma LVGL, compilation), 9 sont propres au Tab5 ou à l'ESP32-P4. Lisez les premiers avant de déboguer votre propre écran.

**La façon de coder avec une IA.** Ce projet a été écrit avec des agents IA, et la méthode est dans les fichiers :

- un [`AGENTS.md`](../AGENTS.md) à la racine (à lire d'abord, règles, limites), un par dossier pour son détail, et à côté de chacun un `CLAUDE.md` qui ne fait que l'importer, pour que chaque outil lise le même texte ;
- un bloc `[AI-CONTEXT]` en tête de chaque fichier du firmware (rôle, contraintes), `[AI-WARNING]` sur un correctif voulu qui ressemble à un bug, pour qu'un audit ne le « corrige » pas, `[AI-DEBUG]` sur un bon point d'observation ([diagnostiquer](debugging.md#version-française)) ;
- les décisions dans [`docs/decisions/`](decisions/README.md) et les incidents dans le [dépannage](troubleshooting.md#version-française), pour qu'une question déjà tranchée ne revienne pas ;
- des tests qui lisent le vrai YAML et le vrai C++ (règles de code, cadre des popups, comptes de lignes de [`CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md)), pour qu'une règle écrite dans `AGENTS.md` soit aussi vérifiée.

### Comment un deuxième écran a été ajouté : la Waveshare 7B

La Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (7 pouces, 1024×600) fait tourner le même firmware que le Tab5 (5 pouces, 1280×720) depuis le 14/10/2026. Comment, dans l'[ADR-0061](decisions/0061-board-and-screen-size.md) :

1. **Une substitution choisit la carte** : `tab5_carte` (`tab5` par défaut, `waveshare-7b`) charge `Tab5/paquets/carte-<carte>.yaml`, le matériel de cette carte, et `geometrie-<carte>.yaml`, sa taille d'écran, la taille de la dalle, la rotation LVGL et l'échelle des polices. Pour la 7B : `python -m esphome -s tab5_carte waveshare-7b compile tab5-ha-hmi.yaml`.
2. **La taille est écrite une fois** : les jetons de mise en page en découlent, et le C++ la reçoit par des options de compilation (`-DTAB5_ECRAN_L`, `-DTAB5_ECRAN_H`). Un test compare le C++ aux jetons pour chaque carte.
3. **Le matériel absent de la nouvelle carte garde ses identifiants, en bouchons** (jauge de batterie, puce d'horloge, interrupteurs d'alimentation) : les autres packages ne changent pas, et Home Assistant retrouve les mêmes entités.
4. **La mise en page est celle du Tab5, à l'échelle** : chaque position `x × L / 1280`, `y × H / 720` ; les grandes polices à 0,82, les petites gardées lisibles ; quelques textes déplacés là où ils ne tenaient plus.
5. **Puis la carte elle-même** : dalle montée à l'envers, ESPHome 2026.9.1 au moins, PSRAM à 200 MHz.

Les ADR suivantes disent ce qu'une carte peut changer : [ADR-0005](decisions/0005-boot-delay-gpio-expander-reset.md) (le délai au démarrage vient de la carte), [ADR-0064](decisions/0064-screen-really-off-night.md) (comment chaque dalle s'éteint vraiment), [ADR-0065](decisions/0065-games-optional-module.md) (les jeux en module optionnel, absents sur une carte sans capteur de mouvement, `tab5_jeux`), [ADR-0069](decisions/0069-screen-direction-from-home-assistant.md) (le sens de l'écran réglé depuis Home Assistant). La page de la carte : [Waveshare 7B](waveshare-7b.md#version-française) ; côte à côte : [Tab5 ou 7B](tab5-vs-waveshare-7b.md#version-française).

Pour une troisième carte ESP32-P4, le même chemin : un `carte-<carte>.yaml` et un `geometrie-<carte>.yaml`, des bouchons pour ce qui manque, puis le rendu hors appareil à sa taille. Pas essayé.

### Ce qui ne vaut que pour l'ESP32-P4 et le Tab5

- **La liaison de l'écran** : les deux cartes pilotent une dalle MIPI-DSI ; la page [`mipi_dsi`](https://esphome.io/components/display/mipi_dsi/) d'ESPHome la décrit comme l'interface « available in the ESP32-P4 ».
- **Le Wi-Fi par un co-processeur** : l'ESP32-P4 n'a pas de radio (« ESP32-P4 does not include built-in Wi-Fi or Bluetooth », [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html)) ; le Wi-Fi vient d'un ESP32-C6 en SDIO, le composant [`esp32_hosted`](https://esphome.io/components/esp32_hosted/) d'ESPHome. Deux incidents du dépannage viennent de ce lien.
- **Le décodeur JPEG matériel** du P4 ([ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/jpeg.html)) décode les images des caméras.
- **La rotation** : la dalle du Tab5 est en portrait, tournée par LVGL (`rotation: 270`) ; sur l'ESP32-P4 cette rotation logicielle passe par le PPA, l'accélérateur 2D du P4 ([`lvgl` d'ESPHome](https://esphome.io/components/lvgl/)).
- **Le matériel propre au Tab5** : le reset de l'écran derrière un expandeur d'E/S (1 s au démarrage), le chargeur et la jauge de batterie, le capteur de mouvement des jeux à inclinaison, les trois révisions d'écran ([matériel](hardware.md#version-française), [brochage](tab5-pinout.md#version-française), [batterie](tab5-battery.md#version-française)).
- **Le côté Home Assistant de ce projet** : l'intégration « Tab5 » pour HACS, le blueprint et les packages sont écrits pour le contrat de ce firmware.

### Et un ESP32-S3 ?

Pas essayé dans ce projet : rien ici n'a été compilé ni n'a tourné sur un ESP32-S3. Ce que disent les sources officielles, lues le 19/11/2026 :

| Point | Ce que dit la source | Source |
|---|---|---|
| LVGL dans ESPHome | demande « an ESP32 or RP2040 » ; PSRAM « generally recommended », surtout pour les grands écrans couleur | [esphome.io/components/lvgl](https://esphome.io/components/lvgl/) |
| ESP32-S3 dans ESPHome | dans la liste des variantes d'ESP32 prises en charge | [esphome.io/components/esp32](https://esphome.io/components/esp32/) |
| Dalles RGB parallèles | `mipi_rgb` « requires an ESP32 (usually an ESP32-S3…) », avec PSRAM | [esphome.io/components/display/mipi_rgb](https://esphome.io/components/display/mipi_rgb/) |
| Wi-Fi | l'ESP32-S3 a un « integrated 2.4 GHz, 802.11 b/g/n Wi-Fi » : ni co-processeur, ni `esp32_hosted` | [espressif.com, ESP32-S3](https://www.espressif.com/en/products/socs/esp32-s3) |
| MIPI-DSI sur l'ESP32-S3 | aucun : dans ESP-IDF v5.5.5, `SOC_MIPI_DSI_SUPPORTED` est déclaré pour l'ESP32-P4 et absent pour l'ESP32-S3, qui déclare `SOC_LCD_I80_SUPPORTED` et `SOC_LCD_RGB_SUPPORTED` (lu le 19/11/2026) | [`soc_caps.h`, ESP32-S3](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32s3/include/soc/soc_caps.h), [`soc_caps.h`, ESP32-P4](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32p4/include/soc/soc_caps.h) |
| Décodeur JPEG matériel sur l'ESP32-S3 | aucun : dans ESP-IDF v5.5.5, `SOC_JPEG_CODEC_SUPPORTED` est déclaré pour l'ESP32-P4 et absent pour l'ESP32-S3 (lu le 19/11/2026) | [`soc_caps.h`, ESP32-S3](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32s3/include/soc/soc_caps.h), [`soc_caps.h`, ESP32-P4](https://github.com/espressif/esp-idf/blob/v5.5.5/components/soc/esp32p4/include/soc/soc_caps.h) |
| Voix (`micro_wake_word`, `voice_assistant`) sur l'ESP32-S3 | non vérifié (les pages des composants ne nomment aucune puce) | [micro_wake_word](https://esphome.io/components/micro_wake_word/), [voice_assistant](https://esphome.io/components/voice_assistant/) |

Sur un ESP32-S3, le package de carte (écran, tactile, Wi-Fi) serait donc à écrire de zéro ; les règles, les décisions d'architecture, le côté Home Assistant et la méthode ci-dessus ne dépendent pas de la puce. Que les écrans, les polices et la mémoire de ce firmware tiennent sur un ESP32-S3 : non vérifié.
