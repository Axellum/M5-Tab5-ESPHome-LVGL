# -*- coding: utf-8 -*-
"""Démarrage de Home Assistant (28/09/2026, HA Core 2026.9.4) : la tablette, restée
allumée, se reconnecte souvent AVANT que les automatisations soient actives ; son
événement esphome.tab5_connected est alors perdu (entités revenues à 21:11:12,
événement vers 21:11:14, automatisations actives à 21:11:16).

La poussée complète (packages/tab5_push.yaml) suit donc aussi le chemin de la
reconnexion au démarrage de HA : météo actuelle, probabilités et volet, qui ne partent
qu'à la reconnexion. Sa garde « liaison `on` depuis moins de 3 min », rendue ici avec
une fausse tablette, ne doit viser que tab5_connected. Le même déclencheur du blueprint
« Tab5 — emplacements » est vérifié au rendu par tests/test_tuiles_blueprint.py."""
import datetime as dt
import os
import re
from types import SimpleNamespace

import jinja2
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PUSH = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_push.yaml")


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _poussee_complete():
    with open(PUSH, encoding="utf-8") as f:
        paquet = yaml.load(f.read(), Loader=_Chargeur)
    return next(a for a in paquet["automation"] if a.get("id") == "tab5_ha_hmi_updater")


def _parcourir(noeud):
    """Tous les dict de l'arbre."""
    if isinstance(noeud, dict):
        yield noeud
        for v in noeud.values():
            yield from _parcourir(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _parcourir(v)


def test_la_poussee_complete_part_au_demarrage_de_ha():
    auto = _poussee_complete()
    assert {"trigger": "homeassistant", "event": "start", "id": "demarrage_ha"} in auto["triggers"]
    # Partout où la reconnexion pousse ce qui ne part qu'à elle (météo actuelle et
    # probabilités, volet), le démarrage aussi. L'attente de la liaison (étape 0) reste
    # propre à tab5_connected : au démarrage, la tablette est déjà là ou pas encore.
    listes = [c["id"] for c in _parcourir(auto["actions"])
              if c.get("condition") == "trigger" and isinstance(c.get("id"), list)]
    avec_reconnexion = [l for l in listes if "tab5_connected" in l]
    assert len(avec_reconnexion) == 2, listes
    assert all(set(l) == {"tab5_connected", "demarrage_ha"} for l in avec_reconnexion), listes


def _garde_des_3_minutes(auto):
    gardes = [c["value_template"] for c in auto["conditions"]
              if c.get("condition") == "template" and "180" in c.get("value_template", "")]
    assert len(gardes) == 1, gardes
    return gardes[0]


def _rendre(modele, declencheur, liaison_depuis):
    """La garde rendue pour une tablette dont « HA API Status » est `on` depuis
    `liaison_depuis` secondes."""
    maintenant = dt.datetime(2026, 9, 28, 21, 11, 16, tzinfo=dt.timezone.utc)
    capteur = "binary_sensor.tab5_ha_api_status"
    etat = SimpleNamespace(state="on", last_changed=maintenant - dt.timedelta(seconds=liaison_depuis))
    env = ImmutableSandboxedEnvironment(
        undefined=jinja2.StrictUndefined,
        # Macros importées (custom_templates/tab5_tablette.jinja, HA-7).
        loader=jinja2.FileSystemLoader(os.path.join(REPO, "HomeAssistant_Config", "custom_templates")))
    env.globals.update(
        integration_entities=lambda domaine: [capteur] if domaine == "esphome" else [],
        device_attr=lambda e, nom: "tab5-ha-hmi" if nom == "model" else None,
        is_state=lambda e, s: s == "on", states={capteur: etat}, now=lambda: maintenant,
    )
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None
    return env.from_string(modele).render(trigger={"id": declencheur}).strip() == "True"


def test_la_garde_des_3_minutes_ne_vise_que_tab5_connected():
    garde = _garde_des_3_minutes(_poussee_complete())
    assert _rendre(garde, "tab5_connected", 4), "liaison qui vient de (re)démarrer"
    assert not _rendre(garde, "tab5_connected", 600), "événement réémis : bloqué"
    assert _rendre(garde, "demarrage_ha", 4)
    assert _rendre(garde, "demarrage_ha", 600), "le démarrage de HA n'est jamais bloqué par l'âge de la liaison"
