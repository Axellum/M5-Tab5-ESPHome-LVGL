# -*- coding: utf-8 -*-
"""Pluie dans l'heure sans Météo-France (02/10/2026, discussion #278 : un utilisateur en
Turquie n'avait rien sur la carte pluie). Météo-France est le premier choix de la liste
« Tab5 · next-hour rain source », donc celui d'une installation neuve ; sans
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
    """(variables, capteur) du modèle à déclencheurs « Tab5 Rain in the next hour »."""
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
    for choix in ["Vigie", "OpenWeatherMap", "Buienradar", "DWD", "Met.no", "Open-Meteo"]:
        assert _source(choix, "") == choix, choix


def test_aucune_source_jamais_rendue_none():
    """« None » (24/10/2026) et « Aucune » d'avant donnent « none » : un rendu « None » serait
    relu par HA comme le None de Python (Template._parse_result de HA 2026.10), la source
    vaudrait None et la carte prendrait la branche Météo-France ; « none » masque la carte."""
    _, capteur = _capteur_pluie()
    for choix in ["None", "Aucune"]:
        assert _source(choix, "sensor.paris_next_rain") == "none", choix
    assert _rendre(capteur["state"], source="none", mf_pluie="sensor.paris_next_rain", serie=None) == "@-"


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


def _serie(source, rep):
    """La variable `serie` des actions du capteur, rendue avec une réponse de rest_command."""
    with open(PAQUET, encoding="utf-8") as f:
        paquet = yaml.safe_load(f)
    for bloc in paquet["template"]:
        if any(c.get("unique_id") == "tab5_pluie_dans_l_heure" for c in bloc.get("sensor", [])):
            modele = next(a["variables"]["serie"] for a in bloc["actions"] if "variables" in a)
            break
    env = ImmutableSandboxedEnvironment(undefined=jinja2.Undefined)
    env.filters["float"] = lambda v, d=0.0: float(v) if v is not None else d
    env.filters["int"] = lambda v, d=0: int(v) if v is not None else d
    env.globals.update(now=lambda: MAINTENANT)
    rendu = env.from_string(modele).render(source=source, rep=rep, owm_meteo="").strip()
    return None if rendu == "None" else yaml.safe_load(rendu)  # HA relit « None » comme None


def test_vigie_douze_pas_de_5_min_en_barres_5_puis_10_min():
    """Vigie (22/11/2026) : la PIAF de Météo-France au point GPS, 12 intensités de 5 min depuis
    le début des 5 min en cours (forme réelle de /v1/point, lue le 22/11/2026). Les barres gardent
    le pas de 5 min la première demi-heure, puis le plus fort de chaque paire (10 min)."""
    _, capteur = _capteur_pluie()
    t = int(MAINTENANT.timestamp()) + 120  # 12 h 02 : les 5 min en cours ont commencé à 12 h 00
    taux = [0.0] * 12
    taux[1] = 1.2   # 12 h 05 - 12 h 10 : faible
    taux[7] = 0.0   # 12 h 35 - 12 h 40 sec…
    taux[8] = 9.0   # …12 h 40 - 12 h 45 forte : la barre 35-45 min est forte
    rep = {"status": 200, "content": {"server_utc": t, "rain_nowcast": {"interval_minutes": 15,
                                                                       "rate_5m_mm_h": taux}}}
    serie = _serie("Vigie", rep)
    debut = int(MAINTENANT.timestamp())
    assert serie[0] == [debut, 5, 0.0] and len(serie) == 12, serie
    variables = {"source": "Vigie", "mf_pluie": "", "serie": serie}
    assert _rendre(capteur["state"], **variables) == f"@1,{debut + 300}"
    barres = _rendre(capteur["attributes"]["barres"], **variables)
    assert barres == "0|0;1|1;2|0;3|0;4|0;5|0;6|3;7|0;8|0;", barres
    # Hors de France, Vigie rend `rate_5m_mm_h` nul : pas de données, jamais « temps sec ».
    rep["content"]["rain_nowcast"]["rate_5m_mm_h"] = None
    assert _serie("Vigie", rep) is None
    assert _rendre(capteur["state"], source="Vigie", mf_pluie="", serie=None) == "@-1,0"
