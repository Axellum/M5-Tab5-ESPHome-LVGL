# Step 1 — Home Assistant files

## English · [Français](#version-française)

---

The whole Home Assistant side is one archive, **`tab5_home_assistant.zip`**, attached to each [release](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases). **Nothing to fill in**: every value of your home is picked afterwards in Home Assistant, with the mouse ([ADR-0024](../decisions/0024-packages-without-placeholders.md)).

Two ways to put them in place: [with HACS](#with-hacs), which then updates them in one click at each release, or by hand, parts A to C below.

## With HACS

From release 3.7.0, the files also come with a small integration, « Tab5 », that [HACS](https://hacs.xyz) installs ([ADR-0035](../decisions/0035-hacs-integration-ha-files.md)). HACS must already be installed. HACS offers full releases only: pre-releases (Beta channel) only if beta versions are switched on for this repository in HACS.

1. Add the `packages:` line of [part B](#b-one-line-in-configurationyaml) to `configuration.yaml`, if it is not there yet.
2. **[Open Tab5 in HACS](https://my.home-assistant.io/redirect/hacs_repository/?owner=Axellum&repository=M5-Tab5-ESPHome-LVGL&category=integration)**: the link goes through My Home Assistant, which asks for your Home Assistant address the first time, then HACS offers to add the repository: *Add* (a box with a title only: reload the page, [troubleshooting](../troubleshooting.md#adding-tab5-to-hacs--repository-not-found--or-an-empty-box-2026-10-07)). Without the link: *⋮ (top right) → Custom repositories*, repository `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL`, type **Integration**, *Add*, then search for **Tab5** in HACS and open it.
3. On the Tab5 page of HACS, *Download*.
4. **Restart** Home Assistant.
5. *Settings → Devices & services → Add integration → Tab5*. Leave « Then update the tablet » ticked to have the firmware of the same version installed after the files, at each release ([updates](updates.md#home-assistant-files)).

The integration then puts the files in place by itself, without another restart. **It worked if** a notification « Tab5: Home Assistant files 3.x.y » says how many files were added, and the « Tab5 · » entities are there (see [part C](#c-check-then-restart)). Your own packages, templates and blueprints are never touched, and removing the integration leaves the files in place. Nothing else to do on this page: go to [step 2, install the firmware](flash.md).

**Setup assistant** (from 3.8, [ADR-0052](../decisions/0052-setup-assistant-rooms.md)): once the files are in place, *Settings → System → Repairs* shows « Tab5: set up the tablet from your rooms ». It prepares the automation of [step 6](devices.md) from your Home Assistant areas: up to five rooms, the best equipped first, each with its lights, covers, switches, fans and media players (five at most), its temperature and humidity sensors and its climate. Then a *Home* page (TV, phone battery, a second temperature, climate, pots, weather: filled in only when your Home Assistant has a single answer) and a *Calendars and people* page (the « Tab5 · … » lists: a choice already made stays, a list still on « Aucun » gets a suggestion when a single entity fits, such as a calendar named « work »). Every page is filled in already: check it, change what you like, *Submit*. The last page sums it up; submitting writes the automation into `automations.yaml` (saved first in `tab5_sauvegardes/automatisations/`) and reloads the automations, then sets the lists that changed. An automation of the blueprint that already exists only changes if you tick « Replace the rooms and home inputs of the existing automation », and then only those ([ADR-0053](../decisions/0053-setup-assistant-home-lists.md)). If `automations.yaml` cannot be written safely (another include, a `!secret` in it), nothing is written and a notification gives the YAML to paste. Start it again any time: *Settings → Devices & services → Tab5 → Configure*, box « Start the setup assistant ». It needs your devices assigned to areas in Home Assistant; without any, it says so and changes nothing.

## A. Download and unzip

Download `tab5_home_assistant.zip` from the latest release (*Assets*, at the bottom of the release) and unzip it into Home Assistant's `config/` folder, the one holding `configuration.yaml`: Samba share, or the File editor or Studio Code Server add-on. It adds three folders:

| Folder | What it holds |
|---|---|
| `packages/` | the automations and scripts that feed the screen, and the « Tab5 · » lists of step 5 |
| `custom_templates/` | the dashboard of step 7, and the reader of some weather warnings |
| `blueprints/automation/tab5/` | the blueprint of step 6 |

> [!WARNING]
> **The folders go straight into `config/`.** Some tools unzip into a new folder named after the archive (`tab5_home_assistant/`). The three folders must sit directly in `config/`, next to `configuration.yaml`. A `packages/` folder you already have keeps its files: the archive only adds its own.

A fourth folder, `tab5_optionnel/`, is not loaded: see [a shutter that reports nothing](#a-shutter-that-reports-nothing).

## B. One line in configuration.yaml

The only YAML you write (skip it if it is already there):

```yaml
homeassistant:
  packages: !include_dir_named packages
```

If `homeassistant:` already exists, add only the `packages:` line under it, indented like the lines already there.

Optional, from 3.8: the alert raised when a push to the tablet fails to render (`packages/tab5_health.yaml`) also needs `system_log: fire_event: true` in `configuration.yaml`, then a restart. HACS cannot add it for you; without it, that one alert stays silent and everything else works.

## C. Check, then restart

*Developer tools → YAML → Check configuration*, then **restart** Home Assistant (*Settings → ⋮ → Restart Home Assistant*).

**It worked if** *Settings → Devices & services → Entities*, searched for **« Tab5 · »**, lists the « Tab5 · source des prévisions », « Tab5 · agenda de travail »… entities. You will set them at step 5, once the tablet is there.

## A shutter that reports nothing

`tab5_optionnel/volet_serre_tracking.yaml` is only for a shutter that reports neither its position nor its travel (the author's Tuya motor): copy it into `packages/`, reload, and pick the shutter in « Tab5 · volet à course simulée ». It is not installed by default: the shutter picked in that list no longer gets the blueprint's commands but the package's script, which times its travel. Set that time in « Tab5 · course du volet » (seconds, 26 by default; from 3.8). Left on « Aucun », the package changes nothing. With HACS, copy it from `config/custom_components/tab5/fichiers/tab5_optionnel/`: once it is in `packages/`, the integration updates it with the others.

## Where these files come from

The same files are in the repository (`HomeAssistant_Config/packages/`, `custom_templates/`, `blueprints/`, `optionnel/`); [`HomeAssistant_Config/README.md`](../../HomeAssistant_Config/README.md) says what each package does. They are exactly what runs on the author's Home Assistant since 2026-09-26, with no per-home edit since 2026-09-28: there are no private versions, and nothing to merge into `automations.yaml` or `scripts.yaml`.

Already installed and updating? [Updates](updates.md#home-assistant-files).

**Next: [step 2, install the firmware](flash.md).**

---

## Version Française

---

Tout le côté Home Assistant tient dans une archive, **`tab5_home_assistant.zip`**, jointe à chaque [release](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases). **Rien à remplir** : chaque valeur de votre maison se choisit ensuite dans Home Assistant, à la souris ([ADR-0024](../decisions/0024-packages-without-placeholders.md)).

Deux façons de les mettre en place : [avec HACS](#avec-hacs), qui les met ensuite à jour en un clic à chaque release, ou à la main, parties A à C ci-dessous.

## Avec HACS

Depuis la release 3.7.0, les fichiers viennent aussi avec une petite intégration, « Tab5 », que [HACS](https://hacs.xyz) installe ([ADR-0035](../decisions/0035-hacs-integration-ha-files.md)). HACS doit déjà être installé. HACS ne propose que les releases complètes : les pré-releases (canal Bêta) seulement si les versions bêta sont activées pour ce dépôt dans HACS.

1. Ajoutez la ligne `packages:` de la [partie B](#b-une-ligne-dans-configurationyaml) à `configuration.yaml`, si elle n'y est pas déjà.
2. **[Ouvrir Tab5 dans HACS](https://my.home-assistant.io/redirect/hacs_repository/?owner=Axellum&repository=M5-Tab5-ESPHome-LVGL&category=integration)** : le lien passe par My Home Assistant, qui demande l'adresse de votre Home Assistant la première fois, puis HACS propose d'ajouter le dépôt : *Add* (ajouter ; une boîte avec un titre seul : rechargez la page, [incidents connus](../troubleshooting.md#ajout-de-tab5-dans-hacs---dépôt-introuvable--ou-une-boîte-vide-07102026)). Sans le lien : *⋮ (en haut à droite) → Custom repositories* (dépôts personnalisés), dépôt `https://github.com/Axellum/M5-Tab5-ESPHome-LVGL`, type **Integration**, *Add*, puis cherchez **Tab5** dans HACS et ouvrez-le.
3. Sur la page de Tab5 dans HACS, *Download* (télécharger).
4. **Redémarrez** Home Assistant.
5. *Paramètres → Appareils et services → Ajouter une intégration → Tab5*. Laissez « Mettre ensuite la tablette à jour » coché pour que le firmware de la même version s'installe après les fichiers, à chaque release ([mises à jour](updates.md#fichiers-home-assistant)).

L'intégration pose alors les fichiers toute seule, sans autre redémarrage. **C'est bon si** une notification « Tab5 : fichiers Home Assistant 3.x.y » dit combien de fichiers ont été posés, et que les entités « Tab5 · » sont là (voir la [partie C](#c-vérifier-puis-redémarrer)). Vos propres packages, modèles et blueprints ne sont jamais touchés, et retirer l'intégration laisse les fichiers en place. Rien d'autre à faire sur cette page : passez à l'[étape 2, installer le firmware](flash.md#version-française).

**Assistant de configuration** (depuis la 3.8, [ADR-0052](../decisions/0052-setup-assistant-rooms.md)) : une fois les fichiers en place, *Paramètres → Système → Réparations* affiche « Tab5 : configurer la tablette depuis vos pièces ». Il prépare l'automatisation de l'[étape 6](devices.md#version-française) à partir de vos pièces de Home Assistant : jusqu'à cinq pièces, les mieux équipées d'abord, chacune avec ses lumières, volets, interrupteurs, ventilateurs et lecteurs (cinq au plus), ses capteurs de température et d'humidité et sa climatisation. Puis une page *Maison* (TV, batterie du téléphone, seconde température, clim, pots, météo : remplis seulement quand votre Home Assistant n'a qu'une réponse) et une page *Agendas et personnes* (les listes « Tab5 · … » : un choix déjà fait reste, une liste encore sur « Aucun » reçoit une proposition quand une seule entité convient, par exemple un agenda nommé « travail »). Chaque page est déjà remplie : vérifiez, changez ce que vous voulez, *Valider*. La dernière page récapitule ; valider écrit l'automatisation dans `automations.yaml` (sauvegardé avant dans `tab5_sauvegardes/automatisations/`) et recharge les automatisations, puis règle les listes changées. Une automatisation du blueprint déjà là ne change que si vous cochez « Remplacer les pièces et les entrées de la maison de l'automatisation existante », et seulement celles-ci ([ADR-0053](../decisions/0053-setup-assistant-home-lists.md)). Si `automations.yaml` ne peut pas être écrit sans risque (autre inclusion, un `!secret` dedans), rien n'est écrit et une notification donne le YAML à coller. Relancez-le quand vous voulez : *Paramètres → Appareils et services → Tab5 → Configurer*, case « Lancer l'assistant de configuration ». Il faut vos appareils rattachés à des pièces dans Home Assistant ; sans aucune, il le dit et ne change rien.

## A. Télécharger et décompresser

Téléchargez `tab5_home_assistant.zip` depuis la dernière release (*Assets*, en bas de la release) et décompressez-la dans le dossier `config/` de Home Assistant, celui de `configuration.yaml` : partage Samba, ou le module File editor ou Studio Code Server. Elle y ajoute trois dossiers :

| Dossier | Ce qu'il contient |
|---|---|
| `packages/` | les automatisations et scripts qui alimentent l'écran, et les listes « Tab5 · » de l'étape 5 |
| `custom_templates/` | le tableau de bord de l'étape 7, et le lecteur de certaines vigilances |
| `blueprints/automation/tab5/` | le blueprint de l'étape 6 |

> [!WARNING]
> **Les dossiers vont directement dans `config/`.** Certains outils décompressent dans un nouveau dossier au nom de l'archive (`tab5_home_assistant/`). Les trois dossiers doivent être directement dans `config/`, à côté de `configuration.yaml`. Un dossier `packages/` que vous avez déjà garde ses fichiers : l'archive n'ajoute que les siens.

Un quatrième dossier, `tab5_optionnel/`, n'est pas chargé : voir [un volet qui ne signale rien](#un-volet-qui-ne-signale-rien).

## B. Une ligne dans configuration.yaml

La seule ligne de YAML à écrire (rien à faire si elle y est déjà) :

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Si `homeassistant:` existe déjà, ajoutez seulement la ligne `packages:` dessous, en retrait comme les lignes déjà là.

Facultatif, depuis la 3.8 : l'alerte levée quand une poussée vers la tablette ne se rend pas (`packages/tab5_health.yaml`) demande aussi `system_log: fire_event: true` dans `configuration.yaml`, puis un redémarrage. HACS ne peut pas l'ajouter pour vous ; sans elle, cette seule alerte reste muette, tout le reste marche.

## C. Vérifier, puis redémarrer

*Outils de développement → YAML → Vérifier la configuration*, puis **redémarrez** Home Assistant (*Paramètres → ⋮ → Redémarrer Home Assistant*).

**C'est bon si** *Paramètres → Appareils et services → Entités*, en cherchant **« Tab5 · »**, liste les entités « Tab5 · source des prévisions », « Tab5 · agenda de travail »… Vous les réglerez à l'étape 5, une fois la tablette là.

## Un volet qui ne signale rien

`tab5_optionnel/volet_serre_tracking.yaml` ne sert qu'à un volet qui ne signale ni sa position ni sa course (le moteur Tuya de l'auteur) : copiez-le dans `packages/`, rechargez, et choisissez le volet dans « Tab5 · volet à course simulée ». Il n'est pas installé par défaut : le volet choisi dans cette liste ne reçoit plus les commandes du blueprint mais celles du script du package, qui chronomètre sa course. Réglez cette durée dans « Tab5 · course du volet » (en secondes, 26 par défaut ; depuis la 3.8). Laissée sur « Aucun », la liste ne change rien. Avec HACS, copiez-le depuis `config/custom_components/tab5/fichiers/tab5_optionnel/` : une fois dans `packages/`, l'intégration le met à jour avec les autres.

## D'où viennent ces fichiers

Les mêmes fichiers sont dans le dépôt (`HomeAssistant_Config/packages/`, `custom_templates/`, `blueprints/`, `optionnel/`) ; [`HomeAssistant_Config/README.md`](../../HomeAssistant_Config/README.md#version-française) dit le rôle de chaque package. Ils sont exactement ce qui tourne sur le Home Assistant de l'auteur depuis le 26/09/2026, sans aucune retouche propre à sa maison depuis le 28/09/2026 : pas de version privée, rien à fusionner dans `automations.yaml` ou `scripts.yaml`.

Déjà installé, et vous mettez à jour ? [Mises à jour](updates.md#fichiers-home-assistant).

**Ensuite : [étape 2, installer le firmware](flash.md#version-française).**
