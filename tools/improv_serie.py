# -*- coding: utf-8 -*-
"""tools/improv_serie.py — Régler le Wi-Fi d'une tablette par Improv sur l'USB (lot 6c).

Le firmware 3.0 écoute Improv sur son port USB (`improv_serial`, ADR-0020) : c'est ce
qu'utilise le bouton Wi-Fi de la page de flashage. Ce module parle le même protocole
depuis le PC, pour qu'une migration depuis la 2.x (tools/migrer_vers_3.py --port) règle le
Wi-Fi toute seule, sans passer par le point d'accès de la tablette ni par un téléphone.

Protocole (https://www.improv-wifi.com/serial/, vérifié dans improv_serial d'ESPHome
2026.9.0) : `IMPROV` + version 1 + type + longueur + données + somme de contrôle (octet de
poids faible de la somme de tout ce qui précède) + saut de ligne. Les octets d'un paquet
doivent arriver à moins de 100 ms d'écart : un paquet part en une seule écriture. Le
journal de la tablette passe sur le même port : les paquets sont repérés au milieu.
La tablette n'enregistre le réseau qu'une fois connectée ; après 30 s sans connexion,
elle répond « connexion impossible » et garde l'ancien réglage.

Le port est ouvert avec DTR et RTS à 0 : ni l'ouverture ni la fermeture ne
réinitialisent la puce (vérifié le 27/09/2026 sur COM6).

Usage direct (état et identité de la tablette, sans rien changer) :
    python tools/improv_serie.py --port COM6
"""
from __future__ import annotations

import argparse
import sys
import time
from typing import Callable

EN_TETE = b"IMPROV" + bytes([1])
TYPE_ETAT, TYPE_ERREUR, TYPE_RPC, TYPE_RESULTAT = 1, 2, 3, 4
RPC_WIFI, RPC_ETAT, RPC_INFOS = 1, 2, 3
ETAT_PRETE, ETAT_CONNEXION, ETAT_REGLEE = 2, 3, 4
ETATS = {ETAT_PRETE: "prête (pas de Wi-Fi réglé)", ETAT_CONNEXION: "connexion en cours",
         ETAT_REGLEE: "Wi-Fi réglé"}
ERREURS = {1: "paquet invalide", 2: "commande inconnue",
           3: "connexion impossible : nom du réseau ou mot de passe ?", 0xFF: "erreur inconnue"}


def paquet(type_: int, donnees: bytes) -> bytes:
    """Un paquet Improv complet, prêt à écrire."""
    corps = EN_TETE + bytes([type_, len(donnees)]) + donnees
    return corps + bytes([sum(corps) & 0xFF, 0x0A])


def rpc(commande: int, donnees: bytes = b"") -> bytes:
    return paquet(TYPE_RPC, bytes([commande, len(donnees)]) + donnees)


def rpc_wifi(ssid: str, mot_de_passe: str) -> bytes:
    s, m = ssid.encode("utf-8"), mot_de_passe.encode("utf-8")
    if not 0 < len(s) <= 32 or len(m) > 64:
        raise ValueError("SSID de 1 à 32 octets, mot de passe de 64 octets au plus")
    return rpc(RPC_WIFI, bytes([len(s)]) + s + bytes([len(m)]) + m)


def chaines(resultat: bytes) -> list[str]:
    """Chaînes d'un résultat RPC : commande, longueur, puis (longueur, texte)…"""
    textes, i = [], 2
    while i < len(resultat):
        n = resultat[i]
        textes.append(resultat[i + 1:i + 1 + n].decode("utf-8", "replace"))
        i += 1 + n
    return textes


class Lecteur:
    """Extrait les paquets Improv (type, données) d'un flux mêlé au journal."""

    def __init__(self) -> None:
        self.tampon = b""

    def ajouter(self, octets: bytes) -> list[tuple[int, bytes]]:
        self.tampon += octets
        paquets = []
        while True:
            debut = self.tampon.find(EN_TETE)
            if debut < 0:
                self.tampon = self.tampon[-(len(EN_TETE) - 1):]  # un en-tête coupé en deux
                return paquets
            self.tampon = self.tampon[debut:]
            if len(self.tampon) < len(EN_TETE) + 2:
                return paquets
            n = self.tampon[len(EN_TETE) + 1]
            fin = len(EN_TETE) + 2 + n
            if len(self.tampon) < fin + 1:
                return paquets
            if sum(self.tampon[:fin]) & 0xFF == self.tampon[fin]:
                paquets.append((self.tampon[len(EN_TETE)], self.tampon[len(EN_TETE) + 2:fin]))
                self.tampon = self.tampon[fin + 1:]
            else:
                self.tampon = self.tampon[1:]  # faux en-tête : on cherche le suivant


def ouvrir_port(port: str):
    """Port série sans réinitialiser la puce (DTR et RTS à 0 avant l'ouverture)."""
    import serial

    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.2
    s.dtr = False
    s.rts = False
    s.open()
    return s


class Echec(Exception):
    """Le Wi-Fi n'a pas pu être réglé (message pour l'utilisateur)."""


def _attendre(serie, lecteur: Lecteur, jusqu_a: float, relance: bytes | None = None):
    """Paquets reçus jusqu'à l'échéance ; renvoie la relance toutes les 2 s si donnée."""
    prochaine = 0.0
    while time.monotonic() < jusqu_a:
        if relance is not None and time.monotonic() >= prochaine:
            serie.write(relance)
            prochaine = time.monotonic() + 2
        for p in lecteur.ajouter(serie.read(256)):
            yield p


def etat(serie, delai: float = 10) -> tuple[int, list[str]]:
    """État Improv de la tablette et ses informations (projet, version, puce…)."""
    lecteur, valeur, infos = Lecteur(), None, []
    serie.write(rpc(RPC_INFOS))
    for type_, donnees in _attendre(serie, lecteur, time.monotonic() + delai, relance=rpc(RPC_ETAT)):
        if type_ == TYPE_ETAT and donnees:
            valeur = donnees[0]
        elif type_ == TYPE_RESULTAT and donnees and donnees[0] == RPC_INFOS:
            infos = chaines(donnees)
        if valeur is not None and infos:
            break
    if valeur is None:
        raise Echec("la tablette ne répond pas à Improv sur ce port (firmware 3.0 démarré ?)")
    return valeur, infos


def regler_wifi(serie, ssid: str, mot_de_passe: str, delai: float = 60,
                journal: Callable[[str], None] = print) -> list[str]:
    """Envoie le réseau, attend la connexion ; renvoie les adresses données par la tablette."""
    lecteur = Lecteur()
    serie.write(rpc_wifi(ssid, mot_de_passe))
    journal(f"Wi-Fi envoyé par l'USB (réseau « {ssid} »), connexion…")
    for type_, donnees in _attendre(serie, lecteur, time.monotonic() + delai):
        if type_ == TYPE_ERREUR and donnees and donnees[0]:
            raise Echec(ERREURS.get(donnees[0], f"erreur {donnees[0]}"))
        if type_ == TYPE_RESULTAT and donnees and donnees[0] == RPC_WIFI:
            return chaines(donnees)
        if type_ == TYPE_ETAT and donnees and donnees[0] == ETAT_REGLEE:
            journal("Wi-Fi réglé.")
    raise Echec(f"pas de réponse en {delai:.0f} s")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="port USB de la tablette (COM6, /dev/ttyACM0…)")
    args = parser.parse_args()
    serie = ouvrir_port(args.port)
    try:
        valeur, infos = etat(serie)
    except Echec as e:
        print(e, file=sys.stderr)
        return 1
    finally:
        serie.close()
    print(f"État : {ETATS.get(valeur, valeur)}")
    if infos:
        print("Firmware : " + " · ".join(i for i in infos if i))
    return 0


if __name__ == "__main__":
    sys.exit(main())
