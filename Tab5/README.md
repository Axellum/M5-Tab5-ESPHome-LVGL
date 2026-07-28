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
   **Exceptions (overlays de jeu)** : les 8 `*_game.yaml` de la section Arcade ci-dessous. Flux plein écran — pas de garde-fou modal (ni `style_modal_card`, ni `color_modal_scrim`, ni glyphe de croix).

---

## Arcade — les 8 consoles

Toutes les consoles suivent la **même architecture** : overlay plein écran
1280×720 (exception ADR-0009), YAML réduit à des conteneurs vides, tout le
contenu construit en C++, `lv_timer` créé à l'ouverture et détruit à la
fermeture, persistance NVS, **zéro dépendance Home Assistant ou réseau**.

**Une exception à l'orientation** : « Neon Apron » (#3) bascule LVGL en
**portrait 720×1280** à l'ouverture et restaure `rotation: 270` à la fermeture —
un flipper couché sur le côté ne ressemble à rien. C'est la seule console qui
touche à l'orientation ; voir sa section plus bas avant d'en écrire une autre.

| # | Console | Module C++ | Script d'ouverture | Icône MDI |
|---|---|---|---|---|
| 1 | **Fil d'Or** — roguelite de bille | `marble_game` | `tab5_marble_open` | `F0E95` circle-double |
| 2 | **Arcanoïde** — casse-briques | `arkanoid_game` | `tab5_arkanoid_open` | `F0570` view-grid |
| 3 | **Neon Apron** — flipper **portrait** | `pinball_game` | `tab5_pinball_open` | `F05DD` bullseye |
| 4 | **Coureur d'Or** — Lode Runner | `lode_game` | `tab5_lode_open` | `F15A2` ladder |
| 5 | **Go Tab** — jeu de Go | `go_engine` + `go_ai` + `go_game` | `tab5_go_open` | `F0B38` circle-multiple |
| 6 | **Trial Poursuite** — quiz | `trivia_game` | `tab5_trivia_open` | `F134A` head-question |
| 7 | **Dames Tab** — dames 10×10 | `draughts_ai` + `draughts_game` | `tab5_draughts_open` | `F013A` checkerboard |
| 8 | **Roi Noir** — échecs FIDE | `chess_ai` + `chess_game` | `tab5_chess_open` | `F0857` chess-king |

### Page Arcade (`ui_components/game_selector.yaml`)

Grille **régulière 4 × 2**, toutes les cartes au même format (298 × 252).
Colonnes `x = 20 / 334 / 648 / 962`, rangées `y = 132 / 402`. Ouverte par la
zone tactile sur la température de la serre (`btn_serre_games`, dans
`climate_card.yaml`).

Chaque carte fait **trois** choses, dans cet ordre :

1. `script.execute: tab5_games_close_all` — ferme le jeu en cours ;
2. `animate_popup_close(...)` — referme le sélecteur ;
3. `script.execute: tab5_<jeu>_open` — ouvre la console.

### Règles à respecter pour ajouter une 9ᵉ console

Une console n'est **intégrée** que si les six points suivants sont faits. Un seul
oubli et le jeu est invisible, ou le firmware ne compile pas :

1. `tab5-ha-hmi.yaml` → `includes:` : **tous** les `.h` et `.cpp`, y compris les
   en-têtes de données inclus par le `.cpp` (c'est ce qui manquait pour
   `trivia_questions.h`, et la compilation échouait dessus) ;
2. `tab5-lvgl.yaml` → `!include ui_components/<jeu>_game.yaml`, **avant**
   `game_selector.yaml` (le sélecteur doit se dessiner au-dessus) ;
3. `tab5-scripts.yaml` → script `tab5_<jeu>_open` qui injecte les pointeurs LVGL ;
4. `tab5-scripts.yaml` → ajouter `<Namespace>::close()` dans
   **`tab5_games_close_all`** (liste unique, ne jamais la recopier ailleurs) ;
5. `game_selector.yaml` → une carte dans la grille ;
6. `tab5-imu.yaml` → `<Namespace>::on_imu(...)`, et `<Namespace>::is_open()` dans
   la liste de poll rapide **uniquement** si le jeu pilote à l'inclinaison.

Les icônes MDI utilisées doivent en outre figurer dans la liste `glyphs` de
`mdi_font_56` / `mdi_font_45` (`tab5-styles.yaml`) : une icône absente de la
sous-police ne s'affiche **pas du tout**, sans le moindre message d'erreur.

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

## Neon Apron — flipper **portrait**

Flipper (pinball) néon, **plein écran PORTRAIT 720×1280**. C'est la seule console
de l'Arcade qui ne se joue pas dans l'orientation du dashboard : on tient la
tablette debout, comme devant une borne. Entièrement local, aucun réseau ni HA.

> Remplace « Flip Noir » (supprimé en `697e2e9`). L'ancien jeu était une table
> **paysage** dessinée avec des rectangles LVGL pivotés à chaque frame
> (`transform_rotation`) : laid et cher. Rien n'en a été réutilisé, sauf les
> principes de collision. Ne pas ressusciter cette approche.

| Action | Où |
|---|---|
| Ouvrir | Sélecteur Arcade → carte « Neon Apron » (slot 3) |
| Quitter | Hub du jeu → « Quitter » (restaure le paysage) |
| Pause | Toucher le fronton pendant une partie |
| Calibrer | Hub → Réglages → « Calibrer à plat », ou Pause → « Recalibrer à plat » |

### Orientation — ce qu'il faut savoir

À l'ouverture, le jeu appelle `LvglComponent::set_rotation(0)` : LVGL passe en
**720×1280** et l'écran devient portrait. À la fermeture, `set_rotation(270)`
restaure exactement l'état de repos du dashboard.

Trois conséquences non évidentes :

1. **`rotation: 270` doit rester dans `tab5-styles.yaml`.** ESPHome ne compile le
   support de rotation (buffer dédié + client PPA) que si la clé existe. Sans
   elle, `set_rotation()` se contente d'un `ESP_LOGW` et le flipper resterait
   couché. Le bloc `lvgl:` porte aussi un `id: tab5_lvgl` explicite — c'est le
   seul moyen d'atteindre le composant depuis une lambda.
2. **Le tactile suit tout seul.** `LVTouchListener` applique
   `rotate_coordinates()`, qui lit la rotation *courante* à chaque lecture
   d'indev. La calibration native 720×1280 de `tab5-hardware.yaml` reste valable
   dans les deux sens — rien à recalibrer.
3. **Le portrait est plus rapide que le paysage.** À `rotation: 0`, le chemin de
   flush d'ESPHome tombe dans son `default:` et ne fait aucune rotation
   logicielle ni passage PPA. Le dashboard paysage, lui, paie une rotation 270 à
   chaque flush (c'est la piste des `lvgl took a long time` observés au boot).

Si l'écran apparaît à l'envers dans les mains d'Axel : **Réglages → Orientation →
retournée** (bascule 0 ↔ 180, persistée, effet immédiat). Le sens dépend du côté
vers lequel on tourne la tablette, il n'y a pas de « bon » réglage universel.

### Contrôles

- **Moitié gauche / moitié droite de l'écran (maintien)** = flipper gauche / droit
- **Bas au centre (maintien puis relâcher)** = lanceur ; la jauge de puissance
  s'affiche dans le fronton. Un tir faible ne sort pas du couloir, seul un tir
  fort fait le tour de l'arche et atteint le skill shot.
- **Secouer la tablette** = nudge. La gravité de la table est **constante** :
  l'IMU ne sert qu'au nudge, jamais à piloter la bille. On peut donc jouer
  tablette posée à plat, tenue droite ou inclinée.
- **3 nudges en moins de 2,6 s = TILT** : flippers morts 2,5 s.

### Gameplay

- 3 billes (+1 bille bonus à 250k et 750k).
- 3 bumpers, 2 slingshots, banque de 3 cibles drop, 3 rollovers (skill shot).
- Banque complétée : alternance **Bumper Frenzy** (bumpers ×3 pendant 9 s) et
  **Multiball** 2 billes (tout ×2 tant que 2 billes sont en jeu).
- Skill shot : une lane tirée au sort à chaque service, 25 000 points.
- Top 10 NVS + carrière (parties, cumul, meilleure bille, tilts, multiballs).
- Une partie quittée en cours **est comptée** : sinon on pourrait quitter pour
  effacer un mauvais score.

### Notes techniques

- Physique 50 Hz (`lv_timer` 20 ms), 4 sous-pas anti-tunnelling ; tick ralenti à
  200 ms dans les menus.
- Collision : segments (rails, guides), **contrainte circulaire** pour l'arche
  (un seul `sqrtf` au lieu de 18 tests de segment, et aucune facette), cercles
  (bumpers), capsules (cibles), segments mobiles pour les flippers (l'impulsion
  vient de la vitesse du bras au point de contact).
- Clapet anti-retour en haut du couloir du lanceur : segment qui n'existe que
  pour une bille qui descend.
- **Rendu : zéro `transform_rotation`.** Table construite une fois — `lv_arc`
  pour l'arche, `lv_line` à boîte englobante serrée pour rails et flippers,
  dégradés verticaux pour le volume. Seuls bille, flippers, flashs et ressort
  bougent. Lampes et inserts sont mis en cache (repeints uniquement au
  changement d'état).
- **`LV_USE_LINE`** n'est activé par ESPHome que si un widget `line:` figure dans
  la config : `pinball_game.yaml` en déclare un, masqué, uniquement pour ça. Ne
  pas le supprimer, la compilation échouerait sur `lv_line_create`.
- Persistance `PinballSave` (magic **`PIN2`**, pas `PIN1` — layout incompatible
  avec l'ancien Flip Noir) via `esphome::global_preferences`.
- Sons : stubs `sfx_*()` vides (même convention qu'Arcanoïde). Le haut-parleur
  est monopolisé par le pipeline vocal HA ; l'octet `sfx` de la sauvegarde est
  réservé pour le jour où un bip local sera possible, sans bump de magic.
- Fichiers : `pinball_game.h`, `pinball_game.cpp`, `ui_components/pinball_game.yaml`.
- Couleurs : `UIColor::PIN_*` dans `tab5_custom.h`, miroirs `color_pinball_*`.

---

## Go Tab — Go 9×9 / 13×13 / 19×19

Jeu de Go **plein écran 1280×720**, 100 % local (NVS). Nom produit : **Go Tab**.

| Action | Où |
|---|---|
| Ouvrir | Sélecteur Arcade → carte « Go Tab » |
| Quitter | Menu → « Quitter » (ADR-0009, pas de croix) |
| Menu / pause | Tap sur le bandeau HUD, ou bouton « Menu » du panneau |

### Écran

`go_game.yaml` ne déclare que **4 conteneurs vides** — HUD 1280×60, aire de jeu
1280×660 à `y=60`, calque de menus plein cadre. Tout le reste est construit en
C++ (`Go::open`). Le C++ suppose exactement cette géométrie : changer une valeur
dans le YAML impose de changer `HUD_H` / `FIELD_H` dans `go_game.cpp`.

- **Goban** : plateau en dégradé bois, liseré, lignes de bord épaissies, points
  étoiles, coordonnées `A..T` (sans le I) et `1..19`. Taille recalculée à chaque
  changement de plateau (écart 64 / 46 / 32 px en 9×9 / 13×13 / 19×19).
- **Pierres** : un seul objet LVGL chacune, relief obtenu par dégradé vertical
  (`bg_grad_dir`). Le même pool de 361 objets sert de pastilles de territoire
  pendant le comptage — aucun widget n'est créé en cours de partie.
- **HUD** : pastille Noir / statut central / pastille Blanc, la pastille au trait
  s'allume en or.
- **Panneau latéral** : liste des coups en police mono (2 colonnes), barre de
  réflexion, bouton de validation, et 4 boutons **Passer / Annuler / Indice / Menu**.

### Règles implémentées

- Placement sur **intersections** ; capture par libertés à 0.
- **Suicide interdit**, sauf si le coup capture d'abord.
- **Ko simple** (pas de superko positionnel — assumé, documenté).
- **Passe** ; deux passes consécutives → écran de **marquage des pierres mortes**.
- **Score chinois (aire)** : pierres vivantes + territoire + **komi 6,5** aux
  Blancs (komi **0,5** en partie à handicap ≥ 2).
- **Handicap 2 à 9 pierres** (placements standards, Blanc commence).
- **Vie/mort non résolue automatiquement** — c'est le joueur qui marque les
  groupes morts en fin de partie (toucher un groupe le bascule mort/vivant,
  « Tout vivant » remet à zéro), avec aperçu du territoire en direct. Une pause
  pendant le marquage **conserve** les groupes déjà marqués. C'est le
  fonctionnement de toutes les applications de Go : un solveur de vie/mort n'a
  pas sa place dans 2 Mo de firmware.
- L'IA **abandonne** si l'écart d'aire estimé est désespéré (≥ 45 pts, niveaux
  Amateur et plus) — évite de remplir le goban en salon.

### Modes & IA

| Mode | |
|---|---|
| Joueur contre Tab | Humain Noir ou Blanc, handicap possible |
| Joueur contre joueur | Hot-seat sur le même Tab |
| Tab contre Tab | Démo automatique |

Niveaux : **Débutant** (choix pondéré), **Amateur** (1 pli), **Confirmé** (2 plis),
**Expert** (3 plis en 9×9, 2 au-delà). La recherche est **bornée par le temps**,
jamais par un compteur de nœuds : `Ai::step(ms)` rend la main au bout de la
tranche demandée et un budget CPU total (80 / 350 / 900 / 1900 ms) garantit
qu'un coup sort toujours. L'évaluation tient en trois termes — matière, sécurité
des chaînes (atari), influence par diffusion — tous calculés en **O(N) par
position grâce à une table des chaînes construite une seule fois par nœud
(`chain_liberties` en compteurs de génération, pas de `memset` par chaîne).

### Contrôles

- Tap sur une intersection → **fantôme** ; second tap (ou bouton « Jouer XX »)
  → coup joué. La confirmation est désactivable dans les réglages, mais elle est
  active par défaut : en 19×19 l'écart entre intersections tombe à 32 px, soit
  moins qu'un doigt.
- **Passer** / **Annuler** (remonte aussi la réponse du Tab) / **Indice**
  (ou secousse BMI270) / **Menu**.
- Réglages : confirmation, coordonnées, marqueur du dernier coup, aperçu du
  territoire, secousse = indice.

### Fichiers

- `go_engine.h/.cpp` — règles pures (aucun LVGL/ESPHome, testable host).
  **Tous les scratchs sont des statiques de module** : le moteur tourne dans le
  contexte LVGL mono-thread et ne doit rien mettre de gros sur la pile.
- `go_ai.h/.cpp` — IA time-slicée
- `go_game.h/.cpp` + `ui_components/go_game.yaml` — UI / NVS
- Tests : `tools/test_go_engine.py` (**miroir Python exécutable sans toolchain**,
  c'est le test de référence) ; `tools/test_go_engine.cpp` si un g++ natif est
  disponible. Toute modification des règles doit être répercutée dans les deux.

### NVS

Une partie de Go fait des centaines de coups : l'écriture flash est **différée**
(drapeau interne + fenêtre de 15 s) et forcée seulement à l'ouverture d'un menu,
en fin de partie et à la fermeture. Réglages, statistiques par taille/niveau et
position en cours sont conservés ; la liste des coups et la pile d'annulation ne
le sont pas (reprendre une sauvegarde restitue la position, pas l'historique).

### Build

```powershell
$env:ESPHOME_ESP_IDF_PREFIX = "C:\espidf"
esphome clean tab5-ha-hmi.yaml
esphome run tab5-ha-hmi.yaml --device 192.168.0.88
python tools/test_go_engine.py
```

---

## Roi Noir — Échecs FIDE

Échiquier **plein écran 1280×720**, 100 % local (NVS), IA embarquée time-slicée.
Nom produit : **Roi Noir**. Aucune dépendance HA / réseau / tablebase.

| Action | Où |
|---|---|
| Ouvrir | Sélecteur Arcade → barre « Roi Noir » |
| Quitter | Hub → « Quitter » (ADR-0009, pas de croix) |
| Pause | Tap sur le bandeau HUD, ou bouton « Menu » du panneau |

### Règles implémentées

- Génération de coups **légaux** complète (pseudo-légaux filtrés : le roi ne peut
  pas rester en prise) — validée par `perft` (voir plus bas).
- **Roque** (4 cas), avec pertes de droits sur déplacement/capture de tour.
- **Prise en passant**, posée uniquement si un pion adverse peut réellement
  capturer (sinon deux positions identiques auraient des clés différentes et la
  triple répétition raterait).
- **Promotion** D / T / F / C, avec fenêtre de choix ; les 4 sous-promotions sont
  générées (conformité perft).
- **Échec, mat, pat**.
- **Nulles** : matériel insuffisant (R/R, R+mineure/R, R+F/R+F même couleur de
  case), règle des **50 coups** (activable), **triple répétition** (exacte, via
  clés Zobrist sur l'historique de la partie).

Limitations assumées :

- **R+C+C contre R n'est pas déclaré nul** (le mat y est possible avec une aide
  adverse) — conforme FIDE, mais surprenant pour un joueur occasionnel.
- La **reprise après reboot** restaure la position et les pendules (FEN en NVS),
  **pas l'historique des coups** : « Annuler » et le compteur de répétition
  repartent de zéro après une reprise. Reprendre depuis le hub *dans la même
  session* conserve, lui, tout l'historique.
- Pas de table de transposition (choix : RAM et simplicité) ; pas de superko.

### Encodage du plateau

**Mailbox 0x88** : `board[128]`, index = `rang*16 + colonne`, hors-échiquier ssi
`(sq & 0x88)`. Choix documenté face aux bitboards : l'ESP32-P4 est un cœur 32
bits, un test hors-plateau en un seul `AND` y bat un jeu de masques 64 bits, et
l'empreinte mémoire reste triviale. Pièce sur 4 bits (bit 3 = couleur).

Les tampons de coups sont **globaux et indexés par ply** (`g_mbuf[16][220]`,
~20,6 Ko de `.bss`) : à ~11 plies de profondeur, un tableau de 220 coups par ply
sur la pile ferait déborder la stack de la tâche ESPHome.

**Empreinte mesurée** (`riscv32-esp-elf-size`, `-Os`, cible ESP32-P4) :

| Unité | `.text` | `.bss` |
|---|---|---|
| `chess_ai.o` | 12,6 Ko | 33,0 Ko |
| `chess_game.o` | 23,4 Ko | 13,5 Ko |
| Police `chess_pieces_80` | ~26 Ko | — |
| **Total** | **~62 Ko** | **46,5 Ko** |

Le `.bss` est **statique** : il est réservé même jeu fermé. Répartition : tampons
de coups 20,6 Ko, table Zobrist 7,7 Ko, état de recherche 2,8 Ko, historique de
partie 10,2 Ko (320 demi-coups × 32 o). C'est le prix d'un hot-path sans
allocation ; ne pas augmenter `MAX_PLY_BUF` ni `MAX_HIST` sans re-mesurer.

### Modes & niveaux d'IA

| Mode | |
|---|---|
| Joueur contre Tab | Humain Blancs ou Noirs |
| Joueur contre joueur | Hot-seat, « Trait aux Blancs/Noirs » |
| Tab contre Tab | Démo auto, vitesse réglable |

| Niveau | Profondeur | Quiescence | Budget CPU | Fenêtre aléatoire | Elo fictif |
|---|---|---|---|---|---|
| Pion | 1 | — | 120 ms | 160 cp | 600 |
| Cavalier | 2 | — | 350 ms | 60 cp | 900 |
| Fou | 3 | 4 plies | 800 ms | 20 cp | 1250 |
| Dame | 4 | 6 plies | 1800 ms | 0 | 1600 |
| Roi | 5 | 6 plies | 3500 ms | 0 | 1900 |

La « fenêtre aléatoire » fait tirer au sort parmi les coups dont le score est à
moins de N centièmes de pion du meilleur : c'est ce qui rend le niveau Pion
battable sans le rendre absurde.

**Le budget est du temps CPU, pas du temps mural.** La recherche est découpée en
tranches de ~18 ms (deadline dure 22 ms, élargie à 40 puis 80 ms si un coup
racine seul dépasse), appelées une par tick de 33 ms : le temps réel d'un coup
vaut donc ~1,8× le budget. Découpage **au niveau des coups racine** ; dès que la
profondeur 1 est terminée, un coup jouable est toujours disponible.

### Contrôles

- **Tap case d'origine** → surbrillance des coups légaux (pastille = case vide,
  anneau = capture) → **tap destination**. Re-tap sur la pièce = désélection.
- Panneau : **Annuler** (vs Tab : annule votre coup *et* la réponse), **Indice**
  (recherche profondeur 2 bornée à 30 ms, recharge 8 s), **Menu**.
- Menu de pause : Reprendre / Annuler / Proposer nulle / Abandonner / Réglages /
  Quitter.
- **IMU** : secousse franche = indice (anti-rebond 900 ms), désactivable dans
  Réglages. Le jeu n'est **pas** ajouté à la liste de poll rapide 30 Hz du
  BMI270 (`tab5-imu.yaml`) : 10 Hz suffisent pour une secousse et une partie
  d'échecs peut durer une demi-heure.

### Figurines : police dédiée `ChessPieces.ttf`

Les polices `roboto_*` de `tab5-styles.yaml` n'embarquent que du Latin-1 : ♔♕♖ y
rendraient un carré vide. On ajoute donc une **police dédiée**, taillée selon la
même méthode que `IconeMeteo.ttf` — on n'embarque que ce qu'on affiche :

| | |
|---|---|
| Fichier | `Tab5/ChessPieces.ttf` — **16,8 Ko** (source DejaVu Sans : 757 Ko) |
| Glyphes | 12 : `U+2654–2659` (creux) + `U+265A–265F` (pleins) |
| Déclaration | `chess_pieces_80`, `size: 80`, `bpp: 4` |
| Coût flash | ~26 Ko rastérisés (12 × 72 × 60 px à 4 bpp) |
| Régénération | `python tools/make_chess_font.py` |
| Licence | Bitstream Vera — voir `Tab5/ChessPieces.LICENSE.txt` |

**Rendu en deux calques** (technique lichess / chess.com) :

- calque *corps* = glyphe **plein** `U+265A–265F`, coloré ivoire ou anthracite ;
- calque *contour* = glyphe **creux** `U+2654–2659`, anthracite, **affiché
  seulement pour les pièces blanches**.

Sans ce contour, une pièce ivoire disparaîtrait sur une case crème. Les 12
glyphes ont des boîtes identiques dans cette police (vérifié), la superposition
est donc pixel-parfaite. `piece_utf8()` encode le codepoint à la volée : l'enum
va `PAWN=1 … KING=6` alors qu'Unicode range roi → pion, d'où l'index `6 - type`.

`bpp: 4` est obligatoire : en `bpp: 1` les traits fins (couronne, crinière du
cavalier) crénellent. `size: 80` est calibré — l'encre fait 72 × 60 px dans une
case de 84, et la boîte du label (94 px de hauteur de ligne) déborde uniquement
sur des pixels transparents. `PIECE_DY = +1` recentre optiquement l'encre ; la
valeur est recalculée et affichée par `tools/make_chess_font.py`.

La notation du panneau reste **textuelle** (SAN français : **R** Roi, **D** Dame,
**T** Tour, **F** Fou, **C** Cavalier, avec désambiguïsation, `x`, `=D`,
`+` / `#`, `O-O` / `O-O-O`) — une liste de coups se lit mieux en lettres.

L'écran de promotion affiche les 4 choix comme de vraies **cases d'échiquier**
(crème / vert alternées) portant la figurine rendue exactement comme sur le
plateau, avec le nom sous chaque tuile.

### Validation du générateur : perft

`Chess::perft_log(depth)` lance `perft(1..depth)` sur la position initiale et
compare aux valeurs FIDE connues (20 / 400 / 8902 / 197281 / 4865609), avec le
temps en ms, via `ESP_LOGI`. À appeler ponctuellement en phase de debug, par
exemple depuis un `on_boot` de priorité basse :

```yaml
    - priority: 100
      then:
        - lambda: 'Chess::perft_log(3);'
```

À retirer ensuite : `perft(4)` bloque la boucle plusieurs centaines de ms.

### Fichiers

- `chess_ai.h/.cpp` — moteur pur : 0x88, make/unmake, eval, négamax αβ +
  quiescence + killers, FEN, SAN, perft. **Aucun LVGL, aucun HA.**
- `chess_game.h/.cpp` + `ui_components/chess_game.yaml` — UI LVGL, machine à
  états, NVS. Le YAML ne déclare que 4 conteneurs vides ; le calque des menus est
  créé en C++ comme enfant de `chess_root`.
- `ChessPieces.ttf` + `ChessPieces.LICENSE.txt` — figurines (12 glyphes).
- `tools/make_chess_font.py` — régénère le sous-ensemble de police.
- `tools/test_chess_perft.py` — miroir Python du générateur, suite perft.
- Palette locale `Chess::Pal` : `tab5_custom.h` n'est pas modifié. Seul ajout à
  `tab5-styles.yaml` : la police `chess_pieces_80` (aucun token de couleur).

### Checklist de test manuel

| Cas | Attendu |
|---|---|
| Mat du berger (Dh5×f7#) | « Échec et mat », partie comptabilisée |
| Pat (roi seul sans coup légal) | « Pat », nulle |
| Petit / grand roque, 2 couleurs | Roi + tour bougent, droits perdus ensuite |
| Roque interdit en échec / à travers une case attaquée | Case g1/c1 non proposée |
| Prise en passant | Le pion capturé disparaît de sa case, pas de celle d'arrivée |
| Promotion (poussée **et** capture) | Fenêtre D/T/F/C, la pièce choisie apparaît |
| Triple répétition (Cf3 Cf6 Cg1 Cg8 ×2) | Nulle annoncée |
| 50 coups | Nulle si l'option est active |
| Chute de pendule | Perte au temps, ou nulle si matériel insuffisant |
| Annuler pendant que le Tab réfléchit | Réflexion abandonnée, trait rendu au joueur |
| Reboot en pleine partie | « Reprendre » présent au hub, position et pendules restaurées |

### Build

```powershell
$env:ESPHOME_ESP_IDF_PREFIX = "C:\espidf"
esphome clean tab5-ha-hmi.yaml
esphome run tab5-ha-hmi.yaml --device 192.168.0.88
```

`esphome clean` est **obligatoire** : `chess_ai.cpp` et `chess_game.cpp` sont de
nouveaux `.cpp` ajoutés à `includes:`.

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
