# Inventaire des configurations YAML et des suites de tests

> **[AI-CONTEXT] RÔLE DE CE FICHIER**
> Ce document consigne l'emplacement précis des fichiers de configuration YAML
> (ESPHome et Home Assistant) ainsi que l'arborescence des suites de tests
> unitaires et d'intégration existantes dans le dépôt `00ProjetTab/`.
> Il sert de référence pour toute tâche d'inspection, de maintenance ou
> d'extension du projet. Les chemins sont relatifs à la racine du dépôt
> `H:\AuxFilsDesIdees\00ProjetTab`.

`Généré le 2026-08-01`, **chiffres revérifiés sur `main` le 2026-09-29**, liste des tests et des workflows complétée le 2026-10-08 (76 composants UI dont 37 inclus par `tab5-lvgl.yaml`, 23 services ; tenus par `tests/test_doc_comptes.py`) · Sources vérifiées directement dans l'arborescence du dépôt.

---

## 1. Configurations YAML ESPHome

### 1.1 Point d'entrée

| Fichier | Emplacement | Rôle |
|---|---|---|
| `tab5-ha-hmi.yaml` | Racine du dépôt | Point d'entrée ESPHome : `substitutions:`, `packages:`, `esphome: includes:`, séquence `on_boot:`. |

### 1.2 Packages ESPHome (`Tab5/paquets/*.yaml`)

| Fichier | Emplacement | Rôle |
|---|---|---|
| `tab5-ui-tokens.yaml` | `Tab5/` | Tokens dimensionnels (modal_card_w/h, modal_body_y). |
| `tab5-hardware.yaml` | `Tab5/` | Bas niveau : display MIPI-DSI, tactile ST7123, DAC/ADC audio, media_player, expander GPIO, esp32_hosted, OTA. |
| `tab5-sensors-diagnostics.yaml` | `Tab5/` | WiFi, alimentation GPIO, statut API HA, uptime, RAM, loop time, horloge SNTP. |
| `tab5-sensors-domotique.yaml` | `Tab5/` | Miroirs d'entités HA : plantes, lumières, PC, températures, batterie, audio. |
| `tab5-api-logic.yaml` | `Tab5/` | Contrat API HA↔Tab5 : bloc `api: services:` (23 services ; `tab5_maj_pluie_1h_bulk` a remplacé `tab5_maj_pluie_1h` le 08/09/2026). |
| `tab5-styles.yaml` | `Tab5/` | Styles de verre partagés par les thèmes : couleurs des jeux (`color:`), déclarations `font:`, `lvgl: style_definitions:` (lisent la palette `UIColor`, ADR-0029). |
| `tab5-globals.yaml` | `Tab5/` | État partagé (`globals:`) + rotateur carte centrale (interval 8s). |
| `tab5-scripts.yaml` | `Tab5/` | Scripts transverses : volume, debounces, rotateur, volet, popup lumière, retour à l'accueil. |
| `tab5-arcade.yaml` | `Tab5/` | Scripts des jeux : fermeture globale, ouverture des 8 consoles, page arcade (lot 8c). |
| `tab5-calendar.yaml` | `Tab5/` | Scripts du popup calendrier (lot 8c). |
| `tab5-assist.yaml` | `Tab5/` | Assistant vocal : mots de réveil, pipeline, image de la réponse, scripts vocaux et popup Assistant (lot 8c). |
| `tab5-lvgl.yaml` | `Tab5/` | Layout complet : page unique 1280×720, swipe prévisions, console, popups. |
| `tab5-imu.yaml` | `Tab5/` | BMI270 IMU : `motion:`, poll adaptatif 10/30Hz, tap-to-wake. |
| `tab5-navigation.yaml` | `Tab5/` | Navigation : registre des modales, `tab5_ecran_ouvrir`, select aller-à, text_sensor écran courant (08/10/2026). |
| `tab5-ha-controls.yaml` | `Tab5/` | Number volume, select langue, interrupteurs et extinction auto des Réglages, button recharger calendrier. |
| `tab5-alarm.yaml` | `Tab5/` | Réveil : rtttl, ~20 entités HA, machine d'état sonnerie, tick 1s. |
| `publication-*.yaml` | `Tab5/` | Publication (lot 6c, ADR-0022), choisie par `tab5_publication` : `locale` (défaut, vide), `stable` / `beta` (CI de publication) → `publication-commune.yaml` : OTA `http_request` + entité de mise à jour « Firmware » sur le manifeste de GitHub Pages. |

### 1.3 Composants UI (`Tab5/ui_components/*.yaml`)

59 fichiers, dont 30 inclus directement par `tab5-lvgl.yaml` (recompté le 06/10/2026, après les popups Réglages et Température). Exemples :

| Fichier | Emplacement | Rôle |
|---|---|---|
| `climate_card.yaml` | `Tab5/ui_components/` | Carte clim compacte. |
| `climate_popup.yaml` | `Tab5/ui_components/` | Popup clim plein écran. |
| `forecast_daily.yaml` | `Tab5/ui_components/` | 5 cartes prévisions journalières (gabarits `forecast_day_card.yaml`, `forecast_day_body.yaml`). |
| `forecast_hourly.yaml` | `Tab5/ui_components/` | 5 cartes prévisions horaires. |
| `switches_card.yaml` | `Tab5/ui_components/` | Les 5 cartes du mode HA (gabarit `switch_card.yaml`). |
| `console_sys.yaml` | `Tab5/ui_components/` | Page Système du popup Réglages (l'ancienne console système), 4 cartes. |
| `reglages_popup.yaml` | `Tab5/ui_components/` | Popup Réglages en 4 pages : écran, apparence, batterie, système. |
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
| `packages/tab5_push.yaml` | `HomeAssistant_Config/packages/` | Package principal : automatisations de poussée, scripts `tab5_push_*`, scripts appelés par le Tab5, capteur de pluie. |
| `packages/tab5_evenements.yaml` | `HomeAssistant_Config/packages/` | Demandes de la tablette (ADR-0025) : une automatisation traduit les événements `esphome.tab5_*` en une liste blanche d'actions (annonces, calendrier, alertes lues, voix, pipeline, console système), pour un appareil de modèle `tab5-ha-hmi` seulement. Remplace l'option « actions HA ». |
| `packages/tab5_alerts.yaml` | `HomeAssistant_Config/packages/` | Alertes de la carte centrale : capteur « Tab5 Alertes » (en cours, lues, historique), script `tab5_dismiss_alert`, sauvegarde des alertes lues, `sensor.tab5_unavailable_count`. |
| `custom_templates/tab5_alertes.jinja` | `HomeAssistant_Config/custom_templates/` | Logique des alertes (06/10/2026) : révisions, fin confirmée, alertes lues, historique (importée par `tab5_alerts.yaml`, règles en tête du fichier). |
| `packages/tab5_calendar.yaml` | `HomeAssistant_Config/packages/` | Package calendrier HA. |
| `custom_templates/tab5_calendar.jinja` | `HomeAssistant_Config/custom_templates/` | Macros Jinja du calendrier (importées par `tab5_calendar.yaml`). |
| `custom_templates/tab5_tablette.jinja` | `HomeAssistant_Config/custom_templates/` | Macros « la tablette » (08/10/2026, HA-7) : tablette connectée, garde d'origine, capteur API ; importées par les packages et le blueprint. |
| `custom_templates/tab5_meteo.jinja` | `HomeAssistant_Config/custom_templates/` | Macros de la météo effective (08/10/2026) : la source choisie, ou un repli tant qu'elle ne répond pas ; importées par « Tab5 Météo » (`tab5_meteo_sources.yaml`). |
| `custom_templates/tab5_dashboard.jinja` | `HomeAssistant_Config/custom_templates/` | Macro `tab5_dashboard()` (04/10/2026) : écrit le tableau de bord HA de la tablette (vues Tab5, Réglages, Santé) avec les entités de la maison, trouvées par le modèle de l'appareil ; rendue dans Outils de développement → Modèle (`docs/installation.md`, étape 7). |
| `packages/tab5_health.yaml` | `HomeAssistant_Config/packages/` | Package santé HA. |
| `packages/tab5_reveil.yaml` | `HomeAssistant_Config/packages/` | Package réveil HA. |
| `packages/tab5_tv.yaml` | `HomeAssistant_Config/packages/` | Package TV HA (TV et adresse choisies dans HA, plus de `!secret`). |
| `packages/tab5_reglages.yaml` | `HomeAssistant_Config/packages/` | Réglages choisis dans HA (listes « Tab5 · … » : agendas, téléphone, présence), tablette détectée par son modèle, miroirs pour les déclencheurs. |
| `optionnel/volet_serre_tracking.yaml` | `HomeAssistant_Config/optionnel/` | Package volet **optionnel** (pas installé par défaut) : helpers, script, synchro écran, suivi des commandes directes ; volet choisi dans HA. |
| `snippets/tab5_assist_reponse_exemple.yaml` | `HomeAssistant_Config/snippets/` | Exemple (non chargé) : réponse du moteur vers le popup Assistant. |

---

## 3. Suites de tests unitaires et d'intégration

### 3.1 Tests pytest (`tests/`)

| Fichier | Emplacement | Type | Cible |
|---|---|---|---|
| `test_verifier_secrets_config.py` | `tests/` | Unitaire | Détection de secrets en clair dans les fichiers suivis (`tools/verifier_secrets_config.py`) : valeurs factices, pragma, `git ls-files`, `secrets.yaml` suivi. |
| `test_rangement.py` | `tests/` | Garde-fou | Rangement de `Tab5/` (08/10/2026) : racine sans fichier du firmware, noms uniques (ESPHome copie les `includes:` à plat), `includes:` existants, `socle/` pur (ni ESPHome ni LVGL), paquets qui incluent `../ui_components/`. |
| `test_batterie.py` | `tests/` | Contenu | Batterie et chargeur : select « Tab5 Limite de charge » dans l'ordre de `LimiteCharge`, CHG_EN commandé par un seul interval, lectures de l'INA226, consommation et événement de batterie faible. |
| `test_rendu_ecrans.py` | `tests/` | Contenu | Plan des écrans du rendu (`tools/rendu/ecrans.py`) : noms uniques, appuis dans l'écran, options du select « Aller à l'écran » et actions de l'API qui existent. |
| `test_rendu_host.py` | `tests/` | Contenu | Rendu hors tablette (ADR-0021) : lambdas de l'`on_boot` copiées telles quelles de `tab5-ha-hmi.yaml`, mêmes sources C++, chaque package repris ou déclaré matériel, bouchons absents du firmware. |
| `test_horloge.py` | `tests/` | Contenu | Géométrie de l'horloge à rouleau, posée dans `Tab5/paquets/tab5-lvgl.yaml` seulement : recalculée depuis les métriques de Roboto 700 (taille lue dans `tab5-styles.yaml`), chaque cadre contient toute l'encre des chiffres, a la largeur d'un chiffre, HH:MM centré dans la tuile, « : » à la hauteur des chiffres, cadres au-dessus de la date ; les 4 rouleaux viennent du gabarit `ui_components/clock_roller.yaml` (déplié comme ESPHome). |
| `test_fuzz_parse.py` | `tests/` | Garde-fou | Lecture des payloads de HA (lot F) : chaque fonction de `tab5_parse.h` a ses cas dans `tools/test_parse.cpp` et un appel dans le harnais libFuzzer ; graines dans l'ordre du harnais et tirées du contrat, plus une graine par défaut corrigé après le lot F (`LIMITES`) ; le job `fuzz-parseurs` garde son témoin positif. |
| `test_sanitizers.py` | `tests/` | Unitaire + contenu | Job sanitizers (lot B de l'audit du 30/09/2026) : lecture des rapports ASan/UBSan dans le journal de la tablette (extraits réels, doublons, lecture au fur et à mesure, codes de sortie et témoin), variante de compilation, cas ciblés conformes au contrat du firmware (sinon la démo les ignorerait sans bruit), et le workflow garde son témoin positif sans `log_path`. |
| `test_sans_secret.py` | `tests/` | Contenu + unitaire | Firmware sans secret (ADR-0020) : aucun `!secret`, clé API fournie par HA, fenêtre d'appairage, OTA signée, Wi-Fi sans identifiants, fuseau de HA, CI sans secrets factices ; clé trouvée dans HA par `tools/tab5_cle_api.py`, ancienne clé lue par `tools/migrer_vers_3.py`. |
| `test_doc_broches.py` | `tests/` | Contenu | Tableau des broches de `docs/hardware.md` (anglais et français) contre le YAML du firmware : chaque ligne donne la broche du YAML (GPIO ou broche d'expandeur PI4IOE5V6408 avec son adresse), et aucune broche du YAML ne manque. Remplace l'image `gpio_pinout_table.png`, fausse. |
| `test_doc_comptes.py` | `tests/` | Contenu | Comptes écrits dans la doc contre le code : liste des packages de `docs/architecture.md` (celle de `tab5-ha-hmi.yaml`, dans l'ordre), leur nombre dans ses « Key design decisions » (en toutes lettres, anglais et français) et la cartographie ; nombre d'actions de `Tab5/paquets/tab5-api-logic.yaml` dans la cartographie et table complète de `Tab5/README.md` ; nombre d'ADR de `docs/decisions/` dans le README et la cartographie ; un nœud et une arête `packages:` par package dans le schéma Mermaid de la cartographie, une section par package (EN et FR) dans `docs/architecture.md`, les fichiers de plus de 500 lignes nommés par `docs/architecture.md`, le nombre de `ui_components/*.yaml` et de ceux inclus directement par `tab5-lvgl.yaml`. |
| `test_contrat.py` | `tests/` | Contenu | Contrat HA ↔ tablette (lot D, 30/09/2026) : clés de chaque appel d'une action de la tablette (packages, blueprint, snippets, rendu hors tablette) égales aux `variables:` de `Tab5/paquets/tab5-api-logic.yaml` ; chaque champ `trigger.event.data.*` lu pour un événement `esphome.tab5_*` émis par le firmware pour cet événement, et chaque champ émis lu (sauf liste blanche) ; lecture du contrat par `demo_pusher.py --dry-run` égale à celle de PyYAML. |
| `test_contrat_versions.py` | `tests/` | Contenu | Contrat HA ↔ tablette entre versions (lot E, 09/10/2026) : `contrat/contrat.yaml` égal au code ; version semver du contrat suffisante contre le dernier tag publié (lu dans son code) ; matrice N-1 firmware × fichiers HA contre le dernier tag et le dernier tag stable, les deux sens sans étiquette, l'ordre annoncé sinon ; étiquettes « **Contrat HA ↔ firmware** » du CHANGELOG confrontées à la matrice ; règles de semver et appels impossibles (événement non émis, garde `protocole`) sur des cas construits. Tags absents : sauté en local, échec en CI. |
| `test_calendrier_prefetch.py` | `tests/` | Contenu | Pré-fetch du calendrier (01/10/2026) : `tab5_cal_prefetch_boot` sans paramètre (appelé par `on_boot` et `status_ha`) délègue à `tab5_cal_prefetch` (`queued`, paramètre `force`), dont chaque demande de mois est sautée si le mois a été reçu il y a moins de `CAL_PREFETCH_FRESH_MS` ; le bouton « Recharger le calendrier » force ; fraîcheur entre 10 s et les 10 min du rendu. |
| `test_version.py` | `tests/` | Contenu | Version par défaut du firmware (`tab5-ha-hmi.yaml`) = dernière version publiée du CHANGELOG + « -dev » ; modèle Jinja de `binary_sensor.tab5_fichiers_ha_en_retard` rendu sur 13 cas (X.Y seulement), ses attributs et sa notification. |
| `test_publication.py` | `tests/` | Unitaire + contenu | Publication (ADR-0022) : `tools/publication/preparer.py` (binaires renommés par révision, manifeste contrôlé et réécrit, refus d'un manifeste ou d'un binaire inattendu), `pages.py` (canaux stable/bêta parmi les releases 3.x, site reconstruit depuis leurs fichiers) ; mêmes révisions partout (fichiers, matrice, page), clé du projet et jamais de clé jetable, ESPHome figé ≥ plancher, mise à jour seulement dans les firmwares publiés. |
| `test_capture_serie.py` | `tests/` | Unitaire | Capture d'un plantage sur le port série (`tools/capture_serie.py`) : bloc de panique RISC-V repéré jusqu'au redémarrage, adresses à décoder sans doublon, redémarrages comptés, rien de signalé pour un démarrage normal, jamais de port deviné entre deux appareils Espressif. |
| `test_meme_code.py` | `tests/` | Unitaire | Même code qu'une image publiée (`tools/publication/meme_code.py`) : signature SBv2 et heure de compilation ignorées, taille différente ou code déplacé refusés, image non signée refusée. |
| `test_alarme_popup.py` | `tests/` | Contenu | Popup du réveil, barre du bas : la ligne « prochain rendez-vous » passe par `texte_ha_coupe()` (une ligne, « … »), sans largeur fixe dans `alarm_popup.yaml`, et sa limite `kLargeurRdvSuivant` (`alarm_render.cpp`) s'arrête avant le bouton « Tester », recalculé depuis le YAML. |
| `test_alertes_ha.py` | `tests/` | Contenu + rendu | Alertes côté HA : aucun `input_text` des packages avec `initial:`, calcul d'un seul tenant, sauvegarde des alertes lues ; la macro réelle de `tab5_alertes.jinja` rendue en bac à sable Jinja et rejouée (redémarrages, plantages, versions, coupures, taps, abonnements, reprise de l'ancienne liste). |
| `test_geometrie_partagee.py` | `tests/` | Contenu | Géométrie écrite en YAML et en C++ tenue égale : jeton `${central_w}` (`tab5-ui-tokens.yaml`) = `kLargeurPanneauCentral` (`tab5_central.cpp`), plus de 1180 en clair ; en-têtes « Lun »…« Dim » de `calendar_popup.yaml` sur les colonnes `kCalColX0`/`kCalColPas`/`kCalColW` de `tab5_calendar.cpp`, grille centrée dans la carte modale. |
| `test_improv_serie.py` | `tests/` | Unitaire | Wi-Fi par Improv sur l'USB (`tools/improv_serie.py`, `migrer_vers_3.py --port`) : paquets conformes (en-tête, longueur, somme de contrôle, saut de ligne), lecture au milieu du journal, réglage réussi / réseau introuvable / tablette muette face à une fausse liaison série, mot de passe jamais affiché, `secrets.yaml` lu en YAML. |
| `test_render_ha_config.py` | `tests/` | Unitaire | Copie des fichiers HA publics, détection de fuite d'identifiants réels et de placeholder restant (`tools/render_ha_config.py`) ; aucun placeholder dans le dépôt. |
| `test_tableau_de_bord.py` | `tests/` | Contenu + rendu | Tableau de bord HA de la tablette (`custom_templates/tab5_dashboard.jinja`) : chaque entité cherchée existe dans le firmware et chaque entité du firmware a sa carte (sauf exceptions motivées), entités et automatisations de package citées définies ; rendu Jinja avec une fausse maison (préfixes mélangés, selects de HA dans une autre langue, sans package, sans tablette, deux tablettes) : YAML valide, chaque entité citée présente. |
| `test_integration_tab5.py` | `tests/` | Unitaire + contenu | Intégration « Tab5 » pour HACS (ADR-0035) sans Home Assistant : `custom_components/tab5/installation.py` dans un dossier temporaire (première installation, mise à jour avec un package modifié à la main et un package retiré, restauration à l'octet près, optionnel posé seulement s'il est déjà là, copie du blueprint importée par URL mise à jour, blueprint homonyme et packages de l'utilisateur intacts, sauvegardes limitées à 5 et jamais deux identiques de suite, chemins refusés, fichiers copiés à la main avant la première installation signalés, version refusée pas réessayée) ; `firmware.py` (mise à jour enchaînée de la tablette : attente gardée jusqu'à la version constatée, nouvel essai, échec après 3) ; `tools/publication/archive_hacs.py` (code à la racine du zip, manifest versionné, `fichiers/` = les octets de `tab5_home_assistant.zip`, reproductible, rien pour un ancien tag) ; `manifest.json`, `hacs.json`, traductions FR/EN, constantes lues dans les packages, notifications. |
| `test_installation_ha.py` | `tests/` | Unitaire + contenu | Job « installation dans un HA neuf » sans conteneur : `preparer_config.py` écrit une installation complète (ligne des packages, tous les packages, blueprint identique), ni placeholder ni `!secret` installés, optionnels seulement sur demande, chaque entité `…tab5_…` lue par un package définie par un package, entrées du blueprint et « Zones masquées » attendues, tablette virtuelle au nom de la vraie, mêmes chemins sur `main` et en PR ; fonctions pures de `verifier_installation.py` (clé, traces, journal de HA). |
| `test_site_doc.py` | `tests/` | Contenu + construction | Site de documentation (ADR-0030) : chaque fichier de `docs/` est dans `tools/site/menu.yml` ou écarté exprès (`HORS_SITE`), chaque page dans les deux langues, liens vers le dépôt et ancres de l'autre langue réécrits, puis le site `en/` et `fr/` est construit en mode strict (`tools/site/construire.py`) ; l'accueil est le README (titre, JSON-LD), la racine `web/index.html` renvoie vers `en/` ou `fr/`. |
| `test_notice.py` | `tests/` | Contenu | Notice d'utilisation (`docs/notice/`) : chaque appui long du YAML et le seul glissement décrits dans les deux langues, chaque fenêtre du rendu (`tools/rendu/ecrans.py`) montrée ou écartée avec sa raison, mêmes images dans les deux moitiés, aucune image citée absente ni orpheline, légende de l'accueil annoté = repères de `tools/site/images_notice.py`. |
| `test_actions_ha.py` | `tests/` | Contenu | Événements seulement (ADR-0025) : aucun `homeassistant.service` / `homeassistant.action` ni `${entity_…}` dans le firmware. |
| `test_alertes_ecran.py` | `tests/` | Contenu + rendu | Bandeaux d'alertes de la carte centrale (lot 3 du 06/10/2026) : payload `tab5_maj_alertes_ha_bulk` rendu depuis le vrai modèle de `packages/tab5_push.yaml`, en-tête `@n:total`. |
| `test_appuis.py` | `tests/` | Contenu | Appuis longs des trois boutons du haut au choix (07/10/2026) : clé `appuis` de `tab5_maj_emplacements`, blueprint ↔ firmware. |
| `test_bouton_alim.py` | `tests/` | Contenu | Bouton d'alimentation : un redémarrage `ESP_RST_WDT` sans rapport de plantage n'est pas classé « plantage » (`tab5_journal.cpp`). |
| `test_ci_pip.py` | `tests/` | Contenu | Chaque `pip install` d'un workflow passe par `tools/ci/pip_reessai.sh` (réessais quand PyPI répond « from versions: none »). |
| `test_ci_securite.py` | `tests/` | Contenu | Chaîne d'approvisionnement de la CI : actions figées par SHA complet, permissions déclarées par workflow (PR en lecture seule), esptool figé avec empreintes. |
| `test_clim.py` | `tests/` | Contenu + rendu | Clim de toute marque (ADR-0026) et clim par tuile (ADR-0027) : clé `climr`, lettres de capacités, blueprint ↔ `Tab5/ecran/tab5_clim.cpp`. |
| `test_demarrage_ha.py` | `tests/` | Contenu | Démarrage de HA : la poussée complète suit aussi le chemin de la reconnexion (événement `tab5_connected` perdu avant les automatisations). |
| `test_demo.py` | `tests/` | Contenu | Mode démo : emplacements et clés de zones poussés = ceux de la tablette (`tab5_maj_emplacements`, `kCles`). |
| `test_demo_pieces.py` | `tests/` | Contenu | Pièces du mode démo contre la grammaire de l'ADR-0023 (types, options, icônes, échappement des champs). |
| `test_economie.py` | `tests/` | Contenu | Mode économie d'énergie : options du select dans l'ordre de `ChoixEconomie`, câblage YAML de `tab5-economie.yaml`. |
| `test_emplacements.py` | `tests/` | Contenu | Emplacements (ADR-0019) : clés poussées par le blueprint = table de `tab5_maj_emplacements`. |
| `test_energie.py` | `tests/` | Contenu + rendu | Popup Énergie (ADR-0028) : contrat des deux actions, champs de l'instantané, créneaux des vues, package, démo, firmware. |
| `test_extinction_auto.py` | `tests/` | Contenu | Extinction automatique de l'écran : défaut « Jamais », délais dans l'ordre des options, exclusions (OTA en cours, voix…). |
| `test_formes_themes.py` | `tests/` | Unitaire | Formes et zones sombres des thèmes (ADR-0029, lot 3) : `tools/gen_themes.py` sur un thème d'essai, dans un dossier temporaire. |
| `test_garde_origine.py` | `tests/` | Contenu | Garde d'origine des événements `esphome.tab5_*` : seul l'appareil Tab5 est écouté (blueprint, `tab5_evenements.yaml`). |
| `test_historique.py` | `tests/` | Contenu + rendu | Popup Température (ADR-0032) : action `tab5_maj_historique`, vues, limites du firmware face au package et à la démo. |
| `test_i18n.py` | `tests/` | Contenu | Traduction de l'écran : chaque `tr()` a sa clé dans `Tab5/lang/*.yaml`, `%d` à leur place, glyphes présents dans les polices, fichier généré à jour. |
| `test_jour_travaille.py` | `tests/` | Rendu | Jour travaillé des tuiles météo et du réveil (HA-3, 07/10/2026) : mêmes macros de `tab5_calendar.jinja` que le popup calendrier. |
| `test_maison.py` | `tests/` | Contenu | Popup Maison (ADR-0037) : registre, select « Aller à l'écran », chrome partagé, rien de nouveau avec HA. |
| `test_meteo_blueprint.py` | `tests/` | Contenu + rendu | Météo choisie dans le blueprint (section « Météo ») : elle écrit les listes « Tab5 · … » de `tab5_meteo_sources.yaml`. |
| `test_meteo_icones_nuit.py` | `tests/` | Rendu | Icônes de nuit des prévisions heure par heure (Met.no : `partlycloudy` de nuit) : modèle de HA (`is_daytime`, `sun.sun`) contre un calcul indépendant. |
| `test_meteo_repli.py` | `tests/` | Rendu | Repli de la météo (08/10/2026) : source choisie indisponible, une autre prend le relais (connue, puis n'importe laquelle), retour tout seul, rien ne part sans aucune météo ; vrais modèles des packages et de `tab5_meteo.jinja`. |
| `test_meteo_sans_meteo_france.py` | `tests/` | Rendu | Chaîne météo rendue sans Météo-France (Met.no seul), avec les vrais modèles des packages. |
| `test_pluie_sans_meteo_france.py` | `tests/` | Rendu | Pluie dans l'heure sans Météo-France : la source effective devient Open-Meteo. |
| `test_polices_themes.py` | `tests/` | Contenu | Polices d'affichage des thèmes : géométrie de l'horloge recalculée depuis les métriques de `Tab5/themes/_polices.yaml`. |
| `test_poussee_pluie.py` | `tests/` | Contenu | Pluie dans l'heure par la poussée légère, plus par la chaîne complète (PERF-3, HA-16 du 07/10/2026). |
| `test_premier_demarrage.py` | `tests/` | Contenu | Premier démarrage après une installation par l'USB : « First boot after install » n'est pas un redémarrage inattendu. |
| `test_rangee.py` | `tests/` | Contenu | Rangée sous l'horloge (ADR-0031) : calage sur le rotateur (7,8 s + 0,2 s), blueprint ↔ firmware. |
| `test_reglables.py` | `tests/` | Contenu | Tuile − / + (ADR-0033) : types, domaines, icônes, bornes par type, liste blanche des commandes, blueprint ↔ `tab5_reglables.cpp`. |
| `test_reglages.py` | `tests/` | Contenu | Popup Réglages : contrat numérique `ReglageId` / index d'option entre `reglages_popup.yaml` et le C++. |
| `test_roue.py` | `tests/` | Contenu | Roue d'actions rapides (ADR-0036) : géométrie, anneaux, liens « Maison » et « Détails », commandes, lue dans le C++ et le YAML. |
| `test_sante_statistiques.py` | `tests/` | Contenu + rendu | Statistiques longues de fiabilité (lot K, 09/10/2026) : la garde « reboot inattendu » émet `tab5_sante_redemarrage` avant son filtre « demandé » ; compteurs `total_increasing` et durée de fonctionnement continu (`duration`, sans `now()`) rendus. |
| `test_solaire.py` | `tests/` | Contenu + rendu | Icône solaire du bandeau d'état : clé `solaire` de `tab5_maj_emplacements`, blueprint ↔ firmware. |
| `test_themes.py` | `tests/` | Contenu | Palettes, catalogue des thèmes et styles de rôle (ADR-0029) : chaque palette donne tous les rôles, chaque style lit la palette. |
| `test_tuiles_blueprint.py` | `tests/` | Contenu + rendu | Pièces et tuiles (ADR-0023), côté HA : types, options et commandes du blueprint = tableaux de l'ADR, états poussés. |
| `test_tuiles_firmware.py` | `tests/` | Contenu | Pièces et tuiles (ADR-0023), côté firmware : grammaire des clés, types, options, commandes de `tab5_tuiles.cpp`. |
| `test_blueprint_genere.py` | `tests/` | Contenu | Blueprint `tab5_emplacements.yaml` : déclencheurs des pièces et de la rangée à jour de `tools/gen_blueprint_emplacements.py`, une seule liste « tout pousser », une seule action `tab5_maj_clim` (HA-8). |
| `test_tuiles_icones.py` | `tests/` | Contenu | Palette des icônes des tuiles : parties générées par `tools/gen_tuiles_icones.py` à jour (C++, glyphes MDI, blueprint). |
| `test_zones.py` | `tests/` | Contenu | Zones optionnelles : enum `Zone`, `kCles`, demande `esphome.tab5_zones` et HA d'accord, dans l'ordre. |
| `test_guards.py` | `tests/` | Contenu | Joue les 8 garde-fous ci-dessous sur le C++/YAML réel (chrome modal, registre, règles de code, salles Marble, niveaux Lode, niveaux d'Arcanoïde, questions de Trial Poursuite, comptes de la cartographie). |
| `test_moteurs_hote.py` | `tests/` | Contenu | Tests C++ des moteurs (audit du 07/10/2026, OUT-2) : chaque `tools/test_*.cpp` compilé et lancé par le job `python`, perft des tests C++ d'échecs et de dames égaux à leurs miroirs Python, moteur des dames (`draughts_engine.*`) pur : ni LVGL, ni ESPHome, ni préférences. |
| `conftest.py` | `tests/` | — | Pose une fois le `sys.path` des outils importés par les tests (`tools/`, `tools/demo/`, `tools/rendu/`…). |
| `commun.py` | `tests/` | — | Utilitaires communs (OUT-3) : chemins, `lire()`, chargeurs YAML (libyaml quand elle est là), `fichiers_du_depot()` (fichiers suivis ou non ignorés), `bloc_service()`, cache Jinja de session. |
| `__init__.py` | `tests/` | — | Marqueur de package. |

### 3.2 Tests moteurs de jeux (`tools/`)

| Fichier | Emplacement | Type | Cible |
|---|---|---|---|
| `test_go_engine.cpp` | `tools/` | Unitaire (C++ hôte) | Règles du Go contre le vrai `go_engine.cpp` : capture, suicide, ko, œil, handicap, territoire, score, parties aléatoires — g++ en CI (job `python`). Seul test du Go : son miroir Python, doublon, est retiré le 08/10/2026. |
| `test_chess_engine.cpp` | `tools/` | Unitaire (C++ hôte) | Le vrai `chess_ai.cpp` contre la suite perft (5 positions, 17 profondeurs) et une recherche courte, sous ASan + UBSan — g++ en CI (job `python`). Fait foi. |
| `test_draughts_engine.cpp` | `tools/` | Unitaire (C++ hôte) | Le `Draughts::Engine` de `draughts_engine.cpp` (module pur, compilé tel quel) contre les perft 10×10 et 8×8 et les règles, sous ASan + UBSan — g++ en CI (job `python`). Fait foi. |
| `test_chess_perft.py` | `tools/` | Unitaire (miroir Python) | Générateur d'échecs contre la suite perft standard ; miroir de `test_chess_engine.cpp`, gardé pour un poste sans g++. |
| `test_draughts_engine.py` | `tools/` | Unitaire (miroir Python) | Générateur de dames (10×10 et 8×8) contre les perft de référence + règles (prise majoritaire, dame volante, promotion) ; miroir de `test_draughts_engine.cpp`, gardé pour un poste sans g++. |
| `hote/` | `tools/` | Support | `esphome.h` minimal (journal, `millis()`) pour compiler le moteur d'échecs hors ESPHome. |
| `test_alarm_clock.cpp` | `tools/` | Unitaire (C++ hôte) | Moteur du réveil réel (`alarm_clock.cpp` + `tab5_core.cpp`) : 13 scénarios, horloge simulée, fuseau Europe/Paris et changements d'heure — g++ en CI (job `python`). |
| `test_tab5_socle.cpp` | `tools/` | Unitaire (C++ hôte) | Socle commun (`tab5_champs.cpp` + `tab5_core.cpp`, lot L5) : lecture bornée des payloads, dates, heures « HH:MM », géométrie et modèle des tuiles — g++ en CI (job `python`). |
| `test_parse.cpp` | `tools/` | Unitaire (C++ hôte) | Lecture des payloads de HA (`tab5_parse.cpp`, lot F de l'audit du 30/09/2026) : prévisions, vigilance, alertes HA, historique, bandeau info, pluie, calendrier, emplacements, production solaire, clim ; cas normaux, champs vides, tronqués, extrêmes, « nan »/« inf », comportements gardés marqués « [figé] » — g++ sous ASan + UBSan en CI (job `python`). |
| `fuzz/` | `tools/fuzz/` | Fuzz (CI) | Harnais libFuzzer des mêmes parseurs (`fuzz_parse.cpp`, le premier octet choisit le parseur), `graines.py` (graines tirées de `tools/sanitizers/fuzz_services.py`), `temoin.txt` (témoin positif) — clang `-fsanitize=fuzzer,address,undefined`, job `fuzz-parseurs` de `sanitizers.yml` (3 min, non requis). |

### 3.3 Outils de validation (intégration)

| Fichier | Emplacement | Type | Rôle |
|---|---|---|---|
| `tools/demo/demo_pusher.py` | `tools/demo/` | Intégration (dry-run) | Valide chaque payload push contre le contrat firmware. |
| `tools/installation_ha/` | `tools/installation_ha/` | Intégration (CI) | Installation dans un Home Assistant neuf, sans matériel (`.github/workflows/installation-ha.yml`) : `preparer_config.py` (dossier `config/` d'une installation neuve : `configuration.yaml`, fichiers de l'archive `tab5_home_assistant.zip` tels quels, `donnees_test.yaml`), `verifier_installation.py` (ordre « Sans compiler » : onboarding, sources choisies dans les listes « Tab5 · … », ajout ESPHome de la tablette virtuelle, clé API, option « actions HA » laissée décochée (ADR-0025), automatisation du blueprint, redémarrage ; traces, zones, captures demandées par HA, demandes de la tablette par événements, tableau de bord de la tablette rendu par HA et enregistré, journal de HA) ; `captures_ha.py` (captures de l'interface de HA du guide d'installation, anglais et français, par Playwright). |
| `tools/installation_ha/verifier_integration.py` | `tools/installation_ha/` | Intégration (CI) | Intégration « Tab5 » pour HACS dans un HA neuf en conteneur (`.github/workflows/integration-hacs.yml`, ADR-0035) : zips construits par `archive_hacs.py` et décompressés comme le fait HACS ; première installation sans redémarrage, mise à jour avec un package modifié à la main et un retiré, retour en arrière sur une configuration cassée, pas de nouvel essai au redémarrage suivant ni de sauvegarde identique, blueprint copié à la main signalé (réparation validée), ligne `packages:` absente puis remise ; journal de HA. |
| `tools/sanitizers/` | `tools/sanitizers/` | Intégration (CI) | Tablette virtuelle sous ASan + UBSan (`.github/workflows/sanitizers.yml`) : `variante.py` + `pio_drapeaux.py` (compilation instrumentée), `fuzz_services.py` (tous les services du contrat, une graine chacun), `cibles_ub.py` (conversions hors bornes, fenêtres ouvertes), `rapports.py` (rapports lus dans le journal de la tablette), `temoin.cpp` (témoin positif). |
| `tools/verifier_secrets_config.py` | `tools/` | Outil | Analyse les fichiers suivis par git (`.yaml`, `.yml`, `.example`, `.jinja`, `.md`) pour détecter des secrets en clair. |
| `tools/render_ha_config.py` | `tools/` | Outil | Copie les fichiers HA publics dans `rendered/` ; `--check` = garde-fou de fuite (valeurs réelles, placeholders). |
| `tools/publication/archive_ha.py` | `tools/publication/` | Outil (CI) | Archive `tab5_home_assistant.zip` d'une release (packages, custom_templates, blueprint, optionnels), reproductible. |
| `tools/publication/archive_hacs.py` | `tools/publication/` | Outil (CI) | Archive `tab5_hacs.zip` d'une release pour HACS (ADR-0035) : l'intégration `custom_components/tab5/` à la racine, `manifest.json` à la version de la release, `fichiers/` = les fichiers de `tab5_home_assistant.zip` (`archive_ha.entrees`). |
| `tools/check_tab5_modal_chrome.py` | `tools/` | Garde-fou | ADR-0009 : chrome modal partagé sur chaque popup (rapatrié du workspace le 06/09/2026). |
| `tools/check_marble_rooms.py` | `tools/` | Garde-fou | Les 6 salles de « Fil d'Or » lues dans `marble_game.cpp` restent traversables (numpy). |
| `tools/check_lode_levels.py` | `tools/` | Garde-fou | Les 10 niveaux de « Coureur d'Or » lus dans `lode_game.cpp` restent jouables. |
| `tools/check_arkanoid_levels.py` | `tools/` | Garde-fou | Les 8 niveaux d'« Arcanoïde » lus dans `arkanoid_game.cpp` : rangées complètes, valeurs connues, aucune brique destructible emmurée, `LEVELS`/`LEVEL_NAMES`/fin de partie cohérents. |
| `tools/check_trivia_questions.py` | `tools/` | Garde-fou | La banque de « Trial Poursuite » (`trivia_questions.h`) : autant d'entrées que chaque `#define`, catégorie et difficulté valides, ni texte vide, ni leurre égal à la réponse, ni question en double. |
| `tools/check_tab5_registry.py` | `tools/` | Garde-fou | ADR-0013 : chaque `*_game.h` figure dans `GameRegistry::kGames`, aucune liste de jeux recopiée dans un YAML. |
| `tools/check_tab5_code_rules.py` | `tools/` | Garde-fou | Règles de code : `snprintf` partout, aucun `lv_*` dans le contrat API, aucun global orphelin, aucune entité HA en dur, glyphes de la date (`roboto_45`), icônes MDI couvertes par la police de leur widget sans glyphe mort (règle 7). |
| `tools/tab5_sources.py` | `tools/` | Bibliothèque | Où sont rangées les sources du firmware (`Tab5/socle|ecran|jeux|paquets`) : `fichiers(motif…)` et `source(nom)`, pour les outils et les tests (`tests/commun.py`) ; `contrat()` : `tab5_custom.h` et les en-têtes de modules qu'il inclut. |
| `tools/cartographie_counts.py` | `tools/` | Garde-fou | Comptes de lignes de `CARTOGRAPHIE_TAB5.md` à 20 % près ; `--write` les recalcule. |
| `tools/contrat_api.py` | `tools/` | Outil | Contrat HA ↔ firmware lu à n'importe quelle révision git : instantané `contrat/contrat.yaml` (`--write`, `--check`), semver contre le dernier tag (`--semver`), matrice N-1 en Markdown pour la note de release (`--matrice [--depuis vX] [--courant vY] [--en]`). |
| `.pre-commit-config.yaml` | Racine | Config | yamllint (dont `*.yaml.example`), BOM, secrets, fuite d'identifiants HA — rejoué par la CI ; style C++ des seules lignes indexées (`git-clang-format --diff --staged`, sans effet en CI où rien n'est indexé). |
| `.clang-format` | Racine | Config | Style C++ relevé sur le code existant (clang-format 23.1.3) ; appliqué aux seules lignes modifiées par le hook pre-commit, jamais à un fichier entier. |
| `.editorconfig` | Racine | Config | Encodage, saut de ligne final et indentation par type de fichier, relevés sur les fichiers suivis. |
| `pyproject.toml` | Racine | Config | `testpaths = tests, tools` : `pytest` nu ne ramasse plus `archives/`. |
| `requirements-dev.txt` | Racine | Config | Dépendances des outils (pytest, numpy, aioesphomeapi, fonttools, pyserial, pre-commit, yamllint) — pas le firmware. |

### 3.4 Commandes de lancement

```bash
# Tous les tests (tests/ + moteurs de jeux sous tools/ — cf. pyproject.toml)
python -m pytest

# Tests moteurs de jeux (miroirs Python ; le vrai C++, qui fait foi, ne se compile qu'en CI, g++)
python tools/test_chess_perft.py
python tools/test_draughts_engine.py

# Garde-fous rejoués par la CI
pre-commit run --all-files
python tools/cartographie_counts.py --write   # après un ajout/retrait de lignes notable

# Validation des payloads push (dry-run, sans matériel)
python tools/demo/demo_pusher.py --dry-run

# Contrat HA ↔ firmware entre versions (tags v* nécessaires)
python tools/contrat_api.py --write           # après un changement d'action ou d'événement
python tools/contrat_api.py --semver          # la version du contrat suffit-elle ?
python tools/contrat_api.py --matrice --en    # tableau N-1 pour la note de release
```

---

## 4. Résumé de l'arborescence des tests

```
00ProjetTab/
├── tests/
│   ├── __init__.py
│   ├── conftest.py   (sys.path des outils)
│   ├── commun.py   (utilitaires communs)
│   └── test_*.py   (67 fichiers au 08/10/2026, un par ligne du § 3.1)
├── tools/
│   ├── demo/
│   │   ├── demo_pusher.py
│   │   ├── requirements.txt
│   │   └── scenarios.py
│   ├── ci/pip_reessai.sh   (pip install avec réessais, tous les workflows)
│   ├── installation_ha/   (job « installation dans un HA neuf »)
│   │   ├── captures_ha.py
│   │   ├── configuration.yaml
│   │   ├── donnees_test.yaml
│   │   ├── preparer_config.py
│   │   ├── verifier_installation.py
│   │   └── verifier_integration.py
│   ├── publication/   (archives et site d'une release)
│   ├── rendu/   (captures du rendu hors tablette)
│   ├── sanitizers/   (fuzz et cas ciblés sous ASan + UBSan)
│   ├── fuzz/   (harnais libFuzzer des parseurs, graines, témoin)
│   ├── site/   (construction du site de documentation)
│   ├── hote/   (esphome.h minimal du test d'échecs)
│   ├── test_go_engine.cpp
│   ├── test_chess_engine.cpp
│   ├── test_draughts_engine.cpp
│   ├── test_alarm_clock.cpp
│   ├── test_tab5_socle.cpp
│   ├── test_parse.cpp
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
    ├── esphome-tab5.yml   (CI : changes, python, build, build-min, build-revisions ; voir § 5)
    ├── installation-ha.yml   (installation dans un HA neuf, voir § 5)
    ├── integration-hacs.yml   (intégration « Tab5 » pour HACS, voir § 5)
    ├── publication.yml   (binaires signés d'une release, voir § 5)
    ├── rendu-host.yml   (rendu hors tablette, voir § 5)
    ├── sanitizers.yml   (tablette virtuelle sous ASan + UBSan, voir § 5)
    └── site.yml   (site GitHub Pages, voir § 5)
```

---

## 5. Notes importantes

- **Pas de suite de tests unitaires pour la HMI** : la logique LVGL (`tab5_*.cpp`) n'a pas de tests hôte. Seuls les moteurs de jeux (Go, échecs, dames), le réveil et le socle commun (`tab5_champs`, `tab5_core`, `tab5_parse` : lecture des payloads de HA, fuzzée aussi) disposent de tests C++ hôte.
- **Les tests C++ des moteurs font foi** (08/10/2026, OUT-2) : `test_go_engine.cpp`, `test_chess_engine.cpp` et `test_draughts_engine.cpp` compilent le vrai moteur (g++, en CI ; échecs et dames sous ASan + UBSan). Les miroirs Python des échecs et des dames restent pour le poste de dev sans g++ : toute modification du C++ doit y être reflétée, et `tests/test_moteurs_hote.py` tient leurs perft égaux à ceux du C++.
- **CI GitHub Actions** (`.github/workflows/esphome-tab5.yml`, PR + push sur `main`) : job `changes` (filtre des chemins) ; job `python` (pre-commit, `pytest`, moteurs Go, échecs, dames et réveil en C++, dry-run démo) ; job `build` (secrets factices + `esphome/build-action@v8.1.0`, image `latest` = canari amont voulu, ADR-0016, ccache conservé entre runs) seulement si `tab5-ha-hmi.yaml`, `Tab5/` (hors `.md`) ou le workflow changent ; job `build-min`, même compilation avec la version plancher lue dans `min_version:` (26/09/2026). `python`, `build` et `build-min` sont des checks requis de `main` ; `build` reste présent et passe en « skipped » sinon. Job `build-revisions` (non requis) : les révisions d'écran ST7121 et ILI9881C compilées en parallèle. Artefact `tab5-firmware` publié sur `main`. Durées sur `main` le 07/10/2026 : `python` 3 à 6 min, `build` ~5 min, `build-min` ~6 min, `build-revisions` 5 à 8 min.
- **Installation dans un HA neuf** (`.github/workflows/installation-ha.yml`, 28/09/2026, ~3 min, non requis) : Home Assistant figé en conteneur (`HA_IMAGE`) + la tablette virtuelle (`tab5-rendu-host.yaml` compilé sous le nom `tab5-ha-hmi`), installés comme par un nouvel utilisateur (`tools/installation_ha/`) : tous les packages rendus et `check_config`, puis l'ordre « Sans compiler » du guide (onboarding, ajout ESPHome sans l'option « actions HA », automatisation du blueprint) et un redémarrage de la tablette, puis deux demandes de la tablette de bout en bout (calendrier par le select « Aller à l'écran », « MAJ Écran » par le doigt virtuel) et un redémarrage de HA forgé par un autre appareil, qui doit être ignoré. Échoue si la clé API n'est pas donnée et gardée par HA, si la clé nulle ou le clair passent encore, si `esphome.tab5_connected` n'arrive pas après la clé, si une trace du blueprint ou de la poussée complète n'aboutit pas, si « Zones masquées » diffère, si la capture demandée par HA manque, si la clé ne survit pas au redémarrage, si une demande de la tablette n'aboutit pas, si HA a refusé une action de l'appareil (réparation « service_calls_not_allowed »), ou si le journal de HA a une erreur Tab5 après la connexion (hors « Not connected » pendant une déconnexion voulue, rapportée). Artefact `installation-ha` : deux captures (juste après l'automatisation du blueprint, puis après le redémarrage), journaux de HA et de la tablette. Sur les PR et `main` qui touchent HA, l'API ou la tablette virtuelle, et à la main. Ne teste pas l'interface de HA cliquée par un humain, la page de flashage ni le vrai matériel.
- **Intégration HACS** (`.github/workflows/integration-hacs.yml`, 07/10/2026, non requis, ADR-0035) : `hassfest` et la validation HACS par leurs images ghcr.io, puis `tools/installation_ha/verifier_integration.py` dans un Home Assistant neuf (même `HA_IMAGE` que le test ci-dessus) : première installation des fichiers sans redémarrage (`rest_command` chargé à chaud), mise à jour comme HACS (sauvegarde, fichier modifié à la main nommé, fichier retiré), configuration cassée remise comme avant avec la réparation « configuration_invalide », ligne `packages:` absente (réparation) puis remise. Le firmware enchaîné n'y est pas exercé (pas de tablette). Artefact `integration-hacs` : journal de HA.
- **Sanitizers** (`.github/workflows/sanitizers.yml`, 01/10/2026, ~25 min, non requis, paquets pip en cache par version d'ESPHome) : la tablette virtuelle compilée avec AddressSanitizer et UndefinedBehaviorSanitizer (`tools/sanitizers/`), après un témoin positif ; fuzzing de tous les services du contrat, cas de conversions hors bornes fenêtre ouverte, tous les écrans. Échoue au premier rapport lu dans le journal de la tablette (UBSan écrit sur la sortie d'erreur et ignore `log_path`), si la tablette s'arrête ou si le programme a été compilé sans sanitizers. Artefact `sanitizers` : journaux, `fuzz.md`, `cibles.md`, reproducteurs. Job `fuzz-parseurs` du même workflow (lot F, 09/10/2026, ~5 min) : libFuzzer sur `tab5_parse.cpp` seul (clang, ASan + UBSan), témoin positif d'abord, 3 min de fuzz, artefact `fuzz-parseurs` (entrées fautives) en cas d'échec.
- **Rendu hors tablette** (`.github/workflows/rendu-host.yml`, ADR-0021, non requis) : `tab5-rendu-host.yaml` compilé une fois pour la plateforme `host` (tâche `compiler`, programme passé en artefact aux tâches `rendu`, 08/10/2026), scènes du mode démo puis chaque fenêtre, sous-fenêtre et écran de jeu dans les sept langues (`tools/rendu/`) ; comparaison informative au dernier run de `main`. Seule comparaison bloquante : la tâche des thèmes, bascule à chaud = démarrage à froid au pixel près.
- **Site** (`.github/workflows/site.yml`, ADR-0022 et ADR-0030) : racine, page de flashage, manifestes des releases (`tools/publication/pages.py`) et documentation en/fr construite par MkDocs (`tools/site/construire.py`) ; lancé par `publication.yml`, par un push sur `main` qui touche le site, ou à la main.
- **Publication** (`.github/workflows/publication.yml`, ADR-0022) : à une release, les trois révisions d'écran compilées et signées (environnement protégé `publication`), binaires, archives `tab5_home_assistant.zip` et `tab5_hacs.zip`, ELF gardés 90 jours, puis `site.yml`.
- **Fichiers gitignorés** : `secrets.yaml` (2.x), `*.pem` / `*.key` (clé de signature), `tools/demo/cle_demo.txt`, `Tab5/user_entities.yaml`, `HomeAssistant_Config/placeholders.yaml`, `HomeAssistant_Config/rendered/`, les anciennes copies privées `automations_tab5.yaml` / `scripts_tab5.yaml` / `template_sensors_meteo_tab5.yaml` (obsolètes, gardées ignorées), `Tab5/tts_library*/`, `archives/`.
