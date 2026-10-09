# -*- coding: utf-8 -*-
"""Statistiques longues de fiabilité (lot K, 09/10/2026), packages/tab5_health.yaml.

Le recorder de HA ne garde les états que quelques jours ; « Tab5 Uptime » (horodatage) et
« Raison du redémarrage » (texte) n'ont pas de statistiques. Trois capteurs à state_class
les remplacent : « Tab5 · redémarrages », « Tab5 · redémarrages inattendus » (comptés par
l'événement tab5_sante_redemarrage de la garde (b), même classement que sa notification)
et « Tab5 · fonctionnement continu » (heures depuis le dernier démarrage, sans now()).
Ce test tient le câblage et rend les modèles comme HA ; la validation par le moteur de
modèles de HA (POST /api/template) est décrite dans la PR."""
import datetime as dt

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

from tests.commun import ChargeurSansBalises as _Chargeur, lire

EVENEMENT = "tab5_sante_redemarrage"


def _sante():
    return yaml.load(lire("HomeAssistant_Config", "packages", "tab5_health.yaml"), Loader=_Chargeur)


def _bloc_et_capteur(unique_id):
    for bloc in _sante()["template"]:
        for capteur in bloc.get("sensor") or []:
            if capteur.get("unique_id") == unique_id:
                return bloc, capteur
    raise AssertionError(f"capteur {unique_id} introuvable dans tab5_health.yaml")


def _env():
    env = ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined)

    def as_timestamp(valeur, defaut=None):
        if isinstance(valeur, dt.datetime):
            return valeur.timestamp()
        try:
            return dt.datetime.fromisoformat(str(valeur)).timestamp()
        except ValueError:
            return defaut

    env.globals["as_timestamp"] = as_timestamp
    return env


# ─── Garde (b) : un événement par redémarrage reconnu, avant le filtre « demandé » ───

def test_la_garde_b_emet_l_evenement_avant_de_filtrer():
    auto = next(a for a in _sante()["automation"] if a["id"] == "tab5_health_unexpected_reboot")
    actions = auto["actions"]
    i_evt = next(i for i, a in enumerate(actions) if a.get("event") == EVENEMENT)
    i_var = next(i for i, a in enumerate(actions) if "variables" in a and "demande" in a["variables"])
    i_cond = next(i for i, a in enumerate(actions) if a.get("value_template") == "{{ not demande }}")
    # Après le classement (variable demande), avant la condition qui arrête les demandés.
    assert i_var < i_evt < i_cond
    modele = actions[i_evt]["event_data"]["inattendu"]
    assert _env().from_string(modele).render(demande=True) == "non"
    assert _env().from_string(modele).render(demande=False) == "oui"


# ─── Compteurs ───────────────────────────────────────────────────────────────

def test_compteurs_declenches_par_l_evenement():
    bloc, _ = _bloc_et_capteur("tab5_redemarrages")
    declencheurs = bloc["triggers"]
    (evt,) = [t for t in declencheurs if t.get("event_type") == EVENEMENT]
    assert evt["id"] == "redemarrage"
    # Rien d'autre ne compte : démarrage de HA et rechargement des modèles rendent +0.
    assert {t.get("event") or t.get("event_type") for t in declencheurs} == {
        EVENEMENT, "start", "event_template_reloaded"}
    for unique_id in ("tab5_redemarrages", "tab5_redemarrages_inattendus"):
        bloc_c, capteur = _bloc_et_capteur(unique_id)
        assert bloc_c == bloc  # même bloc à déclencheurs
        assert capteur["state_class"] == "total_increasing"
        assert capteur["default_entity_id"] == f"sensor.{unique_id}"
        assert capteur["name"].startswith("Tab5 · ")


@pytest.mark.parametrize("etat", ["unknown", "unavailable", "0", "12"])
@pytest.mark.parametrize("declencheur, inattendu, plus_total, plus_inattendus", [
    ("redemarrage", "oui", 1, 1),
    ("redemarrage", "non", 1, 0),
    ("0", None, 0, 0),  # démarrage de HA (trigger.id = index du déclencheur)
    ("2", None, 0, 0),  # rechargement des modèles
])
def test_compteurs_rendus(etat, declencheur, inattendu, plus_total, plus_inattendus):
    trigger = {"id": declencheur}
    if inattendu is not None:
        trigger["event"] = {"data": {"inattendu": inattendu}}
    base = int(etat) if etat.isdigit() else 0
    for unique_id, plus in (("tab5_redemarrages", plus_total), ("tab5_redemarrages_inattendus", plus_inattendus)):
        _, capteur = _bloc_et_capteur(unique_id)
        rendu = _env().from_string(capteur["state"]).render(this={"state": etat}, trigger=trigger)
        assert rendu.strip() == str(base + plus), (unique_id, etat, declencheur)


# ─── Fonctionnement continu ──────────────────────────────────────────────────

def _duree(debut, trigger):
    bloc, capteur = _bloc_et_capteur("tab5_fonctionnement_continu")
    env = _env()
    env.globals["states"] = lambda e: debut if e == "sensor.tab5_demarrage" else "unknown"
    dispo = env.from_string(capteur["availability"]).render().strip()
    return dispo, env.from_string(capteur["state"]).render(trigger=trigger).strip()


def test_fonctionnement_continu_declare():
    bloc, capteur = _bloc_et_capteur("tab5_fonctionnement_continu")
    assert capteur["device_class"] == "duration"
    assert capteur["state_class"] == "measurement"
    assert capteur["unit_of_measurement"] == "h"
    assert capteur["default_entity_id"] == "sensor.tab5_fonctionnement_continu"
    assert "now()" not in capteur["state"] + capteur["availability"]
    ids = {t.get("id"): t for t in bloc["triggers"]}
    assert ids["horloge"]["trigger"] == "time_pattern"
    assert ids["demarrage"] == {"trigger": "state", "entity_id": "sensor.tab5_demarrage", "id": "demarrage"}


UTC = dt.timezone.utc


@pytest.mark.parametrize("debut, trigger, attendu", [
    ("2026-10-01T06:00:00+00:00", {"id": "horloge", "now": dt.datetime(2026, 10, 9, 8, 15, tzinfo=UTC)}, "194.25"),
    # Juste après un démarrage, lu à l'instant du changement (last_updated).
    ("2026-10-09T07:59:00+00:00",
     {"id": "demarrage", "to_state": {"last_updated": dt.datetime(2026, 10, 9, 8, 0, tzinfo=UTC)}}, "0.02"),
    # Démarrage arrondi à ±64 s, après l'instant : 0, jamais négatif.
    ("2026-10-09T08:01:04+00:00", {"id": "horloge", "now": dt.datetime(2026, 10, 9, 8, 0, tzinfo=UTC)}, "0"),
])
def test_fonctionnement_continu_rendu(debut, trigger, attendu):
    dispo, etat = _duree(debut, trigger)
    assert dispo == "True"
    assert float(etat) == float(attendu)


@pytest.mark.parametrize("etat", ["unknown", "unavailable"])
def test_fonctionnement_continu_indisponible_sans_demarrage(etat):
    dispo, _ = _duree(etat, {"id": "horloge", "now": dt.datetime(2026, 10, 9, tzinfo=UTC)})
    assert dispo == "False"
