# Tracking

## English · [Français](#version-française)

---

**Opens with** a tap on the tracked sensor left of the clock, the « Suivi » entry of the tablet's « Aller à l'écran » list in Home Assistant, or a tap or a long press of the clock or of a button top right when the blueprint gives it « Suivi (capteurs suivis) · Tracking (tracked sensors) » ([home screen](home.md#the-three-buttons-top-right-6-to-8)).

The sensors you follow, one card each, up to six: a share price, a temperature, a power, a level… any sensor of Home Assistant that has a number and statistics.

- **Name** at the top of the card, cut with « … » when it is too long.
- **Value** with its unit.
- **Change**, in green going up, in red going down, in grey when flat, with an arrow: the change of the day in % when the sensor gives one (« Aujourd'hui »), otherwise the difference over the last 24 hours, in the sensor's unit (« 24 h »).
- **Curve** of the last 24 hours, one point per hour, its last point (now) marked. An hour without a measure leaves the curve going through its neighbours. The curve shows the trend: its bottom is the lowest value of the 24 hours, its top the highest, whatever the size of the move.

Up to three sensors sit on one row; four on two rows of two; five or six on two rows of three.

**Left of the clock**, the first sensor of the list can take the place of the voice controls (chosen in the « Tab5 — emplacements » blueprint, section « Zone à gauche de l'horloge », or by a tap on the second temperature): its name, its change, its value and its curve over a light gradient. A tap on it opens this window. With no sensor chosen, the tablet skips it.

**Setting up** in Home Assistant, where to find the list: *Settings → Devices & services → Entities*, then search « capteurs suivis » (entity `select.tab5_capteurs_suivis`); or the dashboard's *Tab5 settings* view, section *Home*, tile « Tracked sensors » (a dashboard generated before this version does not have it yet: it is regenerated from `tab5_dashboard.jinja`). It is a drop-down list: open it, pick a sensor (« ○ » becomes « ✓ »), pick the next one. Choose the sensors in the list « Tab5 · capteurs suivis · tracked sensors » (up to six; a choice is added or removed each time you pick it; the first one is the one shown left of the clock). Only sensors with statistics are offered (`state_class: measurement`). With no sensor chosen, the window says where to choose them. Home Assistant sends a change at once, then at most every five minutes, and every hour so the curve moves on. This needs the `tab5_suivi.yaml` package of this version.

Like every window, it closes with its **×**, a tap on the dark area around it, or after 45 s without a touch.

---

## Version Française

---

**S'ouvre par** un tap sur le capteur suivi à gauche de l'horloge, l'entrée « Suivi » de la liste « Aller à l'écran » de la tablette dans Home Assistant, ou un tap ou un appui long sur l'horloge ou un bouton en haut à droite quand le blueprint lui donne « Suivi (capteurs suivis) · Tracking (tracked sensors) » ([écran d'accueil](home.md#les-trois-boutons-en-haut-à-droite-6-à-8)).

Les capteurs que vous suivez, une carte chacun, six au plus : un cours de bourse, une température, une puissance, un niveau… n'importe quel capteur de Home Assistant qui a un nombre et des statistiques.

- **Nom** en haut de la carte, coupé par « … » s'il est trop long.
- **Valeur** avec son unité.
- **Variation**, en vert à la hausse, en rouge à la baisse, en gris quand rien ne bouge, avec une flèche : la variation du jour en % quand le capteur en donne une (« Aujourd'hui »), sinon l'écart sur les 24 dernières heures, dans l'unité du capteur (« 24 h »).
- **Courbe** des 24 dernières heures, un point par heure, son dernier point (maintenant) marqué. Une heure sans mesure laisse la courbe passer par ses voisines. La courbe montre la tendance : son bas est la plus petite valeur des 24 heures, son haut la plus grande, quelle que soit l'ampleur du mouvement.

Jusqu'à trois capteurs tiennent sur une rangée ; quatre sur deux rangées de deux ; cinq ou six sur deux rangées de trois.

**À gauche de l'horloge**, le premier capteur de la liste peut prendre la place des commandes vocales (choisi dans le blueprint « Tab5 — emplacements », section « Zone à gauche de l'horloge », ou par un tap sur la seconde température) : son nom, sa variation, sa valeur et sa courbe sur un léger dégradé. Un tap dessus ouvre cette fenêtre. Sans capteur choisi, la tablette le saute.

**Réglage** dans Home Assistant, où trouver la liste : *Paramètres → Appareils et services → Entités*, puis chercher « capteurs suivis » (entité `select.tab5_capteurs_suivis`) ; ou la vue *Réglages Tab5* du tableau de bord, section *Maison*, tuile « Capteurs suivis (popup Suivi) » (un tableau de bord généré avant cette version ne l'a pas encore : il se régénère depuis `tab5_dashboard.jinja`). C'est une liste déroulante : l'ouvrir, choisir un capteur (« ○ » devient « ✓ »), puis le suivant. Choisir les capteurs dans la liste « Tab5 · capteurs suivis · tracked sensors » (six au plus ; chaque choix ajoute ou retire un capteur ; le premier est celui montré à gauche de l'horloge). Seuls les capteurs qui ont des statistiques sont proposés (`state_class: measurement`). Sans capteur choisi, la fenêtre dit où les choisir. Home Assistant envoie un changement tout de suite, puis au plus toutes les cinq minutes, et toutes les heures pour que la courbe avance. Il faut le package `tab5_suivi.yaml` de cette version.

Comme toute fenêtre, elle se ferme par sa **×**, un tap sur la zone sombre autour, ou après 45 s sans toucher.
