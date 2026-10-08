# -*- coding: utf-8 -*-
"""Jour travaillé des tuiles météo et du réveil (audit du 07/10/2026, HA-3).

La poussée complète (packages/tab5_push.yaml, prévisions jours) et le réveil
(packages/tab5_reveil.yaml) lisent les événements de calendar.get_events par les macros
de custom_templates/tab5_calendar.jinja, comme le popup calendrier : un jour est
travaillé si un événement de travail le couvre — le jour du début pour un événement
horodaté, l'intervalle [début, fin[ pour une journée entière (HA rend la fin d'une
journée entière exclusive : un événement du 3 seul finit le 4). Avant, un test de
sous-chaîne sur le début OU la fin comptait aussi le lendemain d'une journée entière.

Les vrais modèles sont rendus avec le harnais de tests/test_meteo_sans_meteo_france.py
(Met.no à Istanbul, le 02/10/2026 à 20 h 30) ; l'attendu est calculé à part, en Python.
"""
from __future__ import annotations

import datetime as dt

from tests.test_meteo_sans_meteo_france import (
    ISTANBUL, MAINTENANT, _action, _entrees, _maison, _paquet, _poussee, _rendre)

AGENDA = "calendar.travail"
JOURS = "esphome.tab5_ha_hmi_tab5_maj_previsions_jours_bulk"

EVENEMENTS = [
    # Journée entière du 03 seul : le 04 (sa fin) n'est PAS travaillé.
    {"start": "2026-10-03", "end": "2026-10-04", "summary": "Travail"},
    # Journée entière du 05 au 07 inclus : le 06 (milieu) l'est, le 08 (fin) non.
    {"start": "2026-10-05", "end": "2026-10-08", "summary": "Travail"},
    # Horodaté de nuit, du 09 à 22 h au 10 à 6 h : le 09 seulement, avec ses heures.
    {"start": "2026-10-09T22:00:00+03:00", "end": "2026-10-10T06:00:00+03:00", "summary": "Travail"},
    # Ancien format (start_time / message) : même lecture.
    {"start_time": "2026-10-12T08:30:00+03:00", "end_time": "2026-10-12T17:00:00+03:00", "message": "Travail"},
    # Pas un événement de travail (mot « travail » demandé) : ignoré.
    {"start": "2026-10-13T09:00:00+03:00", "end": "2026-10-13T10:00:00+03:00", "summary": "Dentiste"},
]


def _couvre(e, jour: dt.date) -> bool:
    """Calcul indépendant : jour du début (horodaté), [début, fin[ (journée entière)."""
    debut = e.get("start") or e.get("start_time")
    fin = e.get("end") or e.get("end_time")
    if "T" in debut:
        return dt.datetime.fromisoformat(debut).date() == jour
    return dt.date.fromisoformat(debut) <= jour < dt.date.fromisoformat(fin)


def _payload_jours(evenements, mots=("travail",)):
    etats, env = _maison()[:2]
    auto, contexte = _poussee(etats, env)
    contexte.update(agenda=AGENDA, mots_travail=list(mots),
                    agenda_events={AGENDA: {"events": evenements}})
    return _entrees(_rendre(env, _action(auto["actions"], JOURS)["data"]["payload"], contexte))


def test_jour_de_fin_d_une_journee_entiere_exclu():
    entrees = _payload_jours(EVENEMENTS)
    assert len(entrees) == 15
    for i, (_indice, _nom, _cond, _tmin, _tmax, repos, _dim, _passe, _heures) in enumerate(entrees):
        jour = (MAINTENANT + dt.timedelta(days=i)).date()
        travail = any(_couvre(e, jour) for e in EVENEMENTS if "travail" in (e.get("summary") or e.get("message")).lower())
        assert repos == ("0" if travail else "1"), (i, jour)
    par_jour = {(MAINTENANT + dt.timedelta(days=i)).date().isoformat(): e for i, e in enumerate(entrees)}
    # Les frontières, écrites en clair.
    assert par_jour["2026-10-03"][5] == "0" and par_jour["2026-10-04"][5] == "1"
    assert [par_jour[j][5] for j in ("2026-10-05", "2026-10-06", "2026-10-07", "2026-10-08")] == ["0", "0", "0", "1"]
    assert (par_jour["2026-10-09"][5], par_jour["2026-10-09"][8]) == ("0", "22:00-06:00")
    assert (par_jour["2026-10-10"][5], par_jour["2026-10-10"][8]) == ("1", "")
    assert (par_jour["2026-10-12"][5], par_jour["2026-10-12"][8]) == ("0", "08:30-17:00")
    assert par_jour["2026-10-13"][5] == "1"


def test_sans_mot_tout_l_agenda_est_du_travail():
    par_jour = {(MAINTENANT + dt.timedelta(days=i)).date().isoformat(): e
                for i, e in enumerate(_payload_jours(EVENEMENTS, mots=()))}
    assert (par_jour["2026-10-13"][5], par_jour["2026-10-13"][8]) == ("0", "09:00-10:00")
    assert par_jour["2026-10-04"][5] == "1"


def test_reveil_lit_les_evenements_par_les_macros():
    """Rendez-vous poussés au réveil : les deux formats d'événement, journée entière
    sautée, événement de travail exclu."""
    etats, env = _maison()[:2]
    script = _paquet("tab5_reveil.yaml")["script"]["tab5_rdv_prochains"]
    modele = _action(script["sequence"], "esphome.tab5_ha_hmi_tab5_maj_rdv_prochains")["data"]["payload"]
    rdv = "calendar.famille"
    ev = {rdv: {"events": [
        {"start": "2026-10-03T09:15:00+03:00", "end": "2026-10-03T10:00:00+03:00", "summary": "Dentiste | contrôle"},
        {"start_time": "2026-10-03T07:00:00+03:00", "end_time": "2026-10-03T08:00:00+03:00", "message": "Piscine"},
        {"start": "2026-10-03", "end": "2026-10-04", "summary": "Anniversaire"},
        {"start": "2026-10-03T08:00:00+03:00", "end": "2026-10-03T17:00:00+03:00", "summary": "Travail"},
    ]}}
    sortie = _rendre(env, modele, {"ev": ev, "ag_rdv": rdv, "ag_travail": "", "mots_travail": ["travail"]})
    epoch = lambda h, m: int(dt.datetime(2026, 10, 3, h, m, tzinfo=ISTANBUL).timestamp())  # noqa: E731
    assert sortie == f"{epoch(7, 0):011d}|Piscine~{epoch(9, 15):011d}|Dentiste / contrôle"
