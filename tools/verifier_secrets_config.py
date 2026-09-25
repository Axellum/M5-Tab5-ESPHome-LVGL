import os
import re
import subprocess
import sys
from pathlib import Path

# Patterns de détection des secrets
# 1. Jeton HA Long Lived Access Token (commence par eyJ...)
# 2. Clés d'API génériques (patterns courants comme 'api_key', 'token', 'password' suivis de valeurs)
# 3. Adresses IP privées codées en dur (ex: 192.168.x.x) - on signale si elles ne sont pas dans un !secret
# 4. Mots de passe MQTT ou autres identifiants

SECRET_PATTERNS = {
    "HA_TOKEN": re.compile(r'eyJ[a-zA-Z0-9\-_]+\.[a-zA-Z0-9\-_]+\.[a-zA-Z0-9\-_]+'),
    "GENERIC_SECRET": re.compile(r'(password|token|api_key|secret|key)\s*:\s*["\']?([a-zA-Z0-9_\-]{16,})["\']?', re.IGNORECASE),
    "PRIVATE_IP": re.compile(r'\b(192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(1[6-9]|2[0-9]|3[0-1])\.\d{1,3}\.\d{1,3})\b'),
}

# Valeur factice conventionnelle : MAJUSCULES, chiffres et `_` seulement
# (`YOUR_WIFI_PASSWORD`, `CLE_BASE64_32_OCTETS`, `CI_DUMMY_PASSWORD`). Les docs
# d'installation et le secrets.yaml factice de la CI en sont pleins ; un vrai mot
# de passe ou une vraie clé (base64, jeton) a des minuscules.
PLACEHOLDER_VALUE = re.compile(r'^[A-Z0-9_]+$')

# Exception explicite, ligne par ligne (convention detect-secrets) : pour un faux
# secret DOCUMENTÉ, ex. la sortie d'un test citée dans le CHANGELOG. En Markdown,
# la mettre dans un commentaire HTML : `<!-- pragma: allowlist secret -->`.
ALLOWLIST_PRAGMA = 'pragma: allowlist secret'

# Fichiers publics vérifiés : YAML (ESPHome, HA, workflows), modèles HA
# `.example` / `.jinja`, et Markdown (README, CHANGELOG, docs/).
CHECKED_SUFFIXES = ('.yaml', '.yml', '.example', '.jinja', '.md')


def check_file(filepath):
    """
    Analyse un fichier texte pour détecter des secrets en clair.
    Retourne une liste de tuples (ligne, type) si des secrets sont trouvés.
    """
    findings = []
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                stripped = line.strip()

                # On ignore les lignes qui utilisent explicitement !secret
                if '!secret' in stripped:
                    continue

                # On ignore les commentaires
                if stripped.startswith('#'):
                    continue

                if ALLOWLIST_PRAGMA in line:
                    continue

                # Vérification des patterns
                for secret_type, pattern in SECRET_PATTERNS.items():
                    if secret_type == "GENERIC_SECRET":
                        hit = any(not PLACEHOLDER_VALUE.match(m.group(2))
                                  for m in pattern.finditer(line))
                    else:
                        hit = pattern.search(line) is not None
                    if hit:
                        findings.append((line_num, secret_type))

    except Exception as e:
        print(f"Erreur lors de la lecture de {filepath}: {e}")

    return findings


def tracked_files(root_dir):
    """Fichiers suivis par git (index compris : un `git add` récent y est déjà).

    Seul ce qui est suivi part sur le dépôt public : les fichiers locaux
    gitignorés (secrets.yaml, placeholders.yaml, automations_tab5.yaml…) ont le
    droit de contenir des valeurs réelles. Retourne None hors d'un dépôt git.
    """
    try:
        out = subprocess.run(['git', 'ls-files', '-z'], cwd=root_dir,
                             capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    names = out.decode('utf-8').split('\0')
    return [root_dir / n for n in names if n.lower().endswith(CHECKED_SUFFIXES)]


def main(root_dir=None):
    # Détection dynamique de la racine du projet
    # On part du fichier courant et on remonte jusqu'à trouver le dossier racine
    # (celui qui contient le dossier 'tools') ; les tests passent la leur.
    if root_dir is None:
        root_dir = Path(__file__).resolve().parent.parent

    all_findings = {}

    files = tracked_files(root_dir)
    if files is None:
        # Repli hors git (archive zip) : ancien parcours, YAML seulement.
        files = [p for p in root_dir.rglob('*.yaml')
                 if p.name != 'secrets.yaml'
                 and 'archives' not in p.parts and '.claude' not in p.parts]

    for path in files:
        # Un secrets.yaml SUIVI est une fuite en soi, quel que soit son contenu
        # (une clé base64 avec `+` ou `/` échappe à GENERIC_SECRET).
        if path.name == 'secrets.yaml':
            all_findings[str(path)] = [(0, "SECRETS_FILE_TRACKED")]
            continue
        if not path.is_file():   # supprimé de l'arbre, suppression pas encore indexée
            continue
        results = check_file(path)
        if results:
            all_findings[str(path)] = results

    if all_findings:
        print("⚠️ SECRETS DÉTECTÉS EN CLAIR :")
        for file, issues in all_findings.items():
            # On affiche le chemin relatif pour plus de clarté
            rel_path = os.path.relpath(file, root_dir)
            for line, secret_type in issues:
                print(f"Fichier: {rel_path} | Ligne: {line} | Type: {secret_type}")
        return 1

    print(f"✅ Aucun secret en clair détecté ({len(files)} fichiers suivis).")
    return 0

if __name__ == "__main__":
    sys.exit(main())
