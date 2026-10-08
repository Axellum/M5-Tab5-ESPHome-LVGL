# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Boîte à outils commune des tests : à réutiliser avant d'en écrire une copie.

Constat OUT-3 de l'audit du 07/10/2026 : chaque fichier de test avait recopié son chargeur
YAML (25 `_Chargeur(yaml.SafeLoader)`), sa lecture de fichier (27 `_lire`), ses
`sys.path.insert` (39) et ses découpes `.split("- service: X")` de l'API du firmware.
Tout est ici, une fois :

- `REPO`, `TAB5`, `HA` : chemins (`pathlib.Path`) de la racine du dépôt, de `Tab5/` et de
  `HomeAssistant_Config/`. Le `sys.path` des outils (`tools/`, `tools/demo/`…) est posé
  une fois par `tests/conftest.py`.
- `lire(*chemin)` : texte UTF-8 d'un fichier, chemin relatif à la racine ou absolu.
- Chargeurs YAML, tous sur le chargeur C de PyYAML (libyaml, ~10 fois plus rapide, mêmes
  objets que le chargeur Python sur les 169 YAML du dépôt, vérifié le 08/10/2026) :
  * `ChargeurSansBalises` : une balise (`!lambda`, `!include`, `!secret`, `!input`…) vaut None ;
  * `ChargeurBalisesBrutes` : une balise sur un scalaire vaut ce scalaire brut, sinon None ;
  * `ChargeurEntrees` : un `!input x` de blueprint vaut `{"!input": "x"}`.
- `bloc_service(nom)` : le texte d'un service de `Tab5/tab5-api-logic.yaml`, de sa ligne
  `- service: nom` à la suivante (ou à la fin du bloc `api:`).

Un test garde ses propres outils quand ils font autre chose (par exemple le `_Chargeur` du
blueprint de test_tuiles_blueprint.py, qui garde les `!input` comme objets `_Entree`).
"""
from __future__ import annotations

import re
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
HA = REPO / "HomeAssistant_Config"
API = TAB5 / "tab5-api-logic.yaml"

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


def charger(*chemin, chargeur=ChargeurSansBalises):
    """Un YAML du dépôt (chemin comme `lire`), lu par `chargeur`."""
    return yaml.load(lire(*chemin), Loader=chargeur)


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

