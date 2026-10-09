# -*- coding: utf-8 -*-
"""Tests C++ des moteurs de jeu sur PC (constat OUT-2 de l'audit du 07/10/2026).

Le poste de dev n'a pas de g++ : les vrais moteurs (Go, échecs, dames, réveil) ne se
compilent et ne tournent que dans le job `python` de la CI. Ce fichier tient, sans
compilateur, ce qui les rend probants :

- chaque `tools/test_*.cpp` est compilé ET exécuté par ce job (un test C++ ajouté mais
  jamais branché ne prouverait rien) ;
- les valeurs perft du test C++ sont celles du miroir Python (échecs et dames), qui reste
  pour le poste de dev ;
- le moteur des dames (Tab5/jeux/draughts_engine.*) reste pur : ni LVGL, ni ESPHome,
  ni préférences.
"""
import re

import pytest

import test_chess_perft as miroir_echecs
import test_draughts_engine as miroir_dames
from tests.commun import REPO, lire

WORKFLOW = ".github/workflows/esphome-tab5.yml"


def _job_python():
    texte = lire(WORKFLOW)
    return texte.split("\n  python:\n", 1)[1].split("\n  build:\n", 1)[0]


@pytest.mark.parametrize("test_cpp", sorted(p.name for p in (REPO / "tools").glob("test_*.cpp")))
def test_chaque_test_cpp_est_compile_et_lance_par_la_ci(test_cpp):
    job = _job_python()
    nom = test_cpp.removesuffix(".cpp")
    assert f"tools/{test_cpp}" in job, f"{test_cpp} n'est pas compilé par le job python de {WORKFLOW}"
    assert re.search(rf'^\s*"\$RUNNER_TEMP/{nom}"\s*$', job, re.M), f"{nom} est compilé mais jamais lancé"


def test_tests_cpp_presents():
    # Le paramétrage ci-dessus ne prouve rien sur une liste vide.
    noms = {p.name for p in (REPO / "tools").glob("test_*.cpp")}
    assert {"test_go_engine.cpp", "test_chess_engine.cpp", "test_draughts_engine.cpp", "test_alarm_clock.cpp",
            "test_parse.cpp"} <= noms


def test_perft_des_echecs_egaux_au_miroir_python():
    cpp = lire("tools", "test_chess_engine.cpp")
    cas = re.findall(r'\{"([^"]+)", "([^"]+)", \{([\d, ]+)\}\}', cpp)
    assert [(fen, [int(v) for v in refs.split(",")]) for _, fen, refs in cas] == \
        [(fen, refs) for _, fen, refs in miroir_echecs.SUITE]


@pytest.mark.parametrize("nom, attendu", [("PERFT_INTL", miroir_dames.PERFT_INTL), ("PERFT_ENG", miroir_dames.PERFT_ENG)])
def test_perft_des_dames_egaux_au_miroir_python(nom, attendu):
    m = re.search(rf"const uint64_t {nom}\[\] = \{{([\d, ]+)\}};", lire("tools", "test_draughts_engine.cpp"))
    assert m and [int(v) for v in m.group(1).split(",")] == attendu


@pytest.mark.parametrize("fichier", ["draughts_engine.h", "draughts_engine.cpp"])
def test_moteur_des_dames_pur(fichier):
    # La CI le compile sans tools/hote/esphome.h ; ce garde le dit dès pytest, sans g++.
    texte = lire("Tab5", "jeux", fichier)
    inclus = re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', texte, re.M)
    assert set(inclus) <= {"draughts_engine.h", "cstdint", "cstring"}, f"{fichier} inclut {inclus}"
    impurs = sorted(set(re.findall(r"\blv_\w+|\besphome\b|\bglobal_preferences\b|\bApp\.\w+|\bESP_LOG\w*", texte)))
    assert not impurs, f"le moteur des dames n'est plus pur : {impurs}"


def test_moteur_des_dames_complet():
    texte = lire("Tab5", "jeux", "draughts_engine.cpp")
    for fonction in ("void pos_init(", "int gen_moves(", "void apply_move(", "void refresh_endgame(",
                     "bool is_terminal(", "int eval_full("):
        assert fonction in texte, f"{fonction} hors de draughts_engine.cpp"
    assert "namespace Engine" not in lire("Tab5", "jeux", "draughts_game.cpp"), \
        "le moteur des dames est revenu dans draughts_game.cpp"
