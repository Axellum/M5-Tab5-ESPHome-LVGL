# -*- coding: utf-8 -*-
"""Assistant de configuration : les pièces de Home Assistant → l'automatisation du blueprint.

[AI-CONTEXT]
@role Le cœur de l'assistant de l'intégration « Tab5 » (ADR-0052), sans Home Assistant :
      1. propose, d'après les registres de HA (pièces, appareils, entités), jusqu'à
         MAX_PIECES pièces et, pour chacune, ses tuiles (MAX_TUILES entités des domaines
         DOMAINES_AUTO, dans cet ordre), son capteur de température, d'humidité et sa clim
         (entrées piece_n_temperature / _humidite / _clim, ADR-0040) ;
      2. propose les entrées de la maison (celles qui ne dépendent pas d'une pièce,
         CHAMPS_MAISON) quand il n'y a pas d'ambiguïté, et les choix des listes
         « Tab5 · … » de packages/tab5_reglages.yaml (LISTES : les règles du package pour
         celles qu'il devine, une proposition plausible pour les quatre qu'il ne devine
         jamais) ;
      2 bis. construit les entrées du blueprint tab5_emplacements à partir des choix ;
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
    plateforme: str | None = None  # intégration qui la fournit (mobile_app, holiday…)
    interne: bool = False          # une entité des packages du Tab5 (est_interne) : jamais proposée

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


PREFIXE_PACKAGES = "tab5_"


def est_interne(plateforme: str | None, unique_id: str | None) -> bool:
    """Une entité de modèle des packages du Tab5 (leurs unique_id commencent tous par
    « tab5_ », tests/test_assistant_tab5.py) : un miroir comme binary_sensor.tab5_presence,
    qui suit la liste « capteur de présence » elle-même. La proposer ferait une boucle
    (vu par la CI le 10/10/2026)."""
    return plateforme == "template" and (unique_id or "").startswith(PREFIXE_PACKAGES)


def utilisable(entite: Entite, appareils: dict[str, Appareil], modele_tablette: str) -> bool:
    if entite.desactivee or entite.cachee or entite.categorie or not entite.presente or entite.interne:
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


# ─── Maison : les entrées qui ne dépendent pas d'une pièce ──────────────────

# (entrée du blueprint, nature) dans l'ordre du formulaire Maison. Nature : « domaine » ou
# « domaine:classe », « bool », « texte » ; tests/test_assistant_tab5.py les compare au
# blueprint. Les autres entrées (rangées, panneau Ok Nabu, gestes, énergie, tuile − / +,
# météo pluie / vigilances, ancien accueil) ne sont pas proposées : leur réponse n'est pas
# dans les registres de HA (inventaire dans l'ADR-0053).
CHAMPS_MAISON = (
    ("tv", "media_player"), ("tv_telecommande", "remote"), ("telephone", "sensor:battery"),
    ("salon_temperature", "sensor:temperature"), ("salon_humidite", "sensor:humidity"),
    ("serre_temperature", "sensor:temperature"), ("serre_exterieure", "bool"),
    ("clim", "climate"),
    ("pot_1", "sensor:moisture"), ("pot_2", "sensor:moisture"), ("pot_3", "sensor:moisture"),
    ("pot_4", "sensor:moisture"), ("pot_5", "sensor:moisture"),
    ("meteo_previsions", "weather"), ("tablette", "texte"),
)
TABLETTE_DEFAUT = "tab5_ha_hmi"
MAX_POTS = 5  # pot_1 à pot_5 ; plus de capteurs : lesquels montrer n'est pas évident
# Une température dehors, d'après son nom ou son entity_id (la seconde température).
DEHORS = re.compile(r"ext[eé]rieu?r|exterior|outdoor|outside|dehors|au(?:ss|ß)en|buiten|estern[oa]|dış", re.I)


def proposer_maison(entites: list[Entite], appareils: dict[str, Appareil], modele_tablette: str,
                    pieces: list[Piece], nom_esphome: str | None = None) -> dict[str, Any]:
    """Les entrées de la maison, seulement quand la réponse est sans ambiguïté (une seule
    candidate) : la TV (seul media_player de classe tv) et sa télécommande (seul remote de
    son appareil), la batterie du téléphone (seul capteur de batterie de l'application
    mobile), la température et l'humidité de la pièce 1 pour celles du salon, la seule
    température dont le nom dit « dehors » (seconde température, dehors), la seule clim,
    les pots (1 à 5 capteurs d'humidité du sol), la seule entité météo, le nom ESPHome de
    la tablette s'il n'est pas celui par défaut. Sinon rien : le blueprint garde son défaut."""
    utiles = sorted((e for e in entites if utilisable(e, appareils, modele_tablette)), key=_cle_tri)

    def de(domaine: str, classe: str | None = None) -> list[Entite]:
        return [e for e in utiles if e.domaine == domaine and (classe is None or e.classe == classe)]

    def seule(liste: list[Entite]) -> str | None:
        return liste[0].entity_id if len(liste) == 1 else None

    p: dict[str, Any] = {}
    tvs = de("media_player", "tv")
    if (tv := seule(tvs)) is not None:
        p["tv"] = tv
        if tvs[0].appareil:
            p["tv_telecommande"] = seule([e for e in de("remote") if e.appareil == tvs[0].appareil])
    p["telephone"] = seule([e for e in de("sensor", "battery") if e.plateforme == "mobile_app"])
    if pieces:
        p["salon_temperature"], p["salon_humidite"] = pieces[0].temperature, pieces[0].humidite
    dehors = [e for e in de("sensor", "temperature") if DEHORS.search(e.nom) or DEHORS.search(e.entity_id)]
    if (s := seule(dehors)) is not None:
        p["serre_temperature"], p["serre_exterieure"] = s, True
    p["clim"] = seule(de("climate"))
    pots = de("sensor", "moisture")
    if len(pots) <= MAX_POTS:
        p.update({f"pot_{n}": e.entity_id for n, e in enumerate(pots, 1)})
    p["meteo_previsions"] = seule(de("weather"))
    if nom_esphome:
        p["tablette"] = nom_esphome.strip().replace("-", "_")
    return entrees_maison(p)


def entrees_maison(valeurs: dict[str, Any], anciennes: dict[str, Any] | None = None) -> dict[str, Any]:
    """Les entrées de la maison à écrire : vides, décochées et nom de tablette par défaut
    omis (le blueprint a déjà ce défaut), dans l'ordre de CHAMPS_MAISON. Une case décochée
    alors que l'automatisation existante (`anciennes`) l'avait est gardée à False : sinon
    fusionner() reprendrait l'ancienne valeur. Un champ vidé, lui, garde l'ancienne valeur
    (vider n'efface rien ; on retire une entrée dans l'automatisation)."""
    anciennes = anciennes or {}
    entrees = {}
    for cle, nature in CHAMPS_MAISON:
        v = valeurs.get(cle)
        if isinstance(v, str):
            v = v.strip()
        if nature == "bool" and v is False and cle in anciennes:
            entrees[cle] = False
            continue
        if v in (None, "", [], False) or (cle == "tablette" and v == TABLETTE_DEFAUT):
            continue
        entrees[cle] = v
    return entrees


# ─── Listes « Tab5 · … » (packages/tab5_reglages.yaml) ──────────────────────

AUCUN = "Aucun"


@dataclass(frozen=True)
class Liste:
    cle: str        # champ du formulaire Agendas
    select: str     # entity_id du select du package (default_entity_id)
    devinee: bool   # le package la devine-t-il déjà quand elle n'est pas réglée ?


LISTES = (
    Liste("agenda_travail", "select.tab5_agenda_de_travail", False),
    Liste("agenda_rdv", "select.tab5_agenda_des_rendez_vous", False),
    Liste("agenda_anniversaires", "select.tab5_agenda_des_anniversaires", True),
    Liste("agenda_feries", "select.tab5_agenda_des_jours_feries", True),
    Liste("agenda_vacances", "select.tab5_agenda_des_vacances_scolaires", True),
    Liste("telephone_suivi", "select.tab5_telephone", True),
    Liste("presence", "select.tab5_capteur_de_presence", False),
    Liste("pipeline", "select.tab5_pipeline_de_discussion", False),
)
# Les règles du package (auto_anniversaires, auto_vacances, auto_feries : mêmes motifs,
# cherchés dans l'entity_id ; tests/test_assistant_tab5.py les compare), puis des mots
# plausibles pour les deux agendas qu'il ne devine pas (cherchés aussi dans le nom).
MOTS_PACKAGE = {
    "agenda_anniversaires": "anniversaire|birthday|geburtstag|verjaardag|cumplea|compleann",
    "agenda_vacances": ("vacances_scolaires|calendrier_scolaire|school|schulferien|schoolvakantie"
                        "|vacaciones_escolares|vacanze_scolastiche"),
    "agenda_feries": "ferie|holiday|feiertag|feestdag|festiv",
}
MOTS_PROPOSES = {
    "agenda_travail": re.compile(r"travail|boulot|work|job|shift|arbeit|dienst|werk|trabajo|lavoro|mesai", re.I),
    "agenda_rdv": re.compile(r"rendez|rdv|appointment|termin|afspra|cita|appuntament|randevu", re.I),
}
CLASSES_PRESENCE = ("occupancy", "presence")


@dataclass
class Infos:
    """Ce que les règles des listes lisent dans HA, en plus des options de chaque liste."""
    noms: dict[str, str] = field(default_factory=dict)
    plateformes: dict[str, str] = field(default_factory=dict)
    classes: dict[str, str] = field(default_factory=dict)
    pipeline_prefere: str | None = None
    internes: set[str] = field(default_factory=set)  # entités des packages du Tab5 : jamais proposées


def _calendriers(options: list[str]) -> list[str]:
    return [o for o in options if o.startswith("calendar.")]


def _auto(cle: str, options: list[str], infos: Infos) -> list[str]:
    """Les candidates des règles du package (auto_*), pour une liste qu'il devine."""
    agendas = _calendriers(options)
    if cle in ("agenda_anniversaires", "agenda_vacances"):
        return [a for a in agendas if re.search(MOTS_PACKAGE[cle], a)]
    if cle == "agenda_feries":
        vacances = _auto("agenda_vacances", options, infos)
        feries = [a for a in agendas if infos.plateformes.get(a) == "holiday"]
        feries += [a for a in agendas if re.search(MOTS_PACKAGE[cle], a) and a not in vacances]
        return list(dict.fromkeys(feries))
    if cle == "telephone_suivi":
        return [o for o in options if o.startswith("device_tracker.") and infos.plateformes.get(o) == "mobile_app"]
    return []


def _propose(cle: str, options: list[str], infos: Infos) -> list[str]:
    """Les candidates plausibles d'une liste que le package ne devine jamais."""
    if cle in MOTS_PROPOSES:
        deja = {a for c in MOTS_PACKAGE for a in _auto(c, options, infos)}
        autre = "agenda_rdv" if cle == "agenda_travail" else "agenda_travail"
        resultat = []
        for a in _calendriers(options):
            texte = f"{a} {infos.noms.get(a, '')}"
            if a not in deja and MOTS_PROPOSES[cle].search(texte) and not MOTS_PROPOSES[autre].search(texte):
                resultat.append(a)
        return resultat
    if cle == "presence":
        return [o for o in options if o.startswith("binary_sensor.") and infos.classes.get(o) in CLASSES_PRESENCE]
    if cle == "pipeline":
        return [infos.pipeline_prefere] if infos.pipeline_prefere in options else []
    return []


def proposer_liste(liste: Liste, options: list[str], actuel: str | None, infos: Infos) -> tuple[str, bool]:
    """(valeur proposée, est-ce une proposition de l'assistant ?). Un choix déjà fait reste ;
    sinon la seule candidate des règles ; sinon « Aucun »."""
    if actuel and actuel != AUCUN and actuel in options:
        return actuel, False
    candidates = [c for c in (_auto if liste.devinee else _propose)(liste.cle, options, infos)
                  if c in options and c not in infos.internes]
    if len(candidates) == 1:
        return candidates[0], True
    return (AUCUN if AUCUN in options else (actuel or AUCUN)), False


def changements(choix: dict[str, str], actuels: dict[str, str]) -> dict[str, str]:
    """Les listes à changer (select.select_option) : celles dont le choix diffère."""
    return {cle: v for cle, v in choix.items() if v and v != actuels.get(cle)}


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


def fusionner(anciennes: dict[str, Any] | None, nouvelles: dict[str, Any]) -> dict[str, Any]:
    """Les entrées d'une automatisation existante : ses pièces toutes remplacées par celles
    de `nouvelles`, ses autres entrées remplacées seulement par une valeur de `nouvelles`
    (maison) ; tout le reste (énergie, gestes…) gardé tel quel."""
    gardees = {k: v for k, v in (anciennes or {}).items() if not ENTREE_PIECE.match(k) and k not in nouvelles}
    return {**nouvelles, **gardees}


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
    t = TEXTES[langue_de(langue)]
    return {
        "id": id_,
        "alias": t["alias"],
        "description": t["description"],
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


# « AAAAMMJJ-HHMMSS » (l'étiquette, dt_util.now()), puis « -n » à partir de la 2e de la même seconde.
_SAUVEGARDE = re.compile(r"^(\d{8}-\d{6})(?:-(\d+))?_" + re.escape(FICHIER) + "$")


def sauvegardes(config: Path) -> list[Path]:
    """Les sauvegardes de l'assistant, de la plus ancienne à la plus récente. Deux dans la
    même seconde : « <étiquette>_… » puis « <étiquette>-2_… » ; l'ordre des noms seul les
    inverserait (« - » se trie avant « _ »)."""
    dossier = config / SAUVEGARDES
    if not dossier.is_dir():
        return []

    def ordre(p: Path) -> tuple[str, int]:
        m = _SAUVEGARDE.match(p.name)
        return (m.group(1), int(m.group(2) or 1)) if m else (p.name, 0)

    return sorted(dossier.glob(f"*_{FICHIER}"), key=ordre)


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
        # Le numéro suit le plus grand de la même étiquette (un nom libéré par la purge
        # n'est pas repris : il se trierait avant une sauvegarde plus ancienne).
        numeros = [int(m.group(2) or 1) for p in dossier.glob(f"*_{FICHIER}")
                   if (m := _SAUVEGARDE.match(p.name)) and m.group(1) == etiquette]
        n = max(numeros, default=0) + 1
        sauvegarde = dossier / (f"{etiquette}_{FICHIER}" if n == 1 else f"{etiquette}-{n}_{FICHIER}")
        shutil.copy2(cible, sauvegarde)
        for ancienne in sauvegardes(config)[:-GARDER]:
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


# ─── Textes (récapitulatif, nom de l'automatisation) ─────────────────────────
# Sept langues, comme translations/*.json et l'écran ; même liste et même règle que
# messages.LANGUES / messages.langue_de (les deux modules sont purs : test d'égalité).
LANGUES = ("fr", "en", "de", "nl", "es", "it", "tr")


def langue_de(langue: str | None) -> str:
    """« fr », « fr-FR », « de_CH »… → le code d'une des LANGUES ; sinon « en »."""
    code = (langue or "").lower().replace("_", "-").split("-")[0]
    return code if code in LANGUES else "en"


# Mots du récapitulatif. « deux_points » et « sep » : la typographie de chaque langue.
TEXTES: dict[str, dict[str, str]] = {
    "fr": {"alias": "Tab5 — emplacements de l'écran", "description": "Créée par l'assistant de l'intégration Tab5.",
           "piece": "Pièce", "temperature": "température", "humidite": "humidité", "clim": "clim",
           "maison": "Maison", "rien": "rien de proposé", "listes": "Listes « Tab5 · … »",
           "aucun_changement": "aucun changement", "propose": "proposé", "oui": "oui", "aucun": "Aucun",
           "deux_points": " : ", "sep": " ; "},
    "en": {"alias": "Tab5 — screen slots", "description": "Created by the Tab5 integration's assistant.",
           "piece": "Room", "temperature": "temperature", "humidite": "humidity", "clim": "climate",
           "maison": "Home", "rien": "nothing proposed", "listes": "« Tab5 · … » lists",
           "aucun_changement": "no change", "propose": "suggested", "oui": "yes", "aucun": "None",
           "deux_points": ": ", "sep": "; "},
    "de": {"alias": "Tab5 — Bildschirmplätze", "description": "Vom Assistenten der Tab5-Integration erstellt.",
           "piece": "Raum", "temperature": "Temperatur", "humidite": "Feuchte", "clim": "Klima",
           "maison": "Zuhause", "rien": "nichts vorgeschlagen", "listes": "Listen „Tab5 · …“",
           "aucun_changement": "keine Änderung", "propose": "vorgeschlagen", "oui": "ja", "aucun": "Keine",
           "deux_points": ": ", "sep": "; "},
    "nl": {"alias": "Tab5 — schermplaatsen", "description": "Gemaakt door de assistent van de Tab5-integratie.",
           "piece": "Kamer", "temperature": "temperatuur", "humidite": "vochtigheid", "clim": "airco",
           "maison": "Huis", "rien": "niets voorgesteld", "listes": "Lijsten “Tab5 · …”",
           "aucun_changement": "geen wijziging", "propose": "voorgesteld", "oui": "ja", "aucun": "Geen",
           "deux_points": ": ", "sep": "; "},
    "es": {"alias": "Tab5 — posiciones de la pantalla", "description": "Creada por el asistente de la integración Tab5.",
           "piece": "Habitación", "temperature": "temperatura", "humidite": "humedad", "clim": "climatización",
           "maison": "Casa", "rien": "nada propuesto", "listes": "Listas «Tab5 · …»",
           "aucun_changement": "sin cambios", "propose": "propuesto", "oui": "sí", "aucun": "Ninguno",
           "deux_points": ": ", "sep": "; "},
    "it": {"alias": "Tab5 — posizioni dello schermo", "description": "Creata dall'assistente dell'integrazione Tab5.",
           "piece": "Stanza", "temperature": "temperatura", "humidite": "umidità", "clim": "clima",
           "maison": "Casa", "rien": "niente di proposto", "listes": "Liste «Tab5 · …»",
           "aucun_changement": "nessuna modifica", "propose": "proposto", "oui": "sì", "aucun": "Nessuno",
           "deux_points": ": ", "sep": "; "},
    "tr": {"alias": "Tab5 — ekran yerleşimi", "description": "Tab5 entegrasyonunun asistanı tarafından oluşturuldu.",
           "piece": "Oda", "temperature": "sıcaklık", "humidite": "nem", "clim": "klima",
           "maison": "Ev", "rien": "öneri yok", "listes": "“Tab5 · …” listeleri",
           "aucun_changement": "değişiklik yok", "propose": "önerildi", "oui": "evet", "aucun": "Yok",
           "deux_points": ": ", "sep": "; "},
}

# Libellés courts du récapitulatif, dans l'ordre de LANGUES.
_LIBELLES = {
    "tv": ("TV", "TV", "TV", "tv", "TV", "TV", "TV"),
    "tv_telecommande": ("télécommande", "remote", "Fernbedienung", "afstandsbediening", "mando a distancia",
                        "telecomando", "uzaktan kumanda"),
    "telephone": ("batterie du téléphone", "phone battery", "Handy-Akku", "telefoonbatterij",
                  "batería del teléfono", "batteria del telefono", "telefon pili"),
    "salon_temperature": ("température du salon", "room temperature", "Raumtemperatur", "kamertemperatuur",
                          "temperatura de la habitación", "temperatura della stanza", "oda sıcaklığı"),
    "salon_humidite": ("humidité du salon", "room humidity", "Raumfeuchte", "luchtvochtigheid van de kamer",
                       "humedad de la habitación", "umidità della stanza", "oda nemi"),
    "serre_temperature": ("seconde température", "second temperature", "zweite Temperatur", "tweede temperatuur",
                          "segunda temperatura", "seconda temperatura", "ikinci sıcaklık"),
    "serre_exterieure": ("dehors", "outdoors", "draußen", "buiten", "exterior", "all'esterno", "dış mekân"),
    "clim": ("clim", "climate", "Klimaanlage", "airco", "climatización", "climatizzatore", "klima"),
    "meteo_previsions": ("météo", "weather", "Wetter", "weer", "tiempo", "meteo", "hava durumu"),
    "tablette": ("nom ESPHome", "ESPHome name", "ESPHome-Name", "ESPHome-naam", "nombre ESPHome",
                 "nome ESPHome", "ESPHome adı"),
    "agenda_travail": ("agenda de travail", "work calendar", "Arbeitskalender", "werkagenda",
                       "calendario de trabajo", "calendario di lavoro", "iş takvimi"),
    "agenda_rdv": ("agenda des rendez-vous", "appointments calendar", "Terminkalender", "afsprakenagenda",
                   "calendario de citas", "calendario degli appuntamenti", "randevu takvimi"),
    "agenda_anniversaires": ("agenda des anniversaires", "birthdays calendar", "Geburtstagskalender",
                             "verjaardagsagenda", "calendario de cumpleaños", "calendario dei compleanni",
                             "doğum günü takvimi"),
    "agenda_feries": ("agenda des jours fériés", "public holidays calendar", "Feiertagskalender",
                      "feestdagenagenda", "calendario de festivos", "calendario dei giorni festivi",
                      "resmî tatil takvimi"),
    "agenda_vacances": ("agenda des vacances scolaires", "school holidays calendar", "Schulferienkalender",
                        "schoolvakantieagenda", "calendario de vacaciones escolares",
                        "calendario delle vacanze scolastiche", "okul tatili takvimi"),
    "telephone_suivi": ("téléphone", "phone", "Handy", "telefoon", "teléfono", "telefono", "telefon"),
    "presence": ("capteur de présence", "presence sensor", "Anwesenheitssensor", "aanwezigheidssensor",
                 "sensor de presencia", "sensore di presenza", "varlık sensörü"),
    "pipeline": ("pipeline de discussion", "chat pipeline", "Chat-Pipeline", "chatpipeline",
                 "pipeline de conversación", "pipeline di conversazione", "sohbet pipeline'ı"),
}
_POT = ("pot", "pot", "Topf", "pot", "maceta", "vaso", "saksı")
_LIBELLES.update({f"pot_{n}": tuple(f"{mot} {n}" for mot in _POT) for n in range(1, MAX_POTS + 1)})
LIBELLES: dict[str, dict[str, str]] = {cle: dict(zip(LANGUES, mots, strict=True)) for cle, mots in _LIBELLES.items()}


def libelle(cle: str, langue: str | None) -> str:
    return LIBELLES.get(cle, {}).get(langue_de(langue), cle)


def resume_maison(maison: dict[str, Any], noms: dict[str, str], langue: str | None) -> str:
    t = TEXTES[langue_de(langue)]
    if not maison:
        return f"- **{t['maison']}**{t['deux_points']}{t['rien']}"
    parties = []
    for cle, valeur in maison.items():
        texte = t["oui"] if valeur is True else noms.get(valeur, valeur)
        parties.append(f"{libelle(cle, langue)}{t['deux_points']}{texte}")
    return f"- **{t['maison']}**{t['deux_points']}" + t["sep"].join(parties)


def resume_listes(changees: dict[str, str], proposees: set[str], noms: dict[str, str],
                  langue: str | None) -> str:
    t = TEXTES[langue_de(langue)]
    if not changees:
        return f"- **{t['listes']}**{t['deux_points']}{t['aucun_changement']}"
    parties = []
    for cle, valeur in changees.items():
        marque = f" ({t['propose']})" if cle in proposees else ""
        parties.append(f"{libelle(cle, langue)} → {noms.get(valeur, valeur)}{marque}")
    return f"- **{t['listes']}**{t['deux_points']}" + t["sep"].join(parties)


def resume(pieces: list[Piece], noms: dict[str, str], langue: str | None) -> str:
    """Le récapitulatif en Markdown, une ligne par pièce (noms des entités, pas leurs id)."""
    t = TEXTES[langue_de(langue)]
    d, s = t["deux_points"], t["sep"]
    vide = "—"

    def nom(eid: str | None) -> str:
        return noms.get(eid, eid) if eid else vide

    lignes = []
    for n, p in enumerate(pieces, 1):
        tuiles = ", ".join(nom(t_) for t_ in p.tuiles) or vide
        titre = p.nom or f"{t['piece']} {n}"
        lignes.append(f"- **{t['piece']} {n} · {titre}**{d}{tuiles}{s}{t['temperature']}{d}{nom(p.temperature)}{s}"
                      f"{t['humidite']}{d}{nom(p.humidite)}{s}{t['clim']}{d}{nom(p.clim)}")
    return "\n".join(lignes)
