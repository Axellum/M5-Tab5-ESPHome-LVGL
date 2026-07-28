# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Dates are the day each pull request was merged into `main`.

## [Unreleased]

### 2026-07-28 — Calendrier instantané + retour auto écran principal

- **Prefetch calendrier (stale-while-revalidate)** : à l'ouverture du popup, le
  mois déjà en cache s'affiche tout de suite (plus de `cal_cache_clear()` qui
  forçait un écran vide). TTL 10 min ; eviction des mois lointains (max 3 en
  RAM, ~4,5 Ko). Prefetch boot + reconnect HA (mois courant + M+1) et mois
  adjacents à chaque rendu de grille.
- **Retour automatique après inactivité** : popup ouvert → fermeture à **45 s**
  sans toucher ; page météo (J5–J9 / horaire) → retour panneau principal à
  **25 s**. Compteur LVGL d'inactivité (reset à chaque pression) ; les events
  vocaux appellent `ui_mark_activity()` pour ne pas fermer l'Assistant en
  pleine réponse. Jeux et panneau switches HA exclus volontairement.

### 2026-07-28 — Animations : plus courtes, et deux effets « rouleau »

Les transitions étaient jugées lentes à l'œil et donnaient une impression de
latence. L'écran est en `update_interval: never` : c'est LVGL qui redessine
depuis la loop ESPHome, donc **chaque frame d'animation repeint la zone
animée** (verre + dégradé compris). Durée plus courte = moins de frames.

- **Toutes les durées regroupées dans `UIAnim`** (`tab5_custom.h`) — plus de
  constantes éparpillées dans 5 fonctions. Rotateur central 450 → **190 ms**
  (glissement 84 → 28 px), ouverture popup 280 → **150 ms**, fermeture
  200 → **110 ms**, swipe prévisions 350 → **200 ms** (200 → 110 px), entrée
  d'alerte 300 → **180 ms** (100 → 44 px). Feedback tactile inchangé (80 ms).
- **Horloge à rouleau, un rouleau par chiffre** : `lbl_time` (un label
  « HH:MM ») devient **quatre** conteneurs qui rognent — un par chiffre — de
  2 labels chacun. Au changement, les deux labels du chiffre glissent d'une
  hauteur de boîte : le vieux sort par le haut, le neuf entre par le bas
  (240 ms). **Seul le chiffre qui change tourne** : de 22 à 23 mn la dizaine
  ne bouge pas. La géométrie (avance des chiffres, hauteur d'encre, centrage)
  est calculée au boot depuis les métriques réelles de la police
  (`get_capheight()` / `get_baseline()`), pas codée en dur : changer
  `roboto_130_b` ne casse rien. Suppose des chiffres tabulaires (Roboto :
  75 px d'avance pour les 10 glyphes en 130 gras).
- **Rouleau d'icône de prévision** : quand la condition météo d'une tuile
  change, la nouvelle icône monte de 22 px en apparaissant (190 ms), en
  décalé de 28 ms par tuile (effet vague). Ne se déclenche **que** si la
  condition change réellement (cache par tuile) — les push HA qui renvoient la
  même météo ne repeignent plus rien. Supprimé pendant un changement de calque
  horaire↔journalier : le calque glisse déjà.
- **Bascule prévisions ↔ switches HA** en fondu croisé (200 ms) au lieu d'un
  basculement sec de flags.

### 2026-07-28 — Go Tab opti : bugs 19×19, perf, polish

- **PASS = -1** (plus 255) : en 19×19 l'indice 255 est une intersection réelle ;
  un tap y déclenchait une passe. `Pos::ko` / `GoSave::ko` passent en `int16_t`
  (les ko > 255 n'étaient plus stockés correctement). Magic NVS **GOT3 → GOT4**.
- **Perf** : `chain_liberties` utilise des compteurs de génération (plus de
  `memset(N)` par chaîne) ; `render_board` est différentiel ; barre de réflexion
  sans invalidate LVGL si le % n'a pas bougé.
- **Jouabilité** : komi 0,5 en handicap ; marquage des morts conservé si pause
  pendant le comptage ; l'IA abandonne si écart d'aire ≥ 45 pts (Amateur+).
- Tests : collision PASS / ko haut-indice / parties aléatoires 9·13·19.

### 2026-07-27 — « Go Tab » v2 : le jeu de Go devient réellement jouable

Le premier jet du 26/07 compilait et se lançait, mais était **inutilisable** : le
Tab gelait ou redémarrait dès que l'IA réfléchissait, et le score final était
faux. Les trois modules sont réécrits.

**Corrections de fond**

- **L'IA ne rendait plus la main.** La recherche était bornée en *nombre de
  nœuds* : à 3 plis, un seul candidat racine coûtait plus que le budget (2 200
  nœuds), l'index de candidat n'avançait donc jamais et « le Tab réfléchit »
  ne se terminait pas. La recherche est désormais bornée par le **temps**
  (`Ai::step(ms)`, horloge testée tous les 32 nœuds) avec un budget CPU total par
  niveau — un coup valide est disponible dès `Ai::begin()`.
- **Débordement de pile.** `count_liberties` réservait 2,2 Ko de locales,
  `try_play` 1,2 Ko, et `negamax` prenait `Pos` **par valeur** : ~5 Ko par niveau
  de récursion, ~15 Ko à 3 plis, bien au-delà de la pile de la tâche principale.
  Tous les scratchs du moteur et de l'IA sont devenus des **statiques de module**
  (contexte LVGL mono-thread, jamais réentrant).
- **Coût par nœud divisé par ~100.** `collect_candidates` appelait `is_legal` —
  qui **simulait le coup entier**, copie de `Pos` comprise — sur les 361
  intersections, à chaque nœud. Désormais : `is_legal` exact **sans copie** (3
  branches : liberté directe / extension amie / capture), et une **table des
  chaînes construite une seule fois par position**, lue en O(1) par la cotation
  des candidats.
- **Score faux en fin de partie.** Aucun marquage des pierres mortes : tout
  groupe encore sur le goban comptait comme vivant. Ajout d'un **écran de
  marquage** (toucher un groupe le bascule mort/vivant, aperçu du territoire et
  score recalculés en direct) avant la validation du score.
- **Usure flash.** `esphome::global_preferences->sync()` était appelé **à chaque
  coup**. L'écriture est maintenant différée (fenêtre de 15 s) et forcée
  uniquement à l'ouverture d'un menu, en fin de partie et à la fermeture.
- **Taps traversants.** Le calque de menus n'était pas `CLICKABLE` : les appuis
  hors bouton passaient jusqu'au goban placé dessous.
- **`tab5-imu.yaml`** : Go Tab sort de la liste des jeux qui forcent le BMI270 à
  30 Hz — comme Roi Noir et Dames, il ne s'en sert que pour la secousse.

**Nouveautés**

- **Handicap 2 à 9 pierres** (placements standards ; Blanc commence).
- **Confirmation du coup en deux touchers** (fantôme + validation), activée par
  défaut : en 19×19 l'écart entre intersections tombe à 32 px.
- **Réglages** : confirmation, coordonnées, marqueur du dernier coup, aperçu du
  territoire, secousse = indice. **Statistiques** par taille et par niveau,
  parties jouées et temps de jeu cumulé.
- **Rendu retravaillé** : goban en dégradé bois avec liseré et lignes de bord
  épaissies, coordonnées `A..T` / `1..19`, pierres en relief par dégradé
  vertical (un seul objet LVGL chacune), pastille de trait dans le HUD, liste des
  coups en police mono à deux colonnes, barre de réflexion, carte de fin de
  partie détaillée par-dessus le goban resté visible.
- **Tests** : `tools/test_go_engine.py` passe de 5 à 12 cas (capture de groupe,
  capture prioritaire sur le suicide, œils de centre / bord / coin, handicap,
  territoire, score avec pierres mortes, plus 20 parties aléatoires vérifiant
  qu'aucune chaîne sans liberté ne subsiste). `tools/test_go_engine.cpp` aligné.
- **`GO_SAVE_MAGIC` → `GOT3`** : les anciennes sauvegardes sont rejetées et les
  réglages repartent des valeurs d'usine (changement de layout `GoSave`).

### 2026-07-27 — Arcade : 7 consoles supplémentaires + sélecteur 4×2 + IMU adaptative
- **Sélecteur Arcade** (`ui_components/game_selector.yaml`) : grille régulière 4×2 (8 cartes 298×252), ouverte par tap sur la température serre (`btn_serre_games` dans `climate_card.yaml`). Chaque carte ferme le jeu en cours, referme le sélecteur, puis ouvre la console cible.
- **Arcanoïde** (`arkanoid_game.h/.cpp`) : casse-briques rétro Atari 8 niveaux, 3 vies, power-ups (élargir/rétrécir raquette, balle lente/rapide, multi-balles, colle, extra vie), combo multiplicateur, top 10 NVS (`ArkanoidSave`, magic `ARK1`). Contrôles : inclinaison BMI270 + boutons tactiles (mode « Les deux » par défaut).
- **Flip Noir** (`pinball_game.h/.cpp`) : flipper style arcade 70-80's, 3 billes + bonus, 4 bumpers, 2 slingshots, 3 cibles drop, multiball, modes score (Bumper Frenzy, Target Mania), TILT anti-abus nudge, top 10 NVS (`PinballSave`, magic `PIN1`). Physique ~45 Hz.
- **Coureur d'Or** (`lode_game.h/.cpp`) : Lode Runner 1983, 10 niveaux, creuser gauche/droite, grimper échelles, collecter l'or, fuir les gardes, 4 vies, record NVS (`LodeSave`, magic `LOD1`). D-pad tactile + boutons CREUSER.
- **Go Tab** (`go_engine.h/.cpp` + `go_ai.h/.cpp` + `go_game.h/.cpp`) : jeu de Go 9×9/13×13/19×19, règles complètes (capture, suicide interdit, ko simple, score chinois + komi 6,5), 3 modes (vs Tab / vs joueur / Tab vs Tab), IA time-sliced (Débutant → Expert). NVS `GoSave`.
- **Trial Poursuite** (`trivia_game.h` + `trivia_questions.h` + `trivia_game.cpp`) : quiz rétro-salon 1 à 6 équipes, banque de questions embarquée en flash (PROGMEM), catégories variées, score par équipe, NVS `TriviaSave`.
- **Dames Tab** (`draughts_ai.h/.cpp` + `draughts_game.h/.cpp`) : dames internationales 10×10, règles complètes (prises majoritaires, rafles, dames), IA embarquée time-sliced, 3 modes, NVS `DraughtsSave`.
- **Roi Noir** (`chess_ai.h/.cpp` + `chess_game.h/.cpp`) : échecs FIDE complets (roque, en passant, promotion, 50 coups, triple répétition, matériel insuffisant), IA négamax αβ + quiescence + killers, 5 niveaux (Pion → Roi, Elo fictif 600–1900), police dédiée `ChessPieces.ttf` (12 glyphes, rendu 2 calques), perft validé, NVS `ChessSave`. Empreinte : ~62 Ko `.text` + 46,5 Ko `.bss`.
- **`tab5-imu.yaml`** (nouveau package) : BMI270 via plateforme `motion:` native ESPHome. Axes accélération `internal: true` (pas de publication HA). Cadence de poll adaptative : 100 ms au repos, 33 ms quand un jeu à inclinaison est ouvert (`stop_poller()`/`start_poller()`). Tap-to-wake (> 2,5 g, debounce 500 ms). Pitch/roll/temp throttlés 60 s pour diagnostic HA.
- **`tab5-ui-tokens.yaml`** (nouveau package) : tokens dimensionnels partagés (`modal_card_w/h`, `modal_body_y`) pour le cadre modal v4.
- **ESPHome `min_version: 2026.7.0`** : requis pour st7123 officiel (plus de `external_components`), audio zero-copy, VAD, PSRAM SDIO (`use_psram: true` sur `esp32_hosted`), composant `motion:` natif.
- **CPU 360 MHz** : `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360` activé — marge supplémentaire pour la physique des jeux.
- Architecture commune des 8 consoles : overlay plein écran 1280×720 (exception ADR-0009), YAML = conteneurs vides, tout en C++, `lv_timer` créé/détruit, pool LVGL préalloué, zéro allocation dans le tick, zéro dépendance HA/réseau.
- **Statut : prototypes expérimentaux** — premier jet IA pour tester les capacités de génération de code sur hardware embarqué. Fonctionnels mais non finalisés visuellement.

### 2026-07-26 — « Fil d'Or » : roguelite de bille plein écran piloté au BMI270
- **Nouveau module isolé** `Tab5/marble_game.h` + `Tab5/marble_game.cpp` (namespace `Marble`) + overlay `ui_components/marble_game.yaml`. Remplace le prototype précédent (namespace `Game`, 1 niveau, popup 800×500) qui est **supprimé** avec `game_popup.yaml`.
- **Boucle roguelite complète** : hub → **6 salles** distinctes (Seuil / Couloirs / Forge / Sanctuaire / Némésis / Trône) → mort ou victoire → retour hub, pour des runs de 2 à 5 min. La salle 6 demande **3 runes** avant d'ouvrir un portail central gardé par deux orbes en orbite.
- **Contenu** : 6 familles de pièges (pointes, scie oscillante, trou, glu, tapis d'accélération, orbe) plus une **chasseuse** qui poursuit la bille ; 6 bonus ramassables ; **10 boons** intra-run dont 3 proposés au choix après les salles 2 et 4 ; **5 améliorations méta** achetées en fragments.
- **Persistance NVS** (`MarbleSave` via `esphome::global_preferences`, magic `FOR1`) : fragments, runs, victoires, meilleur temps, salle la plus profonde, améliorations, teinte de bille et offset de calibration. **Aucune dépendance Home Assistant** — le jeu fonctionne HA hors ligne.
- **Plein écran assumé** : l'overlay occupe les 1280×720 (HUD compact de 48 px + terrain de 1280×672). C'est l'**unique exception documentée** au chrome modal v4 (ADR-0009) — un flux de jeu n'est pas un popup domotique. Le garde-fou `check_tab5_modal_chrome.py` reste vert, et la règle 7 de `Tab5/README.md` documente l'exception.
- **Perf** : physique 30 Hz sur un `lv_timer` créé à l'ouverture et **détruit à la fermeture** (zéro tick hors jeu) ; **3 sous-pas** de collision par frame contre le tunnelling à 650 px/s ; pool de **48 entités LVGL préallouées** recyclées par `show`/`hide` + `set_pos` (aucune allocation dans la boucle) ; libellés HUD réécrits uniquement au changement de valeur.
- **`tab5-imu.yaml` retravaillé** : les 3 axes d'accélération passent en `internal: true` (ils publiaient ~50 msg/s vers l'API HA pour rien) ; pitch/roll/température restent exposés mais throttlés à 60 s. La **cadence de poll devient adaptative** — 100 ms au repos, 33 ms quand le jeu est ouvert — via `stop_poller()`/`start_poller()`, car `set_update_interval()` seul ne re-règle pas un `PollingComponent` dont l'interval est figé dans `call_setup()`.
- Entrées : console **GESTION** → « Fil d'Or », ou long-press sur la bande centrale. Sortie par le hub ; pause en touchant le bandeau HUD.
- **3 difficultés + mode dieu** (hub → Réglages, persistés, appliqués au lancement de la run) : **Calme** (+1 PV, pièges ×0,75, invulnérabilité 1800 ms, fragments ×0,80), **Normal**, **Impitoyable** (−1 PV, pièges ×1,35, invulnérabilité 800 ms, **fragments ×1,60** — monter d'un cran doit rapporter). La difficulté ne touche **que le timing**, jamais la géométrie : les passages restent identiques. Le **mode dieu** rend invulnérable mais met la run **hors concours** (aucun fragment crédité, aucune stat enregistrée), signalé en clair au HUD.
- **Nouveau garde-fou `scripts/check_marble_rooms.py`** : BFS sur grille d'occupation du centre de la bille (murs dilatés du rayon) qui prouve, pour les 6 salles, que le départ est libre, que la sortie et **chaque bonus/rune** sont atteignables, et que les scies laissent un passage. Dilatation conservatrice ⇒ un chemin trouvé existe forcément en jeu. Testé négativement.
- Côté C++, le décalage des bonus par le seed est contraint par `segment_clear()` : la position tirée doit être reliée en ligne droite à l'originale (bille dilatée comprise). Le simple test « pas dans un mur » laissait un bonus sauter derrière une paroi fine.
- **Progression façon Dark Souls** : les fragments deviennent des **âmes**, monnaie unique servant à la fois à monter de niveau et à commercer. Les 5 anciennes « améliorations » sont remplacées par **6 caractéristiques** au *Feu de camp* — Vitalité, Résistance, Finesse, Agilité, Élan, Découverte — dont le coût suit le **niveau total** (`60 + 14·L + L²`), si bien que monter une caractéristique renchérit toutes les autres et impose de choisir une orientation.
- **Objets et commerce** : 10 objets à effets passifs, découverts dans les **coffres au trésor** (nouvelle entité `K_CHEST`, 6 placés dans les salles 2 à 6), lâchés par les **boss** (Némésis et le Trône, butin garanti), ou achetés chez le **Marchand** (2 pages). **Revente à moitié prix**, avec déséquipement automatique. **2 emplacements d'équipement** cumulables, un même objet ne pouvant occuper les deux. La *Couronne fêlée* assume un vrai compromis (+50 % d'âmes, −1 PV).
- **Finesse ne réduit que le rayon**, jamais l'inverse : la preuve de traversabilité étant établie au rayon maximal (11 px), elle reste valable quelle que soit la progression du joueur. Les coffres sont eux aussi couverts par le garde-fou.
- Le hub passe à 7 entrées et le pool de slots de menu de 6 à 8 ; la teinte de bille rejoint les Réglages. `MarbleSave` étendu → magic `FOR2` → **`FOR3`**.

### 2026-07-26 — Refactoring structurel : CentralPanelCtx + factorisation (audit R2-R5)
- **`CentralPanelCtx`** (`tab5_custom.h`) : struct regroupant les 8 wrappers LVGL de la carte centrale + 7 flags d'activité + `current_panel`. Les signatures C++ passent de 16 paramètres à 1-3 (référence ctx). Pattern *sync → call → write-back* : les globals ESPHome restent source de vérité, le ctx est synchronisé avant chaque appel.
- **Globals C++** : `g_central_ctx`, `g_day_slots[5]`, `g_hour_slots[5]` initialisés une fois au boot (`on_boot` dans `tab5-ha-hmi.yaml`) — supprime la reconstruction des tableaux de slots à chaque swipe/appel service.
- **`highlight_button_border()`** : factorise la lambda `hl` dupliquée (surbrillance bordure boutons mode Domo/Discu) en une fonction C++ réutilisable.
- **Boutons mode centralisés** : les 3 sites de mise à jour bordure (on_boot, btn Domo, btn Discu) passent par `script.execute: tab5_set_assist_mode` (34 lignes YAML → 6).
- **Code mort supprimé** : blocs commentés M1/M2, `#include <vector>` dupliqué, 8 variables `static` file-scope remplacées par le ctx.
- Bilan : **8 fichiers, +298 / −432 lignes** (net −134). Comportement visuel inchangé.

### 2026-07-25 — Cadre modal v4 : une seule barre de titre, compacte, pour les 9 popups
- **Un seul `!include` produit tout le chrome d'en-tête** : nouveau `ui_components/modal_header.yaml` (conteneur pleine largeur de 52 px : icône + titre à gauche, croix 80×44 à droite). Il remplace et supprime `modal_header_brand.yaml` + `modal_close_btn.yaml` du cadre v3. L'alignement vertical est fait par LVGL (`LEFT_MID`/`RIGHT_MID`) : plus aucun `y:` recopié popup par popup, donc plus de dérive possible entre deux fenêtres.
- **Barre compacte** : 4 px au-dessus de la ligne, ligne de 44, titre `roboto_32_b`, icône et croix en `mdi_font_32` → le corps démarre à `y: 52` au lieu de 80, soit **28 px de hauteur utile récupérés** par popup. Les sous-titres de l'assistant et de la télécommande TV sont supprimés : une seule variante de barre, sans exception.
- **Une seule taille de fenêtre** : `1250×690` pour tous, via le nouveau package de tokens `Tab5/tab5-ui-tokens.yaml` (`modal_card_w/h`, `modal_body_y`). L'assistant et la TV passent de 1230×670, la console de 1180×680 — corps recalés en conséquence (grille calendrier 82+r*95 avec des cellules de 91 px, grille console 2×2 recentrée en x 63/637 avec des cartes de 300 px, colonnes assistant/TV étirées).
- **Un seul voile** : `modal_scrim.yaml` prend une var `scrim_opa` (85 % au premier niveau, 60 % pour le détail du jour qui s'empile). Les trois voiles inline (assistant, console, détail du jour) et le style `style_modal_overlay` (72 %) disparaissent.
- **Garde-fou** : `scripts/check_tab5_modal_chrome.py` (dépôt racine) signale tout popup qui réintroduit un voile/une croix inline, une taille de carte en dur ou une carte sans barre partagée.
- `tab5-styles.yaml` : `mdi_font_32` complétée de `F024A`/`F0E17`/`F0141`/`F0142` (les icônes d'en-tête et les chevrons de navigation passent en 32) ; `mdi_font_45` n'est plus utilisée par aucun chrome. Détail et alternatives rejetées : **ADR-0009**.

### 2026-07-22 — Popup Assistant vocal : demande + réponse écrite (tableaux & image)
- Nouveau `ui_components/assistant_popup.yaml` : carte modale plein écran 1230×670 (25 px des bords, recette popups v2) — **gauche = réglages** (choix du cerveau/pipeline Domotique↔Discussion, Ok Nabu ON/OFF, Muet, slider Volume, taille de texte A-/A/A+), **droite = « Votre demande »** (transcription STT) + **« Réponse »** défilante avec prise en charge Markdown : **tableaux** ré-alignés en monospace et **image** téléchargée à la demande.
- Ouverture : **appui long sur la zone micro** (`btn_assist_trigger`), automatiquement en **mode Discussion** sur une demande vocale (`on_stt_end` → `tab5_assist_on_request` ; en mode Domotique le bandeau central 8 s reste le retour rapide, pas de voile du dashboard), ou par le moteur via le service HA `tab5_assist_reponse`. Fermeture par scrim / croix / bouton « Fermer ».
- **Contrat HA** : nouveau service `esphome.<device>_tab5_assist_reponse` (variables `texte` = Markdown, `image_url` = PNG optionnel, vide ⇒ zone image masquée). Le moteur pousse ainsi une réponse riche ; à défaut, le texte parlé (TTS `on_tts_start`) alimente la réponse et la demande vient de `on_stt_end`.
- **Rendu** (`format_assist_markdown()`, `tab5_custom.cpp`) : nettoyage `**gras**`/`` `code` ``/titres `#`, puces `-`→`•`, et surtout **ré-alignement des colonnes de tableau** avec comptage en points de code UTF-8 (les accents restent alignés) — testé hors-cible. Réglage de taille S/M/L persisté (`assist_text_size`) sans perdre le texte affiché.
- **Image** : composants `http_request` + `online_image` (PNG→RGB565, redimensionné 760×360, décodage PSRAM), URL fixée au runtime via `online_image.set_url` ; libellés « Chargement… » / « Image indisponible » selon `on_download_finished`/`on_error`.
- **Anti « boutons qui décalent »** : positions absolues (x/y fixes) et les états actifs ne changent que la bordure (dessinée à l'intérieur du widget) — aucun décalage. Sélecteur de cerveau centralisé (`tab5_set_assist_mode`) synchronisant à la fois les boutons Domo/Discu du dashboard et ceux du popup + libellé d'état Écoute/Analyse/Réponse/Prêt/Erreur.
- `tab5-styles.yaml` : polices `roboto_mono_20/24/28` (tableaux alignés) et `mdi_assist_36/64` (icônes du popup, codepoints vérifiés dans le TTF). `tab5-globals.yaml` : `assist_popup_open`, `assist_text_size`.

### 2026-07-19 — Popup calendrier : appui long sur l'horloge
- Nouveaux `calendar_popup.yaml` + template `cal_day_cell.yaml` (42 instances, grille 7×6 lundi-en-tête) : carte modale 1250×690 (15 px des bords) — en-tête (navigation ◀ mois ▶ + bouton « Aujourd'hui »), grille mensuelle avec numéro du jour, **heures de travail affichées dans les cases**, pastilles RDV (dorée) / anniversaire (rose), fond violet doux = vacances scolaires, numéro rose = férié, bordure cyan = aujourd'hui, légende en bas.
- Ouverture par **appui long sur l'horloge/date** (`btn_clock_calendar_zone`, zone tactile invisible sur `clock_tile`) ; tap court sans effet.
- **Grille calculée en local** (`cal_render_month()`, algorithme de Sakamoto + date SNTP) : numéros, alignement lundi-dimanche, weekend, aujourd'hui et jours passés s'affichent même sans HA. HA enrichit ensuite chaque mois **à la demande** (`script.tab5_calendrier_mois` → service `tab5_maj_calendrier_mois`, codes 2 hex/jour + 31 champs d'heures), avec cache par mois vidé à l'ouverture.
- **Tap sur un jour** → sous-popup détail 780×540 (`cal_day_popup`) : titre « Mardi 21 Juillet », lignes typées férié / vacances scolaires (Zone A) / horaires de travail / RDV / anniversaire / fête civile avec icônes MDI colorées (`script.tab5_calendrier_jour` → `tab5_maj_calendrier_jour`), états « Chargement... » / « Rien de prévu ce jour » / « Home Assistant hors ligne ».
- Nouveau package HA `HomeAssistant_Config/packages/tab5_calendar.yaml` : sources = calendrier boulot (événements « Travail* »), jours fériés Google (liste blanche des vrais fériés — les fêtes civiles type Fête des Mères deviennent des lignes « fête »), calendriers famille + anniversaires, et **table statique des vacances scolaires Zone A** (académie de Bordeaux) vérifiée sur data.education.gouv.fr jusqu'à l'été 2027.
- Anti « croix qui décale » : `scrollable: false` partout + croix = vrai bouton de verre 96×64 (recette popups v2).
- `tab5-styles.yaml` : token `color_warm_pink` (miroir `UIColor::WARM_PINK`) ; glyphes `F0E17`/`F0141`/`F0142` → `mdi_font_45`, `F00D6`/`F1056`/`F0474`/`F00F0`/`F00EB`/`F09D3` → `mdi_font_32` (codepoints vérifiés dans le TTF).

### 2026-07-18 — Popup détails plantes : appui long sur les pots
- Nouveaux `pots_popup.yaml` + template `pot_detail_card.yaml` (5 instances, #T164) : carte modale 1250×690 (15 px des bords), 5 cartes de verre **fixes** (carte N = capteur `moisture_N`, mêmes icônes que le dashboard) — nom, icône colorée par l'humidité, % humidité `roboto_45_b`, statut (OK / Bientôt sec / À arroser ! / Hors ligne) et 4 métriques : Fertilité (EC µS/cm), Lumière (lx), Température (°C, gradient `get_temperature_color`), Batterie (échelle `get_battery_color`).
- Ouverture par **appui long** sur les 4 slots pots du dashboard (`btn_pots_detail_zone`, zone tactile invisible même géométrie que `moisture_sensors_card`) ; le tap court reste sans effet, aucune synchro à l'ouverture (valeurs poussées en continu).
- 20 nouveaux capteurs `platform: homeassistant` (`pot*_ec/lux/temp/bat`, substitutions `entity_plante_*_ec/lux/temp/bat` dans `user_entities(.example).yaml`) — présentation entièrement déléguée à `update_pot_metric_ui()` / `update_pots_popup_moisture_ui()` (`tab5_custom.cpp`), zéro LVGL dans le YAML capteurs.
- `get_battery_color()` factorisé : l'échelle couleur de l'icône téléphone du bandeau (lambda inline) est réutilisée pour la batterie des capteurs plantes.
- Anti « croix qui décale » : `scrollable: false` partout + croix = vrai bouton de verre 96×64 (recette popups v2).
- `tab5-styles.yaml` : glyphes `F0241` (flash) et `F0079` (batterie) ajoutés à `mdi_font_32`.

## [1.0.5] — 2026-07-17

Jalon firmware taillé pour la soumission au M5Stack Global Innovation Contest 2026 — tout ce qui s'est accumulé depuis `v1.0.0` : popups de verre quasi plein écran (clim v2, lumière v2, télécommande TV Samsung), Console Système v2 avec carte de gestion HA, bandeaux alertes/infos HA avec tap-to-dismiss sur le rotateur central, second wake word local « Stop » pour le volet, interruption vocale au tap micro, mode démo autonome sans Home Assistant, et la couche présentation GitHub/Hackster (vidéo, GIF, docs).

### 2026-07-16 — Popup clim v2 : plein écran, modes empilés, cible optimiste, Brise
- `climate_popup.yaml` refondu (1130×650 → carte 1250×690, 15 px des bords) : 3 cartes de verre — MODE (Froid/Chaud/Sec/Ventilation/Éteint empilés, pile flex), TEMPÉRATURE (arc 320 px, cible `roboto_55_b` au centre, boutons ± 160 px, ligne « Pièce » avec thermomètre), OPTIONS (Presets Éco/Boost, Ventilation Silence, Flux d'air Oscillation + **Brise**).
- **Nouveau bouton « Brise »** (`windnice`, 3ᵉ mode réel du Daikin Onecta, jusque-là inaccessible depuis l'écran) : toggle windnice↔stop, icône `popup_icon_clim_windnice` pilotée par `tab5_maj_clim` (windnice sorti de la condition Oscillation).
- **Cible optimiste + débounce** : l'arc et les taps ± mettent à jour label/arc immédiatement (`update_clim_target_ui`, `tab5_custom.cpp`) et un seul `climate.set_temperature` part après 250 ms (`tab5_debounce_clim_temp`) — les taps rapides ± sont groupés, plus d'appel HA par tick d'arc ; le forçage `hvac_mode: cool` si éteinte est conservé.
- Anti « croix qui décale » : `scrollable: false` partout, croix = vrai bouton de verre 96×64.
- Templates restylés : `climate_hvac_mode_btn.yaml` (342×88, labels `roboto_32_b`), `climate_preset_toggle_btn.yaml` (164×88).
- Glyphes : `F059D` (weather-windy) → `mdi_font_45`, `F050F` (thermomètre) → `mdi_font_32`.
- Contrat `tab5_maj_clim` inchangé côté HA (mêmes variables) ; ids `arc_temp_popup`/`clim_target_popup`/`val_temp_int_popup`/`popup_icon_clim_*` conservés.

### 2026-07-16 — Présentation GitHub + Hackster.io (vidéo, GIF, galerie)
- `README.md` (EN+FR) : section « See it in action » avec embed YouTube (`ygNhgtMffu4`), GIF Gemini `docs/images/m5stack_tab5_demo.gif`, schéma push-only, galerie photos corrigée (libellés switches/console/météo).
- Nouveau `docs/hackster.md` : brouillon complet pour Hackster.io / M5Stack Global Innovation Contest 2026 (story EN+FR, BOM, build, critères jury).

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

### 2026-07-16 — Bandeau central roboto_45_b, clim Daikin réelle, télécommande TV, alertes HA ([#43](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/43))
- Nouveau popup **télécommande TV Samsung** (`tv_remote_popup.yaml`) : power, pad de navigation, volume, chaînes, rangée lecture — chaque touche envoie `remote.send_command`/`remote.toggle` à `${entity_tv_remote}` ; ouvert par le bouton TV ou un long-press sur la carte PC.
- **Alertes / infos HA sur le rotateur central** : nouveau service `tab5_maj_alertes_ha_bulk` (jusqu'à 4 bandeaux `ha_alert_wrapper_0…3`, un panneau du rotateur chacun) + **tap-to-dismiss** local (l'id masqué est mémorisé, un re-push du même id reste caché) ; package HA `tab5_alerts.yaml`.
- Nouveau service `tab5_maj_reponse_vocale` : la réponse vocale s'affiche temporairement dans le bandeau central (`tab5_show_vocal_response`).
- Textes du bandeau central harmonisés en `roboto_45_b` ; boutons HA/Sys/TV et onglets météo passés au verre `style_clim_btn`.
- Clim mappée sur le Daikin Onecta réel : swing stop/swing, Éco=away, Silence=quiet, ± force le `hvac_mode` si la clim est éteinte ; glyphes MDI flèches/volume ajoutés à `mdi_font_45`.

### 2026-07-15 — Interruption vocale au tap micro ([#42](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/42))
- Un tap sur l'icône micro pendant que l'assistant parle interrompt la réponse (`assist_satellite.stop` côté HA + arrêt pipeline) et relance immédiatement l'écoute (`tab5_vocal_interrupt_and_listen`) — seule interruption fiable pendant le TTS, le wake word étant inactif en phase de réponse du pipeline Assist.
- Fix de l'icône micro « verte zombie » après interruption.

### 2026-07-15 — Wake word local « Stop » + suivi mouvement volet ([#41](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/41))
- Second modèle microWakeWord `Stop` : armé (`micro_wake_word.enable_model`) uniquement quand le volet est en mouvement (`volet_en_mouvement`), désarmé à l'arrêt — dire « Stop » arrête le volet directement sur l'appareil, sans « Okay Nabu » ni aller-retour pipeline. Pièges corrigés : ESPHome n'active que le premier modèle déclaré, et la valeur détectée est `"Stop"` (S majuscule).
- Suivi du mouvement du volet poussé par HA (package `volet_serre_tracking.yaml`) : l'état/icône du volet à l'écran suit aussi les commandes lancées hors écran (vocal, automations sunrise/sunset).

### 2026-07-15 — Mode démo autonome sans Home Assistant ([#40](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/40))
- `tools/demo/demo_pusher.py` + `docs/demo_mode.md` : un script Python pousse des données synthétiques (météo, planning, clim, plantes…) vers un Tab5 flashé, via l'API native ESPHome — l'interface complète se teste en quelques minutes sans aucune instance HA.

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
