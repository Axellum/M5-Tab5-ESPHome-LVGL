#!/usr/bin/env python3
"""tools/capture_serie.py — Écoute le port série USB de la tablette, sans la réinitialiser,
et repère un plantage : sortie de panique d'ESP-IDF, raison du démarrage suivant.

[AI-CONTEXT] Sert à comprendre un redémarrage anormal qu'on sait provoquer. Premier cas :
la fin d'une mise à jour depuis HA (3.0.0-rc.1 → rc.2, 27/09/2026) s'est terminée par
« plantage (exception) », sans rapport dans le journal des démarrages. La console
d'ESP-IDF est le port USB (CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG) : la panique y est écrite
juste avant le redémarrage, et nulle part ailleurs.

    python tools/capture_serie.py --mac 30:ED:A0:xx:xx:xx         # écoute 10 min
    python tools/capture_serie.py --port COM6 --elf tab5-ha-hmi-st7123.elf
    python tools/capture_serie.py --relire capture.log --elf …    # décoder une capture

Arrêt : Ctrl+C, ou tout seul `--apres` secondes après un redémarrage vu. La capture est
écrite au fil de l'eau (rien de perdu si la fenêtre se ferme).

Règles (docs/debugging.md, « Capturer un plantage sur le port série ») :
- la tablette est trouvée par sa MAC (numéro de série USB) : ne jamais ouvrir un autre
  appareil Espressif branché au même PC ;
- DTR et RTS à False AVANT l'ouverture : ni l'ouverture ni la fermeture ne la
  redémarrent (vérifié le 27/09/2026) ;
- une seule ouverture ; si le port disparaît (le périphérique USB se réannonce), on
  attend qu'il revienne et on le rouvre, une fois par disparition.

L'ELF (`--elf`) doit être celui du firmware QUI A PLANTÉ, pas du suivant : les adresses
n'ont de sens que pour lui. Ceux des versions publiées sont gardés par le workflow de
publication (artefacts `elf-<révision>`).
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

VID_ESPRESSIF = 0x303A
ANSI = re.compile(r"\x1b\[[0-9;]*m")
ADRESSE = re.compile(r"\b0x4[0-9a-fA-F]{7}\b")

# Débuts d'une sortie de panique d'ESP-IDF (RISC-V pour l'ESP32-P4, et formes communes).
PANIQUE = re.compile(
    r"Guru Meditation Error|abort\(\) was called|assert failed|\*\*\*ERROR\*\*\*|"
    r"Stack protection fault|Stack canary watchpoint|Unhandled debug exception|"
    r"panic'ed|Interrupt wdt timeout|Task watchdog got triggered")
# Fin du bloc : l'ESP-IDF redémarre, ou la ROM se réannonce.
FIN_PANIQUE = re.compile(r"Rebooting\.\.\.|ESP-ROM:|^rst:0x")
REDEMARRAGE = re.compile(r"ESP-ROM:|^rst:0x|Rebooting\.\.\.")
# Rapport du gestionnaire de plantage d'ESPHome, écrit au démarrage suivant.
RAPPORT_ESPHOME = re.compile(r"esp32\.crash|\[crash")


def trouver_port(mac: str | None, port: str | None) -> tuple[str, str | None]:
    """(nom du port, numéro de série). Refuse de deviner entre deux appareils Espressif."""
    from serial.tools import list_ports

    espressif = [p for p in list_ports.comports() if p.vid == VID_ESPRESSIF]
    if port:
        for p in espressif:
            if p.device.lower() == port.lower():
                return p.device, p.serial_number
        raise SystemExit(f"{port} : pas un appareil Espressif branché ({[p.device for p in espressif]})")
    if mac:
        cible = mac.upper().replace("-", ":")
        for p in espressif:
            if (p.serial_number or "").upper() == cible:
                return p.device, p.serial_number
        raise SystemExit(f"aucun port dont le numéro de série est {cible}")
    if len(espressif) == 1:
        return espressif[0].device, espressif[0].serial_number
    liste = ", ".join(f"{p.device} ({p.serial_number})" for p in espressif) or "aucun"
    raise SystemExit(f"préciser --mac ou --port : appareils Espressif branchés : {liste}")


def ouvrir(nom: str):
    import serial

    s = serial.Serial()
    s.port = nom
    s.baudrate = 115200
    s.timeout = 0.5
    s.dtr = False   # AVANT open() : sinon l'ouverture peut redémarrer la puce
    s.rts = False
    s.open()
    return s


def attendre_port(numero: str | None, nom: str, delai: float = 30.0) -> str | None:
    """Le périphérique USB peut se réannoncer au redémarrage : on attend son retour."""
    from serial.tools import list_ports

    fin = time.monotonic() + delai
    while time.monotonic() < fin:
        for p in list_ports.comports():
            if p.vid == VID_ESPRESSIF and ((numero and p.serial_number == numero) or p.device == nom):
                return p.device
        time.sleep(0.5)
    return None


def ecouter(nom: str, numero: str | None, sortie: Path, duree: float, apres: float) -> list[str]:
    """Écoute jusqu'à `duree` s, ou `apres` s après un redémarrage vu. Lignes horodatées."""
    import serial

    lignes: list[str] = []
    debut = time.monotonic()
    vu_redemarrage = None
    tampon = b""
    s = ouvrir(nom)
    print(f"écoute de {nom} ({numero or 'numéro inconnu'}), capture dans {sortie}", flush=True)
    with sortie.open("w", encoding="utf-8") as f:
        try:
            while True:
                maintenant = time.monotonic()
                if maintenant - debut > duree:
                    print("durée atteinte", flush=True)
                    break
                if vu_redemarrage and maintenant - vu_redemarrage > apres:
                    print(f"{apres:.0f} s après le redémarrage : fin de la capture", flush=True)
                    break
                try:
                    tampon += s.read(4096)
                except serial.SerialException as e:
                    ligne = f"{datetime.now():%H:%M:%S.%f}"[:-3] + f" [capture] port perdu ({e}), attente de son retour"
                    print(ligne, flush=True)
                    f.write(ligne + "\n")
                    s.close()
                    revenu = attendre_port(numero, nom)
                    if revenu is None:
                        f.write("[capture] port non revenu en 30 s\n")
                        break
                    nom = revenu
                    s = ouvrir(nom)
                    continue
                *completes, tampon = tampon.split(b"\n")
                for brute in completes:
                    texte = ANSI.sub("", brute.decode("utf-8", "replace")).rstrip("\r")
                    ligne = f"{datetime.now():%H:%M:%S.%f}"[:-3] + " " + texte
                    lignes.append(ligne)
                    f.write(ligne + "\n")
                    f.flush()
                    print(ligne, flush=True)
                    if REDEMARRAGE.search(texte) and vu_redemarrage is None:
                        vu_redemarrage = time.monotonic()
        except KeyboardInterrupt:
            print("arrêté (Ctrl+C)", flush=True)
        finally:
            s.close()
    return lignes


def texte_seul(ligne: str) -> str:
    """Retire l'horodatage ajouté par la capture (« HH:MM:SS.mmm »)."""
    return re.sub(r"^\d\d:\d\d:\d\d\.\d{3} ", "", ligne)


def analyser(lignes: list[str]) -> dict:
    """Blocs de panique, adresses à décoder, redémarrages et rapport d'ESPHome."""
    blocs, bloc = [], None
    redemarrages, rapport = [], []
    for ligne in lignes:
        texte = texte_seul(ligne)
        if bloc is None and PANIQUE.search(texte):
            bloc = [ligne]
            continue
        if bloc is not None:
            bloc.append(ligne)
            if FIN_PANIQUE.search(texte) or len(bloc) > 200:
                blocs.append(bloc)
                bloc = None
        if REDEMARRAGE.search(texte):
            redemarrages.append(ligne)
        if RAPPORT_ESPHOME.search(texte):
            rapport.append(ligne)
    if bloc:
        blocs.append(bloc)
    adresses = []
    for b in blocs:
        for ligne in b:
            for a in ADRESSE.findall(texte_seul(ligne)):
                if a.lower() not in adresses:
                    adresses.append(a.lower())
    return {"paniques": blocs, "adresses": adresses[:60], "redemarrages": redemarrages,
            "rapport_esphome": rapport}


def trouver_addr2line(chemin: str | None) -> str | None:
    if chemin:
        return chemin
    trouve = shutil.which("riscv32-esp-elf-addr2line")
    if trouve:
        return trouve
    for racine in (Path("C:/espidf/tools/riscv32-esp-elf"), Path.home() / ".espressif" / "tools"):
        if racine.exists():
            for p in sorted(racine.rglob("riscv32-esp-elf-addr2line*")):
                return str(p)
    return None


def decoder(adresses: list[str], elf: Path, addr2line: str) -> list[str]:
    """Fonction, fichier et ligne de chaque adresse connue de l'ELF (les autres : ignorées)."""
    if not adresses:
        return []
    res = subprocess.run([addr2line, "-pfiaC", "-e", str(elf), *adresses],
                         capture_output=True, text=True, check=False)
    return [l for l in res.stdout.splitlines() if "??" not in l]


def resumer(analyse: dict, elf: Path | None, addr2line: str | None) -> str:
    out = ["", "=== Résumé de la capture ==="]
    out.append(f"redémarrages vus : {len(analyse['redemarrages'])}")
    out += [f"  {l}" for l in analyse["redemarrages"][:10]]
    if not analyse["paniques"]:
        out.append("aucune sortie de panique d'ESP-IDF")
    for i, bloc in enumerate(analyse["paniques"], 1):
        out.append(f"--- panique {i} ({len(bloc)} lignes) ---")
        out += bloc[:80]
    if analyse["rapport_esphome"]:
        out.append("--- rapport du gestionnaire de plantage d'ESPHome ---")
        out += analyse["rapport_esphome"][:40]
    if analyse["adresses"]:
        if elf and addr2line:
            out.append(f"--- adresses décodées avec {elf.name} ---")
            out += decoder(analyse["adresses"], elf, addr2line) or ["(aucune adresse connue de cet ELF : bon ELF ?)"]
        else:
            out.append(f"{len(analyse['adresses'])} adresses à décoder : relancer avec --elf "
                       "(ELF du firmware qui a planté) : " + " ".join(analyse["adresses"][:12]))
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mac", help="numéro de série USB de la tablette (sa MAC, ex. 30:ED:A0:…)")
    parser.add_argument("--port", help="port série (ex. COM6), si on le connaît")
    parser.add_argument("--duree", type=float, default=600, help="écoute au plus N s (défaut 600)")
    parser.add_argument("--apres", type=float, default=60,
                        help="s'arrête N s après un redémarrage vu (défaut 60 : journal de démarrage)")
    parser.add_argument("--sortie", type=Path, help="fichier de capture (défaut capture-serie-<date>.log)")
    parser.add_argument("--elf", type=Path, help="ELF du firmware qui a planté, pour décoder les adresses")
    parser.add_argument("--addr2line", help="riscv32-esp-elf-addr2line (défaut : cherché dans C:/espidf)")
    parser.add_argument("--relire", type=Path, help="analyser une capture déjà faite, sans rien ouvrir")
    args = parser.parse_args()

    if args.relire:
        lignes = args.relire.read_text(encoding="utf-8").splitlines()
    else:
        nom, numero = trouver_port(args.mac, args.port)
        sortie = args.sortie or Path(f"capture-serie-{datetime.now():%Y%m%d-%H%M%S}.log")
        lignes = ecouter(nom, numero, sortie, args.duree, args.apres)
    addr2line = trouver_addr2line(args.addr2line) if args.elf else None
    if args.elf and not addr2line:
        print("riscv32-esp-elf-addr2line introuvable : --addr2line", file=sys.stderr)
    print(resumer(analyser(lignes), args.elf, addr2line))
    return 0


if __name__ == "__main__":
    sys.exit(main())
