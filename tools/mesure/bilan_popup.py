"""Branche essai/mesure-popup-musique (JAMAIS MERGÉE) : bilan avant / après des ouvertures de popups.

    python tools/mesure/bilan_popup.py resultats avant voile100 apres

Lit, pour chaque variante V : resultats/natif_V.log (lignes « [pilote] » du script
ouvrir_popups.py et « [mesure] » du programme, dans l'ordre), resultats/cg_V.log et les
profils resultats/cg_V/cg.out.* (un par image : « desc: Trigger: Client Request: image_N_… »).
Imprime un tableau Markdown : image de l'ouverture (médiane des passes, µs en natif),
instructions (callgrind), objets redessinés (appels de lv_obj_redraw), surface et zones.
"""

from __future__ import annotations

import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

PILOTE = re.compile(r"\[pilote\] (.+?) \| (.+?) \| (\d+)")
MESURE = re.compile(r"\[mesure\] image (\d+) us (\d+) px (\d+) zones (\d+) ecran (.+)")
DESC = "desc: Trigger: Client Request: "


def ouvertures(journal: Path) -> dict[tuple[str, str], list[dict]]:
    """(thème, popup) → une entrée par passe : la première image qui montre ce popup."""
    res: dict[tuple[str, str], list[dict]] = defaultdict(list)
    attendu = None
    for ligne in journal.read_text(encoding="utf-8", errors="replace").splitlines():
        if m := PILOTE.search(ligne):
            attendu = (m.group(1), m.group(2))
        elif (m := MESURE.search(ligne)) and attendu and m.group(5).strip() == attendu[1]:
            res[attendu].append({"n": int(m.group(1)), "us": int(m.group(2)),
                                 "px": int(m.group(3)), "zones": int(m.group(4))})
            attendu = None
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
                cle, _, reste = ligne.partition("=")
                m = re.match(r"\((\d+)\)(?: (.*))?", reste)
                nom = reste
                if m:
                    if m.group(2):
                        noms[m.group(1)] = m.group(2)
                    nom = noms.get(m.group(1), "")
                cfn = nom if cle == "cfn" else None
            elif ligne.startswith("calls=") and cfn is not None and cfn.split("'")[0] == "lv_obj_redraw":
                redraw += int(ligne.split("=", 1)[1].split()[0])
        if nom_image and (m := re.match(r"image_(\d+)_", nom_image)) and total is not None:
            res[int(m.group(1))] = {"instr": total, "redraw": redraw}
    return res


def main() -> int:
    dossier, variantes = Path(sys.argv[1]), sys.argv[2:]
    natif = {v: ouvertures(dossier / f"natif_{v}.log") for v in variantes}
    cg_ouv = {v: ouvertures(dossier / f"cg_{v}.log") for v in variantes}
    cg = {v: profils(dossier / f"cg_{v}") for v in variantes}
    cles = sorted({k for v in variantes for k in natif[v]} | {k for v in variantes for k in cg_ouv[v]})
    print("| Thème | Popup | Variante | Image (ms, natif, médiane) | Instructions (M) | Objets redessinés | px | zones |")
    print("|---|---|---|---:|---:|---:|---:|---:|")
    for cle in cles:
        for v in variantes:
            n = natif[v].get(cle, [])
            ms = f"{statistics.median(x['us'] for x in n) / 1000:.2f} (n={len(n)})" if n else "—"
            c = [cg[v].get(x["n"]) for x in cg_ouv[v].get(cle, [])]
            c = [x for x in c if x]
            instr = f"{statistics.median(x['instr'] for x in c) / 1e6:,.1f}" if c else "—"
            redraw = f"{statistics.median(x['redraw'] for x in c):.0f}" if c else "—"
            px = n[0]["px"] if n else "—"
            zones = n[0]["zones"] if n else "—"
            print(f"| {cle[0]} | {cle[1]} | {v} | {ms} | {instr} | {redraw} | {px} | {zones} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
