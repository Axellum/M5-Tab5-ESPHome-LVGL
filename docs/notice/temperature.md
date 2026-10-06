# Temperature

## English · [Français](#version-française)

---

**Opens with** a long press on one of the two temperatures of the [home screen](home.md): the room's, or the second one (greenhouse or outdoors). A tap on the second one still opens the [Arcade](arcade.md).

![The temperature window of a greenhouse, 24 h view: now, minimum, maximum and the forecast, then the curve of the last 24 hours and the outdoor forecast after « Now »](../images/notice/temperature-serre-en.webp)

- **Top**: **Now** (and the average of the period), **Minimum** and **Maximum**, each with when it was reached. For the second temperature, a fourth card: the highest forecast temperature, and the lowest below it.
- **Bottom**: the curve, with the place and the period as title. **24 h** (hour by hour), **7 days** (every three hours), **30 days** (day by day). The line is the average; the pale bar behind it goes from the minimum to the maximum; the dot at the end is the temperature now.
- **Forecast**, for the second temperature only: in gold, on a tinted background after « Now ». A day-by-day forecast also has its minimum-maximum bar. If the blueprint says that temperature is outdoors (« La seconde température est dehors »), the forecast extends the curve and is named « Forecast »; otherwise (a greenhouse, another room) it stays apart, named « Outdoors, forecast ».

![The same window when the second temperature is outdoors: the forecast extends the curve](../images/notice/temperature-dehors-en.webp)

Home Assistant sends the curve only while the window is open, from its long-term statistics: the sensor needs a state class (thermometers have one), and the forecast is the weather entity picked for the tablet. Without statistics, the window says there is no history; before Home Assistant answers, it says it is waiting.

---

## Version Française

---

**S'ouvre par** un appui long sur l'une des deux températures de l'[écran d'accueil](home.md#version-française) : celle de la pièce, ou la seconde (serre ou dehors). Un tap sur la seconde ouvre toujours l'[Arcade](arcade.md#version-française).

![La fenêtre de la température d'une serre, vue 24 h : maintenant, minimum, maximum et la prévision, puis la courbe des dernières 24 heures et la prévision de dehors après « Maintenant »](../images/notice/temperature-serre-fr.webp)

- **En haut** : **Maintenant** (et la moyenne de la période), **Minimum** et **Maximum**, chacun avec le moment où il a été atteint. Pour la seconde température, une quatrième carte : la température prévue la plus haute, et la plus basse en dessous.
- **En bas** : la courbe, avec le lieu et la période pour titre. **24 h** (heure par heure), **7 jours** (toutes les trois heures), **30 jours** (jour par jour). La ligne est la moyenne ; la barre pâle derrière elle va du minimum au maximum ; le point au bout est la température du moment.
- **Prévision**, pour la seconde température seulement : en or, sur un fond teinté après « Maintenant ». Une prévision par jour a aussi sa barre du minimum au maximum. Si le blueprint dit que cette température est dehors (« La seconde température est dehors »), la prévision prolonge la courbe et s'appelle « Prévu » ; sinon (une serre, une autre pièce), elle reste à part, sous le nom « Dehors, prévu ».

![La même fenêtre quand la seconde température est dehors : la prévision prolonge la courbe](../images/notice/temperature-dehors-fr.webp)

Home Assistant n'envoie la courbe que pendant que la fenêtre est ouverte, depuis ses statistiques longue durée : le capteur doit avoir une classe d'état (les thermomètres en ont une), et la prévision est celle de l'entité météo choisie pour la tablette. Sans statistiques, la fenêtre dit qu'il n'y a pas d'historique ; avant que Home Assistant réponde, elle dit qu'elle attend.
