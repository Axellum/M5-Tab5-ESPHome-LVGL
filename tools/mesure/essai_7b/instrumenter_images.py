"""Branche essai/mesure-popup-musique (JAMAIS MERGÉE) : build de MESURE pour l'essai du popup Musique sur la 7B.

    python tools/mesure/essai_7b/instrumenter_images.py <worktree>

Reprend les parties 1 et 2 de l'instrumentation de la campagne de nuit du 16/10/2026
(.claude/brouillons/mesures_2026-10-16_nuit/instrumenter.py) : diag_mesure.h dans les includes,
« Tab5 Draw Max » visible, mémoire chaque minute, et une ligne « image N us N px N zones » par
image de 2 ms ou plus. Rien d'autre (ni thèmes ni tactile). Le même script s'applique à
« avant » (origin/main) et à « après » (la branche de la PR) : seule la branche change.
"""
import shutil
import sys
from pathlib import Path

ICI = Path(__file__).resolve().parent
wt = Path(sys.argv[1])


def lire(p):
    b = p.read_bytes().decode("utf-8")
    eol = "\r\n" if "\r\n" in b else "\n"
    return b.replace("\r\n", "\n"), eol


def ecrire(p, txt, eol):
    p.write_bytes(txt.replace("\n", eol).encode("utf-8"))


def remplacer(txt, ancien, nouveau, nom):
    assert txt.count(ancien) == 1, f"{nom}: {txt.count(ancien)} occurrences"
    return txt.replace(ancien, nouveau)


shutil.copyfile(ICI / "diag_mesure.h", wt / "Tab5/socle/diag_mesure.h")
p = wt / "tab5-ha-hmi.yaml"
t, eol = lire(p)
t = remplacer(t, "    - Tab5/socle/tab5_tokens.h\n", "    - Tab5/socle/tab5_tokens.h\n    - Tab5/socle/diag_mesure.h\n", "includes")
ecrire(p, t, eol)

p = wt / "Tab5/paquets/tab5-sensors-diagnostics.yaml"
t, eol = lire(p)
t = remplacer(t, '    name: "Tab5 Draw Max"\n    internal: true\n', '    name: "Tab5 Draw Max"\n', "draw max")
t = remplacer(t, "\ninterval:\n", "\ninterval:\n  - interval: 60s\n    then:\n      - lambda: 'diag_memoire();'\n", "interval")
t = remplacer(t, "        id(draw_t0_us) = micros();\n        demarrage_marquer(EtapeDemarrage::DESSIN);\n",
              "        id(draw_t0_us) = micros();\n        id(draw_px) = diag_inv_px();\n        id(draw_n) = diag_inv_n();\n"
              "        demarrage_marquer(EtapeDemarrage::DESSIN);\n", "draw start")
t = remplacer(t, "        if (d > id(draw_max_us)) id(draw_max_us) = d;\n",
              "        if (d > id(draw_max_us)) id(draw_max_us) = d;\n"
              '        if (d >= 2000) ESP_LOGI("mesure", "image %u us %u px %u zones", (unsigned) d, (unsigned) id(draw_px), (unsigned) id(draw_n));\n',
              "draw end")
assert t.endswith("initial_value: '0'\n")
t += "  - id: draw_px\n    type: uint32_t\n    initial_value: '0'\n  - id: draw_n\n    type: uint32_t\n    initial_value: '0'\n"
ecrire(p, t, eol)
print("instrumentation appliquée (images seulement)")
