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
    python tools/migrer_vers_3.py --host 192.168.1.42 --port COM6
    python tools/migrer_vers_3.py --host 192.168.1.42 --secrets ancien/secrets.yaml --bin chemin/firmware.ota.bin

Après l'envoi, la tablette n'a plus d'identifiants Wi-Fi (ils étaient compilés). Avec
`--port` (tablette branchée en USB), le script les lui redonne tout de suite par Improv
(tools/improv_serie.py), avec `wifi_ssid` et `wifi_password` du même secrets.yaml,
jamais affichés : ni téléphone ni point d'accès. Sans `--port`, elle ouvre « Tab5 Fallback
AP » pour qu'on lui donne le réseau. Ensuite, Home Assistant demande de confirmer la fin du
chiffrement et lui donne une nouvelle clé. Voir docs/installation.md.
"""
from __future__ import annotations

import argparse
import logging
import os
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
PORT_OTA = 3232


def lire_secret(secrets: Path, cle: str) -> str | None:
    """Valeur d'une clé d'un secrets.yaml 2.x, ou None. Lu en YAML : guillemets, `#` dans
    un mot de passe entre guillemets et commentaires sont gérés comme par ESPHome."""
    import yaml

    donnees = yaml.safe_load(secrets.read_text(encoding="utf-8"))
    valeur = donnees.get(cle) if isinstance(donnees, dict) else None
    return None if valeur is None or valeur == "" else str(valeur)


def lire_ancienne_cle(secrets: Path) -> str | None:
    """`api_encryption_key` d'un secrets.yaml 2.x, ou None."""
    return lire_secret(secrets, "api_encryption_key")


def wifi_par_usb(port: str, ssid: str, mot_de_passe: str, attente: float = 90) -> int:
    """Redonne le Wi-Fi à la tablette qui redémarre en 3.0, par Improv sur l'USB."""
    import improv_serie

    print(f"Wi-Fi par l'USB ({port}) : attente du démarrage de la 3.0…")
    echeance = time.monotonic() + attente
    while True:
        try:
            serie = improv_serie.ouvrir_port(port)
            try:
                etat, infos = improv_serie.etat(serie, delai=10)
                if etat == improv_serie.ETAT_REGLEE:
                    print("La tablette a déjà un Wi-Fi : rien à envoyer.")
                    return 0
                adresses = improv_serie.regler_wifi(serie, ssid, mot_de_passe)
                print("Wi-Fi réglé" + (f" : {' '.join(a for a in adresses if a)}" if any(adresses) else "."))
                return 0
            finally:
                serie.close()
        except improv_serie.Echec as e:
            if "ne répond pas" not in str(e) or time.monotonic() > echeance:
                print(f"Wi-Fi par l'USB impossible : {e}. Passez par « Tab5 Fallback AP ».", file=sys.stderr)
                return 1
        except OSError:  # port absent le temps que l'USB de la tablette revienne
            if time.monotonic() > echeance:
                print(f"{port} introuvable : passez par « Tab5 Fallback AP ».", file=sys.stderr)
                return 1
        time.sleep(3)


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
    parser.add_argument("--port", help="port USB de la tablette (COM6…) : le Wi-Fi est redonné par Improv, "
                                       "sans passer par son point d'accès")
    args = parser.parse_args()

    if not args.secrets.is_file():
        parser.error(f"{args.secrets} introuvable : il faut l'ancienne clé API de la tablette")
    cle = lire_ancienne_cle(args.secrets)
    if not cle:
        parser.error(f"pas d'api_encryption_key dans {args.secrets}")
    if args.port:
        ssid, mot_de_passe = lire_secret(args.secrets, "wifi_ssid"), lire_secret(args.secrets, "wifi_password")
        if not ssid or mot_de_passe is None:
            parser.error(f"--port : pas de wifi_ssid / wifi_password dans {args.secrets}")
    binaire = args.bin or binaire_par_defaut()
    if not binaire.is_file():
        parser.error(f"{binaire} introuvable : compilez d'abord (python -m esphome compile tab5-ha-hmi.yaml)")

    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    from esphome import espota2

    code, _ = espota2.run_ota(args.host, PORT_OTA, None, binaire, noise_psk=cle)
    if code != 0:
        print("Échec de l'envoi : la tablette a-t-elle bien la clé de ce secrets.yaml ?", file=sys.stderr)
        return code
    if args.port:
        print("Envoyé. La tablette redémarre en 3.0.")
        code = wifi_par_usb(args.port, ssid, mot_de_passe)
        print("Ensuite : confirmez dans Home Assistant (docs/installation.md, « Passer à la 3.0 »).")
        return code
    print("Envoyé. La tablette redémarre en 3.0 : donnez-lui le Wi-Fi par « Tab5 Fallback AP », "
          "puis confirmez dans Home Assistant (docs/installation.md, « Passer à la 3.0 »).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
