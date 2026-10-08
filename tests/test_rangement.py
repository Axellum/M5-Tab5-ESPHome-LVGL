# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Garde-fou du rangement de Tab5/ (08/10/2026) : socle/, ecran/, jeux/, paquets/.

- plus aucun fichier du firmware à la racine de Tab5/ : un glob resté sur la racine ne
  trouverait plus rien et son garde-fou passerait à vide (tools/tab5_sources.py) ;
- deux fichiers C++ ne portent jamais le même nom : ESPHome copie À PLAT dans son src/
  chaque fichier de `includes:` (esphome/core/config.py, add_includes), le second
  écraserait le premier ;
- les `includes:` des deux configurations racine pointent vers des fichiers qui existent ;
- socle/ reste pur : ni ESPHome, ni LVGL, ni ESP-IDF, seulement la bibliothèque standard
  et d'autres en-têtes du socle (c'est ce qui le rend compilable sur PC par la CI) ;
- un paquet inclut ui_components/ par `../ui_components/` (chemin relatif au fichier qui
  inclut, esphome/yaml_util.py) : une forme sans `../` ne se charge pas.
"""
from __future__ import annotations

import re
import subprocess

import pytest

from tests.commun import REPO, TAB5, lire

RACINE_PERMISE = {"README.md", "user_entities.example.yaml", "tuiles_icones.yaml"}
SOUS_DOSSIERS = ("socle", "ecran", "jeux", "paquets")
RE_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)


def _suivis(motif: str) -> list[str]:
    sortie = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", motif],
                            cwd=REPO, capture_output=True, check=True).stdout
    return [p for p in sortie.decode("utf-8").split("\0") if p]


def test_racine_de_tab5_sans_fichier_du_firmware():
    racine = [p[len("Tab5/"):] for p in _suivis("Tab5") if p.count("/") == 1]
    assert racine, "git ls-files ne voit plus Tab5/"
    assert sorted(set(racine) - RACINE_PERMISE) == [], "à ranger dans Tab5/socle|ecran|jeux|paquets"


def test_chaque_sous_dossier_a_ses_fichiers():
    for dossier in SOUS_DOSSIERS:
        assert len(_suivis(f"Tab5/{dossier}")) >= 4, dossier


def test_noms_uniques_car_copies_a_plat():
    noms: dict[str, list[str]] = {}
    for dossier in SOUS_DOSSIERS:
        for p in _suivis(f"Tab5/{dossier}"):
            noms.setdefault(p.rsplit("/", 1)[1], []).append(p)
    doublons = {n: ps for n, ps in noms.items() if len(ps) > 1}
    assert doublons == {}


@pytest.mark.parametrize("config", ["tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"])
def test_includes_existent(config):
    bloc = lire(config).split("\n  includes:\n", 1)[1]
    chemins = re.findall(r"^    - (Tab5/\S+)\s*$", bloc.split("\n\n", 1)[0], re.M)
    assert len(chemins) > 40, f"{config} : le motif ne lit plus les includes:"
    absents = [c for c in chemins if not (REPO / c).exists()]
    assert absents == [], absents
    hors_rangement = [c for c in chemins if not c.startswith(("Tab5/socle/", "Tab5/ecran/", "Tab5/jeux/", "Tab5/rendu/"))]
    assert hors_rangement == [], hors_rangement


def test_socle_pur():
    socle = {p.rsplit("/", 1)[1] for p in _suivis("Tab5/socle")}
    assert "tab5_core.cpp" in socle and "alarm_clock.cpp" in socle, socle
    for nom in sorted(socle):
        for inclus in RE_INCLUDE.findall(lire(TAB5 / "socle" / nom)):
            if "/" in inclus or inclus.endswith((".h", ".hpp", ".inc")):
                assert inclus in socle, f"socle/{nom} inclut {inclus} : le socle ne dépend que de lui-même"


def test_paquets_incluent_ui_components_par_le_parent():
    for p in _suivis("Tab5/paquets"):
        texte = lire(p)
        mauvais = re.findall(r"(?:!include\s+|file:\s*)ui_components/", texte)
        assert mauvais == [], f"{p} : écrire ../ui_components/ (chemin relatif au paquet)"
