# Alarm clock

## English · [Français](#version-française)

---

**Opens with** a long press on the time, hours or minutes (a long press on the date opens the [calendar](calendar.md)); another gesture can be chosen in the blueprint ([home screen](home.md#clock-and-date-5)). The alarm rings even without Home Assistant: the time, the next days' work hours and the melody are on the tablet.

![The alarm clock window, Time page: the time on two wheels, the switch, the next alarm, Test](../images/notice/reveil-en.webp)

The window has **five pages**, named at the top next to the title: **Time**, **Days**, **Shift start**, **Ringtone**, **Announcements**. Tap a name, or slide the window left or right (it loops from the last page back to the first). It always opens on **Time**. Values are set on **wheels**: slide the numbers up or down, the one in the middle is kept.

**Time**

- **Alarm time**: the hours and the minutes (5 by 5) on two large wheels. A minute set otherwise in Home Assistant shows as it is.
- **Alarm off / on**: the large button arms it or turns it off.
- **Next alarm**: the day and time of the next ring, or why there is none.
- **Test** rings now, **■** stops the test.

**Days**

- **Mode**, with a reminder line below it:
    - **Fixed**: at the fixed time, on the checked days;
    - **Work**: at the fixed time, on the days your work calendar has work, whatever the weekday;
    - **Shift start**: before the start of the day's work (settings on the next page).
- **Days for the fixed time**: tap a day to check or uncheck it, or pick **Every day**, **Monday-Friday**, **Monday-Saturday** or **Weekend**.
- **Days off**: **Quiet** or **Fixed time**, what Work and Shift start do on a day off: nothing, or the fixed time if that day is checked.

Without calendar data, Work and Shift start ring at the fixed time: a ring too many costs less than a missed start.

**Shift start** (only for the Shift start mode)

- Four wheels: **Lead time** (before shift start), **Not before**, **Not after**, **Min rest** (the hours of rest after the previous evening's closing). « Not after » wins, so rest never makes you late. A sentence below sums up the result, or says the mode is not in use.

**Ringtone**

- **Melody**: four buttons; tapping one chooses it and plays it.
- **Volume**: the alarm's own volume, apart from the tablet's.
- **Gradual volume**: **Yes** (rises gently up to the set volume) or **No**.
- **Snooze** (between two rings) and **Max length** (then it stops by itself): two wheels.

**Announcements**

- **Spoken wake-up announcement**: **Yes** or **No**. 12 s into the first ring, if it still rings, a spoken briefing (time, today's work, next appointment, temperature); it needs Home Assistant.
- **Appointment announcements**: **Yes** or **No**, the reminder of timed appointments, on screen and by voice; the wheel sets how long before the appointment.
- **Next appointment**: the next one Home Assistant sent, on up to three lines.

**When it rings**

![The ring screen: time, Stop and Snooze · 9 min](../images/notice/reveil-sonnerie-en.webp)

- A touch anywhere stops it, or say « Stop », even with the wake word off.
- **Snooze**: it rings again after the snooze time.

The same settings are entities of the tablet in Home Assistant ([tablet settings](../installation/settings.md#alarm-clock-appointments-voice)); the bell of the status row gives its state ([home screen](home.md#clock-and-date-5)).

---

## Version Française

---

**S'ouvre par** un appui long sur l'heure, heures ou minutes (un appui long sur la date ouvre le [calendrier](calendar.md#version-française)) ; un autre geste peut se choisir dans le blueprint ([écran d'accueil](home.md#horloge-et-date-5)). Le réveil sonne même sans Home Assistant : l'heure, les heures de travail des jours qui viennent et la mélodie sont sur la tablette.

![La fenêtre du réveil, page Heure : l'heure sur deux rouleaux, l'interrupteur, la prochaine sonnerie, Tester](../images/notice/reveil-fr.webp)

La fenêtre a **cinq pages**, nommées en haut à côté du titre : **Heure**, **Jours**, **Ouverture**, **Sonnerie**, **Annonces**. Un tap sur un nom, ou un glissé de la fenêtre vers la gauche ou la droite (de la dernière page, on revient à la première). Elle s'ouvre toujours sur **Heure**. Les valeurs se règlent sur des **rouleaux** : faire glisser les chiffres vers le haut ou vers le bas, celui du milieu est retenu.

**Heure**

- **Heure du réveil** : les heures et les minutes (de 5 en 5) sur deux grands rouleaux. Une minute réglée autrement dans Home Assistant s'affiche telle quelle.
- **Réveil éteint / activé** : le grand bouton l'arme ou l'éteint.
- **Prochaine sonnerie** : le jour et l'heure de la prochaine, ou pourquoi il n'y en a pas.
- **Tester** sonne tout de suite, **■** arrête l'essai.

**Jours**

- **Mode**, avec une ligne de rappel dessous :
    - **Fixe** : à l'heure fixe, les jours cochés ;
    - **Travail** : à l'heure fixe, les jours où votre agenda de travail a du travail, quel que soit le jour de la semaine ;
    - **Ouverture** : avant le début du travail du jour (réglages à la page suivante).
- **Jours de l'heure fixe** : un tap coche ou décoche un jour, ou choisir **Tous les jours**, **Lundi-Vendredi**, **Lundi-Samedi** ou **Week-end**.
- **Jours de repos** : **Silence** ou **Heure fixe**, ce que font Travail et Ouverture un jour de repos : rien, ou l'heure fixe si ce jour est coché.

Sans données d'agenda, Travail et Ouverture sonnent à l'heure fixe : une sonnerie de trop coûte moins qu'une embauche ratée.

**Ouverture** (pour le mode Ouverture seulement)

- Quatre rouleaux : **Délai** (avant l'ouverture), **Pas avant**, **Pas après**, **Repos mini** (les heures de repos après la fermeture de la veille). « Pas après » l'emporte : le repos ne vous met jamais en retard. Une phrase dessous résume le résultat, ou dit que le mode n'est pas choisi.

**Sonnerie**

- **Mélodie** : quatre boutons ; un tap la choisit et la fait entendre.
- **Volume** : le volume propre au réveil, à part de celui de la tablette.
- **Volume progressif** : **Oui** (monte doucement jusqu'au volume réglé) ou **Non**.
- **Répétition** (entre deux sonneries) et **Durée max** (puis arrêt automatique) : deux rouleaux.

**Annonces**

- **Annonce parlée au réveil** : **Oui** ou **Non**. 12 s après le début de la première sonnerie, si elle sonne encore, un point du matin parlé (heure, travail du jour, prochain rendez-vous, température) ; il demande Home Assistant.
- **Annonce des rendez-vous** : **Oui** ou **Non**, le rappel des rendez-vous à heure fixe, à l'écran et à la voix ; le rouleau règle combien de temps avant le rendez-vous.
- **Prochain rendez-vous** : le prochain envoyé par Home Assistant, sur trois lignes au plus.

**Quand il sonne**

![L'écran de sonnerie : l'heure, Arrêter et Répéter · 9 min](../images/notice/reveil-sonnerie-fr.webp)

- Un toucher n'importe où l'arrête, ou dites « Stop », même mot de réveil coupé.
- **Répéter** : il resonne après le temps de répétition.

Les mêmes réglages sont des entités de la tablette dans Home Assistant ([réglages de la tablette](../installation/settings.md#réveil-rendez-vous-voix)) ; la cloche de la ligne d'état donne son état ([écran d'accueil](home.md#horloge-et-date-5)).
