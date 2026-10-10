# Music

## English · [Français](#version-française)

---

**Opens with** a long press on a TV or media player card that is not the blueprint's TV ([tiles](tiles.md)), a tap on the « now playing » bar of the home screen or on the compact player left of the clock, « Musique » under « Devices » in the [navigation wheel](home.md#central-card-12) (long press on the central card; not offered once Home Assistant says no player is chosen), the « Musique » entry of the tablet's « Aller à l'écran » list in Home Assistant, or a tap or a long press of the clock or of a button top right when the blueprint gives it « Musique (lecteur) · Music (player) » ([home screen](home.md#the-three-buttons-top-right-6-to-8)).

A music player for any media player of Home Assistant (an Apple TV, a TV, a Freebox, a speaker, the tablet itself…), in a window that leaves the edges of the screen visible.

- **Header**: the player's name and, on the right, the app it plays from (when Home Assistant knows it).
- **Cover** on the left; a music note when the player sends none.
- **Title, artist, album** on the right, then the **position bar** with the elapsed time and the duration: drag it to move in the track (a live stream has no duration and no bar).
- **Buttons**: shuffle, previous, play / pause, next, repeat (all, one, off). Shuffle and repeat are in colour when on.
- **Volume**: the speaker button mutes, the slider sets the volume.
- **Players** at the bottom: one chip per chosen player, the one shown outlined in colour. A tap shows that player.

A button only shows when the player can do it: a TV that cannot skip tracks has no previous and next. A player that is off says « Lecteur éteint » (player off) with an « Allumer » (turn on) button when it can be turned on.

**The « now playing » bar** takes the place of the Ok Nabu button on the home screen while the shown player plays, and for 5 minutes after a pause: the cover, the title and the artist, and a play / pause button. A tap on the bar opens this window. The Ok Nabu button comes back when the bar goes.

**The compact player** can take the place of the voice controls left of the clock (chosen in the « Tab5 — emplacements » blueprint, section « Zone à gauche de l'horloge », or by a tap on the second temperature): the cover, the title and the artist, a thin position bar, and previous, play / pause and next — the same player and the same buttons as this window. A tap elsewhere on it opens this window. While it shows, the « now playing » bar stays hidden and the Ok Nabu button stays in place; the bar comes back when the area shows something else. With no player chosen, the tablet skips it.

**Setting up** in Home Assistant: choose the players in the list « Tab5 · lecteurs de musique · music players » (up to six; a choice is added or removed each time you pick it). With no player chosen, the window says where to choose them. The player shown is the one picked on the tablet, otherwise the first of the list that plays; a player of the list that starts playing is shown when the current one does not play. This needs the `tab5_lecteur.yaml` package of this version. The cover comes from Home Assistant over http: with a Home Assistant reached only in https, the music note shows instead.

Like every window, it closes with its **×**, a tap on the dark area around it, or after 45 s without a touch.

---

## Version Française

---

**S'ouvre par** un appui long sur une carte TV ou lecteur multimédia qui n'est pas la TV du blueprint ([tuiles](tiles.md#version-française)), un tap sur la barre « en lecture » de l'écran d'accueil ou sur le lecteur compact à gauche de l'horloge, « Musique » sous « Appareils » dans la [roue de navigation](home.md#carte-centrale-12) (appui long sur la carte centrale ; plus proposée quand Home Assistant dit qu'aucun lecteur n'est choisi), l'entrée « Musique » de la liste « Aller à l'écran » de la tablette dans Home Assistant, ou un tap ou un appui long sur l'horloge ou un bouton en haut à droite quand le blueprint lui donne « Musique (lecteur) · Music (player) » ([écran d'accueil](home.md#les-trois-boutons-en-haut-à-droite-6-à-8)).

Un lecteur de musique pour n'importe quel lecteur multimédia de Home Assistant (une Apple TV, une TV, une Freebox, une enceinte, la tablette elle-même…), dans une fenêtre qui laisse voir les bords de l'écran.

- **En-tête** : le nom du lecteur et, à droite, l'application d'où vient la musique (quand Home Assistant la connaît).
- **Pochette** à gauche ; une note de musique quand le lecteur n'en envoie pas.
- **Titre, artiste, album** à droite, puis la **barre de position** avec le temps écoulé et la durée : la faire glisser pour se déplacer dans le morceau (un direct n'a ni durée ni barre).
- **Boutons** : aléatoire, précédent, lecture / pause, suivant, répétition (tout, un seul, non). Aléatoire et répétition sont en couleur quand ils sont actifs.
- **Volume** : le bouton du haut-parleur coupe le son, le curseur règle le volume.
- **Lecteurs** en bas : une pastille par lecteur choisi, celui affiché cerclé de couleur. Un tap affiche ce lecteur.

Un bouton n'apparaît que si le lecteur sait le faire : une TV qui ne sait pas changer de morceau n'a ni précédent ni suivant. Un lecteur éteint affiche « Lecteur éteint » avec un bouton « Allumer » quand il peut être allumé.

**La barre « en lecture »** prend la place du bouton Ok Nabu sur l'écran d'accueil pendant que le lecteur affiché joue, et 5 min après une pause : la pochette, le titre et l'artiste, et un bouton lecture / pause. Un tap sur la barre ouvre cette fenêtre. Le bouton Ok Nabu revient quand la barre s'en va.

**Le lecteur compact** peut prendre la place des commandes vocales à gauche de l'horloge (choisi dans le blueprint « Tab5 — emplacements », section « Zone à gauche de l'horloge », ou par un tap sur la seconde température) : la pochette, le titre et l'artiste, une fine barre de position, et précédent, lecture / pause et suivant — le même lecteur et les mêmes boutons que cette fenêtre. Un tap ailleurs l'ouvre. Pendant qu'il s'affiche, la barre « en lecture » reste masquée et le bouton Ok Nabu reste en place ; la barre revient quand la zone montre autre chose. Sans lecteur choisi, la tablette le saute.

**Réglage** dans Home Assistant : choisir les lecteurs dans la liste « Tab5 · lecteurs de musique · music players » (six au plus ; chaque choix ajoute ou retire un lecteur). Sans lecteur choisi, la fenêtre dit où les choisir. Le lecteur affiché est celui choisi sur la tablette, sinon le premier de la liste qui joue ; un lecteur de la liste qui se met à jouer est affiché quand celui en cours ne joue pas. Il faut le package `tab5_lecteur.yaml` de cette version. La pochette vient de Home Assistant en http : avec un Home Assistant joint seulement en https, la note de musique s'affiche à la place.

Comme toute fenêtre, elle se ferme par sa **×**, un tap sur la zone sombre autour, ou après 45 s sans toucher.
