"""Branche essai/mesure-invalidation (JAMAIS MERGÉE) : bilan des fenêtres d'actions, par variante de LVGL.

    python tools/mesure/bilan_actions.py resultats

Lit, pour chaque variante V : resultats/natif_V_N.log (lignes « [mesure] fenetre … » de
mesure_actions.h) et resultats/callgrind_V/cg.out.* (un profil par fenêtre). Dans chaque
profil : le total d'instructions, le coût inclusif des appels à lv_obj_tree_walk faits
par l'invalidation (lv_obj_invalidate, lv_obj_invalidate_area, obj_invalidate_area_internal :
le parcours de blur_walk_cb) et celui de blur_walk_cb. Imprime des tableaux Markdown.
"""

from __future__ import annotations

import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

ORDRE = ("avant", "caches", "sans")
RE_FEN = re.compile(r"\[mesure\] fenetre (\S+) cpu_ms ([\d.]+) duree_ms ([\d.]+) images (\d+) dessin_ms ([\d.]+) "
                    r"max_ms ([\d.]+) px (\d+) invalidations (\d+) objets (\d+) caches (\d+) flous (\d+)")
DESC = "desc: Trigger: Client Request: "
INVALIDATION = {"lv_obj_invalidate", "lv_obj_invalidate_area", "obj_invalidate_area_internal"}
RE_NOM = re.compile(r"^(c?fn)=\((\d+)\)(?: (.*))?$")


def med(v: list[float]) -> float | None:
    return statistics.median(v) if v else None


def fmt(v: float | None, n: int = 1) -> str:
    return "—" if v is None else f"{v:,.{n}f}".replace(",", " ")


def natif(dossier: Path, v: str) -> dict:
    """{fenêtre: {champ: [valeurs]}}."""
    res: dict = defaultdict(lambda: defaultdict(list))
    for journal in sorted(dossier.glob(f"natif_{v}_*.log")):
        for m in RE_FEN.finditer(journal.read_text(encoding="utf-8", errors="replace")):
            champs = ("cpu_ms", "duree_ms", "images", "dessin_ms", "max_ms", "px", "invalidations", "objets",
                      "caches", "flous")
            for nom, val in zip(champs, m.groups()[1:]):
                res[m[1]][nom].append(float(val))
    return res


def profil(f: Path) -> tuple[str | None, int, int, int, int]:
    """(nom, total, parcours, blur_walk_cb inclusif, appels du parcours)."""
    noms: dict[str, str] = {}
    nom_fenetre, total = None, 0
    fn = cfn = None
    parcours = blur = appels = 0
    attend_cout = False
    with f.open(encoding="utf-8", errors="replace") as fh:
        for ligne in fh:
            ligne = ligne.rstrip("\n")
            if attend_cout:
                attend_cout = False
                champs = ligne.split()
                cout = int(champs[1]) if len(champs) > 1 else 0
                if cfn == "lv_obj_tree_walk" and fn in INVALIDATION:
                    parcours += cout
                if cfn == "blur_walk_cb":
                    blur += cout
                continue
            if ligne.startswith(DESC):
                nom_fenetre = ligne.removeprefix(DESC).strip()
            elif ligne.startswith(("summary:", "totals:")) and not total:
                total = int(ligne.split()[1])
            elif ligne.startswith(("fn=", "cfn=")):
                m = RE_NOM.match(ligne)
                if m:
                    if m[3] is not None:
                        noms[m[2]] = m[3]
                    nom = noms.get(m[2], m[3])
                else:
                    nom = ligne.split("=", 1)[1]
                if ligne.startswith("fn="):
                    fn = nom
                else:
                    cfn = nom
            elif ligne.startswith("calls="):
                attend_cout = True
                if cfn == "lv_obj_tree_walk" and fn in INVALIDATION:
                    appels += int(ligne.split("=", 1)[1].split()[0])
    return nom_fenetre, total, parcours, blur, appels


def callgrind(dossier: Path, v: str) -> dict:
    res = {}
    for f in sorted((dossier / f"callgrind_{v}").glob("cg.out.*")):
        nom, total, parcours, blur, appels = profil(f)
        if nom and total:
            res[nom] = (total, parcours, blur, appels)
    return res


def main() -> int:
    dossier = Path(sys.argv[1])
    trouvees = {f.stem.removeprefix("natif_").rsplit("_", 1)[0] for f in dossier.glob("natif_*.log")}
    trouvees |= {d.name.removeprefix("callgrind_") for d in dossier.glob("callgrind_*") if d.is_dir()}
    variantes = [v for v in ORDRE if v in trouvees] + sorted(trouvees - set(ORDRE))
    nat = {v: natif(dossier, v) for v in variantes}
    cg = {v: callgrind(dossier, v) for v in variantes}
    fenetres = list(dict.fromkeys(f for v in variantes for f in [*nat[v], *cg[v]]))

    print("\n### Part du parcours de blur_walk_cb (callgrind, variante « avant »)\n")
    print("| fenêtre | objets (cachés) | objets avec flou | appels de lv_obj_tree_walk | M instr. | parcours (M) | part | "
          "instr. par appel |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|")
    for f in fenetres:
        n = nat.get("avant", {}).get(f, {})
        c = cg.get("avant", {}).get(f)
        if not c:
            continue
        total, parcours, _blur, appels = c
        par = parcours / appels if appels else None
        print(f"| {f} | {fmt(med(n.get('objets', [])), 0)} ({fmt(med(n.get('caches', [])), 0)}) | "
              f"{fmt(med(n.get('flous', [])), 0)} | {fmt(appels, 0)} | {fmt(total / 1e6)} | {fmt(parcours / 1e6)} | "
              f"{fmt(100 * parcours / total)} % | {fmt(par, 0)} |")

    print("\n### Instructions par fenêtre (callgrind, M) et écart à « avant »\n")
    print("| fenêtre | " + " | ".join(variantes) + " | " + " | ".join(f"{v} / avant" for v in variantes[1:]) + " |")
    print("|---|" + "---:|" * (2 * len(variantes) - 1))
    for f in fenetres:
        tot = [cg[v].get(f, (None,))[0] for v in variantes]
        if tot[0] is None:
            continue
        ecarts = [fmt(100 * (t - tot[0]) / tot[0]) + " %" if t is not None else "—" for t in tot[1:]]
        print(f"| {f} | " + " | ".join(fmt(t / 1e6 if t else None) for t in tot) + " | " + " | ".join(ecarts) + " |")

    print("\n### Natif (médiane des passes) : temps CPU du processus, dessin et image la plus longue (ms)\n")
    print("| fenêtre | " + " | ".join(f"CPU {v}" for v in variantes) + " | " + " | ".join(f"image max {v}" for v in variantes)
          + " | images | invalidations |")
    print("|---|" + "---:|" * (2 * len(variantes) + 2))
    for f in fenetres:
        cpu = [med(nat[v].get(f, {}).get("cpu_ms", [])) for v in variantes]
        mx = [med(nat[v].get(f, {}).get("max_ms", [])) for v in variantes]
        a = nat.get("avant", {}).get(f, {})
        print(f"| {f} | " + " | ".join(fmt(x, 2) for x in cpu) + " | " + " | ".join(fmt(x, 2) for x in mx)
              + f" | {fmt(med(a.get('images', [])), 0)} | {fmt(med(a.get('invalidations', [])), 0)} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
