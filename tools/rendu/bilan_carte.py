# -*- coding: utf-8 -*-
"""tools/rendu/bilan_carte.py — Les écrans d'une autre carte contre ceux du Tab5 (ADR-0061).

tools/rendu/capturer.py écrit, pour chaque capture, ce que le rendu en dit lui-même
(Tab5/rendu/rendu_mesure.h) : l'écran affiché (« Home », un popup, « Arcade »,
« Game · … » ; « Accueil » et « Jeu · … » avant le 24/10/2026, ADR-0070), les objets COUPÉS (qui dessinent et ne tiennent pas dans leur zone
visible) et, parmi eux, ceux HORS de l'écran, les BARRES de défilement dessinées
(18/10/2026), plus la capture identique qui la précède.
Un fichier par carte : `ecrans-tab5.json` (tâche « fr ») et `ecrans-waveshare-7b.json`
(tâche « rendu-7b »).

Un écran de la carte est OUVERT quand il montre le même écran que sur le Tab5 et n'est
pas une copie d'une capture précédente (sauf si celle du Tab5 en est une aussi : « aller-
volet » montre le même popup que « volet »). Le bilan, en Markdown pour le résumé du
job : écrans ouverts / attendus (popups et accueil d'un côté, consoles de jeu de l'autre),
puis les objets coupés des popups ouverts, carte contre Tab5, et les captures qui montrent
une barre de défilement (le journal du rendu dit où : « Barre de défilement »). `--avant` : un bilan
précédent de la carte, pour l'écart. Informatif : le code de sortie ne dépend que de la
lecture des fichiers.

Usage : python tools/rendu/bilan_carte.py --tab5 ecrans-tab5.json --carte ecrans-waveshare-7b.json
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def lire(chemin: Path) -> dict[str, dict]:
    return json.loads(Path(chemin).read_text(encoding="utf-8"))


def est_scene(nom: str) -> bool:
    """Les scènes du mode démo (« 1-journee-ensoleillee ») : l'accueil, pas un écran ouvert."""
    return nom.split("-", 1)[0].isdigit()


def famille(ecran: str | None) -> str:
    """« jeux » (Arcade et consoles), « accueil » ou « popups »."""
    if ecran is None:
        return "inconnu"
    if ecran == "Arcade" or ecran.startswith(("Game · ", "Jeu · ")):
        return "jeux"
    return "accueil" if ecran in ("Home", "Accueil") else "popups"


def ouvert(nom: str, tab5: dict[str, dict], carte: dict[str, dict]) -> bool:
    ref, vu = tab5[nom], carte.get(nom)
    if vu is None or vu.get("ecran") is None or vu["ecran"] != ref.get("ecran"):
        return False
    return vu.get("doublon_de") is None or ref.get("doublon_de") is not None


def attendus(tab5: dict[str, dict]) -> list[str]:
    """Les écrans capturés sur le Tab5 dont le rendu a dit ce qu'il montrait."""
    return [n for n, m in tab5.items() if not est_scene(n) and m.get("ecran") is not None]


def compter(tab5: dict[str, dict], carte: dict[str, dict]) -> dict[str, tuple[int, int]]:
    """{famille: (ouverts, attendus)} et « total »."""
    res: dict[str, list[int]] = {}
    for nom in attendus(tab5):
        f = famille(tab5[nom]["ecran"])
        for cle in (f, "total"):
            res.setdefault(cle, [0, 0])
            res[cle][1] += 1
            res[cle][0] += ouvert(nom, tab5, carte)
    return {k: (v[0], v[1]) for k, v in res.items()}


def coupes_popups(tab5: dict[str, dict], carte: dict[str, dict]) -> list[tuple[str, str, int, int, int]]:
    """(nom, écran, coupés Tab5, coupés carte, hors écran carte) des popups ouverts sur la carte."""
    lignes = []
    for nom in attendus(tab5):
        ref = tab5[nom]
        if famille(ref["ecran"]) != "popups" or not ouvert(nom, tab5, carte):
            continue
        vu = carte[nom]
        lignes.append((nom, ref["ecran"], ref.get("coupes") or 0, vu.get("coupes") or 0, vu.get("hors") or 0))
    return lignes


def barres(tab5: dict[str, dict], carte: dict[str, dict]) -> list[tuple[str, str, int | None, int]]:
    """(nom, écran, barres Tab5, barres carte) des captures de la carte qui montrent au moins
    une barre de défilement, consoles de jeu exceptées. None : pas mesuré sur le Tab5."""
    lignes = []
    for nom, vu in carte.items():
        if (vu.get("barres") or 0) <= 0 or famille(vu.get("ecran")) == "jeux":
            continue
        ref = tab5.get(nom) or {}
        lignes.append((nom, vu["ecran"], ref.get("barres"), vu["barres"]))
    return lignes


def bilan(tab5: dict[str, dict], carte: dict[str, dict], titre: str, avant: dict[str, dict] | None = None) -> str:
    comptes = compter(tab5, carte)
    lignes = [f"### {titre}", ""]

    def part(cle: str, libelle: str) -> None:
        o, a = comptes.get(cle, (0, 0))
        texte = f"- {libelle} : **{o} / {a}** écrans ouverts"
        if avant is not None:
            texte += f" (avant : {compter(tab5, avant).get(cle, (0, 0))[0]})"
        lignes.append(texte)

    part("total", "Tous")
    part("popups", "Popups")
    part("accueil", "Accueil (variantes)")
    part("jeux", "Arcade et consoles")
    coupes = coupes_popups(tab5, carte)
    if coupes:
        t5 = sum(c[2] for c in coupes)
        ct = sum(c[3] for c in coupes)
        hors = sum(c[4] for c in coupes)
        lignes += ["", f"Popups ouverts ({len(coupes)}) : **{ct}** objets coupés sur la carte, dont **{hors}** "
                   f"hors de l'écran ; {t5} sur le Tab5 pour les mêmes écrans."]
        if avant is not None:
            avant_c = {c[0]: c for c in coupes_popups(tab5, avant)}
            communs = [c for c in coupes if c[0] in avant_c]
            if communs:
                lignes.append(f"Sur les {len(communs)} popups ouverts avant et après : "
                              f"{sum(avant_c[c[0]][3] for c in communs)} → {sum(c[3] for c in communs)} coupés, "
                              f"{sum(avant_c[c[0]][4] for c in communs)} → {sum(c[4] for c in communs)} hors écran.")
    avec_barres = barres(tab5, carte)
    if any("barres" in m for m in carte.values()):
        lignes += ["", f"Barres de défilement (jeux exceptés) : **{len(avec_barres)}** captures de la carte "
                   "en montrent au moins une."]
    rates = [n for n in attendus(tab5) if not ouvert(n, tab5, carte)]
    if rates:
        lignes += ["", "<details><summary>Écrans pas ouverts sur la carte "
                   f"({len(rates)})</summary>", "", "| Capture | Tab5 | Carte |", "|---|---|---|"]
        for n in rates:
            vu = carte.get(n) or {}
            montre = vu.get("ecran") or "—"
            if vu.get("doublon_de"):
                montre += f" (= {vu['doublon_de']})"
            lignes.append(f"| {n} | {tab5[n]['ecran']} | {montre} |")
        lignes += ["", "</details>"]
    if coupes:
        lignes += ["", "<details><summary>Objets coupés par popup ouvert</summary>", "",
                   "| Capture | Écran | Tab5 | Carte | dont hors écran |", "|---|---|---|---|---|"]
        lignes += [f"| {n} | {e} | {a} | {b} | {h} |" for n, e, a, b, h in coupes]
        lignes += ["", "</details>"]
    if avec_barres:
        lignes += ["", "<details><summary>Captures avec une barre de défilement</summary>", "",
                   "| Capture | Écran | Tab5 | Carte |", "|---|---|---|---|"]
        lignes += [f"| {n} | {e} | {'—' if a is None else a} | {b} |" for n, e, a, b in avec_barres]
        lignes += ["", "</details>"]
    return "\n".join(lignes) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tab5", type=Path, required=True, help="ecrans-tab5.json (tâche « fr »)")
    parser.add_argument("--carte", type=Path, required=True, help="ecrans-<carte>.json")
    parser.add_argument("--avant", type=Path, help="un bilan précédent de la carte, pour l'écart")
    parser.add_argument("--titre", default="Rendu 1024×600 (Waveshare 7B) : écrans ouverts")
    args = parser.parse_args()
    sys.stdout.write(bilan(lire(args.tab5), lire(args.carte), args.titre,
                           lire(args.avant) if args.avant else None))
    return 0


if __name__ == "__main__":
    sys.exit(main())
