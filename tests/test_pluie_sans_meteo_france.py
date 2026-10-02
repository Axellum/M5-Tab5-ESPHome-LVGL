# -*- coding: utf-8 -*-
"""Pluie dans l'heure sans Météo-France (02/10/2026, discussion #278 : un utilisateur en
Turquie n'avait rien sur la carte pluie). Météo-France est le premier choix de la liste
« Tab5 · source de la pluie dans l'heure », donc celui d'une installation neuve ; sans
capteur de pluie Météo-France, la source effective devient Open-Meteo, seul service qui
répond partout.

Les vrais modèles de packages/tab5_meteo_sources.yaml sont rendus ici avec jinja2 et de
faux états : la variable `source`, puis l'état, les barres et l'attribut `source` du
capteur, qui doivent lire cette variable et jamais la liste elle-même."""
import datetime as dt
import os

import jinja2
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PAQUET = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_meteo_sources.yaml")
LISTE = "input_select.tab5_source_pluie"
MAINTENANT = dt.datetime(2026, 10, 2, 12, 0, 0, tzinfo=dt.timezone.utc)


def _capteur_pluie():
    """(variables, capteur) du modèle à déclencheurs « Tab5 Pluie dans l'heure »."""
    with open(PAQUET, encoding="utf-8") as f:
        paquet = yaml.safe_load(f)
    for bloc in paquet["template"]:
        for capteur in bloc.get("sensor", []):
            if capteur.get("unique_id") == "tab5_pluie_dans_l_heure":
                return bloc["variables"], capteur
    raise AssertionError("capteur tab5_pluie_dans_l_heure introuvable")


def _rendre(modele, choix="Météo-France", **variables):
    env = ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined)
    env.globals.update(
        states=lambda e: choix if e == LISTE else "unknown",
        state_attr=lambda e, a: None,
        now=lambda: MAINTENANT,
    )
    return env.from_string(modele).render(**variables).strip()


def _source(choix, mf_pluie):
    variables, _ = _capteur_pluie()
    return _rendre(variables["source"], choix=choix, mf_pluie=mf_pluie)


def test_meteo_france_sans_capteur_passe_sur_open_meteo():
    assert _source("Météo-France", "") == "Open-Meteo"


def test_meteo_france_avec_capteur_reste_meteo_france():
    assert _source("Météo-France", "sensor.paris_next_rain") == "Météo-France"


def test_les_autres_choix_ne_changent_pas():
    for choix in ["OpenWeatherMap", "Buienradar", "DWD", "Met.no", "Open-Meteo", "Aucune"]:
        assert _source(choix, "") == choix, choix


def test_les_modeles_lisent_la_source_effective_pas_la_liste():
    _, capteur = _capteur_pluie()
    for nom, modele in [("état", capteur["state"]), ("barres", capteur["attributes"]["barres"]),
                        ("source", capteur["attributes"]["source"])]:
        assert LISTE not in modele, f"{nom} relit la liste au lieu de la variable `source`"


def test_carte_pluie_remplie_par_open_meteo_sans_meteo_france():
    _, capteur = _capteur_pluie()
    source = _source("Météo-France", "")
    debut = int(MAINTENANT.timestamp()) + 600
    # Série commune [début, durée en min, mm/h] : 3 mm/h dans 10 min, pluie modérée.
    serie = [[debut - 900, 15, 0.0], [debut, 15, 3.0]]
    variables = {"source": source, "mf_pluie": "", "serie": serie}
    assert _rendre(capteur["state"], **variables) == f"@2,{debut}"
    barres = _rendre(capteur["attributes"]["barres"], **variables)
    assert barres.startswith("0|0;1|0;2|2;"), barres
    assert _rendre(capteur["attributes"]["source"], **variables) == "Open-Meteo"
    # Pas de réponse d'Open-Meteo : « pas de données », plus la carte masquée (@-).
    assert _rendre(capteur["state"], source=source, mf_pluie="", serie=None) == "@-1,0"
