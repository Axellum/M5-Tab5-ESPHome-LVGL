# Step 1 — Home Assistant files

## English · [Français](#version-française)

---

The whole Home Assistant side is one archive, **`tab5_home_assistant.zip`**, attached to each [release](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases). **Nothing to fill in**: every value of your home is picked afterwards in Home Assistant, with the mouse ([ADR-0024](../decisions/0024-packages-without-placeholders.md)).

## 1. Download and unzip

Download `tab5_home_assistant.zip` from the latest release (*Assets*, at the bottom of the release) and unzip it into Home Assistant's `config/` folder, the one holding `configuration.yaml`: Samba share, or the File editor or Studio Code Server add-on. It adds three folders:

| Folder | What it holds |
|---|---|
| `packages/` | the automations and scripts that feed the screen, and the « Tab5 · » lists of step 5 |
| `custom_templates/` | the dashboard of step 7, and the reader of some weather warnings |
| `blueprints/automation/tab5/` | the blueprint of step 6 |

> [!WARNING]
> **The folders go straight into `config/`.** Some tools unzip into a new folder named after the archive (`tab5_home_assistant/`). The three folders must sit directly in `config/`, next to `configuration.yaml`. A `packages/` folder you already have keeps its files: the archive only adds its own.

A fourth folder, `tab5_optionnel/`, is not loaded: see [a shutter that reports nothing](#a-shutter-that-reports-nothing).

## 2. One line in configuration.yaml

The only YAML you write (skip it if it is already there):

```yaml
homeassistant:
  packages: !include_dir_named packages
```

If `homeassistant:` already exists, add only the `packages:` line under it, indented like the lines already there.

## 3. Check, then restart

*Developer tools → YAML → Check configuration*, then **restart** Home Assistant (*Settings → ⋮ → Restart Home Assistant*).

**It worked if** *Settings → Devices & services → Entities*, searched for **« Tab5 · »**, lists the « Tab5 · source des prévisions », « Tab5 · agenda de travail »… entities. You will set them at step 5, once the tablet is there.

## A shutter that reports nothing

`tab5_optionnel/volet_serre_tracking.yaml` is only for a shutter that reports neither its position nor its travel (the author's Tuya motor): copy it into `packages/`, reload, and pick the shutter in « Tab5 · volet à course simulée ». It is not installed by default: the shutter picked in that list no longer gets the blueprint's commands but the package's script, which times its travel. Left on « Aucun », the package changes nothing.

## Where these files come from

The same files are in the repository (`HomeAssistant_Config/packages/`, `custom_templates/`, `blueprints/`, `optionnel/`); [`HomeAssistant_Config/README.md`](../../HomeAssistant_Config/README.md) says what each package does. They are exactly what runs on the author's Home Assistant since 2026-09-26, with no per-home edit since 2026-09-28: there are no private versions, and nothing to merge into `automations.yaml` or `scripts.yaml`.

Already installed and updating? [Updates](updates.md#home-assistant-files).

**Next: [step 2, install the firmware](flash.md).**

---

## Version Française

---

Tout le côté Home Assistant tient dans une archive, **`tab5_home_assistant.zip`**, jointe à chaque [release](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases). **Rien à remplir** : chaque valeur de votre maison se choisit ensuite dans Home Assistant, à la souris ([ADR-0024](../decisions/0024-packages-without-placeholders.md)).

## 1. Télécharger et décompresser

Téléchargez `tab5_home_assistant.zip` depuis la dernière release (*Assets*, en bas de la release) et décompressez-la dans le dossier `config/` de Home Assistant, celui de `configuration.yaml` : partage Samba, ou le module File editor ou Studio Code Server. Elle y ajoute trois dossiers :

| Dossier | Ce qu'il contient |
|---|---|
| `packages/` | les automatisations et scripts qui alimentent l'écran, et les listes « Tab5 · » de l'étape 5 |
| `custom_templates/` | le tableau de bord de l'étape 7, et le lecteur de certaines vigilances |
| `blueprints/automation/tab5/` | le blueprint de l'étape 6 |

> [!WARNING]
> **Les dossiers vont directement dans `config/`.** Certains outils décompressent dans un nouveau dossier au nom de l'archive (`tab5_home_assistant/`). Les trois dossiers doivent être directement dans `config/`, à côté de `configuration.yaml`. Un dossier `packages/` que vous avez déjà garde ses fichiers : l'archive n'ajoute que les siens.

Un quatrième dossier, `tab5_optionnel/`, n'est pas chargé : voir [un volet qui ne signale rien](#un-volet-qui-ne-signale-rien).

## 2. Une ligne dans configuration.yaml

La seule ligne de YAML à écrire (rien à faire si elle y est déjà) :

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Si `homeassistant:` existe déjà, ajoutez seulement la ligne `packages:` dessous, en retrait comme les lignes déjà là.

## 3. Vérifier, puis redémarrer

*Outils de développement → YAML → Vérifier la configuration*, puis **redémarrez** Home Assistant (*Paramètres → ⋮ → Redémarrer Home Assistant*).

**C'est bon si** *Paramètres → Appareils et services → Entités*, en cherchant **« Tab5 · »**, liste les entités « Tab5 · source des prévisions », « Tab5 · agenda de travail »… Vous les réglerez à l'étape 5, une fois la tablette là.

## Un volet qui ne signale rien

`tab5_optionnel/volet_serre_tracking.yaml` ne sert qu'à un volet qui ne signale ni sa position ni sa course (le moteur Tuya de l'auteur) : copiez-le dans `packages/`, rechargez, et choisissez le volet dans « Tab5 · volet à course simulée ». Il n'est pas installé par défaut : le volet choisi dans cette liste ne reçoit plus les commandes du blueprint mais celles du script du package, qui chronomètre sa course. Laissée sur « Aucun », la liste ne change rien.

## D'où viennent ces fichiers

Les mêmes fichiers sont dans le dépôt (`HomeAssistant_Config/packages/`, `custom_templates/`, `blueprints/`, `optionnel/`) ; [`HomeAssistant_Config/README.md`](../../HomeAssistant_Config/README.md#version-française) dit le rôle de chaque package. Ils sont exactement ce qui tourne sur le Home Assistant de l'auteur depuis le 26/09/2026, sans aucune retouche propre à sa maison depuis le 28/09/2026 : pas de version privée, rien à fusionner dans `automations.yaml` ou `scripts.yaml`.

Déjà installé, et vous mettez à jour ? [Mises à jour](updates.md#fichiers-home-assistant).

**Ensuite : [étape 2, installer le firmware](flash.md#version-française).**
