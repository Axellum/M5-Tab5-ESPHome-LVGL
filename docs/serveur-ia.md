# Local AI server

## English · [Français](#version-française)

---

Follow a local inference server — **Ollama**, **llama.cpp** (`llama-server`) or **LM Studio** — from Home Assistant, and put its numbers on the tablet's [Tracking](notice/tracking.md) window. Nothing changes in the firmware: the `tab5_llm.yaml` package creates the sensors, Home Assistant reads the server, the tablet only receives what the Tracking window already pushes.

## Setting up

1. Copy `HomeAssistant_Config/packages/tab5_llm.yaml` with the other packages ([step 1 of the installation](installation/home-assistant-files.md)) and reload, or restart Home Assistant.
2. In **Settings → Devices & services → Entities**, search « Tab5 · » and set:
    - « Tab5 · type de serveur IA · AI server type »: Ollama, llama.cpp or LM Studio. « Aucun » (the default) sends no request at all and leaves the sensors unavailable.
    - « Tab5 · adresse du serveur IA · AI server address »: `http://host:port` of the server, without a path. Default ports: 11434 (Ollama), 8080 for `llama-server` (9931 on recent builds: its port is changing, give it `--port` to be sure), 1234 (LM Studio). Without `http://`, it is added.
3. Home Assistant must reach the server over the network: see [Server side](#server-side).
4. To see a number on the tablet, pick its sensor in « Tab5 · capteurs suivis · tracked sensors ».

Every 30 seconds, and only when a server is chosen, Home Assistant asks the server (5 s at most per request). The tablet asks nothing: it stays push-only.

## The sensors

| Sensor | Ollama | llama.cpp | LM Studio |
|---|---|---|---|
| `binary_sensor.tab5_serveur_ia_en_ligne` — online | `/api/ps` answers | `/health` answers 200, or 503 while the model loads | `/api/v1/models` answers |
| `sensor.tab5_serveur_ia_modele` — loaded model(s), « aucun » when none | names from `/api/ps` | file name of `model_path` (`/props`) | `display_name` of the models with a loaded instance |
| `sensor.tab5_serveur_ia_vram` — VRAM used, GiB | sum of `size_vram` | — | — |
| `sensor.tab5_serveur_ia_vitesse` — generation speed, tokens/s | — | with `--metrics` | — |
| `sensor.tab5_serveur_ia_requetes` — requests in progress | — | `requests_processing` (`/metrics`), else the busy slots of `/slots` | — |

« — » means the server does not give it: the sensor stays unavailable rather than showing an invented number.

- **Ollama has no global tokens/s counter**: its speed only exists in the answer to one request (`eval_count / eval_duration`). Measuring it would mean sending requests to the server, which this package never does.
- **LM Studio** (0.4 or later for `/api/v1`) lists the loaded models but gives neither memory nor speed outside an answer.
- **llama.cpp speed**: the tokens generated between two readings divided by the time spent generating them (counters `llamacpp:tokens_predicted_total` and `llamacpp:tokens_predicted_seconds_total`). When nothing was generated since the last reading, the last speed stays. After a restart of the server, the sensor waits for a generation.
- `sensor.tab5_serveur_ia_releve` holds everything read (state `ok`, `chargement`, `hors_ligne`, `erreur` or `aucun`, the reason in its `raison` attribute, the raw values in its other attributes): look there first when a sensor stays unavailable.

The three measures (VRAM, speed, requests) have `state_class: measurement`, so they are offered in « Tab5 · capteurs suivis » and Home Assistant keeps their statistics.

## On the tablet: what the Tracking window can and cannot do

The [Tracking](notice/tracking.md) window shows a trend, not a live meter:

- Home Assistant pushes a change at once, then **at most once every five minutes**, and every hour. A speed that changes every second shows as its value at those moments.
- The curve is the **hourly means of the last 24 hours**, scaled between their lowest and highest values: it shows whether the VRAM or the speed went up or down, not its absolute level.
- Six sensors at most, shared with whatever else you track.

That suits temperatures, VRAM and RAM over a day. A live view (tokens/s every second or two, the loaded model, the queue) needs a dedicated window and a faster push: it is not part of this package, it is the [AI server](notice/ai-server.md) window's, fed by `packages/tab5_serveur_ia.yaml` — pick these sensors in its « Tab5 · serveur IA, … » lists.

## Hardware: GPU, CPU, RAM, power

This package creates no hardware sensor. Use a Home Assistant integration made for that and pick its sensors in « Tab5 · capteurs suivis »:

- **Glances** (Glances running in web server mode on the AI machine, its REST API not exposed outside your network): CPU and RAM use, disks, temperatures, and for an NVIDIA GPU (`py3nvml` package on that machine) its memory use (VRAM, %), load, temperature and fan. These sensors have `state_class: measurement`, so they are offered in the list.
- **System Monitor**: only the machine Home Assistant runs on — useful when the AI server is that same machine. Its entities are disabled by default: enable the ones you want first.
- A smart plug with power measurement gives the machine's power in W.

## Server side

- The server must listen on the network, not only on `127.0.0.1`, and Home Assistant must reach its port. **Ollama** and **llama-server** listen on `127.0.0.1` by default: set the `OLLAMA_HOST` environment variable (for example `0.0.0.0:11434`) or start `llama-server` with `--host 0.0.0.0`. **LM Studio**: turn on « Serve on Local Network » in the server settings.
- **Server protected by an API key: not supported** (LM Studio: leave « Require Authentication » off). A public package holds no key. `llama-server` answers `/health` without its key, so the « online » sensor works, but `/props`, `/metrics` and `/slots` refuse the request.
- **llama.cpp**: start `llama-server` with `--metrics` for the speed and the requests. In router mode (several models), `/props` and `/metrics` need a model name: not supported, only « online » works.
- Requests go over plain `http://` by default; an `https://` address with a valid certificate works too.

## Actions from the tablet

The [AI server](notice/ai-server.md) window can show three buttons ([ADR-0060](decisions/0060-ai-server-actions.md)). Each one appears only once it is set up here; with nothing set up, the window has no button.

| Button | Set up | Greyed when | What Home Assistant does |
|---|---|---|---|
| « Décharger » — unload the model | server type Ollama or LM Studio, with an address | no model is loaded, or the last reading is not `ok` | Ollama: `POST /api/generate` with `{"model": …, "keep_alive": 0}` for each model of `/api/ps`. LM Studio: `POST /api/v1/models/unload` with `{"instance_id": …}` for each loaded instance of `/api/v1/models`. Eight at most. |
| « Réveiller » — wake the PC | a MAC address in « Tab5 · adresse MAC du serveur IA · AI server MAC address » (`AA:BB:CC:DD:EE:FF`) | the server answers (`ok` or `chargement`) | `wake_on_lan.send_magic_packet` to that address |
| « Redémarrer » — restart the service | a script or a button picked in « Tab5 · serveur IA, redémarrage · AI server, restart » | never | `script.turn_on` or `button.press` on it |

- Nothing with the server type « Aucun ». The list of what is offered is `sensor.tab5_serveur_ia_actions` (`decharger`, `-reveiller`… — a `-` means greyed): look there first when a button is missing.
- **llama.cpp: no « Décharger ».** A single-model `llama-server` cannot unload its model without stopping; its `/models/unload` exists only in router mode, which this package does not support. « Réveiller » and « Redémarrer » work with any type.
- **Restart: you bring the command.** No server has an endpoint to restart itself. Write a Home Assistant script that does it in your home — a `shell_command` or SSH command restarting a systemd unit, the restart button of a Docker or add-on integration, a smart plug turned off and on — and pick it in the list. The list offers every script and button except the project's own (`script.tab5_*`) and the tablet's.
- **Wake-on-LAN**: the package loads the Wake on LAN integration (an empty `wake_on_lan:` key; one already in `configuration.yaml` is no duplicate). A new integration is not loaded by a YAML reload: restart Home Assistant once, unless `wake_on_lan:` was already there or the « Tab5 » integration installed the files (it loads it at once). The PC must allow it (BIOS / UEFI and network card settings) and be on the same network segment as Home Assistant: the magic packet goes out as a broadcast, which a router does not pass on.
- **Unloading cuts an answer in progress**, and so does a restart: on the tablet, both ask a second tap (« Confirmer ? », 4 s).
- The tablet sends only the event `esphome.tab5_serveur_ia_action` with `action` = `decharger`, `reveiller` or `redemarrer`. `script.tab5_serveur_ia_action` runs it only if `sensor.tab5_serveur_ia_actions` offers it as active at that moment, takes the model names, address, MAC and target from Home Assistant (never from the tablet), then waits 5 s: **one action every 5 s at most**, the others are ignored. You can also run the script by hand (Developer tools → Actions, field « Action »). A server that does not answer is not an error: the script's trace keeps the answer.
- After an action, the reading starts again at once (event `tab5_serveur_ia_relever`): an unloaded model leaves the window within a few seconds.

## What was tested

- The templates of the package are rendered by `tests/test_serveur_ia.py` on simulated answers of the three servers (shapes from their official documentation) and compared to a separate Python calculation; on the author's Home Assistant, the same templates were rendered by its template engine.
- **Real servers**: the answer of `/api/ps` of a real Ollama 0.30.6 (no model loaded at that time) was read. **llama.cpp and LM Studio were not tested against a real server.** The package itself had not yet run in a Home Assistant talking to a server when it was published.
- **Actions**: the offered buttons and the script are checked by `tests/test_serveur_ia_actions.py` (rendered on simulated states, only the allowed actions called, guard and 5 s cadence). **No action was sent to a real server** when they were written: no model unloaded, no magic packet sent, no restart. Ollama's `keep_alive: 0` and LM Studio's unload follow their official documentation.

---

## Version Française

---

Suivre un serveur d'inférence local — **Ollama**, **llama.cpp** (`llama-server`) ou **LM Studio** — depuis Home Assistant, et mettre ses chiffres dans la fenêtre [Suivi](notice/tracking.md#version-française) de la tablette. Rien ne change dans le firmware : le package `tab5_llm.yaml` crée les capteurs, Home Assistant interroge le serveur, la tablette reçoit seulement ce que la fenêtre Suivi pousse déjà.

## Mise en place

1. Copier `HomeAssistant_Config/packages/tab5_llm.yaml` avec les autres packages ([étape 1 de l'installation](installation/home-assistant-files.md#version-française)) et recharger, ou redémarrer Home Assistant.
2. Dans **Paramètres → Appareils et services → Entités**, chercher « Tab5 · » et régler :
    - « Tab5 · type de serveur IA · AI server type » : Ollama, llama.cpp ou LM Studio. « Aucun » (le choix par défaut) n'envoie aucune requête et laisse les capteurs indisponibles.
    - « Tab5 · adresse du serveur IA · AI server address » : `http://hôte:port` du serveur, sans chemin. Ports par défaut : 11434 (Ollama), 8080 pour `llama-server` (9931 sur les versions récentes : son port change, lui donner `--port` pour être sûr), 1234 (LM Studio). Sans `http://`, il est ajouté.
3. Home Assistant doit joindre le serveur sur le réseau : voir [Côté serveur](#côté-serveur).
4. Pour voir une valeur sur la tablette, choisir son capteur dans « Tab5 · capteurs suivis · tracked sensors ».

Toutes les 30 secondes, et seulement quand un serveur est choisi, Home Assistant interroge le serveur (5 s au plus par requête). La tablette ne demande rien : elle reste en push-only.

## Les capteurs

| Capteur | Ollama | llama.cpp | LM Studio |
|---|---|---|---|
| `binary_sensor.tab5_serveur_ia_en_ligne` — en ligne | `/api/ps` répond | `/health` répond 200, ou 503 pendant le chargement du modèle | `/api/v1/models` répond |
| `sensor.tab5_serveur_ia_modele` — modèle(s) chargé(s), « aucun » sinon | noms de `/api/ps` | nom du fichier de `model_path` (`/props`) | `display_name` des modèles qui ont une instance chargée |
| `sensor.tab5_serveur_ia_vram` — VRAM utilisée, Gio | somme des `size_vram` | — | — |
| `sensor.tab5_serveur_ia_vitesse` — vitesse de génération, tokens/s | — | avec `--metrics` | — |
| `sensor.tab5_serveur_ia_requetes` — requêtes en cours | — | `requests_processing` (`/metrics`), sinon les slots occupés de `/slots` | — |

« — » : le serveur ne le donne pas ; le capteur reste indisponible plutôt que d'afficher un chiffre inventé.

- **Ollama n'a pas de compteur global de tokens/s** : sa vitesse n'existe que dans la réponse à une requête (`eval_count / eval_duration`). La mesurer obligerait à envoyer des requêtes au serveur, ce que ce package ne fait jamais.
- **LM Studio** (0.4 et plus pour `/api/v1`) liste les modèles chargés mais ne donne ni la mémoire ni la vitesse hors d'une réponse.
- **Vitesse de llama.cpp** : les tokens générés entre deux relevés divisés par le temps passé à les générer (compteurs `llamacpp:tokens_predicted_total` et `llamacpp:tokens_predicted_seconds_total`). Sans génération depuis le relevé précédent, la dernière vitesse reste. Après un redémarrage du serveur, le capteur attend une génération.
- `sensor.tab5_serveur_ia_releve` garde tout ce qui a été lu (état `ok`, `chargement`, `hors_ligne`, `erreur` ou `aucun`, la raison dans son attribut `raison`, les valeurs brutes dans ses autres attributs) : le regarder d'abord quand un capteur reste indisponible.

Les trois mesures (VRAM, vitesse, requêtes) ont `state_class: measurement` : elles sont proposées dans « Tab5 · capteurs suivis » et Home Assistant garde leurs statistiques.

## Sur la tablette : ce que la fenêtre Suivi sait faire, et ce qu'elle ne sait pas

La fenêtre [Suivi](notice/tracking.md#version-française) montre une tendance, pas un compteur en direct :

- Home Assistant pousse un changement tout de suite, puis **au plus une fois toutes les cinq minutes**, et toutes les heures. Une vitesse qui change chaque seconde s'y voit à ces instants seulement.
- La courbe est faite des **moyennes horaires des 24 dernières heures**, ramenées entre leur plus petite et leur plus grande valeur : elle dit si la VRAM ou la vitesse a monté ou baissé, pas son niveau absolu.
- Six capteurs au plus, partagés avec tout ce que vous suivez d'autre.

Cela convient aux températures, à la VRAM et à la RAM sur une journée. Une vue en direct (tokens/s toutes les une ou deux secondes, modèle chargé, file d'attente) demande une fenêtre à elle et une poussée plus rapide : ce n'est pas l'objet de ce package, c'est celui de la fenêtre [Serveur IA](notice/ai-server.md#version-française), nourrie par `packages/tab5_serveur_ia.yaml` — choisir ces capteurs dans ses listes « Tab5 · serveur IA, … ».

## Matériel : GPU, processeur, RAM, puissance

Ce package ne crée aucun capteur matériel. Prendre une intégration de Home Assistant faite pour cela et choisir ses capteurs dans « Tab5 · capteurs suivis » :

- **Glances** (Glances lancé en mode serveur web sur la machine IA, son API REST non exposée hors de votre réseau) : processeur et RAM utilisés, disques, températures et, pour un GPU NVIDIA (paquet `py3nvml` sur cette machine), sa mémoire utilisée (VRAM, %), sa charge, sa température et son ventilateur. Ces capteurs ont `state_class: measurement` : ils sont proposés dans la liste.
- **System Monitor** : seulement la machine où tourne Home Assistant — utile quand le serveur IA est cette même machine. Ses entités sont désactivées par défaut : activer d'abord celles que l'on veut.
- Une prise connectée qui mesure la puissance donne celle de la machine, en W.

## Côté serveur

- Le serveur doit écouter sur le réseau, pas seulement sur `127.0.0.1`, et Home Assistant doit joindre son port. **Ollama** et **llama-server** écoutent sur `127.0.0.1` par défaut : régler la variable d'environnement `OLLAMA_HOST` (par exemple `0.0.0.0:11434`) ou lancer `llama-server` avec `--host 0.0.0.0`. **LM Studio** : activer « Serve on Local Network » dans les réglages du serveur.
- **Serveur protégé par une clé d'API : non pris en charge** (LM Studio : laisser « Require Authentication » désactivé). Un package public ne contient aucune clé. `llama-server` répond à `/health` sans sa clé, donc le capteur « en ligne » marche, mais `/props`, `/metrics` et `/slots` refusent la requête.
- **llama.cpp** : lancer `llama-server` avec `--metrics` pour la vitesse et les requêtes. En mode routeur (plusieurs modèles), `/props` et `/metrics` demandent un nom de modèle : non pris en charge, seul « en ligne » marche.
- Les requêtes partent en `http://` simple par défaut ; une adresse `https://` avec un certificat valide marche aussi.

## Actions depuis la tablette

La fenêtre [Serveur IA](notice/ai-server.md#version-française) peut montrer trois boutons ([ADR-0060](decisions/0060-ai-server-actions.md)). Chacun n'apparaît qu'une fois réglé ici ; sans aucun réglage, la fenêtre n'a pas de bouton.

| Bouton | Réglage | Grisé quand | Ce que fait Home Assistant |
|---|---|---|---|
| « Décharger » — décharger le modèle | type de serveur Ollama ou LM Studio, avec une adresse | aucun modèle chargé, ou le dernier relevé n'est pas `ok` | Ollama : `POST /api/generate` avec `{"model": …, "keep_alive": 0}` pour chaque modèle de `/api/ps`. LM Studio : `POST /api/v1/models/unload` avec `{"instance_id": …}` pour chaque instance chargée de `/api/v1/models`. Huit au plus. |
| « Réveiller » — réveiller le PC | une adresse MAC dans « Tab5 · adresse MAC du serveur IA · AI server MAC address » (`AA:BB:CC:DD:EE:FF`) | le serveur répond (`ok` ou `chargement`) | `wake_on_lan.send_magic_packet` vers cette adresse |
| « Redémarrer » — redémarrer le service | un script ou un bouton choisi dans « Tab5 · serveur IA, redémarrage · AI server, restart » | jamais | `script.turn_on` ou `button.press` dessus |

- Rien avec le type de serveur « Aucun ». La liste de ce qui est offert est `sensor.tab5_serveur_ia_actions` (`decharger`, `-reveiller`… — un `-` veut dire grisé) : le regarder d'abord quand un bouton manque.
- **llama.cpp : pas de « Décharger ».** Un `llama-server` à un seul modèle ne peut pas décharger son modèle sans s'arrêter ; son `/models/unload` n'existe qu'en mode routeur, que ce package ne prend pas en charge. « Réveiller » et « Redémarrer » marchent avec tous les types.
- **Redémarrer : la commande, c'est vous qui l'apportez.** Aucun serveur n'a d'adresse pour se redémarrer. Écrire un script Home Assistant qui le fait chez vous — un `shell_command` ou une commande SSH qui redémarre un service systemd, le bouton de redémarrage d'une intégration Docker ou d'un add-on, une prise coupée puis rallumée — et le choisir dans la liste. La liste propose tous les scripts et boutons, sauf ceux du projet (`script.tab5_*`) et ceux de la tablette.
- **Wake-on-LAN** : le package charge l'intégration Wake on LAN (une clé vide `wake_on_lan:` ; une autre déjà dans `configuration.yaml` ne fait pas doublon). Une nouvelle intégration ne se charge pas par un rechargement du YAML : redémarrer Home Assistant une fois, sauf si `wake_on_lan:` y était déjà ou si l'intégration « Tab5 » a posé les fichiers (elle le charge tout de suite). Le PC doit l'autoriser (réglages du BIOS / UEFI et de la carte réseau) et être sur le même segment de réseau que Home Assistant : le paquet magique part en diffusion (broadcast), qu'un routeur ne fait pas suivre.
- **Décharger coupe une réponse en cours**, redémarrer aussi : sur la tablette, les deux demandent un second tap (« Confirmer ? », 4 s).
- La tablette n'envoie que l'événement `esphome.tab5_serveur_ia_action` avec `action` = `decharger`, `reveiller` ou `redemarrer`. `script.tab5_serveur_ia_action` ne l'exécute que si `sensor.tab5_serveur_ia_actions` l'offre comme active à cet instant, prend les noms des modèles, l'adresse, la MAC et la cible dans Home Assistant (jamais dans la tablette), puis attend 5 s : **une action toutes les 5 s au plus**, les autres sont ignorées. On peut aussi lancer le script à la main (Outils de développement → Actions, champ « Action »). Un serveur qui ne répond pas n'est pas une erreur : la trace du script garde la réponse.
- Après une action, le relevé repart tout de suite (événement `tab5_serveur_ia_relever`) : un modèle déchargé quitte la fenêtre en quelques secondes.

## Ce qui a été testé

- Les modèles du package sont rendus par `tests/test_serveur_ia.py` sur des réponses simulées des trois serveurs (formes de leur documentation officielle) et comparés à un calcul Python écrit à part ; sur le Home Assistant de l'auteur, les mêmes modèles ont été rendus par son moteur de modèles.
- **Vrais serveurs** : la réponse de `/api/ps` d'un vrai Ollama 0.30.6 (aucun modèle chargé à ce moment-là) a été lue. **llama.cpp et LM Studio n'ont pas été testés contre un vrai serveur.** Le package lui-même n'avait pas encore tourné dans un Home Assistant relié à un serveur au moment de sa publication.
- **Actions** : les boutons offerts et le script sont vérifiés par `tests/test_serveur_ia_actions.py` (rendus sur des états simulés, seules les actions permises appelées, garde et cadence de 5 s). **Aucune action n'a été envoyée à un vrai serveur** au moment de leur écriture : aucun modèle déchargé, aucun paquet magique envoyé, aucun redémarrage. Le `keep_alive: 0` d'Ollama et le déchargement de LM Studio suivent leur documentation officielle.
