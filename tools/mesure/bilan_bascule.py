"""Branche essai/mesure-bascule-theme (JAMAIS MERGÉE) : bilan des bascules de thème, avant / après.

    python tools/mesure/bilan_bascule.py resultats

Lit, pour chaque variante V (avant, apres) : resultats/natif_V_N.log (programme) et
resultats/pilote_natif_V_N.log (phases), puis resultats/callgrind_V/cg.out.* (un profil par
étape, nommé « bNN_étape ») et resultats/pilote_callgrind_V.log. Imprime des tableaux Markdown :
médiane par étape et par phase (ms en natif, millions d'instructions sous callgrind), modules
de theme_rejouer_ui() et abonnés de l'affichage.
"""

from __future__ import annotations

import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

VARIANTES = ("avant", "apres")
ETAPES = ["1_boutons_verre", "2_couleurs_styles", "3_theme_update_refresh", "4_formes", "5_polices",
          "6_fond_calendrier", "7_wait_until", "8_rejouer_ui", "9_assist_sync"]
RE_PHASE = re.compile(r"\[pilote\] b(\d+) phase (\w+)")
RE_DEBUT = re.compile(r"\[mesure\] b(\d+) debut objets (\d+) abonnes (\d+)")
RE_ETAPE = re.compile(r"\[mesure\] b(\d+) etape (\S+) ([\d.]+) ms abonnes (\d+)")
RE_MODULE = re.compile(r"\[mesure\] b(\d+) module (\S+) ([\d.]+) ms")
DESC = "desc: Trigger: Client Request: "


def phases(pilote: Path) -> dict[int, str]:
    if not pilote.exists():
        return {}
    return {int(m[1]): m[2] for m in RE_PHASE.finditer(pilote.read_text(encoding="utf-8", errors="replace"))}


def med(valeurs: list[float]) -> float | None:
    return statistics.median(valeurs) if valeurs else None


def fmt(v: float | None, n: int = 1) -> str:
    return "—" if v is None else f"{v:,.{n}f}".replace(",", " ")


def natif(dossier: Path, v: str):
    """{phase: {étape: [ms]}}, {phase: {module: [ms]}}, {phase: [objets]}, {phase: {étape: [abonnés]}}."""
    etapes: dict = defaultdict(lambda: defaultdict(list))
    modules: dict = defaultdict(lambda: defaultdict(list))
    objets: dict = defaultdict(list)
    abonnes: dict = defaultdict(lambda: defaultdict(list))
    for journal in sorted(dossier.glob(f"natif_{v}_*.log")):
        n = journal.stem.rsplit("_", 1)[1]
        ph = phases(dossier / f"pilote_natif_{v}_{n}.log")
        texte = journal.read_text(encoding="utf-8", errors="replace")
        for m in RE_DEBUT.finditer(texte):
            if int(m[1]) in ph:
                objets[ph[int(m[1])]].append(int(m[2]))
        for m in RE_ETAPE.finditer(texte):
            if int(m[1]) in ph:
                etapes[ph[int(m[1])]][m[2]].append(float(m[3]))
                abonnes[ph[int(m[1])]][m[2]].append(int(m[4]))
        for m in RE_MODULE.finditer(texte):
            if int(m[1]) in ph:
                modules[ph[int(m[1])]][m[2]].append(float(m[3]))
    return etapes, modules, objets, abonnes


def callgrind(dossier: Path, v: str) -> dict:
    """{phase: {étape: [instructions]}}."""
    res: dict = defaultdict(lambda: defaultdict(list))
    ph = phases(dossier / f"pilote_callgrind_{v}.log")
    for f in sorted((dossier / f"callgrind_{v}").glob("cg.out.*")):
        nom, total = None, None
        with f.open(encoding="utf-8", errors="replace") as fh:
            for ligne in fh:
                if ligne.startswith(DESC):
                    nom = ligne.removeprefix(DESC).strip()
                elif ligne.startswith(("summary:", "totals:")) and total is None:
                    total = int(ligne.split()[1])
        m = re.fullmatch(r"b(\d+)_(.+)", nom or "")
        if m and total is not None and int(m[1]) in ph:
            res[ph[int(m[1])]][m[2]].append(total)
    return res


def main() -> int:
    dossier = Path(sys.argv[1])
    donnees = {v: natif(dossier, v) for v in VARIANTES}
    cg = {v: callgrind(dossier, v) for v in VARIANTES}
    for phase in ("demarrage", "ecrans"):
        print(f"\n### Phase « {phase} »\n")
        print("| | " + " | ".join(f"{v} (ms natif)" for v in VARIANTES) + " | "
              + " | ".join(f"{v} (M instr.)" for v in VARIANTES) + " |")
        print("|---|" + "---:|" * (2 * len(VARIANTES)))
        print("| objets LVGL | " + " | ".join(fmt(med(donnees[v][2][phase]), 0) for v in VARIANTES)
              + " |" + " |" * len(VARIANTES))
        totaux_n = {v: 0.0 for v in VARIANTES}
        totaux_c = {v: 0.0 for v in VARIANTES}
        for e in [*ETAPES, "10_image_suivante"]:
            ns = [med(donnees[v][0][phase].get(e, [])) for v in VARIANTES]
            cs = [med(cg[v][phase].get(e, [])) for v in VARIANTES]
            for v, a, b in zip(VARIANTES, ns, cs):
                if e != "10_image_suivante":
                    totaux_n[v] += a or 0
                    totaux_c[v] += (b or 0) / 1e6
            print(f"| {e} | " + " | ".join(fmt(a, 2) for a in ns) + " | "
                  + " | ".join(fmt(None if b is None else b / 1e6, 0) for b in cs) + " |")
        print("| **script (1 à 9)** | " + " | ".join(f"**{fmt(totaux_n[v], 2)}**" for v in VARIANTES) + " | "
              + " | ".join(f"**{fmt(totaux_c[v], 0)}**" for v in VARIANTES) + " |")
        print("\nAbonnés de l'affichage à la fin de chaque étape (médiane) : "
              + " ; ".join(f"{v} " + ", ".join(f"{e.split('_', 1)[0]}={fmt(med(donnees[v][3][phase].get(e, [])), 0)}"
                                                for e in [*ETAPES, "10_image_suivante"]) for v in VARIANTES))
        print(f"\n#### Modules de l'étape 8 (phase « {phase} », ms natif, médiane)\n")
        print("| module | " + " | ".join(VARIANTES) + " |")
        print("|---|" + "---:|" * len(VARIANTES))
        noms = sorted({m for v in VARIANTES for m in donnees[v][1][phase]},
                      key=lambda m: -(med(donnees["avant"][1][phase].get(m, [])) or 0))
        for m in noms:
            print(f"| {m} | " + " | ".join(fmt(med(donnees[v][1][phase].get(m, [])), 3) for v in VARIANTES) + " |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
