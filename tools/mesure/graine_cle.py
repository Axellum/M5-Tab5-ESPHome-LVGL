"""Branche essai/mesure-invalidation (JAMAIS MERGÉE) : donne une fois la clé API à la tablette virtuelle.

    ESPHOME_PREFDIR=graine ./programme &
    python tools/mesure/graine_cle.py

Sur la plateforme host, millis() suit l'horloge monotone du SYSTÈME : 30 min après le
démarrage du runner, la fenêtre d'appairage d'un programme tout juste lancé est déjà
fermée (« Provisioning closed; rejecting connection », run 39031518689). La clé est donc
donnée une seule fois, au début du job, dans un dossier de préférences « graine » copié
avant chaque passe ; tools/demo/cle_demo.txt la garde pour le pilote.
"""

from __future__ import annotations

import asyncio
import sys
import time
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RACINE / "tools" / "demo"))
sys.path.insert(0, str(RACINE / "tools"))

from demo_pusher import _donner_une_cle, _lire_cle_demo  # noqa: E402


async def main() -> int:
    from aioesphomeapi import APIClient

    fin = time.monotonic() + 300
    while _lire_cle_demo() is None:
        try:
            await _donner_une_cle("127.0.0.1")
        except (Exception, SystemExit) as exc:  # noqa: BLE001 — le programme démarre encore
            if time.monotonic() > fin:
                raise
            print(f"pas encore : {exc}", flush=True)
            await asyncio.sleep(2.0)
    # Preuve que la clé est gardée : une connexion chiffrée avec elle.
    while True:
        try:
            client = APIClient("127.0.0.1", 6053, "", noise_psk=_lire_cle_demo(), client_info="Tab5 graine")
            await client.connect(login=True)
            info = await client.device_info()
            await client.disconnect()
            print(f"clé gardée par {info.name}", flush=True)
            return 0
        except Exception as exc:  # noqa: BLE001
            if time.monotonic() > fin:
                raise
            print(f"connexion avec la clé : {exc}", flush=True)
            await asyncio.sleep(2.0)


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
