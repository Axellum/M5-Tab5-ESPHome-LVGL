# -*- coding: utf-8 -*-
"""Icônes de nuit des prévisions heure par heure (discussion #278, 03/10/2026 : « night
time cloudy icon is with sun », un utilisateur en Turquie avec Met.no).

Met.no range « beau » et « peu nuageux » de nuit (fair_night, partlycloudy_night) sous
`partlycloudy` (homeassistant/components/met/const.py, CONDITIONS_MAP) : seul
clearsky_night devient `clear-night`, et ses prévisions horaires n'ont pas d'is_daytime
(met/weather.py, FORECAST_MAP). La poussée (packages/tab5_push.yaml) envoie donc, pour
un créneau de nuit, la variante que la tablette sait déjà dessiner (kMeteoIcons,
Tab5/ecran/tab5_forecast.cpp) : `partlycloudy-night` (nuage + lune), `clear-night` (lune).

Le modèle réel est rendu avec les fausses entités de test_meteo_sans_meteo_france.py,
puis comparé à un calcul indépendant : au lieu de reporter les prochains lever et
coucher de 24 h en 24 h comme le modèle, il lit une table de levers et couchers jour
par jour (qui dérivent de quelques minutes par jour, comme en octobre) et regarde si
le début du créneau tombe entre un lever et un coucher. Les heures de la table sont
fictives, proches de celles d'Istanbul début octobre."""
import datetime as dt

import pytest

from tests.test_meteo_sans_meteo_france import (
    ISTANBUL, Etat, _action, _entrees, _maison, _poussee, _rendre,
)

ACTION_HEURES = "esphome.tab5_ha_hmi_tab5_maj_previsions_heures_bulk"
ACTION_JOURS = "esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk"
UTC = dt.timezone.utc

# Lever et coucher du 30/09 au 07/10/2026 (heure d'Istanbul) : le lever recule d'une
# minute par jour, le coucher avance d'une minute et demie.
SOLEIL = [
    (dt.datetime(2026, 10, 2, 7, 5, tzinfo=ISTANBUL) + dt.timedelta(days=n, minutes=n),
     dt.datetime(2026, 10, 2, 18, 40, tzinfo=ISTANBUL) + dt.timedelta(days=n, seconds=-90 * n))
    for n in range(-2, 6)
]
CONDITIONS = ["partlycloudy", "sunny", "rainy", "partlycloudy", "cloudy", "clear-night", "Clear", "partlycloudy"]
NUIT = {"partlycloudy": "partlycloudy-night", "sunny": "clear-night", "Clear": "clear-night"}


def _sun(maintenant):
    """sun.sun tel que HA le tient : prochains lever et coucher (en UTC), au-dessus ou
    au-dessous de l'horizon selon le dernier des deux."""
    levers = [lv for lv, _ in SOLEIL]
    couchers = [co for _, co in SOLEIL]
    lever = min(t for t in levers if t > maintenant)
    coucher = min(t for t in couchers if t > maintenant)
    etat = "below_horizon" if coucher > lever else "above_horizon"
    return Etat("sun.sun", etat, next_rising=lever.astimezone(UTC).isoformat(),
                next_setting=coucher.astimezone(UTC).isoformat(), friendly_name="Sun")


def _creneaux(debut, pas_h=1, n=10, **extra):
    """Prévisions horaires à la forme de Met.no (datetime en UTC, sans is_daytime)."""
    return [dict({"datetime": (debut + dt.timedelta(hours=pas_h * k)).astimezone(UTC).isoformat(),
                  "condition": CONDITIONS[k % len(CONDITIONS)], "temperature": 15.0 + k,
                  "precipitation": 0.0}, **extra)
            for k in range(n)]


def _payload_heures(maintenant, creneaux, soleil=True):
    etats, env = _maison(maintenant=maintenant)[:2]
    if soleil:
        etats.ajouter(_sun(maintenant))
    auto, contexte = _poussee(etats, env)
    contexte["hourly_var_tab5"] = {contexte["meteo"]: {"forecast": creneaux}}
    action = _action(auto["actions"], ACTION_HEURES)
    entrees = []
    for index in (1, 2):
        entrees += _entrees(_rendre(env, action["data"]["payload"], dict(contexte, repeat={"index": index})))
    return entrees


def _attendu(creneau, soleil=True):
    """Calcul indépendant : is_daytime s'il est fourni, sinon la table jour par jour."""
    condition = creneau["condition"]
    de_jour = creneau.get("is_daytime")
    if not isinstance(de_jour, bool):
        if not soleil:
            return condition
        t = dt.datetime.fromisoformat(creneau["datetime"])
        de_jour = any(lever <= t < coucher for lever, coucher in SOLEIL)
    return condition if de_jour else NUIT.get(condition, condition)


def _heures_et_conditions(entrees):
    return {heure: condition for _, heure, condition, _, _ in entrees}


SCENARIOS = {
    # maintenant (Istanbul), premier créneau, pas en heures
    "après-midi, jusqu'au coucher et au-delà": (dt.datetime(2026, 10, 2, 13, 30, tzinfo=ISTANBUL), 13, 1),
    "fin de nuit, autour du lever": (dt.datetime(2026, 10, 3, 3, 30, tzinfo=ISTANBUL), 3, 1),
    "soirée, après minuit": (dt.datetime(2026, 10, 2, 20, 30, tzinfo=ISTANBUL), 20, 1),
    "fin d'après-midi, coucher puis minuit": (dt.datetime(2026, 10, 2, 16, 30, tzinfo=ISTANBUL), 16, 1),
    "pas de 3 h sur 30 h, le lendemain compris": (dt.datetime(2026, 10, 3, 5, 30, tzinfo=ISTANBUL), 6, 3),
}


@pytest.mark.parametrize("nom", SCENARIOS)
def test_le_modele_suit_le_calcul_independant(nom):
    maintenant, heure, pas = SCENARIOS[nom]
    creneaux = _creneaux(maintenant.replace(hour=heure, minute=0), pas)
    entrees = _payload_heures(maintenant, creneaux)
    assert len(entrees) == 10
    for creneau, (_, _, condition, _, _) in zip(creneaux, entrees):
        assert condition == _attendu(creneau), (nom, creneau)


def test_quelques_creneaux_a_la_main():
    """Les mêmes règles dites en clair, sans la table : lever vers 7 h 05, coucher
    vers 18 h 40."""
    maintenant = dt.datetime(2026, 10, 3, 3, 30, tzinfo=ISTANBUL)
    creneaux = _creneaux(maintenant.replace(minute=0))
    vu = _heures_et_conditions(_payload_heures(maintenant, creneaux))
    assert vu["03:00"] == "partlycloudy-night"  # nuit
    assert vu["04:00"] == "clear-night"         # « sunny » de nuit : la lune
    assert vu["05:00"] == "rainy"               # pas de variante de nuit : inchangé
    assert vu["06:00"] == "partlycloudy-night"
    assert vu["08:00"] == "clear-night"         # « clear-night » reste la lune
    assert vu["09:00"] == "Clear"               # jour : inchangé
    assert vu["10:00"] == "partlycloudy"        # jour : le soleil

    maintenant = dt.datetime(2026, 10, 2, 16, 30, tzinfo=ISTANBUL)
    vu = _heures_et_conditions(_payload_heures(maintenant, _creneaux(maintenant.replace(minute=0))))
    assert vu["16:00"] == "partlycloudy"        # créneau en cours, encore de jour
    assert vu["17:00"] == "sunny"
    assert vu["19:00"] == "partlycloudy-night"  # après le coucher
    assert vu["22:00"] == "clear-night"         # « Clear » la nuit : la lune
    assert vu["00:00"] == "partlycloudy-night"  # après minuit


def test_creneau_du_lever():
    """Le créneau de 7 h commence avant le lever (7 h 06) : nuit ; celui de 8 h : jour."""
    maintenant = dt.datetime(2026, 10, 3, 6, 30, tzinfo=ISTANBUL)
    creneaux = [dict(c, condition="partlycloudy") for c in _creneaux(maintenant.replace(minute=0))]
    vu = _heures_et_conditions(_payload_heures(maintenant, creneaux))
    assert (vu["06:00"], vu["07:00"], vu["08:00"]) == ("partlycloudy-night", "partlycloudy-night", "partlycloudy")
    assert vu["15:00"] == "partlycloudy"


def test_is_daytime_du_creneau_passe_avant_le_soleil():
    """Un fournisseur qui dit jour ou nuit (is_daytime, NWS…) l'emporte sur sun.sun ; un
    is_daytime nul ou absent laisse décider le soleil."""
    maintenant = dt.datetime(2026, 10, 2, 13, 30, tzinfo=ISTANBUL)
    debut = maintenant.replace(minute=0)
    creneaux = [dict(c, condition="partlycloudy") for c in _creneaux(debut)]
    creneaux[0]["is_daytime"] = False  # 13 h, de jour pour le soleil
    creneaux[8]["is_daytime"] = True   # 21 h, de nuit pour le soleil
    creneaux[9]["is_daytime"] = None   # 22 h : le soleil décide
    entrees = _payload_heures(maintenant, creneaux)
    vu = _heures_et_conditions(entrees)
    assert (vu["13:00"], vu["14:00"], vu["21:00"], vu["22:00"]) == (
        "partlycloudy-night", "partlycloudy", "partlycloudy", "partlycloudy-night")
    for creneau, (_, _, condition, _, _) in zip(creneaux, entrees):
        assert condition == _attendu(creneau)


def test_is_daytime_sans_sun_sun():
    maintenant = dt.datetime(2026, 10, 2, 20, 30, tzinfo=ISTANBUL)
    creneaux = _creneaux(maintenant.replace(minute=0), is_daytime=False)
    entrees = _payload_heures(maintenant, creneaux, soleil=False)
    assert [e[2] for e in entrees] == [_attendu(c, soleil=False) for c in creneaux]
    assert entrees[0][2] == "partlycloudy-night"


def test_sans_sun_sun_la_condition_brute():
    """Intégration Soleil retirée : rien ne change, et aucune erreur de modèle."""
    maintenant = dt.datetime(2026, 10, 2, 20, 30, tzinfo=ISTANBUL)
    creneaux = _creneaux(maintenant.replace(minute=0))
    entrees = _payload_heures(maintenant, creneaux, soleil=False)
    assert [e[2] for e in entrees] == [c["condition"] for c in creneaux]


@pytest.mark.parametrize("etat, attendu", [("below_horizon", "partlycloudy-night"), ("above_horizon", "partlycloudy")])
def test_nuit_ou_jour_polaire(etat, attendu):
    """Prochain lever et coucher dans des semaines : l'état de sun.sun vaut pour tous."""
    maintenant = dt.datetime(2026, 12, 20, 12, 0, tzinfo=ISTANBUL)
    etats, env = _maison(maintenant=maintenant)[:2]
    loin = (maintenant + dt.timedelta(days=24)).astimezone(UTC)
    etats.ajouter(Etat("sun.sun", etat, next_rising=loin.isoformat(),
                       next_setting=(loin + dt.timedelta(hours=1)).isoformat()))
    auto, contexte = _poussee(etats, env)
    creneaux = [dict(c, condition="partlycloudy") for c in _creneaux(maintenant)]
    contexte["hourly_var_tab5"] = {contexte["meteo"]: {"forecast": creneaux}}
    action = _action(auto["actions"], ACTION_HEURES)
    payload = _rendre(env, action["data"]["payload"], dict(contexte, repeat={"index": 1}))
    assert [e[2] for e in _entrees(payload)] == [attendu] * 5


def test_les_previsions_par_jour_ne_changent_pas():
    """La nuit, un jour « partlycloudy » reste le soleil du jour (prévision de jour)."""
    maintenant = dt.datetime(2026, 10, 2, 20, 30, tzinfo=ISTANBUL)
    rendus = []
    for soleil in (False, True):
        etats, env = _maison(maintenant=maintenant)[:2]
        if soleil:
            etats.ajouter(_sun(maintenant))
        auto, contexte = _poussee(etats, env)
        rendus.append(_rendre(env, _action(auto["actions"], ACTION_JOURS)["data"]["payload"], contexte))
    assert rendus[0] == rendus[1]
    assert "night" not in rendus[1]
