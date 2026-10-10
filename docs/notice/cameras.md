# Cameras

## English · [Français](#version-française)

---

**Opens with** « Agenda ▸ Caméras » on the [navigation wheel](home.md) (long press on the central card), with the « Caméras » entry of the tablet's « Aller à l'écran » list in Home Assistant (an automation can open it, for instance when the doorbell rings), or with a tap or a long press of the clock or of a button top right when the blueprint gives it « Caméras · Cameras » ([home screen](home.md#the-three-buttons-top-right-6-to-8)).

A still image of each camera you picked in the « Caméras · Cameras » section of the blueprint « Tab5 — emplacements », in that order (sixteen at most): no video, new images while the window is open (one at a time), nothing downloaded once it is closed.

- **Rooms.** The cameras are grouped by the area Home Assistant gives them (the camera's, or its device's): nothing to set. With two areas or more, a column on the left lists « Toutes » (all of them), then each room with its number of cameras, then « Autres » for the cameras without an area. **Tap** a room to see only its cameras; the column scrolls when there are many rooms. With a single room, there is no column and the image is centred.
- **Mosaic.** With two cameras or more in the chosen room, the window shows them side by side, four at most (two, three or four squares filling the frame), each with its name; every small image is refreshed about every 10 seconds, one after the other. Beyond four, **swipe** left or right for the next four; the dots under the frame show the page.
- **One camera, large.** **Tap** a small image to see that camera large, with a new image about every 5 seconds; **swipe** to see the next camera of the room; **tap** the image to go back to the mosaic. Under the image, « Room · Camera » and the time of the image (« Image de 14:32:05 »).
- The room, the camera and the view you chose (mosaic or large) are **remembered**, even after a restart. Coming back to a camera shows its last image at once while the new one arrives; the cameras next to the one shown large are fetched in advance.

What the window can say instead of an image:

- « En attente de Home Assistant »: the tablet has asked Home Assistant for the cameras and has no answer yet (the blueprint's automation must be up to date).
- « Aucune caméra choisie »: the « Caméras · Cameras » section is empty.
- « Chargement... »: the first image of this camera is on its way.
- « Hors ligne depuis 14:32 » (or « … depuis Lun 14:32 », « … depuis le 3 Oct »): Home Assistant says the camera is unavailable, or its last two images failed. Its last image, if there is one, stays behind, dimmed. The tablet does not ask Home Assistant for a camera it says unavailable; after failures it tries again 10 s later, then 30 s, then every minute, without holding up the other cameras.
- « Image indisponible »: the image could not be read once (a format the tablet does not read, such as a progressive JPEG, or a network error).
- « Adresse de Home Assistant inconnue »: the tablet does not know how to reach Home Assistant. Fill « Adresse de Home Assistant » in the blueprint's section (for instance `https://ha.example.com`); it is needed anyway with https, another port or a reverse proxy.
- « Plus d'image depuis 14:32:05 », under the image in place of its time: the last image stays on screen, but the next one failed.

Images are downloaded and decoded in the background: the screen does not freeze. Like every window, it closes with its **×**, a tap on the dark area around it, or after 45 s without a touch.

---

## Version Française

---

**S'ouvre par** « Agenda ▸ Caméras » de la [roue de navigation](home.md#version-française) (appui long sur la carte centrale), par l'entrée « Caméras » de la liste « Aller à l'écran » de la tablette dans Home Assistant (une automatisation peut l'ouvrir, par exemple quand on sonne), ou par un tap ou un appui long sur l'horloge ou un bouton en haut à droite quand le blueprint lui donne « Caméras · Cameras » ([écran d'accueil](home.md#les-trois-boutons-en-haut-à-droite-6-à-8)).

Une image fixe de chaque caméra choisie dans la section « Caméras · Cameras » du blueprint « Tab5 — emplacements », dans cet ordre (seize au plus) : pas de vidéo, des images neuves tant que la fenêtre est ouverte (une à la fois), plus rien de téléchargé une fois fermée.

- **Pièces.** Les caméras sont rangées par la pièce que Home Assistant leur donne (celle de la caméra, ou de son appareil) : rien à régler. À partir de deux pièces, une colonne à gauche propose « Toutes », puis chaque pièce avec son nombre de caméras, puis « Autres » pour les caméras sans pièce. **Toucher** une pièce ne montre que ses caméras ; la colonne défile quand il y a beaucoup de pièces. Avec une seule pièce, pas de colonne : l'image est centrée.
- **Mosaïque.** À partir de deux caméras dans la pièce choisie, la fenêtre les montre côte à côte, quatre au plus (deux, trois ou quatre cases qui remplissent le cadre), chacune avec son nom ; chaque petite image est renouvelée environ toutes les 10 secondes, l'une après l'autre. Au-delà de quatre, **glisser** vers la gauche ou la droite montre les quatre suivantes ; les pastilles sous le cadre disent la page.
- **Une caméra en grand.** **Toucher** une petite image montre cette caméra en grand, une nouvelle image environ toutes les 5 secondes ; **glisser** montre la caméra suivante de la pièce ; **toucher** l'image revient à la mosaïque. Sous l'image, « Pièce · Caméra » et l'heure de l'image (« Image de 14:32:05 »).
- La pièce, la caméra et la vue choisies (mosaïque ou en grand) sont **gardées**, même après un redémarrage. Revenir sur une caméra montre tout de suite sa dernière image, le temps que la nouvelle arrive ; les caméras voisines de celle montrée en grand sont chargées d'avance.

Ce que la fenêtre peut dire à la place d'une image :

- « En attente de Home Assistant » : la tablette a demandé les caméras à Home Assistant et n'a pas encore de réponse (l'automatisation du blueprint doit être à jour).
- « Aucune caméra choisie » : la section « Caméras · Cameras » est vide.
- « Chargement... » : la première image de cette caméra arrive.
- « Hors ligne depuis 14:32 » (ou « … depuis Lun 14:32 », « … depuis le 3 Oct ») : Home Assistant dit la caméra indisponible, ou ses deux dernières images ont échoué. Sa dernière image, s'il y en a une, reste derrière, atténuée. La tablette ne demande pas à Home Assistant une caméra qu'il dit indisponible ; après des échecs, elle réessaie 10 s plus tard, puis 30 s, puis toutes les minutes, sans retenir les autres caméras.
- « Image indisponible » : l'image n'a pas pu être lue une fois (format que la tablette ne lit pas, comme un JPEG progressif, ou erreur réseau).
- « Adresse de Home Assistant inconnue » : la tablette ne sait pas joindre Home Assistant. Remplir « Adresse de Home Assistant » dans la section du blueprint (par exemple `https://ha.example.com`) ; c'est nécessaire de toute façon en https, sur un autre port ou derrière un proxy.
- « Plus d'image depuis 14:32:05 », sous l'image à la place de son heure : la dernière image reste affichée, mais la suivante a échoué.

Les images sont téléchargées et décodées en arrière-plan : l'écran ne se fige pas. Comme toute fenêtre, elle se ferme par sa **×**, un tap sur la zone sombre autour, ou après 45 s sans toucher.
