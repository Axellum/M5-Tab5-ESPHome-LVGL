# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Dates are the day each pull request was merged into `main`.

## [Unreleased]

### 2026-07-16 — Popup lumière v2 : plein écran, sélecteur, % live, pastilles couleur
- `light_popup.yaml` refondu (1130×650 → carte 1250×690, 15 px des bords) : 3 cartes de verre — AMPOULE (sélecteur Chambre/Salon/LEDs avec icônes d'état + surbrillance cyan, On/Off, Tout éteindre), LUMINOSITÉ (arc 320 px, valeur % live au centre, raccourcis 10/35/65/100 %), COULEURS (3 blancs nommés + 12 pastilles rondes).
- `light_color_preset_btn.yaml` : template pastille ronde 78 px (bg = couleur), 12 instances (#T164) — remplace l'ancien bouton icône+libellé.
- Nouveau `script.tab5_light_popup_show(light_idx)` (`tab5-scripts.yaml`) : synchro complète titre/sélecteur/power/arc depuis l'état HA réel, appelé par les long-press `forecast_daily.yaml` (3 lambdas dupliquées supprimées) et le sélecteur interne. Logique LVGL dans `tab5_custom.cpp` (`show_light_popup_ui`, `update_light_selector_icon`, `sync_light_popup_brightness`) — règle « pas de `lv_obj_*` dans le YAML » respectée.
- `tab5-sensors-domotique.yaml` : 3 capteurs `attribute: brightness` (sync live de l'arc quand le popup est ouvert, jamais pendant un drag) + recoloration live des icônes du sélecteur dans `light_*_state`.
- Arc luminosité débouncé (`tab5_debounce_light_brightness`, 200 ms, même motif que le volume console) : un seul `light.turn_on` par glissement au lieu d'un par tick.
- Anti « croix qui décale » (même fix que console v2) : `scrollable: false` sur la carte et toutes les sous-cartes, croix = vrai bouton de verre 96×64.
- `tab5-styles.yaml` : glyphe `F1051` (led-strip) ajouté à `mdi_font_45`.

### 2026-07-16 — Docs README : préambule perso + vocal/moteur + TV/alertes
- `README.md` (EN+FR) : note personnelle (architecte vs créateur), tableau pipeline vocal ↔ [vromvrom-engine](https://github.com/Axellum/vromvrom-engine), télécommande TV, infos/alertes HA avec tap-to-dismiss, console v2.
- `docs/related_projects.md` : lien public moteur → `vromvrom-engine` (plus ServeurHA).
- `docs/voice_assistant.md` + `docs/screens.md` : alignés firmware (moteur Discussion, dismiss, popup TV).

### 2026-07-16 — Télécommande TV : calage symétrique plein écran
- `tv_remote_popup.yaml` refondu : modal 1230×670 (~25 px des bords), grille 3 colonnes symétrique, rangée basse unifiée (Play/Pause/Retour/Accueil/Muet).
- Rangée basse calée à 6 px du bord bas ; corps raccourci pour ne plus chevaucher Menu ni volume.
- Overlay 85 % pour masquer le dashboard dessous.
- Vérifié OTA prod : `config_hash=0x218c6309` (validé Axel).

### 2026-07-16 — Tuiles Domo + layout horloge/clim
- `style_meteo_card` aligné sur le verre des boutons Domo (`style_clim_btn` : opa 58 %, bordure 1 px/35 %, radius 18) — horloge, bandeau central, tuiles météo (toutes pages), page HA, zone clim.
- Horloge : 401×200, `y: 5` (−6 px/côté, −10 px haut, remontée 5 px).
- Clim : parent transparent ; verre uniquement autour de − / cible / + (`climate_controls_zone`) ; températures salon/serre inchangées hors zone.
- Backups essai : `docs/essais_design/tab5-styles_avant_meteo_comme_domo.yaml`, `docs/console_sys_v2_essai_glass_card.yaml`.
- Vérifié OTA prod : `config_hash=0x30575f2f` (validé Axel).

### 2026-07-16 — Console Système v2 (redesign + HA management)
- `Tab5/ui_components/console_sys.yaml` rewritten (233 → 415 lines): modal card enlarged to 1180×680, content organized in 4 glass cards (`style_glass_card`) — MÉMOIRE (SRAM/PSRAM bars restyled with `color_arc_track` track, bloc max, flash), RÉSEAU (SSID, IP, RSSI, new `lbl_sys_ha_val` HA connection status), SYSTÈME (uptime, CPU temp, loop time, volume slider with live % readout `lbl_sys_vol_val`), GESTION (new).
- GESTION card: « MAJ Écran » (turns `input_boolean` `${entity_primary_active}` back on then triggers `${entity_push_automation}` — direct remedy for the recurring frozen-screen bug), « Recharger autos » (`automation.reload`), « Redémarrer HA » (`homeassistant.restart`) and « Reboot tablette » — the last two behind Annuler/Confirmer overlays (`overlay_confirm_*`).
- Fix "close button shifts the content": the modal card was scrollable by default, so a slightly dragged tap scrolled the content and left it offset — `scrollable: false` set on the modal card and every sub-card; the close X is now a real 96×64 glass button with pressed feedback.
- Confirm overlays replace the old invisible double-tap arming: the armed state is the overlay's visibility (single source of truth), so the `reboot_armed` global was removed from `tab5-globals.yaml` (rule header reworded, README globals table updated).
- Data contract unchanged (`lbl_sys_*`, `bar_sys_*`, `slider_volume` ids kept); `tab5-sensors-diagnostics.yaml` 2 s interval and `tab5-lvgl.yaml` open handler now also feed `lbl_sys_ha_val` (Connecte/Hors ligne, green/red) from `status_ha`.
- New substitutions `entity_primary_active` / `entity_push_automation` in `user_entities.yaml` + example file.
- Docs resynced: `Tab5/README.md`, `docs/screens.md`, `docs/architecture.md`, `docs/debugging.md`, `CARTOGRAPHIE_TAB5.md` (both copies). Details of the companion edits: `docs/console_v2_modifs_preparees.md`.
- Verified: `esphome config` valid + full `esphome clean` + `compile` SUCCESS (`config_hash=0x5e927d97`) + OTA prod 16/07 (`ha_api_status=on`, uptime croissant).

### 2026-07-14 — Documentation coherence pass (docs vs. real code)
- Audited `Tab5/README.md`, `docs/*.md` and `CARTOGRAPHIE_TAB5.md` line-by-line against the firmware. Doc-only change, no code touched.
- `Tab5/README.md`: central-card rotator corrected 6 s → 8 s and 3 → 4 panels; `tab5_maj_probabilites` and `tab5_maj_info_texte` added to the API service table (10 services total) and `tab5_maj_pluie_1h`/`tab5_maj_clim`/`tab5_maj_volet_etat` payloads corrected; `has_info` added to the globals table; navigation description updated (console via `btn_control_console`, swipe = forecast pagination only, `y ≥ 333`); entry-point description updated for `user_entities.yaml`; MDI font sizes corrected; ST7123 described as the touch (not display) controller.
- `docs/architecture.md`: "seven packages" → eight (EN+FR); fictional `tab5_update_meteo_7j`/`parse_meteo_7j` examples replaced with the real `tab5_maj_previsions_jours_bulk`/`parse_and_update_jours_bulk`; hardware bullets aligned with the code (MIPI-DSI, no SPI/UART, ES7210, SDIO co-processor); C++ layer description refreshed (no "voice state machine" in C++ — mic icon colors are set in `tab5-hardware.yaml` callbacks); info panel and console button integrated.
- `docs/hardware.md`: FR display section corrected (1024×600/RGB parallel → 1280×720/MIPI-DSI); framebuffer math updated (~1.8 MB); ESP32-C6 link corrected (UART → SDIO via `esp32_hosted`); microphone path corrected (PDM → ES7210 ADC, + MCLK GPIO 30); removed the obsolete `i2c.write_bytes` register-0x04 claim and the nonexistent ambient light sensor; boot order fixed (amp enabled before the HA API wait).
- `docs/screens.md`, `docs/debugging.md`: 4th info panel documented; console described as it is (diagnostics + volume + reboot, opened by button, not a log viewer, not swipe); `debugging.md` now shows the actual console photo (`tab5_photo_dashboard_weather.jpg` — the filenames of the two photos are historically swapped); temporary planning override attributed to `show_temporary_planning()` (C++) instead of the removed ESPHome script.
- `docs/ui_design.md`: leftover 1024×600 references → 1280×720.
- `docs/voice_assistant.md`: boot order and icon-state mechanism corrected (no `voice_state` global).
- `README.md` (root): "Six screens" reframed as the real single-page layout with six functional areas; "seven files" → eight; climate modes/presets corrected; console description fixed; data-packing example updated (EN+FR).
- `CARTOGRAPHIE_TAB5.md`: line counts refreshed (incl. `tab5_custom.cpp` 1095 L, `tab5-ha-hmi.yaml` 103 L); deleted `st7123/binary_sensor/` no longer listed as present dead code; `esp32_hosted` SPI → SDIO; `docs/*.md` inventory updated (audit reports removed, troubleshooting/debugging/decisions added); ambiguous `07/12` dates normalized to `12/07`; §4.6 completed with the 14/07 PRs.
- ADR-0002 amended with the 14/07 gesture rework (info panel, swipe zone, console button).

### 2026-07-14 — Split `tab5-sensors.yaml` into diagnostics / domotique packages
- `Tab5/tab5-sensors.yaml` (522 lines) split into `Tab5/tab5-sensors-diagnostics.yaml` (`wifi:` block, GPIO power switches, HA API status, IP/SSID, uptime, RSSI, core temp, free RAM/loop time, antenna select, SNTP clock, console intervals) and `Tab5/tab5-sensors-domotique.yaml` (plant moisture ×5, lights, PC presence, phone battery, temperatures/humidity, audio amp/jack/wake-word). Blocks copied byte-identical, no functional change.
- `packages:` updated in `tab5-ha-hmi.yaml`; docs and `[AI-CONTEXT]` pointers updated (`CARTOGRAPHIE_TAB5.md`, `Tab5/README.md`, `docs/architecture.md`, repo `README.md`, C++ comments).
- Implemented the `tab5_maj_info_texte` API service (empty stub since April): new `info_wrapper`/`lbl_info_text` 4th panel in the central card rotator, showing the 3-day calendar recap (recolor markup, `roboto_22`) or a Rouge/Orange weather-alert banner (`roboto_32_b`, colored by the `couleur` variable) sent by `automations_tab5.yaml` section 7. LVGL updates factored into `update_info_text_ui()` (per `tab5_custom.cpp` rule).
- `show_temporary_planning()` now restores the previously active panel (4-state static helper) and also hides the info panel during the 6 s temporary display.
- Forecast swipe rework: swipe zone limited to the central card band (`y >= 333`), console overlay now opened via `btn_control_console` only (no more up/down swipe); page title overlay (`page_title_wrapper`) shown on non-home forecast pages, day tiles titled "Lun 16" via SNTP on daily pages 2-3.
- UTF-8 accent fix: static strings use proper UTF-8 escapes (`\xC3\xA9` not Latin-1 `\xE9`); vigilance alert banner generated in firmware; `normalize_text_utf8()` for dynamic HA strings (Latin-1/mojibake); helpers `update_clock_date_ui()`, `update_rain_phrase_ui()`, `update_planning_text_ui()`.
- Correction of the 2026-07-12 note below: the service **is** called from HA (Tab5 automation section 7); its removal had already been reverted as a stub by the reboot fix.

### 2026-07-12 — Stabilite reboot 60s + reintegration progressive (#T220–#T226)
- Fix reboot ~60s : retrait `buffer_size` LVGL, planning au tap en C++ (`show_temporary_planning`), stub `tab5_maj_info_texte`.
- Garde visibilite capteurs console (#T222) : pas de MAJ LVGL si overlay masque.
- Durcissement ABI animation rotateur (#T225) : `anim_y_cb` statique.
- Migration complete `UIColor::` / tokens YAML (#T226) : API, meteo, console, popups ; hex restants limites aux presets couleur lumiere HA.

### 2026-07-12 — Entity substitutions split (user_entities.yaml)
- Home Assistant entity IDs moved out of `tab5-ha-hmi.yaml` into a local `Tab5/user_entities.yaml` (gitignored, same pattern as `secrets.yaml`).
- Added tracked template `Tab5/user_entities.example.yaml` with generic placeholder entity IDs for public repo and CI.
- CI workflow copies the example file before compile. `.gitignore` extended for `__pycache__/` / `*.pyc`.

### 2026-07-12 — Concours polish (docs & API cleanup)
- Refreshed `CARTOGRAPHIE_TAB5.md` §4 (resolved debt, current line counts).
- Removed unused stub API service `tab5_maj_info_texte` (never called from HA).
- Added `docs/images/gpio_pinout_table.png` and `push_only_architecture_diagram.png` to architecture/hardware docs.
- README CI badge, `CONTRIBUTING.md`, `docs/architecture.md` updated for `user_entities.yaml`.

### 2026-07-12 — Bug planning tuiles météo + console diagnostic
- Fixed wrong day index on min/max temperature tap (`(forecast_page_index - 2) * 5 + idx`).
- Added `get_day_planning_display_text()` with fallback when opening hours are empty.
- Console overlay: new header icon (`mdi-console-line`), layout rework (SRAM/PSRAM bars, aligned labels).
- Extended `UIColor::` in `tab5_custom.h` for weather icons, rain bars, alert date pastels.
- Migrated remaining hardcoded hex in `tab5_custom.cpp` and `tab5-api-logic.yaml`.
- Removed dead commented block in `tab5_maj_meteo_actuelle`.

## [1.0.0] — 2026-07-06 — first tagged release

This is the first version tagged in git. It was cut here rather than retroactively at the earlier "v1 stable" checkpoint (PR #6) because everything since has made the project strictly more stable and more complete: a confirmed (not just worked-around) fix for the black-screen-after-reboot bug, several rounds of factorization, technical-debt cleanup, and — in this same release — the addition of `AGENTS.md`, `docs/decisions/`, `docs/troubleshooting.md`, `docs/debugging.md`, and this changelog itself.

This is a personal, "100% AI-generated" project (see the README's "Note on AI"): stable and in daily use, but not aesthetically polished, not manually code-reviewed line-by-line, and with known open items — see [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md) §4 for the current, honestly-tracked technical debt. Tagging `1.0.0` here means "stable enough to be a reference point," not "finished" or "audited to a professional standard."

### 2026-07-06 — AI-agent documentation layer ([#22](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/22))
- Added `AGENTS.md` — entry-point instructions for AI coding agents (build/verify commands, read order, boundaries).
- Added `.github/PULL_REQUEST_TEMPLATE.md` — checklist covering compile verification, OTA testing, `[AI-WARNING]` review, and doc upkeep.
- Added `docs/troubleshooting.md` — symptom → root cause → fix log for incidents already diagnosed on this device.
- Added `docs/decisions/` — retroactive architecture decision records (push-only design, single-page navigation, data packing, boot delay, etc.).
- Added `docs/debugging.md` and the `[AI-DEBUG]` tag convention (alongside `[AI-CONTEXT]`/`[AI-WARNING]`) in `Tab5/README.md`.
- Fixed `docs/architecture.md`, which still described a multi-page tab-bar layout (`page_accueil`/`page_meteo`/.../`tab_bar`) that no longer matched the shipped firmware (single `page_main` + popups + swipe). Corrected in both language versions.

### 2026-07-06 — scripts & cartography ([#20](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/20), [#21](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/21))
- AI-CONTEXT headers and `continue_on_error` resilience added to the HA-side example scripts.
- Added `CARTOGRAPHIE_TAB5.md`, the full dependency-graph/file-inventory reference; cleaned up leftover IA scripts and drafts.

### 2026-07-06 — technical debt cleanup ([#15](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/15))
- Removed a tracked backup snapshot (`Tab5_backup_20260525/`, including committed `.pyc` files) and an orphaned `tab5-images.yaml`.
- Removed dead code (`my_components/st7123/binary_sensor/`, a write-only array) and hardcoded hex colors in `tab5_maj_clim`, replaced with `UIColor::` tokens.

### 2026-07-06 — black screen root cause confirmed ([#13](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/13))
- Confirmed root cause of the "black screen after software reboot" bug (the display reset pin runs through an I2C GPIO expander that needs a settle delay after boot) and applied the real fix, superseding the earlier `VERY_VERBOSE`-logging workaround.

### 2026-07-06 — climate popup factorization ([#12](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/12))
- Factorized 6 of the 9 climate popup grid buttons into parametrized templates (task #T164). The remaining 3 + the two temperature +/- buttons were deliberately left as-is — see [ADR-0007](docs/decisions/0007-climate-popup-not-factorized.md).

### 2026-07-06 — buffer fix & UI polish ([#10](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/10), [#11](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/11))
- Weather-alert payload buffer widened 512→1024 bytes to prevent silent truncation of long alert text; `strip_prefix` passed by reference.
- Pressed-state visual feedback added to "glass" buttons; central-panel carousel slowed 6s→8s.

### 2026-07-05 — dedup pass ([#7](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/7), [#8](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/8), [#9](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/9))
- Recovered an orphaned commit fixing the black-screen mitigation and a dead climate entity; deduplicated moisture/light-state sensors.
- Factorized `forecast_daily.yaml`, `switches_card.yaml`, and the light popup's 8 color preset buttons.

### 2026-07-05 — "v1 stable" checkpoint ([#6](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/6))
- Fixed a critical reboot crash (uninitialized `reinterpret_cast`), removed dead weather code, added volume slider debounce, hardened network boot (timeout, conditional heartbeat), fixed a phantom climate entity, and fully rewrote `Tab5/README.md` (the previous version described an unrelated, outdated project iteration). This was the project's own internal "v1 stable" milestone at the time, ahead of an actual git tag.

### Earlier — initial build-out ([#1](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/1)–[#5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/5))
- Initial audio/image asset libraries and ESPHome configuration (styles, climate card, OOM buffer guards).
- Systematic nullptr guards across API services (fixed a reboot loop caused by HA pushing data before LVGL widgets existed).
- Service API params switched to string+`atof`/`atoi` to tolerate non-numeric Jinja values.
- CI fixed by generating a dummy `secrets.yaml` before the ESPHome build.
- OTA reboot fix on ESP32-P4, automation examples updated.
