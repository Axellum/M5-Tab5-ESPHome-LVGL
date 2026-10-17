"""Branche essai/mesure-bascule-theme (JAMAIS MERGÉE) : tablette virtuelle chronométrée pendant la bascule de thème.

    python tools/mesure/variante_bascule.py tab5-mesure.yaml

Écrit tab5-mesure.yaml (tab5-rendu-host.yaml + l'en-tête Tab5/rendu/mesure_bascule.h + le
script PlatformIO des sanitizers pour les drapeaux TAB5_SAN_CCFLAGS / TAB5_SAN_LINKFLAGS),
puis modifie SUR PLACE, dans la copie de travail de la CI :
- Tab5/paquets/tab5-themes.yaml : une marque mesure::etape() entre les étapes du script
  tab5_theme_repeindre (mêmes noms que diag_etape() du build de mesure de la 7B, 16/10/2026) ;
- Tab5/ecran/tab5_theme.cpp : une marque mesure::module() après chaque module de
  theme_rejouer_ui().
Chaque ancre doit se trouver exactement une fois : sinon le script s'arrête.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
SOURCE = RACINE / "tab5-rendu-host.yaml"
SCRIPT = RACINE / "tools" / "sanitizers" / "pio_drapeaux.py"
THEMES = RACINE / "Tab5" / "paquets" / "tab5-themes.yaml"
THEME_CPP = RACINE / "Tab5" / "ecran" / "tab5_theme.cpp"


def marque(nom: str) -> str:
    return f"      - lambda: 'mesure::etape(\"{nom}\");'\n"


def index_unique(lignes: list[str], predicat, quoi: str) -> int:
    trouves = [i for i, x in enumerate(lignes) if predicat(x)]
    if len(trouves) != 1:
        raise SystemExit(f"Ancre « {quoi} » trouvée {len(trouves)} fois (1 attendue)")
    return trouves[0]


def avant_lambda_de(lignes: list[str], commentaire: str) -> int:
    """Index de la ligne `- lambda: |-` dont le corps commence par ce commentaire."""
    i = index_unique(lignes, lambda x: x.strip().startswith(commentaire), commentaire)
    j = i - 1
    while not lignes[j].strip().startswith("- lambda:"):
        j -= 1
    return j


def patcher_themes(texte: str) -> str:
    lignes = texte.splitlines(keepends=True)
    # De la fin vers le début : les index déjà trouvés ne bougent pas.
    ajouts: list[tuple[int, str]] = []
    i = index_unique(lignes, lambda x: x.strip() == "- script.execute: tab5_assist_sync_settings",
                     "assist_sync_settings")
    ajouts.append((i + 1, marque("9_assist_sync")))
    i = index_unique(lignes, lambda x: x.strip().startswith("# Réveil (réglages, icône d'état)"), "réveil")
    ajouts.append((i, marque("8_rejouer_ui")))
    i = index_unique(lignes, lambda x: x.strip().startswith("// Couleurs que les lambdas des capteurs posent"),
                     "capteurs")
    ajouts.append((i, "          mesure::module(\"cal_render_month\");\n"))
    i = index_unique(lignes, lambda x: x.strip().startswith("forecast_rejouer_theme("), "forecast")
    ajouts.append((i + 1, "          mesure::module(\"forecast_rejouer_theme\");\n"))
    i = index_unique(lignes, lambda x: x.strip() == "theme_rejouer_ui();", "theme_rejouer_ui")
    ajouts.append((i + 1, "          mesure::module(\"FIN_theme_rejouer_ui\");\n"))
    ajouts.append((avant_lambda_de(lignes, "// Couleurs posées par les modules C++"), marque("7_wait_until")))
    i = index_unique(lignes, lambda x: x.strip() == "- wait_until:", "wait_until")
    ajouts.append((i, marque("6_fond_calendrier")))
    i = index_unique(lignes, lambda x: x.strip() == "# <<< styles", "<<< styles")
    ajouts.append((i, marque("5_polices")))
    ajouts.append((avant_lambda_de(lignes, "// Polices d'affichage du thème"), marque("4_formes")))
    ajouts.append((avant_lambda_de(lignes, "// Formes du thème"), marque("3_theme_update_refresh")))
    i = index_unique(lignes, lambda x: x.strip() == "- lvgl.theme.update:", "theme.update")
    ajouts.append((i, marque("2_couleurs_styles")))
    i = index_unique(lignes, lambda x: x.strip().startswith("- lambda: 'boutons_verre_marquer("), "boutons verre")
    ajouts.append((i + 1, marque("1_boutons_verre")))
    ajouts.append((i, marque("0")))
    for index, texte_ajout in sorted(ajouts, key=lambda a: a[0], reverse=True):
        lignes.insert(index, texte_ajout)
    return "".join(lignes)


def patcher_theme_cpp(texte: str) -> str:
    texte = texte.replace('#include "tab5_internal.h"\n', '#include "tab5_internal.h"\n#include "mesure_bascule.h"\n', 1)
    debut = texte.index("void theme_rejouer_ui() {")
    fin = texte.index("\n}\n", debut)
    corps = re.sub(r"^(    )(\w+)\(\);$", r'\1\2(); mesure::module("\2");', texte[debut:fin], flags=re.M)
    return texte[:debut] + corps + texte[fin:]


def variante(texte: str) -> str:
    lignes = texte.splitlines(keepends=True)

    def apres(ligne: str, ajout: str) -> None:
        i = index_unique(lignes, lambda x: x.rstrip() == ligne, ligne)
        lignes.insert(i + 1, ajout)

    apres("  platformio_options:", f"    extra_scripts:\n      - pre:{SCRIPT.as_posix()}\n")
    apres("  includes:", "    - Tab5/rendu/mesure_bascule.h\n")
    return "".join(lignes)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    (RACINE / sys.argv[1]).write_text(variante(SOURCE.read_text(encoding="utf-8")), encoding="utf-8")
    THEMES.write_text(patcher_themes(THEMES.read_text(encoding="utf-8")), encoding="utf-8")
    THEME_CPP.write_text(patcher_theme_cpp(THEME_CPP.read_text(encoding="utf-8")), encoding="utf-8")
    n = THEME_CPP.read_text(encoding="utf-8").count("mesure::module(")
    print(f"Variante écrite : {sys.argv[1]} ; tab5-themes.yaml et tab5_theme.cpp marqués ({n} modules)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
