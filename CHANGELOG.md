# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Dates are the day each pull request was merged into `main`.

## [Unreleased]

Pré-releases tirées de cette section, sur le canal bêta :
[v3.8.0-rc.1](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.8.0-rc.1)
le 07/10/2026 : roue d'actions rapides à deux anneaux (#378), « Son de la tablette » dans la liste
de la tuile − / + (#379).
[v3.8.0-rc.2](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.8.0-rc.2)
le 08/10/2026 : plus de souffle sans batterie, limite de charge et consommation (#387), changer de
thème ne fige plus l'écran (#383), Réglages en quatre pages (#400), repli météo et mention
« prévisions périmées » (#393, #395), correctifs et lots de l'audit du 07/10 (#381, #382, #384 à
#386, #388 à #394), rangement de `Tab5/` (#401, #402). Fichiers Home Assistant à recopier avant le
firmware.
[v3.8.0-rc.3](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.8.0-rc.3)
le 08/10/2026 : test de consommation dans Home Assistant et lecture de la batterie toutes les 2 s
pendant le test (#405), `tools/mesure_conso.py` (#404). Fichiers Home Assistant à recopier avant le
firmware.

**Contrat HA ↔ firmware** : compatible dans les deux sens (depuis v3.7.0).

### 2026-10-09 — Panneau « Ok Nabu » à lignes et défilement au choix (lot 3, ADR-0041)

- **Des lignes dans le bouton « Ok Nabu »** : jusqu'à trois lignes de quatre capteurs, choisies dans la nouvelle section « Panneau Ok Nabu · Ok Nabu panel » du blueprint « Tab5 — emplacements », avec le même modèle, le même dessin et les mêmes clés que la rangée sous l'horloge (lettre `n` : `np`, `nd`, `nLI` dans `tab5_maj_tuiles`, états `nLI` dans `tab5_maj_emplacements`). La ligne d'écoute « Ok Nabu : ON / OFF » vient en premier d'origine (deuxième, troisième ou masquée au choix). Un tap sur la ligne d'écoute bascule toujours le mot de réveil ; sur une ligne de capteurs, il ne fait rien. Pastilles sous le panneau, même glissement que la rangée, 14 px de marge dans le cadre pour ne rien couper dans un cadre en gélule (thème Capsule ; `tests/test_nabu.py` le calcule pour les 21 thèmes).
- **Tap sur les heures** : sa valeur « auto » devient « ligne suivante du panneau Ok Nabu » (code `nabu_suivant`, index 17, icône `microphone-message`), aussi au choix pour les onze autres gestes. Sans ligne dans le panneau, il ne fait rien, comme avant.
- **Défilement au choix** pour la rangée sous l'horloge, le panneau Ok Nabu et la tuile − / + : « Automatique » (avec la carte centrale) ou « Fixe ». D'origine : rangée automatique, panneau et tuile fixes, soit l'écran d'avant. En automatique, un geste remet le compte à zéro ; la tuile − / + passe par la clim et les appareils du blueprint (pas le son de la tablette), sans écrire la NVS, jamais liste ouverte. Clé `defil|rangée|nabu|clim|secondes;` poussée avec les gestes, gardée en NVS. `kTourCentralS` n'a plus qu'une définition (`tab5_internal.h`).
- **Gabarits** : `ui_components/rangee_panneau.yaml` (les deux panneaux de quatre éléments de chaque zone) et `ui_components/rangee_pastilles.yaml` (les pastilles des deux zones).
- **Ordre de mise à jour** : indifférent, `contrat/contrat.yaml` ne change pas (des clés dans les payloads existants). Ce blueprint avec un firmware d'avant : les clés `n` et `defil` ignorées, le bouton reste « Ok Nabu : ON / OFF ». Un blueprint d'avant avec ce firmware : le bouton d'avant et les défilements d'origine.
- Non essayé sur la tablette au moment de la PR (rendu dans les 21 thèmes, tap au doigt, défilement).

### 2026-10-09 — Le climat de la pièce en mode HA (ADR-0040, lot C)

- **Demande d'Axel** : en mode HA (Domo), dans une autre pièce, la carte clim montre la température de la pièce (et son humidité en option) et règle la clim de la pièce, à la place des températures du salon et de la serre. **Non testé sur la tablette.**
- **Blueprint** : trois champs facultatifs, vides par défaut, dans les sections « Pièce 1 » à « Pièce 5 » : « Température de la pièce », « Humidité de la pièce », « Climatisation de la pièce ». Nouvelles clés de `tab5_maj_emplacements`, sans nouvelle variable d'action (contrat inchangé, 1.0.0) : `pR|température|humidité|clim`, les réglages et l'état de la clim de la pièce `crpR|…` / `cepR|…` (champs de `crRT` / `ceRT`). Toutes les pièces à la connexion, puis une pièce avec les mesures lentes ou, tout de suite, quand sa clim change d'état ou de consigne (deux déclencheurs par pièce, générés par `tools/gen_blueprint_emplacements.py`). Commandes de la clim de la pièce à l'emplacement `cpR`, par les branches « Clim : … » existantes, vers cette clim seulement.
- **Firmware** (`Tab5/ecran/tab5_piece_climat.cpp`, nouveau ; la clé `pR` est lue par `piece_climat_lire()` de `Tab5/socle/tab5_parse.h`, testée par `tools/test_parse.cpp` et fuzzée) : sur une pièce qui a une température déclarée, thermomètre et température à gauche (icône teintée par l'humidité), goutte et humidité à droite, ou rien ; la tuile − / + règle sa clim (− / +, et un toucher sur la consigne ouvre le popup clim sur elle) sans changer le choix de sa liste ; appui long sur la température : son historique (clés `p0` à `p4` du popup Température et de `packages/tab5_historique.yaml`). Hors du mode HA ou sur une pièce sans ces champs : l'écran d'avant. Clims des pièces dans la table des clims de tuile (30 cases en PSRAM au lieu de 25).
- **Ordre de mise à jour HA ↔ firmware** : indifférent. Le nouveau blueprint avec un ancien firmware : clés ignorées, écran d'avant. Le nouveau firmware avec un ancien blueprint : aucune clé `pR`, écran d'avant. Il faut les deux (blueprint et `tab5_historique.yaml` recopiés, firmware flashé) pour voir le climat des pièces.
- **Limite** : les réglages de la clim d'une pièce (bornes, pas, modes) ne repartent qu'avec un changement de son état ou une reconnexion, comme ceux des clims de tuile.
- **Carrousel des clims** (ADR-0038) : la clim propre de chaque pièce y a sa page, après celles des tuiles (`clims_enumerer()`) ; la même clim sur une tuile de la pièce (même nom) n'a qu'une page. En mode HA, toucher la température de la pièce ouvre le carrousel sur sa clim propre, sinon sur la première de ses tuiles.
- **Rendu et démo** : le Bureau de la démo a température, humidité et clim, l'Entrée une température seule ; nouveaux écrans `temperature-piece` et `climatisation-piece` ; `accueil-ha-piece-2` et `accueil-ha-piece-4` changent, et les écrans du carrousel (`climatisation-par-la-piece` : deux pages, le Bureau ayant sa clim ; `climatisation-carrousel*` : quatre pastilles ; `climatisation-carrousel-mode-ha` : la clim propre du Bureau).

### 2026-10-09 — Corrigé : défauts de lecture des payloads de HA relevés par le lot F

Firmware seul (`Tab5/socle/tab5_parse.cpp`) : aucun fichier Home Assistant à recopier, les payloads de HA (ceux de la v3.7.0 compris) se lisent comme avant. Contrat inchangé (1.0.0).

- **« Pas de données » pour la pluie** : HA envoie `@-1,0` quand il n'a pas de relevé ; la tablette le prenait pour `@-` (aucune source) et laissait la phrase vide. La faute était côté firmware (le contrat de `tab5-api-logic.yaml` dit bien « niveau -1 pas de données ») : `@-` n'est plus « aucune source » que sans chiffre après le « - ».
- **Vigilance** : un champ vide ne fait plus remonter les suivants d'un cran (`split_fields` au lieu de `strtok_r`). HA envoie toujours « Vert », rien ne change pour lui. Alertes HA, barres de pluie et détail du jour du calendrier gardent `strtok_r` : il n'y saute que des enregistrements vides, et chacun porte sa clé (id, index, genre), donc rien ne s'y décale (tests à l'appui).
- **Prévisions** : un index illisible (« abc », vide) n'écrase plus le premier créneau, et une température ou une pluie non finie ou hors bornes (« nan », « inf », « 1e99 » ; températures -100 à 150, pluie 0 à 1000 mm) n'est plus affichée : l'enregistrement est ignoré, le créneau garde ce qu'il avait, et le payload est signalé une fois au journal (`payload_refuse`, comme l'historique des alertes). Un premier créneau illisible fait refuser le bloc horaire au lieu de le prendre pour le bloc 0. Un nombre vide ou illisible vaut toujours 0, comme avant (HA envoie 0 pour une valeur absente).
- **Production solaire** : plus coupée à 15 octets sans le dire ; lue par `champ_nombre()` (au-delà de 31 octets, refusée). HA envoie un entier de 0 à 100 ou `nan`.
- `tools/test_parse.cpp` : chaque test qui figeait un de ces défauts affirme le bon comportement (« [corrigé] »), et `test_payloads_ha()` vérifie que les payloads tels que HA les envoie donnent les mêmes valeurs qu'avant. Graines du fuzz pour ces cas limites (`tools/fuzz/graines.py`). Non testé sur la tablette.

### 2026-10-09 — Carrousel des clims : une page par clim, ouvert par la température de la pièce (lot B)

- **Toucher la température de la pièce** (carte clim de l'accueil) ouvre le popup clim en **carrousel** (demande d'Axel) : une page par clim que la tablette connaît — celle du blueprint, puis chaque tuile de clim sans l'option m dont HA a envoyé les réglages ([ADR-0038](docs/decisions/0038-climate-carousel.md)). Glisser à gauche ou à droite montre la suivante ou la précédente, en boucle, sans animation ; des pastilles sous les cartes disent laquelle (8 au plus). Une seule clim : ni pastilles ni glisse, l'écran d'avant. Il s'ouvre sur la clim de la pièce affichée en mode HA quand elle en a une, sinon sur celle du blueprint. L'appui long (historique) ne change pas.
- **Un seul popup** : une page est le popup clim existant repeint pour cette clim (titre = son nom, ou celui de la pièce de sa tuile quand HA n'en donne pas ; boutons qu'elle n'a pas masqués). Ouvert par la consigne, une tuile ou « Aller à l'écran », il est aussi un carrousel. Le glisser reste dans le popup (rien ne change derrière) ; glisser l'arc de côté règle toujours la consigne.
- **La liste de la tuile − / +** (ADR-0033) s'ouvre maintenant par un **appui long sur la valeur entre − et +** ; sans aucune clim, toucher la température de la pièce la déroule encore. Le tap sur la valeur ne change pas. Description de la section « Tuile − / + » du blueprint mise à jour (texte seul).
- **Pour les lots suivants** : `clims_enumerer()` (`tab5_internal.h`) est la seule liste des clims de la tablette ; une clim propre à une pièce y sera une ligne (boîte à outils d'`AGENTS.md`).
- Tests : `tests/test_carrousel_clim.py`. Rendu hors tablette : `climatisation-par-la-piece` (une clim), `climatisation-carrousel`, `-page-2`, `-page-3` et `-mode-ha` (trois clims, deux de tuile poussées puis oubliées). **Non essayé sur la tablette.**

### 2026-10-09 — Gestes de l'accueil au choix : l'horloge en trois zones (lot A, ADR-0039)

- **L'horloge en trois zones tactiles** : les heures, les minutes et la date (`ui_components/horloge_zone.yaml`, à la place de `btn_clock_calendar_zone`), qui couvrent toute la tuile sans trou ni recouvrement, coupées dans le « : » et entre les chiffres et la date dans les 21 thèmes (`tests/test_gestes.py`). **Ce qui change d'origine** : le réveil passe du tap sur l'horloge à l'appui long sur l'heure (heures ou minutes) ; le calendrier reste à l'appui long, sur la date ; un tap sur les minutes passe à l'appareil suivant de la tuile − / + (sans dérouler la liste, choix gardé) ; un tap sur la date, à la ligne suivante de la rangée sous l'horloge ; un tap sur les heures ne fait rien.
- **Douze gestes au choix** dans le blueprint « Tab5 — emplacements », section renommée « Horloge et boutons du haut · Clock and top buttons » : le tap et l'appui long des heures, des minutes, de la date et des trois boutons du haut. Automatique (le comportement ci-dessus), Rien, un écran, ou une action : `mode_domo` (le tap du bouton maison), `appareil_suivant`, `rangee_suivante`, `ecoute` (le mot de réveil, comme le tap d'Ok Nabu). Les trois entrées `appui_*` restent.
- **Transport** : une clé `gestes|c1|…|c12;` dans `tab5_maj_emplacements` (ordre : tap puis appui long des heures, des minutes, de la date, du bouton maison, de l'engrenage, de la manette), sans nouvelle variable : `contrat/contrat.yaml` ne change pas. Le blueprint pousse toujours `appuis`, qu'un firmware 3.7 lit seul ; le nouveau firmware fait passer `gestes` avant `appuis`, et un payload sans `gestes` remet tout en automatique. Choix en NVS (nouvelle préférence). Un seul script fait un geste (`tab5_geste`, `geste_cible()`), une seule liste de codes (`kCodesGestes`, écrans puis actions, index gardé en NVS).
- **Icônes** : la mini icône d'un bouton du haut montre toujours son appui long ; quand son tap est changé, l'icône du bouton montre ce que fait le tap (`tap_glyphe()`). La clim choisie sur la tuile − / + montre aussi son icône à gauche de la consigne (`clim_consigne_icone`), qui reste au centre exact de la tuile.
- **Ordre de mise à jour** : indifférent. Un blueprint d'avant avec ce firmware : les gestes en automatique, les appuis longs choisis gardés. Ce blueprint avec un firmware d'avant : la clé `gestes` ignorée, une action choisie pour un appui long y vaut « auto ».
- Non essayé sur la tablette au moment de la PR (zones au doigt, icônes dans les 21 thèmes).

### 2026-10-09 — Lecture des payloads de HA testée sur PC et fuzzée (lot F de l'audit du 30/09)

- **`Tab5/socle/tab5_parse.h/.cpp`** (nouveau, pur : ni ESPHome ni LVGL) : la lecture des chaînes poussées par Home Assistant sort des unités d'écran, famille par famille — prévisions heures et jours, vigilance, alertes HA, historique des alertes et bandeau info, pluie (barres et phrase), calendrier (mois et détail du jour), emplacements et production solaire, réglages et état de la clim. Les unités d'écran l'appellent et ne gardent que l'affichage. **Extraction neutre** : boucles recopiées, travers compris (`strtok_r` qui fusionne les champs vides, `atoi` qui lit un index illisible comme 0, « inf » accepté par les prévisions) ; les changer est un changement de contrat, laissé à une PR à part.
- **`tools/test_parse.cpp`** (nouveau) : chaque parseur sur le vrai code — cas normaux, champs vides, payloads tronqués, valeurs extrêmes, « nan » et « inf » ; les comportements discutables gardés sont marqués « [figé] ». Compilé par g++ sous ASan + UBSan dans le job `python` de la CI.
- **Fuzz libFuzzer** permanent (`tools/fuzz/fuzz_parse.cpp`, clang `-fsanitize=fuzzer,address,undefined`) : job `fuzz-parseurs` de `sanitizers.yml` (non requis), 3 min par run, graines tirées des payloads du fuzz de la tablette virtuelle (`tools/fuzz/graines.py`), témoin positif rejoué avant (un comportement indéfini connu doit être signalé). `tests/test_fuzz_parse.py` tient chaque fonction de `tab5_parse.h` testée ET fuzzée.
- Restent dans les unités d'écran : la lecture des tuiles et des tuiles − / + (`tab5_tuiles.cpp`, `tab5_reglables.cpp` : structures gardées en NVS, filtre des glyphes des polices), et les `atof`/`atoi`/`sscanf` des lambdas YAML (`tab5-api-logic.yaml`, `tab5-calendar.yaml`).
- Aucun changement de comportement attendu ; non testé sur la tablette.

### 2026-10-09 — Fiabilité mesurable au-delà de 7 jours (lot K de l'audit du 30/09, Home Assistant seul)

- **Trois capteurs à statistiques longues** dans `packages/tab5_health.yaml` : le recorder ne garde les états que quelques jours, et « Tab5 Uptime » (horodatage) comme « Raison du redémarrage » (texte) n'avaient pas de statistiques. « Tab5 · redémarrages » et « Tab5 · redémarrages inattendus » (`total_increasing`) comptent l'événement `tab5_sante_redemarrage` que la garde « reboot inattendu » émet désormais pour chaque redémarrage reconnu, avec son propre classement demandé / inattendu (rien n'est recopié) ; une coupure Wi-Fi sans reboot, un redémarrage de HA ou l'arrivée de la tablette ne comptent pas. « Tab5 · fonctionnement continu » (`duration`, `measurement`, heures) : temps depuis le dernier démarrage, toutes les 15 min, sans `now()` (instant du déclencheur). Lecture : Outils de développement → Statistiques. Pas de tuile dans le tableau de bord du Tab5.
- Modèles validés par le moteur de modèles du Home Assistant de l'auteur (2026.10.0, `POST /api/template`) contre un calcul Python indépendant ; `tests/test_sante_statistiques.py`. **Non vérifié** : le comptage sur un vrai redémarrage et la restauration après un redémarrage de HA.
- *À faire en mettant à jour* : recopier `packages/tab5_health.yaml`, recharger les automatisations et les modèles. Aucun changement du firmware.

### 2026-10-09 — Moteur des dames dans son propre module (lot G de l'audit du 30/09)

- **`Tab5/jeux/draughts_engine.h` / `.cpp`** (nouveaux) : le moteur des dames (`Draughts::Engine` : position de départ, génération des coups, application, nulles, évaluation) quitte `draughts_game.{h,cpp}`, où il partageait le fichier avec l'interface LVGL et la sauvegarde. Code déplacé tel quel, ligne à ligne : aucun changement de comportement. `draughts_ai.*` garde l'IA, `draughts_game.*` l'interface.
- **Test C++ du moteur** : `tools/test_draughts_engine.cpp` compile `draughts_engine.cpp` directement, sans l'en-tête de remplacement d'ESPHome. Supprimés : `tools/hote/extraire_moteur_dames.py` (qui recopiait le bloc par script) et son garde dans `tests/test_moteurs_hote.py`, remplacé par un garde de pureté du nouveau module. Le miroir Python `tools/test_draughts_engine.py` reste.
- Le test C++ des échecs (`tools/test_chess_engine.cpp`, vrai `chess_ai.cpp` sous ASan + UBSan) était déjà en CI depuis le lot L9 (#391) : inchangé.

### 2026-10-09 — Contrat HA ↔ firmware prouvé entre versions (lot E de l'audit du 30/09)

- **Instantané du contrat** `contrat/contrat.yaml` (nouveau) : les 23 actions de la tablette avec leurs variables, les 22 événements `esphome.tab5_*` avec leurs champs, et une version semver du contrat (1.0.0 = celui de la 3.8.0-rc.3), distincte de celle du firmware. Généré par `tools/contrat_api.py --write`, vérifié par `--check` et pytest. Aucune action ni variable changée.
- **Semver contre le dernier tag** (`tests/test_contrat_versions.py`) : le contrat du dernier tag est lu dans son propre code (`git cat-file`) ; une action retirée, une variable ajoutée, retirée ou de type changé, un événement ou un champ plus émis exigent une version majeure ; une action, un événement ou un champ nouveau, une mineure. Le message dit quelle version poser.
- **Matrice N-1** (`python tools/contrat_api.py --matrice`, tableau Markdown, `--en` pour la note de release) : firmware d'une version avec les fichiers HA de l'autre, dans les deux sens, contre le dernier tag stable et le dernier tag. Un appel qui ne peut pas partir avec l'ancien firmware (déclenché par un événement qu'il n'émet pas, garde `protocole` du blueprint) ne compte pas. Sans étiquette dans `[Unreleased]`, les deux sens doivent passer ; une ligne « **Contrat HA ↔ firmware** : … (depuis vX) » du CHANGELOG annonce l'ordre et pytest la confronte à la matrice. Aujourd'hui, 3.8.0-rc.3 contre 3.7.0 : compatible dans les deux sens (seul `esphome.tab5_batterie_faible` attend l'autre moitié).
- **CI** : le job `python` récupère les tags `v*` sans historique (`git fetch --depth=1`) ; sans eux, ces tests échouent en CI et sont sautés en local.
- `tests/test_contrat.py` (phase 1) lit le contrat par le même module, sans changement de ses vérifications.

### 2026-10-09 — Lot J de l'audit du 30/09 (sécurité) : page d'installation et publication

- **Page `/install/`** : politique de sécurité du contenu (CSP) qui n'autorise que le script de la page (par son empreinte) et ESP Web Tools 10.4.0 depuis son dossier de jsDelivr ; empreinte SRI sur le script d'entrée. Essayé dans un navigateur : la page marche, un script d'un autre dossier ou injecté est bloqué ; non essayé : l'ouverture de la fenêtre d'installation avec une vraie tablette. `tests/test_csp_installation.py` vérifie l'empreinte et la version.
- **Publication** (`publication.yml`) : fichier `SHA256SUMS` joint à la release et attestation de provenance (`actions/attest-build-provenance`, figée par SHA, sans bloquer la publication si elle échoue). **Non essayé avant la prochaine release.** Comment vérifier : `docs/installation/flash.md`.

### 2026-10-09 — Première installation, suite : prérequis, bêta, Home Assistant 2026.8 testé (doc, CI)

- **`installation/README.md`** : « Ce qu'il faut » ajoute le Wi-Fi 2,4 GHz (fiche M5Stack du Tab5) et la source USB-C (conseil 5 V / 2 A, pas une exigence). **`flash.md`** : en bêta, prendre aussi `tab5_home_assistant.zip` de la pré-release. **`updates.md`, `troubleshooting.md`** : le menu est « Paramètres → Système → Réparations ».
- **CI** : `installation-ha.yml` et `integration-hacs.yml` tournent sur Home Assistant 2026.9.4 **et** 2026.8.3 (le plancher annoncé par `hacs.json` et la doc), artefacts suffixés par la version.

### 2026-10-09 — Première installation : ordre, dépannage et repères de version (doc)

- **Page `/install/`** : une note dit de mettre les fichiers Home Assistant en place avant d'installer (la fenêtre d'ajout dure 30 min après le démarrage) ; le renvoi « guide, étape 4 » devient « étape 1 » ; le texte de l'écran ST7121 dit qu'elle tourne chez un autre utilisateur, comme `flash.md`.
- **`home-assistant-files.md`** : les parties internes passent de 1-2-3 à A-B-C (elles se confondaient avec les étapes 1 à 7), avec leurs renvois dans `troubleshooting.md` ; les nouveautés 3.8 (alerte d'erreur de rendu, course du volet) sont marquées.
- **`wifi.md`** : le point d'accès « Tab5 Fallback AP » se coupe aussi 30 min après le démarrage d'une tablette sans clé ; un redémarrage le rouvre.
- **`installation/README.md`** : tableau « première installation : rien n'apparaît ? ». `updates.md` et `weather.md` : nouveaux essais du firmware et repli météo marqués « depuis la 3.8 ». Lien « documentation » de l'intégration HACS vers le guide des fichiers HA.
### 2026-10-08 — Test de consommation dans Home Assistant, lecture toutes les 2 s

- **Script « Tab5 — consumption test »** (`packages/tab5_mesure_conso.yaml`, nouveau) : sur une
  tablette qui tourne sur sa batterie, 11 cas de 5 min (écran 100 / 50 / 10 % et éteint, « Okay
  Nabu » coupé, haut-parleur coupé, thème Obsidienne sombre puis clair, économie d'énergie
  « Toujours », tout coupé, référence rejouée), « Tab5 Consommation » lue toutes les 5 s ; à la fin,
  réglages remis et notification avec un tableau en anglais à copier (moyenne, min, max, écart à la
  référence, niveau, tension, température, énergie, autonomie approximative). Rien à installer ni
  jeton : lisible dans l'interface de HA. S'arrête si la tablette est branchée. *Essai à blanc* :
  1 min par cas, sans batterie. Bouton « Test de consommation » dans le tableau de bord du Tab5.
- **Firmware** : interrupteur « Tab5 Mesure de consommation » (coupé par défaut et au démarrage) :
  l'INA226 est lu toutes les 2 s au lieu de 60 s, coupé seul au bout de 2 h.
- *À faire en mettant à jour* : recopier `packages/tab5_mesure_conso.yaml` et
  `custom_templates/tab5_dashboard.jinja`, recharger les scripts (et les modèles Jinja).

### 2026-10-08 — Outil : consommation sur batterie, scénario par scénario

- **`tools/mesure_conso.py`** : sur une tablette qui tourne sur sa batterie (USB débranché), pilote
  l'écran (100 / 50 / 10 %, éteint), le micro (« Okay Nabu ») et le haut-parleur par l'API REST de
  Home Assistant (jeton longue durée, jamais écrit ni affiché), lit « Tab5 Consommation » et écrit un
  CSV à partager ; 8 scénarios de 5 min (≈ 40 min), la référence rejouée à la fin ; remet les
  réglages trouvés au départ, même après Ctrl+C ; bibliothèque standard de Python seulement. Une
  lecture par minute (INA226 à 60 s) : de quoi classer les gros postes, pas les petits écarts.
  Essayé en `--essai` sur la tablette de l'auteur (sans batterie : pilotage et remise des réglages
  vérifiés, aucune valeur lue) ; pas encore lancé sur une tablette avec batterie.

### 2026-10-08 — Normes de style et CHANGELOG archivé

Aucun fichier de code reformaté, aucun changement du firmware.

- **`.clang-format`** décrit le style C++ déjà en place (clang-format 23.1.3, options choisies
  pour réécrire le moins de lignes possible) : 4 espaces, `T* p`, retours à la ligne de
  l'auteur gardés, formes courtes sur une ligne, `#include` non triés. Son en-tête dit ce qui
  diffère encore d'un fichier à l'autre (alignements des jeux, `alarm_clock.cpp` en 2 espaces).
- **Hook pre-commit `clang-format-lignes-modifiees`** : `git-clang-format --diff --staged` ne
  contrôle que les lignes indexées et ne réécrit rien ; il échoue avec le diff à appliquer.
  Fichiers générés, `tab5_tokens.h` et la table de questions de Trial Poursuite exclus ; en CI,
  rien n'est indexé, il ne contrôle donc rien.
- **`.editorconfig`** : UTF-8 sans BOM, saut de ligne final, 4 espaces (C++, Python), 2 (YAML,
  JSON, Jinja, HTML, SVG), fins de ligne laissées à git (`*.sh` en LF).
- **`CHANGELOG.md` allégé** (6 252 → environ 1 500 lignes) : les versions 3.0.0 à 3.6.0 vont
  dans `docs/changelog/CHANGELOG-3.0-3.6.x.md`, les 2.x dans `docs/changelog/CHANGELOG-2.x.md`,
  recopiées telles quelles (seuls 18 liens relatifs recalés pour ce dossier) ; des renvois
  en bas de ce fichier. `tests/test_version.py` lit toujours la première entrée `## [X.Y.Z]`
  d'ici.

### 2026-10-08 — Réglages en quatre pages, la console devient la page Système

- **Le popup « Réglages » a quatre pages** : **Écran** et **Apparence** (les deux cartes
  d'avant, chacune sur toute la largeur), **Batterie** (nouvelle) et **Système** (l'ancienne
  console système, `console_sys.yaml`, qui n'est plus un popup à part). Les noms des pages sont
  en haut, à côté du titre, la page montrée allumée (`reglages_onglet.yaml`, ×4). Une tape sur
  un nom montre sa page ; glisser à gauche ou à droite dans le popup montre la suivante ou la
  précédente, en boucle, sans transition. Le popup garde le geste (il ne remonte pas à
  `page_main`, l'accueil ne change pas de page derrière), un glissement parti d'un curseur ne
  bouge que le curseur, et la tape au bout d'un glissement ne choisit rien
  (`ui_appui_glisse()`). Changer de page ou fermer le popup annule une confirmation ouverte.
- **Page Batterie** : la limite de charge (100 % / 80 %), l'économie d'énergie (Jamais, Sur
  batterie, Toujours) et « Batterie montée » (Oui / Non) ; en lecture seule, l'état (Sur
  batterie, En charge, Sur USB, Pas de batterie détectée), le niveau, la tension et la
  consommation, repeints seulement pendant que la page est montrée
  (`reglages_batterie_peindre()`).
- **Ouverture** : un tap sur l'engrenage ouvre la page Écran, son appui long la page Système.
  Côté Home Assistant rien ne change de nom : l'option « Console système » de « Aller à
  l'écran » et les clés du blueprint ouvrent la page Système (`tab5_ecran_ouvrir`,
  `check_tab5_registry.py` : alias de l'option vers la fenêtre « Réglages »). Les
  rafraîchissements de la console ne tournent que Réglages ouverts sur la page Système
  (`reglages_page_visible(REGLAGES_PAGE_SYSTEME)`).
- Textes nouveaux dans les sept langues ; `tools/test_alarm_clock.cpp` (page voisine, textes
  de la page Batterie), `tests/test_reglages.py` (pages, geste, gardes de la console),
  `tools/rendu/ecrans.py` (une capture par page). Notice, `docs/screens.md`,
  `docs/installation/settings.md`, `docs/architecture.md`. Pas encore essayé sur la tablette.

### 2026-10-08 — Rangement de `Tab5/` en sous-dossiers

Aucun changement visible ni de comportement : le firmware généré est le même, seuls les
chemins changent.

- **Les 116 fichiers de la racine de `Tab5/` rangés en quatre dossiers** (`git mv`,
  historique gardé) : `socle/` (le C++ pur, compilé et testé sur PC sans ESPHome ni LVGL),
  `ecran/` (la couche LVGL, dont `tab5_custom.h`), `jeux/` (les huit consoles et
  `game_common.h`), `paquets/` (les paquets ESPHome : `tab5-*.yaml`, `ecran-*.yaml`,
  `publication-*.yaml`). Les trois polices d'icônes et la licence de ChessPieces rejoignent
  `fonts/`. Restent à la racine : `README.md`, `user_entities*.yaml`, `tuiles_icones.yaml`.
  La table de correspondance est dans `CARTOGRAPHIE_TAB5.md` (§ 1) et dans `Tab5/README.md`.
- **Aucun `#include` ne change** : ESPHome copie à plat chaque fichier de `includes:`, d'où
  des noms uniques entre dossiers. Un paquet inclut ses composants par `../ui_components/`.
- **Outils et tests** : `tools/tab5_sources.py` trouve une source où qu'elle soit rangée
  (les globs sur la racine de `Tab5/` ne trouveraient plus rien) ; `tests/test_rangement.py`
  garde le rangement (racine vide, noms uniques, `includes:` existants, `socle/` pur). Les
  compilations g++ de la CI prennent `-I Tab5/socle` et `-I Tab5/jeux`.
- **Un en-tête par module** : `tab5_custom.h` (1 583 lignes) devient l'en-tête parapluie qui
  inclut 22 en-têtes de `Tab5/ecran/`, un par module (`tab5_forecast.h`, `tab5_clim.h`,
  `tab5_zones.h`… à côté de leur `.cpp`). Les déclarations sont recopiées telles quelles (mêmes
  lignes, même ordre dans chaque module) ; les lambdas et les unités n'incluent toujours que
  `tab5_custom.h`. Les deux configurations listent les nouveaux en-têtes sous `includes:`.
  Les tests qui y cherchaient une déclaration lisent `contrat()` (`tools/tab5_sources.py`),
  la règle 12 des règles de code aussi (mêmes 210 fonctions publiques).
- **`AGENTS.md`** : l'état vrai de `tab5_maj_planning` (obsolète, aucun appelant ni dans le
  dépôt ni dans le Home Assistant de l'auteur, gardé jusqu'à une future version majeure).

Les anciens chemins restent tels quels dans les entrées plus anciennes de ce fichier.

### 2026-10-08 — Météo : mention « prévisions périmées » au-dessus des tuiles

- **Prévisions qui n'arrivent plus, dites à l'écran** : après l'incident du 07-08/10
  (Météo-France figée de 21 h 04 à 11 h 34, puis indisponible), la tablette retient
  l'heure de chaque poussée des prévisions (jours ou heures). Sans poussée depuis plus de
  30 min (Home Assistant en pousse toutes les 10 min), une ligne discrète s'affiche
  au-dessus des tuiles, à droite, avec une horloge : « Prévisions de 11 h 42 »,
  « Prévisions d'hier 21 h 04 » ou « Prévisions vieilles de 3 jours ». En temps normal,
  rien ne change ; rien non plus avant la première poussée, heure non réglée, ni en mode
  appareils. Contrat inchangé (aucune variable ajoutée). Limite : une source figée que
  Home Assistant continue de pousser n'est pas vue par la tablette, c'est à HA de cesser
  de la pousser. Nouvelle scène du rendu hors tablette, `accueil-previsions-perimees`.

### 2026-10-08 — Code des tuiles regroupé et découpé (lot L7)

Aucun changement visible voulu ; le code des tuiles se lit et se modifie en un seul endroit par
sujet.

- **Une seule source** pour : la clé d'une tuile ou d'une pièce (`tuile_cle()`, `piece_cle()`,
  `tab5_modele_ha.h`) ; ce que fait chaque type de tuile au toucher, à l'appui long et dans la
  roue (table `kGestes[]`, une ligne par type) ; l'ouverture et la mise à jour des trois popups
  d'une tuile (`PopupTuile`) ; les teintes des préréglages de lampe, lues par le popup lumière
  et par la roue (`lampe_teinte()`).
- **La roue ne repeint plus un bouton dont l'aspect n'a pas changé** : chaque bouton garde le
  dernier aspect posé.
- **Fichiers découpés** : `tab5_tuiles.cpp` (2 742 lignes) devient `tab5_tuiles.cpp` (modèle,
  NVS, dessin, gestes), `tab5_tuiles_popups.cpp`, `tab5_tuiles_roue.cpp` et l'en-tête privé
  `tab5_tuiles_priv.h` ; la clim sort de `tab5_cards.cpp` dans `tab5_clim.cpp` ; le câblage de
  la roue sort de `tab5-tuiles.yaml` dans `tab5-roue.yaml` (script `tab5_roue_ui`, lancé par
  `tab5_tuiles_ui` au même moment qu'avant). Code déplacé tel quel ; tests repointés.

### 2026-10-08 — Batterie : plus de souffle sans batterie, limite de charge, consommation

- **Un léger souffle continu sortait d'une tablette sans batterie** (entendu par l'auteur le
  08/10, micro coupé ou pas) : depuis la 3.6.0 (#303), le chargeur était allumé à chaque
  démarrage et chargeait dans le vide. Le couper l'a fait taire (essai sur la tablette, firmware
  de test). Le chargeur est maintenant commandé chaque seconde (`Tab5/tab5_batterie.h`) : allumé
  30 s au démarrage (pour réveiller une batterie dont la protection a coupé), puis coupé quelques
  secondes pour lire la tension. Sans batterie, l'INA226 lit alors 1,83 à 1,94 V (mesuré) ; sous
  3,0 V, pas de batterie, le chargeur reste coupé. Une batterie glissée tablette allumée est vue
  à la lecture suivante (60 s). Avec une batterie, une lecture chargeur coupé toutes les 10 min,
  et tout de suite si la tension tombe sous 6 V (batterie retirée). « Tab5 Batterie détectée »
  remplace son ancienne règle (une lecture sous 6 V dans les 10 dernières minutes, chargeur
  allumé).
- **« Tab5 Limite de charge »** (100 % par défaut, ou 80 %) : pour une tablette branchée en
  permanence, la charge s'arrête à 80 % et reprend à 70 %. Pas encore essayé avec une batterie :
  chargeur arrêté et USB branché, on ne sait pas encore si la tablette tourne sur l'USB ou sur sa
  batterie.
- **« Tab5 Consommation »** (W) : tension × courant de la batterie quand la tablette tourne sur
  elle ; aussi sur la ligne « Batterie » de la console système (« 78% · 3.1 W »), à la place de
  la tension.
- **Batterie faible sur le téléphone** : sur batterie, sous 20 % puis sous 10 %, la tablette
  émet `esphome.tab5_batterie_faible` (niveau, seuil), une fois par seuil ; la garde
  « batterie faible » de `packages/tab5_health.yaml` en fait une notification persistante et une
  notification sur le téléphone.
- `tools/test_alarm_clock.cpp` (chargeur, limite, alerte, console), `tests/test_batterie.py`
  (câblage YAML) ; `docs/troubleshooting.md`, `docs/hardware.md`.

### 2026-10-08 — YAML du firmware rangé (lot L8 de l'audit du 07/10)

Rien ne doit changer à l'écran ni pour Home Assistant (mêmes entités, mêmes options, mêmes
codes gardés en mémoire) : la configuration résolue ne diffère que par l'endroit où le code
est écrit, deux identifiants ajoutés et l'ordre de deux widgets qui ne se chevauchent pas.
- **Gabarits au lieu de copies** : tuiles journalières (`forecast_day_card.yaml`,
  `forecast_day_body.yaml`), cartes du mode HA (`switch_card.yaml`), boutons du haut
  (`bouton_haut.yaml`), boutons des panneaux centraux (`central_bouton.yaml`) ; un seul bouton à
  pas pour le réveil et les Réglages (`bouton_pas.yaml`, à la place de deux) ; le chrome des
  popups se ferme par `popup_id` au lieu d'une lambda écrite deux fois.
- **Navigation à part** : `Tab5/tab5-navigation.yaml` réunit le registre des fenêtres,
  `tab5_ecran_ouvrir`, « Aller à l'écran » et « Écran courant ». Le registre donne aussi
  l'ouverture de chaque écran : le `switch` qui recopiait la liste des écrans disparaît.
- **Logique en C++** : humidité des plantes (`pots_humidite_maj()`, un script au lieu de cinq
  copies), tap-to-wake et cadence de l'IMU (plus de `static` dans une lambda), retour
  automatique à l'accueil (`retour_auto_tick()`).

### 2026-10-08 — Tests et CI de l'audit du 07/10 (lot L9)

- **Les vrais moteurs d'échecs et de dames sont testés en CI**, sous ASan + UBSan, par le job
  `python` : `tools/test_chess_engine.cpp` (`chess_ai.cpp` contre la suite perft) et
  `tools/test_draughts_engine.cpp` (le moteur de `draughts_game.cpp` contre les perft 10×10 et
  8×8 et les règles). Ils font foi ; les miroirs Python restent pour un poste sans g++, tenus égaux
  par `tests/test_moteurs_hote.py`. Le miroir Python du Go, en double, est retiré : ses cas
  manquants sont passés dans `tools/test_go_engine.cpp`.
- **`pytest` en 47 s au lieu de 175 s** sur le poste de dev : blueprint et tableau de bord lus
  une fois par session, gabarits Jinja compilés une fois, YAML lu par libyaml. `tests/commun.py`
  et `tests/conftest.py` remplacent les chargeurs, lectures et `sys.path.insert` recopiés dans
  53 fichiers. `pyserial` est installé : `tests/test_capture_serie.py` ne saute plus en CI.
- `tests/test_contrat.py` ne lit plus `HomeAssistant_Config/rendered/` (ignoré par git) : en local,
  les mêmes fichiers qu'en CI.
- **Rendu hors tablette compilé une fois** (tâche `compiler`, programme passé aux neuf tâches
  `rendu`) ; paquets pip en cache dans le rendu et les sanitizers. Dependabot suit aussi
  `tools/site`, `tools/demo` et `tools/publication`.

### 2026-10-08 — Home Assistant : factorisation des packages et du blueprint (lot L11)

- **Une macro « la tablette »**, `custom_templates/tab5_tablette.jinja` : « une tablette est
  connectée » (recopié 7 fois), la garde d'origine des événements `esphome.tab5_*` (6 fois) et le
  capteur « HA API Status » de la tablette n'existent plus qu'ici, importés par les packages et
  le blueprint.
- **Blueprint « Tab5 — emplacements »** : les déclencheurs des 5 pièces et des 3 lignes de la
  rangée sont écrits par `tools/gen_blueprint_emplacements.py` (`--check`, tenu par
  `tests/test_blueprint_genere.py`) ; la liste des déclenchements qui poussent tout (recopiée
  9 fois) devient la variable `tout_pousser`, l'action de la clim (2 fois) une ancre YAML. Mêmes
  entrées, mêmes déclencheurs.
- **Volet à course simulée** (optionnel) : un seul script chronomètre la course,
  `script.tab5_volet_course`, appelé par la tablette et par le suivi des commandes directes ; sa
  durée est le nouveau réglage « Tab5 · course du volet » (`number.tab5_course_du_volet`, mémoire
  `input_text.tab5_memoire_course_volet`), **26 s par défaut, comme avant**. Une commande de la
  tablette pendant une course lancée d'ailleurs annule maintenant ce premier chrono.
- **Syntaxe actuelle de HA** (`triggers:`, `conditions:`, `actions:`, `- trigger:`) dans tous les
  packages, l'optionnel et le snippet, et des noms « Tab5 — … » pour les automatisations et
  scripts (« Tab5 — poussée complète de l'écran », « Tab5 — santé : … », « Tab5 — volet : … »).
  Les `id:` ne changent pas : les entity_id des automatisations restent les mêmes.
- **Moins de calculs** : le compte des entités indisponibles de la carte Santé
  (`sensor.tab5_unavailable_count`) se fait par filtres et toutes les 5 min au lieu de 2 ; les
  listes « Tab5 · … » (agendas, téléphone, présence, sources météo, TV, volet) ne se recalculent
  plus sur une mise à jour du registre qui ne touche que capabilities, supported_features, options
  ou suggested_object_id (419 des 485 événements du registre chez l'auteur en une semaine).
- **Alerte « entités indisponibles »** : la liste gardée en mémoire est bornée à 100 entités
  (elle grossissait sans fin) ; le nombre affiché par la tablette compte toujours toutes les
  entités.
- **Repli de la météo** (`custom_templates/tab5_meteo.jinja`, nouveau) : tant que l'entité
  choisie dans « Tab5 · source des prévisions » est indisponible, ou sans relève depuis 2 h
  (`last_reported`), la météo actuelle et les prévisions viennent d'une autre entité météo qui
  répond (OpenWeatherMap, Météo-France, puis n'importe laquelle), et reviennent toutes seules à
  la source choisie ; la liste garde le choix. Sans aucune entité qui répond, les poussées météo
  sont sautées : l'écran garde ses dernières prévisions au lieu de « indisponible » à 0 °C (le
  08/10, Météo-France indisponible de 11 h 34 à 11 h 42 chez l'auteur, OpenWeatherMap marchait).
  Après 5 min de repli, la carte centrale affiche une alerte « Météo : OpenWeatherMap utilisé,
  Météo-France indisponible depuis 11 h 34 », retirée au retour de la source ; la vue Santé du
  tableau de bord montre la source utilisée. Nouveaux attributs de `sensor.tab5_meteo` :
  `entite_effective`, `nom_effectif`, `nom_choisi`, `repli`, `repli_depuis`, `repli_heure`.
  Pluie et vigilances inchangées.
- Retiré : le snippet obsolète `snippets/tab5_alerts_dismissed_input_text.yaml`. Documenté (EN et
  FR) : l'alerte « erreur de rendu » demande `system_log: fire_event: true` dans
  `configuration.yaml`.

**À faire en mettant à jour** (fichiers Home Assistant seulement, pas de firmware) :

1. Copier **d'abord** `custom_templates/tab5_tablette.jinja` et `custom_templates/tab5_meteo.jinja`
   (nouveaux), `custom_templates/tab5_alertes.jinja` et `custom_templates/tab5_dashboard.jinja`,
   puis **Outils de développement → YAML → Modèles Jinja
   personnalisés** (`homeassistant.reload_custom_templates`). Avant ce rechargement, les packages
   et le blueprint qui importent la macro échouent à leur condition (la poussée s'arrête).
   L'intégration HACS recharge les modèles en premier.
2. Puis les packages (`tab5_push`, `tab5_health`, `tab5_reveil`, `tab5_alerts`, `tab5_calendar`,
   `tab5_reglages`, `tab5_evenements`, `tab5_meteo_sources`, `tab5_historique`, `tab5_tv`), le blueprint
   `tab5_emplacements.yaml` (réimporter un blueprint importé par son URL) et, s'il est installé,
   `optionnel/volet_serre_tracking.yaml`.
3. Recharger **Entrées de texte**, **Entités de modèle**, **Scripts** et **Automatisations** (ou
   redémarrer HA). Nouvelles entités, seulement avec le volet optionnel :
   `number.tab5_course_du_volet`, `input_text.tab5_memoire_course_volet`,
   `script.tab5_volet_course`. Aucune entité retirée.
4. Pour voir la ligne « Source météo » de la vue Santé : régénérer le tableau de bord du Tab5.

### 2026-10-08 — Données générées des thèmes et polices figées (lot L12)

Rien ne change à l'écran : mêmes couleurs, mêmes formes, mêmes fichiers de police.

- **Retoucher un thème ne recompile plus tout le firmware.** Le catalogue des 21 thèmes
  (`THEMES[]`, ~2 800 lignes générées) quitte `Tab5/tab5_tokens.h`, inclus par 26 unités C++ dont
  deux jeux, pour `Tab5/tab5_themes_data.h`, inclus par `tab5_theme.cpp` et `tab5_reglages.cpp`
  seulement. `tab5_tokens.h` passe de 3 125 à 273 lignes et ne garde que `PALETTE_SOMBRE`, que
  `THEMES[0]` reprend. `tools/gen_themes.py` écrit le nouveau fichier ; `tests/test_themes.py`
  vérifie qu'il est à jour et que personne d'autre ne l'inclut.
- **Tables des thèmes liées à leur nombre** : des `static_assert` générés lient `kPolices`,
  `kFormesDebut` et leurs sentinelles à `THEME_COUNT` ; une table périmée ne compile plus.
- **Rôle de couleur `ICON_MUTED` retiré** : rien ne le lisait (66 rôles). Un test échoue
  désormais sur un rôle que rien ne lit.
- **Polices figées dans le dépôt** : Roboto 700 et les 20 polices des thèmes étaient des
  `gfonts://`, redemandées à Google Fonts chaque jour par ESPHome ; une nouvelle version chez
  Google changeait le firmware sans un mot. Les fichiers (`Tab5/fonts/`, 3,6 Mo, SIL OFL 1.1,
  copyrights dans `Tab5/fonts/OFL.txt`) sont ceux que Google servait, octet pour octet ;
  `tests/test_polices_themes.py` vérifie leur empreinte et qu'aucune police n'est plus
  téléchargée à la compilation.
- Polices des thèmes en mémoire (PERF-2 de l'audit) : aucun glyphe retiré, et toutes restent
  compilées. La police de la date dessine aussi les textes libres de 45 px (réponse vocale,
  alertes, planning, carte centrale) dans les 7 langues, et ESPHome ne sait pas charger une police
  à la demande (raisons dans l'ADR-0029).

### 2026-10-08 — Écritures gardées et petites optimisations (lot L10)

Aucun changement visible : les mêmes valeurs, écrites seulement quand elles changent (en LVGL
9.5.0, `lv_label_set_text()` et `lv_obj_set_style_*()` invalident même à valeur égale). Gain non
mesuré sur la tablette.
- **Popup Température** : le titre n'est plus écrit deux fois à chaque repeint ;
  `peindre_graphique()` (≈ 200 lignes) découpé en onze étapes nommées, mêmes calculs.
- **Repeints sans réécriture égale** : lignes du popup Alertes, bordures des boutons
  (`highlight_button_border()` : vues Température et Énergie, bascules du réveil, assistant),
  popup et sonnerie du réveil, grille du calendrier, titres de page, bandeau info et pluie,
  pastilles des pages ; `set_label_text_utf8()` ne copie plus son texte.
- **Tick de 1 s** : il ne relance plus le script du registre des fenêtres une fois la liste
  remplie. Écran éteint, il referme toujours les popups après l'inactivité, pour se rallumer
  sur l'accueil.
- **Une seule recette** : les pastilles sous l'horloge passent par `pagination_afficher()` ; la
  garde « appui au bout d'un glissement » devient `ui_appui_glisse()` ; `ui_style_num()` et
  `ui_style_couleur()` rejoignent la boîte à outils d'`AGENTS.md`.

### 2026-10-08 — Intégration HACS : trois défauts de l'audit du 07/10 (lot L11)

- **Fichiers refusés plus réessayés à chaque démarrage** : quand la vérification de la
  configuration refusait les fichiers d'une version, l'intégration refaisait le même essai à
  chaque redémarrage de Home Assistant, avec une sauvegarde de plus, identique, qui poussait
  les utiles hors des 5 gardées. Ces fichiers-là ne sont plus réessayés que sur demande
  (*Configurer* → « Réinstaller maintenant les fichiers de cette version ») ou quand HACS en
  apporte d'autres, et une sauvegarde identique à la précédente n'est plus refaite.
- **Fichiers copiés à la main signalés** : une première installation remplaçait sans le dire
  des fichiers du Tab5 déjà copiés à la main. Nouvelle réparation « Tab5 : des fichiers déjà
  là ont été remplacés », qui les liste et nomme la sauvegarde ; la notification les nomme
  aussi.
- **Mise à jour de la tablette réessayée** : l'attente du firmware était oubliée avant même
  de lancer la mise à jour, et rien ne réessayait un OTA raté. Elle reste jusqu'à ce que la
  tablette donne la nouvelle version ; sinon nouvel essai toutes les 15 minutes, 3 en tout,
  puis la réparation « Tab5 : la tablette n'est pas passée en X » dit de l'installer à la main.

### 2026-10-08 — Garde-fous de l'audit du 07/10 (lot L6)

- **Six règles de plus** dans `tools/check_tab5_code_rules.py` (jouées par `pytest`), chacune
  falsifiée sur une copie du firmware : aucun nouvel appel `lv_*` ni `static` modifiable dans une
  lambda YAML (les existants sont listés, plafonds exacts) ; pas de copie de chaîne dans un
  `on_value:` ; chaque fonction de `tab5_custom.h` appelée hors de son fichier (10 exceptions
  listées) ; `nullptr` et tag de journal `tab5.<module>` (corrigés dans trois unités) ; aucun mot
  courant sans accent dans un texte de l'écran.
- **Fuzz des sanitizers** : une graine par service déclaré (Énergie et historique d'énergie
  ajoutés, rangée `hp`/`hd`/`hLI` et tuile − / + `rN`), tenue par `tests/test_sanitizers.py`.
- **`tab5_maj_planning` obsolète** (décision de l'auteur) : gardé pour compatibilité, retiré dans
  une future version majeure ; ses variables ne changent pas.

### 2026-10-08 — Changer de thème ne fige plus l'écran

- **Changer de thème, de mode clair ou sombre, ou passer à la nuit en mode Auto bloquait la
  tablette 3,2 s**, quel que soit le thème (mesuré le 07/10 sur la tablette ; le nouveau thème
  apparaissait 3,7 à 3,9 s après la demande). Ce n'étaient ni les ombres ni les dégradés, qui ne
  coûtent que 25 à 32 ms de plus par image sur les deux « Relief » : chacun des 69 styles
  repeints reparcourait les ~1 600 objets de l'écran et remettait en page tous les textes posés
  sous ses objets. Les couleurs sont maintenant reposées d'un coup puis l'écran est redessiné
  une fois ; seuls les objets qui portent l'un des 12 styles de forme (319 comptés sur la
  tablette) sont remis en page, une seule fois. Les polices (3 styles) et les cases du calendrier
  (5) gardent leur rafraîchissement par style. Prototype mesuré sur la tablette : 0,18-0,19 s,
  nouveau thème affiché en 0,44-0,51 s,
  même géométrie qu'avant sur 5 thèmes. Le démarrage dans un thème autre qu'Ardoise, qui coûtait
  ~2,95 s de plus, passe par le même chemin (gain non mesuré).
- `tools/gen_themes.py`, `theme_formes()` (`Tab5/tab5_theme.cpp`) ; `tests/test_themes.py`
  vérifie que rien d'autre qu'une couleur n'est posé pendant que le rafraîchissement est coupé.

### 2026-10-07 — Home Assistant : correctifs de l'audit du 07/10, plus de `is_primary_active`

- **Plus de garde-fou `input_boolean.is_primary_active`** (décision de l'auteur) : ce reste de
  l'ancienne bascule entre deux Home Assistant ([ADR-0008](docs/decisions/0008-single-ha-instance.md))
  conditionnait toutes les poussées ; resté à `off`, il figeait l'écran sans aucune erreur. Il
  disparaît, avec l'automatisation `force_primary_active_on_boot`, sa garde de santé « OFF depuis
  5 min », sa tuile, sa ligne et sa pastille du tableau de bord. « MAJ Écran » ne fait plus que
  relancer la poussée complète.
- **« Éteindre les lumières » d'une pièce épargne une lampe réglée sur « Confirmer »**, comme
  « Allumer seulement » et « Lecture seule » : une commande confirmée ne passe jamais par un seul
  toucher ([ADR-0036](docs/decisions/0036-quick-action-wheel.md)). « Tout éteindre » de l'ancien
  mode (lumières 1 à 3) suit la même règle.
- **Bandeaux d'alerte** : le libellé est coupé à 100 caractères et le message ne dépasse plus
  jamais 1 024 octets ; une alerte au texte très long faisait refuser tout le message par la
  tablette, et les quatre bandeaux restaient sur les anciennes alertes.
- **Pluie dans l'heure** : un changement du code ou des barres de pluie ne relance plus toute la
  poussée complète (calendrier, prévisions, ~8 envois) ; la poussée légère envoie le code et les 9
  barres, seulement quand l'un des deux change. La poussée complète renvoie encore les barres toutes
  les 10 minutes.
- **Jour travaillé des tuiles météo** calculé comme dans le calendrier (macros de
  `custom_templates/tab5_calendar.jinja`) : le jour de fin d'un événement « journée entière » n'est
  plus compté, les jours du milieu d'un événement de plusieurs jours le sont ; le lendemain d'une
  garde de nuit n'est plus marqué travaillé. Le réveil lit aussi les événements par ces macros.
- **Plus de poussée perdue** : la poussée complète (`queued`, 3 en file) et celle des rendez-vous
  (`queued`, 2) ne laissent plus tomber une reconnexion de la tablette arrivée pendant un passage ;
  le blueprint garde 50 déclenchements en file au lieu de 25 (éteindre toute la maison en produit
  ~30).
- **Popup Énergie** : les statistiques du recorder ne sont plus redemandées qu'au plus toutes les 5
  minutes tant qu'il reste ouvert (au lieu de toutes les 5 s) ; l'instantané suit toujours.
- **Calendrier** : une année ou un mois qui n'est pas un nombre arrête la réponse sans erreur.
- **Mise à jour** : remplacer les packages, le blueprint et `custom_templates/` (le package
  principal et celui du réveil importent maintenant `tab5_calendar.jinja`), recharger les modèles
  Jinja puis la configuration YAML. L'entité `input_boolean.is_primary_active` et les
  automatisations « Force Primary Active on Boot » et « Tab5 Santé — is_primary_active OFF depuis
  5 min » disparaissent : si Home Assistant les garde en « indisponible », les supprimer dans
  Paramètres → Entités. Refaire le tableau de bord (étape 7 du guide). Rien à changer sur la
  tablette. Non essayé sur un vrai Home Assistant.

### 2026-10-07 — Correctifs de l'audit du 07/10 (firmware)

- **La roue d'actions rapides dit « Détails »** au lieu de « Réglages » pour ouvrir la fenêtre
  complète de l'appareil (décision de l'auteur) : « Réglages » restait le nom de la fenêtre de
  l'engrenage. « Details » en anglais, allemand et néerlandais, « Detalles », « Dettagli » et
  « Ayrıntılar » en espagnol, italien et turc. [ADR-0036](docs/decisions/0036-quick-action-wheel.md) et notice à jour.
- **La même lampe affiche le même pourcentage partout** : à 127/255, la carte disait 50 % et le
  popup lumière 49 %. Carte, popup (arc compris) et roue arrondissent de la même façon, et une lampe
  allumée n'affiche jamais 0 %.
- **Popup lumière ouvert quand Home Assistant renvoie les tuiles** : ses lignes suivent les nouvelles
  définitions, comme les popups volet et appareil ; il se ferme si la pièce n'a plus de lumière.
- **Énergie** : l'après-midi, la vue « heures » montrait 23 barres au lieu de 24 (la dernière heure à
  venir était perdue) et leur espacement changeait ; corrigé. L'année de début est bornée
  (1970 à 2200) comme dans l'historique des températures.
- **Bandeaux d'alerte** : leur texte passe par le même filtre que l'historique des alertes (aucun
  caractère que les polices n'ont pas, plus de carré vide).
- **Accents** : « Journée » (onglet du jour des prévisions), « Redémarrage... » (console) et
  « Réflexion... » (assistant vocal).
- **Console** : plus de 0/0 converti en entier quand la mémoire totale vaut 0 (tablette virtuelle),
  ni de volume NaN converti en entier.
- Vérifié sans changement : 10 bandes de prévision mini/maxi suffisent (7 jours au plus, par jour
  seulement) ; un test le garde.

### 2026-10-07 — Tuile − / + : « Son de la tablette »

- **La dernière ligne de la liste de la tuile − / + dit ce qu'elle règle**
  ([ADR-0033](docs/decisions/0033-adjustable-tile.md), [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) :
  « Tablette » et l'icône d'une tablette ne disaient pas que c'était le volume de la tablette, qu'on
  réglait de 0 à 100 % sans rien voir changer. Elle s'appelle « Son de la tablette » (Tablet volume)
  et montre un haut-parleur, barré à 0 % ou quand le son est coupé ; le muet du popup de l'assistant
  la repeint. Rien ne change dans Home Assistant.

### 2026-10-07 — Roue d'actions rapides à deux anneaux

- **Deux anneaux dans une roue** ([ADR-0036](docs/decisions/0036-quick-action-wheel.md), mis à
  jour) : l'appui long d'une lumière, d'un volet ou d'une clim pose un moyeu sur la tuile (icône,
  état, jauge en arc de la luminosité, de la position ou de la consigne, nom) et, au-dessus, un
  premier anneau : **Maison** (le popup Maison, absent quand la roue s'ouvre depuis lui), les
  commandes, les familles de réglages marquées d'un point, **Réglages** (le popup complet, ex-« ⋯ »).
  Toucher une famille déplie ses choix sur un second anneau : luminosité 10 / 25 / 50 / 75 / 100 %,
  blancs (chaud, crème, froid) et couleurs (rouge, orange, or, vert, bleu, violet) d'une lampe à
  couleur ; position 25 / 50 / 75 % d'un volet ; modes, consigne (± 2 pas) et options (Éco, Boost,
  Silence, Oscillation, Brise) d'une clim. Toucher ailleurs ou le moyeu replie, puis ferme ; un choix
  ferme la roue (décision de l'auteur).
- **Plus de lampes** : une lampe sans variateur a sa roue (Allumer, Éteindre, les liens) ; la règle
  « moins de trois commandes : pas de roue » disparaît. Toujours aucune roue avec l'option `k`.
- **Aspect** : voile des popups à 60 % au lieu du disque de verre, bandes de verre sous les anneaux,
  boutons ronds de 72 px au verre des popups, état courant en verre teinté avec liseré et halo,
  pastilles de couleur en dégradé, liens en contour avec leur mot. Ouverture et dépliage secs.
- **Pas de famille « Ventilation »** : HA ne pousse pas la liste des vitesses ; la bascule Silence du
  popup est dans les options. Aucune commande nouvelle : celles des tuiles et des popups lumière et
  clim (`esphome.tab5_action`), rien à changer dans Home Assistant.
- Rendu : `roue-lampe` (luminosités dépliées), `roue-lampe-couleurs` (nouveau), `roue-volet`
  (positions), `roue-clim` (modes) ; les popups de tuile s'ouvrent par « Réglages ».
  `tests/test_roue.py` réécrit (géométrie des deux anneaux pour toutes les tuiles, choix et commandes
  comparés aux popups). Non essayé sur la tablette.

### 2026-10-07 — Images du README et du site, HACS dans le dépannage

- **Photo d'en-tête** : la photo de la tablette de l'auteur, prise le 07/10/2026 (sans
  métadonnées), remplace celle de juillet en tête du README (donc de l'accueil du site), en haut de
  « Écrans » et dans l'image de partage du site (`tab5_social_preview.jpg`).
- **Galerie du README** : les 12 photos de juillet (interface d'alors, en français même dans la
  partie anglaise) laissent la place à 18 écrans dessinés par le firmware lui-même sur un PC
  (rendu hors tablette, données de démonstration), chacun dans un autre thème, en anglais dans la
  partie anglaise et en français dans la française (`docs/images/galerie/`).
- **Tour animé** : refait depuis le rendu de `main` (thème par défaut), 11 écrans dont la maison,
  la roue d'actions et l'énergie, un par langue (`tab5_ui_tour_en.webp`, `tab5_ui_tour_fr.webp`).
- **Planche des thèmes** : six accueils (Pixel, Bonbon, Sorbet, Béton brut, Zen Sumi, Capsule),
  deux en mode pièces, quatre avec une alerte différente sur la carte centrale, en anglais
  (`tab5_themes_en.jpg`) et en français (`tab5_themes.jpg`) ; README, « Écrans » et « Réglages ».
- **« Écrans »** (`docs/screens.md`) : les photos de juillet deviennent les rendus de la notice,
  les trois jeux montrés aussi dans la partie anglaise. Les anciennes photos restent dans
  `docs/images/` pour le kit de presse.
- **HACS** : nouvel incident « Ajout de Tab5 dans HACS : « Dépôt introuvable », ou une boîte
  vide » (dernière release complète avant la 3.7.0 ; textes de HACS pas encore chargés au premier
  passage par le lien, vu avec HACS 2.0.5), et un renvoi depuis l'étape 2 de « Avec HACS ». Ajout du
  dépôt par le bouton essayé le 07/10 avec la 3.7.0 stable : HACS propose la v3.7.0 ;
  téléchargement et ajout de l'intégration pas encore essayés par l'auteur.

### 2026-10-08 — Firmware : socle C++ commun (lot L5 de l'audit du 07/10)

- **Une seule copie** de la lecture bornée d'un payload (`Tab5/tab5_champs.h`, six variantes
  retirées), des dates et des heures « HH:MM » (`tab5_core`), de la géométrie des popups
  (`Tab5/tab5_geometrie.h`) et de ce que partagent les tuiles de pièce et la tuile − / +
  (`Tab5/tab5_modele_ha.h`). Testées sur PC par `tools/test_tab5_socle.cpp` (g++ en CI).
- **Un payload refusé laisse une ligne dans le journal** (`tab5.<module>`, raison et taille, jamais
  le contenu), avec un plafond de taille commun de 16 Ko. Rien d'autre ne change à l'écran.

## [3.7.0] — 2026-10-07

De `v3.6.0` à aujourd'hui : quarante-neuf pull requests (#326 → #375 ; #369 est une discussion),
dont quatre de pré-release (#338, #343, #359, #374) et quinze nées des demandes et des essais de
@husyildiz dans la discussion #278 (#330 à #334, #341, #342, #347 à #350, #361, #363, #366,
#368) — merci à @husyildiz —, et celle de la release. Merci aussi à Jiuhai (@poonjh) pour ses
notes sur le co-processeur WiFi et l'alimentation (#372). Même code que la
[v3.7.0-rc.5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.7.0-rc.5)
(firmware et fichiers HA), hors numéro de version.
- **Les fichiers Home Assistant en un clic, par HACS** (#363, #365, #371, #375, [ADR-0035](docs/decisions/0035-hacs-integration-ha-files.md)) :
  nouvelle intégration « Tab5 ». Le dépôt s'ajoute à HACS comme dépôt personnalisé (un bouton de
  la page d'installation l'ouvre dans HACS). À chaque version, après le redémarrage que demande
  HACS, elle sauvegarde vos fichiers, pose les nouveaux, vérifie la configuration (retour en
  arrière si elle casse) et la recharge, puis lance la mise à jour de la tablette (option, cochée
  par défaut). `tab5_home_assistant.zip` et la mise à jour à la main restent.
- **Des fenêtres comme dans un tableau de bord Home Assistant** : une roue d'actions rapides à
  l'appui long d'une lampe, d'un volet ou d'une clim (#366, [ADR-0036](docs/decisions/0036-quick-action-wheel.md)) ;
  un popup « Maison », toute la maison pièce par pièce (#368, [ADR-0037](docs/decisions/0037-house-popup.md)) ;
  le popup d'un appareil (interrupteur, prise, scène…) et des cartes du mode HA dessinées comme
  la carte « tile » (#350) ; un volet dessiné à faire glisser dans le popup du volet (#333,
  #349) ; un popup Température, historique des deux températures et prévision, lu dans les
  statistiques de HA (#354, [ADR-0032](docs/decisions/0032-temperature-history-popup.md)) ; un
  popup Réglages sur la tablette : luminosité, extinction, thème, langue (#345).
- **Accueil** : sous l'horloge, jusqu'à trois lignes de capteurs en plus des plantes (#344,
  [ADR-0031](docs/decisions/0031-row-under-the-clock.md)) ; les − / + de la carte clim règlent
  l'appareil de votre choix (#352, [ADR-0033](docs/decisions/0033-adjustable-tile.md)) ; un
  appui long sur chacun des trois boutons du haut, son écran au choix dans le blueprint,
  « Maison » compris (#345, #364, #373) ; les appareils des pièces sur la météo au choix (#331).
- **Alertes de la carte centrale** (#351, #353, #355, #356, #358, #360, [ADR-0034](docs/decisions/0034-central-card-alerts.md)) :
  une alerte lue ne revient que si elle change, même après un redémarrage de HA ; six listes pour
  choisir ce qui s'affiche, dont une étiquette « Tab5 · alerte » et les piles faibles ; rang
  « 2/6 » et alerte rouge en premier ; l'historique des 20 dernières à l'appui long, et « Tout
  marquer comme lu ».
- **Écran et batterie** : extinction automatique au choix et rallumage à « Okay Nabu » (#342) ;
  un mode économie d'énergie, enclenché tout seul sur batterie (#357) ; une prise à la place de
  la batterie quand il n'y en a pas (#332) ; batterie et charge des deux cœurs dans la console
  système (#348).
- **Énergie** : plusieurs sources solaires additionnées (#330).
- **Tableau de bord HA de la tablette** : vues Réglages et Santé qui expliquent quoi faire, dans
  les sept langues de l'écran (#326, #328, #329) ; blueprint « Tab5 — emplacements » plus
  lisible, une phrase d'aide par champ (#373).
- **Corrigé** : avec le package du volet à course simulée, un volet qu'il ne suivait pas ne
  répondait plus (#341) ; un appui sur le bouton d'alimentation passait pour un plantage (#347).
- **Documentation** (#327, #334 à #337, #339, #340, #346, #361, #362, #367, #370, #372) : un site
  avec menu et recherche, construit depuis `docs/`, dont l'accueil est le README
  ([ADR-0030](docs/decisions/0030-documentation-site.md)) ; le guide d'installation en pages,
  avec des captures de Home Assistant prises par la CI ; une notice d'utilisation, chaque tap et
  appui long ; les deux modes vocaux ; la ST7121 tourne chez un utilisateur ; pourquoi les
  entités restent en français ; le co-processeur WiFi et l'alimentation.

**Ordre de mise à jour** (lu dans le code, pas essayé en entier) : un firmware 3.7.0 avec les
fichiers HA de la 3.6.0 marche, mais le popup Température et l'historique des alertes restent
vides, les nouvelles sections du blueprint manquent, et un appui sur le bouton d'alimentation
déclenche l'alerte « reboot inattendu » ; un firmware 3.6.0 avec les fichiers de la 3.7.0 ignore
les nouvelles clés (rangée, tuile − / +, boutons du haut, rang des alertes).

### À faire en mettant à jour depuis 3.6.0

1. **Home Assistant d'abord**, au choix :
   - **par HACS** (nouveau) : le guide [Fichiers Home Assistant](docs/installation/home-assistant-files.md),
     section « Avec HACS » (ajouter le dépôt par le bouton « Ouvrir Tab5 dans HACS »,
     *Download*, redémarrer Home Assistant, ajouter l'intégration « Tab5 ») ; les versions
     suivantes se font ensuite en un clic ;
   - **à la main**, comme avant : remplacer les fichiers par ceux de `tab5_home_assistant.zip`
     (dont les nouveaux `packages/tab5_historique.yaml` et `custom_templates/tab5_alertes.jinja`,
     et le blueprint), puis recharger toute la configuration YAML et les modèles Jinja
     personnalisés. Un blueprint importé par son URL : le réimporter.
2. **Firmware** : entité « Firmware » dans Home Assistant (avec l'intégration, son option la
   lance toute seule).
3. **Quand vous voulez** : les nouvelles sections repliées du blueprint (« Sous l'horloge »,
   « Tuile − / + », « Boutons du haut », l'énergie en trois sections) et sa case « La seconde
   température est dehors » ; les listes « Tab5 · alertes : … » ; les réglages de l'appareil
   « Extinction auto de l'écran », « Rallumer l'écran à Okay Nabu » et « Économie d'énergie » ;
   le tableau de bord à refaire (étape 7 du guide), pour les nouvelles entités.

### Mesures de la version

- Image du firmware : +196 608 o par rapport à la 3.6.0 (binaires OTA publiés, `st7123` :
  4 591 616 o pour la rc.5, même code, contre 4 395 008 o). RAM statique : 192 602 o (43,2 %)
  contre 180 720 o (40,6 %), lue dans le journal de la publication de la rc.5.
- Sur la tablette de l'auteur : les builds de `main` y ont tourné au fil des merges, les 06 et
  07/10 ; la rc.5 publiée y est installée depuis le 07/10 à 13 h 33 (OTA, version lue par
  l'API), écran rallumé seul. L'auteur a essayé à l'écran la roue, le popup « Maison » et les
  appuis longs des boutons du haut.
- Rendu hors tablette (CI) : les nouveaux écrans (roue, Maison, appareil, volet, Température,
  Alertes, Réglages, rangée, console) capturés à chaque PR ; vert sur `main` au dernier
  changement d'écran (#373).
- Intégration HACS (CI, `integration-hacs.yml`) : hassfest, validation de HACS, puis
  installation, mise à jour et retour en arrière dans un Home Assistant neuf.
- Compilations requises de la CI (dernière ESPHome et version minimale) : vertes sur `main`.

### Problèmes connus

Ceux de la 3.6.0, et :
- **HACS** ne propose un dépôt qu'à partir de sa dernière release complète : avant cette
  version, l'ajout échouait (« Dépôt introuvable », essayé le 07/10 avec la 3.6.0). L'ajout et
  le téléchargement n'ont donc jamais été faits dans un vrai HACS avant la publication. Au premier
  chargement, la boîte « Ajouter un référentiel personnalisé » de HACS peut s'afficher vide :
  recharger la page ;
- jamais essayés faute de matériel chez l'auteur : une batterie montée (le sens du courant lu
  par M5Unified, dont dépendent « Sur batterie » et le mode économie d'énergie) et plusieurs
  onduleurs solaires ;
- le popup Température lit les statistiques longue durée : un capteur sans `state_class` n'y a
  pas d'historique ;
- la capture de l'éditeur du blueprint, dans le guide, montre encore l'ancienne description ;
- les nouveaux textes en allemand, néerlandais, espagnol, italien et turc (écran et tableau de
  bord) sont traduits par une IA, pas encore relus ;
- les noms des entités de la tablette restent en français (choix expliqué dans
  `docs/translations.md`).

### Pré-releases

Tirées de cette version, sur le canal bêta :
[v3.7.0-rc.1](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.7.0-rc.1)
le 05/10/2026 sur `52a0dba` (batterie ou USB, plusieurs sources solaires, appareils sur la
météo au choix, tableau de bord), puis
[v3.7.0-rc.2](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.7.0-rc.2)
le 05/10/2026, qui ajoute le popup du volet (#333) et la doc des deux modes vocaux (#334), puis
[v3.7.0-rc.3](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.7.0-rc.3)
le 06/10/2026 : le package du volet à course simulée ne rend plus le volet muet (#341),
extinction automatique de l'écran au choix et rallumage à « Okay Nabu » (#342), notice
d'utilisation (#339), puis
[v3.7.0-rc.4](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.7.0-rc.4)
le 06/10/2026 : rangée sous l'horloge (#344), appui long sur les boutons du haut et popup
Réglages (#345), le bouton d'alimentation n'est plus un plantage (#347), alertes lues retenues,
abonnements, rang « 2/6 » et historique (#351, #353, #355, #356, #358), volet dessiné (#349),
popup d'un appareil et cartes du mode HA (#350), batterie et charge du processeur dans la
console (#348), mode économie d'énergie (#357), popup Température (#354), tuile − / + (#352), puis
[v3.7.0-rc.5](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.7.0-rc.5)
le 07/10/2026 : intégration « Tab5 » pour HACS et `tab5_hacs.zip` (#363, #365, #371), roue
d'actions rapides (#366), popup « Maison » (#368), écran de l'appui long des boutons du haut au
choix, « Maison » compris (#364, #373), blueprint plus lisible (#373), table unique des bandeaux
d'alerte (#360).

### 2026-10-07 — Bouton « Ouvrir Tab5 dans HACS »

Plus besoin d'ajouter le dépôt à la main dans les « Custom repositories » de HACS : un bouton
de la page d'installation (étape 5) et un lien du guide « Fichiers Home Assistant » (section
« Avec HACS ») ouvrent le dépôt dans HACS par My Home Assistant (`hacs_repository`, catégorie
`integration`) ; HACS propose alors de l'ajouter, puis « Download ». L'ajout à la main reste
décrit pour qui n'a pas My Home Assistant. Essayé le 07/10 sur un vrai Home Assistant, avec la
3.6.0 comme dernière release complète : le bouton ouvre bien la confirmation de HACS, mais l'ajout
échoue (« Dépôt introuvable »). HACS vérifie le dépôt sur sa dernière release complète, même avec
les bêtas : rien avant la 3.7.0 stable.

### 2026-10-07 — Blueprint « Tab5 — emplacements » plus lisible

Relecture de ce que voit un utilisateur dans l'éditeur de Home Assistant. Aucune clé d'entrée,
aucune valeur par défaut ni aucun sélecteur ne change : les automatisations existantes gardent
leurs valeurs (HA les range à plat par nom d'entrée, quelle que soit la section).
- **Une phrase d'aide sur chaque champ** (français puis anglais) : batterie du téléphone,
  température de la pièce, pots 1 à 5, vigilances, appuis longs des trois boutons, lumières de
  l'ancien accueil, cases « Inverser », batterie domestique…
- **Plus de jargon** : ni nom de fichier, de package ou de script, ni « opening/closing », ni
  numéro de version précis (« firmware 3.7 ou plus récent » là où la contrainte compte) ; les
  réglages faits dans HA sont désignés par leur liste « Tab5 · … ». Textes longs raccourcis.
- **Toutes les sections repliées sauf « Pièce 1 »** (TV, températures, clim, plantes, planning
  et météo s'ouvraient).
- **Énergie en trois sections** : « Énergie : solaire », « Énergie : réseau et maison »,
  « Énergie : batterie » ; les champs « principal » et « autres » (puissance, énergie produite)
  disent clairement leur rôle.
- « Tuiles de l'accueil (réglage 3.x) » devient « Ancien accueil (si la pièce 1 est vide) » ;
  « Automatique : comme aujourd'hui » devient « Automatique (par défaut) ».
- Docs alignées : étape 6 de l'installation (tableau des sections, avec la ligne « Tuile − / + »
  qui manquait), « Adapter à sa maison », notice, ADR-0028, tableau de bord HA (7 langues) ;
  `tests/test_tuiles_blueprint.py` vérifie le repli, les descriptions et les trois sections.
  La capture de l'éditeur (`docs/images/installation/ha_blueprint_*.png`) montre encore
  l'ancienne description.

### 2026-10-07 — Boutons du haut : « Maison » parmi les choix de l'appui long

Le popup « Maison » (#368, ADR-0037) s'ouvre aussi par l'appui long de l'un des trois boutons
du haut : choix « Maison · House » dans la section « Boutons du haut » du blueprint
« Tab5 — emplacements » (code `maison`, ajouté à la fin de `kCodesEcran` : les choix gardés en
NVS ne bougent pas). La mini icône du bouton prend la maison de l'en-tête du popup (glyphe
ajouté à `mdi_font_26`). Mettre le blueprint à jour d'abord : un firmware plus ancien lit
`maison` comme un code inconnu et garde l'appui long automatique. Pas encore essayé sur la
tablette.

### 2026-10-07 — Docs : notes de Jiuhai sur le C6 et l'alimentation (discussion #369)

Merci à Jiuhai (@poonjh), qui construit des appareils sur Tab5 avec ESP-IDF : remerciement dans
le `README.md`, et ses notes, lues sur le schéma et le code, pas mesurées, dans
`docs/hardware.md` : signal de reset du C6 (`RF_C6_RST` → GPIO 15), piège de la broche 54 pour un
firmware ESP-IDF sans le préréglage Tab5 d'ESP-Hosted (et son reset actif bas, en désaccord
inexpliqué avec ce firmware), contrôle de version du C6, mises à jour du C6 difficiles ailleurs,
chemin de l'alimentation (une seule diode idéale de 1 A pour tout le 5 V, entrée HVIN).
`docs/debugging.md` : les avertissements d'ESP-IDF sont retirés à la compilation, leur absence ne
prouve rien. Ses mesures seront ajoutées quand elles seront publiées.

### 2026-10-07 — Docs : l'intégration HACS citée dans les pages d'entrée

La mise à jour par l'intégration « Tab5 » de HACS (ADR-0035) n'était décrite que dans
`docs/installation/updates.md` et `home-assistant-files.md`. Le guide d'installation (prérequis,
étape 1) et la page d'accueil la citent ; la page d'accueil mène aussi à `updates.md` (fichiers HA
puis firmware) et liste les pages Maison et Réglages de la notice. `updates.md` dit comment
remettre à la main une sauvegarde de `config/tab5_sauvegardes/` ; `home-assistant-files.md`, que
HACS ne propose les pré-releases que si les versions bêta sont activées ; `docs/troubleshooting.md`
a une entrée pour chaque message de l'intégration dans Réparations.

### 2026-10-07 — Docs : cause de la panne du C6 du 05/08 revue

`docs/troubleshooting.md` : le build du 05/08/2026 (ESPHome 2026.7, esp_hosted 2.12.9)
réinitialisait très probablement déjà le C6 à chaque démarrage ; l'impulsion de reset n'a donc
pas effacé la panne, seule la coupure d'alimentation l'a fait. Moment de la ligne « not yet up »
au démarrage, piste `wifi_power` notée comme supposition. Même correction dans
`docs/debugging.md`, un commentaire de `Tab5/tab5-hardware.yaml` et le tableau de bord HA
(« USB débranché 15 s, et appui long avec une batterie », 7 langues).

### 2026-10-07 — Popup « Maison » : toute la maison, pièce par pièce, comme un tableau de bord HA

Demandé dans la discussion #278 (voir toute la maison d'un coup). Firmware seul : ni le contrat
avec Home Assistant ni le blueprint ne changent ([ADR-0037](docs/decisions/0037-house-popup.md)).
Pas encore essayé sur la tablette.
- **Nouveau popup « Maison »** (chrome partagé, inscrit au registre) : une colonne par pièce du
  blueprint qui a des appareils, de la pièce 1 à la 5, le nom de la pièce en tête ; une ligne par
  appareil, dessinée comme la carte du mode HA (pastille de la couleur de son état, icône, nom,
  état), coupée avec « … » quand la colonne est étroite.
- **Mêmes gestes que la tuile**, par la même fonction : tap, appui long, et un bouton « ⋯ » sur
  les appareils qui ont un appui long. Le popup ouvert depuis une ligne passe devant, le popup
  Maison reste derrière. L'appui long et « ⋯ » ouvrent d'abord la roue d'actions rapides
  (ADR-0036) autour de la pastille de la ligne, devant le popup Maison ; sans roue, le popup
  de la tuile (écran `maison-roue` du rendu hors tablette).
- **« Éteindre les lumières »** dans la barre de titre, s'il y a une lumière : le « Tout
  éteindre » du popup lumière, pour chaque pièce qui en a, sans confirmation.
- **Ouvert** par « Aller à l'écran → Maison » ou, en mode HA, par un tap sur le nom de la pièce
  dans la carte centrale. Notice : `docs/notice/house.md`.

### 2026-10-07 — Roue d'actions rapides à l'appui long d'une lampe, d'un volet ou d'une clim

Demandée dans la [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)
(le module `sub_button_wheel` de Bubble Card), décidée dans
[ADR-0036](docs/decisions/0036-quick-action-wheel.md). Pas encore essayée sur la tablette.
- L'appui long d'une lumière à variateur (`d`), d'un volet ou d'une clim dont HA a envoyé les
  réglages fait surgir, sur un arc au-dessus de la tuile (tuile météo ou carte du mode HA),
  des boutons ronds : Éteindre · 10 % · 50 % · 100 % pour une lumière, Ouvrir · Stop ·
  Fermer · 50 % (s'il donne sa position) pour un volet, Arrêt et ses modes (chaud, froid,
  sec, ventilation) pour une clim ; puis « ⋯ », qui ouvre le popup d'avant. Le bouton de
  l'état courant est teinté de la couleur d'état.
- Un bouton envoie la commande que la tuile ou son popup envoie déjà : rien ne change côté
  Home Assistant. Toucher ailleurs ferme la roue, comme l'inactivité, un popup, l'écran éteint.
- Sans roue (moins de trois commandes, option `k`, lumière sans variateur) : le popup, comme
  avant. Une tuile clim sans roue ouvre désormais son popup à l'appui long (avant : rien).
- Firmware : `tab5_roue.cpp`, `ui_components/roue_actions.yaml` et `roue_bouton.yaml`, police
  `mdi_font_36` ; rendu hors tablette : écrans `roue-lampe`, `roue-volet`, `roue-clim`, et
  « ⋯ » touché sur les écrans des popups ; `tests/test_roue.py`.

### 2026-10-07 — Docs : co-processeur WiFi et alimentation, faits vérifiés

`docs/troubleshooting.md` et `docs/hardware.md` : le P4 réinitialise le C6 par GPIO 15 à chaque
démarrage (lu dans le `sdkconfig`), débrancher l'USB-C suffit sans batterie, fréquence de la panne ;
consommation jamais mesurée (le « 1,5 A » n'avait pas de source), chargeur 5 V / 2 A conseillé.

### 2026-10-07 — Boutons du haut : l'écran de l'appui long au choix dans le blueprint

Demande d'Axel : choisir dans le blueprint la page qu'ouvre l'appui long de chacun des trois
boutons en haut à droite de l'accueil (maison, engrenage, manette). Les taps ne changent pas.
- **Blueprint** « Tab5 — emplacements » : nouvelle section repliée « Boutons du haut · Top
  buttons », une liste par bouton : Automatique (comme avant : Énergie avec la production
  solaire, console système, télécommande avec une TV), Rien, ou un écran (assistant vocal,
  calendrier, réveil, clim, plantes, télécommande TV, console, Énergie, réglages, alertes,
  Arcade). Poussé avec tous les états dans une clé de plus de `tab5_maj_emplacements`,
  `appuis|maison|engrenage|manette` ; aucune variable nouvelle. Un code inconnu vaut « auto ».
- **Compatibilité** : un firmware plus ancien ignore la clé (lu dans `emplacements_appliquer()`
  de la 3.6.0 et de la 3.7.0-rc.4 : aucune route ne la prend, la table 3.x ne la connaît pas) et garde ses
  appuis longs ; déployer le blueprint avant ou après le firmware, dans n'importe quel ordre.
- **Firmware** : une seule routine d'ouverture, le script `tab5_ecran_ouvrir`
  (`tab5-ha-controls.yaml`), pour le select « Aller à l'écran » et les trois appuis longs
  (garde de la sonnerie, zones absentes, écran déjà affiché, fermeture des autres, compteur
  d'inactivité) ; la console s'ouvre par un seul script, `tab5_console_ouvrir`, qui donne
  aussi au select la ligne d'état et l'état de HA tout de suite. Choix gardés en NVS
  (`tab5_zones.cpp`) : la mini icône est juste dès le démarrage.
- **Mini icônes** : une de plus, sur l'engrenage. En « auto », celles d'avant ; avec un écran
  choisi et disponible, le glyphe de l'en-tête de sa fenêtre (console : `console`, son en-tête
  garde le flocon de la clim ; Arcade : la manette) ; masquée sinon. Neuf glyphes ajoutés à
  `mdi_font_26`.
- **Preuves** : `tests/test_appuis.py` (mêmes codes des deux côtés, enum `Ecran` = options du
  select, trois boutons par la routine unique, glyphes des en-têtes, rendu du vrai modèle Jinja :
  défauts, choix, valeurs inconnues, déclencheurs). Écran « accueil-appuis-choisis » ajouté au
  rendu hors tablette ; la clé entre dans la graine du fuzz des sanitizers. Docs : notice
  (accueil, vue d'ensemble, console), sections du blueprint (`docs/installation/devices.md`),
  `docs/screens.md`, `Tab5/README.md`, cartographie.

### 2026-10-07 — Intégration « Tab5 » pour HACS : publication et guide (lot 2)

- Chaque release joint désormais `tab5_hacs.zip`, l'asset que HACS télécharge
  (`publication.yml`, job `home-assistant`, après `tab5_home_assistant.zip`). Un tag sans
  l'intégration (3.7.0-rc.4 et avant) n'en a pas.
- Guide : « Avec HACS » dans [Fichiers Home Assistant](docs/installation/home-assistant-files.md)
  (dépôt personnalisé, téléchargement, redémarrage, ajout de l'intégration) et dans
  [Mises à jour](docs/installation/updates.md) ; mention dans le démarrage rapide, la page
  d'installation et `HomeAssistant_Config/README.md`.
- La notification « Tab5 : fichiers Home Assistant à mettre à jour » propose aussi la mise à
  jour de HACS.

### 2026-10-07 — Intégration « Tab5 » pour HACS : les fichiers Home Assistant en un clic (lot 1)

Demande d'un utilisateur (discussion #278) : mettre à jour les fichiers Home Assistant sans
télécharger, décompresser et copier l'archive à la main ([ADR-0035](docs/decisions/0035-hacs-integration-ha-files.md)).
- Nouvelle intégration `custom_components/tab5/` et `hacs.json` : le dépôt s'ajoute à HACS
  comme dépôt personnalisé. Chaque release joindra `tab5_hacs.zip`
  (`tools/publication/archive_hacs.py`), qui porte l'intégration et les mêmes fichiers que
  `tab5_home_assistant.zip` (lot 2 : publication et guide).
- Au démarrage, une fois par version : sauvegarde dans `config/tab5_sauvegardes/` (5 gardées),
  fichiers posés (un package optionnel seulement s'il est déjà là, le blueprint aussi sur une
  copie importée par son URL, un fichier que la release ne livre plus retiré), configuration
  vérifiée avec retour en arrière si elle casse, domaines manquants chargés, tout le YAML
  rechargé sans redémarrage, version constatée sur le capteur « Tab5 · version des fichiers
  HA », notification en français ou en anglais. Réparations : ligne `packages:` absente,
  redémarrage (avec un bouton), configuration refusée.
- Option, cochée par défaut : lancer ensuite la mise à jour de la tablette, dès que son entité
  « Firmware » propose la même version.
- `tab5_home_assistant.zip` et la mise à jour à la main ne changent pas ; `archive_ha.py`
  produit la même archive à l'octet près.
- Tests : `tests/test_integration_tab5.py` (sans HA) ; CI non requise `integration-hacs.yml`
  (hassfest, validation HACS, et un Home Assistant neuf en conteneur : installation sans
  redémarrage, mise à jour, retour en arrière, ligne `packages:` absente).

### 2026-10-06 — Bandeaux d'alerte HA : une seule table pour leurs widgets

Refactor : rien ne doit changer à l'écran ni pour Home Assistant. Les widgets des 4 bandeaux
d'alerte HA de la carte centrale (cadre, texte, compteur « 2/6 ») et leur id d'acquittement
étaient listés dans trois lambdas : le service `tab5_maj_alertes_ha_bulk`, le tap
`tab5_dismiss_ha_alert` et « Tout marquer comme lu » (`tab5_alertes_tout_lu`).
- Ils ne sont plus écrits qu'une fois, dans le script `tab5_ha_alert_slots_init`
  (`Tab5/tab5-alertes.yaml`), qui remplit le tableau global `g_ha_alert_slots`
  (`Tab5/tab5_custom.h`). Idempotent, comme le registre des modales : chaque lecteur l'appelle
  avant de lire, car le service de HA peut arriver avant l'`on_boot`, qui attend HA jusqu'à
  30 s.
- L'`on_boot` est inchangé : il nomme toujours les 4 cadres (`g_central_ctx.ha_wrap`).

### 2026-10-06 — Docs : pourquoi les entités de la tablette restent en français

Demande d'un utilisateur : les réglages et capteurs en anglais. Home Assistant reconnaît une entité
ESPHome à son nom (identifiant `{mac}/{appareil}/{type}/{nom}` d'aioesphomeapi) : traduire les noms
créerait de nouvelles entités sur chaque tablette déjà installée et casserait le tableau de bord et
les automatisations, qui retrouvent les entités par la fin de leur entity_id. Les noms restent donc
en français ; `docs/installation/settings.md` et `docs/translations.md` le disent et renvoient au
tableau de bord, dont les libellés suivent la langue de l'écran (ou `tab5_dashboard(langue='English')`).

### 2026-10-06 — Docs : la ST7121 tourne chez un utilisateur

La doc disait encore la révision ST7121 « compilée, jamais essayée ». Un utilisateur fait
tourner ce firmware sur sa ST7121 depuis octobre 2026 (écran, tactile, mot d'activation :
[discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : README,
`docs/hardware.md`, `docs/installation/flash.md`, `docs/architecture.md`, la cartographie et deux
commentaires du firmware le disent désormais. L'ILI9881C d'origine reste compilée sans avoir
jamais tourné.

### 2026-10-06 — Tuile − / + : les boutons de la carte clim règlent l'appareil de votre choix

Demande d'Axel : les − / + de la carte clim de l'accueil (en haut à droite) ne réglaient que la
clim. Ils règlent désormais l'appareil choisi dans une liste qui se déroule au toucher de
l'icône ou de la température du salon ([ADR-0033](docs/decisions/0033-adjustable-tile.md)).
Firmware et blueprint ; un firmware plus ancien ignore les nouvelles clés (la clim seule), un
blueprint plus ancien laisse la clim et le volume de la tablette.
- **La liste** : la clim du blueprint en tête, les appareils de la nouvelle section repliée
  « Tuile − / + · − / + tile » du blueprint « Tab5 — emplacements » (huit au plus, dans l'ordre
  choisi : volume d'une TV ou d'une barre de son, luminosité d'une lampe, consigne d'un
  thermostat ou d'un chauffe-eau, humidité, vitesse d'un ventilateur, position d'un volet ou
  d'une vanne, valeur d'un nombre), et toujours en dernier le volume de la tablette. Un toucher
  choisit ; un toucher ailleurs ferme la liste.
- **Le choix reste**, même après un redémarrage, et suit l'appareil si sa place change dans le
  blueprint. Clim choisie : rien ne change. Autre appareil : son icône et sa valeur s'affichent
  entre − et + ; la valeur bouge tout de suite, d'un pas de l'appareil (5 % pour un volume,
  10 % pour une lumière, le pas du thermostat…), et une seule commande part quand le doigt
  s'arrête.
- **Toucher la valeur** ouvre la fenêtre de l'appareil quand il en a une : la clim, une lampe
  ou un volet placé dans une pièce, la télécommande de la TV.
- **Côté Home Assistant**, la commande « regler » (ou « consigne » pour une clim) ne vise que
  les appareils de la section : volume, luminosité (0 éteint), vitesse, humidité, consigne,
  position (ouvert / fermé sans position réglable), valeur d'un nombre, bornées aux limites de
  l'appareil.

### 2026-10-06 — Popup Température : historique des deux températures, et prévision

Demande d'Axel : un historique en popup au clic long sur la température de la pièce, et pour la
seconde (la serre ; dehors pour la plupart des maisons), l'historique et la prévision, en
graphique ([ADR-0032](docs/decisions/0032-temperature-history-popup.md)). Firmware, blueprint et
un package HA ; sans le package, le popup attend.
- **Appui long sur l'une des deux températures** de l'accueil : popup « Température ». En haut,
  Maintenant (et la moyenne), Minimum et Maximum (avec leur moment) ; pour la seconde, une
  quatrième carte, la prévision (sa maxi, sa mini dessous). En bas, la courbe des moyennes, une
  barre pâle du minimum au maximum de chaque créneau, le point de la valeur actuelle. Trois vues :
  **24 h** (par heure), **7 jours** (par trois heures), **30 jours** (par jour). L'appui court
  sur la seconde température ouvre toujours l'arcade.
- **Prévision**, seconde température seulement : en or, sur un fond teinté après « Maintenant »,
  avec la barre mini-maxi d'une prévision par jour. Nouvelle case du blueprint, **« La seconde
  température est dehors »** : cochée, la prévision prolonge la courbe ; décochée (une serre),
  elle reste à part sous le nom « Dehors, prévu ».
- **Rien de stocké sur la tablette, rien poussé popup fermé** : l'ouverture et chaque bouton de
  vue émettent `esphome.tab5_historique` ; le blueprint lance `script.tab5_historique`
  (`packages/tab5_historique.yaml`), qui lit les **statistiques longue durée du recorder** (aucun
  helper, aucune écriture en base ; le capteur doit avoir un `state_class`) et la prévision de
  l'entité météo de la tablette, puis pousse une fois la nouvelle action `tab5_maj_historique`.
  Les minutes sont comptées à l'horloge locale : un changement d'heure ne décale pas l'axe.
- ~6 Ko de PSRAM à la première ouverture, widgets créés une fois ; tracé en `lv_line` et barres
  (pas de `lv_chart` dans ce firmware). `energie_vue_btn.yaml` devient `vue_btn.yaml`, partagé
  par les deux popups. Démo, rendu (cinq écrans), notice (page Température) et
  `tests/test_historique.py` (modèles du package rendus sur des réponses simulées, deux
  changements d'heure, calcul Python à part ; aussi rendus par le HA d'Axel sur ses vraies
  statistiques). Non essayé sur la tablette.

### 2026-10-06 — Alertes : l'historique, et « Tout marquer comme lu »

Lot 4 du plan des alertes de la carte centrale. Firmware et Home Assistant.
- **Popup « Alertes »** : un appui long sur la carte centrale de l'accueil, quoi qu'elle montre,
  ouvre l'historique des 20 dernières alertes, une ligne chacune : une pastille de sa couleur,
  son texte, puis « apparue 14 h 02 · lue 14 h 10 · terminée 15 h 30 » (l'heure seule
  aujourd'hui, le jour avant l'heure sinon). Pastille vive : à lire ; pâle : lue, en cours ;
  texte gris : terminée. Aussi par « Aller à l'écran → Alertes ».
- **« Tout marquer comme lu »**, dans la barre de titre du popup : les bandeaux de la carte
  centrale sont lus tout de suite, et HA reçoit `alert_id` « * », qui lit aussi les alertes
  qui n'avaient pas de bandeau. Le bouton disparaît quand rien n'est à lire.
- **À la demande** : le popup demande l'historique à l'ouverture (événement
  `esphome.tab5_alertes_historique`), HA répond par la nouvelle action
  `tab5_maj_alertes_historique` (script `tab5_push_alertes_historique` de `tab5_push.yaml`)
  et la repousse tant que le popup est ouvert et que l'historique change. Un firmware d'avant
  n'émet jamais l'événement : **l'ordre de mise à jour firmware / HA est indifférent**.
- L'historique suit les **abonnements** (lot 2) : une alerte d'une source désabonnée n'y
  figure pas. Chaque entrée garde désormais sa source ; celles d'avant la déduisent de leur id.
- **Tableau de bord HA** (vue Santé) : la même liste, en tableau (apparue, lue, terminée), dans
  les 7 langues.
- Notice : nouvelle page « Alertes » ; l'appui long de la carte centrale décrit sur l'accueil.
- Décision : [ADR-0034](docs/decisions/0034-central-card-alerts.md) (lot 5) rassemble les choix
  des lots 0 à 4 : mémoire et révision dans HA, abonnements, ordre à l'écran, historique à la
  demande, et les écarts au plan.
- **Preuves** : `tests/test_alertes_ha.py` (payload rendu depuis le vrai modèle, 20 au plus,
  abonnements, entrées d'avant, câblage tablette ↔ HA), écran « alertes » du rendu hors
  tablette, graine du fuzz des sanitizers pour la nouvelle action.

### 2026-10-06 — Mode économie d'énergie, enclenché tout seul sur batterie

Firmware seul (et une carte du tableau de bord). Demande d'Axel après la discussion #278.
- **Réglage de l'appareil « Tab5 Économie d'énergie »** (select, config, gardé en mémoire) :
  Jamais, **Sur batterie** (défaut) ou Toujours. Sur secteur, le défaut ne change rien.
- **Mode actif** : luminosité plafonnée à 50 % ; au plus bas (10 %, le minimum du curseur des
  Réglages) après 30 s sans toucher, et dès que la batterie descend à 35 % (elle remonte à
  40 %) ; un toucher la rend tout de suite. Le plafond s'applique à la sortie du
  rétroéclairage : HA, le curseur des Réglages et le réveil gardent la luminosité choisie, qui
  revient telle quelle quand le mode s'arrête. Plus aucune animation de l'interface (panneau
  tournant, alertes, glissements, fondus, icônes, horloge) ; LVGL limité à 30 images/s, sauf
  pendant un jeu. Pas d'assombrissement pendant le réveil, la voix, un jeu ou une mise à jour :
  la même liste que l'extinction auto, qui la calcule.
- **« Sur batterie » se décide au courant de la batterie**, lu par l'INA226 toutes les 60 s :
  au-dessus de 50 mA de décharge, sur batterie ; sous 20 mA, sur secteur ; jamais en charge
  ni sans batterie détectée. Deux entités de diagnostic : **Tab5 Courant batterie** (A, + = la
  batterie se décharge ; désactivée par défaut, comme la tension) et **Tab5 Sur batterie**. Le
  sens du courant vient de M5Unified (`getBatteryCurrent`) ; il n'a encore été vu sur aucune
  tablette : à vérifier avec une batterie montée (l'auteur n'en a pas).
- **Preuves** : `tools/test_alarm_clock.cpp` (règles pures de `tab5_economie.h` : seuils et
  hystérésis du courant et du niveau, débranchée à 20 % basse tout de suite, décision par
  option) ; `tests/test_economie.py` (options dans l'ordre de l'enum, la lumière passe par la
  sortie plafonnée, le courant décide, l'extinction range ses raisons avant son délai, plancher =
  minimum du curseur) ; `esphome config` valide (tablette et rendu hors tablette). Non testé
  sur une tablette.

### 2026-10-06 — Console système : batterie et charge du processeur

Demandé dans la discussion #278 (husyildiz, tablette sur batterie : voir le pourcentage, la
tension et la charge du processeur dans le menu système). Firmware seulement ; aucune entité
Home Assistant ajoutée.
- **Batterie** (carte SYSTÈME) : niveau et tension (« 78% · 7.62 V »), avec l'icône du bandeau
  d'état (même glyphe, même couleur ; un éclair pendant la charge). « Sur USB » quand aucune
  batterie n'est détectée, « Non montée » tant que l'interrupteur « Tab5 Batterie montée » est
  éteint, « -- » avant la première lecture : jamais de faux 0 % ou 100 %
  (`batterie_texte_console()`, `tab5_core.cpp`, testé par `tools/test_alarm_clock.cpp`).
- **Charge CPU** : l'occupation de chacun des deux cœurs sur les 2 dernières secondes
  (« 4% · 37% », cœur 0 puis cœur 1 ; la boucle d'ESPHome, écran compris, est sur le cœur 1 :
  une moyenne masquerait un cœur saturé). Mesurée par le temps de la tâche inactive de chaque
  cœur, que FreeRTOS compte avec `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` (nouvelle option de
  `tab5-hardware.yaml`) ; lue seulement console ouverte. Coût estimé, non mesuré : une lecture
  d'horloge par changement de tâche, 12 octets par tâche (`docs/performance.md`).
- La carte SYSTÈME passe de quatre à six lignes, au pas de 39 px au lieu de 52 ; les autres
  cartes ne bougent pas. Trois écrans de plus dans le rendu hors tablette (batterie, en charge,
  sans batterie), notice de la console complétée.

### 2026-10-06 — Cartes du mode HA dessinées comme la carte « tile » de HA

Demandé dans la discussion #278 (« buttons can be like ha dashboard buttons »). Firmware seul :
ni le contrat avec Home Assistant ni le blueprint ne changent ; les cartes météo non plus.
- **Plus d'onglets sur les cinq cartes du mode HA** : chaque carte ressemble à la carte « tile »
  d'un tableau de bord Home Assistant, en version verticale. L'icône est dans une pastille ronde
  de la couleur de son état (la couleur à 20 %, l'icône pleine), le nom dessous, l'état sous le
  nom dans sa couleur. Mêmes couleurs et mêmes mots qu'avant, repeints au changement de thème.
- **Mêmes gestes** : tap et appui long sur la pastille (même zone de 130 × 130 que l'ancien
  bouton), toucher du nom pour le sens d'un volet ; ailleurs sur la carte, un geste reste un
  glissement de pièce.

### 2026-10-06 — Popup d'un appareil à l'appui long, comme dans un tableau de bord HA

Demandé dans la discussion #278 (« buttons can have pop up screen like ha dashboard »). L'appui
long d'un interrupteur, d'une prise, d'un ventilateur, d'une scène, d'un script, d'un bouton ou
d'un lecteur qui n'est pas la TV du blueprint ne faisait rien. Firmware seul : ni le contrat avec
Home Assistant ni le blueprint ne changent.
- **Nouveau popup « Appareil »** (`appareil_popup.yaml`, un seul pour tous ces types, chrome
  partagé, inscrit au registre) : à gauche l'icône de la tuile dans une pastille ronde de la
  couleur de son état, l'état en mots, la pièce et les options de la tuile (« Allumer
  seulement », « Confirmer chaque commande ») ; à droite un grand interrupteur vertical façon HA
  (rempli en haut et en couleur allumé, en bas et gris éteint, plein pour une scène) et ce que
  fera l'appui.
- **Le grand bouton refait le toucher de la tuile**, par la même fonction : même commande
  (`basculer`, `allumer` avec « Allumer seulement », `lancer`), même confirmation (« Confirmer »
  n'est jamais contourné : le premier appui arme, la tuile et le popup demandent « Confirmer ? »),
  même « OK » après une scène. « Lecture seule » : ni toucher ni popup, comme avant. Lumières,
  volets, clims, TV et énergie gardent leurs popups.
- Seulement ce que HA pousse déjà pour les tuiles : pas de « dernière modification » ni
  d'historique.
- Rendu hors tablette : trois écrans (`appareil`, `appareil-scene`, `appareil-confirmer`). Tests
  dans `tests/test_tuiles_firmware.py`. Docs : notice (tableau des appuis, « Fenêtre de
  l'appareil »), `docs/screens.md`, ADR-0023 (mise à jour du 06/10/2026), cartographie.
  **Pas encore essayé sur la tablette.**

### 2026-10-06 — Popup du volet : un volet dessiné à faire glisser, des boutons façon HA

Demandé dans la [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) :
« comme l'animation de HA, pas un simple curseur », et des boutons comme ceux d'un tableau de
bord HA. Firmware seul : même commande, même événement, même branche du blueprint.
- **Un volet dessiné à la place du curseur** : une fenêtre dont le tablier à lames descend
  depuis le coffre selon la position. On le fait glisser du doigt, vers le haut ou le bas,
  n'importe où sur la fenêtre : le dessin et le « 45 % » suivent le doigt, et la position
  (`position`, 0-100) ne part **qu'au relâcher**, comme avant. Un simple toucher n'envoie rien.
- **Il suit le vrai volet** tant que le popup est ouvert : chaque position poussée par Home
  Assistant le redessine aussitôt, sans fondu ni animation à lui (le volet dessiné descend quand
  le vrai descend). Jamais sous le doigt.
- **Volet sans position** (`nan`, volet à course simulée, hors ligne) : pas de glissement ni de
  nombre ; le dessin montre l'état (ouvert en haut, fermé en bas, sinon à mi-hauteur, lames
  estompées) et les mots restent (« Ouvert », « Fermé », « Partiel », « En mouvement »,
  « Hors ligne »).
- **Ouvrir / Stop / Fermer** : l'icône dans une pastille ronde teintée, comme une tuile de Home
  Assistant ; le bouton garde le verre et l'effet d'appui du thème.
- Aucun texte nouveau à l'écran. ADR-0023 (mise à jour du 06/10/2026), `docs/screens.md`,
  notice `shutters.md`, `Tab5/README.md` ; tests du glissement (rien pendant, rien sans
  position, une fois au relâcher) et de la géométrie (`tests/test_tuiles_firmware.py`) ; un
  écran de plus au rendu hors tablette (`volet-glisse`, le volet tiré du doigt).

### 2026-10-06 — Alertes : le rang « 2/6 » et l'alerte rouge en premier sur l'écran

Lot 3 du plan des alertes de la carte centrale. Firmware et Home Assistant.
- **Rang « 2/6 »** : quand Home Assistant a plus de quatre alertes à lire, chaque bandeau affiche
  son rang à droite du texte, en petit et atténué ; avec quatre alertes ou moins, rien ne change.
  HA ajoute en tête du payload de `tab5_maj_alertes_ha_bulk` un jeton `@n:total`, sans « | » :
  un firmware d'avant l'ignore, le package peut donc partir avant le flash.
- **Une alerte rouge passe en premier** : une alerte rouge nouvelle (capteur « problème »,
  fumée, gaz, fuite, mise à jour de HA Core, Supervisor ou OS, vigilance rouge) prend la carte
  centrale tout de suite au lieu d'attendre son tour, puis tourne avec le reste (elle ne bloque
  pas la carte : la météo et la pluie restent visibles). Le rotateur repart de zéro pour qu'elle reste un tour
  entier. L'ordre des bandeaux reste celui de HA : rouge, orange, jaune, puis de la plus
  ancienne à la plus récente.
- Le cache local des taps est déjà par révision (les ids « id#révision » du lot 1) : rien à
  changer.
- **Preuves** : `tests/test_alertes_ecran.py` (payload rendu depuis le vrai modèle du package :
  sans en-tête jusqu'à quatre, `@n:6` et les quatre premières au-delà, vigilance hors compte,
  en-tête sans « | ») ; graine du fuzz des sanitizers avec l'en-tête ; écran
  « accueil-alertes-ha-compteur » ajouté au rendu hors tablette.

### 2026-10-06 — Alertes : choisir ce qui s'affiche (abonnements)

Lot 2 du plan des alertes de la carte centrale. HA seul, aucun changement de firmware.
- **Six listes « Tab5 · alertes : … »** (`packages/tab5_alerts.yaml`), à régler dans HA ou sur
  la page Réglages du tableau de bord de la tablette (nouvelle section « Alertes ») : mises à
  jour (toutes / Home Assistant seulement / aucune), vigilance à partir du jaune, de l'orange ou
  du rouge (ou aucune), capteurs « problème », entités indisponibles, étiquette
  « Tab5 · alerte », piles sous 10 à 30 %. Par défaut : tout, piles sous 20 %. Des listes plutôt
  que des interrupteurs : sans `initial`, une liste démarre sur sa première option puis HA
  restaure le choix (un `input_boolean` sans `initial` démarrerait éteint).
- **Étiquette « Tab5 · alerte »** : toute entité qui la porte devient une alerte quand elle est
  allumée, ouverte, déverrouillée, bloquée ou déclenchée (porte, fuite, serrure, alarme) ; rouge
  pour la fumée, le gaz, le CO, l'eau et la sécurité.
- **Piles faibles** : capteurs `battery` sous le seuil (sauf les téléphones de l'application
  mobile, rechargés chaque jour), ou binaires `battery` allumés. Une pile reste en alerte
  jusqu'à 10 % au-dessus du seuil ; après une recharge, elle revient si elle retombe.
- Se désabonner masque tout de suite ; les alertes restent suivies, donc se réabonner ne fait
  pas revenir ce qui était déjà lu. Une source suivie qui devient indisponible ou inconnue est
  dans le doute, quel que soit son domaine (plus seulement les mises à jour et les capteurs
  binaires).
- **Preuves** : `tests/test_alertes_ha.py` (listes, lecture des abonnements, étiquette, piles,
  réabonnement) ; le job « Installation dans un HA neuf » désabonne puis réabonne les mises à
  jour, pose l'étiquette sur un capteur de la démo et se désabonne de l'étiquette.

### 2026-10-06 — Boutons du haut : un appui long chacun, popup Réglages sur la tablette

Demande d'Axel : compléter les trois boutons du haut par un appui long, et régler la tablette sans
passer par Home Assistant. Firmware seulement ; Home Assistant inchangé.
- **Bouton de droite** : le tap ouvre l'**Arcade** (icône manette, au lieu de l'ordinateur) ;
  l'appui long, la **télécommande TV** quand une TV est choisie dans le blueprint. Le bouton ne
  disparaît plus sans TV, et les deux autres ne glissent plus d'une colonne.
- **Bouton Home Assistant** : tap inchangé ; l'appui long ouvre le **popup Énergie** quand la
  production solaire est reçue (la même condition que son icône dans la ligne d'état).
- Ces deux appuis longs ne servent que si la TV ou le solaire est là : une **mini icône** de
  26 px (petit écran, panneau solaire : les glyphes déjà employés dans la ligne d'état et
  l'en-tête de la télécommande) s'affiche alors dans le coin en haut à droite du bouton.
- **Bouton central** (engrenage, au lieu du flocon) : le tap ouvre le nouveau **popup Réglages**,
  l'appui long la console système.
- **Popup Réglages** (`reglages_popup.yaml`, `tab5-reglages.yaml`, `tab5_reglages.cpp`), chrome
  partagé (ADR-0009) : carte ÉCRAN — luminosité (curseur 10-100 %), extinction auto, rallumer
  l'écran à « Okay Nabu », rallumer d'une tape ; carte APPARENCE — thème (flèches), clair ou
  sombre, nuit du mode Auto, langue (les 7, chacune dans sa langue, derrière une confirmation
  puisque la tablette redémarre). Chaque bouton écrit l'entité que voit HA, et chaque entité
  repeint le popup quand elle change : il suit l'état réel, même changé depuis HA (sauf le
  curseur de luminosité, relu à chaque ouverture). Le curseur n'envoie qu'un `light.turn_on`
  par geste (150 ms après le dernier pas).
  Aussi ouvrable par « Aller à l'écran → Réglages » (option ajoutée en fin de liste).
- **Le thème et son mode quittent la console** (rangée retirée, cartes GESTION en 2 × 2) pour
  le popup Réglages ; `tab5_theme_console` et `theme_console_libelles()` sont supprimés.
- 20 textes nouveaux, traduits dans les 6 langues. Notice (accueil, vue d'ensemble, nouvelle
  page Réglages, console, TV, Arcade, Énergie), `docs/screens.md`, réglages, architecture,
  cartographie, `Tab5/README.md`. Écrans « reglages » et « reglages-langue » ajoutés au rendu
  hors tablette (et la télécommande, la console y passent par l'appui long). Test
  `tests/test_reglages.py` : numéros des réglages, une pastille par option dans l'ordre du
  select, une par langue, boutons posés à leur index, registre des fenêtres assez grand
  (`ModalRegistry::MAX` passe de 16, atteint, à 24).

### 2026-10-06 — Alertes : une alerte lue ne revient que si elle change

Lot 1 du plan des alertes de la carte centrale (demande d'Axel : une alerte reste jusqu'au
tap, puis ne revient plus, même après un redémarrage de HA, sauf si elle change). HA seul,
aucun changement de firmware : la tablette renvoie déjà l'id reçu tel quel au tap.
- **Capteur « Tab5 Alertes »** (`sensor.tab5_alertes`, `packages/tab5_alerts.yaml`, logique
  dans le nouveau `custom_templates/tab5_alertes.jinja`) : il suit les mises à jour, les
  capteurs `problem`, les entités indisponibles depuis 10 min et la vigilance. Une alerte = une
  source + une **révision** ; la tablette reçoit « id#révision » et renvoie celle qu'elle
  montrait. Lue, l'alerte ne revient que si elle change (version plus récente, autre niveau ou
  phénomènes, nouvelle entité indisponible) ou si elle s'arrête vraiment puis recommence :
  revenue à la normale 5 min, ou disparue 1 h ; jamais sur une source indisponible ou
  inconnue, ni dans les 15 min qui suivent un démarrage de HA. Non lue, une alerte revenue à
  la normale quitte l'écran tout de suite.
- **Historique** : les 30 dernières alertes (apparue, lue, terminée) dans l'attribut
  `historique`, pour le popup et le tableau de bord des lots suivants.
- **Mémoire** : le capteur restaure ses attributs au démarrage, et l'automatisation
  `tab5_alertes_sauvegarde` écrit les états restaurés sur le disque dès qu'une alerte est lue.
  Le calcul est d'un seul tenant (les `variables:` du bloc à déclencheurs) : un tap n'est pas
  perdu derrière le calcul de la minute.
- **Reprise** : l'ancienne liste `input_text.tab5_alerts_dismissed` est lue une fois, puis plus
  écrite. Sauf « ha:unavailable », qui rendait muette pour toujours l'alerte des indisponibles :
  après le déploiement, elle apparaît une fois. La purge nocturne de 4 h disparaît.
- **Poussées** (`packages/tab5_push.yaml`) : bandeaux et bandeau info lus dans le capteur ; le
  bandeau info ne dit plus que la vigilance (MAJ, erreurs et indisponibles sont des bandeaux).
- **Preuves** : `tests/test_alertes_ha.py` rejoue la macro réelle (bac à sable Jinja) sur les
  redémarrages, plantages, versions, coupures et taps ; le job « Installation dans un HA neuf »
  lit une alerte de démo, vérifie le payload poussé, tue puis redémarre HA, et installe une
  mise à jour.

### 2026-10-06 — Alertes : une alerte lue ne revient plus après un redémarrage de HA

Demande d'Axel : une alerte touchée sur la tablette ne doit plus revenir, même après un
redémarrage de Home Assistant. Lot 0 du plan des alertes de la carte centrale. HA seul.
- **Cause** : `input_text.tab5_alerts_dismissed` (`packages/tab5_alerts.yaml`, et son snippet)
  était déclaré avec `initial: ""`. Avec une valeur de départ, HA ne restaure pas l'ancienne
  (code de l'`input_text` de HA 2026.9.4) : la liste des alertes lues était vidée à chaque
  démarrage. Vu chez l'auteur dans l'historique de HA, le 29/09 à 13 h 43 et le 03/10 à 4 h 49,
  aux deux démarrages de HA.
- **Plantage** : HA n'écrit ces états sur le disque qu'à l'arrêt propre et toutes les 15 min
  (`STATE_DUMP_INTERVAL`). Le démarrage du 03/10 suivait un plantage, sans arrêt propre : le
  script du tap appelle maintenant `homeassistant.save_persistent_states` juste après.
- **Preuves** : `tests/test_alertes_ha.py` (aucun `input_text` des packages avec `initial:`,
  sauvegarde après l'écriture) ; le job « Installation dans un HA neuf » retient une alerte lue,
  tue HA (`docker kill`) puis le redémarre proprement, et la retrouve à chaque fois ; en
  contre-épreuve, une valeur posée sans sauvegarde est bien perdue au plantage.

### 2026-10-06 — Bouton d'alimentation : un redémarrage, plus une alerte de plantage

Signalé dans la discussion #278 et reproduit le même jour sur la tablette d'Axel : un appui
court sur le bouton d'alimentation redémarre la tablette, puis Home Assistant envoyait
« Tab5 : journal du démarrage (plantage (chien de garde)) » sur le téléphone, et l'entité
« Tab5 Raison du redémarrage » affichait « Reboot request from esphome.ota », la source de la
dernière mise à jour, des heures plus tôt. Firmware et `packages/tab5_health.yaml` : mettre à
jour les fichiers HA **avant ou avec** le firmware (sans eux, la garde « reboot inattendu »
alerterait sur le nouveau texte).
- **Journal des démarrages** (`Tab5/tab5_journal.cpp`) : un reset du chien de garde
  (`ESP_RST_WDT`) **sans rapport de plantage** n'est plus une anomalie. Il s'appelle
  « bouton d'alimentation ou chien de garde RTC (rst 0x..) », avec le code de reset brut du
  ROM, et n'envoie rien. Le firmware ne gère pas ce bouton et aucune source ne dit comment
  il est câblé : le code `rst` dira au prochain appui lequel des six chiens de garde du P4 il
  déclenche. Une panique, un chien de garde de tâche ou d'interruption, une baisse de
  tension, une micro-coupure, un blocage du CPU, un Wi-Fi absent et **tout chien de garde
  accompagné d'un rapport `esp32.crash`** alertent toujours.
- **Rapport de plantage d'ESPHome** : le logger l'écrit avant que le déclencheur
  `on_message` du journal existe, il n'y entrait donc jamais (trouvé à la relecture de ce
  lot). Le journal le lit maintenant par `esp32::crash_handler_has_data()`, le rejoue dans ses
  lignes quand la raison du reset est un plantage, et l'efface une fois le journal arrivé à HA
  (comme ESPHome après un abonnement aux logs) : un vieux rapport jamais lu ne fait pas passer
  les appuis suivants sur le bouton pour des plantages.
- **Entité « Tab5 Raison du redémarrage »** : pour cette raison, ESPHome répète la source du
  dernier redémarrage demandé, qu'il n'efface jamais (`debug_esp32.cpp`). Le filtre publie
  maintenant `Power button or RTC watchdog (rst 0x..)`, ou `Crash, other watchdogs (rst 0x..)`
  avec un rapport de plantage.
- **Garde « reboot inattendu »** (`packages/tab5_health.yaml`) : laisse passer
  `Power button or RTC watchdog`, comme un redémarrage demandé.
- Ce qui n'alerte plus : un démarrage bloqué plus de 9 s (chien de garde RTC du bootloader)
  et un chien de garde de timer qui réinitialise sans passer par la panique ; l'historique de
  l'entité les garde. Test `tests/test_bouton_alim.py` (lit le classement dans le vrai code
  et rend la garde HA) ; cas dans `docs/troubleshooting.md`.

### 2026-10-06 — Rangée sous l'horloge : jusqu'à trois lignes de capteurs, plus les plantes

Demande d'Axel : sous l'horloge, la zone des pots ne montrait que les plantes. Elle devient une
**rangée** de lignes qui tournent avec la carte centrale ([ADR-0031](docs/decisions/0031-row-under-the-clock.md)).
Firmware et blueprint ; un firmware plus ancien ignore les nouvelles clés (plantes seules), un
blueprint plus ancien garde l'écran d'avant.
- **Blueprint « Tab5 — emplacements », nouvelle section repliée « Sous l'horloge · Under the
  clock »** : trois lignes de quatre appareils au plus (capteurs, détecteurs, interrupteurs,
  lampes, clims… ; pas les scènes, scripts et boutons), la place de la ligne des plantes (en
  premier par défaut, deuxième, troisième ou masquée) et la durée d'une ligne (8 à 120 s, par pas
  de 8 s, 32 s par défaut). Laissée vide, rien ne change.
- **Calage** : la carte centrale garde ses 8 s ; la rangée change tous les N tours, 0,2 s avant
  elle (rotateur `tab5_central_rotator_auto` : 7,8 s, la rangée, 0,2 s, la carte), pour un
  enchaînement de haut en bas, avec la même animation que la carte centrale. Un appui sur la
  rangée passe à la ligne suivante ; un appui long sur la ligne des plantes ouvre Mes Plantes.
  De petits tirets sous la rangée montrent la ligne affichée.
- **Dessin** : une icône par appareil, la valeur des capteurs et des clims, à la plus grande taille
  qui tient dans la largeur de l'horloge (la police de la date du thème, puis plus petit, puis
  icône au-dessus de la valeur). Valeur colorée selon sa mesure : échelle des températures
  (« 21.4 ° », °F ramené), de l'humidité, des batteries, or pour la puissance et l'énergie (W /
  kW). Les tuiles des pièces gardent leurs couleurs.
- **Affichage seul** : les éléments de la rangée sont à part des tuiles (variable `rangee` du
  blueprint), la liste blanche des commandes ne les connaît pas.
- Contrat : clés `hp|place`, `hd|secondes` et `hLI|type|icône|options|complément|nom|classe` dans
  `tab5_maj_tuiles`, états `hLI|état|valeur|couleur` dans `tab5_maj_emplacements` (préfixe `h`
  pour ne pas se confondre avec le bandeau d'état). Modèle et NVS à part (« RAN1 ») dans
  `tab5_tuiles.cpp` ; dessin et rotation dans la nouvelle unité `tab5_rangee.cpp`, nouveau package
  `tab5-rangee.yaml`, composants `rangee.yaml` et `rangee_element.yaml` ; la zone tactile
  `btn_pots_detail_zone` devient `btn_rangee`.
- Démo : trois lignes (plantes, climat, énergie et maison) ; le rendu hors tablette capture les
  lignes 2 et 3 (`accueil-rangee-ligne-2`, `-3`). Le job « Installation dans un HA neuf » vérifie
  les clés de la rangée dans un vrai Home Assistant. Tests `tests/test_rangee.py` (calage,
  bornes, clés des deux côtés, blueprint rendu, aucune commande, rendu). Docs : notice (écran
  d'accueil, plantes), étape 6 de l'installation, architecture, cartographie, `Tab5/README.md`.
  **Pas encore essayé sur la tablette.**

### 2026-10-06 — Écran : extinction automatique au choix, rallumage à « Okay Nabu »

Demande de la discussion #278 : « sur batterie ou sur USB, l'écran ne devrait se réveiller que
quand on le touche ». Jusqu'ici rien n'éteignait l'écran tout seul (la main ou l'automatisation de
présence de HA). Firmware ; le tableau de bord de HA gagne la carte.
- **Nouveau réglage de l'appareil « Tab5 Extinction auto de l'écran »** (select, catégorie
  configuration, gardé en mémoire) : Jamais, 1 min, 2 min, 5 min, 10 min ou 30 min. **« Jamais »
  par défaut : rien ne change** pour qui ne choisit rien.
- Après ce délai sans toucher la dalle, l'écran s'éteint exactement comme quand HA l'éteint
  (`light.turn_off` du rétroéclairage, LVGL en pause). Le toucher, la tape (« Tab5 Tap-to-Wake »),
  HA et le réveil le rallument comme avant. Vérifié toutes les 10 s.
- Jamais pendant que le réveil sonne, que l'assistant vocal écoute, réfléchit ou répond, qu'un jeu
  de l'Arcade est ouvert, ou pendant une mise à jour du firmware (nouveau global `ota_en_cours`,
  posé par les deux `ota:`), ni avant la fin du démarrage.
- Le délai repart du dernier allumage, quelle qu'en soit la source (nouveau global
  `ecran_allume_ms`, posé par `on_turn_on` du rétroéclairage) : LVGL étant en pause écran éteint,
  un rallumage par HA ou par la tape gardait sinon une inactivité ancienne et l'écran se serait
  rééteint aussitôt. Le retour automatique à l'accueil n'est pas touché.
- Tableau de bord : la carte « Extinction auto de l'écran » dans la colonne « Tablette » de la vue
  Réglages, juste avant la tape (7 langues). Docs : réglages, notice (« Écran éteint »),
  `Tab5/README.md`, cartographie. Test `tests/test_extinction_auto.py` (options, défaut, ordre des
  délais, exclusions, allumage, OTA). Essayé sur la tablette d'Axel le 06/10/2026 : l'écran se
  rallume après la mise à jour et le reste marche.
- **Nouveau réglage de l'appareil « Tab5 Rallumer l'écran à Okay Nabu »** (interrupteur, catégorie
  configuration, **allumé par défaut**) : écran éteint, « Okay Nabu » le rallume quand l'assistant se
  met vraiment à écouter (mot de réveil actif, HA joignable), jamais sur « Stop » (branche
  `START_PIPELINE` de `tab5_wake_word_dispatch`). Avant, la réponse n'était que parlée, écran noir.
  Éteint, rien ne change : pour qui ne veut le rallumer qu'au toucher, le mot de réveil se
  déclenchant parfois seul. Carte dans la vue Réglages (7 langues), réglages, notice, test.

### 2026-10-06 — Volet : le package du volet à course simulée ne le rend plus muet

Retour de la discussion #278 : popup du volet ouvert, mais ni Ouvrir, ni Stop, ni Fermer ne
faisaient rien (et avant, l'appui court ne faisait qu'ouvrir). Home Assistant seulement :
**réimporter le blueprint** ; firmware inchangé.
- **Le blueprint « Tab5 — emplacements » ne confie un volet au package optionnel
  `volet_serre_tracking.yaml` que si sa liste « Tab5 · volet à course simulée » nomme ce
  volet.** Avant, la seule présence du package suffisait : copié sans choisir de volet (la
  liste reste sur « Aucun »), il recevait les commandes du volet de l'entrée Volet et son
  script s'arrêtait sans rien commander ; la tuile restait sur l'état « Fermé » qu'il tient
  (d'où l'appui court qui n'envoyait que « ouvrir »), et le popup n'avait pas de curseur.
  Sur « Aucun », chaque volet est maintenant commandé directement, avec son vrai état.
- Le volet suivi est celui de la liste, même s'il n'est pas dans l'entrée Volet : sa tuile
  passe par le script, l'entrée Volet est commandée directement.
- Changer la liste repousse aussitôt les tuiles de l'ancien et du nouveau volet (nouveau
  déclencheur `volet_choisi`), et l'état du volet de l'entrée Volet ; le package optionnel
  repousse aussi le sien (« Sync Tab5 Volet Serre » se déclenche sur la liste).
- Chez qui a choisi son volet dans la liste (l'auteur), rien ne change.
- Tests du rendu du blueprint (`tests/test_tuiles_blueprint.py`) : liste sur « Aucun », liste
  sur un autre volet, liste changée ; contre-épreuve faite (rouges sur l'ancien blueprint).
  Docs (`HomeAssistant_Config/README.md`, étape 1 du guide), `AGENTS.md`, cartographie.

### 2026-10-05 — Le site s'ouvre sur l'accueil avec menu, et c'est le README

Lot 4 du site de documentation (ADR-0030, amendement du 05/10) : l'adresse du site ouvrait encore
l'ancienne vitrine, sans menu. Doc et site seulement, firmware inchangé.
- **La racine du site renvoie vers `en/` ou `fr/`** selon la langue du navigateur (les deux liens
  restent sans JavaScript). La page d'installation renvoie à l'accueil de sa langue.
- **Le README est la page d'accueil du site**, en anglais et en français : un seul texte pour
  GitHub et le site, au lieu de la vitrine recopiée à la main. Il reprend l'essentiel de la
  vitrine, en plus court : accroche et photo, liens (installer, guide, notice, mode démo), ce que
  fait l'écran avec un lien vers chaque page de la notice, pluie dans l'heure et vigilances,
  démarrage rapide en sept étapes, compatibilité matérielle, vidéo (un simple lien vers YouTube),
  tour animé, thèmes, énergie, galerie de photos. Titre et fiche JSON-LD de l'accueil gardés.
- **Sorti du README** : la longue liste des fonctions (dans `docs/screens.md` et la notice), le
  tableau de l'Arcade (`docs/arcade.md`), les choix de conception (`docs/architecture.md`), la
  section vocale (`docs/voice_assistant.md`), l'arborescence du dépôt (`CARTOGRAPHIE_TAB5.md` ;
  celle du README comptait encore 45 composants) et la note personnelle entière, devenue la page
  « L'histoire » (`docs/story.md`).
- `docs/README.md`, le sommaire de la documentation, devient la page « Documentation » du site.
  Seule l'image de partage garde un nom parlant sous `images/` ; les autres sont celles de la
  documentation, toujours listées dans `sitemap.xml`.
- `tests/test_doc_comptes.py` lit désormais dans `docs/architecture.md` les comptes déménagés
  (packages, fichiers de plus de 500 lignes, composants) ; `tests/test_site_doc.py` vérifie que
  l'accueil est le README et que la racine mène aux deux langues.

### 2026-10-05 — Notice d'utilisation : chaque tap et appui long, zone par zone

Lot 3 du site de documentation : une notice pour se servir de la tablette, dans le menu
« Utiliser la tablette » du site, en anglais et en français. Doc seulement, firmware inchangé.
- `docs/notice/` : une page par zone (accueil, rangée du bas et pièces, lumières, volets, clim,
  télécommande TV, calendrier, réveil, voix, énergie, plantes, console système, arcade), chaque
  tap, appui long et glissement dans un tableau ou une liste ; l'accueil est montré numéroté
  (14 repères, avec leur légende).
- Les images sont les captures du rendu hors tablette (FR et EN), recadrées et allégées en WebP
  par `tools/site/images_notice.py` (42 images, 1,7 Mo) : rien n'est photographié chez l'auteur.
- `tests/test_notice.py` : un appui long ajouté au YAML, un geste de plus ou une nouvelle fenêtre
  dans le rendu fait échouer `pytest` tant que la notice ne les décrit pas ; une page ne cite
  qu'une image présente, et aucune image ne reste sans page.
- `docs/screens.md` garde le détail technique et renvoie à la notice pour les gestes. Erreurs
  corrigées en l'écrivant : glisser vers la gauche fait le tour des fenêtres des jours et revient
  à l'accueil, vers la droite celui des heures (la page disait que le balayage ne bouclait pas) ; la croix des popups fait 80×44, pas 96×64 ; la disposition de la
  télécommande TV ; les sections françaises en double (assistant, calendrier, plantes) retirées.

### 2026-10-05 — Les captures de Home Assistant du guide, prises par la CI

Suite du guide d'installation (lot 2b) : les écrans de Home Assistant des étapes 4 à 7 sont
photographiés par le test « HA neuf » (`.github/workflows/installation-ha.yml`), en anglais et en
français, sur un Home Assistant neuf avec des données de test : rien de personnel, et la même
capture se refait quand l'interface de Home Assistant change.
- `tools/installation_ha/captures_ha.py`, à la fin du test : Chromium (Playwright) ouvre
  l'interface avec le compte de la CI et photographie la page de l'appareil de la tablette, les
  listes « Tab5 · », l'automatisation du blueprint et la ligne du tableau de bord dans l'éditeur
  de modèle. Chaque capture vérifie la langue de la page et ce qu'elle doit montrer ; une
  capture ratée fait échouer le job (non requis). Mise en page étroite de Home Assistant
  (860 px) : une colonne, lisible une fois réduite dans la page.
- Dans le guide (`docs/images/installation/`, une image par langue) : la page de l'appareil
  (étape 4), les listes (étape 5, à la place de la capture précédente), l'automatisation du
  blueprint (étape 6), la ligne du tableau de bord et son résultat (étape 7). Les vues du
  tableau de bord et des réglages restent celles de #319.
- Étape 5 : chercher **select.tab5_** montre les listes seules (« Tab5 · » montre aussi les
  textes et les capteurs des packages).
- L'automatisation de la CI porte un nom de vraie maison, « Tab5 — emplacements de l'écran ».

### 2026-10-05 — Documentation : régler les deux modes vocaux

Question d'un utilisateur ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) :
comment avoir un assistant vocal avec une IA locale. La doc ne disait nulle part, côté
utilisateur, comment monter les deux assistants. Doc et tableau de bord seulement, firmware
et packages inchangés.
- `docs/installation/settings.md` (EN et FR) : section « Assistant vocal : les deux modes » — Domo =
  l'assistant préféré de HA, Discu = celui de « Tab5 · pipeline de discussion », « Aucun »
  masque les boutons, la liste « Assistant » de l'appareil suit les boutons ; option tout en
  local (Ollama, Whisper ou Speech-to-Phrase, Piper, Home LLM), avec les pages officielles ;
  mot d'activation et second mot. Liée depuis « Ce qu'il faut », la liste « Tab5 · pipeline
  de discussion » (étape 5) et les réglages Voix.
- `docs/voice_assistant.md` (EN et FR) : n'importe quel agent de conversation convient ;
  vromvrom-engine est le moteur de l'auteur (pourquoi il l'a, et pourquoi il ne le conseille
  pas : un gros brouillon géré par l'IA) ; il n'est plus présenté comme un passage obligé.
- Tableau de bord HA, vue Réglages, section « Assistant vocal » : une explication et un lien
  vers la doc, dans les sept langues (traductions écrites par une IA, non relues).

### 2026-10-05 — Le guide d'installation en pages, étape par étape

Suite du site de documentation (lot 2 du plan validé par Axel) : le long `docs/installation.md`
devient un guide de pages courtes, `docs/installation/`, une page par étape, dans le menu du site
« Installer → Guide d'installation ».
- **Sept étapes, dans l'ordre** : fichiers Home Assistant, installer le firmware, Wi-Fi, ajouter
  la tablette à Home Assistant, vos sources (listes « Tab5 · »), vos appareils (blueprint), un
  tableau de bord. Chaque page dit ce qu'il faut faire, à quoi on voit que c'est bon, et les
  pièges déjà rencontrés (pas de bouton BOOT, même version = pas de bouton *Install*, fenêtre de
  30 minutes, archive décompressée dans un sous-dossier, tablette non découverte : son IP et le
  port 6053, effacement = réglages d'usine). Puis : réglages de la tablette, adapter à sa
  maison, fournisseurs météo, mises à jour (et passage depuis une ancienne version), compiler
  son propre firmware.
- **Aucun lien cassé** : `docs/installation.md` garde chacun de ses anciens titres, avec un lien
  vers sa nouvelle place (tableau de bord HA déjà installé, notifications, forums). README,
  vitrine, page d'installation, `docs/` et l'archive HA (`LISEZMOI-Tab5.txt`, adresse du guide
  sur le site) visent directement les nouvelles pages ; un test refuse une page publiée qui
  viserait encore l'ancien fichier.
- Les alertes de GitHub (`> [!WARNING]`) deviennent des encarts sur le site, titrés dans la
  langue de la page.

### 2026-10-05 — Un site de documentation, construit depuis docs/

Demandé par Axel : « un vrai site », des pages, un menu à gauche avec des sous-menus, sans
maintenir deux fois les mêmes textes ([ADR-0030](docs/decisions/0030-documentation-site.md)).
- **La documentation devient un site** : `/en/` et `/fr/` à côté de la vitrine et de la page
  d'installation, avec un menu à gauche et ses sous-menus, une recherche, le mode sombre, un
  menu repliable sur téléphone et un sélecteur de langue qui garde la page.
- **Une seule source** : les fichiers de `docs/` ne bougent pas et restent lisibles sur GitHub.
  `tools/site/construire.py` coupe chaque fichier bilingue à « ## Version Française », réécrit
  les liens (page du site, image copiée, sinon github.com), traduit l'ancre d'un titre de
  l'autre langue, puis MkDocs + Material (figés avec empreintes, sans autre plugin que la
  recherche) construisent en mode strict. Menu unique : `tools/site/menu.yml`.
- **Nouvelle page d'accueil de la documentation**, `docs/README.md`, lue aussi sur GitHub en
  ouvrant `docs/`.
- `site.yml` construit la documentation et `pages.py assembler --doc` la pose sans rien
  remplacer d'autre (les firmwares lisent leurs mises à jour dans `stable/` et `beta/`) ; un
  seul `sitemap.xml`, images comprises ; une page 404. La vitrine et la page d'installation
  mènent aux pages du site, dans la langue choisie.
- `tests/test_site_doc.py` reconstruit tout le site (~3 s) : lien ou ancre cassés dans `docs/`,
  page oubliée du menu, balises de référencement, liens de la vitrine, empreintes des paquets.

### 2026-10-05 — Un popup pour les volets (appui long)

Demandé dans la [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) :
la position d'un volet et ses boutons, dans une fenêtre.
- **Appui long d'une tuile volet** (épaules météo ou carte du mode HA) : un popup au nom de
  la tuile, au lieu d'envoyer l'autre sens. À gauche, la position en grand (« 45 % ») et
  l'état en mots (« Ouvert », « Fermé », « Partiel », « En mouvement », « Hors ligne ») ; un
  curseur 0-100 %, seulement si la position est connue, qui n'envoie qu'au relâcher. À
  droite, **Ouvrir / Stop / Fermer**. Le popup suit le volet tant qu'il est ouvert. Avec
  l'option « Confirmer » (`k`), l'appui long garde l'ancien geste (la confirmation n'est
  jamais contournée) ; « Lecture seule » : rien ; mode héritage 3.x : inchangé.
- **Home Assistant** : nouvelle commande de tuile `position` (0-100), que le blueprint
  « Tab5 — emplacements » passe à `cover.set_cover_position` / `valve.set_valve_position`
  sur l'entité de **cette tuile** seulement, si elle sait régler une position ; une valeur
  illisible ne part pas. **Réimporter le blueprint** : l'ancien ignore `position` (le curseur
  ne ferait rien, les boutons marchent). Le volet à course simulée
  (`optionnel/volet_serre_tracking.yaml`) pousse désormais `nan` au bout de sa course au
  lieu de 100 / 0 (même flèche) : pas de curseur pour lui, il ne sait pas régler une position.
- Textes de l'écran dans les sept langues (traductions faites par une IA, non relues).
- ADR-0023 (mise à jour du 05/10/2026), `docs/screens.md`, `Tab5/README.md` ; tests du
  geste, des commandes et de la branche du blueprint (`tests/test_tuiles_firmware.py`,
  `tests/test_tuiles_blueprint.py`) ; deux captures du rendu hors tablette (`volet`,
  `volet-sans-position`).

### 2026-10-05 — Batterie de la tablette : une prise quand il n'y en a pas

Demandé par husyildiz ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) :
sans batterie, l'icône devient une prise.
- **Détection d'après la tension** : une batterie 2S en état de marche ne lit pas sous
  6 V ; une lecture de l'INA226 sous **6,0 V** dans les **10 dernières minutes** = pas de
  batterie, dix minutes sans = une batterie. Relevés : batterie d'origine ~7,2 V et sans
  batterie 4,2 V (husyildiz, 05/10) ; chez l'auteur, sans batterie, 4,2 ↔ 8,39 V toutes
  les 1 à 3 min (03/10) puis 5,71 V stable (04/10). La fenêtre couvre l'alternance.
  Fonction pure `batterie_lecture()` (`tab5_core.h`), testée par
  `tools/test_alarm_clock.cpp` (CI) ; chaque lecture passe par `on_raw_value`, avant le
  filtre de 50 mV.
- **Nouvelle entité « Tab5 Batterie détectée »** (diagnostic, activée par défaut) : la
  décision, inconnue avant la première lecture. Carte dans la vue Santé du tableau de bord
  (`custom_templates/tab5_dashboard.jinja`, sept langues).
- **« Tab5 Batterie »** vaut inconnu tant que la batterie n'est pas détectée (avant : 100 %
  avec 8,39 V, 0 % avec 5,71 V).
- **Icône** (interrupteur « Tab5 Batterie montée » allumé, inchangé) : prise `power-plug`
  (plus lisible à 26 px que le symbole USB), couleur du texte du thème, repeinte avec le
  thème ; sinon les paliers, l'éclair et les couleurs d'avant. Rendu hors tablette :
  capture `accueil-batterie-prise`, l'action `rendu_batterie` prend la tension.
- `docs/hardware.md` (les trois relevés datés au lieu du seul « 8,39 V, 100 % »),
  `docs/installation.md`, `docs/screens.md`, `Tab5/README.md`. Non testé sur une tablette.

### 2026-10-05 — Appareils des pièces sur la météo : au choix

Retour d'un utilisateur ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) :
les icônes des appareils au-dessus des prévisions ne lui servent pas.
- **Interrupteur « Tab5 Appareils sur la météo »** (réglage de l'appareil dans Home Assistant,
  `tab5-ha-controls.yaml`), allumé par défaut : rien ne change pour qui n'y touche pas. Éteint,
  les cartes de prévisions ressemblent à une page sans appareil : ni épaules, ni bouton d'action
  invisible, ni bascule du sens d'un volet par le titre. Le bouton « HA » montre toujours les
  pièces et leurs appareils. Pris en compte tout de suite, gardé d'un démarrage à l'autre
  (`tuiles_appareils_meteo()`, `tab5_tuiles.cpp` ; amendement du 05/10 de l'ADR-0023).
- Tableau de bord HA : l'interrupteur et une ligne d'explication dans la vue Réglages, dans les
  sept langues (traductions écrites par une IA, non relues).
- Rendu hors tablette : `accueil-sans-appareils` et `accueil-sans-appareils-heures-1` (action
  `rendu_appareils_meteo`). Non testé sur la tablette.

### 2026-10-05 — Énergie : plusieurs sources solaires additionnées

Demandé par husyildiz ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) :
une installation à plusieurs onduleurs ou chaînes de panneaux (« pv1 + pv2 + … »).
Côté Home Assistant seulement, firmware et contrat inchangés
([ADR-0028](docs/decisions/0028-solar-energy-popup.md), amendement du 05/10).
- Section « Énergie » du blueprint : deux champs en liste, **Autres puissances solaires** et
  **Autres énergies solaires produites**, à côté des champs d'avant qui restent une seule
  entité (une automatisation déjà créée y a une chaîne, que l'éditeur de HA n'afficherait
  plus si le champ passait en liste).
- Puissance solaire = somme des sources qui ont une valeur, chacune convertie en W ; un
  onduleur indisponible la nuit est ignoré, toutes indisponibles = inconnu. Elle sert à la
  carte Solaire, à la maison calculée et à l'icône du bandeau (la crête est alors celle de
  tous les panneaux).
- Énergie produite = somme des compteurs (kWh ou Wh), période par période : total du jour,
  barres des heures, des jours et des mois ; un compteur sans donnée sur une période y
  compte pour 0. Une tuile de n'importe laquelle de ces sources ouvre le popup Énergie.
- Rien ne change avec une seule source ; un package `tab5_energie` plus ancien prend la
  première source sans erreur. Non essayé sur une vraie installation à plusieurs onduleurs.
- `tests/test_energie.py`, `tests/test_solaire.py` : sommes, sources indisponibles,
  compteurs à trous, ancien format identique.

### 2026-10-05 — Tableau de bord HA dans les sept langues de l'écran

Demandé par Axel après les vues Réglages et Santé : leurs explications n'existaient qu'en
français et en anglais.
- Les 250 textes du tableau de bord (libellés, explications, cases « En bref ») suivent la
  langue de l'écran : français, anglais, allemand, néerlandais, espagnol, italien ou turc ;
  une autre langue donne l'anglais. `t(français, anglais)` ne change pas : les cinq autres
  langues viennent de la table `TRADUCTIONS` en fin de macro (clé = le texte français).
- Traductions écrites par une IA (une par langue, d'après le français et l'anglais, avec le
  vocabulaire de `Tab5/lang/<code>.yaml`), non relues.
- `tests/test_tableau_de_bord.py` : chaque texte rendu a ses cinq traductions, sans entrée
  orpheline ; aucune traduction n'a d'apostrophe ou de guillemet droits ; ses espaces de
  début et de fin suivent l'anglais (morceaux de phrase) ; le rendu complet passe dans les
  sept langues ; une langue inconnue donne l'anglais.
- `docs/translations.md` (choisir la langue, ajouter une langue) et l'étape 7 de
  `docs/installation.md`.

### 2026-10-05 — Tableau de bord HA : la vue « Santé » explique quoi faire

Même traitement que la vue Réglages, demandé par Axel.
- **Quand quelque chose cloche** (en tête) : que vérifier par symptôme (écran figé, tablette
  déconnectée ou écran noir après une mise à jour, redémarrages, fichiers HA en retard, zone
  absente), avec les cas déjà diagnostiqués de `docs/troubleshooting.md` ; liens vers la page
  de la tablette, le journal et les réparations de HA, la page de dépannage.
- **En bref** : liaison, push HA, date et raison du dernier démarrage, firmware (et mise à
  jour disponible), fichiers HA (et en retard), entités indisponibles, signal Wi-Fi, alertes
  de santé coupées ; ce qui demande une action est en orange.
- Une courte explication sous État, Performances (lire la forme des courbes, lien vers les
  mesures de référence), Réseau, Poussées et Alertes de santé (notifications, à couper
  pendant une mise à jour).
- La macro `doc()` prend le fichier de `docs/` ; `tests/test_tableau_de_bord.py` vérifie les
  ancres de chaque fichier cité, et les liens et le tableau « En bref » de la vue Santé.

### 2026-10-05 — Tableau de bord HA : tout pour régler la tablette dans « Réglages »

Remarque d'Axel : dans la vue Réglages, rien ne disait où brancher ses panneaux solaires
ni où donner leur puissance crête (l'icône du bandeau) ; même chose pour les capteurs.
- **Régler la tablette** (en tête) : les trois endroits où tout se règle, et des liens vers
  la page de la tablette (intégration ESPHome : réglages, diagnostic, mises à jour), sa
  connexion ESPHome, l'automatisation des emplacements et la documentation.
- **Ce que l'écran affiche** : chaque section du blueprint « Tab5 — emplacements » (pièces,
  TV et téléphone, températures, clim, plantes, planning, météo, énergie), ce qu'on y
  choisit et ce qui apparaît à l'écran, puis le lien et « Zones masquées ». Aucun modèle
  de HA ne lit les choix d'un blueprint : la vue les explique, elle ne peut pas les montrer.
- **Énergie solaire** : puissance solaire, puissance crête (kWc, 0 = pas d'icône), popup
  Énergie, pas à pas ; liens vers les emplacements et le tableau Énergie de HA ;
  avertissement si le package `tab5_energie` manque.
- « En bref » dit si l'automatisation des emplacements existe et tourne, et les zones
  masquées ; courtes explications et liens pour la météo, la maison (téléphone, présence),
  les agendas (ajouter une intégration) et la voix (assistants vocaux de HA).
- `tests/test_tableau_de_bord.py` : les nouveaux liens, l'avertissement sans le package, et
  chaque lien vers `docs/installation.md` vise un titre qui existe (français et anglais).

## Versions 3.0.0 à 3.6.0

Archivées dans [`docs/changelog/CHANGELOG-3.0-3.6.x.md`](docs/changelog/CHANGELOG-3.0-3.6.x.md)
(3.0.0 du 28/09/2026 → 3.6.0 du 05/10/2026).

## Versions 2.x

Archivées dans [`docs/changelog/CHANGELOG-2.x.md`](docs/changelog/CHANGELOG-2.x.md)
(2.0.0 du 25/09/2026 → 2.2.0 du 27/09/2026).

## [1.2.0] et versions antérieures

Archivées dans [`docs/changelog/CHANGELOG-1.x.md`](docs/changelog/CHANGELOG-1.x.md)
(1.0.0 du 06/07/2026 → 1.2.0 du 08/09/2026).
