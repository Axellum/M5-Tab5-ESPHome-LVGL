# -*- coding: utf-8 -*-
"""Versions : celle qu'annonce un firmware compilé soi-même, et l'alerte « fichiers HA en
retard » (audit du 30/09/2026, lot D).

- `tab5-ha-hmi.yaml` : le défaut de `project: version` (firmware compilé sans
  `-s tab5_version`) est la dernière version publiée du CHANGELOG suivie de « -dev ». Il
  était resté à 3.2.0-dev après la 3.3.1 : un garde « ≥ 3.3 » du blueprint aurait pris un
  build local pour un firmware trop ancien.
- `binary_sensor.tab5_fichiers_ha_en_retard` (`packages/tab5_health.yaml`) : son VRAI
  modèle Jinja est rendu ici, dans le bac à sable de Jinja comme le fait Home Assistant
  (même méthode que tests/test_tuiles_blueprint.py), avec de faux états. Il comparait
  X.Y.Z : une tablette en 3.3.1 avec des fichiers 3.3.0 (la 3.3.1 ne demandait que le
  blueprint) déclenchait l'alerte, pour rien et sans fin. Il ne compare plus que X.Y.
  La notification qui en dépend est rendue aussi."""
import re
from pathlib import Path

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

REPO = Path(__file__).resolve().parent.parent
HEALTH = REPO / "HomeAssistant_Config" / "packages" / "tab5_health.yaml"

CAPTEUR = "binary_sensor.tab5_fichiers_ha_en_retard"
VERSION_FICHIERS = "sensor.tab5_version_des_fichiers_ha"
TABLETTE = "sensor.tab5_tablette"


def _lire(chemin):
    return chemin.read_text(encoding="utf-8")


# ─── L'alerte « fichiers HA en retard » ─────────────────────────────────────

class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _health():
    return yaml.load(_lire(HEALTH), Loader=_Chargeur)


def _capteur():
    for bloc in _health()["template"]:
        for capteur in bloc.get("binary_sensor") or []:
            if capteur.get("unique_id") == "tab5_fichiers_ha_en_retard":
                return capteur
    raise AssertionError(f"{HEALTH.name} : binary_sensor tab5_fichiers_ha_en_retard introuvable")


def _environnement(etats, attributs):
    """Les fonctions de modèle de HA que ces modèles appellent, au plus près."""
    env = ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined)
    env.globals.update(
        states=lambda e: etats.get(e, "unknown"),
        state_attr=lambda e, nom: attributs.get(e, {}).get(nom),
    )
    env.filters["regex_findall"] = lambda v, motif="", ignorecase=False: re.findall(
        motif, str(v), re.I if ignorecase else 0)
    return env


def _en_retard(fichiers, tablette):
    """État rendu du capteur (True = allumé), comme un binary_sensor de modèle : le texte
    « True » allume, le reste éteint."""
    etats = {VERSION_FICHIERS: fichiers}
    attributs = {TABLETTE: {"version": tablette}}
    rendu = _environnement(etats, attributs).from_string(_capteur()["state"]).render().strip()
    assert rendu in ("True", "False"), f"rendu inattendu : {rendu!r}"
    return rendu == "True"


@pytest.mark.parametrize("fichiers, tablette, allume", [
    ("3.3.0", "3.3.1", False),       # version corrective : la 3.3.1 ne demandait que le blueprint
    ("3.3.1", "3.4.0", True),        # version mineure plus récente
    ("3.3.1", "4.0.0", True),        # version majeure plus récente
    ("3.3.0", "3.10.0", True),       # comparaison de nombres, pas de textes
    ("3.3.0", "3.3.1-dev", False),   # compilée soi-même : suffixe ignoré
    ("3.3.0", "3.4.0-rc.1", True),   # pré-release d'une mineure plus récente
    ("3.3.1", "3.3.1", False),
    ("3.4.0", "3.3.1", False),       # fichiers plus récents : ce capteur ne le dit pas
    ("dépôt", "3.4.0", False),       # fichiers copiés depuis le dépôt : jamais comparés
    ("unknown", "3.4.0", False),     # capteur de version absent
    ("3.3.0", "", False),            # tablette introuvable (sensor.tab5_tablette)
    ("3.3.0", None, False),
    ("3.3.0", "inconnue", False),    # version illisible
], ids=lambda v: str(v))
def test_fichiers_ha_en_retard(fichiers, tablette, allume):
    assert _en_retard(fichiers, tablette) is allume


def test_attributs_du_capteur():
    capteur = _capteur()
    env = _environnement({VERSION_FICHIERS: "3.3.0"}, {TABLETTE: {"version": "3.4.0"}})
    rendus = {nom: env.from_string(modele).render() for nom, modele in capteur["attributes"].items()}
    assert rendus == {"fichiers": "3.3.0", "tablette": "3.4.0"}


def test_notification_suit_le_capteur():
    (auto,) = [a for a in _health()["automation"] if a.get("id") == "tab5_health_fichiers_ha"]
    assert any(t.get("entity_id") == CAPTEUR for t in auto["trigger"]), "déclencheur sur le capteur"
    (branche,) = auto["action"]
    assert branche["if"] == [{"condition": "state", "entity_id": CAPTEUR, "state": "on"}]
    (creer,) = branche["then"]
    (retirer,) = branche["else"]
    assert creer["action"] == "persistent_notification.create"
    assert retirer["action"] == "persistent_notification.dismiss"
    assert creer["data"]["notification_id"] == retirer["data"]["notification_id"] == "tab5_fichiers_ha"
    env = _environnement({}, {CAPTEUR: {"fichiers": "3.3.1", "tablette": "3.4.0"}})
    message = env.from_string(creer["data"]["message"]).render()
    assert "La tablette est en 3.4.0" in message and "ses fichiers Home Assistant en 3.3.1" in message
    assert "The tablet runs 3.4.0" in message and "its Home Assistant files 3.3.1" in message
    # L'archive à prendre : celle de la release de la tablette (FR et EN).
    assert message.count("3.4.0") == 4 and message.count("tab5_home_assistant.zip") == 2
