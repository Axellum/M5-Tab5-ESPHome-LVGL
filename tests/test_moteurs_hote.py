# -*- coding: utf-8 -*-
"""Tests C++ des moteurs de jeu sur PC (constat OUT-2 de l'audit du 07/10/2026).

Le poste de dev n'a pas de g++ : les vrais moteurs (Go, échecs, dames, réveil) ne se
compilent et ne tournent que dans le job `python` de la CI. Ce fichier tient, sans
compilateur, ce qui les rend probants :

- chaque `tools/test_*.cpp` est compilé ET exécuté par ce job (un test C++ ajouté mais
  jamais branché ne prouverait rien) ;
- les valeurs perft du test C++ sont celles du miroir Python (échecs et dames), qui reste
  pour le poste de dev ;
- le bloc pur du moteur des dames s'extrait encore de Tab5/draughts_game.cpp
  (tools/hote/extraire_moteur_dames.py), sans rien de LVGL ni des préférences.
"""
import re

import pytest

import extraire_moteur_dames as dames
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
    assert {"test_go_engine.cpp", "test_chess_engine.cpp", "test_draughts_engine.cpp", "test_alarm_clock.cpp"} <= noms


def test_perft_des_echecs_egaux_au_miroir_python():
    cpp = lire("tools", "test_chess_engine.cpp")
    cas = re.findall(r'\{"([^"]+)", "([^"]+)", \{([\d, ]+)\}\}', cpp)
    assert [(fen, [int(v) for v in refs.split(",")]) for _, fen, refs in cas] == \
        [(fen, refs) for _, fen, refs in miroir_echecs.SUITE]


@pytest.mark.parametrize("nom, attendu", [("PERFT_INTL", miroir_dames.PERFT_INTL), ("PERFT_ENG", miroir_dames.PERFT_ENG)])
def test_perft_des_dames_egaux_au_miroir_python(nom, attendu):
    m = re.search(rf"const uint64_t {nom}\[\] = \{{([\d, ]+)\}};", lire("tools", "test_draughts_engine.cpp"))
    assert m and [int(v) for v in m.group(1).split(",")] == attendu


def test_moteur_des_dames_extractible_et_pur():
    bloc = dames.extraire(lire("Tab5", "draughts_game.cpp"))
    assert bloc.startswith('#line ') and bloc.rstrip().endswith("}  // namespace Draughts")
    for fonction in ("void pos_init(", "int gen_moves(", "void apply_move(", "void refresh_endgame("):
        assert fonction in bloc, f"{fonction} hors du bloc extrait"
    impurs = sorted(set(re.findall(r"\blv_\w+|\bglobal_preferences\b|\bApp\.\w+|\bid\(\w+\)", bloc)))
    assert not impurs, f"le moteur des dames n'est plus pur : {impurs}"


def test_extraction_refuse_une_borne_absente_ou_double():
    with pytest.raises(ValueError):
        dames.extraire("namespace Draughts {\n}\n")
    with pytest.raises(ValueError):
        dames.extraire("namespace Draughts {\nnamespace Draughts {\n}  // namespace Engine\n")
