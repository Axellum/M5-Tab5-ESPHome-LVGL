# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Dates are the day each pull request was merged into `main`.

## [Unreleased]

Pré-releases tirées de cette section, sur le canal bêta :
[v3.8.0-rc.1](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/releases/tag/v3.8.0-rc.1)
le 07/10/2026 : roue d'actions rapides à deux anneaux (#378), « Son de la tablette » dans la liste
de la tuile − / + (#379).

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

## [3.6.0] — 2026-10-05

De `v3.5.0` à aujourd'hui : vingt-huit pull requests (#296 → #319, #321 → #325), dont sept nées
des idées et des retours de @husyildiz (discussion #278 : #302 à #305, #308 à #310) — merci
à @husyildiz —, et celle de la release (#320).
- **Vingt et un thèmes, chacun clair ou sombre** (#312, #314 à #318, #324, #325, [ADR-0029](docs/decisions/0029-themes-palette.md)) :
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
- **Énergie solaire** (#308, #310, [ADR-0028](docs/decisions/0028-solar-energy-popup.md)) : popup Énergie (installation en direct,
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
d'affichage ([ADR-0029](docs/decisions/0029-themes-palette.md), « lot 3 »).
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
seize thèmes arrivent dans la PR suivante ([ADR-0029](docs/decisions/0029-themes-palette.md), section « lot 3 »).
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
([ADR-0029](docs/decisions/0029-themes-palette.md), section « lot 2 »).
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
change **rien à l'écran** : il rend les couleurs changeables ([ADR-0029](docs/decisions/0029-themes-palette.md)).
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
  l'[ADR-0028](docs/decisions/0028-solar-energy-popup.md).
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
  [ADR-0028](docs/decisions/0028-solar-energy-popup.md)) : en haut, l'installation en
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
- **Toutes les clims ont leur popup** (#263, [ADR-0027](docs/decisions/0027-climate-per-tile.md)) :
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

- **Chaque tuile de clim ouvre le popup pour SA clim** ([ADR-0027](docs/decisions/0027-climate-per-tile.md)).
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
- **Clim de toutes marques** (#257, [ADR-0026](docs/decisions/0026-climate-from-device.md)) :
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

- **Le popup clim suit l'appareil** ([ADR-0026](docs/decisions/0026-climate-from-device.md)).
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

Installer le côté Home Assistant ne demande plus ni dépôt ni Python ([ADR-0024](docs/decisions/0024-packages-without-placeholders.md)).
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

Le guide pas à pas : [« Passer à la 3.0 »](docs/installation.md#passer-à-la-30).
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
  ([débogage](docs/debugging.md)).
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

## [2.2.0] — 2026-09-27

De `v2.1.0` (27/09 au matin) à aujourd'hui : 6 pull requests (#181, #183 → #187), plus
celle de la release. Le lot 4 de l'audit « ouverture » est terminé :
- **l'écran parle français ou anglais**, au choix depuis HA : écran, jeux et textes
  envoyés par HA compris. Seules les questions du quiz restent en français ;
- **la météo ne dépend plus de Météo-France** : la source des prévisions, celle de la
  pluie dans l'heure et celle des vigilances se choisissent dans HA (OpenWeatherMap,
  MeteoAlarm, toute entité `weather.*`) ;
- le fuseau horaire est réglable.

**Version mineure** : le plancher reste ESPHome 2026.9.0, et une OTA suffit. Mais le
package HA et le firmware changent **ensemble** : voir l'ordre ci-dessous.

### À faire en mettant à jour depuis 2.1.0

- **Le firmware d'abord, les packages HA ensuite.** Les nouveaux packages envoient
  des codes (`@2,…`, `@ha|…`) que seul le nouveau firmware compose en texte. Un
  firmware 2.1.0 les afficherait tels quels. Dans l'autre sens, le nouveau firmware
  accepte encore les phrases de l'ancien package.
- **Packages HA** : reprendre `tab5_push.yaml`, `tab5_alerts.yaml` et
  `tab5_reveil.yaml`, et **ajouter `tab5_meteo_sources.yaml`** (nouveau). Sans lui,
  la pluie, les vigilances et les prévisions n'ont plus de source.
- **Placeholders** : ajouter `VOTRE_METEO_OWM` et `VOTRE_METEOALARM`, en entity_id
  complets. Les valeurs par défaut conviennent même sans ces intégrations
  (`weather.openweathermap`, `binary_sensor.meteoalarm`). Sans elles, HA refuse le
  package, car un placeholder non remplacé n'est pas un entity_id valide.
- **Recharger** input_select, input_text, template, script et automation (ou
  redémarrer HA).
- **Entité remplacée** : `sensor.phrase_prochaine_pluie` devient
  `sensor.tab5_pluie_dans_l_heure`. Supprimez l'ancienne du registre de HA.
- **Nouveau, facultatif** :
  - select « Langue » de la tablette (un changement redémarre la tablette) ;
    `tab5_langue:` pour le premier démarrage ;
  - `tab5_fuseau:` dans `Tab5/user_entities.yaml` (défaut Europe/Paris) ;
  - les trois listes « Tab5 · source … » dans HA.

### Mesures de la version

- **Compilations de la CI** (ESPHome 2026.9.0), firmware de `v2.1.0` (`c134dfb`)
  contre celui de ce tag :
  - image 3 070 188 → 3 122 396 o (+51 Ko). La langue en fait l'essentiel : 4a
    +14,6 Ko, 4b +30,9 Ko (690 clés), 4c-1 +5,1 Ko ;
  - RAM statique 170 752 → 170 992 o (+240 o) ;
  - aucun avertissement dans notre code.
- **Base HA et poussées** : plus de poussée chaque minute avant une averse. La
  tablette décompte « dans N mn » elle-même.
- **Firmware de la release** : `main` à ce tag est le firmware flashé sur l'appareil
  de l'auteur le 27/09 à 09:55 (OTA, binaire compilé depuis le même arbre, aucun
  fichier firmware changé depuis). Validé à l'écran par Axel, en français puis en
  anglais.

### Problèmes connus

- **Questions du quiz** : en français, par choix. Le sous-titre anglais du jeu le
  dit.
- **Annonce parlée du réveil** (« Bonjour. Il est… ») : composée en français par HA
  (`tab5_reveil.yaml`), elle ne suit pas la langue de la tablette. La voix de
  synthèse est elle aussi réglée en français.
- **Fournisseurs météo** :
  - OpenWeatherMap essayé en réel, mais ses prévisions journalières ne couvrent que
    8 jours : les derniers jours des pages de 15 jours restent vides ;
  - MeteoAlarm (une alerte à la fois) et le regroupement en demi-journées (NWS)
    n'ont été testés qu'avec des données simulées.
- **ST7121 et ILI9881C** : toujours jamais essayées sur une tablette.
- **Flipper** : en portrait, les logs LVGL sont inondés tant qu'un doigt est posé.
- **Garde « reboot inattendu »** : elle notifie aussi les redémarrages voulus
  (changement de langue, OTA).

### 2026-09-27 — Source des prévisions au choix, heures locales (lot 4c-3)

Lot 4c de l'audit « ouverture », troisième partie, demandée par Axel avant la release :
les prévisions et la météo du moment ne dépendent plus d'une entité figée à
l'installation. **HA seulement, aucun flash.**

- **Liste « Tab5 · source des prévisions »** (template `select`) : elle propose les
  entités `weather.*` présentes dans HA. Le choix est gardé dans
  `input_text.tab5_meteo_previsions`. Choix vide ou entité disparue :
  `weather.VOTRE_VILLE`, sinon la première entité météo (vérifié).
- **`sensor.tab5_meteo`** (normalisé) : condition du moment en état, et en attributs
  `temperature`, `humidite`, `uv`, `gel`, `neige`, ainsi que ce que l'entité sait
  fournir (`type_jours`, `heures_ok`, lus sur `supported_features` : 1 jours,
  2 heures, 4 demi-journées, vérifié dans le code de HA 2026.9.3). L'entité est
  recalculée dans chaque modèle : `this` porterait l'état précédent, et un changement
  de source n'aurait pris effet qu'au rendu suivant.
- **Poussée complète** :
  - elle ne demande que les types de prévisions gérés : jours, sinon demi-journées
    (NWS), sinon heures (OpenWeatherMap gratuit). Les demi-journées et les heures
    sont regroupées par date locale (max, min, condition de la période de jour) ;
  - un type non géré était rattrapé par `continue_on_error`, mais écrit dans le
    journal à chaque passage (vérifié dans `helpers/script.py`) ;
  - sans prévisions horaires, la page horaire reste vide, sans erreur.
- **Bug corrigé : heures UTC sur les tuiles horaires.** `strftime('%H:00')`
  s'appliquait à la date UTC de Météo-France : la tuile « 09:00 » portait la
  prévision de 11:00 (2 h de retard l'été, relevé dans la trace du 27/09 à 10:17).
  Passage par `as_local`.
- **Icône flocon** sans capteur Météo-France : elle suit la condition du moment
  (neige). Le réveil lit sa température sur `sensor.tab5_meteo`.
- **Tests** :
  - Météo-France : mêmes valeurs qu'avant (UV 2, gel 0, neige 0, humidité 90,
    `daily`, heures disponibles) ;
  - choix et repli testés en production ;
  - **OpenWeatherMap essayé en réel** : Axel a ajouté l'intégration pendant le lot.
    - prévisions : jours et heures envoyés sans erreur, heures locales justes ;
      l'API ne donne que 8 jours de prévisions journalières, donc les jours 8 à 14
      partent vides ;
    - pluie dans l'heure (`get_minute_forecast`) : réponse réelle au format attendu
      (60 créneaux, clé = entity_id), `@0,0` un jour sec ;
  - regroupement NWS et heures de 3 h simulés : températures justes. La condition
    du jour prenait d'abord la période de nuit (18 h) ; corrigé en préférant
    `is_daytime`, puis revérifié.
- **Docs** : `docs/installation.md` (EN/FR), README de `HomeAssistant_Config/`,
  `placeholders.example.yaml`, cartographie. Sources remises sur Météo-France après
  les essais.

### 2026-09-27 — Choix du fournisseur météo dans Home Assistant (lot 4c-2)

Lot 4c de l'audit « ouverture », seconde partie : la pluie dans l'heure et les
vigilances ne dépendent plus de Météo-France. **HA seulement, aucun flash** : le
firmware de 4c-1 accepte déjà les niveaux de pluie chiffrés et les deux phénomènes
supplémentaires.

- **Nouveau package `packages/tab5_meteo_sources.yaml`** :
  - Deux `input_select` choisissent la source **dans HA, sans YAML** :
    - « Tab5 · source de la pluie dans l'heure » : Météo-France, OpenWeatherMap ou
      Aucune ;
    - « Tab5 · source des vigilances » : Météo-France, MeteoAlarm ou Aucune.
  - Deux capteurs **normalisés**, seuls lus par les poussées :
    - `sensor.tab5_pluie_dans_l_heure` : code `@niveau,début`, attribut `barres` ;
    - `sensor.tab5_vigilance` : niveau global, attribut `phenomenes` (11 cases).
- **Faits vérifiés** dans le code de HA 2026.9.3 et la documentation des
  fournisseurs, le 27/09/2026 :
  - Hors de France, seul **OpenWeatherMap** en mode v3.0 expose une série de pluie
    minute par minute (`openweathermap.get_minute_forecast`, en mm/h, lue dans le
    cache de l'intégration). HA l'interroge toutes les 10 min. L'offre donne 1 000
    appels par jour gratuits, puis 0,0014 € par appel. One Call 3.0 est marqué
    « deprecated » ; la 4.0 est recommandée, sans date d'arrêt annoncée.
  - **MeteoAlarm** : 39 pays européens, intégration en YAML seulement, une seule
    alerte à la fois (la bibliothèque s'arrête à la première), `on` seulement si
    l'alerte n'a pas expiré. Codes `awareness_type` 1 à 13, relevés dans le code de
    la carte MeteoalarmCard.
  - Les capteurs de gabarit à déclencheurs acceptent `actions:`, dont la réponse
    sert aux modèles.
- **Pluie OpenWeatherMap** : premier créneau ≥ 0,1 mm/h = début. Seuils : < 2,5
  faible, < 7,6 modérée, < 50 forte, au-delà très forte. Chaque barre prend le
  maximum de sa fenêtre.
- **Vigilances MeteoAlarm** : chaque type est rangé dans une case de la tablette,
  brouillard et feux de forêt compris. Niveau 2 jaune, 3 orange, 4 rouge.
- **Tests dans le moteur de modèles de HA**, avec des données simulées :
  - OpenWeatherMap : pluie dans 13 min à 3 mm/h, puis 9 mm/h, puis 0,5 mm/h. On
    obtient `@2,…` début dans 13,0 min et les barres `0,0,2,2,3,3,1,1,0` ;
  - MeteoAlarm, 6 cas : brouillard orange, pluie jaune, feux rouge, niveau 1 actif,
    type inconnu, pas d'alerte. Les cases et les niveaux sont ceux attendus.
  - **Aucune des deux intégrations n'est installée chez l'auteur** : pas de test
    réel avec leurs données.
- **Testé en production, source par source** (package déployé le 27/09 vers 10 h 15) :
  - Météo-France : même sortie qu'avant (`@0,0`, mêmes barres). Seule différence, la
    vigilance part sur 11 cases au lieu de 9 ;
  - « Aucune » : `@-` et 9 barres vides ;
  - retour à Météo-France : `@0,0`.
  - **OpenWeatherMap choisi sans l'intégration** : HA lève « Action … not found », et
    `continue_on_error` ne rattrape PAS cette erreur. Le capteur restait figé, avec
    une erreur écrite toutes les 5 min. Correctif : l'action n'est lancée que si
    l'entité existe (`has_value`). Revérifié : `@-1,0` (« Pas de données »), barres
    vides, plus aucune erreur.
- **Placeholders** `VOTRE_METEO_OWM` et `VOTRE_METEOALARM`, en entity_id
  **complets**. « openweathermap » seul apparaîtrait aussi dans le nom de l'action,
  et `render_ha_config.py --check` y verrait une fuite. `VOTRE_VILLE` désigne
  désormais l'entité météo de n'importe quel fournisseur.
- **`tab5_push.yaml`** lit les capteurs normalisés : barres, code de pluie,
  vigilance et bandeau info. Les déclencheurs sont sur ces capteurs. Le capteur de
  pluie déménage dans le nouveau package, avec le même `unique_id`.
- **Nettoyage de `tab5_alerts.yaml`** : un identifiant d'acquittement qui n'est pas
  une entité (sans « . ») ne reste plus pour toujours. Les deux identifiants de test
  du 27/09 y étaient restés ; ils ont été retirés à la main.
- **Docs** : `docs/installation.md` (section « Fournisseurs météo », EN/FR), README,
  README de `HomeAssistant_Config/`, `placeholders.example.yaml`, cartographie.
- **Entité supprimée** du registre de HA (accord d'Axel) :
  `sensor.phrase_prochaine_pluie`, remplacée depuis 4c-1.

### 2026-09-27 — Textes de HA composés par la tablette, dans sa langue ; fuseau réglable (lot 4c-1)

Lot 4c de l'audit « ouverture », première partie. Home Assistant écrivait lui-même,
en français, trois textes affichés par la tablette. Il envoie désormais des **codes**,
et la tablette compose le texte dans sa langue avec `tr()`. Tout ce que l'écran
affiche suit donc la langue choisie, sauf les questions du quiz. La seconde partie
(4c-2 : choix du fournisseur météo) s'appuie sur ces codes.

- **Pluie dans l'heure** : le capteur « Phrase Prochaine Pluie » devient
  « Tab5 Pluie dans l'heure », dont l'état est un code `@niveau,début`.
  - niveau -1 = pas de données, 0 = sec, 1 à 4 = faible à très forte, 5 = intensité
    inconnue ; début = epoch UTC, 0 s'il pleut déjà ; `@-` = pas de source.
  - La tablette écrit « Averses dans 12 mn » / « Showers in 12 min » et **décompte
    les minutes elle-même**, au tick de son horloge (`rain_phrase_tick()`).
  - L'ancien capteur appelait `now()` : il changeait chaque minute avant une averse
    et relançait à chaque fois la poussée légère. Le nouveau ne change qu'avec les
    données Météo-France.
- **Bandeau info** : `@ha|nb MAJ|titre|nb erreurs|nb indispo|jaune|vigilance`. **Rotateur
  d'alertes HA** : `@maj:titre` et `@indispo:n`.
- **Bug corrigé** : le firmware choisissait le bandeau « Alerte Météo … » d'après la
  COULEUR du texte. Or une mise à jour HA (Orange) ou une erreur (Rouge) prennent
  aussi ces couleurs : avec une vigilance verte, « 1 MAJ · … » s'affichait donc
  « Alerte Météo Orange en cours ! » (présent depuis #36, 14/07/2026). Le code dit
  maintenant la vigilance à part. Autre gain : après un tap sur le bandeau
  vigilance, la ligne HA s'affiche aussitôt, sans attendre HA.
- **Français à l'identique**, vérifié dans HA (`ha_eval_template`) :
  - l'ancien modèle et la composition de la tablette donnent le même texte sur
    6 cas de pluie (sec, pluie en cours, dans 12 min, dans 32 min, créneau absent,
    pas de données) et 9 cas de bandeau (MAJ, erreurs, indispos, vigilances jaune,
    orange et rouge, acquittées ou non) ;
  - le cas réel du moment (sec) donne `@0,0`.
- **Compatibilité** : un texte sans `@` (ancien package HA) s'affiche comme avant.
  **Ordre de déploiement : le firmware d'abord, puis le package HA.** Un firmware
  plus ancien afficherait les codes tels quels.
- **Prêt pour d'autres fournisseurs (4c-2)** :
  - les barres de pluie acceptent un niveau chiffré « 0 » à « 4 » en plus des
    libellés Météo-France ;
  - la vigilance accepte deux phénomènes de plus en fin de payload, Brouillard et
    Feux de forêt (types MeteoAlarm sans case Météo-France). Leurs glyphes MDI
    `weather-fog` (F0591) et `fire` (F0238) sont vérifiés dans le TTF et ajoutés à
    `mdi_font_alert`.
- **Fuseau réglable** : `tab5_fuseau:` dans `Tab5/user_entities.yaml` (`sntp` et
  RX8130), par défaut `Europe/Paris`. `esphome config` donne
  `CET-1CEST,M3.5.0,M10.5.0/3` sans la clé, et `EST5EDT,M3.2.0,M11.1.0` avec
  `-s tab5_fuseau America/Montreal`.
- **Traductions** : 13 clés (bandeau, alertes, phrase de pluie).
- **Docs** : `docs/translations.md`, README, `docs/installation.md`, README de
  `HomeAssistant_Config/`, contrat API (`tab5-api-logic.yaml`),
  `user_entities.example.yaml`, cartographie.
- **Mesures** (ESPHome 2026.9.0 ; build CI de `main` @ `9eb732e` contre build local) :
  - image 3 116 684 → 3 121 772 o (+5,1 Ko : deux glyphes de 60 px, le code, 13
    traductions) ;
  - RAM statique +24 o (état de la phrase de pluie) ;
  - aucun avertissement dans notre code.

### 2026-09-27 — Arcanoïde : « Effacer les scores » demande vraiment confirmation

Défaut signalé pendant le lot 4b : le bouton annonçait « Appuie pour confirmer », mais
le classement était effacé dès le premier appui.

- **Écran de confirmation**, comme Coureur d'Or : « Effacer les scores ? », « Oui, tout
  effacer » / « Annuler ». Les deux reviennent au classement.
- **« Annuler » prend la place du bouton « Effacer les scores »** : un double appui ou
  un rebond tombe sur Annuler, jamais sur le Oui.
- **Textes** : la description du bouton devient « Demande confirmation » (clé déjà
  traduite pour Coureur d'Or) ; deux clés nouvelles dans `Tab5/lang/en.yaml`,
  « Appuie pour confirmer » retirée.

### 2026-09-27 — Les huit jeux en français ou en anglais (lot 4b)

Lot 4b de l'audit « ouverture ». Les consoles suivent la langue choisie dans HA, comme
le reste de l'écran depuis le lot 4a. Seules les questions du quiz restent en français
(choix d'Axel).

- **Textes des jeux** : les 8 consoles et la page Arcade passent par `tr()`. Cela fait
  690 clés de plus dans `Tab5/lang/en.yaml`, 870 au total.
  - **`tr_noop()`** (nouveau, `tab5_i18n.h`) marque un texte rangé dans une table.
    Il ne traduit rien : la traduction se fait à l'affichage, `tr(table[i])`.
  - **Contextes** :
    - `echecs|` pour les pièces (« Dame » = Queen, alors qu'aux dames c'est un roi) ;
    - `coup|Annuler` (Undo) ;
    - `plateau|Abandonner` (Resign aux échecs et au Go, « Give up » dans les jeux
      d'arcade) et `raison|Abandon` (Resignation) ;
    - `pendule|B/N` et `ouvrir|…` aux échecs ;
    - `san|R/D/T/F/C` : la notation des coups passe en anglais (« Nf3 » au lieu de
      « Cf3 ») ;
    - `nudge|…` pour la sensibilité du flipper.
- **Pas traduits** :
  - les noms des consoles (Fil d'Or, Roi Noir…) : ce sont des noms propres, et le
    libellé « Écran courant » que lit HA ;
  - les questions et réponses du quiz : le sous-titre anglais du jeu le signale
    (« 720 questions in French ») ;
  - les logs, la NVS, et les termes de flipper déjà anglais (TILT, MULTIBALL…).
- **Français inchangé** : `tr()` rend le texte lui-même. Trois ajustements sans effet
  à l'écran :
  - un `snprintf` sans argument passe par `"%s"` ;
  - quelques textes écrits sur plusieurs lignes sont recollés en un seul littéral,
    identique ;
  - aux dames, 7 `strncpy` deviennent des `snprintf`. Sinon `-Wstringop-truncation`,
    et le `strncpy` ne garantissait pas le zéro final.
- **Défaut corrigé** : le « → » du pied de page de l'Arcade n'existe pas dans les
  polices (jeu `&latin1`). Le texte devient « hub, puis « Quitter » ». Nouvelle garde
  `test_textes_francais_couverts_par_les_polices` : le français aussi est contrôlé.
- **Outillage** :
  - `tools/i18n_keys.py` relève `tr_noop()` et décode les `\n` du YAML ;
  - seul `trivia_questions.h` reste exclu, et les noms des consoles rejoignent
    `NON_TRADUITS` ;
  - `tests/test_i18n.py` recolle les littéraux adjacents par séquences entières ;
  - le contrôle printf ignore le drapeau « espace » : « +40 % d'âmes » était pris pour
    un `%d`.
- **Docs** : `docs/translations.md`, README, `AGENTS.md`, `docs/arcade.md` (7ᵉ règle
  pour ajouter une console), `Tab5/README.md` et la cartographie.
- **Mesures** (ESPHome 2026.9.0 ; build CI de #181 contre build local) :
  - image 3 084 764 → 3 115 708 o (+30,9 Ko : les clés et les traductions) ;
  - RAM statique inchangée (170 968 o) ;
  - aucun avertissement dans notre code.
- **Signalés, non corrigés** (hors périmètre) :
  - Arcanoïde, « Effacer les scores » : l'écran annonce une confirmation, mais le
    code efface au premier appui ;
  - Go, « Groupes morts retires » : le compteur compte des pierres ;
  - Fil d'Or : « Gold %d » à 4 chiffres dépasserait un peu sa colonne du HUD. Le
    maximum réaliste d'une run est d'environ 810.

### 2026-09-27 — Écran en français ou en anglais, réglable depuis Home Assistant (lot 4a)

Lot 4a de l'audit « ouverture ». Demande d'Axel : la langue se règle depuis HA, et
l'ajout d'autres langues doit être prévu. Les jeux suivront (lot 4b) ; les textes
produits par HA et le fuseau horaire aussi (lot 4c).

- **Façon gettext : le texte français du code est la clé.** `tr("Calendrier")` rend
  « Calendar » en anglais, et le texte lui-même en français ou si la traduction
  manque. Les mots à deux sens ont un contexte : `tr_ctx("mardi", "M")` → « T »,
  `tr_ctx("clim", "Chaud")` → « Heat » (et « Warm » pour une lampe). Un ordre de mots
  qui change passe par un modèle : `{jour}, {mois} {quantieme}`.
- **Une langue = un fichier** `Tab5/lang/<code>.yaml`. `tools/gen_i18n.py` génère
  `Tab5/tab5_i18n_data.h`. Ajouter une langue : copier `en.yaml`, traduire, index
  suivant (voir [`docs/translations.md`](docs/translations.md)).
- **`Tab5/tab5_i18n.h/.cpp`**, pur (compilé aussi sur PC par les tests du réveil).
- **Textes posés par le YAML** : gardés en français, et traduits une fois en fin de
  setup par `i18n_apply_boot()`. C'est une ligne en tête du bloc `on_boot` -100,
  avant la première image, **ajoutée avec l'accord d'Axel**. En français, elle ne fait
  rien.
- **Textes du C++ et des lambdas** : `tr()` sur ce que l'écran affiche ou que la
  tablette dit (rappels de rendez-vous, « Volet arrêté. »). **Jamais sur ce que HA
  lit** : noms d'entités, options de select (préréglages de jours, mélodies), états
  et codes. Une valeur HA affichée est traduite à l'affichage seulement.
- **Dates** : `day_short_utf8()`, `month_long_utf8()`… traduites ; les tables `fr_*`
  sont inchangées, et les tests en français aussi.
- **Select « Langue »** (Français / English, `restore_value`). Un changement depuis HA
  redémarre la tablette. Défaut du premier démarrage : `tab5_langue:` dans
  `Tab5/user_entities.yaml`.
- **`Tab5/lang/en.yaml`** : 241 entrées, marqué `_statut: complet`. Il couvre les
  popups, les cartes, la console, le réveil, le calendrier, l'assistant et les dates.
- **Gardes**, `tests/test_i18n.py` (7 tests) : table générée à jour, ordre du select =
  index des langues, anglais complet, aucune clé orpheline, `%d`/`%s` et `{noms}`
  conservés, caractères couverts par les polices (`&latin1`), appel au démarrage
  présent. Chaque garde a été éprouvée par une faute introduite exprès.
  `tools/test_alarm_clock.cpp` gagne des cas anglais ; la CI le compile avec
  `tab5_i18n.cpp`.
- **Preuve « rien ne change en français »** : `tr()` rend la clé elle-même, et le
  passage au démarrage ne fait rien. Les tests des dates en français sont identiques.
  `esphome config` passe.

## [2.1.0] — 2026-09-27

De `v2.0.0` (25/09) à aujourd'hui : 42 pull requests (#137 → #180), plus celle de la
release. Trois chantiers : la fin du **deuxième audit** (organisation du code, dames,
démarrage) et les expériences de performance qui en sont sorties (PSRAM, -O2, cache L2,
pile) ; le **troisième audit, consacré aux ressources** (RAM, CPU, base HA, code mort,
factorisation), soldé ; le début de l'**audit « ouverture »** (vitrine communautaire,
révisions ST7121 et ILI9881C du Tab5, packages HA qui sont la production). **Version
mineure** : les 16 actions de l'API et leurs variables sont inchangées, le plancher reste
ESPHome 2026.9.0, et une OTA suffit depuis la 2.0.0. Côté Home Assistant, quelques gestes
en mettant à jour.

### À faire en mettant à jour depuis 2.0.0

- **Les exemples HA deviennent des packages** (#180). `automations_examples.yaml.example`,
  `scripts_examples.yaml` et `template_sensors_examples.yaml` sont retirés : leur contenu
  est dans `packages/tab5_push.yaml`. Si vous les aviez recopiés dans vos fichiers, retirez
  ces copies avant d'installer le package, pour ne pas définir deux fois les mêmes scripts
  et automatisations.
- **« Tab5 Uptime » devient l'heure du dernier démarrage** (#172) : même nom, même entité.
  Rechargez l'intégration ESPHome après la mise à jour, sinon l'entité reste `unavailable`
  ([troubleshooting](docs/troubleshooting.md)). Une automatisation qui lisait des secondes
  est à revoir ; la garde (b) de `packages/tab5_health.yaml` est déjà réécrite.
- **Redéployer `packages/tab5_health.yaml`** : garde (b) réécrite (#172, #174), garde (e)
  nouvelle, qui reçoit le journal des démarrages (#175, #177).
- **Entités qui quittent HA** : « WiFi Power », « USB Power » et « External 5V Power »
  passent en `internal: true` (#172). Supprimez-les du registre de HA si elles y restent.
- **« Écran courant »** : l'accueil s'appelle « Accueil » tout court (#151).
- **Assistant** : la taille M disparaît, un réglage M enregistré retombe sur A- (#171).
- **Nouveau, facultatif** : `tab5_ecran:` dans `Tab5/user_entities.yaml` pour un Tab5 à
  écran ST7121 ou ILI9881C (#179) ; `packages/tab5_micro_absence.yaml` (#169) ; allumage de
  l'écran à la présence, dans `tab5_push.yaml` (#170).

### Mesures de la version

- **Compilations de la CI** (ESPHome 2026.9.0), firmware de `v2.0.0` (`6a07172`) contre
  celui de ce tag :
  - **image** 2 844 874 → 3 070 188 o (+225 Ko, 37,8 % de la partition). -O2 et le code
    exécuté depuis la PSRAM ont coûté +468 Ko (#149) ; les polices (−179 Ko, #171), le
    calendrier en C++ (−53 Ko, #168) et la factorisation (−20 Ko, #167) en ont repris une
    partie ;
  - **RAM statique** 258 378 → 170 752 o (−87,6 Ko), surtout grâce aux jeux, qui ne
    réservent plus rien quand ils sont fermés (≈ 113 Ko → 377 o, #154 et #162). Le cache L2
    passé à 256 Ko (#159) prend 128 Ko de RAM interne : la RAM annoncée par ESPHome passe de
    576 464 à 445 392 o.
- **Rendu, mesuré sur la tablette pendant les lots** (même protocole, 25-26/09 ; pas refait
  sur le build final) : image pleine au rallumage 202-204 → 163 ms, calendrier 340 → 285 ms,
  console système 400 → 329 ms, boucle au repos 41-42 → 31 ms. Ouverture des popups 30 à
  42 % plus rapide (Climatisation 365 → 216 ms, #166).
- **Pile de la boucle** : 1 476 → 9 988 o libres au minimum (#150).
- **Base HA** : jusqu'à ≈ 10 700 lignes par jour de moins pour « Écran courant » (#151),
  ≈ 5 000 lignes et ≈ 4 300 messages API de moins par jour (#163), 1 440 pour l'uptime
  (#172). 576 appels de poussée de moins par jour, poussée complète 7,6 → 4,1 s (#164).
- **Firmware de la release** : `main` à ce tag n'a pas été flashé seul. L'appareil de
  l'auteur tourne depuis le 27/09 (08:28, OTA) sur ce code plus la PR #181 (langue FR/EN),
  qui n'ajoute que du code. La plupart des lots ont été flashés avant leur merge ; #177
  (journal) et #179 (révisions, ST7123 seulement) n'ont tourné que dans ce build. CI : `build` et
  `build-min` verts sur `c134dfb`, `build-revisions` sur `86e615b` (après lui, seuls des
  fichiers HA changent).

### Problèmes connus

- **ST7121 et ILI9881C** : compilées par la CI, **jamais essayées sur une tablette**
  (#179).
- **Flipper** : en portrait, LVGL écrit en boucle `indev_pointer_proc: X is 832 which is
  greater than hor. res` tant qu'un doigt est posé (coordonnées tactiles non transformées).
  Les flippers fonctionnent ; seuls les logs sont inondés.

### 2026-09-26 — Home Assistant : une seule source, les packages publics sont la production

Lot 3 de l'audit « ouverture » du 26/09 (demande d'Axel : « tout », avec les deux
nettoyages). Jusqu'ici, trois fichiers d'exemples à fusionner à la main étaient tirés de
copies privées gitignorées, et ils dérivaient. Une comparaison avec le HA en service, ce
jour-là, l'a montré :

- **le package du volet n'avait jamais tourné** : chez Axel, le fichier déployé ne
  contenait que des commentaires ; le script vivait dans `scripts.yaml` et les helpers dans
  `configuration.yaml`, sous d'autres noms ;
- `script.allumer_pc_tv`, appelé par le firmware (bouton « PC Bureau »), n'existait dans
  aucun fichier public ;
- la condition `is_primary_active` manquait à 4 automatisations publiques.

Changements :

- **`packages/tab5_push.yaml` (nouveau)** remplace `automations_examples.yaml.example`,
  `scripts_examples.yaml` et `template_sensors_examples.yaml`. Il contient :
  - les automatisations de poussée, les scripts `tab5_push_*`, `allumer_leds` et
    `allumer_pc_tv` (placeholders `VOTRE_TV` / `VOTRE_PC`) ;
  - le capteur « Phrase Prochaine Pluie » ;
  - le helper `is_primary_active` et `force_primary_active_on_boot`.

  Tout vient des fichiers publics, commentaires compris, avec la logique de la production
  reportée dessus.
- **`packages/volet_serre_tracking.yaml`** porte désormais tout le volet :
  - les helpers, avec les noms, l'icône et la valeur initiale de la production ;
  - `tab5_volet_action`, dans la structure exacte de la production ;
  - `tab5_volet_updater` ;
  - `volet_serre_track_direct_cover` (nouveau en public), qui suit les commandes `cover.*`
    venues d'ailleurs.
- **Exemple de réponse de l'assistant** → `snippets/tab5_assist_reponse_exemple.yaml` :
  dans un package, il serait devenu actif chez tout le monde.
- **Preuve d'équivalence** : rendu avec les vraies valeurs, comparé élément par élément à
  la production, en ignorant les libellés, les espaces et les commentaires Jinja (HA retire
  les espaces en tête et en fin de rendu, vérifié sur le HA d'Axel). Tout est identique
  sauf deux changements voulus :
  - le déclencheur `update.bluetooth_proxy_firmware` est retiré, sur choix d'Axel ;
  - la poussée horaire lit `hourly_var_tab5` par `.get()`, même résultat mais plus robuste.
- **Outils et docs** :
  - `tools/render_ha_config.py` : les exemples ne sont plus rendus ;
  - test et README HA mis à jour ;
  - `docs/installation.md` : l'étape 4 dit maintenant d'installer des packages ;
  - `AGENTS.md`, ADR-0017 (addendum), cartographie, inventaire, `placeholders.example.yaml`
    (`VOTRE_PC`, capteur du réveil).
- **Production** : déployée le même soir. Voir `contexte_ia/04_Projets/etat_tab5.md`.

### 2026-09-26 — Révisions du Tab5 : ST7121 et ILI9881C compilées par la CI, choix par `tab5_ecran:`

Lot 2 de l'audit « ouverture » du 26/09 (compatibilité). **Rien ne change pour la
ST7123** : sans la nouvelle clé, la configuration complète (`esphome config`) est
identique à celle d'avant, à une ligne près (`id: tab5_display`, nouvel id de l'écran).

- **Trois fichiers `Tab5/ecran-<révision>.yaml`** : seul ce qui change d'une révision à
  l'autre (modèle d'écran `mipi_dsi`, plateforme tactile, cadence de scrutation).
  - Les broches, les dimensions, la calibration et le réveil au toucher restent dans
    `tab5-hardware.yaml`, sous les id `tab5_display` et `touch`, que le fichier de
    révision étend par `!extend`.
  - Une simple fusion par id ne marche pas entre deux packages : essayée, ESPHome garde
    deux entrées et refuse l'écran (« requires a 'platform' key »).
- **Choix par `tab5_ecran:` dans `Tab5/user_entities.yaml`** : `st7123` (défaut),
  `st7121` ou `ili9881c`. `tab5-ha-hmi.yaml` inclut
  `Tab5/ecran-${ tab5_ecran | default('st7123') | lower }.yaml`.
  - Le défaut Jinja évite d'imposer la clé aux `user_entities.yaml` existants ; `lower`
    tolère « ST7121 ».
  - Vérifié sur ESPHome 2026.9.0, le plancher, sur une petite configuration d'essai
    puis sur la vraie.
- **ST7121** (Tab5 fabriqués depuis le 28/04/2026, journal des versions M5Stack) : modèle
  officiel `M5STACK-TAB5-ST7121` (ESPHome, juillet 2026). ESPHome n'a pas de pilote
  tactile ST7121 ; le code du modèle note que le firmware d'usine M5Stack distingue les
  deux puces en lisant la version du contrôleur tactile. Le même protocole est donc
  probable, d'où la plateforme `st7123`. **Hypothèse non vérifiée.**
- **ILI9881C + GT911** (Tab5 d'origine) : modèle `M5STACK-TAB5` et tactile `gt911`
  repris de la page ESPHome de l'appareil, mêmes broches.
- **CI : job `build-revisions`** (matrice `st7121` / `ili9881c`). Il ajoute la clé au
  `user_entities.yaml` factice et restaure le cache ccache de `build` sans le sauvegarder.
  Pas d'artefact. Il n'est pas requis, et ne tourne que si l'écran, le matériel, l'entrée
  ou le workflow changent.
- **Preuves locales** : `esphome config` passe pour les trois révisions. Comparées à la
  ST7123, la ST7121 ne change que le modèle et ses timings. L'ILI9881C change aussi le
  tactile (GT911 à l'adresse 0x5D). **Aucune des deux n'a tourné sur une tablette.**
- **Docs** :
  - `docs/hardware.md` : tableau avec les dates de fabrication (journal des versions
    M5Stack), la valeur de `tab5_ecran:`, « compile, non testée » ;
  - le README (EN/FR), `docs/installation.md` (prérequis et étape 2),
    `Tab5/README.md`, `user_entities.example.yaml` (clé commentée) et la cartographie
    suivent.

### 2026-09-26 — Journal des démarrages : plus de fausse alerte à chaque démarrage

Constaté au flash de la PR précédente (26/09, 22:23). Un démarrage normal a été
signalé comme grave, avec une notification sur le téléphone. Deux lignes attendues à
chaque démarrage l'expliquent :

- `E (3974) H_API: ESP-Hosted link not yet up`, une « erreur » d'ESP-IDF écrite avant
  que le lien avec le C6 soit monté ;
- les drapeaux d'état d'ESPHome (« waiting for client connection », « scanning for
  networks », puis « cleared ») et « Touch Polling Stopped ».

- **La règle juge le résultat, plus chaque ligne.** Les lignes écrites avant la
  première connexion à HA restent dans le journal, mais seulement comme contexte. Un
  envoi part pour :
  - un reset anormal ou un rapport de plantage — grave ;
  - un Wi-Fi absent 90 s ou plus — grave ;
  - un démarrage qui n'a jamais joint HA ;
  - une erreur (ESPHome ou ESP-IDF) après la connexion à HA ;
  - HA joint plus de 90 s après le démarrage.
- **Une absence de HA avec Wi-Fi présent** (HA qui redémarre, maintenance) ne déclenche
  plus rien.
- **La copie NVS part quand le Wi-Fi manque depuis 90 s**, et non plus après 2 min
  sans HA : aucune écriture flash pendant un redémarrage de HA.
- **Mesures** : image +512 o, RAM inchangée, aucun nouvel avertissement. Le
  `config_hash` ne bouge pas (seul le C++ change) : binaire identifié par son empreinte.

### 2026-09-26 — Communauté : code de conduite, sécurité, formulaires d'issues, README réorganisé, révisions du Tab5

Demande d'Axel : que le projet soit « à la hauteur des meilleurs, voire au-dessus ». Premier
lot de l'audit « ouverture » du 26/09 (vitrine et Community Standards). Aucun changement de
firmware ni de configuration Home Assistant.

- **Profil communautaire GitHub** : `CODE_OF_CONDUCT.md` (adapté du Contributor Covenant 2.1)
  et `SECURITY.md` (signalement privé, périmètre, rappel sur la clé API qui chiffre API et OTA),
  tous deux bilingues. Aucune adresse e-mail n'est écrite dans le dépôt : ils renvoient au
  signalement privé de GitHub et au profil du mainteneur.
- **Formulaires d'issue** (`.github/ISSUE_TEMPLATE/`) : bug (la **puce écran du Tab5** est
  demandée en premier, puis les versions du firmware et d'ESPHome), fonctionnalité, et plus
  d'issue libre : les questions, les montages et les retours de compatibilité vont dans
  Discussions, les failles dans la politique de sécurité.
- **README, premier écran** : ce que c'est, le GIF, les liens (installer, essayer sans HA,
  compatibilité, Discussions), « Why this one », et « Before you start », qui dit franchement les
  limites (ST7123 seulement, pas de binaire précompilé, textes à l'écran en français,
  Météo-France, disposition pensée pour la maison de l'auteur). Le démarrage rapide et la
  compatibilité suivent. La note personnelle, inchangée, passe juste avant la note sur l'IA,
  et une section « Community » est ajoutée. La partie française reçoit les mêmes sections, dont
  un **démarrage rapide qu'elle n'avait pas**.
- **`docs/hardware.md` : tableau des révisions du Tab5.** Il y a trois puces écran (doc ESPHome
  `mipi_dsi`) : **ST7123** prise en charge, **ST7121** jamais compilée ni testée, **ILI9881C +
  GT911** (appareils d'avant le 14/10/2025) pas encore prise en charge. On y explique comment
  lire l'autocollant, et que la page ESPHome de l'appareil comme l'exemple d'IHM de M5Stack ne
  couvrent pas encore la ST7123. `docs/installation.md` le rappelle dans les prérequis.
- **Corrigé en passant** : `docs/hardware.md` citait encore le pilote tactile maison
  `my_components/st7123`, retiré le 06/07/2026 au profit de la plateforme officielle d'ESPHome
  2026.7 ; CONTRIBUTING (FR) disait « Merci d'intéresser ».


### 2026-09-26 — Journal des démarrages et des coupures : plantages et lien Wi-Fi (C6) analysables après coup

Demande d'Axel : « enregistrer les plantages pour pouvoir analyser le C6 », en vérifiant
d'abord si la table de partitions devait changer (flash USB).

- **Vérifié : pas de flash USB.**
  - Un core dump ESP-IDF en flash demanderait une partition `coredump`. La table du
    build (`partitions.csv` : 2 × 7,75 Mo d'application + 448 Ko de NVS) remplit les
    16 Mo, et une table de partitions ne s'écrit que par un flash USB.
  - Il ne servirait pas pour le C6 : ses pannes laissent le P4 tourner, sans plantage.
  - ESPHome 2026.9 a déjà un gestionnaire de plantage, actif ici
    (`USE_ESP32_CRASH_HANDLER`) : PC, adresse fautive et pile d'appels des deux cœurs,
    en `.noinit`. Mais il ne les écrit que dans les logs, au démarrage et au premier
    client abonné aux logs : sans `system_log: fire_event`, HA n'en faisait rien.
- **`Tab5/tab5_journal.cpp` (nouveau)** : `logger: on_message` garde les erreurs, et
  les avertissements tant que HA n'est pas connecté.
  - Les erreurs d'ESP-IDF, dont le pilote ESP-Hosted du C6, arrivent sous l'étiquette
    `esp-idf`. Le rapport de plantage d'ESPHome (`esp32.crash`) est gardé une fois.
  - Une ligne répétée d'affilée est comptée, pas recopiée.
  - 32 lignes en `.noinit` : elles survivent aux redémarrages logiciels, aux plantages,
    aux chiens de garde et au reset par l'USB.
  - Après 2 min sans HA, une copie part en NVS, relue au démarrage suivant si la RAM a
    été perdue (coupure de courant). Au plus une copie toutes les 15 min, seulement
    s'il y a du nouveau.
- **Envoi** : à chaque connexion de HA (`on_client_connected`, sans toucher à
  `on_boot`), le script `tab5_journal_envoi` attend 10 s. Il envoie ensuite
  `esphome.tab5_journal` (`raison`, `demarrages`, `grave`, `lignes`) si le journal
  contient plus qu'un démarrage normal, puis le vide.
- **Garde (e) de `packages/tab5_health.yaml`** : une notification persistante datée par
  journal, et le téléphone seulement si `grave` (plantage, erreur, ou démarrage passé
  sans HA). Déployée en production le 26/09 vers 22:11 et testée avec un faux événement
  non grave : notification rendue, branche sans téléphone, puis retirée.
- **Entité « Tab5 Raison du redémarrage »** (`debug: reset_reason`) : une ligne par
  démarrage dans l'historique HA.
- **Docs** : `docs/debugging.md` (EN/FR : ce que garde le journal, décoder une pile
  d'appels avec `addr2line` et l'ELF du build flashé, pourquoi pas de core dump),
  README de `HomeAssistant_Config/`, cartographie.
- **Mesures** : image 3 061 404 → 3 069 196 o (+7,8 Ko), RAM statique +3,8 Ko, dont
  3 344 o de journal en `.noinit`. Aucun nouvel avertissement de compilation.
  `config_hash` 0x6b2700f1.
- **CI** : `build-min` est un check requis de `main` depuis ce soir (réglage du dépôt ;
  workflow, ADR-0016, `AGENTS.md`, cartographie et inventaire mis à jour).

### 2026-09-26 — Garde « reboot inattendu » : marge de 5 min, heure de démarrage à ±64 s

Suite de la PR précédente, constatée au flash du soir même. Aucun changement de code
firmware : seuls des commentaires changent.

- **L'heure de démarrage est juste à ±64 s.** L'état d'un capteur ESPHome est un float
  32 bits, qui arrondit un horodatage Unix (≈ 1,79 × 10⁹) au multiple de 128 s le plus
  proche. Mesuré : reboot à 19:36:29 UTC, publié 19:35:28.
- **Garde (b) de `packages/tab5_health.yaml` : marge portée de 2 à 5 min.** Avec 2 min,
  un démarrage arrondi de 64 s vers le bas, vu par HA 90 s après la coupure, ne
  déclenchait plus rien. Banc `ha_eval_template` refait avec la valeur arrondie réelle :
  - 7 cas OK à 5 min ;
  - 2 échecs à 2 min, les deux détections tardives.

  Comme avec l'ancienne règle (« uptime < 300 s »), une coupure Wi-Fi dans les 5 min
  qui suivent un démarrage alerte aussi.
- **Mise à jour depuis le capteur en secondes : recharger l'intégration ESPHome.**
  Sinon « Tab5 Uptime » reste `unavailable`. L'intégration ESPHome de HA garde l'ancienne
  unité « s » quand la nouvelle est vide (`esphome/sensor.py`, `_on_static_info_update`),
  et HA refuse alors un horodatage. Entrée ajoutée à `docs/troubleshooting.md`, avec le
  faux positif « décalé d'une minute ».
- **Commentaire de `Tab5/tab5-sensors-diagnostics.yaml` corrigé.** L'heure part au
  premier rappel de synchro de `sntp_time` où l'heure est valide. Après un redémarrage
  logiciel, l'heure système survit : ce rappel part dès la première boucle
  (`SNTPComponent::loop()`), pas « à la première synchro NTP ».

### 2026-09-26 — CI : compilation avec la version plancher d'ESPHome

Nouveau job **`build-min`** dans `.github/workflows/esphome-tab5.yml` : la même
compilation que `build`, mais avec la version d'ESPHome lue dans `min_version:` de
`tab5-ha-hmi.yaml` (2026.9.0 aujourd'hui).

- **Pourquoi** : `build` compile volontairement avec `latest`, pour voir venir les
  ruptures amont. Mais rien ne vérifiait que la version minimale annoncée compile
  encore : une option apparue après elle serait passée inaperçue. Les projets ESPHome
  les plus suivis (NSPanel HA Blueprint, Tessera) compilent eux aussi sur deux versions.
- **Fonctionnement** :
  - la version est lue dans `tab5-ha-hmi.yaml`, seule source du plancher : relever
    `min_version:` suffit ;
  - le job a son propre cache ccache, une clé par version ;
  - il se déclenche dans les mêmes conditions que `build` et ne produit pas
    d'artefact.
- **Check requis de `main`** depuis le soir du 26/09/2026 (réglage du dépôt, à la
  demande d'Axel), comme `build`.

### 2026-09-26 — Alimentations masquées à HA, uptime publié une fois par démarrage

Restes de l'audit des ressources du 26/09/2026 (§6 et H4), relevés dans le code le soir même.

- **« WiFi Power », « USB Power » et « External 5V Power » passent en `internal: true`**
  (`Tab5/tab5-sensors-diagnostics.yaml`). Ce ne sont plus des entités HA. Elles restent
  allumées au démarrage (`ALWAYS_ON`), et aucune automation ni lambda ne les lit.
  Pourquoi : un appui sur la tuile « WiFi » coupait le Wi-Fi, et la tablette restait
  injoignable jusqu'au `reboot_timeout` de l'API, 60 min plus tard.
- **« Tab5 Uptime » devient l'heure du dernier démarrage** (capteur `uptime` de type
  `timestamp`, lié à `sntp_time`). Il est publié une seule fois par démarrage, à la
  première synchro NTP.
  - Il garde le même nom, donc la même entité HA
    (`sensor.m5stack_tab5_home_assistant_hmi_tab5_uptime`). Une carte l'affiche
    maintenant en temps relatif (« il y a 3 heures »).
  - Avant, la valeur en secondes partait chaque minute : 1 440 lignes par jour en base
    et autant d'exécutions de la garde « reboot inattendu ».
  - La console garde ses secondes, par un second capteur `uptime` sans nom, donc
    interne.
- **Garde (b) de `packages/tab5_health.yaml` réécrite pour l'horodatage.** Elle alerte
  dans deux cas :
  - un démarrage plus récent remplace le précédent ;
  - l'entité revient d'`unavailable`/`unknown` avec un démarrage postérieur, à 2 min
    près, au moment où HA a perdu l'appareil.

  Une coupure Wi-Fi sans reboot revient avec l'ancien démarrage et ne déclenche rien ;
  un NTP en retard ne fait rien manquer. Toujours sans `now()`. Condition vérifiée sur
  un banc `ha_eval_template` (10 cas : reboot, NTP en retard, plantage vu 90 s trop
  tard, coupure Wi-Fi, redémarrage de HA, gigue, entité nouvelle).
- **Docs** : README de `HomeAssistant_Config/` et de `Tab5/`, et contrôle après OTA dans
  `AGENTS.md` et le modèle de PR : l'heure de démarrage ne doit plus changer après le
  redémarrage du flash, au lieu de « uptime strictement croissant ».
- **Mesures** : image 3 060 556 → 3 061 404 o (+848 o, le code du capteur horodaté),
  RAM statique +64 o, `config_hash` 0xd9a1f848.

### 2026-09-26 — Polices : −179 Ko (essai D8, validé par Axel)

Essai D8 de l'audit des ressources du 26/09/2026, avec les choix d'Axel ; flashé et
validé à l'œil (« tout marche »).

- **Lissage à 4 niveaux (bpp 2)** au lieu de 16 pour les chiffres de l'horloge
  (`roboto_130_b`), les icônes météo (`font_meteo_card`, `_small`) et les textes gras
  de 45 et 32 px (`roboto_45_b`, `roboto_32_b`) : leurs bitmaps sont divisés par deux.
- **Date sous l'horloge en gras** (`roboto_45_b`) : la police fine `roboto_45`, qui ne
  servait qu'à elle, est retirée. La règle 6 de `tools/check_tab5_code_rules.py` lit
  maintenant la police de `lbl_date` sur son `text_font:`.
- **Assistant vocal** : il réutilise les polices existantes (demande d'Axel), A-
  en `roboto_32_b` et A+ en `roboto_45_b`.
  - Le bouton A (taille M) est retiré, et A- / A+ reprennent la largeur de la rangée.
  - Un réglage M déjà enregistré retombe sur A-.
  - Les tableaux des réponses ne sont plus alignés qu'approximativement, faute de
    police à chasse fixe.
- **Go et flipper** passent de `roboto_mono_24` à `roboto_22`. Plus aucune police
  monospace n'est embarquée (−83 Ko à elles trois).
- **Mesures** : image 3 244 300 → 3 060 556 o (**−179,4 Ko**), RAM statique −376 o.
  Première minute après le démarrage : boucle à 859 ms au plus (démarrage, envoi
  complet de HA, console), puis 175 ms avec l'assistant ouvert.

### 2026-09-26 — Home Assistant : l'écran s'allume à la présence et s'éteint après 15 min

Demande d'Axel. Aucun changement firmware. Déployé sur le HA de production le jour même.

- **`tab5_screen_presence_wifi` remise en place.** La version de production avait perdu
  les déclencheurs du capteur de présence Zigbee : l'écran ne s'éteignait plus qu'au
  départ du téléphone, et restait allumé toute la nuit.
  - **Allumage** à la détection d'une présence, ou au retour du téléphone, seulement
    si l'écran est éteint.
  - **Extinction** après 15 min sans présence dans la pièce, ou au départ du
    téléphone, seulement si l'écran est allumé et que le réveil ne sonne pas. Le
    capteur est un radar qui repasse « off » quelques secondes à chaque sortie de la
    pièce, d'où le délai.
  - `mode: queued`. Écran éteint, le firmware met LVGL en pause, et un toucher ou une
    tape sur la dalle le rallume (inchangé).

### 2026-09-26 — Home Assistant : micro du Tab5 coupé quand personne n'est là (expérience E1)

Expérience E1 de l'audit des ressources du 26/09/2026, demandée par Axel. Aucun
changement firmware. Déployée sur le HA de production le jour même.

- **Nouveau package `packages/tab5_micro_absence.yaml`** : coupe « Ok Nabu » quand la
  maison est vide depuis 10 min, le rallume dès le retour. Sans cela, le mot
  d'activation écoute 24 h/24 : 5 à 15 % d'un cœur, le bus I2S et l'ADC du micro
  (estimation de l'audit).
  - Présence lue sur `zone.home`, sans identifiant personnel.
  - Un « Ok Nabu » coupé à la main reste coupé : seul ce que l'automation a coupé est
    rallumé (`input_boolean.tab5_micro_coupe_absence`).
  - Rattrapage à la reconnexion de la tablette et au redémarrage de HA.
  - Le réveil garde son « Stop » vocal pendant la sonnerie : le firmware arme ce
    modèle et démarre le micro lui-même, puis rend le micro à l'état de l'interrupteur.

### 2026-09-26 — Calendrier : grille construite en C++ (−53 Ko de flash)

Lot 8 de l'audit des ressources du 26/09/2026 (point D3). Rien ne change à l'écran.

- **Les 42 cellules du calendrier sont construites en C++** (`cal_grid_build()`,
  `tab5_calendar.cpp`) au lieu de 42 `!include` de `cal_day_cell.yaml`, supprimé.
  - ESPHome recopiait le code de chaque instance dans `setup()`. Chaque cellule
    portait aussi un bouton invisible, avec son déclencheur, son automatisation et
    son action.
  - La boucle crée les mêmes objets avec les mêmes propriétés que le code que générait
    ESPHome. La cellule reçoit elle-même le tap court, et les pastilles ne captent
    plus le toucher.
  - La grille est créée à la première ouverture du calendrier, juste avant la légende
    (`cal_legend`) : même place dans l'arbre, même ordre de dessin.
  - La première ligne reste lue dans le jeton `${cal_grid_y}`.
- **Mesures** : image 3 297 660 → 3 244 300 o (**−53,4 Ko**), RAM statique
  **−2 184 o**. L'arbre YAML passe de 1 236 à 984 widgets ; le C++ en recrée 210, soit
  42 de moins qu'avant (les boutons invisibles).
- **Preuve** : les 984 autres widgets sont identiques, propriété par propriété, à ceux
  de `main`.

### 2026-09-26 — Factorisation : carte centrale à source unique, gabarits, menus des jeux

Lot 7 de l'audit des ressources du 26/09/2026. Refactor : rien ne doit changer à
l'écran, sauf la correction de la carte centrale ci-dessous. −946 lignes de code
dans `Tab5/` (C++ −389, YAML −557). Flash ≈ −19,6 Ko (code −20,3 Ko),
RAM statique −1 352 o.

- **Carte centrale à source unique** : pluie, vigilance, info, les 4 bandeaux HA et le
  panneau affiché n'existent plus que dans `g_central_ctx`.
  - Les 8 globals ESPHome qui les doublaient sont retirés, avec `sync_central_ctx()`,
    les recopies avant et après chaque appel C++ (7 scripts, 2 services, un onglet de
    tuile) et leur copie dans `on_boot` (accord d'Axel).
  - **Correction** : quand le C++ changeait le panneau affiché sans script YAML
    derrière (retour sur l'accueil, synchro après une alerte), le global n'était pas
    mis à jour. Au tour suivant, le rotateur repartait de l'ancien panneau.
- **Popups** : `animate_popup_open(card)` affiche le popup et le passe au premier plan.
  Les 8 appelants écrivaient ce premier plan à la suite de l'appel. Le paramètre du
  voile, jamais fourni, est retiré de l'ouverture et des 18 fermetures.
- **Scripts et gabarits YAML** :
  - calendrier : un seul envoi de demande de mois (`tab5_cal_request`) et un seul
    calcul « mois ± 1 » (`cal_shift_month()`), au lieu de 5 copies ;
  - volet : le tap des deux cartes passe par `tab5_volet_tap` ;
  - télécommande TV : 14 touches passent par `tab5_tv_key(touche)`, avec 3 gabarits
    (flèches, transport, applications) ;
  - popup lumière : gabarits du sélecteur, des blancs et des raccourcis de niveau ;
  - assistant : la police de la réponse vient d'`assist_font()` (3 copies).
- **Réveil** :
  - les réglages partagent 3 modèles YAML (ancres) ;
  - `g_alarm_cfg` porte aussi la mélodie, le volume et l'avance des rendez-vous, ce qui
    supprime 6 gardes NaN ;
  - un seul script de décalage d'heure (`tab5_alarm_shift(cible, delta)`) remplace les
    trois précédents, et un seul script d'overlay les deux précédents.
  - Les 25 entités exposées à HA sont inchangées.
- **Jeux** : `SlotMenu<N>` remplace le trio d'entrées de menu recopié dans les 8 jeux.
  S'y ajoutent `set_pressed_bg()`, `show_front()`, `hud_num()` et un `panel_text()`
  local dans 6 jeux. Soit −297 lignes dans les jeux.
- **C++ du cœur** :
  - `start_anim()` remplace 16 blocs d'animation ;
  - `central_wraps()` fournit les 8 panneaux de la carte centrale ;
  - `trim_ws()` / `split_fields()` (`tab5_core`) remplacent 5 rognages et 3 découpes ;
  - l'icône météo passe en table ;
  - `highlight_button_border()` sert aussi au sélecteur de lumière et aux boutons S/M/L
    de l'assistant.
- **Alias LVGL** : les 108 appels aux noms de compatibilité v8 / v9.x sont migrés vers
  les noms LVGL 9.5 :
  - `lv_obj_clear_flag` → `lv_obj_remove_flag` ;
  - `lv_obj_move_foreground(o)` → `lv_obj_move_to_index(o, -1)` ;
  - `lv_anim_del` → `lv_anim_delete` ;
  - `lv_coord_t` → `int32_t`, `LV_LABEL_LONG_*` → `LV_LABEL_LONG_MODE_*`, etc.
- **Preuves** :
  - l'arbre des 1 236 widgets est identique avant et après (hors actions voulues) ;
  - les 25 entités du réveil sont identiques clé par clé ;
  - les découpes, l'icône météo et les animations sont vérifiées à la compilation
    contre les anciennes copies (`static_assert` sur un LVGL simulé) ;
  - pytest (55) passe.

### 2026-09-26 — Design : popups plus rapides, boutons instantanés, thème ESPHome, nettoyage

Lot 6 de l'audit des ressources du 26/09/2026.

- **Popups 30 à 42 % plus rapides à s'ouvrir** (essai validé par Axel). L'ouverture coûtait
  ≈ 320-365 ms de rendu : sous le voile à 85 %, LVGL redessinait tout le tableau de bord
  caché. Mesures (boucle max/min, mêmes écrans) :
  - ouverture : Climatisation 365 → 216 ms, Calendrier 320 → 223 ms, Console 328 → 191 ms ;
  - fermeture : 180 → 153-155 ms.

  Changements :
  - **voile opaque** : 100 % au lieu de 85 % (96 % pour la sonnerie), coins carrés. Le
    tableau de bord n'apparaît plus derrière les popups ; c'est le seul changement visible ;
  - **verre pré-mélangé** : la carte des popups et les 55 tuiles et boutons du tableau
    de bord reçoivent des couleurs calculées d'avance sur leur fond uni
    (`color_glass_*_page` / `_modal`). Même rendu, mais opaques ;
  - **teinte à l'appui** : l'effet pressé passe de 30 % à 52 % d'opacité sur ces surfaces
    (`style_btn_pressed_opaque`), pour garder la même teinte ;
  - **exceptions** : le sous-popup du jour (voile à 60 %) et les boutons − / + de la clim,
    posés sur une autre tuile en verre, restent translucides.
- **Boutons instantanés** (demande d'Axel) : `CONFIG_LV_THEME_DEFAULT_TRANSITION_TIME: "0"`.
  Le thème LVGL par défaut animait chaque appui en 80 ms, et chaque relâchement en 80 ms
  après 70 ms de délai, ce qui se voyait pendant l'ouverture d'un popup. Boutons,
  curseurs et interrupteurs basculent désormais d'une image à l'autre. Les animations du
  projet (swipe des prévisions, rouleaux, alertes) ne changent pas.
- **Thème ESPHome** (`theme:` dans `tab5-styles.yaml`) :
  - `label` porte le blanc (`color_text`) et la police (`roboto_22`) courants.
    292 réglages locaux identiques sont retirés des sources. Comparaison des 1 247
    widgets avant/après : aucune couleur ni police effective ne change ;
  - `button` n'a plus d'ombre. Le thème LVGL en posait une sous chaque bouton qui ne
    l'annulait pas (voiles des popups, cartes de l'arcade, pastilles), recalculée à
    chaque repeint sans cache. Seule différence visible : le liseré gris sous ces boutons.
- **`default_font: roboto_22`** : la montserrat_14 d'origine, jamais affichée, n'est
  plus embarquée.
- **Nettoyage** :
  - 10 calques d'icône météo (`*_icon_layer3`) cachés et sans référence ;
  - `style_card`, jamais utilisé ;
  - `style_meteo_tab`, identique à `style_meteo_card` et remplacé par lui ;
  - 6 couleurs jamais référencées ;
  - le conteneur de centrage du volume dans la télécommande TV ;
  - 6 `scrollbar_mode` qu'ESPHome ignore en silence dans `style_definitions`.
- **Réveil** : ses deux libellés passaient du blanc YAML (#F1F5F9) au blanc C++
  (#FFFFFF) au premier rafraîchissement. Ils gardent désormais #F1F5F9.
- **Doc corrigée** : `pressed: { styles: x }` est accepté sur un widget
  (`troubleshooting.md`, cartographie, `tab5_anim.cpp`).

### 2026-09-26 — Jeux : fin du repeint plein écran aux dames, HUD sans réécriture

Lot 5 de l'audit des ressources du 26/09/2026. Rien ne change à l'écran.

- **Dames** :
  - chaque case foncée retient ce qu'elle affiche. Un coup ne restyle plus les 40
    pièces, seulement les 2 ou 3 cases qui changent, plus les prises. Au-delà de 32
    zones à redessiner, LVGL repeignait tout l'écran à chaque tap ;
  - pièces, couronnes et surbrillances ne sont plus créées que sur les 50 cases
    foncées : 150 objets LVGL de moins à l'ouverture.
- **Échecs** : l'évaluation affichée n'est plus recalculée à chaque tick de 33 ms,
  seulement quand la position change. Les couleurs du bandeau ne sont plus reposées
  si elles sont identiques.
- **`set_text_color_if()`** (`game_common.h`) remplace les `set_color` d'Échecs, du Go
  et de Trivia, qui recoloraient sans comparer. En LVGL 9.5, poser un style invalide
  l'objet même à valeur identique.
- **Fil d'Or** : l'opacité de la bille (3 styles) et la pulsation du portail « Oeil du
  dédale » ne sont réécrites qu'au changement, au lieu de chaque tick.

### 2026-09-26 — Home Assistant : scripts de poussée, plus de renvoi d'un état inchangé

Lot 2 de l'audit des ressources du 26/09/2026. Déployé sur le HA de production le jour
même. Seul le firmware change pour Draw Max.

- **Scripts de poussée** `tab5_push_alertes`, `tab5_push_meteo`, `tab5_push_clim` et
  `tab5_push_volet` (`scripts_examples.yaml`). Chaque bloc n'existe qu'une fois, au lieu
  d'être recopié dans la poussée complète et dans son automatisation au changement :
  l'exemple public passe de 649 à 426 lignes.
  - `tab5_push_alertes` relève les MAJ, les capteurs « problem » et le compte
    d'indisponibles une fois par passage (trois parcours de `states` auparavant).
  - Sorties identiques sur 2 880 scénarios comparés dans HA.
- **Plus de renvoi toutes les 10 min** de la météo actuelle, des probabilités, de la clim
  et du volet, déjà poussés au changement. Ils ne partent plus qu'à la (re)connexion du
  Tab5 et au retour à `on` de `is_primary_active` (nouveau déclencheur `resync`) :
  576 appels par jour en moins, chacun repeint par l'appareil. La poussée complète dure
  4,1 s au lieu de 7,6 s.
- **Météo au changement** : nouvelle automatisation `tab5_ha_hmi_meteo_push`
  (condition, température, humidité, UV / gel / neige). Avant, la carte météo n'avait
  pas d'autre source que le cycle de 10 min.
- **Code mort retiré** de `packages/tab5_alerts.yaml` :
  - l'automatisation qui écoutait `esphome.tab5_alert_dismiss`, un événement que le
    firmware n'émet pas (dernier passage le 16/07/2026) ;
  - le script `tab5_dismiss_info_panel`, sans appelant.
- **Firmware** : « Tab5 Draw Max » passe en `internal: true`. La campagne de mesure est
  close, et le capteur faisait ≈ 1 500 lignes par jour en base. Un capteur interne
  n'apparaît plus dans `esphome logs` non plus : pour une nouvelle mesure, retirer
  `internal: true`.
- **Corrigé en production au passage** :
  - la vigilance lisait un attribut Météo-France `Crues` qui n'existe plus
    (`Inondation`), donc une vigilance inondation n'arrivait jamais sur l'écran ;
  - le bandeau affichait « Vigilance Jaune - … » ou « Vigilance Jaune · … » selon
    l'automatisation ;
  - l'automatisation de suivi du volet se réveillait à chaque `call_service` de HA :
    elle filtre désormais `domain: cover` dans son déclencheur ;
  - `tab5_rdv_push` (`packages/tab5_reveil.yaml`) poussait les rendez-vous même
    tablette hors ligne ou pas encore authentifiée, soit 66 erreurs les 25 et 26/09.
    Elle attend maintenant la liaison, comme la poussée complète.
- **Exemple public aligné sur la prod** pour le libellé des prévisions jours. HA envoie le
  jour seul (« Auj », « Lun »), affiché tel quel sur la page d'accueil ; les deux pages
  suivantes ajoutent la date elles-mêmes (« Lun 05 »). Depuis mai, l'exemple envoyait
  « Lun 05 » partout.

### 2026-09-26 — Moins de travail permanent : I²C, repeints à l'identique, base HA

Lot 3 de l'audit des ressources du 26/09/2026. Rien ne change à l'écran ni dans les
entités HA ; les tuiles du tableau de bord « Accueil » (vue Tab5) restent alimentées.

- **Prise casque** (`Headphone Detect`) : l'expander `pi4ioe1` n'a pas de broche
  d'interruption, donc ESPHome le relisait en I²C à chaque tour de boucle (≈ 60 fois/s sur
  `bsp_bus`). La lecture se fait désormais une fois par seconde, par un `interval` qui
  coupe la boucle du capteur et l'appelle lui-même.
- **IMU** : cadence de lecture adaptée à l'usage.
  - 1 s écran allumé sans jeu, contre 100 ms avant ;
  - 100 ms écran éteint avec tap-to-wake, ou jeu ouvert ;
  - 33 ms pour un jeu piloté à l'inclinaison.

  Trial Poursuite ne demande plus 30 Hz pour une simple secousse (`imu_fast` à `false`).
- **Écritures LVGL conditionnelles** : nouveaux helpers `ui_text()` et `ui_text_color()`
  dans `tab5_internal.h`. En LVGL 9.5, `lv_label_set_text()` et `lv_obj_set_style_*()`
  invalident l'objet même à valeur identique. Ils sont appliqués aux endroits suivants :
  - icônes d'état, dont l'icône Wi-Fi repeinte toutes les 5 s ;
  - cartes lumière, clim, plantes et températures, et popup des pots ;
  - console système, date sous l'horloge (chaque minute) ;
  - textes des tuiles de prévisions.
- **Icônes météo** : une tuile dont la condition n'a pas changé n'est plus repeinte
  (`icon_cond_update()` : SAME / FIRST / CHANGED). La police des bandeaux d'alertes HA
  n'est plus reposée à chaque push, puisque c'est celle du YAML.
- **Publications vers HA** :
  - tangage, roulis, température IMU et signal Wi-Fi : publiés sur variation réelle
    (2°, 0,5 °C, 2 dB), et au moins toutes les 15 min ;
  - « Prochain réveil » et « Prochain rendez-vous » : publiés seulement s'ils changent ;
  - « Volume » : n'est plus relu chaque minute (déjà publié à chaque changement).

  Estimation : ≈ 5 000 lignes par jour de moins dans la base HA et ≈ 4 300 messages API
  par jour.

### 2026-09-26 — Jeux : plus aucune mémoire réservée quand ils sont fermés

Lot 4 de l'audit des ressources du 26/09/2026. Décision d'Axel : « pas de réserve mémoire
pour les jeux si non actif ».

| Mémoire statique des 8 consoles (`nm`) | Avant | Après |
|---|---|---|
| RAM interne | 46 767 o | 377 o |
| PSRAM (`.ext_ram.bss`) | 34 300 o | 0 o |
| RAM interne statique du firmware | 216 888 o | 170 280 o (−46 608) |

- **État pris à l'ouverture, rendu à la fermeture.** Tableaux, pointeurs LVGL, tampons texte
  des menus et brouillons d'IA passent dans des blocs créés par `open()` et rendus par
  `close()` : `game_mem_new<T>(MemPref)` et `game_mem_free()` dans `game_common.h`.
  - `MemPref::Internal` pour ce qui est lu à chaque tick.
  - `MemPref::Psram` pour les historiques et piles d'annulation, qui étaient en
    `EXT_RAM_BSS_ATTR` jusque-là.
- **Objets LVGL détruits à la fermeture et reconstruits à l'ouverture** par `ui_destroy()`.
  Les conteneurs YAML sont conservés. Les callbacks posés sur ces conteneurs sont retirés
  à la fermeture, pour ne pas s'empiler à la réouverture.
- **Ce qui survit, quelques octets par jeu** :
  - l'état ouvert/fermé (lu par le registre) ;
  - les dernières lectures IMU (`dispatch_imu()` les envoie aussi aux jeux fermés) ;
  - le `NvsSlot` (le recréer ferait fuir un backend de préférences à chaque ouverture) ;
  - les filtres et les graines d'aléa dont la remise à zéro changerait le jeu (lissage
    d'inclinaison, passe-haut du flipper, lane du skill shot).
- **Exception voulue** : une partie d'échecs ou de Trial Poursuite **en cours** garde son
  état (bloc `Play`) et reprend à l'identique, comme avant. Pour les échecs, c'est
  ≈ 1,2 Ko en interne et 10 Ko d'historique en PSRAM. Le bloc est rendu dès que la partie
  est finie.
- **Moteurs et IA** :
  - échecs : `SQ64` et `CASTLE_MASK` calculés à la compilation (plus d'`init()`), coups
    « tueurs » dans le bloc de recherche, brouillon SAN pris le temps de l'appel ;
  - dames : `Ai::acquire()` / `Ai::release()` ;
  - Go : `Engine::scratch_acquire()` et `Ai::scratch_acquire()`, utilisés aussi par
    `tools/test_go_engine.cpp` ;
  - Trial Poursuite : topologie du plateau en table `constexpr`.
- **Effets visibles**, tous mineurs :
  - le plateau derrière le menu d'un jeu repart vide à chaque ouverture, comme à la
    première ouverture après un démarrage ;
  - aux échecs, une pièce restait masquée si on fermait pendant son animation : corrigé.
- **Garde-fous** : les fonctions publiques atteignables jeu fermé ne touchent plus au
  bloc ; si la mémoire manque à l'ouverture, un avertissement est journalisé et on revient
  à l'arcade sans ouvrir.

### 2026-09-26 — Réveil et voix : trois bugs trouvés par l'audit des ressources

Audit du 26/09/2026 (ressources, code mort, factorisation), §2. Aucun nouveau calcul, aucune
entité touchée.

- **Le « Stop » vocal du réveil pouvait tomber en panne en pleine sonnerie.** La sonnerie arme
  le modèle « Stop », mais la poussée HA de l'état du volet (toutes les 10 min) le désarmait
  sans regarder si le réveil sonnait. Un seul script désarme désormais le modèle,
  `tab5_stop_model_release` (`tab5-assist.yaml`), et seulement si rien ne le tient plus :
  réveil qui sonne, volet en mouvement ou réponse vocale en cours. Les quatre désarmements
  (poussée du volet, fin de mouvement, fin de réponse vocale, fin de sonnerie) passent par lui ;
  chacun testait jusque-là sa propre combinaison.
- **« jeudi 1 octobre » dans le détail du prochain réveil** : `alarm_next_detail` refaisait
  son propre libellé ; il reprend `format_long_day_label` (« jeudi 1er octobre »). Nouveau cas
  dans `tools/test_alarm_clock.cpp`.
- **Carte centrale figée après deux réponses vocales rapprochées** : `tab5_show_vocal_response`
  est en `mode: restart`. Une deuxième réponse arrivée pendant les 8 s d'affichage de la
  première, page quittée entre-temps, laissait le drapeau `is_showing_vocal_response` levé et
  le rotateur arrêté. L'exécution coupée est désormais rangée d'abord ; la fin d'une réponse est
  un script unique, `tab5_vocal_response_end`.

### 2026-09-26 — Horloge temps réel RX8130 : l'heure dès le démarrage, même sans réseau

Le Tab5 a une horloge RX8130CE (0x32 sur `bsp_bus`, supercondensateur de 70 000 µF) que le
firmware n'utilisait pas. Sondée le 26/09/2026 : présente, elle avançait normalement (36 s de
retard sur l'UTC, stable sur 14 min).

- `time:` plateforme `rx8130` (`tab5-sensors-diagnostics.yaml`), sans lecture périodique.
- **Lecture au démarrage** par un `interval: 24h` (première exécution dans les 5 s après le
  setup, sans effet ensuite tant que l'heure est valide) : `on_boot` n'est pas touché. Log
  `tab5.rtc` avec l'heure reprise ; l'horloge de l'écran est peinte aussitôt.
- **Écriture à chaque synchro NTP** (`rx8130.write_time` dans `on_time_sync` du SNTP).
- Au setup, le composant ESPHome écrit les registres de contrôle de l'horloge, dont la charge
  du supercondensateur, comme la bibliothèque M5Unified de M5Stack.
- `docs/hardware.md` (EN + FR) : section horloge ; INA226 (0x41) présent mais non utilisé.

### 2026-09-26 — Performance : cache L2 de 256 Ko

`CONFIG_CACHE_L2_CACHE_256KB` (128 Ko par défaut). Depuis #149 le code et les polices sont lus
en PSRAM à travers ce cache. Mesuré sur la tablette, même protocole que le 25/09 (fenêtres
d'une minute, mêmes actions aux mêmes minutes par rapport à l'envoi HA) :

| Situation | 128 Ko (image / boucle) | 256 Ko | Gain |
|---|---|---|---|
| Repos | 10-11 / 35-36 ms | 10 / 31 ms | boucle −12 % |
| Écran éteint/rallumé | 179 / 195 ms | 163 / 179 ms | image −9 % |
| Calendrier | 305-306 / 353-365 ms | 285 / 338-380 ms | image −7 % |
| Console système | 352-356 / 371-376 ms | 329 / 347-348 ms | image −7 % |
| Envoi HA | 12 / 38 ms | 13 / 37 ms | ≈ |

- Coût : le cache est pris sur la RAM interne. RAM libre 356 → 226 Ko ; RAM statique inchangée.
- 5 redémarrages de contrôle : écran affiché à chaque fois (vérifié par Axel), API revenue en
  16,5-16,8 s comme avant.


### 2026-09-26 — Documentation : 32 Mo de PSRAM, pas 16

`docs/hardware.md` (EN + FR) annonçait 16 Mo de PSRAM « OCT-SPI ». Mesuré sur la tablette le
26/09/2026 par une sonde d'essai (non mergée) : `esp_psram_get_size()` = 32 768 Ko, dont
29 567 Ko pour le tas ; le bus est en mode `hex` (16 lignes) à 200 MHz (`psram:` de
`tab5-hardware.yaml`). Les fiches de `contexte_ia` disaient déjà 32 Mo.


### 2026-09-26 — Diagnostic : version du logiciel du co-processeur Wi-Fi (ESP32-C6)

Le Tab5 a deux puces : le P4 fait tourner notre firmware (pile TCP/IP comprise), le C6 la
radio, avec le logiciel ESP-Hosted d'Espressif et sa propre RAM. ESPHome compile la partie
P4 d'ESP-Hosted (2.12.12) mais ne met pas le C6 à jour, dont la version était inconnue.

- Nouveau capteur texte **« Tab5 C6 Version »** (diagnostic, `tab5-sensors-diagnostics.yaml`),
  via `read_c6_firmware_version()` (`tab5_console.cpp`) → `esp_hosted_get_coprocessor_fwversion()`.
  Log `tab5.c6` avec les deux versions (C6 et bibliothèque du P4).
- Lu une seule fois, Wi-Fi connecté. La requête attend au plus 1 s la réponse du C6 : 3 essais
  au plus, puis « inconnue », pour ne pas figer la boucle à chaque minute.
- Lecture seule : aucune mise à jour du C6 (décision à part, plus risquée : un C6 sans
  Wi-Fi ne se rattrape qu'en USB).
- `docs/hardware.md` (EN + FR) : RAM du C6, logiciel qui y tourne, capteur.

### 2026-09-26 — Réseau : fenêtre TCP laissée à la valeur d'ESPHome

`CONFIG_LWIP_TCP_WND_DEFAULT: "16384"` est retiré de `tab5-hardware.yaml` : ESPHome
applique sa propre valeur (65 534 o, réglages lwIP « optimisés » de son composant
`network`) et la fera évoluer seul.

- Origine du forçage : le correctif OTA d'avril 2026 (sockets coupés côté PC, WinError
  10053/10054) avait posé deux réglages ensemble, sans essai séparé.
  `CONFIG_SPI_FLASH_YIELD_DURING_ERASE`, qui laisse le réseau répondre pendant
  l'effacement de la flash, est gardé. L'explication donnée pour la fenêtre (débordement
  d'une « boîte aux lettres SPI ») ne tient pas : le lien avec la puce Wi‑Fi (ESP32-C6)
  est en SDIO, et la fenêtre ne borne que les octets envoyés par le PC avant un accusé
  de réception.
- Essai du 25/09/2026 : un firmware à 64 Ko a reçu des OTA normalement (14,5 s).
- Coût : jusqu'à ≈ 64 Ko de RAM interne tamponnés pendant une réception rapide (OTA),
  à comparer aux 302 Ko libres mesurés le 26/09/2026 avant #154 (qui en rend ≈ 66).

### 2026-09-26 — Jeux : 66 Ko de RAM interne rendus quand aucun jeu n'est ouvert

Audit du 25/09/2026, §4.1 (étapes 1 à 3). Les jeux réservaient ≈ 113 Ko de RAM interne en
permanence, même fermés. Il en reste ≈ 47 Ko. RAM interne statique du firmware : 282 692 →
216 616 o (`riscv32-esp-elf-size -A`, `.iram0.text` + `.dram0.*` + `.dram1.bss` +
`.noinit`).

- **Échecs, table Zobrist calculée à la compilation** (`constexpr`, 7,7 Ko) : elle vit en
  flash. Même xorshift, même graine, même ordre : les 985 clefs du firmware ont été
  comparées à celles de l'ancien `init()` (0 écart).
- **Échecs, tampons de recherche alloués à la demande** : les coups par ply et l'état de
  recherche (≈ 24 Ko) forment un bloc pris au premier `search_start()`, `search_quick()`
  ou `perft()`, en RAM interne (PSRAM en repli), et rendu par `search_release()` à la
  fermeture du jeu. Une trace `chess` indique où il a été pris. Sans bloc, les accesseurs
  répondent comme une recherche terminée et l'IA joue un coup légal tiré au sort.
- **Tampons froids en PSRAM** (`EXT_RAM_BSS_ATTR`, 34,3 Ko) : historique des échecs, pile
  d'annulation du Go, coups légaux et pile d'annulation des dames, coups racine de l'IA
  des dames. Ils ne sont touchés qu'une fois par coup ; seuls les coups racine des dames
  sont lus par la recherche, une fois par coup racine et par profondeur, pas par nœud.
  L'option `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` est activée pour cela ; le
  `.map` confirme que rien d'autre ne passe en PSRAM (la BSS de lwIP reste interne).
- Vitesse des IA : tout ce que la recherche lit à chaque nœud reste en RAM interne ;
  non mesurée sur la tablette. 0 avertissement dans notre code.

### 2026-09-26 — Compilation : plus aucun avertissement dans notre code en -O2

Le passage en `compiler_optimization: PERF` (#149) fait analyser les formats plus finement.
Trois avertissements `-Wformat-truncation` / `-Wstringop-truncation` sont apparus dans notre
code. Les PR #149 et #150 annonçaient « aucun avertissement » à tort : leurs compilations ne
filtraient que les erreurs.

- `alarm_hhmm()` (`alarm_clock.cpp`) : minutes bornées à une journée, champs non signés.
- `sq_name()` (`go_game.cpp`) : numéro de rangée borné.
- `push_hist()` (`draughts_game.cpp`) : `memcpy` de même taille au lieu d'un `strncpy`
  tronquant.
- Sorties identiques pour toutes les valeurs valides. Vérifié par une recompilation forcée de
  tous nos fichiers C++ et de `main.cpp` : 0 avertissement.

### 2026-09-26 — Dames : triple répétition, fins de partie réduites, raison de la nulle

Suite de #146 (règle des 25 coups). Règles FMJD (celles de lidraughts) ; le texte FFJD dit
la même chose pour la répétition et les 16 coups, mais ne chiffre pas le cas « deux dames
contre une ».

- **Triple répétition** (les deux variantes) : l'interface garde l'empreinte (FNV-1a des
  cases + trait) de chaque position depuis le dernier coup irréversible (prise ou coup de
  pion) ; la même position avec le même trait pour la 3ᵉ fois termine la partie. Remise à
  zéro prudente après une annulation ou une reprise de partie.
- **Fins de partie réduites** (international) : une dame seule contre au plus deux pièces
  dont une dame → nulle après 5 coups de chaque camp ; contre trois pièces dont une dame →
  après 16. Deux champs dans `Engine::Pos` (`eg_limit`, `eg_plies`), recalculés à chaque
  prise ou promotion, donc pris en compte par la recherche de l'IA. Non sauvegardés (layout
  NVS inchangé) : recalculés à la reprise, le décompte repart de zéro.
- **Écran de fin** : une nulle affiche sa raison (« 25 coups sans pion ni prise »,
  « Position repetee 3 fois », « Fin de partie : 5/16 coups chacun »).
- Miroir Python (`tools/test_draughts_engine.py`) : deux tests des fins de partie réduites.

### 2026-09-25 — « Écran courant » : publié seulement quand il change

Lot 3 bis de l'audit du 25/09/2026 (§3.1). Le capteur partait toutes les 5 s vers HA, même
inchangé, et suivait le panneau du rotateur central (planning, pluie, vigilance, info,
alertes, 8 s chacun) : jusqu'à ≈ 10 700 lignes par jour en base.

- La lambda ne publie plus que les changements (elle renvoie « rien » sinon). HA reçoit
  quand même l'état courant à chaque reconnexion : ESPHome le renvoie à l'abonnement.
- L'accueil s'appelle désormais « Accueil » tout court. Popups, « Arcade » et « Jeu · … »
  sont inchangés.
- Aucune automation HA ne lit ce capteur (seule une carte du tableau de bord l'affiche).

### 2026-09-25 — Pile de la boucle : 16 Ko au lieu de 8

Le capteur « Tab5 Stack Free Min » (#148) ne laissait que 1 476 o libres (1 796 en -O2) sur
les 8 Ko de la pile de la boucle ESPHome, qui porte `setup()` et toute la boucle : LVGL,
API, lambdas.

- Diagnostic (build d'essai à sondes, non mergé) : le minimum est atteint **pendant
  l'initialisation**, avant le premier tour de boucle (t = 7,3 s). La zone la plus profonde
  porte le premier rendu LVGL et un `snprintf` avec formatage de nombre (`_svfprintf_r`,
  1 152 o de cadre à lui seul). Aucune action HA, aucun popup, aucune image pleine ni partie
  de dames ne descend plus bas ensuite : les tampons de nos fonctions d'analyse ne sont pas
  en cause.
- Correctif : `esp32: framework: advanced: loop_task_stack_size: 16384`.
- Mesuré sur la tablette : pile libre minimale 1 796 → **9 988 o** ; RAM interne libre
  310,5 → 302,4 Ko (les 8 Ko de la pile, statique) ; flash inchangée.

### 2026-09-25 — Performance : code exécuté depuis la PSRAM, compilé en -O2

Expériences de l'audit du 25/09/2026 (§3.3), une à la fois, mesurées avec les capteurs
« Tab5 Draw Max » et « Tab5 Loop Time ». Même protocole pour chaque build : repos, envoi HA,
écran éteint/rallumé (image pleine), calendrier et console système ouverts à distance,
deux passages de chaque ; écarts entre passages ≤ 5 ms sur les images.

| | Image pleine | Calendrier | Console | Boucle au repos | RAM interne libre |
|---|---|---|---|---|---|
| Référence (-Os, flash DIO 40 MHz) | 202-204 ms | 340 ms | 400 ms | 41-42 ms | 325,5 Ko |
| `execute_from_psram` | 179 ms | 311 ms | 359-363 ms | 34-35 ms | 316,2 Ko |
| `compiler_optimization: PERF` | 196-197 ms | 322 ms | 381-384 ms | 37-40 ms | 320,2 Ko |
| **les deux (retenu)** | **172 ms** | **292-293 ms** | **340 ms** | **33-34 ms** | 310,0 Ko |

- Retenu : `execute_from_psram: true` + `compiler_optimization: PERF` (`Tab5/tab5-hardware.yaml`).
  Coût : firmware +468 Ko (40,8 % de la partition), RAM interne libre −15,5 Ko, OTA
  12,9 → 14,7 s. Démarrage inchangé (liaison API revenue en 16,6 s). 5 redémarrages de
  contrôle sans écran noir.
- Écarté : fenêtre TCP à 64 Ko (défaut ESPHome) — l'OTA du même binaire passe de 14,7 à
  14,5 s, il est borné par l'écriture flash ; `CONFIG_LWIP_TCP_WND_DEFAULT: 16384` reste.
- Pile libre minimale de la boucle : 1 476 → 1 796 o avec -O2 (atteinte dès le démarrage).
### 2026-09-25 — Diagnostic : durée des images LVGL et pile libre de la boucle

Point de départ des expériences de performance (audit du 25/09/2026, §3.3). Deux
capteurs de diagnostic, publiés chaque minute comme « Tab5 Loop Time » :

- **Tab5 Draw Max** (ms) : image LVGL la plus longue de la minute écoulée, rendu et envoi
  à l'écran compris (`on_draw_start` / `on_draw_end`, soit `LV_EVENT_RENDER_START` →
  `LV_EVENT_REFR_READY`). 0 = rien n'a été redessiné.
- **Tab5 Stack Free Min** (o) : pile libre minimale de la boucle ESPHome depuis le
  démarrage (`uxTaskGetStackHighWaterMark`, 8 Ko en tout). Les logs de l'IA des dames
  (#145) n'y laissaient que 1 492 o.

### 2026-09-25 — Boot : le push complet de Home Assistant n'est plus perdu

Modification de `on_boot` autorisée expressément par Axel le 25/09/2026.

- Constat : sur trois redémarrages du 25/09 au soir, un seul événement
  `esphome.tab5_connected` est arrivé dans HA (base `events`). Les deux autres fois,
  l'écran est resté sans agenda ni météo jusqu'au cycle HA suivant (/10 min, 8 à 9 min
  plus tard), et le réveil calculait en repli sur l'heure fixe pendant ce temps.
- Cause : `on_boot` attendait `api.connected` (n'importe quel client) puis 500 ms. Le
  premier client venu — Home Assistant en cours de connexion, ou `esphome logs` — ouvrait
  la porte, et l'événement partait avant que HA soit abonné.
- Correctif : même garde que la reconnexion (`on_client_connected`,
  `tab5-api-logic.yaml`) — attendre `api.connected` avec `state_subscription_only: true`,
  puis 2 s, et revérifier avant d'émettre. Le délai de 30 s et le message « HA injoignable
  au boot » sont inchangés.

### 2026-09-25 — Dames : la partie n'est plus déclarée nulle au 25ᵉ demi-coup

Vu au premier essai après #145 : une partie contre l'IA Amateur s'est terminée « sans
raison » après 13 coups des Blancs et 12 des Noirs, sans aucune prise. Le jeu déclarait la
nulle après 25 **demi-coups** sans prise ni promotion, déplacements de pions compris.

- Règle officielle : nulle quand aucun pion n'a bougé et que rien n'a été pris pendant
  25 coups de **chaque camp** en international (FFJD/FMJD), soit 50 demi-coups, et 40 en
  anglais (WCDF), soit 80.
- `apply_move` remet `no_progress` à 0 à chaque coup de pion (et non plus seulement à la
  promotion) ; `is_terminal` compare au seuil de la variante. Le miroir Python suit, avec un
  test du compteur.
- La triple répétition et les fins de partie réduites (16 coups) ne sont toujours pas
  gérées.
- Aucun changement de sauvegarde : le compteur tient toujours sur un octet.

### 2026-09-25 — Dames : l'IA ne déborde plus la pile et ne réfléchit plus sans fin

Point 2.1 de l'audit du 25/09/2026 (« l'IA ne joue jamais son coup, niveau Amateur »).

- **Pile** : chaque niveau de la recherche posait une liste de 96 coups (4,7 Ko) sur la
  pile de la tâche ESPHome, qui n'en a que 8. Au premier coup réfléchi, `negamax` puis
  `has_legal_move` réservaient déjà ≈ 10 Ko. Les listes vivent maintenant dans un tampon
  de 42 Ko alloué au premier coup réfléchi (RAM interne, PSRAM en repli) et rendu à la
  fermeture du jeu. `has_legal_move` n'a plus besoin de liste : un coup trouvé suffit.
  Réservations relevées dans le binaire : `negamax` 5 008 → 192 o, `quiescence`
  4 976 → 176 o, `has_legal_move` 4 720 → 80 o, `eval_full` 4 720 → 32 o.
- **Réflexion sans fin** : le budget de nœuds d'une tranche s'appliquait coup racine par
  coup racine, et un coup dont le sous-arbre dépassait ce budget était repris de zéro à
  chaque tranche, à l'identique. Le niveau Expert pouvait ainsi réfléchir indéfiniment
  (reproduit sur le miroir Python du moteur : une partie sur trois bloquée au 47ᵉ
  demi-coup). Désormais le budget double (jusqu'à ×4), puis l'IA joue le meilleur coup de
  la dernière profondeur complète. Hors de ce cas, l'IA choisit exactement les mêmes coups.
- **Logs** : une ligne quand l'IA prend la main, une quand elle joue (durée, tranches,
  nœuds, profondeur atteinte, pile libre minimale de la tâche).
- Si l'allocation échoue, l'IA joue un coup du niveau Débutant au lieu de réfléchir, et le
  signale dans les logs.

### 2026-09-25 — Jeux : sauvegarde d'Arcanoïde vérifiée au chargement, record du Coureur d'Or

Les deux bugs relevés pendant le lot 8f.

- **Arcanoïde** : une sauvegarde abîmée mais au bon en-tête (`magic`) était reprise telle
  quelle. `score_count` > 10 faisait lire le classement hors du tableau des scores à la
  partie suivante, et un `ctrl_mode` hors 0..2 laissait la raquette sans commande (ni
  inclinaison ni boutons). Le chargement borne maintenant ces champs et la sensibilité,
  comme le fait déjà le Coureur d'Or.
- **Coureur d'Or** : le record n'était mis à jour que si le score entrait dans le top 10.
  Quand des parties hors concours (mode entraînement) occupent les dix places, une partie
  classée pouvait battre le record sans entrer au classement, et le record restait l'ancien.
  Le record se juge maintenant à part, et la sauvegarde est écrite dans les deux cas.

### 2026-09-25 — Jeux : les mécanismes recopiés entre consoles passent dans `game_common.h`

Lot 8f de l'audit du 25/09/2026 (§6), dernier du lot 8. Aucun changement de comportement :
chaque helper reproduit exactement l'ordre des opérations d'origine, et les seuils, périodes,
délais et gardes d'état restent dans chaque jeu — ce sont eux qui font son ressenti.

- `timer_period_sync()` : tick adaptatif de Go, Trivia, Coureur d'Or et Neon Apron.
- `topn_insert<>()` : classement d'Arcanoïde, de Coureur d'Or et de Neon Apron. Pour Neon
  Apron, dont le tableau n'a pas de compteur (cases vides à 0), c'est le même algorithme
  avec un compteur fixé à N, puisque le score inséré est toujours > 0.
- `tilt_calibrate()` (calibration « à plat » de 4 jeux) et `tilt_smooth()` (inclinaison
  lissée de Fil d'Or et Coureur d'Or, chacun avec son coefficient).
- `accel_delta_norm()` et `shake_fire()` : secousse d'Échecs (norme lissée, 1,9 g / 900 ms),
  Trivia (2,2 g / 900 ms), Dames (variation, 1,2 / 800 ms) et Go (1,4 / 1 200 ms).
- Laissés tels quels, à dessein : les lignes de menu (`slot_list`, une géométrie et un ordre
  d'opérations par jeu), `slots_hide_from` (chaque jeu garderait un emballage d'une ligne),
  le coup de hanche du flipper (passe-haut sur deux axes), la course de Fil d'Or.
- Relevé exhaustif fait avant d'écrire une ligne (code exact de chaque copie, différences
  de constantes, d'ordre et de gardes) ; chaque remplacement est parti du texte d'origine
  exact.
- Signalés, non corrigés ici (ce serait changer le comportement) : Arcanoïde ne borne pas
  `score_count` au chargement NVS (une sauvegarde corrompue ferait lire hors du tableau des
  scores), et Coureur d'Or ne met pas à jour `best` quand un score hors top 10 le dépasse
  (possible si des parties hors concours occupent le haut du classement).

**Preuve** (`nm -S`, `main` @ `a1d2999` contre cette branche) : sur 18 404 symboles, seules les
8 fonctions retouchées changent de taille, de quelques octets (génération de code, par
exemple `Go::on_imu` 226 → 184 o), et rien d'autre ne bouge. `config_hash` identique
(0x485c6667), flash 2 843 524 → 2 843 470 o (−54), RAM identique, 0 warning.

### 2026-09-25 — YAML : templates pour les widgets recopiés (arcade, bandeaux d'alerte, popup réveil)

Lot 8e de l'audit du 25/09/2026 (§6, règle 5 : un widget répété 3 fois ou plus passe par un
template `!include` + `vars`). Aucun changement de comportement.

- **5 templates** dans `Tab5/ui_components/`, sur le modèle de `cal_day_cell.yaml` :
  - `arcade_card.yaml` : les 8 cartes de la page Arcade (`game_selector.yaml` 291 → 123
    lignes). Ajouter une console = une ligne `!include` (`docs/arcade.md` mis à jour) ;
  - `ha_alert_panel.yaml` : les 4 bandeaux d'alerte HA de la carte centrale
    (`tab5-lvgl.yaml` 742 → 666 lignes) ;
  - `alarm_day_chip.yaml` (×7), `alarm_step_script_btn.yaml` (×10, pas scripté : heure,
    bornes, mélodie) et `alarm_step_number_btn.yaml` (×10, `number.${op}`) : 27 boutons du
    popup réveil (`alarm_popup.yaml` 771 → 420 lignes).
- **Aucun bloc remplacé à l'aveugle** : chaque bloc d'origine a d'abord été reconstruit à
  partir du gabarit avec ses propres valeurs et comparé octet pour octet ; un seul écart et
  le script s'arrêtait sans rien écrire.
- **Deux pièges vus en route**, documentés dans les templates :
  - dans un mapping en ligne `{ … }`, une valeur `${…}` doit être entre guillemets
    (l'accolade casse la syntaxe YAML, yamllint le signale) ;
  - la substitution garde le type de la variable : un paramètre de script (`slot`, `day`,
    `delta`) doit recevoir un nombre NON quoté dans `vars`, sinon il part en chaîne
    (`slot: '0'`). Les coordonnées ne sont pas concernées : leur schéma les convertit.

**Preuve** : `esphome config` (worktrees jetables, fichiers d'exemple, secrets factices) donne
les mêmes 18 457 lignes que `main`, à une exception près : l'ordre de deux clés par défaut
(`long_press_time` / `long_press_repeat_time`), qui **varie d'un lancement à l'autre sur
`main` lui-même** (vérifié : 3 fois dans un ordre, 1 fois dans l'autre). C'était aussi la
seule différence notée au lot 8c : ce n'était donc pas l'ordre des packages, contrairement à
ce qu'on avait cru. Binaire : **`config_hash` identique à `main` (0x485c6667) et même flash
(2 843 524 o)** ; comparée à la compilation du lot 8d, la table des symboles ne diffère que
par les quatre fonctions du correctif #141, déjà dans `main`.

### 2026-09-25 — Planning de la carte centrale : le bon jour, même un soir de changement d'heure ou sans HA depuis la veille

Deux bugs du même genre dans `build_planning_lines_from_jours()` (`tab5_services.cpp`),
les lignes « 1/ Auj. : 08:00-16:00 » de la carte centrale.

- **Nom du jour faux les nuits de changement d'heure** (audit du 25/09/2026, §5) : J+n
  était calculé par `maintenant + n × 86 400 s`. Une journée de 23 h ou 25 h décalait le
  résultat d'un jour près de minuit (« Mar. » affiché pour le lundi). Il passe par
  `local_day_from_offset()`, normalisée à midi, comme le reste du projet.
- **Horaire de la veille affiché sous « Auj. » quand HA est muet depuis minuit** : la boucle
  lisait `cal_jours_data[jour]` comme si la case 0 était aujourd'hui, alors que le lot est
  daté par le jour de sa poussée (lot 2). Elle passe maintenant par `cal_index_for_offset()`,
  le recalage que le réveil utilise déjà : la fonction sort d'`alarm_clock.cpp` vers
  `tab5_core.cpp` pour être partagée. Un lot non daté (reçu avant la synchro SNTP) garde
  l'ancienne lecture, case 0 = aujourd'hui.
- Test hôte : `cal_index_for_offset()` vérifié directement (lot du jour, lot de la veille,
  lot périmé, lot non daté) ; le calcul de date était déjà couvert par les nuits de
  changement d'heure du test du réveil.

### 2026-09-25 — Firmware : `tab5_custom.h` ne déclare plus que le contrat YAML, noms de jours et de mois au même endroit

Lot 8d de l'audit du 25/09/2026 (§6). Aucun changement de comportement.

- **`tab5_custom.h` = exactement les 76 fonctions qu'une lambda YAML appelle** (vérifié par un
  script de classement : 76 appelées depuis le YAML, 0 interne, 0 sans appelant). Les 17
  autres en sortent :
  - 8 ne servaient qu'à leur propre unité et y deviennent `static` :
    `central_panel_is_active`, `central_panel_wrapper`, `get_day_planning_display_text`,
    `sync_central_panel_visibility`, `format_assist_markdown`, `parse_and_update_heures_bulk`,
    `setup_button_press_animation`, `update_rain_bar_ui` ;
  - 9 sont partagées entre unités et passent dans `tab5_internal.h` : `transition_widgets`,
    `close_popup_if_open`, `animate_swipe_horizontal/_alert_enter/_icon_roll_in`,
    `get_temperature_color`, `tab5_dismiss_local_has/_prune`, `update_rain_phrase_ui`.
- **Noms de jours et de mois : une seule source, `tab5_core.cpp`.** Les tables recopiées de
  `tab5_anim.cpp` (date sous l'horloge), `tab5_services.cpp` (« Dim. »), `tab5_calendar.cpp`
  (« Janvier », « Lundi ») et `tab5_text.cpp` (« Janv ») disparaissent ; chaque appelant garde
  son format à partir de `fr_day_short_utf8()`, `fr_day_long_utf8()`,
  `fr_month_long_utf8()`, `clock_month_short_utf8()` et `fr_capitalized()` (majuscule
  initiale, aussi utilisée par le libellé du réveil). Mêmes octets affichés partout.
- **Règle 6** (glyphes de la date sous l'horloge) : elle lit désormais les deux tables dans
  `tab5_core.cpp` ; prouvé par mutation (un « ï » glissé dans « Dim » est signalé). Le test
  hôte du noyau vérifie les nouveaux noms (jours et mois abrégés, majuscule d'un mois accentué).
- Reste signalé, non corrigé ici (changement de comportement) : `build_planning_lines_from_jours()`
  calcule J+n par `maintenant + n × 86 400 s`, faux d'un jour les nuits de changement d'heure
  (audit §5) — à passer par `local_day_from_offset()`.

**Preuve** (`main` @ `9602c2f` contre cette branche, deux compilations) : **`config_hash`
identique** (0x485c6667, aucun YAML ne change). Côté binaire (`nm -S`), tout écart
s'explique : les helpers devenus `static` sont inlinés dans leur unique appelant
(`format_assist_markdown` disparaît, `assist_set_response` grossit d'autant ; idem
`get_day_planning_display_text` → `show_temporary_planning`, `update_rain_bar_ui` →
`update_rain_bars_bulk_ui`, `setup_button_press_animation` → `apply_pressed_scale_to_tree`),
les tables recopiées disparaissent (`cal_month_name_utf8::months`, `days_short`), et
`fr_day_short_utf8` + `fr_capitalized` apparaissent. Flash 2 843 934 → 2 843 526 o
(**−408**), RAM 258 370 o (identique), 0 warning.

### 2026-09-25 — YAML : un package par fonctionnalité (arcade, calendrier, assistant vocal)

Lot 8c de l'audit du 25/09/2026 (§6). Aucun changement de comportement.

- **`tab5-scripts.yaml` n'est plus un fourre-tout** (1 147 → 459 lignes) : sur le modèle
  de `tab5-alarm.yaml`, trois familles partent dans leur package, contenu inchangé.
  - `Tab5/tab5-arcade.yaml` : fermeture globale des jeux, ouverture des 8 consoles, page
    arcade ;
  - `Tab5/tab5-calendar.yaml` : les 7 scripts du popup calendrier ;
  - `Tab5/tab5-assist.yaml` : tout l'assistant vocal — la pile (`micro_wake_word`,
    `voice_assistant`, image de la réponse), sortie de `tab5-hardware.yaml` (473 → 314
    lignes, qui garde le matériel audio), et les scripts vocaux et du popup Assistant.
- Les trois packages sont chargés après `tab5_lvgl`, comme `tab5_alarm`. `on_boot` n'est
  pas touché.
- **Règle 2 de `check_tab5_code_rules.py` étendue** : elle interdisait `lv_*` dans
  `tab5-hardware.yaml`, donc dans les callbacks vocaux ; elle l'interdit maintenant dans
  la pile de `tab5-assist.yaml` (tout ce qui précède `script:`), prouvé par mutation.
- Commentaires et docs repointés (`tab5-arcade.yaml` pour ajouter une console,
  `tab5-assist.yaml` pour le modèle « Stop »…). `docs/architecture.md` et
  `docs/voice_assistant.md` disaient encore que les callbacks vocaux coloraient l'icône
  eux-mêmes : c'est `assist_set_pipeline_state()` depuis le 08/09. Les comptes de lignes
  du graphe de la cartographie, jamais vérifiés, sont retirés (le tableau l'est).

**Preuve de neutralité** (mesurée contre `main` @ `caf93c5`, avant le merge de #137) :
- `esphome config` (deux worktrees, fichiers d'exemple et secrets factices) : les mêmes
  18 452 lignes en multi-ensemble. Seule différence : deux clés par défaut d'un même bloc
  (`long_press_time`, `long_press_repeat_time`) sortent dans l'ordre inverse.
- Binaire (`nm -S`) : les 18 408 symboles se correspondent une fois neutralisés les
  numéros auto-générés (lambdas, `ifaction_id_N`, tableaux de polices), qui suivent
  l'ordre des packages. Seul vrai écart : `setup()`, qui construit les composants dans
  un autre ordre, maigrit de 1 286 o.
- Flash 2 844 996 → 2 843 706 o (−1 290), RAM −8 o, 0 warning, `config_hash` 0x4906c7d8.
### 2026-09-25 — Réveil : moteur testé sur PC, dates et données calendrier dans un noyau pur

Lot 8b de l'audit du 25/09/2026 (§6, §7 : « le plus rentable, le moteur du réveil »).
Aucun changement de comportement.

- **`tools/test_alarm_clock.cpp`** : le vrai moteur (`alarm_clock.cpp` + `tab5_core.cpp`)
  compilé par g++ et exécuté en CI, horloge simulée, fuseau Europe/Paris. 13 scénarios :
  heure fixe et jours cochés, sonnerie consommée une seule fois, répétition puis arrêt,
  fenêtre de grâce au démarrage, réglage posé dans le passé, mode embauche (délai, bornes,
  horaire inconnu), repos minimum après une fermeture tardive, calendrier daté par son jour
  d'ancrage (non-régression du bug §2.2), repos silencieux ou à heure fixe, calendrier
  absent ou périmé, nuits de changement d'heure (8 h et 10 h réelles entre 22:00 et 07:00),
  rendez-vous (annonce unique, appariement par epoch, entrées illisibles).
- **`Tab5/tab5_core.h/.cpp`** : logique pure partagée par le HMI et le réveil —
  `DayForecastData`, `cal_jours_data[]`, `local_day_from_offset()`,
  `local_day_number_today()`, jours et mois en toutes lettres, titres de jour. Déplacés
  tels quels de `tab5_custom.h` / `tab5_text.cpp` ; l'heure passe par `tab5_time_source`
  (par défaut `time`) pour que le test fixe « maintenant ». `tab5_custom.h` l'inclut.
- **`Tab5/alarm_render.h/.cpp`** : le rendu LVGL du réveil sort d'`alarm_clock.cpp`, qui
  n'inclut plus ni ESPHome ni LVGL (vérifié par `-fsyntax-only` avec `-I Tab5` seul).
  `hhmm()` devient `alarm_hhmm()` (partagée avec le rendu) ; `alarm_reset_state()` remet le
  moteur à l'état du démarrage pour les tests (écartée du binaire par l'éditeur de liens).
- Règle 7 de `check_tab5_code_rules.py` : les trois fonctions du réveil qui posent une icône
  sont repointées vers `alarm_render.cpp` dans `MDI_CODE_TARGETS` — la règle avait signalé le
  déménagement, comme prévu.

Firmware : 0 warning, RAM 258 378 o (identique), flash 2 844 970 → 2 845 224 o (**+254**) —
comparé symbole par symbole : deux instanciations de `std::string` recopiées dans la
nouvelle unité `tab5_core.cpp` (≈ 190 o), l'inlining qui change d'une unité à l'autre
(`cal_is_early_shift`, `set_label_text_utf8`…) et le pointeur `tab5_time_source` (4 o).
Aucune fonction du moteur ne change de logique. `config_hash` 0x82dc12d5 (liste `includes:`).

### 2026-09-25 — Firmware : jetons sans dépendance, les jeux ne recompilent plus avec le HMI, code mort retiré

Lot 8a de l'audit du 25/09/2026 (§6, organisation). Aucun changement de comportement.

- **`Tab5/tab5_tokens.h`** : `UIColor::`, `UIAnim::` et `UIIdle::` sortent de `tab5_custom.h`
  dans un en-tête sans dépendance (que des `constexpr`). `tab5_custom.h` l'inclut : les
  lambdas YAML et les unités C++ ne voient aucune différence.
- **Les 8 consoles n'incluent plus `tab5_custom.h`** : Arcanoïde et Fil d'Or, qui lisent
  quatre couleurs, incluent `tab5_tokens.h` ; les six autres n'incluent plus rien du HMI.
  Une retouche du dashboard ne recompile plus les jeux : 20 159 des 25 909 lignes de C++ de
  `Tab5/` (78 %), ce qui compte vu les gels du PC pendant les compilations.
- **Code mort retiré** (chaque symbole vérifié sans appelant, lambdas YAML comprises) :
  `alarm_melody_ms()` et le champ `ms` des mélodies (le cycle de sonnerie s'arrête sur
  `rtttl.is_playing`), `alarm_mode_name()` et sa table, `alarm_snooze_until()`,
  `rdv_count()`, `cal_cache_clear()`, `cal_month_has_details()`, `GameRegistry::count/at`,
  `ModalRegistry::count`, `UIAnim::POPUP_IN/POPUP_OUT/BTN_PRESS`.
- **Contrats corrigés** : `alarm_snooze()` promettait `false` au-delà d'un maximum de
  répétitions qui n'existe pas et rendait toujours `true` ; elle devient `void`. Les
  commentaires de `animate_popup_open/_close` annonçaient des durées alors que les deux
  sont instantanées.
- Gardés à dessein : `Chess::perft_log` (outil documenté de validation du générateur sur
  la cible) et `Draughts::ai_step`, qui attend l'enquête sur l'IA des dames.

**Preuve de neutralité** (table des symboles de l'ELF, `nm -S`, avant/après) : sur 18 408
symboles, seuls cinq changent, tous attendus — `kMelodies` (−16 o, champ `ms`),
`alarm_melody_name/_rtttl` (−4 o chacune), `alarm_snooze` (plus de valeur de retour) et
l'initialiseur statique de `tab5_calendar.cpp`, renommé d'après sa première fonction. Les
fonctions retirées n'étaient déjà plus dans le binaire (`--gc-sections`) ; le découplage des
jeux ne change aucun symbole. Flash 2 844 996 → 2 844 970 o (−26), RAM 258 378 o
(identique), 0 warning, `config_hash` 0xf09d9e81 (la liste `includes:` change).

## [2.0.0] — 2026-09-25

De `v1.2.0` (08/09) à aujourd'hui : 21 pull requests (#116 → #136, celle de la release
comprise), dont le **deuxième audit complet du dépôt** (25/09) et ses lots — push HA allégé, réveil daté, rendu 2,6× plus rapide
au push, 284 Ko de flash rendus, jeux qui survivent à l'écran éteint, icônes enfin justes, CI
qui vérifie ce qu'elle annonce, docs remises d'aplomb. **Version majeure** parce que deux
changements sont cassants pour qui met à jour depuis 1.x.

### ⚠️ Changements cassants — à lire avant de mettre à jour

- **ESPHome 2026.9.0 minimum, OTA chiffrée par la clé API, envoi en clair refusé** (#124,
  [ADR-0015](docs/decisions/0015-ota-encrypted-with-api-key.md)). Le poste qui flashe doit avoir
  `api_encryption_key` dans `secrets.yaml` ; `ota_password` n'est plus lu. **Depuis un
  firmware 1.x**, qui n'accepte que l'OTA à mot de passe, passer par une OTA intermédiaire :
  compiler avec ESPHome 2026.9.0 en retirant `encryption:` du bloc `ota:`
  (`Tab5/tab5-hardware.yaml`) et en remettant `password: !secret ota_password`, flasher, puis
  flasher la 2.0.0 telle quelle — c'est le chemin suivi le 16/09. À défaut : flash USB.
- **Service `tab5_maj_pluie_1h` retiré** (#117) : une automation HA qui l'appelle encore
  échoue. La remplacer par `tab5_maj_pluie_1h_bulk` (les 9 barres en un appel, payload
  `idx|intensité;…`), comme dans `HomeAssistant_Config/automations_examples.yaml.example`.
- Recommandé, sans être cassant : reprendre l'automation de push de l'exemple. Le firmware
  n'émet plus `esphome.tab5_connected` toutes les 5 min, seulement à la connexion du client HA ;
  l'exemple filtre ce déclencheur, ne relance plus tout sur les attributs de `next_rain`
  (`to: ~`) et lit les prévisions en `.get()`.

### Mesures de la version

- **Push HA** : 18 → 6 déclenchements complets par heure ; boucle de rendu à la minute du push
  **122 → 46-48 ms** (poussées identiques ignorées par empreinte, seul le calque visible repeint).
- **Flash** : 3 129 074 → **2 844 996 o** (−284 078 o) entre le lot 3 et la fin — polices
  (270/190 px, `roboto_45` réduite) et 85 glyphes d'icônes jamais affichés.
- **CI** : artefact `tab5-firmware` de nouveau publié, `python` et `build` requis, ccache
  conservé entre deux compilations (job `build` : 633 → 202 s sur `main`).
- **Firmware de la release** : `config_hash` 0x26da4842, flashé sur l'appareil de l'auteur
  (OTA du 25/09, 17:10).

### 2026-09-25 — Pictogrammes : un robot pour la discussion, un tableau pour les tableaux, un glissement de terrain pour les avalanches

Suite du lot polices : les noms relevés dans le TTF MDI ont montré trois icônes qui ne
disaient pas ce qu'elles annonçaient.

- **Mode « Discussion LLM »** (bouton « Discu » du dashboard et bouton du popup assistant) :
  F0450 `refresh` → **F06A9 `robot`**, l'intention notée dans l'ancien commentaire. Le
  `refresh` de la Console système reste : c'est bien « Recharger autos ».
- **En-tête de la carte « RÉPONSE »** de l'assistant, à côté de l'icône image : F0A71 `smog`
  (un nuage de pollution) → **F04EB `table`**.
- **Vigilance avalanches** : F067E `weather-lightning-rainy` (un orage, confondu avec la
  vigilance orages) → **F1A48 `landslide`** ; MDI n'a pas d'icône avalanche.

Glyphes échangés dans `mdi_font_32`, `mdi_assist_36` et `mdi_font_alert` (règle 7 verte).
Flash 2 844 802 → 2 844 996 o (+194), RAM inchangée, 0 warning, `config_hash` 0x26da4842.

### 2026-09-25 — Polices : l'icône du réveil éteint revient, 85 glyphes d'icônes jamais affichés retirés

Audit des polices après le lot 4. Chaque icône affichée a été rattachée à son widget,
jusqu'aux appels C++ qui reçoivent le widget en paramètre, puis comparée aux glyphes de
sa police.

- **Bug corrigé : icône vide sur le bouton « Réveil » du popup quand le réveil est
  éteint.** `alarm_render_settings()` y pose la cloche barrée (U+F0023, `alarm-off`), mais
  `mdi_font_45` ne l'avait pas : un glyphe absent s'affiche vide, sans erreur de compilation.
- **« -- » de la consigne clim enfin visible** : `roboto_55_b` n'avait pas le tiret, donc
  le texte initial du popup restait vide jusqu'au premier push de HA. Une consigne inconnue
  (NaN) affiche maintenant « -- » au lieu de « nan » (invisible aussi) et ne déplace plus l'arc.
- **85 glyphes MDI et 2 glyphes météo jamais affichés retirés**. `mdi_font_70` embarquait
  16 icônes d'alerte météo, copiées de `mdi_font_alert`, que rien n'affiche en 70 px.
  `mdi_font_56` gardait F02E4, le « rectangle vide » remplacé en 45 px. Les glyphes F003
  et F004 d'`IconeMeteo.ttf` ne correspondaient à aucune constante de `MeteoIcon`.
- **Label mort retiré** : `icon_mode_status`, caché, sans police, encore réécrit à chaque
  changement de mode de l'assistant.
- **Organisation** : la chaîne Latin-1 (216 glyphes) est déclarée une fois (`&latin1`) au
  lieu de six copies, et les polices MDI sont en liste, une icône par ligne avec son nom
  relevé dans le TTF (les anciens commentaires se trompaient parfois : F0450 est `refresh`,
  pas « discussion (robot) »).
- **Garde-fou : règle 7 de `tools/check_tab5_code_rules.py`** (pytest et CI). Chaque icône
  `\U000Fxxxx` affichée doit être dans la police `mdi_*` de son widget, et chaque glyphe
  d'une police `mdi_*` doit être affiché quelque part. Les icônes posées en C++ passent par
  la table `MDI_CODE_TARGETS` (fonction → widgets) ; une icône qu'on ne sait pas rattacher
  fait échouer la règle. Falsifiée par trois tests sur une copie modifiée du firmware
  (glyphe retiré, glyphe mort ajouté, icône C++ non déclarée). La règle 6 lit les glyphes
  avec le même analyseur (chaîne, ancre ou liste).
- **Docs** : règle de code n° 9 dans `AGENTS.md` et `Tab5/README.md`, renvoi dans
  `docs/arcade.md` et l'inventaire des tests ; ligne MDI du README corrigée (8 tailles,
  `mdi_assist_64` retirée au lot 4) ; comptes de `CARTOGRAPHIE_TAB5.md` réécrits.

Flash 2 867 084 → 2 844 802 o (**−22 282**), RAM −8 o, 0 warning, `config_hash` 0xef195db7.
Les six polices texte gardent exactement leur taille (l'ancre ne change rien au binaire) ;
polices embarquées : 356 638 o.

### 2026-09-25 — Docs : LVGL 9.5, une seule cartographie aux comptes vérifiés, l'arcade dans son propre fichier, trois ADR

Lot 7 de l'audit du 25/09/2026 (§8, documentation). Aucun changement de firmware (deux
commentaires de `chess_game.*` repointés).

- **« LVGL 8.4 » → 9.5** dans `AGENTS.md`, le badge et les deux présentations du `README.md`,
  le kit Hackster : le build compile LVGL 9.5.0 (`lv_version.h`). Piège direct pour un agent
  qui choisit une API d'après la doc.
- **`Tab5/README.md`** : l'OTA n'est plus « protégée par mot de passe » mais chiffrée par la
  clé API (ADR-0015) ; les 9 comptes de lignes des unités C++ retirés (ils ne vivent plus
  que dans la cartographie).
- **Les 8 consoles déménagent dans `docs/arcade.md`** (42 Ko) : elles faisaient ≈ 80 % du
  `Tab5/README.md` (68 → 27 Ko) qu'`AGENTS.md` impose de lire avant toute modification.
  Un résumé et un lien restent ; `AGENTS.md` dit de lire `docs/arcade.md` seulement pour un
  jeu. Liens repointés (README ×2, `docs/screens.md` ×2, `chess_game.*`, `test_chess_perft.py`).
- **Une seule `CARTOGRAPHIE_TAB5.md`**, celle du dépôt : les deux corrections que seule la
  copie de `contexte_ia/` portait (`cal_heures[15]` retiré le 08/09) y sont reportées, et
  **22 comptes de lignes sur 49 étaient faux** (`tab5-api-logic.yaml` annoncé à 332 lignes pour
  508). Nouveau `tools/cartographie_counts.py` : `--write` les recalcule, la vérification
  (tolérance 20 %) est jouée par pytest — elle ne peut plus dériver en silence.
- **`AGENTS.md`** : trois moteurs testés (pas deux), six garde-fous (pas trois), CI décrite
  telle qu'elle est depuis le lot 6 (PR + `main`, `python` et `build` requis, ccache, `.md`
  exclus). Arborescence du `README.md` complétée (`tests/`, `check_*.py`, `tab5_registry.*`,
  pre-commit) ; `docs/INVENTAIRE_CONFIGS_TESTS.md` remis à jour.
- **Trois ADR** : 0015 OTA chiffrée par la clé API (clair refusé, pas de mot de passe),
  0016 CI sur ESPHome `latest` = canari amont voulu, 0017 placeholders publics → valeurs
  dans `placeholders.yaml` → HA déployé depuis `rendered/`.
- **`docs/troubleshooting.md`** : l'incident du 18/09 (icônes des jours disparues, `templow`
  absent à j+14, erreur masquée par `continue_on_error`), avec la façon de forcer un push
  complet depuis que `esphome.tab5_connected` est filtré.
- `docs/hackster*.md` → `docs/press/` (kit de publication, 45 Ko).

### 2026-09-25 — CI : cache ccache entre deux compilations, docs du Tab5 sans recompilation

Suite du lot 6. Le job `build` durait ~10 min, dont **8 min 20 s de compilation pure**
(2 055 cibles ninja sur les 4 cœurs du runner) ; le reste est fixe (image Docker 18 s,
ESP-IDF 40 s, CMake 27 s, génération et `idedata` ≈ 40 s).

- **Cache ccache conservé d'un run à l'autre** (`actions/cache`, clé par run, restauration
  du plus récent). ccache tournait déjà dans le conteneur ESPHome, mais son dossier
  disparaissait avec le runner : les ~2 000 objets ESP-IDF, LVGL et composants, identiques
  d'un commit à l'autre, étaient recompilés à chaque fois. L'heure de build vit dans
  `build_info_data.*` (aucun `-D` horodaté dans les 1 945 unités de `compile_commands.json`
  local) : elle n'invalide pas le cache. `compiler_check = content`, puisque la toolchain
  est réinstallée à chaque run. Les statistiques ccache s'affichent dans le résumé du job.
- **Une PR qui ne touche que `Tab5/README.md` ne recompile plus** : filtre
  `predicate-quantifier: some-with-excludes` + `!**/*.md` (sémantique vérifiée dans le code
  de `dorny/paths-filter@v4` et avec picomatch 2.3).

### 2026-09-25 — CI : l'artefact firmware revient, les garde-fous couvrent enfin les fichiers publics

Lot 6 de l'audit du 25/09/2026 (§8, CI). Aucun changement de firmware.

- **Artefact `tab5-firmware` de nouveau publié** (sur `main` et en lancement manuel). Le
  chemin visait encore `.pioenvs/…/firmware.bin` (PlatformIO) alors que `build-action`
  copie ses binaires dans `tab5-ha-hmi-esp32p4/` : plus aucun artefact depuis le 15/07,
  avec un simple avertissement. Le chemin vient maintenant de la sortie `name` de l'action,
  et `if-no-files-found: error` fait échouer le job si cela se reproduit.
- **Une compilation par modification au lieu de deux** : `push` limité à `main` (une branche
  avec PR était compilée au push ET à la PR, 2 × ~10 min), et `concurrency` annule la
  compilation d'une PR dépassée par un nouveau commit (jamais sur `main`).
- **Vérificateur de secrets sur les fichiers suivis** (`git ls-files`) au lieu de `*.yaml`
  sur disque : il lit maintenant aussi `.yml` (workflows), `.example`, `.jinja` et `.md`,
  et ignore les fichiers locaux gitignorés, qui ont le droit de contenir des valeurs
  réelles. Un `secrets.yaml` suivi est signalé en soi. Valeurs factices en MAJUSCULES
  (`YOUR_WIFI_PASSWORD`) ignorées, exception ligne par ligne par
  `pragma: allowlist secret`. Premier passage : l'ancienne IP de la tablette traînait
  deux fois dans ce CHANGELOG — retirée. Le step CI qui le relançait après pre-commit
  (double exécution) est supprimé.
- **yamllint lit `automations_examples.yaml.example`** : `identify` le classait en `text`,
  le fichier public le plus copié échappait au lint (prouvé par mutation : une espace en
  fin de ligne est maintenant refusée).
- **`render_ha_config.py --check` vérifie enfin quelque chose en CI** : sans
  `placeholders.yaml` (gitignoré) il n'avait rien à chercher et répondait 0. Le fichier
  vient du secret de dépôt `HA_PLACEHOLDERS` ; s'il manque, la CI l'annonce par un
  avertissement au lieu de réussir en silence.
- **Le vrai moteur Go C++ est testé** : `tools/test_go_engine.cpp` est compilé par `g++`
  et exécuté dans le job `python` (le poste de dev n'a qu'un cross-compilateur RISC-V, seul
  le miroir Python tournait jusqu'ici). Le `-fsyntax-only` du cross-compilateur, avant de
  pousser, a trouvé un `#include <initializer_list>` manquant pour
  `for (int n : {9, 13, 19})` : ajouté.
- Modèle de PR : cases pytest/pre-commit et miroirs Python des moteurs Go et échecs.
  `.pre-commit-config.yaml` : avertissement avant tout passage de `pre-commit-hooks` en v6,
  où `check-byte-order-marker` n'est plus qu'un hook « removed » qui échoue toujours.

### 2026-09-25 — Jeux : l'écran éteint ne fait plus perdre, la partie d'échecs survit à un reboot

Lot 5 de l'audit du 25/09/2026 (§2.5, §2.6, §5, §6), sans les dames (reportées).

- **Les chronos ne comptent plus l'écran éteint.** `lvgl.pause` (rétroéclairage coupé,
  y compris par l'automation de présence) arrête les ticks des jeux mais pas l'horloge :
  au rallumage, la pendule d'échecs débitait toute la pause au camp au trait (défaite
  au temps possible), la question Trivia en cours était comptée fausse, et le temps de
  run de Fil d'Or (et donc son record) était gonflé. Chaque jeu détecte maintenant un
  écart de plus de `PAUSE_GAP_MS` (2 s, `game_common.h`) entre deux ticks et ne le
  décompte pas : pendule inchangée, échéances Trivia décalées d'autant (une partie gardée
  en RAM reprend aussi son chrono à la réouverture), début de run de Fil d'Or décalé.
- **Échecs : sauvegarde différée de la partie en cours**, comme le Go (drapeau + au plus
  une écriture NVS toutes les 15 s). Elle n'était écrite qu'à la fermeture de la
  console : un reboot (OTA, watchdog API, coupure) la perdait, ou proposait une partie
  plus ancienne encore marquée reprenable.
- **Go et Trivia : cache de période du tick remis à zéro à chaque ouverture.** C'était une
  `static` locale jamais réinitialisée : fermé pendant une animation, le jeu repartait
  avec un timer à 100 ms en croyant être à 33 ms (animations saccadées).
- **Index de la page d'arcade** : `id(page_arcade)->index` au lieu de `1` en dur (8 retours
  de console + le texte « Écran courant ») — ajouter une page avant l'arcade ne casse plus
  le retour de toutes les consoles en silence.
- Non retenu, à dessein : réécrire les comparaisons `now >= échéance` en différence signée
  pour le bouclage de `millis()` (49,7 jours). Appliquée en masse, la forme signée casse
  les échéances « non armées » à 0 dès 24,8 jours d'uptime (`now < g_frenzy_until`
  deviendrait vrai en permanence) : plus dangereux que le mal, à traiter au cas par cas.

### 2026-09-25 — Flash : 262 Ko de polices jamais affichées retirés

Lot 4 de l'audit du 25/09/2026 (§4.1, §4.2). Mesuré à la compilation : flash
3 129 074 → **2 866 746 o (−262 328, −8,4 %)**, RAM statique 258 602 → 258 354 o (−248),
0 warning, `config_hash` 0x45d032d8 — l'OTA transfère 262 Ko de moins.

- **`font_meteo_main` (270 px) et `font_meteo_main_small` (190 px) retirées** : 161 866 +
  34 521 o de bitmaps que personne n'affichait — `update_meteo_icon()` n'était appelée
  qu'avec `is_card = true`. Les paramètres `f_main` / `f_main_s` et `is_card` disparaissent
  de six signatures (`update_meteo_icon`, `refresh_daily/hourly_forecast`,
  `handle_swipe_gesture`, `reset_forecast_to_main_page`, `apply_forecast_page`) et de quatre
  appels YAML. Rendu inchangé : même police 120/80 px, même ratio 0.4444f.
- **`mdi_assist_64` retirée** (jamais référencée).
- **`roboto_45` réduite à 37 glyphes** : elle ne sert qu'à `lbl_date` (« Jeu 02 Avr »), texte
  fabriqué par le firmware ; la règle « tout Latin-1 » ne vaut que pour le texte poussé
  par HA. **Règle 6** de `tools/check_tab5_code_rules.py` : chaque caractère des jours
  (`update_clock_date_ui`), des mois (`clock_month_short_utf8`), des chiffres et du texte
  initial doit avoir son glyphe — vérifiée falsifiable (retirer « û », « é » ou « 7 » la
  fait échouer).
- **Réponse de l'assistant plafonnée à 4 Ko** avant le formatage Markdown (coupure sur une
  frontière UTF-8, « [...] ») : `format_assist_markdown()` allouait une chaîne par ligne
  et par cellule sur le tas interne, sans borne.
- Non retenus : libérer l'image de l'assistant (le popup se ferme aussi par
  `ModalRegistry::close_all()`, un widget encore branché sur une image libérée lirait de
  la mémoire libérée — et la PSRAM n'est pas contrainte) ; Zobrist des échecs en
  `constexpr` (7,9 Ko de RAM, touche au moteur : avec les dames, plus tard).

### 2026-09-25 — Les poussées HA identiques ne redessinent plus l'écran

Lot 3 de l'audit du 25/09/2026 (§3.1, §3.2). Mesuré le matin même : boucle à 66 ms au
repos, **144 ms** pendant la poussée de 10:50:00, « api took a long time (102 ms) ». Or HA
repousse tout toutes les 10 min, le plus souvent à l'identique, et en LVGL 9 un setter
réécrit et invalide même à valeur égale.

- **Garde anti-rendu** (`push_unchanged()`, empreinte FNV-1a 32 bits + longueur par
  canal) sur les six services qui repeignent : prévisions jours, prévisions heures (par
  bloc), vigilance, pluie 1 h, alertes HA et bandeau info. Pour ces deux derniers,
  l'empreinte inclut les acquittements locaux (`tab5_dismissed_local`), sinon un tap sur
  la dalle suivi d'un push identique n'aurait pas été repeint. Les drapeaux (`has_rain`,
  `has_alerts`…) gardent leur valeur quand le push est ignoré. Le réveil n'en souffre pas :
  les quinze jours changent au moins une fois par jour (libellés décalés), la date
  d'ancrage suit.
- **Prévisions : on ne repeint que le calque à l'écran.** Les tuiles journalières étaient
  repeintes sur les pages horaires (calque masqué), et chacun des TROIS blocs horaires
  repeignait les cinq tuiles de la page affichée — trois fois par push, calque masqué
  compris. `accept_heures_bulk()` ne repeint que si le bloc reçu est celui de la page
  affichée ; `apply_forecast_page()` repeint déjà depuis les données au changement de page.
- **Deux appels horaires au lieu de trois** (prod HA, exemple public, copie privée, mode
  démo) : l'écran n'a que deux pages horaires (créneaux 0-9) ; le bloc 10-14 était analysé
  à chaque cycle mais jamais lu. Le firmware l'accepte toujours (contrat inchangé).
- **Plus de battement `interval: 5min`** : `esphome.tab5_connected` part au boot (on_boot,
  inchangé) et à chaque **reconnexion de Home Assistant** (`api: on_client_connected`,
  filtré sur `client_info` « Home Assistant », après `boot_complete`, 2 s puis
  `api.connected: state_subscription_only`) — `esphome logs` ne relance plus le push.
- Rotateur central : pas de rotation sous un popup ouvert (la bande repeinte toutes les
  8 s sous le voile semi-transparent).

### 2026-09-25 — Réveil daté, carte centrale sans superpositions

Lot 2 de l'audit du 25/09/2026 (§2.2 et §2.4), sans les dames (reportées : l'IA ne joue
pas son coup au niveau Amateur, cause pas encore établie).

- **Le réveil sait de quel jour datent les horaires.** `cal_jours_data[0]` était
  « aujourd'hui » quel que soit l'âge du dernier push : HA muet de minuit à l'heure du
  réveil (mise à jour nocturne, VM en panne comme le 18/09), les modes Embauche/Travail
  appliquaient le planning de la VEILLE — embauche ratée ou sonnerie un jour de repos,
  contre l'exigence « sonne sans HA » d'`alarm_clock.h`. `parse_and_update_jours_bulk()`
  date désormais la case 0 (`cal_jours_anchor_day`, numéro de jour civil local, algorithme
  `days_from_civil` vérifié contre `datetime` sur 25 933 jours) et le moteur relit chaque
  jour avec le bon décalage (`cal_index_for_offset`). Données d'hier : aujourd'hui = case 1,
  et la « fermeture de la veille » devient connue. Plus de 14 jours sans push ou heure pas
  encore synchronisée au moment du push : retour à l'heure fixe, comme sans calendrier.
- **Carte centrale : le rotateur ne s'affiche plus que quand il a la main** (accueil, hors
  planning temporaire et hors réponse vocale — `rotator_owns_card()`). Trois
  superpositions corrigées :
  - un swipe pendant le planning temporaire (6 s) : 6 s plus tard, le titre de la page
    d'ORIGINE s'affichait sur l'accueil, le texte du tap restait dans le bandeau et le
    rotateur tournait par-dessus. Changer de page termine maintenant le planning
    temporaire (et masque une réponse vocale en cours) ;
  - le panneau d'origine n'était jamais restauré, même au premier tap : le timer écrivait
    `g_central_ctx` mais chaque script YAML y recopiait le global `current_central_panel`,
    resté à 0. Le global est passé à `show_temporary_planning()` et réécrit ; un second tap
    pendant les 6 s ne remplace plus le panneau d'origine (bug connu du 08/09) ;
  - un push d'alertes HA, un bandeau info qui se vide ou la fin d'une réponse vocale
    réaffichaient un panneau transparent du rotateur par-dessus le titre de page ou le
    planning du tap.
- `hide_central_panel()` coupe la transition en cours : son callback de fin masquait un
  panneau que la synchro venait de réafficher (carte vide jusqu'au tour suivant, 8 s).
- Alertes HA : un payload de plus de 1 024 octets est rejeté AVANT de vider les slots
  (vidés puis rejetés, ils laissaient des panneaux vides marqués actifs).
- Aucune modification de `on_boot` : l'occupant de la carte est suivi côté C++
  (`apply_forecast_page`, `show/hide_vocal_response_ui`, timer du planning temporaire).

### 2026-09-25 — HA : la pluie ne relance plus tout le push, les prévisions ne cassent plus en silence

Lot 1 de l'audit du 25/09/2026 (côté Home Assistant seulement, aucun changement firmware).

- **`next_rain` ne déclenche plus sur ses attributs.** Le déclencheur `state:` nu de
  « MAJ Ecran Tab5 ESPHome Push » partait à chaque rafraîchissement Météo-France de
  l'attribut `1_hour_forecast` (toutes les 5 min) alors que l'état restait `unknown` —
  trace du 25/09 à 10:49 : *from* `unknown` *to* `unknown`. Chaque fois, la chaîne complète
  repartait (~6 s, 13 appels ESPHome, `calendar.get_events`, deux `weather.get_forecasts`) :
  ≈ 18 poussées complètes par heure au lieu de 6, avec des données identiques, et un rendu
  du bandeau météo à chaque fois côté tablette. `to: ~` = état seulement.
- **Le battement `tab5_connected` ne relance plus la chaîne.** Le firmware réémet
  `esphome.tab5_connected` toutes les 5 min (`interval: 5min`), en plus du boot. Jusqu'ici
  il tombait pile pendant les poussées `next_rain` et finissait en « Already running »
  (×1 627 depuis le 19/09) ; une fois `next_rain` corrigé, il a relancé la chaîne complète
  à son tour (vu en prod à 11:34:03 le 25/09 : la redondance s'était juste déplacée).
  Condition native ajoutée : ce déclencheur ne passe que si
  `binary_sensor.*_ha_api_status` est `on` depuis moins de 3 min — vrai au boot de la
  tablette, à une reconnexion et après un redémarrage de HA, faux pour le battement.
  Reste 6 poussées complètes par heure (cycle /10 min) + les vrais événements. Retirer le
  battement côté firmware relève du lot 3. Les deux changements sont appliqués en prod le
  25/09 (automation gérée par l'UI).
- **Prévisions : `.get()` partout, plus d'accès direct.** Le correctif `templow` du 18/09
  (Météo-France omet `templow` au 15ᵉ jour ; `fcasts[i].templow` levait `UndefinedError`
  avant le `| float(0)`, le payload n'était jamais rendu) n'existait qu'en prod : l'exemple
  public, la copie privée et `rendered/` le réintroduisaient. Même piège sur l'horaire :
  au-delà de ~48 h Météo-France n'envoie plus `precipitation` (constaté le 25/09) —
  `condition`/`temperature`/`precipitation` passent aussi en `.get()` (prod comprise).
  Prouvé dans le moteur HA : l'accès direct échoue, `.get()` rend `unknown 0 0`.
- **Nouvelle garde (d) dans `packages/tab5_health.yaml`** : une automation Tab5 qui
  journalise « Error rendering » déclenche `tab5_health_notify` (au plus une notification
  par heure). HA journalise bien l'erreur en ERROR avant que `continue_on_error` ne la
  rattrape (`helpers/script.py` de 2026.9.2, `log_exceptions` vrai par défaut pour les
  automations). **Prérequis** : `system_log: fire_event: true` (redémarrage de HA) et
  `system_log_event` exclu du recorder — documenté dans l'en-tête du package et le README HA.
- Exemple public : défaut de `swing` aligné sur la prod et le firmware (`stop`, plus `off`).

### 2026-09-17 — Contrat API : chaque action décrite, chaque variable avec un exemple

ESPHome 2026.9.0 accepte des métadonnées sur les actions définies par l'utilisateur
(amont #18881). Home Assistant les affiche dans « Outils de développement → Actions » :
jusqu'ici les 16 actions du Tab5 n'y étaient qu'une liste de champs texte sans indice, alors
que tous leurs payloads sont sérialisés à la main (« idx|heure|condition|… » séparés par `;`,
« epoch|titre » séparés par `~`, 62 caractères hexadécimaux pour un mois de calendrier…).

- **16 actions décrites et 34 variables documentées** dans `Tab5/tab5-api-logic.yaml` :
  `description:` sur l'action, et forme longue `type:` + `description:` + `example:` sur
  chaque variable. Les exemples sont des payloads réels, repris des automations de
  `HomeAssistant_Config/`. Les paramètres réservés (`uv`, `gel` de `tab5_maj_probabilites`,
  `condition`, `temperature` de `tab5_maj_meteo_actuelle`) le disent désormais dans leur
  propre description, au lieu d'un commentaire que seul le firmware voit.
- **Règle 5 de `tools/check_tab5_code_rules.py`** (jouée par `pytest`) : une action sans
  `description:`, ou une variable restée en forme courte `payload: string`, fait échouer la
  suite. Vérifiée falsifiable sur trois oublis simulés (description d'action retirée,
  variable en forme courte, `example:` retiré) — et le premier de ces trois tests a
  effectivement révélé un défaut de la règle, qui acceptait la description d'une variable
  comme celle de son action.
- **Aucun changement de contrat** : noms d'actions, noms et types de variables inchangés,
  donc aucune automation Home Assistant à retoucher.
- **Mesuré** : `config_hash` 0x137762d8 → **0x30c70fbb**, RAM **258 442 o (identique)**,
  flash 3 120 230 → **3 126 256 o (+6 026)**. Les chaînes et leurs tables de pointeurs
  vivent en flash : sur cet ESP32-P4 la RAM ne bouge pas.

### 2026-09-16 — OTA chiffrée avec la clé API, plancher ESPHome 2026.9.0

ESPHome 2026.9.0 (bilan du 16/09/2026 : aucune rupture pour ce projet) apporte le chiffrement
Noise des mises à jour OTA avec la clé API (esphome #18489, #18979).

- **`ota: encryption:`** (`Tab5/tab5-hardware.yaml`) : le bloc vide hérite de
  `api: encryption: key`, une seule clé protège l'appareil. `password: !secret ota_password`
  retiré — redondant (la clé authentifie déjà l'uploader), coûteux (~3,5 Ko de flash + 60 o de
  RAM d'après le warning 2026.9.0) et de toute façon incompatible avec `encryption:`. **Depuis
  ce firmware l'upload en clair est refusé** : le poste qui flashe doit avoir
  `api_encryption_key` dans `secrets.yaml` (le CLI la lit et chiffre seul). `ota_password`
  n'est plus lu ; la ligne peut rester dans un `secrets.yaml` existant.
- **Migration en deux OTA** : 2026.9.0 sans le bloc d'abord (le firmware « offre » alors le
  chiffrement tout en acceptant le clair — faite le 16/09/2026, `config_hash` 0xe62c67fb),
  puis cette PR. Redescendre = retirer `encryption:`, remettre `password:`, flasher.
- **`min_version: 2026.9.0`** (`tab5-ha-hmi.yaml`, badge README, `docs/installation.md`,
  `docs/hackster.md`) : cette fois une vraie dépendance d'API (`ota: encryption:` n'existe
  pas avant). En prime, sans rien changer : effacement flash paresseux par blocs de 64 Kio
  (préparation OTA < 0,2 s au lieu de ~4 s) et délais tolérants aux acks perdus (#18580,
  #19041), correctifs i2s du bus partagé micro/haut-parleur (#19045, #19027, #19046), API
  qui coupe la connexion au lieu de crasher quand l'allocation d'un tampon échoue (#18802,
  #18803).
- **CI** : `ota_password` retiré du `secrets.yaml` factice (plus lu) ; `api_encryption_key`
  factice conservée, c'est elle que `ota: encryption:` hérite. `docs/installation.md` :
  l'exemple de `secrets.yaml` perd `ota_password` et gagne `wifi_ap_password` (secret
  réellement lu par `wifi: ap:`, absent de l'exemple jusqu'ici).
- **Mesuré** (ESPHome 2026.9.0, build incrémental) : `config_hash` 0xe62c67fb → **0x137762d8**,
  RAM 258 474 → **258 442 o (−32)**, flash 3 121 312 → **3 120 230 o (−1 082)**. Le gain net est
  plus petit que les « 3,5 Ko » du warning : le firmware précédent portait déjà le code Noise de
  l'OTA (il « offrait » le chiffrement), seul le chemin mot de passe disparaît ici.

### 2026-09-08 — Firmware : les sensors n'appellent plus LVGL, init des boutons dans `on_boot`, polices

Audit du 06/09/2026, §4.1 points 8, 12 et 14 — dernier lot. Rien de visible à l'écran.

- **Règle 2 (`sensor:`/`text_sensor:` sans `lv_*`)** : les 19 appels LVGL restants de
  `tab5-sensors-diagnostics.yaml` et `tab5-sensors-domotique.yaml` (icônes HA, Wi-Fi, PC,
  TV, téléphone, salon, ligne « HA » de la console) passent par quatre helpers C++ :
  `set_icon_color_ui()`, `set_icon_active_ui()`, `update_pc_status_ui()` (`tab5_cards.cpp`)
  et `update_console_ha_status_ui()` (`tab5_console.cpp`) — mêmes couleurs, mêmes gardes
  `nullptr`. `tools/check_tab5_code_rules.py` couvre désormais les deux fichiers sensors :
  le prochain `lv_*` qui y revient fait échouer `pytest`.
- **Init des micro-interactions** (`apply_pressed_scale_to_tree` + pointeurs du rouleau
  d'horloge) : l'`interval: 2s` à flag `static` de `tab5-styles.yaml` (logique d'init dans
  le fichier de styles) devient un `on_boot` `priority: -100` + `delay: 2s` dans
  `tab5-ha-hmi.yaml`, à côté des autres étapes de démarrage : même fenêtre (2 s après la
  fin du setup, layout LVGL fait), une seule exécution par construction, plus de `static`.
- **Polices** : `mdi_font_60` (un glyphe, jamais référencé) retiré ; `mdi_font_80`, qui
  chargeait du 70 px, renommé `mdi_font_70` (six usages). Les jeux de glyphes Latin-1 +
  Windows-1252 des Roboto sont **gardés** : ces polices affichent du texte poussé par HA
  (titres d'événements, réponses de l'assistant), un glyphe absent s'affiche vide, et le
  gain de flash ne vaut pas ce risque. Les deux `static` de `tab5-imu.yaml` restent
  (état d'un seul handler, tolérés par la règle 3).

### 2026-09-08 — Outillage : pre-commit (yamllint, BOM, secrets, placeholders HA)

Audit du 06/09/2026, §6 (« pas de pre-commit : yamllint, détection de BOM et vérificateur
de secrets suffiraient »).

- `.pre-commit-config.yaml` : `check-byte-order-marker` + `check-merge-conflict`
  (pre-commit-hooks v5.0.0), `yamllint --strict` (v1.38.0, règles dans `.yamllint`), et deux
  hooks locaux, `tools/verifier_secrets_config.py` et `tools/render_ha_config.py --check`.
  Aucun hook ne modifie un fichier : ils signalent. `pre-commit install` une fois
  (CONTRIBUTING, AGENTS), la CI rejoue `pre-commit run --all-files` dans le job `python`.
- `.yamllint` : base « relaxed », sans `line-length` (lambdas, Jinja), `new-lines` (CRLF
  Windows), `truthy` (`on:`/`off:`), `document-start` ni `commas` (alignement voulu des
  mappings en ligne) ; le fragment `snippets/tab5_alerts_dismissed_input_text.yaml`
  (indenté par construction) et `rendered/` sont ignorés.
- Pour que tout le dépôt passe du premier coup : espaces en fin de ligne retirés sur 12
  lignes (`template_sensors_examples.yaml`, `tab5-globals.yaml`, `tab5-lvgl.yaml`, les deux
  fichiers sensors), lignes vides finales retirées dans cinq YAML ESPHome, et la liste
  `then:` du `on_boot` `priority: 600` de `tab5-ha-hmi.yaml` indentée comme les autres
  (`indent-sequences: consistent`) — même document YAML, vérifié par comparaison des
  arbres chargés avant/après.
- `requirements-dev.txt` : `pre-commit`, `yamllint`.

### 2026-09-08 — HA : script de notification santé, acquittement sans push lourd, macros Jinja du calendrier

Audit du 06/09/2026, §5. Fichiers publics de `HomeAssistant_Config/` uniquement — à redéployer
depuis `rendered/` (voir l'ordre de déploiement du README HA : `custom_templates/` d'abord).

- `packages/tab5_health.yaml` : les trois gardes appelaient chacune
  `persistent_notification.create` puis `notify.notify` avec les mêmes champs. Un script
  `tab5_health_notify` (persistante + mobile, `continue_on_error` par canal, `mode: parallel`)
  les remplace ; titres et messages inchangés.
- `packages/tab5_alerts.yaml` : les deux scripts d'acquittement relançaient
  `automation.maj_ecran_tab5_esphome_push` — la chaîne complète (~5,5 s de delays,
  calendrier, météo) — alors que l'automation « push léger » (`tab5_ha_hmi_alerts_push`) se
  déclenche déjà sur `input_text.tab5_alerts_dismissed` et repousse les sections 1, 7 et 7b
  filtrées par la liste (vérifié sur la configuration de prod le 08/09). Les deux
  `automation.trigger` sont retirés : un acquittement ne coûte plus qu'un push léger.
  L'exemple public de cette automation est resynchronisé avec la prod (section 7 filtrée
  par la liste d'acquittement, section 7b ajoutée) — l'audit demandait de « cibler l'id »,
  mais un appel de service ne cible qu'un `entity_id`, et le vrai défaut était le double push.
- `packages/tab5_calendar.yaml` : le bloc « début / fin / résumé / couvre le jour » recopié
  dans chacune des onze boucles devient quatre macros dans
  `custom_templates/tab5_calendar.jinja` (`ev_start`, `ev_end`, `ev_summary`, `couvre`),
  importées en tête des trois templates. Équivalence vérifiée deux fois : les macros contre
  l'expression d'origine dans le moteur Jinja de HA (0 écart sur 40 cas), puis les trois
  templates rendus avant/après sur des événements de test (même sortie). **Déploiement** :
  `rendered/custom_templates/tab5_calendar.jinja` → `config/custom_templates/`, puis
  `homeassistant.reload_custom_templates`, avant le package — sans lui les deux scripts
  échouent à l'import. `tools/render_ha_config.py` rend désormais aussi `custom_templates/`.

### 2026-09-08 — Firmware C++ : contexte du planning temporaire, `cal_heures[]` retiré

Audit du 06/09/2026, §4.2 points 18 et 19. C++ seul : `config_hash` inchangé.

- `show_temporary_planning()` (`tab5_central.cpp`) : les neuf `static` de fichier que le
  timer de restauration (6 s) devait retrouver — dont un pointeur vers le global ESPHome
  `is_showing_temp_planning` — sont regroupés dans une `TempPlanningCtx`, une seule
  instance de fichier, même approche que `CentralPanelCtx`. Aucun changement de
  comportement.
- `cal_heures[15]` (`tab5_custom.h`) retiré : il recopiait
  `cal_jours_data[].heures_ouverture` (seule écriture, au même endroit, dans
  `parse_and_update_jours_bulk()`), et ses deux lecteurs —
  `get_day_planning_display_text()` et `build_planning_lines_from_jours()` — arbitraient
  entre deux copies d'une valeur toujours identique. Dette listée dans la cartographie
  §4.2 depuis le 06/07, soldée.

### 2026-09-08 — Firmware : hygiène YAML, reste de l'audit du 06/09

Audit du 06/09/2026, §4.1 points 9, 11, 13 et 16. Rien de visible à l'écran.

- **`ota: on_end` retiré** : le `delay: 2s` puis `button.press: btn_restart` (« fix
  reboot OTA » du 05/07/2026) était du code mort. ESPHome redémarre lui-même 100 ms
  après le déclencheur `on_end` (`components/esphome/ota/ota_esphome.cpp` :
  `notify_state_(OTA_COMPLETED)` → `delay(100)` → `App.safe_reboot()`), le délai de
  2 s n'aboutissait donc jamais. L'écran noir après OTA (co-processeur C6 non
  réinitialisé par un reboot logiciel, `docs/troubleshooting.md`) ne se règle par
  aucun redémarrage logiciel : le bloc `ota:` le dit désormais, pour que personne ne
  remette ce `on_end`.
- **`game_selector.yaml` sans hex en dur** : ses 48 couleurs (24 teintes) passent en
  tokens `color:` de `tab5-styles.yaml` — 20 tokens `color_arcade_*` nouveaux, et
  quatre tokens existants réutilisés là où la valeur était déjà la même
  (`color_marble_floor`, `color_arkanoid_floor`, `color_trivia_floor` pour trois fonds
  de carte, `color_text` pour les titres). Valeurs identiques au hex près, vérifiées
  par script : le sélecteur arcade ne fait plus exception à la règle 1.
- **Fin du double interligne** dans `tab5-sensors-diagnostics.yaml` (116 lignes vides
  retirées) et `tab5-sensors-domotique.yaml` (44) : `git diff --ignore-blank-lines`
  vide, le contenu n'a pas bougé.
- **`captive_portal:` et l'AP de secours documentés** comme conservés à dessein : seul
  chemin de récupération sans câble USB après un flash avec de mauvais identifiants
  Wi-Fi, actifs seulement après l'échec de la connexion.

### 2026-09-08 — Firmware : plus de trace INFO à chaque push météo

Audit du 06/09/2026, §4.2 point 24. Le projet tourne en `logger: level: INFO`.

- `parse_and_update_heures_bulk()` / `parse_and_update_jours_bulk()` (`tab5_forecast.cpp`)
  journalisaient la longueur de chaque payload en `ESP_LOGI` : quatre lignes toutes les
  dix minutes (trois chunks horaires + un journalier), sans valeur en fonctionnement
  normal. Passées en `ESP_LOGD`, donc compilées hors binaire au niveau INFO.
- Les `logger.log "DEBUG: VOICE_ASSISTANT on_*"` et « WAKE WORD DETECTED » cités par
  l'audit **ne sortaient déjà pas** : un `logger.log` sans `level:` est un `ESP_LOGD`
  (documenté dans `tab5-hardware.yaml`), donc silencieux en INFO. Laissés tels quels,
  ils servent quand on repasse en DEBUG.
- Restent en INFO, à dessein : la décision du mot de réveil (événement rare, lot (c)),
  le refus d'ouverture d'écran pendant la sonnerie, le layout du rouleau d'horloge
  (une fois par boot).

### 2026-09-08 — Contrat HA : l'ancien service `tab5_maj_pluie_1h` est retiré

- Une barre par appel, neuf appels par rafraîchissement : remplacé le même jour par
  `tab5_maj_pluie_1h_bulk` (#114). Retiré une fois vérifié qu'aucun appelant ne
  restait — automation de prod et exemple public sur le bulk, mode démo sur le bulk,
  aucune occurrence dans les YAML du serveur HA (packages compris). Le contrat repasse
  à 16 services. Renommer ou supprimer un service reste un changement de contrat :
  README Tab5 (table des services), README HA, `screens.md`, inventaire, cartographie
  et ADR-0003 suivent.

### 2026-09-08 — HA : le push au reboot attend que la liaison API soit prête

- L'automation « MAJ Ecran Tab5 ESPHome Push » part sur l'événement
  `esphome.tab5_connected`, que le firmware émet dès qu'un client API se connecte —
  parfois avant que l'intégration HA ait fini d'authentifier sa connexion. Les premiers
  appels de la séquence partaient alors en « Authenticated connection not ready yet »
  à chaque reboot (704 occurrences dans les logs HA entre le 02 et le 08/09/2026), et
  l'écran attendait le rattrapage des dix minutes.
- Le trigger porte désormais l'id `tab5_connected` et, dans ce seul cas, la séquence
  commence par un `wait_template` sur `binary_sensor.*_ha_api_status` = `on`
  (10 s max, puis on continue quand même). `wait_template` plutôt que
  `wait_for_trigger` à dessein : il passe immédiatement si l'entité est déjà `on`.
- Exemple public et automation de prod (modifiée en direct via l'API config) alignés.

## [1.2.0] et versions antérieures

Archivées dans [`docs/changelog/CHANGELOG-1.x.md`](docs/changelog/CHANGELOG-1.x.md)
(1.0.0 du 06/07/2026 → 1.2.0 du 08/09/2026).
