# -*- coding: utf-8 -*-
"""Tableau de bord Tab5 : le marqueur de version que la macro écrit, et ce qu'il faut pour le
réécrire en un clic (ADR-0075). Pur : testé par pytest sans Home Assistant.

[AI-CONTEXT]
@role La macro custom_templates/tab5_dashboard.jinja écrit, à la racine de la configuration du
      tableau, une clé `tab5_dashboard` : {macro, version, + les arguments de l'appel}.
      `version` est celle du capteur « Tab5 · HA files version » au moment du rendu. Ici :
      lire ce marqueur (lire_marqueur), dire si le tableau est en retard sur les fichiers
      actifs (en_retard), construire l'appel de la macro à partir de ce marqueur
      (source_modele), refuser un rendu inutilisable avant d'écraser quoi que ce soit
      (valider_rendu), et garder l'ancien tableau (sauvegarder). Ce qui parle à Home
      Assistant (lire les tableaux, rendre le modèle, enregistrer) est dans tableau_ha.py.
@contraintes Le marqueur vient d'un tableau que l'utilisateur peut avoir modifié : rien de ce
      qu'il contient n'est interprété comme du code. Le nom de la macro et ceux des
      arguments sont pris dans MACROS (liste fermée) ; les valeurs passent comme variables du
      modèle, jamais dans son texte. Une clé inconnue de la liste est ignorée.
@ai_warning La clé du marqueur est lue par la macro (tab5_marqueur) ET ici : tests/
      test_integration_tableau.py rend la vraie macro et lit son marqueur avec ce module.
"""
from __future__ import annotations

import datetime as dt
import json
import os
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

# Clé racine de la configuration du tableau. Le frontend de HA accepte une clé inconnue à la
# racine (hui-editor.ts : `type({title, views})` de superstruct, qui tolère les clés en plus) et
# la garde à l'enregistrement.
CLE_MARQUEUR = "tab5_dashboard"
# Macros du fichier custom_templates/tab5_dashboard.jinja qui écrivent un tableau entier, avec
# les arguments que le marqueur garde (le reste de leur signature n'est pas un choix de
# l'utilisateur : vues_seules est réservé à tab5_dashboard_ecrans).
MACROS: dict[str, tuple[str, ...]] = {
    "tab5_dashboard": ("adresse", "langue", "appareil", "chemin", "titre"),
    "tab5_dashboard_ecrans": ("adresse", "langue"),
}
SAUVEGARDES = "tab5_sauvegardes/tableaux"
TEMPORAIRE = ".tab5-tmp"
GARDER = 5

# Code d'erreur de TableauRefuse = clé de traduction (issues.tableau_a_mettre_a_jour.fix_flow.error).
ERREUR_API = "api"                  # l'API interne de HA manque ou a changé
ERREUR_INTROUVABLE = "introuvable"  # le tableau a disparu
ERREUR_RENDU = "rendu"              # la macro n'a rien donné d'utilisable (tablette introuvable…)
ERREUR_VERSION = "version"          # les fichiers de cette version ne sont pas encore actifs


class TableauRefuse(Exception):
    """La mise à jour est refusée AVANT d'écraser le tableau : rien n'a été changé."""

    def __init__(self, code: str, detail: str = "") -> None:
        super().__init__(f"{code} : {detail}" if detail else code)
        self.code = code
        self.detail = detail


@dataclass(frozen=True)
class Marqueur:
    version: str                                   # vide : la macro n'a pas lu de version
    macro: str                                     # une clé de MACROS
    args: dict[str, str] = field(default_factory=dict)


def lire_marqueur(config: Any) -> Marqueur | None:
    """Le marqueur de la configuration d'un tableau ; None s'il n'y en a pas ou s'il n'est
    pas celui de la macro (macro hors MACROS, forme inattendue)."""
    if not isinstance(config, dict):
        return None
    brut = config.get(CLE_MARQUEUR)
    if not isinstance(brut, dict) or brut.get("macro") not in MACROS:
        return None
    macro = brut["macro"]
    version = brut.get("version")
    args = {cle: brut[cle] for cle in MACROS[macro] if isinstance(brut.get(cle), str)}
    return Marqueur(version if isinstance(version, str) else "", macro, args)


_VERSION = re.compile(r"^v?(\d+)\.(\d+)(?:\.(\d+))?(?:-([A-Za-z]+)\.?(\d+)?)?$")
_PRE = {"dev": 0, "alpha": 0, "beta": 1, "rc": 2}


def cle_version(texte: str | None) -> tuple[int, int, int, int, int] | None:
    """Clé de comparaison d'une version « 3.9.1 », « 3.9.0-rc.7 » (avant la 3.9.0) ; None si
    ce n'en est pas une (« dépôt », « unknown », vide)."""
    m = _VERSION.match((texte or "").strip())
    if not m:
        return None
    majeure, mineure, correctif, pre, numero = m.groups()
    base = (int(majeure), int(mineure), int(correctif or 0))
    if pre is None:
        return (*base, 1, 0)
    return (*base, 0, _PRE.get(pre.lower(), 0) * 1000 + int(numero or 0))


def en_retard(marqueur: Marqueur, version_fichiers: str) -> bool:
    """Le tableau a-t-il été écrit par une version plus ancienne que celle des fichiers
    actifs ? Une version de fichiers illisible (« dépôt » chez l'auteur) : jamais. Un marqueur
    sans version lisible : oui (un rendu l'écrit à jour)."""
    actuelle = cle_version(version_fichiers)
    if actuelle is None:
        return False
    posee = cle_version(marqueur.version)
    return posee is None or posee < actuelle


def source_modele(marqueur: Marqueur) -> tuple[str, dict[str, Any]]:
    """(texte du modèle, variables) qui rendent le tableau comme le fait l'utilisateur dans
    Outils de développement → Modèle. Les valeurs des arguments passent par la variable
    `args`, jamais dans le texte."""
    if marqueur.macro not in MACROS:
        raise ValueError(f"macro inconnue : {marqueur.macro!r}")
    appel = ", ".join(f"{cle}=args['{cle}']" for cle in MACROS[marqueur.macro] if cle in marqueur.args)
    source = (f"{{% from 'tab5_dashboard.jinja' import {marqueur.macro} %}}"
              f"{{{{ {marqueur.macro}({appel}) }}}}")
    return source, {"args": dict(marqueur.args)}


def valider_rendu(rendu: Any, version_fichiers: str, marqueur: Marqueur) -> None:
    """Refuse (TableauRefuse) un rendu qu'il serait dommage d'enregistrer à la place du
    tableau : ce n'est pas un tableau (la macro n'a écrit qu'un commentaire : tablette
    introuvable), il n'a plus de marqueur (un modèle d'avant le marqueur est encore chargé), ou
    le marqueur dit une autre version que celle des fichiers actifs (le capteur n'a pas suivi)."""
    if not isinstance(rendu, dict) or not isinstance(rendu.get("views"), list) or not rendu["views"]:
        raise TableauRefuse(ERREUR_RENDU, "le modèle n'a pas écrit de vues (tablette introuvable ?)")
    nouveau = lire_marqueur(rendu)
    if nouveau is None:
        raise TableauRefuse(ERREUR_RENDU, "le rendu n'a pas de marqueur de version (modèle d'avant ?)")
    if nouveau.macro != marqueur.macro:
        raise TableauRefuse(ERREUR_RENDU, "le rendu ne vient pas de la même macro")
    if cle_version(nouveau.version) != cle_version(version_fichiers):
        raise TableauRefuse(ERREUR_VERSION, f"marqueur {nouveau.version!r}, fichiers {version_fichiers!r}")


# ─── Sauvegarde de l'ancien tableau ──────────────────────────────────────────

def _nom(texte: str | None, defaut: str) -> str:
    return re.sub(r"[^0-9A-Za-z.-]", "_", texte or defaut)


def sauvegarder(config_dir: Path, url_path: str | None, ancien: dict[str, Any], version_avant: str,
                maintenant: dt.datetime) -> Path:
    """Écrit l'ancien tableau dans tab5_sauvegardes/tableaux/<date>_<adresse>_<version>.json
    (du JSON, qui est aussi du YAML : à recoller tel quel dans l'éditeur de configuration brute),
    de façon atomique, et ne garde que les GARDER plus récentes de ce tableau. Renvoie le
    fichier."""
    dossier = config_dir / SAUVEGARDES
    dossier.mkdir(parents=True, exist_ok=True)
    adresse = _nom(url_path, "defaut")
    fin = f"_{adresse}_{_nom(version_avant, 'inconnue')}.json"
    etiquette = f"{maintenant:%Y%m%d-%H%M%S}"
    nom, n = etiquette + fin, 1
    while (dossier / nom).exists():  # deux dans la même seconde : jamais écraser une sauvegarde
        n += 1
        nom = f"{etiquette}-{n}{fin}"
    cible = dossier / nom
    tmp = cible.with_name(cible.name + TEMPORAIRE)
    try:
        tmp.write_text(json.dumps(ancien, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
        os.replace(tmp, cible)
    finally:
        tmp.unlink(missing_ok=True)
    de_ce_tableau = re.compile(r"^\d{8}-\d{6}(?:-\d+)?_" + re.escape(adresse) + r"_.+\.json$")
    celui_ci = sorted((p for p in dossier.iterdir() if de_ce_tableau.match(p.name)),
                      key=lambda p: (p.stat().st_mtime_ns, p.name))
    for ancienne in celui_ci[:-GARDER]:
        ancienne.unlink(missing_ok=True)
    return cible
