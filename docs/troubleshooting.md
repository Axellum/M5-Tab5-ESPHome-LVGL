# Troubleshooting — incidents already diagnosed

## English · [Français](#version-française)

---

This is a log of real symptoms hit on this exact device/firmware, with the confirmed root cause and fix. Check here **before** re-diagnosing something that looks familiar — several of these took hours of live debugging with the device on a desk. If you're an AI agent auditing this code, several of these look like bugs at first glance; they are documented here precisely because they were already investigated and closed.

Format: **Symptom → Root cause → Fix**. Entries are chronological, most recent last within each topic isn't guaranteed — read the whole thing, it's short.

---

### Black screen after a software reboot (not a power cycle)

**Symptom:** device reboots (OTA, crash, `api.reboot`), display stays black afterward. A power cycle (unplug/replug) fixes it; a soft reboot alone does not, or does so unreliably.

**Root cause:** the display's `reset_pin` is wired through the `PI4IOE5V6408` I2C GPIO expander, not a native ESP32 GPIO. Right after boot, the expander itself needs a short settle time before it reliably drives its output pins — toggling `reset_pin` too early is a no-op from the display's point of view.

**Fix:** a blocking `delay(1000)` was added in `on_boot: priority: 700` in `tab5-hardware.yaml`, right before the display reset sequence. Confirmed root cause on 2026-07-06 after 5 live reboot tests with the developer watching the screen. This is marked `[AI-WARNING]` in the file — **do not remove this delay** to "optimize boot time" without re-testing across several reboots; a previous permissive `logger: level: VERY_VERBOSE` workaround (kept for weeks as a correlation-only mitigation) was replaced by this confirmed fix.

---

### Weather / planning / climate data missing, but the device is online

**Symptom:** the device shows a healthy Wi-Fi/API connection (diagnostics entities look fine, uptime climbing), but the weather, planning, and forecast cards never update — while the clock and a couple of directly-mirrored sensors keep working, which makes the failure look partial and confusing.

**Root cause:** the push automations on the Home Assistant side are all gated behind one shared `condition: state` on a boolean helper (a "which HA instance is the active one" flag, relevant only in a dual-instance setup). That helper was stuck `off`; a guard automation meant to force it back `on` at every HA boot was itself disabled, so even a full HA restart didn't self-heal.

**Fix:** turn the helper back on, re-enable the guard automation, manually trigger the push automation once. **If your setup pushes conditionally on a shared state flag, make sure the automation that's supposed to re-arm it on boot is actually enabled** — an automation that silently no-ops because its own guard is off is easy to miss for a long time (in this case, ~3 hours before being noticed).

---

### STT/TTS broken, dozens of "entity already exists — ignoring" log lines

**Symptom:** voice pipeline reports a missing/broken speech-to-text provider despite the underlying service responding correctly when tested directly. Home Assistant logs show many `does not generate unique IDs ... already exists - ignoring` warnings across automation/sensor/climate/calendar entities.

**Root cause:** a `remote_homeassistant`-style integration was mirroring an entire second Home Assistant instance's entities into this one. ID collisions silently shadowed the real STT/TTS entities the voice pipeline depended on. This only reproduces in a **dual Home Assistant instance** setup — not relevant if you run a single HA instance.

**Fix:** disable the mirroring integration, full HA restart. If you don't run two HA instances feeding into each other, this can't happen to you.

---

### Touch "not working" but the touchscreen is actually detecting presses fine

**Symptom:** tapping the screen does nothing; looks like a dead touch controller.

**Root cause:** Home Assistant was rejecting the device's service calls (`Service call ... rejected; ... enable this functionality in the options flow`) — the touch events were reaching HA, HA just refused to act on them.

**Fix:** enable "Allow service calls" in the ESPHome integration's options flow for this device in Home Assistant. Check this before assuming a hardware/firmware touch bug.

---

### A calendar/weather push silently breaks the rest of the same automation

**Symptom:** after fixing the two issues above, planning/weather/climate were *still* not showing up on screen — a distinct bug from the ones above.

**Root cause:** the automation called `calendar.get_events` with `start_date_time: "{{ now() }}"` — a raw Jinja `now()` datetime object including microseconds, which Home Assistant's schema silently rejected. Because the calendar call was one step among several sequential actions in the same automation, the validation failure aborted everything after it — including unrelated weather and forecast pushes further down the same automation.

**Fix:** format the datetime explicitly: `"{{ now().strftime('%Y-%m-%d %H:%M:%S') }}"`. More broadly: add `continue_on_error: true` on individual push actions (calendar, each `weather.get_forecasts` call, each screen-push service call) so one bad payload doesn't take down unrelated pushes later in the same automation, plus `is defined` guards on any template variable consumed downstream.

---

### ESPHome `pressed:` style rejected when placed in a shared `style_definitions`

**Symptom:** `esphome compile` fails with `[pressed] is an invalid option for [style_definitions]` after trying to centralize a button's pressed-state styling.

**Root cause:** confirmed by reading ESPHome's own LVGL component source (`defines.py` / `widgets/__init__.py` / `styles.py`) rather than guessing — the `pressed:` state key is only valid on the widget itself, not inside a reusable `style_definitions:` block.

**Fix:** repeat the `pressed:` block on each individual "glass" button widget instead of trying to share it. This is a genuine ESPHome/LVGL framework limitation, not a code smell to refactor away — don't spend time trying to DRY it further.

---

### ESPHome API connections exhausted (device drops connections under otherwise normal use)

**Symptom:** device intermittently fails to accept new API connections (from HA, from `esphome logs`, from OTA) for no obvious reason.

**Root cause:** the ESPHome native API only allows a small, fixed number of simultaneous connections (8). Leftover `esphome` CLI processes from earlier debugging sessions (e.g. `esphome logs` left running in a forgotten terminal) each hold one connection open indefinitely.

**Fix:** close CLI sessions when you're done with them; if connections seem exhausted, check for and kill orphaned local `esphome` processes before assuming a device-side bug.

---

### False positives worth knowing about (don't "fix" these again)

- **Forecast pagination "wrap-around"**: the 5 forecast pages (indices 0–4) intentionally do **not** wrap from 4 back to 0 on a further right-swipe. This was already "corrected" once by an LLM audit that assumed non-wrapping was a bug, then reverted. See [`docs/decisions/`](decisions/README.md).
- **A cover entity showing `unknown` in Home Assistant**: this can be real on the HA side while being irrelevant to the Tab5 firmware, if the corresponding UI card drives its own internal state via globals + a script rather than reading that entity's state directly. Check what the specific `ui_components/*.yaml` card actually binds to before assuming the firmware is affected.
- **Touch "overlap" between two adjacent buttons flagged by a static audit**: verify the actual pixel geometry (`x`/`y`/`width`/`height`) before trusting a reported overlap — a past report was off by roughly a dozen pixels and wasn't a real overlap.

---

---

## Version Française

---

Journal d'incidents réels rencontrés sur ce firmware précis, avec la cause racine confirmée et le correctif. À consulter **avant** de re-diagnostiquer quelque chose qui semble familier — plusieurs de ces cas ont demandé des heures de debug en direct avec l'appareil sur le bureau.

Format : **Symptôme → Cause racine → Correctif**.

### Écran noir après un reboot logiciel (pas une coupure d'alimentation)

**Symptôme :** après un reboot (OTA, crash, `api.reboot`), l'écran reste noir. Un cycle d'alimentation (débranché/rebranché) corrige le problème ; un reboot logiciel seul non, ou pas de façon fiable.

**Cause racine :** le `reset_pin` de l'écran passe par l'expander GPIO I2C `PI4IOE5V6408`, pas un GPIO natif ESP32. Juste après le boot, l'expander a lui-même besoin d'un court délai de stabilisation avant de piloter ses sorties de façon fiable — actionner `reset_pin` trop tôt est un no-op du point de vue de l'écran.

**Correctif :** un `delay(1000)` bloquant a été ajouté dans `on_boot: priority: 700` (`tab5-hardware.yaml`), juste avant la séquence de reset écran. Cause racine confirmée le 06/07/2026 après 5 tests de reboot en direct. Marqué `[AI-WARNING]` dans le fichier — **ne pas retirer ce délai** sans re-tester sur plusieurs reboots.

### Météo / planning / clim absents alors que l'appareil est en ligne

**Symptôme :** l'appareil affiche une connexion Wi-Fi/API saine, mais les cartes météo/planning/prévisions ne se mettent jamais à jour.

**Cause racine :** les automations de push côté HA sont toutes conditionnées par un flag booléen partagé (pertinent uniquement dans un setup à double instance HA). Ce flag était bloqué à `off` ; l'automation de garde-fou censée le reforcer à `on` à chaque boot HA était elle-même désactivée.

**Correctif :** réactiver le flag et l'automation de garde-fou, déclencher manuellement le push une fois. **Si votre setup pousse conditionnellement sur un flag d'état partagé, vérifiez que l'automation censée le réarmer au boot est bien active.**

### STT/TTS cassé, dizaines de logs "entity already exists — ignoring"

**Symptôme :** pipeline vocal en échec malgré un service STT sain testé directement. Logs HA pleins d'avertissements de collision d'ID.

**Cause racine :** une intégration type `remote_homeassistant` dupliquait toute une seconde instance HA dans celle-ci — collisions d'ID masquant les vraies entités STT/TTS. Ne se reproduit que dans un setup à **double instance HA**.

**Correctif :** désactiver l'intégration de duplication, redémarrage HA complet.

### Tactile "mort" alors que le contrôleur détecte bien les appuis

**Symptôme :** taper sur l'écran ne fait rien.

**Cause racine :** HA rejetait les appels de service de l'appareil (`Service call ... rejected`).

**Correctif :** cocher "Autoriser les appels de service" dans l'options flow de l'intégration ESPHome pour cet appareil.

### Un push calendrier/météo casse silencieusement le reste de la même automation

**Cause racine :** `calendar.get_events` avec `start_date_time: "{{ now() }}"` (datetime brut avec microsecondes) — échec de validation silencieux qui interrompait toute la suite de l'automation.

**Correctif :** `"{{ now().strftime('%Y-%m-%d %H:%M:%S') }}"`, plus `continue_on_error: true` sur chaque action de push + gardes `is defined`.

### `pressed:` LVGL refusé dans un `style_definitions` partagé

**Cause racine :** limitation confirmée dans le code source du composant LVGL d'ESPHome — `pressed:` n'est valide que sur le widget lui-même.

**Correctif :** répéter le bloc `pressed:` sur chaque bouton "verre" individuel — pas un refacto à pousser plus loin.

### Connexions API ESPHome épuisées

**Cause racine :** l'API native n'autorise que 8 connexions simultanées ; des process `esphome` CLI orphelins (sessions de debug oubliées) en occupent chacun une indéfiniment.

**Correctif :** fermer les sessions CLI une fois terminées ; vérifier les process `esphome` orphelins avant de soupçonner l'appareil.

### Faux positifs à connaître (ne pas re-"corriger")

- **Pagination prévisions sans bouclage** : intentionnel (0↔4), déjà "corrigé à tort" une fois par un audit LLM puis reverté. Voir [`docs/decisions/`](decisions/README.md).
- **Entité `cover` à `unknown` côté HA** : peut être réel côté HA sans affecter le firmware si la carte concernée pilote son propre état via des globals + un script plutôt que de lire cette entité.
- **"Chevauchement" tactile signalé par un audit statique** : vérifier la géométrie pixel réelle avant de faire confiance au rapport — un cas signalé était décalé d'une douzaine de pixels, pas un vrai chevauchement.
