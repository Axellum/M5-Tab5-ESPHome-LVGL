# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Garde-fou du découpage d'AGENTS.md (30/10/2026).

Le tronc AGENTS.md est importé par CLAUDE.md dans CHAQUE session Claude Code, avant tout
travail : il faisait 38 805 octets. Le détail vit dans un AGENTS.md par dossier, importé par
un CLAUDE.md voisin que Claude Code ne charge qu'une fois un fichier du dossier lu ou modifié
(https://code.claude.com/docs/en/memory). Ce test garde :
- le tronc court (un ajout va dans le fichier du dossier concerné) ;
- chaque AGENTS.md de dossier importé par son CLAUDE.md, et cité par le tronc.
"""
from __future__ import annotations

import pytest

from tests.commun import REPO, lire

TRONC_MAX_OCTETS = 9000
DOSSIERS = ("Tab5", "Tab5/socle", "Tab5/ecran", "Tab5/jeux", "HomeAssistant_Config", "tools")


def test_tronc_court():
    taille = (REPO / "AGENTS.md").stat().st_size
    assert taille <= TRONC_MAX_OCTETS, (
        f"AGENTS.md : {taille} octets (> {TRONC_MAX_OCTETS}) : mettre le détail dans l'AGENTS.md du dossier")


def test_claude_racine_importe_le_tronc():
    assert "\n@AGENTS.md\n" in lire("CLAUDE.md")


@pytest.mark.parametrize("dossier", DOSSIERS)
def test_dossier_importe_et_cite(dossier):
    assert (REPO / dossier / "AGENTS.md").is_file(), dossier
    assert "\n@AGENTS.md\n" in lire(f"{dossier}/CLAUDE.md"), f"{dossier}/CLAUDE.md doit importer @AGENTS.md"
    assert f"]({dossier}/AGENTS.md)" in lire("AGENTS.md"), f"le tronc ne cite pas {dossier}/AGENTS.md"
