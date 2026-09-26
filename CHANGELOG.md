# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Dates are the day each pull request was merged into `main`.

## [Unreleased]

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
- **Pas encore un check requis** : pour qu'un échec bloque le merge, il faut l'ajouter
  aux checks requis de `main` dans les réglages du dépôt.

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
