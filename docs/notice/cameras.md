# Cameras

## English · [Français](#version-française)

---

**Opens with** « Agenda ▸ Caméras » on the [navigation wheel](home.md) (long press on the central card), with the « Cameras » entry of the tablet's « Go to screen » list in Home Assistant (an automation can open it, for instance when the doorbell rings), or with a tap or a long press of the clock or of a button top right when the blueprint gives it « Caméras · Cameras » ([home screen](home.md#the-three-buttons-top-right-6-to-8)).

A still image of each camera you picked in the « Caméras · Cameras » section of the blueprint « Tab5 — emplacements », in that order (sixteen at most): new images while the window is open (one at a time; the camera shown large can play as video, see below), nothing downloaded once it is closed.

- **Rooms.** The cameras are grouped by the area Home Assistant gives them (the camera's, or its device's): nothing to set. With two areas or more, a column on the left lists « Toutes » (all of them), then each room with its number of cameras, then « Autres » for the cameras without an area. **Tap** a room to see only its cameras; the column scrolls when there are many rooms. With a single room, there is no column and the image is centred.
- **Mosaic.** With two cameras or more in the chosen room, the window shows them side by side, four at most (two, three or four squares filling the frame), each with its name; every small image is refreshed about every 10 seconds, one after the other. Beyond four, **swipe** left or right for the next four; the dots under the frame show the page.
- **One camera, large.** **Tap** a small image to see that camera large, with a new image about every 5 seconds; **swipe** to see the next camera of the room; **tap** the image to go back to the mosaic. Under the image, « Room · Camera » and the time of the image (« Image de 14:32:05 »).
- **Video (optional).** Check « Vidéo de la caméra en grand · Video of the enlarged camera » in the blueprint's section: the camera shown large then plays Home Assistant's own video stream (MJPEG, no add-on, no sound) instead of one image every 5 seconds. What you get depends on the camera: ESPHome, MJPEG and ffmpeg / ONVIF / TP-Link (Tapo) cameras send their real frames; the others, two images per second at most. A picture larger than 1280 × 736 pixels (the main stream of a Full HD camera) is not played: the tablet goes back to still images by itself and tries the video again after 1, 5, then 30 minutes. Unchecked (the default), nothing changes. The mosaic always shows still images, and the video does not keep the window open: the 45-second return still applies without a touch.
- The room, the camera and the view you chose (mosaic or large) are **remembered**, even after a restart. Coming back to a camera shows its last image at once while the new one arrives; the cameras next to the one shown large are fetched in advance (not while it plays video).

**How smooth the video is, and what sets it.** The tablet shows the most recent image it has received and decodes it in 3 to 4 ms, so the rate is the one the camera sends. With an ESP32 camera on ESPHome, that is almost always the camera itself: it makes each JPEG on its own chip, one image at a time. Measured on the author's camera (M5Stack AtomS3R-CAM, 640 × 480 sensor, ESPHome 2026.9.0), the tablet showed 1.5 to 1.7 images per second, and reading Home Assistant's stream from a PC, or the camera directly, gave the same: the tablet was not the limit. Two settings in the camera's ESPHome file brought the same picture to about 2.5 to 2.8 images per second, and a smaller picture goes further:

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_LWIP_TCP_SND_BUF_DEFAULT: "32768"   # bigger TCP send buffer (5760 by default)
esp32_camera:
  frame_buffer_count: 2   # capture the next image while the last one is being sent
```

| `resolution` of `esp32_camera` | Picture | JPEG size | Images per second |
|---|---|---|---|
| `VGA`, default settings | 640 × 480 | about 48 KB | 1.4 to 1.5 |
| `VGA` and the two settings above | 640 × 480 | about 46 KB | 2.5 to 2.8 |
| `CIF` and the two settings | 400 × 296 | about 24 KB | 6.0 to 6.3 |
| `QVGA` and the two settings | 320 × 240 | about 18 KB | 8.9 to 9.3 |

Shared in case it helps, with its limits: **one camera on one network**, 30 seconds per measure and two or three measures per line, read from a PC on Home Assistant's stream (`/api/camera_proxy_stream`), **not on the tablet**, which was not measured again after the change. A smaller picture is enlarged to fill the window: faster, but softer. The second image buffer takes about 0.6 MB of the camera's PSRAM at 640 × 480. Sending the new file to the camera by OTA makes it unavailable in Home Assistant for about 45 seconds. Tried without a gain worth keeping: compiling the camera for speed (`compiler_optimization: PERF`, no change) and a lower JPEG quality (`jpeg_quality: 30`, +6 %, softer picture). A camera that is not on ESPHome sets its own rate; nothing here applies to it.

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

**S'ouvre par** « Agenda ▸ Caméras » de la [roue de navigation](home.md#version-française) (appui long sur la carte centrale), par l'entrée « Cameras » de la liste « Go to screen » de la tablette dans Home Assistant (une automatisation peut l'ouvrir, par exemple quand on sonne), ou par un tap ou un appui long sur l'horloge ou un bouton en haut à droite quand le blueprint lui donne « Caméras · Cameras » ([écran d'accueil](home.md#les-trois-boutons-en-haut-à-droite-6-à-8)).

Une image fixe de chaque caméra choisie dans la section « Caméras · Cameras » du blueprint « Tab5 — emplacements », dans cet ordre (seize au plus) : des images neuves tant que la fenêtre est ouverte (une à la fois ; la caméra montrée en grand peut passer en vidéo, voir plus bas), plus rien de téléchargé une fois fermée.

- **Pièces.** Les caméras sont rangées par la pièce que Home Assistant leur donne (celle de la caméra, ou de son appareil) : rien à régler. À partir de deux pièces, une colonne à gauche propose « Toutes », puis chaque pièce avec son nombre de caméras, puis « Autres » pour les caméras sans pièce. **Toucher** une pièce ne montre que ses caméras ; la colonne défile quand il y a beaucoup de pièces. Avec une seule pièce, pas de colonne : l'image est centrée.
- **Mosaïque.** À partir de deux caméras dans la pièce choisie, la fenêtre les montre côte à côte, quatre au plus (deux, trois ou quatre cases qui remplissent le cadre), chacune avec son nom ; chaque petite image est renouvelée environ toutes les 10 secondes, l'une après l'autre. Au-delà de quatre, **glisser** vers la gauche ou la droite montre les quatre suivantes ; les pastilles sous le cadre disent la page.
- **Une caméra en grand.** **Toucher** une petite image montre cette caméra en grand, une nouvelle image environ toutes les 5 secondes ; **glisser** montre la caméra suivante de la pièce ; **toucher** l'image revient à la mosaïque. Sous l'image, « Pièce · Caméra » et l'heure de l'image (« Image de 14:32:05 »).
- **Vidéo (facultative).** Cocher « Vidéo de la caméra en grand · Video of the enlarged camera » dans la section du blueprint : la caméra montrée en grand est alors lue dans le flux vidéo de Home Assistant lui-même (MJPEG, sans module ni son) au lieu d'une image toutes les 5 secondes. Le résultat dépend de la caméra : les caméras ESPHome, MJPEG et ffmpeg / ONVIF / TP-Link (Tapo) envoient leurs vraies images ; les autres, deux images par seconde au plus. Une image de plus de 1280 × 736 pixels (le flux principal d'une caméra Full HD) n'est pas lue : la tablette revient seule aux images fixes et retente la vidéo après 1, 5 puis 30 minutes. Décochée (par défaut), rien ne change. La mosaïque montre toujours des images fixes, et la vidéo ne garde pas la fenêtre ouverte : le retour au bout de 45 s sans toucher reste.
- La pièce, la caméra et la vue choisies (mosaïque ou en grand) sont **gardées**, même après un redémarrage. Revenir sur une caméra montre tout de suite sa dernière image, le temps que la nouvelle arrive ; les caméras voisines de celle montrée en grand sont chargées d'avance (pas pendant sa vidéo).

**La fluidité de la vidéo, et ce qui la fixe.** La tablette montre l'image la plus récente qu'elle a reçue et la décode en 3 à 4 ms : la cadence est celle que la caméra envoie. Avec une caméra ESP32 sous ESPHome, c'est presque toujours la caméra elle-même : elle fabrique chaque JPEG avec sa propre puce, une image à la fois. Mesuré sur la caméra de l'auteur (M5Stack AtomS3R-CAM, capteur 640 × 480, ESPHome 2026.9.0), la tablette montrait 1,5 à 1,7 image par seconde, et la lecture du flux de Home Assistant depuis un PC, ou de la caméra directement, donnait la même chose : la tablette n'était pas la limite. Deux réglages dans le fichier ESPHome de la caméra ont porté la même image à environ 2,5 à 2,8 images par seconde, et une image plus petite va plus loin :

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_LWIP_TCP_SND_BUF_DEFAULT: "32768"   # tampon d'envoi TCP plus grand (5760 par défaut)
esp32_camera:
  frame_buffer_count: 2   # capture l'image suivante pendant l'envoi de la précédente
```

| `resolution` de `esp32_camera` | Image | Taille du JPEG | Images par seconde |
|---|---|---|---|
| `VGA`, réglages par défaut | 640 × 480 | environ 48 Ko | 1,4 à 1,5 |
| `VGA` et les deux réglages ci-dessus | 640 × 480 | environ 46 Ko | 2,5 à 2,8 |
| `CIF` et les deux réglages | 400 × 296 | environ 24 Ko | 6,0 à 6,3 |
| `QVGA` et les deux réglages | 320 × 240 | environ 18 Ko | 8,9 à 9,3 |

Partagé au cas où cela servirait, avec ses limites : **une seule caméra sur un seul réseau**, 30 secondes par mesure et deux ou trois mesures par ligne, lues depuis un PC sur le flux de Home Assistant (`/api/camera_proxy_stream`), **pas sur la tablette**, qui n'a pas été remesurée après le changement. Une image plus petite est agrandie pour remplir la fenêtre : plus rapide, mais plus floue. Le deuxième tampon d'image prend environ 0,6 Mo de la PSRAM de la caméra en 640 × 480. Envoyer le nouveau fichier à la caméra par OTA la rend indisponible dans Home Assistant pendant environ 45 secondes. Essayés sans gain qui vaille d'être gardé : compiler la caméra pour la vitesse (`compiler_optimization: PERF`, aucun changement) et une qualité JPEG plus basse (`jpeg_quality: 30`, +6 %, image plus floue). Une caméra qui n'est pas sous ESPHome fixe sa propre cadence ; rien de ceci ne s'y applique.

Ce que la fenêtre peut dire à la place d'une image :

- « En attente de Home Assistant » : la tablette a demandé les caméras à Home Assistant et n'a pas encore de réponse (l'automatisation du blueprint doit être à jour).
- « Aucune caméra choisie » : la section « Caméras · Cameras » est vide.
- « Chargement... » : la première image de cette caméra arrive.
- « Hors ligne depuis 14:32 » (ou « … depuis Lun 14:32 », « … depuis le 3 Oct ») : Home Assistant dit la caméra indisponible, ou ses deux dernières images ont échoué. Sa dernière image, s'il y en a une, reste derrière, atténuée. La tablette ne demande pas à Home Assistant une caméra qu'il dit indisponible ; après des échecs, elle réessaie 10 s plus tard, puis 30 s, puis toutes les minutes, sans retenir les autres caméras.
- « Image indisponible » : l'image n'a pas pu être lue une fois (format que la tablette ne lit pas, comme un JPEG progressif, ou erreur réseau).
- « Adresse de Home Assistant inconnue » : la tablette ne sait pas joindre Home Assistant. Remplir « Adresse de Home Assistant » dans la section du blueprint (par exemple `https://ha.example.com`) ; c'est nécessaire de toute façon en https, sur un autre port ou derrière un proxy.
- « Plus d'image depuis 14:32:05 », sous l'image à la place de son heure : la dernière image reste affichée, mais la suivante a échoué.

Les images sont téléchargées et décodées en arrière-plan : l'écran ne se fige pas. Comme toute fenêtre, elle se ferme par sa **×**, un tap sur la zone sombre autour, ou après 45 s sans toucher.
