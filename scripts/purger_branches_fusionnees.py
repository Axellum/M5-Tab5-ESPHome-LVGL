import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import Optional, List, Dict, Tuple

# Variable d'environnement pour le token GitHub (optionnel, pour les API rate limits)
GITHUB_TOKEN = os.environ.get("GITHUB_TOKEN")

def run_git(repo_path: Path, *args, capture_output=True) -> subprocess.CompletedProcess:
    """Exécute une commande git dans le dépôt donné."""
    cmd = ["git", "-C", str(repo_path)] + list(args)
    result = subprocess.run(
        cmd,
        capture_output=capture_output,
        text=True
    )
    return result

def get_default_branch(repo_path: Path, remote: str = "origin") -> str:
    """Récupère le nom de la branche par défaut du remote."""
    result = run_git(repo_path, "ls-remote", "--symref", remote, "HEAD")
    for line in result.stdout.splitlines():
        if line.startswith("ref: "):
            ref_part = line.split()[1]
            if ref_part.startswith("refs/heads/"):
                return ref_part.replace("refs/heads/", "")
    raise RuntimeError(f"Impossible de déterminer la branche par défaut du remote {remote}")

def get_remote_branches(repo_path: Path, remote: str = "origin") -> List[str]:
    """Liste toutes les branches distantes du remote."""
    result = run_git(repo_path, "ls-remote", "--heads", remote)
    branches = []
    for line in result.stdout.strip().splitlines():
        if line:
            parts = line.split()
            if len(parts) >= 2:
                ref = parts[1]
                if ref.startswith("refs/heads/"):
                    branches.append(ref.replace("refs/heads/", ""))
    return branches

def get_branch_commit(repo_path: Path, branch: str, remote: str = "origin") -> Optional[str]:
    """Récupère le commit SHA d'une branche distante."""
    result = run_git(repo_path, "ls-remote", remote, f"refs/heads/{branch}")
    for line in result.stdout.strip().splitlines():
        if line and f"refs/heads/{branch}" in line:
            parts = line.split()
            if parts:
                return parts[0]
    return None

def is_merged_into(repo_path: Path, branch: str, target_branch: str, remote: str = "origin") -> bool:
    """Vérifie si tous les commits de la branche sont des ancêtres de la branche cible."""
    branch_commit = get_branch_commit(repo_path, branch, remote)
    if not branch_commit:
        return False
    
    target_commit = get_branch_commit(repo_path, target_branch, remote)
    if not target_commit:
        result = run_git(repo_path, "rev-parse", target_branch)
        if result.returncode == 0:
            target_commit = result.stdout.strip()
        else:
            return False
    
    result = run_git(repo_path, "merge-base", "--is-ancestor", branch_commit, target_commit)
    return result.returncode == 0

def get_remote_url(repo_path: Path, remote: str = "origin") -> Optional[str]:
    """Récupère l'URL du remote."""
    result = run_git(repo_path, "remote", "get-url", remote)
    if result.returncode == 0:
        return result.stdout.strip()
    return None

def get_repo_owner_and_name(repo_path: Path, remote: str = "origin") -> Optional[Tuple[str, str]]:
    """Extrait owner et nom du dépôt depuis l'URL du remote."""
    url = get_remote_url(repo_path, remote)
    if not url:
        return None
    if url.startswith("https://"):
        path = url.replace("https://github.com/", "").replace(".git", "")
        parts = path.split("/")
    elif url.startswith("git@"):
        path = url.replace("git@github.com:", "").replace(".git", "")
        parts = path.split("/")
    else:
        return None
    if len(parts) >= 2:
        return parts[0], parts[1]
    return None

def check_pr_exists(repo_path: Path, branch: str, remote: str = "origin") -> bool:
    """Vérifie si une PR ouverte existe pour cette branche via l'API GitHub."""
    repo_info = get_repo_owner_and_name(repo_path, remote)
    if not repo_info:
        return False
    
    owner, repo = repo_info
    api_url = f"https://api.github.com/repos/{owner}/{repo}/pulls"
    headers = {"Accept": "application/vnd.github.v3+json", "User-Agent": "GitBranchPurger/1.0"}
    if GITHUB_TOKEN:
        headers["Authorization"] = f"token {GITHUB_TOKEN}"
    
    try:
        import urllib.request
        import json
        request = urllib.request.Request(api_url, headers=headers)
        with urllib.request.urlopen(request, timeout=10) as response:
            if response.status == 200:
                pulls = json.loads(response.read().decode())
                for pr in pulls:
                    if pr.get("head", {}).get("ref") == branch:
                        return True
    except Exception:
        pass
    return False

def analyze_branches(repo_path: Path, remote: str = "origin", dry_run: bool = True) -> Dict:
    """Analyse les branches distantes et détermine lesquelles peuvent être supprimées."""
    try:
        default_branch = get_default_branch(repo_path, remote)
    except RuntimeError as e:
        print(f"Erreur: {e}", file=sys.stderr)
        sys.exit(1)

    results = {
        "default_branch": default_branch,
        "branches_to_delete": [],
        "branches_with_pr": [],
        "branches_not_merged": [],
        "branches_skipped": [],
    }
    
    branches = get_remote_branches(repo_path, remote)
    for branch in branches:
        if branch == default_branch:
            results["branches_skipped"].append({"branch": branch, "reason": "branche par défaut"})
            continue
        
        if check_pr_exists(repo_path, branch, remote):
            results["branches_with_pr"].append(branch)
            continue
        
        if is_merged_into(repo_path, branch, default_branch, remote):
            results["branches_to_delete"].append(branch)
        else:
            results["branches_not_merged"].append(branch)
    
    return results

def delete_remote_branches(repo_path: Path, branches: List[str], remote: str = "origin") -> Dict:
    """Supprime les branches distantes spécifiées."""
    results = {"deleted": [], "failed": []}
    for branch in branches:
        result = run_git(repo_path, "push", remote, "--delete", branch)
        if result.returncode == 0:
            results["deleted"].append(branch)
        else:
            results["failed"].append({"branch": branch, "error": result.stderr})
    return results

def main():
    parser = argparse.ArgumentParser(description="Supprime les branches distantes fusionnées")
    parser.add_argument("--repo", type=Path, default=Path.cwd(), help="Chemin du dépôt Git")
    parser.add_argument("--remote", default="origin", help="Nom du remote")
    parser.add_argument("--supprimer", action="store_true", help="Exécute la suppression")
    parser.add_argument("--token", help="Token GitHub")
    
    args = parser.parse_args()
    global GITHUB_TOKEN
    if args.token:
        GITHUB_TOKEN = args.token
    
    repo_path = args.repo.resolve()
    if not (repo_path / ".git").is_dir():
        print(f"Erreur: {repo_path} n'est pas un dépôt Git", file=sys.stderr)
        sys.exit(1)
    
    results = analyze_branches(repo_path, args.remote, dry_run=not args.supprimer)
    
    print(f"Branche par défaut: {results['default_branch']}")
    print(f"Branches à supprimer: {len(results['branches_to_delete'])}")
    for b in results["branches_to_delete"]: print(f"  - {b}")
    
    if not args.supprimer:
        print("\n*** MODE APERÇU - Utilisez --supprimer pour agir ***")
        sys.exit(0)
    
    if results["branches_to_delete"]:
        del_res = delete_remote_branches(repo_path, results["branches_to_delete"], args.remote)
        print(f"Suppressions réussies: {len(del_res['deleted'])}")
        print(f"Échecs: {len(del_res['failed'])}")
        if del_res["failed"]:
            sys.exit(1)

if __name__ == "__main__":
    main()
