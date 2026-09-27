# -*- coding: utf-8 -*-
"""tools/tab5_logs.py — Journaux de la tablette sur le PC, avec la clé gardée par HA (lot 6b).

`esphome logs` cherche la clé API dans le YAML : depuis la 3.0 (ADR-0020), elle n'y est
plus, et la tablette refuse une connexion en clair une fois sa clé reçue de HA. Ce
script fait la même chose qu'`esphome logs`, avec la clé trouvée par
tools/tab5_cle_api.py (--cle, TAB5_CLE_API, ou le dossier de configuration de HA).

Usage :
    python tools/tab5_logs.py --host 192.168.1.42 --config-ha \\\\192.168.1.10\\config
    python tools/tab5_logs.py --host 192.168.1.42 --fichier journal.txt

Arrêt : Ctrl+C. Le niveau affiché est celui du logger de la tablette (INFO).
"""
from __future__ import annotations

import argparse
import asyncio
import contextlib
import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tab5_cle_api import ajouter_options, trouver_cle  # noqa: E402


async def _lire(hote: str, cle: str | None, fichier: Path | None) -> None:
    from aioesphomeapi import ZERO_NOISE_PSK, APIClient
    from aioesphomeapi.log_parser import parse_log_message
    from aioesphomeapi.log_runner import async_run

    sortie = fichier.open("a", encoding="utf-8") if fichier else None
    cli = APIClient(hote, 6053, "", noise_psk=cle or ZERO_NOISE_PSK,
                    client_info="Tab5 logs (PC)", keepalive=10)

    def on_log(msg) -> None:
        maintenant = datetime.now()
        horodatage = f"[{maintenant:%H:%M:%S}.{maintenant.microsecond // 1000:03}]"
        texte = msg.message.decode("utf8", "backslashreplace")
        for ligne in parse_log_message(texte, horodatage, strip_ansi_escapes=sortie is not None):
            print(ligne, file=sortie or sys.stdout, flush=True)

    arreter = await async_run(cli, on_log, subscribe_states=False)
    try:
        await asyncio.Event().wait()
    finally:
        await arreter()
        if sortie:
            sortie.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="IP ou nom mDNS de la tablette")
    parser.add_argument("--fichier", type=Path, help="écrit les lignes dans ce fichier (sans couleurs)")
    ajouter_options(parser)
    args = parser.parse_args()

    try:
        cle = trouver_cle(args.cle, args.host, args.config_ha)
    except OSError as err:
        parser.error(f"clé de HA illisible : {err}")
    if not cle:
        print("Aucune clé trouvée : clé nulle d'appairage, qui ne marche qu'avec une tablette "
              "encore sans clé (jamais ajoutée à HA) et dans sa fenêtre d'appairage.",
              file=sys.stderr)

    with contextlib.suppress(KeyboardInterrupt):
        asyncio.run(_lire(args.host, cle, args.fichier))


if __name__ == "__main__":
    main()
