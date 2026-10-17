"""Branche essai/mesure-popup-musique (JAMAIS MERGÉE) : ouverture des popups chronométrée sur la Waveshare 7B.

    python tools/mesure/essai_7b/essai_musique.py --ip <IP de la 7B lue dans HA> --nom avant
    python tools/mesure/essai_7b/essai_musique.py --ip <IP> --nom apres

Il faut le build de MESURE (instrumenter_images.py) flashé : il écrit « image N us N px N zones »
pour chaque image de 2 ms ou plus. Comme la nuit du 16/10/2026 (nuit.py, phase « ecrans ») :
« Aller à l'écran » → popup, 3,5 s d'écoute, « Accueil », 2,5 s ; rien dans les 30 s qui
suivent une minute multiple de 5 (envois du blueprint) ; l'image la plus longue de chaque
ouverture, médiane des passes. Thèmes : Zen Sumi (10 passes) puis Relief doux (5), sombre.
Le thème et le mode du départ sont remis à la fin. Écrit essai_<nom>_<date>.csv à côté.
"""
from __future__ import annotations

import argparse
import asyncio
import csv
import datetime as dt
import re
import statistics
import sys
import time
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parents[2]
sys.path.insert(0, str(RACINE / "tools"))
from tab5_cle_api import trouver_cle  # noqa: E402

MAC = "E8:F6:0A:E8:BB:1A"  # la 7B de la maison (COM8) ; toujours vérifier
CONFIG_HA = r"\\192.168.0.16\config"
POPUPS = ("Musique", "Climatisation", "Énergie")
PLAN = (("Zen Sumi", 10), ("Relief doux", 5))
RE_IMG = re.compile(r"image (\d+) us (\d+) px (\d+) zones")
RE_COUL = re.compile(r"\x1b\[[0-9;]*m")


async def hors_fenetre() -> None:
    """Rien de -8 s à +30 s autour des minutes multiples de 5 (envois du blueprint)."""
    while True:
        n = dt.datetime.now()
        if n.minute % 5 == 0 and n.second < 30:
            await asyncio.sleep(30.5 - n.second)
        elif n.minute % 5 == 4 and n.second >= 52:
            await asyncio.sleep(60 - n.second + 30.5)
        else:
            return


async def main_async(ip: str, nom: str) -> int:
    from aioesphomeapi import APIClient
    from aioesphomeapi.model import LogLevel

    images: list[tuple[float, int, int, int]] = []
    etats: dict[str, object] = {}

    def sur_log(m) -> None:
        txt = RE_COUL.sub("", m.message.decode("utf-8", "replace"))
        if g := RE_IMG.search(txt):
            images.append((time.time(), int(g[1]), int(g[2]), int(g[3])))

    cli = APIClient(ip, 6053, "", noise_psk=trouver_cle(None, ip, CONFIG_HA), client_info="essai musique")
    await cli.connect(login=True)
    info = await cli.device_info()
    print(f"{info.mac_address} {info.project_version} {info.compilation_time}")
    if info.mac_address.upper() != MAC:
        print("Ce n'est pas la 7B de la maison : arrêt.")
        await cli.disconnect()
        return 1
    ents, _ = await cli.list_entities_services()
    cles = {e.name: e.key for e in ents}
    noms = {e.key: e.name for e in ents}
    cli.subscribe_states(lambda s: etats.__setitem__(noms.get(s.key, ""), getattr(s, "state", None)))
    cli.subscribe_logs(sur_log, log_level=LogLevel.LOG_LEVEL_INFO)
    await asyncio.sleep(3)
    depart = (etats.get("Thème"), etats.get("Clair ou sombre"))
    print(f"Départ : thème {depart[0]}, mode {depart[1]}")
    if not any("Draw Max" in n for n in cles):
        print("« Tab5 Draw Max » absent : ce n'est pas un build de mesure (instrumenter_images.py).")

    def choisir(select: str, option: str) -> None:
        cli.select_command(cles[select], option)

    lignes = []
    try:
        for theme, passes in PLAN:
            choisir("Clair ou sombre", "Sombre")
            await asyncio.sleep(4)
            choisir("Thème", theme)
            await asyncio.sleep(10)
            for popup in POPUPS:
                for p in range(passes):
                    for action, option, ecoute in (("ouvrir", popup, 3.5), ("fermer", "Accueil", 2.5)):
                        await hors_fenetre()
                        t0 = time.time()
                        choisir("Aller à l'écran", option)
                        await asyncio.sleep(ecoute)
                        im = [i for i in images if t0 <= i[0] <= time.time()]
                        mx = max(im, key=lambda i: i[1], default=(0, 0, 0, 0))
                        lignes.append([nom, theme, popup, action, p, len(im), mx[1] / 1000, mx[2], mx[3]])
                print(f"{theme} {popup} : {passes} passes")
    finally:
        if depart[0]:
            choisir("Thème", str(depart[0]))
        if depart[1]:
            await asyncio.sleep(2)
            choisir("Clair ou sombre", str(depart[1]))
        await asyncio.sleep(3)
        await cli.disconnect()

    f = ICI / f"essai_{nom}_{dt.datetime.now():%Y%m%d_%H%M}.csv"
    with f.open("w", newline="", encoding="utf-8") as fh:
        csv.writer(fh).writerows([["variante", "theme", "popup", "action", "passe", "images", "max_ms", "px", "zones"],
                                  *lignes])
    print(f"\n{f.name}")
    print("| Thème | Popup | Ouverture (ms, médiane) | Fermeture (ms) | px | zones |")
    print("|---|---|---:|---:|---:|---:|")
    for theme, _ in PLAN:
        for popup in POPUPS:
            o = [x for x in lignes if x[1] == theme and x[2] == popup and x[3] == "ouvrir"]
            c = [x for x in lignes if x[1] == theme and x[2] == popup and x[3] == "fermer"]
            if o:
                print(f"| {theme} | {popup} | {statistics.median(x[6] for x in o):.1f} | "
                      f"{statistics.median(x[6] for x in c):.1f} | {o[0][7]} | {o[0][8]} |")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ip", required=True, help="adresse de la 7B, lue dans HA (DHCP)")
    ap.add_argument("--nom", required=True, help="avant ou apres (nom de la variante flashée)")
    a = ap.parse_args()
    return asyncio.run(main_async(a.ip, a.nom))


if __name__ == "__main__":
    sys.exit(main())
