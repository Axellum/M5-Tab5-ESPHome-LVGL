# Step 3 — Wi-Fi

## English · [Français](#version-française)

---

The firmware holds no Wi-Fi password ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)): the tablet starts without a network, and you give it yours, either way.

## Over USB, right after the install

The window of the install page offers it as soon as the install ends: *Connect to Wi-Fi*, pick your network, type its password. It goes over the USB cable (Improv).

The window is already closed? Open the [install page](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) again, *Connect and install*, then its Wi-Fi button. [ESPHome Web](https://web.esphome.io) does the same (*Connect*, then *Configure Wi-Fi*).

## Without a cable, with a phone

1. On the phone, join the open Wi-Fi network **« Tab5 Fallback AP »**.
2. A page opens by itself; otherwise go to `http://192.168.4.1`. <!-- pragma: allowlist secret -->
3. Pick your network and type its password.

## Good to know

- The network is kept across updates.
- The « Tab5 Fallback AP » network comes back whenever the tablet loses its Wi-Fi for a minute, to give it another one.
- Note the tablet's IP address if you can (your router, or the page of « Tab5 Fallback AP » right after you picked the network): it helps at step 4 if Home Assistant does not find the tablet by itself, and for the [demo mode](../demo_mode.md).

**Next: [step 4, add the tablet to Home Assistant](add-to-home-assistant.md)**, within 30 minutes of its start.

---

## Version Française

---

Le firmware ne contient aucun mot de passe Wi-Fi ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)) : la tablette démarre sans réseau, et vous lui donnez le vôtre, au choix.

## Par l'USB, juste après l'installation

La fenêtre de la page d'installation le propose dès la fin de l'installation : *Connect to Wi-Fi*, choisissez votre réseau, tapez son mot de passe. Il passe par le câble USB (Improv).

La fenêtre est déjà fermée ? Rouvrez la [page d'installation](https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/), *Connecter et installer*, puis son bouton Wi-Fi. [ESPHome Web](https://web.esphome.io) fait la même chose (*Connect*, puis *Configure Wi-Fi*).

## Sans câble, avec un téléphone

1. Sur le téléphone, connectez-vous au réseau Wi-Fi ouvert **« Tab5 Fallback AP »**.
2. Une page s'ouvre toute seule ; sinon allez sur `http://192.168.4.1`. <!-- pragma: allowlist secret -->
3. Choisissez votre réseau et tapez son mot de passe.

## Bon à savoir

- Le réseau est gardé d'une mise à jour à l'autre.
- Le réseau « Tab5 Fallback AP » revient dès que la tablette perd son Wi-Fi une minute, pour lui en donner un autre.
- Notez l'adresse IP de la tablette si vous le pouvez (votre box, ou la page de « Tab5 Fallback AP » juste après le choix du réseau) : elle sert à l'étape 4 si Home Assistant ne trouve pas la tablette seul, et pour le [mode démo](../demo_mode.md#version-française).

**Ensuite : [étape 4, ajouter la tablette à Home Assistant](add-to-home-assistant.md#version-française)**, dans les 30 minutes qui suivent son démarrage.
