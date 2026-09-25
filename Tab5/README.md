# Tab5 ESPHome Files

## English · [Français](#version-française)

---

> ⚠️ This README was rewritten on 2026-07-05 to match the current codebase after the previous version was found describing an unrelated, outdated iteration of the project (different service names, a 6-page layout that no longer exists). If you find another mismatch, it's the code that's right — please fix this file, not the other way around.

This folder contains the ESPHome configuration packages and the C++ source files for the Tab5 firmware.

The entry point is `../tab5-ha-hmi.yaml` at the repository root. It loads `substitutions` from `Tab5/user_entities.yaml` (gitignored — copy `user_entities.example.yaml` and edit your HA entity IDs), declares the `on_boot` sequence, and the `packages:` import list for everything in this folder.

**Screen layout:** the whole dashboard lives on a single 1280×720 page (`page_main` in `tab5-lvgl.yaml`) — every home-automation feature is a popup or a layer on it, never a separate screen. Navigation is by touch (clim/light/TV-remote/assistant/calendar/plants popups, diagnostics console via the `btn_control_console` button, top right) and left/right swipes on the lower band of the screen (`y ≥ 333`) to cycle the 5 forecast pages (2 hourly + 3 daily windows). Since the 14/07/2026 swipe rework, the console is **not** opened by swipe anymore.

The only other LVGL pages are the **9 gaming ones** (`page_arcade` + one per console), all declared `skip: true` so swipe navigation can never reach them — see the Arcade section below.

---

## `[AI-CONTEXT]` / `[AI-WARNING]` / `[AI-DEBUG]` convention (read this before editing)

Most files in this folder (and every file in `ui_components/`) start with a comment block tagged `[AI-CONTEXT]` — a short "system prompt" local to that file: its role, its architectural constraints, and explicit `@ai_instruction`s for common edits. Non-obvious decisions (a bug fix that looks removable, a duplication kept on purpose, a `!include` that must not be inlined) are documented **inside the file itself**, not only in the external knowledge base (`contexte_ia/` in the parent workspace) — a session that only has access to this repo (no cross-repo context) must still be able to find them.

A `[AI-WARNING]` (sometimes `[AI-WARNING-CRITICAL]`) marks something that looks like a bug/anti-pattern but is a deliberate, validated fix — e.g. the boot `delay(1000)` in `tab5-ha-hmi.yaml` (documented at length in the `logger:` block of `tab5-hardware.yaml`), or the pagination wrap logic in `handle_swipe_gesture()` (`tab5_central.cpp`). **Read the warning before "fixing" it** — at least one of these was already reverted once after being "corrected" by an LLM audit that hadn't read it. See [`../docs/decisions/`](../docs/decisions/README.md) for the full reasoning behind each one.

A `[AI-DEBUG]` marks a good observation point when diagnosing a runtime issue — a log line worth watching, a diagnostics entity/overlay, or a technique already proven to work on this device (e.g. inserting a temporary marker directly into the real HA automation rather than reproducing its logic in an isolated test script, which can pass while masking the actual bug). See [`../docs/debugging.md`](../docs/debugging.md).

If you add a genuinely new architectural constraint or a non-obvious decision while editing a file, add or extend its `[AI-CONTEXT]` block rather than leaving the reasoning only in a commit message or an external doc.

---

## File descriptions

### `tab5-hardware.yaml`
Low-level hardware: display/touch buses, ES8388 DAC I2C init, speaker/mic I2S, PI4IOE5V6408 GPIO expander (Wi-Fi power/antenna switches), `ota:` (encrypted with the API key since 2026-09-16 — `api_encryption_key` in `secrets.yaml`; plain uploads are refused, there is no OTA password anymore, see ADR-0015). The audio hardware stays here (shared I2S bus, ES7210 mic ADC, ES8388 DAC, `media_player`); the voice pipeline itself moved to `tab5-assist.yaml` on 2026-09-25 (audit lot 8c).

### `tab5-sensors-diagnostics.yaml`
System/network entities: the `wifi:` block, GPIO power switches (Wi-Fi, USB, external 5V, antenna select), HA API status, IP/SSID, uptime, Wi-Fi RSSI, core temperature, free RAM/loop time (`debug`), SNTP clock and the status-bar/console refresh `interval:`s.

### `tab5-sensors-domotique.yaml`
Home-automation entities pushed by HA over the ESPHome API: plant moisture (5×, dynamically sorted), light/PC state mirrors, phone battery, room & greenhouse temperature/humidity, audio (speaker amp, headphone jack, wake-word switch). The 20 plant-detail sensors (EC / light / temperature / battery × 5 pots) come from **`pot_sensors.yaml`**, one parameterized package included five times through a nested `packages:` (`!include` + `vars: {n}`) — a sixth pot is one line here plus its four `entity_plante_6_*` keys.

### `tab5-api-logic.yaml`
The `api: services:` block — the actual contract with Home Assistant. Each `tab5_maj_*` service receives a payload from an HA automation and calls into the C++ layer (`tab5_services.cpp`, declared in `tab5_custom.h`) via lambdas to update the LVGL widgets. See the service table below.

### `tab5-globals.yaml`
All `globals:` (shared state read/written across files) + the 8s central-panel rotator (planning/rain/alerts/info — 4 panels, paused while off the default forecast window). See the globals table below.

### `tab5-scripts.yaml`
Cross-cutting ESPHome `script:` blocks: the **modal registry** (`tab5_modal_registry_init`, ADR-0013), **debounces** (`tab5_debounce_volume_set` 150 ms, `tab5_debounce_light_brightness` 200 ms, `tab5_debounce_clim_temp` 250 ms — one HA call per gesture instead of one per tick), **volume** (`tab5_volume_apply`, single entry point), **climate** (`tab5_clim_recolor`), **central rotator** (`tab5_central_rotator_auto`, `tab5_central_panel_next`, `tab5_dismiss_info_tap`, `tab5_dismiss_ha_alert` [paramétré slot 0-3]), **shutter** (`tab5_volet_end_movement`, `tab5_volet_stop_voice_feedback`), **light popup** (`tab5_light_popup_show`), plus the 1 s `interval:` that returns to the dashboard after inactivity. Since 2026-09-25 (audit lot 8c) the families that are a feature of their own live in their own package — games, calendar, voice/assistant below, alarm in `tab5-alarm.yaml`. The temporary planning display moved to C++ (`show_temporary_planning()`, `tab5_central.cpp`). Prefer adding a script over duplicating a `delay` + action pattern inline.

### `tab5-arcade.yaml`
Game scripts: `tab5_games_close_all` (closes whatever console is open, list read from `GameRegistry::kGames`), one `tab5_<game>_open` per console (injects the LVGL pointers and fonts only reachable through `id()`), and `tab5_arcade_open` (the `page_arcade` selector). See [`docs/arcade.md`](../docs/arcade.md).

### `tab5-calendar.yaml`
Calendar popup scripts: `tab5_calendar_open`, `tab5_cal_render`, `tab5_cal_prev`/`tab5_cal_next`/`tab5_cal_today`, `tab5_cal_prefetch_boot`, `tab5_cal_day_tap`. The month grid is computed locally (`cal_render_month()`); HA only enriches it on demand.

### `tab5-assist.yaml`
The whole voice assistant in one place: `micro_wake_word` with **two models** — `okay_nabu` (always on when the wake-word switch is enabled) and `Stop` (armed only while the shutter moves, stops it locally) —, the `voice_assistant:` callbacks (they share `assist_set_pipeline_state()`), the assistant reply image (`http_request` + `online_image`), the **voice** scripts (`tab5_vocal_arm_stop`/`tab5_vocal_disarm_stop`, `tab5_vocal_interrupt`/`tab5_vocal_interrupt_and_listen`, `tab5_wake_word_dispatch` — runs the action chosen by `WakeWord::decide()` in `tab5_assist.cpp` —, `tab5_assist_toggle`, `tab5_show_vocal_response`) and the **assistant popup** scripts (`tab5_assist_open`/`close`/`on_request`/`sync_settings`, `tab5_set_assist_mode`, `tab5_assist_set_text_size`). The pipeline part (before `script:`) must stay free of `lv_*` — checked by `tools/check_tab5_code_rules.py`.

### `tab5-styles.yaml`
All LVGL `style_definitions` (glassmorphism "Slate" theme) + font declarations (Roboto sizes, MDI icon sizes, weather icon font). Color tokens live in `UIColor::` (`tab5_tokens.h`, dependency-free, included by `tab5_custom.h`) — **never hardcode a hex color in a YAML lambda**, add a token instead.

### `tab5-lvgl.yaml`
The dashboard layout on `page_main`: clock/date, status icons, quick-action buttons, climate card, moisture card, central rotating card, 5 forecast cards (daily/hourly), swipe gesture handling — plus the 9 gaming `pages:` entries (`page_arcade` + one per console, all `skip: true`).

### `ui_components/*.yaml`
Included by `tab5-lvgl.yaml`: `climate_card.yaml`/`climate_popup.yaml` (near-fullscreen 1250×690 modal in 3 glass cards: stacked HVAC modes Froid/Chaud/Sec/Ventilation/Éteint, 320 px thermostat arc with optimistic target readout and a debounced `climate.set_temperature` + room temperature line, presets Éco/Boost + Silence + airflow Oscillation/Brise `windnice`), `light_popup.yaml` (near-fullscreen 1250×690 modal in 3 glass cards: Chambre/Salon/LEDs selector + On/Off + all-off, 320 px brightness arc with live % readout synced from the HA `brightness` attribute + 10/35/65/100 % shortcuts and a debounced `light.turn_on`, 3 named whites + 12 round color swatches; opened via `script.tab5_light_popup_show`), `console_sys.yaml` (4 glass cards: memory/network/system diagnostics, volume, and a management card — screen re-push, automation reload, HA restart and device reboot, the last two behind confirm overlays), `tv_remote_popup.yaml` (near-fullscreen 1250×690 Samsung remote: power/pad/volume/channels/playback row via `remote.send_command` on `${entity_tv_remote}`, opened by the TV button or a long-press on the PC card), `pots_popup.yaml` + `pot_detail_card.yaml` (near-fullscreen 1250×690 plant-details modal: 5 **fixed** glass cards — card N = sensor `moisture_N`, same icons as the dashboard — with soil-moisture %, watering status and Fertility EC / Light / Temperature / Battery rows, values pushed continuously by `update_pots_popup_moisture_ui()`/`update_pot_metric_ui()`; opened by a long-press on the dashboard moisture slots via the invisible `btn_pots_detail_zone`), `calendar_popup.yaml` + `cal_day_cell.yaml` (near-fullscreen 1250×690 monthly calendar: 7×6 Monday-first grid of 42 templated cells with work hours inside the cells, public-holiday/school-holiday/appointment/birthday markers, ◀ month ▶ + "Aujourd'hui" navigation, and a 780×540 day-detail sub-popup; the grid itself is computed **locally** from SNTP (`cal_render_month()`) and HA enriches each month on demand via `script.tab5_calendrier_mois`/`_jour` — opened by a long-press on the clock via the invisible `btn_clock_calendar_zone`), `assistant_popup.yaml` (near-fullscreen 1250×690 voice-assistant modal: left column = settings — Domotique/Discussion brain selector, Ok Nabu toggle, mute, volume, A-/A/A+ text size; right column = the STT transcription and the Markdown-rendered LLM reply, with an on-demand `online_image` slot fed by `tab5_assist_reponse`), `forecast_daily.yaml`/`forecast_hourly.yaml`, `moisture_sensors.yaml`, `switches_card.yaml`, `game_selector.yaml` + the 8 `*_game.yaml` (see the Arcade section — each is the sole widget of its own LVGL page).

Two of them are **shared chrome**, not standalone components: `modal_scrim.yaml` (the dimming veil, `scrim_opa` var) and `modal_header.yaml` (icon + title + close cross, 52 px bar). Every popup includes them via `!include { file: …, vars: {…} }` — that is ADR-0009, and the games are the documented exception.

The remaining files are **parametrized sub-templates** included with `vars` from the components above rather than from `tab5-lvgl.yaml`: `climate_hvac_mode_btn.yaml` (×4), `climate_preset_toggle_btn.yaml` (×2), `forecast_day_title_tab.yaml`/`forecast_day_temp_tab.yaml` (×5), `forecast_hour_card.yaml` (×5), `switch_card_title_tab.yaml`/`switch_card_state_tab.yaml` (×3), `light_color_preset_btn.yaml` (×12), `pot_detail_card.yaml` (×5), `cal_day_cell.yaml` (×42). 23 files are included directly by `tab5-lvgl.yaml`, 35 exist in total.

### `tab5_custom.h` + the `tab5_*.cpp` units (formerly a single `tab5_custom.cpp`)
All non-trivial C++ logic, declared in **`tab5_custom.h`** (the single public header — YAML lambdas only ever call functions declared there) and, since 2026-09-08 (audit lot (e)), implemented in **nine units** split by responsibility — same functions, same order as the former 3 169-line `tab5_custom.cpp`, which now only holds the shared globals (`g_central_ctx`, `g_day_slots`, `g_hour_slots`, `cal_*`) and a map of the units:

Line counts per unit live in [`CARTOGRAPHIE_TAB5.md`](../CARTOGRAPHIE_TAB5.md) only (refreshed by `python tools/cartographie_counts.py --write`), so they stop drifting here.

- `tab5_text.cpp` — UTF-8 / mojibake, store local des alertes rejetées, libellés français des jours/mois, `set_label_text_utf8()`
- `tab5_forecast.cpp` — `update_meteo_icon()`, couleurs température/humidité, `parse_and_update_heures_bulk()`/`_jours_bulk()`, `refresh_daily_forecast()`/`refresh_hourly_forecast()`
- `tab5_central.cpp` — carte centrale (rotateur, `parse_and_update_ha_alerts_bulk()`, `update_info_text_ui()`, titre de page), `handle_swipe_gesture()`/`reset_forecast_to_main_page()`, `show_temporary_planning()`, `show_vocal_response_ui()`
- `tab5_services.cpp` — logique des services HA : `update_volet_ui()`, `parse_and_update_vigilance()`, `update_rain_bar_ui()`, `update_rain_predict_icon_ui()`, `update_clim_from_ha_ui()`, `update_planning_text_ui()`
- `tab5_assist.cpp` — `format_assist_markdown()`, `assist_set_pipeline_state()`, `assist_image_state_ui()`, `WakeWord::decide()`
- `tab5_cards.cpp` — `update_light_card_ui()`, popup lumière, `update_clim_target_ui()`, `sort_and_update_moisture_slots()`, `update_pots_popup_moisture_ui()`/`update_pot_metric_ui()`, `update_temp_ui()`
- `tab5_console.cpp` — ligne status, `ui_sync_volume_widgets()`, `update_console_diagnostics_ui()`
- `tab5_anim.cpp` — `transition_widgets()`, `animate_popup_open()`/`_close()`, `animate_swipe_horizontal()`, `animate_alert_enter()`, `ui_idle_ms()`/`close_popup_if_open()`, `animate_icon_roll_in()`, `layout_clock_roller()`/`update_clock_date_ui()`, `setup_button_press_animation()`/`highlight_button_border()`
- `tab5_calendar.cpp` — `cal_store_month_data()`, `cal_render_month()`, `cal_render_day_detail()` (cache mensuel `static`)

`tab5_internal.h` declares the six helpers shared *between* units (`normalize_text_utf8()`, `set_label_text_utf8()`, day-label formatters…); it is not part of the YAML contract. **Rule: sensors/services should only read HA state and call these C++ functions — never manipulate `lv_obj_*` directly from a `sensor:`/`text_sensor:`/`api: services:` lambda.** Adding a feature = one `update_*_ui()` in the unit that owns the responsibility + its declaration in `tab5_custom.h`.

**Architecture `CentralPanelCtx`** (depuis refacto 26/07) : le struct `CentralPanelCtx` regroupe les 8 wrappers LVGL de la carte centrale (+ `page_title_sub`, la ligne chapeau du titre de page prévisions) + 7 flags d'activité + `current_panel`. Les pointeurs sont initialisés **une fois au boot** (`on_boot` dans `tab5-ha-hmi.yaml`) ; les bools sont synchronisés depuis les globals ESPHome (`id(has_rain)` etc.) avant chaque appel C++ (pattern *sync → call → write-back*). Globals C++ : `g_central_ctx`, `g_day_slots[5]`, `g_hour_slots[5]`.

---

### `tab5_registry.h` / `tab5_registry.cpp`

Registre unique des **consoles** (`GameRegistry::kGames` : libellé, `is_open`,
`close`, `on_imu`, `imu_fast`) et des **fenêtres modales** (`ModalRegistry`,
rempli une fois par le script `tab5_modal_registry_init` de `tab5-scripts.yaml`,
seul endroit où `id()` est disponible ; genres `POPUP` / `SUBWINDOW` / `LAYER`,
ce dernier pour la sonnerie du réveil : nommée, jamais refermée). Tout ce qui a
besoin de « quel jeu est ouvert ? » ou « quel popup couvre l'écran ? » —
fermeture globale, poll IMU, text_sensor « Écran courant », select « Aller à
l'écran », retour automatique à l'inactivité — passe par lui (ADR-0013).
Garde-fou : `tools/check_tab5_registry.py`.

## Services HA exposés (`api: services:`)

| Service | Payload | Rôle |
|---|---|---|
| `tab5_maj_clim` | target, current, mode, preset, fan, swing (strings) | État climatisation : cible + température intérieure (`update_clim_from_ha_ui()`), modes dans les globals recolorés par `tab5_clim_recolor` |
| `tab5_maj_volet_etat` | etat_physique (string) | État volet (ouvert/fermé/en mouvement) — `update_volet_ui()` ; arme/désarme le wake word « Stop » |
| `tab5_maj_planning` | ligne1, ligne2 (strings) | Texte planning affiché dans la carte centrale |
| `tab5_maj_alerte_meteo_france` | payload (string, 11 champs `\|`-delimited) | Alertes météo France (vent, inondation, orages...) + recoloration de la date — `parse_and_update_vigilance()` |
| `tab5_maj_meteo_actuelle` | condition, temperature, humidite | Hygrométrie → couleur de la goutte « pluie prédictive » (`update_rain_predict_icon_ui()`). `condition` et `temperature` sont **réservés** : reçus, non exploités depuis le retrait de la grosse icône météo centrale ; le contrat n'est pas rétréci (3 appelants HA) |
| `tab5_maj_probabilites` | uv, gel, neige (strings) | Flocon si probabilité de neige ≥ 5, sinon goutte (`update_rain_predict_icon_ui()`, partagée avec la météo actuelle). `uv` et `gel` sont **réservés** : reçus, non exploités |
| `tab5_maj_pluie_1h_bulk` | payload (string, `idx\|intensité;…` × 9) | Les 9 barres du graphe pluie 1h en un appel (ADR-0003) ; met à jour `has_rain` (`update_rain_bars_bulk_ui()`) — ce que HA appelle depuis le 08/09/2026 |
| `tab5_maj_info_texte` | texte, couleur, meteo_id (strings) | 4ᵉ panneau du rotateur : alerte météo (Rouge/Orange) ou résumé santé HA 1 ligne — MAJ en attente, erreurs, indispos (`update_info_text_ui()`). `meteo_id` = identifiant de dismiss au tap ; vide = bandeau non masquable. **Les 3 variables sont obligatoires côté appelant** |
| `tab5_maj_previsions_heures_bulk` | payload (string) | 5 cartes prévisions horaires |
| `tab5_maj_previsions_jours_bulk` | payload (string) | 5 cartes prévisions journalières (fenêtre glissante selon `forecast_page_index`) |
| `tab5_maj_reponse_vocale` | texte (string) | Affiche temporairement la réponse vocale dans le bandeau central (`tab5_show_vocal_response`) |
| `tab5_assist_reponse` | texte, image_url (strings) | Réponse « riche » du moteur pour le popup Assistant : Markdown (tableaux alignés en monospace, gras, code) + URL d'image optionnelle téléchargée à la demande (`online_image`). `image_url` vide = zone image masquée. Ouvre le popup automatiquement |
| `tab5_maj_alertes_ha_bulk` | payload (string) | Jusqu'à 4 bandeaux d'alertes/infos HA, un panneau du rotateur chacun, tap-to-dismiss local (`parse_and_update_ha_alerts_bulk()`) |
| `tab5_maj_rdv_prochains` | payload (string) | Réveil : liste des prochains rendez-vous `epoch\|titre~…` (8 max) poussée par `script.tab5_rdv_prochains` (package HA `tab5_reveil.yaml`) ; le firmware tient le compte à rebours et annonce lui-même (`rdv_store()`, `alarm_clock.cpp`) |
| `tab5_maj_calendrier_mois` | annee, mois, codes, heures, details (strings) | Popup calendrier : bitmask 2 hex/jour (travail/férié/vacances scolaires/RDV/anniversaire) + 31 champs d'heures de travail + libellés de détail — mis en cache, re-rendu si le mois est affiché (`cal_store_month_data()`/`cal_render_month()`) |
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
| `cal_view_year`, `cal_view_month`, `cal_detail_date` | int/int/string | popup calendrier : mois affiché + date du détail ouvert (le cache mensuel vit en `static` dans `tab5_calendar.cpp`) |

## Règles de code à respecter (issues de l'audit du 05/07/2026)

1. **Pas de couleur en dur** (`0xFFAABB`) dans un YAML/lambda — ajouter un token dans `UIColor::` (`tab5_tokens.h`, inclus par `tab5_custom.h`) et l'utiliser partout.
2. **Les `sensor:`/`text_sensor:` ne manipulent pas LVGL directement** — ils appellent une fonction C++ de la couche `Tab5/tab5_*.cpp` (déclarée dans `tab5_custom.h`) (ex: `update_light_ui()`, pas de `lv_obj_set_style_*` inline). Idem pour les services de `tab5-api-logic.yaml`, et là c'est **vérifié** : `tools/check_tab5_code_rules.py` (joué par `pytest`) échoue sur tout `lv_*` du contrat hors `lv_obj_has_flag`, sur tout `sprintf` brut dans `Tab5/` et sur tout `globals:` que personne ne référence.
3. **Pas de `static` dans une lambda pour de l'état partagé entre deux handlers différents** (`on_short_click`/`on_long_press`) — utiliser un `globals:` (cf. bug `reboot_armed` corrigé le 05/07 ; global retiré le 16/07 quand la console est passée aux overlays de confirmation).
4. **Pas de `std::string` par valeur ni de `to_string()` dans un hot-path** (sliders, `on_value` fréquents) — `const std::string&` ou buffer `snprintf` statique.
5. **Toute nouvelle carte/widget répété ≥3 fois** (météo, switches...) doit passer par une fonction C++ builder paramétrée **ou** un template `!include` + `vars` (ex. `climate_hvac_mode_btn.yaml`, `cal_day_cell.yaml` ×42) — jamais un copier-coller YAML. Même règle dans `AGENTS.md`.
6. Avant de committer : `python -m esphome compile tab5-ha-hmi.yaml` doit réussir (toolchain déjà en cache localement, ~20-45s).
7. **Tout popup modal réutilise le chrome partagé** (ADR-0009) : `modal_scrim.yaml` (var `scrim_opa`) + `modal_header.yaml` (icône, titre, croix — barre de 52 px, corps à `y: ${modal_body_y}`), carte dimensionnée par `${modal_card_w}`/`${modal_card_h}`. Jamais de voile, de titre ou de croix réécrits à la main ; les boutons d'options d'en-tête restent des frères en `y: 4, height: 44`. Vérification : `python tools/check_tab5_modal_chrome.py` (joué aussi par `pytest` et par la CI, `tests/test_guards.py`).
   **Exceptions (pages de jeu)** : les 8 `*_game.yaml` de la section Arcade ci-dessous, plus `game_selector.yaml`. Ce ne sont pas des popups posés sur `page_main` mais des **pages LVGL autonomes** en flux plein écran — pas de garde-fou modal (ni `style_modal_card`, ni `color_modal_scrim`, ni glyphe de croix).
8. **Aucune entité Home Assistant en dur** dans un YAML du firmware — toujours une substitution de `user_entities.yaml` (`${entity_…}`) ou un `!lambda`. Les entités que la tablette expose elle-même (`assist_satellite.*`, `media_player.*`, dérivées par HA du nom de l'appareil) passent par `entity_tab5_satellite` / `entity_tab5_media_player` : défauts dans `tab5-scripts.yaml`, surcharge dans `user_entities.yaml` si l'appareil est renommé. **Vérifié** : `tools/check_tab5_code_rules.py` échoue sur toute valeur `entity_id:` littérale.
9. **Toute icône MDI affichée est dans la liste `glyphs:` de la police `mdi_*` de son widget** (`tab5-styles.yaml`), et tout glyphe listé y est affiché quelque part : un glyphe absent s'affiche vide, sans erreur de compilation. Une icône posée en C++ sur un widget reçu en paramètre exige sa fonction dans `MDI_CODE_TARGETS` (règle 7 de `tools/check_tab5_code_rules.py`, jouée par `pytest`).

---

## Arcade — les 8 consoles

Huit consoles 100 % locales (ni Home Assistant ni réseau) : Fil d'Or, Arcanoïde, Neon Apron, Coureur d'Or, Go Tab, Trial Poursuite, Dames Tab, Roi Noir. Chacune est sa propre page LVGL `skip: true` (exception ADR-0009 : pas de chrome modal), son contenu est construit en C++, elle est déclarée une seule fois dans `GameRegistry::kGames` (ADR-0013) et partage `game_common.h` (ADR-0014).

**Documentation complète — architecture, page Arcade, les six points pour ajouter une 9ᵉ console, puis une section par jeu (contrôles, IA, NVS, tests) : [`docs/arcade.md`](../docs/arcade.md).**

---

## Version Française

Ce dossier contient les packages de configuration ESPHome et les fichiers source C++ du firmware Tab5. Point d'entrée : `../tab5-ha-hmi.yaml`.

**Ce fichier est volontairement bilingue par section, pas dupliqué** (contrairement au `README.md` racine et aux `docs/*.md`) : la description fichier par fichier, la table des services HA et la table des globals sont en **anglais** ci-dessus ; les règles de code sont en **français** (les sections des 8 consoles, en français aussi, vivent dans [`docs/arcade.md`](../docs/arcade.md) depuis le 25/09/2026). Le doubler intégralement coûterait plus qu'il ne rapporte — et une traduction qui dérive est pire qu'une section unique à jour.

Historique de vérification : écrit contre le code réel le 05/07/2026, re-vérifié ligne à ligne le 14/07/2026, puis le 17/07/2026, complété le 19/07/2026 (15 services dont `tab5_maj_calendrier_mois`/`_jour`, popups v2 + calendrier, télécommande TV, wake word « Stop », scripts par familles), et re-vérifié le 30/07/2026 (migration des jeux en pages LVGL dédiées, ajout des sections Coureur d'Or / Trial Poursuite / Dames Tab, `st7123` officiel).

---

## Fichiers de polices

| Fichier | Contenu |
|---------|---------|
| `materialdesignicons-webfont.ttf` | Material Design Icons — 8 tailles chargées séparément : `mdi_font_26/32/45/56/70/120`, `mdi_assist_36`, `mdi_font_alert` (60 px), chacune avec la seule liste des icônes qu'elle affiche (une par ligne, nommée ; règle 7 de `tools/check_tab5_code_rules.py`). Le 08/09/2026 : `mdi_font_80` (qui chargeait du 70) renommé `mdi_font_70`, `mdi_font_60` (un glyphe, jamais référencé) retiré ; le 25/09/2026 : `mdi_assist_64` retirée, 85 glyphes jamais affichés supprimés |
| `IconeMeteo.ttf` | Police d'icônes météo personnalisée (`font_meteo_card` 120, `font_meteo_card_small` 80 — les tailles 270/190 sont retirées depuis le 25/09/2026, jamais affichées) |
| `ChessPieces.ttf` | Figurines d'échecs vectorielles pour « Roi Noir » — licence dans `ChessPieces.LICENSE.txt` |

## Sous-répertoires

### `ui_components/`
Les 35 composants LVGL décrits plus haut. Seul sous-répertoire versionné. (`my_components/st7123/` n'existe plus : `st7123` est une plateforme officielle depuis ESPHome 2026.7.0.)

### `tts_library/`, `tts_library_v2/` — **non versionnés**
Fichiers audio TTS expérimentaux antérieurs à l'intégration Voice de HA, gitignorés et inutilisés par la config actuelle. Ils n'existent pas dans un clone : ne les cherchez pas.
