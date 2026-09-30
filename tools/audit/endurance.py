"""Audit qualité : endurance de la tablette virtuelle (fuite mémoire, descripteurs, lenteur).

    python tools/audit/endurance.py --programme <program> --resultats <dossier> --minutes 180

Lance la tablette virtuelle (build `host` normal, sans sanitizer : la quarantaine
d'ASan gonflerait la mémoire), puis répète des cycles jusqu'à la durée demandée :
toutes les scènes du mode démo et tous les écrans de tools/rendu/ecrans.py ouverts et
refermés (tools/rendu/capturer.py), suivis de 5 minutes de poussées de scènes à
cadence accélérée. En parallèle, toutes les 15 s : mémoire résidente (VmRSS), pic
(VmHWM), descripteurs ouverts, fils d'exécution, temps CPU.

Ce que ça prouve : une mémoire qui monte d'un cycle à l'autre (même travail, même
point du cycle) est une fuite dans le code commun à la tablette (LVGL, nos builders,
nos parseurs). Ce que ça ne prouve pas : la fragmentation de la PSRAM ou du tas
interne de l'ESP32-P4 (allocateur différent) — à lire sur la vraie tablette.

Sorties : <resultats>/endurance.csv, endurance.json, endurance.md, tablette.log.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RACINE / "tools" / "demo"))
sys.path.insert(0, str(RACINE / "tools" / "rendu"))

from capturer import capturer  # noqa: E402
from demo_pusher import _donner_une_cle, _lire_cle_demo, _pousser_scene  # noqa: E402
import demo_pusher  # noqa: E402
from scenarios import SCENES  # noqa: E402

logger = logging.getLogger("endurance")
PERIODE = 15.0


def lire_proc(pid: int) -> dict:
    etat = {}
    for ligne in Path(f"/proc/{pid}/status").read_text().splitlines():
        cle, _, valeur = ligne.partition(":")
        if cle in ("VmRSS", "VmHWM", "Threads"):
            etat[cle] = int(valeur.split()[0])
    champs = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
    tck = os.sysconf("SC_CLK_TCK")
    etat["cpu_s"] = round((int(champs[11]) + int(champs[12])) / tck, 2)
    etat["fds"] = len(os.listdir(f"/proc/{pid}/fd"))
    return etat


async def echantillonner(pid: int, t0: float, mesures: list, phase: dict) -> None:
    while True:
        try:
            m = lire_proc(pid)
        except (FileNotFoundError, ProcessLookupError):
            return
        m.update(t=round(time.monotonic() - t0, 1), cycle=phase["cycle"], phase=phase["nom"])
        mesures.append(m)
        await asyncio.sleep(PERIODE)


async def poussees(hote: str, minutes: float) -> int:
    from aioesphomeapi import APIClient

    cle = _lire_cle_demo() or await _donner_une_cle(hote)
    client = APIClient(hote, 6053, "", noise_psk=cle, client_info="Tab5 audit endurance")
    await client.connect(login=True)
    _, services = await client.list_entities_services()
    par_nom = {s.name: s for s in services}
    n, fin = 0, time.monotonic() + minutes * 60
    try:
        while time.monotonic() < fin:
            await _pousser_scene(client, par_nom, SCENES[n % len(SCENES)], frozenset())
            n += 1
    finally:
        await client.disconnect()
    return n


def pente(points: list[tuple[float, float]]) -> float | None:
    """Moindres carrés : unité de y par heure."""
    if len(points) < 3:
        return None
    n = len(points)
    mx = sum(x for x, _ in points) / n
    my = sum(y for _, y in points) / n
    den = sum((x - mx) ** 2 for x, _ in points)
    return None if den == 0 else sum((x - mx) * (y - my) for x, y in points) / den * 3600


async def endurance(args) -> dict:
    resultats = Path(args.resultats)
    captures = Path(os.environ.get("ESPHOME_SNAPSHOT_DIR", resultats / "captures"))
    captures.mkdir(parents=True, exist_ok=True)
    journal = open(resultats / "tablette.log", "w", encoding="utf-8", errors="replace")
    proc = subprocess.Popen([args.programme], stdout=journal, stderr=subprocess.STDOUT)
    await asyncio.sleep(8)
    # Cadence accélérée : les pauses de production entre blocs (1 s) réduites.
    demo_pusher.DELAI_ENTRE_BLOCS = 0.2
    t0 = time.monotonic()
    mesures: list[dict] = []
    phase = {"cycle": 0, "nom": "demarrage"}
    tache = asyncio.create_task(echantillonner(proc.pid, t0, mesures, phase))
    fin = t0 + args.minutes * 60
    cycles, erreurs = [], []
    while time.monotonic() < fin and proc.poll() is None:
        phase["cycle"] += 1
        debut = time.monotonic()
        phase["nom"] = "ecrans"
        try:
            await capturer(args.hote, captures, "", None, None)
        except Exception as exc:
            erreurs.append(f"cycle {phase['cycle']} écrans : {exc!r}")
            logger.warning("cycle %d : %r", phase["cycle"], exc)
        shutil.rmtree(captures, ignore_errors=True)
        captures.mkdir(parents=True, exist_ok=True)
        phase["nom"] = "poussees"
        try:
            n = await poussees(args.hote, min(5.0, max(0.0, (fin - time.monotonic()) / 60)))
        except Exception as exc:
            n = 0
            erreurs.append(f"cycle {phase['cycle']} poussées : {exc!r}")
        phase["nom"] = "repos"
        await asyncio.sleep(PERIODE * 2)  # deux mesures au repos, même point de chaque cycle
        repos = [m for m in mesures if m["cycle"] == phase["cycle"] and m["phase"] == "repos"]
        cycles.append({"cycle": phase["cycle"], "secondes": round(time.monotonic() - debut),
                       "scenes_poussees": n, "t_repos": repos[-1]["t"] if repos else None,
                       "rss_repos_ko": repos[-1]["VmRSS"] if repos else None,
                       "fds_repos": repos[-1]["fds"] if repos else None})
        logger.info("cycle %s", cycles[-1])
    tache.cancel()
    vivante = proc.poll() is None
    code = None
    if vivante:
        proc.terminate()
        try:
            code = proc.wait(10)
        except subprocess.TimeoutExpired:
            proc.kill()
            code = proc.wait()
    else:
        code = proc.returncode
    utiles = [c for c in cycles if c["rss_repos_ko"] is not None]
    # Le 1er cycle remplit les caches (polices, images, pages créées à la demande) : exclu.
    stables = utiles[1:] if len(utiles) > 2 else utiles
    return {
        "minutes_demandees": args.minutes, "duree_s": round(time.monotonic() - t0),
        "vivante_a_la_fin": vivante, "code_sortie": code, "cycles": cycles, "erreurs": erreurs,
        "rss_repos_premier_ko": stables[0]["rss_repos_ko"] if stables else None,
        "rss_repos_dernier_ko": stables[-1]["rss_repos_ko"] if stables else None,
        "pente_rss_repos_ko_par_h": pente([(c["t_repos"], c["rss_repos_ko"]) for c in stables]),
        "fds_repos": [c["fds_repos"] for c in utiles],
        "rss_max_ko": max((m["VmHWM"] for m in mesures), default=None),
        "cpu_s_total": mesures[-1]["cpu_s"] if mesures else None,
        "mesures": mesures,
    }


def resume(r: dict) -> str:
    lignes = ["## Endurance (tablette virtuelle, build normal)", "",
              f"Durée : {r['duree_s'] // 60} min sur {r['minutes_demandees']} demandées, {len(r['cycles'])} cycles "
              "(tous les écrans ouverts et refermés + 5 min de poussées accélérées).",
              f"Vivante à la fin : {'oui' if r['vivante_a_la_fin'] else 'NON (code ' + str(r['code_sortie']) + ')'}", "",
              "| Cycle | Durée (s) | Scènes poussées | RSS au repos (Ko) | Descripteurs |", "|---|---|---|---|---|"]
    for c in r["cycles"]:
        lignes.append(f"| {c['cycle']} | {c['secondes']} | {c['scenes_poussees']} | {c['rss_repos_ko']} | {c['fds_repos']} |")
    p = r["pente_rss_repos_ko_par_h"]
    texte_pente = f"{p:.0f} Ko/h" if p is not None else "non calculable (moins de 3 cycles)"
    lignes += ["", f"RSS au repos, 2e cycle → dernier : {r['rss_repos_premier_ko']} → {r['rss_repos_dernier_ko']} Ko ; "
               f"pente {texte_pente}", f"Pic de mémoire (VmHWM) : {r['rss_max_ko']} Ko"]
    if r["erreurs"]:
        lignes += ["", "Erreurs :"] + [f"- {e}" for e in r["erreurs"][:20]]
    return "\n".join(lignes) + "\n"


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--programme", required=True)
    p.add_argument("--resultats", required=True)
    p.add_argument("--hote", default="127.0.0.1")
    p.add_argument("--minutes", type=float, default=180)
    args = p.parse_args()
    Path(args.resultats).mkdir(parents=True, exist_ok=True)
    r = asyncio.run(endurance(args))
    res = Path(args.resultats)
    with open(res / "endurance.csv", "w", encoding="utf-8") as f:
        f.write("t,cycle,phase,VmRSS,VmHWM,Threads,fds,cpu_s\n")
        for m in r["mesures"]:
            f.write(f"{m['t']},{m['cycle']},{m['phase']},{m.get('VmRSS')},{m.get('VmHWM')},{m.get('Threads')},{m['fds']},{m['cpu_s']}\n")
    (res / "endurance.json").write_text(json.dumps({k: v for k, v in r.items() if k != "mesures"}, ensure_ascii=False, indent=1), encoding="utf-8")
    texte = resume(r)
    (res / "endurance.md").write_text(texte, encoding="utf-8")
    print(texte)
    return 0


if __name__ == "__main__":
    sys.exit(main())
