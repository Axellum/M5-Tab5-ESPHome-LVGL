# -*- coding: utf-8 -*-
"""tools/tab5_redonner_cle.py — Redonner à la tablette la clé API que Home Assistant garde déjà.

Cas (docs/troubleshooting.md) : une tablette ou une Waveshare 7B que HA connaît,
réinstallée depuis la page /install/ AVEC effacement. L'effacement vide sa NVS, clé API
comprise ; HA essaie encore son ancienne clé, se voit refuser (« Invalid encryption key »)
et ouvre un flux de réauthentification (`reauth_encryption_removed_confirm`) ; tant qu'il
attend, HA ne réessaie plus. Confirmer ce flux dans l'interface de HA suffit (HA redonne
alors sa clé tout seul) ; cet outil fait la même chose sans l'interface, avec la clé de HA
trouvée par tools/tab5_cle_api.py (--cle, TAB5_CLE_API ou --config-ha), jamais affichée :

1. connexion avec la clé nulle bien connue de Noise (ZERO_NOISE_PSK) : seule une tablette
   sans clé l'accepte, et seulement dans sa fenêtre d'appairage (`provisioning:`, les
   30 minutes qui suivent son démarrage) ;
2. `noise_encryption_set_key` avec la clé de HA ;
3. connexion de contrôle avec cette clé.

Reste à recharger l'entrée ESPHome de la tablette dans HA (Paramètres → Appareils et
services → ESPHome → la tablette → ⋮ → Recharger) : le rechargement abandonne le flux de
réauth et HA se reconnecte avec sa clé. Rejoué par la CI « HA neuf »
(tools/installation_ha/reinstallation.py), et fait à la main le 19/10/2026 sur la 7B de
l'auteur.

Usage :
    python tools/tab5_redonner_cle.py --host 192.168.1.42 --config-ha \\\\192.168.1.10\\config
"""
from __future__ import annotations

import argparse
import asyncio
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tab5_cle_api import ajouter_options, trouver_cle  # noqa: E402

PORT_API = 6053
# La tablette coupe ses connexions pour passer à la nouvelle clé (tools/demo/demo_pusher.py).
PAUSE_NOUVELLE_CLE_S = 2.0

RECHARGER = ("rechargez maintenant son entrée dans Home Assistant : Paramètres → Appareils et "
             "services → ESPHome → la tablette → ⋮ → Recharger.")
# Résultat de redonner_cle() → (réussi, message). Aucun message ne contient la clé.
MESSAGES = {
    "donnee": (True, "Clé de Home Assistant redonnée à la tablette, qui l'accepte : " + RECHARGER),
    "deja": (True, "La tablette a déjà la clé de Home Assistant : " + RECHARGER),
    "autre_cle": (False, "La tablette a une clé, mais pas celle de Home Assistant : elle n'a pas "
                         "été effacée. Rien n'a été changé."),
    "fenetre_fermee": (False, "Fenêtre d'appairage fermée (plus de 30 minutes depuis le démarrage) : "
                              "redémarrez la tablette (bouton reset ou débranchée), puis relancez "
                              "l'outil dans les 30 minutes."),
    "ancien_firmware": (False, "Firmware d'avant la 3.0 : il ne reçoit pas de clé par le réseau."),
    "refusee": (False, "La tablette a refusé la clé (fenêtre d'appairage fermée entre-temps ?) : "
                       "redémarrez-la, puis relancez l'outil."),
    "pas_ouverte": (False, "La tablette a pris la clé mais ne s'ouvre pas avec : relancez l'outil ; "
                           "si ça recommence, voir docs/troubleshooting.md."),
    "injoignable": (False, "Tablette injoignable sur le port 6053 : allumée, sur le Wi-Fi, bonne "
                           "adresse ?"),
}


async def _ouvre(hote: str, port: int, cle: str) -> bool:
    """La tablette accepte-t-elle une connexion avec `cle` ?"""
    from aioesphomeapi import APIClient, APIConnectionError

    client = APIClient(hote, port, "", noise_psk=cle, client_info="Tab5 redonner la clé (contrôle)")
    try:
        await asyncio.wait_for(client.connect(login=True), 20)
        await client.device_info()
        return True
    except (APIConnectionError, asyncio.TimeoutError):
        return False
    finally:
        await client.disconnect(force=True)


async def redonner_cle(hote: str, cle: str, port: int = PORT_API) -> str:
    """Donne `cle` (base64, celle que garde HA) à la tablette `hote`, sans clé depuis son
    effacement, puis vérifie qu'elle l'ouvre. Renvoie une clé de MESSAGES."""
    from aioesphomeapi import (ZERO_NOISE_PSK, APIClient, APIConnectionError, DisconnectReason,
                               InvalidEncryptionKeyAPIError)

    client = APIClient(hote, port, "", noise_psk=ZERO_NOISE_PSK, client_info="Tab5 redonner la clé (PC)")
    fermee = False

    def _fermeture(evenement) -> None:
        nonlocal fermee
        fermee = fermee or evenement.reason is DisconnectReason.PROVISIONING_CLOSED

    client.add_connection_closed_callback(_fermeture)
    try:
        await asyncio.wait_for(client.connect(login=True), 20)
        info = await client.device_info()
    except InvalidEncryptionKeyAPIError:
        # Clé nulle refusée : la tablette a une clé. Celle de HA, ou une autre ?
        await client.disconnect(force=True)
        return "deja" if await _ouvre(hote, port, cle) else "autre_cle"
    except (APIConnectionError, asyncio.TimeoutError):
        await client.disconnect(force=True)
        return "fenetre_fermee" if fermee else "injoignable"
    try:
        if not info.api_encryption_provisionable:
            return "ancien_firmware"
        acceptee = await client.noise_encryption_set_key(cle.encode())
    except APIConnectionError:
        return "fenetre_fermee" if fermee else "refusee"
    finally:
        await client.disconnect(force=True)
    if not acceptee:
        return "refusee"
    await asyncio.sleep(PAUSE_NOUVELLE_CLE_S)
    return "donnee" if await _ouvre(hote, port, cle) else "pas_ouverte"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="IP ou nom mDNS de la tablette")
    parser.add_argument("--port", type=int, default=PORT_API, help=f"port de l'API (défaut {PORT_API})")
    ajouter_options(parser)
    args = parser.parse_args()

    try:
        cle = trouver_cle(args.cle, args.host, args.config_ha)
    except OSError as err:
        parser.error(f"clé de HA illisible : {err}")
    if not cle:
        parser.error("aucune clé trouvée : Home Assistant ne connaît pas cette tablette à cette "
                     "adresse (--host), ou il faut --config-ha / TAB5_CONFIG_HA.")
    reussi, message = MESSAGES[asyncio.run(redonner_cle(args.host, cle, args.port))]
    print(message, file=sys.stdout if reussi else sys.stderr)
    return 0 if reussi else 1


if __name__ == "__main__":
    sys.exit(main())
