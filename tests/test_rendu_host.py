# -*- coding: utf-8 -*-
"""Rendu hors tablette (lot 7) : tab5-rendu-host.yaml reprend l'on_boot de l'interface.

Ses lambdas sont des copies de celles de tab5-ha-hmi.yaml (sans le matériel). Si l'une
change d'un côté seulement, le rendu dessinerait autre chose que la tablette : ce test
exige que chaque lambda du rendu figure telle quelle dans le point d'entrée réel. Il
vérifie aussi que les bouchons ne partent jamais dans le firmware de la tablette."""
import os
import re

import yaml
from tests.commun import lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def _on_boot(texte):
    return texte.split("  on_boot:", 1)[1].split("\nhost:", 1)[0].split("\npackages:", 1)[0]


def _lambdas(bloc):
    """Corps des lambdas (bloc `|-` ou ligne simple), espaces de tête retirés."""
    corps = []
    # `[ \t].*` et non `[ \t]+.*` : une seule façon de lire chaque ligne, pas de ReDoS (CodeQL py/redos).
    for m in re.finditer(r"- lambda: \|-\n((?:[ \t].*\n|\n)+?)(?=[ \t]*- |\Z)", bloc):
        lignes = [l.strip() for l in m.group(1).splitlines() if l.strip()]
        corps.append("\n".join(lignes))
    corps += re.findall(r"- lambda: '([^'\n]+)'", bloc)
    return corps


def _normaliser(texte):
    return "\n".join(l.strip() for l in texte.splitlines() if l.strip())


def test_lambdas_du_rendu_copiees_de_la_tablette():
    rendu = _lambdas(_on_boot(_lire("tab5-rendu-host.yaml")))
    tablette = _normaliser(_on_boot(_lire("tab5-ha-hmi.yaml")))
    assert len(rendu) == 4, "le motif ne trouve plus les lambdas de l'on_boot du rendu"
    for corps in rendu:
        assert corps in tablette, "lambda du rendu absente de tab5-ha-hmi.yaml :\n" + corps[:200]


def test_bouchons_jamais_dans_le_firmware():
    tablette = _lire("tab5-ha-hmi.yaml")
    assert "Tab5/rendu" not in tablette and "external_components" not in tablette
    assert "platform: snapshot" not in tablette


def test_rendu_sans_materiel_de_la_tablette():
    rendu = _lire("tab5-rendu-host.yaml")
    for package in MATERIEL:
        assert package not in rendu, package
    assert "\nhost:" in rendu


# Packages de la tablette que le rendu ne charge pas : du matériel, remplacé par
# Tab5/rendu/bouchons.yaml, ou la publication (OTA et mise à jour, sans écran, lot 6c).
# Tout autre package doit être repris.
MATERIEL = ("Tab5/tab5-hardware.yaml", "Tab5/ecran-", "Tab5/tab5-sensors-diagnostics.yaml", "Tab5/tab5-imu.yaml",
            "Tab5/publication-")


def _includes(texte):
    bloc = texte.split("  includes:", 1)[1].split("\n  on_boot:", 1)[0]
    return re.findall(r"^\s+- (\S+)\s*$", bloc, re.M)


def _packages(texte):
    bloc = texte.split("\npackages:", 1)[1]
    return re.findall(r"!include (\S+)", bloc)


def test_memes_sources_cpp_que_la_tablette():
    rendu = _includes(_lire("tab5-rendu-host.yaml"))
    tablette = _includes(_lire("tab5-ha-hmi.yaml"))
    remplacants = [i for i in rendu if i.startswith("Tab5/rendu/")]
    assert remplacants, "les en-têtes de remplacement ont disparu"
    assert [i for i in rendu if i not in remplacants] == tablette


def test_chaque_package_de_la_tablette_repris_ou_materiel():
    rendu = set(_packages(_lire("tab5-rendu-host.yaml")))
    for package in _packages(_lire("tab5-ha-hmi.yaml")):
        if package.startswith(MATERIEL):
            continue
        assert package in rendu, f"{package} : à reprendre dans tab5-rendu-host.yaml (ou matériel ?)"


def test_workflow_seulement_si_l_ecran_change_et_annule_sur_pr():
    """rendu-host.yml (28/09/2026) : un merge de doc seule ne relance pas 13 min de rendu
    sur main, et un nouveau commit sur une PR annule le run devenu inutile."""
    workflow = yaml.safe_load(_lire(".github", "workflows", "rendu-host.yml"))
    declencheurs = workflow.get("on") or workflow[True]  # « on » lu comme un booléen
    assert declencheurs["push"]["paths"] == declencheurs["pull_request"]["paths"]
    assert "Tab5/**" in declencheurs["pull_request"]["paths"]
    assert workflow["concurrency"]["cancel-in-progress"] == "${{ github.event_name == 'pull_request' }}"


def test_rendu_compile_une_fois_et_partage_le_programme():
    """OUT-6 (audit du 07/10/2026) : les neuf tâches « rendu » recompilaient chacune la
    même tablette virtuelle (~2 min de runner chacune). La tâche « compiler » la compile
    une fois et passe le programme en artefact ; les noms des tâches lues par la protection
    de branche et par les références (rendu, comparer) ne changent pas."""
    jobs = yaml.safe_load(_lire(".github", "workflows", "rendu-host.yml"))["jobs"]
    assert {"compiler", "rendu", "comparer"} <= set(jobs)
    assert jobs["rendu"]["needs"] == "compiler"
    compiler = "\n".join(str(e.get("run", "")) for e in jobs["compiler"]["steps"])
    rendu = "\n".join(str(e.get("run", "")) for e in jobs["rendu"]["steps"])
    assert "esphome compile tab5-rendu-host.yaml" in compiler
    assert "esphome compile" not in rendu
    assert any(e.get("with", {}).get("name") == "rendu-programme" for e in jobs["rendu"]["steps"])
    assert 'chmod +x "$prog"' in rendu  # un artefact perd le bit d'exécution
