# Patch à appliquer — jeu « Coureur d'Or » (clone Lode Runner)

> **Statut : PRÉPARÉ, NON APPLIQUÉ.**
> Une seconde IA travaille en parallèle sur « Arcanoïde » **et** « Flip Noir »
> (`arkanoid_game.*`, `pinball_game.*`). Tous les fichiers **partagés** du Tab5
> sont donc laissés intacts par moi : ce document contient les hunks exacts à
> appliquer, avec les points de fusion signalés.
>
> **Les hunks ci-dessous sont calés sur l'état du dépôt au moment de la
> rédaction**, branche `feat/animations-lvgl-bmi270`, c'est-à-dire *après* les
> ajouts Arcanoïde + Flip Noir déjà présents dans le working tree. Si l'autre IA
> continue d'éditer, revérifier le contexte avant d'appliquer.
>
> Les fichiers **nouveaux** (aucun risque de conflit) sont déjà écrits :
>
> | Fichier | Rôle |
> |---|---|
> | `Tab5/lode_game.h` | API `namespace Lode`, `LodeSave` (NVS), palette `Lode::Pal` |
> | `Tab5/lode_game.cpp` | ~1900 lignes : grille, physique, IA gardes, 10 maps, UI, NVS |
> | `Tab5/ui_components/lode_game.yaml` | 5 conteneurs vides (root / hud / field / pad / panel) |
> | `scripts/check_lode_levels.py` | garde-fou : rejoue les règles de déplacement sur les 10 maps |

---

## A. `00ProjetTab/tab5-ha-hmi.yaml` — déclarer les sources

Bloc `esphome: includes:` (lignes 16-21 actuellement).

```diff
     # Jeu « Arcanoïde » (casse-briques rétro) — sous-module isolé du HMI.
     - Tab5/arkanoid_game.h
     - Tab5/arkanoid_game.cpp
     # Jeu « Flip Noir » (flipper rétro arcade) — sous-module isolé du HMI.
     - Tab5/pinball_game.h
     - Tab5/pinball_game.cpp
+    # Jeu « Coureur d'Or » (clone Lode Runner) — sous-module isolé du HMI.
+    - Tab5/lode_game.h
+    - Tab5/lode_game.cpp
```

> Les quatre jeux cohabitent sans interaction, l'ordre n'a pas d'importance.

---

## B. `00ProjetTab/Tab5/tab5-lvgl.yaml` — inclure l'overlay

Dernière ligne du fichier.

```diff
         - !include ui_components/marble_game.yaml
         - !include ui_components/arkanoid_game.yaml
         - !include ui_components/pinball_game.yaml
+        - !include ui_components/lode_game.yaml
```

> L'overlay est déclaré **après** les popups : il se dessine donc au-dessus, et
> `Lode::open()` fait de toute façon un `lv_obj_move_foreground(root)`.

---

## C. `00ProjetTab/Tab5/tab5-scripts.yaml` — scripts d'ouverture / fermeture

À insérer juste après le script `tab5_marble_open` (après la ligne 26).

```yaml
  # Ouvre le jeu « Coureur d'Or » (clone Lode Runner, overlay plein écran).
  # Même principe que tab5_marble_open : `id(...)` n'étant utilisable que dans une
  # lambda, les pointeurs LVGL, les polices et l'horodatage SNTP sont injectés ici
  # plutôt que dans le C++. Le jeu reste 100 % jouable sans réseau : une horloge
  # non synchronisée donne simplement des scores non datés.
  - id: tab5_lode_open
    mode: single
    then:
      - lambda: |-
          Lode::UI ui;
          ui.root    = id(lode_root);
          ui.field   = id(lode_field);
          ui.hud     = id(lode_hud);
          ui.panel   = id(lode_panel);
          ui.pad     = id(lode_pad);
          ui.f_small = id(roboto_22);
          ui.f_mid   = id(roboto_32_b);
          ui.f_big   = id(roboto_45_b);
          auto t = id(sntp_time).now();
          ui.epoch = t.is_valid() ? (uint32_t) t.timestamp : 0;
          Lode::open(ui);

  # Fermeture forcée (retour dashboard) — le hub « Quitter » appelle déjà
  # Lode::close() directement ; ce script sert aux automatisations éventuelles.
  - id: tab5_lode_close
    mode: single
    then:
      - lambda: 'Lode::close();'
```

---

## D. `00ProjetTab/Tab5/tab5-imu.yaml` — brancher l'IMU **sans casser Marble**

### D.1 — alimentation du filtre d'inclinaison (bloc `on_value` de `imu_accel_z`)

```diff
             // 1) Alimentation du jeu (simple stockage, aucun calcul lourd ici).
             Marble::on_imu(ax, ay, az);
             Arkanoid::on_imu(ax, ay, az);
             Pinball::on_imu(ax, ay, az);
+            Lode::on_imu(ax, ay, az);
```

### D.2 — cadence de poll adaptative (bloc `interval:` en bas de fichier)

```diff
           static bool fast = false;
-          bool want = Marble::is_open() || Arkanoid::is_open() || Pinball::is_open();
+          // Un seul poller pour tous les jeux : cadence rapide dès que l'un
+          // d'eux est ouvert (ils sont mutuellement exclusifs à l'écran).
+          bool want = Marble::is_open() || Arkanoid::is_open()
+                   || Pinball::is_open() || Lode::is_open();
           if (want == fast) return;
```

### D.3 — entête `[AI-CONTEXT]` (facultatif mais recommandé)

```diff
- # @role BMI270 (IMU embarquée) — accel/gyro/temp, tap-to-wake, et source
- #     d'inclinaison du jeu « Fil d'Or » (marble_game.cpp).
+ # @role BMI270 (IMU embarquée) — accel/gyro/temp, tap-to-wake, et source
+ #     d'inclinaison des jeux plein écran (marble_game.cpp, lode_game.cpp).
```

> **⚠ POINT DE FUSION PRINCIPAL** — les 4 jeux se partagent ces deux lignes.
> Aucun ne consomme de CPU quand il est fermé : `on_imu()` ne fait que stocker
> deux flottants, et le `lv_timer` de gameplay n'existe qu'entre `open()` et
> `close()`. Quatre appels à `on_imu()` à 30 Hz = 4 stockages de floats, coût nul.

---

## E. Point d'entrée depuis le dashboard — 1 ligne, zéro conflit

« Fil d'Or » a **trois** points d'entrée dont deux strictement redondants :
long-press sur le bandeau **planning** *et* sur le bandeau **pluie** lancent le
même jeu. On en réaffecte un seul, ce qui n'enlève aucun accès à Marble (le
bouton de console GESTION reste).

`Tab5/tab5-lvgl.yaml`, bouton `btn_rain_tap` (~ligne 389) :

```diff
                         on_short_click:
                           - script.execute: tab5_central_panel_next
                         on_long_press:
-                          - script.execute: tab5_marble_open
+                          - script.execute: tab5_lode_open
```

Carte des lanceurs après application :

| Geste / bouton | Jeu |
|---|---|
| Long-press bandeau **planning** | Fil d'Or |
| Long-press bandeau **pluie** | **Coureur d'Or** |
| Long-press zone de **pagination** | Arcanoïde |
| Console GESTION → « Fil d'Or » | Fil d'Or |
| Console GESTION → « Flip Noir » | Flip Noir |

> **Pourquoi pas un bouton de console ?** La carte qui héberge les lanceurs fait
> **550×300** (`console_sys.yaml`) et les deux boutons occupent déjà `y: 246` et
> `y: 296`. Un troisième à `y: 346` sortirait franchement de la carte.
>
> ⚠️ **Défaut constaté (hors de mon périmètre, à signaler à l'autre IA)** : le
> bouton `btn_open_pinball` ajouté à `y: 296` avec `height: 44` finit à **y=340
> dans une carte de 300 px** — il déborde déjà de 40 px. À corriger côté
> Arcanoïde/Flip Noir, par exemple en passant les lanceurs sur une rangée de
> boutons de 164 px à `y: 246`.

---

## F. `00ProjetTab/Tab5/README.md`

### F.1 — compléter l'exception ADR-0009 (règle 7)

La ligne a déjà été étendue par l'autre IA (Arcanoïde + Flip Noir). Il ne reste
qu'à y ajouter Lode — et à corriger « Unique exception », qui n'est plus exact :

```diff
-   **Unique exception** : `ui_components/marble_game.yaml` (jeu « Fil d'Or »), `ui_components/arkanoid_game.yaml` (jeu « Arcanoïde ») et `ui_components/pinball_game.yaml` (jeu « Flip Noir »). Ce ne sont pas des popups domotiques mais des **flux plein écran** séparés — voir les sections dédiées ci-dessous. Ils ne déclenchent pas le garde-fou (ni `style_modal_card`, ni `color_modal_scrim`, ni glyphe de croix).
+   **Exceptions (les overlays de jeu)** : `ui_components/marble_game.yaml` (« Fil d'Or »), `ui_components/arkanoid_game.yaml` (« Arcanoïde »), `ui_components/pinball_game.yaml` (« Flip Noir ») et `ui_components/lode_game.yaml` (« Coureur d'Or »). Ce ne sont pas des popups domotiques mais des **flux plein écran** séparés — voir les sections dédiées ci-dessous. Ils ne déclenchent pas le garde-fou (ni `style_modal_card`, ni `color_modal_scrim`, ni glyphe de croix).
```

### F.2 — nouvelle section, à insérer après la section « Marble Roguelite »

```markdown
---

## Coureur d'Or — clone de Lode Runner

Clone de Lode Runner (Broderbund, 1983), **plein écran 1280×720**, entièrement
local : ni Home Assistant, ni réseau, ni pour jouer, ni pour sauvegarder.

### Lancer / quitter

| Action | Où |
|---|---|
| Ouvrir | **Long-press sur le bandeau pluie** de la bande centrale |
| Quitter | Hub du jeu → « Quitter » (timer arrêté, score banqué, overlay masqué) |
| Pause | **Toucher le bandeau HUD** pendant une partie (pas de croix : flux plein cadre) |
| Calibrer | Hub → **Réglages** → « Calibrer à plat », ou Pause → « Recalibrer à plat » |

### Contrôles

Le Tab5 n'a **aucun bouton physique** : le pad est fait de zones LVGL
semi-transparentes (opacité 50 %) posées sur le damier, masquées hors partie.

| Mode (Réglages) | Déplacement | Creuser |
|---|---|---|
| **Boutons** (défaut) | D-pad tactile en bas à gauche (maintien) | 2 boutons en bas à droite (tap) |
| **Inclinaison** | BMI270, 4 directions discrètes ; D-pad masqué | 2 boutons (indispensables : le tilt ne peut pas exprimer « creuser à gauche/droite ») |
| **Mixte** | Inclinaison **et** D-pad, le doigt prime | 2 boutons |

> **Pourquoi le mode Mixte est le plus confortable** : la dalle ne remonte
> qu'**un seul point de contact** à LVGL. En mode Boutons on creuse donc à
> l'arrêt (ce qui reste le cas normal dans Lode Runner) ; en Mixte, l'inclinaison
> déplace pendant que le doigt reste libre pour creuser.

Mapping IMU (rotation écran 270°, même convention que Marble) :
`X_écran = −tilt_Y`, `Y_écran = +tilt_X`, avec lissage, zone morte réglable
(5 crans) et hystérésis pour éviter le papillonnement entre deux directions.

### Règles

- **Creuser** : uniquement debout sur un sol **solide** (ni échelle, ni barre),
  la case visée doit être une **brique** et la case latérale au-dessus doit être
  libre. Le béton (`@`) ne se creuse jamais.
- **Trous** : ouverts 5,4 s, clignotent 1,4 s avant de se refermer. Un garde
  dedans est détruit (+75 pts, réapparition en haut après 1,6 s) ; **le joueur
  dedans meurt**.
- **Sortie** : les cases `S` sont vides tant qu'il reste de l'or, puis
  deviennent une échelle. Le niveau est gagné en atteignant la **rangée 0**.
- **Gardes** : 1 à 4 selon le niveau, poursuite par **BFS inverse** depuis le
  joueur (recalculé 1 tick sur 4). Ils volent un lingot au passage (45 %), le
  relâchent parfois, et le lâchent en tombant dans un trou. Contact = mort.
  Un garde **piégé** ne tue pas : on lui court sur la tête.
- **Vies** : 5. Mort = niveau relancé à zéro (or et gardes réinitialisés).
- **Score** : lingot 250 · garde détruit 75 · niveau terminé 1500 + bonus de
  temps (jusqu'à 2880).

### Format d'une map

30 colonnes × 16 rangées, tuiles de 42 px (damier 1260×672 centré sous le HUD).
Les espaces de fin de ligne peuvent être omis.

| Char | Tuile |
|---|---|
| (espace) | vide |
| `#` | brique creusable |
| `@` | béton indestructible |
| `H` | échelle |
| `-` | barre de suspension |
| `$` | lingot d'or |
| `P` | départ du joueur (exactement 1) |
| `G` | départ d'un garde (4 max) |
| `S` | échelle de sortie (vide tant qu'il reste de l'or ; doit atteindre la rangée 0) |

### Garde-fou : les 10 niveaux sont prouvés jouables

`python scripts/check_lode_levels.py` relit les maps **directement dans
`lode_game.cpp`** (source unique) et rejoue le même modèle de déplacement que le
C++ (`passable` / `supported` / `can_step` / arête de creusement). Il vérifie
que tout l'or est atteignable depuis le départ, et que la rangée 0 reste
joignable **depuis chaque lingot** une fois la sortie activée — donc qu'aucun
ordre de ramassage ne peut enfermer le joueur. Il plafonne aussi le nombre
d'objets LVGL du damier (340) et de lingots (40).

### Ce qui est persisté (NVS, survit aux reboots et aux OTA)

Top 10 (score, niveau, mode de contrôle, horodatage SNTP, drapeau hors
classement), meilleur score, plus haut niveau débloqué, mode de contrôle,
sensibilité, calibration IMU, mode entraînement. Magic `LOD1` : un layout
différent est rejeté et repart à zéro.

### Notes techniques / perf

- Logique **verrouillée sur la grille** : chaque acteur s'engage sur un pas
  d'une case et l'achève avant toute nouvelle décision (joueur 7 ticks/case,
  chute 3 ticks/case, gardes 1 tick sauté sur 5). Pas de platformer flottant.
- Objets LVGL **préalloués** (340 tuiles + 40 lingots + 5 acteurs × 2 + pad) puis
  recyclés par `show/hide` + `set_pos` : **aucune allocation dans le tick**. Les
  runs horizontaux de béton sont fusionnés en un seul objet.
- `lv_timer` de 33 ms créé à `open()`, **détruit à `close()`** → zéro tick hors jeu.
- Libellés HUD réécrits uniquement si leur valeur change.
- Palette **locale au module** (`Lode::Pal` dans `lode_game.h`) : aucun token
  ajouté à `tab5_custom.h` ni à `tab5-styles.yaml`.
- Feedback de mort : voile rouge bref + gel de 900 ms — **pas** de secousse
  plein écran (elle invaliderait 1280×672 à chaque frame).
- Audio : `sfx()` est un crochet neutre. Le Tab5 n'expose pas de générateur de
  bip côté C++ (le `media_player` joue des flux, pas des tonalités) ; les points
  d'appel sont en place pour brancher un vrai SFX plus tard.
```

---

## G. `00ProjetTab/Tab5/tab5-styles.yaml` — **AUCUNE MODIFICATION**

Contrairement à Marble (3 tokens `color_marble_*`) et à Arcanoïde / Flip Noir
(qui ont ajouté des tokens dans `tab5-styles.yaml` **et** `tab5_custom.h`),
l'overlay Lode ne pose **aucune couleur en YAML** : les fonds des 5 conteneurs
sont appliqués en C++ dans `build_ui()` depuis `Lode::Pal`.

C'est délibéré : ce sont deux fichiers partagés de moins à toucher pendant le
travail en parallèle, et **deux conflits de merge évités**. L'esprit de la
règle 1 du README (pas d'hexadécimal en dur dans un YAML ou une lambda) reste
respecté : ce sont des constantes nommées, dans un en-tête, référencées
uniquement par `lode_game.cpp`.

Si l'homogénéité avec les autres jeux est préférée à terme, il suffira de
déplacer le contenu de `namespace Lode::Pal` vers `UIColor::LODE_*` dans
`tab5_custom.h` puis de remplacer `Pal::` par `UIColor::LODE_` dans
`lode_game.cpp` — un `sed` mécanique, à faire une fois les merges terminés.

---

## H. Build — rappel du piège connu

Un **nouveau `.cpp`** dans `includes:` n'est pas vu par un `esphome compile`
normal (CMake/ESP-IDF ne globbe les sources qu'au *configure*) : la build renvoie
« Successfully compiled » sans avoir bâti `lode_game.cpp.obj`. Et sous Git
Bash/MSys la build post-clean s'arrête sur `Firmware not found` en retournant 0.

**Depuis PowerShell, à la racine `00ProjetTab/` :**

```bash
$env:ESPHOME_ESP_IDF_PREFIX = "C:\espidf"; python -m esphome clean tab5-ha-hmi.yaml
```

```bash
$env:ESPHOME_ESP_IDF_PREFIX = "C:\espidf"; python -m esphome compile tab5-ha-hmi.yaml
```

Vérifier ensuite que `lode_game.cpp.obj` existe réellement dans
`.esphome/build/tab5-ha-hmi/.pioenvs/` (c'est le seul contrôle fiable).

OTA :

```bash
$env:ESPHOME_ESP_IDF_PREFIX = "C:\espidf"; python -m esphome run tab5-ha-hmi.yaml --device 192.168.0.88
```

Garde-fous à repasser au vert avant commit :

```bash
python scripts/check_lode_levels.py
```

```bash
python scripts/check_tab5_modal_chrome.py
```

---

## I. Checklist de test sur l'appareil

**Ouverture / fermeture**
- [ ] Long-press bandeau pluie → hub « COUREUR D'OR » plein écran, dashboard masqué
- [ ] Hub → « Quitter » → retour dashboard fluide, aucun ralentissement résiduel
- [ ] Long-press bandeau planning → « Fil d'Or » **toujours fonctionnel** (non régressé)
- [ ] Tap-to-wake toujours opérationnel après avoir joué

**Boucle de jeu (niveau 1)**
- [ ] Le damier remplit l'écran sous le HUD, 30×16 cases lisibles
- [ ] Courir gauche/droite, monter/descendre les échelles, se suspendre aux barres
- [ ] Lâcher une barre (bas) → chute franche jusqu'au sol
- [ ] Creuser à gauche puis à droite → la brique disparaît, le trou clignote puis se referme
- [ ] Rester dans un trou qui se referme → **mort** (voile rouge, une vie en moins)
- [ ] Ramasser tout l'or → toast « Sortie ouverte », l'échelle blanche apparaît
- [ ] Grimper jusqu'en haut → écran « NIVEAU TERMINE » avec bonus de temps

**Gardes**
- [ ] Le garde poursuit réellement (monte les échelles, ne reste pas bloqué)
- [ ] Un garde tombe dans un trou → immobile, silhouette grise
- [ ] Courir sur la tête d'un garde piégé → **pas de mort**
- [ ] Le trou se referme sur lui → +75 pts, réapparition en haut
- [ ] Un garde vole un lingot (couleur verte) et le relâche en tombant dans un trou
- [ ] Contact frontal → mort

**Contrôles**
- [ ] Réglages → cycle Boutons / Inclinaison / Mixte
- [ ] Mode Inclinaison : le D-pad disparaît, seuls les 2 boutons creuser restent
- [ ] **Vérifier le sens des axes** (écran en rotation 270°) : incliner à gauche
      doit faire courir à gauche. Si inversé, changer le signe dans
      `update_imu_dir()` (`lode_game.cpp`, section 16) et le documenter
- [ ] Calibrer à plat → la bille… le coureur ne dérive plus au repos
- [ ] Sensibilité 1/5 vs 5/5 : différence nette de zone morte
- [ ] Mise en pause avec un doigt posé sur le D-pad → à la reprise, **le coureur
      ne repart pas tout seul**

**Menus & persistance**
- [ ] Tap HUD en jeu → PAUSE ; « Relancer le niveau » ne coûte pas de vie
- [ ] Niveaux : seuls les niveaux débloqués sont cliquables
- [ ] Game over → le score entre au classement, daté si SNTP est synchronisé
- [ ] Mode entraînement → vies « oo », score marqué **H.C.**
- [ ] Reboot / OTA → scores, record et progression toujours là
- [ ] Réglages → « Effacer » → confirmation → tout est remis à zéro

**Perf**
- [ ] Aucune saccade avec 4 gardes (niveaux 9 et 10)
- [ ] `RAM` / `Flash` après build : noter l'écart (attendu ~+15 ko de flash)
