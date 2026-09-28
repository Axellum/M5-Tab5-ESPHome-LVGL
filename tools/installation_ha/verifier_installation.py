# -*- coding: utf-8 -*-
"""tools/installation_ha/verifier_installation.py — Installer le Tab5 dans un Home Assistant neuf, sans
matériel, comme un nouvel utilisateur, puis vérifier que tout marche.

[AI-CONTEXT]
@role Seconde moitié du job « installation dans un HA neuf »
      (.github/workflows/installation-ha.yml). Le conteneur Home Assistant tourne déjà sur
      le dossier écrit par preparer_config.py ; ce script lance la tablette virtuelle (le rendu
      hors tablette compilé sous le nom de la vraie, tab5-rendu-host.yaml), puis fait ce
      que docs/installation.md (« Sans compiler », dans cet ordre) fait faire à la souris :
        1. créer le compte (onboarding), fuseau Europe/Paris ; un agenda de travail
           (Calendrier local) ; le blueprint est déjà dans config/ (preparer_config.py) ;
        2. ajouter la tablette (ESPHome, hôte + port), puis cocher « Autoriser
           l'appareil à effectuer des actions Home Assistant » ;
        3. créer l'automatisation du blueprint « Tab5 — emplacements », avec des entités
           de test (emplacements 3.x, deux pièces, une personnalisation, ADR-0023) ;
           puis redémarrer la tablette.
      et vérifie : clé API créée par HA et gardée (jamais affichée), refus de la clé
      nulle et du clair une fois la clé posée, esphome.tab5_connected reçu APRÈS la clé,
      poussée complète terminée sans erreur ; puis, après le redémarrage (la clé
      persiste, HA se reconnecte en chiffré), traces du blueprint et de la poussée sans
      erreur, zones masquées renvoyées par la tablette, capture d'écran demandée PAR HA
      (action esphome.tab5_ha_hmi_rendu_capture), journal de HA sans erreur Tab5 ;
      pièces : définitions et états des tuiles calculés à la connexion (relus dans la
      trace), tab5_maj_tuiles jamais appelée si la tablette est en dessous de 3.2.0
      (protocole 1), appelée avec les définitions sinon. Ce que
      montre l'écran entre la création de l'automatisation et le redémarrage est
      rapporté (capture 1), sans faire échouer.
@contraintes Le mot de passe du compte est tiré au hasard ici et n'est jamais affiché ;
      la clé API est lue dans .storage par `docker exec` (fichiers de root dans le
      conteneur) et n'est jamais affichée non plus, seulement sa longueur.
@ai_instruction Un échec doit dire QUOI et OÙ regarder (trace, journal) : c'est le
      seul retour d'un job sans écran. Les vérifications non fatales s'accumulent dans
      le rapport (résumé du job) ; une étape sans laquelle la suite n'a pas de sens
      lève Echec.

Usage (voir le workflow) :
    python tools/installation_ha/verifier_installation.py --programme .esphome/build/tab5-ha-hmi/.../program \\
        --prefs "$RUNNER_TEMP/prefs" --captures captures --conteneur homeassistant
"""
from __future__ import annotations

import argparse
import asyncio
import base64
import datetime as dt
import json
import logging
import os
import re
import secrets
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

logger = logging.getLogger("installation_ha")

URL_HA = "http://127.0.0.1:8123"
# client_id du compte : l'adresse de HA, comme le fait son interface.
CLIENT_ID = "http://127.0.0.1:8123/"
HOTE_TABLETTE = "127.0.0.1"
PORT_API = 6053
FUSEAU = "Europe/Paris"

# Nom de l'appareil ESPHome (tab5-ha-hmi.yaml) et ce qu'en tire HA : préfixe des actions
# (esphome.<nom avec _>_<action>, écrit en dur dans les packages) et des entités
# (friendly_name « M5Stack Tab5 Home Assistant HMI »).
PREFIXE_ACTIONS = "tab5_ha_hmi"
PREFIXE_ENTITES = "m5stack_tab5_home_assistant_hmi"

# Agenda de travail (Calendrier local) : calendar.travail_ci = VOTRE_EMAIL_gmail_com
# dans placeholders_ci.yaml.
NOM_AGENDA = "Travail CI"
AGENDA = "calendar.travail_ci"

# Automatisation créée depuis le blueprint (étape 4, point 6) et ses entrées : des
# entités de l'intégration demo et de donnees_test.yaml. Pots 4 et 5 laissés vides :
# la tablette doit les masquer (ZONES_ABSENTES).
ID_AUTOMATISATION = "tab5_emplacements_ci"
CHEMIN_BLUEPRINT = "tab5/tab5_emplacements.yaml"
EMPLACEMENTS = {
    "lumiere_1": "light.bed_light",
    "lumiere_2": "light.ceiling_lights",
    "lumiere_3": "light.kitchen_lights",
    "pc": "switch.decorative_lights",
    "tv": "media_player.living_room",
    "tv_telecommande": "remote.remote_one",
    "telephone": "sensor.telephone_ci_batterie",
    "salon_temperature": "sensor.outside_temperature",
    "salon_humidite": "sensor.outside_humidity",
    "serre_temperature": "sensor.serre_ci_temperature",
    "clim": "climate.hvac",
    "volet": "cover.hall_window",
    "pot_1": "sensor.pot_ci_1",
    "pot_2": "sensor.pot_ci_2",
    "pot_3": "sensor.pot_ci_3",
    "agenda_travail": AGENDA,
}
# Pièces (ADR-0023) : la pièce 1 reste vide, l'accueil est donc construit depuis les
# entrées 3.x ci-dessus (t00 = PC, t01 = volet, t02..t04 = lumières) ; les pièces 2 et 3
# sont choisies (la 2 a six appareils : le sixième n'a pas de tuile) ; 4 et 5 vides.
# « Kitchen » est retiré des noms (« Kitchen Lights » → « Lights »). Une même entité
# (kitchen_lights) est dans deux tuiles.
PIECES = {
    "piece_2_nom": "Kitchen",
    "piece_2_tuiles": ["light.kitchen_lights", "cover.kitchen_window", "sensor.outside_temperature",
                       "lock.front_door", "climate.hvac", "valve.front_garden"],
    "piece_3_tuiles": ["media_player.living_room", "binary_sensor.movement_backyard",
                       "light.office_rgbw_lights", "button.push"],
}
PERSONNALISATION = [
    {"entite": "light.office_rgbw_lights", "nom": "Bureau | CI; test", "icone": "mdi:led-strip-variant",
     "comportement": "confirmer"},
]
# Type de tuile par domaine : variables.types_par_domaine du blueprint
# (tests/test_installation_ha.py compare).
TYPES_PAR_DOMAINE = {
    "light": "lum", "switch": "int", "input_boolean": "int", "fan": "int", "humidifier": "int",
    "automation": "int", "cover": "vol", "valve": "vol", "media_player": "med", "scene": "act",
    "script": "act", "button": "act", "input_button": "act", "sensor": "cap", "number": "cap",
    "input_number": "cap", "binary_sensor": "bin", "device_tracker": "bin", "person": "bin",
    "lock": "bin", "climate": "cli",
}
# Ce que le blueprint doit calculer pour ces tuiles, au-delà de la clé et du type :
# {clé: {champ: valeur}} (champs : icone, options, complement, nom). L'icône de la
# personnalisation est lue dans le bloc généré du blueprint (icone_du_blueprint).
TUILES_DETAILS = {
    "t10": {"options": "dc", "nom": "Lights"},
    "t11": {"nom": "Window"},
    "t12": {"complement": "°C"},
    "t13": {"complement": "lock"},
    "t14": {"options": "m"},
    "t20": {"options": "t"},
    "t21": {"complement": "motion"},
    "t22": {"options": "dck", "nom": "Bureau / CI, test", "icone": "mdi:led-strip-variant"},
}
# Texte du capteur « Zones masquées » attendu (zones_texte_masquees(), tab5_zones.cpp :
# ordre de kCles, séparateur « , »).
ZONES_ABSENTES = "pot_4, pot_5"
ZONES_ENTITE = f"sensor.{PREFIXE_ENTITES}_zones_masquees"

# Automatisations dont les traces doivent être « finished » sans erreur après une
# connexion de la tablette : (id, déclencheur attendu dans la trace). À l'ajout, le
# blueprint n'a pas encore d'automatisation (ordre « Sans compiler ») : la poussée
# complète seulement ; après le redémarrage, les trois.
ID_POUSSEE = "tab5_ha_hmi_updater"          # packages/tab5_push.yaml
TRACES_A_L_AJOUT = (
    (ID_POUSSEE, "esphome.tab5_connected"),
)
# Création de l'automatisation du blueprint, tablette déjà connectée : il pousse tout,
# zones comprises, sur le rechargement des automatisations (défaut 4 du 28/09/2026).
TRACES_A_LA_CREATION = (
    (ID_AUTOMATISATION, "automation_reloaded"),
)
TRACES_ATTENDUES = (
    (ID_AUTOMATISATION, "esphome.tab5_connected"),
    (ID_AUTOMATISATION, "esphome.tab5_zones"),
    (ID_POUSSEE, "esphome.tab5_connected"),
)

# Après la création de l'automatisation, sans reconnexion : temps laissé au blueprint
# pour pousser ce qu'il peut, avant de rapporter l'état de l'écran.
ATTENTE_APRES_CREATION = 20.0

# Tolérance sur « reçu après la clé » : HA écrit .storage une seconde après le
# changement (Store, SAVE_DELAY), et ce script le relit toutes les 0,25 s.
TOLERANCE_CLE = 2.0


class Echec(Exception):
    """Étape sans laquelle la suite n'a pas de sens."""


# ─────────────────────────────────────────────────────────────────────────────
# Rapport : chaque vérification, dans le journal et dans le résumé du job.
# ─────────────────────────────────────────────────────────────────────────────

class Rapport:
    def __init__(self) -> None:
        self.lignes: list[tuple[bool | None, str]] = []

    def ok(self, texte: str) -> None:
        logger.info("OK   %s", texte)
        self.lignes.append((True, texte))

    def echec(self, texte: str) -> None:
        logger.error("ÉCHEC %s", texte)
        self.lignes.append((False, texte))
        print(f"::error title=Installation HA::{texte}")

    def info(self, texte: str) -> None:
        logger.info("     %s", texte)
        self.lignes.append((None, texte))

    def verifier(self, condition: bool, texte: str, detail: str = "") -> bool:
        if condition:
            self.ok(texte)
        else:
            self.echec(texte + (f" — {detail}" if detail else ""))
        return condition

    @property
    def reussi(self) -> bool:
        return all(ok is not False for ok, _ in self.lignes)

    def resume(self) -> str:
        symboles = {True: "✅", False: "❌", None: "ℹ️"}
        lignes = ["### Installation dans un Home Assistant neuf", ""]
        lignes += [f"- {symboles[ok]} {texte}" for ok, texte in self.lignes]
        return "\n".join(lignes) + "\n"


# ─────────────────────────────────────────────────────────────────────────────
# Fonctions pures (tests/test_installation_ha.py)
# ─────────────────────────────────────────────────────────────────────────────

def entree_esphome(config_entries: dict, entry_id: str) -> dict | None:
    """L'entrée `entry_id` dans le contenu de .storage/core.config_entries."""
    for entree in config_entries.get("data", {}).get("entries", []):
        if entree.get("entry_id") == entry_id:
            return entree
    return None


def longueur_cle(cle: str | None) -> int:
    """Octets de la clé Noise (base64 de 32 octets attendue), 0 si absente ou illisible."""
    if not cle:
        return 0
    try:
        return len(base64.b64decode(cle, validate=True))
    except ValueError:
        return 0


def horodatage(texte: str | None) -> float:
    """Epoch d'un horodatage ISO de HA (time_fired, timestamp.start), 0 sinon."""
    if not texte:
        return 0.0
    try:
        return dt.datetime.fromisoformat(texte).timestamp()
    except ValueError:
        return 0.0


def erreurs_de_trace(trace: dict) -> list[str]:
    """Erreurs d'une trace complète (trace/get) : celle du passage et celles des étapes.

    Une étape en `continue_on_error` qui échoue garde son erreur dans son élément de
    trace même quand le passage continue : c'est là qu'une poussée ratée se voit."""
    erreurs = []
    if trace.get("error"):
        erreurs.append(f"passage : {trace['error']}")
    for chemin, elements in (trace.get("trace") or {}).items():
        for element in elements:
            if element.get("error"):
                erreurs.append(f"{chemin} : {element['error']}")
    return erreurs


def entrees_blueprint() -> dict[str, Any]:
    """Entrées de l'automatisation (use_blueprint.input) : emplacements 3.x, pièces et
    personnalisation."""
    return {**EMPLACEMENTS, **PIECES, "personnalisation": PERSONNALISATION}


def entites_de_test() -> list[str]:
    """Toutes les entités que les entrées nomment (elles doivent exister dans HA)."""
    entites = set(EMPLACEMENTS.values())
    for cle, valeur in PIECES.items():
        if cle.endswith("_tuiles"):
            entites.update(valeur)
    entites.update(p["entite"] for p in PERSONNALISATION)
    return sorted(entites)


def protocole_de(version: str | None) -> int:
    """Protocole de la tablette selon son sw_version (« 3.1.0 (ESPHome 2026.9.0) ») :
    2 (tuiles) à partir de 3.2.0, 1 sinon ou illisible — la règle du blueprint
    (variable `protocole`)."""
    m = re.match(r" *([0-9]+)[.]([0-9]+)[.]([0-9]+)", version or "")
    return 2 if m and tuple(int(x) for x in m.groups()) >= (3, 2, 0) else 1


def tuiles_attendues() -> list[tuple[str, str]]:
    """(clé, type) des tuiles que le blueprint doit décrire pour ces entrées : la pièce 1
    depuis les entrées 3.x (places fixes), puis les cinq premières entités de chaque
    pièce choisie."""
    tuiles = []
    premiere = "pc" if EMPLACEMENTS.get("pc") else "tv"
    for t, source in enumerate([premiere, "volet", "lumiere_1", "lumiere_2", "lumiere_3"]):
        if entite := EMPLACEMENTS.get(source):
            tuiles.append((f"t0{t}", TYPES_PAR_DOMAINE[entite.split(".")[0]]))
    for n in range(2, 6):
        for t, entite in enumerate(PIECES.get(f"piece_{n}_tuiles", [])[:5]):
            tuiles.append((f"t{n - 1}{t}", TYPES_PAR_DOMAINE[entite.split(".")[0]]))
    return tuiles


def entrees_de(payload: str) -> list[list[str]]:
    """« a|b;c|d; » → [['a', 'b'], ['c', 'd']] (définitions et états des tuiles)."""
    return [e.split("|") for e in (payload or "").split(";") if e]


def juger_definitions(definitions: str, icones_mdi: dict[str, str]) -> list[str]:
    """Écarts entre les définitions calculées par le blueprint et ce qu'on attend."""
    problemes = []
    entrees = entrees_de(definitions)
    pieces = {e[0]: e for e in entrees if e[0].startswith("p")}
    tuiles = {e[0]: e for e in entrees if e[0].startswith("t")}
    if mauvaises := [e for e in entrees if (len(e) != 2 if e[0].startswith("p") else len(e) != 6)]:
        problemes.append(f"entrées mal formées : {mauvaises}")
    attendues = tuiles_attendues()
    if [(cle, e[1]) for cle, e in tuiles.items()] != attendues:
        problemes.append(f"tuiles {[(c, e[1]) for c, e in tuiles.items()]} au lieu de {attendues}")
    pieces_attendues = sorted({f"p{cle[1]}" for cle, _ in attendues})
    if sorted(pieces) != pieces_attendues:
        problemes.append(f"pièces {sorted(pieces)} au lieu de {pieces_attendues}")
    if pieces.get("p1", ["", ""])[1] != PIECES["piece_2_nom"]:
        problemes.append(f"nom de la pièce 2 : {pieces.get('p1')}")
    champs = {"icone": 2, "options": 3, "complement": 4, "nom": 5}
    for cle, details in TUILES_DETAILS.items():
        for champ, attendu in details.items():
            if champ == "icone":
                attendu = icones_mdi.get(attendu, "")
            vu = tuiles.get(cle, [""] * 6)[champs[champ]]
            if vu != attendu:
                problemes.append(f"{cle}.{champ} = {vu!r} au lieu de {attendu!r}")
    return problemes


def variables_de_trace(trace: dict) -> dict[str, Any]:
    """Variables d'un passage (trace/get) : celles du déclencheur (les variables du
    blueprint) puis celles de chaque étape `variables:`."""
    variables: dict[str, Any] = {}
    for elements in (trace.get("trace") or {}).values():
        for element in elements:
            variables.update(element.get("changed_variables") or {})
    return variables


def appels_de_trace(trace: dict) -> list[dict]:
    """Actions appelées pendant un passage : {domain, service, service_data}."""
    appels = []
    for elements in (trace.get("trace") or {}).values():
        for element in elements:
            params = ((element.get("result") or {}).get("params")) or {}
            if params.get("domain") and params.get("service"):
                appels.append(params)
    return appels


def icones_du_blueprint(texte: str) -> dict[str, str]:
    """« mdi:nom » → code, lu entre les marqueurs du bloc généré du blueprint (un YAML
    autonome une fois désindenté ; le reste du blueprint a des !input)."""
    import textwrap

    import yaml  # tiré par esphome dans le job, par requirements-dev.txt en local

    bloc = texte.split("# >>> icones", 1)[1].split("\n", 1)[1].split("# <<< icones", 1)[0]
    return (yaml.safe_load(textwrap.dedent(bloc)) or {}).get("icones_mdi") or {}


# ─────────────────────────────────────────────────────────────────────────────
# La tablette virtuelle (programme de la plateforme host d'ESPHome)
# ─────────────────────────────────────────────────────────────────────────────

class Tablette:
    """Lance, arrête et relance le programme ; ses préférences (clé API comprise)
    restent dans `prefs` d'un lancement à l'autre, comme la NVS de la vraie."""

    def __init__(self, programme: Path, prefs: Path, captures: Path) -> None:
        self.programme = programme
        self.prefs = prefs
        self.captures = captures
        self.proc: subprocess.Popen | None = None
        self.journal = None
        self.lancements = 0

    def demarrer(self) -> float:
        self.lancements += 1
        self.prefs.mkdir(parents=True, exist_ok=True)
        self.captures.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, ESPHOME_PREFDIR=str(self.prefs.resolve()),
                   ESPHOME_SNAPSHOT_DIR=str(self.captures.resolve()), TZ=FUSEAU)
        self.journal = open(self.captures / f"tablette-{self.lancements}.log", "wb")  # noqa: SIM115
        self.proc = subprocess.Popen([str(self.programme)], env=env, stdout=self.journal,
                                     stderr=subprocess.STDOUT)
        logger.info("Tablette virtuelle lancée (%d), pid %d", self.lancements, self.proc.pid)
        return time.time()

    async def attendre_port(self, delai: float = 30.0) -> None:
        fin = time.monotonic() + delai
        while time.monotonic() < fin:
            if self.proc is not None and self.proc.poll() is not None:
                raise Echec(f"la tablette virtuelle s'est arrêtée (code {self.proc.returncode}), "
                            f"voir tablette-{self.lancements}.log")
            try:
                _, ecriture = await asyncio.open_connection(HOTE_TABLETTE, PORT_API)
                ecriture.close()
                await ecriture.wait_closed()
                return
            except OSError:
                await asyncio.sleep(0.5)
        raise Echec(f"l'API de la tablette virtuelle ne répond pas sur le port {PORT_API}")

    async def arreter(self) -> None:
        if self.proc is None:
            return
        self.proc.terminate()
        for _ in range(40):
            if self.proc.poll() is not None:
                break
            await asyncio.sleep(0.25)
        else:
            self.proc.kill()
            self.proc.wait()
        if self.journal:
            self.journal.close()
        self.proc = None


async def essayer_connexion(psk: str | None) -> str:
    """Se connecte à la tablette avec `psk` (None : en clair). « acceptée » ou le nom
    de l'exception d'aioesphomeapi."""
    from aioesphomeapi import APIClient, APIConnectionError

    client = APIClient(HOTE_TABLETTE, PORT_API, "", noise_psk=psk,
                       client_info="Tab5 installation CI")
    try:
        await asyncio.wait_for(client.connect(login=True), 20)
        await client.device_info()
    except APIConnectionError as exc:
        return type(exc).__name__
    except asyncio.TimeoutError:
        return "délai dépassé"
    finally:
        await client.disconnect(force=True)
    return "acceptée"


# ─────────────────────────────────────────────────────────────────────────────
# Home Assistant : REST et websocket
# ─────────────────────────────────────────────────────────────────────────────

class WS:
    """Websocket de HA : commandes (id → réponse) et événements abonnés."""

    def __init__(self, ws) -> None:
        self.ws = ws
        self.n = 0
        self.attente: dict[int, asyncio.Future] = {}
        self.evenements: list[dict] = []
        self.nouveau = asyncio.Event()
        self.tache: asyncio.Task | None = None

    async def authentifier(self, jeton: str) -> None:
        await self.ws.receive_json()  # auth_required
        await self.ws.send_json({"type": "auth", "access_token": jeton})
        reponse = await self.ws.receive_json()
        if reponse.get("type") != "auth_ok":
            raise Echec(f"websocket : authentification refusée ({reponse.get('type')})")
        self.tache = asyncio.create_task(self._lire())

    async def _lire(self) -> None:
        async for message in self.ws:
            if message.type.name != "TEXT":
                continue
            donnees = json.loads(message.data)
            for d in donnees if isinstance(donnees, list) else [donnees]:
                if d.get("type") == "event":
                    self.evenements.append(d["event"])
                    self.nouveau.set()
                elif (futur := self.attente.pop(d.get("id"), None)) and not futur.done():
                    futur.set_result(d)

    async def commande(self, type_: str, **champs) -> Any:
        self.n += 1
        futur = asyncio.get_running_loop().create_future()
        self.attente[self.n] = futur
        await self.ws.send_json({"id": self.n, "type": type_, **champs})
        reponse = await asyncio.wait_for(futur, 60)
        if not reponse.get("success", False):
            raise Echec(f"websocket {type_} : {reponse.get('error')}")
        return reponse.get("result")

    async def abonner(self, type_evenement: str) -> None:
        await self.commande("subscribe_events", event_type=type_evenement)

    async def attendre_evenement(self, type_evenement: str, apres: float, delai: float) -> dict | None:
        """Premier événement `type_evenement` émis à `apres` ou plus tard (epoch)."""
        fin = time.monotonic() + delai
        while True:
            for evt in self.evenements:
                if evt.get("event_type") == type_evenement and horodatage(evt.get("time_fired")) >= apres:
                    return evt
            reste = fin - time.monotonic()
            if reste <= 0:
                return None
            self.nouveau.clear()
            try:
                await asyncio.wait_for(self.nouveau.wait(), reste)
            except asyncio.TimeoutError:
                pass


class HA:
    def __init__(self, session, base: str, conteneur: str) -> None:
        self.session = session
        self.base = base.rstrip("/")
        self.conteneur = conteneur
        self.jeton: str | None = None
        self.ws: WS | None = None

    @property
    def _entetes(self) -> dict:
        return {"Authorization": f"Bearer {self.jeton}"} if self.jeton else {}

    async def requete(self, methode: str, chemin: str, attendu: tuple[int, ...] = (200,), **kw) -> Any:
        async with self.session.request(methode, self.base + chemin, headers=self._entetes, **kw) as r:
            texte = await r.text()
            if r.status not in attendu:
                raise Echec(f"{methode} {chemin} : HTTP {r.status} {texte[:300]}")
            try:
                return json.loads(texte) if texte else None
            except ValueError:
                return texte

    async def get(self, chemin: str, **kw) -> Any:
        return await self.requete("GET", chemin, **kw)

    async def post(self, chemin: str, donnees: Any = None, **kw) -> Any:
        return await self.requete("POST", chemin, json=donnees, **kw)

    async def attendre_http(self, delai: float = 240.0) -> list:
        """HA répond (page d'accueil de l'onboarding) : les étapes restantes."""
        fin = time.monotonic() + delai
        while time.monotonic() < fin:
            try:
                return await self.get("/api/onboarding")
            except Exception:  # noqa: BLE001 — HA démarre : connexion refusée, 502…
                await asyncio.sleep(2)
        raise Echec("Home Assistant ne répond pas sur /api/onboarding (voir docker logs)")

    async def onboarding(self, mot_de_passe: str) -> None:
        """Étape « créer un compte » de l'interface, puis le jeton de ce compte."""
        reponse = await self.post("/api/onboarding/users", {
            "client_id": CLIENT_ID, "name": "CI", "username": "ci",
            "password": mot_de_passe, "language": "fr",
        })
        async with self.session.post(self.base + "/auth/token", data={
            "grant_type": "authorization_code", "code": reponse["auth_code"], "client_id": CLIENT_ID,
        }) as r:
            if r.status != 200:
                raise Echec(f"/auth/token : HTTP {r.status}")
            self.jeton = (await r.json())["access_token"]

    async def terminer_onboarding(self) -> None:
        """Les écrans suivants de l'onboarding (lieu, statistiques, intégrations)."""
        for chemin, donnees in (("/api/onboarding/core_config", None),
                                ("/api/onboarding/analytics", None),
                                ("/api/onboarding/integration",
                                 {"client_id": CLIENT_ID, "redirect_uri": CLIENT_ID + "?auth_callback=1"})):
            await self.post(chemin, donnees, attendu=(200, 403))

    async def attendre_demarrage(self, delai: float = 180.0) -> dict:
        fin = time.monotonic() + delai
        while time.monotonic() < fin:
            config = await self.get("/api/config")
            if config.get("state") == "RUNNING":
                return config
            await asyncio.sleep(2)
        raise Echec("Home Assistant n'a pas fini de démarrer (état RUNNING)")

    async def websocket(self) -> WS:
        ws = await self.session.ws_connect(self.base.replace("http", "ws", 1) + "/api/websocket",
                                           heartbeat=30, max_msg_size=0)
        self.ws = WS(ws)
        await self.ws.authentifier(self.jeton)
        return self.ws

    async def etats(self) -> dict[str, dict]:
        return {e["entity_id"]: e for e in await self.get("/api/states")}

    async def flux(self, chemin: str, handler: str, saisie: dict | None = None) -> dict:
        """Un flux de configuration ou d'options, comme l'interface : ouverture, puis
        une saisie (sinon les valeurs par défaut du formulaire)."""
        etape = await self.post(chemin, {"handler": handler, "show_advanced_options": False})
        if etape.get("type") != "form":
            return etape
        valeurs = {c["name"]: c["default"] for c in etape.get("data_schema", []) if "default" in c}
        valeurs.update(saisie or {})
        return await self.post(f"{chemin}/{etape['flow_id']}", valeurs)

    async def storage(self, nom: str) -> dict | None:
        """Un fichier de /config/.storage, lu dans le conteneur (propriété de root)."""
        proc = await asyncio.create_subprocess_exec(
            "docker", "exec", self.conteneur, "cat", f"/config/.storage/{nom}",
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        sortie, _ = await proc.communicate()
        if proc.returncode != 0 or not sortie:
            return None
        return json.loads(sortie)

    async def traces(self, item_id: str) -> list[dict]:
        return await self.ws.commande("trace/list", domain="automation", item_id=item_id)

    async def trace(self, item_id: str, run_id: str) -> dict:
        return await self.ws.commande("trace/get", domain="automation", item_id=item_id, run_id=run_id)


# ─────────────────────────────────────────────────────────────────────────────
# Le scénario
# ─────────────────────────────────────────────────────────────────────────────

async def creer_agenda(ha: HA, rapport: Rapport) -> None:
    """Un agenda de travail avec deux journées « Travail » (heures du planning)."""
    resultat = await ha.flux("/api/config/config_entries/flow", "local_calendar",
                             {"calendar_name": NOM_AGENDA, "import": "create_empty"})
    if resultat.get("type") != "create_entry":
        raise Echec(f"Calendrier local : flux terminé en « {resultat.get('type')} » ({resultat.get('reason')})")
    for _ in range(40):
        if AGENDA in await ha.etats():
            break
        await asyncio.sleep(0.5)
    else:
        raise Echec(f"{AGENDA} n'apparaît pas après l'ajout du Calendrier local")
    from zoneinfo import ZoneInfo

    aujourdhui = dt.datetime.now(ZoneInfo(FUSEAU)).date()
    for decalage in (1, 2):
        jour = (aujourdhui + dt.timedelta(days=decalage)).isoformat()
        await ha.post("/api/services/calendar/create_event", {
            "entity_id": AGENDA, "summary": "Travail",
            "start_date_time": f"{jour} 09:00:00", "end_date_time": f"{jour} 17:30:00",
        })
    rapport.ok(f"agenda de travail {AGENDA} (Calendrier local) avec deux journées « Travail »")


async def verifier_blueprint(ha: HA, rapport: Rapport) -> None:
    """Le blueprint copié dans config/ (étape 4, point 6) est lu par HA sans erreur."""
    liste = await ha.ws.commande("blueprint/list", domain="automation")
    if CHEMIN_BLUEPRINT not in liste:
        raise Echec(f"blueprint {CHEMIN_BLUEPRINT} absent de la liste de HA : {sorted(liste)}")
    if erreur := liste[CHEMIN_BLUEPRINT].get("error"):
        raise Echec(f"blueprint {CHEMIN_BLUEPRINT} refusé par HA : {erreur}")
    rapport.ok(f"blueprint {CHEMIN_BLUEPRINT} chargé par HA")


async def creer_automatisation(ha: HA, rapport: Rapport) -> None:
    """« Vos appareils » : une automatisation depuis le blueprint, entités choisies."""
    etats = await ha.etats()
    manquantes = [e for e in entites_de_test() if e not in etats]
    if manquantes:
        domaines = sorted({e.split(".")[0] for e in manquantes})
        existantes = sorted(e for e in etats if e.split(".")[0] in domaines)
        raise Echec(f"entités de test absentes : {manquantes} ; présentes : {existantes}")

    await ha.post(f"/api/config/automation/config/{ID_AUTOMATISATION}", {
        "alias": "Tab5 — emplacements (CI)",
        "description": "Créée par tools/installation_ha/verifier_installation.py",
        "use_blueprint": {"path": CHEMIN_BLUEPRINT, "input": entrees_blueprint()},
    })
    for _ in range(40):
        automatisations = [e for e in (await ha.etats()).values()
                           if e["entity_id"].startswith("automation.")
                           and e["attributes"].get("id") == ID_AUTOMATISATION]
        if automatisations and automatisations[0]["state"] == "on":
            rapport.ok(f"automatisation du blueprint créée et active ({automatisations[0]['entity_id']})")
            return
        await asyncio.sleep(0.5)
    raise Echec(f"l'automatisation {ID_AUTOMATISATION} n'est pas active après sa création")


async def ajouter_tablette(ha: HA, rapport: Rapport) -> str:
    """Étape 6 : Paramètres → Appareils et services → Ajouter → ESPHome → hôte, port."""
    flux = "/api/config/config_entries/flow"
    etape = await ha.post(flux, {"handler": "esphome", "show_advanced_options": False})
    if etape.get("type") != "form" or etape.get("step_id") != "user":
        raise Echec(f"flux ESPHome : première étape inattendue {etape}")
    resultat = await ha.post(f"{flux}/{etape['flow_id']}", {"host": HOTE_TABLETTE, "port": PORT_API})
    if resultat.get("type") != "create_entry":
        raise Echec("flux ESPHome : pas d'entrée créée directement — étape "
                    f"« {resultat.get('step_id')} », erreurs {resultat.get('errors')}, "
                    f"type {resultat.get('type')}, raison {resultat.get('reason')}")
    entree = resultat["result"]
    rapport.ok(f"tablette ajoutée sans saisir de clé (entrée « {resultat.get('title')} »)")
    return entree["entry_id"]


async def attendre_cle(ha: HA, entry_id: str, delai: float = 60.0) -> tuple[str, float]:
    """La clé que HA a donnée à la tablette, relue dans core.config_entries."""
    fin = time.monotonic() + delai
    while time.monotonic() < fin:
        contenu = await ha.storage("core.config_entries")
        entree = entree_esphome(contenu or {}, entry_id)
        cle = (entree or {}).get("data", {}).get("noise_psk")
        if longueur_cle(cle) == 32:
            return cle, time.time()
        await asyncio.sleep(0.25)
    raise Echec("aucune clé de 32 octets dans core.config_entries (data.noise_psk) "
                f"{delai:.0f} s après l'ajout (voir docker logs : provisioning)")


async def autoriser_actions(ha: HA, entry_id: str, rapport: Rapport) -> None:
    """« Autoriser l'appareil à effectuer des actions Home Assistant » (étape 6)."""
    resultat = await ha.flux("/api/config/config_entries/options/flow", entry_id,
                             {"allow_service_calls": True})
    if resultat.get("type") != "create_entry":
        raise Echec(f"options ESPHome : {resultat}")
    for _ in range(20):
        entree = entree_esphome(await ha.storage("core.config_entries") or {}, entry_id)
        if (entree or {}).get("options", {}).get("allow_service_calls") is True:
            rapport.ok("option « autoriser les actions Home Assistant » cochée et enregistrée")
            return
        await asyncio.sleep(0.5)
    rapport.echec("option allow_service_calls absente de core.config_entries après le flux d'options")


async def verifier_cle(ha: HA, entry_id: str, cle: str, rapport: Rapport) -> None:
    """La clé est gardée deux fois par HA (entrée, et esphome.encryption_keys par MAC),
    ouvre la tablette, et la tablette refuse désormais la clé nulle et le clair."""
    from aioesphomeapi import ZERO_NOISE_PSK

    rapport.ok("clé API générée par HA et gardée dans core.config_entries (32 octets, non affichée)")
    entree = entree_esphome(await ha.storage("core.config_entries") or {}, entry_id) or {}
    cles = ((await ha.storage("esphome.encryption_keys")) or {}).get("data", {}).get("keys", {})
    rapport.verifier(cles.get(entree.get("unique_id")) == cle,
                     "même clé dans esphome.encryption_keys (MAC de la tablette)",
                     f"{len(cles)} clé(s) gardée(s), aucune identique pour {entree.get('unique_id')}")
    rapport.verifier((r := await essayer_connexion(cle)) == "acceptée",
                     "la clé gardée par HA ouvre la tablette", r)
    rapport.verifier((r := await essayer_connexion(ZERO_NOISE_PSK)) == "InvalidEncryptionKeyAPIError",
                     "clé nulle (appairage) refusée une fois la clé posée", f"résultat : {r}")
    rapport.verifier((r := await essayer_connexion(None)) == "RequiresEncryptionAPIError",
                     "connexion en clair refusée une fois la clé posée", f"résultat : {r}")


# Issues d'un passage refusé parce qu'un autre tourne (mode single, max atteint) : la
# poussée complète est en `mode: single`, et une deuxième connexion rapprochée (le
# rechargement qui suit l'option « actions HA ») tombe pendant la première. Rapportées,
# pas fautives, si un autre passage du même déclencheur a abouti.
REFUS_DE_MODE = ("failed_single", "failed_max_runs")


def resume_passage(t: dict) -> str:
    return (f"{(t.get('timestamp') or {}).get('start', '?')[11:19]} {t.get('state')}/"
            f"{t.get('script_execution')} (étape {t.get('last_step')}){' — ' + t['error'] if t.get('error') else ''}")


async def attendre_traces(ha: HA, apres: float, rapport: Rapport, attendues=TRACES_ATTENDUES,
                          delai: float = 150.0) -> None:
    """Chaque (automatisation, déclencheur) de `attendues` : ses passages depuis `apres`,
    jugés quand l'un a abouti et qu'aucun ne tourne encore. Au moins un « finished »,
    aucune étape en erreur, et aucun passage en erreur ni arrêté par ses conditions (le
    cas de la poussée complète sans l'entité « HA API Status », vu le 28/09/2026)."""
    fin = time.monotonic() + delai
    restantes = list(attendues)
    vus: dict[tuple[str, str], list[dict]] = {}
    while restantes and time.monotonic() < fin:
        for item_id, declencheur in list(restantes):
            passages = [t for t in await ha.traces(item_id)
                        if declencheur in (t.get("trigger") or "")
                        and horodatage((t.get("timestamp") or {}).get("start")) >= apres]
            vus[(item_id, declencheur)] = passages
            finis = any(t.get("script_execution") == "finished" for t in passages)
            if finis and all(t.get("state") == "stopped" for t in passages):
                restantes.remove((item_id, declencheur))
                await juger_passages(ha, item_id, declencheur, passages, rapport)
        if restantes:
            await asyncio.sleep(2)
    for item_id, declencheur in restantes:
        passages = vus.get((item_id, declencheur)) or []
        rapport.echec(f"{item_id} ({declencheur}) : aucun passage abouti en {delai:.0f} s — "
                      + ("; ".join(resume_passage(t) for t in passages) or "aucun passage"))


async def juger_passages(ha: HA, item_id: str, declencheur: str, passages: list[dict], rapport: Rapport) -> None:
    fautifs = []
    for t in passages:
        execution = t.get("script_execution")
        if execution in REFUS_DE_MODE:
            rapport.info(f"{item_id} ({declencheur}) : un passage refusé ({execution}), un autre tournait")
        elif execution != "finished":
            fautifs.append(resume_passage(t))
        elif erreurs := erreurs_de_trace(await ha.trace(item_id, t["run_id"])):
            fautifs.append(f"{resume_passage(t)} : " + " ; ".join(erreurs[:5]))
    rapport.verifier(not fautifs, f"trace {item_id} ({declencheur}) terminée sans erreur", " | ".join(fautifs))


async def rapporter_apres_creation(ha: HA, cree: float, rapport: Rapport) -> None:
    """Ce que le blueprint a fait depuis la création de son automatisation, sans
    reconnexion de la tablette : ses déclencheurs sont la connexion, la demande des
    zones (une par connexion), le rechargement des automatisations (sa propre
    création), les changements d'état et les mesures toutes les 5 minutes. Rapporté
    pour relecture ; le rechargement et les zones sont jugés dans scenario()."""
    passages = [t for t in await ha.traces(ID_AUTOMATISATION)
                if horodatage((t.get("timestamp") or {}).get("start")) >= cree]
    declencheurs = sorted({t.get("trigger") or "?" for t in passages})
    zones = (await ha.etats()).get(ZONES_ENTITE, {}).get("state")
    rapport.info(f"automatisation créée après l'ajout (ordre « Sans compiler ») : "
                 f"{ATTENTE_APRES_CREATION:.0f} s plus tard, sans reconnexion, {len(passages)} passage(s) "
                 f"du blueprint ({', '.join(declencheurs) or 'aucun'}) et « Zones masquées » = {zones!r} "
                 f"(attendu après une connexion : {ZONES_ABSENTES!r}) — capture installation-ha-1")


async def attendre_etat(ha: HA, entity_id: str, attendu: str, delai: float = 60.0) -> str | None:
    fin = time.monotonic() + delai
    etat = None
    while time.monotonic() < fin:
        etat = (await ha.etats()).get(entity_id, {}).get("state")
        if etat == attendu:
            return etat
        await asyncio.sleep(1)
    return etat


async def capturer(ha: HA, dossier: Path, nom: str, rapport: Rapport) -> None:
    """Capture demandée PAR Home Assistant (action de la tablette), puis PNG à l'endroit
    (même rotation que tools/rendu/capturer.py : la dalle est en portrait)."""
    await ha.post(f"/api/services/esphome/{PREFIXE_ACTIONS}_rendu_capture", {"fichier": nom})
    bmp = dossier / f"{nom}.bmp"
    for _ in range(40):
        if bmp.exists() and bmp.stat().st_size > 0:
            break
        await asyncio.sleep(0.25)
    else:
        rapport.echec(f"capture {nom} : pas de BMP écrit par la tablette après l'action de HA")
        return
    await asyncio.sleep(0.5)
    from PIL import Image

    with Image.open(bmp) as image:
        image.rotate(270, expand=True).save(bmp.with_suffix(".png"), optimize=True)
    bmp.unlink()
    rapport.ok(f"capture d'écran demandée par HA : {nom}.png")


# Ligne du journal de HA (docker logs) : « 2026-09-28 11:17:49.794 ERROR (MainThread)
# [logger] message », à l'heure locale du conteneur (TZ=Europe/Paris) ; les lignes
# suivantes sans en-tête (pile d'appels, détail d'une condition) prolongent le message.
LIGNE_JOURNAL = re.compile(r"^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3}) (DEBUG|INFO|WARNING|ERROR|CRITICAL) "
                           r"\([^)]*\) \[([^\]]+)\] (.*)$")
# Couleurs de la console de HA (« \x1b[31m » devant chaque ligne d'erreur).
COULEURS = re.compile(r"\x1b\[[0-9;]*m")


def lignes_du_journal(texte: str) -> list[dict]:
    """Entrées du journal de HA : epoch, niveau, logger, message (suite comprise)."""
    from zoneinfo import ZoneInfo

    fuseau = ZoneInfo(FUSEAU)
    entrees: list[dict] = []
    for ligne in COULEURS.sub("", texte).splitlines():
        if m := LIGNE_JOURNAL.match(ligne):
            quand = dt.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S.%f").replace(tzinfo=fuseau)
            entrees.append({"quand": quand.timestamp(), "niveau": m.group(2),
                            "logger": m.group(3), "message": m.group(4)})
        elif entrees and ligne.strip():
            entrees[-1]["message"] += "\n" + ligne.rstrip()
    return entrees


def concerne_le_tab5(entree: dict) -> bool:
    """Entrée du journal qui parle du Tab5 : ses packages, son blueprint, ses entités ou
    l'intégration ESPHome."""
    texte = (entree.get("logger", "") + " " + entree.get("message", "")).lower()
    return any(mot in texte for mot in ("tab5", "m5stack", "esphome"))


def classer_journal(entrees: list[dict], connexion: float,
                    deconnexions: list[tuple[float, float]]) -> tuple[list[str], dict[tuple, int], int]:
    """(erreurs fautives, groupes rapportés, nombre d'entrées sans rapport avec le Tab5).

    Fautive : une ERREUR qui concerne le Tab5, avant comme après la première connexion
    de la tablette (avant : poussées vers une tablette pas encore ajoutée, « Action …
    not found », défaut 3 du 28/09/2026 : les packages doivent l'attendre). Sauf « Not connected to … » pendant une déconnexion VOULUE (`deconnexions`,
    intervalles d'epoch : rechargement après l'option « actions HA », redémarrage de la
    tablette) : une poussée partie vers une tablette hors ligne, rapportée à part."""
    fautives: list[str] = []
    groupes: dict[tuple, int] = {}
    autres = 0
    for e in entrees:
        if e["niveau"] not in ("WARNING", "ERROR", "CRITICAL"):
            continue
        if not concerne_le_tab5(e):
            autres += 1
            continue
        premiere = e["message"].splitlines()[0][:220]
        if e["niveau"] == "WARNING":
            moment = "avant la connexion" if e["quand"] < connexion else "après la connexion"
        elif e["quand"] < connexion:
            fautives.append(f"(avant la connexion) {e['niveau']} [{e['logger']}] {e['message'][:600]}")
            continue
            moment = "après la connexion"
        elif "Not connected to" in e["message"] and any(a <= e["quand"] <= b for a, b in deconnexions):
            moment = "pendant une déconnexion voulue de la tablette"
        else:
            fautives.append(f"{e['niveau']} [{e['logger']}] {e['message'][:600]}")
            continue
        cle = (moment, e["niveau"], e["logger"], premiere)
        groupes[cle] = groupes.get(cle, 0) + 1
    return fautives, groupes, autres


async def journal_ha(ha: HA, connexion: float, deconnexions: list[tuple[float, float]], rapport: Rapport) -> None:
    """Journal de HA (docker logs) jugé par classer_journal. Avant la connexion, les
    actions esphome.tab5_ha_hmi_* n'existent pas : dans l'ordre de docs/installation.md
    (packages, puis tablette), les poussées doivent attendre la tablette au lieu
    d'échouer (« Action … not found »)."""
    proc = await asyncio.create_subprocess_exec("docker", "logs", ha.conteneur, stdout=subprocess.PIPE,
                                                stderr=subprocess.STDOUT)
    sortie, _ = await proc.communicate()
    entrees = lignes_du_journal(sortie.decode("utf-8", "replace"))
    # Au moins les lignes d'info de l'intégration ESPHome (logger: dans configuration.yaml) :
    # un journal où rien n'est reconnu ne doit pas passer pour un journal propre.
    if not any(e["logger"].startswith("homeassistant.components.esphome") for e in entrees):
        rapport.echec(f"journal HA illisible : {len(entrees)} ligne(s) reconnue(s), aucune de l'intégration ESPHome")
        return
    fautives, groupes, autres = classer_journal(entrees, connexion, deconnexions)
    for texte in fautives:
        rapport.echec(f"journal HA, erreur Tab5 — {texte}")
    for (moment, niveau, logger_, premiere), n in groupes.items():
        rapport.info(f"journal HA, {moment} — {niveau} ×{n} [{logger_}] {premiere}")
    rapport.info(f"journal HA : {autres} avertissement(s) ou erreur(s) sans rapport avec le Tab5 (demo, traductions…)")


async def version_tablette(ha: HA) -> str | None:
    """sw_version de la tablette dans le registre des appareils (trouvée par son modèle,
    comme le fait le blueprint)."""
    appareils = await ha.ws.commande("config/device_registry/list")
    for appareil in appareils:
        if appareil.get("model") == "tab5-ha-hmi":
            return appareil.get("sw_version")
    return None


async def verifier_tuiles(ha: HA, cree: float, connexion: float, rapport: Rapport) -> None:
    """Pièces (ADR-0023) : ce que le blueprint calcule à la connexion (définitions et
    états des tuiles, dans la trace), et ce qu'il appelle selon la version de la
    tablette. Protocole 1 (tablette virtuelle « rendu », firmware 3.0/3.1) :
    tab5_maj_tuiles ne doit JAMAIS partir (action absente : erreur que
    continue_on_error n'attrape pas) ; protocole 2 : elle part, avec les définitions."""
    version = await version_tablette(ha)
    protocole = protocole_de(version)
    rapport.info(f"tablette {version!r} : protocole {protocole} "
                 f"({'tuiles' if protocole == 2 else 'clés 3.x seulement, pas de tab5_maj_tuiles'})")
    passages = [t for t in await ha.traces(ID_AUTOMATISATION)
                if horodatage((t.get("timestamp") or {}).get("start")) >= cree]
    traces = {t["run_id"]: await ha.trace(ID_AUTOMATISATION, t["run_id"]) for t in passages}

    a_la_connexion = [t for t in passages if "esphome.tab5_connected" in (t.get("trigger") or "")
                      and horodatage((t.get("timestamp") or {}).get("start")) >= connexion
                      and t.get("script_execution") == "finished"]
    if not a_la_connexion:
        rapport.echec("pièces : aucun passage du blueprint abouti à la connexion, rien à relire")
        return
    variables = variables_de_trace(traces[a_la_connexion[-1]["run_id"]])
    definitions = variables.get("definitions")
    if not rapport.verifier(isinstance(definitions, str) and definitions != "",
                            "pièces : le blueprint calcule les définitions des tuiles à la connexion",
                            f"variable definitions = {definitions!r}"):
        return
    racine = Path(__file__).resolve().parents[2]
    icones = icones_du_blueprint((racine / "HomeAssistant_Config" / "blueprints" / "automation"
                                  / CHEMIN_BLUEPRINT).read_text(encoding="utf-8"))
    problemes = juger_definitions(definitions, icones)
    tuiles = [cle for cle, _ in tuiles_attendues()]
    rapport.verifier(not problemes,
                     f"pièces : définitions conformes ({len(tuiles)} tuiles ; pièce 1 depuis les entrées 3.x, "
                     "pièces 2 et 3 choisies, cinq tuiles au plus, nom de pièce retiré, personnalisation)",
                     " ; ".join(problemes))
    rapport.info(f"définitions : {definitions}")
    etats = variables.get("etats_tuiles") or ""
    rapport.verifier([e[0] for e in entrees_de(etats)] == tuiles and all(len(e) == 4 for e in entrees_de(etats)),
                     "pièces : l'état de chaque tuile est calculé après les définitions (tRT|état|valeur|couleur)",
                     f"etats_tuiles = {etats!r}")

    appels = [(run_id, a) for run_id, trace in traces.items() for a in appels_de_trace(trace)
              if str(a.get("service", "")).endswith("_tab5_maj_tuiles")]
    if protocole == 1:
        rapport.verifier(not appels, f"protocole 1 : aucun appel de tab5_maj_tuiles ({len(traces)} passage(s) relus)",
                         f"{len(appels)} appel(s)")
        # Et les états tRT ne partent pas non plus.
        trt = [a for _, trace in traces.items() for a in appels_de_trace(trace)
               if str(a.get("service", "")).endswith("_tab5_maj_emplacements")
               and re.search(r"(^|;)t[0-4][0-4][|]", str((a.get("service_data") or {}).get("payload", "")))]
        rapport.verifier(not trt, "protocole 1 : aucun état de tuile (tRT) poussé", f"{len(trt)} poussée(s)")
    else:
        charges = [(a.get("service_data") or {}).get("payload") for _, a in appels]
        rapport.verifier(definitions in charges, "protocole 2 : tab5_maj_tuiles appelée avec les définitions",
                         f"{len(appels)} appel(s)")


async def rapporter_traces(ha: HA, item_id: str, depuis: float, rapport: Rapport) -> None:
    """Passages d'une automatisation depuis `depuis` : déclencheur et issue (informatif)."""
    passages = [t for t in await ha.traces(item_id)
                if horodatage((t.get("timestamp") or {}).get("start")) >= depuis]
    for t in passages:
        rapport.info(f"{item_id} : {t.get('trigger')} — {resume_passage(t)}")


async def scenario(args, rapport: Rapport) -> None:
    import aiohttp

    tablette = Tablette(args.programme, args.prefs, args.captures)
    tablette.demarrer()
    try:
        async with aiohttp.ClientSession() as session:
            ha = HA(session, args.ha, args.conteneur)
            await ha.attendre_http()
            await ha.onboarding(secrets.token_urlsafe(24))
            config = await ha.attendre_demarrage()
            rapport.ok(f"Home Assistant {config.get('version')} neuf, compte créé (onboarding)")
            ws = await ha.websocket()
            await ws.commande("config/core/update", time_zone=FUSEAU, country="FR",
                              language="fr", currency="EUR", unit_system="metric")
            await ha.terminer_onboarding()
            for evenement in ("esphome.tab5_connected", "esphome.tab5_zones"):
                await ws.abonner(evenement)

            # « Sans compiler », 1 : Home Assistant d'abord. Packages et blueprint sont
            # déjà dans config/ (preparer_config.py) ; un agenda, comme chez l'utilisateur.
            await creer_agenda(ha, rapport)
            await verifier_blueprint(ha, rapport)

            # 4 et 5 : ajout de la tablette dans sa fenêtre d'appairage, actions HA.
            await tablette.attendre_port()
            debut = time.time()
            entry_id = await ajouter_tablette(ha, rapport)
            cle, vue = await attendre_cle(ha, entry_id)
            await verifier_cle(ha, entry_id, cle, rapport)
            evt = await ws.attendre_evenement("esphome.tab5_connected", vue - TOLERANCE_CLE, 120)
            if not rapport.verifier(evt is not None, "esphome.tab5_connected reçu après la clé (connexion chiffrée)",
                                    "rien en 120 s : HA s'est-il reconnecté avec la clé ? (docker logs)"):
                raise Echec("la tablette n'est pas reconnectée à HA après la clé")
            connexion = horodatage(evt["time_fired"])
            rapport.info(f"clé vue {vue - debut:.1f} s après l'ajout, tab5_connected "
                         f"{connexion - debut:.1f} s après")
            # L'option recharge l'intégration : HA se déconnecte puis revient (une
            # déconnexion voulue, comme le redémarrage plus bas).
            deconnexions: list[tuple[float, float]] = []
            option = time.time()
            await autoriser_actions(ha, entry_id, rapport)
            retour = await ws.attendre_evenement("esphome.tab5_connected", option, 60)
            deconnexions.append((option, (horodatage(retour["time_fired"]) if retour else time.time()) + 1))
            rapport.info("après l'option, HA revient : tab5_connected " + (
                f"{horodatage(retour['time_fired']) - option:.1f} s après" if retour else "non reçu en 60 s"))
            await attendre_traces(ha, debut, rapport, TRACES_A_L_AJOUT)

            # 6 : « Vos appareils », l'automatisation du blueprint, APRÈS l'ajout : elle
            # doit remplir l'écran tout de suite, sans attendre une reconnexion.
            cree = time.time()
            await creer_automatisation(ha, rapport)
            await attendre_traces(ha, cree, rapport, TRACES_A_LA_CREATION)
            etat = await attendre_etat(ha, ZONES_ENTITE, ZONES_ABSENTES, ATTENTE_APRES_CREATION)
            rapport.verifier(etat == ZONES_ABSENTES,
                             f"dès la création de l'automatisation, sans reconnexion, la tablette masque "
                             f"les emplacements vides ({ZONES_ABSENTES})", f"capteur « Zones masquées » = {etat!r}")
            await rapporter_apres_creation(ha, cree, rapport)
            await capturer(ha, args.captures, "installation-ha-1", rapport)

            # Redémarrage de la tablette : la clé persiste, HA revient en chiffré, et
            # l'automatisation du blueprint voit enfin une connexion.
            arret = time.time()
            await tablette.arreter()
            relance = tablette.demarrer()
            await tablette.attendre_port()
            evt = await ws.attendre_evenement("esphome.tab5_connected", relance, 120)
            deconnexions.append((arret, (horodatage(evt["time_fired"]) if evt else time.time()) + 1))
            rapport.verifier(evt is not None, "après un redémarrage de la tablette, HA se reconnecte (tab5_connected)",
                             "rien en 120 s")
            from aioesphomeapi import ZERO_NOISE_PSK

            rapport.verifier((r := await essayer_connexion(ZERO_NOISE_PSK)) == "InvalidEncryptionKeyAPIError",
                             "après le redémarrage, la clé est toujours là (clé nulle refusée)", f"résultat : {r}")
            await attendre_traces(ha, relance, rapport)
            await verifier_tuiles(ha, cree, relance, rapport)
            etat = await attendre_etat(ha, ZONES_ENTITE, ZONES_ABSENTES)
            rapport.verifier(etat == ZONES_ABSENTES, f"la tablette masque les emplacements vides ({ZONES_ABSENTES})",
                             f"capteur « Zones masquées » = {etat!r}")
            await asyncio.sleep(3)
            await capturer(ha, args.captures, "installation-ha-2", rapport)
            # Rendez-vous (packages/tab5_reveil.yaml) : ses passages et leurs déclencheurs,
            # pour relire une erreur « Not connected » du journal.
            await rapporter_traces(ha, "tab5_rdv_push", debut, rapport)
            await journal_ha(ha, connexion, deconnexions, rapport)
    finally:
        await tablette.arreter()


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--programme", type=Path, required=True, help="programme de la tablette virtuelle")
    parser.add_argument("--prefs", type=Path, required=True, help="dossier de ses préférences (ESPHOME_PREFDIR)")
    parser.add_argument("--captures", type=Path, required=True, help="dossier des captures et journaux")
    parser.add_argument("--conteneur", default="homeassistant", help="nom du conteneur Home Assistant")
    parser.add_argument("--ha", default=URL_HA, help=f"adresse de HA (défaut {URL_HA})")
    args = parser.parse_args()

    rapport = Rapport()
    try:
        asyncio.run(scenario(args, rapport))
    except Echec as exc:
        rapport.echec(str(exc))
    except Exception as exc:  # noqa: BLE001 — le résumé doit sortir quoi qu'il arrive
        logger.exception("erreur inattendue")
        rapport.echec(f"erreur inattendue : {type(exc).__name__}: {exc}")
    if resume := os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(resume, "a", encoding="utf-8") as f:
            f.write(rapport.resume())
    print(rapport.resume())
    return 0 if rapport.reussi else 1


if __name__ == "__main__":
    sys.exit(main())
