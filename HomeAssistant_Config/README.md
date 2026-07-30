# Home Assistant Configuration for Tab5

## English · [Français](#version-française)

---

This folder contains the Home Assistant side of the Tab5 integration: automations that push data to the device, scripts triggered by the device, and template sensors that pre-process data before it's sent.

These are **example files** — they reflect the author's own Home Assistant setup. You will need to adapt entity names to match your own installation.

> **What is actually in a clone.** Only the `*_examples*` files and everything under `packages/` and `snippets/` are versioned. The author's real production files (`automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`) are gitignored: the examples below are generated from them with placeholder entity IDs.

> **Root include vs. `packages/` — pick the right one.** The three `*_examples*` files are **not** packages: they are a bare automation *list*, a bare script *dict* and a bare template *list*. Merge them into your existing `automations.yaml` / `scripts.yaml` / `template:` block. Dropping them into `packages/` gives you `expected a dictionary` and `Integration 'tab5_volet_action' not found`, because a HA package must be a dictionary keyed by integration (`automation:`, `script:`, `template:`).
> Everything in `packages/` *is* in valid package format and goes there.

---

## Files

### `automations_examples.yaml.example`
The main push automation, with generic placeholder entity names. Triggered by state changes in Home Assistant, it pushes updated data to the Tab5 via native ESPHome service calls. This is the file to start from.

What it pushes:
- **Daily forecast (15 days):** on weather entity state change — serializes 15 × (index, day label, condition, min, max, weekend/holiday flags, work hours) into a `|`/`;`-delimited string sent to `tab5_maj_previsions_jours_bulk`
- **Hourly forecast (15 slots):** three chunks of 5 through `tab5_maj_previsions_heures_bulk`
- **Short-term rain chart:** on `sensor.*_next_rain` state change — **9** bars (`tab5_maj_pluie_1h`, one call per index 0–8, i.e. 0/5/10/…/55 min) built from Météo-France's `v1/vision/rain` data
- **Current weather / probabilities:** `tab5_maj_meteo_actuelle` (condition, temperature, humidity) and `tab5_maj_probabilites` (UV, frost, snow)
- **Climate state:** on climate entity state change — `tab5_maj_clim` (target, current, mode, preset, fan, swing)
- **Shutter state:** `tab5_maj_volet_etat` — also arms the device-local “Stop” wake word while the shutter moves
- **Info banner:** `tab5_maj_info_texte` (text, colour, dismiss id) — 3-day calendar recap or a weather-alert banner
- **Météo-France vigilance:** `tab5_maj_alerte_meteo_france` — a single 11-field `|`-delimited payload
- **HA alert queue:** `tab5_maj_alertes_ha_bulk` — up to 4 banners in the central rotator (see `packages/tab5_alerts.yaml`)

Room temperatures, humidity, light states and plant moisture do **not** go through these services: they are “mirror” entities (`platform: homeassistant` in `Tab5/tab5-sensors-domotique.yaml`), which HA syncs automatically — nothing to write on the HA side.

**Traffic pacing:** the automation uses `delay: 1s` between each push block and `delay: 150ms` within forecast loops. This prevents multiple large payloads from overwhelming the ESP32-P4's TCP socket buffer simultaneously with the active I2S audio stream.

---

### `scripts_examples.yaml`
Scripts called **by** the Tab5 (from a `homeassistant.service:` in `Tab5/tab5-api-logic.yaml` or an LVGL `on_short_click:`), not the other way round.

The main one is `tab5_volet_action` (open / close / stop): the device sends the intent, the script drives the cover *and* maintains the two helpers that track a blind Tuya motor's state. Simple pass-through — it keeps the ESPHome code thin and the logic on the HA side where it belongs.

---

### `template_sensors_examples.yaml`
Template sensors that pre-process Météo-France data into short strings the Tab5 expects.

The main one generates a weather sentence from the next-rain forecast:
- `"Pluie dans 10 min"` if rain is coming
- `"Pas de pluie prévue"` if clear
- `"Averses possibles"` for uncertain conditions

This runs on the HA side rather than on the device to keep the C++ code simple. Add it to your `template:` block in `configuration.yaml` or a dedicated `template.yaml`.

---

### `packages/tab5_health.yaml`
Health-monitoring package: three guard automations that alert when the push pipeline silently degrades. Because the Tab5 is push-only (see `docs/decisions/0001-push-only-zero-polling.md`), a stale screen raises no error on its own — these automations are the HA-side safety net.

What it watches:
- **`input_boolean.is_primary_active` OFF for more than 5 min** — this boolean gates every push automation; stuck OFF means the screen silently freezes (a real incident, see `docs/troubleshooting.md`)
- **`sensor.tab5_uptime` decreasing** — unexpected device reboot (brownout, firmware crash, power cut); a plain Wi-Fi drop without reboot does *not* trigger it
- **`HA API Status` off/unavailable for more than 2 min** — device unreachable, every push fails during the outage

Design notes:
- Every notification action carries `continue_on_error: true` so one failing channel doesn't block the others
- No template uses raw `now()` — detection relies on trigger `for:` windows and `trigger.from_state` / `trigger.to_state`
- Numeric comparisons use `| float(0)` defaults (boot safety)

It's a self-contained HA *package*; enable packages in `configuration.yaml` first:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Then adapt the entity names at the top of the file (`notify.notify`, the `tab5_ha_hmi` entity prefix, and uncomment the `input_boolean` block if the helper doesn't exist in your setup).

---

### `packages/tab5_calendar.yaml`
Backend of the firmware's **calendar popup** (long press on the clock). Two scripts called *by the device* (`homeassistant.service:`), both `mode: restart`:

- **`tab5_calendrier_mois`** (`annee`, `mois`) — reads the work / public-holidays / family / birthdays calendars over the requested month and pushes back `esphome.<device>_tab5_maj_calendrier_mois`: a 62-hex-char string (2 per day — bits: work / public holiday / school holiday / appointment / birthday) plus 31 `|`-separated work-hour fields
- **`tab5_calendrier_jour`** (`date`) — builds the day-detail lines (`type|text;...`, max 6) and pushes `esphome.<device>_tab5_maj_calendrier_jour`

School holidays come from a **static Zone A table** (Bordeaux academy) verified against data.education.gouv.fr — edit it for your zone, and extend it once the next school year is published (see the `@ai_warning` in the file). The Google public-holidays calendar mixes real holidays with civil observances, hence the `feries_connus` whitelist. Same package install as above.

---

### `packages/tab5_alerts.yaml`
Backend of the **HA alert queue** — panels 4 to 7 of the central rotating card. Provides the `input_text.tab5_alerts_dismissed` helper (the dismiss list), the `tab5_dismiss_alert` script the device calls when you tap a banner, and the automation that builds the `tab5_maj_alertes_ha_bulk` payload (max 4 banners, already-dismissed ids filtered out).

Tapping a banner on screen removes it immediately and stores its id here, so a re-push of the same id stays hidden until HA sends a new one. `snippets/tab5_alerts_dismissed_input_text.yaml` is the same helper on its own, if you prefer declaring it in your existing `input_text:` block instead of loading the whole package.

---

### `packages/volet_serre_tracking.yaml`
Helpers + central script for a roller shutter whose motor reports **no position and no end-stop** (typical cheap Tuya module). An `input_boolean` is armed for the measured travel time and an `input_text` carries the label shown on screen; the push automation forwards that label to `tab5_maj_volet_etat`.

Adapt the `26 s` travel delay to your own shutter, and route every other shutter automation (sunrise/sunset, HA UI) through `script.tab5_volet_action` — otherwise the screen won't know the shutter moved.

---

## Adapting to your setup

Replace these placeholders throughout the files:

| Placeholder | What to replace with |
|-------------|---------------------|
| `VOTRE_VILLE` | Your city entity from Météo-France (`weather.your_city`) |
| `VOTRE_DEPARTEMENT` | Your department number for weather alerts (e.g., `40`) |
| `VOTRE_CLIMATISATION` | Your climate entity (`climate.your_ac_unit`) |
| `VOTRE_EMAIL_gmail_com` | Your Google Calendar entity (`calendar.your_email_gmail_com`) |
| `VOTRE_VOLET` | Your roller shutter / cover entity |
| `tab5-ha-hmi` | Your ESPHome device name (as configured in `tab5-ha-hmi.yaml`) |

After editing:

1. In Home Assistant, go to **Developer Tools → YAML → Reload Automations** (or restart HA)
2. The Tab5 should receive its first push within a few seconds of connecting to the API

---

## How the push works (quick summary)

```
HA state change (e.g., the weather entity updates)
  ↓
Automation trigger fires
  ↓
HA calls: action: esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk
          data:
            payload: "0|Auj 30|sunny|16.0|28.0|0|0|0|09h00 - 17h30;1|Ven 31|..."
  ↓
ESPHome receives the service call (Tab5/tab5-api-logic.yaml)
  ↓
C++ parse_and_update_jours_bulk() runs, updates the LVGL labels in one pass
```

The service name seen by HA is `esphome.<device_name>_<service>` — with the stock `name: tab5-ha-hmi`, `tab5_maj_previsions_jours_bulk` becomes `esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk`. Rename the device and every call in the automation has to follow.

The device never polls. It only receives. When nothing changes in HA, the device uses near-zero CPU.

---

---

## Version Française

---

Ce dossier contient le côté Home Assistant de l'intégration Tab5 : automations qui poussent des données vers l'appareil, scripts déclenchés par l'appareil, et template sensors qui pré-traitent les données avant envoi.

Ce sont des **fichiers d'exemple** — ils reflètent le setup Home Assistant de l'auteur. Vous devrez adapter les noms d'entités pour correspondre à votre propre installation.

> **Ce qu'un clone contient réellement.** Seuls les fichiers `*_examples*` et tout ce qui est sous `packages/` et `snippets/` sont versionnés. Les vrais fichiers de production de l'auteur (`automations_tab5.yaml`, `scripts_tab5.yaml`, `template_sensors_meteo_tab5.yaml`) sont gitignorés : les exemples en sont dérivés avec des IDs d'entités placeholder.

> **Include à la racine ou `packages/` — ne pas confondre.** Les trois fichiers `*_examples*` ne sont **pas** des packages : ce sont une *liste* d'automations, un *dict* de scripts et une *liste* de templates, nus. Fusionnez-les dans vos `automations.yaml` / `scripts.yaml` / bloc `template:`. Les déposer dans `packages/` produit exactement `expected a dictionary` et `Integration 'tab5_volet_action' not found`, car un package HA doit être un dictionnaire à clés d'intégration (`automation:`, `script:`, `template:`).
> Tout ce qui est dans `packages/` est, lui, au bon format et va bien là.

---

## Fichiers

### `automations_examples.yaml.example`
L'automatisation push principale, avec des noms d'entités placeholder. Déclenchée par les changements d'état dans Home Assistant, elle pousse les données vers le Tab5 via des appels de service ESPHome natifs. C'est le fichier par lequel commencer.

Ce qu'elle pousse :
- **Prévisions journalières (15 jours) :** sur changement d'état de l'entité météo — sérialise 15 × (index, libellé jour, condition, min, max, drapeaux week-end/férié, heures de travail) en chaîne délimitée `|`/`;` vers `tab5_maj_previsions_jours_bulk`
- **Prévisions horaires (15 créneaux) :** trois chunks de 5 via `tab5_maj_previsions_heures_bulk`
- **Graphe de pluie court terme :** sur changement de `sensor.*_next_rain` — **9** barres (`tab5_maj_pluie_1h`, un appel par index 0–8, soit 0/5/10/…/55 min) construites depuis `v1/vision/rain` de Météo-France
- **Météo actuelle / probabilités :** `tab5_maj_meteo_actuelle` (condition, température, humidité) et `tab5_maj_probabilites` (UV, gel, neige)
- **État climatisation :** `tab5_maj_clim` (cible, actuelle, mode, preset, ventilation, oscillation)
- **État volet :** `tab5_maj_volet_etat` — arme aussi le wake word local « Stop » pendant le mouvement
- **Bandeau info :** `tab5_maj_info_texte` (texte, couleur, id de dismiss) — récap calendrier 3 jours ou bannière d'alerte météo
- **Vigilance Météo-France :** `tab5_maj_alerte_meteo_france` — un seul payload à 11 champs délimités `|`
- **File d'alertes HA :** `tab5_maj_alertes_ha_bulk` — jusqu'à 4 bandeaux dans le rotateur central (voir `packages/tab5_alerts.yaml`)

Les températures/humidités des pièces, les états de lumière et l'humidité des plantes ne passent **pas** par ces services : ce sont des entités « miroir » (`platform: homeassistant` dans `Tab5/tab5-sensors-domotique.yaml`), synchronisées automatiquement par HA — rien à écrire côté HA.

**Traffic pacing :** l'automatisation utilise `delay: 1s` entre chaque bloc push et `delay: 150ms` dans les boucles de prévisions. Cela empêche plusieurs gros payloads de saturer le buffer de sockets TCP de l'ESP32-P4 simultanément avec le flux audio I2S actif.

---

### `scripts_examples.yaml`
Scripts appelés **par** le Tab5 (depuis un `homeassistant.service:` de `Tab5/tab5-api-logic.yaml` ou un `on_short_click:` LVGL), et pas l'inverse.

Le principal est `tab5_volet_action` (open / close / stop) : l'appareil envoie l'intention, le script pilote le volet *et* maintient les deux helpers qui suivent l'état d'un moteur Tuya aveugle. Pass-through simple — ça garde le code ESPHome léger et la logique côté HA où est sa place.

---

### `template_sensors_examples.yaml`
Template sensors qui pré-traitent les données Météo-France en chaînes courtes attendues par le Tab5.

Le principal génère une phrase météo depuis les prévisions de pluie :
- `"Pluie dans 10 min"` si de la pluie arrive
- `"Pas de pluie prévue"` si dégagé
- `"Averses possibles"` pour les conditions incertaines

Ça tourne côté HA plutôt que sur l'appareil pour garder le code C++ simple. Ajoutez-le à votre bloc `template:` dans `configuration.yaml` ou dans un fichier `template.yaml` dédié.

---

### `packages/tab5_health.yaml`
Package de surveillance santé : trois automations de garde qui alertent quand le pipeline de push se dégrade silencieusement. Le Tab5 étant push-only (voir `docs/decisions/0001-push-only-zero-polling.md`), un écran figé ne lève aucune erreur par lui-même — ces automations sont le filet de sécurité côté HA.

Ce qui est surveillé :
- **`input_boolean.is_primary_active` OFF depuis plus de 5 min** — ce booléen conditionne toutes les automations de push ; bloqué sur OFF, l'écran se fige silencieusement (incident réel, voir `docs/troubleshooting.md`)
- **`sensor.tab5_uptime` qui redescend** — reboot inattendu de l'appareil (brownout, crash firmware, coupure d'alimentation) ; une simple coupure Wi-Fi sans reboot ne déclenche *pas*
- **`HA API Status` off/unavailable depuis plus de 2 min** — appareil injoignable, toutes les poussées échouent pendant la coupure

Notes de conception :
- Chaque action de notification porte `continue_on_error: true` : un canal en échec ne bloque pas les autres
- Aucun template n'utilise `now()` brut — la détection repose sur les fenêtres `for:` des déclencheurs et sur `trigger.from_state` / `trigger.to_state`
- Les comparaisons numériques utilisent des défauts `| float(0)` (sécurité au boot)

C'est un *package* HA autonome ; activez d'abord les packages dans `configuration.yaml` :

```yaml
homeassistant:
  packages: !include_dir_named packages
```

Puis adaptez les noms d'entités en tête de fichier (`notify.notify`, le préfixe d'entité `tab5_ha_hmi`, et décommentez le bloc `input_boolean` si le helper n'existe pas chez vous).

---

### `packages/tab5_calendar.yaml`
Backend du **popup calendrier** du firmware (appui long sur l'horloge). Deux scripts appelés *par l'appareil* (`homeassistant.service:`), tous deux `mode: restart` :

- **`tab5_calendrier_mois`** (`annee`, `mois`) — lit les calendriers boulot / jours fériés / famille / anniversaires sur le mois demandé et repousse `esphome.<device>_tab5_maj_calendrier_mois` : chaîne de 62 hex (2 par jour — bits : travail / férié / vacances scolaires / RDV / anniversaire) + 31 champs d'heures de travail séparés par `|`
- **`tab5_calendrier_jour`** (`date`) — construit les lignes de détail du jour (`type|texte;...`, max 6) et pousse `esphome.<device>_tab5_maj_calendrier_jour`

Les vacances scolaires viennent d'une **table statique Zone A** (académie de Bordeaux) vérifiée sur data.education.gouv.fr — adaptez-la à votre zone, et complétez-la à la publication de l'année scolaire suivante (voir l'`@ai_warning` dans le fichier). Le calendrier Google des jours fériés mélange vrais fériés et fêtes civiles, d'où la liste blanche `feries_connus`. Même installation package que ci-dessus.

---

### `packages/tab5_alerts.yaml`
Backend de la **file d'alertes HA** — panneaux 4 à 7 de la carte centrale rotative. Fournit le helper `input_text.tab5_alerts_dismissed` (liste de dismiss), le script `tab5_dismiss_alert` que l'appareil appelle au tap sur un bandeau, et l'automatisation qui construit le payload `tab5_maj_alertes_ha_bulk` (4 bandeaux max, ids déjà masqués filtrés).

Un tap sur un bandeau le retire tout de suite et mémorise son id ici : un re-push du même id reste masqué tant que HA n'envoie pas un id différent. `snippets/tab5_alerts_dismissed_input_text.yaml` contient le helper seul, si vous préférez le déclarer dans votre bloc `input_text:` existant plutôt que charger tout le package.

---

### `packages/volet_serre_tracking.yaml`
Helpers + script central pour un volet dont le moteur ne renvoie **ni position ni fin de course** (module Tuya bas de gamme typique). Un `input_boolean` est armé pendant la durée de course mesurée et un `input_text` porte le libellé affiché à l'écran ; l'automatisation push relaie ce libellé vers `tab5_maj_volet_etat`.

Adaptez le délai de course de `26 s` à votre volet, et faites passer toutes vos autres automatisations de volet (lever/coucher du soleil, UI HA) par `script.tab5_volet_action` — sinon l'écran ne saura pas que le volet a bougé.

---

## Adapter à votre setup

Remplacez ces placeholders dans les fichiers :

| Placeholder | Par quoi le remplacer |
|-------------|----------------------|
| `VOTRE_VILLE` | Votre entité ville Météo-France (`weather.votre_ville`) |
| `VOTRE_DEPARTEMENT` | Votre numéro de département pour les alertes (ex: `40`) |
| `VOTRE_CLIMATISATION` | Votre entité climate (`climate.votre_clim`) |
| `VOTRE_EMAIL_gmail_com` | Votre entité Google Calendar (`calendar.votre_email_gmail_com`) |
| `VOTRE_VOLET` | Votre entité volet roulant / cover |
| `tab5-ha-hmi` | Le nom de votre appareil ESPHome |

Après édition : dans HA, allez dans **Outils de développement → YAML → Recharger Automations** (ou redémarrez HA). Le Tab5 devrait recevoir son premier push en quelques secondes après connexion à l'API.

---

## Comment fonctionne le push (résumé rapide)

```
Changement d'état HA (ex: l'entité météo se met à jour)
  ↓
Déclencheur d'automatisation se déclenche
  ↓
HA appelle : action: esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk
             data:
               payload: "0|Auj 30|sunny|16.0|28.0|0|0|0|09h00 - 17h30;1|Ven 31|..."
  ↓
ESPHome reçoit l'appel de service (Tab5/tab5-api-logic.yaml)
  ↓
La fonction C++ parse_and_update_jours_bulk() s'exécute et met à jour tous les labels LVGL en une passe
```

Le nom vu par HA est `esphome.<nom_appareil>_<service>` — avec le `name: tab5-ha-hmi` livré, `tab5_maj_previsions_jours_bulk` devient `esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk`. Renommez l'appareil et tous les appels de l'automatisation doivent suivre.

L'appareil ne poll jamais. Il reçoit seulement. Quand rien ne change dans HA, l'appareil utilise un CPU quasi nul.
