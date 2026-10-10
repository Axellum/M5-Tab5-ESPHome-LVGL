# AI server

## English · [Français](#version-française)

---

**Opens with** the « Serveur IA » entry of the tablet's « Aller à l'écran » list in Home Assistant, « Serveur IA » in the Agenda family of the navigation wheel (long press on the central card; offered only once Home Assistant has sent a server), or a tap or a long press of the clock or of a button top right when the blueprint gives it « Serveur IA (LLM local) · AI server (local LLM) » ([home screen](home.md#the-three-buttons-top-right-6-to-8)).

The window « Serveur IA » is the dashboard of a local LLM server (Ollama, llama.cpp, LM Studio, vLLM…):

- **Top band**: a dot, green when the server is online, grey when it is offline or unknown, with « En ligne », « Hors ligne » or « État inconnu »; the server's name (its device in Home Assistant); on the right, the loaded model, cut with « … » when it is too long, or « Aucun modèle chargé ».
- **Tokens per second**, large, and their curve: one point each time Home Assistant sends the server (two seconds apart at least), the last 24 — about the last minute while the server works. The curve is kept by the tablet and starts again after a restart. Below: requests running and queued.
- **VRAM**: used, « of » total and the percentage, with a bar.
- **GPU**: its temperature, in green when normal, orange when high, red when critical (thresholds in Home Assistant, 80 and 90 °C by default).
- **RAM**: in %, with a bar.
- **Power**: in W.

A value Home Assistant does not know shows « — », never a made-up 0.

**Buttons** in the top band, right of the model, when Home Assistant offers them (each one is set up in Home Assistant, see [Local AI server](../serveur-ia.md#actions-from-the-tablet)):

- **Décharger** (eject icon): unloads the loaded model, which frees the VRAM. Ollama and LM Studio only; greyed when no model is loaded.
- **Réveiller** (power icon): wakes the PC by Wake-on-LAN. Greyed while the server answers.
- **Redémarrer** (restart icon): runs the script or presses the button you picked in Home Assistant to restart the AI service.

« Décharger » and « Redémarrer » cut an answer in progress, so they ask a second tap: the first turns the button orange with « Confirmer ? » for 4 s; tap again within that time to send, or do nothing and it comes back. « Réveiller » leaves at the first tap. Once sent, the button says « Envoyé » in green for 5 s, and no other button sends during that time. A greyed button does nothing. With nothing set up in Home Assistant, there is no button and the band is as before.

**Setting up** in Home Assistant: the `tab5_serveur_ia.yaml` package of this version, then a sensor in each list « Tab5 · serveur IA, … · AI server, … » (status, model, tokens per second, running and queued requests, VRAM used and total, GPU temperature, RAM, power). Each list offers only the sensors that fit (a temperature for the GPU, a power for the power…); leave « Aucun » for what you do not have. The sensors can come from anywhere: the `tab5_llm.yaml` package for the server itself, Glances or System Monitor for the machine, a metered plug for the power. With every list on « Aucun », the window says where to choose them.

Like every window, it closes with its **×**, a tap on the dark area around it, or after 45 s without a touch.

---

## Version Française

---

**S'ouvre par** l'entrée « Serveur IA » de la liste « Aller à l'écran » de la tablette dans Home Assistant, « Serveur IA » dans la famille Agenda de la roue de navigation (appui long sur la carte centrale ; proposé seulement quand Home Assistant a envoyé un serveur), ou un tap ou un appui long sur l'horloge ou un bouton en haut à droite quand le blueprint lui donne « Serveur IA (LLM local) · AI server (local LLM) » ([écran d'accueil](home.md#les-trois-boutons-en-haut-à-droite-6-à-8)).

La fenêtre « Serveur IA » est le tableau de bord d'un serveur de LLM local (Ollama, llama.cpp, LM Studio, vLLM…) :

- **Bandeau du haut** : une pastille, verte quand le serveur est en ligne, grise hors ligne ou inconnu, avec « En ligne », « Hors ligne » ou « État inconnu » ; le nom du serveur (son appareil dans Home Assistant) ; à droite, le modèle chargé, coupé par « … » s'il est trop long, ou « Aucun modèle chargé ».
- **Tokens par seconde**, en grand, et leur courbe : un point à chaque envoi de Home Assistant (deux secondes d'écart au moins), les 24 derniers — environ la dernière minute quand le serveur travaille. La courbe est gardée par la tablette et repart de zéro après un redémarrage. En dessous : les requêtes en cours et en file.
- **VRAM** : utilisée, « sur » la totale et le pourcentage, avec une barre.
- **GPU** : sa température, en vert quand elle est normale, en orange élevée, en rouge critique (seuils dans Home Assistant, 80 et 90 °C par défaut).
- **RAM** : en %, avec une barre.
- **Puissance** : en W.

Une valeur que Home Assistant ne connaît pas s'écrit « — », jamais un 0 inventé.

**Boutons** dans le bandeau du haut, à droite du modèle, quand Home Assistant les offre (chacun se règle dans Home Assistant, voir [Serveur IA local](../serveur-ia.md#actions-depuis-la-tablette)) :

- **Décharger** (icône d'éjection) : décharge le modèle chargé, ce qui libère la VRAM. Ollama et LM Studio seulement ; grisé quand aucun modèle n'est chargé.
- **Réveiller** (icône marche) : réveille le PC par Wake-on-LAN. Grisé tant que le serveur répond.
- **Redémarrer** (icône de redémarrage) : lance le script ou appuie sur le bouton choisi dans Home Assistant pour redémarrer le service IA.

« Décharger » et « Redémarrer » coupent une réponse en cours : ils demandent un second tap. Le premier passe le bouton en orange avec « Confirmer ? » pendant 4 s ; retaper dans ce délai pour envoyer, ou ne rien faire et il revient. « Réveiller » part au premier tap. Une fois envoyé, le bouton dit « Envoyé » en vert pendant 5 s, et aucun autre bouton n'envoie pendant ce temps. Un bouton grisé ne fait rien. Sans rien de réglé dans Home Assistant, il n'y a aucun bouton et le bandeau reste comme avant.

**Réglage** dans Home Assistant : le package `tab5_serveur_ia.yaml` de cette version, puis un capteur dans chaque liste « Tab5 · serveur IA, … · AI server, … » (état, modèle, tokens par seconde, requêtes en cours et en file, VRAM utilisée et totale, température du GPU, RAM, puissance). Chaque liste ne propose que les capteurs qui conviennent (une température pour le GPU, une puissance pour la puissance…) ; laisser « Aucun » pour ce que vous n'avez pas. Les capteurs peuvent venir de partout : le package `tab5_llm.yaml` pour le serveur lui-même, Glances ou System Monitor pour la machine, une prise mesurée pour la puissance. Avec toutes les listes sur « Aucun », la fenêtre dit où les choisir.

Comme toute fenêtre, elle se ferme par sa **×**, un tap sur la zone sombre autour, ou après 45 s sans toucher.
