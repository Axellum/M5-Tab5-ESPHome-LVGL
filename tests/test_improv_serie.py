# -*- coding: utf-8 -*-
"""Wi-Fi par Improv sur l'USB (tools/improv_serie.py, migrer_vers_3.py --port).

Paquets conformes au protocole (en-tête, longueur, somme de contrôle, saut de ligne),
lecture au milieu du journal de la tablette, et déroulé d'un réglage face à une fausse
liaison série qui répond comme improv_serial d'ESPHome : succès, échec de connexion,
tablette muette. Le 27/09/2026, `improv_serie.py --port COM6` a lu l'état et l'identité
de la vraie tablette (firmware 3.0.0-rc.1)."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import improv_serie as imp  # noqa: E402
from migrer_vers_3 import lire_secret  # noqa: E402


def test_paquet_conforme_au_protocole():
    p = imp.rpc(imp.RPC_ETAT)
    assert p[:7] == b"IMPROV\x01"
    assert p[7:11] == bytes([imp.TYPE_RPC, 2, imp.RPC_ETAT, 0])
    assert p[11] == sum(p[:11]) & 0xFF and p[12:] == b"\n"


def test_rpc_wifi_porte_ssid_et_mot_de_passe():
    p = imp.rpc_wifi("Maison", "mot#passe")
    donnees = p[9:-2]
    assert donnees[0] == imp.RPC_WIFI and donnees[1] == len(donnees) - 2
    assert donnees[2:] == bytes([6]) + b"Maison" + bytes([9]) + b"mot#passe"
    with pytest.raises(ValueError):
        imp.rpc_wifi("x" * 33, "")


def test_lecteur_trouve_les_paquets_dans_le_journal():
    etat = imp.paquet(imp.TYPE_ETAT, bytes([imp.ETAT_REGLEE]))
    faux = bytearray(imp.paquet(imp.TYPE_ETAT, bytes([imp.ETAT_PRETE])))
    faux[-2] ^= 0xFF  # somme de contrôle fausse : ignoré
    flux = b"[I][wifi:1]: Connected\r\n" + bytes(faux) + b"IMPR" + etat + b"[D] suite\n"
    lecteur = imp.Lecteur()
    recus = []
    for i in range(0, len(flux), 5):  # paquets coupés entre deux lectures
        recus += lecteur.ajouter(flux[i:i + 5])
    assert recus == [(imp.TYPE_ETAT, bytes([imp.ETAT_REGLEE]))]


def test_chaines_d_un_resultat():
    donnees = bytes([imp.RPC_INFOS, 0]) + bytes([3]) + b"abc" + bytes([0]) + bytes([2]) + b"P4"
    assert imp.chaines(donnees) == ["abc", "", "P4"]


class FausseSerie:
    """Répond comme improv_serial : à chaque paquet écrit, les réponses prévues."""

    def __init__(self, reponses):
        self.reponses, self.ecrits, self.a_lire = reponses, [], b""

    def write(self, octets):
        self.ecrits.append(octets)
        commande = octets[9] if len(octets) > 9 else None
        self.a_lire += b"[I][app]: journal\n" + b"".join(self.reponses.get(commande, []))

    def read(self, n):
        morceau, self.a_lire = self.a_lire[:n], self.a_lire[n:]
        return morceau


def _resultat(commande, *textes):
    corps = b"".join(bytes([len(t)]) + t.encode() for t in textes)
    return imp.paquet(imp.TYPE_RESULTAT, bytes([commande, len(corps)]) + corps)


def test_reglage_reussi():
    serie = FausseSerie({imp.RPC_WIFI: [imp.paquet(imp.TYPE_ETAT, bytes([imp.ETAT_CONNEXION])),
                                        imp.paquet(imp.TYPE_ETAT, bytes([imp.ETAT_REGLEE])),
                                        _resultat(imp.RPC_WIFI, "http://192.168.1.42")]})
    journal = []
    assert imp.regler_wifi(serie, "Maison", "secret", delai=2, journal=journal.append) == ["http://192.168.1.42"]
    assert "secret" not in "".join(journal), "le mot de passe n'est jamais affiché"


def test_reseau_introuvable():
    serie = FausseSerie({imp.RPC_WIFI: [imp.paquet(imp.TYPE_ETAT, bytes([imp.ETAT_CONNEXION])),
                                        imp.paquet(imp.TYPE_ERREUR, bytes([3]))]})
    with pytest.raises(imp.Echec, match="connexion impossible"):
        imp.regler_wifi(serie, "Maison", "faux", delai=2, journal=lambda _: None)


def test_etat_et_identite():
    serie = FausseSerie({imp.RPC_INFOS: [_resultat(imp.RPC_INFOS, "axellum.tab5-ha-hmi", "3.0.0", "ESP32-P4")],
                         imp.RPC_ETAT: [imp.paquet(imp.TYPE_ETAT, bytes([imp.ETAT_PRETE]))]})
    assert imp.etat(serie, delai=2) == (imp.ETAT_PRETE, ["axellum.tab5-ha-hmi", "3.0.0", "ESP32-P4"])


def test_tablette_muette():
    with pytest.raises(imp.Echec, match="ne répond pas"):
        imp.etat(FausseSerie({}), delai=0.5)


def test_secrets_lus_en_yaml(tmp_path):
    secrets = tmp_path / "secrets.yaml"
    secrets.write_text('wifi_ssid: Maison  # la box\nwifi_password: "a#b c"\nvide: ""\n', encoding="utf-8")
    assert lire_secret(secrets, "wifi_ssid") == "Maison"
    assert lire_secret(secrets, "wifi_password") == "a#b c"
    assert lire_secret(secrets, "vide") is None and lire_secret(secrets, "absente") is None
