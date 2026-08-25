import pytest
import os
import sys
from pathlib import Path

# Ajout du dossier racine au PYTHONPATH pour permettre l'import de 'tools'
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from tools.verifier_secrets_config import check_file

def test_detects_ha_token(tmp_path):
    """Vérifie que le script détecte un jeton HA en clair."""
    content = "api_token: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoyNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2sS7S6S6S6S6S6S6S6S6S6S6S6S6S"
    f = tmp_path / "bad_config.yaml"
    f.write_text(content)
    
    findings = check_file(str(f))
    assert len(findings) > 0
    assert findings[0][1] == "HA_TOKEN"

def test_detects_generic_secret(tmp_path):
    """Vérifie que le script détecte un mot de passe en clair."""
    content = "mqtt_password: \"super_secret_password_12345\""
    f = tmp_path / "bad_mqtt.yaml"
    f.write_text(content)
    
    findings = check_file(str(f))
    assert len(findings) > 0
    assert findings[0][1] == "GENERIC_SECRET"

def test_detects_private_ip(tmp_path):
    """Vérifie que le script détecte une IP privée en clair."""
    content = "server_ip: 192.168.1.50"
    f = tmp_path / "bad_ip.yaml"
    f.write_text(content)
    
    findings = check_file(str(f))
    assert len(findings) > 0
    assert findings[0][1] == "PRIVATE_IP"

def test_ignores_secrets_tag(tmp_path):
    """Vérifie que le script ignore les valeurs utilisant !secret."""
    content = "mqtt_password: !secret mqtt_pass"
    f = tmp_path / "good_config.yaml"
    f.write_text(content)
    
    findings = check_file(str(f))
    assert len(findings) == 0

def test_ignores_comments(tmp_path):
    """Vérifie que le script ignore les secrets dans les commentaires."""
    content = "# This is a comment with a token eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
    f = tmp_path / "comment_config.yaml"
    f.write_text(content)
    
    findings = check_file(str(f))
    assert len(findings) == 0

def test_output_does_not_contain_secret_value(tmp_path):
    """
    Vérification CRITIQUE : la valeur du secret ne doit JAMAIS apparaître 
    dans le résultat de check_file.
    """
    secret_val = "eyJ_SECRET_TOKEN_123456789"
    content = f"token: {secret_val}"
    f = tmp_path / "leak_test.yaml"
    f.write_text(content)
    
    findings = check_file(str(f))
    for line, secret_type in findings:
        assert secret_val not in secret_type
