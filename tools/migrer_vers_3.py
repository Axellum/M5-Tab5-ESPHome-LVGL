# -*- coding: utf-8 -*-
"""tools/migrer_vers_3.py — Passer une tablette 2.x à la 3.0 par le réseau (lot 6b, ADR-0020).

Le firmware 2.x refuse une OTA en clair : il ne l'accepte que chiffrée avec sa clé API,
celle de votre ancien `secrets.yaml` (`api_encryption_key`). La 3.0 n'a plus de clé dans
le YAML, donc `esphome upload` enverrait en clair et serait refusé. Ce script fait ce
seul envoi avec l'ancienne clé, sans l'afficher ; ensuite, la 3.0 accepte les OTA en
clair, protégées par la signature du firmware.

Avant : créer la clé de signature et compiler (docs/installation.md, « Passer à la 3.0 ») :
    python -m espsecure generate-signing-key --version 2 --scheme rsa3072 tab5_signature.pem
    python -m esphome compile tab5-ha-hmi.yaml

Usage :
    python tools/migrer_vers_3.py --host 192.168.1.42
    python tools/migrer_vers_3.py --host 192.168.1.42 --secrets ancien/secrets.yaml --bin chemin/firmware.ota.bin

Après : la tablette n'a plus d'identifiants Wi-Fi (ils étaient compilés) et ouvre son
point d'accès « Tab5 Fallback AP » ; puis Home Assistant demande de confirmer la fin du
chiffrement et lui donne une nouvelle clé. Voir docs/installation.md.
"""
from __future__ import annotations

import argparse
import logging
import os
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PORT_OTA = 3232


def lire_ancienne_cle(secrets: Path) -> str | None:
    """`api_encryption_key` d'un secrets.yaml 2.x, ou None."""
    for ligne in secrets.read_text(encoding="utf-8").splitlines():
        ligne = ligne.strip()
        if ligne.startswith("api_encryption_key:"):
            return ligne.split(":", 1)[1].split("#", 1)[0].strip().strip('"').strip("'") or None
    return None


def binaire_par_defaut() -> Path:
    """firmware.ota.bin de la dernière compilation de tab5-ha-hmi.yaml."""
    donnees = Path(os.environ.get("ESPHOME_DATA_DIR") or REPO / ".esphome")
    return donnees / "build" / "tab5-ha-hmi" / "build" / "firmware.ota.bin"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="IP de la tablette")
    parser.add_argument("--secrets", type=Path, default=REPO / "secrets.yaml",
                        help="secrets.yaml de la 2.x (défaut : celui de la racine du dépôt)")
    parser.add_argument("--bin", type=Path, help="firmware.ota.bin de la 3.0 (défaut : la dernière compilation)")
    args = parser.parse_args()

    if not args.secrets.is_file():
        parser.error(f"{args.secrets} introuvable : il faut l'ancienne clé API de la tablette")
    cle = lire_ancienne_cle(args.secrets)
    if not cle:
        parser.error(f"pas d'api_encryption_key dans {args.secrets}")
    binaire = args.bin or binaire_par_defaut()
    if not binaire.is_file():
        parser.error(f"{binaire} introuvable : compilez d'abord (python -m esphome compile tab5-ha-hmi.yaml)")

    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    from esphome import espota2

    code, _ = espota2.run_ota(args.host, PORT_OTA, None, binaire, noise_psk=cle)
    if code != 0:
        print("Échec de l'envoi : la tablette a-t-elle bien la clé de ce secrets.yaml ?", file=sys.stderr)
        return code
    print("Envoyé. La tablette redémarre en 3.0 : donnez-lui le Wi-Fi par « Tab5 Fallback AP », "
          "puis confirmez dans Home Assistant (docs/installation.md, « Passer à la 3.0 »).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
