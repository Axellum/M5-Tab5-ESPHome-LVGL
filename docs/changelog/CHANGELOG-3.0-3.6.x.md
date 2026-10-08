# Changelog — versions 3.0.0 à 3.6.0 (archive)

Archivé le 08/10/2026, après la 3.7.0 : ces sections faisaient près de la moitié du
[`CHANGELOG.md`](../../CHANGELOG.md), qui garde `[Unreleased]` et la 3.7.x. Les dates sont
celles du merge dans `main`. Le texte des sections est **recopié à l'octet près** : les
liens relatifs (`docs/…`) y restent écrits depuis la racine du dépôt, comme dans le
`CHANGELOG.md` de chaque tag `v3.x.y`, où ils fonctionnent.

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

