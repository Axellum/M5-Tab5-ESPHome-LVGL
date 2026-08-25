import pytest
import subprocess
import shutil
from pathlib import Path
from scripts.purger_branches_fusionnees import (
    get_default_branch,
    get_remote_branches,
    is_merged_into,
    analyze_branches,
    delete_remote_branches
)

@pytest.fixture
def git_repo(tmp_path):
    """Crée un dépôt git local et un remote local pour les tests."""
    # 1. Créer le remote (un dépôt nu)
    remote_path = tmp_path / "remote_repo"
    remote_path.mkdir()
    subprocess.run(["git", "init", "--bare", str(remote_path)], check=True)

    # 2. Créer le dépôt de travail
    repo_path = tmp_path / "work_repo"
    repo_path.mkdir()
    subprocess.run(["git", "init", "-C", str(repo_path)], check=True)
    
    # Configurer l'utilisateur pour les commits
    subprocess.run(["git", "-C", str(repo_path), "config", "user.email", "test@example.com"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "config", "user.name", "Test User"], check=True)

    # Ajouter le remote
    subprocess.run(["git", "-C", str(repo_path), "remote", "add", "origin", str(remote_path)], check=True)

    # Créer un commit initial sur main
    (repo_path / "file.txt").write_text("initial")
    subprocess.run(["git", "-C", str(repo_path), "add", "."], check=True)
    subprocess.run(["git", "-C", str(repo_path), "commit", "-m", "initial commit"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "branch", "-M", "main"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "push", "-u", "origin", "main"], check=True)

    return repo_path, remote_path

def test_merged_branch_is_deleted(git_repo):
    """Vérifie qu'une branche fusionnée est identifiée pour suppression."""
    repo_path, remote_path = git_repo

    # Créer une branche, faire un commit, et la fusionner dans main
    subprocess.run(["git", "-C", str(repo_path), "checkout", "-b", "feature-merged"], check=True)
    (repo_path / "feature.txt").write_text("feature")
    subprocess.run(["git", "-C", str(repo_path), "add", "."], check=True)
    subprocess.run(["git", "-C", str(repo_path), "commit", "-m", "feature commit"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "push", "origin", "feature-merged"], check=True)
    
    subprocess.run(["git", "-C", str(repo_path), "checkout", "main"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "merge", "feature-merged"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "push", "origin", "main"], check=True)

    # Analyse
    results = analyze_branches(repo_path, "origin")
    assert "feature-merged" in results["branches_to_delete"]
    assert results["default_branch"] == "main"

def test_unmerged_branch_is_kept(git_repo):
    """Vérifie qu'une branche non fusionnée est conservée."""
    repo_path, remote_path = git_repo

    # Créer une branche avec un commit non fusionné
    subprocess.run(["git", "-C", str(repo_path), "checkout", "-b", "feature-unmerged"], check=True)
    (repo_path / "unmerged.txt").write_text("unmerged")
    subprocess.run(["git", "-C", str(repo_path), "add", "."], check=True)
    subprocess.run(["git", "-C", str(repo_path), "commit", "-m", "unmerged commit"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "push", "origin", "feature-unmerged"], check=True)

    # Analyse
    results = analyze_branches(repo_path, "origin")
    assert "feature-unmerged" not in results["branches_to_delete"]
    assert "feature-unmerged" in results["branches_not_merged"]

def test_deletion_execution(git_repo):
    """Vérifie que la suppression effective fonctionne."""
    repo_path, remote_path = git_repo

    # Créer branche fusionnée
    subprocess.run(["git", "-C", str(repo_path), "checkout", "-b", "to-delete"], check=True)
    (repo_path / "del.txt").write_text("del")
    subprocess.run(["git", "-C", str(repo_path), "add", "."], check=True)
    subprocess.run(["git", "-C", str(repo_path), "commit", "-m", "del commit"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "push", "origin", "to-delete"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "checkout", "main"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "merge", "to-delete"], check=True)
    subprocess.run(["git", "-C", str(repo_path), "push", "origin", "main"], check=True)

    # Supprimer
    del_res = delete_remote_branches(repo_path, ["to-delete"], "origin")
    assert "to-delete" in del_res["deleted"]

    # Vérifier que la branche a disparu du remote
    remote_branches = get_remote_branches(repo_path, "origin")
    assert "to-delete" not in remote_branches
