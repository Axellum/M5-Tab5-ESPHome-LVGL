# -*- coding: utf-8 -*-
"""Fragments du CHANGELOG (`changelog.d/`, tools/changelog_assembler.py ; audit du
30/10/2026, lot A).

Chaque PR dépose son entrée dans un fichier à elle au lieu d'écrire en tête de
`## [Unreleased]`, où deux PR parallèles entraient toujours en conflit. Ce test :
- valide les fragments présents (nom, sous-titres permis, puces) et que leur assemblage
  dans le vrai CHANGELOG passe (`--check`, joué par le job `python` de la CI) ;
- prouve l'assemblage sur des copies : place de chaque entrée, sous-titre créé dans
  l'ordre, préambule (étiquette du contrat) en tête, fins de ligne gardées, fragments
  supprimés, rien d'écrit sur une erreur.
"""
import os
import subprocess

import changelog_assembler as ca
import pytest
from tests.commun import REPO

CHANGELOG = """# Changelog

Intro.

## [Unreleased]

### Ajouté

- **Ancienne A** : déjà là.

### Corrigé

- **Ancienne C** : déjà là.

## [3.9.0] — 2026-10-25

Résumé.

### Ajouté

- **Publiée** : dans la 3.9.0.
"""


def _copie(tmp_path, changelog=CHANGELOG, fragments=None, crlf=False):
    cl = tmp_path / "CHANGELOG.md"
    texte = changelog.replace("\n", "\r\n") if crlf else changelog
    cl.write_bytes(texte.encode("utf-8"))
    d = tmp_path / "changelog.d"
    d.mkdir()
    (d / "README.md").write_text("# pas un fragment\n", encoding="utf-8")
    for nom, corps in (fragments or {}).items():
        (d / nom).write_text(corps, encoding="utf-8")
    return cl, d


def _section(texte, nom):
    debut = texte.index(f"## [{nom}]")
    fin = texte.find("\n## ", debut + 1)
    return texte[debut:fin if fin != -1 else len(texte)]


def test_fragments_du_depot_valides():
    erreurs = ca.verifier()
    assert not erreurs, "\n".join(erreurs) + "\nFormat : en-tête de tools/changelog_assembler.py"


def test_readme_des_fragments_present():
    assert (REPO / "changelog.d" / "README.md").is_file()


def test_slug_du_nom_de_branche():
    assert ca.slug("claude/changelog-fragments") == "claude-changelog-fragments.md"
    assert ca.slug("Fix/Thème Nom") == "fix-th-me-nom.md"
    assert ca.NOM.match(ca.slug("docs/readme_video.v2"))


def test_assemblage_en_tete_de_chaque_sous_titre(tmp_path):
    cl, d = _copie(tmp_path, fragments={
        "b-fix.md": "### Corrigé\n\n- **Nouveau C** : réparé.\n  suite de la ligne.\n",
        "a-ajout.md": "### Ajouté\n\n- **Nouveau A** : ajouté.\n",
    })
    sortie = ca.assembler(cl.read_text(encoding="utf-8"), ca.lire_fragments(d, ordre_git=False))
    sec = _section(sortie, "Unreleased")
    assert sec.index("Nouveau A") < sec.index("Ancienne A") < sec.index("### Corrigé")
    assert "### Corrigé\n\n- **Nouveau C** : réparé.\n  suite de la ligne.\n- **Ancienne C**" in sec
    assert "### Ajouté\n\n- **Nouveau A** : ajouté.\n- **Ancienne A**" in sec
    assert _section(sortie, "3.9.0") == _section(CHANGELOG, "3.9.0"), "la 3.9.0 ne bouge pas"


def test_ordre_des_fragments_puis_sous_titre_cree_a_sa_place(tmp_path):
    cl, d = _copie(tmp_path, fragments={
        "x.md": "### Modifié\n\n- **Mod X** : x.\n",
        "y.md": "### Modifié\n\n- **Mod Y** : y.\n\n### Sécurité\n\n- **Sec Y** : y.\n",
    })
    frags = ca.lire_fragments(d, ordre_git=False)      # par nom : x puis y
    sec = _section(ca.assembler(cl.read_text(encoding="utf-8"), frags), "Unreleased")
    # Modifié entre Ajouté et Corrigé, Sécurité après Corrigé ; les fragments dans leur ordre.
    assert (sec.index("### Ajouté") < sec.index("### Modifié") < sec.index("Mod X")
            < sec.index("Mod Y") < sec.index("### Corrigé") < sec.index("### Sécurité"))
    assert "### Modifié\n\n- **Mod X** : x.\n- **Mod Y** : y.\n\n### Corrigé" in sec
    assert sec.rstrip().endswith("- **Sec Y** : y.")


def test_preambule_en_tete_de_section(tmp_path):
    ligne = "**Contrat HA ↔ firmware** : le firmware d'abord (depuis v3.9.0)."
    cl, d = _copie(tmp_path, fragments={"c.md": f"{ligne}\n\n### Ajouté\n\n- **Avec contrat** : c.\n"})
    sec = _section(ca.assembler(cl.read_text(encoding="utf-8"), ca.lire_fragments(d, ordre_git=False)),
                   "Unreleased")
    assert sec.startswith(f"## [Unreleased]\n\n{ligne}\n\n### Ajouté\n\n- **Avec contrat**")


def test_section_de_version_et_section_vide(tmp_path):
    cl, d = _copie(tmp_path, changelog=CHANGELOG.replace(
        "### Ajouté\n\n- **Ancienne A** : déjà là.\n\n### Corrigé\n\n- **Ancienne C** : déjà là.\n\n", ""),
        fragments={"r.md": "### Corrigé\n\n- **Pour la release** : r.\n"})
    frags = ca.lire_fragments(d, ordre_git=False)
    texte = cl.read_text(encoding="utf-8")
    vide = _section(ca.assembler(texte, frags), "Unreleased")
    assert vide == "## [Unreleased]\n\n### Corrigé\n\n- **Pour la release** : r.\n"
    version = _section(ca.assembler(texte, frags, "3.9.0"), "3.9.0")
    assert version.index("Résumé.") < version.index("### Ajouté") < version.index("### Corrigé")
    with pytest.raises(ca.ErreurFragment):
        ca.assembler(texte, frags, "9.9.9")


def test_main_ecrit_garde_crlf_et_supprime(tmp_path):
    cl, d = _copie(tmp_path, crlf=True, fragments={"z.md": "### Ajouté\n\n- **Z** : z.\n"})
    assert ca.main(["--changelog", str(cl), "--fragments", str(d)]) == 0
    brut = cl.read_bytes().decode("utf-8")
    assert "- **Z** : z.\r\n- **Ancienne A**" in brut
    assert "\n" not in brut.replace("\r\n", "")
    assert sorted(p.name for p in d.iterdir()) == ["README.md"], "fragment supprimé, README gardé"
    assert ca.main(["--changelog", str(cl), "--fragments", str(d)]) == 0, "rien à assembler"


@pytest.mark.parametrize("nom, corps, raison", [
    ("Majuscule.md", "### Ajouté\n\n- **x**\n", "nom"),
    ("titre.md", "### Added\n\n- **x**\n", "Added"),
    ("section.md", "## [Unreleased]\n\n### Ajouté\n\n- **x**\n", "## [Unreleased]"),
    ("vide.md", "\n\n", "vide"),
    ("sans-puce.md", "### Ajouté\n\nTexte sans puce.\n", "puce"),
    ("double.md", "### Ajouté\n\n- **a**\n\n### Ajouté\n\n- **b**\n", "deux fois"),
    ("creux.md", "### Ajouté\n\n### Corrigé\n\n- **b**\n", "sans entrée"),
])
def test_fragment_refuse(tmp_path, nom, corps, raison):
    cl, d = _copie(tmp_path, fragments={nom: corps})
    erreurs = ca.verifier(cl, d)
    assert len(erreurs) == 1 and raison in erreurs[0], erreurs
    assert ca.main(["--changelog", str(cl), "--fragments", str(d)]) == 1
    assert cl.read_text(encoding="utf-8") == CHANGELOG and (d / nom).exists(), "rien d'écrit ni supprimé"


def test_ordre_git_le_plus_recent_d_abord(tmp_path):
    cl, d = _copie(tmp_path)

    def git(*args, date=None):
        env = dict(os.environ, GIT_AUTHOR_DATE=date, GIT_COMMITTER_DATE=date) if date else None
        subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false",
                        *args], cwd=tmp_path, check=True, capture_output=True, env=env)

    git("init", "-q")
    # « recent » commité d'abord mais daté après : l'ordre suit la date du commit, pas le nom.
    for nom, date in (("recent.md", "2026-10-20T10:00:00"), ("ancien.md", "2026-10-01T10:00:00")):
        (d / nom).write_text(f"### Ajouté\n\n- **{nom}**\n", encoding="utf-8")
        git("add", f"changelog.d/{nom}")
        git("commit", "-q", "-m", nom, date=date)
    (d / "pas-commite.md").write_text("### Ajouté\n\n- **pas-commite.md**\n", encoding="utf-8")
    assert [f.nom for f in ca.lire_fragments(d)] == ["pas-commite.md", "recent.md", "ancien.md"]
