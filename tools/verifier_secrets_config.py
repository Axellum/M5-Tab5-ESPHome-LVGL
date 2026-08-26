import os
import re
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

def check_file(filepath):
    """
    Analyse un fichier YAML pour détecter des secrets en clair.
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

                # Vérification des patterns
                for secret_type, pattern in SECRET_PATTERNS.items():
                    if pattern.search(line):
                        findings.append((line_num, secret_type))
                        
    except Exception as e:
        print(f"Erreur lors de la lecture de {filepath}: {e}")
        
    return findings

def main():
    # Détection dynamique de la racine du projet
    # On part du fichier courant et on remonte jusqu'à trouver le dossier racine
    # (celui qui contient le dossier 'tools')
    current_file = Path(__file__).resolve()
    root_dir = current_file.parent.parent
    
    # Fichiers à ignorer (ex: secrets.yaml lui-même, car c'est là qu'ils DOIVENT être)
    ignore_files = {'secrets.yaml'}
    
    all_findings = {}
    
    # Parcours récursif des fichiers .yaml
    for yaml_file in root_dir.rglob('*.yaml'):
        if yaml_file.name in ignore_files:
            continue
            
        # On ignore aussi les dossiers d'archives ou de cache
        if 'archives' in yaml_file.parts or '.claude' in yaml_file.parts:
            continue
            
        results = check_file(yaml_file)
        if results:
            all_findings[str(yaml_file)] = results

    if all_findings:
        print("⚠️ SECRETS DÉTECTÉS EN CLAIR :")
        for file, issues in all_findings.items():
            # On affiche le chemin relatif pour plus de clarté
            rel_path = os.path.relpath(file, root_dir)
            for line, secret_type in issues:
                print(f"Fichier: {rel_path} | Ligne: {line} | Type: {secret_type}")
        return 1
    
    print("✅ Aucun secret en clair détecté dans les fichiers de configuration.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
