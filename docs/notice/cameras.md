# Cameras

## English · [Français](#version-française)

---

**Opens with** the « Caméras » entry of the tablet's « Aller à l'écran » list in Home Assistant (an automation can open it, for instance when the doorbell rings), or with a tap or a long press of the clock or of a button top right when the blueprint gives it « Caméras · Cameras » ([home screen](home.md#the-three-buttons-top-right-6-to-8)).

A still image of each camera you picked in the « Caméras · Cameras » section of the blueprint « Tab5 — emplacements », in that order (eight at most): no video, a new image about every 5 seconds while the window is open, nothing downloaded once it is closed. One camera per page: **swipe** left or right to see the next one; the dots under the image show which one it is (with two cameras or more). Under the image, the camera's name and the time of the image (« Image de 14:32:05 »).

What the window can say instead of an image:

- « En attente de Home Assistant »: the tablet has asked Home Assistant for the cameras and has no answer yet (the blueprint's automation must be up to date).
- « Aucune caméra choisie »: the « Caméras · Cameras » section is empty.
- « Chargement... »: the first image of this camera is on its way.
- « Image indisponible »: the image could not be read (camera offline, a format the tablet does not read, such as a progressive JPEG); the tablet tries again 10 s later, then 30 s, then every minute while it keeps failing (each try may freeze the screen up to 12 s).
- « Adresse de Home Assistant inconnue »: the tablet does not know how to reach Home Assistant. Fill « Adresse de Home Assistant » in the blueprint's section (for instance `https://ha.example.com`); it is needed anyway with https, another port or a reverse proxy.
- « Plus d'image depuis 14:32:05 », under the image in place of its time: the last image stays on screen, but the next ones fail.

While an image is being taken and decoded, the screen may freeze for a moment (up to a few seconds with a slow camera). Like every window, it closes with its **×**, a tap on the dark area around it, or after 45 s without a touch.

---

## Version Française

---

**S'ouvre par** l'entrée « Caméras » de la liste « Aller à l'écran » de la tablette dans Home Assistant (une automatisation peut l'ouvrir, par exemple quand on sonne), ou par un tap ou un appui long sur l'horloge ou un bouton en haut à droite quand le blueprint lui donne « Caméras · Cameras » ([écran d'accueil](home.md#les-trois-boutons-en-haut-à-droite-6-à-8)).

Une image fixe de chaque caméra choisie dans la section « Caméras · Cameras » du blueprint « Tab5 — emplacements », dans cet ordre (huit au plus) : pas de vidéo, une nouvelle image environ toutes les 5 secondes tant que la fenêtre est ouverte, plus rien de téléchargé une fois fermée. Une caméra par page : **glisser** vers la gauche ou la droite pour voir la suivante ; les pastilles sous l'image disent laquelle est montrée (à partir de deux caméras). Sous l'image, le nom de la caméra et l'heure de l'image (« Image de 14:32:05 »).

Ce que la fenêtre peut dire à la place d'une image :

- « En attente de Home Assistant » : la tablette a demandé les caméras à Home Assistant et n'a pas encore de réponse (l'automatisation du blueprint doit être à jour).
- « Aucune caméra choisie » : la section « Caméras · Cameras » est vide.
- « Chargement... » : la première image de cette caméra arrive.
- « Image indisponible » : l'image n'a pas pu être lue (caméra hors ligne, format que la tablette ne lit pas, comme un JPEG progressif) ; la tablette réessaie 10 s plus tard, puis 30 s, puis toutes les minutes tant que l'échec dure (chaque essai peut figer l'écran jusqu'à 12 s).
- « Adresse de Home Assistant inconnue » : la tablette ne sait pas joindre Home Assistant. Remplir « Adresse de Home Assistant » dans la section du blueprint (par exemple `https://ha.example.com`) ; c'est nécessaire de toute façon en https, sur un autre port ou derrière un proxy.
- « Plus d'image depuis 14:32:05 », sous l'image à la place de son heure : la dernière image reste affichée, mais les suivantes échouent.

Pendant qu'une image est prise et décodée, l'écran peut se figer un instant (jusqu'à quelques secondes avec une caméra lente). Comme toute fenêtre, elle se ferme par sa **×**, un tap sur la zone sombre autour, ou après 45 s sans toucher.
