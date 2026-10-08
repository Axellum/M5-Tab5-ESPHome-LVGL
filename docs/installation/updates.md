# Updates

## English · [Français](#version-française)

---

**In this order: the Home Assistant files first, then the firmware.** Each release says what changes in its notes ([releases](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases)).

## The firmware, from Home Assistant

A tablet installed from the [install page](flash.md) gets its updates from Home Assistant: its « Firmware » entity reads the published manifest every 6 hours, and *Install* downloads the image, which the tablet checks against the project key ([ADR-0022](../decisions/0022-published-firmware-pages-channels.md)). Not willing to wait: run the action `homeassistant.update_entity` on that entity.

The tablet follows the channel it was installed from (Stable or Beta); to switch, install again from the [install page](flash.md#3-pick-the-channel), without erasing. A firmware you compile yourself has no « Firmware » entity: see [build your own firmware](build.md#updates-over-wi-fi).

## Home Assistant files

**With the HACS integration** ([installed this way](home-assistant-files.md#with-hacs)): each release shows up in *Settings → Updates* as « Tab5 — fichiers HA · HA files ». *Install*, then restart Home Assistant. At start, the integration:

- saves the current files in `config/tab5_sauvegardes/` (the 5 latest backups are kept);
- puts the new ones in place: a Tab5 file you edited by hand is replaced too, and the notification names it (its copy stays in the backup); a Tab5 file that was already there without coming from the integration (copied by hand) and differs is also named in *Settings → Repairs*, with its backup folder;
- checks the configuration, and puts the previous files back if the new ones break it (those files are then not tried again at each start: see Repairs);
- reloads the YAML, then says what changed in a notification « Tab5: Home Assistant files X.Y.Z »;
- if « Then update the tablet » is ticked, installs the firmware of the same version as soon as the tablet's « Firmware » entity offers it; if the tablet still runs the old version 15 minutes later, it tries again, 3 times in all, then says so in *Settings → Repairs* (the retries come with 3.8).

A problem shows in *Settings → Repairs*: missing `packages:` line, files refused, restart needed ([what to do](../troubleshooting.md#tab5-integration-hacs-a-message-in-repairs)). HACS offers full releases only: pre-releases (Beta channel) only if beta versions are switched on for this repository in HACS. To put the files back after a mistake: *Settings → Devices & services → Tab5 → Configure*, « Install the files of this version again now ».

**By hand, they do not update themselves**: replace them with those of `tab5_home_assistant.zip` from the same release ([step 1](home-assistant-files.md)), then *Developer tools → YAML → All YAML configuration*, and run the action `homeassistant.reload_custom_templates` (or restart Home Assistant). A blueprint imported from its URL rather than unzipped: import it again.

The files of an archive know their version: when the tablet runs a newer major or minor release (X.Y; a patch release alone does not count), Home Assistant says so (notification « Tab5 : fichiers Home Assistant à mettre à jour », sensor « Tab5 · fichiers HA en retard »). Files copied from the repository have no version and are never compared.

Using the [dashboard](dashboard.md)? After an update that adds entities, do its items 2 and 3 again.

### Putting older files back by hand

At each update, the integration keeps the files it replaces or removes in `config/tab5_sauvegardes/`, in a folder named after the date, the time and the version replaced: `20261012-081530_3.7.0` holds 3.7.0 files. Inside, the same folders as in `config/` (`packages/`, `custom_templates/`, `blueprints/`). Only the 5 latest folders are kept. A refused update (*Settings → Repairs*) leaves one too, holding the files still in place; trying the same files again does not add an identical folder.

1. Copy the content of the most recent folder into `config/`, replacing the files there (Samba share, File editor or Studio Code Server). To go back further, copy the folders one after the other, from the most recent to the one you want: each one holds only the files that changed at its update.
2. A file added by the newer version is in no backup, so it stays. To remove it, compare `config/packages/` with the `tab5_home_assistant.zip` of the version you go back to.
3. *Developer tools → YAML → Check configuration*, then reload as for files put by hand (above), or restart Home Assistant.

The integration then leaves these files alone: it installs its files once per version, when HACS brings a new one, or when « Install the files of this version again now » is ticked (that puts the newer files back). The next version installed through HACS replaces them, names them as edited by hand, and keeps them in a new backup. While the files are older than the integration, « Then update the tablet » waits: it starts the firmware only once « Tab5 · version des fichiers HA » shows the integration's version.

## Upgrading from 3.2

Replace the Home Assistant files with those of the new `tab5_home_assistant.zip` (or install the update of the [HACS integration](#home-assistant-files)), then check three lists ([step 5](sources.md)):

1. **Work calendar holding other events?** Type your word in « Tab5 · mot des événements de travail » (the author: `Travail`). Up to 3.2, only titles containing « Travail » counted; left empty, every event of the work calendar now counts as work.
2. **School holidays** no longer come from a table of the French Zone A: pick a calendar in « Tab5 · agenda des vacances scolaires ».
3. **Discussion mode**: pick its pipeline in « Tab5 · pipeline de discussion ». Up to 3.2, the tablet asked for a pipeline named exactly « Discussion LLM ».

The lists keep their entity ids, so your dashboards and automations need nothing.

## Upgrading from 3.1

After 3.1 the tablet no longer calls Home Assistant actions: it sends events, which the new package `packages/tab5_evenements.yaml` turns into actions ([ADR-0025](../decisions/0025-events-only.md)). In this order:

1. **Home Assistant files first**: replace them with those of `tab5_home_assistant.zip`, which bring `tab5_evenements.yaml`, and set the « Tab5 · … » lists as told in [coming from packages with placeholders](#coming-from-packages-with-placeholders-310-and-earlier). A 3.1 tablet sends none of these events: the package just waits, nothing changes.
2. **Then the firmware** (« Firmware » entity, or your own build).
3. **Then untick** « Allow the device to perform Home Assistant actions » (*ESPHome → Configure*): the tablet no longer needs it, and without it Home Assistant refuses any action the device would ask for.

A new firmware **without** the package does not crash and logs nothing, but what it asks Home Assistant is lost: the calendar popup shows only its local grid (no work hours, holidays or appointments; a tapped day stays on « Chargement... »), nothing is spoken (appointments, alarm briefing, « Volet arrêté »), the Domotique / Discussion buttons no longer change the pipeline, a dismissed alert is hidden on the tablet only, until its next restart, and « MAJ Écran », « Recharger autos » and « Redémarrer HA » do nothing.

## Coming from packages with placeholders (3.1.0 and earlier)

Replace the files, then set each list to your old value: `VOTRE_EMAIL_gmail_com` → « Tab5 · agenda de travail », `calendar.famille` → rendez-vous, `calendar.anniversaires` and the public-holiday calendar (usually picked by default), `VOTRE_TELEPHONE` → téléphone, `VOTRE_CAPTEUR_PRESENCE` → capteur de présence, `VOTRE_TV` → TV Samsung, and the IP of the `tab5_tv_app_url` line of `secrets.yaml` → « Tab5 · adresse de la TV » (the line can then go). The weather choice is kept. Keep `volet_serre_tracking.yaml` in `config/packages/` if you use it, and pick your shutter in its list.

**Set « Tab5 · agenda de travail » before the new automations run**: reload *Input texts* and *Template entities* first, choose, then reload *Scripts*, *Automations* and *REST commands*. Until it is set, every day counts as a rest day, and the alarm clock follows.

## Upgrading from 2.x

3.0 changes how the tablet is protected ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)). Once, in person (items 3 and 4 below must happen within 30 minutes of the tablet's start):

1. **Before flashing**, set up Home Assistant for 3.0: the blueprint of [step 6](devices.md) (the tablet no longer knows your entities, [ADR-0019](../decisions/0019-logical-slots-blueprint.md)).
2. **Signing key** ([build your own firmware](build.md#3-create-your-firmware-signing-key)), then compile: `esphome compile tab5-ha-hmi.yaml`.
3. **Flash.** The 2.x firmware refuses a plain upload, so this one goes out encrypted with your old key (`api_encryption_key` of your `secrets.yaml`, never printed):
   ```bash
   python tools/migrer_vers_3.py --host 192.168.x.x --port COM3
   ```
   `--port` is the tablet's USB port (`COM…` on Windows, `/dev/ttyACM0` on Linux; `python -m serial.tools.list_ports -v` lists them, the tablet's serial number is its MAC address). Or over USB only: `esphome upload tab5-ha-hmi.yaml --device COM3`.
4. **Wi-Fi.** 2.x had the credentials compiled in, 3.0 does not. With `--port`, the script gives them back right after the restart, over USB (Improv, `wifi_ssid` and `wifi_password` of the same `secrets.yaml`, never printed): no phone, no access point. Without it, the tablet starts without network and opens « Tab5 Fallback AP »: join it and pick your network, or use the web flasher's Wi-Fi button over USB ([step 3](wifi.md)).
5. **Home Assistant** notifies that the tablet « disabled transport encryption » (*Settings → Devices & services*, re-authentication): confirm. HA then gives it a new key by itself. The entities, automations and history stay the same.

Then `secrets.yaml` can go (keep the old key only if you may flash 2.x again), and `tab5_fuseau` is ignored.

---

## Version Française

---

**Dans cet ordre : les fichiers Home Assistant d'abord, puis le firmware.** Chaque release dit ce qui change dans ses notes ([releases](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases)).

## Le firmware, depuis Home Assistant

Une tablette installée depuis la [page d'installation](flash.md#version-française) reçoit ses mises à jour par Home Assistant : son entité « Firmware » lit le manifeste publié toutes les 6 h, et *Installer* télécharge l'image, que la tablette vérifie avec la clé du projet ([ADR-0022](../decisions/0022-published-firmware-pages-channels.md)). Pour ne pas attendre : lancez l'action `homeassistant.update_entity` sur cette entité.

La tablette suit le canal depuis lequel elle a été installée (Stable ou Bêta) ; pour en changer, réinstallez depuis la [page d'installation](flash.md#3-choisir-le-canal), sans effacer. Un firmware compilé soi-même n'a pas d'entité « Firmware » : voir [compiler son propre firmware](build.md#mises-à-jour-par-le-wi-fi).

## Fichiers Home Assistant

**Avec l'intégration de HACS** ([installée ainsi](home-assistant-files.md#avec-hacs)) : chaque release apparaît dans *Paramètres → Mises à jour*, « Tab5 — fichiers HA · HA files ». *Installer*, puis redémarrez Home Assistant. Au démarrage, l'intégration :

- garde les fichiers actuels dans `config/tab5_sauvegardes/` (les 5 dernières sauvegardes sont gardées) ;
- pose les nouveaux : un fichier du Tab5 modifié à la main est remplacé lui aussi, et la notification le nomme (sa copie reste dans la sauvegarde) ; un fichier du Tab5 déjà là sans venir de l'intégration (copié à la main) et différent est aussi nommé dans *Paramètres → Réparations*, avec son dossier de sauvegarde ;
- vérifie la configuration, et remet les anciens fichiers si les nouveaux la cassent (ces fichiers ne sont alors plus réessayés à chaque démarrage : voir Réparations) ;
- recharge le YAML, puis dit ce qui a changé dans une notification « Tab5 : fichiers Home Assistant X.Y.Z » ;
- si « Mettre ensuite la tablette à jour » est coché, installe le firmware de la même version dès que l'entité « Firmware » de la tablette le propose ; si la tablette tourne toujours l'ancienne version 15 minutes plus tard, réessaie, 3 fois en tout, puis le dit dans *Paramètres → Réparations* (les nouveaux essais arrivent avec la 3.8).

Un problème s'affiche dans *Paramètres → Réparations* : ligne `packages:` absente, fichiers refusés, redémarrage nécessaire ([que faire](../troubleshooting.md#intégration-tab5-hacs--un-message-dans-réparations)). HACS ne propose que les releases complètes : les pré-releases (canal Bêta) seulement si les versions bêta sont activées pour ce dépôt dans HACS. Pour remettre les fichiers après une erreur : *Paramètres → Appareils et services → Tab5 → Configurer*, « Réinstaller maintenant les fichiers de cette version ».

**À la main, ils ne se mettent pas à jour seuls** : remplacez-les par ceux de `tab5_home_assistant.zip` de la même release ([étape 1](home-assistant-files.md#version-française)), puis *Outils de développement → YAML → Toute la configuration YAML*, et lancez l'action `homeassistant.reload_custom_templates` (ou redémarrez Home Assistant). Un blueprint importé depuis son adresse plutôt que décompressé : importez-le de nouveau.

Les fichiers d'une archive connaissent leur version : quand la tablette tourne une release majeure ou mineure plus récente (X.Y ; une version corrective seule ne compte pas), Home Assistant le dit (notification « Tab5 : fichiers Home Assistant à mettre à jour », capteur « Tab5 · fichiers HA en retard »). Des fichiers copiés depuis le dépôt n'ont pas de version et ne sont jamais comparés.

Vous utilisez le [tableau de bord](dashboard.md#version-française) ? Après une mise à jour qui ajoute des entités, refaites ses points 2 et 3.

### Remettre à la main des fichiers plus anciens

À chaque mise à jour, l'intégration garde les fichiers qu'elle remplace ou retire dans `config/tab5_sauvegardes/`, dans un dossier nommé d'après la date, l'heure et la version remplacée : `20261012-081530_3.7.0` contient des fichiers de la 3.7.0. Dedans, les mêmes dossiers que dans `config/` (`packages/`, `custom_templates/`, `blueprints/`). Seuls les 5 derniers dossiers sont gardés. Une mise à jour refusée (*Paramètres → Réparations*) en laisse un aussi, avec les fichiers restés en place ; un nouvel essai des mêmes fichiers n'ajoute pas de dossier identique.

1. Copiez le contenu du dossier le plus récent dans `config/`, en remplaçant les fichiers qui y sont (partage Samba, File editor ou Studio Code Server). Pour remonter plus loin, copiez les dossiers l'un après l'autre, du plus récent à celui que vous voulez : chacun ne contient que les fichiers changés à sa mise à jour.
2. Un fichier ajouté par la version plus récente n'est dans aucune sauvegarde : il reste. Pour le retirer, comparez `config/packages/` avec le `tab5_home_assistant.zip` de la version où vous revenez.
3. *Outils de développement → YAML → Vérifier la configuration*, puis rechargez comme pour des fichiers posés à la main (plus haut), ou redémarrez Home Assistant.

L'intégration laisse ensuite ces fichiers tranquilles : elle installe ses fichiers une fois par version, quand HACS en apporte une nouvelle, ou quand « Réinstaller maintenant les fichiers de cette version » est coché (cela remet les fichiers plus récents). La version suivante installée par HACS les remplace, les nomme comme modifiés à la main, et les garde dans une nouvelle sauvegarde. Tant que les fichiers sont plus anciens que l'intégration, « Mettre ensuite la tablette à jour » attend : le firmware n'est lancé qu'une fois que « Tab5 · version des fichiers HA » donne la version de l'intégration.

## Passer d'une 3.2 à la suite

Remplacez les fichiers Home Assistant par ceux du nouveau `tab5_home_assistant.zip` (ou installez la mise à jour de l'[intégration de HACS](#fichiers-home-assistant)), puis vérifiez trois listes ([étape 5](sources.md#version-française)) :

1. **Votre agenda de travail contient d'autres événements ?** Tapez votre mot dans « Tab5 · mot des événements de travail » (l'auteur : `Travail`). Jusqu'à la 3.2, seuls les titres contenant « Travail » comptaient ; laissé vide, tout l'agenda de travail compte désormais comme du travail.
2. **Les vacances scolaires** ne viennent plus d'une table de la zone A : choisissez un agenda dans « Tab5 · agenda des vacances scolaires ».
3. **Mode Discussion** : choisissez son pipeline dans « Tab5 · pipeline de discussion ». Jusqu'à la 3.2, la tablette demandait un pipeline nommé exactement « Discussion LLM ».

Les listes gardent leurs identifiants d'entité : vos tableaux de bord et automatisations n'ont rien à changer.

## Passer d'une 3.1 à la suite

Après la 3.1, la tablette n'appelle plus d'action de Home Assistant : elle envoie des événements, que le nouveau package `packages/tab5_evenements.yaml` traduit en actions ([ADR-0025](../decisions/0025-events-only.md)). Dans cet ordre :

1. **Les fichiers Home Assistant d'abord** : remplacez-les par ceux de `tab5_home_assistant.zip`, qui apportent `tab5_evenements.yaml`, et réglez les listes « Tab5 · … » comme le dit [vous aviez les packages à placeholders](#vous-aviez-les-packages-à-placeholders-310-et-avant). Une tablette en 3.1 n'envoie aucun de ces événements : le package attend, rien ne change.
2. **Puis le firmware** (entité « Firmware », ou votre propre compilation).
3. **Puis décochez** « Autoriser l'appareil à effectuer des actions Home Assistant » (*ESPHome → Configurer*) : la tablette n'en a plus besoin, et sans elle Home Assistant refuse toute action que l'appareil demanderait.

Un firmware récent **sans** le package ne plante pas et n'écrit rien au journal, mais ce qu'il demande à Home Assistant se perd : le popup calendrier n'affiche que sa grille locale (ni horaires, ni fériés, ni rendez-vous ; un jour touché reste sur « Chargement... »), rien n'est dit (rendez-vous, briefing du réveil, « Volet arrêté »), les boutons Domotique / Discussion ne changent plus le pipeline, une alerte touchée n'est masquée que sur la tablette, jusqu'à son prochain redémarrage, et « MAJ Écran », « Recharger autos » et « Redémarrer HA » ne font rien.

## Vous aviez les packages à placeholders (3.1.0 et avant)

Remplacez les fichiers, puis réglez chaque liste sur votre ancienne valeur : `VOTRE_EMAIL_gmail_com` → « Tab5 · agenda de travail », `calendar.famille` → rendez-vous, `calendar.anniversaires` et l'agenda des jours fériés (en général choisis par défaut), `VOTRE_TELEPHONE` → téléphone, `VOTRE_CAPTEUR_PRESENCE` → capteur de présence, `VOTRE_TV` → TV Samsung, et l'IP de la ligne `tab5_tv_app_url` de `secrets.yaml` → « Tab5 · adresse de la TV » (la ligne peut ensuite partir). Le choix météo est gardé. Gardez `volet_serre_tracking.yaml` dans `config/packages/` si vous l'utilisez, et choisissez votre volet dans sa liste.

**Réglez « Tab5 · agenda de travail » avant que les nouvelles automatisations tournent** : rechargez d'abord *Entrées de texte* et *Entités de template*, choisissez, puis rechargez *Scripts*, *Automatisations* et *Commandes REST*. Tant qu'il n'est pas choisi, tous les jours comptent comme des jours de repos, et le réveil suit.

## Passer à la 3.0

La 3.0 change la façon dont la tablette est protégée ([ADR-0020](../decisions/0020-no-secret-firmware-signed-ota.md)). Une fois, sur place (les points 3 et 4 ci-dessous doivent tenir dans les 30 minutes qui suivent le démarrage de la tablette) :

1. **Avant de flasher**, préparez Home Assistant pour la 3.0 : le blueprint de l'[étape 6](devices.md#version-française) (la tablette ne connaît plus vos entités, [ADR-0019](../decisions/0019-logical-slots-blueprint.md)).
2. **Clé de signature** ([compiler son propre firmware](build.md#3-créer-votre-clé-de-signature-du-firmware)), puis compilez : `esphome compile tab5-ha-hmi.yaml`.
3. **Flashez.** Le firmware 2.x refuse un envoi en clair : celui-ci part donc chiffré avec votre ancienne clé (`api_encryption_key` de votre `secrets.yaml`, jamais affichée) :
   ```bash
   python tools/migrer_vers_3.py --host 192.168.x.x --port COM3
   ```
   `--port` est le port USB de la tablette (`COM…` sous Windows, `/dev/ttyACM0` sous Linux ; `python -m serial.tools.list_ports -v` les liste, le numéro de série de la tablette est son adresse MAC). Ou seulement par USB : `esphome upload tab5-ha-hmi.yaml --device COM3`.
4. **Wi-Fi.** La 2.x avait les identifiants compilés, la 3.0 non. Avec `--port`, le script les lui redonne juste après le redémarrage, par l'USB (Improv, `wifi_ssid` et `wifi_password` du même `secrets.yaml`, jamais affichés) : ni téléphone ni point d'accès. Sans lui, la tablette démarre sans réseau et ouvre « Tab5 Fallback AP » : connectez-vous-y et choisissez votre réseau, ou utilisez le bouton Wi-Fi du flasheur web par l'USB ([étape 3](wifi.md#version-française)).
5. **Home Assistant** signale que la tablette « a désactivé le chiffrement du transport » (*Paramètres → Appareils et services*, réauthentification) : confirmez. HA lui donne ensuite une nouvelle clé tout seul. Les entités, les automatisations et l'historique restent les mêmes.

Ensuite, `secrets.yaml` peut partir (gardez l'ancienne clé seulement si vous risquez de reflasher une 2.x), et `tab5_fuseau` est ignoré.
