# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Configuration commune des tests de tests/ (lue par pytest avant eux).

Pose UNE fois le `sys.path` des outils que les tests importent par leur nom
(`import capture_serie`, `import scenarios`, `import preparer`…) : avant l'audit du
07/10/2026 (OUT-3), 39 `sys.path.insert` le refaisaient fichier par fichier. Aucun nom de
module ne se répète entre ces dossiers (vérifié le 08/10/2026), l'ordre n'importe donc pas.
Les utilitaires communs (chemins, lecture, chargeurs YAML, blocs de service, cache Jinja)
sont dans tests/commun.py.
"""
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

for dossier in ("tools", "tools/demo", "tools/hote", "tools/rendu", "tools/publication", "tools/installation_ha",
                "tools/sanitizers", "tools/site"):
    chemin = str(REPO / dossier)
    if chemin not in sys.path:
        sys.path.insert(0, chemin)
