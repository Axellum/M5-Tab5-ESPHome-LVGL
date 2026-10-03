# -*- coding: utf-8 -*-
"""Météo de Home Assistant sans Météo-France (02/10/2026, discussion #278 : un utilisateur
en Turquie, avec la seule météo que HA installe d'office, Met.no).

La chaîne est rendue ici avec les vrais modèles des packages et de fausses entités :
« Tab5 · sources météo » choisit l'entité météo (packages/tab5_meteo_sources.yaml),
« Tab5 Météo » dit ce qu'elle sait fournir, puis la poussée complète
(packages/tab5_push.yaml) demande les prévisions et compose les payloads des 15 jours,
des 10 heures, de la météo du moment et des probabilités.

Les prévisions sont la vraie réponse de Met.no pour Istanbul le 02/10/2026 vers 20 h 30,
passée par le code de l'intégration de HA 2026.9.4 (met/coordinator.py :
get_forecast(time_zone, False, 0) pour les jours, range_stop=49 pour les heures ;
PyMetno 0.13.0) : 6 jours, aujourd'hui compris, et 48 heures. Seules les clés lues par
les modèles sont gardées, avec les 10 premières heures (la poussée n'en lit pas plus)."""
import ast
import datetime as dt
import os
import re

import jinja2
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PAQUETS = os.path.join(REPO, "HomeAssistant_Config", "packages")
ISTANBUL = dt.timezone(dt.timedelta(hours=3))  # UTC+3 toute l'année depuis 2016
MAINTENANT = dt.datetime(2026, 10, 2, 20, 30, tzinfo=ISTANBUL)
# Entité créée par l'accueil de HA (appareil « Forecast » + nom du domicile).
MET_NO = "weather.forecast_home"

JOURS = [
    {"datetime": "2026-10-02T09:00:00+00:00", "condition": "rainy", "temperature": 16.5, "templow": 15.6, "precipitation": 2.3},
    {"datetime": "2026-10-03T09:00:00+00:00", "condition": "sunny", "temperature": 19.2, "templow": 14.9, "precipitation": 1.5},
    {"datetime": "2026-10-04T09:00:00+00:00", "condition": "partlycloudy", "temperature": 19.2, "templow": 14.6, "precipitation": 0.0},
    {"datetime": "2026-10-05T09:00:00+00:00", "condition": "cloudy", "temperature": 19.8, "templow": 13.5, "precipitation": 0.0},
    {"datetime": "2026-10-06T09:00:00+00:00", "condition": "partlycloudy", "temperature": 19.7, "templow": 14.9, "precipitation": 0.1},
    {"datetime": "2026-10-07T09:00:00+00:00", "condition": "partlycloudy", "temperature": 20.7, "templow": 15.7, "precipitation": 0.0},
]
HEURES = [
    {"datetime": "2026-10-02T18:00:00+00:00", "condition": "rainy", "temperature": 16.2, "precipitation": 0.3},
    {"datetime": "2026-10-02T19:00:00+00:00", "condition": "rainy", "temperature": 15.6, "precipitation": 0.6},
    {"datetime": "2026-10-02T20:00:00+00:00", "condition": "rainy", "temperature": 16.0, "precipitation": 0.5},
    {"datetime": "2026-10-02T21:00:00+00:00", "condition": "rainy", "temperature": 16.3, "precipitation": 0.4},
    {"datetime": "2026-10-02T22:00:00+00:00", "condition": "rainy", "temperature": 15.9, "precipitation": 0.3},
    {"datetime": "2026-10-02T23:00:00+00:00", "condition": "rainy", "temperature": 15.6, "precipitation": 0.2},
    {"datetime": "2026-10-03T00:00:00+00:00", "condition": "rainy", "temperature": 15.8, "precipitation": 0.3},
    {"datetime": "2026-10-03T01:00:00+00:00", "condition": "rainy", "temperature": 15.0, "precipitation": 0.3},
    {"datetime": "2026-10-03T02:00:00+00:00", "condition": "partlycloudy", "temperature": 15.4, "precipitation": 0.0},
    {"datetime": "2026-10-03T03:00:00+00:00", "condition": "rainy", "temperature": 15.5, "precipitation": 0.2},
]


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _paquet(nom):
    with open(os.path.join(PAQUETS, nom), encoding="utf-8") as f:
        return yaml.load(f.read(), Loader=_Chargeur)


def _parcourir(noeud):
    """Tous les dict de l'arbre."""
    if isinstance(noeud, dict):
        yield noeud
        for v in noeud.values():
            yield from _parcourir(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _parcourir(v)


def _action(paquet, nom):
    return next(n for n in _parcourir(paquet) if n.get("action") == nom)


# ─────────────────────────────────────────────────────────────────────────────
# Imitation du Jinja de Home Assistant (même principe que test_tuiles_blueprint.py)
# ─────────────────────────────────────────────────────────────────────────────

class Etat:
    def __init__(self, entity_id, state, **attributes):
        self.entity_id = entity_id
        self.state = state
        self.attributes = attributes


class Etats:
    def __init__(self, etats):
        self.d = {e.entity_id: e for e in etats}

    def __call__(self, entity_id):
        e = self.d.get(entity_id)
        return e.state if e else "unknown"

    def __getitem__(self, entity_id):
        return self.d.get(entity_id)

    def __getattr__(self, domaine):
        """`states.weather` : les états d'un domaine."""
        if domaine.startswith("_") or domaine == "d":
            raise AttributeError(domaine)
        return [e for e in self.d.values() if e.entity_id.split(".")[0] == domaine]

    def attr(self, entity_id, nom):
        e = self.d.get(entity_id)
        return e.attributes.get(nom) if e else None

    def ajouter(self, etat):
        self.d[etat.entity_id] = etat


def _analyser(brut):
    """Comme Home Assistant (Template._parse_result) : un littéral Python devient sa
    valeur, une chaîne reste le texte rendu."""
    brut = brut.strip()
    try:
        valeur = ast.literal_eval(brut)
    except (ValueError, SyntaxError, TypeError, MemoryError):
        return brut
    return brut if isinstance(valeur, str) else valeur


def _as_datetime(valeur):
    """Filtre as_datetime de HA : un texte qui n'est pas une date donne None."""
    if isinstance(valeur, dt.datetime):
        return valeur
    try:
        return dt.datetime.fromisoformat(str(valeur))
    except ValueError:
        return None


_SANS_DEFAUT = object()


def _as_timestamp(valeur, defaut=_SANS_DEFAUT):
    """as_timestamp de HA (fonction et filtre) : une date ou un texte de date donne des
    secondes depuis 1970 ; sinon le défaut, ou une erreur sans défaut."""
    date = _as_datetime(valeur) if valeur is not None else None
    if date is None:
        if defaut is _SANS_DEFAUT:
            raise ValueError(f"as_timestamp : {valeur!r} n'est pas une date")
        return defaut
    return (date if date.tzinfo else date.replace(tzinfo=ISTANBUL)).timestamp()


def _environnement(etats, maintenant=MAINTENANT):
    env = ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined)
    env.globals.update(
        states=etats, state_attr=etats.attr, now=lambda: maintenant, timedelta=dt.timedelta,
        as_timestamp=_as_timestamp,
        # Seule Met.no est installée : aucune entité Météo-France, OpenWeatherMap, DWD…
        integration_entities=lambda domaine: [MET_NO] if domaine == "met" else [],
        device_id=lambda e: None, device_entities=lambda d: [],
        expand=lambda e: [etats[e]] if etats[e] else [],
        has_value=lambda e: etats(e) not in ("unknown", "unavailable"),
    )
    env.filters.update(as_datetime=_as_datetime, as_local=lambda d: d.astimezone(ISTANBUL),
                       as_timestamp=_as_timestamp, bitwise_and=lambda a, b: a & b)
    env.tests.update(match=lambda v, motif: bool(re.match(motif, str(v))))
    return env


def _rendre(env, valeur, contexte=None):
    if isinstance(valeur, str):
        if "{{" in valeur or "{%" in valeur:
            return _analyser(env.from_string(valeur).render(contexte or {}))
        return valeur
    if isinstance(valeur, list):
        return [_rendre(env, v, contexte) for v in valeur]
    if isinstance(valeur, dict):
        return {k: _rendre(env, v, contexte) for k, v in valeur.items()}
    return valeur


def _maison(choix="", uv_index=0.0, maintenant=MAINTENANT):
    """Une installation neuve : Met.no (état et attributs de la réponse réelle), les
    listes des packages sur leur premier choix, rien dans « Tab5 · entité météo
    choisie ». Puis « Tab5 · sources météo » et « Tab5 Météo » rendus dans l'ordre où
    HA les calcule, chacun lisant le précédent."""
    etats = Etats([
        Etat(MET_NO, "rainy", temperature=16.5, humidity=76, uv_index=uv_index,
             supported_features=3, friendly_name="Forecast Home"),
        Etat("input_text.tab5_meteo_previsions", choix),
        Etat("input_select.tab5_source_pluie", "Météo-France"),
        Etat("input_select.tab5_source_vigilance", "Météo-France"),
    ])
    env = _environnement(etats, maintenant)
    paquet = _paquet("tab5_meteo_sources.yaml")
    blocs = paquet["template"]

    bloc = next(b for b in blocs if any(c.get("unique_id") == "tab5_sources_meteo" for c in b.get("sensor", [])))
    variables = {}
    for nom, modele in bloc["variables"].items():
        variables[nom] = _rendre(env, modele, variables)
    capteur = bloc["sensor"][0]
    etats.ajouter(Etat("sensor.tab5_sources_meteo", _rendre(env, capteur["state"], variables),
                       **_rendre(env, capteur["attributes"], variables)))

    bloc = next(b for b in blocs if any(c.get("unique_id") == "tab5_meteo" for c in b.get("sensor", [])))
    capteur = next(c for c in bloc["sensor"] if c.get("unique_id") == "tab5_meteo")
    etats.ajouter(Etat("sensor.tab5_meteo", _rendre(env, capteur["state"]),
                       **_rendre(env, capteur["attributes"])))
    return etats, env, bloc["select"][0]


def _poussee(etats, env):
    """Variables de la poussée complète, puis réponses de weather.get_forecasts."""
    auto = next(a for a in _paquet("tab5_push.yaml")["automation"] if a.get("id") == "tab5_ha_hmi_updater")
    modeles = next(n["variables"] for n in _parcourir(auto["action"])
                   if isinstance(n.get("variables"), dict) and "type_jours" in n["variables"])
    contexte = {}
    for nom, modele in modeles.items():
        contexte[nom] = _rendre(env, modele, contexte)
    meteo = contexte["meteo"]
    contexte.update(daily_var={meteo: {"forecast": JOURS}}, hourly_var_tab5={meteo: {"forecast": HEURES}})
    return auto, contexte


def _entrees(payload):
    return [e.split("|") for e in payload.split(";") if e]


# ─────────────────────────────────────────────────────────────────────────────

def test_l_entite_met_no_est_prise_toute_seule():
    """Rien de choisi (ou une entité disparue) et pas de Météo-France : l'entité Met.no
    est prise, et « Tab5 · source des prévisions » la montre choisie."""
    for choix in ["", "unknown", "weather.disparue"]:
        etats, env, liste = _maison(choix)
        assert etats.attr("sensor.tab5_sources_meteo", "meteo") == MET_NO, choix
        for cle in ["meteo_france", "mf_pluie", "mf_vigilance", "mf_uv", "mf_gel", "mf_neige", "owm", "meteoalarm"]:
            assert etats.attr("sensor.tab5_sources_meteo", cle) == "", (choix, cle)
        assert _rendre(env, liste["options"]) == [MET_NO]
        assert _rendre(env, liste["state"]) == MET_NO


def test_tab5_meteo_dit_ce_que_met_no_sait_fournir():
    etats, _, _ = _maison()
    assert etats("sensor.tab5_meteo") == "rainy"
    attributs = etats["sensor.tab5_meteo"].attributes
    assert attributs["entite"] == MET_NO
    # supported_features = 3 (FORECAST_DAILY | FORECAST_HOURLY, met/weather.py).
    assert attributs["type_jours"] == "daily"
    assert attributs["heures_ok"] is True
    assert (attributs["temperature"], attributs["humidite"]) == (16.5, 76)
    # Sans capteur Météo-France : UV de l'entité, gel inconnu (0), neige selon la condition.
    assert (attributs["uv"], attributs["gel"], attributs["neige"]) == (0, 0, 0)
    etats, _, _ = _maison(uv_index=5.0)
    assert etats["sensor.tab5_meteo"].attributes["uv"] == 5


def test_seuls_des_types_geres_sont_demandes():
    etats, env = _maison()[:2]
    auto, contexte = _poussee(etats, env)
    demandes = [_rendre(env, n["data"]["type"], contexte) for n in _parcourir(auto["action"])
                if n.get("action") == "weather.get_forecasts"]
    assert demandes == ["daily", "hourly"]
    for n in _parcourir(auto["action"]):
        if n.get("action") == "weather.get_forecasts":
            assert _rendre(env, n["target"]["entity_id"], contexte) == MET_NO


def test_six_jours_de_met_no_puis_des_jours_vides():
    """Aujourd'hui et les 5 jours suivants remplis ; les 9 derniers des 15 jours
    marqués passés (« -- » estompé sur la tablette, tab5_forecast.cpp)."""
    etats, env = _maison()[:2]
    auto, contexte = _poussee(etats, env)
    payload = _rendre(env, _action(auto["action"], "esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk")["data"]["payload"], contexte)
    entrees = _entrees(payload)
    assert len(entrees) == 15
    noms = ["Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"]
    for i, (indice, nom, condition, tmin, tmax, repos, dimanche, passe, heures) in enumerate(entrees):
        jour = MAINTENANT + dt.timedelta(days=i)
        assert indice == str(i)
        assert nom == ("Auj" if i == 0 else noms[int(jour.strftime("%w"))])
        assert dimanche == ("1" if jour.strftime("%w") == "0" else "0")
        assert (repos, heures) == ("1", ""), i  # pas d'agenda de travail choisi
        if i < len(JOURS):
            prevu = JOURS[i]
            assert (condition, float(tmin), float(tmax), passe) == (
                prevu["condition"], prevu["templow"], prevu["temperature"], "0"), i
        else:
            assert (condition, tmin, tmax, passe) == ("unknown", "0", "0", "1"), i


def test_dix_heures_de_met_no_a_l_heure_locale():
    etats, env = _maison()[:2]
    auto, contexte = _poussee(etats, env)
    action = _action(auto["action"], "esphome.tab5_ha_hmi_tab5_maj_previsions_heures_bulk")
    entrees = []
    for index in (1, 2):
        entrees += _entrees(_rendre(env, action["data"]["payload"], dict(contexte, repeat={"index": index})))
    assert len(entrees) == 10
    for i, (indice, heure, condition, temp, pluie) in enumerate(entrees):
        prevu = HEURES[i]
        locale = dt.datetime.fromisoformat(prevu["datetime"]).astimezone(ISTANBUL)
        assert indice == str(i)
        assert heure == locale.strftime("%H:00"), i  # 21:00 pour 18:00 UTC
        assert (condition, float(temp), float(pluie)) == (prevu["condition"], prevu["temperature"], prevu["precipitation"]), i


def test_meteo_du_moment_et_probabilites():
    etats, env = _maison()[:2]
    paquet = _paquet("tab5_push.yaml")
    actuelle = _rendre(env, _action(paquet, "esphome.tab5_ha_hmi_tab5_maj_meteo_actuelle")["data"])
    assert actuelle == {"condition": "rainy", "temperature": 16.5, "humidite": 76.0}
    probabilites = _rendre(env, _action(paquet, "esphome.tab5_ha_hmi_tab5_maj_probabilites")["data"])
    assert probabilites == {"uv": 0, "gel": 0, "neige": 0}
