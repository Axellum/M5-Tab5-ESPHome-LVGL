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

- The firmware encrypts the native API **and** OTA updates with the key you generate in your
  own `secrets.yaml` ([ADR-0015](docs/decisions/0015-ota-encrypted-with-api-key.md)). Anyone
  holding that key can control the tablet and flash it: if it leaks, generate a new one and
  reflash over USB or OTA.
- `secrets.yaml` and `Tab5/user_entities.yaml` are gitignored, and a pre-commit hook blocks
  secrets in tracked files. If you fork the project, keep it that way.

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

- Le firmware chiffre l'API native **et** les mises à jour OTA avec la clé que vous générez
  dans votre propre `secrets.yaml` ([ADR-0015](docs/decisions/0015-ota-encrypted-with-api-key.md)).
  Quiconque détient cette clé peut piloter la tablette et la flasher : si elle fuite, générez-en
  une nouvelle et reflashez en USB ou en OTA.
- `secrets.yaml` et `Tab5/user_entities.yaml` sont gitignorés, et un hook pre-commit bloque les
  secrets dans les fichiers suivis. Si vous forkez le projet, gardez cette règle.
