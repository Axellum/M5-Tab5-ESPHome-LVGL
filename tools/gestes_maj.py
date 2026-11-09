#!/usr/bin/env python3
"""tools/gestes_maj.py — Ce qu'une mise à jour demande à l'utilisateur, profil par profil.

[AI-CONTEXT] Les notes de release ont fait croire à « télécharger et copier des fichiers à
chaque version » (discussion #278, audit du 09/11/2026) alors qu'avec l'intégration HACS il
n'y a que deux gestes : *Installer*, puis redémarrer Home Assistant. Ce script écrit, d'après le
DIFF de deux révisions git (tags), ce qui est REQUIS, FACULTATIF ou INUTILE pour chacun des
deux profils, sans jamais les mélanger dans une même phrase :

- **intégration HACS** : « Mises à jour → Tab5 — fichiers HA · HA files » ; l'intégration
  sauvegarde, pose les fichiers, vérifie, recharge et enchaîne le firmware ;
- **zip à la main** : `tab5_home_assistant.zip` décompressé dans `config/`.
  (`tab5_hacs.zip` n'est lu que par HACS : jamais un geste de l'utilisateur.)

Ce qui est lu, révision contre révision :

- **firmware** : `Tab5/` hors `lang/`, hors documentation (`*.md`) et sources de dessin
  (`*.svg`), plus `tab5-ha-hmi.yaml` ;
- **fichiers Home Assistant** : exactement ceux que `tools/publication/archive_ha.py`
  met dans l'archive (`CONTENU`, source unique) : packages, modèles Jinja, blueprint,
  `optionnel/` ; les `snippets/` et les `*.md` ne s'installent pas, ils ne comptent pas ;
- **macro du tableau de bord** : `custom_templates/tab5_dashboard.jinja` et les modèles qu'elle
  importe au moment du rendu (hors blocs `{% raw %}`, évalués plus tard par Home Assistant) ;
- **code de l'intégration** : `custom_components/tab5/` ;
- **entités des packages** ajoutées ou retirées (`default_entity_id`, aides `input_*`,
  `counter`, `timer`). Celles du firmware lui-même ne sont PAS comptées ;
- **version minimale de Home Assistant** (`hacs.json`) ;
- **contrat HA ↔ firmware** : `contrat_api.matrice()` (ordre de mise à jour).

Règle de « requis » (rien n'est déclaré requis sans preuve) :

- REQUIS = s'en passer casse quelque chose, ou laisse sans effet le geste d'avant. Preuves
  admises : le contrat (HA refuse un appel du nouveau firmware avec les anciens fichiers, ou
  l'inverse ; le statut dépend alors de l'autre geste et le dit : « REQUIS si vous installez le
  firmware »), une version minimale de Home Assistant relevée, le rechargement du YAML après
  une copie à la main. Le tableau de bord n'est JAMAIS requis : c'est la configuration de
  l'utilisateur, et une carte dont l'entité a disparu s'affiche en erreur sans rien casser ;
- FACULTATIF = la version change cet élément mais l'ancien continue de marcher sans lui ; la
  liste des fichiers est donnée, c'est à l'auteur de la note d'écrire ce que cela apporte ;
- INUTILE = la version ne change pas cet élément, ou l'intégration le fait seule.

Limites connues (à ne pas contourner par des devinettes) : le contenu des payloads n'est pas lu
(voir `contrat_api.py`) ; un changement de comportement d'un package n'est pas jugé ; sans
contrat lisible (révision trop ancienne, git absent), rien n'est déclaré requis et le dit ;
pour un blueprint seul modifié, « recharger le YAML » est quand même annoncé requis après la
copie (c'est ce que dit le guide) : non vérifié qu'il soit indispensable.

    python tools/gestes_maj.py v3.9.0..v3.9.1          # profils, en français (Markdown)
    python tools/gestes_maj.py v3.9.0 v3.9.1 --en      # en anglais (notes de release)
    python tools/gestes_maj.py v3.9.0                  # contre HEAD : avant de poser le tag d'une release
    python tools/gestes_maj.py v3.9.0..v3.9.1 --notes  # le bloc « À faire » / « Upgrading from »
    python tools/gestes_maj.py v3.9.0..v3.9.1 --json   # les faits et les statuts, pour un test
    python tools/gestes_maj.py v3.9.0..v3.9.1 --fichiers   # liste aussi les fichiers du firmware

Lecture seule : ne modifie rien. Lancé depuis la racine du dépôt, tags récupérés
(`git fetch --tags`)."""
from __future__ import annotations

import argparse
import fnmatch
import json
import re
import subprocess
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent / "publication"))

from archive_ha import CONTENU  # noqa: E402  (source unique de ce que l'archive installe)

REPO = Path(__file__).resolve().parent.parent
HA_DIR = "HomeAssistant_Config"
MACRO = "custom_templates/tab5_dashboard.jinja"        # chemin dans l'archive
INTEGRATION = "custom_components/tab5/"
FIRMWARE_RACINE = "tab5-ha-hmi.yaml"
FIRMWARE_DOSSIER = "Tab5/"
FIRMWARE_EXCLUS = ("Tab5/lang/",)
FIRMWARE_EXCLUS_FIN = (".md", ".svg")
# Aides de HA dont la clé de premier niveau est un entity_id (sans `default_entity_id`).
DOMAINES_AIDE = ("input_boolean", "input_number", "input_select", "input_text",
                 "input_datetime", "input_button", "counter", "timer")

REQUIS, FACULTATIF, INUTILE = "requis", "facultatif", "inutile"
CATEGORIES = ("packages", "modeles", "blueprint", "optionnel")
_CIBLE_PAR_DOSSIER = {"packages": "packages", "custom_templates": "modeles",
                      "blueprints/automation/tab5": "blueprint", "optionnel": "optionnel"}


# ─── Git ─────────────────────────────────────────────────────────────────────

def _git(args: list[str], racine: Path) -> str:
    return subprocess.run(["git", *args], cwd=racine, capture_output=True, check=True,
                          encoding="utf-8").stdout


def _texte(racine: Path, ref: str, chemin: str) -> str | None:
    """Contenu d'un fichier à une révision (None s'il n'y est pas)."""
    try:
        return _git(["show", f"{ref}:{chemin}"], racine)
    except subprocess.CalledProcessError:
        return None


def _verifier_ref(racine: Path, ref: str) -> None:
    try:
        _git(["rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}"], racine)
    except subprocess.CalledProcessError:
        raise SystemExit(f"révision inconnue : {ref} (git fetch --tags ?)")


# ─── Classement des fichiers ─────────────────────────────────────────────────

def classer(chemin: str) -> tuple[str, str] | None:
    """(catégorie, chemin dans l'archive) d'un fichier que l'archive installe, sinon None.
    Même liste que `archive_ha.CONTENU` : un dossier, un motif, pas de sous-dossier."""
    if not chemin.startswith(HA_DIR + "/"):
        return None
    relatif = chemin[len(HA_DIR) + 1:]
    for dossier, motif, cible in CONTENU:
        prefixe = dossier + "/"
        if relatif.startswith(prefixe):
            nom = relatif[len(prefixe):]
            if "/" not in nom and fnmatch.fnmatch(nom, motif):
                return _CIBLE_PAR_DOSSIER[dossier], f"{cible}/{nom}"
    return None


def est_firmware(chemin: str) -> bool:
    if chemin == FIRMWARE_RACINE:
        return True
    return (chemin.startswith(FIRMWARE_DOSSIER) and not chemin.startswith(FIRMWARE_EXCLUS)
            and not chemin.endswith(FIRMWARE_EXCLUS_FIN))


def entrees_du_diff(racine: Path, depuis: str, vers: str) -> list[tuple[str, str]]:
    """(statut A/M/D, chemin) ; un renommage donne D (ancien) + A (nouveau) : pour l'utilisateur
    d'un zip, c'est un fichier à supprimer et un à copier. Le D d'un renommage dont la
    destination est aussi du firmware est omis : le firmware compte un fichier déplacé une fois."""
    sortie = _git(["diff", "--name-status", "-M", "-z", depuis, vers], racine)
    morceaux = sortie.split("\0")
    entrees: list[tuple[str, str]] = []
    i = 0
    while i < len(morceaux) and morceaux[i]:
        statut = morceaux[i][:1]
        if statut in "RC":                       # R100 \0 ancien \0 nouveau
            ancien, nouveau = morceaux[i + 1], morceaux[i + 2]
            if not (est_firmware(ancien) and est_firmware(nouveau)):
                entrees.append(("D", ancien))
            entrees.append(("A", nouveau))
            i += 3
        else:
            entrees.append((statut, morceaux[i + 1]))
            i += 2
    return entrees


# ─── Faits ───────────────────────────────────────────────────────────────────

@dataclass
class ContratLu:
    """Ce que dit la matrice du contrat (tools/contrat_api.py) de ces deux révisions."""
    ordre: str                       # indifferent | ha | firmware | ensemble
    fw_nouveau_ha_ancien_casse: bool  # nouveau firmware + anciens fichiers HA : HA refuse un appel
    fw_ancien_ha_nouveau_casse: bool  # ancien firmware + nouveaux fichiers HA : idem
    details: list[str] = field(default_factory=list)   # cassé / dégradé, une ligne chacun


@dataclass
class Faits:
    depuis: str
    vers: str
    firmware: list[str] = field(default_factory=list)
    packages: list[str] = field(default_factory=list)      # chemins dans l'archive, tous statuts
    modeles: list[str] = field(default_factory=list)
    blueprint: list[str] = field(default_factory=list)
    optionnel: list[str] = field(default_factory=list)
    retires: list[str] = field(default_factory=list)       # fichiers installés plus livrés
    tableau: list[str] = field(default_factory=list)       # macro et modèles qu'elle importe, changés
    integration: list[str] = field(default_factory=list)
    entites_ajoutees: list[str] = field(default_factory=list)
    entites_retirees: list[str] = field(default_factory=list)
    entites_citees: list[str] = field(default_factory=list)   # retirées ET citées par l'ancienne macro
    entites_lues: bool = True                              # False : packages illisibles
    ha_minimum: tuple[str | None, str | None] = (None, None)
    contrat: ContratLu | None = None

    @property
    def fichiers_ha(self) -> list[str]:
        return self.packages + self.modeles + self.blueprint + self.optionnel

    @property
    def ha_min_releve(self) -> bool:
        """Relevée, pas apparue : `hacs.json` naît à la 3.7.0 (avant : None), ce n'est pas un relèvement."""
        avant, apres = (_version_tuple(v) for v in self.ha_minimum)
        return avant is not None and apres is not None and apres > avant


def _version_tuple(version: str | None) -> tuple[int, ...] | None:
    if not version or not re.fullmatch(r"\d+(\.\d+)*", version):
        return None
    return tuple(int(x) for x in version.split("."))


def _dependances_rendu(racine: Path, ref: str) -> set[str]:
    """La macro et les modèles qu'elle importe au rendu (fermeture), à une révision. Les blocs
    `{% raw %}` sont ignorés : Home Assistant les évalue à l'affichage, pas au collage."""
    vus: set[str] = set()
    a_voir = [MACRO]
    while a_voir:
        chemin = a_voir.pop()
        if chemin in vus:
            continue
        vus.add(chemin)
        texte = _texte(racine, ref, f"{HA_DIR}/{chemin}")
        if texte is None:
            continue
        texte = re.sub(r"\{%-?\s*raw\s*-?%\}.*?\{%-?\s*endraw\s*-?%\}", "", texte, flags=re.S)
        for nom in re.findall(r"\{%-?\s*from\s+'([\w.]+\.jinja)'\s+import", texte):
            a_voir.append(f"custom_templates/{nom}")
    return vus


def _entites_d_un_package(texte: str) -> set[str]:
    """entity_id fixés par un package : `default_entity_id` n'importe où, clés des aides `input_*`."""
    try:
        from contrat_api import charger
        doc = charger(texte)
    except Exception:  # noqa: BLE001 — YAML que le chargeur refuse : repli sur le motif
        return set(re.findall(r"default_entity_id:\s*([a-z_]+\.[a-z0-9_]+)", texte))
    ids: set[str] = set()

    def marcher(noeud):
        if isinstance(noeud, dict):
            for cle, valeur in noeud.items():
                if cle == "default_entity_id" and isinstance(valeur, str):
                    ids.add(valeur)
                else:
                    marcher(valeur)
        elif isinstance(noeud, list):
            for element in noeud:
                marcher(element)

    if isinstance(doc, dict):
        marcher(doc)
        for domaine in DOMAINES_AIDE:
            if isinstance(doc.get(domaine), dict):
                ids |= {f"{domaine}.{cle}" for cle in doc[domaine]}
    return ids


def _entites(racine: Path, ref: str) -> tuple[set[str], bool]:
    """(entity_id des packages à `ref`, lecture complète ?)."""
    ids: set[str] = set()
    complet = True
    try:
        chemins = _git(["ls-tree", "-r", "--name-only", ref, "--", f"{HA_DIR}/packages"], racine).split("\n")
    except subprocess.CalledProcessError:
        return ids, False
    for chemin in (c for c in chemins if c.endswith(".yaml")):
        texte = _texte(racine, ref, chemin)
        if texte is None:
            complet = False
            continue
        ids |= _entites_d_un_package(texte)
    return ids, complet


def _ha_minimum(racine: Path, ref: str) -> str | None:
    texte = _texte(racine, ref, "hacs.json")
    if texte is None:
        return None
    try:
        return json.loads(texte).get("homeassistant")
    except ValueError:
        return None


def lire_contrat(depuis: str, vers: str) -> ContratLu | None:
    """Le contrat HA ↔ firmware entre les deux révisions, ou None s'il n'est pas lisible."""
    try:
        import contrat_api
        m = contrat_api.matrice(depuis, vers)
        details = [t for x in (m.fw_precedent_ha_courant, m.fw_courant_ha_precedent) for t in x.casse + x.degrade]
        return ContratLu(m.ordre,
                         m.fw_courant_ha_precedent.niveau >= contrat_api.CASSE,
                         m.fw_precedent_ha_courant.niveau >= contrat_api.CASSE,
                         sorted(set(details)))
    except Exception:  # noqa: BLE001 — révision trop ancienne, git ou PyYAML absent : le dire, ne pas deviner
        return None


def analyser(depuis: str, vers: str, racine: Path = REPO, contrat=lire_contrat) -> Faits:
    """Les faits de `depuis..vers`. `contrat(depuis, vers)` est injectable (tests sur un dépôt
    jetable : contrat_api lit toujours le vrai dépôt)."""
    for ref in (depuis, vers):
        _verifier_ref(racine, ref)
    f = Faits(depuis, vers)
    changes: dict[str, list[str]] = {c: [] for c in CATEGORIES}
    modeles_changes: set[str] = set()
    for statut, chemin in entrees_du_diff(racine, depuis, vers):
        if est_firmware(chemin):
            f.firmware.append(chemin)
        elif chemin.startswith(INTEGRATION) and "__pycache__" not in chemin:
            f.integration.append(chemin)
        elif (rang := classer(chemin)) is not None:
            categorie, dans_archive = rang
            changes[categorie].append(dans_archive)
            if statut == "D":
                f.retires.append(dans_archive)
            if categorie == "modeles":
                modeles_changes.add(dans_archive)
    f.packages, f.modeles = changes["packages"], changes["modeles"]
    f.blueprint, f.optionnel = changes["blueprint"], changes["optionnel"]
    dependances = _dependances_rendu(racine, vers) | _dependances_rendu(racine, depuis)
    f.tableau = sorted(modeles_changes & dependances)
    avant, complet_a = _entites(racine, depuis)
    apres, complet_b = _entites(racine, vers)
    f.entites_lues = complet_a and complet_b
    f.entites_ajoutees, f.entites_retirees = sorted(apres - avant), sorted(avant - apres)
    if f.entites_retirees:
        anciens = [_texte(racine, depuis, f"{HA_DIR}/{c}") or "" for c in _dependances_rendu(racine, depuis)]
        f.entites_citees = [e for e in f.entites_retirees if any(e in texte for texte in anciens)]
    f.ha_minimum = (_ha_minimum(racine, depuis), _ha_minimum(racine, vers))
    f.contrat = contrat(depuis, vers)
    return f


# ─── Statuts ─────────────────────────────────────────────────────────────────

@dataclass
class Geste:
    cle: str
    statut: str
    raison_cle: str
    valeurs: dict = field(default_factory=dict)
    condition: str = ""        # clé de TEXTES[..]["cond"] : le statut dépend de l'autre geste


def _liste(chemins: list[str], max_: int = 6) -> str:
    vus = [f"`{c}`" for c in chemins[:max_]]
    if len(chemins) > max_:
        vus.append(f"… (+{len(chemins) - max_})")
    return ", ".join(vus)


def statuts(f: Faits) -> dict[str, list[Geste]]:
    """{profil: [gestes]} ; profils « hacs » et « zip »."""
    ha = bool(f.fichiers_ha)
    integ = bool(f.integration)
    fw = bool(f.firmware)
    recharge = bool(f.packages or f.modeles or f.optionnel)
    c = f.contrat
    ha_avant_fw = bool(c and c.fw_nouveau_ha_ancien_casse)
    fw_avec_ha = bool(c and c.fw_ancien_ha_nouveau_casse)
    v = {"fichiers": _liste(f.fichiers_ha), "n": len(f.fichiers_ha), "retires": _liste(f.retires),
         "ha_min": f.ha_minimum[1], "ha_min_avant": f.ha_minimum[0],
         "entites_ajoutees": _liste(f.entites_ajoutees), "entites_retirees": _liste(f.entites_retirees),
         "tableau": _liste(f.tableau), "integration": len(f.integration)}

    # Tableau de bord (les deux profils)
    v["entites_citees"] = _liste(f.entites_citees)
    # Jamais REQUIS : c'est la configuration de l'utilisateur, et une carte dont l'entité a
    # disparu s'affiche en erreur sans rien casser d'autre.
    if f.entites_citees:
        tableau = Geste("tableau", FACULTATIF, "tableau_citees", v)
    elif f.entites_retirees:
        tableau = Geste("tableau", FACULTATIF, "tableau_retirees", v)
    elif f.tableau:
        tableau = Geste("tableau", FACULTATIF, "tableau_ajout" if f.entites_ajoutees else "tableau_facultatif", v)
    elif f.entites_ajoutees:
        tableau = Geste("tableau", FACULTATIF, "tableau_entites", v)
    else:
        tableau = Geste("tableau", INUTILE, "tableau_inutile", v)

    # Firmware (les deux profils, la raison change)
    if not fw:
        firmware = ("firmware_inutile", INUTILE, "")
    elif fw_avec_ha and ha:
        firmware = ("firmware_requis", REQUIS, "fichiers")
    else:
        firmware = ("firmware_facultatif", FACULTATIF, "")

    prerequis = ([Geste("prerequis_ha", REQUIS, "prerequis_ha", v)] if f.ha_min_releve else [])

    # Profil A : intégration HACS
    if ha_avant_fw:
        maj = Geste("hacs_maj", REQUIS, "hacs_maj_requis", v, condition="firmware")
    elif ha or integ:
        maj = Geste("hacs_maj", FACULTATIF, "hacs_maj_facultatif" if ha else "hacs_maj_code", v)
    else:
        maj = Geste("hacs_maj", INUTILE, "hacs_maj_inutile", v)
    hacs = prerequis + [maj,
                        Geste("hacs_firmware", firmware[1], "hacs_" + firmware[0], v, condition=firmware[2]),
                        Geste("hacs_recharger", INUTILE, "hacs_recharger", v),
                        Geste("tableau", tableau.statut, tableau.raison_cle, v)]

    # Profil B : zip à la main
    if not ha:
        copier = Geste("zip_copier", INUTILE, "zip_copier_inutile", v)
    elif ha_avant_fw:
        copier = Geste("zip_copier", REQUIS, "zip_copier_requis", v, condition="firmware")
    else:
        copier = Geste("zip_copier", FACULTATIF, "zip_copier_facultatif", v)
    copie_faite = copier.statut != INUTILE
    zip_ = prerequis + [
        copier,
        Geste("zip_recharger", REQUIS if (copie_faite and (recharge or f.blueprint)) else INUTILE,
              "zip_recharger" if (copie_faite and (recharge or f.blueprint)) else "zip_recharger_inutile", v,
              condition="copie" if (copie_faite and (recharge or f.blueprint)) else ""),
        Geste("zip_blueprint", copier.statut if f.blueprint else INUTILE,
              "zip_blueprint" if f.blueprint else "zip_blueprint_inutile", v),
        Geste("zip_firmware", firmware[1], "zip_" + firmware[0], v, condition="copie" if firmware[2] else ""),
        Geste("tableau", tableau.statut, tableau.raison_cle, v),
        Geste("zip_hacs", INUTILE, "zip_hacs", v),
    ]
    return {"hacs": hacs, "zip": zip_}


# ─── Textes ──────────────────────────────────────────────────────────────────

LANGUES = ("fr", "en")

TEXTES = {
    "fr": {
        "statut": {REQUIS: "REQUIS", FACULTATIF: "FACULTATIF", INUTILE: "INUTILE"},
        "cond": {"copie": "si vous copiez les fichiers", "fichiers": "si vous posez les fichiers",
                 "firmware": "si vous installez le firmware"},
        "titre": "Mise à jour {depuis} → {vers} : ce que ça demande",
        "faits": "Ce qui change", "faits_col": ("Élément", "Changé ?", "Détail"),
        "oui": "oui", "non": "non",
        "f_firmware": "Firmware (`Tab5/` hors `lang/`, documentation et `.svg` exclus)",
        "f_ha": "Fichiers Home Assistant installés (packages, modèles, blueprint, `optionnel/`)",
        "f_tableau": "Macro du tableau de bord (et modèles qu'elle importe)",
        "f_blueprint": "Blueprint",
        "f_integration": "Code de l'intégration (`custom_components/tab5/`)",
        "f_entites": "Entités des packages (le firmware n'est pas compté)",
        "f_ha_min": "Version minimale de Home Assistant",
        "f_contrat": "Contrat HA ↔ firmware",
        "n_fichiers": "{n} fichier(s)", "n_retires": "dont retiré(s) : {liste}",
        "ni": "ni ajoutée ni retirée", "ajoutees": "ajoutée(s) : {liste}", "retirees": "retirée(s) : {liste}",
        "ha_min_inchange": "inchangée ({v})", "ha_min_change": "{avant} → {apres}", "ha_min_inconnue": "non lue",
        "contrat_inconnu": "non lu : rien n'est déclaré requis, voir `tools/contrat_api.py --matrice`",
        "contrat": {"indifferent": "compatible dans les deux sens", "firmware": "le firmware d'abord",
                    "ha": "Home Assistant d'abord", "ensemble": "firmware et Home Assistant ensemble"},
        "entites_non_lues": "(packages illisibles : liste incomplète)",
        "profil_hacs": "Avec l'intégration HACS",
        "profil_hacs_qui": "*Paramètres → Mises à jour → « Tab5 — fichiers HA · HA files »*",
        "profil_zip": "À la main (zip)",
        "profil_zip_qui": "`tab5_home_assistant.zip` décompressé dans `config/`",
        "col": ("Geste", "Pour cette version", "Pourquoi"),
        "g": {
            "prerequis_ha": "Home Assistant à jour",
            "hacs_maj": "*Installer* la mise à jour « fichiers HA », puis redémarrer Home Assistant",
            "hacs_firmware": "Firmware",
            "hacs_recharger": "Recharger le YAML",
            "zip_copier": "Télécharger `tab5_home_assistant.zip` et copier les fichiers dans `config/`",
            "zip_recharger": "*Toute la configuration YAML*, puis l'action `homeassistant.reload_custom_templates` (ou redémarrer Home Assistant)",
            "zip_blueprint": "Blueprint importé par son adresse : l'importer de nouveau",
            "zip_firmware": "Firmware : entité « Firmware » → *Installer*",
            "tableau": "Tableau de bord, si vous en avez un (étape 7)",
            "zip_hacs": "`tab5_hacs.zip`",
        },
        "r": {
            "prerequis_ha": "cette version demande Home Assistant {ha_min} au moins (la précédente : {ha_min_avant}).",
            "hacs_maj_requis": "le contrat HA ↔ firmware le prouve : le nouveau firmware casse avec les anciens fichiers. À faire avant le firmware ({fichiers}).",
            "hacs_maj_facultatif": "{n} fichier(s) Home Assistant changent : {fichiers}. Sans eux, l'ancienne version continue de marcher.{retires_hacs}",
            "hacs_maj_code": "seul le code de l'intégration change ({integration} fichier(s)) ; HACS demande de toute façon un redémarrage.",
            "hacs_maj_inutile": "aucun fichier Home Assistant ni code de l'intégration ne change, seul le numéro de version : HACS propose quand même la mise à jour. Installer le firmware suffit ; passer par HACS le fait enchaîner tout seul.",
            "hacs_firmware_inutile": "le code du firmware ne change pas, seul le numéro de version.",
            "hacs_firmware_facultatif": "le firmware change mais ne casse pas avec l'ancienne version des fichiers. L'intégration l'enchaîne seule après les fichiers (case « Mettre ensuite la tablette à jour » cochée, par défaut) ; sinon entité « Firmware » → *Installer*.",
            "hacs_firmware_requis": "le contrat le prouve : les nouveaux fichiers cassent avec l'ancien firmware. L'intégration l'enchaîne seule après les fichiers (case « Mettre ensuite la tablette à jour » cochée) ; sinon entité « Firmware » → *Installer*.",
            "hacs_recharger": "l'intégration recharge seule, dans le bon ordre.",
            "zip_copier_inutile": "aucun fichier Home Assistant ne change.",
            "zip_copier_facultatif": "{n} fichier(s) Home Assistant changent : {fichiers}. Sans eux, l'ancienne version continue de marcher.{retires_zip}",
            "zip_copier_requis": "le contrat HA ↔ firmware le prouve : le nouveau firmware casse avec les anciens fichiers. À faire avant le firmware ({fichiers}).{retires_zip}",
            "zip_recharger": "à faire après la copie : les packages et les modèles ne sont lus qu'au chargement.",
            "zip_recharger_inutile": "aucun fichier n'est copié.",
            "zip_blueprint": "seulement si vous l'avez importé par son adresse plutôt que décompressé : la copie ne le met pas à jour.",
            "zip_blueprint_inutile": "le blueprint ne change pas.",
            "zip_firmware_inutile": "le code du firmware ne change pas, seul le numéro de version.",
            "zip_firmware_facultatif": "le firmware change mais ne casse pas avec l'ancienne version des fichiers. Rien ne l'enchaîne à la main.",
            "zip_firmware_requis": "le contrat le prouve : les nouveaux fichiers cassent avec l'ancien firmware. Rien ne l'enchaîne à la main.",
            "zip_hacs": "lue par HACS seulement ; ne la téléchargez pas à la main.",
            "tableau_citees": "des entités sont retirées et l'ancienne macro les cite : {entites_citees}. Sans refaire le tableau, les cartes correspondantes s'affichent en erreur ; le reste marche. Refaire ses points 2 et 3 les retire.",
            "tableau_retirees": "des entités sont retirées ({entites_retirees}) mais l'ancienne macro ne les cite pas en toutes lettres : à vérifier dans votre tableau.",
            "tableau_ajout": "la macro change ({tableau}) et des entités s'ajoutent ({entites_ajoutees}). Refaire ses points 2 et 3 les fait apparaître ; l'ancien tableau continue de marcher.",
            "tableau_facultatif": "la macro change ({tableau}) ; l'ancien tableau continue de marcher. Refaire ses points 2 et 3 pour en profiter.",
            "tableau_entites": "des entités s'ajoutent ({entites_ajoutees}) sans que la macro change ; l'ancien tableau continue de marcher.",
            "tableau_inutile": "ni la macro ni les entités des packages ne changent.",
            "retires_hacs": " L'intégration retire seule : {retires}.",
            "retires_zip": " À supprimer vous-même, ils ne sont plus livrés : {retires}.",
        },
        "limites": "« Requis » n'est écrit que s'il est prouvé (contrat, version de Home Assistant, rechargement après copie). Le contenu des payloads et les entités du firmware ne sont pas lus : à l'auteur de la note de dire ce que chaque changement apporte.",
        "notes_hacs": "Avec l'intégration (Paramètres → Mises à jour)",
        "notes_zip": "À la main (zip)",
        "notes_titre": "**À faire en mettant à jour depuis la {depuis}** :",   # comme le CHANGELOG
    },
    "en": {
        "statut": {REQUIS: "REQUIRED", FACULTATIF: "OPTIONAL", INUTILE: "NOT NEEDED"},
        "cond": {"copie": "if you copy the files", "fichiers": "if you put the files in place",
                 "firmware": "if you install the firmware"},
        "titre": "Update {depuis} → {vers}: what it asks of you",
        "faits": "What changes", "faits_col": ("Item", "Changed?", "Detail"),
        "oui": "yes", "non": "no",
        "f_firmware": "Firmware (`Tab5/` except `lang/`, documentation and `.svg` excluded)",
        "f_ha": "Home Assistant files installed (packages, templates, blueprint, `optionnel/`)",
        "f_tableau": "Dashboard macro (and the templates it imports)",
        "f_blueprint": "Blueprint",
        "f_integration": "Integration code (`custom_components/tab5/`)",
        "f_entites": "Package entities (the firmware's own are not counted)",
        "f_ha_min": "Minimum Home Assistant version",
        "f_contrat": "Home Assistant ↔ firmware contract",
        "n_fichiers": "{n} file(s)", "n_retires": "of which removed: {liste}",
        "ni": "none added or removed", "ajoutees": "added: {liste}", "retirees": "removed: {liste}",
        "ha_min_inchange": "unchanged ({v})", "ha_min_change": "{avant} → {apres}", "ha_min_inconnue": "not read",
        "contrat_inconnu": "not read: nothing is declared required, see `tools/contrat_api.py --matrice`",
        "contrat": {"indifferent": "compatible both ways", "firmware": "firmware first",
                    "ha": "Home Assistant first", "ensemble": "firmware and Home Assistant together"},
        "entites_non_lues": "(packages unreadable: incomplete list)",
        "profil_hacs": "With the HACS integration",
        "profil_hacs_qui": "*Settings → Updates → « Tab5 — fichiers HA · HA files »*",
        "profil_zip": "By hand (zip)",
        "profil_zip_qui": "`tab5_home_assistant.zip` unzipped into `config/`",
        "col": ("Step", "For this release", "Why"),
        "g": {
            "prerequis_ha": "Home Assistant up to date",
            "hacs_maj": "*Install* the « fichiers HA » update, then restart Home Assistant",
            "hacs_firmware": "Firmware",
            "hacs_recharger": "Reload the YAML",
            "zip_copier": "Download `tab5_home_assistant.zip` and copy the files into `config/`",
            "zip_recharger": "*All YAML configuration*, then the action `homeassistant.reload_custom_templates` (or restart Home Assistant)",
            "zip_blueprint": "Blueprint imported from its URL: import it again",
            "zip_firmware": "Firmware: « Firmware » entity → *Install*",
            "tableau": "Dashboard, if you have one (step 7)",
            "zip_hacs": "`tab5_hacs.zip`",
        },
        "r": {
            "prerequis_ha": "this release needs Home Assistant {ha_min} or later (the previous one: {ha_min_avant}).",
            "hacs_maj_requis": "the Home Assistant ↔ firmware contract proves it: the new firmware breaks with the old files. Do it before the firmware ({fichiers}).",
            "hacs_maj_facultatif": "{n} Home Assistant file(s) change: {fichiers}. Without them the previous version keeps working.{retires_hacs}",
            "hacs_maj_code": "only the integration code changes ({integration} file(s)); HACS asks for a restart anyway.",
            "hacs_maj_inutile": "no Home Assistant file and no integration code change, only the version number: HACS offers the update anyway. Installing the firmware is enough; going through HACS makes it follow by itself.",
            "hacs_firmware_inutile": "the firmware code does not change, only the version number.",
            "hacs_firmware_facultatif": "the firmware changes but does not break with the previous version of the files. The integration chains it by itself after the files (« Then update the tablet » ticked, the default); otherwise « Firmware » entity → *Install*.",
            "hacs_firmware_requis": "the contract proves it: the new files break with the old firmware. The integration chains it by itself after the files (« Then update the tablet » ticked); otherwise « Firmware » entity → *Install*.",
            "hacs_recharger": "the integration reloads by itself, in the right order.",
            "zip_copier_inutile": "no Home Assistant file changes.",
            "zip_copier_facultatif": "{n} Home Assistant file(s) change: {fichiers}. Without them the previous version keeps working.{retires_zip}",
            "zip_copier_requis": "the Home Assistant ↔ firmware contract proves it: the new firmware breaks with the old files. Do it before the firmware ({fichiers}).{retires_zip}",
            "zip_recharger": "after the copy: packages and templates are only read when loaded.",
            "zip_recharger_inutile": "no file is copied.",
            "zip_blueprint": "only if you imported it from its URL instead of unzipping it: the copy does not update it.",
            "zip_blueprint_inutile": "the blueprint does not change.",
            "zip_firmware_inutile": "the firmware code does not change, only the version number.",
            "zip_firmware_facultatif": "the firmware changes but does not break with the previous version of the files. Nothing chains it by hand.",
            "zip_firmware_requis": "the contract proves it: the new files break with the old firmware. Nothing chains it by hand.",
            "zip_hacs": "read by HACS only; do not download it by hand.",
            "tableau_citees": "entities are removed and the old macro names them: {entites_citees}. Without redoing the dashboard, the matching cards show an error; the rest works. Doing its items 2 and 3 again removes them.",
            "tableau_retirees": "entities are removed ({entites_retirees}) but the old macro does not name them outright: check your dashboard.",
            "tableau_ajout": "the macro changes ({tableau}) and entities are added ({entites_ajoutees}). Doing its items 2 and 3 again shows them; the old dashboard keeps working.",
            "tableau_facultatif": "the macro changes ({tableau}); the old dashboard keeps working. Do its items 2 and 3 again to benefit.",
            "tableau_entites": "entities are added ({entites_ajoutees}) without the macro changing; the old dashboard keeps working.",
            "tableau_inutile": "neither the macro nor the package entities change.",
            "retires_hacs": " The integration removes by itself: {retires}.",
            "retires_zip": " Delete them yourself, they are no longer shipped: {retires}.",
        },
        "limites": "« Required » is only written when proven (contract, Home Assistant version, reload after a copy). Payload contents and the firmware's own entities are not read: the author of the note says what each change brings.",
        "notes_hacs": "With the integration (Settings → Updates)",
        "notes_zip": "By hand (zip)",
        "notes_titre": "## Upgrading from {depuis}",                            # comme les notes de release
    },
}


def _retires(f: Faits, t: dict, cle: str) -> str:
    return t["r"][cle].format(retires=_liste(f.retires)) if f.retires else ""


def raison(f: Faits, g: Geste, langue: str) -> str:
    t = TEXTES[langue]
    valeurs = dict(g.valeurs)
    valeurs["retires_hacs"] = _retires(f, t, "retires_hacs")
    valeurs["retires_zip"] = _retires(f, t, "retires_zip")
    return t["r"][g.raison_cle].format(**{k: ("—" if v is None else v) for k, v in valeurs.items()})


def libelle(g: Geste, langue: str) -> str:
    return TEXTES[langue]["g"][g.cle]


def etiquette_statut(g: Geste, langue: str) -> str:
    """« REQUIS », ou « REQUIS si vous copiez les fichiers » quand le statut en dépend."""
    t = TEXTES[langue]
    return t["statut"][g.statut] + (" " + t["cond"][g.condition] if g.condition else "")


# ─── Rendu ───────────────────────────────────────────────────────────────────

def _cellule(texte: str) -> str:
    return texte.replace("|", "\\|").replace("\n", " ")


def table_faits(f: Faits, langue: str, fichiers: bool = False) -> str:
    t = TEXTES[langue]
    oui, non = t["oui"], t["non"]

    def ligne(nom, change, detail=""):
        return f"| {_cellule(nom)} | {change} | {_cellule(detail)} |"

    ha_detail = _liste(f.fichiers_ha, 12) if f.fichiers_ha else ""
    if f.retires:
        ha_detail += (" ; " if ha_detail else "") + t["n_retires"].format(liste=_liste(f.retires))
    if f.entites_ajoutees or f.entites_retirees:
        entites = " ; ".join(x for x in (
            t["ajoutees"].format(liste=_liste(f.entites_ajoutees)) if f.entites_ajoutees else "",
            t["retirees"].format(liste=_liste(f.entites_retirees)) if f.entites_retirees else "") if x)
        entites_change = oui
    else:
        entites, entites_change = t["ni"], non
    if not f.entites_lues:
        entites += " " + t["entites_non_lues"]
    avant, apres = f.ha_minimum
    if apres is None:
        ha_min = t["ha_min_inconnue"]
    elif avant == apres:
        ha_min = t["ha_min_inchange"].format(v=apres)
    else:
        ha_min = t["ha_min_change"].format(avant=avant or "?", apres=apres)
    contrat = (t["contrat"][f.contrat.ordre] if f.contrat else t["contrat_inconnu"])
    lignes = [
        f"| {' | '.join(t['faits_col'])} |", "|---|---|---|",
        ligne(t["f_firmware"], oui if f.firmware else non,
              t["n_fichiers"].format(n=len(f.firmware)) if f.firmware else ""),
        ligne(t["f_ha"], oui if f.fichiers_ha else non, ha_detail),
        ligne(t["f_tableau"], oui if f.tableau else non, _liste(f.tableau)),
        ligne(t["f_blueprint"], oui if f.blueprint else non, _liste(f.blueprint)),
        ligne(t["f_integration"], oui if f.integration else non,
              t["n_fichiers"].format(n=len(f.integration)) if f.integration else ""),
        ligne(t["f_entites"], entites_change, entites),
        ligne(t["f_ha_min"], oui if (avant != apres and apres) else non, ha_min),
        ligne(t["f_contrat"], "", contrat),
    ]
    if fichiers and f.firmware:
        lignes += ["", ", ".join(f"`{c}`" for c in f.firmware)]
    return "\n".join(lignes)


def table_profil(f: Faits, profil: str, langue: str) -> str:
    t = TEXTES[langue]
    lignes = [f"| {' | '.join(t['col'])} |", "|---|---|---|"]
    for g in statuts(f)[profil]:
        lignes.append(f"| {_cellule(libelle(g, langue))} | **{etiquette_statut(g, langue)}** | "
                      f"{_cellule(raison(f, g, langue))} |")
    return "\n".join(lignes)


def rendu(f: Faits, langue: str = "fr", fichiers: bool = False) -> str:
    t = TEXTES[langue]
    sortie = [f"### {t['titre'].format(depuis=f.depuis, vers=f.vers)}", "",
              f"#### {t['faits']}", "", table_faits(f, langue, fichiers), ""]
    for profil in ("hacs", "zip"):
        sortie += [f"#### {t['profil_' + profil]}", "", f"{t['profil_' + profil + '_qui']}", "",
                   table_profil(f, profil, langue), ""]
    sortie += [f"> {t['limites']}", ""]
    return "\n".join(sortie)


def notes(f: Faits, langue: str = "fr") -> str:
    """Le bloc de la note de release : une liste par profil, jamais mélangés. La phrase sur
    ce que chaque changement apporte reste à écrire à la main."""
    t = TEXTES[langue]
    depuis = f.depuis.removeprefix("v")
    sortie = [t["notes_titre"].format(depuis=depuis), ""]
    for profil, titre in (("hacs", t["notes_hacs"]), ("zip", t["notes_zip"])):
        sortie.append(f"**{titre}**")
        sortie.append("")
        for g in statuts(f)[profil]:
            sortie.append(f"- {libelle(g, langue)} — **{etiquette_statut(g, langue)}**"
                          f"{' :' if langue == 'fr' else ':'} {raison(f, g, langue)}")
        sortie.append("")
    sortie.append(f"<!-- python tools/gestes_maj.py {f.depuis}..{f.vers} -->")
    return "\n".join(sortie) + "\n"


def en_json(f: Faits) -> str:
    return json.dumps({"faits": asdict(f),
                       "profils": {p: [{"geste": g.cle, "statut": g.statut} for g in gs]
                                   for p, gs in statuts(f).items()}},
                      ensure_ascii=False, indent=2)


# ─── Ligne de commande ───────────────────────────────────────────────────────

def lire_plage(args: list[str]) -> tuple[str, str]:
    """`A..B`, `A B`, ou `A` seul (contre HEAD : avant de poser le tag d'une release)."""
    if len(args) == 1 and ".." in args[0]:
        depuis, _, vers = args[0].partition("..")
    elif len(args) == 1:
        depuis, vers = args[0], "HEAD"
    elif len(args) == 2:
        depuis, vers = args
    else:
        raise SystemExit("révisions attendues : `v3.9.0..v3.9.1`, `v3.9.0 v3.9.1` ou `v3.9.0` (contre HEAD)")
    if not depuis or not vers or ".." in vers:
        raise SystemExit("révisions attendues : `v3.9.0..v3.9.1`, `v3.9.0 v3.9.1` ou `v3.9.0` (contre HEAD)")
    return depuis, vers


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("plage", nargs="+", help="`v3.9.0..v3.9.1` ou `v3.9.0 v3.9.1` (tags ou révisions git)")
    ap.add_argument("--en", action="store_true", help="en anglais (notes de release)")
    ap.add_argument("--notes", action="store_true", help="seulement le bloc de la note de release")
    ap.add_argument("--json", action="store_true", help="les faits et les statuts en JSON")
    ap.add_argument("--fichiers", action="store_true", help="liste aussi les fichiers du firmware")
    args = ap.parse_args(argv)
    depuis, vers = lire_plage(args.plage)
    faits = analyser(depuis, vers)
    langue = "en" if args.en else "fr"
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if args.json:
        print(en_json(faits))
    elif args.notes:
        print(notes(faits, langue), end="")
    else:
        print(rendu(faits, langue, args.fichiers), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
