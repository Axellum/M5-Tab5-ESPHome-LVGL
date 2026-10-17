"""Branche essai/mesure-popups-decoupe (JAMAIS MERGÉE) : bilan avant / après des ouvertures de popups.

    python tools/mesure/bilan_popup.py resultats avant apres

Lit, pour chaque variante V : resultats/natif_V.log (lignes « [pilote] thème | geste | passe |
écran attendu » du script ouvrir_popups.py et « [mesure] » du programme, dans l'ordre),
resultats/cg_V.log et les profils resultats/cg_V/cg.out.* (un par image : « desc: Trigger:
Client Request: image_N_… »). Chaque geste reçoit la première grande image de l'écran attendu
qui le suit (avant le geste suivant). Imprime deux tableaux Markdown : l'écart des
instructions (callgrind, médiane des passes) de chaque variante contre la première, puis le
détail (ms en natif, instructions, objets redessinés = appels de lv_obj_redraw, surface, zones).
"""

from __future__ import annotations

import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

PILOTE = re.compile(r"\[pilote\] (.+?) \| (.+?) \| (\d+) \| (.+)")
MESURE = re.compile(r"\[mesure\] image (\d+) us (\d+) px (\d+) zones (\d+) ecran (.+)")
DESC = "desc: Trigger: Client Request: "


def gestes(journal: Path) -> dict[tuple[str, str], list[dict]]:
    """(thème, geste) → une entrée par passe : la première image de l'écran attendu."""
    res: dict[tuple[str, str], list[dict]] = defaultdict(list)
    if not journal.exists():
        return res
    cle, attendu = None, None
    for ligne in journal.read_text(encoding="utf-8", errors="replace").splitlines():
        if m := PILOTE.search(ligne):
            cle, attendu = (m.group(1), m.group(2)), m.group(4).strip()
        elif (m := MESURE.search(ligne)) and cle and m.group(5).strip() == attendu:
            res[cle].append({"n": int(m.group(1)), "us": int(m.group(2)),
                             "px": int(m.group(3)), "zones": int(m.group(4))})
            cle = None
    return res


def profils(dossier: Path) -> dict[int, dict]:
    """Numéro d'image → instructions et appels de lv_obj_redraw."""
    res: dict[int, dict] = {}
    for f in dossier.glob("cg.out.*"):
        noms: dict[str, str] = {}
        nom_image, total, redraw, cfn = None, None, 0, None
        for ligne in f.read_text(encoding="utf-8", errors="replace").splitlines():
            if ligne.startswith(DESC):
                nom_image = ligne.removeprefix(DESC).strip()
            elif ligne.startswith(("summary:", "totals:")) and total is None:
                total = int(ligne.split()[1])
            elif ligne.startswith(("fn=", "cfn=")):
                c, _, reste = ligne.partition("=")
                m = re.match(r"\((\d+)\)(?: (.*))?", reste)
                nom = reste
                if m:
                    if m.group(2):
                        noms[m.group(1)] = m.group(2)
                    nom = noms.get(m.group(1), "")
                cfn = nom if c == "cfn" else None
            elif ligne.startswith("calls=") and cfn is not None and cfn.split("'")[0] == "lv_obj_redraw":
                redraw += int(ligne.split("=", 1)[1].split()[0])
        if nom_image and (m := re.match(r"image_(\d+)_", nom_image)) and total is not None:
            res[int(m.group(1))] = {"instr": total, "redraw": redraw}
    return res


def main() -> int:
    dossier, variantes = Path(sys.argv[1]), sys.argv[2:]
    natif = {v: gestes(dossier / f"natif_{v}.log") for v in variantes}
    cg_g = {v: gestes(dossier / f"cg_{v}.log") for v in variantes}
    cg = {v: profils(dossier / f"cg_{v}") for v in variantes}
    ordre: list[tuple[str, str]] = []
    for v in variantes:
        for k in list(natif[v]) + list(cg_g[v]):
            if k not in ordre:
                ordre.append(k)

    def instr(v: str, cle) -> float | None:
        c = [cg[v].get(x["n"]) for x in cg_g[v].get(cle, [])]
        c = [x["instr"] for x in c if x]
        return statistics.median(c) if c else None

    def ms(v: str, cle) -> float | None:
        n = natif[v].get(cle, [])
        return statistics.median(x["us"] for x in n) / 1000 if n else None

    ref = variantes[0]
    print(f"### Écart contre « {ref} » (instructions callgrind, médiane ; ms natif, médiane)\n")
    print("| Thème | Geste | " + " | ".join(f"{v} (M)" for v in variantes) + " | "
          + " | ".join(f"{v} / {ref}" for v in variantes[1:]) + " | "
          + " | ".join(f"{v} ms" for v in variantes) + " |")
    print("|---|---|" + "---:|" * (2 * len(variantes) - 1 + len(variantes)))
    for cle in ordre:
        i = {v: instr(v, cle) for v in variantes}
        t = {v: ms(v, cle) for v in variantes}
        cols = [f"{i[v] / 1e6:,.1f}" if i[v] else "—" for v in variantes]
        ecarts = [f"{(i[v] / i[ref] - 1) * 100:+.1f} %" if i[v] and i[ref] else "—" for v in variantes[1:]]
        temps = [f"{t[v]:.2f}" if t[v] else "—" for v in variantes]
        print(f"| {cle[0]} | {cle[1]} | " + " | ".join(cols + ecarts + temps) + " |")

    print("\n### Détail\n")
    print("| Thème | Geste | Variante | Image (ms, natif, médiane) | Instructions (M) | Objets redessinés | px | zones |")
    print("|---|---|---|---:|---:|---:|---:|---:|")
    for cle in ordre:
        for v in variantes:
            n = natif[v].get(cle, [])
            t = f"{ms(v, cle):.2f} (n={len(n)})" if n else "—"
            c = [cg[v].get(x["n"]) for x in cg_g[v].get(cle, [])]
            c = [x for x in c if x]
            ins = f"{statistics.median(x['instr'] for x in c) / 1e6:,.1f} (n={len(c)})" if c else "—"
            redraw = f"{statistics.median(x['redraw'] for x in c):.0f}" if c else "—"
            px = n[0]["px"] if n else "—"
            zones = n[0]["zones"] if n else "—"
            print(f"| {cle[0]} | {cle[1]} | {v} | {t} | {ins} | {redraw} | {px} | {zones} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
