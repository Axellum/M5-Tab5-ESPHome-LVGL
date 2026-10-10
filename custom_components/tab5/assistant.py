# -*- coding: utf-8 -*-
"""Assistant de configuration : les pièces de Home Assistant → l'automatisation du blueprint.

[AI-CONTEXT]
@role Le cœur de l'assistant de l'intégration « Tab5 » (ADR-0052), sans Home Assistant :
      1. propose, d'après les registres de HA (pièces, appareils, entités), jusqu'à
         MAX_PIECES pièces et, pour chacune, ses tuiles (MAX_TUILES entités des domaines
         DOMAINES_AUTO, dans cet ordre), son capteur de température, d'humidité et sa clim
         (entrées piece_n_temperature / _humidite / _clim, ADR-0040) ;
      2. construit les entrées du blueprint tab5_emplacements à partir des choix ;
      3. lit et écrit automations.yaml : une automatisation `use_blueprint` ajoutée, ou les
         pièces d'une automatisation existante remplacées (seulement sur demande
         explicite), sauvegarde d'abord, écriture atomique, relecture qui vérifie le
         résultat.
      assistant_flux.py (ce qui parle à HA : formulaires, registres, rechargement) l'appelle.
@contraintes Ne jamais écraser ni créer en double : une automatisation du blueprint déjà là
      n'est remplacée que si l'utilisateur l'a demandé, et ses autres entrées restent. Un
      automations.yaml illisible par yaml.safe_load (étiquette !include, !secret…) ou que
      configuration.yaml n'inclut pas n'est pas écrit : l'assistant donne le YAML à coller.
@ai_instruction Pas d'import de homeassistant ni d'import relatif ici : pytest le charge
      seul, par son chemin (tests/test_assistant_tab5.py). Une entité de la tablette
      elle-même (modèle de son appareil), désactivée, cachée ou de catégorie
      config/diagnostic n'est jamais proposée.
"""
from __future__ import annotations

import os
import re
import shutil
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath
from typing import Any

import yaml

MAX_PIECES = 5
MAX_TUILES = 5
# Domaines proposés d'office comme tuiles, dans cet ordre (lumières d'abord). Le formulaire
# accepte tous ceux du blueprint (DOMAINES_TUILES), l'utilisateur en ajoute s'il veut.
DOMAINES_AUTO = ("light", "cover", "switch", "fan", "media_player")
# Les domaines du sélecteur « Appareils » d'une pièce du blueprint (&domaines_tuiles) ;
# tests/test_assistant_tab5.py les compare au blueprint.
DOMAINES_TUILES = ("light", "switch", "input_boolean", "fan", "humidifier", "automation", "cover",
                   "valve", "media_player", "scene", "script", "button", "input_button", "sensor",
                   "number", "input_number", "binary_sensor", "device_tracker", "person", "lock",
                   "climate")
# Ce qui rend une pièce intéressante pour l'écran : ses tuiles possibles et sa clim.
DOMAINES_PILOTABLES = (*DOMAINES_AUTO, "climate")

BLUEPRINT_NOM = "tab5_emplacements.yaml"
# Chemin du blueprint posé par l'intégration, relatif à blueprints/automation/.
BLUEPRINT_CHEMIN = "tab5/tab5_emplacements.yaml"
FICHIER = "automations.yaml"
# Sous-dossier des sauvegardes d'automations.yaml ; installation.derniere_sauvegarde et
# nettoyer_sauvegardes ne lisent que les dossiers datés, celui-ci leur échappe.
SAUVEGARDES = "tab5_sauvegardes/automatisations"
GARDER = 5
TEMPORAIRE = ".tab5-tmp"
# Les entrées d'une pièce dans le blueprint ; une mise à jour les remplace toutes.
CHAMPS_PIECE = ("nom", "tuiles", "temperature", "humidite", "clim")
ENTREE_PIECE = re.compile(r"^piece_[1-9]_(?:%s)$" % "|".join(CHAMPS_PIECE))
# `automation: !include automations.yaml` (ou `automation ui:` …) au premier niveau.
INCLUSION = re.compile(r"^automation(?:\s+[^:#\n]+)?:\s*!include\s+automations\.yaml\s*(?:#.*)?$", re.M)


# ─── Inventaire et propositions ──────────────────────────────────────────────

@dataclass(frozen=True)
class Zone:
    id: str
    nom: str


@dataclass(frozen=True)
class Appareil:
    id: str
    zone: str | None = None
    modele: str | None = None


@dataclass(frozen=True)
class Entite:
    entity_id: str
    nom: str = ""
    zone: str | None = None        # pièce de l'entité elle-même (prime sur celle de l'appareil)
    appareil: str | None = None
    classe: str | None = None      # device_class (celle de l'utilisateur, sinon d'origine)
    desactivee: bool = False
    cachee: bool = False
    categorie: str | None = None   # entity_category : config, diagnostic
    presente: bool = True          # un état dans HA (intégration chargée)

    @property
    def domaine(self) -> str:
        return self.entity_id.split(".", 1)[0]


@dataclass(frozen=True)
class Piece:
    """Une pièce telle qu'elle partira dans le blueprint."""
    zone: str | None = None
    nom: str = ""
    tuiles: tuple[str, ...] = ()
    temperature: str | None = None
    humidite: str | None = None
    clim: str | None = None


def zone_de(entite: Entite, appareils: dict[str, Appareil]) -> str | None:
    """Sa pièce à elle, sinon celle de son appareil."""
    if entite.zone:
        return entite.zone
    appareil = appareils.get(entite.appareil or "")
    return appareil.zone if appareil else None


def utilisable(entite: Entite, appareils: dict[str, Appareil], modele_tablette: str) -> bool:
    if entite.desactivee or entite.cachee or entite.categorie or not entite.presente:
        return False
    appareil = appareils.get(entite.appareil or "")
    return not (appareil and appareil.modele == modele_tablette)


def _cle_tri(e: Entite) -> tuple[str, str]:
    return ((e.nom or e.entity_id).casefold(), e.entity_id)


def par_zone(entites: list[Entite], appareils: dict[str, Appareil],
             modele_tablette: str) -> dict[str, list[Entite]]:
    """{pièce : ses entités utilisables, triées par nom}."""
    zones: dict[str, list[Entite]] = {}
    for e in entites:
        if (z := zone_de(e, appareils)) and utilisable(e, appareils, modele_tablette):
            zones.setdefault(z, []).append(e)
    return {z: sorted(liste, key=_cle_tri) for z, liste in zones.items()}


def _capteur(entites: list[Entite], classe: str) -> str | None:
    return next((e.entity_id for e in entites if e.domaine == "sensor" and e.classe == classe), None)


def proposer_piece(zone: Zone, entites: list[Entite]) -> Piece:
    """La proposition pour une pièce : son nom, ses tuiles dans l'ordre de DOMAINES_AUTO
    (par nom dans un domaine), le premier capteur de température et d'humidité, la
    première clim."""
    tuiles = [e.entity_id for d in DOMAINES_AUTO for e in entites if e.domaine == d][:MAX_TUILES]
    clim = next((e.entity_id for e in entites if e.domaine == "climate"), None)
    return Piece(zone=zone.id, nom=zone.nom, tuiles=tuple(tuiles),
                 temperature=_capteur(entites, "temperature"), humidite=_capteur(entites, "humidity"),
                 clim=clim)


def score(entites: list[Entite]) -> int:
    return sum(e.domaine in DOMAINES_PILOTABLES for e in entites)


def classer_zones(zones: list[Zone], parzone: dict[str, list[Entite]]) -> list[Zone]:
    """Les pièces qui ont quelque chose à montrer (une tuile, une clim ou une mesure), les
    plus pilotables d'abord, puis par nom."""
    def montre(z: Zone) -> bool:
        p = proposer_piece(z, parzone.get(z.id, []))
        return bool(p.tuiles or p.clim or p.temperature or p.humidite)
    candidates = [z for z in zones if montre(z)]
    return sorted(candidates, key=lambda z: (-score(parzone.get(z.id, [])), z.nom.casefold(), z.id))


# ─── Entrées du blueprint ────────────────────────────────────────────────────

def entrees_blueprint(pieces: list[Piece]) -> dict[str, Any]:
    """Les entrées piece_n_* (n = 1 à 5, dans l'ordre des pièces) ; un champ vide est omis,
    le blueprint lui donne sa valeur par défaut."""
    if len(pieces) > MAX_PIECES:
        raise ValueError(f"{len(pieces)} pièces, {MAX_PIECES} au plus")
    entrees: dict[str, Any] = {}
    for n, p in enumerate(pieces, 1):
        if len(p.tuiles) > MAX_TUILES:
            raise ValueError(f"pièce {n} : {len(p.tuiles)} tuiles, {MAX_TUILES} au plus")
        if p.nom.strip():
            entrees[f"piece_{n}_nom"] = p.nom.strip()
        if p.tuiles:
            entrees[f"piece_{n}_tuiles"] = list(p.tuiles)
        for champ in ("temperature", "humidite", "clim"):
            if valeur := getattr(p, champ):
                entrees[f"piece_{n}_{champ}"] = valeur
    return entrees


def fusionner(anciennes: dict[str, Any] | None, pieces: dict[str, Any]) -> dict[str, Any]:
    """Les entrées d'une automatisation existante, ses pièces remplacées par `pieces` ; tout
    le reste (météo, énergie, gestes…) gardé tel quel, dans son ordre."""
    gardees = {k: v for k, v in (anciennes or {}).items() if not ENTREE_PIECE.match(k)}
    return {**pieces, **gardees}


# ─── automations.yaml ────────────────────────────────────────────────────────

class FichierInutilisable(Exception):
    """automations.yaml ne peut pas être écrit sans risque : l'assistant donne le YAML."""


def inclut_automations(configuration: str) -> bool:
    """configuration.yaml charge-t-il automations.yaml (celui qu'écrit l'éditeur de HA) ?"""
    return bool(INCLUSION.search(configuration))


def lire(texte: str) -> list[dict]:
    """La liste des automatisations ; FichierInutilisable si ce n'en est pas une, ou si une
    étiquette de HA (!include, !secret…) empêche de la relire et de la réécrire telle quelle."""
    try:
        donnees = yaml.safe_load(texte) if texte.strip() else []
    except yaml.YAMLError as err:
        raise FichierInutilisable(f"{FICHIER} illisible ({str(err).splitlines()[0]})") from err
    if donnees is None:
        donnees = []
    if not isinstance(donnees, list) or not all(isinstance(a, dict) for a in donnees):
        raise FichierInutilisable(f"{FICHIER} n'est pas une liste d'automatisations")
    return donnees


def du_blueprint(automatisations: list[dict]) -> list[dict]:
    """Celles qui utilisent un blueprint tab5_emplacements.yaml (le nôtre ou une copie
    importée par son URL)."""
    resultat = []
    for a in automatisations:
        chemin = str(((a.get("use_blueprint") or {}).get("path")) or "")
        if PurePosixPath(chemin.replace("\\", "/")).name == BLUEPRINT_NOM:
            resultat.append(a)
    return resultat


def nouvel_id(automatisations: list[dict], millisecondes: int) -> str:
    """Un id comme ceux de l'éditeur de HA (horodatage en ms), jamais déjà pris."""
    pris = {str(a.get("id")) for a in automatisations}
    n = millisecondes
    while str(n) in pris:
        n += 1
    return str(n)


def automatisation(id_: str, entrees: dict[str, Any], langue: str | None) -> dict[str, Any]:
    fr = (langue or "").lower().startswith("fr")
    return {
        "id": id_,
        "alias": "Tab5 — emplacements de l'écran" if fr else "Tab5 — screen slots",
        "description": ("Créée par l'assistant de l'intégration Tab5." if fr
                        else "Created by the Tab5 integration's assistant."),
        "use_blueprint": {"path": BLUEPRINT_CHEMIN, "input": entrees},
    }


def vers_yaml(donnees: Any) -> str:
    """Comme l'éditeur de HA (homeassistant.util.yaml.dump) : blocs, accents gardés, ordre
    des clés gardé."""
    return yaml.safe_dump(donnees, default_flow_style=False, allow_unicode=True, sort_keys=False)


def _en_blocs(texte: str) -> bool:
    """La liste est-elle écrite en blocs (« - id: … ») ? Alors on ajoute à la fin, sans
    toucher au reste (commentaires compris)."""
    for ligne in texte.splitlines():
        s = ligne.strip()
        if s and not s.startswith("#"):
            return ligne.startswith("-")
    return False


def ajouter(texte: str, nouvelle: dict[str, Any]) -> str:
    """automations.yaml avec `nouvelle` à la fin. Le texte existant est gardé tel quel quand
    il est en blocs ; sinon (« [] », liste en ligne) il est réécrit comme le fait HA."""
    liste = lire(texte)
    if liste and _en_blocs(texte):
        resultat = texte.rstrip("\r\n") + "\n" + vers_yaml([nouvelle])
    elif not liste:
        commentaires = "".join(l + "\n" for l in texte.splitlines() if l.strip().startswith("#"))
        resultat = commentaires + vers_yaml([nouvelle])
    else:
        resultat = vers_yaml([*liste, nouvelle])
    if lire(resultat) != [*liste, nouvelle]:
        raise FichierInutilisable(f"{FICHIER} : la relecture ne donne pas le résultat attendu")
    return resultat


def remplacer_pieces(texte: str, index: int, pieces: dict[str, Any]) -> str:
    """automations.yaml où l'automatisation n° `index` de la liste (une du blueprint) a ses
    pièces remplacées (fusionner). Le fichier est réécrit comme le fait l'éditeur de HA :
    ses commentaires ne restent pas, la sauvegarde les garde."""
    liste = lire(texte)
    if not 0 <= index < len(liste) or not du_blueprint([liste[index]]):
        raise FichierInutilisable(f"{FICHIER} : l'automatisation du blueprint n'est plus à sa place")
    blueprint = liste[index]["use_blueprint"]
    blueprint["input"] = fusionner(blueprint.get("input"), pieces)
    resultat = vers_yaml(liste)
    if lire(resultat) != liste:
        raise FichierInutilisable(f"{FICHIER} : la relecture ne donne pas le résultat attendu")
    return resultat


def ecrire_si_inchange(config: Path, lu: str, nouveau: str, etiquette: str) -> Path | None:
    """Sauvegarde automations.yaml dans tab5_sauvegardes/automatisations/<étiquette>_automations.yaml
    (les GARDER plus récentes), puis l'écrit de façon atomique. FichierInutilisable s'il a
    changé depuis sa lecture `lu` (l'éditeur de HA, une autre session) : rien n'est écrit.
    Renvoie la sauvegarde (None si le fichier n'existait pas)."""
    cible = config / FICHIER
    actuel = cible.read_text(encoding="utf-8") if cible.is_file() else ""
    if actuel != lu:
        raise FichierInutilisable(f"{FICHIER} a changé pendant l'assistant")
    sauvegarde = None
    if cible.is_file():
        dossier = config / SAUVEGARDES
        dossier.mkdir(parents=True, exist_ok=True)
        sauvegarde = dossier / f"{etiquette}_{FICHIER}"
        n = 1
        while sauvegarde.exists():
            n += 1
            sauvegarde = dossier / f"{etiquette}-{n}_{FICHIER}"
        shutil.copy2(cible, sauvegarde)
        for ancienne in sorted(dossier.glob(f"*_{FICHIER}"))[:-GARDER]:
            ancienne.unlink()
    tmp = cible.with_name(cible.name + TEMPORAIRE)
    try:
        tmp.write_text(nouveau, encoding="utf-8", newline="\n")
        os.replace(tmp, cible)
    finally:
        tmp.unlink(missing_ok=True)
    return sauvegarde


# ─── Ce qui sera fait ────────────────────────────────────────────────────────

@dataclass
class Situation:
    """Où en est automations.yaml, et donc ce que l'assistant peut faire."""
    action: str                       # « creer », « mettre_a_jour », « ailleurs », « plusieurs », « fichier »
    texte: str = ""
    existante: dict | None = None     # celle à mettre à jour…
    index: int | None = None          # …et sa place dans la liste
    raison: str = ""                  # pour « fichier » : pourquoi pas d'écriture
    automatisations: list[dict] = field(default_factory=list)


def situation(config: Path, ids_dans_ha: set[str]) -> Situation:
    """`ids_dans_ha` : ids des automatisations que HA a chargées depuis un blueprint
    tab5_emplacements (n'importe quel fichier). Lecture seule."""
    conf = config / "configuration.yaml"
    configuration = conf.read_text(encoding="utf-8", errors="replace") if conf.is_file() else ""
    fichier = config / FICHIER
    texte = fichier.read_text(encoding="utf-8") if fichier.is_file() else ""
    try:
        liste = lire(texte)
    except FichierInutilisable as err:
        # Illisible, mais HA a chargé une automatisation du blueprint : pas de YAML à coller
        # (ce serait un doublon).
        return Situation("ailleurs" if ids_dans_ha else "fichier", texte=texte, raison=str(err))
    nos = du_blueprint(liste)
    ids_fichier = {str(a.get("id")) for a in nos}
    if ids_dans_ha - ids_fichier:
        return Situation("ailleurs", texte=texte, automatisations=liste)
    if len(nos) > 1:
        return Situation("plusieurs", texte=texte, automatisations=liste)
    if not inclut_automations(configuration):
        return Situation("fichier", texte=texte,
                         raison=f"configuration.yaml n'inclut pas {FICHIER} (`automation: !include {FICHIER}`)")
    if nos:
        index = next(i for i, a in enumerate(liste) if a is nos[0])
        return Situation("mettre_a_jour", texte=texte, existante=nos[0], index=index, automatisations=liste)
    return Situation("creer", texte=texte, automatisations=liste)


def resume(pieces: list[Piece], noms: dict[str, str], langue: str | None) -> str:
    """Le récapitulatif en Markdown, une ligne par pièce (noms des entités, pas leurs id)."""
    fr = (langue or "").lower().startswith("fr")
    vide = "—"

    def nom(eid: str | None) -> str:
        return noms.get(eid, eid) if eid else vide

    lignes = []
    for n, p in enumerate(pieces, 1):
        tuiles = ", ".join(nom(t) for t in p.tuiles) or vide
        titre = p.nom or (f"Pièce {n}" if fr else f"Room {n}")
        if fr:
            lignes.append(f"- **Pièce {n} · {titre}** : {tuiles} ; température : {nom(p.temperature)} ; "
                          f"humidité : {nom(p.humidite)} ; clim : {nom(p.clim)}")
        else:
            lignes.append(f"- **Room {n} · {titre}**: {tuiles}; temperature: {nom(p.temperature)}; "
                          f"humidity: {nom(p.humidite)}; climate: {nom(p.clim)}")
    return "\n".join(lignes)
