# Inventaire des configurations YAML et des suites de tests

> **[AI-CONTEXT] RÔLE DE CE FICHIER**
> Ce document consigne l'emplacement précis des fichiers de configuration YAML
> (ESPHome et Home Assistant) ainsi que l'arborescence des suites de tests
> unitaires et d'intégration existantes dans le dépôt `00ProjetTab/`.
> Il sert de référence pour toute tâche d'inspection, de maintenance ou
> d'extension du projet. Les chemins sont relatifs à la racine du dépôt
> `H:\AuxFilsDesIdees\00ProjetTab`.

`Généré le 2026-08-01`, **chiffres revérifiés sur `main` le 2026-09-29** (54 composants UI dont 28 inclus par `tab5-lvgl.yaml`, 21 services ; tenus par `tests/test_doc_comptes.py`) · Sources vérifiées directement dans l'arborescence du dépôt.

---

## 1. Configurations YAML ESPHome

### 1.1 Point d'entrée

| Fichier | Emplacement | Rôle |
|---|---|---|
| `tab5-ha-hmi.yaml` | Racine du dépôt | Point d'entrée ESPHome : `substitutions:`, `packages:`, `esphome: includes:`, séquence `on_boot:`. |

### 1.2 Packages ESPHome (`Tab5/*.yaml`)

| Fichier | Emplacement | Rôle |
|---|---|---|
| `tab5-ui-tokens.yaml` | `Tab5/` | Tokens dimensionnels (modal_card_w/h, modal_body_y). |
| `tab5-hardware.yaml` | `Tab5/` | Bas niveau : display MIPI-DSI, tactile ST7123, DAC/ADC audio, media_player, expander GPIO, esp32_hosted, OTA. |
| `tab5-sensors-diagnostics.yaml` | `Tab5/` | WiFi, alimentation GPIO, statut API HA, uptime, RAM, loop time, horloge SNTP. |
| `tab5-sensors-domotique.yaml` | `Tab5/` | Miroirs d'entités HA : plantes, lumières, PC, températures, batterie, audio. |
| `tab5-api-logic.yaml` | `Tab5/` | Contrat API HA↔Tab5 : bloc `api: services:` (21 services ; `tab5_maj_pluie_1h_bulk` a remplacé `tab5_maj_pluie_1h` le 08/09/2026). |
| `tab5-styles.yaml` | `Tab5/` | Styles de verre partagés par les thèmes : couleurs des jeux (`color:`), déclarations `font:`, `lvgl: style_definitions:` (lisent la palette `UIColor`, ADR-0029). |
| `tab5-globals.yaml` | `Tab5/` | État partagé (`globals:`) + rotateur carte centrale (interval 8s). |
| `tab5-scripts.yaml` | `Tab5/` | Scripts transverses : registre des modales, volume, debounces, rotateur, volet, popup lumière, retour à l'accueil. |
| `tab5-arcade.yaml` | `Tab5/` | Scripts des jeux : fermeture globale, ouverture des 8 consoles, page arcade (lot 8c). |
| `tab5-calendar.yaml` | `Tab5/` | Scripts du popup calendrier (lot 8c). |
| `tab5-assist.yaml` | `Tab5/` | Assistant vocal : mots de réveil, pipeline, image de la réponse, scripts vocaux et popup Assistant (lot 8c). |
| `tab5-lvgl.yaml` | `Tab5/` | Layout complet : page unique 1280×720, swipe prévisions, console, popups. |
| `tab5-imu.yaml` | `Tab5/` | BMI270 IMU : `motion:`, poll adaptatif 10/30Hz, tap-to-wake. |
| `tab5-ha-controls.yaml` | `Tab5/` | Number volume, text_sensor écran courant, select aller-à, button recharger calendrier. |
| `tab5-alarm.yaml` | `Tab5/` | Réveil : rtttl, ~20 entités HA, machine d'état sonnerie, tick 1s. |
| `publication-*.yaml` | `Tab5/` | Publication (lot 6c, ADR-0022), choisie par `tab5_publication` : `locale` (défaut, vide), `stable` / `beta` (CI de publication) → `publication-commune.yaml` : OTA `http_request` + entité de mise à jour « Firmware » sur le manifeste de GitHub Pages. |

### 1.3 Composants UI (`Tab5/ui_components/*.yaml`)

50 fichiers, dont 27 inclus directement par `tab5-lvgl.yaml` (recompté le 01/10/2026, après les gabarits de l'horloge et de la pluie). Exemples :

| Fichier | Emplacement | Rôle |
|---|---|---|
| `climate_card.yaml` | `Tab5/ui_components/` | Carte clim compacte. |
| `climate_popup.yaml` | `Tab5/ui_components/` | Popup clim plein écran. |
| `forecast_daily.yaml` | `Tab5/ui_components/` | 5 cartes prévisions journalières. |
| `forecast_hourly.yaml` | `Tab5/ui_components/` | 5 cartes prévisions horaires. |
| `switches_card.yaml` | `Tab5/ui_components/` | Cartes switches (PC, volet, lumières). |
| `console_sys.yaml` | `Tab5/ui_components/` | Console Système en 4 cartes. |
| `light_popup.yaml` | `Tab5/ui_components/` | Popup contrôle lumière. |
| `tv_remote_popup.yaml` | `Tab5/ui_components/` | Popup télécommande TV Samsung. |
| `rangee.yaml` | `Tab5/ui_components/` | Rangée sous l'horloge (ADR-0031) : ligne des plantes (`moisture_sensors.yaml`, 4 slots humidité) et lignes de capteurs. |
| `pots_popup.yaml` | `Tab5/ui_components/` | Popup détails plantes. |
| `calendar_popup.yaml` | `Tab5/ui_components/` | Popup calendrier mensuel. |
| `assistant_popup.yaml` | `Tab5/ui_components/` | Popup assistant vocal. |
| `modal_scrim.yaml` | `Tab5/ui_components/` | Voile d'assombrissement (chrome partagé). |
| `modal_header.yaml` | `Tab5/ui_components/` | Barre de titre 52px (chrome partagé). |
| `game_selector.yaml` | `Tab5/ui_components/` | Sélecteur Arcade (grille 4×2). |
| `marble_game.yaml` … `draughts_game.yaml` | `Tab5/ui_components/` | 8 consoles de jeu (pages LVGL dédiées). |

### 1.4 Fichiers de substitution et secrets

| Fichier | Emplacement | Statut |
|---|---|---|
| `Tab5/user_entities.yaml` | `Tab5/` | **Gitignoré** — entités HA réelles d'Axel. |
| `Tab5/user_entities.example.yaml` | `Tab5/` | Modèle public des substitutions. |
| `tab5_signature.pem` | Racine | **Gitignoré** — clé privée RSA-3072 qui signe le firmware (3.0, ADR-0020) ; autre chemin : `tab5_cle_signature` dans `Tab5/user_entities.yaml`. Remplace `secrets.yaml`, que le firmware ne lit plus (seul `tools/migrer_vers_3.py` y prend l'ancienne clé API, une fois). |
| `HomeAssistant_Config/placeholders.yaml` | `HomeAssistant_Config/` | **Gitignoré** — identifiants HA réels de l'auteur (`nom: valeur`), cherchés par `render_ha_config.py --check` ; modèle suivi `placeholders.example.yaml`. |
| `HomeAssistant_Config/rendered/` | `HomeAssistant_Config/` | **Gitignoré** — copie des fichiers publics produite par `tools/render_ha_config.py` (plus de rendu depuis l'ADR-0024). |

---

## 2. Configurations YAML Home Assistant (`HomeAssistant_Config/`)

### 2.1 Production = packages rendus

Depuis le 26/09/2026, il n'y a plus de fichiers de production privés : le HA de l'auteur fait tourner les packages ci-dessous, tels quels depuis le 28/09/2026 (ADR-0024 : plus de placeholder, les valeurs de la maison se choisissent dans HA). Les releases les joignent dans `tab5_home_assistant.zip` (`tools/publication/archive_ha.py`).

### 2.2 Fichiers publics (trackés)

| Fichier | Emplacement | Rôle |
|---|---|---|
| `packages/tab5_push.yaml` | `HomeAssistant_Config/packages/` | Package principal : automatisations de poussée, scripts `tab5_push_*`, scripts appelés par le Tab5, capteur de pluie, garde-fou `is_primary_active`. |
| `packages/tab5_evenements.yaml` | `HomeAssistant_Config/packages/` | Demandes de la tablette (ADR-0025) : une automatisation traduit les événements `esphome.tab5_*` en une liste blanche d'actions (annonces, calendrier, alertes lues, voix, pipeline, console système), pour un appareil de modèle `tab5-ha-hmi` seulement. Remplace l'option « actions HA ». |
| `packages/tab5_alerts.yaml` | `HomeAssistant_Config/packages/` | Package alertes HA. |
| `packages/tab5_calendar.yaml` | `HomeAssistant_Config/packages/` | Package calendrier HA. |
| `custom_templates/tab5_calendar.jinja` | `HomeAssistant_Config/custom_templates/` | Macros Jinja du calendrier (importées par `tab5_calendar.yaml`). |
| `custom_templates/tab5_dashboard.jinja` | `HomeAssistant_Config/custom_templates/` | Macro `tab5_dashboard()` (04/10/2026) : écrit le tableau de bord HA de la tablette (vues Tab5, Réglages, Santé) avec les entités de la maison, trouvées par le modèle de l'appareil ; rendue dans Outils de développement → Modèle (`docs/installation.md`, étape 7). |
| `packages/tab5_health.yaml` | `HomeAssistant_Config/packages/` | Package santé HA. |
| `packages/tab5_reveil.yaml` | `HomeAssistant_Config/packages/` | Package réveil HA. |
| `packages/tab5_tv.yaml` | `HomeAssistant_Config/packages/` | Package TV HA (TV et adresse choisies dans HA, plus de `!secret`). |
| `packages/tab5_reglages.yaml` | `HomeAssistant_Config/packages/` | Réglages choisis dans HA (listes « Tab5 · … » : agendas, téléphone, présence), tablette détectée par son modèle, miroirs pour les déclencheurs. |
| `optionnel/volet_serre_tracking.yaml` | `HomeAssistant_Config/optionnel/` | Package volet **optionnel** (pas installé par défaut) : helpers, script, synchro écran, suivi des commandes directes ; volet choisi dans HA. |
| `snippets/tab5_alerts_dismissed_input_text.yaml` | `HomeAssistant_Config/snippets/` | Snippet input_text alertes. |
| `snippets/tab5_assist_reponse_exemple.yaml` | `HomeAssistant_Config/snippets/` | Exemple (non chargé) : réponse du moteur vers le popup Assistant. |

---

## 3. Suites de tests unitaires et d'intégration

### 3.1 Tests pytest (`tests/`)

| Fichier | Emplacement | Type | Cible |
|---|---|---|---|
| `test_verifier_secrets_config.py` | `tests/` | Unitaire | Détection de secrets en clair dans les fichiers suivis (`tools/verifier_secrets_config.py`) : valeurs factices, pragma, `git ls-files`, `secrets.yaml` suivi. |
| `test_rendu_ecrans.py` | `tests/` | Contenu | Plan des écrans du rendu (`tools/rendu/ecrans.py`) : noms uniques, appuis dans l'écran, options du select « Aller à l'écran » et actions de l'API qui existent. |
| `test_rendu_host.py` | `tests/` | Contenu | Rendu hors tablette (ADR-0021) : lambdas de l'`on_boot` copiées telles quelles de `tab5-ha-hmi.yaml`, mêmes sources C++, chaque package repris ou déclaré matériel, bouchons absents du firmware. |
| `test_horloge.py` | `tests/` | Contenu | Géométrie de l'horloge à rouleau, posée dans `Tab5/tab5-lvgl.yaml` seulement : recalculée depuis les métriques de Roboto 700 (taille lue dans `tab5-styles.yaml`), chaque cadre contient toute l'encre des chiffres, a la largeur d'un chiffre, HH:MM centré dans la tuile, « : » à la hauteur des chiffres, cadres au-dessus de la date ; les 4 rouleaux viennent du gabarit `ui_components/clock_roller.yaml` (déplié comme ESPHome). |
| `test_sanitizers.py` | `tests/` | Unitaire + contenu | Job sanitizers (lot B de l'audit du 30/09/2026) : lecture des rapports ASan/UBSan dans le journal de la tablette (extraits réels, doublons, lecture au fur et à mesure, codes de sortie et témoin), variante de compilation, cas ciblés conformes au contrat du firmware (sinon la démo les ignorerait sans bruit), et le workflow garde son témoin positif sans `log_path`. |
| `test_sans_secret.py` | `tests/` | Contenu + unitaire | Firmware sans secret (ADR-0020) : aucun `!secret`, clé API fournie par HA, fenêtre d'appairage, OTA signée, Wi-Fi sans identifiants, fuseau de HA, CI sans secrets factices ; clé trouvée dans HA par `tools/tab5_cle_api.py`, ancienne clé lue par `tools/migrer_vers_3.py`. |
| `test_doc_broches.py` | `tests/` | Contenu | Tableau des broches de `docs/hardware.md` (anglais et français) contre le YAML du firmware : chaque ligne donne la broche du YAML (GPIO ou broche d'expandeur PI4IOE5V6408 avec son adresse), et aucune broche du YAML ne manque. Remplace l'image `gpio_pinout_table.png`, fausse. |
| `test_doc_comptes.py` | `tests/` | Contenu | Comptes écrits dans la doc contre le code : liste des packages de `docs/architecture.md` (celle de `tab5-ha-hmi.yaml`, dans l'ordre), leur nombre dans ses « Key design decisions » (en toutes lettres, anglais et français) et la cartographie ; nombre d'actions de `Tab5/tab5-api-logic.yaml` dans la cartographie et table complète de `Tab5/README.md` ; nombre d'ADR de `docs/decisions/` dans le README et la cartographie ; un nœud et une arête `packages:` par package dans le schéma Mermaid de la cartographie, une section par package (EN et FR) dans `docs/architecture.md`, les fichiers de plus de 500 lignes nommés par `docs/architecture.md`, le nombre de `ui_components/*.yaml` et de ceux inclus directement par `tab5-lvgl.yaml`. |
| `test_contrat.py` | `tests/` | Contenu | Contrat HA ↔ tablette (lot D, 30/09/2026) : clés de chaque appel d'une action de la tablette (packages, blueprint, snippets, rendu hors tablette) égales aux `variables:` de `Tab5/tab5-api-logic.yaml` ; chaque champ `trigger.event.data.*` lu pour un événement `esphome.tab5_*` émis par le firmware pour cet événement, et chaque champ émis lu (sauf liste blanche) ; lecture du contrat par `demo_pusher.py --dry-run` égale à celle de PyYAML. |
| `test_calendrier_prefetch.py` | `tests/` | Contenu | Pré-fetch du calendrier (01/10/2026) : `tab5_cal_prefetch_boot` sans paramètre (appelé par `on_boot` et `status_ha`) délègue à `tab5_cal_prefetch` (`queued`, paramètre `force`), dont chaque demande de mois est sautée si le mois a été reçu il y a moins de `CAL_PREFETCH_FRESH_MS` ; le bouton « Recharger le calendrier » force ; fraîcheur entre 10 s et les 10 min du rendu. |
| `test_version.py` | `tests/` | Contenu | Version par défaut du firmware (`tab5-ha-hmi.yaml`) = dernière version publiée du CHANGELOG + « -dev » ; modèle Jinja de `binary_sensor.tab5_fichiers_ha_en_retard` rendu sur 13 cas (X.Y seulement), ses attributs et sa notification. |
| `test_publication.py` | `tests/` | Unitaire + contenu | Publication (ADR-0022) : `tools/publication/preparer.py` (binaires renommés par révision, manifeste contrôlé et réécrit, refus d'un manifeste ou d'un binaire inattendu), `pages.py` (canaux stable/bêta parmi les releases 3.x, site reconstruit depuis leurs fichiers) ; mêmes révisions partout (fichiers, matrice, page), clé du projet et jamais de clé jetable, ESPHome figé ≥ plancher, mise à jour seulement dans les firmwares publiés. |
| `test_capture_serie.py` | `tests/` | Unitaire | Capture d'un plantage sur le port série (`tools/capture_serie.py`) : bloc de panique RISC-V repéré jusqu'au redémarrage, adresses à décoder sans doublon, redémarrages comptés, rien de signalé pour un démarrage normal, jamais de port deviné entre deux appareils Espressif. |
| `test_meme_code.py` | `tests/` | Unitaire | Même code qu'une image publiée (`tools/publication/meme_code.py`) : signature SBv2 et heure de compilation ignorées, taille différente ou code déplacé refusés, image non signée refusée. |
| `test_alarme_popup.py` | `tests/` | Contenu | Popup du réveil, barre du bas : la ligne « prochain rendez-vous » passe par `texte_ha_coupe()` (une ligne, « … »), sans largeur fixe dans `alarm_popup.yaml`, et sa limite `kLargeurRdvSuivant` (`alarm_render.cpp`) s'arrête avant le bouton « Tester », recalculé depuis le YAML. |
| `test_geometrie_partagee.py` | `tests/` | Contenu | Géométrie écrite en YAML et en C++ tenue égale : jeton `${central_w}` (`tab5-ui-tokens.yaml`) = `kLargeurPanneauCentral` (`tab5_central.cpp`), plus de 1180 en clair ; en-têtes « Lun »…« Dim » de `calendar_popup.yaml` sur les colonnes `kCalColX0`/`kCalColPas`/`kCalColW` de `tab5_calendar.cpp`, grille centrée dans la carte modale. |
| `test_improv_serie.py` | `tests/` | Unitaire | Wi-Fi par Improv sur l'USB (`tools/improv_serie.py`, `migrer_vers_3.py --port`) : paquets conformes (en-tête, longueur, somme de contrôle, saut de ligne), lecture au milieu du journal, réglage réussi / réseau introuvable / tablette muette face à une fausse liaison série, mot de passe jamais affiché, `secrets.yaml` lu en YAML. |
| `test_render_ha_config.py` | `tests/` | Unitaire | Copie des fichiers HA publics, détection de fuite d'identifiants réels et de placeholder restant (`tools/render_ha_config.py`) ; aucun placeholder dans le dépôt. |
| `test_tableau_de_bord.py` | `tests/` | Contenu + rendu | Tableau de bord HA de la tablette (`custom_templates/tab5_dashboard.jinja`) : chaque entité cherchée existe dans le firmware et chaque entité du firmware a sa carte (sauf exceptions motivées), entités et automatisations de package citées définies ; rendu Jinja avec une fausse maison (préfixes mélangés, selects de HA dans une autre langue, sans package, sans tablette, deux tablettes) : YAML valide, chaque entité citée présente. |
| `test_installation_ha.py` | `tests/` | Unitaire + contenu | Job « installation dans un HA neuf » sans conteneur : `preparer_config.py` écrit une installation complète (ligne des packages, tous les packages, blueprint identique), ni placeholder ni `!secret` installés, optionnels seulement sur demande, chaque entité `…tab5_…` lue par un package définie par un package, entrées du blueprint et « Zones masquées » attendues, tablette virtuelle au nom de la vraie, mêmes chemins sur `main` et en PR ; fonctions pures de `verifier_installation.py` (clé, traces, journal de HA). |
| `test_site_doc.py` | `tests/` | Contenu + construction | Site de documentation (ADR-0030) : chaque fichier de `docs/` est dans `tools/site/menu.yml` ou écarté exprès (`HORS_SITE`), chaque page dans les deux langues, liens vers le dépôt et ancres de l'autre langue réécrits, puis le site `en/` et `fr/` est construit en mode strict (`tools/site/construire.py`) ; l'accueil est le README (titre, JSON-LD), la racine `web/index.html` renvoie vers `en/` ou `fr/`. |
| `test_notice.py` | `tests/` | Contenu | Notice d'utilisation (`docs/notice/`) : chaque appui long du YAML et le seul glissement décrits dans les deux langues, chaque fenêtre du rendu (`tools/rendu/ecrans.py`) montrée ou écartée avec sa raison, mêmes images dans les deux moitiés, aucune image citée absente ni orpheline, légende de l'accueil annoté = repères de `tools/site/images_notice.py`. |
| `test_guards.py` | `tests/` | Contenu | Joue les 8 garde-fous ci-dessous sur le C++/YAML réel (chrome modal, registre, règles de code, salles Marble, niveaux Lode, niveaux d'Arcanoïde, questions de Trial Poursuite, comptes de la cartographie). |
| `__init__.py` | `tests/` | — | Marqueur de package. |

### 3.2 Tests moteurs de jeux (`tools/`)

| Fichier | Emplacement | Type | Cible |
|---|---|---|---|
| `test_go_engine.py` | `tools/` | Unitaire (miroir Python) | Règles Go : capture, suicide, ko, territoire, score. |
| `test_chess_perft.py` | `tools/` | Unitaire (miroir Python) | Générateur d'échecs contre la suite perft standard. |
| `test_draughts_engine.py` | `tools/` | Unitaire (miroir Python) | Générateur de dames (10×10 et 8×8) contre les perft de référence + règles (prise majoritaire, dame volante, promotion). |
| `test_go_engine.cpp` | `tools/` | Unitaire (C++ hôte) | Même suite compilée contre le vrai `go_engine.cpp` — g++ en CI (job `python`). |
| `test_alarm_clock.cpp` | `tools/` | Unitaire (C++ hôte) | Moteur du réveil réel (`alarm_clock.cpp` + `tab5_core.cpp`) : 13 scénarios, horloge simulée, fuseau Europe/Paris et changements d'heure — g++ en CI (job `python`). |

### 3.3 Outils de validation (intégration)

| Fichier | Emplacement | Type | Rôle |
|---|---|---|---|
| `tools/demo/demo_pusher.py` | `tools/demo/` | Intégration (dry-run) | Valide chaque payload push contre le contrat firmware. |
| `tools/installation_ha/` | `tools/installation_ha/` | Intégration (CI) | Installation dans un Home Assistant neuf, sans matériel (`.github/workflows/installation-ha.yml`) : `preparer_config.py` (dossier `config/` d'une installation neuve : `configuration.yaml`, fichiers de l'archive `tab5_home_assistant.zip` tels quels, `donnees_test.yaml`), `verifier_installation.py` (ordre « Sans compiler » : onboarding, sources choisies dans les listes « Tab5 · … », ajout ESPHome de la tablette virtuelle, clé API, option « actions HA » laissée décochée (ADR-0025), automatisation du blueprint, redémarrage ; traces, zones, captures demandées par HA, demandes de la tablette par événements, tableau de bord de la tablette rendu par HA et enregistré, journal de HA) ; `captures_ha.py` (captures de l'interface de HA du guide d'installation, anglais et français, par Playwright). |
| `tools/sanitizers/` | `tools/sanitizers/` | Intégration (CI) | Tablette virtuelle sous ASan + UBSan (`.github/workflows/sanitizers.yml`) : `variante.py` + `pio_drapeaux.py` (compilation instrumentée), `fuzz_services.py` (19 services), `cibles_ub.py` (conversions hors bornes, fenêtres ouvertes), `rapports.py` (rapports lus dans le journal de la tablette), `temoin.cpp` (témoin positif). |
| `tools/verifier_secrets_config.py` | `tools/` | Outil | Analyse les fichiers suivis par git (`.yaml`, `.yml`, `.example`, `.jinja`, `.md`) pour détecter des secrets en clair. |
| `tools/render_ha_config.py` | `tools/` | Outil | Copie les fichiers HA publics dans `rendered/` ; `--check` = garde-fou de fuite (valeurs réelles, placeholders). |
| `tools/publication/archive_ha.py` | `tools/publication/` | Outil (CI) | Archive `tab5_home_assistant.zip` d'une release (packages, custom_templates, blueprint, optionnels), reproductible. |
| `tools/check_tab5_modal_chrome.py` | `tools/` | Garde-fou | ADR-0009 : chrome modal partagé sur chaque popup (rapatrié du workspace le 06/09/2026). |
| `tools/check_marble_rooms.py` | `tools/` | Garde-fou | Les 6 salles de « Fil d'Or » lues dans `marble_game.cpp` restent traversables (numpy). |
| `tools/check_lode_levels.py` | `tools/` | Garde-fou | Les 10 niveaux de « Coureur d'Or » lus dans `lode_game.cpp` restent jouables. |
| `tools/check_arkanoid_levels.py` | `tools/` | Garde-fou | Les 8 niveaux d'« Arcanoïde » lus dans `arkanoid_game.cpp` : rangées complètes, valeurs connues, aucune brique destructible emmurée, `LEVELS`/`LEVEL_NAMES`/fin de partie cohérents. |
| `tools/check_trivia_questions.py` | `tools/` | Garde-fou | La banque de « Trial Poursuite » (`trivia_questions.h`) : autant d'entrées que chaque `#define`, catégorie et difficulté valides, ni texte vide, ni leurre égal à la réponse, ni question en double. |
| `tools/check_tab5_registry.py` | `tools/` | Garde-fou | ADR-0013 : chaque `*_game.h` figure dans `GameRegistry::kGames`, aucune liste de jeux recopiée dans un YAML. |
| `tools/check_tab5_code_rules.py` | `tools/` | Garde-fou | Règles de code : `snprintf` partout, aucun `lv_*` dans le contrat API, aucun global orphelin, aucune entité HA en dur, glyphes de la date (`roboto_45`), icônes MDI couvertes par la police de leur widget sans glyphe mort (règle 7). |
| `tools/cartographie_counts.py` | `tools/` | Garde-fou | Comptes de lignes de `CARTOGRAPHIE_TAB5.md` à 20 % près ; `--write` les recalcule. |
| `.pre-commit-config.yaml` | Racine | Config | yamllint (dont `*.yaml.example`), BOM, secrets, fuite d'identifiants HA — rejoué par la CI. |
| `pyproject.toml` | Racine | Config | `testpaths = tests, tools` : `pytest` nu ne ramasse plus `archives/`. |
| `requirements-dev.txt` | Racine | Config | Dépendances des outils (pytest, numpy, aioesphomeapi, fonttools, pre-commit, yamllint) — pas le firmware. |

### 3.4 Commandes de lancement

```bash
# Tous les tests (tests/ + moteurs de jeux sous tools/ — cf. pyproject.toml)
python -m pytest

# Tests moteurs de jeux (miroirs Python)
python tools/test_go_engine.py
python tools/test_chess_perft.py
python tools/test_draughts_engine.py

# Garde-fous rejoués par la CI
pre-commit run --all-files
python tools/cartographie_counts.py --write   # après un ajout/retrait de lignes notable

# Validation des payloads push (dry-run, sans matériel)
python tools/demo/demo_pusher.py --dry-run
```

---

## 4. Résumé de l'arborescence des tests

```
00ProjetTab/
├── tests/
│   ├── __init__.py
│   ├── test_guards.py
│   ├── test_notice.py
│   ├── test_render_ha_config.py
│   ├── test_site_doc.py
│   └── test_verifier_secrets_config.py
├── tools/
│   ├── demo/
│   │   ├── demo_pusher.py
│   │   ├── requirements.txt
│   │   └── scenarios.py
│   ├── installation_ha/   (job « installation dans un HA neuf »)
│   │   ├── captures_ha.py
│   │   ├── configuration.yaml
│   │   ├── donnees_test.yaml
│   │   ├── preparer_config.py
│   │   └── verifier_installation.py
│   ├── test_go_engine.py
│   ├── test_go_engine.cpp
│   ├── test_alarm_clock.cpp
│   ├── test_chess_perft.py
│   ├── test_draughts_engine.py
│   ├── check_arkanoid_levels.py
│   ├── check_lode_levels.py
│   ├── check_marble_rooms.py
│   ├── check_tab5_code_rules.py
│   ├── check_tab5_modal_chrome.py
│   ├── check_tab5_registry.py
│   ├── check_trivia_questions.py
│   ├── cartographie_counts.py
│   ├── make_chess_font.py
│   ├── render_ha_config.py
│   └── verifier_secrets_config.py
└── .github/workflows/
    ├── esphome-tab5.yml   (CI : jobs python + build + build-min, voir § 5)
    ├── installation-ha.yml   (installation dans un HA neuf, voir § 5)
    └── sanitizers.yml   (tablette virtuelle sous ASan + UBSan, voir § 5)
```

---

## 5. Notes importantes

- **Pas de suite de tests unitaires pour la HMI** : la logique LVGL (`tab5_*.cpp`) n'a pas de tests hôte. Seuls les moteurs de jeux (Go, échecs, dames) disposent de tests exécutables sur PC.
- **Les tests Go/échecs/dames sont des miroirs Python** du C++ : toute modification du C++ doit être reflétée dans le miroir Python, sinon le test ne prouve plus rien. Exception : `test_go_engine.cpp` compile le vrai moteur Go (g++, en CI).
- **CI GitHub Actions** (`.github/workflows/esphome-tab5.yml`, PR + push sur `main`) : job `python` (pre-commit, `pytest`, moteur Go C++, dry-run démo) ; job `build` (secrets factices + `esphome/build-action@v8.1.0`, image `latest` = canari amont voulu, ADR-0016, ccache conservé entre runs) seulement si `tab5-ha-hmi.yaml`, `Tab5/` (hors `.md`) ou le workflow changent ; job `build-min`, même compilation avec la version plancher lue dans `min_version:` (26/09/2026). `python`, `build` et `build-min` sont des checks requis de `main` ; `build` reste présent et passe en « skipped » sinon. Artefact `tab5-firmware` publié sur `main`.
- **Installation dans un HA neuf** (`.github/workflows/installation-ha.yml`, 28/09/2026, ~3 min, non requis) : Home Assistant figé en conteneur (`HA_IMAGE`) + la tablette virtuelle (`tab5-rendu-host.yaml` compilé sous le nom `tab5-ha-hmi`), installés comme par un nouvel utilisateur (`tools/installation_ha/`) : tous les packages rendus et `check_config`, puis l'ordre « Sans compiler » du guide (onboarding, ajout ESPHome sans l'option « actions HA », automatisation du blueprint) et un redémarrage de la tablette, puis deux demandes de la tablette de bout en bout (calendrier par le select « Aller à l'écran », « MAJ Écran » par le doigt virtuel) et un redémarrage de HA forgé par un autre appareil, qui doit être ignoré. Échoue si la clé API n'est pas donnée et gardée par HA, si la clé nulle ou le clair passent encore, si `esphome.tab5_connected` n'arrive pas après la clé, si une trace du blueprint ou de la poussée complète n'aboutit pas, si « Zones masquées » diffère, si la capture demandée par HA manque, si la clé ne survit pas au redémarrage, si une demande de la tablette n'aboutit pas, si HA a refusé une action de l'appareil (réparation « service_calls_not_allowed »), ou si le journal de HA a une erreur Tab5 après la connexion (hors « Not connected » pendant une déconnexion voulue, rapportée). Artefact `installation-ha` : deux captures (juste après l'automatisation du blueprint, puis après le redémarrage), journaux de HA et de la tablette. Sur les PR et `main` qui touchent HA, l'API ou la tablette virtuelle, et à la main. Ne teste pas l'interface de HA cliquée par un humain, la page de flashage ni le vrai matériel.
- **Sanitizers** (`.github/workflows/sanitizers.yml`, 01/10/2026, ~20 min, non requis) : la tablette virtuelle compilée avec AddressSanitizer et UndefinedBehaviorSanitizer (`tools/sanitizers/`), après un témoin positif ; fuzzing des 19 services, cas de conversions hors bornes fenêtre ouverte, tous les écrans. Échoue au premier rapport lu dans le journal de la tablette (UBSan écrit sur la sortie d'erreur et ignore `log_path`), si la tablette s'arrête ou si le programme a été compilé sans sanitizers. Artefact `sanitizers` : journaux, `fuzz.md`, `cibles.md`, reproducteurs.
- **Fichiers gitignorés** : `secrets.yaml` (2.x), `*.pem` / `*.key` (clé de signature), `tools/demo/cle_demo.txt`, `Tab5/user_entities.yaml`, `HomeAssistant_Config/placeholders.yaml`, `HomeAssistant_Config/rendered/`, les anciennes copies privées `automations_tab5.yaml` / `scripts_tab5.yaml` / `template_sensors_meteo_tab5.yaml` (obsolètes, gardées ignorées), `Tab5/tts_library*/`, `archives/`.
