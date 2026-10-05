# Install the tablet

## English · [Français](#version-française)

---

Seven steps, in this order: nothing to compile, the firmware installs from the browser. Steps 1 to 6 are needed, step 7 is optional.

> **Just want to see it first?** The [demo mode](../demo_mode.md) shows the full dashboard on a flashed tablet, with no Home Assistant at all.

## What you need

- An **M5Stack Tab5**, and the name of its **display chip**: it is printed on the sticker on the back, above the Espressif logo (ST7123, ST7121 or ILI9881C, see [hardware revisions](../hardware.md#hardware-revisions)).
- A **computer with Chrome or Edge** (the install page talks to the tablet through Web Serial) and a **USB-C cable that carries data**: with a charge-only cable, no port shows up.
- **Home Assistant 2026.8 or newer**, any installation method, and a way to copy files into its `config/` folder: Samba share, or the File editor or Studio Code Server add-on.
- Optional: a weather integration (Met.no comes with Home Assistant), your calendars, a voice pipeline (Assist).

## The seven steps

| Step | Where | What you do |
|---|---|---|
| [1. Home Assistant files](home-assistant-files.md) | Home Assistant | unzip one archive, add one line to `configuration.yaml`, restart |
| [2. Install the firmware](flash.md) | the install page, over USB | pick your display chip, *Connect and install* |
| [3. Wi-Fi](wifi.md) | the same window, or a phone | give the tablet your network |
| [4. Add the tablet to Home Assistant](add-to-home-assistant.md) | Home Assistant | *Configure* the discovered device, within 30 minutes |
| [5. Your sources](sources.md) | Home Assistant | weather, calendars, phone… in the « Tab5 · » lists |
| [6. Your devices](devices.md) | Home Assistant | one automation from the blueprint: rooms and tiles |
| [7. A dashboard](dashboard.md) (optional) | Home Assistant | every setting of the tablet in one dashboard |

**Why Home Assistant first:** the tablet gets everything it shows from these files, and asks them for the rest through events. Without them it does not crash, but nothing of your home reaches the screen.

The screenshots of Home Assistant's own pages (device page, entity table, automation and template editors) come from the project's fresh-install test: a new Home Assistant with test devices, where these steps are run for real whenever the Home Assistant files or the tablet's API change.

## After the installation

- [Tablet settings](settings.md): theme, language, alarm clock, sound…
- [Adapt to your home](adapt-to-your-home.md): rooms, solar energy, what disappears when you don't have it.
- [Weather providers](weather.md): outside France, or another rain or warning source.
- [Updates](updates.md): firmware from Home Assistant, Home Assistant files, coming from an older version.
- [Build your own firmware](build.md), instead of step 2.

Something went wrong? [Known incidents](../troubleshooting.md).

---

## Version Française

---

Sept étapes, dans cet ordre : rien à compiler, le firmware s'installe depuis le navigateur. Les étapes 1 à 6 sont nécessaires, la 7 est facultative.

> **Envie de voir d'abord ?** Le [mode démo](../demo_mode.md#version-française) montre le tableau de bord complet sur une tablette flashée, sans aucun Home Assistant.

## Ce qu'il faut

- Un **M5Stack Tab5**, et le nom de sa **puce d'écran** : il est écrit sur l'autocollant au dos, au-dessus du logo Espressif (ST7123, ST7121 ou ILI9881C, voir les [révisions matérielles](../hardware.md#révisions-matérielles)).
- Un **ordinateur avec Chrome ou Edge** (la page d'installation parle à la tablette par Web Serial) et un **câble USB-C qui transmet les données** : avec un câble de charge seule, aucun port n'apparaît.
- **Home Assistant 2026.8 ou plus récent**, toute méthode d'installation, et un moyen de copier des fichiers dans son dossier `config/` : partage Samba, ou le module File editor ou Studio Code Server.
- Facultatif : une intégration météo (Met.no vient avec Home Assistant), vos agendas, un pipeline vocal (Assist).

## Les sept étapes

| Étape | Où | Ce que vous faites |
|---|---|---|
| [1. Fichiers Home Assistant](home-assistant-files.md#version-française) | Home Assistant | décompresser une archive, ajouter une ligne à `configuration.yaml`, redémarrer |
| [2. Installer le firmware](flash.md#version-française) | la page d'installation, par l'USB | choisir sa puce d'écran, *Connecter et installer* |
| [3. Wi-Fi](wifi.md#version-française) | la même fenêtre, ou un téléphone | donner son réseau à la tablette |
| [4. Ajouter la tablette à Home Assistant](add-to-home-assistant.md#version-française) | Home Assistant | *Configurer* l'appareil découvert, dans les 30 minutes |
| [5. Vos sources](sources.md#version-française) | Home Assistant | météo, agendas, téléphone… dans les listes « Tab5 · » |
| [6. Vos appareils](devices.md#version-française) | Home Assistant | une automatisation depuis le blueprint : pièces et tuiles |
| [7. Un tableau de bord](dashboard.md#version-française) (facultatif) | Home Assistant | tous les réglages de la tablette dans un tableau de bord |

**Pourquoi Home Assistant d'abord :** la tablette reçoit de ces fichiers tout ce qu'elle affiche, et leur demande le reste par des événements. Sans eux elle ne plante pas, mais rien de votre maison n'arrive à l'écran.

Les captures des pages de Home Assistant lui-même (page de l'appareil, table des entités, éditeurs d'automatisation et de modèle) viennent du test d'installation à neuf du projet : un Home Assistant neuf avec des appareils de test, où ces étapes sont rejouées pour de vrai à chaque changement des fichiers Home Assistant ou de l'API de la tablette.

## Après l'installation

- [Réglages de la tablette](settings.md#version-française) : thème, langue, réveil, son…
- [Adapter à sa maison](adapt-to-your-home.md#version-française) : pièces, énergie solaire, ce qui disparaît quand on ne l'a pas.
- [Fournisseurs météo](weather.md#version-française) : hors de France, ou une autre source de pluie ou de vigilances.
- [Mises à jour](updates.md#version-française) : le firmware depuis Home Assistant, les fichiers Home Assistant, depuis une ancienne version.
- [Compiler son propre firmware](build.md#version-française), à la place de l'étape 2.

Un souci ? [Incidents connus](../troubleshooting.md#version-française).
