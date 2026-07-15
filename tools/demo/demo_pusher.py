# -*- coding: utf-8 -*-
"""tools/demo/demo_pusher.py — Pousse des données synthétiques vers un Tab5 flashé,
sans Home Assistant, pour tester le projet en quelques minutes.

Le firmware Tab5 est *push-only* (docs/decisions/0001-push-only-zero-polling.md) :
il ne fait que réagir aux appels de service ESPHome natifs (`tab5_maj_*`, cf.
Tab5/tab5-api-logic.yaml) et aux mises à jour d'entités "miroir" que Home
Assistant lui envoie normalement. Ce script se fait passer pour HA via
`aioesphomeapi` (la même librairie que l'intégration ESPHome de HA) — sans
jamais installer ni configurer de vrai Home Assistant.

Ne touche à aucun fichier du firmware (Tab5/*.yaml, tab5_custom.cpp/.h) ni à
Tab5/user_entities.yaml : flashez avec Tab5/user_entities.example.yaml tel
quel (voir docs/demo_mode.md).

Usage :
    pip install -r tools/demo/requirements.txt
    python tools/demo/demo_pusher.py --host 192.168.1.42 --key <api_encryption_key>
    python tools/demo/demo_pusher.py --dry-run   # vérifie le format des payloads, sans matériel ni dépendance

Arrêt : Ctrl+C. Rien à nettoyer ailleurs (pas de HA, pas de compte, aucune
config persistée en dehors de l'appareil lui-même).
"""
from __future__ import annotations

import argparse
import asyncio
import logging
from pathlib import Path

from scenarios import (
    SCENES,
    build_alerte_payload,
    build_heures_bulk_payload,
    build_jours_bulk_payload,
    mirror_state_for,
)

logger = logging.getLogger("demo_pusher")

# Pacing repris de HomeAssistant_Config/automations_examples.yaml.example : évite de
# saturer le socket TCP de l'ESP32-P4 (partagé avec le flux audio I2S).
DELAI_ENTRE_BLOCS = 1.0
DELAI_BOUCLE_PLUIE = 0.05
DELAI_BOUCLE_HEURES = 0.15

SERVICES_ATTENDUS = (
    "tab5_maj_meteo_actuelle", "tab5_maj_probabilites", "tab5_maj_alerte_meteo_france",
    "tab5_maj_pluie_1h", "tab5_maj_previsions_heures_bulk", "tab5_maj_previsions_jours_bulk",
    "tab5_maj_clim", "tab5_maj_volet_etat", "tab5_maj_planning", "tab5_maj_info_texte",
)


def _lire_cle_depuis_secrets(repo_root: Path) -> str | None:
    """Essaie de lire api_encryption_key depuis secrets.yaml à la racine du repo."""
    secrets_path = repo_root / "secrets.yaml"
    if not secrets_path.exists():
        return None
    for ligne in secrets_path.read_text(encoding="utf-8").splitlines():
        ligne = ligne.strip()
        if ligne.startswith("api_encryption_key:"):
            return ligne.split(":", 1)[1].strip().strip('"').strip("'")
    return None


def _dry_run() -> None:
    """Affiche les payloads de chaque scène sans se connecter à un appareil."""
    for scene in SCENES:
        print(f"\n=== Scène : {scene.nom} ===")
        print("tab5_maj_meteo_actuelle:", {
            "condition": scene.meteo_condition,
            "temperature": str(scene.meteo_temperature),
            "humidite": str(scene.meteo_humidite),
        })
        print("tab5_maj_probabilites:", scene.probabilites)
        print("tab5_maj_alerte_meteo_france:", build_alerte_payload(**scene.alerte))
        print("tab5_maj_pluie_1h (9 appels):", scene.pluie_1h)
        print("tab5_maj_previsions_heures_bulk (3 appels):")
        for debut in (0, 5, 10):
            print("  -", build_heures_bulk_payload(scene.heures[debut:debut + 5]))
        print("tab5_maj_previsions_jours_bulk:", build_jours_bulk_payload(scene.jours))
        print("tab5_maj_clim:", scene.clim)
        print("tab5_maj_volet_etat:", scene.volet_etat)
        print("tab5_maj_planning:", scene.planning)
        print("tab5_maj_info_texte:", scene.info_texte)
    print("\nOK — tous les payloads respectent le contrat (assertions dans scenarios.py).")


async def _pousser_scene(client, services_par_nom: dict, scene) -> None:
    """Appelle les 10 services tab5_maj_* pour une scène, avec le pacing de prod."""

    async def appeler(nom: str, **data: str) -> None:
        service = services_par_nom.get(nom)
        if service is None:
            logger.warning("Service %s absent du device (firmware différent du contrat attendu ?)", nom)
            return
        await client.execute_service(service, data)

    await appeler(
        "tab5_maj_meteo_actuelle",
        condition=scene.meteo_condition,
        temperature=str(scene.meteo_temperature),
        humidite=str(scene.meteo_humidite),
    )
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_probabilites", **{k: str(v) for k, v in scene.probabilites.items()})
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_alerte_meteo_france", payload=build_alerte_payload(**scene.alerte))
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    for idx, intensite in enumerate(scene.pluie_1h):
        await appeler("tab5_maj_pluie_1h", index_5mn=str(idx), intensite=intensite)
        await asyncio.sleep(DELAI_BOUCLE_PLUIE)

    for debut in (0, 5, 10):
        payload = build_heures_bulk_payload(scene.heures[debut:debut + 5])
        await appeler("tab5_maj_previsions_heures_bulk", payload=payload)
        await asyncio.sleep(DELAI_BOUCLE_HEURES)

    await appeler("tab5_maj_previsions_jours_bulk", payload=build_jours_bulk_payload(scene.jours))
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_clim", **scene.clim)
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    await appeler("tab5_maj_volet_etat", etat_physique=scene.volet_etat)
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    ligne1, ligne2 = scene.planning
    await appeler("tab5_maj_planning", ligne1=ligne1, ligne2=ligne2)
    await asyncio.sleep(DELAI_ENTRE_BLOCS)

    texte, couleur = scene.info_texte
    await appeler("tab5_maj_info_texte", texte=texte, couleur=couleur)


def _gerer_demande_etat(client):
    """Callback appelé quand le device demande l'état d'une entité miroir (au
    moment de l'abonnement) — on répond immédiatement avec la valeur démo."""

    def _repondre(entity_id: str, attribute: str | None) -> None:
        valeur = mirror_state_for(entity_id)
        if valeur is None:
            logger.info("Entité miroir %s inconnue du script démo — ignorée", entity_id)
            return
        client.send_home_assistant_state(entity_id, attribute, valeur)

    return _repondre


def _gerer_appel_service(interactive: bool):
    """Callback appelé quand le firmware envoie un homeassistant.service: (bouton pressé).

    Limitation assumée (voir docs/demo_mode.md) : le popup lumière cible
    id(current_light_entity), un global interne au firmware réglé par appui
    long — invisible depuis le protocole natif. On loggue l'intention (preuve
    que le tactile fonctionne) sans simuler d'état de retour à l'écran.
    """

    def _gerer(call) -> None:
        if not interactive:
            return
        logger.info("Bouton pressé -> %s %s", call.service, dict(call.data))

    return _gerer


async def _run(host: str, key: str, interval: float, interactive: bool) -> None:
    import aioesphomeapi

    client = aioesphomeapi.APIClient(host, 6053, "", noise_psk=key)
    await client.connect(login=True)
    logger.info("Connecté à %s", host)

    _, services = await client.list_entities_services()
    services_par_nom = {s.name: s for s in services}
    manquants = set(SERVICES_ATTENDUS) - services_par_nom.keys()
    if manquants:
        logger.warning("Services absents du device (firmware différent du contrat attendu) : %s", manquants)

    client.subscribe_home_assistant_states_and_services(
        on_state=lambda state: None,
        on_service_call=_gerer_appel_service(interactive),
        on_state_sub=_gerer_demande_etat(client),
    )

    try:
        while True:
            for scene in SCENES:
                logger.info("Scène : %s", scene.nom)
                await _pousser_scene(client, services_par_nom, scene)
                await asyncio.sleep(interval)
    finally:
        await client.disconnect()
        logger.info("Déconnecté — rien à nettoyer ailleurs.")


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", help="IP ou nom mDNS du Tab5 (ex: 192.168.1.42 ou tab5-ha-hmi.local)")
    parser.add_argument("--key", help="api_encryption_key en base64 (défaut : lue depuis secrets.yaml à la racine du repo)")
    parser.add_argument("--interval", type=float, default=20.0, help="Secondes entre chaque scène (défaut 20s)")
    parser.add_argument("--interactive", dest="interactive", action="store_true", default=True,
                         help="Loggue les appuis lumière/clim/volet (activé par défaut)")
    parser.add_argument("--no-interactive", dest="interactive", action="store_false",
                         help="Ignore les appuis, se contente du push passif")
    parser.add_argument("--dry-run", action="store_true",
                         help="Affiche les payloads sans se connecter (aucune dépendance requise)")
    args = parser.parse_args()

    if args.dry_run:
        _dry_run()
        return

    if not args.host:
        parser.error("--host est requis (sauf en --dry-run)")

    repo_root = Path(__file__).resolve().parent.parent.parent
    key = args.key or _lire_cle_depuis_secrets(repo_root)
    if not key:
        parser.error("--key requis, ou un secrets.yaml avec api_encryption_key à la racine du repo")

    try:
        asyncio.run(_run(args.host, key, args.interval, args.interactive))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
