# -*- coding: utf-8 -*-
"""Pluie dans l'heure sans Météo-France (02/10/2026, discussion #278 : un utilisateur en
Turquie n'avait rien sur la carte pluie). Météo-France est le premier choix de la liste
« Tab5 · next-hour rain source », donc celui d'une installation neuve ; sans
capteur de pluie Météo-France, la source effective devient Vigie (Open-Meteo jusqu'au
24/11/2026, ADR-0084), qui répond partout : la PIAF de Météo-France en France, le modèle
Open-Meteo ailleurs.

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


def test_meteo_france_sans_capteur_passe_sur_vigie():
    assert _source("Météo-France", "") == "Vigie"


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


def test_carte_pluie_remplie_par_vigie_sans_meteo_france():
    _, capteur = _capteur_pluie()
    source = _source("Météo-France", "")
    debut = int(MAINTENANT.timestamp()) + 600
    # Série commune [début, durée en min, mm/h] : 3 mm/h dans 10 min, pluie modérée.
    serie = [[debut - 900, 15, 0.0], [debut, 15, 3.0]]
    variables = {"source": source, "mf_pluie": "", "serie": serie,
                 "source_lue": _source_lue(source, serie)}
    assert _rendre(capteur["state"], **variables) == f"@2,{debut}"
    barres = _rendre(capteur["attributes"]["barres"], **variables)
    assert barres.startswith("0|0;1|0;2|2;"), barres
    assert _rendre(capteur["attributes"]["source"], **variables) == "Vigie"
    # Pas de réponse de Vigie : « pas de données », plus la carte masquée (@-).
    assert _rendre(capteur["state"], source=source, mf_pluie="", serie=None) == "@-1,0"


def _actions_pluie():
    with open(PAQUET, encoding="utf-8") as f:
        paquet = yaml.safe_load(f)
    for bloc in paquet["template"]:
        if any(c.get("unique_id") == "tab5_pluie_dans_l_heure" for c in bloc.get("sensor", [])):
            return bloc["actions"]
    raise AssertionError("actions du capteur de pluie introuvables")


def _env_ha():
    env = ImmutableSandboxedEnvironment(undefined=jinja2.Undefined)
    env.filters["float"] = lambda v, d=0.0: float(v) if v is not None else d
    env.filters["int"] = lambda v, d=0: int(v) if v is not None else d
    env.globals.update(now=lambda: MAINTENANT)
    return env


def _sans_absents(**variables):
    """Une variable à None est absente, comme une réponse de rest_command en erreur
    (continue_on_error : `response_variable` n'est pas posée)."""
    return {k: v for k, v in variables.items() if v is not None}


def _serie(source, rep, rep_repli=None):
    """La variable `serie` des actions du capteur, rendue avec une réponse de rest_command
    (et celle du repli Open-Meteo de Vigie quand il a été appelé)."""
    modele = next(a["variables"]["serie"] for a in _actions_pluie() if "variables" in a)
    rendu = _env_ha().from_string(modele).render(
        **_sans_absents(source=source, rep=rep, rep_repli=rep_repli, owm_meteo="")).strip()
    return None if rendu == "None" else yaml.safe_load(rendu)  # HA relit « None » comme None


def _source_lue(source, serie, rep_repli=None):
    """La variable `source_lue` (attribut `source` du capteur)."""
    modele = next(a["variables"]["source_lue"] for a in _actions_pluie() if "variables" in a)
    return _env_ha().from_string(modele).render(
        serie=serie, **_sans_absents(source=source, rep_repli=rep_repli)).strip()  # serie à None reste définie


def _repli_appele(source, rep, lat=35.68, lon=139.69):
    """La condition du repli Open-Meteo de Vigie (l'action dont la réponse est `rep_repli`)."""
    for action in _actions_pluie():
        suite = action.get("then") or []
        if any(a.get("response_variable") == "rep_repli" for a in suite):
            assert suite[0]["data"]["source"] == "Open-Meteo", suite
            modele = action["if"][0]["value_template"]
            rendu = _env_ha().from_string(modele).render(**_sans_absents(source=source, rep=rep, lat=lat, lon=lon))
            return rendu.strip() == "True"
    raise AssertionError("repli Open-Meteo introuvable")


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
    # Ni pas de 5 min ni pas de 15 min : pas de données, jamais « temps sec ».
    rep["content"]["rain_nowcast"]["rate_5m_mm_h"] = None
    assert _serie("Vigie", rep) is None
    assert _rendre(capteur["state"], source="Vigie", mf_pluie="", serie=None) == "@-1,0"


def test_vigie_hors_de_france_quatre_pas_de_15_min():
    """Hors de France (24/11/2026, ADR-0084), Vigie rend `rate_5m_mm_h` nul et
    `mm_per_step` = 4 sommes de 15 min (mm) du modèle Open-Meteo, sans leur heure (forme
    réelle de /v1/point, Tokyo, lue le 24/11/2026). Datées comme la source Open-Meteo : les
    heures d'Open-Meteo partent du quart d'heure en cours (`server_utc` // 900 × 900) et
    chaque valeur est la pluie des 15 min qui précèdent."""
    _, capteur = _capteur_pluie()
    t = int(MAINTENANT.timestamp()) + 120  # 12 h 02 : le quart d'heure en cours a commencé à 12 h 00
    rep = {"status": 200, "content": {"server_utc": t, "rain_nowcast": {
        "interval_minutes": 15, "rate_5m_mm_h": None, "mm_per_step": [0.0, 0.5, 2.0, 0.0]}}}
    serie = _serie("Vigie", rep)
    debut = int(MAINTENANT.timestamp())
    assert serie == [[debut - 900, 15, 0.0], [debut, 15, 2.0], [debut + 900, 15, 8.0],
                     [debut + 1800, 15, 0.0]], serie
    variables = {"source": "Vigie", "mf_pluie": "", "serie": serie}
    # 2 mm/h de 12 h 00 à 12 h 15 : pluie faible en cours ; 8 mm/h de 12 h 15 à 12 h 30 : forte.
    assert _rendre(capteur["state"], **variables) == "@1,0"
    barres = _rendre(capteur["attributes"]["barres"], **variables)
    assert barres == "0|1;1|1;2|1;3|3;4|3;5|3;6|0;7|0;8|0;", barres
    # Même série que la source Open-Meteo pour la même pluie : l'écran ne change pas.
    om = {"status": 200, "content": {"minutely_15": {
        "time": [debut, debut + 900, debut + 1800, debut + 2700], "precipitation": [0.0, 0.5, 2.0, 0.0]}}}
    assert _serie("Open-Meteo", om) == serie


def _vigie(rate=None, quarts=None, status=200, t=None, debut=None):
    """Réponse de rest_command pour Vigie (forme réelle de /v1/point ; `debut` =
    `rain_nowcast.start_utc`, absent des réponses d'avant le 24/11/2026)."""
    t = int(MAINTENANT.timestamp()) + 120 if t is None else t
    nowcast = {"interval_minutes": 15, "rate_5m_mm_h": rate, "mm_per_step": quarts}
    if debut is not None:
        nowcast["start_utc"] = debut
    return {"status": status, "content": {"server_utc": t, "rain_nowcast": nowcast}}


def test_vigie_quinze_minutes_datees_par_start_utc():
    """Vigie garde la réponse d'Open-Meteo 15 min sans la caler sur les quarts d'heure
    (constaté le 24/11/2026 sur Tokyo, doc de vigie-api#123) : à 12 h 02, sa copie peut dater
    de 11 h 58, ses pas partant alors de 11 h 30 et non de 11 h 45. `start_utc` donne ce début ;
    sans lui, la règle de `server_utc` reste ; une valeur folle (à plus d'une heure) est ignorée."""
    debut = int(MAINTENANT.timestamp())  # 12 h 00
    quarts = [0.0, 0.5, 2.0, 0.0]
    attendu = [[debut - 1800, 15, 0.0], [debut - 900, 15, 2.0], [debut, 15, 8.0], [debut + 900, 15, 0.0]]
    assert _serie("Vigie", _vigie(None, quarts, debut=debut - 1800)) == attendu
    assert _serie("Vigie", _vigie(None, quarts, debut=debut - 900)) == _serie("Vigie", _vigie(None, quarts))
    regle = _serie("Vigie", _vigie(None, quarts))
    assert regle[0][0] == debut - 900, regle
    for fou in (0, None, debut - 7200, debut + 7200):
        assert _serie("Vigie", _vigie(None, quarts, debut=fou)) == regle, fou
    # Le début ne joue que sur les pas de 15 min : les 12 pas de 5 min (PIAF) restent sur
    # les 5 min en cours de `server_utc`, et le repli ne change pas.
    assert _serie("Vigie", _vigie([0.0] * 12, debut=debut - 1800))[0][0] == debut
    assert not _repli_appele("Vigie", _vigie(None, quarts, debut=debut - 1800))


def _open_meteo(mm):
    debut = int(MAINTENANT.timestamp())
    return {"status": 200, "content": {"minutely_15": {
        "time": [debut + 900 * i for i in range(len(mm))], "precipitation": mm}}}


def test_repli_open_meteo_seulement_quand_vigie_manque():
    """Repli (24/11/2026, ADR-0085) : un second appel, à Open-Meteo, seulement quand Vigie ne
    répond pas (`rep` absent : délai dépassé, erreur réseau), répond autre chose que 200, ou
    n'a ni pas de 5 min ni pas de 15 min. Jamais pour les autres sources, ni sans domicile."""
    assert _repli_appele("Vigie", None)                                   # muette
    assert _repli_appele("Vigie", _vigie([0.0] * 12, status=503))        # en panne
    assert _repli_appele("Vigie", _vigie())                               # ni l'un ni l'autre
    assert _repli_appele("Vigie", _vigie([], []))                          # listes vides
    assert _repli_appele("Vigie", _vigie([0.0] * 12, t=0))                # sans heure
    assert _repli_appele("Vigie", {"status": 200, "content": "<html>"})  # pas du JSON
    assert not _repli_appele("Vigie", _vigie([0.0] * 12))                 # France : PIAF
    assert not _repli_appele("Vigie", _vigie(None, [0.0, 0.5, 2.0, 0.0]))  # ailleurs : 15 min
    assert not _repli_appele("Vigie", _vigie([], [0.0, 0.5, 2.0, 0.0]))
    assert not _repli_appele("Open-Meteo", None)
    assert not _repli_appele("Met.no", {"status": 500, "content": ""})
    assert not _repli_appele("Vigie", None, lat=None)


def test_repli_open_meteo_remplit_la_carte_et_le_dit():
    """Vigie muette, Open-Meteo répond : la série est celle de la source Open-Meteo et
    l'attribut `source` dit « Open-Meteo » ; muet aussi : pas de données, `source` = Vigie."""
    _, capteur = _capteur_pluie()
    om = _open_meteo([0.0, 0.5, 2.0, 0.0])
    serie = _serie("Vigie", None, rep_repli=om)
    assert serie == _serie("Open-Meteo", om) and serie, serie
    assert _source_lue("Vigie", serie, rep_repli=om) == "Open-Meteo"
    variables = {"source": "Vigie", "mf_pluie": "", "serie": serie, "source_lue": "Open-Meteo"}
    assert _rendre(capteur["state"], **variables) == "@1,0"
    assert _rendre(capteur["attributes"]["source"], **variables) == "Open-Meteo"
    # Le repli a répondu, mais mal (Open-Meteo en panne) : rien, et la source reste Vigie.
    panne = {"status": 502, "content": ""}
    assert _serie("Vigie", _vigie(), rep_repli=panne) is None
    assert _source_lue("Vigie", None, rep_repli=panne) == "Vigie"
    # Sans repli : la source effective, telle quelle.
    assert _source_lue("Vigie", [[0, 5, 0.0]]) == "Vigie"
    assert _source_lue("Buienradar", None) == "Buienradar"


def test_vigie_liste_de_5_min_vide_lit_les_15_min():
    """Une liste de pas de 5 min vide ne cache plus les pas de 15 min (même règle que le repli)."""
    serie = _serie("Vigie", _vigie([], [0.0, 0.5, 2.0, 0.0]))
    assert serie == _serie("Vigie", _vigie(None, [0.0, 0.5, 2.0, 0.0])) and len(serie) == 4, serie