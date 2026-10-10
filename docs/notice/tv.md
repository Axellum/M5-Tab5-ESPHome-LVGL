# TV remote

## English · [Français](#version-française)

---

**Opens with** a long press on the gamepad button, top right of the home screen, on the card of the TV picked in the blueprint ([bottom row](tiles.md)), or « TV » under « Devices » in the [navigation wheel](home.md#central-card-12) (long press on the central card).

![The TV remote: Power, Source, Menu, the pad with OK, volume, playback keys and app buttons](../images/notice/telecommande-tv-en.webp)

- **Power**, **Source**, **Menu** (left).
- The round pad: the four arrows and **OK** (centre).
- **Play**, **Pause**, **Back**, **Home**, right of the pad.
- The volume column (right): **+**, mute, **−**.
- The bottom row opens an app on the TV: **Netflix**, **Prime**, **YouTube**, **CANAL+**; **PC** switches to the computer's input.

**Several remotes.** Up to four: the TV's, then the ones picked in « Autres télécommandes » in the blueprint (an Apple TV, a Freebox Player…). Each one has its page: their names at the top — tap one, or swipe left or right. On a box of the Apple TV integration (Apple TV, Freebox Player), **Stop** takes the place of Source, the volume has no mute, and the bottom row becomes **Previous**, **Rewind**, **Play / Pause**, **Forward**, **Next**; there are no app buttons.

The keys go through Home Assistant, to the remote of the page shown (« Télécommande de la TV » and « Autres télécommandes » in the blueprint, [other zones](../installation/adapt-to-your-home.md#other-zones)); the tablet has no infrared. The app buttons need a Samsung TV, picked in the « Tab5 · TV Samsung » list ([step 5](../installation/sources.md)). The **PC** key sequence is set for the author's TV, in the `tab5_tv.yaml` package.

---

## Version Française

---

**S'ouvre par** un appui long sur le bouton manette, en haut à droite de l'accueil, sur la carte de la TV choisie dans le blueprint ([rangée du bas](tiles.md#version-française)), ou par « TV » sous « Appareils » dans la [roue de navigation](home.md#carte-centrale-12) (appui long sur la carte centrale).

![La télécommande TV : Marche / Arrêt, Source, Menu, le pavé avec OK, le volume, les touches de lecture et les boutons d'applications](../images/notice/telecommande-tv-fr.webp)

- **Marche / Arrêt**, **Source**, **Menu** (à gauche).
- Le pavé rond : les quatre flèches et **OK** (au centre).
- **Lecture**, **Pause**, **Retour**, **Accueil**, à droite du pavé.
- La colonne du volume (à droite) : **+**, muet, **−**.
- La rangée du bas ouvre une application sur la TV : **Netflix**, **Prime**, **YouTube**, **CANAL+** ; **PC** passe sur l'entrée de l'ordinateur.

**Plusieurs télécommandes.** Jusqu'à quatre : celle de la TV, puis celles choisies dans « Autres télécommandes » du blueprint (un Apple TV, un Freebox Player…). Chacune a sa page : leurs noms en haut — touchez-en un, ou glissez à gauche ou à droite. Sur un boîtier de l'intégration Apple TV (Apple TV, Freebox Player), **Stop** prend la place de Source, le volume n'a pas de muet, et la rangée du bas devient **Précédent**, **Reculer**, **Lecture / Pause**, **Avancer**, **Suivant** ; pas de boutons d'applications.

Les touches passent par Home Assistant, vers la télécommande de la page montrée (« Télécommande de la TV » et « Autres télécommandes » dans le blueprint, [autres zones](../installation/adapt-to-your-home.md#autres-zones)) ; la tablette n'a pas d'infrarouge. Les boutons d'applications demandent une TV Samsung, choisie dans la liste « Tab5 · TV Samsung » ([étape 5](../installation/sources.md#version-française)). La suite de touches de **PC** est réglée pour la TV de l'auteur, dans le package `tab5_tv.yaml`.
