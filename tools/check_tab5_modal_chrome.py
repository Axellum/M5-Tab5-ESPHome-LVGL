#!/usr/bin/env python3
"""Garde-fou du cadre modal v4 du Tab5 (ADR-0009).

Vérifie que chaque popup de Tab5/ui_components/ utilise le chrome partagé au
lieu de le redupliquer :

  1. toute carte `style_modal_card` est précédée d'un include modal_header.yaml
     dans le même fichier (barre de titre : icône + titre + croix) ;
  2. aucun voile inline : `color_modal_scrim` n'apparaît que dans modal_scrim.yaml ;
  3. aucune croix inline dans un popup : le glyphe F0156 n'apparaît que dans
     modal_header.yaml (les pages LVGL plein écran, sans carte modale, sont hors
     périmètre : elles fournissent leur propre sortie) ;
  4. les includes modal_scrim.yaml passent bien `scrim_opa` ;
  5. les tailles de carte modale passent par les tokens ${modal_card_w/h}
     (aucune valeur en dur), et les boutons d'options d'en-tête restent sur la
     ligne de la barre (y: 4, height: 44).

Usage : python tools/check_tab5_modal_chrome.py   (aussi lancé par `pytest`, tests/test_guards.py)
Sortie : 0 si tout est conforme, 1 sinon (liste des écarts sur stdout).

Historique : vivait dans le workspace privé (`scripts/`) jusqu'au 06/09/2026 —
cité par Tab5/README.md et ADR-0009 sans qu'un clone puisse le lancer.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
UI = REPO / "Tab5" / "ui_components"

# Fichiers qui ONT le droit de contenir le chrome (ce sont les templates).
TEMPLATE_HEADER = "modal_header.yaml"
TEMPLATE_SCRIM = "modal_scrim.yaml"

# mdi close — uniquement dans modal_header.yaml. Les YAML du projet écrivent les
# glyphes en séquence d'échappement ("\U000F0156") ; on couvre aussi le caractère
# brut au cas où quelqu'un le colle directement.
GLYPH_CLOSE = ("F0156", chr(0xF0156))
HDR_OPTION_GEOMETRY = ("y: 4", "height: 44")


def strip_comments(text: str) -> str:
    """Retire les lignes de commentaire : un chrome commenté n'en est pas un."""
    return "\n".join(l for l in text.splitlines() if not l.lstrip().startswith("#"))


def scan(ui_dir: Path = UI) -> list[str]:
    problems: list[str] = []

    if not ui_dir.is_dir():
        return [f"dossier introuvable : {ui_dir}"]

    for path in sorted(ui_dir.glob("*.yaml")):
        name = path.name
        raw = path.read_text(encoding="utf-8")
        text = strip_comments(raw)

        # Un fichier n'est soumis au chrome modal que s'il construit une carte
        # modale. Les pages LVGL plein écran (page_arcade, pages de jeux) sont
        # hors périmètre d'ADR-0009 — c'est l'exception déjà documentée règle 7
        # de Tab5/README.md pour les flux plein cadre.
        is_modal = "styles: style_modal_card" in text

        # 1. carte modale => barre de titre partagée
        if "styles: style_modal_card" in text and name != TEMPLATE_HEADER:
            n_cards = text.count("styles: style_modal_card")
            n_headers = text.count(f"file: {TEMPLATE_HEADER}")
            if n_headers < n_cards:
                problems.append(
                    f"{name}: {n_cards} carte(s) style_modal_card pour "
                    f"{n_headers} include(s) {TEMPLATE_HEADER} — barre de titre "
                    f"dupliquée ou absente"
                )

        # 2. voile inline
        if "color_modal_scrim" in text and name != TEMPLATE_SCRIM:
            problems.append(
                f"{name}: voile inline (color_modal_scrim) — utiliser "
                f"!include {TEMPLATE_SCRIM} avec scrim_opa"
            )

        # 3. croix inline — seulement dans un fichier qui construit VRAIMENT une
        #    carte modale. ADR-0009 encadre le chrome des popups ; il ne dit rien
        #    des pages LVGL plein écran, qui n'ont ni carte, ni voile, ni barre de
        #    titre partagée et doivent donc fournir leur propre sortie.
        #    Sans cette restriction le garde-fou était rouge en permanence sur
        #    game_selector.yaml (page_arcade), ce qui le rendait inutile : une
        #    vraie régression serait passée inaperçue dans le bruit.
        if (name != TEMPLATE_HEADER and is_modal
                and any(g in text for g in GLYPH_CLOSE)):
            problems.append(
                f"{name}: croix de fermeture inline (glyphe {GLYPH_CLOSE[0]}) — "
                f"elle est fournie par {TEMPLATE_HEADER}"
            )

        # 4. scrim sans opacité explicite
        for line in text.splitlines():
            if f"file: {TEMPLATE_SCRIM}" in line and "scrim_opa" not in line:
                problems.append(f"{name}: include {TEMPLATE_SCRIM} sans var scrim_opa")

        # 5a. taille de carte en dur
        if "styles: style_modal_card" in text and name not in (TEMPLATE_HEADER,):
            for m in re.finditer(r"^\s*width: (\d{3,4})\s*$", text, re.M):
                if m.group(1) in ("1230", "1250", "1180"):
                    problems.append(
                        f"{name}: largeur de carte en dur ({m.group(1)}) — "
                        f"utiliser ${{modal_card_w}}"
                    )

        # 5b. boutons d'options d'en-tête (frères de la barre) hors ligne
        if "Options d'en-tête" in raw:
            for key in HDR_OPTION_GEOMETRY:
                if key not in text:
                    problems.append(
                        f"{name}: options d'en-tête sans '{key}' — elles doivent "
                        f"rester sur la ligne de la barre de titre"
                    )

    return problems


def main() -> int:
    problems = scan()
    if problems:
        print(f"[KO] {len(problems)} écart(s) au cadre modal v4 (ADR-0009) :")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("[OK] cadre modal v4 : chrome partagé respecté sur tous les popups")
    return 0


if __name__ == "__main__":
    sys.exit(main())
