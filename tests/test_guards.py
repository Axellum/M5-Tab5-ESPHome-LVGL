# -*- coding: utf-8 -*-
"""Les quatre garde-fous de contenu, joués par pytest (donc par la CI) :

- cadre modal v4 (ADR-0009) sur chaque popup de Tab5/ui_components/ ;
- registre unique des consoles et des fenêtres modales (ADR-0013) : aucune
  liste recopiée dans un YAML, aucun jeu ni popup oublié ;
- les 6 salles de « Fil d'Or » sont traversables et tout le loot atteignable ;
- les 10 niveaux de « Coureur d'Or » sont jouables jusqu'à la sortie.

Chaque script reste lançable seul (`python tools/check_*.py`) ; ici on ne fait
que relire son verdict. Ils lisent le C++/YAML réel du dépôt : une salle ou une
map cassée fait échouer la suite avant tout flash."""
import os
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from tools import (  # noqa: E402
    check_lode_levels,
    check_marble_rooms,
    check_tab5_modal_chrome,
    check_tab5_registry,
)


def test_modal_chrome_adr_0009():
    assert check_tab5_modal_chrome.scan() == []


def test_registry_adr_0013():
    assert check_tab5_registry.scan() == []


def test_marble_rooms_all_traversable(capsys):
    assert check_marble_rooms.main() == 0, capsys.readouterr().out


def test_lode_levels_all_playable(capsys):
    assert check_lode_levels.main() == 0, capsys.readouterr().out
