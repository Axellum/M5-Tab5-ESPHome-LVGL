# -*- coding: utf-8 -*-
"""tools/demo/demo_pusher.py — Pousse des données synthétiques vers un Tab5 flashé,
sans Home Assistant, pour tester le projet en quelques minutes.

Le firmware Tab5 est *push-only* (docs/decisions/0001-push-only-zero-polling.md) :
il ne fait que réagir aux appels de service ESPHome natifs (`tab5_maj_*`, cf.
Tab5/tab5-api-logic.yaml). Depuis le lot 6a (ADR-0019), les appareils de la maison
arrivent eux aussi par une poussée, `tab5_maj_emplacements`, que le blueprint HA
envoie normalement. Ce script se fait passer pour HA via `aioesphomeapi` (la même
librairie que l'intégration ESPHome de HA) — sans jamais installer ni configurer
de vrai Home Assistant. Depuis la 3.2 (ADR-0023), les tuiles du bas sont des pièces
décrites par HA : la démo pousse les siennes (`tab5_maj_tuiles`, puis leurs états)
quand la tablette a cette action, et rien de plus à un firmware 3.x.

Ne modifie aucun fichier du firmware (Tab5/*.yaml, tab5_custom.cpp/.h ; --dry-run
lit Tab5/tab5-api-logic.yaml) et ne lit pas Tab5/user_entities.yaml : flashez avec
Tab5/user_entities.example.yaml tel quel (voir docs/demo_mode.md).

Clé API (lot 6b, ADR-0020) : aucune n'est compilée depuis la 3.0. Sur une tablette
déjà ajoutée à HA, la démo prend celle que HA garde (--cle, TAB5_CLE_API ou
--config-ha, voir tools/tab5_cle_api.py). Sur une tablette jamais ajoutée à HA, elle
lui en donne une, comme HA le ferait, et la garde dans tools/demo/cle_demo.txt
(gitignoré) : c'est la clé à donner à HA le jour où vous l'ajouterez.

Usage :
    pip install -r tools/demo/requirements.txt
    python tools/demo/demo_pusher.py --host 192.168.1.42
    python tools/demo/demo_pusher.py --host 192.168.1.42 --config-ha \\\\192.168.1.10\\config
    python tools/demo/demo_pusher.py --host 192.168.1.42 --maison-minimale   # zones optionnelles (lot 5)
    python tools/demo/demo_pusher.py --dry-run   # sans matériel ni dépendance : clés de chaque appel
                                                 # = variables de Tab5/tab5-api-logic.yaml, format des payloads

Arrêt : Ctrl+C. Rien à nettoyer ailleurs (pas de HA, pas de compte ; seule la clé
donnée à une tablette neuve est gardée, dans tools/demo/cle_demo.txt).
"""
from __future__ import annotations

import argparse
import asyncio
import datetime as _dt
import logging
import re
import sys
from pathlib import Path
from types import SimpleNamespace

from scenarios import (
    MAISON_MINIMALE,
    SCENES,
    build_alerte_payload,
    build_emplacements_payload,
    build_tuiles_payload,
    build_zones_absentes,
    rangee_de,
    build_energie_historique,
    build_energie_payload,
    ENERGIE_VUES,
    code_pluie,
    build_heures_bulk_payload,
    build_pluie_1h_bulk_payload,
    build_jours_bulk_payload,
    decrire_emplacement,
    pieces_de,
)

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from tab5_cle_api import ajouter_options, trouver_cle  # noqa: E402

logger = logging.getLogger("demo_pusher")

# Clé donnée par la démo à une tablette jamais ajoutée à HA (gitignoré).
FICHIER_CLE_DEMO = Path(__file__).resolve().parent / "cle_demo.txt"

# Le contrat du firmware : ses actions et leurs variables (lues par --dry-run).
API_LOGIC = Path(__file__).resolve().parents[2] / "Tab5" / "tab5-api-logic.yaml"

# Pauses entre les envois d'une scène de la démo. La poussée de HA n'en a plus depuis le
# 01/10/2026 (packages/tab5_push.yaml) ; celles-ci restent telles quelles : le
# rendu hors tablette (tools/rendu/) et tools/sanitizers/ passent aussi par ici.
DELAI_ENTRE_BLOCS = 1.0
DELAI_BOUCLE_HEURES = 0.15

SERVICES_ATTENDUS = (
    "tab5_maj_meteo_actuelle", "tab5_maj_probabilites", "tab5_maj_alerte_meteo_france",
    "tab5_maj_pluie_1h_bulk", "tab5_maj_previsions_heures_bulk", "tab5_maj_previsions_jours_bulk",
    "tab5_maj_clim", "tab5_maj_volet_etat", "tab5_maj_info_texte", "tab5_maj_zones",
    "tab5_maj_emplacements",
)
# Pièces (ADR-0023, firmware 3.2) : absente d'un firmware 3.x, qui garde ses cinq
# tuiles fixes. Comme le blueprint, la démo ne l'appelle alors pas et ne pousse pas les
# états des tuiles (clés tRT) : les emplacements 3.x suffisent.
SERVICE_TUILES = "tab5_maj_tuiles"
# Popup Énergie (ADR-0028) : absent d'un firmware plus ancien. HA ne pousse qu'à
# l'ouverture du popup (événement esphome.tab5_energie) ; la démo pousse aussi à chaque
# scène, pour que le rendu (tools/rendu/capturer.py, qui ne répond à aucun événement)
# trouve ses données.
SERVICE_ENERGIE = "tab5_maj_energie"
SERVICE_ENERGIE_HISTORIQUE = "tab5_maj_energie_historique"


def lire_contrat(chemin: Path = API_LOGIC) -> dict[str, tuple[str, ...]]:
    """{action: variables} du bloc `api: services:` du firmware, dans l'ordre du fichier.

    Lu ligne à ligne, sans PyYAML (--dry-run ne demande aucune dépendance) : une action
    est `    - service: nom`, ses variables les clés à 8 espaces sous `      variables:`.
    tests/test_contrat.py compare ce résultat à une lecture YAML du même fichier."""
    contrat: dict[str, list[str]] = {}
    action = None
    dans_variables = False
    for ligne in chemin.read_text(encoding="utf-8").splitlines():
        if not ligne.strip() or ligne.lstrip().startswith("#"):
            continue
        retrait = len(ligne) - len(ligne.lstrip(" "))
        if m := re.fullmatch(r" {4}- service: (\w+)\s*", ligne):
            action, dans_variables = m.group(1), False
            contrat[action] = []
        elif action is None:
            continue
        elif retrait < 4:
            action = None                      # fin du bloc services:
        elif re.fullmatch(r" {6}variables:\s*", ligne):
            dans_variables = True
        elif dans_variables and retrait <= 6:
            dans_variables = False             # then:, description:…
        elif dans_variables and (m := re.match(r" {8}(\w+):", ligne)):
            contrat[action].append(m.group(1))
    return {nom: tuple(variables) for nom, variables in contrat.items()}


def ecart_de_contrat(attendus, donnees) -> str | None:
    """Ce qui sépare les clés d'un appel des variables déclarées par le firmware, ou None.

    Home Assistant refuse les deux sens (schéma voluptuous sans `extra`) ; aioesphomeapi
    lève un KeyError brut sur une variable manquante."""
    attendus, donnees = set(attendus), set(donnees)
    ecarts = []
    if manquants := attendus - donnees:
        ecarts.append(f"argument(s) manquant(s) {sorted(manquants)}")
    if en_trop := donnees - attendus:
        ecarts.append(f"argument(s) en trop {sorted(en_trop)}")
    return " ; ".join(ecarts) or None


def services_du_contrat(contrat: dict) -> dict:
    """Les actions du contrat sous la forme que donne aioesphomeapi (`.name`, `.args[].name`)."""
    return {nom: SimpleNamespace(name=nom, args=[SimpleNamespace(name=v) for v in variables])
            for nom, variables in contrat.items()}


class _TabletteABlanc:
    """Client aioesphomeapi factice de --dry-run : note chaque appel."""

    def __init__(self):
        self.appels: list[tuple[str, dict]] = []

    async def execute_service(self, service, data):
        self.appels.append((service.name, dict(data)))


class _Ecarts(logging.Handler):
    """Garde les avertissements et erreurs de la démo pendant --dry-run."""

    def __init__(self):
        super().__init__(logging.WARNING)
        self.messages: list[str] = []

    def emit(self, record):
        self.messages.append(record.getMessage())


def _lire_cle_demo() -> str | None:
    """Clé que la démo a donnée à la tablette lors d'un lancement précédent."""
    if FICHIER_CLE_DEMO.exists():
        return FICHIER_CLE_DEMO.read_text(encoding="utf-8").strip() or None
    return None


async def _donner_une_cle(host: str) -> str:
    """Donne une clé à une tablette qui n'en a pas, comme HA le fait à son ajout.

    Connexion avec la clé nulle bien connue (chiffrée, mais sans authentification),
    acceptée seulement par une tablette sans clé et pendant sa fenêtre d'appairage
    (30 min après son démarrage). La clé est gardée dans FICHIER_CLE_DEMO.
    """
    import base64
    import secrets

    from aioesphomeapi import ZERO_NOISE_PSK, APIClient, InvalidEncryptionKeyAPIError

    client = APIClient(host, 6053, "", noise_psk=ZERO_NOISE_PSK, client_info="Tab5 demo pusher")
    try:
        await client.connect(login=True)
    except InvalidEncryptionKeyAPIError:
        raise SystemExit("La tablette a déjà une clé (ajoutée à HA ?) : passez --cle, "
                         "TAB5_CLE_API ou --config-ha.") from None
    try:
        info = await client.device_info()
        if not info.api_encryption_provisionable:
            raise SystemExit("Firmware d'avant la 3.0 : passez sa clé API avec --cle.")
        cle = base64.b64encode(secrets.token_bytes(32))
        if not await client.noise_encryption_set_key(cle):
            raise SystemExit("La tablette a refusé la clé (fenêtre d'appairage fermée ? "
                             "Redémarrez-la puis relancez la démo).")
    finally:
        await client.disconnect()
    FICHIER_CLE_DEMO.write_text(cle.decode() + "\n", encoding="utf-8")
    logger.info("Clé donnée à la tablette et gardée dans %s : à donner à HA quand vous "
                "ajouterez la tablette.", FICHIER_CLE_DEMO)
    # La tablette coupe ses connexions pour passer à la nouvelle clé.
    await asyncio.sleep(2)
    return cle.decode()


def _dry_run(absentes: frozenset) -> None:
    """Joue chaque scène contre le contrat du firmware, sans appareil ni dépendance.

    Les actions et leurs variables sont lues dans Tab5/tab5-api-logic.yaml
    (lire_contrat) : chaque appel de la démo passe par la même garde que sur une vraie
    tablette (_appeler), qui refuse une variable manquante ou en trop, et par les
    assertions de scenarios.py (format des payloads). Deux passes : firmware récent
    (avec tab5_maj_tuiles, appels affichés) et firmware 3.x (sans, vérifiée seulement).
    Au moindre écart : message, puis sortie en erreur (étape de la CI)."""
    contrat = lire_contrat(API_LOGIC)
    print(f"Contrat du firmware : {API_LOGIC.parent.name}/{API_LOGIC.name}, "
          f"{len(contrat)} actions.")
    ecarts = _Ecarts()
    logger.addHandler(ecarts)
    try:
        for avec_tuiles in (True, False):
            services = services_du_contrat(
                {n: v for n, v in contrat.items() if avec_tuiles or n != SERVICE_TUILES})
            tablette = _TabletteABlanc()
            appels_par_scene = []
            asyncio.run(_pousser_zones(tablette, services, absentes))
            for scene in SCENES:
                debut = len(tablette.appels)
                asyncio.run(_pousser_scene(tablette, services, scene, absentes, attendre=_sans_attente))
                appels_par_scene.append((scene.nom, tablette.appels[debut:]))
            if not avec_tuiles:
                print()
                print(f"=== Firmware 3.x (sans {SERVICE_TUILES}) : "
                      f"{len(tablette.appels)} appels vérifiés, non affichés ===")
                continue
            print("tab5_maj_zones:", tablette.appels[0][1])
            for nom_scene, appels in appels_par_scene:
                print()
                print(f"=== Scène : {nom_scene} ===")
                for nom, donnees in appels:
                    print(f"{nom}:", donnees)
    finally:
        logger.removeHandler(ecarts)
    print()
    if ecarts.messages:
        for message in ecarts.messages:
            print("ÉCART :", message)
        raise SystemExit(f"{len(ecarts.messages)} écart(s) entre la démo et le contrat du firmware.")
    print("OK — chaque appel a exactement les variables déclarées par le firmware, et tous les "
          "payloads respectent leur format (assertions dans scenarios.py).")


async def _sans_attente(_secondes: float) -> None:
    """--dry-run : pas de pacing."""


def _clim_poussee(scene, absentes: frozenset) -> dict | None:
    """La clim de la scène, si elle est poussée : la tuile de la clim du blueprint la suit."""
    return None if "clim" in absentes else scene.clim


async def _appeler(client, services_par_nom: dict, nom: str, **data: str) -> None:
    """Appelle un service tab5_maj_* du firmware, en vérifiant ses arguments."""
    service = services_par_nom.get(nom)
    if service is None:
        logger.warning("Service %s absent du device (firmware différent du contrat attendu ?)", nom)
        return
    # Garde-fou de contrat : aioesphomeapi fait `data[arg.name]` pour CHAQUE
    # argument déclaré par le firmware — un argument manquant lève un KeyError
    # brut en plein milieu d'une scène. On préfère un message lisible qui
    # nomme le service et l'argument (c'est exactement ce qui est arrivé quand
    # `meteo_id` a été ajouté à tab5_maj_info_texte sans mettre la démo à jour).
    # Un argument en trop est refusé aussi : Home Assistant le refuserait.
    if ecart := ecart_de_contrat((arg.name for arg in service.args), data):
        logger.error("%s : %s — le firmware a changé de contrat, mettre à jour "
                     "tools/demo/. Appel ignoré.", nom, ecart)
        return
    await client.execute_service(service, data)


async def _pousser_zones(client, services_par_nom: dict, absentes: frozenset) -> None:
    """Réponse de HA à l'événement esphome.tab5_zones (lot 5) : les zones absentes."""
    await _appeler(client, services_par_nom, "tab5_maj_zones", absentes=build_zones_absentes(absentes))


async def _pousser_energie(client, services_par_nom: dict, vues=tuple(ENERGIE_VUES),
                           aujourd_hui: _dt.date | None = None) -> None:
    """Réponse de script.tab5_energie (packages/tab5_energie.yaml) : l'instantané, puis
    l'historique de chaque vue de `vues`. Rien sur un firmware sans le popup Énergie."""
    if SERVICE_ENERGIE not in services_par_nom:
        return
    jour = aujourd_hui or _dt.date.today()
    await _appeler(client, services_par_nom, SERVICE_ENERGIE, payload=build_energie_payload())
    for vue in vues:
        await _appeler(client, services_par_nom, SERVICE_ENERGIE_HISTORIQUE,
                       **build_energie_historique(vue, jour))


async def _pousser_scene(client, services_par_nom: dict, scene, absentes: frozenset,
                         attendre=None, aujourd_hui: _dt.date | None = None) -> None:
    """Appelle les services tab5_maj_* d'une scène, avec le pacing de prod. Comme le
    package HA (lot 5b), rien pour la clim ni le volet quand leur zone est absente.
    Les définitions des pièces (tab5_maj_tuiles) partent à chaque scène, juste avant
    les états : la tablette ne réécrit ses définitions en NVS que si elles changent
    (ADR-0023), et le rendu (tools/rendu/capturer.py) n'a pas d'autre moment pour les
    recevoir. `attendre` remplace asyncio.sleep (--dry-run : aucune attente).
    `aujourd_hui` : date de l'historique de l'énergie (le rendu la fige)."""

    attendre = attendre or asyncio.sleep

    async def appeler(nom: str, **data: str) -> None:
        await _appeler(client, services_par_nom, nom, **data)

    await appeler(
        "tab5_maj_meteo_actuelle",
        condition=scene.meteo_condition,
        temperature=str(scene.meteo_temperature),
        humidite=str(scene.meteo_humidite),
    )
    await attendre(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_probabilites", **{k: str(v) for k, v in scene.probabilites.items()})
    await attendre(DELAI_ENTRE_BLOCS)

    # Code de pluie calculé maintenant : « dans N mn » part d'un epoch à jour.
    await appeler("tab5_maj_alerte_meteo_france",
                  payload=build_alerte_payload(phrase_pluie=code_pluie(*scene.pluie), **scene.alerte))
    await attendre(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_pluie_1h_bulk", payload=build_pluie_1h_bulk_payload(scene.pluie_1h))
    await attendre(DELAI_ENTRE_BLOCS)

    # Deux blocs comme la prod depuis le 25/09/2026 : l'écran n'affiche que les
    # créneaux 0-9 (deux pages horaires), le bloc 10-14 n'était jamais peint.
    for debut in (0, 5):
        payload = build_heures_bulk_payload(scene.heures[debut:debut + 5])
        await appeler("tab5_maj_previsions_heures_bulk", payload=payload)
        await attendre(DELAI_BOUCLE_HEURES)

    await appeler("tab5_maj_previsions_jours_bulk", payload=build_jours_bulk_payload(scene.jours))
    await attendre(DELAI_ENTRE_BLOCS)

    if "clim" not in absentes:
        await appeler("tab5_maj_clim", **scene.clim)
        await attendre(DELAI_ENTRE_BLOCS)

    if "volet" not in absentes:
        await appeler("tab5_maj_volet_etat", etat_physique=scene.volet_etat)
        await attendre(DELAI_ENTRE_BLOCS)

    # Plus de tab5_maj_planning, comme HA depuis le 08/09/2026 : la tablette dérive
    # le bandeau des horaires de la poussée des jours.
    texte, couleur, meteo_id = scene.info_texte
    await appeler("tab5_maj_info_texte", texte=texte, couleur=couleur, meteo_id=meteo_id)
    await attendre(DELAI_ENTRE_BLOCS)

    # Pièces (ADR-0023) : les définitions d'abord, puis les emplacements 3.x suivis des
    # états des tuiles (clés tRT), comme le blueprint à chaque connexion. Sans l'action
    # (firmware 3.x), les emplacements seuls.
    pieces = None
    if SERVICE_TUILES in services_par_nom:
        pieces = pieces_de(absentes)
        await appeler(SERVICE_TUILES, payload=build_tuiles_payload(pieces, rangee_de(absentes)))
        await attendre(DELAI_ENTRE_BLOCS)

    # Emplacements de la maison (lot 6a), comme le blueprint à chaque connexion.
    await appeler("tab5_maj_emplacements",
                  payload=build_emplacements_payload(absentes, pieces, _clim_poussee(scene, absentes),
                                                     rangee_de(absentes)))

    # Popup Énergie (ADR-0028) : la maison solaire de la démo, sauf maison minimale.
    if not absentes:
        await attendre(DELAI_ENTRE_BLOCS)
        await _pousser_energie(client, services_par_nom, aujourd_hui=aujourd_hui)


def _gerer_appel_service(interactive: bool, repondre_zones, pieces: dict | None = None,
                        repondre_energie=None):
    """Callback appelé quand le firmware envoie un homeassistant.event: (bouton pressé,
    demande à HA ; depuis l'ADR-0025 il n'envoie plus de homeassistant.service:, un
    firmware 3.1 ou plus ancien si).

    L'événement esphome.tab5_zones (lot 5) reçoit la réponse que ferait le package HA
    (automatisation tab5_zones_reponse) : repondre_zones() la planifie.

    Limitation assumée (voir docs/demo_mode.md) : le popup lumière cible
    id(current_light_slot), un global interne au firmware réglé par appui
    long — invisible depuis le protocole natif. On loggue l'intention (preuve
    que le tactile fonctionne) sans simuler d'état de retour à l'écran.

    `pieces` : celles poussées (firmware 3.2), pour nommer la tuile (« tRT ») ou la
    pièce (« pR ») d'une commande dans le journal.

    L'événement esphome.tab5_energie (popup Énergie ouvert, ou sa vue changée, ADR-0028)
    reçoit la réponse de script.tab5_energie : repondre_energie(vue) la planifie.
    """

    def _gerer(call) -> None:
        if getattr(call, "is_event", False) and call.service == "esphome.tab5_zones":
            repondre_zones()
            return
        if getattr(call, "is_event", False) and call.service == "esphome.tab5_energie":
            if repondre_energie is not None:
                repondre_energie(dict(call.data).get("vue", "heures"))
            return
        if not interactive:
            return
        if getattr(call, "is_event", False) and call.service == "esphome.tab5_action":
            # Commande d'un emplacement (lot 6a) ou d'une tuile de pièce (ADR-0023) : le
            # blueprint l'appliquerait à l'entité choisie ; la démo la journalise seulement.
            data = dict(call.data)
            cle = data.get("emplacement", "")
            if pieces is not None and cle:
                logger.info("Commande -> %s : %s %s", decrire_emplacement(cle, pieces),
                            data.get("action", ""), data.get("valeur", ""))
            else:
                logger.info("Commande -> %s", data)
            return
        logger.info("Bouton pressé -> %s %s", call.service, dict(call.data))

    return _gerer


async def _run(host: str, key: str | None, interval: float, interactive: bool, absentes: frozenset) -> None:
    import aioesphomeapi

    if not key:
        key = await _donner_une_cle(host)
    client = aioesphomeapi.APIClient(host, 6053, "", noise_psk=key)
    await client.connect(login=True)
    logger.info("Connecté à %s", host)

    _, services = await client.list_entities_services()
    services_par_nom = {s.name: s for s in services}
    manquants = set(SERVICES_ATTENDUS) - services_par_nom.keys()
    if manquants:
        logger.warning("Services absents du device (firmware différent du contrat attendu) : %s", manquants)
    pieces = pieces_de(absentes) if SERVICE_TUILES in services_par_nom else None
    if pieces is None:
        logger.info("Pas d'action %s (firmware d'avant la 3.2) : les tuiles gardent leurs "
                    "emplacements 3.x, pièces non poussées", SERVICE_TUILES)
    else:
        logger.info("Pièces (ADR-0023) : %d pièce(s), %d tuile(s) poussées avant les états",
                    len(pieces), sum(len(p.tuiles) for p in pieces.values()))

    # Zones (lot 5) : la tablette ne redemande qu'une fois par connexion de HA, et
    # garde sa dernière liste en NVS. On répond donc aussi d'office au démarrage :
    # une démo complète rétablit ainsi les zones d'une démo « maison minimale ».
    reponse = build_zones_absentes(absentes)
    taches: set = set()

    def repondre_zones() -> None:
        logger.info("Zones absentes -> %s", reponse or "(aucune)")
        tache = asyncio.get_running_loop().create_task(
            _pousser_zones(client, services_par_nom, absentes))
        taches.add(tache)
        tache.add_done_callback(taches.discard)

    def repondre_energie(vue: str) -> None:
        if absentes:
            return   # maison minimale : pas d'installation solaire
        vues = (vue,) if vue in ENERGIE_VUES else ("heures",)
        logger.info("Popup Énergie -> instantané et %s", vues[0])
        tache = asyncio.get_running_loop().create_task(
            _pousser_energie(client, services_par_nom, vues))
        taches.add(tache)
        tache.add_done_callback(taches.discard)

    # Plus d'abonnement de la tablette à des entités (lot 6a) : on_state_sub reste
    # branché pour un firmware plus ancien, sans rien y répondre.
    client.subscribe_home_assistant_states_and_services(
        on_state=lambda state: None,
        on_service_call=_gerer_appel_service(interactive, repondre_zones, pieces, repondre_energie),
        on_state_sub=lambda entity_id, attribute: logger.info(
            "Abonnement à %s demandé par un firmware d'avant le lot 6a — ignoré", entity_id),
    )
    repondre_zones()

    try:
        while True:
            for scene in SCENES:
                logger.info("Scène : %s", scene.nom)
                await _pousser_scene(client, services_par_nom, scene, absentes)
                await asyncio.sleep(interval)
    finally:
        await client.disconnect()
        logger.info("Déconnecté — rien à nettoyer ailleurs.")


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", help="IP ou nom mDNS du Tab5 (ex: 192.168.1.42 ou tab5-ha-hmi.local)")
    ajouter_options(parser)
    # Ancien nom de --cle (avant la 3.0, la clé venait de secrets.yaml).
    parser.add_argument("--key", dest="cle", help=argparse.SUPPRESS)
    parser.add_argument("--interval", type=float, default=20.0, help="Secondes entre chaque scène (défaut 20s)")
    parser.add_argument("--interactive", dest="interactive", action="store_true", default=True,
                         help="Loggue les appuis lumière/clim/volet (activé par défaut)")
    parser.add_argument("--no-interactive", dest="interactive", action="store_false",
                         help="Ignore les appuis, se contente du push passif")
    parser.add_argument("--maison-minimale", action="store_true",
                         help="Zones optionnelles (lot 5) : sans clim, TV, téléphone, LEDs, serre, "
                              "pots 3 à 5, volet ni planning ; en 3.2, une seule pièce (PC et deux "
                              "lampes). Redémarrer la tablette en passant d'une démo complète à "
                              "celle-ci : une donnée déjà reçue garde sa zone")
    parser.add_argument("--dry-run", action="store_true",
                         help="Affiche les payloads sans se connecter (aucune dépendance requise)")
    args = parser.parse_args()
    absentes = MAISON_MINIMALE if args.maison_minimale else frozenset()

    if args.dry_run:
        _dry_run(absentes)
        return

    if not args.host:
        parser.error("--host est requis (sauf en --dry-run)")

    try:
        key = trouver_cle(args.cle, args.host, args.config_ha) or _lire_cle_demo()
    except OSError as err:
        parser.error(f"clé de HA illisible : {err}")

    try:
        asyncio.run(_run(args.host, key, args.interval, args.interactive, absentes))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
