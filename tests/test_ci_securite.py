# -*- coding: utf-8 -*-
"""Chaîne d'approvisionnement de la CI (audit sécurité du 30/09/2026).

- Chaque action distante (`uses: propriétaire/dépôt@…`) est figée par le SHA complet de
  son commit, le tag en commentaire (`@<40 hexa> # v7`) : un tag se déplace, un SHA non.
  Dependabot (`github-actions`) met à jour le SHA et le commentaire ensemble. Les
  images Docker `ghcr.io/esphome/esphome:latest` d'esphome-tab5.yml ne sont pas des
  `uses:` et restent sur `latest` exprès (canari d'ESPHome, ADR-0016).
- Chaque workflow déclare ses `permissions:` au niveau du workflow, au lieu d'hériter
  du réglage du dépôt.
- esptool, qui tourne à côté de la clé de signature dans publication.yml, s'installe
  depuis des fichiers figés (versions exactes, empreintes, `--require-hashes`) ; la
  CI des PR rejoue exactement les mêmes commandes.
"""
import re
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
WORKFLOWS = REPO / ".github" / "workflows"
PUBLICATION = REPO / "tools" / "publication"
FIGES = ("requirements-esptool-build.txt", "requirements-esptool.txt")

USES = re.compile(r"^\s*(?:-\s+)?uses:\s*([^\s#]+)\s*(#.*)?$")
SHA = re.compile(r"^[0-9a-f]{40}$")
ETAPE_ESPTOOL = "esptool (espsecure), versions et empreintes figées"


def _workflows():
    return sorted(WORKFLOWS.glob("*.yml"))


def _uses():
    """[(fichier:ligne, cible, commentaire)] de chaque `uses:` des workflows."""
    trouves = []
    for chemin in _workflows():
        for n, ligne in enumerate(chemin.read_text(encoding="utf-8").splitlines(), 1):
            m = USES.match(ligne)
            if m:
                trouves.append((f"{chemin.name}:{n}", m.group(1), (m.group(2) or "").lstrip("# ").strip()))
    return trouves


# ─── Actions figées par SHA ──────────────────────────────────────────────────

def test_les_uses_sont_trouves():
    # Garde du test lui-même : sans `uses:` trouvé, il passerait sans rien vérifier.
    assert len([u for u in _uses() if not u[1].startswith("./")]) >= 30


def test_chaque_action_distante_est_figee_par_sha():
    fautifs = []
    for ou, cible, commentaire in _uses():
        if cible.startswith("./"):
            continue  # workflow du dépôt lui-même (site.yml)
        _, _, ref = cible.partition("@")
        if not SHA.match(ref) or not re.match(r"^v\d", commentaire):
            fautifs.append(f"{ou} {cible} {commentaire}".strip())
    assert not fautifs, ("action non figée par le SHA complet de son commit (« @<40 hexa> # vX », "
                         "SHA lu par `gh api repos/<propriétaire>/<dépôt>/commits/<tag> --jq .sha`) : "
                         + " ; ".join(fautifs))


def test_un_meme_tag_un_meme_sha():
    vus = {}
    for ou, cible, commentaire in _uses():
        if cible.startswith("./"):
            continue
        action, _, ref = cible.partition("@")
        vus.setdefault((action.split("/")[0] + "/" + action.split("/")[1], commentaire), set()).add(ref)
    divergents = {k: v for k, v in vus.items() if len(v) > 1}
    assert not divergents, f"même tag, SHA différents : {divergents}"


# ─── Permissions explicites ──────────────────────────────────────────────────

def test_chaque_workflow_declare_ses_permissions():
    sans = [c.name for c in _workflows()
            if "permissions" not in (yaml.safe_load(c.read_text(encoding="utf-8")) or {})]
    assert not sans, f"workflow sans bloc `permissions:` au niveau du workflow : {sans}"


def test_la_ci_des_pr_est_en_lecture_seule():
    wf = yaml.safe_load((WORKFLOWS / "esphome-tab5.yml").read_text(encoding="utf-8"))
    assert wf["permissions"] == {"contents": "read"}
    ecritures = {nom: job["permissions"] for nom, job in wf["jobs"].items()
                 if any(v != "read" for v in (job.get("permissions") or {}).values())}
    assert not ecritures, ecritures
    # dorny/paths-filter lit les fichiers d'une PR par l'API.
    assert wf["jobs"]["changes"]["permissions"] == {"contents": "read", "pull-requests": "read"}


# ─── esptool figé ────────────────────────────────────────────────────────────

def _exigences(nom):
    """{paquet: [empreintes]} d'un fichier figé (lignes jointes aux « \\ »)."""
    texte = (PUBLICATION / nom).read_text(encoding="utf-8").replace("\r\n", "\n").replace("\\\n", " ")
    exigences = {}
    for ligne in texte.splitlines():
        ligne = ligne.split("#", 1)[0].strip()
        if not ligne:
            continue
        paquet = ligne.split()[0]
        assert re.match(r"^[A-Za-z0-9_.\-]+==[0-9][^=\s]*$", paquet), f"{nom} : version non figée « {paquet} »"
        exigences[paquet] = re.findall(r"--hash=sha256:[0-9a-f]{64}\b", ligne)
    return exigences


def test_esptool_et_ses_dependances_sont_figes_avec_empreintes():
    tout = {}
    for nom in FIGES:
        exigences = _exigences(nom)
        sans = [p for p, h in exigences.items() if not h]
        assert not sans, f"{nom} : sans empreinte : {sans}"
        tout.update(exigences)
    noms = {p.split("==")[0].lower() for p in tout}
    assert {"esptool", "setuptools", "cryptography", "pyserial"} <= noms, sorted(noms)


def _etape_esptool(chemin):
    wf = yaml.safe_load(chemin.read_text(encoding="utf-8"))
    etapes = [e for job in wf["jobs"].values() for e in job.get("steps", []) if e.get("name") == ETAPE_ESPTOOL]
    assert len(etapes) == 1, f"{chemin.name} : étape « {ETAPE_ESPTOOL} » introuvable ou en double"
    return etapes[0]["run"]


def _installation(run):
    """Les commandes d'installation, sans l'emplacement des outils (o=…) ni le venv."""
    lignes = [ligne.strip() for ligne in run.replace("\\\n", " ").splitlines()]
    return [ligne for ligne in lignes if "pip_reessai.sh" in ligne or "espsecure" in ligne]


def test_publication_installe_esptool_depuis_les_fichiers_figes():
    run = _etape_esptool(WORKFLOWS / "publication.yml")
    assert "o=outils-workflow/tools" in run
    commandes = _installation(run)
    assert len(commandes) == 3, commandes
    for commande, nom in zip(commandes, FIGES):
        assert "--require-hashes" in commande and f"-r \"$o/publication/{nom}\"" in commande, commande
    assert "--no-build-isolation" in commandes[1] and "--no-binary esptool" in commandes[1]
    texte = (WORKFLOWS / "publication.yml").read_text(encoding="utf-8")
    assert not re.search(r"pip install[^\n]*esptool", texte), "esptool installé hors des fichiers figés"


def test_la_ci_des_pr_rejoue_l_installation_de_la_publication():
    assert _installation(_etape_esptool(WORKFLOWS / "esphome-tab5.yml")) == \
        _installation(_etape_esptool(WORKFLOWS / "publication.yml"))
