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
    Vérification CRITIQUE : la valeur du secret ne doit JAMAIS apparaître dans
    ce que renvoie check_file — c'est ce qui rend l'outil sûr à lancer en CI,
    où la sortie est publique.

    La boucle porte sur le tuple ENTIER (`repr`), pas sur le seul libellé de
    type : ce libellé est une constante ("HA_TOKEN", "GENERIC_SECRET"…), donc
    le comparer au secret ne pouvait rien prouver.
    """
    secret_val = "eyJ_SECRET_TOKEN_123456789"
    content = f"token: {secret_val}"
    f = tmp_path / "leak_test.yaml"
    f.write_text(content)

    findings = check_file(str(f))
    # Sans cette garde, une liste vide ferait passer le test sans rien vérifier.
    assert findings, "le secret de test doit être détecté, sinon l'assertion suivante est vide de sens"
    for finding in findings:
        assert secret_val not in repr(finding)


def test_ignores_uppercase_placeholder(tmp_path):
    """Une valeur factice en MAJUSCULES (docs d'installation, secrets.yaml de la CI)
    n'est pas un secret ; la même clé avec une vraie valeur reste signalée."""
    f = tmp_path / "installation.md"
    f.write_text('wifi_password: "YOUR_WIFI_PASSWORD"\n'
                 'api_encryption_key: "CLE_BASE64_32_OCTETS"\n'
                 'wifi_password: "vraiMotDePasse_2026"\n')

    assert check_file(str(f)) == [(3, "GENERIC_SECRET")]


def test_allowlist_pragma_skips_line(tmp_path):
    """Un faux secret documenté se marque ligne par ligne, pas fichier par fichier."""
    f = tmp_path / "CHANGELOG.md"
    f.write_text("`token: eyJ_SECRET_TOKEN_123456789` <!-- pragma: allowlist secret -->\n"
                 "token: eyJ_SECRET_TOKEN_123456789\n")

    assert check_file(str(f)) == [(2, "GENERIC_SECRET")]


def _git(repo, *args):
    import subprocess
    subprocess.run(["git", *args], cwd=repo, check=True, capture_output=True)


def test_tracked_files_covers_markdown_and_example_only_if_tracked(tmp_path):
    """Seuls les fichiers suivis comptent (un fichier local gitignoré a le droit
    de contenir des valeurs réelles), mais .md et .example en font partie."""
    from tools.verifier_secrets_config import tracked_files

    _git(tmp_path, "init", "-q")
    (tmp_path / "README.md").write_text("x")
    (tmp_path / "automations.yaml.example").write_text("x")
    (tmp_path / "script.py").write_text("x")
    (tmp_path / "local.yaml").write_text("x")          # jamais ajouté
    _git(tmp_path, "add", "README.md", "automations.yaml.example", "script.py")

    names = sorted(p.name for p in tracked_files(tmp_path))
    assert names == ["README.md", "automations.yaml.example"]


def test_tracked_secrets_yaml_is_reported(tmp_path, monkeypatch, capsys):
    """Un secrets.yaml suivi est une fuite même si aucun motif ne le reconnaît
    (clé base64 avec `+` ou `/`) — et la valeur n'est jamais affichée."""
    import tools.verifier_secrets_config as vsc

    key = "26IVe/bZ/aI+T8G96D7aYZlM2EH8QUG8JtsZ50HxaCE="
    (tmp_path / "secrets.yaml").write_text(f'api_encryption_key: "{key}"\n')
    monkeypatch.setattr(vsc, "tracked_files", lambda root: [tmp_path / "secrets.yaml"])

    assert vsc.main(tmp_path) == 1
    out = capsys.readouterr().out
    assert "SECRETS_FILE_TRACKED" in out and key not in out
