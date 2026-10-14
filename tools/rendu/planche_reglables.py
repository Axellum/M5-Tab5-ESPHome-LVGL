# -*- coding: utf-8 -*-
"""tools/rendu/planche_reglables.py — Planche de la tuile − / + pièce par pièce (14/10/2026).

Met sur une seule image les captures « reglables-<maison>-piece-n » (la zone − / + de la
carte clim) et « …-liste » (sa liste ouverte) du rendu (tools/rendu/ecrans.py,
ecrans_reglables_clims) : une colonne par pièce de la démo, et pour chaque maison (plusieurs
clims, une seule, aucune) une rangée de zones puis une rangée de listes. Demande d'Axel,
discussion #278 : voir d'un coup quelle clim la ligne « Clim » désigne dans chaque pièce.

Usage, sur les captures téléchargées d'un run de rendu-host.yml :
    gh run download <id> --name rendu-captures --dir captures
    python tools/rendu/planche_reglables.py --captures captures --sortie planche.png
    python tools/rendu/planche_reglables.py --captures captures --suffixe en --sortie planche-en.png

Pillow n'est importé que pour dessiner : plan() reste lisible sans lui
(tests/test_rendu_ecrans.py).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ecrans import REGLABLES_MAISONS  # noqa: E402
from scenarios import PIECES  # noqa: E402  (tools/demo, chemin ajouté par ecrans)

# Ce que dit chaque maison, en tête de ses rangées.
TITRES = {
    "clims-n": "Plusieurs clims : blueprint, tuile cli de la Chambre d'amis, clim propre du Bureau",
    "clims-1": "Une seule clim : celle du Bureau (clim du blueprint absente)",
    "clims-0": "Aucune clim : pas de ligne « Clim »",
}
# Découpes dans une capture paysage 1280 × 720 : la carte clim (températures et zone − / +,
# climate_card.yaml : carte à x 855, y 110) et le panneau de la liste (reglables_liste.yaml :
# 520 px à x 740, depuis y 110, dix lignes de 54 px).
ZONE = (845, 100, 1275, 300)
LISTE = (735, 100, 1275, 680)
COLONNE = 440  # largeur d'une colonne (la zone à l'échelle 1, la liste réduite à sa largeur)
MARGE = 12
TITRE_H = 34


def plan(pieces: dict = PIECES) -> list[tuple[str, list[tuple[int, str, str]]]]:
    """[(maison, [(R, capture de la zone, capture de la liste), …]), …], dans l'ordre des
    maisons de REGLABLES_MAISONS et des pièces occupées."""
    sortie = []
    for maison in REGLABLES_MAISONS:
        cases = []
        for r in sorted(pieces):
            if pieces[r].tuiles:
                nom = f"reglables-{maison}-piece-{r + 1}"
                cases.append((r, nom, f"{nom}-liste"))
        sortie.append((maison, cases))
    return sortie


# Polices avec les accents (« Pièce », « Entrée ») : celle de Linux (CI), de Windows, puis
# celle de Pillow, qui n'a pas « è ».
POLICES = ("DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "arial.ttf", "segoeui.ttf")


def _police(taille: int):
    from PIL import ImageFont
    for nom in POLICES:
        try:
            return ImageFont.truetype(nom, taille)
        except OSError:
            continue
    try:
        return ImageFont.load_default(size=taille)
    except TypeError:  # Pillow < 10.1
        return ImageFont.load_default()


def dessiner(captures: Path, suffixe: str, sortie: Path, pieces: dict = PIECES) -> list[str]:
    """Écrit la planche ; renvoie les captures absentes (case grise)."""
    from PIL import Image, ImageDraw

    lignes = plan(pieces)
    colonnes = len(lignes[0][1])
    zone_h = ZONE[3] - ZONE[1]
    liste_l, liste_h = LISTE[2] - LISTE[0], LISTE[3] - LISTE[1]
    liste_h_reduite = round(liste_h * (COLONNE - MARGE) / liste_l)
    bloc_h = TITRE_H + zone_h + MARGE + liste_h_reduite + 2 * MARGE
    largeur = MARGE + colonnes * COLONNE
    hauteur = TITRE_H + len(lignes) * bloc_h
    planche = Image.new("RGB", (largeur, hauteur), (24, 26, 30))
    d = ImageDraw.Draw(planche)
    petite, grande = _police(18), _police(22)
    absentes = []
    for c, (r, _, _) in enumerate(lignes[0][1]):
        d.text((MARGE + c * COLONNE, 6), f"Pièce {r + 1} · {pieces[r].nom or '—'}", fill=(230, 230, 230), font=grande)

    def coller(nom: str, boite: tuple, x: int, y: int, largeur_cible: int) -> int:
        fichier = captures / (f"{nom}-{suffixe}.png" if suffixe else f"{nom}.png")
        l, h = boite[2] - boite[0], boite[3] - boite[1]
        h_cible = round(h * largeur_cible / l)
        if not fichier.is_file():
            absentes.append(fichier.name)
            d.rectangle((x, y, x + largeur_cible, y + h_cible), fill=(70, 70, 70))
            d.text((x + 8, y + 8), f"absente : {fichier.name}", fill=(255, 200, 200), font=petite)
            return h_cible
        image = Image.open(fichier).convert("RGB").crop(boite)
        if largeur_cible != l:
            image = image.resize((largeur_cible, h_cible), Image.LANCZOS)
        planche.paste(image, (x, y))
        return h_cible

    y = TITRE_H
    for maison, cases in lignes:
        d.text((MARGE, y + 6), TITRES.get(maison, maison), fill=(255, 210, 120), font=grande)
        for c, (_, zone, liste) in enumerate(cases):
            x = MARGE + c * COLONNE
            coller(zone, ZONE, x, y + TITRE_H, ZONE[2] - ZONE[0])
            coller(liste, LISTE, x, y + TITRE_H + zone_h + MARGE, COLONNE - MARGE)
        y += bloc_h
    sortie.parent.mkdir(parents=True, exist_ok=True)
    planche.save(sortie)
    return absentes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    parser.add_argument("--captures", type=Path, required=True, help="dossier des PNG du rendu")
    parser.add_argument("--suffixe", default="", help="langue des captures (« en ») ; vide : le français")
    parser.add_argument("--sortie", type=Path, required=True, help="PNG de la planche")
    args = parser.parse_args()
    absentes = dessiner(args.captures, args.suffixe, args.sortie)
    print(f"Planche : {args.sortie}" + (f" ({len(absentes)} capture(s) absente(s))" if absentes else ""))
    for nom in absentes:
        print(f"  absente : {nom}")
    return 1 if absentes else 0


if __name__ == "__main__":
    sys.exit(main())
