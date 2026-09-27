# Security Policy

## English · [Français](#version-française)

### Supported versions

Only the **latest release** and the `main` branch receive fixes. This is a personal project
maintained in spare time: fixes are best effort, but security reports are read first.

### Reporting a vulnerability

**Please do not open a public issue or discussion.** Report privately instead:

1. **Preferred:** the **Report a vulnerability** button in this repository's
   [Security tab](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/security) (GitHub private
   vulnerability reporting).
2. **Otherwise:** the e-mail address shown on the maintainer's GitHub profile (@Axellum).

Please include what is affected (file, feature, release or commit), how to reproduce it, and
what an attacker could do with it. Strip your own secrets from logs before sending them.

### Scope

In scope: the firmware (`tab5-ha-hmi.yaml`, `Tab5/`), the Home Assistant examples
(`HomeAssistant_Config/`) and the tools (`tools/`).

Out of scope — please report these upstream:
[ESPHome](https://github.com/esphome/esphome/security),
[Home Assistant](https://www.home-assistant.io/security/),
[ESP-IDF](https://github.com/espressif/esp-idf/security),
[LVGL](https://github.com/lvgl/lvgl/security).

### Good to know

- Since 3.0 the firmware holds no secret ([ADR-0020](docs/decisions/0020-no-secret-firmware-signed-ota.md)).
  The native API is encrypted with a key Home Assistant creates when you add the tablet, and
  keeps. A tablet that has no key yet accepts that first contact only in the 30 minutes after
  it starts.
- OTA updates are not encrypted, but **signed**: the tablet refuses a firmware that was not
  signed with the key of the firmware it runs. Your private key (`tab5_signature.pem`) is what
  lets someone flash it over the network: keep it off git, with a copy outside your computer.
  If it leaks, create a new one and reflash over USB.
- The fallback access point (« Tab5 Fallback AP », used to set the Wi-Fi) is open. Someone
  within range can change the network of a tablet that lost its own; they can neither flash it
  nor talk to its API.
- `Tab5/user_entities.yaml`, `*.pem` and `*.key` are gitignored, and a pre-commit hook blocks
  secrets and private keys in tracked files. If you fork the project, keep it that way.

---

## Version Française

### Versions prises en charge

Seules la **dernière release** et la branche `main` reçoivent des correctifs. C'est un projet
personnel entretenu sur du temps libre : les correctifs se font au mieux, mais les signalements
de sécurité passent en premier.

### Signaler une faille

**Merci de ne pas ouvrir d'issue ni de discussion publique.** Signalez-la en privé :

1. **De préférence :** le bouton **Report a vulnerability** de l'onglet
   [Security](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/security) de ce dépôt
   (signalement privé de GitHub).
2. **Sinon :** l'adresse e-mail indiquée sur le profil GitHub du mainteneur (@Axellum).

Indiquez ce qui est touché (fichier, fonction, release ou commit), comment le reproduire, et ce
qu'un attaquant pourrait en faire. Retirez vos propres secrets des logs avant de les envoyer.

### Périmètre

Dans le périmètre : le firmware (`tab5-ha-hmi.yaml`, `Tab5/`), les exemples Home Assistant
(`HomeAssistant_Config/`) et les outils (`tools/`).

Hors périmètre — à signaler aux projets concernés :
[ESPHome](https://github.com/esphome/esphome/security),
[Home Assistant](https://www.home-assistant.io/security/),
[ESP-IDF](https://github.com/espressif/esp-idf/security),
[LVGL](https://github.com/lvgl/lvgl/security).

### Bon à savoir

- Depuis la 3.0, le firmware ne contient aucun secret ([ADR-0020](docs/decisions/0020-no-secret-firmware-signed-ota.md)).
  L'API native est chiffrée avec une clé que Home Assistant crée quand vous ajoutez la tablette,
  et qu'il garde. Une tablette encore sans clé n'accepte ce premier contact que dans les
  30 minutes qui suivent son démarrage.
- Les mises à jour OTA ne sont pas chiffrées, mais **signées** : la tablette refuse un firmware
  qui n'est pas signé par la clé de celui qu'elle fait tourner. Votre clé privée
  (`tab5_signature.pem`) est ce qui permet de la flasher par le réseau : jamais dans git, avec
  une copie hors de votre ordinateur. Si elle fuite, créez-en une autre et reflashez en USB.
- Le point d'accès de secours (« Tab5 Fallback AP », qui sert à donner le Wi-Fi) est ouvert.
  Quelqu'un à portée peut changer le réseau d'une tablette qui a perdu le sien ; il ne peut ni
  la flasher ni parler à son API.
- `Tab5/user_entities.yaml`, `*.pem` et `*.key` sont gitignorés, et un hook pre-commit bloque
  les secrets et les clés privées dans les fichiers suivis. Si vous forkez le projet, gardez
  cette règle.
