# Debugging this device

## English · [Français](#version-française)

---

This is a short methodology note, not an incident log — see [`docs/troubleshooting.md`](troubleshooting.md) for already-diagnosed symptoms. Read that one first; only reach for the techniques below if the symptom isn't already documented there.

## Where to look

- **Live ESPHome logs** (`esphome logs tab5-ha-hmi.yaml`, or the ESPHome dashboard's log view) — the primary source of truth for boot sequence issues, API connection state, and any `ESP_LOG*` line in the C++ code. Close the session when you're done; a leaked `esphome logs` process holds an API connection open indefinitely (see the "API connections exhausted" entry in `troubleshooting.md`).
- **The on-screen console overlay** (`console_sys.yaml`, opened via the console button `btn_control_console`, top right — not by swipe since the 14/07/2026 rework) — shows live diagnostics (SRAM/PSRAM usage, max free block, uptime, Wi-Fi signal/SSID/IP, CPU temperature, loop time) plus a volume slider and a double-tap reboot button, directly on the device. It is **not** a log viewer — for payload/event logs use `esphome logs`. Useful when you don't have a laptop connected but can see the screen.

  ![Console overlay on the real device](images/tab5_photo_dashboard_weather.jpg)

- **Home Assistant's own logs** for anything upstream of the device — automation trigger/condition evaluation, template rendering errors, service call rejections.

## Marking a spot for later, in code

Use the `[AI-DEBUG]` tag (see [`Tab5/README.md`](../Tab5/README.md)) in a comment when you find a good observation point while investigating something, even if you don't fix the underlying issue in the same session — it saves the next debugging pass (human or AI) from re-finding the same vantage point.

## Diagnosing a silent automation failure on the Home Assistant side

If a Home Assistant automation appears to do nothing — no error, no expected result — and you suspect a step is failing validation or a condition is silently false, **insert a debug marker directly into the real automation** rather than trying to reproduce its logic in a separate, isolated test script.

Concretely: add a `input_text.set_value` (or similar low-cost, visible side effect) call as an extra step at the point you want to check, using the real automation, the real trigger, the real entity states — then trigger it for real and read the value back.

This matters because a from-scratch reproduction script can differ from the real automation in a way that's exactly the bug — different Jinja context, different entity availability at trigger time, a condition that only fails on a subset of runs. A reproduction that "works" in isolation tells you nothing about why the real one doesn't; it just wastes a debugging cycle. This is exactly how the `now()`-without-`strftime` calendar bug (see `troubleshooting.md`) was found: the isolated repro would have needed to guess the exact template context, while the marker-in-the-real-automation approach surfaced the failing step immediately.

Remove or comment out the marker once the real fix is confirmed — don't leave debug-only automation steps live in production without a reason.

---

---

## Version Française

---

Note de méthodologie, pas un journal d'incidents — voir [`docs/troubleshooting.md`](troubleshooting.md) pour les symptômes déjà diagnostiqués. À lire en premier ; les techniques ci-dessous ne servent que si le symptôme n'y est pas déjà documenté.

## Où regarder

- **Logs ESPHome en direct** (`esphome logs tab5-ha-hmi.yaml`, ou la vue logs du dashboard ESPHome) — source de vérité principale pour les problèmes de séquence de boot, l'état des connexions API, et toute ligne `ESP_LOG*` du code C++. Fermer la session une fois terminée ; un process `esphome logs` oublié occupe une connexion API indéfiniment (voir "connexions API épuisées" dans `troubleshooting.md`).
- **La Console Système à l'écran** (`console_sys.yaml`, ouverte via le bouton console `btn_control_console`, en haut à droite — plus par swipe depuis la refonte du 14/07/2026) — 4 cartes : MÉMOIRE (SRAM/PSRAM, bloc max, flash), RÉSEAU (SSID/IP/signal + état connexion HA), SYSTÈME (uptime, température CPU, temps de boucle, volume) et GESTION (MAJ écran = re-push du flag `is_primary_active` + automation de push, reload des automations, redémarrage HA et reboot tablette — les deux derniers derrière un overlay de confirmation, refonte du 16/07/2026). Ce n'est **pas** un visualiseur de logs — pour les payloads/événements, utiliser `esphome logs`.

  ![Overlay console sur l'appareil réel](images/tab5_photo_dashboard_weather.jpg)

- **Les logs Home Assistant** pour tout ce qui est en amont de l'appareil — évaluation trigger/condition d'automation, erreurs de rendu de template, rejets d'appel de service.

## Marquer un point d'observation dans le code

Utiliser le tag `[AI-DEBUG]` (voir [`Tab5/README.md`](../Tab5/README.md)) en commentaire quand on trouve un bon point d'observation en cours d'investigation, même sans corriger le problème sous-jacent dans la même session.

## Diagnostiquer un échec silencieux d'automation côté Home Assistant

Si une automation HA ne semble rien faire — pas d'erreur, pas de résultat attendu — et qu'on soupçonne qu'une étape échoue à la validation ou qu'une condition est silencieusement fausse, **insérer un marqueur de debug directement dans la vraie automation** plutôt que d'essayer de reproduire sa logique dans un script de test isolé.

Concrètement : ajouter un appel `input_text.set_value` (ou effet de bord similaire, visible et peu coûteux) comme étape supplémentaire au point à vérifier, en utilisant la vraie automation, le vrai trigger, les vrais états d'entités — puis déclencher pour de vrai et relire la valeur.

Une reproduction isolée peut diverger de la vraie automation exactement sur le point qui cause le bug (contexte Jinja différent, disponibilité d'entité différente au moment du trigger, condition qui échoue seulement sur un sous-ensemble d'exécutions) — une repro qui "marche" en isolation ne dit rien sur le pourquoi de l'échec réel. C'est exactement comme ça qu'a été trouvé le bug `now()` sans `strftime` sur le calendrier (voir `troubleshooting.md`) : une repro isolée aurait dû deviner le contexte de template exact, alors que le marqueur dans la vraie automation a fait apparaître l'étape en échec immédiatement.

Retirer ou commenter le marqueur une fois le vrai correctif confirmé — ne pas laisser d'étapes de debug actives en production sans raison.
