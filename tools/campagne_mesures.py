# -*- coding: utf-8 -*-
"""tools/campagne_mesures.py — Consommation d'un écran sur USB, rétroéclairage, chaque écran, redémarrages.

[AI-CONTEXT] Campagne du 16/10/2026 sur la Waveshare 7B (docs/consumption.md, section
« Waveshare 7B »). Pendant que tools/fnb58.py lit le courant à 100 éch/s dans un fil,
le script pilote l'écran par son API ESPHome (clé : tools/tab5_cle_api.py) :
1. référence (accueil, rétroéclairage de départ) ;
2. rétroéclairage 0 (éteint), 100, 75, 50, 25, 10, 5, 1 % ;
3. chaque écran du select « Aller à l'écran » : 3 s d'ouverture puis 15 s établies ;
4. N redémarrages par le bouton « Redémarrage Système », 50 s de courbe chacun.
Il remet l'écran d'accueil et le rétroéclairage lus au départ. Chaque mesure évite
−5 s / +30 s autour des minutes multiples de 5 (envois groupés du blueprint, poussée
complète) : une poussée de HA réveille le Wi-Fi et fausserait un segment.

Measures an ESPHome screen powered by USB with an FNB58: backlight levels, every
screen, reboots. Restores the start state. `--analyser` prints one row per segment.

Une campagne pilote l'écran de la maison (rétroéclairage, écrans, redémarrages) :
seulement avec l'accord de son propriétaire. Le 16/10/2026, le 3e redémarrage a laissé
la 7B en boucle (ESP-Hosted : « link not yet up ») jusqu'à une coupure du courant :
--redemarrages 0 pour ne pas en faire.

Usage :
    python tools/campagne_mesures.py 192.168.0.197 \\\\192.168.0.16\\config --sortie mesures_7b
    python tools/campagne_mesures.py --analyser mesures_7b
"""
from __future__ import annotations

import argparse
import asyncio
import bisect
import csv
import json
import statistics
import sys
import threading
import time
from pathlib import Path

import fnb58

NIVEAUX_PCT = (0, 100, 75, 50, 25, 10, 5, 1)


class Capteur(threading.Thread):
    """Lit le FNB58 en continu ; rouvre s'il disparaît de l'USB et note le trou."""

    def __init__(self):
        super().__init__(daemon=True)
        self.ech, self.crc, self.trous, self.arret = [], 0, [], False

    def run(self):
        dev, _ = fnb58.ouvrir()
        ref = time.monotonic() + 1
        while not self.arret:
            try:
                if time.monotonic() >= ref:
                    fnb58.rafraichir(dev)
                    ref += 1
                trame = bytes(dev.read(64, 500))
            except (OSError, ValueError):
                debut = time.time()
                print(f"{time.strftime('%H:%M:%S')} FNB58 perdu, reconnexion", flush=True)
                try:
                    dev.close()
                except Exception:
                    pass
                while not self.arret:
                    time.sleep(1)
                    try:
                        dev, _ = fnb58.ouvrir()
                        break
                    except SystemExit:
                        continue
                self.trous.append((round(debut, 3), round(time.time(), 3)))
                ref = time.monotonic() + 1
                continue
            t = time.time()
            ech, ok = fnb58.decoder(trame)
            if ech is None:
                continue
            if not ok:
                self.crc += 1
                continue
            for i, (v, a, *_reste) in enumerate(ech):
                self.ech.append((round(t - (3 - i) * 0.01, 3), v, a))
        dev.close()


class Campagne:
    def __init__(self, ip, config_ha):
        from tab5_cle_api import trouver_cle

        self.ip = ip
        self.cle = trouver_cle(None, ip, config_ha)
        self.segments, self.etats = [], []
        self.cli, self.k, self.options, self.lum_depart = None, {}, [], None

    async def connecter(self, essais=60):
        from aioesphomeapi import APIClient

        for _ in range(essais):
            try:
                cli = APIClient(self.ip, 6053, "", noise_psk=self.cle, client_info="campagne mesures")
                await cli.connect(login=True)
                ents, _ = await cli.list_entities_services()
                self.k = {e.object_id: e.key for e in ents}
                noms = {e.key: e.object_id for e in ents}
                self.options = next(e.options for e in ents if e.object_id == "aller___l__cran")

                def sur_etat(s):
                    nom = noms.get(s.key, "")
                    if nom == "display_backlight" and self.lum_depart is None:
                        self.lum_depart = round(s.brightness * 255) if s.state else 0
                    if nom.startswith(("tab5_loop", "tab5_free", "tab5_core", "tab5_conso", "tab5_chrono")):
                        self.etats.append((round(time.time(), 3), nom, getattr(s, "state", None)))
                cli.subscribe_states(sur_etat)
                self.cli = cli
                await asyncio.sleep(2)
                return
            except Exception:
                await asyncio.sleep(2)
        raise SystemExit("écran injoignable")

    async def deconnecter(self):
        try:
            await self.cli.disconnect()
        except Exception:
            pass

    @staticmethod
    async def fenetre(duree):
        """Attend que [maintenant, maintenant + duree] évite −5 s / +30 s autour des minutes
        multiples de 5."""
        while True:
            now = time.time()
            lt = time.localtime(now)
            depuis = (lt.tm_min % 5) * 60 + lt.tm_sec + (now % 1)
            if depuis < 30:
                await asyncio.sleep(30 - depuis)
            elif 300 - depuis < duree + 5:
                await asyncio.sleep(300 - depuis + 30)
            else:
                return

    async def segment(self, nom, duree, **info):
        t0 = time.time()
        await asyncio.sleep(duree)
        self.segments.append({"nom": nom, "t0": round(t0, 3), "t1": round(time.time(), 3), **info})
        print(f"{time.strftime('%H:%M:%S')} {nom}", flush=True)

    def lumiere(self, lum):
        k = self.k["display_backlight"]
        if lum == 0:
            self.cli.light_command(k, state=False, transition_length=0)
        else:
            self.cli.light_command(k, state=True, brightness=lum / 255, transition_length=0)

    def ecran(self, nom):
        self.cli.select_command(self.k["aller___l__cran"], nom)

    def accueil(self):
        # La deuxième option, « Home » (« Accueil » pour un firmware d'avant le 24/10/2026 :
        # options en anglais, ADR-0070) : lue dans la liste de la tablette, pas écrite ici.
        self.ecran(self.options[1] if len(self.options) > 1 else "Home")

    async def lancer(self, redemarrages):
        await self.connecter()
        depart = self.lum_depart or 189
        self.accueil()
        self.lumiere(depart)
        await asyncio.sleep(10)
        await self.fenetre(30)
        await self.segment(f"accueil_lum{depart}", 30, lum=depart)

        for pct in NIVEAUX_PCT:
            await self.fenetre(26)
            self.lumiere(round(pct * 255 / 100))
            await asyncio.sleep(6)
            await self.segment(f"retro_{pct}pct", 20, lum=round(pct * 255 / 100))
        self.lumiere(depart)
        await asyncio.sleep(5)

        for nom in self.options[2:]:  # ni « — » ni l'accueil
            await self.fenetre(24)
            t = time.time()
            self.ecran(nom)
            await asyncio.sleep(3)
            self.segments.append({"nom": f"ouverture:{nom}", "t0": round(t, 3), "t1": round(time.time(), 3)})
            await asyncio.sleep(3)
            await self.segment(f"ecran:{nom}", 15)
            self.accueil()
            await asyncio.sleep(4)

        for i in range(redemarrages):
            await self.fenetre(80)
            t = time.time()
            self.cli.button_command(self.k["red_marrage_syst_me"])
            await asyncio.sleep(0.5)
            await self.deconnecter()
            await asyncio.sleep(50 - (time.time() - t))
            self.segments.append({"nom": f"redemarrage_{i + 1}", "t0": round(t, 3), "t1": round(time.time(), 3)})
            print(f"{time.strftime('%H:%M:%S')} redemarrage_{i + 1}", flush=True)
            await self.connecter()
            await asyncio.sleep(25)

        self.accueil()
        self.lumiere(depart)
        await asyncio.sleep(3)
        await self.deconnecter()


def mesurer(a) -> None:
    sortie = Path(a.sortie)
    sortie.mkdir(parents=True, exist_ok=True)
    cap = Capteur()
    cap.start()
    time.sleep(2)
    camp = Campagne(a.ip, a.config_ha)
    try:
        asyncio.run(camp.lancer(a.redemarrages))
    finally:
        cap.arret = True
        cap.join(3)
        with open(sortie / "echantillons.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["t", "V", "A"])
            w.writerows(cap.ech)
        (sortie / "segments.json").write_text(json.dumps(
            {"segments": camp.segments, "trous_fnb58": cap.trous}, ensure_ascii=False, indent=1), encoding="utf-8")
        (sortie / "etats.json").write_text(json.dumps(camp.etats, ensure_ascii=False, indent=1), encoding="utf-8")
        print(f"FIN : {len(cap.ech)} échantillons, CRC faux {cap.crc}, {len(camp.segments)} segments, "
              f"trous {len(cap.trous)}", flush=True)


def stats_segment(ech, ts, s) -> dict | None:
    e = ech[bisect.bisect_left(ts, s["t0"]):bisect.bisect_right(ts, s["t1"])]
    if not e:
        return None
    w = sorted(v * a for _, v, a in e)
    r = {
        "nom": s["nom"], "duree_s": round(s["t1"] - s["t0"], 1), "n": len(e),
        "W_moy": statistics.mean(w), "W_med": statistics.median(w),
        "W_p95": w[int(len(w) * 0.95)], "W_max": w[-1],
        "mA_moy": statistics.mean(a for _, _, a in e) * 1000,
        "V_moy": statistics.mean(v for _, v, _ in e),
    }
    if s["nom"].startswith("redemarrage"):
        wt = [v * a for _, v, a in e]
        r["energie_mWh"] = sum(wt) * 0.01 / 3.6
        r["W_min"] = min(wt)
        r["t_creux_s"] = e[wt.index(min(wt))][0] - s["t0"]
        r["W_fin"] = statistics.mean(wt[-1000:])
    return r


def analyser(dossier: Path) -> None:
    with open(dossier / "echantillons.csv", encoding="utf-8") as f:
        r = csv.reader(f)
        next(r)
        ech = [(float(t), float(v), float(a)) for t, v, a in r]
    seg = json.loads((dossier / "segments.json").read_text(encoding="utf-8"))
    ts = [e[0] for e in ech]
    res = [x for x in (stats_segment(ech, ts, s) for s in seg["segments"]) if x]
    (dossier / "resume.json").write_text(json.dumps(res, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"{'segment':34} {'s':>5} {'W moy':>7} {'méd':>7} {'p95':>7} {'max':>7} {'mA':>7} {'V':>6}")
    for r in res:
        fin = (f"  {r['energie_mWh']:.2f} mWh, creux {r['W_min']:.2f} W à {r['t_creux_s']:.1f} s, "
               f"fin {r['W_fin']:.2f} W" if "energie_mWh" in r else "")
        print(f"{r['nom'][:34]:34} {r['duree_s']:5.1f} {r['W_moy']:7.3f} {r['W_med']:7.3f} {r['W_p95']:7.3f} "
              f"{r['W_max']:7.3f} {r['mA_moy']:7.1f} {r['V_moy']:6.3f}{fin}")
    if seg.get("trous_fnb58"):
        print(f"trous du FNB58 : {seg['trous_fnb58']}")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("ip", nargs="?", help="adresse de l'écran (lue dans HA, DHCP)")
    p.add_argument("config_ha", nargs="?", help="dossier config/ de HA (partage Samba) pour la clé API")
    p.add_argument("--sortie", default="mesures", help="dossier des résultats (défaut ./mesures)")
    p.add_argument("--redemarrages", type=int, default=3)
    p.add_argument("--analyser", metavar="DOSSIER", help="résumer une campagne déjà faite")
    a = p.parse_args()
    if a.analyser:
        analyser(Path(a.analyser))
    elif a.ip and a.config_ha:
        mesurer(a)
    else:
        p.error("ip et config_ha, ou --analyser DOSSIER")
        sys.exit(2)


if __name__ == "__main__":
    main()
