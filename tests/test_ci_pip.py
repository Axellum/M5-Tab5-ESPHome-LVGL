# -*- coding: utf-8 -*-
"""Installations pip de la CI avec réessais (28/09/2026).

PyPI répond parfois « Could not find a version … (from versions: none) » : un ou deux
jobs rataient l'installation d'ESPHome et mettaient une croix rouge sur `main` sans
rapport avec le code. Chaque `pip install` d'un workflow passe donc par
tools/ci/pip_reessai.sh, sauf publication.yml, qui peut reconstruire un ancien tag
sans ce script et porte sa propre boucle."""
import re
import shutil
import subprocess
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parent.parent
WORKFLOWS = REPO / ".github" / "workflows"
SCRIPT = REPO / "tools" / "ci" / "pip_reessai.sh"


def test_chaque_pip_install_des_workflows_reessaie():
    for chemin in sorted(WORKFLOWS.glob("*.yml")):
        texte = chemin.read_text(encoding="utf-8")
        for ligne in texte.splitlines():
            # Les appels, pas les messages (« echo "::warning::pip install a échoué" »).
            appel = ligne.split("echo", 1)[0]
            if ligne.lstrip().startswith("#") or not re.search(r"(^|[\s;&|(])pip install\b", appel):
                continue
            if chemin.name == "publication.yml":
                assert "--retries" in ligne, f"{chemin.name} : {ligne.strip()}"
                assert "for essai in" in texte
            else:
                pytest.fail(f"{chemin.name} : pip install sans réessai → {ligne.strip()}")
        if chemin.name != "publication.yml":
            for appel in re.findall(r"bash tools/ci/pip_reessai\.sh[^\n]*", texte):
                assert appel.split("pip_reessai.sh", 1)[1].strip(), f"{chemin.name} : appel sans paquet"


def test_le_script_est_en_lf_et_se_lit():
    brut = SCRIPT.read_bytes()
    assert b"\r\n" not in brut, "fins de ligne CRLF : bash refuserait le script sur les runners"
    if shutil.which("bash") is None:
        pytest.skip("pas de bash")
    assert subprocess.run(["bash", "-n", str(SCRIPT)], capture_output=True).returncode == 0
