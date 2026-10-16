# -*- coding: utf-8 -*-
"""tools/fnb58.py — Lit le testeur USB FNIRSI FNB58 par son port de données (HID), 100 échantillons/s.

[AI-CONTEXT] Sert à mesurer ce que consomme un écran alimenté par USB (Waveshare 7B,
Tab5 sans batterie…) bien plus finement que « Tab5 Consommation » (INA226, une valeur
par minute, batterie seulement : tools/mesure_conso.py). Utilisé seul (résumé, CSV) ou
comme module par tools/campagne_mesures.py.

Reads the FNIRSI FNB58 USB tester through its data port (HID), 100 samples per second.

Branchement : le FNB58 entre l'alimentation et l'écran, ET son port micro-USB « PC »
(sur le dessus) relié au PC. Le câble de passage du courant ne suffit pas : sans le
micro-USB, Windows ne voit pas le FNB58 (VID 2E3C). Paquet Python : `hidapi`
(`pip install hidapi`, module `hid`), importé seulement à l'ouverture.

Protocole repris de baryluk/fnirsi-usb-power-data-logger (fnirsi_logger.py) :
- trois commandes de 64 octets (0xAA 0x81 …, 0xAA 0x82 …, 0xAA 0x82 …), puis 0xAA 0x83
  chaque seconde pour que le flux continue ;
- trames de 64 octets : 0xAA, type 0x04, quatre échantillons de 15 octets (tension et
  courant en entiers petit-boutistes de 1e-5 V et 1e-5 A, D+ et D− en mV, température en
  0,1 °C), dernier octet = CRC-8 (polynôme 0x39, départ 0x42) des octets 1 à 62.

Vérifié le 16/10/2026 sur un FNB58 (PID 0x5558) : 100 éch/s, aucun CRC faux sur
157 000 échantillons, mais ~37 valeurs nouvelles par seconde seulement (les autres
répètent la précédente). Le FNB58 a disparu une fois de l'USB en pleine mesure :
tools/campagne_mesures.py rouvre et note le trou. Après certaines lectures il refuse
toute commande (écriture HID = -1) jusqu'à un rebranchement du micro-USB : ouvrir() le
détecte et le dit (cause inconnue, firmware du FNB58 non relevé).

Usage :
    python tools/fnb58.py --duree 30                 # résumé : moyenne, min, max, p95
    python tools/fnb58.py --duree 30 --csv conso.csv # et chaque échantillon
"""
from __future__ import annotations

import argparse
import csv
import statistics
import sys
import time

VID = 0x2E3C
PIDS = (0x5558, 0x5555)  # 0x5555 relevé par une revue (Gough Lui) : variante possible


def crc8(data: bytes) -> int:
    crc = 0x42
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x39) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def commande(dev, code: int, fin: int) -> int:
    """Envoie une commande ; renvoie le nombre d'octets écrits, -1 si le FNB58 la refuse."""
    return dev.write(bytes([0x00, 0xAA, code]) + bytes(61) + bytes([fin]))


def ouvrir():
    """Ouvre le premier FNB58 trouvé et lance le flux. SystemExit s'il n'y en a pas."""
    import hid  # paquet hidapi, seulement pour ce matériel

    for d in hid.enumerate(VID):
        if d["product_id"] in PIDS:
            dev = hid.device()
            dev.open_path(d["path"])
            ecrits = [commande(dev, 0x81, 0x8E), commande(dev, 0x82, 0x96), commande(dev, 0x82, 0x96)]
            if min(ecrits) < 0:
                # Constaté le 16/10/2026, cause inconnue : après certaines lectures, le FNB58
                # refuse toute écriture HID (et n'envoie plus rien) jusqu'à un rebranchement.
                dev.close()
                raise SystemExit("le FNB58 refuse les commandes : débrancher puis rebrancher son micro-USB « PC ».")
            return dev, d
    raise SystemExit("FNB58 introuvable (VID 2E3C) : brancher son port micro-USB « PC » au PC.")


def rafraichir(dev) -> None:
    commande(dev, 0x83, 0x9E)


def decoder(trame: bytes):
    """(échantillons, crc_ok) ; échantillons = None si ce n'est pas une trame de mesures.
    Un échantillon = (volts, ampères, D+ en V, D− en V, °C)."""
    if len(trame) < 64 or trame[0] != 0xAA or trame[1] != 0x04:
        return None, False
    crc_ok = crc8(trame[1:63]) == trame[63]
    ech = []
    for i in range(4):
        o = 2 + 15 * i
        ech.append((
            int.from_bytes(trame[o:o + 4], "little") / 100000,
            int.from_bytes(trame[o + 4:o + 8], "little") / 100000,
            int.from_bytes(trame[o + 8:o + 10], "little") / 1000,
            int.from_bytes(trame[o + 10:o + 12], "little") / 1000,
            int.from_bytes(trame[o + 13:o + 15], "little") / 10,
        ))
    return ech, crc_ok


def lire(duree: float):
    """Lit `duree` secondes ; renvoie ([(t, V, A, D+, D−, °C)], trames au CRC faux).
    Les quatre échantillons d'une trame sont datés à 10 ms d'écart, le dernier à
    l'arrivée de la trame."""
    dev, _ = ouvrir()
    lignes, crc_faux = [], 0
    ref = time.monotonic() + 1
    fin = time.monotonic() + duree
    try:
        while time.monotonic() < fin:
            if time.monotonic() >= ref:
                rafraichir(dev)
                ref += 1
            trame = bytes(dev.read(64, 1000))
            t = time.time()
            ech, ok = decoder(trame)
            if ech is None:
                continue
            if not ok:
                crc_faux += 1
                continue
            for i, e in enumerate(ech):
                lignes.append((round(t - (3 - i) * 0.01, 3), *e))
    finally:
        dev.close()
    return lignes, crc_faux


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--duree", type=float, default=10, help="secondes de mesure (défaut 10)")
    p.add_argument("--csv", help="écrire chaque échantillon dans ce fichier")
    a = p.parse_args()

    lignes, crc_faux = lire(a.duree)
    if a.csv:
        with open(a.csv, "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["t", "V", "A", "D+", "D-", "temp_C"])
            w.writerows(lignes)
    if not lignes:
        sys.exit(f"aucun échantillon (trames au CRC faux : {crc_faux})")
    v = [x[1] for x in lignes]
    i = [x[2] for x in lignes]
    w = sorted(x[1] * x[2] for x in lignes)
    print(f"{len(lignes)} échantillons en {a.duree:g} s, CRC faux : {crc_faux}")
    print(f"tension   moy {statistics.mean(v):.4f} V   min {min(v):.4f}  max {max(v):.4f}")
    print(f"courant   moy {statistics.mean(i) * 1000:.1f} mA  min {min(i) * 1000:.1f}  max {max(i) * 1000:.1f}")
    print(f"puissance moy {statistics.mean(w):.3f} W   p95 {w[int(len(w) * 0.95)]:.3f}  max {w[-1]:.3f}")


if __name__ == "__main__":
    main()
