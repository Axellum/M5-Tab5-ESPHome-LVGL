#!/usr/bin/env python3
"""tools/contrat_api.py — contrat Home Assistant ↔ firmware : instantané, semver, matrice N-1.

[AI-CONTEXT] Phase 2 du contrat (audit « niveau pro » du 30/09/2026, lot E). La phase 1
(tests/test_contrat.py) compare, dans le dépôt courant, chaque appel de HA aux variables
du firmware et chaque champ d'événement lu à ceux émis. Ce module lit les deux moitiés du
contrat à N'IMPORTE QUELLE révision git (tag publié compris), ce qui permet :

1. l'instantané `contrat/contrat.yaml` : actions de la tablette (variables et types) et
   événements `esphome.tab5_*` émis (champs), plus une `version:` semver tenue à la main ;
   `--check` échoue s'il ne correspond plus au code, `--write` le régénère (version gardée) ;
2. le semver contre le dernier tag publié (`--semver`) : ce qui a changé depuis ce tag
   exige une version de contrat plus haute (règle ci-dessous) ;
3. la matrice N-1 (`--matrice`) : firmware d'une version × fichiers HA de l'autre, dans
   les deux sens, d'où l'ordre de mise à jour, en tableau Markdown pour la note de release.

Règle de version (le firmware est le fournisseur, les fichiers HA le client) :
- MAJEURE : un fichier HA d'avant peut casser avec le nouveau firmware — action retirée ;
  variable ajoutée (toutes obligatoires : HA refuse l'appel sans elle), retirée (HA refuse
  la clé en trop) ou de type changé ; événement plus émis ; champ d'événement plus émis ;
- MINEURE : ajout pur — nouvelle action, nouvel événement, nouveau champ d'événement ;
- CORRECTIF : libre, rien dans l'instantané ne l'exige.
Un tag publié sans `contrat/contrat.yaml` (tous ceux d'avant ce fichier, jusqu'à la
3.8.0-rc.3) vaut VERSION_SANS_INSTANTANE quand on compare au DERNIER tag ; son contenu est
lu dans son code. La version 1.0.0 décrit donc le contrat de la 3.8.0-rc.3.

Matrice : un appel de HA ne compte que s'il peut partir avec ce firmware — pas s'il ne
part que sur un événement `esphome.tab5_*` que ce firmware n'émet pas (déclencheur, branche
de ce déclencheur, script lancé seulement de là), ni sous `if: "{{ protocole == 2 … }}"` du
blueprint avec un firmware plus ancien que son seuil. Une autre garde à l'exécution (par
exemple « l'écran courant vaut Alertes », que seul un firmware récent rapporte) n'est pas
vue : la matrice dit alors « cassé » par prudence (cas réel : 3.6.0 × fichiers 3.7.0, appel
de tab5_maj_alertes_historique). Le contenu des payloads n'est pas lu.

Lecture : PyYAML sur le texte des fichiers (copie de travail, ou `git cat-file --batch`
pour une révision). Sans PyYAML ni git, rien ne marche : outil de développement et de CI.
Les balises ESPHome et HA (!lambda, !include, !input…) valent None.

    python tools/contrat_api.py --check              # instantané = code ? (exit 1 sinon)
    python tools/contrat_api.py --write              # régénère l'instantané, garde la version
    python tools/contrat_api.py --semver             # version suffisante contre le dernier tag ?
    python tools/contrat_api.py --matrice            # copie de travail × dernier tag stable
    python tools/contrat_api.py --matrice --depuis v3.7.0 --courant v3.8.0-rc.3 [--en]

Les étiquettes « **Contrat HA ↔ firmware** : … (depuis vX.Y.Z) » du CHANGELOG sont
vérifiées contre la matrice par tests/test_contrat_versions.py ; la phrase exacte vient
de `etiquette()` (sortie de --matrice)."""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass, field
from functools import lru_cache
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
INSTANTANE = "contrat/contrat.yaml"
VERSION_SANS_INSTANTANE = "1.0.0"
TAG = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-rc\.(\d+))?$")
SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")

# « esphome.<appareil>_<action> » : `{{ tablette }}` dans le blueprint, le nom de
# l'appareil (tab5_ha_hmi) dans les packages. Les actions de la tablette commencent
# toutes par tab5_maj_ ou tab5_assist_.
APPEL = re.compile(r"esphome\.(?:\{\{\s*\w+\s*\}\}|[a-z0-9_]+?)_(tab5_(?:maj|assist)_\w+)")
LECTURE = re.compile(r"trigger\.event\.data(?:\.(\w+)|\[\s*['\"](\w+)['\"]\s*\])")
ID_UNIQUE = re.compile(r"\{\{\s*trigger\.id\s*==\s*['\"](\w+)['\"]\s*\}\}")
ID_LISTE = re.compile(r"\{\{\s*trigger\.id\s+in\s+\[([^\]]*)\]\s*\}\}")
# Ajouté par l'intégration ESPHome de HA à chaque événement d'un appareil.
CHAMPS_DE_HA = frozenset({"device_id"})


class ErreurContrat(AssertionError):
    """Forme inattendue dans un fichier : le contrat ne peut pas être lu sans deviner."""


# ─── Lecture des fichiers ────────────────────────────────────────────────────

_Base = getattr(yaml, "CSafeLoader", yaml.SafeLoader)


class Chargeur(_Base):
    """Balises ESPHome et HA à None ; `<<: !include …` sauté (le fichier inclus n'est pas
    lu, et PyYAML refuse une fusion qui n'est pas un dict)."""

    def flatten_mapping(self, noeud):
        noeud.value = [(k, v) for k, v in noeud.value
                       if not (k.tag == "tag:yaml.org,2002:merge" and isinstance(v, yaml.ScalarNode))]
        super().flatten_mapping(noeud)


Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def charger(texte: str):
    return yaml.load(texte, Loader=Chargeur)


def _git(args, racine=REPO, entree: bytes | None = None) -> bytes:
    return subprocess.run(["git", *args], cwd=racine, input=entree, capture_output=True, check=True).stdout


class Arbre:
    """Les fichiers du dépôt à une révision git (`ref`), ou la copie de travail (ref None,
    sans les fichiers que .gitignore écarte : le même jeu en local qu'en CI)."""

    def __init__(self, ref: str | None = None, racine: Path = REPO):
        self.ref, self.racine = ref, racine
        self._textes: dict[str, str] = {}
        if ref is None:
            try:
                sortie = _git(["ls-files", "-z", "--cached", "--others", "--exclude-standard"], racine)
                noms = [p for p in sortie.decode("utf-8").split("\0") if p]
            except (OSError, subprocess.CalledProcessError):
                noms = [p.relative_to(racine).as_posix() for p in racine.rglob("*.yaml")]
            self.chemins = sorted(p for p in noms if (racine / p).is_file())
        else:
            sortie = _git(["ls-tree", "-r", "-z", "--name-only", ref], racine)
            self.chemins = sorted(p for p in sortie.decode("utf-8").split("\0") if p)

    @property
    def nom(self) -> str:
        return self.ref or "copie de travail"

    def precharger(self, chemins):
        """Lit d'un coup (un seul `git cat-file --batch`) les fichiers d'une révision."""
        manquants = [c for c in chemins if c not in self._textes]
        if self.ref is None or not manquants:
            return
        sortie = _git(["cat-file", "--batch"], self.racine,
                      "".join(f"{self.ref}:{c}\n" for c in manquants).encode("utf-8"))
        pos = 0
        for chemin in manquants:
            fin = sortie.index(b"\n", pos)
            entete = sortie[pos:fin].split()
            taille = int(entete[2])
            self._textes[chemin] = sortie[fin + 1:fin + 1 + taille].decode("utf-8")
            pos = fin + 1 + taille + 1

    def lire(self, chemin: str) -> str:
        if chemin not in self._textes:
            if self.ref is None:
                self._textes[chemin] = (self.racine / chemin).read_text(encoding="utf-8")
            else:
                self.precharger([chemin])
        return self._textes[chemin]

    def existe(self, chemin: str) -> bool:
        return chemin in self.chemins


@lru_cache(maxsize=None)
def arbre(ref: str | None = None) -> Arbre:
    """Un Arbre par révision, gardé pour le processus (pytest le relit souvent)."""
    return Arbre(ref)


def fichiers_firmware(a: Arbre) -> list[str]:
    """YAML du firmware : tab5-*.yaml de la racine et Tab5/ (traductions exclues), quelle
    que soit la disposition des dossiers à cette révision."""
    return [c for c in a.chemins if c.endswith(".yaml") and (
        (c.startswith("tab5") and "/" not in c)
        or (c.startswith("Tab5/") and not c.startswith("Tab5/lang/") and not c.endswith("user_entities.yaml")))]


def fichiers_ha(a: Arbre) -> list[str]:
    return [c for c in a.chemins if c.startswith("HomeAssistant_Config/") and c.endswith(".yaml")
            and not c.endswith("placeholders.example.yaml")]


def parcourir(noeud):
    """Chaque dict du document, en profondeur."""
    if isinstance(noeud, dict):
        yield noeud
        for v in noeud.values():
            yield from parcourir(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from parcourir(v)


# ─── Le contrat du firmware ──────────────────────────────────────────────────

def actions_du_texte(texte: str) -> dict[str, dict[str, str]]:
    """{action: {variable: type}} d'un bloc `api: services:`, dans l'ordre du fichier.
    Forme longue (`type:` + `description:`) ou courte (`payload: string`)."""
    api = (charger(texte) or {}).get("api") or {}
    actions = {}
    for s in api.get("services") or []:
        variables = s.get("variables") or {}
        actions[s["service"]] = {v: (t.get("type") if isinstance(t, dict) else t) for v, t in variables.items()}
    return actions


def fichier_api(a: Arbre) -> str:
    (chemin,) = [c for c in fichiers_firmware(a) if c.endswith("/tab5-api-logic.yaml")]
    return chemin


def champs_emis(a: Arbre) -> dict[str, frozenset]:
    """{événement: champs} des `homeassistant.event` du firmware. Un même événement émis
    à plusieurs endroits porte partout les mêmes champs."""
    fichiers = fichiers_firmware(a)
    a.precharger(fichiers)
    emis, ou = {}, {}
    for chemin in fichiers:
        texte = a.lire(chemin)
        if "homeassistant.event" not in texte:
            continue
        for d in parcourir(charger(texte)):
            evt = d.get("homeassistant.event")
            if not isinstance(evt, dict):
                continue
            nom = evt["event"]
            if "variables" in evt:
                raise ErreurContrat(f"{chemin} : {nom}, champs en `variables:` non lus ici")
            champs = frozenset(evt.get("data") or {}) | frozenset(evt.get("data_template") or {})
            if nom in emis and emis[nom] != champs:
                raise ErreurContrat(f"{nom} : {sorted(champs)} dans {chemin}, {sorted(emis[nom])} dans {ou[nom]}")
            emis[nom], ou[nom] = champs, chemin
    return emis


@dataclass(frozen=True)
class Contrat:
    """Ce que le firmware accepte (actions) et ce qu'il émet (événements)."""
    services: dict = field(default_factory=dict)      # {action: {variable: type}}
    evenements: dict = field(default_factory=dict)    # {événement: frozenset(champs)}


@lru_cache(maxsize=None)
def contrat_firmware(ref: str | None = None) -> Contrat:
    a = arbre(ref)
    return Contrat(actions_du_texte(a.lire(fichier_api(a))), champs_emis(a))


# ─── Les appelants : fichiers Home Assistant ─────────────────────────────────

@dataclass(frozen=True)
class Lieu:
    """Où une action est appelée, pour savoir si elle peut s'exécuter avec un firmware :
    - `declencheurs` : dans une automatisation, les déclencheurs possibles à cet endroit
      (restreints comme dans `_lectures`) ; un événement `esphome.tab5_*` par son nom,
      tout autre déclencheur par "" ; None hors automatisation ;
    - `script` : id du script de package englobant (hors automatisation), sinon None ;
    - `protocole` : sous un `if: "{{ protocole == 2 … }}"` du blueprint (firmware ≥ seuil)."""
    declencheurs: frozenset | None = None
    script: str | None = None
    protocole: bool = False


@dataclass(frozen=True)
class Appel:
    fichier: str
    action: str           # tab5_maj_… pour une action de la tablette, script.<id> pour un script
    cles: tuple           # clés de `data:`
    lieux: tuple          # Lieu de chaque passage (une ancre YAML reprise en compte plusieurs)


# `if: "{{ protocole == 2 }}"` ou `"{{ protocole == 2 and … }}"` : garde de version du
# blueprint (ADR-0023). Un `or` de premier niveau n'est pas une garde.
GARDE_PROTOCOLE = re.compile(r"^\{\{-?\s*protocole\s*==\s*2\s*(?:\}\}|and\b(?!.*\bor\b))", re.S)
SEUIL_PROTOCOLE = re.compile(r"set p = 2 if .*?>=\s*(\d+)\s+else 1")


def _garde_protocole(garde) -> bool:
    if isinstance(garde, str):
        return bool(GARDE_PROTOCOLE.match(garde.strip()))
    if isinstance(garde, list):
        return any(_garde_protocole(g) for g in garde)
    if isinstance(garde, dict) and garde.get("condition") == "template":
        return _garde_protocole(garde.get("value_template"))
    return False


def _marcher(noeud, possibles, evenements, script, protocole, lieux):
    """Pose le Lieu de chaque dict sous `noeud` (clé : id de l'objet Python)."""
    if isinstance(noeud, list):
        for v in noeud:
            _marcher(v, possibles, evenements, script, protocole, lieux)
    elif isinstance(noeud, dict):
        decl = None if possibles is None else frozenset(evenements.get(k, "") for k in possibles)
        lieux.setdefault(id(noeud), []).append(Lieu(decl, script, protocole))
        garde = noeud.get("conditions", noeud.get("if"))
        ids = _ids_restreints(garde) if garde is not None else None
        dedans = possibles if ids is None or possibles is None else possibles & ids
        garde_proto = protocole or _garde_protocole(garde)
        for cle, v in noeud.items():
            if cle == "else":
                _marcher(v, possibles, evenements, script, protocole, lieux)
            else:
                _marcher(v, dedans, evenements, script, garde_proto, lieux)


def _lieux_du_document(doc) -> dict[int, list[Lieu]]:
    lieux = {}
    for auto, declencheurs in _automatisations(doc):
        evenements = {}
        for i, t in enumerate(declencheurs):
            evt = t.get("event_type") if (t.get("trigger") or t.get("platform")) == "event" else None
            evenements[_cle(t, i)] = evt if isinstance(evt, str) and evt.startswith("esphome.tab5_") else ""
        _marcher({k: v for k, v in auto.items() if k not in ("triggers", "trigger")},
                 frozenset(evenements), evenements, None, False, lieux)
    if isinstance(doc, dict) and isinstance(doc.get("script"), dict):
        for sid, config in doc["script"].items():
            _marcher(config, None, None, f"script.{sid}", False, lieux)
    return lieux


def _scripts_appeles(d) -> list[str]:
    """Scripts lancés par cette action : `action: script.x`, ou `script.turn_on` /
    `script.toggle` avec `entity_id` (direct, sous `target:` ou sous `data:`)."""
    nom = d.get("action", d.get("service"))
    if not isinstance(nom, str) or not nom.startswith("script."):
        return []
    if nom not in ("script.turn_on", "script.toggle", "script.reload", "script.turn_off"):
        return [nom]
    if nom in ("script.reload", "script.turn_off"):
        return []
    cibles = []
    for bloc in (d, d.get("target") or {}, d.get("data") or {}):
        if isinstance(bloc, dict):
            e = bloc.get("entity_id")
            cibles += e if isinstance(e, list) else [e]
    return [c for c in cibles if isinstance(c, str) and c.startswith("script.")]


def appels_detail(a: Arbre) -> list[Appel]:
    """Chaque appel d'une action de la tablette (`esphome.…_tab5_…`) et chaque lancement
    d'un script, avec ses Lieux, dans les fichiers HA de l'arbre."""
    appels = []
    fichiers = fichiers_ha(a)
    a.precharger(fichiers)
    for chemin in fichiers:
        doc = charger(a.lire(chemin))
        lieux = _lieux_du_document(doc)
        for d in parcourir(doc):
            ici = tuple(lieux.get(id(d), [Lieu()]))
            for script in _scripts_appeles(d):
                appels.append(Appel(chemin, script, (), ici))
            nom = d.get("action", d.get("service"))
            if not (isinstance(nom, str) and nom.startswith("esphome.")):
                continue
            m = APPEL.fullmatch(nom)
            if not m:
                raise ErreurContrat(f"{chemin} : « {nom} », forme d'appel inconnue de ce test")
            data = d.get("data") or {}
            if not isinstance(data, dict):
                raise ErreurContrat(f"{chemin} : {nom}, `data:` doit être un dictionnaire")
            appels.append(Appel(chemin, m.group(1), tuple(data), ici))
    return appels


def appels_ha(a: Arbre) -> list[tuple[str, str, tuple]]:
    """[(fichier, action, clés de data)] de chaque appel d'une action de la tablette."""
    return [(x.fichier, x.action, x.cles) for x in appels_detail(a) if not x.action.startswith("script.")]


def seuil_protocole(a: Arbre) -> int | None:
    """Version (X·10⁶ + Y·10³ + Z) dès laquelle le blueprint passe au protocole 2."""
    for chemin in fichiers_ha(a):
        if m := SEUIL_PROTOCOLE.search(a.lire(chemin)):
            return int(m.group(1))
    return None


def _cle(declencheur, rang):
    """Un déclencheur sans id a pour id son rang (comme dans HA)."""
    return str(declencheur.get("id", rang))


def _ids_restreints(conditions):
    """Ids des déclencheurs auxquels ces conditions (liées par ET) limitent l'exécution,
    ou None si elles ne disent rien du déclencheur."""
    if isinstance(conditions, str):
        if m := ID_UNIQUE.fullmatch(conditions.strip()):
            return {m.group(1)}
        if m := ID_LISTE.fullmatch(conditions.strip()):
            return set(re.findall(r"['\"](\w+)['\"]", m.group(1)))
        return None
    if isinstance(conditions, dict):
        if conditions.get("condition") == "trigger":
            ids = conditions["id"]
            return {str(i) for i in ids} if isinstance(ids, list) else {str(ids)}
        if conditions.get("condition") == "template":
            return _ids_restreints(conditions.get("value_template"))
        return None
    if isinstance(conditions, list):
        restreints = None
        for c in conditions:
            if (ids := _ids_restreints(c)) is not None:
                restreints = ids if restreints is None else restreints & ids
        return restreints
    return None


def _lectures(noeud, possibles, sortie):
    """(champ, ids des déclencheurs possibles) de chaque lecture de trigger.event.data."""
    if isinstance(noeud, str):
        for a, b in LECTURE.findall(noeud):
            sortie.append((a or b, possibles))
    elif isinstance(noeud, list):
        for v in noeud:
            _lectures(v, possibles, sortie)
    elif isinstance(noeud, dict):
        # Option d'un choose (conditions + sequence) ou if/then : la branche ne s'exécute
        # que pour les déclencheurs admis ; le else, pour les autres.
        garde = noeud.get("conditions", noeud.get("if"))
        ids = _ids_restreints(garde) if garde is not None else None
        dedans = possibles if ids is None else possibles & ids
        for cle, v in noeud.items():
            _lectures(v, possibles if cle == "else" else dedans, sortie)


def _automatisations(noeud):
    """Chaque dict qui a une liste de déclencheurs : automatisation, blueprint, entité de
    modèle à déclencheurs."""
    if isinstance(noeud, dict):
        declencheurs = noeud.get("triggers", noeud.get("trigger"))
        if isinstance(declencheurs, list):
            yield noeud, declencheurs
            return
        for v in noeud.values():
            yield from _automatisations(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _automatisations(v)


def champs_lus(a: Arbre) -> dict[str, dict[str, list[str]]]:
    """{événement esphome.tab5_*: {champ: [fichiers]}} lus par les fichiers de HA. La
    lecture est attribuée aux déclencheurs possibles à cet endroit : ceux de
    l'automatisation, restreints par une condition `trigger` ou un modèle
    `{{ trigger.id == '…' }}` / `{{ trigger.id in [...] }}` englobant."""
    lus = {}
    fichiers = fichiers_ha(a)
    a.precharger(fichiers)
    for chemin in fichiers:
        for auto, declencheurs in _automatisations(charger(a.lire(chemin))):
            evenements = {_cle(t, i): t.get("event_type") for i, t in enumerate(declencheurs)
                          if (t.get("trigger") or t.get("platform")) == "event"}
            sortie = []
            _lectures({k: v for k, v in auto.items() if k not in ("triggers", "trigger")},
                      {_cle(t, i) for i, t in enumerate(declencheurs)}, sortie)
            for champ, possibles in sortie:
                for cle in possibles:
                    evt = evenements.get(cle)
                    if isinstance(evt, str) and evt.startswith("esphome.tab5_"):
                        lus.setdefault(evt, {}).setdefault(champ, []).append(chemin)
    return lus


def evenements_ecoutes(a: Arbre) -> dict[str, list[str]]:
    """{événement esphome.tab5_*: [fichiers]} des déclencheurs des fichiers de HA."""
    ecoutes = {}
    for chemin in fichiers_ha(a):
        for d in parcourir(charger(a.lire(chemin))):
            evt = d.get("event_type")
            if isinstance(evt, str) and evt.startswith("esphome.tab5_"):
                ecoutes.setdefault(evt, []).append(chemin)
    return ecoutes


@dataclass(frozen=True)
class Appelants:
    """Ce que les fichiers HA d'une révision appellent et lisent."""
    appels: list          # Appel des actions de la tablette
    scripts: dict         # {script.<id>: [Appel qui le lance]}
    lus: dict
    ecoutes: dict
    seuil_protocole: int | None


@lru_cache(maxsize=None)
def appelants_ha(ref: str | None = None) -> Appelants:
    a = arbre(ref)
    detail = appels_detail(a)
    scripts = {}
    for x in detail:
        if x.action.startswith("script."):
            scripts.setdefault(x.action, []).append(x)
    return Appelants([x for x in detail if not x.action.startswith("script.")], scripts,
                     champs_lus(a), evenements_ecoutes(a), seuil_protocole(a))


def version_firmware(ref: str | None) -> int | None:
    """Version du firmware d'un tag (X·10⁶ + Y·10³ + Z) ; None pour une autre révision ou
    la copie de travail, plus récente que tout tag (les gardes de version y sont vraies)."""
    if ref and (m := TAG.match(ref)):
        x, y, z, _ = m.groups()
        return int(x) * 1_000_000 + int(y) * 1_000 + int(z)
    return None


def _possible(lieu: Lieu, c: Contrat, version: int | None, ha: Appelants, pile=()) -> bool:
    """Un appel à cet endroit peut-il partir avec ce firmware ? Non s'il est gardé par une
    version plus haute, si tous ses déclencheurs possibles sont des événements que ce
    firmware n'émet pas, ou s'il est dans un script que seuls de tels endroits lancent.
    Un script qu'aucun fichier ne lance (tableau de bord, autre automatisation) : oui."""
    if lieu.protocole and ha.seuil_protocole and version is not None and version < ha.seuil_protocole:
        return False
    if lieu.declencheurs is not None:
        return any(d == "" or d in c.evenements for d in lieu.declencheurs)
    if lieu.script is not None:
        if lieu.script in pile:
            return False
        lanceurs = ha.scripts.get(lieu.script)
        if not lanceurs:
            return True
        return any(_possible(l, c, version, ha, pile + (lieu.script,)) for x in lanceurs for l in x.lieux)
    return True


def appel_possible(x: Appel, c: Contrat, version: int | None, ha: Appelants) -> bool:
    return any(_possible(l, c, version, ha) for l in x.lieux)


# ─── Instantané contrat/contrat.yaml ─────────────────────────────────────────

ENTETE = """\
# Contrat Home Assistant <-> firmware de la tablette : instantané lu dans le code.
# Généré par `python tools/contrat_api.py --write` : ne modifier à la main que `version:`.
# - services : actions `api: services:` (Tab5/paquets/tab5-api-logic.yaml), variable: type.
#   Toutes les variables sont obligatoires, et HA refuse une clé en trop.
# - evenements : événements `homeassistant.event` émis par le firmware, et leurs champs.
# Version (semver du contrat, distincte de celle du firmware), contre le dernier tag :
# MAJEURE si un fichier HA d'avant peut casser (action retirée, variable ajoutée, retirée
# ou de type changé, événement ou champ plus émis), MINEURE pour un ajout pur (action,
# événement ou champ nouveau). tests/test_contrat_versions.py le vérifie.
"""


def texte_instantane(c: Contrat, version: str) -> str:
    lignes = [ENTETE.rstrip("\n"), f"version: {version}", "services:"]
    for nom in sorted(c.services):
        variables = c.services[nom]
        if not variables:
            lignes.append(f"  {nom}: {{}}")
            continue
        lignes.append(f"  {nom}:")
        lignes += [f"    {v}: {variables[v]}" for v in sorted(variables)]
    lignes.append("evenements:")
    for nom in sorted(c.evenements):
        lignes.append(f"  {nom}: [{', '.join(sorted(c.evenements[nom]))}]")
    return "\n".join(lignes) + "\n"


def lire_instantane(texte: str) -> tuple[str, Contrat]:
    d = yaml.safe_load(texte)
    version = str(d["version"])
    if not SEMVER.match(version):
        raise ErreurContrat(f"{INSTANTANE} : version « {version} », attendu X.Y.Z")
    services = {nom: dict(v or {}) for nom, v in (d.get("services") or {}).items()}
    evenements = {nom: frozenset(v or ()) for nom, v in (d.get("evenements") or {}).items()}
    return version, Contrat(services, evenements)


def version_a(ref: str | None) -> str:
    """Version du contrat à une révision : celle de son instantané, ou
    VERSION_SANS_INSTANTANE s'il n'y en avait pas encore."""
    a = arbre(ref)
    if not a.existe(INSTANTANE):
        return VERSION_SANS_INSTANTANE
    return lire_instantane(a.lire(INSTANTANE))[0]


# ─── Semver ──────────────────────────────────────────────────────────────────

MAJEURE, MINEURE = "majeure", "mineure"


def differences(avant: Contrat, apres: Contrat) -> list[tuple[str, str]]:
    """[(niveau, texte)] de ce qui sépare deux contrats, selon la règle de l'en-tête."""
    diffs = []
    for nom in sorted(avant.services.keys() - apres.services.keys()):
        diffs.append((MAJEURE, f"action {nom} retirée"))
    for nom in sorted(apres.services.keys() - avant.services.keys()):
        diffs.append((MINEURE, f"action {nom} ajoutée"))
    for nom in sorted(avant.services.keys() & apres.services.keys()):
        va, vb = avant.services[nom], apres.services[nom]
        for v in sorted(va.keys() - vb.keys()):
            diffs.append((MAJEURE, f"variable {v} retirée de {nom} (HA d'avant l'envoie : refusée en trop)"))
        for v in sorted(vb.keys() - va.keys()):
            diffs.append((MAJEURE, f"variable {v} ajoutée à {nom} (obligatoire : HA d'avant ne l'envoie pas)"))
        for v in sorted(va.keys() & vb.keys()):
            if va[v] != vb[v]:
                diffs.append((MAJEURE, f"variable {v} de {nom} : type {va[v]} → {vb[v]}"))
    for nom in sorted(avant.evenements.keys() - apres.evenements.keys()):
        diffs.append((MAJEURE, f"événement {nom} plus émis"))
    for nom in sorted(apres.evenements.keys() - avant.evenements.keys()):
        diffs.append((MINEURE, f"événement {nom} ajouté"))
    for nom in sorted(avant.evenements.keys() & apres.evenements.keys()):
        for champ in sorted(avant.evenements[nom] - apres.evenements[nom]):
            diffs.append((MAJEURE, f"champ {champ} de {nom} plus émis"))
        for champ in sorted(apres.evenements[nom] - avant.evenements[nom]):
            diffs.append((MINEURE, f"champ {champ} ajouté à {nom}"))
    return diffs


def _semver(version: str) -> tuple[int, int, int]:
    m = SEMVER.match(version)
    if not m:
        raise ErreurContrat(f"version de contrat « {version} », attendu X.Y.Z")
    return tuple(int(x) for x in m.groups())


def version_minimale(base: str, diffs) -> str:
    """Plus petite version de contrat qui satisfait la règle depuis `base`."""
    x, y, z = _semver(base)
    niveaux = {n for n, _ in diffs}
    if MAJEURE in niveaux:
        return f"{x + 1}.0.0"
    if MINEURE in niveaux:
        return f"{x}.{y + 1}.0"
    return base


def ecart_semver(base: str, version: str, diffs) -> str | None:
    """Ce qui manque à `version` pour respecter la règle depuis `base`, ou None."""
    v, b = _semver(version), _semver(base)
    niveaux = {n for n, _ in diffs}
    if v < b:
        return f"version {version} plus basse que celle du tag ({base})"
    if MAJEURE in niveaux and v[0] <= b[0]:
        return f"changement incompatible : la version doit monter en majeure (au moins {version_minimale(base, diffs)})"
    if MINEURE in niveaux and MAJEURE not in niveaux and v[:2] <= b[:2]:
        return f"ajout : la version doit monter en mineure (au moins {version_minimale(base, diffs)})"
    return None


# ─── Tags publiés ────────────────────────────────────────────────────────────

def cle_tag(tag: str):
    """Ordre semver d'un tag v X.Y.Z[-rc.N] : la rc avant la version finale."""
    m = TAG.match(tag)
    x, y, z, rc = m.groups()
    return (int(x), int(y), int(z), 0 if rc else 1, int(rc or 0))


def tags_publies(racine: Path = REPO) -> list[str]:
    """Tags v X.Y.Z[-rc.N] du dépôt local, dans l'ordre semver. [] sans git ou sans tag
    (checkout superficiel de la CI sans l'étape qui les récupère)."""
    try:
        sortie = _git(["tag", "--list", "v*"], racine).decode("utf-8")
    except (OSError, subprocess.CalledProcessError):
        return []
    return sorted((t for t in sortie.split() if TAG.match(t)), key=cle_tag)


def _tags_sur(ref: str, racine: Path = REPO) -> set[str]:
    try:
        return set(_git(["tag", "--points-at", ref], racine).decode("utf-8").split())
    except (OSError, subprocess.CalledProcessError):
        return set()


def tag_precedent(courant: str | None = None, stable: bool = False, racine: Path = REPO) -> str | None:
    """Dernier tag publié avant `courant` : avant ce tag si `courant` en est un, sinon le
    plus haut tag qui ne pointe pas sur `courant` (HEAD pour la copie de travail).
    `stable` : sans les pré-releases (-rc.N). None si aucun."""
    tags = [t for t in tags_publies(racine) if not (stable and "-rc." in t)]
    if courant and TAG.match(courant):
        tags = [t for t in tags if cle_tag(t) < cle_tag(courant)]
    else:
        tags = [t for t in tags if t not in _tags_sur(courant or "HEAD", racine)]
    return tags[-1] if tags else None


# ─── Matrice N-1 ─────────────────────────────────────────────────────────────

OK, DEGRADE, CASSE = 0, 1, 2


@dataclass
class Croisement:
    """Un firmware avec des fichiers HA : ce qui casse (appel refusé par HA, le script
    s'arrête) et ce qui se dégrade (champ ou événement attendu jamais émis : vide)."""
    casse: list = field(default_factory=list)
    degrade: list = field(default_factory=list)

    @property
    def niveau(self) -> int:
        return CASSE if self.casse else DEGRADE if self.degrade else OK


def croiser(c: Contrat, version: int | None, ha: Appelants,
            ha_du_firmware: Appelants | None = None) -> Croisement:
    """Firmware `c` (version `version`) avec les fichiers `ha`. Un appel qui ne peut pas
    partir avec ce firmware (voir `_possible`) ne casse rien. `ha_du_firmware` : les
    fichiers HA de la même version que ce firmware ; ce qu'ils appellent ou écoutent et que
    `ha` ignore est une fonction qui attend l'autre moitié (dégradé)."""
    x = Croisement()
    refus = {}
    for appel in ha.appels:
        fichier, nom, cles = appel.fichier, appel.action, appel.cles
        if not appel_possible(appel, c, version, ha):
            continue
        if nom not in c.services:
            raison = f"action {nom} absente du firmware"
        else:
            attendues, donnees = set(c.services[nom]), set(cles)
            parts = []
            if manquantes := attendues - donnees:
                parts.append(f"variable(s) manquante(s) {sorted(manquantes)}")
            if en_trop := donnees - attendues:
                parts.append(f"variable(s) en trop {sorted(en_trop)}")
            if not parts:
                continue
            raison = f"{nom} : " + " ; ".join(parts)
        refus.setdefault(raison, set()).add(fichier)
    x.casse = [f"{r} ({', '.join(sorted(f))})" for r, f in sorted(refus.items())]
    for evt, fichiers in sorted(ha.ecoutes.items()):
        if evt not in c.evenements:
            x.degrade.append(f"{evt} écouté, jamais émis ({', '.join(sorted(set(fichiers)))})")
    for evt, champs in sorted(ha.lus.items()):
        if evt not in c.evenements:
            continue  # déjà compté ci-dessus
        for champ, fichiers in sorted(champs.items()):
            if champ not in CHAMPS_DE_HA and champ not in c.evenements[evt]:
                x.degrade.append(f"{evt} : champ {champ} lu, jamais émis ({', '.join(sorted(set(fichiers)))})")
    if ha_du_firmware is not None and ha_du_firmware is not ha:
        appelees = {x.action for x in ha.appels}
        for nom in sorted({x.action for x in ha_du_firmware.appels} - appelees):
            if nom in c.services:
                x.degrade.append(f"action {nom} jamais appelée par ces fichiers HA")
        for evt in sorted(ha_du_firmware.ecoutes.keys() - ha.ecoutes.keys()):
            if evt in c.evenements:
                x.degrade.append(f"{evt} émis, écouté par aucun de ces fichiers HA")
    return x


@dataclass
class Matrice:
    precedent: str
    courant: str          # nom lisible (tag ou « copie de travail »)
    fw_courant_ha_precedent: Croisement
    fw_precedent_ha_courant: Croisement
    fw_courant_ha_courant: Croisement
    fw_precedent_ha_precedent: Croisement

    def sur(self, ordre: str) -> bool:
        """L'ordre annoncé ne passe par aucun état cassé. « le firmware d'abord » = le
        nouveau firmware avec les anciens fichiers HA pendant un moment ; « ensemble » ne
        se justifie que si les deux sens cassent."""
        fw_d_abord = self.fw_courant_ha_precedent.niveau < CASSE
        ha_d_abord = self.fw_precedent_ha_courant.niveau < CASSE
        return {"indifferent": fw_d_abord and ha_d_abord, "firmware": fw_d_abord, "ha": ha_d_abord,
                "ensemble": not (fw_d_abord or ha_d_abord)}[ordre]

    @property
    def ordre(self) -> str:
        """Ordre conseillé, clé de ETIQUETTES : seul ce qui casse (HA refuse l'appel)
        décide ; une fonction qui attend l'autre moitié (dégradé) est listée, pas bloquante."""
        for ordre in ("indifferent", "ha", "firmware"):
            if self.sur(ordre):
                return ordre
        return "ensemble"


def matrice(precedent: str, courant: str | None = None) -> Matrice:
    cc, cp = contrat_firmware(courant), contrat_firmware(precedent)
    vc, vp = version_firmware(courant), version_firmware(precedent)
    hc, hp = appelants_ha(courant), appelants_ha(precedent)
    return Matrice(precedent, courant or "copie de travail",
                   croiser(cc, vc, hp, hc), croiser(cp, vp, hc, hp), croiser(cc, vc, hc), croiser(cp, vp, hp))


ETIQUETTES = {
    "fr": {
        "indifferent": "compatible dans les deux sens",
        "firmware": "le firmware d'abord",
        "ha": "Home Assistant d'abord",
        "ensemble": "firmware et Home Assistant ensemble",
    },
    "en": {
        "indifferent": "compatible both ways",
        "firmware": "firmware first",
        "ha": "Home Assistant first",
        "ensemble": "firmware and Home Assistant together",
    },
}
ETIQUETTE = re.compile(r"^\*\*Contrat HA ↔ firmware\*\* : (?P<etiquette>[^(]+?) \(depuis (?P<depuis>v\S+?)\)\.?\s*$")


def etiquette(m: Matrice) -> str:
    """Ligne à poser dans le CHANGELOG, vérifiée par tests/test_contrat_versions.py."""
    return f"**Contrat HA ↔ firmware** : {ETIQUETTES['fr'][m.ordre]} (depuis {m.precedent})."


_TEXTES = {
    "fr": {"titre": "Contrat Home Assistant ↔ firmware : {c} contre {p}",
           "fw": "Firmware", "ha": "Fichiers HA", "ok": "OK", "degrade": "dégradé ({n})", "casse": "cassé ({n})",
           "ordre": "Ordre de mise à jour : **{e}**.",
           "legende": "cassé = HA refuse l'appel et le script s'arrête ; dégradé = un champ ou un événement "
                      "attendu n'est jamais émis (valeur vide, sans erreur). Lu dans le code (signatures), "
                      "pas le contenu des payloads.",
           "detail": "Détail"},
    "en": {"titre": "Home Assistant ↔ firmware contract: {c} against {p}",
           "fw": "Firmware", "ha": "HA files", "ok": "OK", "degrade": "degraded ({n})", "casse": "broken ({n})",
           "ordre": "Update order: **{e}**.",
           "legende": "broken = HA refuses the call and the script stops; degraded = an expected field or event "
                      "is never sent (empty value, no error). Read from the code (signatures), not the payload "
                      "contents.",
           "detail": "Details"},
}


def tableau(m: Matrice, langue: str = "fr") -> str:
    t = _TEXTES[langue]

    def case(x: Croisement) -> str:
        if x.niveau == CASSE:
            return t["casse"].format(n=len(x.casse))
        if x.niveau == DEGRADE:
            return t["degrade"].format(n=len(x.degrade))
        return t["ok"]

    lignes = [
        f"#### {t['titre'].format(c=m.courant, p=m.precedent)}", "",
        f"| {t['fw']} \\ {t['ha']} | {m.precedent} | {m.courant} |", "|---|---|---|",
        f"| {m.precedent} | {case(m.fw_precedent_ha_precedent)} | {case(m.fw_precedent_ha_courant)} |",
        f"| {m.courant} | {case(m.fw_courant_ha_precedent)} | {case(m.fw_courant_ha_courant)} |", "",
        t["ordre"].format(e=ETIQUETTES[langue][m.ordre]), "", t["legende"],
    ]
    details = [((m.precedent, m.precedent), m.fw_precedent_ha_precedent),
               ((m.precedent, m.courant), m.fw_precedent_ha_courant),
               ((m.courant, m.precedent), m.fw_courant_ha_precedent),
               ((m.courant, m.courant), m.fw_courant_ha_courant)]
    if any(x.niveau for _, x in details):
        lignes += ["", f"{t['detail']} :", ""]
        for (fw, ha), x in details:
            for texte in x.casse + x.degrade:
                lignes.append(f"- {t['fw']} {fw} × {t['ha']} {ha} : {texte}")
    return "\n".join(lignes) + "\n"


# ─── Ligne de commande ───────────────────────────────────────────────────────

def _check() -> int:
    chemin = REPO / INSTANTANE
    if not chemin.is_file():
        print(f"{INSTANTANE} absent : `python tools/contrat_api.py --write`")
        return 1
    texte = chemin.read_text(encoding="utf-8").replace("\r\n", "\n")
    version, lu = lire_instantane(texte)
    attendu = contrat_firmware(None)
    if texte != texte_instantane(attendu, version):
        for _, d in differences(lu, attendu) or [("", "mise en forme seulement")]:
            print(f"  {d}")
        print(f"{INSTANTANE} ne correspond plus au code : `python tools/contrat_api.py --write`, "
              "puis monter `version:` si `--semver` le demande")
        return 1
    print(f"{INSTANTANE} : à jour (version {version})")
    return 0


def _write() -> int:
    chemin = REPO / INSTANTANE
    version = lire_instantane(chemin.read_text(encoding="utf-8"))[0] if chemin.is_file() else VERSION_SANS_INSTANTANE
    chemin.parent.mkdir(exist_ok=True)
    with open(chemin, "w", encoding="utf-8", newline="\n") as f:
        f.write(texte_instantane(contrat_firmware(None), version))
    print(f"{INSTANTANE} écrit (version {version})")
    return 0


def _semver_cli(depuis: str | None) -> int:
    tag = depuis or tag_precedent()
    if tag is None:
        print("aucun tag v* dans ce dépôt (`git fetch --tags`) : rien à comparer")
        return 1
    base = version_a(tag)
    version = lire_instantane((REPO / INSTANTANE).read_text(encoding="utf-8"))[0]
    diffs = differences(contrat_firmware(tag), contrat_firmware(None))
    print(f"contrat {base} au tag {tag}, {version} dans la copie de travail")
    for niveau, texte in diffs:
        print(f"  [{niveau}] {texte}")
    if ecart := ecart_semver(base, version, diffs):
        print(f"{ecart} : `version:` de {INSTANTANE}")
        return 1
    print("version suffisante" if diffs else "aucun changement de contrat")
    return 0


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--check", action="store_true", help="instantané = code ?")
    g.add_argument("--write", action="store_true", help="régénère l'instantané (version gardée)")
    g.add_argument("--semver", action="store_true", help="version suffisante contre le dernier tag ?")
    g.add_argument("--matrice", action="store_true", help="tableau N-1 en Markdown")
    p.add_argument("--depuis", help="tag de référence (défaut : dernier tag ; dernier tag stable pour --matrice)")
    p.add_argument("--courant", help="révision courante pour --matrice (défaut : copie de travail)")
    p.add_argument("--en", action="store_true", help="--matrice en anglais (note de release)")
    args = p.parse_args(argv)
    if sys.stdout.encoding and sys.stdout.encoding.lower() not in ("utf-8", "utf8"):
        sys.stdout.reconfigure(encoding="utf-8")
    if args.check:
        return _check()
    if args.write:
        return _write()
    if args.semver:
        return _semver_cli(args.depuis)
    depuis = args.depuis or tag_precedent(args.courant, stable=True)
    if depuis is None:
        print("aucun tag v* dans ce dépôt (`git fetch --tags`) : rien à comparer")
        return 1
    m = matrice(depuis, args.courant)
    print(tableau(m, "en" if args.en else "fr"), end="")
    if not args.en:
        print("\nLigne du CHANGELOG :\n" + etiquette(m))
    return 0


if __name__ == "__main__":
    sys.exit(main())
