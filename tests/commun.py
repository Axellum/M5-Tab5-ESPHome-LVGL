# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Boîte à outils commune des tests : à réutiliser avant d'en écrire une copie.

Constat OUT-3 de l'audit du 07/10/2026 : chaque fichier de test avait recopié son chargeur
YAML (25 `_Chargeur(yaml.SafeLoader)`), sa lecture de fichier (27 `_lire`), ses
`sys.path.insert` (39) et ses découpes `.split("- service: X")` de l'API du firmware.
Tout est ici, une fois :

- `REPO`, `TAB5`, `HA` : chemins (`pathlib.Path`) de la racine du dépôt, de `Tab5/` et de
  `HomeAssistant_Config/`. Le `sys.path` des outils (`tools/`, `tools/demo/`…) est posé
  une fois par `tests/conftest.py`.
- `source(nom)`, `sources(*motifs)` : un fichier du firmware par son nom seul, et les
  fichiers d'un motif, dans `Tab5/socle|ecran|jeux|paquets` (tools/tab5_sources.py).
- `lire(*chemin)` : texte UTF-8 d'un fichier, chemin relatif à la racine ou absolu.
- `fichiers_du_depot(dossier, motif)` : comme `rglob`, sans les fichiers que .gitignore
  écarte (le même jeu en local qu'en CI).
- Chargeurs YAML, tous sur le chargeur C de PyYAML (libyaml, ~10 fois plus rapide, mêmes
  objets que le chargeur Python sur les 169 YAML du dépôt, vérifié le 08/10/2026) :
  * `ChargeurSansBalises` : une balise (`!lambda`, `!include`, `!secret`, `!input`…) vaut None ;
  * `ChargeurBalisesBrutes` : une balise sur un scalaire vaut ce scalaire brut, sinon None ;
  * `ChargeurEntrees` : un `!input x` de blueprint vaut `{"!input": "x"}`.
- `jeton(nom)`, `avec_jetons(texte)` : un jeton de géométrie de
  `Tab5/paquets/tab5-ui-tokens.yaml` en entier, et un texte YAML où chaque `${jeton}` connu
  est remplacé par sa valeur (pour lire une hauteur posée par un jeton comme un nombre).
- `bloc_service(nom)` : le texte d'un service de `Tab5/paquets/tab5-api-logic.yaml`, de sa ligne
  `- service: nom` à la suivante (ou à la fin du bloc `api:`).
- `CacheJinja` : cache en mémoire du code compilé des modèles Jinja (`bytecode_cache=` d'un
  environnement, pour les modèles lus par un loader ; `.depuis_texte(env, texte)` pour un
  `env.from_string(texte)`). Une instance par configuration d'environnement (mêmes
  extensions, filtres et tests) : le code compilé n'est partagé qu'entre environnements
  bâtis par la même fonction.

Un test garde ses propres outils quand ils font autre chose (par exemple le `_Chargeur` du
blueprint de test_tuiles_blueprint.py, qui garde les `!input` comme objets `_Entree`).
"""
from __future__ import annotations

import re
import subprocess
from pathlib import Path

import jinja2
import yaml

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
HA = REPO / "HomeAssistant_Config"
API = TAB5 / "paquets" / "tab5-api-logic.yaml"

# Sources du firmware rangées dans Tab5/socle|ecran|jeux|paquets (08/10/2026) :
# `source("x.cpp")` trouve un fichier par son nom seul, `sources("*.cpp", …)` remplace
# un glob sur la racine de Tab5/ (qui ne trouverait plus rien). Voir tools/tab5_sources.py
# (tools/ est dans le sys.path posé par tests/conftest.py). `contrat()` : le texte de
# tab5_custom.h et des en-têtes de modules qu'il inclut (un par module depuis le 08/10/2026).
from tab5_sources import contrat, fichiers as sources, source  # noqa: E402,F401

# Chargeur C de PyYAML s'il est compilé (roues officielles), sinon le chargeur Python.
BaseChargeur = getattr(yaml, "CSafeLoader", yaml.SafeLoader)


def lire(*chemin) -> str:
    """Texte UTF-8 d'un fichier ; `lire("Tab5", "x.cpp")` ou `lire(chemin_absolu)`."""
    return REPO.joinpath(*chemin).read_text(encoding="utf-8")


class ChargeurSansBalises(BaseChargeur):
    """YAML ESPHome ou HA sans résoudre ses balises : chacune vaut None."""


ChargeurSansBalises.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


class ChargeurBalisesBrutes(BaseChargeur):
    """Balises ESPHome (!lambda, !extend, !include…) : leur valeur brute suffit."""


ChargeurBalisesBrutes.add_multi_constructor(
    "!", lambda chargeur, suffixe, noeud: chargeur.construct_scalar(noeud) if isinstance(noeud, yaml.ScalarNode) else None)


class ChargeurEntrees(BaseChargeur):
    """Blueprint HA : un `!input x` vaut `{"!input": "x"}`."""


ChargeurEntrees.add_constructor("!input", lambda chargeur, noeud: {"!input": chargeur.construct_scalar(noeud)})


def fichiers_du_depot(dossier: Path, motif: str = "*") -> list[Path]:
    """Fichiers de `dossier` qui correspondent à `motif`, SANS ceux que .gitignore écarte
    (HomeAssistant_Config/rendered/, placeholders.yaml…) : en local, le même jeu qu'en CI,
    où ils n'existent pas (constat HA-12 de l'audit du 07/10/2026). Un fichier neuf pas
    encore ajouté à git compte. Sans git : tout le dossier (rglob)."""
    try:
        sortie = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", str(dossier)],
                                cwd=REPO, capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return sorted(dossier.rglob(motif))
    chemins = {REPO / p for p in sortie.decode("utf-8").split("\0") if p}
    return sorted(p for p in chemins if p.match(motif) and p.is_file())


def charger(*chemin, chargeur=ChargeurSansBalises):
    """Un YAML du dépôt (chemin comme `lire`), lu par `chargeur`."""
    return yaml.load(lire(*chemin), Loader=chargeur)


def _jetons() -> dict[str, str]:
    return {k: str(v) for k, v in charger("Tab5", "paquets", "tab5-ui-tokens.yaml")["substitutions"].items()}


def jeton(nom: str) -> int:
    """Valeur entière d'un jeton de géométrie (`Tab5/paquets/tab5-ui-tokens.yaml`)."""
    return int(_jetons()[nom])


def avec_jetons(texte: str) -> str:
    """`texte` où chaque `${jeton}` de `tab5-ui-tokens.yaml` est remplacé par sa valeur,
    comme ESPHome le fait (les autres substitutions restent telles quelles)."""
    valeurs = _jetons()
    return re.sub(r"\$\{(\w+)\}", lambda m: valeurs.get(m.group(1), m.group(0)), texte)


_SERVICE = re.compile(r"^([ \t]*)- service: (\S+)[ \t]*$", re.M)


def bloc_service(nom: str, texte: str | None = None) -> str:
    """Texte d'un service de l'API du firmware, ligne `- service: nom` comprise, jusqu'au
    service suivant ou à la première clé de premier niveau qui suit (`provisioning:`…).
    AssertionError si le service n'existe pas."""
    texte = lire(API) if texte is None else texte
    debut = next((m for m in _SERVICE.finditer(texte) if m.group(2) == nom), None)
    assert debut, f"service {nom} introuvable dans {API.name}"
    suite = texte[debut.end():]
    fins = [m.start() for m in _SERVICE.finditer(suite) if m.group(1) == debut.group(1)]
    fins += [m.start() for m in re.finditer(r"^[^\s#]", suite, re.M)]
    return texte[debut.start():debut.end() + min(fins, default=len(suite))]


class CacheJinja(jinja2.BytecodeCache):
    """Code compilé des modèles, en mémoire, partagé par les environnements d'une même
    configuration. Clé : nom du modèle ET somme du source (un source modifié se recompile)."""

    def __init__(self):
        self._codes = {}

    def load_bytecode(self, bucket):
        code = self._codes.get((bucket.key, bucket.checksum))
        if code is not None:
            bucket.code = code

    def dump_bytecode(self, bucket):
        self._codes[(bucket.key, bucket.checksum)] = bucket.code

    def depuis_texte(self, env, texte: str):
        """Comme `env.from_string(texte)`, sans recompiler un texte déjà vu."""
        code = self._codes.get(("texte", texte))
        if code is None:
            code = self._codes[("texte", texte)] = env.compile(texte)
        return env.template_class.from_code(env, code, env.make_globals(None))
