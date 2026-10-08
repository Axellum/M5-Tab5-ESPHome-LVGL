# Step 2 — Install the firmware

## English · [Français](#version-française)

---

The firmware installs from the [install page](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/), over USB: nothing to compile, it is built and signed by the project ([ADR-0022](../decisions/0022-published-firmware-pages-channels.md)). To compile it yourself instead: [build your own firmware](build.md).

## 1. Open the install page

Open the [install page](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) in **Chrome or Edge, on a computer**: it talks to the tablet through Web Serial, which other browsers and phones lack (the page says so in place of its button). Plug the tablet into the computer with a **USB-C cable that carries data**.

## 2. Pick your display chip

The display chip is printed on the sticker on the back of the tablet, just above the Espressif logo. Each one has its own firmware ([hardware revisions](../hardware.md#hardware-revisions)):

| Display chip | Units made | With this firmware |
|---|---|---|
| ST7123 | October 2025 → April 2026 | the author's unit, used every day |
| ST7121 | since April 2026 | runs on another user's unit since October 2026 |
| ILI9881C | May → October 2025 | built, never tested |

A unit labelled « ST7123 » may carry an ST7121: if you are unsure, try one, then the other.

## 3. Pick the channel

**Stable** unless you want to try the next version before everyone (**Beta**, which moves on to the next stable one). The tablet then follows the channel you installed: its updates show up in Home Assistant ([updates](updates.md)). To switch channels later, install again from the page, without erasing. On Beta, also take `tab5_home_assistant.zip` from that pre-release (*Assets* on its [releases page](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases)), or let the HACS integration do it with beta versions switched on: the files of the stable release do not have what the pre-release adds on the Home Assistant side.

## 4. Connect and install

1. *Connect and install*. The browser lists the serial ports: the tablet is « USB JTAG/serial debug unit ». If several ports have that name, unplug the tablet: the one that disappears is hers.
2. **A new tablet** (it comes with M5Stack's demo): *Install*, then accept to erase the device.
   **A tablet already installed**: *Update*, without erasing: it keeps its Wi-Fi, its Home Assistant key and its settings.
3. Wait for the end of the install. The same window then offers to set the Wi-Fi: that is [step 3](wifi.md).

Closing the window restarts the tablet once: that is normal.

## Check a download (optional)

From the release after 3.8.0-rc.3, each release carries a `SHA256SUMS` file and a signed attestation of where its files were built (the GitHub account, the repository, the commit). To check a file you downloaded by hand, from the folder holding it:

```bash
sha256sum -c SHA256SUMS --ignore-missing
gh attestation verify tab5-ha-hmi-st7123.ota.bin --repo Axellum/M5-Tab5-ESPHome-LVGL
```

The install page and the tablet do not need this: the tablet only accepts a firmware signed with the project key ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)), and the page checks the SHA-256 of the manifest.

## If it does not work

| What you see | What to do |
|---|---|
| No port in the list | a cable that only charges shows no port: try another cable, then another USB port of the computer |
| « Failed to initialize… holding the BOOT button » | the Tab5 has no BOOT button. Hold its reset button for about 2 s, until the internal green LED blinks fast: the tablet is in download mode. Start *Connect and install* again; at the end, a short press on reset restarts it |
| No *Install* button | the same version is already on the tablet. To start from scratch, « Erase User Data » (in red, at the bottom) erases everything, Wi-Fi, key and settings included (alarm clock, brightness…: note them in Home Assistant first), then installs again |
| « This browser has no Web Serial » | Chrome or Edge, on a computer |
| The screen is not right after the install | the wrong display chip, maybe: a sticker saying « ST7123 » may hide an ST7121. Install the other one (*Update*, without erasing) |

Other cases already met on this tablet: [known incidents](../troubleshooting.md).

**Next: [step 3, Wi-Fi](wifi.md).**

---

## Version Française

---

Le firmware s'installe depuis la [page d'installation](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/), par l'USB : rien à compiler, il est compilé et signé par le projet ([ADR-0022](../decisions/0022-published-firmware-pages-channels.md)). Pour le compiler vous-même à la place : [compiler son propre firmware](build.md#version-française).

## 1. Ouvrir la page d'installation

Ouvrez la [page d'installation](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) dans **Chrome ou Edge, sur un ordinateur** : elle parle à la tablette par Web Serial, que les autres navigateurs et les téléphones n'ont pas (la page le dit à la place de son bouton). Branchez la tablette à l'ordinateur avec un **câble USB-C qui transmet les données**.

## 2. Choisir sa puce d'écran

La puce d'écran est écrite sur l'autocollant au dos de la tablette, juste au-dessus du logo Espressif. Chacune a son firmware ([révisions matérielles](../hardware.md#révisions-matérielles)) :

| Puce d'écran | Fabriquées | Avec ce firmware |
|---|---|---|
| ST7123 | octobre 2025 → avril 2026 | celle de l'auteur, utilisée tous les jours |
| ST7121 | depuis avril 2026 | tourne chez un autre utilisateur depuis octobre 2026 |
| ILI9881C | mai → octobre 2025 | compilée, jamais essayée |

Une tablette étiquetée « ST7123 » peut porter une ST7121 : dans le doute, essayez l'une, puis l'autre.

## 3. Choisir le canal

**Stable**, sauf si vous voulez essayer la prochaine version avant tout le monde (**Bêta**, qui passe à la stable suivante). La tablette suit ensuite le canal installé : ses mises à jour arrivent dans Home Assistant ([mises à jour](updates.md#version-française)). Pour changer de canal plus tard, réinstallez depuis la page, sans effacer. En Bêta, prenez aussi `tab5_home_assistant.zip` de cette pré-release (*Assets* sur la [page des releases](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases)), ou laissez l'intégration HACS le faire avec les versions bêta activées : les fichiers de la stable n'ont pas ce que la pré-release ajoute côté Home Assistant.

## 4. Connecter et installer

1. *Connecter et installer*. Le navigateur liste les ports série : la tablette s'appelle « USB JTAG/serial debug unit ». Si plusieurs ports portent ce nom, débranchez la tablette : celui qui disparaît est le sien.
2. **Une tablette neuve** (elle porte la démo de M5Stack) : *Install*, puis acceptez d'effacer l'appareil.
   **Une tablette déjà installée** : *Update*, sans effacer : elle garde son Wi-Fi, sa clé de Home Assistant et ses réglages.
3. Attendez la fin de l'installation. La même fenêtre propose ensuite de régler le Wi-Fi : c'est l'[étape 3](wifi.md#version-française).

Fermer la fenêtre redémarre la tablette une fois : c'est normal.

## Vérifier un téléchargement (facultatif)

À partir de la release qui suit la 3.8.0-rc.3, chaque release porte un fichier `SHA256SUMS` et une attestation signée de l'endroit où ses fichiers ont été construits (le compte GitHub, le dépôt, le commit). Pour vérifier un fichier téléchargé à la main, depuis le dossier qui le contient :

```bash
sha256sum -c SHA256SUMS --ignore-missing
gh attestation verify tab5-ha-hmi-st7123.ota.bin --repo Axellum/M5-Tab5-ESPHome-LVGL
```

La page d'installation et la tablette n'en ont pas besoin : la tablette n'accepte qu'un firmware signé par la clé du projet ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)), et la page contrôle le SHA-256 du manifeste.

## Si ça ne marche pas

| Ce que vous voyez | Que faire |
|---|---|
| Aucun port dans la liste | un câble qui ne fait que charger ne montre aucun port : essayez un autre câble, puis un autre port USB de l'ordinateur |
| « Failed to initialize… holding the BOOT button » | le Tab5 n'a pas de bouton BOOT. Maintenez son bouton reset environ 2 s, jusqu'à ce que la LED verte interne clignote vite : la tablette est en mode téléchargement. Relancez *Connecter et installer* ; à la fin, un appui court sur reset la redémarre |
| Pas de bouton *Install* | la même version est déjà sur la tablette. Pour repartir de zéro, « Erase User Data » (en rouge, en bas) efface tout, Wi-Fi, clé et réglages compris (réveil, luminosité… : notez-les d'abord dans Home Assistant), puis réinstalle |
| « Ce navigateur ne gère pas Web Serial » | Chrome ou Edge, sur un ordinateur |
| L'écran n'est pas normal après l'installation | peut-être la mauvaise puce d'écran : un autocollant « ST7123 » peut cacher une ST7121. Installez l'autre (*Update*, sans effacer) |

Autres cas déjà rencontrés sur cette tablette : [incidents connus](../troubleshooting.md#version-française).

**Ensuite : [étape 3, le Wi-Fi](wifi.md#version-française).**
