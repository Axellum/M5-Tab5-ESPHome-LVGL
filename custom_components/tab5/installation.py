# -*- coding: utf-8 -*-
"""Les fichiers Home Assistant du Tab5 : plan, sauvegarde, écriture, restauration.

[AI-CONTEXT]
@role Le cœur de l'intégration « Tab5 », sans Home Assistant : ce module ne fait que lire
      et écrire des fichiers dans le dossier config/. __init__.py l'appelle dans un
      exécuteur, puis vérifie la configuration, recharge le YAML et prévient.
      tests/test_integration_tab5.py l'exécute dans un dossier temporaire.
@contenu Les fichiers embarqués (dossier fichiers/ de l'intégration, posé par
      tools/publication/archive_hacs.py à partir de la même liste que
      tab5_home_assistant.zip, archive_ha.entrees) vont :
      - dans packages/, custom_templates/ et blueprints/automation/tab5/ : toujours ;
      - tab5_optionnel/x.yaml → packages/x.yaml seulement si packages/x.yaml existe déjà
        (copié à la main, comme le dit le guide) : un optionnel que l'utilisateur a
        retiré n'est pas remis ;
      - le blueprint aussi par-dessus chaque copie importée par son URL
        (blueprints/automation/<auteur>/tab5_emplacements.yaml qui cite ce dépôt).
      Un fichier posé par la version précédente et absent de la nouvelle est retiré
      (rangé dans la sauvegarde). Rien d'autre n'est touché : les autres packages,
      modèles et blueprints de config/ restent à l'utilisateur.
@ai_instruction Pas d'import de homeassistant ni d'import relatif ici : pytest le charge
      seul, par son chemin. Toute écriture passe d'abord par la sauvegarde : restaurer()
      doit pouvoir tout remettre à l'octet près (test_integration_tab5.py le vérifie).
"""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
import re
import shutil
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

# Liste des fichiers de la release, écrite par archive_hacs.py dans fichiers/ : un fichier
# resté d'une version précédente dans le dossier n'en fait pas partie.
MANIFESTE = "MANIFESTE.json"
OPTIONNEL = "tab5_optionnel/"
BLUEPRINT = "blueprints/automation/tab5/tab5_emplacements.yaml"
# Une copie importée par l'URL du blueprint garde cette URL dans son `source_url`.
SIGNATURE_BLUEPRINT = "github.com/Axellum/M5-Tab5-ESPHome-LVGL/"
SAUVEGARDES = "tab5_sauvegardes"
# Suffixe de l'écriture atomique : ne finit pas par .yaml, que `!include_dir_named
# packages` chargerait si HA relisait la configuration au mauvais moment.
TEMPORAIRE = ".tab5-tmp"
# Premiers dossiers autorisés dans fichiers/ (et donc dans config/, après tab5_optionnel/).
RACINES = ("packages", "custom_templates", "blueprints", "tab5_optionnel")
# Version des fichiers, dans packages/tab5_health.yaml (archive_ha.MARQUEUR_VERSION).
VERSION_FICHIERS = re.compile(r"\"([^\"]*)\"  # >>> version de l'archive\r?$", re.M)
PACKAGE_VERSION = "packages/tab5_health.yaml"
# Clés de premier niveau d'un package : les domaines qu'il configure.
CLE_DE_PACKAGE = re.compile(r"^([a-z_][a-z0-9_]*):", re.M)


def empreinte(donnees: bytes) -> str:
    return hashlib.sha256(donnees).hexdigest()


def chemin_sur(chemin: str) -> bool:
    """Un chemin relatif, sans « .. », sous l'une des RACINES."""
    p = PurePosixPath(chemin)
    return (not p.is_absolute() and ".." not in p.parts and "\\" not in chemin
            and len(p.parts) >= 2 and p.parts[0] in RACINES)


def lire_embarques(dossier: Path) -> dict[str, bytes]:
    """{chemin dans l'archive : contenu} d'après fichiers/MANIFESTE.json ; {} sans manifeste
    (intégration copiée depuis le dépôt, sans les fichiers d'une release)."""
    manifeste = dossier / MANIFESTE
    if not manifeste.is_file():
        return {}
    liste = json.loads(manifeste.read_text(encoding="utf-8"))["fichiers"]
    if bad := [c for c in liste if not chemin_sur(c)]:
        raise ValueError(f"{MANIFESTE} : chemin refusé {bad[0]!r}")
    return {chemin: (dossier / chemin).read_bytes() for chemin in liste}


def copies_du_blueprint(config: Path) -> list[str]:
    """Les autres copies du blueprint (importé par son URL), relatives à config/."""
    racine = config / "blueprints" / "automation"
    if not racine.is_dir():
        return []
    copies = []
    for f in sorted(racine.rglob("tab5_emplacements.yaml")):
        relatif = f.relative_to(config).as_posix()
        if relatif == BLUEPRINT:
            continue
        try:
            if SIGNATURE_BLUEPRINT in f.read_text(encoding="utf-8", errors="replace"):
                copies.append(relatif)
        except OSError:
            continue
    return copies


@dataclass
class Plan:
    """Ce qu'une installation fait dans config/ (chemins relatifs, en « / »)."""
    contenus: dict[str, bytes] = field(default_factory=dict)  # toutes les cibles → contenu
    ecrire: list[str] = field(default_factory=list)           # absentes ou différentes
    identiques: list[str] = field(default_factory=list)
    retirer: list[str] = field(default_factory=list)          # posées avant, plus livrées
    modifies: list[str] = field(default_factory=list)         # changées à la main depuis
    # Déjà dans config/ sans que l'intégration les y ait mis (copiés à la main, souvent
    # avant sa première installation), et différents de ceux de la release : remplacés
    # quand même, avec une copie dans la sauvegarde ; __init__.py le dit (réparation).
    differents: list[str] = field(default_factory=list)
    installes: dict[str, str] = field(default_factory=dict)   # cible → empreinte

    @property
    def vide(self) -> bool:
        return not self.ecrire and not self.retirer


def planifier(config: Path, embarques: dict[str, bytes], precedents: dict[str, str]) -> Plan:
    """`precedents` : {cible : empreinte} de la dernière installation (vide la première fois)."""
    plan = Plan()
    for chemin, donnees in sorted(embarques.items()):
        if chemin.startswith(OPTIONNEL):
            cible = "packages/" + chemin.removeprefix(OPTIONNEL)
            if not (config / cible).is_file():
                continue
        else:
            cible = chemin
        plan.contenus[cible] = donnees
        if chemin == BLUEPRINT:
            for copie in copies_du_blueprint(config):
                plan.contenus[copie] = donnees

    for cible, donnees in plan.contenus.items():
        local = config / cible
        actuel = local.read_bytes() if local.is_file() else None
        if actuel is not None and cible in precedents and empreinte(actuel) != precedents[cible]:
            plan.modifies.append(cible)
        elif actuel is not None and cible not in precedents and actuel != donnees:
            plan.differents.append(cible)
        (plan.identiques if actuel == donnees else plan.ecrire).append(cible)
        plan.installes[cible] = empreinte(donnees)

    for cible in sorted(precedents):
        if cible not in plan.contenus and chemin_sur(cible) and (config / cible).is_file():
            plan.retirer.append(cible)
            if empreinte((config / cible).read_bytes()) != precedents[cible]:
                plan.modifies.append(cible)
    return plan


def etiquette(maintenant: dt.datetime, version_avant: str | None) -> str:
    """Nom du dossier de sauvegarde : date, puis la version remplacée."""
    version = re.sub(r"[^0-9A-Za-z.-]", "_", version_avant or "inconnue")
    return f"{maintenant:%Y%m%d-%H%M%S}_{version}"


def appliquer(config: Path, plan: Plan, nom: str) -> Path | None:
    """Sauvegarde ce qui va changer dans config/tab5_sauvegardes/<nom>/, puis écrit et
    retire. Renvoie le dossier de sauvegarde (None si rien n'existait avant)."""
    a_sauver = [c for c in plan.ecrire + plan.retirer if (config / c).is_file()]
    sauvegarde = None
    if a_sauver:
        sauvegarde = config / SAUVEGARDES / nom
        n = 1
        while sauvegarde.exists():
            n += 1
            sauvegarde = config / SAUVEGARDES / f"{nom}-{n}"
        for c in a_sauver:
            dest = sauvegarde / c
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(config / c, dest)
    try:
        for c in plan.ecrire:
            cible = config / c
            cible.parent.mkdir(parents=True, exist_ok=True)
            tmp = cible.with_name(cible.name + TEMPORAIRE)
            tmp.write_bytes(plan.contenus[c])
            os.replace(tmp, cible)
        for c in plan.retirer:
            (config / c).unlink()
    except OSError:
        # Disque plein, droits… : rien à moitié, tout est remis comme avant.
        for c in plan.ecrire:
            (config / c).with_name(PurePosixPath(c).name + TEMPORAIRE).unlink(missing_ok=True)
        restaurer(config, plan, sauvegarde)
        raise
    return sauvegarde


def restaurer(config: Path, plan: Plan, sauvegarde: Path | None) -> None:
    """Défait appliquer() : chaque fichier remis tel qu'il était, les nouveaux retirés."""
    for c in plan.ecrire:
        ancien = sauvegarde / c if sauvegarde else None
        if ancien is not None and ancien.is_file():
            shutil.copy2(ancien, config / c)
        else:
            (config / c).unlink(missing_ok=True)
    for c in plan.retirer:
        if sauvegarde is None:
            raise RuntimeError("fichier retiré sans sauvegarde")
        shutil.copy2(sauvegarde / c, config / c)


def nettoyer_sauvegardes(config: Path, garder: int) -> list[str]:
    """Garde les `garder` sauvegardes les plus récentes (leur nom commence par la date)."""
    racine = config / SAUVEGARDES
    if not racine.is_dir():
        return []
    dossiers = sorted(d for d in racine.iterdir() if d.is_dir())
    retires = []
    for d in dossiers[:-garder] if garder > 0 else dossiers:
        shutil.rmtree(d)
        retires.append(d.name)
    return retires


def version_installee(config: Path) -> str | None:
    """Version des fichiers en place (« dépôt » s'ils viennent du dépôt), None sans eux."""
    f = config / PACKAGE_VERSION
    if not f.is_file():
        return None
    m = VERSION_FICHIERS.search(f.read_text(encoding="utf-8", errors="replace"))
    return m.group(1) if m else None


def domaines(contenus: dict[str, bytes]) -> set[str]:
    """Domaines configurés par les packages posés (template, rest_command…)."""
    trouves: set[str] = set()
    for cible, donnees in contenus.items():
        if cible.startswith("packages/") and cible.endswith(".yaml"):
            trouves.update(CLE_DE_PACKAGE.findall(donnees.decode("utf-8", errors="replace")))
    return trouves


# Ordre du chargement à chaud : les entrées (input_*) et les commandes avant les modèles,
# scripts et automatisations qui les lisent (sinon « unknown entity » au premier passage).
ORDRE_DOMAINES = ("input_boolean", "input_button", "input_datetime", "input_number", "input_select",
                  "input_text", "rest_command", "template", "script", "automation")


def ordre_de_chargement(domaines_: set[str]) -> list[str]:
    """Domaines dans l'ordre où les charger ; un domaine inconnu passe avant `template`."""
    rang = {d: float(i) for i, d in enumerate(ORDRE_DOMAINES)}
    defaut = rang["template"] - 0.5
    return sorted(domaines_, key=lambda d: (rang.get(d, defaut), d))


def packages(contenus: dict[str, bytes]) -> set[str]:
    """Noms des packages posés, tels que `!include_dir_named packages` les nomme."""
    return {PurePosixPath(c).stem for c in contenus
            if c.startswith("packages/") and c.count("/") == 1 and c.endswith(".yaml")}
