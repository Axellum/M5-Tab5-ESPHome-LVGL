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

## What was tested

- The templates of the package are rendered by `tests/test_serveur_ia.py` on simulated answers of the three servers (shapes from their official documentation) and compared to a separate Python calculation; on the author's Home Assistant, the same templates were rendered by its template engine.
- **Real servers**: the answer of `/api/ps` of a real Ollama 0.30.6 (no model loaded at that time) was read. **llama.cpp and LM Studio were not tested against a real server.** The package itself had not yet run in a Home Assistant talking to a server when it was published.

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

## Ce qui a été testé

- Les modèles du package sont rendus par `tests/test_serveur_ia.py` sur des réponses simulées des trois serveurs (formes de leur documentation officielle) et comparés à un calcul Python écrit à part ; sur le Home Assistant de l'auteur, les mêmes modèles ont été rendus par son moteur de modèles.
- **Vrais serveurs** : la réponse de `/api/ps` d'un vrai Ollama 0.30.6 (aucun modèle chargé à ce moment-là) a été lue. **llama.cpp et LM Studio n'ont pas été testés contre un vrai serveur.** Le package lui-même n'avait pas encore tourné dans un Home Assistant relié à un serveur au moment de sa publication.
