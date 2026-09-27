# -*- coding: utf-8 -*-
"""tools/demo/demo_pusher.py — Pousse des données synthétiques vers un Tab5 flashé,
sans Home Assistant, pour tester le projet en quelques minutes.

Le firmware Tab5 est *push-only* (docs/decisions/0001-push-only-zero-polling.md) :
il ne fait que réagir aux appels de service ESPHome natifs (`tab5_maj_*`, cf.
Tab5/tab5-api-logic.yaml). Depuis le lot 6a (ADR-0019), les appareils de la maison
arrivent eux aussi par une poussée, `tab5_maj_emplacements`, que le blueprint HA
envoie normalement. Ce script se fait passer pour HA via `aioesphomeapi` (la même
librairie que l'intégration ESPHome de HA) — sans jamais installer ni configurer
de vrai Home Assistant.

Ne touche à aucun fichier du firmware (Tab5/*.yaml, tab5_custom.cpp/.h) ni à
Tab5/user_entities.yaml : flashez avec Tab5/user_entities.example.yaml tel
quel (voir docs/demo_mode.md).

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
    python tools/demo/demo_pusher.py --dry-run   # vérifie le format des payloads, sans matériel ni dépendance

Arrêt : Ctrl+C. Rien à nettoyer ailleurs (pas de HA, pas de compte ; seule la clé
donnée à une tablette neuve est gardée, dans tools/demo/cle_demo.txt).
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import sys
from pathlib import Path

from scenarios import (
    MAISON_MINIMALE,
    SCENES,
    build_alerte_payload,
    build_emplacements_payload,
    build_zones_absentes,
    code_pluie,
    build_heures_bulk_payload,
    build_pluie_1h_bulk_payload,
    build_jours_bulk_payload,
)

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from tab5_cle_api import ajouter_options, trouver_cle  # noqa: E402

logger = logging.getLogger("demo_pusher")

# Clé donnée par la démo à une tablette jamais ajoutée à HA (gitignoré).
FICHIER_CLE_DEMO = Path(__file__).resolve().parent / "cle_demo.txt"

# Pacing repris de HomeAssistant_Config/packages/tab5_push.yaml : évite de
# saturer le socket TCP de l'ESP32-P4 (partagé avec le flux audio I2S).
DELAI_ENTRE_BLOCS = 1.0
DELAI_BOUCLE_HEURES = 0.15

SERVICES_ATTENDUS = (
    "tab5_maj_meteo_actuelle", "tab5_maj_probabilites", "tab5_maj_alerte_meteo_france",
    "tab5_maj_pluie_1h_bulk", "tab5_maj_previsions_heures_bulk", "tab5_maj_previsions_jours_bulk",
    "tab5_maj_clim", "tab5_maj_volet_etat", "tab5_maj_info_texte", "tab5_maj_zones",
    "tab5_maj_emplacements",
)


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
    """Affiche les payloads de chaque scène sans se connecter à un appareil."""
    print("tab5_maj_zones:", {"absentes": build_zones_absentes(absentes)})
    print("tab5_maj_emplacements:", build_emplacements_payload(absentes))
    for scene in SCENES:
        print(f"\n=== Scène : {scene.nom} ===")
        print("tab5_maj_meteo_actuelle:", {
            "condition": scene.meteo_condition,
            "temperature": str(scene.meteo_temperature),
            "humidite": str(scene.meteo_humidite),
        })
        print("tab5_maj_probabilites:", scene.probabilites)
        print("tab5_maj_alerte_meteo_france:",
              build_alerte_payload(phrase_pluie=code_pluie(*scene.pluie), **scene.alerte))
        print("tab5_maj_pluie_1h_bulk:", build_pluie_1h_bulk_payload(scene.pluie_1h))
        print("tab5_maj_previsions_heures_bulk (2 appels):")
        for debut in (0, 5):
            print("  -", build_heures_bulk_payload(scene.heures[debut:debut + 5]))
        print("tab5_maj_previsions_jours_bulk:", build_jours_bulk_payload(scene.jours))
        print("tab5_maj_clim:", "(zone absente, rien)" if "clim" in absentes else scene.clim)
        print("tab5_maj_volet_etat:", "(zone absente, rien)" if "volet" in absentes else scene.volet_etat)
        print("tab5_maj_info_texte:", scene.info_texte)
    print("\nOK — tous les payloads respectent le contrat (assertions dans scenarios.py).")


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
    attendus = {arg.name for arg in service.args}
    if manquants := attendus - data.keys():
        logger.error("%s : argument(s) %s manquant(s) — le firmware a changé de "
                     "contrat, mettre à jour tools/demo/. Appel ignoré.",
                     nom, sorted(manquants))
        return
    await client.execute_service(service, data)


async def _pousser_scene(client, services_par_nom: dict, scene, absentes: frozenset) -> None:
    """Appelle les services tab5_maj_* d'une scène, avec le pacing de prod. Comme le
    package HA (lot 5b), rien pour la clim ni le volet quand leur zone est absente."""

    async def appeler(nom: str, **data: str) -> None:
        await _appeler(client, services_par_nom, nom, **data)

    await appeler(
        "tab5_maj_meteo_actuelle",
        condition=scene.meteo_condition,
        temperature=str(scene.meteo_temperature),
        humidite=str(scene.meteo_humidite),
    )
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_probabilites", **{k: str(v) for k, v in scene.probabilites.items()})
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    # Code de pluie calculé maintenant : « dans N mn » part d'un epoch à jour.
    await appeler("tab5_maj_alerte_meteo_france",
                  payload=build_alerte_payload(phrase_pluie=code_pluie(*scene.pluie), **scene.alerte))
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_pluie_1h_bulk", payload=build_pluie_1h_bulk_payload(scene.pluie_1h))
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    # Deux blocs comme la prod depuis le 25/09/2026 : l'écran n'affiche que les
    # créneaux 0-9 (deux pages horaires), le bloc 10-14 n'était jamais peint.
    for debut in (0, 5):
        payload = build_heures_bulk_payload(scene.heures[debut:debut + 5])
        await appeler("tab5_maj_previsions_heures_bulk", payload=payload)
        await asyncio.sleep(DELAI_BOUCLE_HEURES)

    await appeler("tab5_maj_previsions_jours_bulk", payload=build_jours_bulk_payload(scene.jours))
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    if "clim" not in absentes:
        await appeler("tab5_maj_clim", **scene.clim)
        await asyncio.sleep(DELAI_ENTRE_BLOCS)

    if "volet" not in absentes:
        await appeler("tab5_maj_volet_etat", etat_physique=scene.volet_etat)
        await asyncio.sleep(DELAI_ENTRE_BLOCS)

    # Plus de tab5_maj_planning, comme HA depuis le 08/09/2026 : la tablette dérive
    # le bandeau des horaires de la poussée des jours.
    texte, couleur, meteo_id = scene.info_texte
    await appeler("tab5_maj_info_texte", texte=texte, couleur=couleur, meteo_id=meteo_id)
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    # Emplacements de la maison (lot 6a), comme le blueprint à chaque connexion.
    await appeler("tab5_maj_emplacements", payload=build_emplacements_payload(absentes))


def _gerer_appel_service(interactive: bool, repondre_zones):
    """Callback appelé quand le firmware envoie un homeassistant.service: (bouton pressé)
    ou un homeassistant.event:.

    L'événement esphome.tab5_zones (lot 5) reçoit la réponse que ferait le package HA
    (automatisation tab5_zones_reponse) : repondre_zones() la planifie.

    Limitation assumée (voir docs/demo_mode.md) : le popup lumière cible
    id(current_light_slot), un global interne au firmware réglé par appui
    long — invisible depuis le protocole natif. On loggue l'intention (preuve
    que le tactile fonctionne) sans simuler d'état de retour à l'écran.
    """

    def _gerer(call) -> None:
        if getattr(call, "is_event", False) and call.service == "esphome.tab5_zones":
            repondre_zones()
            return
        if not interactive:
            return
        if getattr(call, "is_event", False) and call.service == "esphome.tab5_action":
            # Commande d'un emplacement (lot 6a) : le blueprint l'appliquerait à
            # l'entité choisie ; la démo la journalise seulement.
            logger.info("Commande -> %s", dict(call.data))
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

    # Zones (lot 5) : la tablette ne redemande qu'une fois par connexion de HA, et
    # garde sa dernière liste en NVS. On répond donc aussi d'office au démarrage :
    # une démo complète rétablit ainsi les zones d'une démo « maison minimale ».
    reponse = build_zones_absentes(absentes)
    taches: set = set()

    def repondre_zones() -> None:
        logger.info("Zones absentes -> %s", reponse or "(aucune)")
        tache = asyncio.get_running_loop().create_task(
            _appeler(client, services_par_nom, "tab5_maj_zones", absentes=reponse))
        taches.add(tache)
        tache.add_done_callback(taches.discard)

    # Plus d'abonnement de la tablette à des entités (lot 6a) : on_state_sub reste
    # branché pour un firmware plus ancien, sans rien y répondre.
    client.subscribe_home_assistant_states_and_services(
        on_state=lambda state: None,
        on_service_call=_gerer_appel_service(interactive, repondre_zones),
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
                              "pots 3 à 5, volet ni planning. Redémarrer la tablette en passant "
                              "d'une démo complète à celle-ci : une donnée déjà reçue garde sa zone")
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
