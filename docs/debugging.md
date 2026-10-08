# Debugging this device

## English · [Français](#version-française)

---

This is a short methodology note, not an incident log — see [`docs/troubleshooting.md`](troubleshooting.md) for already-diagnosed symptoms. Read that one first; only reach for the techniques below if the symptom isn't already documented there.

## Where to look

- **Live ESPHome logs** (`python tools/tab5_logs.py --host <ip> --config-ha <HA config folder>` since 3.0, which reads the API key Home Assistant keeps; `esphome logs` no longer finds a key in the YAML) — the primary source of truth for boot sequence issues, API connection state, and any `ESP_LOG*` line in the C++ code. Close the session when you're done; a leaked log process holds an API connection open indefinitely (see the "API connections exhausted" entry in `troubleshooting.md`).
- **The on-screen system console** (`console_sys.yaml`, the System page of the settings popup since 2026-10-08, opened directly by a long press on the gear button `btn_control_console`, top right — a tap opens the settings on their Screen page; not by swipe since the 14/07/2026 rework) — 4 glass cards: MÉMOIRE (SRAM/PSRAM, max free block, flash), RÉSEAU (SSID/IP/signal + HA connection status), SYSTÈME (uptime, CPU temperature, CPU load of each core, loop time, the tablet's battery, volume) and GESTION (screen re-push = the full push automation run again, automation reload, HA restart and device reboot — the last two behind a confirm overlay, 16/07/2026 redesign). It is **not** a log viewer — for payload/event logs use `tools/tab5_logs.py`. Useful when you don't have a laptop connected but can see the screen.

  ![Console overlay on the real device](images/tab5_photo_console_v2.jpg)

- **Home Assistant's own logs** for anything upstream of the device — automation trigger/condition evaluation, template rendering errors, service call rejections.

## Seeing the screen without the tablet

The CI job « Rendu hors tablette » (`.github/workflows/rendu-host.yml`, [ADR-0021](decisions/0021-host-render-stubs.md)) compiles the interface for ESPHome's `host` platform (`tab5-rendu-host.yaml`), runs it on the runner, pushes the demo scenes, then opens the ~80 screens of `tools/rendu/ecrans.py` one by one (popups, sub-windows, Arcade, game menus and games) with a virtual finger, as on the panel. One picture per screen, in the seven languages (one job per language). On a pull request that touches the screen, it says which screens changed compared with `main` and attaches before/after images (artifact `rendu-captures`, folder `diff/`). It does not block the PR. Two tasks do fail on a difference: « clair » and « galerie » (every theme, both modes) compare a live theme switch with a cold start, to the pixel.

- A change on purpose: nothing to do for the screens, `main` becomes the reference once the PR is merged. For the gallery scenes of [`screens.md`](screens.md): `python tools/rendu/maj_references.py --run <run id>` replaces `docs/images/rendu/`; review the images, then commit.
- A new screen: an entry in `tools/rendu/ecrans.py` (taps at the captures' coordinates, landscape 1280×720). « identique à … » in the run means a tap missed.
- Tuning one screen: *Actions → Rendu hors tablette → Run workflow*, fields « seulement » (screen names) and « langues » (e.g. `["de"]`).
- It runs on Linux or macOS only (the `host` platform): on Windows, use the CI (*Actions → Rendu hors tablette → Run workflow*).
- It shows the layout, not the device: no touch, no sound, no voice assistant, no timings or memory.

## Marking a spot for later, in code

Use the `[AI-DEBUG]` tag (see [`Tab5/README.md`](../Tab5/README.md)) in a comment when you find a good observation point while investigating something, even if you don't fix the underlying issue in the same session — it saves the next debugging pass (human or AI) from re-finding the same vantage point.

## After a crash or a Wi-Fi outage: the boot and outage journal

Since 2026-09-26 the device keeps its own journal (`Tab5/ecran/tab5_journal.cpp`), so a crash or a failure of the Wi-Fi co-processor link (ESP32-C6) can be analysed after the fact, without a USB cable plugged in at the time:

- **What is kept:** every error (ESPHome's and ESP-IDF's, including the ESP-Hosted driver of the C6, tag `esp-idf`), plus the warnings logged while Home Assistant is not connected (boot before HA, Wi-Fi down). A line repeated in a row is counted, not duplicated. ESPHome's own crash report (tag `esp32.crash`: PC, fault address and backtrace of both cores) is written by the logger before the journal's `on_message` hook exists, so it never reached the journal on its own; since 2026-10-06 the journal reads it (`esp32::crash_handler_has_data()`), replays it into its lines when the reset reason is a crash, and clears it once the journal has reached HA (as ESPHome does after a log subscription).
- **ESP-IDF warnings are not in the firmware:** ESPHome builds ESP-IDF with its log level at ERROR by default (`esp32: framework: log_level`, hence `CONFIG_LOG_MAXIMUM_LEVEL=1`), so the `ESP_LOGW` and `ESP_LOGI` lines of ESP-IDF and ESP-Hosted are removed at compile time: they show neither in the journal nor on the serial port. One of them is ESP-Hosted's version check of the C6 (see [Hardware](hardware.md#esp32-c6-co-processor)). The absence of such a warning therefore proves nothing. Errors stay, including an ESP-Hosted RPC timeout (`Response not received for …`, or `Error reported: Response Timeout` for an asynchronous request). Seeing the warnings would take a test build with `log_level: WARN` under `esp32: framework:` (not tried). Found while answering [discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369).
- **Where:** 32 lines in `.noinit` RAM, which survives software resets, crashes, watchdogs and a reset through USB, but not a power cut. Once Wi-Fi has been missing for 90 s, a copy goes to NVS (at most every 15 min, only when something new was logged) and is read back at the next boot if RAM was lost.
- **How it reaches you:** at each HA connection, the `esphome.tab5_journal` event is sent for an abnormal reset or a crash report, Wi-Fi missing for 90 s or more, a boot that never reached HA, an error after HA connected, or HA reached more than 90 s after boot. Lines logged before the first HA connection are context only (the ESP-Hosted link is legitimately "not yet up" once on every boot, 7 to 12 s after it): a normal boot sends nothing. Guard (e) of `HomeAssistant_Config/packages/tab5_health.yaml` turns it into a persistent notification (and a phone push when `grave`: abnormal reset, crash, or Wi-Fi missing 90 s). Events stay in the recorder's `events` table.
- **Reset reason of every boot:** the `Tab5 Raison du redémarrage` entity (`Software Reset`, `Exception`, `Task Watchdog`, `Brownout`, `Power On`…), one row per boot in the HA history.
- **Watchdog reset with no crash report = the power button (since 2026-10-06):** a short press on the power button reboots the tablet with `ESP_RST_WDT` and no `esp32.crash` report. The journal calls it « bouton d'alimentation ou chien de garde RTC (rst 0x..) » and sends nothing; the entity shows `Power button or RTC watchdog (rst 0x..)` instead of ESPHome's text, which for this reason repeats the source of the last *requested* reboot (`Reboot request from esphome.ota`: ESPHome never clears it). `rst 0x..` is the ROM's own reset code (`esp_rom_get_reset_reason(0)`; on the ESP32-P4, `ESP_RST_WDT` covers 0x07 and 0x0B, timer-group watchdogs, 0x09, 0x0D and 0x10, RTC watchdog, 0x12, super watchdog). The same reason *with* a crash report is still a crash and still alerts (`Crash, other watchdogs (rst 0x..)`). What no longer alerts: a boot stuck for more than 9 s (the bootloader's RTC watchdog) and a timer-group watchdog that resets without reaching the panic handler; both stay visible in the entity's history. Reasoning and sources in the `[AI-WARNING]` of `Tab5/ecran/tab5_journal.cpp`.
- **Decoding a backtrace:** the addresses belong to the build that crashed. For a local build, keep its ELF (`H:/.esphome_data/build/tab5-ha-hmi/build/tab5-ha-hmi.elf`, compare `config_hash`); for a published one, take the `elf-<revision>` artifact of its « Publication » run (kept 90 days), or rebuild it with *Run workflow*, tag and `elf_seulement` (the run checks that the code is the published one). Then `riscv32-esp-elf-addr2line -pfiaC -e <elf> <addresses>`, or `tools/capture_serie.py --relire … --elf …`. If the report says *captured by a different firmware build*, use that build's ELF.
- **Not done on purpose:** an ESP-IDF core dump to flash. It needs a `coredump` partition, the current table (`partitions.csv`: 2 × 7.75 MB apps + 448 KB NVS) fills the 16 MB, and a partition table change is only written by a USB flash, never by OTA. The journal covers the C6 case, which is not a crash of the P4.

## Capturing a crash on the serial port

When the journal says « crash (exception) » with no report (first seen at the end of an update from HA, 3.0.0-rc.1 → rc.2, 2026-09-27), the panic output of ESP-IDF went to the only place it is written: the USB console (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`), just before the reboot.

1. Tablet plugged into the PC over USB-C. `python tools/capture_serie.py` lists the Espressif devices and refuses to guess: pass `--mac` (the tablet's MAC is its USB serial number) or `--port`.
2. `python tools/capture_serie.py --mac <MAC>`: listens up to 10 min without resetting the tablet (DTR and RTS off before opening), writes `capture-serie-<date>.log` as it goes, and stops 60 s after a reboot.
3. Trigger the event (for example *Install* on the « Firmware » entity in HA).
4. Read the summary: panic block, reboots, ESPHome's crash report, addresses. Decode with the ELF of the firmware **that crashed** (the one before the update): `python tools/capture_serie.py --relire capture-serie-….log --elf tab5-ha-hmi-st7123.elf`.

Never open another Espressif device plugged into the same PC. Closing the ESP Web Tools window (Chrome) restarts the tablet once.

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

- **Logs ESPHome en direct** (`python tools/tab5_logs.py --host <ip> --config-ha <dossier de configuration de HA>` depuis la 3.0, qui lit la clé API que garde Home Assistant ; `esphome logs` ne trouve plus de clé dans le YAML) — source de vérité principale pour les problèmes de séquence de boot, l'état des connexions API, et toute ligne `ESP_LOG*` du code C++. Fermer la session une fois terminée ; un process de logs oublié occupe une connexion API indéfiniment (voir "connexions API épuisées" dans `troubleshooting.md`).
- **La Console Système à l'écran** (`console_sys.yaml`, page Système du popup Réglages depuis le 08/10/2026, ouverte directement par un appui long sur le bouton engrenage `btn_control_console`, en haut à droite — un tap ouvre les réglages sur leur page Écran ; plus par swipe depuis la refonte du 14/07/2026) — 4 cartes : MÉMOIRE (SRAM/PSRAM, bloc max, flash), RÉSEAU (SSID/IP/signal + état connexion HA), SYSTÈME (uptime, température CPU, charge CPU de chaque cœur, temps de boucle, batterie de la tablette, volume) et GESTION (MAJ écran = l'automatisation de poussée complète relancée, reload des automations, redémarrage HA et reboot tablette — les deux derniers derrière un overlay de confirmation, refonte du 16/07/2026). Ce n'est **pas** un visualiseur de logs — pour les payloads/événements, utiliser `tools/tab5_logs.py`.

  ![Overlay console sur l'appareil réel](images/tab5_photo_console_v2.jpg)

- **Les logs Home Assistant** pour tout ce qui est en amont de l'appareil — évaluation trigger/condition d'automation, erreurs de rendu de template, rejets d'appel de service.

## Voir l'écran sans la tablette

Le job CI « Rendu hors tablette » (`.github/workflows/rendu-host.yml`, [ADR-0021](decisions/0021-host-render-stubs.md)) compile l'interface pour la plateforme `host` d'ESPHome (`tab5-rendu-host.yaml`), la lance sur le runner, lui pousse les scènes du mode démo, puis ouvre un à un les ~80 écrans de `tools/rendu/ecrans.py` (fenêtres, sous-fenêtres, Arcade, menus et parties des jeux) avec un doigt virtuel, comme sur la dalle. Une image par écran, dans les sept langues (une tâche par langue). Sur une pull request qui touche l'écran, il dit quels écrans ont changé par rapport à `main` et joint les images avant/après (artefact `rendu-captures`, dossier `diff/`). Il ne bloque pas la PR. Deux tâches échouent sur un écart : « clair » et « galerie » (chaque thème, dans les deux modes) comparent une bascule de thème à chaud et un démarrage à froid, au pixel près.

- Un changement voulu : rien à faire pour les écrans, `main` devient la référence une fois la PR mergée. Pour les scènes de la galerie de [`screens.md`](screens.md#version-française) : `python tools/rendu/maj_references.py --run <id du run>` remplace `docs/images/rendu/` ; relire les images, puis committer.
- Un nouvel écran : une entrée dans `tools/rendu/ecrans.py` (appuis aux coordonnées des captures, paysage 1280×720). « Une capture identique à … » dans le run veut dire qu'un appui est tombé à côté.
- Mise au point d'un écran : *Actions → Rendu hors tablette → Run workflow*, champ « seulement » (noms d'écrans) et « langues » (par ex. `["de"]`).
- Il ne tourne que sous Linux ou macOS (plateforme `host`) : sous Windows, passer par la CI (*Actions → Rendu hors tablette → Run workflow*).
- Il montre la mise en page, pas l'appareil : ni tactile, ni son, ni assistant vocal, ni temps de rendu ou mémoire.

## Marquer un point d'observation dans le code

Utiliser le tag `[AI-DEBUG]` (voir [`Tab5/README.md`](../Tab5/README.md)) en commentaire quand on trouve un bon point d'observation en cours d'investigation, même sans corriger le problème sous-jacent dans la même session.

## Après un plantage ou une coupure Wi-Fi : le journal des démarrages

Depuis le 26/09/2026, l'appareil tient son propre journal (`Tab5/ecran/tab5_journal.cpp`). Un plantage ou une panne du lien avec le co-processeur Wi-Fi (ESP32-C6) s'analyse donc après coup, sans câble USB branché au moment de l'incident :

- **Ce qui est gardé :** toutes les erreurs (d'ESPHome et d'ESP-IDF, dont le pilote ESP-Hosted du C6, étiquette `esp-idf`), plus les avertissements écrits pendant que Home Assistant n'est pas connecté (démarrage avant HA, Wi-Fi tombé). Une ligne répétée d'affilée est comptée, pas recopiée. Le rapport de plantage d'ESPHome (étiquette `esp32.crash` : PC, adresse fautive et pile d'appels des deux cœurs) est écrit par le logger avant que le déclencheur `on_message` du journal existe : il n'y entrait donc jamais seul. Depuis le 06/10/2026, le journal le lit (`esp32::crash_handler_has_data()`), le rejoue dans ses lignes quand la raison du reset est un plantage, et l'efface une fois le journal arrivé à HA (comme ESPHome après un abonnement aux logs).
- **Les avertissements d'ESP-IDF ne sont pas dans le firmware :** ESPHome compile ESP-IDF avec son niveau de journal à ERROR par défaut (`esp32: framework: log_level`, d'où `CONFIG_LOG_MAXIMUM_LEVEL=1`) : les lignes `ESP_LOGW` et `ESP_LOGI` d'ESP-IDF et d'ESP-Hosted sont retirées à la compilation et ne se voient ni dans le journal ni sur le port série. Le contrôle de version du C6 par ESP-Hosted en fait partie (voir [Matériel](hardware.md#esp32-c6-co-processeur)). L'absence d'un tel avertissement ne prouve donc rien. Les erreurs restent, dont un délai RPC dépassé d'ESP-Hosted (`Response not received for …`, ou `Error reported: Response Timeout` pour une requête asynchrone). Pour voir les avertissements, il faudrait un build d'essai avec `log_level: WARN` sous `esp32: framework:` (pas essayé). Trouvé en répondant à la [discussion #369](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/369).
- **Où :** 32 lignes en RAM `.noinit`, qui survit aux redémarrages logiciels, aux plantages, aux chiens de garde et au reset par l'USB, pas à une coupure de courant. Quand le Wi-Fi manque depuis 90 s, une copie part en NVS (au plus toutes les 15 min, seulement s'il y a du nouveau), relue au démarrage suivant si la RAM a été perdue.
- **Comment il arrive :** à chaque connexion de HA, l'événement `esphome.tab5_journal` part pour un reset anormal ou un rapport de plantage, un Wi-Fi absent 90 s ou plus, un démarrage qui n'a jamais joint HA, une erreur après la connexion à HA, ou HA joint plus de 90 s après le démarrage. Les lignes d'avant la première connexion à HA ne sont que du contexte (le lien ESP-Hosted est normalement « not yet up » une fois à chaque démarrage, 7 à 12 s après) : un démarrage normal n'envoie rien. La garde (e) de `HomeAssistant_Config/packages/tab5_health.yaml` en fait une notification persistante (et une notification sur le téléphone si `grave` : reset anormal, plantage, ou Wi-Fi absent 90 s). Les événements restent dans la table `events` du recorder.
- **Raison de chaque démarrage :** l'entité `Tab5 Raison du redémarrage` (`Software Reset`, `Exception`, `Task Watchdog`, `Brownout`, `Power On`…), une ligne par démarrage dans l'historique HA.
- **Chien de garde sans rapport de plantage = le bouton d'alimentation (depuis le 06/10/2026) :** un appui court sur le bouton d'alimentation redémarre la tablette avec `ESP_RST_WDT`, sans rapport `esp32.crash`. Le journal l'appelle « bouton d'alimentation ou chien de garde RTC (rst 0x..) » et n'envoie rien ; l'entité affiche `Power button or RTC watchdog (rst 0x..)` au lieu du texte d'ESPHome, qui pour cette raison répète la source du dernier redémarrage *demandé* (`Reboot request from esphome.ota` : ESPHome ne l'efface jamais). `rst 0x..` est le code de reset du ROM (`esp_rom_get_reset_reason(0)` ; sur l'ESP32-P4, `ESP_RST_WDT` regroupe 0x07 et 0x0B, chiens de garde des groupes de timers, 0x09, 0x0D et 0x10, chien de garde RTC, 0x12, super chien de garde). La même raison *avec* un rapport de plantage reste un plantage et alerte toujours (`Crash, other watchdogs (rst 0x..)`). Ce qui n'alerte plus : un démarrage bloqué plus de 9 s (chien de garde RTC du bootloader) et un chien de garde de timer qui réinitialise sans passer par la panique ; les deux restent visibles dans l'historique de l'entité. Raisonnement et sources dans l'`[AI-WARNING]` de `Tab5/ecran/tab5_journal.cpp`.
- **Décoder une pile d'appels :** les adresses appartiennent au build qui a planté. Pour un build local, garder son ELF (`H:/.esphome_data/build/tab5-ha-hmi/build/tab5-ha-hmi.elf`, comparer le `config_hash`) ; pour un firmware publié, prendre l'artefact `elf-<révision>` de son run « Publication » (gardé 90 jours), ou le recompiler par *Run workflow*, tag et `elf_seulement` (le run vérifie que le code est celui publié). Puis `riscv32-esp-elf-addr2line -pfiaC -e <elf> <adresses>`, ou `tools/capture_serie.py --relire … --elf …`. Si le rapport dit *captured by a different firmware build*, prendre l'ELF de ce build-là.
- **Écarté volontairement :** le core dump d'ESP-IDF en flash. Il lui faut une partition `coredump` ; la table actuelle (`partitions.csv` : 2 × 7,75 Mo d'application + 448 Ko de NVS) remplit les 16 Mo, et une table de partitions ne s'écrit que par un flash USB, jamais par OTA. Le journal couvre le cas du C6, qui n'est pas un plantage du P4.

## Capturer un plantage sur le port série

Quand le journal dit « plantage (exception) » sans rapport (vu la première fois à la fin d'une mise à jour depuis HA, 3.0.0-rc.1 → rc.2, le 27/09/2026), la sortie de panique d'ESP-IDF est partie au seul endroit où elle s'écrit : la console USB (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`), juste avant le redémarrage.

1. Tablette branchée au PC en USB-C. `python tools/capture_serie.py` liste les appareils Espressif et refuse de deviner : donner `--mac` (la MAC de la tablette est son numéro de série USB) ou `--port`.
2. `python tools/capture_serie.py --mac <MAC>` : écoute jusqu'à 10 min sans réinitialiser la tablette (DTR et RTS coupés avant l'ouverture), écrit `capture-serie-<date>.log` au fil de l'eau, s'arrête 60 s après un redémarrage.
3. Provoquer l'événement (par exemple *Installer* sur l'entité « Firmware » dans HA).
4. Lire le résumé : bloc de panique, redémarrages, rapport de plantage d'ESPHome, adresses. Décoder avec l'ELF du firmware **qui a planté** (celui d'avant la mise à jour) : `python tools/capture_serie.py --relire capture-serie-….log --elf tab5-ha-hmi-st7123.elf`.

Ne jamais ouvrir un autre appareil Espressif branché au même PC. Fermer la fenêtre d'ESP Web Tools (Chrome) redémarre la tablette une fois.

## Diagnostiquer un échec silencieux d'automation côté Home Assistant

Si une automation HA ne semble rien faire — pas d'erreur, pas de résultat attendu — et qu'on soupçonne qu'une étape échoue à la validation ou qu'une condition est silencieusement fausse, **insérer un marqueur de debug directement dans la vraie automation** plutôt que d'essayer de reproduire sa logique dans un script de test isolé.

Concrètement : ajouter un appel `input_text.set_value` (ou effet de bord similaire, visible et peu coûteux) comme étape supplémentaire au point à vérifier, en utilisant la vraie automation, le vrai trigger, les vrais états d'entités — puis déclencher pour de vrai et relire la valeur.

Une reproduction isolée peut diverger de la vraie automation exactement sur le point qui cause le bug (contexte Jinja différent, disponibilité d'entité différente au moment du trigger, condition qui échoue seulement sur un sous-ensemble d'exécutions) — une repro qui "marche" en isolation ne dit rien sur le pourquoi de l'échec réel. C'est exactement comme ça qu'a été trouvé le bug `now()` sans `strftime` sur le calendrier (voir `troubleshooting.md`) : une repro isolée aurait dû deviner le contexte de template exact, alors que le marqueur dans la vraie automation a fait apparaître l'étape en échec immédiatement.

Retirer ou commenter le marqueur une fois le vrai correctif confirmé — ne pas laisser d'étapes de debug actives en production sans raison.
