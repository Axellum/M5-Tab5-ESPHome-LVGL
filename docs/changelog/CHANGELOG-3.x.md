# Changelog — versions 3.x (archive)

Les versions 3.0.0 à 3.6.0 ont été archivées le 08/10/2026, après la 3.7.0, la 3.7.0
après la 3.8.0, et les 3.8.0 et 3.8.1 le 25/10/2026, après la 3.9.0 : le
[`CHANGELOG.md`](../../CHANGELOG.md) ne garde que `[Unreleased]` et la dernière version
mineure. Les dates sont celles du merge dans `main`. Le texte des sections est inchangé, sauf
les liens relatifs recalés pour ce dossier (`docs/…` → `../../docs/…`) : 29 dans les 3.8.0 et
3.8.1, 20 dans la 3.7.0, 15 dans les versions 3.0.0 à 3.6.0.

## [3.8.1] — 2026-10-11

Version de sécurité et corrective : protection thermique anti-surchauffe et fiabilité de la détection batterie sans batterie physique.

- **Ordre de mise à jour** :
  - **Contrat HA ↔ firmware** : compatible dans les deux sens (depuis v3.8.0).
- **Sécurité thermique (anti-surchauffe)** (#457) :
  - Verrou thermique autonome sur l'ESP32-P4 en C++ (`Tab5/socle/tab5_batterie.h`, `Tab5/ecran/tab5_batterie.cpp`) : si le SoC dépasse 55 °C ou subit une élévation brutale (+4 °C en <= 60 s avec CPU > 48 °C), coupure immédiate et verrouillage du chargeur (CHG_EN et Quick Charge) pendant au moins 15 minutes et jusqu'à refroidissement complet sous 44 °C.
  - Surveillance accélérée de la température cœur `sys_core_temp` avec période ramenée à 10 s et filtrage dynamique vers Home Assistant (`Tab5/paquets/tab5-sensors-diagnostics.yaml`).
- **Détection batterie sans batterie physique** (#457) :
  - Rehaussement du seuil de présence batterie (`kBatterieSeuilV`) de 3,0 V à 5,5 V pour éviter les faux positifs dus aux tensions résiduelles/flottantes (3,29 V relevés) lorsque la tablette est alimentée sans batterie physique.
- **Documentation et maintenance** (#456) :
  - Archivage de la section 3.7.0 dans `docs/changelog/CHANGELOG-3.x.md`.

## [3.8.0] — 2026-10-10

De `v3.7.0` à aujourd'hui : soixante-dix-sept pull requests (#377 → #454, sans #398, fermée),
dont quatre de pré-release (#380, #403, #406, #433) et trois de Dependabot (#396, #397, #399), et
celle de la release. Quatre sont nées de la
[discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) (#379, #429,
#444, #451) — merci à @husyildiz, pour son test de consommation et la capture de son tableau
Énergie. Stable publiée trois jours après la 3.7.0, à la demande de l'auteur, au lieu des deux
semaines habituelles entre deux stables ; vingt-deux de ces PR (#429, #434 → #454) sont arrivées
après la [v3.8.0-rc.4](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.8.0-rc.4)
et ne sont passées par aucune pré-release.
- **Nouveaux popups** : **Musique**, un lecteur pour tout `media_player`, avec une mini-barre
  « en lecture » sur l'accueil (#430, #436, [ADR-0050](../../docs/decisions/0050-music-player.md)) ;
  **Caméras**, une image fixe par caméra, rangées par pièce, en mosaïque, téléchargées et décodées
  hors de la boucle principale (#429, #439, #440, #446, #447, #449,
  [ADR-0049](../../docs/decisions/0049-cameras-popup.md), [ADR-0057](../../docs/decisions/0057-cameras-rooms.md)) ;
  **Suivi**, les courbes des capteurs choisis dans Home Assistant (#438,
  [ADR-0054](../../docs/decisions/0054-tracked-sensors.md)) ; **Frigos et congélateurs**, normes, porte
  mal fermée, alertes et, au niveau grave, une icône qui clignote sur l'horloge (#441,
  [ADR-0055](../../docs/decisions/0055-fridge-freezer-monitoring.md)) ; **Serveur IA**, le tableau de
  bord d'un serveur de LLM local (débit, VRAM, température du GPU, RAM, puissance) avec
  Décharger, Réveiller (Wake-on-LAN) et Redémarrer (#450, #453,
  [ADR-0059](../../docs/decisions/0059-local-llm-server-popup.md),
  [ADR-0060](../../docs/decisions/0060-ai-server-actions.md)), alimenté par les nouveaux packages
  `tab5_llm.yaml` (Ollama, llama.cpp, LM Studio, #448) et `tab5_serveur_ia.yaml`.
- **Popups en pages**, une seule brique pour le glissé et les noms des pages
  ([ADR-0046](../../docs/decisions/0046-popups-a-pages.md)) : Réglages en quatre pages, la console
  système devient la page Système (#400) ; Lumières et Volets, une page par pièce (#424) ; Météo
  en graphiques sur trois pages (#425, [ADR-0043](../../docs/decisions/0043-weather-popup.md)) ; Réveil
  en cinq pages, réglé sur des rouleaux (#426) ; Température avec l'humidité, une page par pièce
  (#422, [ADR-0047](../../docs/decisions/0047-room-climate-chart.md)) ; Énergie en quatre pages : flux,
  soleil et prévision du jour apprise sur la maison, production, bilan avec revente et gains
  (#451, [ADR-0058](../../docs/decisions/0058-energy-sun-forecast-balance.md)) ; télécommande : TV,
  Apple TV, Freebox Player, une page chacune (#445, [ADR-0056](../../docs/decisions/0056-several-remotes.md)).
- **Accueil au choix** : la zone à gauche de l'horloge montre le vocal, un graphique des 15
  prochaines heures, un lecteur compact ou un capteur suivi (#431, #432, #438,
  [ADR-0051](../../docs/decisions/0051-left-zone-choice.md)) ; l'horloge en trois zones et douze gestes
  au choix dans le blueprint (#415, [ADR-0039](../../docs/decisions/0039-gestes-accueil.md)) ; jusqu'à
  trois lignes de capteurs dans le panneau « Ok Nabu », défilement automatique ou fixe (#418,
  [ADR-0041](../../docs/decisions/0041-ok-nabu-panel-scrolling.md)) ; tuiles météo en cellule pleine et
  fond de l'horloge au choix (#443) ; mention « prévisions périmées » quand la météo n'arrive plus
  (#395) ; lignes centrées, cadres et points ajustés (#419, #428).
- **Roues et climat** : roue d'actions rapides à deux anneaux (#378) ; roue de navigation sur
  l'appui long de la carte centrale, Musique et TV dans « Appareils » (#423, #434,
  [ADR-0042](../../docs/decisions/0042-navigation-wheel.md)) ; roue de la clim au toucher de la
  température de la pièce, et carrousel des clims (#414, #420,
  [ADR-0038](../../docs/decisions/0038-climate-carousel.md),
  [ADR-0048](../../docs/decisions/0048-climate-wheel-from-temperature.md)) ; le climat de chaque pièce
  en mode HA : température, humidité, clim (#416, [ADR-0040](../../docs/decisions/0040-room-climate.md)) ;
  « Son de la tablette » dans la liste de la tuile − / + (#379).
- **Batterie et alimentation** : plus de souffle sans batterie (le chargeur ne charge plus dans le
  vide), limite de charge à 80 %, consommation en W, batterie faible sur le téléphone (#387) ;
  chargeur laissé éteint au démarrage quand le démarrage précédent n'a vu aucune batterie (#452) ;
  mode de charge, niveau des animations et Wi-Fi éco (#421,
  [ADR-0045](../../docs/decisions/0045-charging-animations-eco-wifi.md)) ; un test de consommation
  lancé depuis Home Assistant (#405), `tools/mesure_conso.py` (#404) et une première mesure
  publiée, page « Consommation » (#444).
- **Plus fluide** : changer de thème ne fige plus l'écran 3,2 s (#383) ; le popup Caméras ne gèle
  plus l'écran à chaque image (#439, #440).
- **Home Assistant** : l'intégration HACS a un assistant de configuration qui part des pièces de
  Home Assistant, puis propose la maison et les listes « Tab5 · … », en sept langues (#435, #437,
  [ADR-0052](../../docs/decisions/0052-setup-assistant-rooms.md),
  [ADR-0053](../../docs/decisions/0053-setup-assistant-home-lists.md)) ; trois défauts de l'intégration
  corrigés (#390) ; repli automatique sur une autre météo quand la source choisie se tait (#393) ;
  plus de garde-fou `is_primary_active`, qui pouvait figer l'écran sans erreur (#382) ;
  statistiques de fiabilité au-delà de 7 jours (#413) ; chronologie du démarrage dans une entité
  de diagnostic (#452).
- **Solidité** (audits du 30/09 et du 07/10) : correctifs du firmware et de Home Assistant (#381,
  #382) ; lecture des payloads de HA en C++ pur, testée sur PC et fuzzée, et ses défauts corrigés
  (#411, #417) ; contrat HA ↔ firmware vérifié entre versions (#410) ; socle C++ commun, code des
  tuiles, YAML et données des thèmes rangés, polices figées dans le dépôt (#386, #388, #389, #392,
  #394) ; moteur des dames dans son module (#412) ; garde-fous, tests et CI (#385, #391, #427) ;
  `Tab5/` rangé en sous-dossiers (#401) ; normes de style et CHANGELOG archivé (#402) ; page
  d'installation sous CSP et SRI, `SHA256SUMS` et provenance des binaires (#409).
- **Documentation** : photo et rendus du firmware dans le README et le site (#377) ; docs de
  l'audit du 07/10 (#384) ; première installation : ordre, prérequis, dépannage (#407, #408) ;
  mesures des caméras sur la tablette (#442) ; où choisir les capteurs suivis (#454) ; notices
  Musique, Caméras, Suivi, Frigos, Serveur IA et page « Consommation ».

**Contrat HA ↔ firmware** : le firmware d'abord (depuis v3.7.0).

**Ordre de mise à jour** (`python tools/contrat_api.py --matrice`, lu dans le code, pas essayé) :
le firmware 3.7.0 avec les fichiers HA de la 3.8.0 est « cassé (6) » : six actions qu'il n'a pas
(`tab5_maj_energie_soleil`, `tab5_maj_energie_bilan`, `tab5_maj_froid`, `tab5_maj_lecteur`,
`tab5_maj_serveur_ia`, `tab5_maj_suivi`) sont refusées par Home Assistant et le script qui les
appelle s'arrête ; le firmware 3.8.0 avec les fichiers HA de la 3.7.0 est « dégradé (11) » : il
marche, mais les nouveaux popups restent vides, sans erreur. Même verdict depuis la v3.8.0-rc.4
(canal bêta).

### À faire en mettant à jour depuis la 3.7.0

1. **Firmware d'abord** : entité « Firmware » dans Home Assistant. Avec l'intégration « Tab5 »
   (HACS) : *Download* de la nouvelle version dans HACS et le redémarrage qu'il demande ;
   l'intégration pose alors les fichiers, puis lance elle-même la mise à jour de la tablette
   (option cochée par défaut). Dans ce cas les fichiers arrivent avant le firmware : jusqu'à la
   fin de l'OTA, les six nouvelles poussées sont refusées.
2. **Puis les fichiers Home Assistant**, à la main : remplacer les fichiers par ceux de
   `tab5_home_assistant.zip` de cette version. D'abord `custom_templates/` (nouveaux :
   `tab5_tablette.jinja`, `tab5_meteo.jinja`), puis **Outils de développement → YAML → Modèles
   Jinja personnalisés** ; ensuite les packages, dont six nouveaux (`tab5_lecteur.yaml`,
   `tab5_suivi.yaml`, `tab5_froid.yaml`, `tab5_llm.yaml`, `tab5_serveur_ia.yaml`,
   `tab5_mesure_conso.yaml`), et le blueprint « Tab5 — emplacements » (le réimporter s'il a été
   importé par son URL) ; recharger toute la configuration YAML. Le popup Serveur IA a besoin de
   `tab5_llm.yaml` et de `tab5_serveur_ia.yaml`, recopiés après le firmware ; son bouton Réveiller
   charge l'intégration Wake on LAN (`wake_on_lan:` dans `tab5_llm.yaml`) : redémarrer Home
   Assistant si la clé n'était pas déjà dans `configuration.yaml` (l'intégration « Tab5 » la
   charge sans redémarrage).
3. **Ménage** : `input_boolean.is_primary_active` et les automatisations « Force Primary Active on
   Boot » et « Tab5 Santé — is_primary_active OFF depuis 5 min » disparaissent ; si Home
   Assistant les garde en « indisponible », les supprimer dans Paramètres → Entités. Le snippet
   `tab5_alerts_dismissed_input_text.yaml` n'existe plus.
4. **Quand vous voulez** : dans le blueprint, les nouvelles sections (« Zone à gauche de
   l'horloge », « Panneau Ok Nabu », « Horloge et boutons du haut » et ses douze gestes,
   « Caméras », les autres télécommandes, la température, l'humidité et la clim de chaque pièce,
   les nouvelles entrées de l'énergie) ; les listes « Tab5 · lecteurs de musique », « Tab5 ·
   capteurs suivis », « Tab5 · réfrigérateurs », « Tab5 · congélateurs », « Tab5 · type de
   serveur IA » et les autres « Tab5 · serveur IA, … » ; les réglages de la tablette (Mode de
   charge, Limite de charge, Animations, Wi-Fi éco, Fond de l'horloge) ; le tableau de bord à
   refaire (étape 7 du guide), pour les nouvelles entités.

### Mesures de la version

- Taille, lue dans le journal de la CI (job `build-min` de `main` à `5bc4634`, ESPHome 2026.9.0,
  la version des binaires publiés ; la publication recompile) : application 5 051 426 o (62,2 %
  de 8 126 464 o), contre 4 836 246 o (59,5 %) pour la v3.8.0-rc.4 publiée. RAM statique :
  239 834 o (53,8 %), contre 209 794 o (47,1 %) pour la rc.4 et 192 602 o (43,2 %) pour la 3.7.0.
  Binaires OTA publiés (`st7123`) : 4 591 616 o pour la 3.7.0, 4 853 760 o pour la rc.4 ; celui
  de la 3.8.0 n'existe qu'à la publication.
- Mesuré sur la tablette pendant le développement (chiffres des entrées ci-dessous) : changer de
  thème, prototype affiché en 0,44 à 0,51 s au lieu de 3,7 à 3,9 s (#383) ; Caméras, avec une
  caméra 640 × 480, décodage matériel en 3 à 12 ms au lieu de 1,0 s et pire tour de boucle de la
  minute à 217 ms au lieu de 1 020 ms (#440) ; sans batterie, plus de souffle une fois le chargeur
  coupé (firmware de test, #387) ; tuiles météo en cellule pleine, essai validé (#443) ;
  chronologie du démarrage, première version des repères, trois démarrages : expandeur à 7,2 s,
  rétroéclairage à 21,4 s. Consommation mesurée par @husyildiz sur la rc.4 : 2,63 W écran à
  100 %, 1,74 W à 50 %, 1,23 W écran éteint (#444).
- La RAM interne libre et la durée d'une image n'ont pas été remesurées sur la tablette pour
  cette version.
- CI sur `main` à `5bc4634` : compilations requises vertes (dernière ESPHome, 2026.9.1, et
  version minimale, 2026.9.0), job `python` vert (pytest, moteurs des jeux et parseurs sous ASan
  et UBSan) ; intégration HACS et installation dans un Home Assistant neuf (2026.9.4 et 2026.8.3)
  vertes. Rendu hors tablette et sanitizers : non finis à la rédaction, à lire sur ce commit.

### Problèmes connus

Ceux de la 3.7.0, et :
- **Peu essayée sur la tablette** : stable trois jours après la 3.7.0, par choix de l'auteur, et
  les vingt-deux PR d'après la rc.4 n'ont été dans aucune pré-release. Au merge, leur entrée
  ci-dessous dit « non essayé » ou « non testé sur la tablette » pour : les popups Musique,
  Suivi, Frigos, Serveur IA, Énergie en pages, Météo, Réveil, Lumières et Volets en pages,
  Température avec l'humidité, Réglages en pages et leurs nouveaux réglages ; plusieurs
  télécommandes ; les caméras par pièce et la mosaïque ; la zone à gauche de l'horloge ; la roue
  à deux anneaux, la roue de navigation, la roue et le carrousel des clims ; le climat des
  pièces ; le panneau Ok Nabu à lignes ; les gestes de l'horloge ; les repères fins de la
  chronologie du démarrage et le chargeur éteint au démarrage.
- **Mémoire** : la RAM statique passe de 192 602 o (3.7.0) à 239 834 o (+47 232 o, 53,8 %) ; ce
  qu'il reste de RAM interne libre sur la tablette n'a pas été remesuré.
- **Serveur IA** : seul un Ollama 0.30.6 a été lu en vrai, sans modèle chargé ; llama.cpp et
  LM Studio n'ont jamais été interrogés ; aucune action (Décharger, Redémarrer) envoyée à un vrai
  serveur, aucun paquet Wake-on-LAN envoyé ; le popup n'a jamais été ouvert sur la tablette. Les
  seuils du GPU (80 et 90 °C) ne sont pas vérifiés selon les cartes. Ollama ne donne pas de débit
  global en tokens/s, LM Studio ni VRAM ni débit hors d'une réponse : ces valeurs restent à
  « — ».
- **Énergie** : la courbe « ciel clair » apprise et la prévision du jour n'ont jamais été
  confrontées à une vraie installation solaire.
- **Caméras** : un seul essai réel, avec une caméra 640 × 480, avant les pièces et la mosaïque ;
  jamais plusieurs vraies caméras.
- **Musique** : la pochette est téléchargée en http ; https n'est pas pris en charge.
- **Batterie** : la limite de charge n'a pas été essayée avec une batterie (chargeur arrêté et USB
  branché, on ne sait pas encore si la tablette tourne sur l'USB ou sur sa batterie) ; la charge
  « Rapide » n'est pas mesurée ; le gain et la latence du Wi-Fi éco ne sont pas vérifiés. Une
  batterie neuve ou totalement plate posée sur une tablette qui n'en avait pas est vue absente :
  allumer « Tab5 Batterie montée ».
- **Fiabilité** : les compteurs de redémarrages n'ont pas été vérifiés sur un vrai redémarrage.
- Les images de la notice des popups Lumières et Volets montrent encore l'ancien popup.
- Les textes en allemand, néerlandais, espagnol, italien et turc (écran, tableau de bord,
  intégration) sont écrits par une IA, pas encore relus.

### Pré-releases

Tirées de cette version, sur le canal bêta :
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
[v3.8.0-rc.4](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.8.0-rc.4)
le 10/10/2026 : zone à gauche de l'horloge au choix — vocal, graphique des prochaines heures ou
lecteur compact (#431, #432), popup Musique pour tout lecteur multimédia (#430), popups Météo en
graphiques (#425), Réveil (#426), Lumières et Volets (#424) en pages, roue de navigation (#423) et
roue de la clim (#420), carrousel des clims (#414), climat de la pièce (#416), historique avec
l'humidité (#422), gestes de l'accueil au choix (#415), panneau Ok Nabu à lignes (#418), Réglages :
charge, animations, Wi-Fi éco (#421), parseurs testés et fuzzés (#411, #417), corrections de
l'accueil (#419, #428). Fichiers Home Assistant à recopier avant le firmware.

### 2026-10-10 — Popup « Serveur IA » : décharger le modèle, réveiller le PC, redémarrer le service (ADR-0060)

- **Demande d'Axel** (lot 3 de l'audit « Tab5 et LLM locaux » du 10/10) : agir sur le serveur d'IA depuis son popup.
- **Trois boutons** dans le bandeau, à droite du modèle (`serveur_ia_action.yaml`, un `!include` par bouton) : « Décharger » (Ollama, LM Studio ; grisé sans modèle chargé), « Réveiller » (Wake-on-LAN ; grisé quand le serveur répond), « Redémarrer » (le script ou le bouton choisi dans HA). Chacun n'apparaît que réglé dans Home Assistant ; rien de réglé, le popup reste comme avant. Décharger et redémarrer demandent un second tap (« Confirmer ? » en orange, 4 s) ; réveiller part tout de suite ; « Envoyé » en vert 5 s, sans autre envoi.
- **Événements seulement** (ADR-0025) : la tablette émet `esphome.tab5_serveur_ia_action` avec un code (`decharger`, `reveiller`, `redemarrer`), jamais un nom de modèle, une adresse ou une entité. `tab5_evenements.yaml` ne laisse passer que ces trois codes vers `script.tab5_serveur_ia_action`.
- **`packages/tab5_llm.yaml`** : `sensor.tab5_serveur_ia_actions` (ce qui est offert, actif ou grisé, à un seul endroit), une adresse MAC « Tab5 · adresse MAC du serveur IA » (vide par défaut), une liste « Tab5 · serveur IA, redémarrage » (scripts et boutons, sans ceux du projet ni de la tablette, « Aucun » par défaut), `rest_command.tab5_serveur_ia_envoyer` (POST JSON, 10 s), le script (mode single, ne fait que ce qui est offert à cet instant, une action toutes les 5 s au plus, relevé relancé tout de suite après) et la clé `wake_on_lan:` (charge l'intégration Wake on LAN ; une clé déjà dans `configuration.yaml` ne fait pas doublon). llama.cpp : pas de « Décharger » (son `/models/unload` n'existe qu'en mode routeur).
- **Payload `tab5_maj_serveur_ia` : un 14e champ `actions`** (`code` actif, `-code` grisé), ajouté par `packages/tab5_serveur_ia.yaml`. Compatible dans les deux sens : un ancien firmware ignore le champ, un ancien package n'en envoie pas (aucun bouton). Lu par `serveur_ia_lire()` (masques de bits, codes inconnus ignorés), testé sur PC et fuzzé.
- Tableau de bord HA (deux réglages en sept langues), notice et guide `docs/serveur-ia.md` (section « Actions depuis la tablette »), rendu hors tablette (`serveur-ia-actions`, `serveur-ia-confirmer`), démo (actions dans deux scènes), `tests/test_serveur_ia_actions.py` (actions offertes contre un calcul à part, script rejoué : seules quatre actions permises, rien sans action offerte et active, corps JSON, cadence). 4 textes d'écran en six langues, glyphes MDI `eject` et `restart` dans `mdi_font_32`. Contrat 1.2.0 inchangé (déjà mineur depuis v3.8.0-rc.4), un événement de plus.
- **Ordre de mise à jour : le firmware d'abord, puis les packages HA** (`tab5_llm.yaml`, `tab5_serveur_ia.yaml`, `tab5_evenements.yaml`, puis recharger le YAML). Wake on LAN : rien de plus si `wake_on_lan:` est déjà dans `configuration.yaml` ; sinon un redémarrage de HA, ou l'intégration « Tab5 », qui le charge à chaud.
- **Non vérifié** : aucune action envoyée à un vrai serveur (l'Ollama de l'auteur seulement lu), aucun paquet magique envoyé, pas essayé sur la tablette.
### 2026-10-10 — Popup Énergie en pages : flux, soleil et prévision du jour apprise sur la maison, bilan avec revente et gains (ADR-0058)

- **Demande d'Axel**, après la capture du tableau Énergie de HA envoyée par husyildiz ([discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) : un popup plus graphique et plus lisible, la course du soleil, les heures de forte production, la météo des prochaines heures, la consommation face à la production, la revente et les gains.
- **Écran** (`tab5_energie.cpp`, `energie_popup.yaml`, nouveau `energie_flux_noeud.yaml`) : quatre pages par la brique commune (ADR-0046), ouvertes sur la première qui a des données. **Flux** : cercles solaire, maison, réseau, batterie, traits aussi épais que la puissance, anneau de la maison partagé entre soleil et réseau, sans animation. **Aujourd'hui** : arc du soleil du lever au coucher, production heure par heure, prévision en contour, courbe « ciel clair » apprise, bande du meilleur créneau, produit / prévu aujourd'hui et demain. **Production** : la page d'avant, inchangée. **Bilan** : production consommée sur place / vendue, consommation solaire / réseau, taux d'autoconsommation, vente, achat et gains par heure, jour ou mois. Lecture des deux nouveaux payloads dans `tab5_parse.*` (testée, fuzzée) ; 31 textes en six langues ; glyphe `piggy-bank`.
- **Home Assistant** (`packages/tab5_energie.yaml`, blueprint « Tab5 — emplacements ») : lever, midi et coucher par `sun.sun` ; courbe « ciel clair » = pour chaque heure, la 2e meilleure production des 30 derniers jours (statistiques `hour`), qui prend en compte l'orientation et les ombres sans réglage ; prévision = cette courbe × (1 − 0,75 × nébulosité^3,4) avec la météo horaire déjà utilisée (nébulosité, sinon par condition), ou mise à l'échelle d'une prévision externe (Forecast.Solar, Solcast…) ; meilleur créneau de 3 h jusqu'au coucher ; bilan et gains par les compteurs d'achat et de vente et les prix. Nouvelles entrées facultatives : prévision du jour et de demain, énergie achetée et vendue, prix d'achat et de revente (nombre ou capteur), monnaie. Rien n'est poussé popup fermé.
- **Contrat** : deux actions ajoutées, `tab5_maj_energie_soleil` et `tab5_maj_energie_bilan` (version mineure) ; HA les appelle avec `continue_on_error`, un firmware plus ancien garde le popup d'avant. Démo (journée de juin), rendu hors tablette (`energie-flux`, `energie-aujourdhui`, `energie-bilan`), graines du fuzz des services, `tests/test_energie.py` (modèles Jinja contre un calcul Python indépendant).
- Non essayé sur la tablette, ni sur une vraie installation solaire : la justesse de la courbe apprise et de la prévision reste à confirmer avec des données réelles.

### 2026-10-10 — Capteurs suivis : où les choisir, dit dans la notice et dans le tableau de bord

- **Demande d'Axel** : il n'a pas trouvé où choisir son capteur de bourse. La notice « Suivi » dit maintenant le chemin exact (Paramètres → Appareils et services → Entités, « capteurs suivis », entité `select.tab5_capteurs_suivis`, ou la vue Réglages du tableau de bord) et comment cocher (« ○ » devient « ✓ »). Le tableau de bord (`custom_templates/tab5_dashboard.jinja`) ajoute, sous les tuiles de la section Maison, un texte court en sept langues avec un lien vers la notice, quand la liste existe. **Documentation et texte de tableau de bord seulement** : aucun changement de firmware ni de contrat ; le tableau de bord déjà posé dans HA ne le montre qu'après régénération.

### 2026-10-10 — Repères plus fins de la chronologie du démarrage

- **Demande d'Axel**, après la première mesure sur la tablette (3 démarrages : `expandeur` à 7,2 s, `retro` à 21,4 s) : découper les ~7 s d'avant l'expandeur et les ~14 s d'après. 25 étapes au lieu de 7, en deux textes de moins de 255 caractères (limite d'un état de HA) : **Tab5 Chronologie du démarrage** jusqu'à la fin de `setup()` (`ctor`, `objets`, `setup`, `bus`, `expandeur`, `avant1s`, `apres1s`, `p600`, `donnees`, `lvgl`, `wifiinit`, `reseau`, `ecoute`, `fin`, `i18n`, `zones`) et la nouvelle **Tab5 Chronologie du démarrage (suite)** (`ordo`, `retro`, `dessin`, `image`, `tard`, `wifi`, `api`, `fin600`, `chargeur`).
- **Horloge** : `esp_timer` (depuis l'initialisation d'ESP-IDF) au lieu de `millis()`, qui compte les ticks de FreeRTOS depuis le lancement de l'ordonnanceur ; `ordo` donne l'écart entre les deux (une valeur de la première version = valeur − `ordo`).
- **Séquence `on_boot`** (accord d'Axel du 10/10/2026, marques seulement) : marques avant et après la lambda du `delay(1000)`, laissée telle quelle ; au début et à la fin de l'`on_boot` 600 ; au début, après la langue, après les zones et à la fin de l'`on_boot` -100 ; et, en fin de liste, sept entrées qui ne font que marquer, à des priorités libres (2000, 801, 500, 399, 249, 199, 99), pour borner les bandes de priorité de `setup()`. Les priorités et l'ordre des trois entrées d'origine ne changent pas (`tests/test_chronologie_demarrage.py`). Autres marques : un constructeur C++ global (`ctor`) et un style posé sur aucun widget, créé avant eux (`objets`, `tab5-styles.yaml`).
- Limite : l'écran, le tactile, les codecs audio et les capteurs ont la même priorité (600) ; sans changer de priorité, `donnees` les mesure ensemble. Lecture de chaque repère : `docs/performance.md`, « Chronologie du démarrage ». Tableau de bord de HA : une carte de plus (`custom_templates/tab5_dashboard.jinja` à recopier, sept langues). **Non testé sur la tablette.**

### 2026-10-10 — Chargeur éteint au démarrage quand le démarrage précédent n'a vu aucune batterie

- **Demande d'Axel** : sans batterie, les 30 s de chargeur allumé au démarrage (réveil d'une batterie en protection, comme M5Unified) chargeaient dans le vide (8,39 V lus) et ajoutaient un pic de consommation au pire moment. `charge_enable` passe de `ALWAYS_ON` à `ALWAYS_OFF` ; le premier pas de l'interval du chargeur (fin de `setup()`) décide d'après la mémoire du démarrage précédent, gardée en NVS (globals `tab5_batterie_memoire` : 0 inconnue, 1 aucune, 2 vue ; écrite seulement quand elle change). **Aucune** : chargeur laissé coupé, sonde tout de suite. **Vue** ou **inconnue** (premier démarrage après la mise à jour) : réveil de 30 s puis sonde, comme avant, mais à partir de la fin de `setup()` au lieu du début (~7 s plus tard).
- **Risque documenté** : une batterie neuve ou totalement plate posée sur une tablette qui n'en avait pas lit moins de 3,0 V et est vue absente. Allumer **Tab5 Batterie montée** la réveille désormais **tout de suite** (30 s), puis une fois par heure tant qu'elle n'est pas vue (avant : la première heure d'attente).
- Logique pure dans `Tab5/socle/tab5_batterie.h` (`MemoireBatterie`, `chargeur_demarrer()`, `batterie_memoire_suivante()`), cas dans `tools/test_alarm_clock.cpp` ; `tests/test_batterie.py` relit le câblage. Chronologie du démarrage : étape `chargeur` (premier allumage ; « - » sans batterie). Docs : `docs/hardware.md` (Battery / Batterie), `docs/troubleshooting.md`. Contrat HA ↔ firmware inchangé. **Non testé sur la tablette.**

### 2026-10-10 — Chronologie du démarrage dans Home Assistant

- **Demande d'Axel** : la tablette, sur chargeur secteur, n'a pas de journal série ; on ne savait ni ce qui occupe les ~5 s de `setup()` ni quand le rétroéclairage s'allume. Nouvelle entité de diagnostic **Tab5 Chronologie du démarrage** : `expandeur=…; retro=…; dessin=…; image=…; wifi=…; api=…`, en ms depuis le lancement du minuteur d'ESP-IDF (`Tab5/socle/tab5_demarrage.h`, testé sur PC par `tools/test_tab5_socle.cpp`). Marques posées sur l'alimentation USB (setup des composants matériels, avant l'attente de 1 s), la première écriture du rétroéclairage, le premier dessin LVGL et la première image, le Wi-Fi et l'API ; publiée à partir de la première image, jamais pendant `setup()`. **Aucun changement de comportement** : la séquence `on_boot` (ADR-0005) n'est pas touchée. Lecture : `docs/performance.md`, « Chronologie du démarrage ». Tableau de bord de HA : une carte de plus sous « État » (page Santé, `custom_templates/tab5_dashboard.jinja` à recopier, sept langues). **Non testé sur la tablette.**

### 2026-10-10 — Popup « Serveur IA » : le tableau de bord d'un serveur de LLM local (ADR-0059)

- **Demande d'Axel** (lot 2 de l'audit « Tab5 et LLM locaux » du 10/10, audits 2 et 4) : une vue en direct du serveur d'IA de la maison sur la tablette, plus rapide que la tendance du popup Suivi.
- **Nouveau popup « Serveur IA »** (`tab5_serveur_ia.cpp`, `serveur_ia_popup.yaml`, `serveur_ia_carte.yaml`, chrome partagé) : un bandeau (pastille en ligne / hors ligne, état, nom du serveur, modèle chargé coupé par « … »), la carte des **tokens par seconde** (grand chiffre, courbe lissée des 24 dernières poussées gardée par la tablette, requêtes en cours et en file) et quatre cartes : **VRAM** utilisée / totale avec une barre, **température du GPU** colorée selon le niveau décidé par Home Assistant, **RAM** en % avec une barre, **puissance** en W. Une valeur inconnue s'écrit « — », jamais un 0 inventé. Rien n'est repeint tant que le popup est fermé. Pas encore de boutons (lot 3 : leur place est gardée dans le bandeau).
- Ouvert par « Aller à l'écran → Serveur IA » (option ajoutée à la fin), le geste `serveur_ia` (blueprint, fin de la liste) et la roue de navigation (Agenda ▸ Serveur IA), **seulement une fois qu'un serveur a été poussé**.
- **Nouvelle action `tab5_maj_serveur_ia`** (`payload` : « nom|etat|modele|tps|cours|file|vram|vram_total|vram_pct|temp|niveau|ram|puissance »), lue par `serveur_ia_lire()` (`tab5_parse`, section 14), testée sur PC (`tools/test_parse.cpp`) et fuzzée (`fuzz_parse.cpp`, `fuzz_services.py`).
- **Nouveau package `packages/tab5_serveur_ia.yaml`** (rien en dur, ADR-0024) : dix listes « Tab5 · serveur IA, … » (« Aucun » par défaut, chaque liste ne propose que les capteurs qui conviennent ; capteurs de `tab5_llm.yaml`, Glances, System Monitor, une prise mesurée…), le payload assemblé par Home Assistant (unités converties en Go base 1024, °C, W ; arrondis contre le bruit ; seuils du GPU 80 et 90 °C à un seul endroit, **non vérifiés par GPU**), poussé à chaque changement avec **deux secondes au moins** entre deux poussées, et tout de suite à la connexion, à « MAJ Écran », au changement d'une liste et au démarrage de HA. Tableau de bord HA du Tab5 : les dix listes dans « Maison » (sept langues).
- Démo (16 services, un serveur par scène), rendu hors tablette (`serveur-ia`, `serveur-ia-hors-ligne`, `serveur-ia-vide`), notice `docs/notice/ai-server.md`, 15 textes d'écran en six langues, glyphes MDI `server`, `speedometer`, `memory`, `expansion-card` et « — » dans `roboto_55_b`. Contrat 1.2.0 (mineure, déjà comptée). **Ordre de mise à jour : le firmware d'abord**, puis `packages/tab5_serveur_ia.yaml` et le blueprint. Non essayé sur la tablette.

### 2026-10-10 — Popup Caméras : la colonne des pièces suit le changement de thème

- **Correctif** des lots 1 et 2 (ADR-0057) : popup ouvert, un changement de thème laissait la puce de la pièce montrée et le texte des puces dans les couleurs de l'ancien thème (`choix_peindre()` les écrit en style local). `cameras_rejouer_theme()`, appelée par `theme_rejouer_ui()`, les repeint. Vu par le rendu « clair » (bascule à chaud ≠ démarrage à froid sur `cameras` et `cameras-piece-hors-ligne`). **Non testé sur la tablette.**

### 2026-10-10 — Serveur IA local : capteurs Ollama, llama.cpp et LM Studio dans Home Assistant, sans firmware

- **Demande d'Axel** (lot 0 de l'audit « Tab5 et LLM locaux » du 10/10) : superviser un serveur d'inférence local depuis Home Assistant et mettre ses chiffres dans le popup Suivi existant (ADR-0054), sans toucher au firmware.
- **Nouveau package `packages/tab5_llm.yaml`** (rien en dur, ADR-0024) : « Tab5 · type de serveur IA · AI server type » (Aucun par défaut / Ollama / llama.cpp / LM Studio) et « Tab5 · adresse du serveur IA · AI server address » ; avec « Aucun » ou sans adresse, **aucune requête**. Un capteur à déclencheurs (`sensor.tab5_serveur_ia_releve`) interroge le serveur toutes les 30 s par `rest_command.tab5_serveur_ia` (GET, 5 s au plus) : Ollama `/api/ps` ; llama.cpp `/health`, `/props`, `/metrics` (avec `--metrics`), `/slots` ; LM Studio `/api/v1/models` (0.4 et plus). Capteurs : en ligne, modèle(s) chargé(s), et trois mesures à `state_class: measurement` proposées dans « Tab5 · capteurs suivis » — VRAM utilisée (Ollama), vitesse de génération en tokens/s (llama.cpp, écart des compteurs entre deux relevés), requêtes en cours (llama.cpp). Ce qu'un serveur ne donne pas reste indisponible : **Ollama n'a pas de compteur global de tokens/s**, LM Studio ni VRAM ni débit hors d'une réponse. Aucun capteur matériel : température et VRAM du GPU, RAM, puissance viennent de Glances ou de System Monitor.
- Tableau de bord HA du Tab5 : les deux réglages dans « Maison » (sept langues). Nouvelle page `docs/serveur-ia.md` (EN/FR, menu « Utiliser la tablette »), dont la limite du Suivi (une poussée toutes les cinq minutes au plus, une tendance). `HomeAssistant_Config/README.md`, `docs/README.md`, cartographie.
- **Tests** : `tests/test_serveur_ia.py` rend les vrais modèles Jinja du package sur des réponses simulées des trois serveurs (formes des docs officielles) et les compare à un calcul Python indépendant ; mêmes modèles rendus par le moteur de modèles du HA d'Axel (lecture seule). **Seul Ollama a été lu en vrai** (`/api/ps` d'un Ollama 0.30.6, aucun modèle chargé) ; **llama.cpp et LM Studio non testés contre un vrai serveur.** Contrat HA ↔ firmware inchangé. Rien à faire sur la tablette ; package à recopier dans Home Assistant.

### 2026-10-10 — Popup Caméras : la mosaïque (ADR-0057, lot 2)

- **Suite de la demande d'Axel** (plusieurs caméras dans plusieurs pièces). **Non testé sur la tablette ni avec de vraies caméras.**
- **Mosaïque** dès deux caméras dans la pièce choisie (ou « Toutes ») : le cadre en quatre cases de 476 × 266 (`cameras_vignette.yaml` ×4), quatre caméras par page (deux au milieu, trois avec la troisième centrée en bas, ou 2 × 2) ; au-delà, le glissé change de page et les pastilles comptent les pages. Chaque case : le nom de la caméra dans une pastille de verre, sa petite image, « Chargement... » ou « Hors ligne depuis … » (image atténuée). **Tap sur une case** : la caméra en grand (la vue du lot 1) ; **tap sur l'image** : retour à la mosaïque, sur la page de cette caméra. La vue est gardée en NVS avec la pièce et la caméra.
- **Vignettes demandées petites** (`width=480&height=270`) et montrées « couvrantes » : recadrées sans transformation quand elles tiennent presque (une caméra 1080p arrive à 480 × 270), réduites juste assez sinon.
- **Toujours une image à la fois** : en mosaïque, les cases de la page à tour de rôle (d'abord celles sans image), chacune toutes les 10 s ; rien pour les autres pages. Chaque caméra garde sa dernière grande image et sa dernière vignette dans le même budget de 8 Mio. Le chargeur ne réutilise plus un tampon deux fois trop grand (une grande image rendue ne porte plus une vignette).
- Deux textes d'écran en sept langues (« Toutes les caméras », « Touchez une image pour l'agrandir »). Test `tests/test_cameras_popup.py` : autant de puces, de cases et de pastilles dans le YAML que de constantes dans le C++. Rendu : `cameras` (mosaïque), `cameras-mosaique-page-2`, `cameras-plein-ecran`, `cameras-piece-hors-ligne`. Docs : ADR-0057 (lot 2), notice Caméras, `docs/screens.md`, cartographie.

### 2026-10-10 — Popup Caméras : plusieurs caméras dans plusieurs pièces (ADR-0057, lot 1)

- **Demande d'Axel** (« fais comme tu le vois ») : gérer plusieurs caméras dans plusieurs pièces, intuitif, complet, propre, beau et léger. **Non testé sur la tablette ni avec plusieurs vraies caméras.**
- **La pièce vient de Home Assistant, sans réglage** : le blueprint « Tab5 — emplacements » ajoute à chaque caméra sa pièce (`area_name` : celle de l'entité, sinon de son appareil) et, si elle est `unavailable`, l'heure depuis laquelle (`last_changed`). Format « nom|image|pièce|depuis », **16 caméras** au plus (8 avant). Même action, mêmes variables : `contrat/contrat.yaml` inchangé (1.2.0, pas encore publiée). Rétrocompatible dans les deux sens : un firmware d'avant lit « nom|image » et ignore la suite ; ce firmware lit « nom|image » d'un blueprint d'avant comme des caméras sans pièce.
- **Colonne des pièces** (à partir de deux pièces ; `cameras_puce.yaml` ×17) : « Toutes », une puce par pièce avec son nombre de caméras, « Autres » pour les caméras sans pièce ; défilante au-delà de huit. Le cadre de l'image passe contre le bord droit pour lui laisser 238 px (un nom de pièce entier). Avec une seule pièce : pas de colonne, image centrée comme avant. Le glissé et les pastilles ne parcourent que la pièce choisie (brique des popups à pages, ADR-0046). Sous l'image, « Pièce · Caméra ». Pièce et caméra **gardées en NVS** (par une empreinte de leur nom).
- **La dernière image de chaque caméra est gardée** tant que le popup est ouvert (8 Mio au plus, jamais sous 6 Mio de PSRAM libre ; la plus anciennement vue part d'abord) : revenir sur une caméra la montre tout de suite. Le chargeur **donne** son tampon décodé à l'écran au lieu d'échanger deux tampons. Les voisines de la caméra montrée sont chargées une fois entre deux images : un glissé tombe sur une image.
- **Hors ligne** : une caméra que HA dit indisponible n'est pas demandée ; « Hors ligne depuis 14:32 » (ou « … depuis Lun 14:32 », « … depuis le 3 Oct ») au milieu du cadre, sur sa dernière image atténuée. Deux échecs de suite disent la même chose. Chaque caméra a son propre compte d'échecs et son attente (10 s, 30 s, 60 s) : une caméra en panne ne retient plus le tour des autres.
- **Lecture du payload** en C++ pur (`cameras_lire()` : rang de la pièce de chaque caméra, pièce vide en dernier ; `tools/test_parse.cpp`, invariants dans le fuzzeur `tools/fuzz/fuzz_parse.cpp`) ; réponse du blueprint rendue par `tests/test_cameras_blueprint.py` (pièces, hors ligne, séparateurs, seize au plus, lisible par un firmware d'avant). Hors tablette, le bouchon du chargeur rend une mire calculée (une teinte par caméra) : le rendu montre la mise en page avec des images (`cameras`, `cameras-piece-hors-ligne`).
- Cinq textes d'écran en sept langues (« Toutes », « Autres », trois « Hors ligne depuis … »). Docs : ADR-0057 (et une note dans l'ADR-0049), notice Caméras (EN/FR), `docs/screens.md`, `docs/architecture.md`, `Tab5/README.md` (table des services), `HomeAssistant_Config/README.md`, cartographie. **Blueprint à recopier** dans Home Assistant pour les pièces ; le firmware marche aussi avec l'ancien.

### 2026-10-10 — Popup télécommande : plusieurs télécommandes (TV, Apple TV, Freebox Player), une page chacune (ADR-0056)

- **Demande d'Axel** : la télécommande à l'écran pour la TV Samsung, l'Apple TV et le Freebox Player, dans le même popup, plus belle et au moins aussi pratique.
- **Côté Home Assistant** (aucune entité dans le firmware, ADR-0025) : nouvelle entrée « Autres télécommandes · Other remotes » du blueprint « Tab5 — emplacements » (`telecommandes_autres`, plusieurs `remote`) ; la liste = celle de la TV puis les autres, sans doublon, quatre au plus, poussée avec tous les états (clé `telecommandes|écran|nom|…` de `tab5_maj_emplacements` : `boitier` pour l'intégration Apple TV, `tv` sinon ; nom de l'appareil sans la pièce ni la parenthèse finale). Les branches « Télécommande : … » envoient chaque touche à la télécommande de la page (`tv`, `tv1`, `tv2`, `tv3`) et la traduisent pour un boîtier (`telecommande_apple_tv` : `select`, `menu`, `top_menu`, `play_pause`, `skip_backward`… ; ni muet ni source chez pyatv) ; Marche / Arrêt d'un boîtier = les commandes `turn_on` / `turn_off` selon son lecteur (`remote.turn_on` d'Apple TV ne fait que connecter HA). L'assistant de l'intégration propose les autres télécommandes quand il a trouvé celle de la TV et qu'il y en a trois au plus.
- **Écran** (`tv_remote_popup.yaml` refait, `tv_touche.yaml`, `Tab5/ecran/tab5_telecommande.cpp`, `Tab5/paquets/tab5-telecommande.yaml`) : les noms des télécommandes en haut et le glissement gauche / droite (brique commune, ADR-0046 ; rien avec une seule) ; zone principale au pixel (Marche / Arrêt, Source ou Stop, Menu ; pavé en disque de 400 px avec OK ; Lecture, Pause, Retour, Accueil à droite du pavé ; volume) et rangée basse : applications pour la TV, rangée de lecture pour un boîtier (Précédent, Reculer, Lecture / Pause, Avancer, Suivant). Touches au moins aussi grandes qu'avant (Muet 150 × 100 au lieu de 60 px de haut, lecture 258 × 100). L'écran TV est proposé aussi sans la TV du blueprint quand HA a poussé des télécommandes.
- **Rétrocompatible** : la première télécommande garde l'emplacement `tv` ; un blueprint plus ancien (sans la clé) garde la page unique d'avant, un firmware plus ancien ignore la clé et commande la TV. Contrat inchangé (1.2.0) : aucune action ni variable ajoutée.
- 6 textes d'écran en six langues ; glyphes MDI `rewind`, `fast-forward`, `play-pause` et `volume-high` (`mdi_font_45`) ; `tests/test_telecommandes.py` (codes, emplacements, traductions, clé rendue et aiguillage par les vrais modèles du blueprint) ; `telecommandes_lire()` testée (`tools/test_parse.cpp`) et fuzzée. Non essayé sur la tablette ni sur les vrais appareils.

### 2026-10-10 — Tuiles météo en cellule pleine et fond de l'horloge au choix

- **Demande d'Axel** (essai validé sur la tablette) : les tuiles de prévisions prennent la même cellule que les tuiles des pièces — une carte pleine de 230 × 275 px, sans les deux onglets en saillie du jour et de la température ; le nom du jour et les températures s'écrivent directement sur la carte (`forecast_day_body.yaml` pleine hauteur, `forecast_day_title_tab.yaml` et `forecast_day_temp_tab.yaml` sans fond ni relief). L'icône météo, les épaules et les appuis ne bougent pas.
- **Fond de la tuile de l'horloge au choix** : interrupteur « Tab5 Fond de l'horloge » (`tab5-ha-controls.yaml`, éteint par défaut, gardé en mémoire, appliqué sans redémarrage par `horloge_fond_ui()`), réglé depuis Réglages ▸ Apparence (Oui / Non à droite de « Clair ou sombre », qui passe en demi-largeur, `REGLAGE_HORLOGE_FOND`). Sans fond, la tuile garde sa bordure.
- Une entrée de plus dans le tableau de bord HA (`tab5_dashboard.jinja`, traduite) ; le texte « Fond de l'horloge » en six langues. Aucun changement du contrat HA ↔ firmware.

### 2026-10-10 — Première mesure de consommation du Tab5 : page « Consommation »

- **Demande d'Axel** : mettre en doc les chiffres du test de husyildiz (rc.4, discussion #278) avec une analyse, la consommation annuelle et ce qu'on peut en tirer.
- Nouvelle page `docs/consumption.md` (FR/EN, menu « Comprendre le projet ») : 2,63 W à 100 %, 1,74 W à 50 %, 1,23 W écran éteint ; le thème (0,02 W) et Okay Nabu / haut-parleur (0,05 W chacun) ne changent rien ; l'économie d'énergie revient à son plafond de 50 % ; kWh par an ; pistes (extinction auto sur batterie, mesure à la prise). Le « 23,7 Wh » et les « 9 h » du script de test sont signalés comme trop hauts (capacité nominale 14,8 Wh). Aucune autre tablette n'est chiffrée : non mesuré, méthode donnée.
- `docs/hardware.md` et `docs/performance.md` ne disent plus « jamais mesurée ». Documentation seule.

### 2026-10-10 — Réfrigérateurs et congélateurs : normes, porte mal fermée, popup « Froid » et icône qui clignote sur l'horloge (ADR-0055)

- **Demande d'Axel** : suivre le frigo et le congélateur — les déclarer dans les réglages, contrôler la température par rapport aux normes, repérer une montée (porte mal fermée), voir les 24 dernières heures, les alertes habituelles de la carte centrale et, pour un problème grave seulement, une icône qui clignote dans le coin de l'horloge ; un popup de gestion avec la courbe et les incidents.
- **Côté Home Assistant** (rien en dur, ADR-0024) : deux listes à choix multiples « Tab5 · réfrigérateurs · fridges » et « Tab5 · congélateurs · freezers » (`packages/tab5_reglages.yaml` : capteurs de température à statistiques, quatre appareils au plus en tout, un capteur choisi dans une liste absent de l'autre) ; nouveau package `packages/tab5_froid.yaml` : `script.tab5_froid_calculer` décide du niveau (0 conforme, 1 attention, 2 grave), de la cause et de sa durée, avec **les seuils à un seul endroit** (variable `seuils`, commentée) — réfrigérateur 0 à 5 °C, attention si la moyenne de 15 min sort de la norme, grave au-dessus de 8 °C pendant 30 min ou à 10 °C ; congélateur -18 °C au plus, attention au-dessus de -15 °C, grave au-dessus de -12 °C pendant 30 min ou à -10 °C ; montée de 2 / 4 °C depuis le plus bas des 30 dernières minutes = « porte ouverte ? », encore là après 20 min sans redescendre = « porte mal fermée » ; 30 min sans mesure = « capteur muet ». État gardé dans `sensor.tab5_froid` (modèle à déclencheur, restauré, mis à jour par l'événement `tab5_froid_bilan`), dernier incident terminé compris. Calcul au changement d'un capteur (une minute au plus entre deux), toutes les cinq minutes, au changement d'une liste, à la connexion, à « MAJ Écran » et au démarrage de HA.
- **Alertes** : source « froid » dans `custom_templates/tab5_alertes.jinja` (un id par appareil, Orange en attention, Rouge au niveau grave, libellé codé `@froid:` composé par la tablette dans sa langue : « Frigo cuisine : porte mal fermée, 9.1 °C »), abonnement « Tab5 · alertes : réfrigérateurs et congélateurs » (`packages/tab5_alerts.yaml`, tableau de bord de la tablette).
- **Icône `fridge-alert`** dans le coin haut droit de l'horloge (`btn_froid_alerte`, rôle `ERROR`), seulement au niveau grave : clignote sans fondu (un `lv_timer` de 500 ms créé une fois, en pause quand elle est cachée, allumée fixe pendant que le popup est ouvert) ; un tap ouvre le popup.
- **Popup « Froid »** titré « Frigos et congélateurs » (`froid_popup.yaml`, `froid_carte.yaml` ×4, `Tab5/ecran/tab5_froid.cpp`) : chrome partagé (ADR-0009), une ligne du registre (ADR-0013, 26 fenêtres sur 32), `Ecran::FROID` (option « Froid » à la fin de « Aller à l'écran », code de geste `froid` à la fin de `kCodesGestes`, index 27, proposé par le blueprint), et **« Frigos » au bout de la famille Agenda de la roue de navigation** (Appareils et le premier anneau sont pleins ; proposé sauf si HA dit qu'aucun appareil n'est déclaré). Une carte par appareil : icône réfrigérateur ou flocon, nom, température et statut colorés par niveau, extrêmes des 24 h et norme, courbe des 24 h entre les pointillés de la norme, dernier incident. Titre et mot de la roue : « Frigos », pas « Froid », dont la traduction est déjà « Cool » (mode d'une clim).
- **Contrat** : nouvelle action `tab5_maj_froid` (`payload` : « nom|type|valeur|niveau|cause|depuis|min|max|bas|haut|points|icause|idebut|iduree|imax;… »), lue en C++ pur par `froid_lire()` (`tab5_parse.*`, section 12 ; cas dans `tools/test_parse.cpp`, fuzzée) ; `contrat/contrat.yaml` reste en 1.2.0 (déjà une mineure depuis la dernière version publiée, `contrat_api.py --semver` : « version suffisante »). **Ordre de mise à jour : le firmware d'abord**, puis `packages/tab5_reglages.yaml`, `packages/tab5_froid.yaml`, `packages/tab5_alerts.yaml`, `custom_templates/tab5_alertes.jinja`, `custom_templates/tab5_dashboard.jinja` et le blueprint « Tab5 — emplacements » (geste « Froid »). Fichiers HA avant le firmware : la poussée échoue sans rien arrêter, et le libellé des alertes s'affiche brut (`@froid:…`) jusqu'au firmware.
- 37 textes d'écran en six langues ; glyphes MDI `fridge` et `fridge-alert` ; `tests/test_froid.py` rend les vrais modèles Jinja du package appel après appel et les compare à un calcul Python indépendant (seuils, porte, capteur muet, incident, °F) ; rendu hors tablette : `froid-vide`, `froid`, `accueil-froid` (non comparé : l'icône clignote). Non essayé sur la tablette.

### 2026-10-10 — Capteurs suivis : la courbe d'un capteur à gauche de l'horloge et un popup « Suivi » (ADR-0054)

- **Demande d'Axel** : le graphique d'un capteur numérique choisi dans Home Assistant à la place du vocal, du graphique météo ou du lecteur, et un popup plein écran avec tous les capteurs qu'il suit. Générique : n'importe quel capteur à statistiques (`state_class: measurement`) — un cours de bourse, une température, une puissance… — ; ni le code ni l'écran ne parlent de bourse.
- **Quatrième contenu de la zone à gauche de l'horloge** (`ZoneGauche::CAPTEUR`, code `capteur`, à la fin : la NVS garde l'index) : carte de l'emprise du graphique (`zone_suivi`, `Tab5/ui_components/suivi_zone.yaml`) avec le nom, la variation en haut à droite (flèche de tendance, vert à la hausse, rouge à la baisse, gris stable : rôles `SUCCESS`, `ERROR`, `TEXT_DIM` de la palette), la valeur dans la police de la date avec son unité, et la courbe des 24 dernières heures sur le dégradé du graphique (même tampon PSRAM, partagé : `zone_degrade_peindre()`, le graphique et la carte ne sont jamais montrés ensemble). Le **premier capteur de la liste** est celui de la zone. Sauté au tap et au départ tant que Home Assistant dit qu'aucun capteur n'est suivi. Un tap ouvre le popup.
- **Popup « Suivi »** (`suivi_popup.yaml`, `suivi_carte.yaml` ×6, `Tab5/ecran/tab5_suivi.cpp`) : chrome partagé (ADR-0009), une ligne du registre (ADR-0013), `Ecran::SUIVI` (option « Suivi » à la fin de « Aller à l'écran », code de geste `suivi` à la fin de `kCodesGestes`, index 26, icône `chart-line`). Une carte par capteur (six au plus) : nom, valeur et unité, flèche et variation colorées, période (« Aujourd'hui » pour la variation du jour en %, « 24 h » pour un écart dans l'unité), courbe lissée (`ui_courbe_lisse()`) et son dernier point. Une rangée jusqu'à trois capteurs, deux au-delà, la dernière centrée.
- **Côté Home Assistant** (rien en dur, ADR-0024) : liste à choix multiples « Tab5 · capteurs suivis · tracked sensors » (`packages/tab5_reglages.yaml`, six au plus, capteurs `measurement` seulement) ; nouveau package `packages/tab5_suivi.yaml` : `script.tab5_suivi_pousser` lit `recorder.get_statistics` (moyennes horaires des 24 dernières heures, en UTC) et pousse ; une poussée au changement d'un capteur suivi, puis cinq minutes au plus entre deux ; tout de suite quand la liste change, à la connexion, à « MAJ Écran », au démarrage de HA et toutes les heures. La variation est le `change_pct` de l'entité quand elle en a un (variation du jour), sinon l'écart depuis la première heure de la courbe. Modèle vérifié en lecture sur le HA d'Axel (rendu du modèle de la poussée, sans appel d'action). Le tableau de bord HA du Tab5 montre la nouvelle liste.
- **Contrat** : nouvelle action `tab5_maj_suivi` (`payload` : « nom|valeur|unité|variation|genre|points;… ») ; `contrat/contrat.yaml` passe en 1.2.0. **Ordre de mise à jour : le firmware d'abord**, puis `packages/tab5_reglages.yaml`, `packages/tab5_suivi.yaml` et le blueprint « Tab5 — emplacements » (option « Capteur suivi en courbe » de la zone gauche, geste « Suivi »). Fichiers HA neufs avec un firmware d'avant : la poussée échoue sans arrêter le reste (`continue_on_error`) ; firmware neuf avec des fichiers HA d'avant : le popup et la carte restent sur « En attente de Home Assistant ».
- **Lecture du payload** en C++ pur testé sur PC (`suivis_lire()`, `Tab5/socle/tab5_parse.*`, section 11 ; cas dans `tools/test_parse.cpp`, appelée par le fuzzeur `tools/fuzz/fuzz_parse.cpp`) ; graine pour `tools/sanitizers/fuzz_services.py`. Quatre textes d'écran en six langues. Glyphes MDI `chart-line` (polices 26, 32, 70) et `trending-up` / `-down` / `-neutral` (32).
- Docs : ADR-0054, notice « Suivi » (`docs/notice/tracking.md`, EN/FR, dans le menu du site), notice de l'accueil, `docs/screens.md`, `docs/architecture.md`, `Tab5/README.md`, `HomeAssistant_Config/README.md`, cartographie. Rendu hors tablette : quatre captures (`suivi-vide`, `suivi`, `accueil-zone-capteur`, `-gelule`). Tests : `tests/test_suivi.py` (nouveau : modèles Jinja de la poussée rendus et comparés à un calcul indépendant), `test_zone_gauche.py`, `test_appuis.py`, `test_gestes.py`, `test_maison.py`, `test_emplacements.py`. **Non essayé sur la tablette.**

### 2026-10-10 — Popup Caméras : les images arrivent en arrière-plan, l'écran ne gèle plus (ADR-0049)

- Chaque image du popup Caméras figeait l'écran et le tactile ~1,6 s (après #439), parce que l'`online_image` d'ESPHome télécharge et décode dans la boucle principale. L'image est maintenant téléchargée et décodée **hors de la boucle** (`Tab5/ecran/tab5_cameras_charge.cpp`, première tâche FreeRTOS du firmware) : `esp_http_client` avec une connexion gardée d'une image à l'autre (plus de poignée de main TLS à chaque image), **décodeur JPEG matériel** de l'ESP32-P4 en RGB565, deux tampons en PSRAM échangés quand l'image est prête. La boucle ne fait plus que sonder (100 ms) et montrer l'image. Une caméra hors ligne ne gèle plus rien ; après un échec, la dernière image reste avec « Plus d'image depuis … ». Mémoire : deux images au lieu d'une pendant que le popup est ouvert (2 × 600 Ko de PSRAM pour une caméra 640 × 480), tout rendu à la fermeture ; 12 Ko de RAM interne pour la pile de la tâche.
- Mesuré sur la tablette le 10/10/2026 avec la même caméra 640 × 480 (12 images de 37 à 39 Ko) : HA **~0,4 s** par image avec la connexion gardée (0,6 s pour la première), décodage matériel **3 à 12 ms** (1,0 s avant), plus aucun « took a long time » de la caméra dans la boucle (4 par minute avant), pire tour de boucle de la minute **217 ms au lieu de 1 020 ms**, une image toutes les 5,4 s au lieu de 6,6 s. Pile de la tâche : 9 860 octets jamais utilisés sur 12 288. Popup fermé 6 fois pendant un chargement, puis Home Assistant injoignable (`ESP_ERR_HTTP_CONNECT`, nouvel essai 10 s après) : ni plantage ni redémarrage, RAM interne revenue au même niveau après chaque fermeture.

### 2026-10-10 — Popup Caméras : l'image n'est plus agrandie (ADR-0049)

- Premier essai réel avec une caméra 640 × 480 (M5Stack AtomS3R-CAM) : chaque image figeait l'écran ~2 s, dont 1,45 s de décodage, parce que `resize: 960x540` faisait agrandir l'image à 720 × 540 par ESPHome, pixel par pixel (même durée pour un JPEG de 11 et de 45 Ko). `cameras_image` n'a plus de `resize:` : l'image est décodée à sa taille, montrée telle quelle si elle tient dans le cadre (une caméra 640 × 480 apparaît donc un peu plus petite), réduite à l'affichage par LVGL (`lv_image_set_scale()`) si elle le dépasse (HA envoie 1152 × 648 pour une caméra 2304 × 1296). Mesuré sur la tablette avec la même caméra (7 images de 36 Ko) : décodage **1,0 s au lieu de 1,45 s**, écran figé **~1,6 s par image au lieu de ~2 s** (l'attente de HA, ~0,6 s, ne change pas), une image toutes les 6,6 s au lieu de 7 s. Le reste du décodage est le coût du dessin pixel par pixel d'ESPHome.

### 2026-10-10 — Intégration HACS : l'assistant propose aussi la maison et les listes « Tab5 · … » (ADR-0053, lot 2)
- **Page « Maison »** : les entrées du blueprint qui ne dépendent pas d'une pièce (TV et sa télécommande, batterie du téléphone, température et humidité du haut de l'accueil, seconde température et « dehors », clim, pots, météo des prévisions, nom ESPHome de la tablette), pré-remplies **seulement quand Home Assistant n'a qu'une réponse** (une seule TV, une seule clim, une seule météo, la seule température « extérieure »…) ; la température du haut de l'accueil est celle de la pièce 1. Une automatisation déjà là : ses valeurs d'abord.
- **Page « Agendas et personnes »** : les listes « Tab5 · … » de `tab5_reglages.yaml`, une liste déroulante chacune. Un choix déjà fait reste ; une liste encore sur « Aucun » reçoit une proposition quand une seule entité convient — mêmes règles que le package pour les quatre qu'il devine, et pour les quatre qu'il ne devine jamais : l'agenda dont le nom dit « travail » ou « rendez-vous », le seul capteur de présence ou d'occupation, le pipeline préféré (marquées « proposé » au récapitulatif). Réglées à la validation par `select.select_option`, comme à la main, puis vérifiées ; la notification dit ce qui est réglé ou pas.
- **Mise à jour d'une automatisation existante** (case cochée) : pièces et entrées de la maison remplacées, un champ vidé garde sa valeur, une case « dehors » décochée est écrite. Les listes se règlent avec ou sans la case.
- **Code** : `assistant.py` (`proposer_maison`, `proposer_liste`, testés sans HA), `assistant_flux.py` (étapes `maison` et `agendas`), `messages.py` ; `manifest.json` : `after_dependencies` + `assist_pipeline`. **CI** : étape 1 bis étendue (température extérieure, agenda local « Travail CI », liste réglée et vérifiée, case décochée). Doc : `installation/home-assistant-files.md`.
- **L'intégration parle les sept langues de l'écran** (demandé par Axel) : allemand, néerlandais, espagnol, italien et turc rejoignent le français et l'anglais — `translations/{de,nl,es,it,tr}.json` (toute l'intégration : installation, options, réparations, assistant), les notifications de `messages.py` et le récapitulatif de l'assistant, choisis d'après la langue du serveur (une autre langue → l'anglais). Traduits avec Claude depuis le français, pas encore relus par un locuteur natif ; les noms des menus de Home Assistant cités dans les textes ne sont pas vérifiés dans chaque langue. `tests/test_integration_tab5.py` vérifie que les sept fichiers ont les mêmes clés, les mêmes paramètres `{…}` et le même code entre accents graves ; `tests/test_assistant_tab5.py`, la même chose pour les textes Python.
- Correctif du lot 1 : deux sauvegardes d'`automations.yaml` dans la même seconde se trient dans l'ordre (la purge des cinq dernières pouvait garder la plus ancienne).

### 2026-10-10 — Intégration HACS : assistant de configuration, les pièces de Home Assistant → l'automatisation du blueprint (ADR-0052, lot 1)

- **Assistant de configuration** (demandé par Axel : un nouvel utilisateur presque tout réglé dès l'installation) : une fois les fichiers en place, et tant qu'aucune automatisation du blueprint « Tab5 — emplacements de l'écran » n'existe, la réparation « Tab5 : configurer la tablette depuis vos pièces » l'ouvre ; il se relance depuis les options de l'intégration (case « Lancer l'assistant de configuration »). Pages : les pièces (jusqu'à cinq pièces de Home Assistant, pré-choisies, les plus pilotables d'abord, les pièces vides écartées), une page par pièce (nom, tuiles — lumières, volets, interrupteurs, ventilateurs, lecteurs, cinq au plus —, capteur de température, d'humidité, clim : les entrées `piece_n_*` de l'ADR-0040, toutes pré-remplies et modifiables), puis un récapitulatif avec des noms, pas des identifiants. Jamais proposées : les entités de la tablette elle-même, désactivées, cachées, de configuration ou de diagnostic. Aucune pièce utilisable : il le dit et ne change rien.
- **Écriture** : seulement à la validation du récapitulatif, dans `automations.yaml` (inclus par `configuration.yaml`, lisible sans étiquette `!secret` / `!include`), sauvegardé avant dans `tab5_sauvegardes/automatisations/` (5 gardées), relu juste avant d'écrire (changé entre-temps : rien n'est écrit), puis `automation.reload` et constat que l'automatisation est chargée ; une notification dit le résultat. **Rien n'est écrasé ni créé en double** : une automatisation du blueprint déjà là ne change que si la case « Remplacer les pièces de l'automatisation existante » est cochée, et alors seulement ses pièces ; plusieurs, ou une hors d'`automations.yaml` : rien n'est écrit. Fichier pas utilisable : le YAML à coller dans une notification. Le Home Assistant d'Axel n'installe pas l'intégration, et son automatisation existe : rien n'y change.
- **Code** : `custom_components/tab5/assistant.py` (pur, testé sans HA par `tests/test_assistant_tab5.py` : propositions, entrées du blueprint, ajout et mise à jour d'`automations.yaml`, sauvegardes, refus) et `assistant_flux.py` (formulaires, partagés par la réparation et les options) ; `manifest.json` : `after_dependencies: [automation]`. Les sauvegardes d'installation ignorent désormais les dossiers non datés de `tab5_sauvegardes/`.
- **CI** (`integration-hacs.yml`, HA 2026.9.4 et 2026.8.3) : étape 1 bis du scénario dans un Home Assistant neuf — deux pièces, des entités de modèle, un thermostat, une lumière cachée ; la réparation suivie comme l'interface, valeurs proposées acceptées → automatisation écrite, chargée, réparation retirée ; relancé depuis les options sans la case → `automations.yaml` à l'octet près ; avec la case → seules les pièces changent, sauvegarde faite.
- Textes en français et en anglais (les cinq autres langues de l'écran : lot 2). Doc : `installation/home-assistant-files.md` (avec HACS), `installation/devices.md`.

### 2026-10-10 — Correctif : la mini-barre « en lecture » se montre à chaque pause

- **Mini-barre masquée une pause sur deux** (#430) : l'instant de la pause était gardé comme `millis() | 1` pour que 0 veuille dire « pas en pause ». Quand la milliseconde était paire, cet instant dépassait `millis()` d'un, et la peinture faite dans la même milliseconde lisait un écart de 2^32 − 1 ms au lieu de 0 : la mini-barre restait cachée et le cadre Ok Nabu en place. Un drapeau séparé (`s_en_pause`) remplace l'astuce (`Tab5/ecran/tab5_lecteur.cpp`). C'est aussi ce qui faisait échouer au hasard l'étape « bascule à chaud = démarrage à froid » du rendu clair (`accueil-musique-clair.png`, l'une des deux séries avec la mini-barre, l'autre sans). Non essayé sur la tablette.

### 2026-10-10 — Roue de navigation : Musique et TV dans « Appareils ▸ » (ADR-0042)

- **Demande d'Axel** (« ajoute musique et tv dans appareils ») : la famille « Appareils ▸ » de la roue de navigation (appui long de la carte centrale) finit par **Musique** (le popup Musique, ADR-0050, icône de son en-tête) et **TV** (la télécommande, icône de son en-tête). La TV n'est proposée qu'avec sa zone ; Musique, comme le lecteur de la zone à gauche de l'horloge, tant que Home Assistant n'a pas dit qu'aucun lecteur n'est choisi (le geste « musique » et « Aller à l'écran » l'ouvrent toujours).
- **Second anneau à huit choix** (`kRoueChoix` = 8, deux boutons `roue_choix` et leurs mots de plus) : au-dessus de la carte centrale, 100 px au moins entre deux choix (28 px d'air), 109,6 px jusqu'au premier anneau, tout dans l'écran ; les huit mots d'« Appareils » dans les sept langues gardent 22 px d'air entre eux et 38 px jusqu'aux autres boutons (`tests/test_roue_navigation.py`). La roue d'une tuile (tuiles, popup Maison, roue d'une clim) reste à six (`kRoueChoixTuile`) : à huit, l'éventail d'une tuile au bord de l'écran ferait chevaucher ses choix (`tests/test_roue.py`).
- Clé d'écran nouvelle « TV » (six langues). Docs : note datée dans l'ADR-0042, notice (accueil, musique, TV, EN/FR), `docs/screens.md`, cartographie. Rendu hors tablette : `roue-navigation-appareils` montre sept choix (Températures, Clims, Lumières, Volets, Plantes, Musique, TV ; pas d'Énergie, absente de cette scène de la démo). **Non testé sur la tablette.**

### 2026-10-10 — Accueil : le lecteur de musique dans la zone à gauche de l'horloge, et un graphique plus lisible (ADR-0051, lot 2)

- **Lecteur compact** (plan validé par Axel : « très beau, léger ») : troisième contenu de la zone à gauche de l'horloge, à la place du vocal ou du graphique, dans une carte de la même emprise (`zone_lecteur`, nouveau composant `Tab5/ui_components/lecteur_zone.yaml`). La pochette (la même image que le popup Musique, sans second téléchargement), le titre, l'artiste, une fine barre de position, précédent / lecture-pause / suivant ; rien en lecture : la phrase du popup (« Rien en lecture », « Lecteur éteint »…) et le nom du lecteur. Un tap hors des boutons ouvre le popup Musique. Mêmes données et mêmes commandes que le lecteur de l'ADR-0050 (`tab5_maj_lecteur`, événement `esphome.tab5_lecteur`) : rien de nouveau côté Home Assistant, aucune entité nommée, contrat inchangé, aucune nouvelle clé de traduction. Pochette aux coins concentriques à ceux de la carte, textes coupés avant le coin arrondi, dans chaque thème ; repeint seulement montré (sinon au retour). Le volume n'y est pas (il ne tiendrait pas proprement ; le popup l'a).
- **Cycle** : « lecteur » entre dans le cycle (vocal → graphique → lecteur) et dans le contenu de départ possible ; il est **sauté quand Home Assistant a dit qu'aucun lecteur n'est choisi** (liste « Tab5 · lecteurs de musique » vide) et revient quand la liste se remplit. Avant toute poussée, il s'affiche (« En attente de Home Assistant »).
- **Mini-barre « en lecture »** (choix d'Axel) : gardée sur le cadre Ok Nabu, masquée tant que la zone gauche montre le lecteur, elle revient quand la zone change.
- **Graphique des 15 heures** (demande d'Axel sur la capture du rendu) : trait de 5 px arrondi, dégradé discret de la couleur de la courbe dessous (une image ARGB8888 calculée à la peinture, tampon de 156 Kio en PSRAM pris une fois ; une seule image en plus), points du plus chaud et du plus froid de 12 px cerclés, leurs valeurs posées à la première place libre (jamais sur la courbe, une barre ou l'autre valeur, ni hors du cadre), barres de pluie visibles dès qu'il pleut (8 px au moins, racine carrée de la quantité), rien sans pluie.
- **Blueprint « Tab5 — emplacements »** : option « Lecteur de musique · Music player » dans la section « Zone à gauche de l'horloge », dans le cycle par défaut. **Blueprint à recopier** dans Home Assistant pour proposer le lecteur ; ordre libre (un firmware d'avant saute le code `lecteur`, un blueprint d'avant ne le propose pas).
- Docs : ADR-0051 (lot 2), note dans l'ADR-0050, notice (accueil, musique, vue d'ensemble, EN/FR), `docs/screens.md`, `docs/architecture.md`, `Tab5/README.md`, cartographie. Rendu hors tablette : trois captures (`accueil-zone-lecteur`, `-vide`, `-gelule`). Tests : `tests/test_zone_gauche.py`, `test_emplacements.py`. Non essayé sur la tablette.

### 2026-10-10 — Popup Musique : un lecteur de musique pour les media_player de la maison (ADR-0050)

- **Demande d'Axel** : « une pop up qui ne prend pas tout l'écran, très belle, pour faire un lecteur de musique genre Spotify, pratique, avec miniature ». Carte centrée de 940 × 536 sur un voile, chrome partagé (ADR-0009) : en-tête avec l'icône, le nom du lecteur et l'application en cours (pastille), pochette de 360 px à gauche (icône de note sans pochette), titre, artiste et album à droite, barre de position glissable avec le temps écoulé et la durée, aléatoire / précédent / lecture-pause / suivant / répétition, volume avec la sourdine, puis une pastille par lecteur en bas pour passer de l'un à l'autre. Un bouton n'apparaît que si le lecteur sait le faire (`supported_features`) ; lecteur éteint : « Lecteur éteint » et un bouton « Allumer » ; aucun lecteur choisi : une phrase qui dit où les choisir. Générique : n'importe quel `media_player` (Apple TV, TV, Freebox, Steam Deck, la tablette elle-même…), pas seulement Spotify. **Non testé sur la tablette.**
- **Trois façons de l'ouvrir** : l'appui long d'une tuile `med` sans option `t` (avant : le popup appareil ; avec `t`, toujours la télécommande), la **mini-barre « en lecture »** de l'accueil, posée sur le cadre « Ok Nabu » quand le lecteur montré joue, et 5 min après une pause (pochette, titre, artiste, lecture-pause ; un toucher ouvre le popup), et le **geste « musique »** (code ajouté à la fin de la liste des gestes, index 24, après celui de la zone à gauche de l'horloge, et dans le blueprint « Tab5 — emplacements » : « Musique (lecteur) · Music (player) »), plus l'option « Musique » à la fin de « Aller à l'écran ».
- **Côté Home Assistant** (rien en dur, ADR-0024) : les lecteurs proposés se choisissent dans la liste à choix multiples « Tab5 · lecteurs de musique · music players » (`packages/tab5_reglages.yaml`, six au plus). Nouveau package `packages/tab5_lecteur.yaml` : le lecteur montré (celui choisi sur l'écran, sinon le premier qui joue), une poussée quand son état change (une seule à la fois, mode `restart`, 400 ms de regroupement : pas de rafale pendant la lecture, la position avance sur la tablette), et les commandes du popup en liste blanche sur les lecteurs choisis (`script.tab5_lecteur_commande`, appelé par `tab5_evenements.yaml`). La pochette est téléchargée par la tablette depuis Home Assistant (`entity_picture`, en http sur l'adresse de HA vue par la tablette : https n'est pas pris en charge).
- **Contrat** : nouvelle action `tab5_maj_lecteur` (`lecteurs`, `etat`) et nouvel événement `esphome.tab5_lecteur` (`action`, `lecteur`, `valeur`) ; `contrat/contrat.yaml` passe en 1.1.0. **Ordre de mise à jour : le firmware d'abord**, puis `packages/tab5_reglages.yaml`, `packages/tab5_lecteur.yaml`, `packages/tab5_evenements.yaml` et le blueprint « Tab5 — emplacements ». Fichiers HA neufs avec un firmware d'avant : la poussée du lecteur échoue sans arrêter le reste (`continue_on_error`) ; firmware neuf avec des fichiers HA d'avant : le popup reste sur « En attente de Home Assistant » et la mini-barre ne paraît pas.
- **Lecture du payload** en C++ pur testé sur PC (`Tab5/socle/tab5_parse.*`, section 9, cas dans `tools/test_parse.cpp`, appelée par le fuzzeur `tools/fuzz/fuzz_parse.cpp`) ; graine du service pour `tools/sanitizers/fuzz_services.py`. Textes en six langues (7 clés). Glyphes MDI ajoutés aux polices 26, 32, 45, 70 et 120. Le tableau de bord HA du Tab5 montre la nouvelle liste.
- **Fichiers partagés avec d'autres lots** (blocs isolés) : `Tab5/paquets/tab5-lvgl.yaml`, `Tab5/ecran/tab5_zones.h` / `.cpp`, `Tab5/paquets/tab5-navigation.yaml`, le blueprint `tab5_emplacements.yaml`, `tests/test_appuis.py`.

### 2026-10-10 — Accueil : la zone à gauche de l'horloge au choix, vocal ou graphique des prochaines heures (ADR-0051)

- **Demande d'Axel** : choisir ce que montre la zone à gauche de l'horloge (au-dessus d'Ok Nabu) — les commandes vocales, un lecteur audio ou un graphique —, comme les autres zones, et passer au contenu suivant d'un tap sur une zone de l'écran (pour l'instant la seconde température, qui ouvrait l'Arcade) ou par un geste au choix.
- **Deux contenus** : le **vocal** (micro, Domo, Discu : regroupés dans le conteneur transparent `zone_vocal`, rendu et gestes inchangés) et un **graphique des 15 prochaines heures** (`zone_graphique`, nouveau module `Tab5/ecran/tab5_zone_gauche.cpp`) : courbe de la température lissée comme celle du popup Météo (`ui_courbe_lisse()`, sortie de `tab5_meteo.cpp` pour être partagée), point et valeur à l'heure la plus chaude et la plus froide, barres de pluie en mm aux couleurs de la pluie, trois heures et le cumul de pluie. Lu dans `cal_heures_data` : rien de nouveau demandé à Home Assistant. Une trentaine d'objets (`lv_line` + `lv_obj`), construits au premier affichage, repeints seulement aux nouvelles prévisions horaires ou au changement de thème ; couleurs de la palette, aucune nouvelle police, aucun nouveau texte. Un tap sur le graphique ouvre le popup Météo. Le **lecteur** est réservé (`ZoneGauche::LECTEUR`, code `lecteur`) : le firmware le saute et le blueprint ne le propose pas tant qu'il n'existe pas.
- **Bascule** : le tap sur la seconde température (`btn_serre_games`, id gardé) passe au contenu suivant ; son appui long (historique) ne change pas. Sans seconde sonde, l'icône à sa place n'est plus une manette mais `view-carousel` (U+F056C). Nouveau code de geste `zone_gauche_suivante` (index 23, à la fin de `kCodesGestes`), au choix sur les douze gestes de l'accueil. **L'Arcade reste à un toucher** : le tap du bouton manette (« auto ») et « Jeux » dans la roue de navigation.
- **Blueprint « Tab5 — emplacements »** : section « Zone à gauche de l'horloge » (contenu au démarrage, contenus que le tap fait défiler) et option `zone_gauche_suivante` dans les gestes ; nouvelle clé `gauche|départ|c1|c2…;` de `tab5_maj_emplacements`, lue en C++ pur par `zone_gauche_lire()` (`tab5_parse`, cas dans `tools/test_parse.cpp`, appelée par le harnais libFuzzer, graine ajoutée à `fuzz_services.py`). Sans la clé (blueprint d'avant) : vocal au démarrage, vocal et graphique au tap. Choix gardé en NVS (« zgau »).
- **Contrat inchangé** (aucune variable de service ajoutée ou retirée ; `contrat/contrat.yaml` intact). Ordre de mise à jour libre : un firmware d'avant ignore la clé `gauche` et le code `zone_gauche_suivante` ; un blueprint d'avant laisse le cycle par défaut. **Blueprint à recopier** dans Home Assistant pour choisir le contenu.
- Docs : ADR-0051, notice de l'accueil (EN/FR), `docs/screens.md`, `docs/arcade.md`, `docs/architecture.md`, `docs/demo_mode.md`, `adapt-to-your-home.md`, `Tab5/README.md`, boîte à outils d'`AGENTS.md`. Rendu hors tablette : l'Arcade s'ouvre par le bouton manette, deux nouvelles captures (`accueil-zone-graphique`, et dans le thème Capsule). Tests : `tests/test_zone_gauche.py` (nouveau), `test_appuis.py`, `test_gestes.py`, `test_emplacements.py`, `test_historique.py`. Non essayé sur la tablette.

### 2026-10-09 — Popup Caméras : une image fixe de chaque caméra (ADR-0049)

Demandé dans la [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278) (caméras Tapo). Firmware et blueprint « Tab5 — emplacements » à recopier ; ni package ni script. **Non testé sur la tablette ni avec une vraie caméra.**

- **Une page par caméra** choisie dans la nouvelle section « Caméras · Cameras » du blueprint (8 au plus), changée d'un glissé ; le nom et l'heure de l'image dessous, des pastilles à partir de deux caméras. **Pas de vidéo** : une image fixe, la suivante 5 s après la précédente, **seulement popup ouvert** ; l'image décodée (960 × 540 RGB565, environ 1 Mo de PSRAM) est libérée à la fermeture.
- **Contrat 1.2.0 (ajout)** : événement `esphome.tab5_cameras` (ouverture, toutes les 4 min popup ouvert, après une image en échec) et action `tab5_maj_cameras(adresse, cameras)` (« nom|image;… », `image` = l'`entity_picture`, jeton tourné par HA toutes les 5 min). Lecture en C++ pur (`cameras_lire()`, `camera_url()`, `ha_base_depuis_hote()`, `tab5_parse`), testée et fuzzée. La tablette ajoute `width=960&height=540` pour que HA réduise l'image ; l'adresse de HA est celle d'où il se connecte (port 8123), ou celle du blueprint.
- **Ouverture** : option « Caméras » de « Aller à l'écran » (HA peut l'ouvrir, une sonnette par exemple), code de geste `cameras` (index 25) dans le blueprint. **Roue de navigation** (demande d'Axel) : « Caméras » à la fin de la famille « Agenda ▸ », Appareils étant plein (six choix au plus) ; toujours proposé, comme Météo (sans caméra, le popup dit « Aucune caméra choisie »). Glyphe `cctv` ajouté à `mdi_font_70`, `_36`, `_32` et `_26`. Pas de tuile (écartée dans l'ADR).
- **Coûts estimés, non mesurés** : boucle principale bloquée pendant la capture par HA puis le décodage (0,2 à 0,5 s estimés par image) ; tampon de téléchargement de la taille du JPEG gardé après la fermeture (limite d'ESPHome). JPEG progressif refusé (« Image indisponible »). Une caméra hors ligne fige l'écran jusqu'à 12 s par essai (timeout de `http_request`) : essais espacés de 10, 30 puis 60 s après des échecs de suite.
- Notice [Caméras](../../docs/notice/cameras.md), rendu hors tablette `cameras` (la page sans image : le rendu ne télécharge rien).

### 2026-10-09 — Accueil : lignes de capteurs centrées en hauteur, cadres Ok Nabu et − / + ajustés au texte, points des prévisions plus bas

- **Demande d'Axel** : les lignes de capteurs de la rangée sous l'horloge (ADR-0031) et du panneau « Ok Nabu » (ADR-0041) collaient au haut de leur zone — sur le rendu, encre à 4 px du haut du cadre Ok Nabu et 31 px du bas, à 3 px du haut de la rangée et 17 px du bas. Cause : LVGL pose la piste d'une flexbox en haut (`flex_align_track` à START par défaut) ; `flex_align_cross: CENTER` ne centrait l'élément que dans cette piste. `rangee_panneau.yaml` centre maintenant la piste. La ligne des plantes et la ligne d'écoute ne changent pas. **Non testé sur la tablette.**
- **Demande d'Axel** : les cadres « Ok Nabu » et − / + étaient trop hauts pour leur texte (90 px pour une ligne de 45 px, le texte « nageait »). Ils font 72 px (`cadre_bas_h`) : un intérieur d'une ligne de zone (`ligne_zone_h`, 70 px, la hauteur de la rangée sous l'horloge, où se dessinent les mêmes lignes ; deux jetons de `tab5-ui-tokens.yaml`), bas toujours à y 308. La place libérée va au-dessus : la rangée Domo / micro / Discu reste centrée entre le bandeau d'état et le cadre Ok Nabu (56 px d'air de chaque côté, 47 avant ; y 90 au lieu de 81), les températures entre les boutons du haut et la tuile − / + (centre à y 173 au lieu de 164, zones tactiles avec). − et + font 52 px, à 14 px des côtés (alignés sur les températures, comme avant) et 10 px du haut et du bas, au lieu de 62 px à 14 px des quatre bords : plus grands ou plus près du bord, l'arrondi de la tuile en gélule du thème Capsule rognait leurs coins (vu au rendu avec 56 px à 8 px ; `test_nabu.py` le calcule dans les 21 thèmes). Dans le cadre Ok Nabu, une ligne d'icônes sans valeur prend des icônes de 45 px (70 sous l'horloge) et les éléments gardent 17 px des bords (`kMargeCadre`, 14 avant) pour rester dans la gélule du thème Capsule. **Non testé sur la tablette** ; à regarder sur l'écran : la taille de − et + au doigt.
- **Demande d'Axel** : les points de pagination des prévisions (sous la carte centrale) descendent de 3 px (`pagination_container`, y 417 → 420, après 414 → 417 le même jour) : encre à y 420-423, 3 px sous la carte centrale (bas à 416 ; ils la touchaient), 18 px au-dessus des onglets des tuiles (442). **Non testé sur la tablette.**
- Tests : `tests/commun.py` gagne `jeton()` et `avec_jetons()` (lire une géométrie posée par un jeton) ; `test_nabu.py` (cadre, arrondi dans les 21 thèmes avec le nouveau rayon borné, tuile − / + en miroir, lignes centrées), `test_carrousel_clim.py`, `test_roue_clim.py`, `test_gestes.py` suivent. Rendu : le point de toucher de la température du salon passe à (955, 173).

### 2026-10-09 — Réveil en cinq pages, réglé sur des rouleaux

Firmware seul : aucun fichier Home Assistant à recopier, mêmes entités, contrat inchangé.

- **Cinq pages** (demande d'Axel : « plus claire, plus grande, avec le switch pour passer d'une partie à l'autre comme les Paramètres ») : **Heure** (l'heure fixe en grand, la bascule marche / arrêt, la prochaine sonnerie, Tester et l'arrêt), **Jours** (mode, jours de l'heure fixe, jours de repos), **Ouverture** (délai, pas avant, pas après, repos mini), **Sonnerie** (mélodie, volume, volume progressif, répétition, durée max) et **Annonces** (annonce parlée, annonce des rendez-vous et son avance, prochain rendez-vous). Une page se choisit par son nom à côté du titre ou d'un glissé sur la fenêtre, en boucle, sans animation ; la fenêtre s'ouvre toujours sur Heure.
- **Des rouleaux à la place des 20 boutons ‹ › / ▲ ▼** : l'heure et les minutes (de 5 en 5) en 130 px, les sept autres valeurs en 45 px (`ui_components/rouleau.yaml`, un `lv_roller` LVGL). La liste des valeurs et le calcul index ↔ valeur sont du C++ pur testé sur PC (`rouleau_*()`, `tab5_core.cpp`, `tools/test_alarm_clock.cpp`) ; une valeur réglée autrement dans HA (une minute hors des multiples de 5) s'insère à sa place au lieu d'être arrondie. Bornes et pas = ceux des entités (`tests/test_alarme_popup.py`).
- **La roue d'actions rapides (ADR-0036) n'est pas utilisée** : son premier anneau ne porte que des icônes, elle doublerait des rouleaux qui montrent déjà toutes les valeurs, et elle passerait par `tab5_roue.cpp`, que d'autres lots changent.
- **Ajouts** : quatre préréglages de jours (Tous les jours, Lundi-Vendredi, Lundi-Samedi, Week-end : l'option du select `Réveil : jours`, jusqu'ici réglable seulement dans HA) ; une phrase sur la page Ouverture qui dit l'effet des quatre réglages (« Sonne 1h00 avant l'ouverture… ») ou que le mode n'est pas choisi ; le prochain rendez-vous sur trois lignes au lieu d'une ; une mélodie se choisit d'un tap (elle se fait entendre, comme avant).
- **Retiré** : le bouton « Fermer » (la croix de l'en-tête fait la même chose, ADR-0009) ; les scripts internes `tab5_alarm_shift` et `tab5_alarm_cycle_melody`, remplacés par `tab5_alarm_rouleau` et `tab5_alarm_melodie` (aucun n'était appelé par HA).
- **Partagé avec les autres popups à pages** : le geste et la couleur du nom de la page affichée viennent de la brique commune (`pages_brancher()` et `choix_peindre()`, `Tab5/ecran/tab5_pages.cpp`, ADR-0046) ; les noms gardent leur place du popup. La brique ignore désormais aussi un geste parti d'un rouleau (`lv_roller`), comme d'un curseur ou d'un arc. Avec les Réglages : les noms des pages (`reglages_onglet.yaml` avec `prefixe: reveil` et une largeur `w` facultative) et les titres de rangée ; nouveaux gabarits `choix_btn.yaml` (19 boutons à choix) et `rouleau.yaml` (9 rouleaux). `bouton_pas.yaml` ne sert plus qu'aux flèches du thème. Glyphe `volume-plus` ajouté à `mdi_font_32`, `chevron-up` / `chevron-down` retirés (plus affichés).
- **Sanitizers** : UBSan signale un décalage d'une valeur négative dans le dessin des rouleaux de LVGL 9.5.0 (`lv_roller.c:598`, amont, bénin sous GCC). Il est inscrit dans la liste des rapports connus de `tools/sanitizers/rapports.py`, toujours affiché (« connu, amont »), sans faire échouer le job ; tout autre rapport le fait.
- Rendu hors tablette : `reveil` (page Heure), `reveil-jours`, `reveil-ouverture` (par un glissé parti d'un préréglage, qui ne doit pas le déclencher), `reveil-reglages-sonnerie`, `reveil-annonces` ; `reveil-sonnerie` suit le bouton Tester. **Non essayé sur la tablette** (geste des rouleaux au doigt, lisibilité).

### 2026-10-09 — Popup Météo : les prévisions en graphiques (ADR-0043)

- **Demande d'Axel** : une fenêtre « Météo » plein écran, chrome partagé, trois pages changées par leur nom à côté du titre ou d'un glissé, comme les Réglages. « Aujourd'hui » : la météo du moment (icône, température, condition en mots, minimum et maximum du jour, pluie sur les heures montrées) puis jusqu'à 15 heures en courbe lissée (monotone, sans dépassement), icônes, valeurs à la couleur de la température, barres de pluie en mm, l'heure en cours marquée, « Demain » après minuit. « 10 jours » : une ligne par jour, barre en dégradé du minimum au maximum sur une échelle commune, la température du moment en point. « Détails » : la pluie dans l'heure à l'échelle du temps devant des lignes de niveau, la phrase de la carte centrale, humidité, indice UV (niveaux de l'OMS), probabilités de gel et de neige. **Non testé sur la tablette.**
- **Accès** : option « Météo » à la fin de « Aller à l'écran » (`Ecran::METEO`), code de geste `meteo` à la fin de la liste (index 22, après ceux de la roue de navigation) et dans le blueprint « Tab5 — emplacements » (« Météo · Weather »), « Agenda ▸ Météo » en tête de la roue de navigation (ADR-0042), appui long sur le corps d'une carte de prévision **qui ne porte pas d'appareil** (celle qui en porte garde l'appui long de l'appareil). L'appui long de la carte centrale ne change pas.
- **Rien de nouveau demandé à HA** : le popup lit les prévisions horaires et journalières, les barres et la phrase de pluie de l'accueil, et garde enfin les champs de `tab5_maj_meteo_actuelle` et `tab5_maj_probabilites` (jusque-là « réservés »). `packages/tab5_push.yaml` renvoie le troisième bloc horaire (`count: 3`, heures 10 à 14) ; la démo aussi.
- **Ordre de mise à jour** : indifférent, `contrat/contrat.yaml` ne change pas. Un firmware d'avant reçoit le troisième bloc sans l'afficher (comme avant le 25/09) ; un package d'avant avec ce firmware : dix heures dans la courbe.
- **Factorisé** (règle 5) : les pages passent par la brique commune des popups à pages (`pages_brancher()`, `choix_peindre()`, `tab5_pages.cpp`, ADR-0046), les noms des pages restant aux places fixes de `reglages_onglet.yaml`, qui prend un `prefixe` (`reglages` ou `meteo`) ; `ui_rectangle()` / `ui_ligne()` / `ui_poser()` quittent `tab5_historique.cpp` ; `update_meteo_icon()` prend une échelle (deux polices de 48 et 32 px pour les icônes des heures). Textes en six langues (32 clés, contexte `uv` pour les niveaux d'UV).
- Rendu hors tablette : `meteo-aujourdhui`, `meteo-jours`, `meteo-details`.

### 2026-10-09 — La roue de la clim par la température de la pièce (ADR-0048)

- **Demande d'Axel** (correction du carrousel, ADR-0038) : toucher la température de la pièce ouvre une **roue**, celle des tuiles (ADR-0036), pas une page. La roue de la clim de la pièce affichée en mode HA (sa clim propre d'abord), sinon de celle du blueprint, en éventail entier vers le haut depuis un point bas sous la température (posée sur elle, trop haute, la roue s'ouvrait dessous, serrée) : Éteindre, Mode ▸, Consigne ▸, Options ▸ (ce que l'appareil a), « Détails » = le carrousel des clims ouvert sur elle. Avec plusieurs clims, **« Clims ▸ »** prend la place de « Maison » : ses choix montrent chaque clim (consigne ou icône éteinte, nom dessous) et un toucher passe la roue sur elle. **Non testé sur la tablette.**
- **Inchangé** : sans réglages reçus pour cette clim (celle du blueprint avant `climr`), le toucher ouvre le carrousel comme avant ; sans aucune clim, la liste de la tuile − / + ; l'appui long ouvre l'historique ; le carrousel, son glisser et ses pastilles ; la roue d'une tuile `cli`. Aucune commande nouvelle, contrat inchangé (emplacements `clim`, `tRT`, `cpR`) : rien à recopier dans Home Assistant.
- **Un seul code de roue clim** (`Tab5/ecran/tab5_tuiles_roue.cpp`) : `clim_roue_ouvrir(ClimRef, ancre)` est la roue d'une tuile sans tuile ; les boutons d'une clim sont construits par le même code pour une tuile `cli` et pour une clim quelconque (blueprint, tuile, pièce). `tab5_clim.cpp` lit aussi la clim d'une pièce pour la roue ; `clim_carrousel_ouvrir()` devient `clim_ref_choisir()` + `clim_carrousel_ouvrir_sur()`, et la carte appelle `clim_temperature_ouvrir()`. Un réglage ou un état poussé d'une clim repeint la roue ouverte sur une clim (avant : seulement l'état de la tuile). Une icône de plus dans `mdi_font_36` (air-conditioner, « Clims »).
- Tests : `tests/test_roue_clim.py`, mises à jour de `test_roue.py`, `test_carrousel_clim.py`, `test_piece_climat.py`. Rendu hors tablette : nouveaux écrans `roue-clim-temperature` (mode HA, la clim du Bureau), `roue-clim-temperature-clims` (« Clims ▸ » déplié) et `roue-clim-temperature-meteo` (mode météo, la clim du blueprint) ; `climatisation-carrousel-mode-ha` passe par le « Détails » de la roue (même image attendue).

### 2026-10-09 — Popup Température : l'humidité avec la température, une page par pièce (ADR-0047)

- **Demande d'Axel** : un graphique des températures et de l'humidité des capteurs de chaque pièce, si présents, et passer d'une pièce à l'autre dans le popup. **Non testé sur la tablette.**
- **Humidité** : quand l'emplacement a une sonde d'humidité (« Salon — humidité » pour le salon, « Humidité de la pièce » pour une pièce, ADR-0040 ; jamais la serre), sa courbe est tracée sur le même graphique, dans la couleur de l'humidité (`HUMIDITY_WET`, l'accent secondaire sur un thème dont l'accent est du même bleu), avec son échelle en % à droite sur les mêmes lignes de grille ; les degrés passent en couleur d'accent, la légende devient « Température », « Minimum et maximum », « Humidité », et la quatrième carte montre l'humidité du moment et sa plage. Sans sonde d'humidité : le popup d'avant, à l'identique.
- **Pages** : un onglet par température connue à côté du titre (salon, chaque pièce à température déclarée, seconde température ; aucun avec une seule), comme les pages des Réglages ; un toucher ou un glissement à gauche ou à droite montre la suivante, à la même vue, tout de suite d'après ce que la tablette garde (les trois vues de chaque température déjà vue, ~40 Ko de PSRAM au lieu de ~6 Ko), puis la réponse de HA. Un appui long sur l'humidité d'une pièce (à droite, mode HA) ouvre aussi le popup. Onglets et geste écrits dans `tab5_historique.cpp`, en attendant la brique commune « popup à pages ».
- **Contrat inchangé (1.0.0)** : mêmes variables de `tab5_maj_historique`, champs plus longs (septième champ d'en-tête = humidité actuelle, trois valeurs d'humidité en % entiers par créneau), qu'un firmware plus ancien ignore. Lecture déplacée dans `historique_lire()` (`Tab5/socle/tab5_parse.h`), testée par `tools/test_parse.cpp` et fuzzée (onzième parseur de `tools/fuzz/fuzz_parse.cpp`).
- **Home Assistant** : `packages/tab5_historique.yaml` (champ `humidite` du script, deuxième statistique dans le même `recorder.get_statistics`) et le blueprint « Tab5 — emplacements » (il passe la sonde d'humidité de l'emplacement) changent : à recopier.
- **Ordre de mise à jour HA ↔ firmware** : indifférent. Nouveaux fichiers HA avec un ancien firmware : champs d'humidité ignorés, popup d'avant. Nouveau firmware avec d'anciens fichiers HA : pas d'humidité, le popup d'avant avec ses onglets. Il faut les deux pour voir la courbe d'humidité.
- **Rendu et démo** : humidité du salon et du Bureau dans la démo ; `temperature-salon`, `temperature-piece` et les écrans `temperature-serre*` / `temperature-dehors` changent (onglets) ; nouveaux écrans `temperature-glisser` (du salon à l'Entrée, sa température seule) et `temperature-onglet` (l'onglet du Bureau, vue 7 jours).


### 2026-10-09 — Roue de navigation sur l'appui long de la carte centrale (ADR-0042)

- **Demande d'Axel** : l'appui long de la carte centrale (chacun de ses panneaux, et le titre d'une page de prévisions ou de la pièce du mode HA) ouvre, au-dessus d'elle, la roue d'actions rapides des tuiles (ADR-0036) sur les écrans de la tablette. Six boutons, un mot à côté de chacun : **Alertes** (l'historique, ce qu'ouvrait l'appui long jusqu'ici ; marqué quand une alerte est à l'écran), **Pièces** (Maison, puis chaque pièce qui a des appareils : le mode HA sur elle, d'un toucher), **Appareils** (Températures, Clims, Lumières, Volets, Énergie, Plantes), **Agenda** (Calendrier, Réveil ; la Météo du lot du popup météo y prendra la tête), **Tablette** (Jeux, Réglages, Système) et **Assistant**. Une famille se déplie sur le second anneau, le moyeu dit laquelle. Seuls les écrans qui ont quelque chose à montrer dans la maison sont proposés ; une famille vide disparaît.
- **Roue** : un mode « mots » (`RoueTete::mots`) écrit le mot de chaque bouton dans son axe, à une distance mesurée sur le mot (`mot_recul()`), les mots d'une rangée se partagent la largeur (`largeurs_libres()`). Les mots du second anneau, dans toutes les roues (tuiles comprises), suivent la même règle : un mot sur le côté ne couvre plus son bouton. Six légendes (`roue_bouton_N_legende`) remplacent les deux des liens « Maison » / « Détails », qui restent à leur place. Rose des vents au moyeu (`mdi_font_45`, `mdi_font_70`), seize icônes de plus dans `mdi_font_36`.
- **Trois écrans de plus** dans « Aller à l'écran » et les gestes de l'accueil : *Lumières* et *Volet* (le popup d'une tuile, sur la pièce affichée en mode HA, sinon la première qui en a : `tuiles_ecran_ouvrir()`), *Température* (l'historique de la température de gauche de l'accueil, sinon la serre). Codes de geste `lumieres`, `volet`, `temperature` et `roue` (la roue elle-même, depuis l'horloge ou un bouton du haut), ajoutés après `nabu_suivant` dans `kCodesGestes` et le blueprint « Tab5 — emplacements » (la NVS garde l'index).
- **Ordre de mise à jour** : indifférent, `contrat/contrat.yaml` ne change pas (aucune variable, aucun événement). Ce blueprint avec un firmware d'avant : un geste réglé sur un des quatre nouveaux codes ne fait rien. Un blueprint d'avant avec ce firmware : les nouveaux choix ne sont pas proposés.
- Nouveau fichier `Tab5/ecran/tab5_roue_navigation.cpp`, test `tests/test_roue_navigation.py`. Non essayé sur la tablette au moment de la PR (rendu, lisibilité des mots, appui long au doigt).
### 2026-10-09 — Réglages : mode de charge, animations, Wi-Fi éco (ADR-0045)

- **Demande d'Axel** : trois réglages de plus, dans les Réglages de la tablette et dans Home Assistant (tableau de bord du Tab5 compris). Les défauts gardent le comportement d'avant. **Non testé sur la tablette.**
- **« Tab5 Mode de charge »** (Classique / Rapide, page Batterie, à côté de la limite de charge) : « Rapide » allume la charge rapide du chargeur (`nCHG_QC_EN`, P5), seulement quand le chargeur est allumé — jamais sans batterie, pendant une sonde ou en pause à 80 % (`charge_rapide_voulue()`, `tab5_batterie.h`, appliqué par l'interval qui commande CHG_EN). M5Unified appelle ces deux états 500 mA et 1000 mA : **non mesuré**.
- **« Tab5 Animations »** (Complètes / Essentielles / Aucune, page Apparence, à côté de Nuit) : « Essentielles » garde la seule rotation de la carte centrale ; « Aucune » rend tout instantané. Un seul point de décision, `economie_decider()` : l'économie d'énergie active impose « Aucune », comme avant. `animations_reduites()` devient `animations_niveau()`.
- **« Tab5 Wi-Fi éco »** (Jamais / Sur batterie / Toujours, page Batterie) : l'économie légère du Wi-Fi (`WIFI_PS_MIN_MODEM`), appliquée à chaud par une requête au C6 à chaque changement de décision, jamais pendant la voix, un son ou une OTA (`wifi_eco_voulu()`, `wifi_eco_appliquer()`, journal `tab5.wifi`). Courant gagné, latence ajoutée et prise en compte par le C6 1.4.1 : **non vérifiés**.
- **Écran** : rangées côte à côte (Nuit et Animations ; Limite et Mode de charge), Wi-Fi éco sous l'économie d'énergie ; la phrase sous la limite de charge (« 80 % : la charge s'arrête à 80 % et reprend à 70 % ») quitte l'écran faute de place, elle reste sous la tuile du tableau de bord HA. `reglages_rangee.yaml` prend un `x` facultatif. Traductions dans les six langues.
- **Home Assistant** : trois tuiles sur le tableau de bord du Tab5 (`custom_templates/tab5_dashboard.jinja`, à recopier pour les voir). Pas de nouvelle variable de service : contrat inchangé, ordre de mise à jour indifférent.
### 2026-10-09 — Popups Lumières et Volets : toute la maison, une page par pièce (ADR-0046)

Firmware seul pour les popups : contrat inchangé, aucun événement nouveau.

- **Lumières** (demande d'Axel) : le popup montre toutes les lumières de la maison, **une page par pièce** qui en a. Les noms des pièces sont en haut, à côté de la × (la pièce affichée en couleur d'accent) : toucher un nom ou **glisser à gauche / à droite** change de pièce ; une seule pièce, ni noms ni glissement. Chaque ligne est dessinée comme la carte de la tuile en mode HA : **toucher la pastille ronde** refait le toucher de la tuile (allumer / éteindre, confirmation de l'option `k`), **appui long** ouvre la roue d'actions rapides de la tuile, toucher la ligne la choisit pour l'arc et les couleurs. Le popup s'ouvre sur la pièce de la lumière touchée.
- **Retiré** : le bouton **On/Off** du popup Lumières (la pastille de chaque ligne le remplace) ; l'icône F0425 sort de `mdi_font_56`. Le script `tab5_light_popup_show` (plus d'appelant) disparaît.
- **Volets** : même organisation, une page par pièce qui a des volets ; à gauche les volets de la pièce et deux boutons **Tout ouvrir** / **Tout fermer** (la commande `ouvrir` / `fermer` de chaque volet de la page, un événement par volet), à droite le volet choisi (volet dessiné à glisser, son nom, position, état, Ouvrir / Stop / Fermer). La carte COMMANDES disparaît (ses trois boutons passent sous la position). Un volet aux options `k` ou `r` n'est pas listé, comme avant il n'ouvrait pas ce popup.
- **Sans tuile touchée** : les écrans de la roue de navigation (ADR-0042) — « Aller à l'écran » → *Lumières* / *Volet*, la roue (*Appareils* → *Lumières* / *Volets*) et les gestes `lumieres` / `volet` du blueprint — ouvrent ces popups sur la pièce affichée, ou la première qui en a ; un volet compte s'il est dans le popup (ni `k` ni `r`), sinon l'écran s'ouvrirait vide. Aucun écran, code ni option nouveau : ceux de l'ADR-0042. Le libellé du geste `volet` dans le blueprint devient « Volets de la pièce affichée · Shutters of the room shown » (même valeur : rien à refaire dans HA).
- **Une brique commune des popups à pages**, `Tab5/ecran/tab5_pages.cpp` (`PagesPopup`, `pages_brancher()`, `pages_onglets()`, `choix_peindre()`) : le glissement de page et les noms en haut. Le popup Réglages s'en sert (comportement inchangé), et le popup Température (ADR-0047) pour son glissement et la couleur de son onglet affiché (la place de ses onglets reste la sienne : rendu inchangé). Composants : `pages_onglet.yaml` (nouveau), `piece_ligne.yaml` (issu de `light_sel_btn.yaml`), `volet_btn.yaml` paramétré (taille, pastille, police, fonction appelée).
- Textes : « AMPOULES », « VOLETS », « Tout ouvrir », « Tout fermer » dans les sept langues (« Lumières » et « Volets », les titres, viennent de la roue de navigation) ; « Ampoule », « AMPOULE », « COMMANDES » et les trois noms des lumières 3.x (« Ampoule Chambre / Salon / LEDs ») retirés, plus affichés.
- Non essayé sur la tablette : rendu des noms de pièces à cinq pièces (158 px chacun), glissement, appui long d'une ligne. Les images de la notice (`lumieres-chambre-*.webp`, `volet-*.webp`) montrent encore l'ancien popup.

### 2026-10-09 — Panneau « Ok Nabu » à lignes et défilement au choix (lot 3, ADR-0041)

- **Des lignes dans le bouton « Ok Nabu »** : jusqu'à trois lignes de quatre capteurs, choisies dans la nouvelle section « Panneau Ok Nabu · Ok Nabu panel » du blueprint « Tab5 — emplacements », avec le même modèle, le même dessin et les mêmes clés que la rangée sous l'horloge (lettre `n` : `np`, `nd`, `nLI` dans `tab5_maj_tuiles`, états `nLI` dans `tab5_maj_emplacements`). La ligne d'écoute « Ok Nabu : ON / OFF » vient en premier d'origine (deuxième, troisième ou masquée au choix). Un tap sur la ligne d'écoute bascule toujours le mot de réveil ; sur une ligne de capteurs, il ne fait rien. Pastilles sous le panneau, même glissement que la rangée, 14 px de marge dans le cadre pour ne rien couper dans un cadre en gélule (thème Capsule ; `tests/test_nabu.py` le calcule pour les 21 thèmes).
- **Tap sur les heures** : sa valeur « auto » devient « ligne suivante du panneau Ok Nabu » (code `nabu_suivant`, index 17, icône `microphone-message`), aussi au choix pour les onze autres gestes. Sans ligne dans le panneau, il ne fait rien, comme avant.
- **Défilement au choix** pour la rangée sous l'horloge, le panneau Ok Nabu et la tuile − / + : « Automatique » (avec la carte centrale) ou « Fixe ». D'origine : rangée automatique, panneau et tuile fixes, soit l'écran d'avant. En automatique, un geste remet le compte à zéro ; la tuile − / + passe par la clim et les appareils du blueprint (pas le son de la tablette), sans écrire la NVS, jamais liste ouverte. Clé `defil|rangée|nabu|clim|secondes;` poussée avec les gestes, gardée en NVS. `kTourCentralS` n'a plus qu'une définition (`tab5_internal.h`).
- **Gabarits** : `ui_components/rangee_panneau.yaml` (les deux panneaux de quatre éléments de chaque zone) et `ui_components/rangee_pastilles.yaml` (les pastilles des deux zones).
- **Ordre de mise à jour** : indifférent, `contrat/contrat.yaml` ne change pas (des clés dans les payloads existants). Ce blueprint avec un firmware d'avant : les clés `n` et `defil` ignorées, le bouton reste « Ok Nabu : ON / OFF ». Un blueprint d'avant avec ce firmware : le bouton d'avant et les défilements d'origine.
- Non essayé sur la tablette au moment de la PR (rendu dans les 21 thèmes, tap au doigt, défilement).

### 2026-10-09 — Corrigé : points de pagination des prévisions 3 px plus bas

- Les points sous la carte centrale (`pagination_container`, `tab5-lvgl.yaml`) passent de y 414 à 417 (demande d'Axel). Les points restent à 430-434, sous la carte (bas à 416) et au-dessus des onglets des tuiles (442) ; rien d'autre ne bouge. Rendu à l'écran non vérifié.

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

- **Toucher la température de la pièce** (carte clim de l'accueil) ouvre le popup clim en **carrousel** (demande d'Axel) : une page par clim que la tablette connaît — celle du blueprint, puis chaque tuile de clim sans l'option m dont HA a envoyé les réglages ([ADR-0038](../../docs/decisions/0038-climate-carousel.md)). Glisser à gauche ou à droite montre la suivante ou la précédente, en boucle, sans animation ; des pastilles sous les cartes disent laquelle (8 au plus). Une seule clim : ni pastilles ni glisse, l'écran d'avant. Il s'ouvre sur la clim de la pièce affichée en mode HA quand elle en a une, sinon sur celle du blueprint. L'appui long (historique) ne change pas.
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
  l'ancienne bascule entre deux Home Assistant ([ADR-0008](../../docs/decisions/0008-single-ha-instance.md))
  conditionnait toutes les poussées ; resté à `off`, il figeait l'écran sans aucune erreur. Il
  disparaît, avec l'automatisation `force_primary_active_on_boot`, sa garde de santé « OFF depuis
  5 min », sa tuile, sa ligne et sa pastille du tableau de bord. « MAJ Écran » ne fait plus que
  relancer la poussée complète.
- **« Éteindre les lumières » d'une pièce épargne une lampe réglée sur « Confirmer »**, comme
  « Allumer seulement » et « Lecture seule » : une commande confirmée ne passe jamais par un seul
  toucher ([ADR-0036](../../docs/decisions/0036-quick-action-wheel.md)). « Tout éteindre » de l'ancien
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
  « Ayrıntılar » en espagnol, italien et turc. [ADR-0036](../../docs/decisions/0036-quick-action-wheel.md) et notice à jour.
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
  ([ADR-0033](../../docs/decisions/0033-adjustable-tile.md), [discussion #278](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/discussions/278)) :
  « Tablette » et l'icône d'une tablette ne disaient pas que c'était le volume de la tablette, qu'on
  réglait de 0 à 100 % sans rien voir changer. Elle s'appelle « Son de la tablette » (Tablet volume)
  et montre un haut-parleur, barré à 0 % ou quand le son est coupé ; le muet du popup de l'assistant
  la repeint. Rien ne change dans Home Assistant.

### 2026-10-07 — Roue d'actions rapides à deux anneaux

- **Deux anneaux dans une roue** ([ADR-0036](../../docs/decisions/0036-quick-action-wheel.md), mis à
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
- **Les fichiers Home Assistant en un clic, par HACS** (#363, #365, #371, #375, [ADR-0035](../../docs/decisions/0035-hacs-integration-ha-files.md)) :
  nouvelle intégration « Tab5 ». Le dépôt s'ajoute à HACS comme dépôt personnalisé (un bouton de
  la page d'installation l'ouvre dans HACS). À chaque version, après le redémarrage que demande
  HACS, elle sauvegarde vos fichiers, pose les nouveaux, vérifie la configuration (retour en
  arrière si elle casse) et la recharge, puis lance la mise à jour de la tablette (option, cochée
  par défaut). `tab5_home_assistant.zip` et la mise à jour à la main restent.
- **Des fenêtres comme dans un tableau de bord Home Assistant** : une roue d'actions rapides à
  l'appui long d'une lampe, d'un volet ou d'une clim (#366, [ADR-0036](../../docs/decisions/0036-quick-action-wheel.md)) ;
  un popup « Maison », toute la maison pièce par pièce (#368, [ADR-0037](../../docs/decisions/0037-house-popup.md)) ;
  le popup d'un appareil (interrupteur, prise, scène…) et des cartes du mode HA dessinées comme
  la carte « tile » (#350) ; un volet dessiné à faire glisser dans le popup du volet (#333,
  #349) ; un popup Température, historique des deux températures et prévision, lu dans les
  statistiques de HA (#354, [ADR-0032](../../docs/decisions/0032-temperature-history-popup.md)) ; un
  popup Réglages sur la tablette : luminosité, extinction, thème, langue (#345).
- **Accueil** : sous l'horloge, jusqu'à trois lignes de capteurs en plus des plantes (#344,
  [ADR-0031](../../docs/decisions/0031-row-under-the-clock.md)) ; les − / + de la carte clim règlent
  l'appareil de votre choix (#352, [ADR-0033](../../docs/decisions/0033-adjustable-tile.md)) ; un
  appui long sur chacun des trois boutons du haut, son écran au choix dans le blueprint,
  « Maison » compris (#345, #364, #373) ; les appareils des pièces sur la météo au choix (#331).
- **Alertes de la carte centrale** (#351, #353, #355, #356, #358, #360, [ADR-0034](../../docs/decisions/0034-central-card-alerts.md)) :
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
  ([ADR-0030](../../docs/decisions/0030-documentation-site.md)) ; le guide d'installation en pages,
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
   - **par HACS** (nouveau) : le guide [Fichiers Home Assistant](../../docs/installation/home-assistant-files.md),
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
avec Home Assistant ni le blueprint ne changent ([ADR-0037](../../docs/decisions/0037-house-popup.md)).
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
[ADR-0036](../../docs/decisions/0036-quick-action-wheel.md). Pas encore essayée sur la tablette.
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
- Guide : « Avec HACS » dans [Fichiers Home Assistant](../../docs/installation/home-assistant-files.md)
  (dépôt personnalisé, téléchargement, redémarrage, ajout de l'intégration) et dans
  [Mises à jour](../../docs/installation/updates.md) ; mention dans le démarrage rapide, la page
  d'installation et `HomeAssistant_Config/README.md`.
- La notification « Tab5 : fichiers Home Assistant à mettre à jour » propose aussi la mise à
  jour de HACS.

### 2026-10-07 — Intégration « Tab5 » pour HACS : les fichiers Home Assistant en un clic (lot 1)

Demande d'un utilisateur (discussion #278) : mettre à jour les fichiers Home Assistant sans
télécharger, décompresser et copier l'archive à la main ([ADR-0035](../../docs/decisions/0035-hacs-integration-ha-files.md)).
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
l'icône ou de la température du salon ([ADR-0033](../../docs/decisions/0033-adjustable-tile.md)).
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
graphique ([ADR-0032](../../docs/decisions/0032-temperature-history-popup.md)). Firmware, blueprint et
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
- Décision : [ADR-0034](../../docs/decisions/0034-central-card-alerts.md) (lot 5) rassemble les choix
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
**rangée** de lignes qui tournent avec la carte centrale ([ADR-0031](../../docs/decisions/0031-row-under-the-clock.md)).
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
maintenir deux fois les mêmes textes ([ADR-0030](../../docs/decisions/0030-documentation-site.md)).
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
([ADR-0028](../../docs/decisions/0028-solar-energy-popup.md), amendement du 05/10).
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

## [3.6.0] — 2026-10-05

De `v3.5.0` à aujourd'hui : vingt-huit pull requests (#296 → #319, #321 → #325), dont sept nées
des idées et des retours de @husyildiz (discussion #278 : #302 à #305, #308 à #310) — merci
à @husyildiz —, et celle de la release (#320).
- **Vingt et un thèmes, chacun clair ou sombre** (#312, #314 à #318, #324, #325, [ADR-0029](../../docs/decisions/0029-themes-palette.md)) :
  couleurs, formes (rayons, bordures, ombres) et polices de l'heure, de la date et des
  titres. Choisis dans Home Assistant (« Thème », « Clair ou sombre ») ou depuis la
  console ; l'écran se repeint sans redémarrer. « Auto » passe en clair le jour et en
  sombre la nuit (automatisation « Tab5 — thème jour/nuit »). Une tablette neuve démarre
  en « Relief doux » ; un thème déjà choisi reste.
- **Accueil redessiné** (#322, #323) : trois colonnes alignées autour de l'horloge, boutons
  et pots à icône seule (le nom et la valeur des pots restent dans « Mes Plantes »), tuile
  clim et « Ok Nabu » en bas des colonnes ; le bouton muet quitte l'accueil (le son se
  coupe depuis le popup Assistant vocal). La police de la date du thème sert aussi aux
  températures, à la consigne, à « Ok Nabu », aux textes de la carte centrale et aux
  titres des prévisions, qui perdent « Prévisions horaires 1/2 » et « Prévisions
  journalières 2/3 ». Horloge : autant d'air au-dessus de l'heure que sous les jambages
  de la date, dans tous les thèmes.
- **Tableau de bord Home Assistant de la tablette** (#313) : la macro
  `custom_templates/tab5_dashboard.jinja` écrit trois vues (Tab5, Réglages, Santé) avec
  vos entités.
- **Énergie solaire** (#308, #310, [ADR-0028](../../docs/decisions/0028-solar-energy-popup.md)) : popup Énergie (installation en direct,
  production par heure, jour et mois) et icône de la production dans le bandeau d'état,
  depuis une section facultative du blueprint.
- **Batterie d'origine** (#303, #309) : la charge est activée au démarrage ; trois entités
  de diagnostic (désactivées par défaut) ; une icône dans le bandeau, montrée seulement si
  l'interrupteur « Tab5 Batterie montée » est allumé.
- **Météo choisie dans le blueprint** (#305), section facultative.
- **Corrigé** : console système lisible en mode clair (#325), prévisions horaires de
  gauche à droite (#304), icônes de nuit (#302), noms des plantes plus rognés (#301),
  accent d'Arcanoïde (#300), classement d'Arcanoïde borné (#299).
- **Documentation** (#296, #306, #319, #321) : réglages et options de la tablette, captures
  des thèmes, du popup Énergie et de Home Assistant, énergie solaire dans le README et sur le
  site, nouveautés du site à jour.

**Compatible dans les deux sens** (lu dans le code, pas essayé) : un firmware 3.6.0 avec les
fichiers HA de la 3.5.0 marche, thèmes compris, mais « Auto » ne bascule jamais (rien
n'allume « Nuit (thème auto) ») et le popup Énergie reste vide ; un firmware 3.5.0 avec les
fichiers de la 3.6.0 ignore l'automatisation des thèmes, qui ne trouve pas d'interrupteur.

### À faire en mettant à jour depuis 3.5.0

1. **Home Assistant d'abord** : remplacer les fichiers par ceux de
   `tab5_home_assistant.zip` (les packages, dont le nouveau `tab5_energie.yaml`, le
   blueprint et `custom_templates/`), puis recharger toute la configuration YAML (Outils de
   développement → YAML) et les modèles Jinja personnalisés (action
   `homeassistant.reload_custom_templates`, ou un redémarrage). Sans cela, la notification
   « Tab5 : fichiers Home Assistant à mettre à jour » le rappelle (3.6 contre 3.5).
2. **Firmware** : entité « Firmware » dans Home Assistant. Une tablette qui n'avait jamais
   choisi de thème passe en « Relief doux ».
3. **Quand vous voulez** : le tableau de bord (étape 7 du guide d'installation, à refaire
   après chaque mise à jour qui ajoute des entités), le thème et « Clair ou sombre », les
   sections « Énergie » et « Météo » du blueprint ; avec une batterie montée, « Tab5
   Batterie montée » et les trois entités de la batterie.

### Mesures de la version

- Image du firmware : +983 040 o par rapport à la 3.5.0 (binaires OTA publiés, `st7123` :
  4 395 008 o contre 3 411 968 o) ; thèmes (palettes, formes, polices), popup Énergie et
  batterie compris, dont 263 536 o pour les polices de date complètes de #323 (ASCII et
  caractères des sept langues). RAM statique : 180 720 o (40,6 %), lue dans le journal de
  la publication. Corrigé après le tag : le CHANGELOG de `v3.6.0` donne +917 504 o et
  180 200 o, ceux de la compilation `build-min` de la CI, sans le composant de mise à jour
  des binaires publiés.
- Sur la tablette de l'auteur : le code des thèmes (`main` à 544d4b2) a tourné du 05/10
  à 6 h 24 jusqu'au flash suivant sans redémarrer, temps de boucle lu dans Home Assistant
  39 ms en « Relief doux », contre 16 ms avant les ombres ; le code de cette version
  (build local avec #323 et #325, même code hors numéro de version) y tourne depuis le
  05/10 à 10 h 53, écran rallumé seul après le flash ; la 3.6.0 publiée y est installée
  depuis le 05/10 à 12 h 01 (OTA, version lue par l'API). Le dessin d'un écran entier passe de
  134 à 169 ms avec les ombres de « Relief plat » (mesuré le 04/10) ; la carte centrale
  qui tourne n'a pas ralenti.
- Rendu hors tablette (CI) : les vingt et un thèmes dans les deux modes, bascule à chaud
  identique au démarrage à froid, au pixel près ; marge du haut de l'horloge mesurée sur
  les 42 captures de la galerie : au pixel près dans vingt thèmes, 1 px de plus en Capsule.
- Compilations requises de la CI (dernière ESPHome et 2026.9.0) : vertes sur `main`.

### Problèmes connus

Ceux de la 3.5.0, et :
- en mode clair, quelques couleurs d'accent (or, avertissement, pluie) restent entre
  3:1 et 3,7:1 sur le verre des popups selon le thème : lisibles, mais sous les 4,5:1
  visés pour le petit texte ;
- les ombres de certains thèmes ralentissent le dessin d'un écran entier (voir les mesures) ;
- trois esquisses avaient un état « bouton actif » propre, pas repris ; les jeux restent
  sombres ; un caractère absent d'une police de thème est dessiné en Roboto ;
- batterie et énergie solaire jamais essayées avec une vraie batterie ni une vraie
  installation solaire (le niveau est estimé depuis la tension) ;
- les nouveaux textes en allemand, néerlandais, espagnol, italien et turc sont traduits
  par une IA, pas encore relus.

### 2026-10-05 — Mode clair : console système lisible

Retour d'Axel sur la tablette : « beaucoup de texte blanc sur fond clair, illisible, dans
les popups en mode clair ».
- **Console système** : en clair, les 21 thèmes gardaient les couleurs de la console
  sombre, des valeurs blanches et des libellés gris-bleu sur le verre clair du popup, et
  des encadrés de confirmation noirs (« Redémarrer la tablette ? ») sous un texte foncé.
  Elle prend maintenant l'encre de chaque thème : valeurs en `TEXT_PRIMARY`, libellés en
  `TEXT_DIM`, encadrés sur le verre du popup.
- **Un rôle de la palette peut renvoyer à un autre** (`CONSOLE_VALUE: TEXT_PRIMARY`,
  `tools/gen_themes.py`) : résolu après l'héritage, chaque thème y met sa propre couleur ;
  Ardoise le fait en clair, les vingt autres en héritent.
- `tests/test_themes.py` : console lisible dans chaque mode (libellés 4,5:1, valeurs 7:1
  sur le verre des popups en clair ; texte des encadrés 7:1 sur leur fond).

### 2026-10-05 — Accueil : une seule police de 45 px, marges de l'horloge égales

Demande d'Axel : égaliser les marges de l'horloge ; la police de la date pour les
températures salon / serre, la consigne de la clim, « Ok Nabu » et les textes de la carte
centrale ; les titres des pages de prévisions sans « Prévisions horaires 1/2 » ni
« Prévisions journalières 2/3 », dans la police de la date ; les popups qui avaient une
autre police de la même taille ; les polices et les textes de langue devenus inutiles.
- **Horloge** : autant d'air entre le haut de la tuile et l'encre des chiffres qu'entre
  le bas des jambages de la date (g, j, p, q, y) et le bas de la tuile, dans les 21 thèmes
  (2e demande d'Axel : l'horloge plus haute, plus d'espace entre l'horloge et la date) ;
  23 px en Roboto, de 12 px (Pacifico, aux longs jambages) à 27 px ; la date ne bouge pas,
  sa ligne de base reste à 32 px du bas. HH:MM centré en moyenne sur les heures possibles
  (l'écart gauche / droite dépend des chiffres : un « 1 » est étroit).
  `theme_polices()` pose maintenant les cadres des rouleaux pour chaque police et retranche
  la bordure du thème (0 à 4 px, parfois d'un seul côté : Relief doux, Obsidienne, Terre
  cuite…), qui décalait l'heure : Relief doux mesurait 34 px en haut pour 30 en bas.
  L'encre se mesure telle qu'elle s'affiche : en bpp 2, ESPHome vide la 1re rangée des
  chiffres ronds de Roboto ou de Nunito (`tools/police_theme.py` les rend avec FreeType
  comme lui), les jambages aussi (`jambage_visible()`) ; les cadres Roboto passent de
  y 27 à 17.
- **Police de la date du thème** (`style_police_date`) sur les températures et la consigne
  de la clim (avant 32 et 55 px), « Ok Nabu », les textes de la carte centrale (planning,
  pluie, alertes, réponse vocale, info sur une ligne ; sur deux lignes, l'info reste en
  32 px pour tenir) et le titre des prévisions, qui ne garde
  que la plage (« Du mercredi 5 août au dimanche 9 août ») : les points sous la carte
  disent la page. Popups : valeurs de la lumière, de l'énergie et des pots, prochain réveil,
  sonnerie, « OK » de la télécommande, A+ de l'assistant (avant roboto_45_b en dur).
  Un changement de thème change donc tous ces textes.
- **Coût** : les 15 polices de date des thèmes passent de 58 à 151 glyphes (146 pour
  Fredoka ; ASCII et caractères des 7 langues, le reste est dessiné par roboto_45_b) :
  +263 536 o de flash (+257 Ko, 49,6 → 52,9 %), RAM inchangée (40,5 %), mesurés par la
  compilation `build-min` de la CI (ESPHome 2026.9.0) avant et après. Aucune police ne disparaît : roboto_45_b reste
  la police de date des thèmes Roboto et le repli des autres, roboto_55_b sert encore au
  réveil, au popup clim et au flipper.
- **Langues** : les deux titres retirés sortent des 6 fichiers de langue ; `Sys`, `HA`,
  `TV`, `Ok Nabu: OFF` et `Ok Nabu : OFF`, textes YAML disparus, sortent de la liste des
  textes non traduits (`tools/i18n_keys.py`). Aucune autre clé morte (recherche stricte,
  commentaires exclus).

### 2026-10-05 — Trois thèmes de plus : Bonbon, Sorbet, Ultraviolet

Demande d'Axel : « ajoute les trois nouveaux thèmes au choix possible pour le Tab ». Les
trois dernières esquisses de la galerie du 04/10/2026 (Claude Fable 5.1) deviennent des
thèmes, convertis comme les dix-sept de #316 ; ils s'ajoutent à la fin du select
« Thème » (`ordre:` 19 à 21), un choix déjà enregistré ne bouge pas.
- **Bonbon** : stickers rose bonbon, bord blanc de 3 px, ombre dure framboise décalée de
  5 px, boutons blancs (mûre la nuit), onglets rose dragée, Pacifico pour l'heure, la date
  et les titres. Les ombres sont les plus lourdes du catalogue : l'esquisse les estimait à
  ≈ 512 000 px ombrés sur l'accueil (≈ 50 ms au pire par redessin complet) et ≈ 29 ms à
  l'ouverture du popup clim ; aucune sur le cadre des popups ni sur les cartes internes.
- **Sorbet** : coques de macaron lilas, boutons menthe, onglets blancs, bord blanc de
  2 px, sans ombre, Quicksand. Non repris : le dégradé rose → bleu de toute la page (le
  fond des pages n'est pas un style de `formes:`).
- **Ultraviolet** (l'esquisse s'appelait « Obsidienne », nom déjà pris) : noir pur, filets
  de 1 px, coins courts, Anton. Non repris : la lueur violette du bouton choisi (état
  « bouton actif », propriétés locales posées par le C++) et l'interlettrage d'Anton.
- Bandeau central sombre en clair dans les trois (`zones_sombres: [bandeau]`) : il garde
  lisibles les icônes de vigilance FFFF00 et FF0000. Les boutons de l'accueil prennent la
  matière des boutons de l'esquisse (`style_clim_btn_page`), comme ceux des popups.
- Écarts à l'esquisse pour les contrastes de `tests/test_themes.py` : Bonbon, le texte
  principal framboise assombri le jour et éclairci la nuit, l'or assombri le jour ;
  Sorbet, le texte principal violet et l'or assombris le jour.
- `tests/test_doc_comptes.py` lit les nombres en lettres jusqu'à 59 (« Twenty-one »,
  « Vingt et un ») ; README, site, `docs/installation.md`, `docs/screens.md` et
  `docs/ui_design.md` disent vingt et un thèmes.

### 2026-10-05 — Accueil : grille du haut alignée, boutons à icône seule

Demande d'Axel : aligner l'horloge sur les boutons de droite, des marges égales autour
de l'heure et de la date, des boutons sans texte avec de grandes icônes, les pots sans
texte, la clim et « Ok Nabu » descendus, le bouton muet retiré, « un joli ensemble bien
propre ».
- **Trois colonnes** à 20 px des bords de l'écran, comme le bandeau central : gauche
  (Domo, micro, Discu, Ok Nabu), horloge, droite (HA, Sys, TV, températures, clim) ;
  15 px entre les boutons, 14 et 15 px de part et d'autre de l'horloge. Hauts alignés
  (horloge et HA / Sys / TV à y 20), bas alignés à y 308 (Ok Nabu, pied des icônes des pots, tuile clim) : 25 px
  au-dessus du bandeau central, l'écart qui sépare le bandeau des titres des cartes météo.
- **Horloge** : tuile de 401 × 210, 32 px d'air entre son bord et l'encre des chiffres en
  haut, la ligne de base de la date en bas, à peu près autant sur les côtés. La date est
  recalée pour chaque police de thème (`theme_polices()`, `tools/police_theme.py`) : leurs
  ascendantes vont de 40 à 53 px et la déplaçaient de 17 px d'un thème à l'autre.
- **Boutons** Domo, Discu, HA, Sys, TV : icône seule, tous en 125 × 90 avec une icône de
  70 px (16 px d'air au-dessus et au-dessous). **Pots** : icône seule, même taille, sur la
  largeur de l'horloge ; le nom et la valeur de chaque pot restent dans le popup « Mes
  Plantes » (appui long). Une seule police d'icônes pour les deux (`mdi_font_70`, déjà là).
- **Clim** : la tuile − / consigne / + prend toute la colonne de droite (405 × 90, − et +
  à 14 px des quatre bords) ; les températures salon / serre sont centrées entre les
  boutons et la tuile. **Ok Nabu** : même place que la tuile clim, en miroir, texte à la
  taille de la date (45 px). **Bouton muet retiré** de l'accueil : le son se coupe depuis
  le popup Assistant vocal.

### 2026-10-05 — Documentation : énergie solaire, nouveautés du site, merci à husyildiz

Demande d'Axel avant la release : l'énergie solaire manquait au README et au site, et un
remerciement à @husyildiz.
- **README** (EN, FR) : énergie solaire dans l'accroche et dans « Ce que ça fait » (popup,
  icône du bandeau, batterie d'origine), avec une capture du popup Énergie (vue Jours,
  rendu de la CI, données de démonstration) ; même capture dans `docs/installation.md`
  (section « Énergie solaire ») et `docs/screens.md`.
- **Merci** à [@husyildiz](https://github.com/husyildiz) dans la section Communauté du
  README et sur le site : ses idées, essais et retours de la discussion #278 ont amené la
  page d'installation pas à pas, l'écran en turc, la météo hors de France, la batterie
  d'origine, le popup Énergie et la météo choisie dans le blueprint.
- **Site** : carte et section « Énergie solaire », « Thèmes » et « Énergie solaire » dans
  le sommaire ; « Nouveautés » s'arrêtait à la 3.1 : cartes 3.6, 3.5 et 3.4 à la place de
  3.1, 3.0.1 et 3.0 (toujours dans les releases et ce journal) ; « À savoir » ne cite
  plus OpenWeatherMap et MeteoAlarm comme seules sources hors de France.

### 2026-10-05 — Documentation : thèmes, réglages de la tablette, captures de Home Assistant

Demande d'Axel pour la release : docs et descriptifs à jour, quelques captures des thèmes
et de Home Assistant, l'installation et les options bien expliquées.
- **README** (EN, FR) : les dix-huit thèmes dans l'accroche, « Pourquoi celui-ci » et
  « Ce que ça fait » ; une planche de six thèmes (rendus de la CI) ; la vue Tab5 du
  tableau de bord de Home Assistant ; après l'installation, liens vers le tableau de bord
  et les réglages.
- **`docs/installation.md`** : nouvelle section « Réglages et options de la tablette »
  (thème, clair ou sombre, nuit, langue, écran, son, réseau, batterie, « Aller à
  l'écran », réveil, rendez-vous, voix, et où les trouver) ; captures de Home Assistant :
  listes « Tab5 · », vues Tab5 et Santé du tableau de bord, colonne « Tablette » des
  réglages, carte Configuration de l'appareil (recadrées, sans donnée personnelle).
- **Site** : étiquette et carte « 18 thèmes », section Thèmes avec la planche, étape
  « tableau de bord et réglages » avec sa capture ; « Home Assistant d'abord » ne parle
  plus du dépôt ni de Python (inutiles depuis l'ADR-0024, le site disait encore le
  contraire) ; sources de la pluie et des vigilances à jour (Buienradar, DWD, Met.no,
  Open-Meteo, CAP Alerts).
- `docs/screens.md`, `docs/ui_design.md` (thèmes ; la couleur des horaires vient de la
  palette, plus de « logique de couleur côté Home Assistant »),
  `HomeAssistant_Config/README.md` (automatisation « Tab5 — thème jour/nuit »),
  `Tab5/README.md` et l'inventaire (plus de « thème Slate »).
- `tests/test_doc_comptes.py` : le nombre de thèmes écrit dans le README, le site,
  `docs/screens.md` et `docs/installation.md` est compté dans `Tab5/themes/`.

### 2026-10-05 — Thèmes : horaires du planning lisibles en mode clair

Le bandeau du planning écrivait « Auj. » et les horaires en blanc, en dur : en mode
clair, sur le bandeau clair des thèmes qui ne le gardent pas sombre (Ardoise, Almanach
imprimé, Ardoise douce, Bento, Graphite, Terre cuite), on ne les voyait presque plus.
Les couleurs du planning (horaires, embauche tôt, « Dem. », « Aucun travail de prévu »)
et celle de l'embauche tôt du jour touché viennent désormais de la palette du bandeau,
et le texte est recalculé au changement de thème. Vu sur la galerie des thèmes du rendu
hors tablette.

### 2026-10-05 — Thèmes : les dix-sept thèmes de la galerie, Relief doux par défaut

Demande d'Axel : « mets tous les thèmes qu'on a faits ce soir », Relief doux par défaut.
Les dix-sept esquisses du 04/10 deviennent des thèmes de l'écran, chacun avec son mode
sombre et son mode clair, ses formes et, pour treize d'entre eux, ses polices
d'affichage ([ADR-0029](../../docs/decisions/0029-themes-palette.md), « lot 3 »).
- **Dix-huit thèmes** dans le select « Thème » : Ardoise, Relief doux, Relief plat,
  Graphite, Almanach imprimé, Ardoise douce, Terre cuite, Craie et ardoise, Almanach,
  Béton brut, Néon calme, Zen Sumi, Bento, Obsidienne, Platine et or, Signalisation,
  Capsule, Pixel. Un thème déjà choisi sur une tablette reste ; une tablette neuve
  démarre en **Relief doux**.
- **Polices** : l'heure, la date et les titres prennent la police du thème (Nunito,
  IBM Plex Serif, Fraunces, Barlow, Oxanium, Murecho, Familjen Grotesk, Gloock,
  Bodoni Moda, Manrope, Overpass, Fredoka, Jersey 10) ; le reste du texte reste en
  Roboto. Un caractère absent d'une police (lettres turques de Fredoka, par exemple)
  est dessiné en Roboto.
- **Formes** : ombres, liserés, rayons et aplats des esquisses, sur les cartes, les
  boutons, les onglets des jours et les cartes des popups ; bandeau central sombre dans le mode clair de douze thèmes
  (horloge aussi pour Béton brut et Obsidienne). Les ombres des cartes des
  jours ne sont plus coupées par leur cellule.
- **Pas repris** : l'état « bouton actif » propre à trois esquisses (Relief doux, Relief
  plat, Néon calme) ; ces boutons gardent la bordure d'accent de l'interface.
- **Coût** : les ombres allongent le dessin d'un écran entier (mesuré le 04/10 sur la
  tablette avec les ombres de Relief plat : 134 → 169 ms ; la carte centrale qui
  tourne n'a pas ralenti) ; les polices
  ajoutent leurs glyphes au firmware.
- **Galerie** : le rendu hors tablette gagne une tâche « galerie » (accueil et popup de la
  climatisation de chaque thème, dans les deux modes), capturée à chaud puis à froid et
  comparée au pixel près.
- Corrigé au passage : la police de date d'un thème a les dix chiffres (seulement 0, 2,
  3 et 8 avant, ceux des dates d'essai).

### 2026-10-05 — Thèmes, lot 3 : formes, zones sombres et polices d'affichage par thème

Suite de la galerie de seize esquisses du 04/10 : un thème change plus que ses couleurs
(ombres douces sans bordure, horloge en police à empattements, bandeau sombre sur un
écran clair). Ce lot pose les mécanismes avec Ardoise seul, **rendu inchangé** ; les
seize thèmes arrivent dans la PR suivante ([ADR-0029](../../docs/decisions/0029-themes-palette.md), section « lot 3 »).
- **Formes** (`formes:` d'un fichier de thème) : rayon, bordure, dégradé, ombre et
  contour de neuf styles partagés (cartes de la page, boutons verre, cartes des popups).
  Cinq styles de la page de plus, copies exactes des cartes météo : horloge, bandeau
  central, carte de la clim, onglets des jours. `tools/gen_themes.py` en écrit des
  tables C++ ; `theme_formes()` repose l'état compilé avant les formes du thème.
- **Zones sombres** (`zones_sombres: [bandeau, horloge]`) : en mode clair, le bandeau
  central et l'horloge peuvent rester sombres (palettes `UIBandeau`, `UIHorloge`).
- **Polices d'affichage** (`polices:`) : l'heure, la date et les titres (en-têtes des
  popups, titre de la carte centrale), dans une police de Google Fonts choisie par le
  thème ; le reste reste en Roboto. `tools/police_theme.py` mesure chaque police
  (taille qui tient dans l'horloge, place du « : », glyphes absents, dessinés par la
  Roboto du même rôle).
- **Boutons verre** : l'effet d'appui reconnaît un bouton à son rayon de 18 ; ils sont
  marqués avant qu'un thème change ce rayon (`on_boot` inchangé).

### 2026-10-04 — Thèmes, lot 2 : mode clair, bascule sans redémarrage, mode Auto

Suite de la demande d'Axel (« un mode clair et sombre pour chacun », une douzaine de
thèmes au choix, bascule sans redémarrage, mode Auto jour/nuit). Ce lot pose le
mécanisme avec un premier thème, **Ardoise** (le sombre d'aujourd'hui et un clair) ;
les douze thèmes viennent au lot 4, les polices propres à quelques thèmes au lot 3
([ADR-0029](../../docs/decisions/0029-themes-palette.md), section « lot 2 »).
- **Trois entités** (`Tab5/tab5-themes.yaml`) : select « Thème », select « Clair ou
  sombre » (Sombre, Clair, Auto) et interrupteur « Nuit (thème auto) ». En Auto, l'écran
  est clair le jour et sombre la nuit : Home Assistant allume l'interrupteur au coucher
  du soleil (automatisation « Tab5 — thème jour/nuit », `packages/tab5_push.yaml` ;
  la tablette est trouvée par l'attribut `theme_nuit` de `sensor.tab5_tablette`). Sans
  HA, le dernier état connu reste. Les trois ont leur tuile dans le tableau de bord
  généré (vue Réglages).
- **Sur la tablette** : une rangée « Thème » dans la carte GESTION de la console
  système, un bouton pour le thème suivant, un pour le mode suivant ; l'écran se repeint
  aussitôt. Les quatre boutons existants passent de 86 à 68 px de haut pour lui faire
  de la place.
- **Bascule sans redémarrage** : les styles partagés sont repeints depuis la palette
  active, puis chaque module C++ repeint les couleurs qu'il a posées lui-même, depuis
  son dernier état (carte centrale, vigilance, pluie, tuiles, mesures, clim, plantes,
  énergie, zones, assistant, calendrier, prévisions). Les jeux restent sombres.
- **Découverte** : ESPHome 2026.9 crée styles et widgets *avant* le setup des
  composants, et le thème gardé en mémoire est restauré pendant ce setup. Un démarrage
  en clair passe donc par le même chemin qu'une bascule à chaud, sans toucher à
  l'`on_boot`.
- **Catalogue** : un fichier par thème (`Tab5/themes/<thème>.yaml`, mode sombre et mode
  clair) ; `tools/gen_themes.py` en écrit `THEMES[]`, les options du select et la
  repeinture des styles. Ajouter un thème = un fichier et une commande, sans C++.
- **Lisibilité** : douze rôles de couleur de plus (67 en tout), dont le texte sur une
  pastille accent pleine (« Tester », « Parler », « OK ») et l'orange de vigilance ;
  `tests/test_themes.py` exige pour chaque mode un contraste de 7:1 pour le texte,
  4,5:1 pour le texte secondaire et 3:1 pour les couleurs d'état, sur les quatre
  surfaces des cartes.
- **Preuve** : le rendu hors tablette gagne une tâche « clair » : chaque écran peint en
  sombre puis basculé à chaud, contre le même écran après un démarrage à froid en clair,
  au pixel près (la tâche échoue sur un écart ; sans les consoles de jeu, qui restent
  sombres).
- Textes de l'écran : « Sombre », « Clair », « Auto » traduits dans les six langues.

### 2026-10-04 — Thèmes, lot 1 : une seule palette pour toutes les couleurs de l'interface

Demande d'Axel : des thèmes, avec un mode sombre et un mode clair. Ce premier lot ne
change **rien à l'écran** : il rend les couleurs changeables ([ADR-0029](../../docs/decisions/0029-themes-palette.md)).
- **Pourquoi c'était impossible** : ESPHome écrit une couleur YAML en dur dans le C++
  généré (`lv_color_make(148, 163, 184)`), et 483 couleurs étaient posées widget par
  widget. Les jetons C++ (`UIColor::X`) étaient des constantes, recopiées à la main du
  YAML.
- **Une palette** : `struct Palette` (`Tab5/tab5_tokens.h`), 55 rôles, et
  `PALETTE_SOMBRE` avec les valeurs d'aujourd'hui. `UIColor` devient la palette active :
  le C++ et les lambdas lisent `UIColor.X` (264 usages renommés, et 34 dans les jeux vers
  `PALETTE_SOMBRE.X`). La table des icônes météo garde un pointeur vers le rôle, plus
  une valeur figée au démarrage.
- **Les styles lisent la palette** : verre, cartes, thème des labels et fond des pages
  par une lambda. Les couleurs de l'interface quittent la section `color:` de
  `tab5-styles.yaml` (restent celles des jeux).
- **33 styles de rôle** (`style_text_dim`, `style_bg_accent`, `style_border_error`…) :
  les 248 couleurs posées sur des widgets (33 fichiers YAML) passent par eux, en
  dernier de leur liste `styles:` (même priorité que la couleur locale remplacée).
  Migration contrôlée arbre contre arbre, fichier par fichier. Gabarits :
  `modal_header.yaml` et `tv_transport_btn.yaml` reçoivent `icon_style`, `tv_app_btn.yaml`
  `style`. Les couleurs propres des ampoules (préréglages du popup lumière) restent des
  données.
- **Les jeux restent sombres** : ils lisent `PALETTE_SOMBRE.X` et gardent leurs palettes.
- **Garde-fous** : règle 8 de `tools/check_tab5_code_rules.py` (aucune couleur figée
  sur un widget hors jeux, aucune couleur de jeu dans l'interface, aucun jeu sur la
  palette active) et `tests/test_themes.py` (chaque palette donne tous les rôles dans
  l'ordre, vigilance Météo-France officielle, chaque style lit la palette, aucun style
  de rôle mort). `tools/check_tab5_modal_chrome.py` reconnaît le voile par son style
  (`style_bg_modal_scrim`).
- Suite prévue : lot 2, la palette claire et un select « Thème » ; lot 3 (au choix
  d'Axel), la bascule sans redémarrage et un mode automatique jour/nuit.

**Non testé sur la tablette.** Preuve attendue : le rendu hors tablette identique au
pixel à celui de `main`, dans les sept langues.

### 2026-10-04 — Tableau de bord Home Assistant de la tablette

Demande d'Axel : tous les réglages de la tablette dans Home Assistant, plus lisibles, et
partagés avec la façon de l'installer.
- **`custom_templates/tab5_dashboard.jinja`** (dans l'archive `tab5_home_assistant.zip`) :
  la macro `tab5_dashboard()` écrit un tableau de bord de trois vues. **Tab5** pour l'usage
  courant (luminosité, volume, écran affiché, haut-parleur, réveil, rendez-vous, assistant
  vocal ; pastilles d'alerte seulement quand quelque chose cloche), **Réglages** (tableau
  « En bref » de ce qui est choisi, puis chaque réglage de la tablette et chaque liste
  « Tab5 · … ») et **Santé** (liaison, courbes de performances, réseau, matériel,
  poussées, alertes de santé). Libellés en français ou en anglais selon la langue de
  l'écran.
- Pourquoi une macro et pas un fichier : les entity_id de la tablette changent d'une
  maison à l'autre (pièce + nom de l'appareil + nom de l'entité, vérifié dans le code de
  HA 2026.9.4), et une entité ajoutée après coup prend la pièce (`m5stack_…` et
  `<pièce>_m5stack_…` sur la même tablette). La tablette est trouvée par le modèle de son
  appareil, chaque entité par la fin de son identifiant, les selects ajoutés par HA
  (pipeline, mots d'activation, fin de la parole) par leurs options, car leur
  identifiant suit la langue de HA. Une carte n'apparaît que si son entité existe.
- L'automatisation du blueprint « Tab5 — emplacements » est trouvée par son nom : tuile
  dans Santé et lien direct vers son éditeur (sinon vers la liste des blueprints). Chaque
  tuile écrit sa largeur : sans `grid_options`, le frontend lui donne 6 colonnes sur 12.
- Installation (`docs/installation.md`, étape 7, et LISEZMOI de l'archive) : un tableau
  de bord vide « Tab5 », la ligne
  `{% from 'tab5_dashboard.jinja' import tab5_dashboard %}{{ tab5_dashboard() }}` dans
  Outils de développement → Modèle, le résultat collé dans l'éditeur de configuration
  brute.
- Preuves : `tests/test_tableau_de_bord.py` (chaque entité du firmware a sa carte, sauf
  le volume en double ; chaque entité citée existe ; rendu avec une fausse maison :
  préfixes mélangés, HA dans une autre langue, sans package, sans tablette, deux
  tablettes ; contre-épreuve : quatre erreurs volontaires, chacune vue) ; le job
  « Installation dans un HA neuf » rend la ligne dans un vrai HA, vérifie les entités et
  l'absence d'avertissement de modèle, enregistre le tableau de bord et le relit. Chez
  l'auteur : 3 vues, 135 cartes, 99 entités, aucune absente, en français et en anglais.

### 2026-10-04 — Icône de la production solaire dans le bandeau d'état

Demande d'Axel : au même endroit que les icônes PC, téléphone, Wi-Fi et batterie, la
production des panneaux solaires en pourcentage de leur maximum, en couleur.
- **Icône** avant la batterie (qui reste en fin de bandeau). Couleur de
  `get_battery_color()`, comme le téléphone et la batterie : vert au-dessus de 80 %, bleu
  de 41 à 80 %, ambre de 20 à 40 %, rouge en dessous, gris à 0 % (la nuit). Glyphe : le
  panneau seul (`solar-panel`) à tous les paliers, le plus net à cette taille. Cachée tant que Home
  Assistant n'a rien envoyé, et sans installation solaire (« nan »).
- **Blueprint « Tab5 — emplacements »**, section « Énergie · Energy » : nouvelle entrée
  facultative **« Puissance crête des panneaux · Panel peak power »** (kWc, 0 = pas
  d'icône). Le blueprint calcule puissance solaire / crête, arrondie et bornée 0-100 (unité
  du capteur lue : W, kW ou MW), et la pousse dans `tab5_maj_emplacements` sous la clé
  **`solaire`** : avec tous les états (connexion, rechargement, « MAJ Écran », démarrage
  de HA), puis avec les mesures lentes toutes les 5 minutes si le capteur a changé ; au
  plus 12 poussées par heure de soleil, aucune la nuit. Une clé plutôt qu'une nouvelle
  action : un firmware plus ancien ignore une clé inconnue, alors qu'une action absente
  arrête le script de HA (même raison que `climr` et `crRT`/`ceRT`). Amendement de
  l'[ADR-0028](../../docs/decisions/0028-solar-energy-popup.md).
- Deux glyphes ajoutés à `mdi_font_26`. `tests/test_solaire.py` : la clé des deux côtés,
  le chemin firmware, le pourcentage du blueprint contre un calcul Python indépendant, et
  quand il part. Rendu hors tablette : six captures (`accueil-solaire-nuit`, `-faible`,
  `-moyen`, `-bon`, `-fort`, et `accueil-solaire-et-batterie`) ; les autres ne changent
  pas (la démo ne pousse pas la clé). `docs/screens.md` et `docs/installation.md` (EN et
  FR), `HomeAssistant_Config/README.md`, `Tab5/README.md`.

**Non testé sur la tablette.** Le blueprint est à redéployer sur Home Assistant.

### 2026-10-04 — Icône de la batterie de la tablette dans le bandeau d'état

Demande d'Axel : la batterie du Tab5 en haut à gauche, avec les icônes PC, téléphone,
Wi-Fi et réveil, aux couleurs de la batterie du téléphone.
- **Icône** en fin de bandeau (après la cloche, comme sur un téléphone : la montrer ou la
  cacher ne déplace aucune autre icône). Glyphe selon le niveau, quatre paliers alignés sur
  les seuils de couleur : pleine (> 80 %), moitié (41 à 80 %), basse (20 à 40 %), « ! »
  (< 20 %) ; un éclair pendant la charge, « ? » sans mesure. Couleur de
  `get_battery_color()`, la même fonction que le téléphone et les capteurs des plantes.
- **Interrupteur « Tab5 Batterie montée »** (réglage de l'appareil dans Home Assistant,
  `tab5-ha-controls.yaml`, éteint par défaut, gardé d'un démarrage à l'autre) : éteint,
  l'icône est cachée. Pas de détection automatique : sans batterie, le chargeur dit « en
  charge » et 8,39 V, soit 100 % (relevé du 03/10). L'icône lit les capteurs sur la
  tablette : elle marche même avec les entités de la batterie laissées désactivées.
- **Bandeau en table** (`BandeauIcone`, `tab5_custom.h`, et `bandeau_apply_ui()`,
  `tab5_zones.cpp`) : les icônes visibles se resserrent au pas de 35 px ; une icône de plus
  = une valeur de l'enum, un label, un pointeur et, si elle peut disparaître, sa condition.
- Six glyphes ajoutés à `mdi_font_26`. Rendu hors tablette : action `rendu_batterie` et
  trois captures (`accueil-batterie-pleine`, `-faible`, `-en-charge`) ; les autres captures
  ne changent pas (interrupteur éteint). `docs/hardware.md` et `docs/screens.md` (EN et FR).

**Non testé sur la tablette ni avec une batterie.**

### 2026-10-04 — Popup Énergie pour une installation solaire

- **Popup Énergie** (idée d'un utilisateur, discussion #278,
  [ADR-0028](../../docs/decisions/0028-solar-energy-popup.md)) : en haut, l'installation en
  direct, en quatre cartes (solaire et production du jour, maison, réseau acheté ou vendu,
  batterie avec niveau, charge ou décharge et température) ; en bas, la production en
  barres par heure (aujourd'hui), par jour (30 jours) et par mois (12 mois), avec le total
  de la période. Une carte sans capteur disparaît ; sans compteur d'énergie, pas de
  graphique. Chrome partagé, registre unique, transitions instantanées, sept langues.
- **Blueprint « Tab5 — emplacements »** : nouvelle section facultative « Énergie ·
  Energy » (puissance solaire, énergie produite, réseau avec ou sans capteur de vente,
  maison, batterie ; cases « Inverser » pour les signes). Laissée vide, rien ne change.
  Remplie, une tuile capteur de l'un de ces capteurs prend la nouvelle option `e` : elle
  montre sa valeur en W/kW ou kWh et ouvre le popup au toucher ; le capteur solaire prend
  la nouvelle icône `solaire` (panneau solaire). « Aller à l'écran → Énergie » l'ouvre
  aussi.
- **Nouveau package `tab5_energie.yaml`** : `script.tab5_energie`, lancé par le blueprint
  quand la tablette ouvre le popup (événement `esphome.tab5_energie`), lit l'historique
  dans les statistiques du recorder (`recorder.get_statistics`, rien de plus en base) et
  pousse l'instantané à chaque changement tant que le popup est ouvert (15 min au plus).
  Rien n'est poussé popup fermé.
- **Firmware** : deux actions de plus, `tab5_maj_energie` et `tab5_maj_energie_historique`
  (21 au total) ; nouveau `tab5_energie.cpp`. La démo pousse une maison solaire (pièce
  « Bureau ») et le rendu hors tablette capture le popup dans ses trois vues.
- `tests/test_energie.py` : contrat et payloads des deux côtés, modèles du package rendus
  sur des réponses de `get_statistics` simulées et comparés à un calcul Python
  indépendant, blueprint avec la section vide et remplie. Non essayé sur la tablette ni
  avec une vraie installation solaire.

### 2026-10-04 — Les pièces décrites dans le README et sur le site

- **README** (EN et FR) et **site** : la limite d'avant la 3.2 (« plus de 3 lumières ou un
  autre appareil par tuile n'est pas encore possible ») est remplacée par les pièces : jusqu'à
  5 pièces de 5 appareils, noms et icônes pris dans Home Assistant ; hors des pièces, une seule
  place par zone (carte clim de l'accueil, TV, téléphone, deux températures, 5 plantes au plus).
  Popup lumières : « les lumières de la pièce (5 au plus) » au lieu de « 3 lumières ».
- **`docs/press/forum_ha_en.md`** : mêmes passages mis à jour, et une note signale ce qui date
  encore de la 3.0 (langues).

### 2026-10-03 — Plantes de l'accueil : plus de nom rogné

- La rangée des 4 plantes sous l'horloge faisait 350 px pour 4 cases de 90 px séparées
  de 11 px (espacement par défaut du thème) : les deux cases du bord dépassaient de 8 et
  11 px et la carte rognait leur texte. Invisible avec « Pot 2 », visible en espagnol : le
  « P » de « Planta 2 » coupé à gauche, « Planta 3 » à droite (rendu hors tablette du
  03/10) ; en turc, le « 3 » de « Saksı 3 » perdait 2 px. La carte fait maintenant 375 px
  (4 × 90 + 5 × 3, espacement à 0) : chaque case garde ses 90 px entiers, la rangée se
  décale d'1 px vers la gauche et devient centrée. Zone d'appui long de même largeur.

### 2026-10-03 — Météo choisie dans le blueprint

- **Blueprint « Tab5 — emplacements »** : nouvelle section facultative « Météo · Weather »
  (idée d'un utilisateur, discussion #278) : l'entité météo des prévisions, la source de la
  pluie dans l'heure et celle des vigilances se choisissent à la souris, comme les appareils
  des pièces. Laissée vide, rien ne change : les listes « Tab5 · … » de
  `tab5_meteo_sources` décident, avec leur repli automatique.
- **Une seule source à la fois** : remplie, la section **écrit** son choix dans ces listes
  (à l'enregistrement de l'automatisation et au démarrage de Home Assistant), qui restent
  la seule chose que lisent les capteurs et les poussées ; l'écran suit sans autre réglage.
  Le blueprint prime tant que le champ est rempli : une liste changée à la main y revient,
  avec une notification qui dit où changer ; une écriture faite par une automatisation
  n'est jamais reprise (pas de va-et-vient entre deux tablettes). Ni le firmware ni les
  packages ne changent.
- `tests/test_meteo_blueprint.py` rend les vrais modèles : champ vide = rien d'écrit et la
  poussée lit la liste ; champ rempli = la poussée demande les prévisions de la météo
  choisie. Contre-épreuves faites (garde « personne » retirée, écriture retirée, tablette
  hors ligne). Le job « Installation dans un HA neuf » refait le parcours dans un vrai
  Home Assistant.

### 2026-10-03 — Prévisions horaires dans l'ordre, de gauche à droite

- **Prévisions par heure** : les cinq tuiles d'une page horaire se lisent maintenant de
  gauche à droite, l'heure la plus proche à gauche, comme les jours. Elles allaient à
  rebours depuis le premier commit (bandeau « De 07:00 à 11:00 », tuiles 11:00 … 07:00) ;
  signalé par husyildiz (discussion #278). Seul l'index du créneau change dans
  `refresh_hourly_forecast()` (`tab5_forecast.cpp`) : l'ordre des deux pages horaires,
  le bandeau, les pièces et leurs boutons (posés par position visuelle, ADR-0023) restent
  tels quels.

### 2026-10-03 — La batterie d'origine se charge, état et niveau dans Home Assistant

Demande d'un utilisateur (discussion #278) : avec la batterie d'origine, on ne voyait pas
si elle chargeait. Le firmware ne touchait pas au chargeur : sur l'expandeur 0x44, seules
les broches du Wi-Fi et de l'USB étaient posées.
- **Charge activée au démarrage** : CHG_EN (PI4IOE 0x44, P7) à 1, comme la bibliothèque
  M5Unified de M5Stack et la config de référence ESPHome (PR #1396 de devices.esphome.io).
  Charge rapide (P5) laissée à l'arrêt, comme cette référence : la tablette reste
  branchée, et la charge standard demande moins de courant au chargeur USB.
- **Trois entités de diagnostic** : `Tab5 Batterie en charge` (binary_sensor
  `battery_charging`, P6 lue toutes les 10 s, changement publié après 30 s),
  `Tab5 Tension batterie` (INA226 en 0x41, à 50 mV près ou toutes les 15 min) et
  `Tab5 Batterie` (niveau en %, estimé d'après la tension, 6,0 → 8,23 V ; inconnu sous 5 V).
- `docs/hardware.md` (broches et section Alimentation, EN et FR), README et cartographie.

**Non testé avec une batterie** : la tablette de l'auteur n'en a pas. Sans batterie, les
trois entités disent « en charge », 8,39 V et 100 % (relevé sur sa tablette le 03/10) :
elles sont donc **désactivées par défaut** dans Home Assistant ; avec la batterie montée,
les activer sur la page de l'appareil.

### 2026-10-03 — Icônes de nuit dans les prévisions heure par heure

- **Prévisions horaires** (discussion #278) : la nuit, un créneau « peu nuageux » montrait
  un nuage avec un soleil. Met.no range « beau » et « peu nuageux » de nuit sous
  `partlycloudy` (les états de Home Assistant n'ont pas de « peu nuageux de nuit ») ;
  la poussée (`packages/tab5_push.yaml`) envoie maintenant, pour un créneau de nuit,
  `partlycloudy-night` (nuage + lune) et `clear-night` à la place de `sunny`, deux icônes
  que la tablette dessine déjà. Jour ou nuit : `is_daytime` du créneau s'il est fourni,
  sinon le lever et le coucher de `sun.sun` (après minuit et le lendemain compris) ; sans
  `sun.sun`, rien ne change. Les prévisions par jour ne changent pas.
- **À installer** : remplacer les fichiers Home Assistant (`tab5_push.yaml`) et recharger
  les automatisations ; pas de nouveau firmware.
- `tests/test_meteo_icones_nuit.py` rend le modèle réel (jour, nuit, autour du lever et du
  coucher, après minuit, pas de 3 h, `is_daytime` présent ou non, sans `sun.sun`, nuit
  polaire) et le compare à un calcul indépendant ; contre-épreuve : ancien modèle → échecs.

### 2026-10-02 — Les sept langues mises en avant

- **README** (EN et FR) : « en sept langues » dans la phrase d'accroche, et une puce dans
  « Pourquoi celui-ci » (menus, jeux, dates, textes de Home Assistant et briefing du réveil
  dans chaque langue ; traduites par une IA, seul le français relu). La phrase météo de
  « Avant de commencer » suit les sources d'aujourd'hui : prévisions de n'importe quelle
  entité météo, pluie de Météo-France ou d'Open-Meteo tout seul, DWD, CAP Alerts…
- **Site** : « Six langues » corrigé en « Sept langues » (la liste en comptait déjà sept),
  étiquette « 7 langues » en haut de page, langues citées dans la description et l'aperçu
  des liens. Description et sujets du dépôt GitHub mis à jour aussi (« 6 languages »).
- `tests/test_doc_comptes.py` compte les fichiers de `Tab5/lang/` et vérifie le nombre
  écrit à ces huit endroits, en chiffres ou en lettres. Contre-épreuves : « Six » remis
  sur le site, puis une huitième langue ajoutée → échecs.

## [3.5.0] — 2026-10-02

De `v3.4.0` à aujourd'hui : trois pull requests du 02/10 (#292 → #294), nées du retour d'un
utilisateur en Turquie (discussion #278), et celle de la release.
- **Écran en turc** (#293) : Türkçe dans le select « Langue », les 953 textes, jeux
  compris, sauf les questions du quiz ; dates dans l'ordre turc. Traduit par une IA, pas
  encore relu par une personne dont c'est la langue.
- **Météo hors de France** : sans Météo-France dans Home Assistant, la carte pluie passe
  toute seule sur Open-Meteo, sans compte ni clé (#292), au lieu de rester masquée. Les
  prévisions venaient déjà de n'importe quelle entité météo ; avec Met.no, celle que Home
  Assistant installe d'office, toute la chaîne est maintenant tenue par un test (#294).

**Compatible dans les deux sens** (lu dans le code, pas essayé) : un firmware 3.5.0 avec les
fichiers HA de la 3.4.0 marche, en turc aussi, mais hors de France la carte pluie reste
masquée tant qu'Open-Meteo n'est pas choisi à la main, et le briefing du réveil en turc parle
français ; un firmware 3.4.0 avec les fichiers de la 3.5.0 a la pluie d'Open-Meteo, sans le
turc.

### À faire en mettant à jour depuis 3.4.0

1. **Home Assistant** : remplacer par ceux de `tab5_home_assistant.zip` les packages
   `tab5_meteo_sources.yaml` (pluie) et `tab5_reveil.yaml` (briefing en turc), avec
   `tab5_health.yaml` qui porte la version des fichiers, puis recharger toute la
   configuration YAML (Outils de développement → YAML). Sans cela, la notification « Tab5 : fichiers Home Assistant à
   mettre à jour » le rappelle (elle compare X.Y : 3.5 contre 3.4).
2. **Firmware** : entité « Firmware » dans Home Assistant.

### Mesures de la version

- Firmware : seul #293 le change. Compilations locales du lot (ESPHome 2026.9) : image
  +26 096 o, dont 2 464 pour les cinq lettres turques des polices ; RAM statique
  inchangée. Le même code (build local) tourne sur la tablette de l'auteur depuis le 02/10
  à 19 h 43 (version et heure de compilation lues par l'API), sans redémarrage depuis.
- Compilations requises de la CI (dernière ESPHome et 2026.9.0) : vertes sur `main`.

### Problèmes connus

Ceux de la 3.4.0, et :
- le turc n'a pas été relu par une personne dont c'est la langue ;
- avec Met.no, les deux pages suivantes des prévisions sur 15 jours ne montrent que le
  6e jour (l'intégration de HA n'en donne que 6) ;
- la page des jours prend les prévisions dans l'ordre reçu, sans lire leur date : avec
  Met.no, qui recalcule sa liste environ toutes les heures, elle peut commencer par la
  veille pendant au plus une heure après minuit (lu dans le code, pas vu).

### 2026-10-02 — Météo de Home Assistant sans Météo-France, prouvée par un test

- `tests/test_meteo_sans_meteo_france.py` rend toute la chaîne des prévisions avec la seule
  météo que Home Assistant installe d'office, Met.no : l'entité est prise sans rien
  choisir, puis les 15 jours, les 10 heures (à l'heure locale), la météo du moment et les
  probabilités envoyés à la tablette. Prévisions = vraie réponse de Met.no pour Istanbul
  passée par le code de l'intégration de HA 2026.9.4. Contre-épreuve : cinq erreurs
  introduites dans les packages, cinq échecs. Les trois payloads rendus aussi dans le
  moteur Jinja de Home Assistant (installation de l'auteur, lecture seule) : identiques.
- **Fait corrigé** : l'intégration Met.no de HA ne donne que **6 jours** (aujourd'hui
  compris) et 48 heures, pas une dizaine de jours ; les deux pages suivantes des 15 jours
  ne montrent que le 6e. Écrit dans les limites du guide d'installation.

### 2026-10-02 — L'écran parle aussi turc

- **`Tab5/lang/tr.yaml`** (Türkçe, index 6), complet : les 953 textes, jeux compris,
  sauf les questions du quiz. Traduit par une IA, pas encore relu par une personne dont
  c'est la langue.
  - Jours en trois lettres (Pzt Sal Çar Per Cum Cmt Paz) ; noms longs des jours et des
    mois écrits avec leur majuscule, comme dans une date turque (le firmware ne met en
    majuscule qu'une première lettre ASCII) ; dates dans l'ordre turc (« 2 Ekim Cuma »,
    et « 02 Eki Cum » sous l'horloge de l'accueil : l'ordre de cette date courte est
    devenu un modèle traduisible, `{jour_court} {quantieme} {mois_court}`, que les autres
    langues gardent dans l'ordre français ; vu par l'auteur sur la tablette le 02/10).
  - Place mesurée en pixels (Roboto 700) contre la plus large des langues française,
    anglaise, allemande et néerlandaise ; ce qui dépasse a été raccourci, ou vérifié dans
    le code (zone plus large, texte qui passe à la ligne).
- **Polices** : Ğ ğ ı Ş ş ajoutés au jeu `&latin1` (roboto_32_b, roboto_45_b,
  roboto_22 ; İ, ç, ö, ü, â, î, û y étaient déjà) : +2 464 octets de firmware. Le filtre
  des noms de tuiles envoyés par HA (`kHorsLatin1`, `tab5_tuiles.cpp`) les garde aussi.
- Select « Langue » : Türkçe ajouté à la fin (index gardés). Le briefing parlé du réveil
  (`packages/tab5_reveil.yaml`) a ses phrases turques.
- **CI** : le rendu hors tablette dessine aussi le turc (sept tâches).
- Mesure (compilations locales, ESPHome 2026.9) : image 3 331 882 → 3 357 978 o
  (+26 096 o, dont 2 464 pour les polices), RAM statique inchangée (171 798 o). Tout
  nouveau texte de l'écran devra aussi être traduit en turc (`_statut: complet`).
- Docs (README, traductions, installation, débogage, site, README HA), cartographie.

### 2026-10-02 — Pluie dans l'heure hors de France sans rien régler

- **Carte pluie** : la liste « Tab5 · source de la pluie dans l'heure » démarre sur
  Météo-France, qui ne couvre que la France. Sans capteur de pluie Météo-France dans Home
  Assistant, elle passe maintenant sur **Open-Meteo** (sans compte ni clé, partout, un
  modèle au pas de 15 min) au lieu de masquer la carte. La liste garde le choix : Météo-France
  ajouté plus tard reprend la main tout seul, « Aucune » n'envoie rien, et l'attribut
  `source` de « Tab5 Pluie dans l'heure » montre la source vraiment utilisée. Chez qui a
  Météo-France, rien ne change. Les vigilances laissées sur Météo-France sans l'intégration
  restaient déjà toutes vertes, comme « Aucune » : c'est maintenant écrit dans le guide.
  Retour d'un utilisateur en Turquie (discussion #278), dont l'écran n'affichait rien.
- `tests/test_pluie_sans_meteo_france.py` rend les vrais modèles du package : source
  effective, état et barres remplis par Open-Meteo. Contre-épreuve sur le package de `main` :
  les trois tests du changement échouent. Rendu aussi dans le moteur Jinja de Home Assistant
  (installation de l'auteur, lecture seule).

## [3.4.0] — 2026-10-02

De `v3.3.2` à aujourd'hui : huit pull requests du soir du 01/10 (#277, #279 → #285), la
page d'installation du 02/10 (#291) et celle de la release.
- **Écran** : verre plein dans les popups (#285) : cartes, boutons et cadres sans
  transparence, plus clairs qu'avant, et chaque popup s'ouvre 8 à 16 % plus vite. L'horloge
  n'est plus coupée au démarrage (#279) et n'affiche plus d'heure ni de date fausses avant
  l'heure réelle (#284) ; dans le popup du réveil, le prochain rendez-vous ne passe plus
  sous « Tester » (#280).
- **Démarrage** : le firmware rejoint Home Assistant 6 s plus tôt (#281), Home Assistant
  envoie tout l'écran sans pause d'une seconde entre les envois (#277), et chaque mois du
  calendrier n'est demandé qu'une fois (#282).
- **Code** : dettes de l'audit des conteneurs (#283), rien ne change à l'écran.
- **Page d'installation** : la suite côté Home Assistant en clair (fichiers, ajout de la
  tablette, blueprint pas à pas) et un bouton pour importer le blueprint (#291).

**Compatible dans les deux sens** : un firmware 3.3.2 avec les fichiers HA de la 3.4.0
reçoit la poussée sans pauses (c'est ainsi qu'elle a été mesurée, #277) ; un firmware 3.4.0
avec les fichiers de la 3.3.2 la reçoit avec ses pauses, comme avant (lu dans le code, pas
essayé).

### À faire en mettant à jour depuis 3.3.2

1. **Home Assistant** : remplacer par ceux de `tab5_home_assistant.zip` le package
   `tab5_push.yaml` et le blueprint `blueprints/automation/tab5/tab5_emplacements.yaml`,
   puis recharger les automatisations et les scripts. Sans cela tout marche, avec les
   pauses d'avant, et la notification « Tab5 : fichiers Home Assistant à mettre à jour »
   le rappelle (elle compare X.Y : 3.4 contre 3.3).
2. **Firmware** : entité « Firmware » dans Home Assistant.

### Mesures de la version

Sur la tablette de l'auteur, chaque gain mesuré par son lot (les deux gains du démarrage
n'ont pas été mesurés ensemble) :
- **Ouverture d'un popup** (#285, build de mesure, ouverture par l'API, médiane de 5) :
  réveil 199,7 → 167,8 ms, clim 181,8 → 158,4, télécommande 173,4 → 151,5, console
  170,3 → 150,9, assistant 163,7 → 144,1, calendrier 163,2 → 145,0, plantes 172,1 → 158,3.
  Écran entier et fermeture d'un popup inchangés (≈ 135 ms).
- **Démarrage du firmware** (#281, 3 démarrages de chaque) : Wi-Fi connecté 10,55 s après
  la coupure (14,85 s en 3.3.2), `esphome.tab5_connected` reçu par HA à 12,0 s
  (18,2-18,4 s).
- **Poussée de HA** (#277, firmware 3.3.2, 5 redémarrages) : toute la poussée part en 0,13
  à 0,19 s (6,1 s avant) ; écran complet 17,5 à 17,8 s après la coupure (24,5 s avant).
- Compilation locale du code de `main` @ `7953a19` (canal bêta et ESPHome 2026.9.0,
  comme les binaires publiés ; la release ne change ensuite que la version par défaut de
  `tab5-ha-hmi.yaml`) : image 3 371 322 o (41,5 % de la partition), RAM statique
  172 254 o ; code et constantes −984 o par rapport à l'ELF publié de la 3.3.2. Ce build
  tourne sur la tablette de l'auteur depuis le 01/10 à 22 h 54 (version lue par l'API).
- Compilations requises de la CI (dernière ESPHome et 2026.9.0) : vertes.

### Problèmes connus

Ceux de la 3.3.2, et :
- les captures de la galerie (`docs/screens.md`) montrent encore le verre translucide ;
- pendant les 2 s qui suivent le démarrage, les boutons de verre ne s'assombrissent pas à
  l'appui (#283) ;
- un conflit d'adresse avec un appareil réglé à la main sur la même IP n'est plus détecté
  au DHCP (#281).

### 2026-10-02 — Page d'installation : la suite côté Home Assistant en clair

- **Page `/install/`, étape 5 « Ensuite »** : les fichiers Home Assistant (lien direct vers
  `tab5_home_assistant.zip` de la dernière release, ligne `packages:`, vérification,
  redémarrage, Home Assistant 2026.8 ou plus récent), le chemin pour ajouter la tablette, et
  le blueprint pas à pas (où le trouver, « Créer une automatisation », cinq appareils au plus
  dans « Pièce 1 ») avec un bouton **Importer le blueprint** (redirection
  `blueprint_import` de My Home Assistant) quand l'archive n'est pas installée. Avant, la
  page disait seulement « vos appareils se choisissent dans le blueprint », sans lien : un
  premier utilisateur d'une ST7121 n'a pas su le configurer (forum Home Assistant, discussion
  #278).

### 2026-10-01 — Verre plein dans les popups, ouverture 8 à 16 % plus rapide

- **Écran : plus de transparence dans les popups.** Cartes de verre (`style_glass_card`,
  80 %), boutons de verre (`style_clim_btn`, 58 %), cadres de la télécommande
  (`style_meteo_card`, 58 %), carte d'un popup empilé (`style_modal_card_verre`, 88 %),
  boutons cyan « Tester », « Parler » et « OK » (`style_pill_accent`, 18 %) et panneaux de
  la console (96 %) passent à 100 %. Verre plein, plus clair qu'avant : choix de l'auteur
  après l'essai sur la tablette (« c'était plus joli »). Sur le tableau de bord, seuls − et +
  de la carte clim changent ; ses tuiles étaient déjà opaques (verre pré-mélangé, #166).
- **Cases du calendrier** : opaques aussi, mais avec la teinte qu'elles avaient en
  transparence, calculée sur la carte du popup (`cal_fond_case()`). Passées telles quelles à
  100 %, elles devenaient gris clair sous des chiffres gris : week-end et jours passés
  illisibles.
- **Mesuré sur la tablette de l'auteur** (build de mesure, ouverture par l'API, médiane de 5,
  avant → après) : réveil 199,7 → 167,8 ms, clim 181,8 → 158,4, télécommande
  173,4 → 151,5, console 170,3 → 150,9, assistant 163,7 → 144,1, calendrier 163,2 → 145,0,
  plantes 172,1 → 158,3. Écran entier et fermeture d'un popup : inchangés (≈ 135 ms). Ces
  chiffres viennent de l'essai, où les cases du calendrier étaient opaques sans
  pré-mélange (même dessin : un fond opaque, seule la couleur change).
- **Appui** : un bouton de verre désormais opaque prend l'appui des surfaces opaques
  (52 % au lieu de 30 %, `tab5_anim.cpp`), sans autre changement.

### 2026-10-01 — Plus d'heure fausse au démarrage

- **Accueil, tuile horloge** : jusqu'à ce que la tablette connaisse l'heure, le YAML
  affichait « 19:50 » et « Jeu 02 Avr », une heure et une date fausses. Chiffres et date
  restent maintenant vides (le « : » reste) jusqu'au premier affichage de l'heure réelle,
  posé sans rouler comme avant. Trouvé par l'audit des conteneurs du 01/10.
  `tests/test_horloge.py` le vérifie ; la clé de traduction « Jeu 02 Avr » est retirée
  des 5 langues.

### 2026-10-01 — Dettes de l'audit des conteneurs (rien ne change à l'écran)

- **Une seule source pour ce qui était recopié** (règle 5), même rendu :
  - les 4 rouleaux de l'horloge viennent du gabarit `ui_components/clock_roller.yaml` et
    les 9 barres de pluie de `ui_components/rain_bar.yaml` (ids inchangés) ;
  - les touches « Pause » et « CANAL+ » de la télécommande rejoignent leurs gabarits
    (`tv_transport_btn.yaml`, `tv_app_btn.yaml`) : leur couleur, celle du thème, y est
    passée en clair (`color_text`) ;
  - la largeur 1180 des panneaux de la carte centrale devient le jeton `${central_w}`
    (`tab5-ui-tokens.yaml`, 7 emplois) ; le C++ la reprend dans
    `kLargeurPanneauCentral`.
- **Calendrier** : les 42 cases partagent leurs styles (`lv_style_t` de
  `tab5_calendar.cpp`) au lieu de 39 propriétés locales chacune (≈ 1 600 en tout) ; ne
  restent en local que la place de la case, la police et ce que le rendu du mois pose.
  Les colonnes (`kCalColX0`, `kCalColPas`, `kCalColW`) sont nommées.
- **Appui des boutons verre (D6)** : 68 `pressed: { bg_opa: 30% }` (52 % pour les trois
  boutons du haut) répétaient exactement ce que `apply_pressed_scale_to_tree()` pose déjà
  sur tout bouton cliquable de rayon 18 ; ils sont retirés. Les 72 autres disent autre
  chose (rayon 12, autre opacité, bordure) et restent. Seule différence : pendant les 2 s
  qui suivent le démarrage, avant cet appel, ces boutons ne s'assombrissent pas à l'appui.
- Nouveau test `tests/test_geometrie_partagee.py` : jeton et constante C++ égaux, en-têtes
  « Lun »…« Dim » sur les colonnes des cases, grille centrée dans la carte (contre-épreuve :
  une colonne de 171 px au lieu de 172 le fait échouer). `tests/test_horloge.py` déplie
  le gabarit et vérifie que les 4 rouleaux en viennent.

### 2026-10-01 — Le firmware rejoint Home Assistant 6 s plus tôt au démarrage

- **Firmware : quatre attentes retirées du démarrage**, trouvées en lisant le démarrage
  sur le port USB, chaque ligne horodatée à sa réception (redémarrages par le bouton HA
  « Redémarrage Système », origine = la coupure de la liaison API).
  - `esphome.tab5_connected` part dès que HA est abonné, sans les 2 s d'attente d'on_boot :
    HA (≥ 2025.9) s'abonne aux états et aux actions dans un seul paquet, et l'API
    d'ESPHome lit les deux dans le même passage. `on_client_connected` (reconnexion sans
    redémarrage) garde ses 2 s : une ancienne connexion pas encore fermée peut y tromper
    la garde. Reçu par HA aux 10 démarrages des deux builds d'essai, aucun « event
    dropped » dans les 9 journaux USB.
  - Wi-Fi `fast_connect` : retour direct au point d'accès et au canal de la dernière
    connexion, sans balayer les canaux (1,7 s) ; la connexion part pendant le setup au
    lieu d'attendre la première image de l'écran. Point d'accès éteint : ESPHome balaie
    après un essai manqué (lu dans son code, pas essayé).
  - Plus de test des 32 Mo de PSRAM avant le lancement du firmware (0,7 s).
  - DHCP sans les deux requêtes ARP de vérification d'ESP-IDF (1,0 s). Contrepartie : un
    conflit avec un appareil réglé à la main sur la même adresse ne serait plus détecté.
  - Mesuré sur la tablette de l'auteur : Wi-Fi connecté 10,55 s après la coupure (14,85 s
    en 3.3.2, 3 démarrages de chaque), `tab5_connected` reçu par HA à 12,0 s (18,2-18,4 s),
    poussée complète 0,25 s plus tard. Détail phase par phase dans `docs/performance.md`.
    Écran validé par l'auteur sur le build d'essai avec l'horloge du même jour.
- **Hors firmware** : rien à mettre à jour dans Home Assistant (l'automation de poussée
  attendait déjà, 10 s au plus, que la liaison de la tablette soit `on`).

### 2026-10-01 — Calendrier : chaque mois demandé une seule fois au démarrage

- **Pré-fetch du calendrier** : base de HA, chaque démarrage du 01/10 demandait octobre,
  novembre, puis encore octobre et novembre (4 événements `esphome.tab5_calendrier_mois`
  et 4 lancements de `script.tab5_calendrier_mois` au lieu de 2). `on_boot` et le front
  montant de `status_ha` lancent tous deux le pré-fetch, à 3 s d'écart, et le script
  repartait de zéro (`restart`).
  - Le corps passe dans `tab5_cal_prefetch` (`tab5-calendar.yaml`). Il ne redemande pas
    un mois reçu il y a moins de 30 s (`CAL_PREFETCH_FRESH_MS`, `tab5_custom.h`) ; HA
    répond en 70 à 110 ms par mois. Il est en mode `queued` : un second appel attend la
    fin du premier au lieu de le couper pendant son délai.
  - `tab5_cal_prefetch_boot` garde son nom et n'a pas de paramètre : `on_boot` n'est pas
    modifié. Le bouton « Recharger le calendrier » force (`force: true`). Une
    reconnexion de HA redemande toujours, le mois ayant été reçu plus de 30 s avant.
  - `tests/test_calendrier_prefetch.py` tient la structure. Mesure sur la tablette :
    non faite (pas de flash de ce lot).

### 2026-10-01 — Réveil : le prochain rendez-vous ne passe plus sous « Tester »

- **Popup du réveil, barre du bas** : la ligne « prochain rendez-vous » faisait 500 px de
  large alors que le bouton « Tester » commence à 401 px ; un titre de plus d'une
  trentaine de caractères passait sous ce bouton translucide. La ligne tient maintenant
  sur une ligne et se coupe avec « … » à 380 px (`texte_ha_coupe()`, comme les tuiles).
  `tests/test_alarme_popup.py` refait le calcul depuis `alarm_popup.yaml` : déplacer le
  bouton ou élargir la barre sans revoir la limite le fait échouer. Trouvé par l'audit
  des conteneurs du 01/10.

### 2026-10-01 — L'horloge n'est plus coupée au démarrage

- **Firmware : la géométrie de l'horloge à rouleau est écrite dans `Tab5/tab5-lvgl.yaml`
  seulement.** Jusqu'ici, `layout_clock_roller()` la recalculait en C++ depuis la police,
  2 s après la fin du démarrage, et les cadres provisoires du YAML (105 px de haut, alors
  que l'encre des chiffres descend à 123 px) coupaient le bas des chiffres en attendant.
  - Vu par l'auteur au démarrage ; mesuré sur le journal série d'un redémarrage de la
    3.3.2 : première image à 33,1 s, horloge recalée à 34,6 s, soit ~1,5 s d'horloge
    coupée. Les valeurs que le C++ calculait (cadres 75 × 104, chiffres à y −23, « : » à
    x 181) sont désormais celles du YAML : l'horloge est juste dès la première image, à la
    même place qu'avant.
  - `layout_clock_roller()` et sa mesure de texte sont retirés (≈ 80 lignes de C++) ; le
    rouleau lit sa course dans la hauteur du cadre. `on_boot` ne fait plus que poser les
    pointeurs du rouleau (accord de l'auteur pour toucher la séquence).
  - Nouveau test `tests/test_horloge.py` : il refait le calcul depuis les métriques de
    Roboto 700 et vérifie le YAML (encre entière dans chaque cadre, largeur d'un chiffre,
    HH:MM centré, « : » aligné). Contre-épreuve : sur les valeurs d'avant, il échoue sur
    « encre jusqu'à 123 px, cadre de 105 px ».

### 2026-10-01 — L'écran se remplit 6 s plus vite après un redémarrage

- **Home Assistant : plus de pause d'une seconde entre les envois vers la tablette** (rien
  ne change dans le firmware).
  - Mesuré au redémarrage du 01/10 à 19 h 51 (flash de la 3.3.2), dans la base de HA : HA
    reconnecté 16,4 s après la coupure, `esphome.tab5_connected` 2 s plus tard, puis la
    poussée complète étalée sur 6,1 s par six `delay: 1s` de `packages/tab5_push.yaml`,
    alors que les appels eux-mêmes (agenda, prévisions) répondent en 10 ms environ. Le
    blueprint « Tab5 — emplacements » attendait aussi 1 s avant la clim et le volet.
  - Ces pauses dataient de juillet, quand une poussée faisait une vingtaine d'appels en
    boucle. Leur raison écrite (ne pas saturer le socket TCP de la tablette en même temps
    que le flux audio) n'avait jamais été mesurée, et le blueprint envoyait déjà 6 appels en
    60 ms en parallèle. L'ordre des envois est gardé par la séquence ; les payloads groupés
    restent découpés (la tablette refuse plus de 2048 octets). Documentation corrigée :
    README de `HomeAssistant_Config/`, `docs/architecture.md`, `docs/voice_assistant.md`.
  - Mesuré après déploiement sur le HA de l'auteur, 5 redémarrages par le bouton HA
    « Redémarrage Système » : toute la poussée part en 0,13 à 0,19 s après
    `tab5_connected` (6,1 s avant), et l'écran est complet 17,5 à 17,8 s après la coupure
    (24,5 s avant). Aucune nouvelle erreur dans le journal de HA, écran vérifié par
    l'auteur.

## [3.3.2] — 2026-10-01

De `v3.3.1` à aujourd'hui : les lots A à D de l'audit du 30/09 (#269 → #272), la clé de
publication dans un environnement protégé (#275), deux garde-fous et de la documentation
(#262, #265 → #267), plus la pull request de la release.
- **Firmware** : un nombre absurde venu de Home Assistant (`inf`, `1e30`) n'est plus
  converti en entier sans limite (#269) ; il s'affiche « -- ». C'est le seul changement du
  firmware, rien ne change pour des valeurs normales.
- **Home Assistant** : seule la tablette peut commander les tuiles et créer des
  notifications par ses événements (#271) ; l'alerte « fichiers HA en retard » ne compare
  plus que X.Y (#272).
- **Projet** : le contrat HA ↔ tablette est vérifié champ par champ, la tablette virtuelle
  passe sous ASan et UBSan à chaque PR, les actions de la CI sont figées par SHA, et la clé
  qui signe les firmwares publiés n'est lue que depuis `main` ou un tag, avec l'accord du
  mainteneur.

**Compatible dans les deux sens** : un firmware 3.3.1 avec les fichiers HA de la 3.3.2 passe
la nouvelle garde (elle lit le modèle de l'appareil, `tab5-ha-hmi` ; vérifié sur une
tablette en 3.3.1 le 01/10) ; un firmware 3.3.2 avec les fichiers de la 3.3.1 marche
comme avant, sans la garde.

### À faire en mettant à jour depuis 3.3.1

1. **Home Assistant** : remplacer par ceux de `tab5_home_assistant.zip` le blueprint
   `blueprints/automation/tab5/tab5_emplacements.yaml` et les packages `tab5_push.yaml`,
   `tab5_reveil.yaml`, `tab5_health.yaml` (et `tab5_evenements.yaml`, qui ne change que par
   ses commentaires), puis recharger les automatisations et les entités de modèle (ou
   redémarrer Home Assistant).
2. **Firmware** : entité « Firmware » dans Home Assistant.

### Mesures de la version

- Compilation locale du même code firmware (`main` @ `6fd0ac9` ; la release ne change
  ensuite que la version par défaut de `tab5-ha-hmi.yaml`) : image 3 331 682 o (41,0 % de la partition, +384 o), RAM statique
  171 702 o (inchangée). Ce build tourne sur la tablette de l'auteur depuis le 01/10 à
  8 h 30 (version lue par l'API), sans redémarrage inattendu.
- Job « Sanitizers (tablette virtuelle) » sur l'arbre final : 0 rapport ASan/UBSan (19
  services fuzzés, 8 cas ciblés, tous les écrans) ; sans le correctif du lot A, 6 rapports.
- Relance de la publication de la 3.3.1 avec la clé lue dans l'environnement protégé
  (ELF seulement) : empreinte, signature et même code que l'image publiée, sur les trois
  révisions d'écran.
- Compilations requises de la CI (dernière ESPHome et 2026.9.0) : vertes.

### Problèmes connus

Ceux de la 3.3.1.

### 2026-10-01 — Clé de publication dans un environnement protégé

- **Sécurité : la clé qui signe les firmwares publiés n'est plus lisible par n'importe quel
  workflow** (audit du 30/09, S2 ; rien ne change sur la tablette).
  - Le job `firmware` de `publication.yml` tourne dans l'environnement protégé `publication`,
    seul à garder le secret `TAB5_CLE_SIGNATURE` : il ne part que de `main` ou d'un tag `v*`
    et attend l'accord du mainteneur dans Actions (« Review deployments »),
    administrateurs compris. `tests/test_publication.py` échoue si un autre job ou un autre
    workflow lit la clé.
  - Réglages du dépôt du même jour : les tags `v*` ne peuvent plus être supprimés ni
    déplacés (règle « Tags de version immuables »), la protection de `main` s'applique aussi
    aux administrateurs, et l'analyse CodeQL par défaut de GitHub est active.

### 2026-10-01 — Audit du 30/09 : lots A à D

- **Sécurité : seule la tablette pilote ses tuiles, et la CI ne dépend plus d'un tag qui
  bouge** (audit du 30/09, §6, lot C ; rien ne change sur l'écran ni dans le firmware).
  - Home Assistant : un événement `esphome.*` n'exige aucune option, donc n'importe quel autre
    appareil ESPHome de la maison pouvait émettre `esphome.tab5_action` et commander toutes les
    tuiles du blueprint « Tab5 — emplacements », ou `esphome.tab5_journal` et créer des
    notifications. Le blueprint, `tab5_push.yaml` et `tab5_reveil.yaml` (`tab5_connected`) et
    `tab5_health.yaml` (`tab5_journal`) vérifient désormais, comme `tab5_evenements.yaml`, que
    l'appareil émetteur est une tablette (modèle `tab5-ha-hmi`) ; les autres déclencheurs ne
    changent pas. `tests/test_garde_origine.py` rend chaque garde et échoue si une
    automatisation écoutant un événement de la tablette n'en a pas. À redéployer dans HA
    (packages et blueprint).
  - CI : les 39 `uses:` des cinq workflows sont figés par le SHA complet de leur commit, le tag
    en commentaire (mêmes versions) ; l'image `esphome:latest` du canari reste voulue
    (ADR-0016). `esphome-tab5.yml` déclare `permissions: contents: read` (plus
    `pull-requests: read` pour le filtre de chemins). La publication installe esptool, qui
    tourne à côté de la clé de signature, depuis `tools/publication/requirements-esptool*.txt`
    (esptool 5.4.0 et toutes ses dépendances, versions exactes et empreintes,
    `--require-hashes`) au lieu de `esptool>=5` ; chaque PR rejoue cette installation sur
    Linux. `tests/test_ci_securite.py` tient ces trois règles.
- **Contrat Home Assistant ↔ tablette vérifié champ par champ, et deux correctifs** (audit du
  30/09/2026, lot D). Home Assistant refuse l'appel d'une action de la tablette qui a une variable
  manquante ou en trop, ou un nom inconnu (« Action … not found ») ; l'erreur n'est que dans son
  journal et arrête le script, donc les poussées suivantes ne partent pas. `tests/test_contrat.py`
  compare désormais les clés de chaque appel (packages, blueprint, snippets, rendu hors tablette)
  aux `variables:` de `Tab5/tab5-api-logic.yaml`, et chaque champ `trigger.event.data.*` lu pour
  un événement `esphome.tab5_*` à ceux que le firmware émet. `tools/demo/demo_pusher.py --dry-run`
  lit enfin ce même contrat (l'étape de la CI l'annonçait sans le faire) et échoue sur un écart ;
  sa garde refuse aussi une variable en trop. Correctifs : `binary_sensor.tab5_fichiers_ha_en_retard`
  ne compare plus que X.Y, une tablette en 3.3.1 avec des fichiers 3.3.0 n'est plus signalée (la
  3.3.1 ne demandait que le blueprint) ; la version par défaut d'un firmware compilé soi-même
  passe de 3.2.0-dev à 3.3.1-dev, et un test exige qu'elle suive la dernière version publiée.
  Documentation corrigée et tenue par `tests/test_doc_comptes.py` : 11 à 13 champs de vigilance
  (et non 11), 12 services appelés par la démo et 7 non (dont `tab5_maj_planning`), consommateurs
  de `tab5_connected` et `tab5_maj_ecran` dans la table des événements. Rien ne change sur l'écran.
  Côté HA, seul `tab5_health.yaml` change de comportement : à déployer.
- **Nombres absurdes venus de Home Assistant : plus aucune conversion hors bornes** (lot A de
  l'audit du 30/09). Une consigne de clim, une luminosité ou une humidité reçue en `inf` ou
  `1e30`, ou des bornes de clim en `-1e30`, étaient converties en entier sans limite :
  comportement indéfini relevé par UBSan sur la tablette virtuelle (6 endroits, dont un calcul
  qui débordait ensuite dans LVGL). Sur la tablette, la conversion saturait : pas de plantage,
  mais un arc faux. Toute valeur de HA convertie en entier passe maintenant par
  `tab5_float_vers_int` (bornée, troncature inchangée pour une valeur normale) ; `inf` est
  traité comme une valeur inconnue (« -- ») et des bornes de clim hors de −100…200 sont
  ignorées. Testé sur PC par `tools/test_alarm_clock.cpp`. Rien ne change pour des valeurs
  normales.
- **Nouveau job CI « Sanitizers (tablette virtuelle) », non requis** (lot B de l'audit du 30/09).
  La tablette virtuelle (`tab5-rendu-host.yaml`) est compilée avec AddressSanitizer et
  UndefinedBehaviorSanitizer (`tools/sanitizers/variante.py` + `pio_drapeaux.py`), puis les 19
  services de HA sont fuzzés (`fuzz_services.py`), les cas de conversions hors bornes rejoués
  fenêtre ouverte (`cibles_ub.py`) et tous les écrans ouverts. Le job échoue au premier rapport,
  lu dans le journal de la tablette (`rapports.py`) : UBSan ignore `log_path` et écrit sur la
  sortie d'erreur, ce qui avait fait conclure « 0 rapport » à tort au premier passage de l'audit.
  Un témoin positif (`temoin.cpp`) prouve à chaque run que la détection marche.
  Tests : `tests/test_sanitizers.py`.

### 2026-09-29 et 30 — Garde-fous et documentation

- **Deux garde-fous de plus, joués par `pytest` et la CI** (audit du 25/09, §7). Une valeur
  oubliée dans un niveau d'Arcanoïde ou une question supprimée de Trial Poursuite compilait sans
  erreur (le C++ complète avec des zéros, soit une question nulle). `tools/check_arkanoid_levels.py`
  vérifie les 8 niveaux (rangées complètes, valeurs connues, aucune brique emmurée par des
  indestructibles, `LEVELS` et fin de partie cohérents) ; `tools/check_trivia_questions.py` la
  banque de 720 questions (autant d'entrées que chaque `#define`, catégorie et difficulté valides,
  ni texte vide, ni leurre égal à la réponse, ni doublon). Les deux attrapent une erreur injectée
  (`tests/test_guards.py`). Rien ne change sur l'écran.
- **Documentation : schéma de la cartographie complet.** Le schéma Mermaid de
  `CARTOGRAPHIE_TAB5.md` n'avait pas de nœud pour `ecran-*.yaml`, `publication-*.yaml`,
  `tab5-tuiles.yaml` et `tab5-zones.yaml`, ni d'arête `packages:` vers l'arcade, le calendrier
  et l'assistant ; la pile vocale y restait rattachée à `tab5-hardware.yaml` (elle est dans
  `tab5-assist.yaml`). Il montre désormais tout le bloc `packages:` de l'entrée, dans son ordre.
  Il annonçait 40 `ui_components` (45), `docs/architecture.md` 23 inclus directs par
  `tab5-lvgl.yaml` (24) ; la phrase du README sur les fichiers de plus de 500 lignes ne
  donne plus de nombre, et leur liste est vérifiée. `docs/architecture.md` a une section, en anglais et en
  français, pour chaque package (sept manquaient), et celle de `tab5-scripts.yaml` ne lui prête
  plus les scripts partis dans l'arcade, le calendrier et l'assistant.
  `tests/test_doc_comptes.py` vérifie ces points.

## [3.3.1] — 2026-09-29

De `v3.3.0` à aujourd'hui : une pull request de fonction (#263) et trois de documentation
(#259 → #261), plus celle de la release.
- **Toutes les clims ont leur popup** (#263, [ADR-0027](../../docs/decisions/0027-climate-per-tile.md)) :
  une tuile de clim placée dans une pièce ouvre le popup pour sa propre clim (réglages,
  état, commandes), et plus seulement la clim du blueprint. La carte de l'accueil reste
  celle du blueprint. La clim du blueprint reçoit les mêmes commandes qu'en 3.3.0.
- **Documentation** : comptes remis au code et vérifiés par un test (#260), deux images
  fausses remplacées (#259), `CLAUDE.md` qui importe `AGENTS.md` (#261).

**Compatible dans les deux sens** : un firmware plus ancien ignore les clés `crRT` / `ceRT`
(la tuile de clim ne fait rien au toucher, comme avant) ; un firmware 3.3.1 avec le
blueprint de la 3.3.0 se comporte comme la 3.3.0.

### À faire en mettant à jour depuis 3.3.0

1. **Home Assistant** : seul le blueprint change. Remplacer
   `blueprints/automation/tab5/tab5_emplacements.yaml` par celui de
   `tab5_home_assistant.zip`, puis recharger les automatisations.
2. **Firmware** : entité « Firmware » dans Home Assistant.

### Mesures de la version

- Compilation locale du même code : image 3 331 298 o (41,0 % de la partition), RAM
  statique 171 702 o (−40 o) ; la table des clims de tuile (4 600 o) n'est prise en PSRAM
  que si une maison en a.
- Test d'installation dans un HA neuf : la clim `climate.heatpump` de l'intégration demo,
  placée dans la pièce 3, reçoit `cr24|7.0|35.0|0.5|°C|h|HeatPump` et son état.
- Compilations requises de la CI (dernière ESPHome et 2026.9.0) : vertes.

### Problèmes connus

Ceux de la 3.3.0, sauf le popup réservé à une seule clim, et : pour une clim de tuile, un
changement de ventilation, d'oscillation ou de préréglage fait hors de l'écran arrive
dans le popup au plus tard 5 minutes après.

### 2026-09-29 — Documentation remise au code

- **Documentation : comptes remis au code.** `docs/architecture.md` annonçait douze packages
  et en listait quinze : la vue d'ensemble ne donne plus de nombre, et la liste reprend les
  dix-neuf de `tab5-ha-hmi.yaml` (`tab5_ecran`, `tab5_publication`, `tab5_tuiles` et
  `tab5_zones` manquaient). La cartographie donnait 18 actions API (19, avec `tab5_maj_tuiles`),
  le README et le site 25 décisions d'architecture (26). `tests/test_doc_comptes.py` compare
  ces comptes au code.
- **Documentation : deux images fausses remplacées.** `gpio_pinout_table.png` (générée par IA
  en juillet 2026 : écran RGB parallèle 1024×600, 16 Mo de PSRAM, GPIO 26 pour BCLK et DOUT)
  laisse place à un tableau des broches dans `docs/hardware.md`, tiré du YAML et vérifié par
  `tests/test_doc_broches.py` ; le tableau audio, qui donnait BCLK sur GPIO 26 (c'est GPIO 27),
  est corrigé. `push_only_architecture_diagram.png` (LVGL 8.4 à 60 FPS, 6 écrans, Google
  Calendar, aucun événement) laisse place à un schéma SVG en français et en anglais :
  HA pousse par les actions `tab5_maj_*`, la tablette répond par des événements
  `esphome.tab5_*` (ADR-0025).

### 2026-09-29 — Toutes les clims ont leur popup

- **Chaque tuile de clim ouvre le popup pour SA clim** ([ADR-0027](../../docs/decisions/0027-climate-per-tile.md)).
  Jusqu'ici, seule la clim du blueprint (option `m`) ouvrait le popup ; une autre clim
  placée dans une pièce ne montrait que sa température. Le blueprint pousse désormais,
  avec les tuiles, les réglages de chaque clim de tuile (clé `crRT`, les champs de
  `climr`) et son état (clé `ceRT`, les champs de `tab5_maj_clim`) : bornes, pas, unité,
  boutons gérés, nom en titre, consigne, modes. Ses boutons envoient les mêmes commandes
  avec `emplacement: tRT`, traduites par les mêmes branches « Clim : … » que la clim du
  blueprint (une seule traduction, sur l'entité de la tuile).
- **Deux états séparés** : la carte − / consigne / + de l'accueil reste la clim du
  blueprint, même quand le popup montre une autre clim ; un retour de HA pour l'une ne
  touche jamais l'autre. Fermer le popup (croix, voile) revient à la clim du blueprint.
  La coloration du popup est passée du YAML au C++ (`clim_recolorer()`).
- **Rythme** : consigne et mode d'une clim de tuile arrivent tout de suite (nouveau
  déclencheur par pièce sur l'attribut `temperature`) ; ventilation, oscillation,
  préréglage et température de la pièce avec les mesures (5 minutes), pour ne pas
  réveiller l'automatisation à chaque dixième de degré.
- **Compatible dans les deux sens** : un firmware plus ancien ignore `crRT` / `ceRT` (la
  tuile ne fait rien, comme avant) ; sans ces clés (blueprint plus ancien), rien ne change.
  La clim du blueprint reçoit exactement les mêmes commandes et poussées (ancien et
  nouveau blueprint rendus : 2 080 commandes et 400 poussées, 0 écart). Test « HA neuf » :
  la clim `climate.heatpump` de l'intégration demo dans la pièce 3.

## [3.3.0] — 2026-09-29

De `v3.2.2` à aujourd'hui : six pull requests (#252 → #257), plus celle de la release.
L'écran et Home Assistant s'adaptent à d'autres maisons que celle de l'auteur.
- **Clim de toutes marques** (#257, [ADR-0026](../../docs/decisions/0026-climate-from-device.md)) :
  bornes, pas, °C ou °F, boutons et nom viennent de l'appareil ; les commandes sont
  traduites vers ses vrais modes. La Daikin de l'auteur reçoit les mêmes commandes qu'avant.
- **Home Assistant** (#256) : mot des événements de travail, vacances scolaires prises
  dans un agenda (la table de la zone A disparaît), pipeline du mode Discussion au choix
  (boutons Domo / Discu masqués sans pipeline, #257), briefing du réveil dans la langue
  de l'écran, listes et blueprint en français et en anglais, alerte quand les fichiers HA
  sont plus anciens que le firmware.
- **Météo hors de France** : vigilances DWD et CAP Alerts (#253, #255), pluie dans l'heure
  sans clé par Buienradar, DWD, Met.no ou Open-Meteo (#254).
- **Flipper** : plus de rafales d'avertissements LVGL « X/Y is … greater than res » après
  la bascule d'orientation (#252).

**Version mineure, compatible dans les deux sens** : le firmware 3.3.0 marche avec les
fichiers HA de la 3.2 (popup clim et boutons vocaux comme avant), et les fichiers HA de
la 3.3.0 avec un firmware 3.2 (réglages de la clim et zone « discussion » ignorés).

### À faire en mettant à jour depuis 3.2

1. **Home Assistant d'abord** : remplacer les fichiers par ceux de
   `tab5_home_assistant.zip`, puis **avant de recharger** les modèles, les scripts et les
   automatisations :
   - agenda de travail qui contient aussi d'autres événements : taper son mot dans « Tab5 ·
     mot des événements de travail » (`Travail` pour garder le comportement d'avant ;
     vide, tout l'agenda compte comme du travail) ;
   - choisir un agenda dans « Tab5 · agenda des vacances scolaires » (en France, le fichier
     ICS du ministère par l'intégration Remote Calendar, `docs/installation.md`) ;
   - choisir son pipeline dans « Tab5 · pipeline de discussion » (avant : un pipeline
     nommé exactement « Discussion LLM »).
2. **Firmware** : entité « Firmware » dans Home Assistant.

### Mesures de la version

- Tablette de l'auteur (ST7123) : même code que ce tag (hors numéro de version et
  documentation) depuis le 29/09 19:25 ; fichiers HA déployés à 19:10 (blueprint à 19:27).
  La tablette a reçu les réglages de sa clim (`18.0-32.0, pas 0.50, C, [chdfebqsw]`).
- Compilation locale du même code : image 3 321 506 o (40,9 % de la partition), RAM
  statique 171 742 o.
- Compilations requises de la CI, installation dans un HA neuf et rendu des écrans
  (« identique aux références », six langues) : verts.

### Problèmes connus

Ceux de la 3.2.2, et :
- boutons Domo / Discu masqués : leur place reste vide à l'accueil et dans le popup
  assistant ;
- le briefing du réveil n'est relu qu'en français ; il n'a pas encore été entendu en vrai
  sur la 3.3.0 ;
- une seule clim a son popup (celle du blueprint) ; les autres tuiles de clim affichent
  leur température.

### 2026-09-29 — Clim de toutes marques, mode Discussion sans pipeline masqué

- **Le popup clim suit l'appareil** ([ADR-0026](../../docs/decisions/0026-climate-from-device.md)).
  Le blueprint envoie ses réglages dans `tab5_maj_emplacements` (nouvelle clé `climr`,
  avant `tab5_maj_clim`) : bornes, pas, °C ou °F (l'unité de l'entité météo de HA), modes
  gérés et nom. L'arc et les boutons − / + suivent les bornes et le pas, le titre devient
  le nom de la clim, un bouton que l'appareil ne sait pas faire disparaît, et une section
  OPTIONS sans bouton disparaît avec son titre (les autres remontent).
- **Commandes traduites par le blueprint** : l'écran envoie toujours les noms de la Daikin
  (Éco = `away`, Silence = `quiet`, `swing` / `stop`, `windnice`) ; le blueprint envoie
  l'équivalent que l'appareil connaît (`eco`, `low`, `off`, `vertical`…) ou rien. Une
  consigne est bornée aux limites de l'appareil ; clim éteinte, il l'allume en froid, à
  défaut en chaud, chaud/froid ou auto (un chauffage seul prend enfin une consigne).
- **Bascules corrigées** : Silence, Oscillation et Éco reconnaissent leur état actif sous
  tous ses noms (`low`, `on`, `eco`…) ; un appareil dans l'un de ces modes ne pouvait plus
  en sortir depuis l'écran. Un mode sans bouton (chaud/froid, auto) n'allume plus « Éteint ».
- **Zone `discussion`** : quand la liste « Tab5 · pipeline de discussion » vaut « Aucun »,
  les boutons Domo / Discu (accueil et popup assistant) disparaissent et la tablette
  repasse en Domotique. Sans la liste (package plus ancien), rien ne change. Le blueprint
  renvoie les zones dès que la liste change.
- **Calendrier** : dans le détail d'un jour, « Travail » suit la langue de l'écran.
- **Compatible dans les deux sens** : un firmware plus ancien ignore `climr` ; sans
  `climr` (blueprint plus ancien), le popup reste tel qu'avant (16-30 °C, pas de 0,5, tous
  les boutons). La Daikin de l'auteur reçoit exactement les mêmes commandes
  (`tests/test_clim.py`) ; son arc passe à ses bornes (18-32) et son titre à son nom.

### 2026-09-29 — Home Assistant : la maison des autres

Côté Home Assistant seulement (aucun flash). Ce qui restait réglé pour la maison de
l'auteur se choisit désormais dans Home Assistant.

- **Mot des événements de travail** : le texte « Tab5 · mot des événements de travail »
  dit quels événements de l'agenda de travail sont des postes (mots séparés par des
  virgules, sans casse). **Vide = tous** : un agenda qui ne contient que ses postes marche
  sans rien taper. Avant, seul un titre contenant « Travail » comptait (planning, heure du
  réveil, jours de repos). **Mise à jour : l'auteur, dont l'agenda de travail porte aussi
  ses rendez-vous, tape `Travail`.**
- **Vacances scolaires** : la liste « Tab5 · agenda des vacances scolaires » remplace la
  table fixe de la zone A (Bordeaux), montrée à tout le monde et arrêtée à l'été 2027. En
  France, le fichier ICS du ministère pour sa zone, par l'intégration *Remote Calendar*
  (vérifié le 29/09 : jusqu'à l'été 2028). Un seul agenda dont le nom le dit est choisi
  tout seul, et il n'est jamais pris pour un agenda de jours fériés.
- **Pipeline de discussion** : la liste « Tab5 · pipeline de discussion » choisit le
  pipeline du mode « Discu » parmi ceux de Home Assistant. Avant, il devait s'appeler
  exactement « Discussion LLM ». « Aucun » ramène au pipeline préféré.
- **Briefing du réveil** dans la langue de l'écran (français, anglais, allemand,
  néerlandais, espagnol, italien) ; le texte français ne change pas d'un mot.
- **Noms en deux langues** : les listes « Tab5 · … » (« Tab5 · agenda de travail · work
  calendar ») et tous les libellés du blueprint. Les listes gardent leur entity_id
  (`default_entity_id`, HA 2026.8), les entrées du blueprint leurs clés.
- **Fichiers HA plus anciens que le firmware** : l'archive `tab5_home_assistant.zip`
  porte sa version (« Tab5 · version des fichiers HA ») ; quand la tablette tourne une
  release plus récente, « Tab5 · fichiers HA en retard » s'allume et une notification
  persistante le dit (garde (f) de `tab5_health.yaml`).
- Essais : le mot du travail, la détection des agendas scolaires, la comparaison de
  versions, le briefing dans les 6 langues et les codes et le détail du calendrier rendus
  dans le moteur de Home Assistant 2026.9.4 de l'auteur, avec de faux événements ; la
  liste des pipelines lue sur la vraie tablette.

### 2026-09-29 — Pluie dans l'heure hors de France, sans clé

- « Tab5 · source de la pluie dans l'heure » propose aussi quatre services **sans clé et
  sans rien à installer**, interrogés par Home Assistant (`rest_command.tab5_pluie`)
  toutes les 5 min, seulement quand ils sont choisis :
  - radar **Buienradar** : Pays-Bas, Belgique ;
  - radar du **DWD** par Bright Sky : Allemagne et pays voisins ;
  - radar **Met.no** Nowcast : pays nordiques ;
  - **Open-Meteo** : partout, mais un modèle au pas de 15 min.
- Les coordonnées du domicile sont arrondies à 0,01° (environ 1 km) et envoyées au seul
  service choisi. Hors de sa zone, un service donne « pas de données ».
- Toutes les sources à la minute ou au radar, OpenWeatherMap compris, passent par une
  même série (début, durée, mm/h). OpenWeatherMap donne les mêmes codes qu'avant
  (vérifié sur sa réponse réelle et sur une averse simulée).
- Essais : les quatre services interrogés le 29/09 (jour sec) et leurs réponses lues par
  les modèles dans le moteur de Home Assistant 2026.9.4, pluie simulée, hors zone, erreur
  HTTP, panne réseau : 20 cas. La CI d'installation à neuf interroge Open-Meteo.

### 2026-09-29 — Vigilances hors de France : DWD et CAP Alerts

- « Tab5 · source des vigilances » propose aussi **DWD** (Allemagne, intégration
  *DWD Weather Warnings* fournie avec Home Assistant) et **CAP Alerts** (intégration HACS
  `seevee/cap_alerts` : MeteoAlarm avec toutes les alertes de la région, NWS, Environnement
  Canada, une centaine de services nationaux par l'OMM). Contrairement à MeteoAlarm dans
  Home Assistant, les deux donnent **toutes** les alertes en cours.
- Une alerte compte si elle est en cours ou commence dans les 24 h. Le niveau global est
  la plus forte. La case vient du code du DWD, du type de phénomène MeteoAlarm ou de
  l'icône que CAP Alerts donne à l'alerte. Les préavis du DWD, les séismes GDACS et les
  alertes d'essai sont ignorés.
- Nouveau fichier `custom_templates/tab5_vigilance.jinja` (macro lue par ces deux sources
  seulement : Météo-France et MeteoAlarm marchent sans lui).
- Essais : le DWD ajouté au Home Assistant de l'auteur (Berlin, jour sans alerte), entités
  trouvées alors que leurs identifiants sont en français (`…_niveau_d_alerte_actuel`) ;
  alertes simulées rendues dans le moteur de Home Assistant 2026.9.4 (11 cas) et dans la
  CI d'installation à neuf, qui essaie maintenant chaque source de vigilances.

## [3.2.2] — 2026-09-29

De `v3.2.1` à aujourd'hui : une pull request (#250), plus celle de la release.
- **Météo** : changer de fournisseur (prévisions, pluie dans l'heure, vigilances) a été
  essayé avec les vraies données de Météo-France, d'OpenWeatherMap et de MeteoAlarm.
  Aucun défaut de format ; trois défauts corrigés (#250) : une averse finie depuis
  quelques minutes restait « en cours » avec OpenWeatherMap ; les tuiles attendaient
  jusqu'à 10 min après un changement de source des prévisions ; la condition
  `exceptional` (fumée, poussière, sable chez OpenWeatherMap) s'affichait comme un nuage.

**Version corrective** : aucune nouvelle fonction, rien d'incompatible. Le firmware 3.2.2
marche avec les fichiers HA de la 3.2.1, et inversement.

### À faire en mettant à jour depuis 3.2.1

1. **Firmware** : entité « Firmware » dans Home Assistant (icône `exceptional`).
2. **Home Assistant, facultatif** (pluie OpenWeatherMap, changement de source) : remplacer
   `packages/tab5_meteo_sources.yaml` et `packages/tab5_push.yaml` par ceux de
   `tab5_home_assistant.zip`, recharger les modèles et les automatisations.

### Mesures de la version

- Tablette de l'auteur (ST7123) : même code que ce tag (hors numéro de version et
  documentation) depuis le 29/09 12:12 ; fichiers HA déployés depuis 12:06.
- Compilations requises de la CI et installation dans un HA neuf : vertes.

### Problèmes connus

Ceux de la 3.2.0, et une limite de MeteoAlarm : la bibliothèque de Home Assistant ne lit
que la première alerte de la zone, même expirée.

### 2026-09-29 — Fournisseurs météo essayés avec leurs vraies données

- Demande d'Axel : vérifier qu'un autre fournisseur donne des données que la tablette sait
  lire. Les modèles du dépôt ont été rendus dans le moteur de Home Assistant 2026.9.4 avec
  les réponses réelles de Météo-France et d'OpenWeatherMap, les alertes MeteoAlarm du jour
  (lues par `meteoalertapi` 0.3.1) et des cas simulés (pluie à venir, en cours, très forte,
  réponse vide, entité façon NWS), puis décodés comme sur la tablette : aucun défaut de
  format sur 30 cas.
- Pluie OpenWeatherMap : la série vient du cache de l'intégration, rafraîchi toutes les
  10 min ; les minutes déjà passées sont ignorées.
- Changer « Tab5 · source des prévisions » relance la poussée complète.
- `exceptional` prend l'icône du brouillard (glyphe déjà dans la police).

## [3.2.1] — 2026-09-29

De `v3.2.0` à aujourd'hui : 14 pull requests (#234 → #248, sans #242, mise à jour des
actions GitHub), plus celle de la release.
- **Carte centrale** : la rotation ne redessine plus que le texte (−13 à −20 % par
  rotation, #244) ; sa logique est simplifiée (#237) ; trois défauts corrigés (#243) : tap
  pendant une réponse vocale ou une rotation, fin de la pluie qui restait à l'écran.
- **Écran** : tout l'affichage pouvait rester décalé après un swipe des prévisions ; le
  calendrier est centré en hauteur et plus lisible ; « Aucun travail de prévu » a son
  accent (#245).
- **Boutons** : les transitions du thème LVGL (80 ms à l'appui) sont enfin coupées (#235).
- **Jeux** : la raquette d'Arcanoïde ne repart plus dans l'ancien sens (#238) ; l'élan de
  Fil d'Or part d'une secousse, plus d'une inclinaison (#240).
- **Accéléromètre** : il ne sert plus qu'aux jeux ; trois capteurs de moins dans HA
  (« Tab5 Pitch », « Tab5 Roll », « Tab5 IMU Temperature », #239).
- **Home Assistant** : tout est repoussé au démarrage de HA, même si la tablette s'est
  reconnectée avant les automatisations (#234).
- **Docs** : performances mesurées (#236, #241), vérifications pour les testeurs ST7121
  (#246).

**Version corrective** : aucune nouvelle fonction, rien d'incompatible. Le firmware 3.2.1
marche avec les fichiers HA de la 3.2.0, et inversement.

### À faire en mettant à jour depuis 3.2.0

1. **Firmware** : entité « Firmware » dans Home Assistant.
2. **Home Assistant, facultatif** (pour le démarrage de HA, #234) : remplacer
   `packages/tab5_push.yaml` par celui de `tab5_home_assistant.zip`, ré-importer le
   blueprint « Tab5 — emplacements », recharger les automatisations.
3. Les trois capteurs de position (« Tab5 Pitch », « Tab5 Roll », « Tab5 IMU
   Temperature ») deviennent indisponibles : les retirer des tableaux de bord qui les
   affichent.

### Mesures de la version

- Carte centrale, même tablette, même matinée : 107,8 → 86,4-93,4 ms par rotation
  (`docs/performance.md`).
- Tablette de l'auteur (ST7123) : même code que ce tag (hors numéro de version et
  documentation) depuis le 29/09 10:43, essayé par l'auteur (swipe, calendrier, carte
  centrale). Rendu hors tablette : calendrier vérifié (juin 2026).
- Compilations requises de la CI (dernière ESPHome et 2026.9.0) : vertes.

### Problèmes connus

Ceux de la 3.2.0.

### 2026-09-29 — Écran décalé au swipe, calendrier, « prévu »

- Retour d'Axel, photos à l'appui : passer des prévisions par heure à l'accueil pouvait
  décaler tout l'écran vers la droite. Pendant un swipe, le calque entrant part à ±110 px
  (`animate_swipe_horizontal()`) : `page_main` devenait défilable et le doigt encore posé
  la faisait glisser ; le décalage restait ensuite. `page_main` n'est plus défilable
  (`scrollable: false`) ; le geste n'en dépend pas.
- Calendrier : autant de lignes que de semaines dans le mois (4 à 6), réparties sur toute
  la hauteur et centrées (`cal_grid_h`) ; numéros centrés sous le nom du jour (74 px
  d'écart avant) ; chaque jour sur un fond de verre, plus pâle s'il est passé ; jours
  passés en gris clair au lieu de l'ardoise ; noms des jours de semaine en blanc.
- « Aucun travail de prévu » : l'accent manquait dans la clé française et ses traductions.

### 2026-09-29 — Carte centrale : la rotation ne redessine plus que le texte

- Demande d'Axel : essayer de ne redessiner que le texte. Chaque panneau du rotateur fait
  toute la largeur de la carte (bouton invisible de 1180 px) ; c'est maintenant son
  contenu qui glisse (`translate_y`), pas le panneau. Même animation (190 ms, 28 px).
- Mesuré sur la tablette, même matinée : 107,8 → 86,4-93,4 ms par rotation (−13 à −20 %
  selon la largeur du texte), 604 000 → 447 000-475 000 pixels. Détail dans
  `docs/performance.md`.
- Le texte des bandeaux d'alertes HA sort du bouton (premier enfant du panneau, comme
  ailleurs) ; plus de barre de défilement quand le texte déborde en glissant.

### 2026-09-29 — Carte centrale : trois défauts corrigés

Relevés en cartographiant la carte centrale (#237), corrigés à la demande d'Axel :
- **Tap sur une température pendant une réponse vocale** : le planning du jour
  s'affichait par-dessus la réponse, les deux textes superposés. Le tap termine
  maintenant la réponse vocale, comme un changement de page.
- **Tap sur une température pendant les 190 ms d'une rotation** : les deux panneaux
  restaient figés à mi-course (décalés, à demi transparents), jusqu'à 6 s pour le
  planning. Couper une animation remet maintenant le panneau à sa place, opaque
  (`couper_animation()`, `lv_anim_delete()` ne pose pas la valeur finale).
- **Fin de la pluie ou de la vigilance** : le panneau restait à l'écran, barres vides
  ou sans icône, jusqu'au tour suivant (≤ 8 s). Il cède maintenant la place tout de
  suite, avec la transition habituelle ; et une pluie qui commence sur une carte vide
  s'affiche sans attendre (`central_set_pluie()`, `central_set_vigilance()`).

### 2026-09-29 — Carte centrale : logique simplifiée, rien ne change à l'écran

Refactor demandé par Axel (« on laisse l'anim comme ça […] la logique de gestion est
complexe à force ») : même animation (`transition_widgets()`, 190 ms, non touchée), même
période (8 s), mêmes règles de priorité entre rotateur, titre de page, titre de pièce,
planning du tap et réponse vocale.
- **Deux globals miroirs retirés** : `is_showing_temp_planning` recopiait le timer de 6 s
  du planning du tap (les scripts lisent maintenant `temp_planning_active()`), et
  `forecast_page_index` recopiait `g_central_ctx.forecast_page`, écrit juste avant par le
  swipe, le retour automatique et le mode HA. `handle_swipe_gesture()`,
  `reset_forecast_to_main_page()` et `show_temporary_planning()` perdent leurs paramètres
  de page ; `show_temporary_planning()` reçoit la tuile et calcule le jour elle-même.
- **Code recopié nommé une fois** (`tab5_central.cpp`) : `liberer_carte()` (changement de
  page ou de mode HA), `prendre_carte()` (planning du tap, réponse vocale),
  `retirer_panneau()` (acquittement au tap d'une info ou d'une alerte HA) ;
  `update_info_text_ui()` reçoit le contexte ; la couleur du bandeau info reprend celle
  des alertes HA. En tête du fichier, la liste des occupants de la carte.
- Bilan : −33 lignes de code (hors commentaires), deux globals, cinq paramètres et un
  pointeur de `TuilesUI` en moins. L'`on_boot` n'est pas touché.
- Cas limites relevés, **pas corrigés** (ce serait changer ce qu'on voit) : un tap sur
  une tuile pendant la réponse vocale superpose le planning et la réponse ; un tap sur
  une température pendant les 190 ms d'une rotation fige les deux panneaux à mi-course
  (visible 6 s si l'un est le planning, ou après un aller-retour de pages jusqu'au tour
  suivant) ; la fin de la pluie ou de la vigilance ne retire leur panneau qu'au tour
  suivant du rotateur (jusqu'à 8 s).
- `docs/screens.md` (EN/FR) : le rotateur est le script `tab5_central_rotator_auto`
  (pas un `interval:` de `tab5-globals.yaml`), jusqu'à huit panneaux, planning absent
  sans agenda de travail, un seul script d'acquittement des alertes HA.
### 2026-09-29 — Fil d'Or : l'élan part d'une secousse, plus tout seul

- Retour d'Axel : « la bille saute de temps en temps toute seule ». C'était l'élan (dash) :
  il partait dès qu'on penchait à plus de ~38° (0,62 g), sans que le jeu le dise.
- Il part maintenant d'une **secousse brève** (passe-haut sur l'écart à la calibration,
  seuil 0,35 g, comme le coup de hanche du flipper), dans le sens où l'on penche, à défaut
  dans le sens où roule la bille. Recharge de 0,9 s et bonus « dash » inchangés. Le filtre
  est amorcé à l'ouverture : une tablette déjà penchée ne donne pas d'élan fantôme.
- `docs/arcade.md` décrit l'élan.

### 2026-09-29 — Accéléromètre : la position ne sert plus qu'aux jeux

- Demande d'Axel : la tablette ne change jamais de sens, la position ne sert qu'aux jeux.
  « Tab5 Pitch », « Tab5 Roll » et « Tab5 IMU Temperature » ne sont plus envoyés à HA
  (≈ 120 lignes par heure en moins dans la base) ; leurs trois cartes du tableau de bord
  de l'auteur sont retirées.
- L'IMU n'est plus lue du tout écran allumé hors jeu (1 fois par seconde avant, pour ces
  seuls capteurs). Inchangé : 10 Hz écran éteint pour le réveil par une tape, 10 Hz ou
  30 Hz jeu ouvert.

### 2026-09-29 — Docs : où part le temps d'une image, essai du dessin sur deux cœurs

- `docs/performance.md` (EN/FR) : part d'envoi (rotation + copie, ~59 ms fixes pour un
  écran entier) et part de dessin, mesurées image par image ; essai du dessin LVGL sur les
  deux cœurs (4 à 11 % de gain, pas retenu) ; `runtime_stats` : au repos la boucle
  n'attend que LVGL (28 ms, un pas du panneau tournant, que la pluie fait tourner).

### 2026-09-29 — Arcanoïde : la raquette ne repart plus dans l'ancien sens

- Retour d'Axel : en changeant de sens, la raquette partait d'abord dans l'ancien
  sens. En mode « Mix » (défaut), sa vitesse restait en mémoire quand on relâchait
  (la raquette s'arrêtait, pas sa vitesse) et resservait au prochain appui ; et, sans
  relâcher, l'inertie la gardait ~7 images (≈ 80 px à pleine vitesse) dans l'ancien sens.
- La vitesse est remise à zéro sans commande en mode « Mix », et annulée dès qu'une
  commande va dans l'autre sens (`arkanoid_game.cpp`, déplacement de la raquette).
  L'inclinaison garde son inertie tant qu'on va dans le même sens.

### 2026-09-28 (nuit) — Boutons : les transitions du thème LVGL enfin coupées

- Le 26/09 (audit ressources, lot 6), `CONFIG_LV_THEME_DEFAULT_TRANSITION_TIME: "0"` avait
  été posé dans le sdkconfig pour des boutons instantanés. Il n'a jamais agi : ESPHome
  compile LVGL avec `-DLV_KCONFIG_IGNORE`, qui ignore tous les `CONFIG_LV_*`. Le binaire
  de la 3.2.0 animait toujours chaque appui en 80 ms et chaque relâchement en 80 ms après
  70 ms de délai (`lv_theme_default_init`, vu au désassemblage).
- La macro passe maintenant par `esphome: build_flags` (`-DLV_THEME_DEFAULT_TRANSITION_TIME=0`,
  absente des `LV_DEFINES` d'ESPHome, donc pas écrasée) ; la ligne du sdkconfig est
  retirée, commentaires et `docs/troubleshooting.md` corrigés.
- Trouvé en analysant les PR d'ESPHome sur la rapidité d'affichage du P4 (esphome#16853,
  #16863).
- Vérifié le 28/09 : `lv_theme_default_init` n'appelle plus `lv_style_transition_dsc_init`
  (0 appel dans tout le binaire, 2 avant). Build de mesure sur la tablette : repos
  20,2-20,3 ms, écran rallumé 133,3 ms, calendrier 126,1 ms, comme la 3.2.0 (ouvertures
  commandées depuis HA, sans appui : le gain se voit au doigt, pas dans ces chiffres).

### 2026-09-28 (nuit) — Docs : performances mesurées, chiffres faux corrigés

- **`docs/performance.md`** (nouvelle, EN/FR) : mesures de la 3.2.0 sur la tablette le
  28/09 — image la plus longue au repos 16,7-20,4 ms, écran entier 133 ms, popups
  126-197 ms, reconnexion à HA 16,4-17,0 s après un redémarrage, RAM interne libre
  286,5-287,3 Ko —, la méthode (capteur Draw Max, actions depuis HA au milieu d'une
  minute) et les limites. Le coût au repos vient du panneau tournant de la carte centrale
  (bandeau de 1180 × 86 px redessiné 6-7 fois toutes les 8 s), vu avec un build de
  diagnostic local ; un firmware du 27/09 mesuré le même soir donne les mêmes 20 ms.
- Plus de « 60 FPS » (README, site, `ui_design.md`, kit Hackster) : les chiffres mesurés
  à la place. LVGL rafraîchit jusqu'à 60 fois par seconde, mais un redessin complet prend
  133 ms.
- Corrigés : 25 ADR (22), 19 fichiers YAML par domaine (15), 360 MHz (400 : la puce du
  Tab5 est en révision v1.3), 32 Mo de PSRAM (16) ; description du dépôt GitHub :
  6 langues (4).

### 2026-09-28 (soir) — HA : tout repousser au démarrage de Home Assistant

Constaté en passant HA Core de 2026.9.3 à 2026.9.4 : la tablette, restée allumée, s'est
reconnectée **avant** que les automatisations soient actives (entités revenues à
21:11:12, `esphome.tab5_connected` vers 21:11:14, automatisations actives à 21:11:16).
L'événement était perdu : le blueprint « Tab5 — emplacements » n'a rien poussé jusqu'au
rechargement manuel des automatisations (21:13:21), et un appareil changé pendant la
coupure restait faux à l'écran jusqu'à son prochain changement. Côté HA seulement, sans
flash :
- **Blueprint** : déclencheur `homeassistant` / `start` (`demarrage_ha`), qui rejoue la
  connexion perdue : définitions des pièces, tous les états, clim, volet. Pas les zones :
  la tablette ne les demande qu'avec la première poussée des prévisions, qui vient d'une
  automatisation active. Tablette pas encore reconnectée : la garde « tablette
  connectée » arrête, et son `tab5_connected`, émis plus tard, est entendu.
- **Poussée complète** (`packages/tab5_push.yaml`) : même déclencheur. Prévisions et
  pluie repartaient au passage des 10 minutes, mais la météo actuelle, les probabilités
  et le volet restaient ceux d'avant la coupure. La garde « liaison `on` depuis moins de
  3 min » ne regarde que `tab5_connected` : elle laisse passer le démarrage.
- Tests : le démarrage suit le chemin de la connexion dans le blueprint (rendu des
  modèles, chaque garde des actions comparée) et dans la poussée complète ; la garde des
  3 min, rendue, bloque un `tab5_connected` réémis mais pas le démarrage.
- `CARTOGRAPHIE_TAB5.md` : `tab5_push.yaml` n'a plus `tab5_push_clim` ni les scripts
  `allumer_leds` / `allumer_pc_tv` (retirés par #193).

Mise à jour : remplacer `packages/tab5_push.yaml` et ré-importer le blueprint.

## [3.2.0] — 2026-09-28

De `v3.1.0` à aujourd'hui : 8 pull requests (#219 → #223, #230 → #232 ; #223 regroupe
#224 → #229), plus celle de la release.
- **Pièces** (ADR-0023) : jusqu'à 5 pièces de 5 appareils, une par page du bas, choisies
  dans le blueprint ; noms, icônes et couleurs venus de HA. En mode HA, le glisser passe
  de pièce en pièce ; en mode météo, chaque tuile montre l'appareil de sa page dans ses
  épaules. Une lampe à variateur affiche sa luminosité en %.
- **Home Assistant sans placeholder** (ADR-0024) : une archive `tab5_home_assistant.zip`
  jointe à la release, une ligne de YAML, les valeurs de la maison choisies dans les
  listes « Tab5 · … ».
- **Plus d'option « actions HA » à cocher** (ADR-0025) : la tablette envoie des
  événements, que `tab5_evenements.yaml` traduit en une liste fixe d'actions.
- **Corrigés** : le popup calendrier n'obtenait qu'une demande de mois sur trois ; la
  pause du volet retenait toute commande pendant la course simulée (26 s).
- **Site** : la page d'accueil raconte le projet (#219) ; un canal ne sert qu'une release
  dont les fichiers sont joints (#221, #222) ; plus de faux succès de déploiement (#231,
  #232).

**Version mineure** : nouvelles fonctions, compatibles dans les deux sens. Un firmware
3.2 avec l'ancien blueprint garde l'accueil de la 3.1 ; un blueprint 3.2 avec un
firmware 3.1 ne pousse que les clés 3.x. Les fichiers HA changent (ci-dessous).

### À faire en mettant à jour depuis 3.1.0

Dans cet ordre (détail dans « Passer d'une 3.1 à la suite » de `docs/installation.md`) :
1. **HA d'abord** : remplacer les fichiers par ceux de `tab5_home_assistant.zip`, puis
   régler les listes « Tab5 · … » sur les anciennes valeurs des placeholders, et
   « Tab5 · agenda de travail » **avant** de recharger les automatisations (sinon tous
   les jours comptent comme des jours de repos, et le réveil suit).
   `volet_serre_tracking.yaml` vient maintenant de `tab5_optionnel/` : le garder dans
   `packages/` seulement si on s'en sert.
2. **Puis le firmware** (entité « Firmware »).
3. **Puis décocher** « Autoriser l'appareil à effectuer des actions Home Assistant »
   (*ESPHome → Configurer*).
4. **Ré-importer le blueprint** quand on veut, pour les pièces. La pièce 1 laissée vide
   garde l'accueil 3.x (PC ou TV, volet, trois lumières).
5. La ligne `tab5_tv_app_url` de `secrets.yaml` peut partir, une fois son IP reportée
   dans « Tab5 · adresse de la TV ».

### Mesures de la version

- Compilations de publication (ESPHome 2026.9.0, ST7123), `v3.1.0` contre
  `v3.2.0-rc.1`, même firmware que ce tag hors numéro de version : image
  3 280 330 → 3 358 098 o (+78 Ko : pièces, 80 glyphes d'icônes, textes), RAM statique
  171 526 → 172 430 o (+904 o). Aucun avertissement de compilation dans notre code.
- Rendu hors tablette : six langues, galerie mise à jour (#230).
- Test « installation dans un HA neuf » : vert, avec deux pièces (définitions relues
  dans la trace, rien envoyé au protocole 1).
- Tablette de l'auteur (ST7123) : 3.2.0-dev depuis le 28/09 17:11, rc.1 installée à
  19:10 (capture série propre, entité « Firmware » revenue), essayée le soir même.

### Problèmes connus

Ceux de la 3.1.0. En plus :
- les nouveaux textes en allemand, néerlandais, espagnol et italien sont traduits par
  une IA, pas encore relus ;
- un autre appareil ESPHome déjà ajouté à HA pourrait envoyer les événements de la
  tablette (écrit dans l'ADR-0025).

### 2026-09-28 (soir) — Site : plus de faux succès sur un commit déjà déployé

- La pré-release v3.2.0-rc.1 visait `de1cd42`, que le push de #230 sur `main` venait de
  déployer (galerie du rendu, dans `docs/images/`). Son déploiement s'est dit réussi,
  mais le site servi est resté celui du push : le canal bêta proposait encore 3.1.0, et
  la page d'installation aussi.
- Cause : GitHub Pages garde un déploiement par commit (`actions/deploy-pages` donne le
  commit comme `pages_build_version`). #231 donnait une version par run
  (`<commit>-<run>-<tentative>`) : l'API la refuse (404), et le déploiement depuis
  `main` a échoué ; il revient à `actions/deploy-pages`.
- `site.yml` échoue maintenant avant de déployer un commit qui a déjà un déploiement
  réussi, avec la marche à suivre : publier la release sur un nouveau commit, ou
  déployer depuis un nouveau commit de `main`. Le merge de ce correctif est un nouveau
  commit qui touche le site : il redéploie le site, avec la rc.1 sur le canal bêta.
- Test `tests/test_publication.py` ; ADR-0022, une ligne dans l'amendement du 28/09.

### 2026-09-28 (soir) — Pièces : retours d'Axel sur la tablette

- **Volet** : la pause remarche (le blueprint lançait le script du volet à course simulée
  et attendait sa fin, 26 s, en retenant toute commande suivante) ; le sens se choisit de
  nouveau d'un toucher sur le titre de la tuile, flèche comprise, comme en 3.1, sur toutes
  les tuiles volet et aussi en mode HA (la ligne d'état dit « Ouvrir » / « Fermer »).
- **Mode HA** : le glisser passe par les cinq pages, pièces vides comprises (« Aucun
  appareil ») ; le bouton « HA » est entouré de bleu quand le mode est actif, comme le
  bouton « Domo », et son icône garde la couleur de la connexion à HA.
- Blueprint : nom de pièce seulement si l'aire couvre au moins la moitié de ses
  appareils ; tuile PC du réglage 3.x en écran ; lampe allumée à luminosité 0 = « Allumé ».

### 2026-09-28 — Firmware : les pièces et leurs tuiles (ADR-0023, côté tablette)

- **Modèle** (`Tab5/tab5_tuiles.cpp`, nouvelle unité) : 5 pièces × 5 tuiles (type, icône
  de la palette, options, complément, nom gardé sur 24 octets et filtré aux glyphes des
  polices ; état, valeur, couleur). Nouvelle action **`tab5_maj_tuiles`** (instantané
  complet, grammaire de l'ADR) ; les états `tRT|état|valeur|couleur` passent par
  `tab5_maj_emplacements`, routés avant la table des emplacements 3.x. Définitions
  gardées en NVS (magie `TUI1`, écrites seulement si elles changent) : les pièces se
  dessinent avant que HA réponde ; les états ne sont pas gardés (« -- » grisé).
- **Mode héritage** : tant qu'aucune définition n'est arrivée (firmware mis à jour avant
  le blueprint), la pièce 0 est construite depuis les emplacements 3.x — PC/TV, volet,
  trois lumières — avec leurs noms, icônes, gestes et commandes 3.x.
- **Mode météo** : sur chaque page, une tuile qui porte un appareil de la pièce de la page
  le montre dans ses épaules (icône colorée par l'état ; ampoule ou flèche du prochain
  mouvement du volet) et reçoit son bouton invisible — les tuiles horaires aussi
  (`forecast_hour_card.yaml`). Appui court / long selon le type, options `o k r t m`
  (confirmation `k` : second appui dans les 3 s, « Confirmer ? »).
- **Mode HA** : les cinq cartes montrent la pièce de la page (icône de la palette, nom
  coupé avec « … », état traduit, couleur par type et état, cartes vides masquées et
  les autres centrées) ; la carte centrale affiche « Pièce n/N » et le nom de la pièce.
  **Le swipe change de pièce** (suivante / précédente qui a des appareils) et ne
  réaffiche plus la météo sous les cartes (bug) ; entrée sur une page vide → la pièce
  la plus proche ; le bouton « HA » montre le mode actif et disparaît sans appareil ;
  « Aller à l'écran → Accueil » quitte le mode HA. Le global `show_switches` disparaît
  (`g_central_ctx.ha_mode`, seule source).
- **Popup lumière** : le sélecteur liste les lumières de la pièce (5 au plus), s'ouvre
  sur la lumière appuyée ; « Tout éteindre » → `pR / eteindre`.
- Version par défaut `3.2.0-dev`, rendu `3.2.0-rendu` : le blueprint envoie les pièces.
  L'`on_boot` n'est pas touché (widgets posés par `tab5-tuiles.yaml`, lancé depuis
  `tab5_zones_apply`). 10 textes nouveaux, traduits dans les 5 langues.
- Tests : `tests/test_tuiles_firmware.py` (types, options, pièces, commandes, filtre des
  noms, routage, version, boutons contre l'ADR) — il a trouvé `t` et `r` inversés dans
  la table des options avant tout essai.

### 2026-09-28 — Blueprint : les pièces (ADR-0023, côté Home Assistant)

- **Cinq pièces de cinq appareils** dans le blueprint « Tab5 — emplacements » (même
  fichier, un ré-import suffit) : une section par pièce (la 1, l'accueil, ouverte ; les
  autres repliées), un nom et une liste d'appareils réordonnable, filtrée sur les
  domaines du contrat ; une section « Personnaliser des tuiles » (nom, icône,
  comportement : allumer seulement, confirmer, lecture seule). Les entrées 3.x
  (`lumiere_1..3`, `pc`, `volet`) gardent leurs noms, dans une section repliée
  « Tuiles de l'accueil (réglage 3.x) » : les automatisations existantes continuent.
- **Définitions** (`tab5_maj_tuiles`, à la connexion, au rechargement, à la demande des
  zones) : type par domaine, icône (personnalisée, attribut `icon`, classe, domaine, via
  le bloc généré `icones_mdi` / `icones_defaut`), options `d c o k r t m`, complément
  (unité ≤ 7 octets, classe), nom sans celui de la pièce ; nom de pièce saisi, sinon
  l'aire de ses appareils. Pièce 1 vide : l'accueil vient des entrées 3.x, et la tuile
  PC garde son comportement PC + TV.
- **États** `tRT|état|valeur|couleur` : tous après les définitions, une tuile quand ce que
  montre l'écran change (état, luminosité, `rgb_color`, position), les capteurs avec les
  mesures de 5 minutes. Cinq déclencheurs par pièce : un capteur qui change, la position
  GPS d'une personne ou le volume d'un lecteur ne réveillent pas l'automatisation.
- **Protocole** lu dans le `sw_version` de la tablette (« 3.1.0 (ESPHome 2026.9.0) ») : en
  dessous de 3.2.0, ou illisible, jamais `tab5_maj_tuiles`, seulement les clés 3.x.
- **Commandes** `tRT` et `pR / eteindre` aiguillées par le domaine de l'entité de la
  tuile ; seulement sur les entités placées dans une tuile. Le volet suivi par
  `volet_serre_tracking.yaml` passe toujours par son script, et sa tuile montre l'état
  tenu par le package (le moteur reste « unknown »).
- Tests : `tests/test_tuiles_blueprint.py` rend les vrais modèles Jinja du blueprint dans
  le bac à sable de Jinja (types, options, commandes = tableaux de l'ADR, protocole,
  définitions, états, un seul chemin de poussée, aiguillage) ; `jinja2` rejoint
  `requirements-dev.txt`. Le job « Installation dans un HA neuf » configure deux pièces
  et relit dans la trace les définitions calculées, sans `tab5_maj_tuiles` au protocole 1.
- Docs : « Adapt to your home » / « Adapter à sa maison », `HomeAssistant_Config/README.md`.

### 2026-09-28 — Pièces : la palette des icônes des tuiles (ADR-0023)

- **`Tab5/tuiles_icones.yaml`, source unique** : 51 codes (lumières, pièces, appareils,
  ouvrants, capteurs, actions), chacun avec son glyphe éteint / allumé (variantes -on,
  -off, -open, fermé / ouvert de MDI quand elles existent, sinon un seul glyphe), les
  303 noms `mdi:` qu'il représente et ses défauts : un par type de tuile (lum, int, vol,
  med, act, cap, bin, cli) et par domaine ou « domaine.classe » HA (`cover.garage`,
  `binary_sensor.door`, `sensor.temperature`…). `lit`, `canape`, `led`, `ordinateur` et
  `volet` gardent les glyphes de la 3.1.
- **Aucun point de code deviné** : le TTF du projet est Material Design Icons 7.4.47
  (mêmes 7 447 points de code que le `meta.json` de `@mdi/svg@7.4.47`) ; chaque couple
  nom ↔ point de code est vérifié contre ce `meta.json` (`--meta`) et contre le cmap du
  TTF (test, hors ligne).
- **`tools/gen_tuiles_icones.py`** écrit `Tab5/tab5_tuiles_icones.h` (même API
  `tuile_icone(code, actif, type)`), les 80 glyphes dans `mdi_font_70`, `mdi_font_45` et
  `mdi_font_32` (entre `# >>> tuiles` et `# <<< tuiles`, sans ceux que la police liste
  déjà), `icones_mdi` / `icones_defaut` du blueprint (entre ses marqueurs) et le tableau
  de `docs/tiles_icons.md` ; `--check` échoue si une partie est périmée, sans rien écrire.
  Les fins de ligne de chaque fichier sont gardées (même résultat sous Windows et Linux).
- Règle 7 : la table est rattachée aux cartes du mode HA (`icon_sw?`), aux épaules des
  tuiles (`icon_card_*`) et au sélecteur du popup lumière (`icon_light_sel_*`).
- **Coût mesuré par la CI** (job `build`, contre `main` @ `e6b81db`, dernier firmware
  compilé sur `main`) : image 3 239 558 → 3 290 102 octets, **+50 544 octets (≈ 49 Kio)**
  pour 215 glyphes ajoutés (75 à 70 px, 72 à 45 px, 68 à 32 px), soit ~235 octets
  chacun ; RAM inchangée (171 002 octets) ; flash 39,9 % → 40,5 %.
- Doc `docs/tiles_icons.md` (EN + FR) : la palette, comment l'icône est choisie, comment
  en demander une. Tests `tests/test_tuiles_icones.py`.

### 2026-09-28 — Pièces : la démo et le rendu montrent une maison de cinq pièces (ADR-0023)

- **Mode démo** : une maison de cinq pièces et vingt appareils, de tous les types du
  contrat (lampe couleur à variateur, interrupteur, volet en mouvement, média, scène et
  script, capteurs avec unité, porte, mouvement, présence, clim), avec un nom que la
  tablette coupe, des accents, un appareil hors ligne et une pièce de deux tuiles. Elle
  n'est poussée qu'à une tablette qui a l'action `tab5_maj_tuiles` (firmware 3.2) : les
  définitions, puis les états à la suite des emplacements (clés `tRT`) ; un firmware 3.x
  ne reçoit rien de plus, comme avec le blueprint. La maison minimale n'a qu'une pièce
  (le PC et deux lampes). Les commandes des tuiles sont journalisées avec leur pièce et
  leur nom. La tuile de la clim suit la carte clim de chaque scène.
- **Rendu hors tablette** : le mode HA de chaque pièce (`accueil-ha-piece-1` à `-5`,
  qui remplacent `accueil-interrupteurs`), par des gestes partis du bord de l'écran,
  seul endroit libre quel que soit le nombre de cartes ; chaque écran revient de
  lui-même à l'accueil en mode météo. Les pages 3-4 et horaires montrent les épaules
  des pièces en mode météo. Tant que le firmware des pièces n'est pas fusionné, ces
  captures montrent l'ancien affichage.
- Tests : `tests/test_demo_pieces.py` relit la grammaire dans l'ADR-0023 (types,
  options, code d'icône, longueurs, table pièce ↔ page) et y confronte les payloads,
  l'échappement (`|` → `/`, `;` → `,`), la cohérence des états et la palette ;
  `tests/test_demo.py` (pièces seulement avec l'action, définitions avant les états,
  journal) ; `tests/test_rendu_ecrans.py` (un écran HA par pièce, retour à l'accueil,
  gestes hors des boutons, boutons du haut).

### 2026-09-28 — Événements seulement : plus d'option « actions HA » à cocher (ADR-0025)

- **Le firmware n'appelle plus aucune action de Home Assistant.** Ses 13 derniers
  `homeassistant.service` (briefing du réveil, annonces, calendrier mois et jour, alertes
  lues, interruption de la voix, choix du pipeline, « MAJ Écran », « Recharger autos »,
  « Redémarrer HA ») deviennent des événements `esphome.tab5_*`. L'étape d'installation
  « Autoriser l'appareil à effectuer des actions Home Assistant » disparaît ; l'option
  peut être décochée, ce qui ferme à la tablette l'accès à *toutes* les actions de HA.
- **Nouveau package `packages/tab5_evenements.yaml`** : une automatisation traduit ces
  événements en une liste blanche d'actions, pour un appareil de modèle `tab5-ha-hmi`
  seulement, sur les entités de CETTE tablette (`device_entities`) ; aucun nom d'action
  ni d'entité ne vient de l'événement. `homeassistant.restart` ne part que de
  l'événement de confirmation, émis par le seul bouton « Confirmer ». Le pipeline n'est
  choisi que si l'option existe (plus d'erreur au démarrage sans « Discussion LLM »).
- **Plus d'entité à régler** : les substitutions `entity_tab5_satellite`,
  `_media_player`, `_pipeline_select`, `entity_primary_active` et `entity_push_automation`
  sont supprimées (une ligne restée dans `user_entities.yaml` est ignorée) ; un
  renommage de la tablette ou de l'automatisation de poussée ne casse plus rien.
- **Mise à jour depuis la 3.1** : déployer le package d'abord (inactif avec une 3.1),
  puis le firmware, puis décocher l'option. Un firmware récent sans le package ne plante
  pas mais ses demandes se perdent (détail dans `docs/installation.md`).
- Tests : `tests/test_actions_ha.py` réécrit (aucune action dans le firmware, chaque
  événement émis a un consommateur et inversement, liste blanche, garde du modèle,
  redémarrage sur confirmation seulement). Job « Installation dans un HA neuf » : sans
  l'option, calendrier ouvert par le select « Aller à l'écran » et « MAJ Écran » touché
  par le doigt virtuel, de bout en bout ; un redémarrage forgé par un autre appareil est
  ignoré ; aucune réparation « service_calls_not_allowed ». Le job se relance aussi sur
  les fichiers du firmware qui émettent ces demandes.
- Docs : guide d'installation (étape retirée, section « Passer d'une 3.1 à la suite »),
  ADR-0025, contrat des événements dans `Tab5/README.md`, README HA, assistant vocal,
  dépannage, site (vitrine et page d'installation, avec la note pour la 3.1).

### 2026-09-28 — Popup calendrier : chaque demande de mois a sa réponse

- `tab5_calendrier_mois` et `tab5_calendrier_jour` (`packages/tab5_calendar.yaml`) passent
  de `mode: restart` à `mode: queued` (`max: 10`). La tablette demande d'affilée le mois
  affiché et ses deux voisins (pré-chargement) : en `restart`, chaque demande annulait la
  précédente et une seule des trois aboutissait (vu par le job « HA neuf »). Chaque
  réponse porte son mois et va dans le cache de la tablette ; une réponse de jour
  périmée est déjà ignorée par le firmware. Test : `tests/test_installation_ha.py`.

### 2026-09-28 — Home Assistant sans placeholder : une archive, une ligne de YAML, des choix dans l'interface

Installer le côté Home Assistant ne demande plus ni dépôt ni Python ([ADR-0024](../../docs/decisions/0024-packages-without-placeholders.md)).
- **Archive `tab5_home_assistant.zip` jointe aux releases** (`tools/publication/archive_ha.py`,
  job `home-assistant` de `publication.yml`) : `packages/`, `custom_templates/`, le blueprint
  et `tab5_optionnel/`, dans l'arborescence de `config/`, avec un LISEZMOI. À décompresser
  dans `config/`, puis une seule ligne de YAML (`packages: !include_dir_named packages`).
- **Plus aucun placeholder** dans les packages : chaque valeur de la maison se choisit dans
  HA, dans des listes « Tab5 · … » (nouveau `packages/tab5_reglages.yaml`) : agenda de
  travail, des rendez-vous, des anniversaires, des jours fériés, téléphone, capteur de
  présence ; TV Samsung et son adresse (`tab5_tv.yaml`). Choix par défaut seulement sans
  ambiguïté ; « Aucun » éteint la fonction, sans erreur. Les agendas `calendar.famille`,
  `calendar.anniversaires` et des jours fériés ne sont plus écrits en dur ; un agenda de
  l'intégration Jours fériés compte tous ses événements comme fériés.
- **Détectés** : la tablette par le modèle de son appareil (`sensor.tab5_tablette` : écran,
  réveil en cours, micro, satellite, uptime… quel que soit son nom) ; les capteurs
  Météo-France de la ville, la météo OpenWeatherMap et MeteoAlarm (`sensor.tab5_sources_meteo`).
- **Plus de configuration HA refusée faute de secret** : `tab5_tv.yaml` n'a plus de
  `!secret tab5_tv_app_url` ; l'adresse de la TV est un réglage de HA (ou l'IP d'un suivi du
  routeur), et le package reste inerte tant qu'elle manque (une notification dit quoi régler).
- **Volet à course simulée optionnel** : `volet_serre_tracking.yaml` passe dans
  `HomeAssistant_Config/optionnel/` (`tab5_optionnel/` de l'archive), volet choisi dans
  « Tab5 · volet à course simulée ». Livré par défaut, son script aurait pris au blueprint
  les boutons du volet de tout le monde.
- `render_ha_config.py` ne fait plus que copier ; `--check` refuse aussi un placeholder
  restant. `placeholders.example.yaml` réduit à la liste des valeurs à ne jamais publier.
- CI « HA neuf » : installation sans rien remplir (plus de `placeholders_ci.yaml` ni de
  ligne dans `secrets.yaml`), sources choisies par `select.select_option`, tablette détectée
  par son modèle, `check_config` aussi avec les optionnels. Tests : entités `…tab5_…` lues
  toutes définies, archive reproductible et identique aux fichiers installés.
- **Migration depuis la 3.1** : remplacer les fichiers, régler les listes (docs/installation.md,
  étape 4), « Tab5 · agenda de travail » AVANT de recharger les automatisations, sinon le
  réveil voit tous les jours en repos.

### 2026-09-28 — Site : une release n'est retenue qu'avec ses binaires (suite de #221)

- `pages.py choisir` exigeait les trois manifestes, sans leurs binaires. Les neuf
  fichiers de v3.1.0 sont arrivés dans la même seconde (11:41:12-13 UTC, envoyés en
  parallèle par `gh release upload`) : un manifeste peut être joint avant ses binaires,
  et l'assemblage du site échoue alors (« binaire manquant »). Il faut maintenant le
  manifeste ST7123 et, pour chaque manifeste, ses deux binaires ; `site.yml` ne compte
  que les fichiers entièrement envoyés (état « uploaded » dans l'API).
- Les trois manifestes ne sont plus tous exigés : ajouter une révision à `ECRANS`
  (`pages.py`, qui redéploie le site) aurait écarté toutes les releases existantes et
  vidé les canaux stable et bêta jusqu'à la release suivante.
- Tests `tests/test_publication.py` : manifestes sans binaires, sans l'écran ST7123,
  autre écran absent ; les noms attendus sont ceux qu'écrit `preparer.py`. ADR-0022 :
  une ligne dans l'amendement du 28/09.

### 2026-09-28 — CI : le rendu hors tablette ne tourne plus pour rien

Le rendu (`rendu-host.yml`, 13 min, six langues en parallèle) était de loin le plus long
des checks, et le seul qui n'annulait rien :
- **un nouveau commit sur une PR annule le rendu en cours**, comme les deux autres
  workflows : trois pushes en dix minutes lançaient trois rendus complets (18 jobs) ;
- **sur `main`, seulement quand l'écran change** : même liste de chemins que pour les
  PR. Un merge de doc seule relançait 13 min de rendu. Les runs de `main` se suivent au
  lieu de se chevaucher : le 28/09, une PR et un merge simultanés dépassaient la limite
  de jobs de GitHub, 4 jobs attendaient 17 min et le rendu durait 30 min ;
- rien de moins n'est vérifié : mêmes écrans, mêmes langues, mêmes références ;
- test `tests/test_rendu_host.py` : mêmes chemins sur `main` et en PR, annulation en PR.

### 2026-09-28 — Site : une release encore en compilation n'est plus choisie

- #219 a été mergée quatre minutes après la création de la release v3.1.0, pendant que
  `publication.yml` compilait encore ses binaires : le déploiement du site l'a prise
  pour la stable, n'a trouvé aucun fichier (« no assets to download ») et a échoué
  (croix rouge sur `main` @ `4195e89`, page d'accueil de #219 pas mise en ligne).
- `site.yml` lit l'API des releases, avec leurs fichiers, au lieu de `gh release list` ;
  `pages.py choisir` écarte une release dont les trois manifestes ne sont pas encore
  joints : les canaux restent sur la précédente jusqu'à la fin de la publication.
- Tests : release sans fichiers ou incomplète ignorée ; `site.yml` lit bien les
  fichiers.

### 2026-09-28 — Le site raconte le projet, pas seulement l'installation

La page d'accueil du site (`web/index.html`) reprend une partie du README et de `docs/`,
en français et en anglais :
- **nouvelles sections** : pourquoi celui-ci, démarrer en six étapes (le parcours « sans
  compiler »), matériel (les trois puces d'écran et leur statut), comment ça marche
  (schéma Home Assistant ⇄ Tab5), assistant vocal (les deux pipelines, Domotique et
  Discussion, la chaîne en cinq étapes, les couleurs du micro), arcade (les 8 jeux),
  nouveautés, l'histoire, la documentation, questions ;
- **« Ce qu'il fait »** : 12 fonctions au lieu de 6 (télécommande TV et volet séparés) ;
- **sommaire** qui suit la lecture sur grand écran, barre de navigation en haut ;
- **version stable** affichée en tête, lue dans `versions.json` : rien à changer à chaque
  release ;
- **vidéo de démo** lue sur place, depuis YouTube en mode sans cookie, seulement au clic ;
  le tour animé des écrans (`tab5_ui_tour_hq.webp`) ajouté aux images du site ;
- **pluie dans l'heure et vigilances** : une section qui les explique, avec deux
  recadrages de rendus de la CI (scène « pluie + vigilance orange », en français et en
  anglais, dans `docs/images/site/`, hors de `docs/images/rendu/` que
  `maj_references.py` vide) et deux mini-écrans dessinés en HTML aux couleurs du code :
  les 9 barres sur l'heure et leurs 4 intensités, les couleurs des icônes et de la date ;
- **visionneuse** : une photo des galeries (tablette, jeux, rendus, météo) s'ouvre en
  grand au clic, avec sa légende ; flèches, clavier, balayage sur téléphone ;
- **typographie** : espace insécable avant « ; : ? » et dans les guillemets ;
- **langues** : la carte annonçait six langues sous un titre « Quatre langues » ; titre
  corrigé (« Six langues »).
- **Qui a écrit les langues** (site, README, `docs/translations.md`) : toutes par une IA,
  comme le reste du projet ; le français relu par l'auteur, l'anglais pas encore, comme
  les quatre autres. Les docs laissaient croire que seules ces quatre venaient d'une IA.

## [3.1.0] — 2026-09-28

De `v3.0.1` à aujourd'hui : 5 pull requests (#210, #214 → #217), plus celle de la
release.
- **L'écran parle aussi espagnol et italien** (#214), au choix dans le select
  « Langue » ;
- **installation dans un Home Assistant neuf, testée en CI** sans matériel (#210) ;
  le test a trouvé cinq défauts côté HA, corrigés (#215) ;
- **voix** : plus d'appel à une action inexistante de HA en interrompant (#217) ;
- CI : l'installation pip réessaie (#216).

**Version mineure** : une nouvelle fonction (deux langues), rien à changer dans une
installation existante hors les packages HA ci-dessous.

### À faire en mettant à jour depuis 3.0.1

- **Firmware** : depuis HA (entité « Firmware »).
- **HA** : reprendre `tab5_push.yaml`, `tab5_calendar.yaml`, `tab5_reveil.yaml` et le
  blueprint `tab5_emplacements.yaml`, puis recharger scripts et automatisations. L'ordre
  est libre. Le blueprint repousse tout l'écran au rechargement.
- `tab5_tv.yaml` exige la ligne `tab5_tv_app_url` dans `secrets.yaml` (déjà le cas
  avant, maintenant écrit dans le guide).

### Mesures de la version

- Compilations de la CI (ESPHome 2026.9.0), firmware de `v3.0.1` contre celui de ce
  tag : image 3 191 110 → 3 239 558 o (+47 Ko, l'espagnol et l'italien), RAM statique
  171 066 → 171 002 o (−64 o) ; aucun avertissement dans notre code.
- Rendu hors tablette : identique aux références dans les six langues (#217).
- Test « installation dans un HA neuf » : vert (#215, #216).

### Problèmes connus

Ceux de la 3.0.1. En plus :
- **Espagnol et italien** : traduits par une IA, pas encore relus par une personne dont
  c'est la langue ; vus seulement sur le rendu hors tablette, pas encore sur la
  tablette.

### 2026-09-28 — Voix : plus d'appel à une action inexistante de HA en interrompant

- `Tab5/tab5-assist.yaml`, `tab5_vocal_interrupt` (taper le micro pendant une réponse,
  Stop vocal, réveil) : le firmware appelait `assist_satellite.stop`, qui n'existe pas
  dans HA (le domaine n'a que `announce`, `start_conversation`, `ask_question`). Chaque
  interruption écrivait « Action assist_satellite.stop not found » dans le journal de
  HA (vu le 28/09 à 12:27). L'appel est retiré : `voice_assistant.stop` arrête déjà le
  pipeline, et HA clôt la session du satellite de lui-même.
- Test `tests/test_actions_ha.py` : chaque action HA appelée par le firmware est une
  action vérifiée dans HA 2026.9 ou un script défini par un package du projet.
- Docs : `screens.md` et `voice_assistant.md` (EN/FR) ne citent plus cette action.
### 2026-09-28 — CI : l'installation pip réessaie, plus de croix rouge venue de PyPI

- PyPI répondait parfois « Could not find a version that satisfies the requirement
  esphome==2026.9.0 (from versions: none) » : un ou deux jobs de rendu (six langues en
  parallèle depuis #214) ou le job d'installation dans un HA neuf ratait l'installation
  d'ESPHome, pendant que les autres la réussissaient. Croix rouges sur `main` à
  `e89bc76` et `09bd0ee`, sans rapport avec le code.
- `tools/ci/pip_reessai.sh` : 4 essais (20, 40 puis 60 s d'attente), avec les réessais
  de pip sur les erreurs de connexion. Utilisé par `esphome-tab5.yml`,
  `installation-ha.yml` et `rendu-host.yml`. `publication.yml` porte sa propre boucle :
  il peut reconstruire un ancien tag, qui n'a pas le script.
- `.gitattributes` : les scripts `.sh` restent en LF, même dans un checkout Windows.
- Test `tests/test_ci_pip.py` : chaque `pip install` d'un workflow réessaie.

### 2026-09-28 — HA attend la tablette : les défauts trouvés par le test « HA neuf »

Quatre défauts relevés par le nouveau test d'installation dans un Home Assistant neuf
(#210), corrigés côté HA, sans flash :
- **La tablette est reconnue par son modèle** (`tab5-ha-hmi`, bloc `project:` du
  firmware) et non plus par le nom de son capteur « HA API Status » :
  `integration_entities('esphome')` → capteur `…_ha_api_status` à `on` → modèle.
- **Poussée complète bloquée si l'appareil est renommé** : sa garde « événement
  réémis » était une condition d'état sur `binary_sensor.m5stack_…_ha_api_status` ;
  absente, elle arrêtait tout. Elle passe par le modèle (`packages/tab5_push.yaml`).
- **Erreurs « Action … not found » au démarrage de HA** (packages installés avant la
  tablette, comme le dit le guide) et « Not connected » (tablette hors ligne) : la
  poussée complète et les scripts qui appellent la tablette (alertes, météo, volet,
  calendrier, rendez-vous) commencent par la garde « tablette connectée » et
  s'arrêtent sans erreur.
- **Écran vide après la création du blueprint** (automatisation créée après l'ajout de
  la tablette) : le blueprint pousse tout, zones comprises, au rechargement des
  automatisations (`automation_reloaded`) — donc aussi quand on change un emplacement.
  Ses déclencheurs venus de HA attendent eux aussi une tablette connectée.
- **Rendez-vous poussés vers une tablette déconnectée** : le `number` « Rendez-vous :
  annoncer avant » passait par `unavailable` à chaque déconnexion ; le déclencheur
  ignore maintenant ces passages (`not_from` / `not_to`).
- `placeholders.example.yaml` : correspondance du capteur « HA API Status » (gardes de
  `tab5_health.yaml` et `tab5_micro_absence.yaml`) pour une tablette renommée.
- Tablette virtuelle : même bloc `project:` que la vraie, donc même modèle.
- **Test « HA neuf »** : une ERREUR Tab5 avant la connexion de la tablette fait
  maintenant échouer le job ; il vérifie aussi que le blueprint remplit l'écran (zones
  masquées) dès la création de son automatisation, sans reconnexion. Tests pytest : la
  garde ouvre chaque script qui appelle la tablette ; même modèle pour la tablette
  virtuelle.

### 2026-09-28 — CI : installer le Tab5 dans un Home Assistant neuf, sans matériel

- **`.github/workflows/installation-ha.yml`** (~3 min, non requis) fait ce que fait un
  nouvel utilisateur : un Home Assistant 2026.9.4 **neuf** en conteneur et la tablette
  virtuelle (le rendu hors tablette compilé sous le nom `tab5-ha-hmi`).
  `tools/installation_ha/preparer_config.py` écrit la configuration (celle d'une installation
  neuve, **tous** les packages rendus avec des valeurs factices, le blueprint, des données
  de test : intégration `demo` et `donnees_test.yaml`), `check_config` la valide ;
  `verifier_installation.py` suit l'ordre « Sans compiler » du guide : compte, agenda,
  ajout de la tablette par le flux ESPHome (hôte, port), « actions Home Assistant »,
  automatisation du blueprint, puis redémarrage de la tablette.
- Le job échoue si : la clé API n'est pas créée par HA sans rien saisir, gardée (jamais
  affichée) et capable d'ouvrir la tablette ; la clé nulle ou le clair passent encore ensuite ;
  `esphome.tab5_connected` n'arrive pas après la clé ; une trace du blueprint (connexion,
  zones) ou de la poussée complète n'aboutit pas ; « Zones masquées » ne vaut pas
  `pot_4, pot_5` ; la capture demandée **par HA** (`rendu_capture`) manque ; l'un de ces
  points rate après un redémarrage de la tablette ; le journal de HA a une erreur Tab5
  après la connexion (hors « Not connected » pendant une déconnexion voulue, rapportée).
  Artefact `installation-ha` : deux captures (juste après l'étape 6, puis après le
  redémarrage), journaux de HA et de la tablette.
- **Trouvé par le job** :
  - `packages/tab5_tv.yaml` exige `tab5_tv_app_url` dans `secrets.yaml`, sans quoi HA
    refuse **toute** sa configuration ; seule l'en-tête du package le disait.
    `docs/installation.md` (étape 4) le dit maintenant ;
  - la poussée complète ne part pas à la (re)connexion quand
    `binary_sensor.<appareil>_ha_api_status` n'existe pas : sa condition échoue
    (« unknown entity », un simple avertissement) au lieu de laisser passer, et l'écran
    reste sans prévisions jusqu'au passage des 10 minutes. Sans effet avec le nom livré ;
    en suspens pour un appareil renommé (cette entité n'est pas dans
    `placeholders.example.yaml`) ;
  - dans l'ordre de la documentation (packages, puis tablette), chaque poussée lancée
    avant l'ajout de la tablette écrit une ERREUR « Action esphome.tab5_ha_hmi_… not
    found » dans le journal de HA (`continue_on_error` ne la rattrape pas) ;
  - « Sans compiler » fait créer l'automatisation du blueprint **après** l'ajout de la
    tablette : ses déclencheurs (connexion, demande des zones, une par connexion) sont
    passés. Jusqu'à la prochaine reconnexion, l'écran garde températures « -- », clim
    vide, pots en attente et aucune zone masquée (capture 1 du job : aucun passage du
    blueprint en 20 s, au mieux celui des mesures toutes les 5 minutes ou d'une lumière
    qui change). En suspens : redémarrer la tablette après l'étape 6, ou créer
    l'automatisation avant l'ajout (ordre de l'étape 4) ;
  - `tab5_rdv_push` (`packages/tab5_reveil.yaml`) se déclenche quand le `number`
    « Rendez-vous : annoncer avant » passe à `unavailable`, donc à chaque déconnexion de
    la tablette ; son attente sur « HA API Status » passe encore (l'entité n'est pas
    encore marquée indisponible) et l'envoi écrit une ERREUR « Not connected ». Rapporté
    par le job avec ses traces, en suspens (piste : `not_to: [unavailable, unknown]`).
- Rendu hors tablette : nom de l'appareil en substitution (`rendu_nom`, défaut
  `tab5-rendu`) ; `status_ha` des bouchons nommé « HA API Status » comme sur la tablette.
- Tests : `tests/test_installation_ha.py` (préparation, placeholders et secrets couverts,
  entrées du blueprint, nom de la tablette virtuelle, lecture des traces et du journal).
### 2026-09-28 — L'écran parle aussi espagnol et italien

- **`Tab5/lang/es.yaml`** (Español, index 4) et **`Tab5/lang/it.yaml`** (Italiano,
  index 5), complets : les 937 textes, jeux compris, sauf les questions du quiz. Traduits
  par une IA, pas encore relus par une personne dont c'est la langue.
  - Espagnol neutre (Espagne et Amérique latine), tutoiement ; italien, tutoiement.
  - Jours en trois lettres, comme en français et en anglais : Lun Mar Mié Jue Vie Sáb
    Dom, Lun Mar Mer Gio Ven Sab Dom. L'allemand (Mo Di Mi) et le néerlandais (Ma Di Wo)
    restent en deux lettres : c'est leur usage.
  - Initiales du réveil : X pour miércoles en espagnol (usage des calendriers) ; en
    italien, martedì et mercoledì partagent le M, comme en français.
  - Place mesurée en pixels (Roboto 700) contre la plus large des quatre langues déjà
    en place ; ce qui dépasse a été raccourci, ou vérifié dans le code (zone plus large,
    texte qui passe à la ligne).
- Select « Langue » : Español et Italiano ajoutés à la fin (index gardés).
- **CI** : le rendu hors tablette dessine aussi l'espagnol et l'italien (six tâches).
- Docs (README, traductions, installation, débogage, site), cartographie.

## [3.0.1] — 2026-09-28

Correctif tiré de la première installation à neuf de la 3.0.0 par la page (28/09 au
matin, tablette effacée) : #209, #211 et #212, plus celle de la release.
- **Plus de fausse alerte « plantage »** au premier démarrage après une installation
  par l'USB (#211) ;
- **page `/install/` et guide** : parcours court « sans compiler » et pièges de la page
  (#212) ; la page a déménagé dans `/install/`, la racine du site est une vitrine (#209).

### À faire en mettant à jour depuis 3.0.0

- **Firmware** : depuis HA (entité « Firmware »). Rien d'autre ne change sur la tablette.
- **HA** : reprendre `packages/tab5_health.yaml` si vous l'utilisez. L'ordre est libre.

### Mesures de la version

- Compilation de la CI (ESPHome 2026.9.0) : image 3 188 470 → 3 191 110 o (+2,6 Ko),
  RAM statique 170 626 → 171 066 o (+440 o) ; aucun avertissement dans notre code.
- Rendu hors tablette : identique aux références (#211).

### Problèmes connus

Ceux de la 3.0.0, sauf l'installation à neuf, faite une fois par l'auteur. En plus :
- un flash en mode téléchargement **sans** effacement, sur une tablette déjà en 3.0.1,
  reste signalé comme « other watchdogs ».

### 2026-09-28 — Installer sans compiler : un parcours court, les pièges de la page

Tirés de l'installation à neuf du 28/09, tablette effacée, par la page :
- **Guide** (`docs/installation.md`, EN/FR) : une section « Sans compiler » en six
  étapes dans l'ordre (HA d'abord, flash, Wi-Fi, ajout dans HA, actions HA, blueprint),
  à la place de l'encadré qui renvoyait aux étapes 4 et 6. ESPHome n'est plus un
  prérequis pour qui ne compile pas.
- **Page `/install/`** :
  - reconnaître la tablette dans la liste des ports ;
  - les trois cas : première installation (effacer), mise à jour (sans effacer), même
    version déjà installée (pas de bouton « Install », seulement « Erase User Data ») ;
  - « Failed to initialize… BOOT button » : le Tab5 n'a pas de bouton BOOT ; maintenir
    reset ~2 s jusqu'au clignotement rapide de la LED verte (mode téléchargement,
    procédure M5Stack), puis un appui sur reset à la fin ;
  - changer de canal = réinstaller sans effacer ;
  - une tablette déjà connue de HA reçoit une nouvelle clé toute seule ;
  - l'option « actions Home Assistant » à cocher (voix, calendrier, réveil).
- README : le démarrage rapide renvoie à ce parcours.
### 2026-09-28 — Plus de fausse alerte au premier démarrage après une installation

Vu à l'installation à neuf du 28/09 (page d'installation, mode téléchargement, flash
effacée) : le flash finit par un reset du chien de garde RTC (`ESP_RST_WDT`, « other
watchdogs » pour ESPHome). La garde « reboot inattendu » et le journal des démarrages
ont alors signalé un « plantage (chien de garde) », sur le téléphone.

- **Firmware** (`tab5_journal.cpp`) : une marque en NVS, écrite au premier démarrage,
  absente juste après un effacement. Marque absente **et** `ESP_RST_WDT` : c'est
  l'installation, pas un plantage (repère « premier démarrage après installation »).
  Une panique ou un chien de garde de tâche alertent toujours, même au premier
  démarrage. Sur une tablette neuve, le Wi-Fi pas encore réglé et HA qui tarde à
  l'ajouter ne sont plus des anomalies, tant qu'elle n'a jamais vu son réseau.
- Le capteur « Tab5 Raison du redémarrage » publie alors « First boot after install
  (other watchdogs) » (filtre `journal_raison_ha`).
- **HA** (`packages/tab5_health.yaml`) : la garde « reboot inattendu » laisse passer
  cette raison.
- Test `tests/test_premier_demarrage.py` : le préfixe du firmware est celui que lit la
  garde, et seul le chien de garde RTC est excusé.

### 2026-09-28 — Site du projet : une vitrine, des images que Google peut indexer

- **Pourquoi les images du README ne sortaient pas dans Google** : github.com les sert en
  `/<owner>/<repo>/raw/main/…`, chemin interdit à tous les robots par son `robots.txt`
  (`Disallow: /*/raw/`). Le site GitHub Pages, lui, n'a pas de `robots.txt`.
- `web/index.html` devient une **vitrine** (français et anglais) : ce que fait l'écran,
  12 photos légendées et les rendus hors tablette, limites dites simplement, balises de
  partage (Open Graph), adresse canonique, JSON-LD. **La page de flashage passe dans
  `install/`** ; les dossiers des canaux restent à la racine, les firmwares publiés lisent
  leur mise à jour à la même adresse.
- `tools/publication/pages.py` copie les images de `docs/images/` sous un nom parlant
  (`IMAGES`) et écrit `sitemap.xml` (pages et images).
- **`.github/workflows/site.yml`** redéploie le site sans rien compiler : après une
  publication, à chaque push sur `main` qui touche le site, ou à la main. Avant, corriger la
  page recompilait les trois firmwares et remplaçait les fichiers de la release.
- `docs/images/tab5_social_preview.jpg` (1280×640) : image de partage du dépôt et du site.
- README : titre avec « Home Assistant », textes alternatifs des images, 22 ADR et 45
  composants (et non 17 et 35/40), plus de « pas de firmware précompilé » (la 3.0 s'installe
  depuis le navigateur), liens vers `install/` (doc d'installation, brouillons
  Hackster et forum HA compris).
- ADR-0022 amendée ; tests : images, balises de chaque page, sitemap, site sans compilation.

## [3.0.0] — 2026-09-28

De `v2.2.0` (27/09, 11:14) à aujourd'hui : 19 pull requests (#189 → #207),
plus celle de la release. Les lots 5 à 8 de l'audit « ouverture » sont terminés :
- **installer sans compiler** : une page de flashage dans le navigateur (3 révisions
  d'écran, Wi-Fi par Improv), puis les mises à jour proposées dans Home Assistant ;
- **aucun secret dans le firmware** : la clé API est donnée par HA, les mises à jour
  sont signées par la clé du projet ;
- **les appareils se choisissent dans HA, à la souris** (blueprint), et ce qui manque
  disparaît de l'écran ;
- l'écran parle aussi **allemand et néerlandais** ; la CI dessine **80 écrans en quatre
  langues** sans tablette.

**Version majeure** : le firmware ne connaît plus les entités de la maison (le blueprint
devient obligatoire), la clé API et le Wi-Fi ne sont plus compilés, et une 2.x refuse une
OTA en clair. Le passage se fait une fois, sur place.

### À faire en mettant à jour depuis 2.2.0

Le guide pas à pas : [« Passer à la 3.0 »](../../docs/installation.md#passer-à-la-30).
- **Home Assistant 2026.8 ou plus récent** : c'est lui qui donne sa clé à la tablette.
- **HA d'abord** : reprendre `tab5_push.yaml`, `tab5_health.yaml` et
  `tab5_meteo_sources.yaml`, importer le blueprint `tab5_emplacements` et créer
  l'automatisation avec vos appareils. Les placeholders `VOTRE_CLIMATISATION`,
  `VOTRE_LEDS` et `VOTRE_PC` disparaissent.
- **Firmware** : `tools/migrer_vers_3.py` l'envoie chiffré avec l'ancienne clé
  (`api_encryption_key` de `secrets.yaml`) et redonne le Wi-Fi par l'USB (`--port`).
  Pour compiler vous-même, il faut d'abord une clé de signature (étape 3 du guide).
- **Dans les 30 minutes après le démarrage** : confirmer dans HA la réauthentification
  « chiffrement désactivé ». HA donne alors une nouvelle clé ; entités, automatisations
  et historique ne changent pas.
- Ensuite, `secrets.yaml` n'est plus lu et `tab5_fuseau:` est ignoré (fuseau de HA).
- **Entités orphelines** : trois anciennes entités peuvent rester dans le registre, en
  « indisponible » (vu chez l'auteur, supprimées le 28/09) :
  `automation.maj_ecran_tab5_climatisation_push_rapide`, `script.tab5_push_clim`,
  `automation.tab5_zones_presentes_reponse_a_la_tablette`.
- **Nouveau** : l'entité « Firmware » (mise à jour) dans les binaires publiés, le
  select « Langue » à quatre choix.

### Mesures de la version

- **Compilations de la CI** (ESPHome 2026.9.0), firmware de `v2.2.0` contre celui de ce
  tag (`5bf704a`, aucun fichier du firmware changé depuis) :
  - image 3 122 396 → 3 188 470 o (+65 Ko). L'allemand et le néerlandais en font
    l'essentiel (≈ 45 Ko), les emplacements en rendent 12,6 Ko ;
  - RAM statique 170 992 → 170 626 o (−366 o) ;
  - aucun avertissement dans notre code.
- Les binaires publiés embarquent en plus la mise à jour par HTTP (+64 Kio, mesuré sur
  la rc.1).
- **Home Assistant** : l'automatisation des emplacements tourne au plus 288 fois par jour
  pour les mesures, au lieu de ~1 150 (#207).
- **Chaîne de publication essayée sur la tablette de l'auteur** : 3.0.0-rc.1 flashée par
  la page (27/09), puis rc.2 et rc.3 installées depuis HA. La 3.0.0 a le même code que la
  rc.3 ; seul le numéro de version change.

### Problèmes connus

- **Installation à neuf** : jamais faite sur une tablette effacée, ni par quelqu'un
  d'autre que l'auteur. La page a été essayée sans effacement, sur une tablette déjà en
  3.0.
- **Fin de mise à jour depuis HA** : un plantage vu une fois (rc.1 → rc.2), pas
  reproduit sous capture (rc.2 → rc.3). `tools/capture_serie.py` le capture
  ([débogage](../../docs/debugging.md)).
- **ST7121** : signalée fonctionnelle par un tiers avec la configuration d'ESPHome, notre
  firmware jamais essayé dessus. **ILI9881C** : jamais essayée.
- **Allemand et néerlandais** : traduits par une IA, pas encore relus par une personne
  dont c'est la langue. **Questions du quiz** : en français, par choix.
- **Annonce parlée du réveil** : composée en français par HA (`tab5_reveil.yaml`).
- **Disposition** : pas plus de 3 lumières, pas d'autre type d'appareil par tuile ; le
  bouton « Brise » de la clim est le préréglage Daikin `windnice`.
- **Au démarrage**, « Prochain réveil » affiche « Demain 07:00 » environ 50 s, le temps
  que HA envoie le planning.
- **Météo** : OpenWeatherMap ne prévoit que 8 jours (fin des pages de 15 jours vide) ;
  MeteoAlarm et le regroupement NWS n'ont été testés qu'avec des données simulées.
- **Flipper** : en portrait, les logs LVGL sont inondés tant qu'un doigt est posé.

### 2026-09-28 — Blueprint des emplacements : mesures regroupées toutes les 5 minutes

- `blueprints/automation/tab5/tab5_emplacements.yaml` : l'automatisation tournait
  ~1 300 fois par jour chez l'auteur, dont 775 pour une température de serre qui oscille
  d'un dixième toutes les 40 s (capteur BLE). L'écran affiche ce dixième : ne pousser que
  la valeur affichée n'aurait rien retiré.
  - **Mesures lentes** (téléphone, température et humidité de la pièce, serre, pots) :
    plus de déclencheur par capteur, mais un passage toutes les 5 minutes qui pousse en
    un seul envoi celles qui ont changé. Un pot part avec ses 4 détails si l'un des 5 a
    changé : conductivité, lumière, température et batterie suivent désormais leurs
    propres changements, et plus seulement ceux de l'humidité.
  - **Lumières, PC, TV** : toujours immédiats, mais seulement quand l'état ou la
    luminosité change, pas pour un autre attribut (couleur, lecture en cours).
  - Un passage sans rien de neuf s'arrête à la condition, sans ligne au journal.
  - `min_version` du blueprint : 2026.8.0, celle que demande le firmware 3.0.
  - Test : chaque emplacement a un seul chemin de poussée, et la fenêtre couvre la période.

### 2026-09-27 — La garde « reboot inattendu » ignore les redémarrages demandés

- `packages/tab5_health.yaml`, garde (b) : elle notifiait à chaque nouveau démarrage, mise à
  jour comprise (vu ce soir, rc.2 → rc.3). Elle lit maintenant `Tab5 Raison du redémarrage`,
  en attendant au plus 1 min celle de ce démarrage (elle repasse par `unavailable` à chaque
  coupure). Ne notifient plus : « Reboot request from … » (mise à jour depuis HA ou par
  `esphome upload`, bouton de redémarrage), « software via esp_restart » (select Langue),
  « USB peripheral » (flasheur web). Un plantage, une coupure ou une chute de tension
  notifient toujours, avec la raison dans le message ; sans raison reçue à temps aussi.
- Déployé sur HA (rendu, `.bak` à côté, configuration vérifiée, automatisations
  rechargées) ; logique vérifiée sur les vraies raisons avec l'évaluateur de templates.

### 2026-09-27 — Capturer un plantage sur le port série, garder l'ELF des firmwares publiés

La mise à jour de la tablette depuis HA (3.0.0-rc.1 → rc.2, 27/09/2026) a fini par
« plantage (exception) », sans rapport dans le journal des démarrages : la sortie de
panique d'ESP-IDF ne s'écrit que sur la console USB, et l'ELF qui décode ses adresses
n'était pas gardé.

- **`tools/capture_serie.py`** : écoute le port USB de la tablette sans la réinitialiser
  (trouvée par sa MAC, jamais devinée entre deux appareils Espressif ; DTR et RTS coupés
  avant l'ouverture), écrit la capture au fil de l'eau, s'arrête 60 s après un
  redémarrage, extrait le bloc de panique et décode ses adresses avec `--elf`
  (`riscv32-esp-elf-addr2line`). Essayé en écoute sur la tablette : pas de redémarrage.
- **Publication** : l'ELF de chaque révision est gardé 90 jours (artefact
  `elf-<révision>`). L'entrée `elf_seulement` recompile une version déjà publiée pour
  retrouver son ELF, sans rien publier ni remplacer, et vérifie que le code est celui de
  l'image publiée (`tools/publication/meme_code.py` : même taille, seuls l'heure de
  compilation, les empreintes et la signature diffèrent).
- **Docs** : `debugging.md`, « Capturer un plantage sur le port série », en français et
  en anglais ; la partie anglaise du rendu hors tablette mise à jour (80 écrans, 4 langues).

### 2026-09-27 — Les jeux parlent un français accentué (lot b)

- **Accents** : les textes français des huit consoles avaient été écrits sans accents
  (« Reglages », « Equipement », « Difficulte », « ARCANOIDE »…), alors que les polices
  ont les glyphes : l'allemand affichait ses umlauts. Le texte français étant la clé de
  traduction, 233 clés sont renommées dans le code et dans les trois langues (traductions
  inchangées ; 10 fusionnées avec une clé qui existait déjà, comme « Réglages »). Les mots
  ambigus (a / à, ou / où, termine / terminé, Active / Activé…) relus phrase par phrase ;
  les contextes de `tr_ctx` (« echecs|Pion ») ne changent pas.
- **Dames** : plus d'anglais dans l'interface française (« flying kings », « Setup »,
  « PvP », « Hint », « Undo », « Reset », « 0W / 0D / 0L ») ; boutons Annuler / Indice
  élargis à 96 px, avec les clés déjà traduites de Go et du Roi Noir.
- **Arcanoïde** : « Record » au lieu de « Best ». « GAME OVER » reste, comme sur les bornes.

### 2026-09-27 — Calages relevés sur le rendu hors tablette (lot a)

Défauts vus sur les captures de tous les écrans (lot 7 bis), réels sur la tablette : le
rendu dessine avec le même code.

- **Prévisions horaires** : dès qu'il pleuvait, « 0.5mm » recouvrait la température.
  Onglet du bas à 210 px (mesuré en Roboto 32 gras), sans décimale au-delà de 10 mm.
- **Interrupteurs** : les onglets étaient déclarés avant le corps de la carte, donc dessinés
  dessous (titres coupés, accent de « Éteint » masqué). Ordre des prévisions repris ;
  onglet de titre à 200 px (« Woonkamer » : 173 px).
- **Go** : lignes de menu de 68 px et pastilles des joueurs de 54 px, la seconde ligne
  n'est plus coupée.
- **Trial Poursuite** : « – » au lieu du signe moins, absent des polices (rectangle vide) ;
  İ ō ř ajoutés au jeu latin-1 pour trois questions ; nouveau test : toute chaîne du code
  s'affiche avec les glyphes des polices.
- **Réveil** : libellés des boutons centrés à droite du pictogramme, sans la marge du thème
  (« Ouverture », « Werkbegin », « Shift start » passaient dessous) ; délai en « 1h30 »
  (« 90 min » ne tenait pas en 59 px) ; « 15 min » sous « RDV avant » ; « Voice
  announcement » en anglais.
- **Coureur d'Or** : grille des niveaux resserrée, « Retour » ne la touche plus ; classement
  vide centré. **Dames** : « Reprendre » grisé au lieu d'un trou. **Roi Noir** : bilan par
  niveau en lignes centrées (les colonnes à l'espace ne tombaient pas juste). **Console** :
  valeur de « Bloc max » décalée (« Max. Block » la touchait).
- **Rendu** : temps actif de la console figé ; les parties de Fil d'Or (salle tirée d'une
  graine prise sur l'horloge monotone) sont capturées mais plus comparées.
- Vérifié sur le rendu, dans les 4 langues : seuls les écrans visés changent.

### 2026-09-27 — Chaque écran capturé, en quatre langues, au doigt virtuel (lot 7 bis)

Suite du lot 7 (ADR-0021, amendée) : le rendu hors tablette ne capturait que les trois
scènes de l'accueil, en français et en anglais.

- **Doigt virtuel** (`Tab5/rendu/rendu_doigt.h`, rendu seulement) : un pointeur LVGL piloté
  par les actions `rendu_toucher` (appui, appui long) et `rendu_glisser` (geste), aux
  coordonnées des captures. Les écrans s'ouvrent comme sur la dalle, widgets des jeux
  créés en C++ compris.
- **Plan de 80 écrans** (`tools/rendu/ecrans.py`) : variantes de l'accueil, toutes les
  fenêtres et sous-fenêtres, le sélecteur Arcade, menus, partie, pause et fin des 8 jeux.
  Toute partie lancée est abandonnée (une sauvegarde décalerait les menus).
  `capturer.py` signale une capture identique à une autre (appui tombé à côté) ;
  `tests/test_rendu_ecrans.py` garde le plan cohérent.
- **CI** : une tâche par langue en parallèle (français, anglais, allemand, néerlandais),
  préférences neuves pour chacune, ~14 min. Une PR est comparée aux captures du dernier
  run réussi sur `main` (artefact gardé 90 jours) ; seules les scènes FR/EN restent
  versionnées (galerie). `workflow_dispatch` : langues et écrans au choix.
- **Correctif** : « Connecté » avait perdu son accent dans la console système.
- Vérifié : 332 captures, aucune en double ; les 6 références d'origine identiques au pixel.

### 2026-09-27 — Migration depuis la 2.x : le Wi-Fi redonné par l'USB, sans point d'accès (lot 6c-3)

Demande d'Axel après sa propre migration : ne plus passer par « Tab5 Fallback AP » et un
téléphone pour le premier réglage du Wi-Fi.

- **`tools/migrer_vers_3.py --port COM…`** : juste après l'envoi de la 3.0, le script
  attend que la tablette redémarre et lui redonne son réseau par Improv sur l'USB, avec
  `wifi_ssid` et `wifi_password` du même `secrets.yaml` (jamais affichés). Si la tablette
  ne répond pas ou n'arrive pas à se connecter, il le dit et renvoie au point d'accès.
- **`tools/improv_serie.py`** : le protocole Improv série (celui du bouton Wi-Fi de la
  page de flashage), réutilisable ; lancé seul, il lit l'état et l'identité de la
  tablette sans rien changer. Port ouvert DTR et RTS à 0 : pas de réinitialisation.
  - Essayé sur la tablette le 27/09 (3.0.0-rc.1, COM6), en lecture seule : « Wi-Fi
    réglé » et son identité (projet, version, puce, nom), rien de redémarré.
- `secrets.yaml` lu en YAML (un mot de passe entre guillemets contenant `#` était
  coupé par l'ancien découpage à la main).
- **Page de flashage** : ne pas effacer une tablette déjà installée (elle garde son
  Wi-Fi et sa clé de HA) ; fermer la fenêtre la redémarre une fois (vu le 27/09).
- **Tests** : `tests/test_improv_serie.py` (paquets, lecture au milieu du journal,
  réglage face à une fausse liaison série).
- Docs : installation (étape 5 et « Passer à la 3.0 », EN/FR).

### 2026-09-27 — Installer depuis le navigateur, mettre à jour depuis Home Assistant (lot 6c-2)

Lot 6 de l'audit « ouverture », fin : le firmware publié, ADR-0022. Choix d'Axel : clé du
projet dans un secret GitHub (posé par lui), GitHub Pages, mise à jour dans les seuls
firmwares publiés, pré-release `v3.0.0-rc.1` pour essayer la chaîne.

- **Workflow `.github/workflows/publication.yml`**, à la publication d'une release (ou à
  la main pour un tag) :
  - compile les trois révisions d'écran avec ESPHome **figé** (`ESPHOME_PUBLICATION`,
    2026.9.0, jamais sous `min_version`) ;
  - les signe avec la **clé du projet** (secret `TAB5_CLE_SIGNATURE`). Avant, il vérifie
    que le secret redonne l'empreinte publique SBv2 écrite dans le workflow ; après, il
    vérifie la signature de chaque image, puis efface la clé ;
  - joint `tab5-ha-hmi-<révision>.factory.bin`, `.ota.bin` et `manifest-<révision>.json`
    à la release ;
  - reconstruit GitHub Pages depuis les fichiers des releases : `stable/` = dernière
    release 3.x non « pre-release », `beta/` = la plus récente.
- **Page de flashage `web/index.html`** (ESP Web Tools 10.4.0 figé, français et anglais) :
  choix de la révision d'écran (avec ce qui a été essayé ou non) et du canal, puis Wi-Fi
  par Improv, ajout à HA, blueprint. Vérifiée en local avec un site assemblé par
  `pages.py` : versions affichées, manifeste suivi, langue.
- **Firmware** :
  - `tab5_publication` choisit `Tab5/publication-<valeur>.yaml` : `locale` (défaut) est
    vide, `stable` / `beta` ajoutent `ota: http_request` et l'entité de mise à jour
    « Firmware », qui lit `<canal>/<révision>/manifest.json` toutes les 6 h ;
  - `project: version` vient de `tab5_version` (le tag), `3.0.0-dev` en local ;
  - `ota:` passe en liste dans `tab5-hardware.yaml`. **Piège** : écrit en dictionnaire,
    il était REMPLACÉ par celui du package (plus aucune OTA `esphome`), vu avec
    `esphome config` avant tout flash.
- **Vérifié dans le code** (ESPHome 2026.9.0, `esphome/build-action` v8.1.0) :
  - le manifeste lu par la tablette : `name`, `version`, `chipFamily` = « ESP32-P4 »,
    `ota.path` et `ota.md5`, chemin relatif au manifeste ;
  - une mise à jour téléchargée passe par le backend OTA commun, qui vérifie la
    signature ;
  - `build-action` écrit le manifeste complet et accepte des substitutions.
- **Outils** : `tools/publication/preparer.py` (renomme par révision, contrôle projet,
  puce, version et empreintes), `tools/publication/pages.py` (canaux et site).
- **Tests** : `tests/test_publication.py` (12 cas) ; `test_sans_secret.py` et
  `test_rendu_host.py` adaptés (OTA en liste, version par substitution, packages de
  publication hors rendu).
- `esphome config` valide en local (aucune entité de mise à jour, `3.0.0-dev`) et publié
  (`esphome` + `http_request`, manifeste du bon canal et de la bonne révision).
- **Docs** : ADR-0022, installation (encadré « sans compiler », mises à jour), README
  (démarrage rapide), SECURITY, brouillons forum HA et Hackster (lien du flasheur),
  cartographie, inventaire.
- **À faire avant la première publication** : Axel pose le secret, Pages est activé
  (source « GitHub Actions »), puis `v3.0.0-rc.1` en pré-release.

### 2026-09-27 — Plus aucune entité à renseigner pour compiler (lot 6c-1)

Lot 6 de l'audit « ouverture », troisième partie, préalable au flasheur web : un binaire
publié ne lit pas de `user_entities.yaml`, donc chaque entité HA qu'il appelle doit avoir
un défaut qui existe chez tout le monde.

- **Trois défauts de plus dans `Tab5/tab5-scripts.yaml`**, à côté du satellite et du
  lecteur média :
  - `entity_tab5_pipeline_select` (boutons Domotique / Discussion) :
    `select.m5stack_tab5_home_assistant_hmi_assistant`, dérivé par HA du nom livré ;
  - `entity_primary_active` et `entity_push_automation` (bouton « MAJ Écran » de la
    console) : `input_boolean.is_primary_active` et
    `automation.maj_ecran_tab5_esphome_push`, les noms que crée
    `packages/tab5_push.yaml`.
  - Ce sont les valeurs de l'installation de l'auteur : rien ne change sur sa tablette.
- **`user_entities.example.yaml`** : ces lignes deviennent des exemples commentés.
  Pour une installation standard, plus rien n'y est à remplacer.
- **Test** `test_entites_par_defaut_generiques` : défauts dérivés du nom livré de
  l'appareil ou présents dans le package, aucune clé `entity_…` active dans le modèle.
- `esphome config` valide avec le modèle seul : les trois défauts arrivent dans les
  appels HA, plus aucun `your_…`.
- Docs : installation (EN/FR, étape 2).
- **Reste propre à l'auteur** : le nom du pipeline « Discussion LLM » du bouton
  Discussion.

### 2026-09-27 — Rendu hors tablette : captures stables (suite du lot 7)

- **Captures instables** : sur un run, la scène « pluie » en anglais est tombée en plein
  fondu entre le panneau pluie et le panneau des alertes. La carte centrale change de
  panneau toutes les 8 s, et la capture tombait à un moment différent de ce cycle selon
  le run.
- **Correctif, rendu seulement** : l'action `rendu_panneau` (`Tab5/rendu/bouchons.yaml`)
  arrête le rotateur et avance la carte, un pas à la fois comme un appui, jusqu'au
  panneau voulu. `tools/rendu/capturer.py` fixe un panneau par scène : planning, pluie,
  info. Rien ne change sur la tablette.
- **Heure figée** : `faketime` arrête l'horloge (sans « @ »). Une heure qui avance
  franchissait une minute pendant les ~64 s des trois scènes : la dernière capture
  passait de 07:45 à 07:46 (vu sur le premier run de ce correctif). « Dans 10 mn »
  devient exact pour le rendu comme pour le script ; références régénérées.
- ADR-0021 complété.
### 2026-09-27 — L'écran parle aussi allemand et néerlandais

Suite du lot 4 (langue). Choix d'après les statistiques publiques de Home Assistant
(26/09/2026) : l'Allemagne est le premier pays des installations (19 %), les
néerlandophones pèsent plus que les hispanophones ou les sinophones, et ces deux
langues tiennent dans les polices actuelles (Latin-1), sans flash en plus.

- `Tab5/lang/de.yaml` (`Deutsch`, index 2) et `Tab5/lang/nl.yaml` (`Nederlands`,
  index 3), complets : tout ce que l'écran affiche, jeux compris (sauf les questions
  du quiz, comme en anglais). Traductions faites par une IA et **pas encore relues
  par une personne dont c'est la langue** : corrections bienvenues.
- Le select « Langue » propose `Deutsch` et `Nederlands` à la suite : les index déjà
  mémorisés par les tablettes ne bougent pas.
- Le rappel parlé du réveil n'ajoute plus un « s » pour le pluriel (« minute%s ») :
  deux phrases, singulier et pluriel, parce que le pluriel n'est pas un « s » partout
  (Minuten, minuten).
- **Repli sur l'anglais** : dans une langue pas encore complète, un texte manquant
  s'affiche en anglais plutôt qu'en français (puis en français si l'anglais ne l'a
  pas). Le repli est écrit dans les tables par `tools/gen_i18n.py` : rien ne change à
  l'exécution. Une nouvelle langue peut donc arriver partielle ; l'anglais doit rester
  complet (nouveau test).
- Documentation : les phrases dites par la tablette suivent la langue de l'écran,
  mais la voix est celle du pipeline vocal de HA, à régler dans la même langue.

### 2026-09-27 — Textes de présentation prêts pour la 3.0 (lot 8)

Lot 8 de l'audit « ouverture » (communication), **docs seulement**. Choix d'Axel : forum
HA et Hackster seulement, textes préparés maintenant, publiés par lui après la 3.0 (flasheur
web du lot 6c), ton « partagé au cas où ».

- **`docs/press/forum_ha_en.md`** (nouveau) : brouillon pour « Share your Projects », en
  anglais, avec la liste de ce qu'il faut vérifier juste avant de publier (lien du
  flasheur, limites toujours vraies, image).
- **`docs/press/hackster_paste_en.md`** mis à jour pour la 3.0 :
  - étape 2 : flasheur web, puis ajout dans HA qui fournit la clé ; compilation avec une
    clé de signature au lieu de `secrets.yaml` ;
  - démo sans `--key` ;
  - étape 3 : blueprint pour choisir les appareils, 17 actions `tab5_maj_*`, exemple de
    payload au format réel ;
  - chiffres du firmware (17 packages, 45 fichiers de composants) et release 3.0.0.
- `docs/press/hackster.md` : renvoi vers le texte à jour.
### 2026-09-27 — L'écran dessiné sans la tablette : rendu sur PC et captures en CI (lot 7)

Lot 7 de l'audit « ouverture », ADR-0021. Choix d'Axel : comparaison informative, galerie
dans `docs/screens.md`, scènes en anglais en plus.

- **`tab5-rendu-host.yaml`** : les mêmes packages d'interface et les mêmes sources C++
  que la tablette, compilés pour la plateforme `host` d'ESPHome (Linux/macOS). LVGL
  dessine dans un affichage `snapshot` en mémoire ; l'action API `rendu_capture` écrit
  un BMP. Ne se flashe nulle part.
- **Bouchons, jamais dans le firmware** :
  - `Tab5/rendu/composants/` : composants de même nom qu'ESPHome, sans effet
    (`voice_assistant`, `micro_wake_word`, `speaker` et `microphone` qui tirent `audio`
    réservé à l'ESP32, `rtttl`, `online_image`, `http_request`) et `rendu_muet`
    (haut-parleur, micro et lecteur muets, actions et conditions sans effet) ;
  - `Tab5/rendu/bouchons.yaml` : écran, horloges, rétroéclairage, diagnostics lus par
    la console, `!extend`/`!remove` sur l'ampli et la prise casque ;
  - `Tab5/rendu/hote/`, `Tab5/rendu/freertos/` : en-têtes ESP-IDF remplacés.
- **La partie interface de l'`on_boot` est copiée**, la séquence protégée reste
  intacte ; `tests/test_rendu_host.py` exige chaque lambda telle quelle dans
  `tab5-ha-hmi.yaml`, les mêmes sources C++, et chaque package repris ou déclaré
  matériel.
- **Portabilité, même comportement sur la tablette** : `std::isnan` au lieu de `isnan`
  (`tab5_cards.cpp`, `tab5_forecast.cpp`, un lambda), branche Arduino morte retirée de
  la carte mémoire (`tab5_console.cpp`).
- **CI « Rendu hors tablette »** (`rendu-host.yml`, non requis) : compile, lance le rendu
  sous `faketime` (16/06/2026 07:45, heure de Paris, horloge monotone intacte), pousse
  les trois scènes du mode démo par la vraie API (`tools/rendu/capturer.py`), en
  français, puis en anglais après un vrai changement du select « Langue ». Compare aux
  références `docs/images/rendu/` (`tools/rendu/comparer.py`) : écarts signalés avec une
  image de différence, sans bloquer. `tools/rendu/maj_references.py --run <id>` accepte
  un changement voulu.
- **Déterministe** : deux runs identiques donnent les mêmes pixels (vérifié).
- **Premier défaut trouvé par le rendu** : la démo envoyait « Auj 16 », « Mer 17 »
  au lieu du jour seul que HA envoie et que la tablette traduit ; en anglais, les
  tuiles restaient en français. Corrigé dans `tools/demo/scenarios.py`, avec un test
  qui compare à `tab5_push.yaml`.
- **Docs** : galerie des captures (`docs/screens.md`), « Voir l'écran sans la
  tablette » (`docs/debugging.md`), ADR-0021, cartographie.

### 2026-09-27 — Un firmware sans aucun secret : clé API fournie par HA, firmwares signés (lot 6b)

Lot 6 de l'audit « ouverture », deuxième partie, **rupture** (3.0.0), ADR-0020 (remplace
l'ADR-0015). Un même binaire doit pouvoir servir à tout le monde (flasheur web du lot 6c).

- **Wi-Fi** : plus d'identifiants compilés. Improv par le port USB (`improv_serial`, depuis
  ESPHome Web ou le futur flasheur) ou portail de l'AP de secours, devenu **ouvert** (un mot
  de passe écrit dans un dépôt public ne protège rien). Le réseau reste en NVS d'une OTA à
  l'autre.
- **Clé API fournie par Home Assistant** : `api: encryption: {}` sans clé. HA la crée à
  l'ajout de la tablette, la lui envoie par une connexion déjà chiffrée et la garde.
  Fenêtre d'appairage `provisioning: timeout: 30min` : une tablette sans clé n'accepte ce
  premier contact que dans les 30 minutes qui suivent son démarrage.
- **Firmwares signés** : l'OTA n'est plus chiffrée (une API sans clé compilée ne peut pas
  la chiffrer) ni protégée par mot de passe, mais `signed_ota_verification` (RSA-3072) fait
  refuser tout firmware qui n'est pas signé par la clé de celui qui tourne. Clé privée
  `tab5_signature.pem` à la racine (gitignorée, comme l'était `secrets.yaml`), ou chemin dans
  la substitution `tab5_cle_signature`.
- **Fuseau horaire de HA** (`time: platform: homeassistant`), gardé en NVS et remis au
  démarrage (`fuseau_restaurer()` / `fuseau_memoriser()`, `tab5_services.cpp`) : le réveil
  sonne à l'heure locale même quand HA manque après une coupure. `tab5_fuseau` disparaît.
- **Identité** : `esphome: project: axellum.tab5-ha-hmi` version `3.0.0-dev`, pour le
  manifeste de mise à jour du lot 6c (qui apportera `update: http_request`).
- **Outils du PC** : la clé n'est plus dans le YAML.
  - `tools/tab5_cle_api.py` la trouve (`--cle`, `TAB5_CLE_API`, ou le fichier où HA la
    garde, `--config-ha`) sans jamais l'afficher ;
  - `tools/tab5_logs.py` remplace `esphome logs` ;
  - le mode démo donne sa propre clé à une tablette jamais ajoutée à HA
    (`tools/demo/cle_demo.txt`, gitignoré, la clé à donner ensuite à HA) ;
  - `tools/migrer_vers_3.py` envoie une fois la 3.0 avec l'ancienne clé d'un `secrets.yaml`
    2.x, que le firmware 2.x exige.
- **CI** : plus de `secrets.yaml` factice, une clé de signature jetable par run
  (`openssl genrsa`). `tools/verifier_secrets_config.py` refuse aussi un `.pem` / `.key`
  suivi et un en-tête de clé privée.
- **Tests** : `tests/test_sans_secret.py` (aucun `!secret` dans le firmware, API, OTA,
  Wi-Fi, fuseau, projet, CI, clé trouvée dans HA, ancienne clé) ; 2 tests de plus pour le
  vérificateur de secrets. pytest : 89 passent.
- **Docs** : installation (EN/FR : étape 3 « clé de signature », étape 5 « premier flash et
  Wi-Fi », étape 6 « ajouter la tablette à HA », OTA et journaux, « Passer à la 3.0 »),
  SECURITY, README, CONTRIBUTING, AGENTS, mode démo, débogage, architecture, inventaire,
  cartographie, ADR-0020.
- **Vérifié dans le code** d'ESPHome 2026.9.0 et de HA 2026.9.3 : fourniture de la clé par
  connexion à clé nulle, fenêtre d'appairage (coupe aussi l'AP à son expiration),
  réauthentification de HA quand une tablette perd sa clé (« chiffrement désactivé »,
  confirmer, puis nouvelle clé), identifiants Wi-Fi rangés par hash de config seulement
  quand le YAML en a. `esphome config` valide.
- **Essai sur la tablette** (27/09, Axel sur place, ESP32-P4 rev1.3, antérieure à la v3) :
  - migration depuis `main` @ `b13237b` : `tools/migrer_vers_3.py` à 16:30, OTA chiffrée
    avec l'ancienne clé acceptée ; la tablette redémarre sans réseau, Wi-Fi donné par
    « Tab5 Fallback AP » depuis un téléphone, réauthentification « chiffrement désactivé »
    confirmée dans HA, qui fournit une nouvelle clé (différente de l'ancienne) : connectée
    à 16:33, moins de 3 minutes après le démarrage ;
  - aucune entité indisponible, l'automatisation des emplacements pousse les 34
    emplacements et la clim à la connexion, **écran complet (Axel)** ;
  - la tablette refuse ensuite la clé nulle et le clair ; `tools/tab5_logs.py` lit son
    journal avec la clé gardée par HA ;
  - **signature** : le firmware signé par la clé du projet passe en OTA (en clair) ; un
    firmware signé par une autre clé et un firmware non signé sont refusés (« Firmware
    signature verification failed »), la tablette reste sur le sien sans redémarrer ;
  - **fuseau** : « Fuseau du dernier passage de HA remis (UTC+1 h en hiver) » au
    démarrage suivant, 6 s avant le Wi-Fi. Lu sur l'USB : le journal par l'API arrive
    trop tard pour ces lignes.
- **Mesures** (build local, ESPHome 2026.9.0) : image 3 141 222 o (+23,8 Ko), RAM
  statique 170 626 o. Aucun avertissement dans le code du projet.
- **À savoir** :
  - une tablette 2.x migre une fois, sur place (docs/installation.md, « Passer à la 3.0 ») ;
  - clé de signature perdue = plus d'OTA, seulement l'USB ;
  - `esphome logs` ne trouve plus de clé : `tools/tab5_logs.py`.

### 2026-09-27 — Les appareils se choisissent dans HA, à la souris : emplacements et blueprint (lot 6a)

Lot 6 de l'audit « ouverture » (firmware générique), première partie, **rupture** (3.0.0)
(ADR-0019). Choix d'Axel : blueprint HA, firmwares signés, nom `tab5-ha-hmi` gardé,
emplacements d'abord, appareils seulement.

- **La tablette ne connaît plus aucune entité de la maison.** Elle parle en emplacements
  (`lumiere_1` à `lumiere_3`, `pc`, `tv`, `telephone`, `salon`, `serre`, `pot_1` à
  `pot_5` et leurs détails, `clim`, `volet`, `planning`).
  - Les 37 capteurs `platform: homeassistant` deviennent des `template` internes,
    alimentés par la nouvelle action `tab5_maj_emplacements` (« clé|état|valeur;… »).
    Leurs `on_value` n'ont pas changé.
  - Les commandes nommant une entité (lumières et popup lumière, clim, TV, PC, volet)
    passent par un script unique, `tab5_action`, qui émet l'événement
    `esphome.tab5_action` (emplacement, action, valeur). Un événement n'exige pas
    l'option « autoriser les actions HA ».
  - Restent des actions HA, parce qu'elles ne visent aucune entité de la maison :
    annonces et arrêt de la voix de la tablette, pipeline vocal, agenda, réveil,
    acquittement des alertes, console système.
- **Blueprint « Tab5 — emplacements »**
  (`HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml`) :
  - un sélecteur par emplacement, filtré par domaine et classe, rangé en sections, tous
    facultatifs ; un seul choix par pot (conductivité, éclairement, température et
    batterie sont pris sur l'appareil du capteur d'humidité, par classe, vérifié sur des
    Flower Care) ;
  - une automatisation par tablette : elle pousse tous les emplacements à la connexion
    et un seul à chaque changement, pousse la clim et le volet qui signale sa course,
    exécute les commandes et répond aux zones (lot 5 : emplacement vide = zone absente).
  - Changer d'appareil = modifier l'automatisation dans HA : ni flash ni redémarrage.
- **Package `tab5_push.yaml`** : il perd ce que le blueprint reprend (réponse des zones,
  poussée de la clim, scripts `allumer_leds` et `allumer_pc_tv`). Placeholders
  `VOTRE_CLIMATISATION`, `VOTRE_LEDS` et `VOTRE_PC` retirés.
- **`user_entities.yaml`** : les clés des appareils disparaissent ; celles d'un fichier
  existant sont simplement ignorées.
- **Mode démo** : il pousse les emplacements (`tab5_maj_emplacements`) au lieu de
  répondre à des abonnements, et journalise les commandes `esphome.tab5_action`.
- **Tests** :
  - `tests/test_emplacements.py` : clés du blueprint = table du firmware, chaque
    commande de l'écran a sa branche, entrées facultatives, plus aucun abonnement ;
  - `test_zones.py` : la réponse vient du blueprint, plus du package ;
  - `test_demo.py` : emplacements de la démo = table du firmware ;
  - pytest : 77 passent.
- **Vérifié sur le HA d'Axel** : tous les modèles du blueprint (le filtre `extract`,
  d'Ansible, n'existe pas dans HA : remplacé par une boucle), rendu du blueprint avec
  des entrées vides.
- **Essai sur la tablette** (27/09) :
  - package déployé (sauvegarde `.bak_20260927_lot6a`) et automatisation créée à partir
    du blueprint avec les entités d'Axel ; firmware flashé à 13:07 ;
  - à la connexion, 34 emplacements poussés avec les vraies valeurs (pots 4-5 et LEDs
    indisponibles : « -- », zones gardées), clim poussée, volet laissé au package, zones
    `absentes: ""` ;
  - **écran et commandes validés par Axel** (lampes, popup, clim, TV).
- **À savoir** : le blueprint exige le firmware 3.0. Avec un plus ancien, l'action
  `tab5_maj_emplacements` n'existe pas, et `continue_on_error` ne rattrape pas une
  action inexistante (vu juste avant le flash).
- **Mesures** (build local, ESPHome 2026.9.0) : image −12,6 Ko (3 116 876 o), RAM
  statique −1,4 Ko, sans les 37 abonnements HA. Aucun avertissement dans le code du
  projet.

### 2026-09-27 — ST7121 : le couple écran + tactile signalé fonctionnel par un tiers (docs)

- **Docs seulement**, ni firmware ni HA. Sur esphome/esphome#17471 (11/07/2026), un
  utilisateur qui testait la PR ESPHome du modèle ST7121 signale que le couple
  `M5STACK-TAB5-ST7121` + tactile `st7123`, celui de `Tab5/ecran-st7121.yaml`, marche sur
  une vraie ST7121 : écran et tactile, en paysage.
- `docs/hardware.md` et `README.md` (EN/FR) : statut « Compile, non testée **ici** », lien
  vers ce retour ; le choix du tactile n'est plus présenté comme une simple hypothèse.
  Ce firmware n'a toujours pas tourné sur une ST7121.
- En-tête de `Tab5/ecran-st7121.yaml` et `CARTOGRAPHIE_TAB5.md` alignés.

### 2026-09-27 — Mode démo « maison minimale », guide « Adapter à sa maison » (lot 5c)

Fin du lot 5 : **ni firmware ni HA**, l'outil de démo et la documentation.

- **Mode démo, option `--maison-minimale`** : la démo se comporte comme un HA sans clim,
  TV, téléphone, troisième lumière, serre, pots 3 à 5, volet ni agenda de travail.
  - Elle ne répond pas pour ces entités.
  - Elle répond à la demande `esphome.tab5_zones` et envoie aussi la réponse d'office
    à sa connexion, car la tablette ne redemande qu'à une connexion de HA.
  - Elle ne pousse rien pour la clim ni le volet.
  - Sans l'option, elle répond « rien ne manque » : une démo complète rétablit donc les
    zones d'une démo minimale.
- **Démo alignée sur le contrat actuel** :
  - l'entité PC suit le modèle `user_entities.example.yaml` (`switch.…`, elle ne
    répondait plus depuis le 16/07) ; la TV est simulée ;
  - pluie et bandeau passent aux codes du lot 4c (`@niveau,début` calculé à l'envoi,
    `@ha|…`) : la démo suit la langue de l'écran ;
  - horaires au format `HH:MM-HH:MM` (la démo envoyait `09h00 - 17h30`) ;
  - plus de `tab5_maj_planning`, que HA n'envoie plus depuis le 08/09.
- **`tests/test_demo.py`** (5 tests) : entités simulées = modèle, clés de zones = celles
  de la tablette, deux modes à blanc, format des codes et des horaires.
- **Docs** :
  - `docs/installation.md` (EN/FR) : section « Adapter à sa maison » (tableau des zones,
    réglage, diagnostic, limites) ;
  - étape 2 : fin de la phrase « aucun autre YAML à modifier » et de l'exemple PC périmé ;
  - étape 4 : `tab5_meteo_sources.yaml` est obligatoire depuis la 2.2.0 ;
  - `docs/demo_mode.md` (maison minimale ; 14 entités miroir au lieu de « 15 ») ;
  - README (« Avant de commencer ») et README de `HomeAssistant_Config/` (zones, bandeau
    `@ha|…`).
- **Vérifié dans le code de HA** (`config_validation.py`) : un placeholder laissé tel
  quel (`climate.VOTRE_CLIMATISATION`) est mis en minuscules à la validation. Il donne
  donc une entité inexistante, c'est-à-dire une zone absente, sans faire refuser le
  package.

### 2026-09-27 — HA n'envoie plus rien pour une zone absente (lot 5b)

Suite du lot 5, **HA seulement, aucun flash**. La tablette masque déjà ces zones
(lot 5a) ; HA cesse d'y pousser des valeurs ou d'appeler des entités absentes.

- **Clim** : `tab5_push_clim` s'arrête sans entité climat. Il poussait une fausse clim
  à 20.0 (`| float(20)`) à chaque connexion.
- **Volet** : `tab5_push_volet` s'arrête sans volet ou sans le package
  `volet_serre_tracking.yaml` (même règle que la zone « volet »). Il poussait
  « unknown ».
- **`allumer_leds`** et **`allumer_pc_tv`** : chaque entité seulement si elle existe.
  Une maison sans TV garde son PC, et inversement.
- **Pluie, source Météo-France sans l'intégration** (le choix par défaut) : code
  « aucune source » (`@-`), comme « Aucune », au lieu de « temps sec » (`@0,0`). Sans
  effet visible jusqu'ici, car la phrase ne s'affiche que dans le panneau pluie, qui
  ne tourne que s'il pleut. Une entité indisponible garde l'ancien comportement.
- **Pas touché** : une clim *indisponible* (qui existe) reçoit toujours 20 par défaut.
  Une consigne « nan » serait plus honnête, mais les boutons ± de la tablette en
  feraient une consigne invalide envoyée à HA : c'est un changement de firmware, à
  faire à part.
- **Vérifié sur le HA d'Axel** : toutes les gardes sont vraies (rien ne change chez
  lui), et fausses pour une entité inventée.

### 2026-09-27 — Zones optionnelles : une zone sans entité disparaît de l'écran (lot 5a)

Lot 5 de l'audit « ouverture » : l'écran ne suppose plus la maison de l'auteur. Chez
quelqu'un sans clim, une fausse clim à 20.0 s'affichait (le `| float(20)` de HA sur une
entité absente). Une lampe sans entité paraissait « éteinte », et 1 à 3 capteurs de
plantes se répétaient sur les 4 emplacements. Tous les boutons appelaient des entités
inexistantes. **Firmware et package HA ensemble**, le firmware d'abord (ADR-0018).

- **Retirer une zone = mettre sa ligne en commentaire** dans `Tab5/user_entities.yaml`.
  Le nouveau package `Tab5/tab5-zones.yaml` donne à chaque `entity_…` une valeur par
  défaut qui n'existe dans aucun HA. Une clé de l'utilisateur l'emporte (vérifié sur
  ESPHome 2026.9.0 par `esphome config`, sur une « maison minimale »).
- **HA confirme, la tablette demande** :
  - une fois par connexion, à la première poussée des prévisions, la tablette envoie
    ses entités (`esphome.tab5_zones`) ;
  - l'automatisation `tab5_zones_reponse` (`tab5_push.yaml`) répond celles qui
    n'existent pas (`tab5_maj_zones`). Une entité « unavailable » existe : sa zone
    reste. HA ajoute seul `clim`, `volet` (entité ou package absent) et `planning`
    (pas d'agenda de travail) ;
  - raison, vérifiée dans le `manager.py` de l'intégration ESPHome de HA : une entité
    créée après l'abonnement de la tablette n'est transmise qu'à son prochain
    changement. Pendant le démarrage de HA, un silence ne prouve donc rien, et une
    lampe aurait pu disparaître des heures durant ;
  - une donnée reçue fait toujours revenir sa zone. Sans le package, rien ne disparaît.
- **Ce qui disparaît** :
  - bandeau d'état : PC, téléphone (les icônes restantes se resserrent) ;
  - bouton TV et télécommande. Sans TV, HA et Sys glissent d'une colonne, et l'épaule
    de la tuile J0 suit le PC ;
  - boutons et épaules des tuiles J0 à J4 et cartes du calque « HA » (recentrées) ;
  - lampes du sélecteur du popup lumière et de « Tout éteindre » ;
  - − / consigne / + de la clim ;
  - température du salon ;
  - sans serre, l'icône devient une manette : l'arcade reste à sa place ;
  - pots de l'accueil (jusqu'à 4, un emplacement chacun, plus de doublon ; le résumé
    « 2 plus secs, médiane, plus humide » reste à 5) et cartes du popup (recentrées) ;
  - planning : il sort du rotateur de la carte centrale, qui reste vide s'il n'a rien
    d'autre à montrer ;
  - « Aller à l'écran » (HA) n'ouvre plus la clim, les plantes ou la TV absentes.
- **Pas de clignotement** : la liste est gardée en NVS et appliquée dans `on_boot`
  (priorité -100, une ligne ajoutée avec l'accord d'Axel), avant la première image.
- **Diagnostic** : capteur « Zones masquées » (« aucune », ou « clim, pot_4… »). Une
  faute de frappe dans un entity_id s'y voit.
- **Tests** : `tests/test_zones.py` compare les clés aux quatre endroits (enum,
  `kCles`, demande, automatisation HA), les valeurs par défaut et les `zone_vue()`.
  Falsifié : une clé renommée le fait échouer. Gabarit HA essayé sur le HA d'Axel
  (entités inventées signalées, réelles non).
- **Essai sur la tablette** (27/09) :
  - firmware d'Axel flashé à 11:53 : demande envoyée après une reconnexion, réponse
    `absentes: ""`, rien ne disparaît ;
  - build de test « maison minimale » (clim, TV, téléphone, LEDs, serre et pots 3 à 5
    en commentaire) flashé à 11:58. HA a répondu exactement ces 7 zones, puis clim,
    volet et planning ont été envoyés à la main. **Écran validé par Axel** ;
  - retour au firmware d'Axel à 12:02 (`esphome upload --file`, sans recompiler) :
    toutes les zones sont revenues, « Zones masquées » = « aucune ».
- **Mesures** (build local, ESPHome 2026.9.0) : image +7,8 Ko (3 129 532 o), aucun
  avertissement dans le code du projet.

