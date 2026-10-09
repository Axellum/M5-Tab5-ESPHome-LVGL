# Weather

## English · [Français](#version-française)

---

**Opens with** a long press on a weather card of the bottom row that holds no device, with the « Météo » entry of the tablet's « Aller à l'écran » list in Home Assistant, or with a tap or a long press of the clock or of a button top right when the blueprint gives it « Météo · Weather » ([home screen](home.md#the-three-buttons-top-right-6-to-8)).

The weather in graphs, on three pages. Their names are at the top, next to the title, the one shown in colour: **tap a name**, or **swipe** left or right anywhere in the window, to change page (the last one leads back to the first).

- **Today**: on top, now: the weather icon, the temperature in its colour, the weather in words (« Éclaircies », « Pluie »…), the low and high of the day, and on the right the rain expected over the hours shown below. Under it, the next hours (up to 15): the hour, its icon, a curve of the temperatures with each value in its colour, and a bar with the millimetres where it rains. The current hour is on a lightly tinted column; where the hours pass midnight, a thin line and « Demain » (tomorrow).
- **10 days**: one row per day from today (« Aujourd'hui », « Demain », then « Mer 17 »…): its icon, the low, a bar from the low to the high, and the high. All the bars share the same scale: a warm day sits to the right, a cold one to the left, and the bar goes from the colour of the low to the colour of the high. A dot on today's row marks the temperature now.
- **Details**: the rain in the next hour, from now to 60 min, in bars from light to very heavy against dashed level lines (the same bars as the central card, on a time scale), with the central card's sentence on the right (« Averses dans 12 mn »…). Under it four cards: humidity (dry, comfortable or humid air), the UV index with its level (low to extreme), and the probabilities of frost and snow.

Everything comes from what Home Assistant already sends to the home page: nothing to set up. A value Home Assistant has not sent yet shows « -- », and a page without any data says « En attente de Home Assistant » (waiting for Home Assistant). The window follows the data while it is open. The hours past the tenth need the Home Assistant files of this version. Like every window, it closes with its **×**, a tap on the dark area around it, or after 45 s without a touch.

---

## Version Française

---

**S'ouvre par** un appui long sur une carte météo de la rangée du bas qui ne porte pas d'appareil, par l'entrée « Météo » de la liste « Aller à l'écran » de la tablette dans Home Assistant, ou par un tap ou un appui long sur l'horloge ou un bouton en haut à droite quand le blueprint lui donne « Météo · Weather » ([écran d'accueil](home.md#les-trois-boutons-en-haut-à-droite-6-à-8)).

La météo en graphiques, sur trois pages. Leurs noms sont en haut, à côté du titre, celle affichée en couleur : **tap sur un nom**, ou **glisser** vers la gauche ou la droite n'importe où dans la fenêtre, pour changer de page (la dernière ramène à la première).

- **Aujourd'hui** : en haut, le moment : l'icône météo, la température dans sa couleur, le temps en mots (« Éclaircies », « Pluie »…), le minimum et le maximum du jour, et à droite la pluie attendue sur les heures montrées dessous. Dessous, les heures qui viennent (jusqu'à 15) : l'heure, son icône, une courbe des températures avec chaque valeur dans sa couleur, et une barre avec les millimètres là où il pleut. L'heure en cours est sur une colonne légèrement teintée ; là où les heures passent minuit, un trait fin et « Demain ».
- **10 jours** : une ligne par jour à partir d'aujourd'hui (« Aujourd'hui », « Demain », puis « Mer 17 »…) : son icône, le minimum, une barre du minimum au maximum, et le maximum. Toutes les barres partagent la même échelle : un jour chaud est à droite, un jour froid à gauche, et la barre va de la couleur du minimum à celle du maximum. Un point sur la ligne d'aujourd'hui marque la température du moment.
- **Détails** : la pluie dans l'heure, de maintenant à 60 min, en barres de faible à très forte devant des lignes de niveau pointillées (les mêmes barres que la carte centrale, à l'échelle du temps), avec la phrase de la carte centrale à droite (« Averses dans 12 mn »…). Dessous, quatre cartes : l'humidité (air sec, confortable ou humide), l'indice UV avec son niveau (faible à extrême), et les probabilités de gel et de neige.

Tout vient de ce que Home Assistant envoie déjà à la page d'accueil : rien à régler. Une valeur que Home Assistant n'a pas encore envoyée affiche « -- », et une page sans aucune donnée dit « En attente de Home Assistant ». La fenêtre suit les données pendant qu'elle est ouverte. Les heures au-delà de la dixième demandent les fichiers Home Assistant de cette version. Comme toute fenêtre, elle se ferme par sa **×**, un tap sur la zone sombre autour, ou après 45 s sans toucher.
