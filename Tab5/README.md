# Tab5 ESPHome Files

## English · [Français](#version-française)

---

> ⚠️ This README was rewritten on 2026-07-05 to match the current codebase after the previous version was found describing an unrelated, outdated iteration of the project (different service names, a 6-page layout that no longer exists). If you find another mismatch, it's the code that's right — please fix this file, not the other way around.

This folder contains the ESPHome configuration packages and the C++ source files for the Tab5 firmware.

The entry point is `../tab5-ha-hmi.yaml` at the repository root. It loads `substitutions` from `Tab5/user_entities.yaml` (gitignored — copy `user_entities.example.yaml` and edit your HA entity IDs), declares the `on_boot` sequence, and the `packages:` import list for everything in this folder.

**Screen layout:** a single 1280×720 page (`page_main` in `tab5-lvgl.yaml`), not multiple pages. Navigation is by touch (clim/light/TV-remote popups, diagnostics console via the `btn_control_console` button, top right) and left/right swipes on the lower band of the screen (`y ≥ 333`) to cycle the 5 forecast pages (2 hourly + 3 daily windows). Since the 14/07/2026 swipe rework, the console is **not** opened by swipe anymore.

---

## `[AI-CONTEXT]` / `[AI-WARNING]` / `[AI-DEBUG]` convention (read this before editing)

Most files in this folder (and every file in `ui_components/`) start with a comment block tagged `[AI-CONTEXT]` — a short "system prompt" local to that file: its role, its architectural constraints, and explicit `@ai_instruction`s for common edits. Non-obvious decisions (a bug fix that looks removable, a duplication kept on purpose, a `!include` that must not be inlined) are documented **inside the file itself**, not only in the external knowledge base (`contexte_ia/` in the parent workspace) — a session that only has access to this repo (no cross-repo context) must still be able to find them.

A `[AI-WARNING]` (sometimes `[AI-WARNING-CRITICAL]`) marks something that looks like a bug/anti-pattern but is a deliberate, validated fix — e.g. the boot `delay(1000)` in `tab5-ha-hmi.yaml` (documented at length in the `logger:` block of `tab5-hardware.yaml`), or the pagination wrap logic in `handle_swipe_gesture()` (`tab5_custom.cpp`). **Read the warning before "fixing" it** — at least one of these was already reverted once after being "corrected" by an LLM audit that hadn't read it. See [`../docs/decisions/`](../docs/decisions/README.md) for the full reasoning behind each one.

A `[AI-DEBUG]` marks a good observation point when diagnosing a runtime issue — a log line worth watching, a diagnostics entity/overlay, or a technique already proven to work on this device (e.g. inserting a temporary marker directly into the real HA automation rather than reproducing its logic in an isolated test script, which can pass while masking the actual bug). See [`../docs/debugging.md`](../docs/debugging.md).

If you add a genuinely new architectural constraint or a non-obvious decision while editing a file, add or extend its `[AI-CONTEXT]` block rather than leaving the reasoning only in a commit message or an external doc.

---

## File descriptions

### `tab5-hardware.yaml`
Low-level hardware: display/touch buses, ES8388 DAC I2C init, speaker/mic I2S, PI4IOE5V6408 GPIO expander (Wi-Fi power/antenna switches), `ota:` (password-protected, see `secrets.yaml`). Also hosts the voice stack: `micro_wake_word` with **two models** — `okay_nabu` (always on when the wake-word switch is enabled) and `Stop` (armed only while the shutter moves, stops it locally) — and the `voice_assistant:` callbacks that drive the mic icon colors.

### `tab5-sensors-diagnostics.yaml`
System/network entities: the `wifi:` block, GPIO power switches (Wi-Fi, USB, external 5V, antenna select), HA API status, IP/SSID, uptime, Wi-Fi RSSI, core temperature, free RAM/loop time (`debug`), SNTP clock and the status-bar/console refresh `interval:`s.

### `tab5-sensors-domotique.yaml`
Home-automation entities pushed by HA over the ESPHome API: plant moisture (5×, dynamically sorted), light/PC state mirrors, phone battery, room & greenhouse temperature/humidity, audio (speaker amp, headphone jack, wake-word switch).

### `tab5-api-logic.yaml`
The `api: services:` block — the actual contract with Home Assistant. Each `tab5_maj_*` service receives a payload from an HA automation and calls into `tab5_custom.cpp` (via lambdas) to update the LVGL widgets. See the service table below.

### `tab5-globals.yaml`
All `globals:` (shared state read/written across files) + the 8s central-panel rotator (planning/rain/alerts/info — 4 panels, paused while off the default forecast window). See the globals table below.

### `tab5-scripts.yaml`
Reusable ESPHome `script:` blocks, grouped by family: **debounces** (`tab5_debounce_volume_set` 150 ms, `tab5_debounce_light_brightness` 200 ms, `tab5_debounce_clim_temp` 250 ms — one HA call per gesture instead of one per tick), **voice** (`tab5_vocal_arm_stop`/`tab5_vocal_disarm_stop` for the `Stop` wake word, `tab5_vocal_interrupt`/`tab5_vocal_interrupt_and_listen` for tap-to-interrupt, `tab5_assist_toggle`, `tab5_show_vocal_response`), **central rotator** (`tab5_central_rotator_auto`, `tab5_central_panel_next`, `tab5_dismiss_info_tap`, `tab5_dismiss_ha_alert` [paramétré slot 0-3]), **mode** (`tab5_set_assist_mode` — surbrillance bordure Domo/Discu centralisée), **shutter** (`tab5_volet_end_movement`, `tab5_volet_stop_voice_feedback`), **light popup** (`tab5_light_popup_show`), **calendar popup** (`tab5_calendar_open`, `tab5_cal_render`, `tab5_cal_prev`/`tab5_cal_next`/`tab5_cal_today`, `tab5_cal_day_tap`) and **assistant popup** (`tab5_assist_open`/`close`/`on_request`/`sync_settings`/`set_mode`/`set_text_size`). The temporary planning display moved to C++ (`show_temporary_planning()`, `tab5_custom.cpp`). Prefer adding a script here over duplicating a `delay` + action pattern inline.

### `tab5-styles.yaml`
All LVGL `style_definitions` (glassmorphism "Slate" theme) + font declarations (Roboto sizes, MDI icon sizes, weather icon font). Color tokens live in `UIColor::` (`tab5_custom.h`) — **never hardcode a hex color in a YAML lambda**, add a token instead.

### `tab5-lvgl.yaml`
The single-page layout: clock/date, status icons, quick-action buttons, climate card, moisture card, central rotating card, 5 forecast cards (daily/hourly), swipe gesture handling.

### `ui_components/*.yaml`
Included by `tab5-lvgl.yaml`: `climate_card.yaml`/`climate_popup.yaml` (near-fullscreen 1250×690 modal in 3 glass cards: stacked HVAC modes Froid/Chaud/Sec/Ventilation/Éteint, 320 px thermostat arc with optimistic target readout and a debounced `climate.set_temperature` + room temperature line, presets Éco/Boost + Silence + airflow Oscillation/Brise `windnice`), `light_popup.yaml` (near-fullscreen 1250×690 modal in 3 glass cards: Chambre/Salon/LEDs selector + On/Off + all-off, 320 px brightness arc with live % readout synced from the HA `brightness` attribute + 10/35/65/100 % shortcuts and a debounced `light.turn_on`, 3 named whites + 12 round color swatches; opened via `script.tab5_light_popup_show`), `console_sys.yaml` (4 glass cards: memory/network/system diagnostics, volume, and a management card — screen re-push, automation reload, HA restart and device reboot, the last two behind confirm overlays), `tv_remote_popup.yaml` (near-fullscreen 1230×670 Samsung remote: power/pad/volume/channels/playback row via `remote.send_command` on `${entity_tv_remote}`, opened by the TV button or a long-press on the PC card), `pots_popup.yaml` + `pot_detail_card.yaml` (near-fullscreen 1250×690 plant-details modal: 5 **fixed** glass cards — card N = sensor `moisture_N`, same icons as the dashboard — with soil-moisture %, watering status and Fertility EC / Light / Temperature / Battery rows, values pushed continuously by `update_pots_popup_moisture_ui()`/`update_pot_metric_ui()`; opened by a long-press on the dashboard moisture slots via the invisible `btn_pots_detail_zone`), `calendar_popup.yaml` + `cal_day_cell.yaml` (near-fullscreen 1250×690 monthly calendar: 7×6 Monday-first grid of 42 templated cells with work hours inside the cells, public-holiday/school-holiday/appointment/birthday markers, ◀ month ▶ + "Aujourd'hui" navigation, and a 780×540 day-detail sub-popup; the grid itself is computed **locally** from SNTP (`cal_render_month()`) and HA enriches each month on demand via `script.tab5_calendrier_mois`/`_jour` — opened by a long-press on the clock via the invisible `btn_clock_calendar_zone`), `forecast_daily.yaml`/`forecast_hourly.yaml`, `moisture_sensors.yaml`, `switches_card.yaml`.

### `tab5_custom.h` / `tab5_custom.cpp`
All non-trivial C++ logic: `update_meteo_icon()`, `get_temperature_color()`/`get_humidity_color()`, `parse_and_update_heures_bulk()`/`parse_and_update_jours_bulk()`, `sort_and_update_moisture_slots()`, `transition_widgets()`, `highlight_button_border()`. **Rule: sensors/services should only read HA state and call these C++ functions — never manipulate `lv_obj_*` directly from a `sensor:`/`text_sensor:` lambda** (keeps LVGL logic in one place, testable and greppable).

**Architecture `CentralPanelCtx`** (depuis refacto 26/07) : le struct `CentralPanelCtx` regroupe les 8 wrappers LVGL de la carte centrale + 7 flags d'activité + `current_panel`. Les pointeurs sont initialisés **une fois au boot** (`on_boot` dans `tab5-ha-hmi.yaml`) ; les bools sont synchronisés depuis les globals ESPHome (`id(has_rain)` etc.) avant chaque appel C++ (pattern *sync → call → write-back*). Globals C++ : `g_central_ctx`, `g_day_slots[5]`, `g_hour_slots[5]`.

---

## Services HA exposés (`api: services:`)

| Service | Payload | Rôle |
|---|---|---|
| `tab5_maj_clim` | target, current, mode, preset, fan, swing (strings) | État climatisation (couleurs, cible, mode, presets) |
| `tab5_maj_volet_etat` | etat_physique (string) | État volet (ouvert/fermé/en mouvement) |
| `tab5_maj_planning` | ligne1, ligne2 (strings) | Texte planning affiché dans la carte centrale |
| `tab5_maj_alerte_meteo_france` | payload (string, 11 champs `\|`-delimited) | Alertes météo France (vent, inondation, orages...) + recoloration de la date |
| `tab5_maj_meteo_actuelle` | condition, temperature, humidite | Icône pluie prédictive + hygrométrie (l'ancienne grosse icône météo centrale a été retirée de l'UI) |
| `tab5_maj_probabilites` | uv, gel, neige (strings) | Bascule l'icône pluie prédictive en flocon si probabilité de neige ≥ 5 |
| `tab5_maj_pluie_1h` | index_5mn, intensite (strings) | Une barre du graphe pluie 1h (9 barres) ; met à jour `has_rain` |
| `tab5_maj_info_texte` | texte, couleur (strings) | 4ᵉ panneau du rotateur : alerte météo (Rouge/Orange) ou résumé santé HA 1 ligne — MAJ en attente, erreurs, indispos (`update_info_text_ui()`) |
| `tab5_maj_previsions_heures_bulk` | payload (string) | 5 cartes prévisions horaires |
| `tab5_maj_previsions_jours_bulk` | payload (string) | 5 cartes prévisions journalières (fenêtre glissante selon `forecast_page_index`) |
| `tab5_maj_reponse_vocale` | texte (string) | Affiche temporairement la réponse vocale dans le bandeau central (`tab5_show_vocal_response`) |
| `tab5_maj_alertes_ha_bulk` | payload (string) | Jusqu'à 4 bandeaux d'alertes/infos HA, un panneau du rotateur chacun, tap-to-dismiss local (`parse_and_update_ha_alerts_bulk()`) |
| `tab5_maj_calendrier_mois` | annee, mois, codes, heures (strings) | Popup calendrier : bitmask 2 hex/jour (travail/férié/vacances scolaires/RDV/anniversaire) + 31 champs d'heures de travail — mis en cache, re-rendu si le mois est affiché (`cal_store_month_data()`/`cal_render_month()`) |
| `tab5_maj_calendrier_jour` | date, payload (strings) | Popup calendrier : lignes de détail du jour tapé "type\|texte;..." (`cal_render_day_detail()`), ignoré si le détail affiché a changé |

## Globals principaux (`tab5-globals.yaml`)

| Global | Type | Rôle |
|---|---|---|
| `boot_complete` | bool | true une fois le `on_boot` terminé |
| `conversation_mode` | bool | mode assistant vocal (persiste au reboot) |
| `forecast_page_index` | int (0-4) | page prévisions active — 0-1 horaire, 2-4 journalier |
| `clim_target_temp`, `clim_preset_mode`, `clim_fan_mode`, `clim_swing_mode` | float/string | état climatisation |
| `volet_target_open`, `volet_en_mouvement` | bool | état volet |
| `plan_ligne_1`, `plan_ligne_2` | string | texte planning brut |
| `has_alerts`, `has_rain`, `has_info`, `current_central_panel` | bool/int | rotateur carte centrale (8s) : planning / pluie / alertes météo / info + jusqu'à 4 bandeaux HA |
| `has_ha_alert_0…3`, `ha_alert_id_0…3` | bool/string | bandeaux alertes/infos HA (`tab5_maj_alertes_ha_bulk`) ; `tab5_dismissed_local` mémorise les ids masqués au tap |
| `current_light_entity` | string | entité lumière pilotée par le popup lumière (`tab5_light_popup_show`) |
| `va_stop_armed` | bool | modèle wake word « Stop » armé (volet en mouvement) |
| `system_volume`, `system_muted` | float/bool | volume haut-parleur |
| `cal_view_year`, `cal_view_month`, `cal_detail_date` | int/int/string | popup calendrier : mois affiché + date du détail ouvert (le cache mensuel vit en `static` dans `tab5_custom.cpp`) |

## Règles de code à respecter (issues de l'audit du 05/07/2026)

1. **Pas de couleur en dur** (`0xFFAABB`) dans un YAML/lambda — ajouter un token dans `UIColor::` (`tab5_custom.h`) et l'utiliser partout.
2. **Les `sensor:`/`text_sensor:` ne manipulent pas LVGL directement** — ils appellent une fonction C++ dans `tab5_custom.cpp` (ex: `update_light_ui()`, pas de `lv_obj_set_style_*` inline).
3. **Pas de `static` dans une lambda pour de l'état partagé entre deux handlers différents** (`on_short_click`/`on_long_press`) — utiliser un `globals:` (cf. bug `reboot_armed` corrigé le 05/07 ; global retiré le 16/07 quand la console est passée aux overlays de confirmation).
4. **Pas de `std::string` par valeur ni de `to_string()` dans un hot-path** (sliders, `on_value` fréquents) — `const std::string&` ou buffer `snprintf` statique.
5. **Toute nouvelle carte/widget répété ≥3 fois** (météo, switches...) doit passer par une fonction C++ builder paramétrée plutôt qu'un copier-coller YAML (cf. refacto architecture en cours).
6. Avant de committer : `python -m esphome compile tab5-ha-hmi.yaml` doit réussir (toolchain déjà en cache localement, ~20-45s).
7. **Tout popup modal réutilise le chrome partagé** (ADR-0009) : `modal_scrim.yaml` (var `scrim_opa`) + `modal_header.yaml` (icône, titre, croix — barre de 52 px, corps à `y: ${modal_body_y}`), carte dimensionnée par `${modal_card_w}`/`${modal_card_h}`. Jamais de voile, de titre ou de croix réécrits à la main ; les boutons d'options d'en-tête restent des frères en `y: 4, height: 44`. Vérification : `python scripts/check_tab5_modal_chrome.py` (dépôt racine du workspace).
   **Unique exception** : `ui_components/marble_game.yaml` (jeu « Fil d'Or »), `ui_components/arkanoid_game.yaml` (jeu « Arcanoïde ») et `ui_components/pinball_game.yaml` (jeu « Flip Noir »). Ce ne sont pas des popups domotiques mais des **flux plein écran** séparés — voir les sections dédiées ci-dessous. Ils ne déclenchent pas le garde-fou (ni `style_modal_card`, ni `color_modal_scrim`, ni glyphe de croix).

---

## Marble Roguelite — « Fil d'Or »

Petit roguelite de bille piloté à l'inclinaison (BMI270), **plein écran 1280×720**, entièrement local : il tourne sans Home Assistant et sans réseau.

### Lancer / quitter

| Action | Où |
|---|---|
| Ouvrir | Console **GESTION** → bouton « Fil d'Or » · **ou** long-press sur la bande centrale (bandeau planning ou bandeau pluie) |
| Quitter | Hub du jeu → « Quitter » (retour propre au dashboard : timer arrêté, run banquée, overlay masqué) |
| Pause | **Toucher le bandeau HUD** pendant une partie (il n'y a volontairement pas de croix : le jeu est un flux plein cadre) |
| Calibrer | Hub → **Réglages** → « Calibrer à plat », ou Pause → « Recalibrer à plat ». Poser la tablette **puis** appuyer. |

Le tactile ne sert qu'aux menus — la bille se pilote **uniquement** à l'inclinaison.

### Difficulté et mode dieu

Hub → **Réglages**. Les deux sont persistés et s'appliquent **au lancement de la run suivante** (les changer en cours de partie n'aurait pas de sens — l'écran n'est joignable que depuis le hub).

| Difficulté | PV | Pièges | Vitesse bille | Invulnérabilité | Fragments |
|---|---|---|---|---|---|
| **Calme** | +1 | ×0,75 | ×0,92 | 1800 ms | ×0,80 |
| **Normal** | — | ×1,00 | ×1,00 | 1200 ms | ×1,00 |
| **Impitoyable** | −1 | ×1,35 | ×1,10 | 800 ms | **×1,60** |

Monter d'un cran rapporte davantage — sinon personne ne le ferait. La difficulté ne change **que le timing**, jamais la géométrie : les passages restent les mêmes (garde-fou ci-dessous), un piège plus rapide reste franchissable.

**Mode dieu** : invulnérable (les trous se contentent de replacer la bille au départ). La run reste entièrement jouable, mais elle est **hors concours** — aucun fragment crédité, aucune statistique enregistrée, `runs` non incrémenté. Sinon l'invulnérabilité viderait la méta-progression de son sens. Le HUD l'affiche en clair (`[ DIEU ]` + « PV invulnérable »).

### Progression façon Dark Souls

Une seule monnaie : les **âmes**, gagnées en ramassant de l'or, en ouvrant des coffres et en battant les boss. Elles servent **à la fois** à monter de niveau et à commercer — c'est le même arbitrage que dans Dark Souls.

**Feu de camp** (hub → *Feu de camp*) — 6 caractéristiques :

| Caractéristique | Effet par niveau | Max |
|---|---|---|
| **Vitalité** | +1 point de vie | 5 |
| **Résistance** | +300 ms d'invulnérabilité ; bouclier par salle dès le niveau 3 | 5 |
| **Finesse** | −1 px de rayon (bille plus difficile à toucher) | 4 |
| **Agilité** | +12 % de réponse à l'inclinaison | 5 |
| **Élan** | +60 de vitesse maximale | 5 |
| **Découverte** | +15 % d'âmes et +12 % de chance de butin en coffre | 5 |

Le coût d'un point suit le **niveau total** — `60 + 14·L + L²` — donc monter n'importe quelle caractéristique renchérit toutes les autres. Il faut choisir une orientation, exactement comme un build DS.

> **Finesse ne peut que réduire le rayon**, jamais l'augmenter. C'est délibéré : le garde-fou de traversabilité prouve les parcours au rayon maximal (11 px), donc sa preuve reste valable quelle que soit la progression.

**Objets** — 10 pièces, découvertes dans les **coffres au trésor**, lâchées par les **boss** (Némésis en salle 5, le Trône en salle 6, butin garanti), ou achetées chez le **Marchand**. On en équipe **2 à la fois** (hub → *Équipement*, un appui fait défiler). Revente à la moitié du prix — un objet vendu est automatiquement déséquipé. La *Couronne fêlée* est volontairement à double tranchant (+50 % d'âmes, −1 PV).

Un coffre donne 25 à 60 âmes, plus un jet de butin modulé par Découverte. Si la collection est complète, le butin est converti en âmes plutôt que perdu.

### Ce qui est persisté (NVS, survit aux reboots et aux OTA)

`MarbleSave` (voir `marble_game.h`) via `esphome::global_preferences` — **aucune dépendance HA** :
âmes, runs, victoires, meilleur temps, salle la plus profonde, les 6 caractéristiques, le masque des objets possédés, les 2 emplacements d'équipement, la teinte de bille, la difficulté, le mode dieu et l'offset de calibration.
Le layout est validé par un `magic` (`SAVE_MAGIC`) : **le modifier oblige à bumper la constante**, sinon une vieille sauvegarde serait relue de travers (elle est alors rejetée et remise à zéro). Historique : `FOR1` initial → `FOR2` (difficulté/dieu) → `FOR3` (âmes, caractéristiques, objets).

### Design note v1

- **Boucle** : hub → 6 salles enchaînées → mort ou victoire → retour hub. Cible **2 à 5 min** par run.
- **Seed** : `lv_tick_get() ^ 0x9E3779B9 ^ (runs × 2654435761)`, xorshift32. Il pilote (a) le décalage ±28 px des pickups — **annulé si la nouvelle position tombe dans un mur**, (b) le déphasage des scies/orbes, (c) le tirage des 3 boons proposés. Les layouts eux-mêmes restent fixes : c'est le contenu qui varie, pas la lisibilité.
- **Salles** : 1 Seuil (★☆☆☆, tuto implicite) · 2 Couloirs (★★☆☆, serpentin + scie) · 3 Forge (★★☆☆, tapis d'accélération, or au contact des pointes) · 4 Sanctuaire (★★★☆, route haute sûre vs route basse à trous mieux dotée) · 5 Némésis (★★★★, 2 orbes en orbite + chasseuse + glu) · 6 Trône (★★★★, 3 runes puis portail central gardé).
- **Pièges (6)** : pointes fixes, scie oscillante, trou/vide, zone de glu, tapis d'accélération, orbe en orbite — plus la **chasseuse** qui poursuit la bille (salles 5-6).
- **Bonus (6)** : or, bouclier (1 coup), aimant, frein, dash, rune d'objectif.
- **Boons intra-run (10)**, 3 proposés au choix après les salles **2 et 4** : Main d'Ariane, Cœur de braise, Bourse tressée, Aimant mineur, Semelles lourdes, Élan, Peau de bronze, Œil du dédale, Seconde chance, Pas de velours.
- **Méta (5)** : Vigueur (+1 PV, ×3), Filon (+12 % fragments, ×3), Main sûre (pilotage plus doux, ×2), Relique (bouclier au départ, ×1), Teinte (cosmétique, ×2).
- **Balance v1** : 3 PV de base (+1 par Vigueur, ± la difficulté), or = 10/pickup, dégât = retour au départ de la salle + invulnérabilité. Salle 1 volontairement généreuse (1 seul piège, sortie visible) ; salle 6 exigeante mais lisible (orbes télégraphiés par leur orbite régulière autour du portail). **Une run perdue rapporte quand même ses fragments** — y compris si le jeu est quitté en cours de partie (sauf en mode dieu).

### Garde-fou : toutes les salles restent traversables

```bash
python scripts/check_marble_rooms.py
```

Lit les 6 salles **directement dans `marble_game.cpp`** (pas de duplication : le test suit le contenu) et vérifie, pour chacune, que le départ n'est pas dans un mur, que la sortie est atteignable, que **chaque bonus et chaque rune** l'est aussi, et que les scies laissent un passage à au moins une phase de leur course.

Méthode : BFS sur une grille d'occupation du **centre de la bille**, murs dilatés du rayon (11 px). La dilatation utilise le rectangle et non le vrai arrondi de Minkowski aux coins — elle bloque donc un peu **plus** que la réalité, ce qui rend le résultat sûr : un chemin trouvé par le test existe forcément en jeu. À lancer après toute modification d'une salle (testé négativement : un mur qui scelle la sortie de la salle 1 est bien détecté).

Le décalage de position des bonus par le seed est en plus contraint côté C++ par `segment_clear()` : la nouvelle position doit être **reliée en ligne droite** à l'ancienne, bille dilatée comprise. « Ne pas être dans un mur » ne suffisait pas — un bonus aurait pu sauter de l'autre côté d'une paroi fine et devenir inatteignable.

### Notes techniques / perf

- Physique à **30 Hz** (`lv_timer` 33 ms) créé à l'ouverture et **détruit à la fermeture** → zéro tick gameplay hors jeu.
- **3 sous-pas** de collision par frame (anti-tunnelling à 650 px/s), résolution cercle/AABB avec réflexion sur la normale.
- Objets LVGL **préalloués une seule fois** (pool de 48 entités + bille + 4 bandes de vignette) puis recyclés par `show/hide` + `set_pos` : aucune allocation dans la boucle. Les libellés du HUD ne sont réécrits que si leur valeur change.
- IMU : les 3 axes d'accélération sont `internal: true` (ils saturaient l'API HA pour rien) ; la **cadence de poll est adaptative** — 100 ms au repos, 33 ms quand le jeu est ouvert (`stop_poller()`/`start_poller()`, car `set_update_interval()` seul ne re-régle pas le poller déjà enregistré).
- Feedback de dégât : 4 bandes de bord fines + clignotement de la bille + micro-tremblement — **pas** de shake plein écran (il invaliderait 1280×672 à chaque frame).

---

## Arcanoïde — casse-briques rétro Atari

Casse-briques style Arkanoid / Breakout, **plein écran 1280×720**, esthétique rétro Atari 80's (fond sombre, briques colorées par rangée, contraste fort). Entièrement local : aucun réseau ni HA requis.

### Lancer / quitter

| Action | Où |
|---|---|
| Ouvrir | **Long-press sur la barre de pagination** (les 5 petits points sous le panneau central météo) |
| Quitter | Hub du jeu → « Quitter » (retour dashboard : timer arrêté, score sauvegardé, overlay masqué) |
| Pause | **Toucher le bandeau HUD** pendant une partie (pas de croix : flux plein cadre, exception ADR-0009) |
| Lancer la balle | **Tap sur l'écran** quand la balle est collée à la raquette |

### Contrôles (réglables dans le hub → Réglages)

| Mode | Principe |
|---|---|
| **Inclinaison** (BMI270) | Incliner la tablette gauche/droite déplace la raquette. Mapping rotation 270° : `X_écran = −tilt_Y`. Deadzone radiale + lissage + calibration « à plat » (offset NVS). |
| **Boutons tactiles** | Deux zones semi-transparentes (opa 50 %) dans les coins bas gauche/droite du playfield. Hold = déplacer, release = stop. |
| **Les deux** (défaut) | Somme clampée des deux entrées. |

Sensibilité IMU réglable (5 crans). Calibration dans Réglages ou Pause.

### Gameplay

- 8 niveaux (mur plein, pyramide, colonnes, damier, couloirs, forteresse, zigzag, boss final).
- 3 vies, score + multiplicateur combo (cassages rapides < 1,2 s).
- Types de briques : normales (1 coup, couleur par rangée), renforcées (2-3 coups), indestructibles, bonus (lâchent un power-up).
- Power-ups : élargir / rétrécir raquette, balle lente / rapide, multi-balles (max 3), colle, extra vie (rare).
- Collision balle/raquette : angle selon le point d'impact (pas de rebond vertical monotone).
- Difficulté progressive : vitesse balle + densité par niveau.

### Classement

Top 10 local en NVS (score, niveau atteint, mode de contrôle, uptime). Écran « Classement » dans le hub, avec bouton « Effacer les scores ».

### Notes techniques

- Physique 30 Hz (`lv_timer` 33 ms), 3 sous-pas anti-tunnelling.
- Pool LVGL préalloué : 120 briques + 3 balles + 1 raquette + 8 power-ups. Zéro allocation dans le tick.
- HUD réécrit seulement si valeur change.
- Persistance `ArkanoidSave` (magic `ARK1`) via `esphome::global_preferences`.
- Fichiers : `arkanoid_game.h`, `arkanoid_game.cpp`, `ui_components/arkanoid_game.yaml`.
- Couleurs : `UIColor::ARK_*` dans `tab5_custom.h`.

---

## Flip Noir — flipper rétro arcade 70-80's

Flipper (pinball) style « Getaway: High Speed », **plein écran 1280×720**, esthétique rétro borne arcade (table sombre, bumpers colorés, inserts vifs). Entièrement local : aucun réseau ni HA requis.

| Action | Où |
|---|---|
| Ouvrir | Console **GESTION** → bouton « Flip Noir » |
| Quitter | Hub du jeu → « Quitter » |
| Pause | Toucher le bandeau HUD pendant une partie |

### Contrôles

- **Zone gauche (hold)** = flipper gauche
- **Zone droite (hold)** = flipper droit
- **Coin bas-droit (hold)** = plunger (charger), release = tirer
- **IMU (mode Mixte)** = nudge latéral (inclinaison légère de la tablette)
- Abuse de nudge = **TILT** (flippers morts 2,5 s)

### Gameplay

- 3 billes par partie (+1 bille bonus à 50k et 150k).
- 4 bumpers, 2 slingshots, 3 cibles drop (bank), 2 rollovers.
- Multiball (2 billes) après 3 cibles drop.
- Modes score : « Bumper Frenzy » (×2 bumpers 10 s), « Target Mania » (×2 cibles 8 s).
- Top 10 high scores NVS + compteurs carrière (parties, tilts, multiballs).

### Notes techniques

- Physique ~45 Hz (`lv_timer` 22 ms), 3 sous-pas anti-tunnelling.
- Collision : bille (cercle) vs segments (murs, flippers) + cercles (bumpers, cibles).
- Flippers = segments pivotants avec vitesse angulaire (impulsion à la frappe).
- Pool LVGL préalloué. Zéro allocation dans le tick.
- Persistance `PinballSave` (magic `PIN1`) via `esphome::global_preferences`.
- Fichiers : `pinball_game.h`, `pinball_game.cpp`, `ui_components/pinball_game.yaml`.
- Couleurs : `UIColor::PIN_*` dans `tab5_custom.h`.

---

## Version Française

Ce dossier contient les packages de configuration ESPHome et les fichiers source C++ du firmware Tab5. Point d'entrée : `../tab5-ha-hmi.yaml`. Voir la section anglaise ci-dessus pour le détail par fichier, la table des services HA, la table des globals et les règles de code — écrit contre le code réel le 05/07/2026, re-vérifié ligne à ligne le 14/07/2026, puis le 17/07/2026, complété le 19/07/2026 (14 services dont `tab5_maj_calendrier_mois`/`_jour`, popups v2 + calendrier, télécommande TV, wake word « Stop », scripts par familles).

---

## Fichiers de polices

| Fichier | Contenu |
|---------|---------|
| `materialdesignicons-webfont.ttf` | Material Design Icons — plusieurs tailles chargées séparément (26/32/45/56/60/70/120 px — l'id `mdi_font_80` charge en réalité du 70) |
| `IconeMeteo.ttf` | Police d'icônes météo personnalisée |

## Sous-répertoires

### `my_components/st7123/`
Composant ESPHome personnalisé pour le contrôleur tactile I2C ST7123 (certains lots Tab5 V2).

### `tts_library/`, `tts_library_v2/`
Fichiers audio TTS expérimentaux, antérieurs à l'intégration Voice HA. Non utilisés dans la config actuelle.
