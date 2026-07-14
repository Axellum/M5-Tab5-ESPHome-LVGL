# Home Assistant Configuration for Tab5

## English · [Français](#version-française)

---

This folder contains the Home Assistant side of the Tab5 integration: automations that push data to the device, scripts triggered by the device, and template sensors that pre-process data before it's sent.

These are **example files** — they reflect the author's own Home Assistant setup. You will need to adapt entity names to match your own installation.

> **Production Freebox (Axel) — ne pas copier tel quel dans `packages/`**
>
> Si `configuration.yaml` charge déjà `automation: !include automations.yaml`, `script: !include scripts.yaml` et `template: … !include_dir_merge_list template_sensors`, ces trois fichiers doivent être **fusionnés / inclus à la racine**, pas déposés dans `packages/tab5/`.
>
> Un package HA (`!include_dir_named packages`) exige un **dictionnaire** avec des clés d'intégration (`automation:`, `script:`, `template:`). Copier une liste d'automations ou des scripts « nus » dans `packages/` provoque exactement les avertissements :
> `expected a dictionary` et `Integration 'tab5_volet_action' not found`.
>
> Seul `packages/tab5_health.yaml` est au format package valide. Le script `scripts/tab5_ha_sync/sync_to_freebox.ps1` ne copie vers `packages/tab5/` **que** avec `-CopyPackages` (déconseillé en prod).

---

## Files

### `automations_tab5.yaml`
The main push automation. Triggered by state changes in Home Assistant, it pushes updated data to the Tab5 via native ESPHome service calls.

What it pushes:
- **Weather (7-day forecast):** on weather entity state change — queries Météo-France entities and serializes 7 × (icon code, max temp, min temp, condition) into a semicolon-delimited string
- **Hourly rain chart:** on `sensor.next_rain` state change — builds a 60-value rain probability array from Météo-France's `v1/vision/rain` data
- **Room temperatures and humidity:** on sensor state change — direct value push
- **Climate state:** on climate entity state change — mode + target temp + current temp
- **Calendar events:** on `calendar.sync` trigger — fetches up to 4 upcoming events from Google Calendar and formats them with title, time, and color tag
- **Weather alerts:** on `sensor.weather_alert` state change — alert text + severity level
- **Plant moisture:** on BLE sensor update — 5 moisture values + temperature

**Traffic pacing:** the automation uses `delay: 1s` between each push block and `delay: 150ms` within forecast loops. This prevents multiple large payloads from overwhelming the ESP32-P4's TCP socket buffer simultaneously with the active I2S audio stream.

### `automations_examples.yaml.example`
Same as above but with generic placeholder names. Start here if you want to build the automation from scratch for your own setup.

---

### `scripts_tab5.yaml`
Scripts called **by** the Tab5 (from YAML `button:` or `on_press:` blocks in the ESPHome config).

Examples:
- Toggle a specific light group
- Adjust roller shutter position
- Trigger a media player action

These are simple pass-through scripts — the device sends a button press event, the script executes the corresponding HA action. This keeps the ESPHome code thin and the logic on the HA side where it belongs.

### `scripts_examples.yaml`
Generic example versions of the same scripts.

---

### `template_sensors_meteo_tab5.yaml`
Template sensors that pre-process Météo-France data into strings the Tab5 expects.

The main one is a sensor that generates a short weather sentence from the next-rain forecast:
- `"Pluie dans 10 min"` if rain is coming
- `"Pas de pluie prévue"` if clear
- `"Averses possibles"` for uncertain conditions

This runs on the HA side rather than on the device to keep the C++ code simple. Add this to your `template:` block in `configuration.yaml` or in a dedicated `template.yaml` file.

### `template_sensors_examples.yaml`
Generic placeholder version.

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
HA state change (e.g., outdoor temp sensor updates)
  ↓
Automation trigger fires
  ↓
HA calls: service: esphome.tab5_ha_hmi_tab5_update_meteo_7j
          data:
            payload: "0;Soleil;28;15;1;Nuageux;24;13;..."
  ↓
ESPHome receives the service call
  ↓
C++ function parse_meteo_7j() runs, updates LVGL labels
```

The device never polls. It only receives. When nothing changes in HA, the device uses near-zero CPU.

---

---

## Version Française

---

Ce dossier contient le côté Home Assistant de l'intégration Tab5 : automations qui poussent des données vers l'appareil, scripts déclenchés par l'appareil, et template sensors qui pré-traitent les données avant envoi.

Ce sont des **fichiers d'exemple** — ils reflètent le setup Home Assistant de l'auteur. Vous devrez adapter les noms d'entités pour correspondre à votre propre installation.

---

## Fichiers

### `automations_tab5.yaml`
L'automatisation push principale. Déclenchée par les changements d'état dans Home Assistant, elle pousse les données mises à jour vers le Tab5 via des appels de service ESPHome natifs.

Ce qu'elle pousse :
- **Météo (prévisions 7 jours) :** sur changement d'état de l'entité météo — interroge les entités Météo-France et sérialise 7 × (code icône, temp max, temp min, condition) en chaîne délimitée par des points-virgules
- **Graphique pluie horaire :** sur changement de `sensor.next_rain` — construit un tableau de 60 valeurs de probabilité de pluie
- **Températures et humidité des pièces :** sur changement de capteur — push direct de valeur
- **État climatisation :** sur changement de l'entité climate — mode + temp cible + temp actuelle
- **Événements calendrier :** sur déclencheur `calendar.sync` — récupère jusqu'à 4 événements Google Calendar prochains formatés avec titre, heure et tag couleur
- **Alertes météo :** sur changement de `sensor.weather_alert` — texte d'alerte + niveau de sévérité
- **Humidité plantes :** sur mise à jour capteur BLE — 5 valeurs d'humidité + température

**Traffic pacing :** l'automatisation utilise `delay: 1s` entre chaque bloc push et `delay: 150ms` dans les boucles de prévisions. Cela empêche plusieurs gros payloads de saturer le buffer de sockets TCP de l'ESP32-P4 simultanément avec le flux audio I2S actif.

---

### `scripts_tab5.yaml`
Scripts appelés **par** le Tab5 (depuis des blocs `button:` ou `on_press:` dans la config ESPHome).

Exemples :
- Basculer un groupe de lumières spécifique
- Ajuster la position d'un volet roulant
- Déclencher une action media player

Ce sont des scripts de pass-through simples — l'appareil envoie un événement de pression de bouton, le script exécute l'action HA correspondante. Ça garde le code ESPHome léger et la logique côté HA où est sa place.

---

### `template_sensors_meteo_tab5.yaml`
Template sensors qui pré-traitent les données Météo-France en chaînes attendues par le Tab5.

Le principal génère une courte phrase météo depuis les prévisions de pluie :
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
Changement d'état HA (ex: capteur temp extérieure se met à jour)
  ↓
Déclencheur d'automatisation se déclenche
  ↓
HA appelle : service: esphome.tab5_ha_hmi_tab5_update_meteo_7j
             data:
               payload: "0;Soleil;28;15;1;Nuageux;24;13;..."
  ↓
ESPHome reçoit l'appel de service
  ↓
La fonction C++ parse_meteo_7j() s'exécute, met à jour les labels LVGL
```

L'appareil ne poll jamais. Il reçoit seulement. Quand rien ne change dans HA, l'appareil utilise un CPU quasi nul.
