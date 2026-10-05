# Build your own firmware

## English · [Français](#version-française)

---

Instead of [step 2](flash.md) (the install page), you can compile the firmware yourself, with your own changes. The other steps of the [guide](README.md) stay the same: the [Home Assistant files](home-assistant-files.md), then [steps 4 to 7](add-to-home-assistant.md).

> [!NOTE]
> Over the air, the tablet only accepts a firmware signed with the same key as the one it runs. A tablet installed from the install page runs the project key: to switch to your own builds (your own key), flash it once over USB.

## Prerequisites

- **ESPHome 2026.9.0 or newer**: the ESPHome add-on, or the command line (`pip install esphome`). It is enforced by `min_version:` in `tab5-ha-hmi.yaml`, so an older ESPHome refuses to compile. 2026.7.0 brought the official `st7123` touchscreen platform (no more `external_components`), zero-copy audio, VAD and PSRAM-over-SDIO; the floor was raised to 2026.8.1 on 2026-08-26 for the API, voice-assistant and crash-handler fixes this project exercises daily, then to 2026.9.0 on 2026-09-16; the pairing window, the key given by Home Assistant and signed firmware (3.0) are checked on it (reasoning in the comment above `min_version:`).
- **Home Assistant 2026.8 or newer** (it gives the tablet its encryption key, see [step 4](add-to-home-assistant.md)).
- A M5Stack Tab5 and the name of its display chip ([hardware revisions](../hardware.md#hardware-revisions)).

## 1. Clone and locate the entry point

```bash
git clone https://github.com/Axellum/M5-Tab5-ESPHome-LVGL.git
cd M5-Tab5-ESPHome-LVGL
```

The main file is `tab5-ha-hmi.yaml` at the repository root. All other YAML files in `Tab5/` are included as packages by this entry point.

## 2. Create your build settings file

Copy the example file:

```bash
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
```

`Tab5/user_entities.yaml` is gitignored (never committed). For a standard install there is nothing to replace in it: every line is optional.

**Screen language:** French by default; add `tab5_langue: English` (or `Deutsch`, `Nederlands`, `Español`, `Italiano`, `Türkçe`) for another language on the first boot. It can then be changed from Home Assistant (select « Langue »), see [translations](../translations.md).

**Time zone:** nothing to set since 3.0. The tablet takes Home Assistant's time zone and keeps the last one it received, so the alarm clock stays right when HA is down after a power cut. An old `tab5_fuseau` line is ignored.

**Tab5 revision:** if the display chip on your sticker is not the ST7123, add `tab5_ecran: st7121` or `tab5_ecran: ili9881c` to this file (see [hardware revisions](../hardware.md#hardware-revisions)). Leave it out for the ST7123.

**Your devices (lights, climate, plants, TV…) are not set here any more (since 3.0)**: you pick them in Home Assistant with the mouse, see [step 6](devices.md) and [adapt to your home](adapt-to-your-home.md); old `entity_light_…` keys in an existing file are simply ignored. The entry point `tab5-ha-hmi.yaml` includes this file via `substitutions: !include Tab5/user_entities.yaml`. No entity key is left: Home Assistant finds the tablet's own entities (voice satellite, media player, pipeline select) and those of the « MAJ Écran » button by itself ([ADR-0025](../decisions/0025-events-only.md)); old `entity_tab5_…`, `entity_primary_active` and `entity_push_automation` lines are ignored.

## 3. Create your firmware signing key

Since 3.0 the firmware holds **no secret**: no Wi-Fi password, no API key ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)). What protects the tablet is a signature: over the network, it only accepts a firmware signed with the same key as the one it runs. Create this private key once, at the repository root:

```bash
python -m espsecure generate-signing-key --version 2 --scheme rsa3072 tab5_signature.pem
```

`espsecure` comes with ESPHome. The file is gitignored; to keep it elsewhere, set its path in `Tab5/user_entities.yaml` (`tab5_cle_signature: …`). **Keep a copy outside your computer**: without it, the tablet can only be updated over USB.

No `secrets.yaml` any more: one left from 2.x is simply not read (see [upgrading from 2.x](updates.md#upgrading-from-2x)).

## 4. First flash (USB) and Wi-Fi

Connect the Tab5 to your computer via USB-C. Then:

```bash
esphome run tab5-ha-hmi.yaml
```

The tablet has no Wi-Fi network yet. Give it yours, either way:
- **over USB**, right after the flash: the project's [install page](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) (Chrome or Edge), *Connect and install*, then its Wi-Fi button, or [ESPHome Web](https://web.esphome.io), *Connect*, then *Configure Wi-Fi*. Both use Improv and work with a firmware you built yourself. Closing that window restarts the tablet once: that is normal;
- **without a cable**: join the open **« Tab5 Fallback AP »** network with a phone; a page opens (otherwise go to `http://192.168.4.1`) to pick your network. <!-- pragma: allowlist secret -->

The network is kept across updates. Then [add the tablet to Home Assistant](add-to-home-assistant.md), within 30 minutes of its start.

## Updates over Wi-Fi

Once the tablet is on your network, later builds go over Wi-Fi:

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

The transfer is not encrypted any more (there is no key in the YAML); the tablet checks the signature and refuses a firmware signed by another key. Your builds are signed by your key at compile time, nothing else to do.

**Logs:** `esphome logs` looks for the key in the YAML and no longer finds one. Use `python tools/tab5_logs.py --host 192.168.x.x --config-ha \\<ha-ip>\config`: it reads the key Home Assistant keeps (`.storage/core.config_entries`, or the `TAB5_CLE_API` variable) and never prints it. More in [debugging](../debugging.md).

---

## Version Française

---

À la place de l'[étape 2](flash.md#version-française) (la page d'installation), vous pouvez compiler le firmware vous-même, avec vos propres changements. Les autres étapes du [guide](README.md#version-française) restent les mêmes : les [fichiers Home Assistant](home-assistant-files.md#version-française), puis les [étapes 4 à 7](add-to-home-assistant.md#version-française).

> [!NOTE]
> Par le réseau, la tablette n'accepte qu'un firmware signé par la même clé que celui qu'elle fait tourner. Une tablette installée depuis la page d'installation a la clé du projet : pour passer à vos propres compilations (votre clé), flashez-la une fois par USB.

## Prérequis

- **ESPHome 2026.9.0 ou plus récent** : le module ESPHome, ou la ligne de commande (`pip install esphome`). C'est imposé par le `min_version:` de `tab5-ha-hmi.yaml` : une version antérieure refuse de compiler. La 2026.7.0 a apporté la plateforme tactile `st7123` officielle (plus besoin d'`external_components`), l'audio zero-copy, le VAD et la PSRAM via SDIO ; le plancher est passé à 2026.8.1 le 26/08/2026 pour les correctifs API, assistant vocal et handler de crash que ce projet exerce tous les jours, puis à 2026.9.0 le 16/09/2026 ; la fenêtre d'appairage, la clé donnée par Home Assistant et les firmwares signés (3.0) y sont vérifiés (raisons dans le commentaire au-dessus de `min_version:`).
- **Home Assistant 2026.8 ou plus récent** (c'est lui qui donne sa clé de chiffrement à la tablette, voir l'[étape 4](add-to-home-assistant.md#version-française)).
- Un M5Stack Tab5 et le nom de sa puce d'écran ([révisions matérielles](../hardware.md#révisions-matérielles)).

## 1. Cloner et localiser le point d'entrée

```bash
git clone https://github.com/Axellum/M5-Tab5-ESPHome-LVGL.git
cd M5-Tab5-ESPHome-LVGL
```

Le fichier principal est `tab5-ha-hmi.yaml` à la racine du dépôt. Tous les autres fichiers YAML dans `Tab5/` sont inclus comme packages par ce point d'entrée.

## 2. Créer votre fichier de réglages de compilation

Copiez le modèle :

```bash
cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
```

`Tab5/user_entities.yaml` est gitignoré (ne jamais le committer). Pour une installation standard, rien n'y est à remplacer : toutes les lignes sont facultatives.

**Langue de l'écran :** le français par défaut ; ajoutez `tab5_langue: English` (ou `Deutsch`, `Nederlands`, `Español`, `Italiano`, `Türkçe`) pour une autre langue au premier démarrage. Elle se change ensuite depuis Home Assistant (select « Langue »), voir [traductions](../translations.md#version-française).

**Fuseau horaire :** rien à régler depuis la 3.0. La tablette prend celui de Home Assistant et garde le dernier reçu : le réveil reste juste quand HA manque après une coupure de courant. Une ancienne ligne `tab5_fuseau` est ignorée.

**Révision du Tab5 :** si la puce écran de votre autocollant n'est pas la ST7123, ajoutez `tab5_ecran: st7121` ou `tab5_ecran: ili9881c` dans ce fichier (voir [révisions matérielles](../hardware.md#révisions-matérielles)). Pour la ST7123, ne mettez rien.

**Vos appareils (lumières, clim, plantes, TV…) ne se règlent plus ici (depuis la 3.0)** : vous les choisissez dans Home Assistant, à la souris, voir l'[étape 6](devices.md#version-française) et [adapter à sa maison](adapt-to-your-home.md#version-française) ; les anciennes clés `entity_light_…` d'un fichier existant sont simplement ignorées. Le point d'entrée `tab5-ha-hmi.yaml` les charge via `substitutions: !include Tab5/user_entities.yaml`. Il ne reste aucune clé d'entité : Home Assistant retrouve seul les entités de la tablette (satellite vocal, lecteur média, select de pipeline) et celles du bouton « MAJ Écran » ([ADR-0025](../decisions/0025-events-only.md)) ; d'anciennes lignes `entity_tab5_…`, `entity_primary_active` et `entity_push_automation` sont ignorées.

## 3. Créer votre clé de signature du firmware

Depuis la 3.0, le firmware ne contient **aucun secret** : ni mot de passe Wi-Fi, ni clé API ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)). Ce qui protège la tablette, c'est une signature : par le réseau, elle n'accepte qu'un firmware signé par la même clé que celui qu'elle fait tourner. Créez cette clé privée une fois, à la racine du dépôt :

```bash
python -m espsecure generate-signing-key --version 2 --scheme rsa3072 tab5_signature.pem
```

`espsecure` est installé avec ESPHome. Le fichier est gitignoré ; pour le ranger ailleurs, donnez son chemin dans `Tab5/user_entities.yaml` (`tab5_cle_signature: …`). **Gardez-en une copie hors de votre ordinateur** : sans elle, la tablette ne se met plus à jour que par USB.

Plus de `secrets.yaml` : celui d'une 2.x n'est simplement plus lu (voir [passer à la 3.0](updates.md#passer-à-la-30)).

## 4. Premier flash (USB) et Wi-Fi

Connectez le Tab5 à votre ordinateur via USB-C. Ensuite :

```bash
esphome run tab5-ha-hmi.yaml
```

La tablette n'a pas encore de réseau Wi-Fi. Donnez-lui le vôtre, au choix :
- **par l'USB**, juste après le flash : la [page d'installation](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) du projet (Chrome ou Edge), *Connecter et installer*, puis son bouton Wi-Fi, ou [ESPHome Web](https://web.esphome.io), *Connect*, puis *Configure Wi-Fi*. Les deux passent par Improv et marchent avec un firmware compilé soi-même. Fermer cette fenêtre redémarre la tablette une fois : c'est normal ;
- **sans câble** : connectez un téléphone au réseau ouvert **« Tab5 Fallback AP »** ; une page s'ouvre (sinon allez sur `http://192.168.4.1`) pour choisir votre réseau. <!-- pragma: allowlist secret -->

Le réseau est gardé d'une mise à jour à l'autre. Ensuite, [ajoutez la tablette à Home Assistant](add-to-home-assistant.md#version-française), dans les 30 minutes qui suivent son démarrage.

## Mises à jour par le Wi-Fi

Une fois la tablette sur votre réseau, les compilations suivantes passent par le Wi-Fi :

```bash
esphome run tab5-ha-hmi.yaml --device 192.168.x.x
```

L'envoi n'est plus chiffré (il n'y a pas de clé dans le YAML) ; la tablette vérifie la signature et refuse un firmware signé par une autre clé. Vos compilations sont signées par votre clé, rien d'autre à faire.

**Journaux :** `esphome logs` cherche la clé dans le YAML et n'en trouve plus. Utilisez `python tools/tab5_logs.py --host 192.168.x.x --config-ha \\<ip-de-ha>\config` : il lit la clé que garde Home Assistant (`.storage/core.config_entries`, ou la variable `TAB5_CLE_API`) et ne l'affiche jamais. Plus de détails dans [diagnostiquer](../debugging.md#version-française).
