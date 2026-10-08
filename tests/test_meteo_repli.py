# -*- coding: utf-8 -*-
"""Repli de la météo quand la source choisie ne répond plus (08/10/2026).

Ce matin-là, weather.<ville> (Météo-France) de l'auteur est passée `unavailable` de
11 h 34 à 11 h 42 (« Connection reset by peer ») : sensor.tab5_meteo a suivi,
script.tab5_push_meteo a envoyé « unavailable » à 0 °C, et les weather.get_forecasts de la
poussée complète ont échoué (« did not match any entities »), alors
qu'OpenWeatherMap répondait. Les vrais modèles (packages/tab5_meteo_sources.yaml,
custom_templates/tab5_meteo.jinja, packages/tab5_push.yaml) sont rendus ici avec de
fausses entités, comme dans test_meteo_sans_meteo_france.py."""
import datetime as dt
import json

import jinja2
import pytest

from tests.test_alertes_ha import Etat as EtatAlerte
from tests.test_alertes_ha import _env as _env_alertes
from tests.test_alertes_ha import logique

from tests.test_meteo_sans_meteo_france import (
    MAINTENANT, Etat, Etats, _environnement, _paquet, _parcourir, _rendre,
)

MF = "weather.saint_vincent_de_tyrosse"
OWM = "weather.openweathermap"
MET = "weather.forecast_home"
INTEGRATIONS = {"meteo_france": [MF], "openweathermap": [OWM], "met": [MET]}


def _maison(mf="rainy", owm="cloudy", met=None, choix=MF, releve_mf=5, releve_owm=5, panne_mf=26):
    """Météo-France choisie ; OpenWeatherMap (et Met.no si `met`) à côté. Un état None =
    entité absente ; « unavailable » = entité sans attributs, comme dans HA. releve_* :
    minutes depuis la dernière relève (last_reported) ; panne_mf : minutes depuis le
    dernier changement d'état de Météo-France (last_changed)."""
    def meteo(entite, etat, nom, releve=5, **attributs):
        if etat is None:
            return []
        if etat in ("unavailable", "unknown"):
            e = Etat(entite, etat, friendly_name=nom)
        else:
            e = Etat(entite, etat, supported_features=3, friendly_name=nom, **attributs)
        e.last_reported = MAINTENANT - dt.timedelta(minutes=releve)
        e.last_changed = MAINTENANT - dt.timedelta(minutes=panne_mf if entite == MF else 60)
        return [e]

    etats = Etats(
        meteo(MF, mf, "Météo-France forecast for city Saint-Vincent-de-Tyrosse", releve_mf,
              temperature=16.1, humidity=85)
        + meteo(OWM, owm, "OpenWeatherMap", releve_owm, temperature=17.0, humidity=70, uv_index=2.0)
        + meteo(MET, met, "Forecast Home", temperature=15.0, humidity=90)
        + [Etat("input_text.tab5_meteo_previsions", choix),
           Etat("input_select.tab5_source_pluie", "Météo-France"),
           Etat("input_select.tab5_source_vigilance", "Météo-France")])
    env = _environnement(etats)
    env.globals["integration_entities"] = lambda domaine: [e for e in INTEGRATIONS.get(domaine, []) if etats[e]]
    blocs = _paquet("tab5_meteo_sources.yaml")["template"]
    bloc = next(b for b in blocs if any(c.get("unique_id") == "tab5_sources_meteo" for c in b.get("sensor", [])))
    variables = {}
    for nom, modele in bloc["variables"].items():
        variables[nom] = _rendre(env, modele, variables)
    capteur = bloc["sensor"][0]
    etats.ajouter(Etat("sensor.tab5_sources_meteo", _rendre(env, capteur["state"], variables),
                       **_rendre(env, capteur["attributes"], variables)))
    bloc = next(b for b in blocs if any(c.get("unique_id") == "tab5_meteo" for c in b.get("sensor", [])))
    capteur = next(c for c in bloc["sensor"] if c.get("unique_id") == "tab5_meteo")
    etats.ajouter(Etat("sensor.tab5_meteo", _rendre(env, capteur["state"]), **_rendre(env, capteur["attributes"])))
    return etats, env, bloc["select"][0]


def _poussee(env):
    auto = next(a for a in _paquet("tab5_push.yaml")["automation"] if a.get("id") == "tab5_ha_hmi_updater")
    modeles = next(n["variables"] for n in _parcourir(auto["actions"])
                   if isinstance(n.get("variables"), dict) and "type_jours" in n["variables"])
    contexte = {}
    for nom, modele in modeles.items():
        contexte[nom] = _rendre(env, modele, contexte)
    return auto, contexte


def _meteo_actuelle_part(env):
    script = _paquet("tab5_push.yaml")["script"]["tab5_push_meteo"]
    garde = next(e for e in script["sequence"]
                 if e.get("condition") == "template" and "entite_effective" in e["value_template"])
    return _rendre(env, garde["value_template"]) is True


def test_source_choisie_disponible_rien_ne_change():
    etats, env, liste = _maison()
    a = etats["sensor.tab5_meteo"].attributes
    assert (etats("sensor.tab5_meteo"), a["entite"], a["entite_effective"]) == ("rainy", MF, MF)
    assert (a["temperature"], a["humidite"], a["nom_effectif"]) == (16.1, 85, "Météo-France")
    assert _poussee(env)[1]["meteo"] == MF and _poussee(env)[1]["meteo_en_panne"] is False
    assert _rendre(env, liste["state"]) == MF and _meteo_actuelle_part(env)


@pytest.mark.parametrize("panne", ["unavailable", "unknown"])
def test_meteo_france_indisponible_openweathermap_prend_le_relais(panne):
    etats, env, liste = _maison(mf=panne)
    a = etats["sensor.tab5_meteo"].attributes
    # Le choix ne bouge pas (liste et attribut entite) ; tout le reste vient d'OWM.
    assert a["entite"] == MF and _rendre(env, liste["state"]) == MF
    assert a["entite_effective"] == OWM and a["nom_effectif"] == "OpenWeatherMap"
    assert etats("sensor.tab5_meteo") == "cloudy"
    assert (a["temperature"], a["humidite"], a["uv"], a["type_jours"], a["heures_ok"]) == (17.0, 70, 2, "daily", True)
    contexte = _poussee(env)[1]
    assert contexte["meteo"] == OWM and contexte["meteo_en_panne"] is False
    assert _meteo_actuelle_part(env)


def test_sans_source_connue_une_autre_meteo_disponible():
    etats, _, _ = _maison(mf="unavailable", owm=None, met="sunny")
    assert etats["sensor.tab5_meteo"].attributes["entite_effective"] == MET
    assert etats["sensor.tab5_meteo"].attributes["nom_effectif"] == "Met.no"
    assert etats("sensor.tab5_meteo") == "sunny"


def test_aucune_meteo_disponible_rien_ne_part():
    """Ni « unavailable » à 0 °C, ni get_forecasts sur une entité absente : la tablette
    garde son dernier affichage."""
    etats, env, _ = _maison(mf="unavailable", owm="unavailable")
    assert etats["sensor.tab5_meteo"].attributes["entite_effective"] == ""
    assert not _meteo_actuelle_part(env)
    auto, contexte = _poussee(env)
    assert contexte["meteo"] == "" and contexte["meteo_en_panne"] is True
    # Chaque get_forecasts est sous « meteo != '' », chaque poussée de prévisions sous
    # « not meteo_en_panne ».
    for noeud in _parcourir(auto["actions"]):
        for etape in noeud.get("then") or []:
            if not isinstance(etape, dict):
                continue
            garde = str(noeud.get("if"))
            if etape.get("action") == "weather.get_forecasts":
                assert "meteo != ''" in garde
            if "previsions_" in str(etape.get("action", "")) or "repeat" in etape:
                assert "{{ not meteo_en_panne }}" in garde
    poussees = [n for n in _parcourir(auto["actions"]) if "previsions_" in str(n.get("action", ""))]
    assert len(poussees) == 2


def test_sans_aucune_entite_meteo_comme_avant():
    """Une maison sans météo du tout : la poussée part comme avant (planning de l'agenda
    dans les jours), seulement sans prévisions."""
    _, env, _ = _maison(mf=None, owm=None)
    assert _poussee(env)[1]["meteo_en_panne"] is False
    assert _meteo_actuelle_part(env)


def test_retour_automatique_sur_la_source_choisie():
    etats, _, _ = _maison(mf="unavailable")
    assert etats["sensor.tab5_meteo"].attributes["entite_effective"] == OWM
    etats, _, _ = _maison(mf="rainy")
    assert etats["sensor.tab5_meteo"].attributes["entite_effective"] == MF


def test_la_poussee_complete_suit_l_entite_effective():
    auto = next(a for a in _paquet("tab5_push.yaml")["automation"] if a.get("id") == "tab5_ha_hmi_updater")
    assert {"trigger": "state", "entity_id": "sensor.tab5_meteo", "attribute": "entite_effective"} in auto["triggers"]


def test_macro_importable_sans_ha():
    """Le fichier se charge (pas d'erreur de syntaxe Jinja)."""
    env = jinja2.Environment(loader=jinja2.FileSystemLoader(
        str(__import__("pathlib").Path(__file__).resolve().parents[1] / "HomeAssistant_Config" / "custom_templates")))
    env.get_template("tab5_meteo.jinja")


# ── Source périmée : disponible, mais plus de relève depuis 2 h ─────────────────────

def test_source_figee_depuis_plus_de_2_h_repli():
    etats, env, liste = _maison(releve_mf=121)
    a = etats["sensor.tab5_meteo"].attributes
    assert a["entite"] == MF and a["entite_effective"] == OWM and _rendre(env, liste["state"]) == MF
    assert _poussee(env)[1]["meteo"] == OWM


def test_relevee_il_y_a_moins_de_2_h_pas_de_repli():
    etats, _, _ = _maison(releve_mf=119)
    assert etats["sensor.tab5_meteo"].attributes["entite_effective"] == MF


def test_un_repli_perime_n_est_pas_pris():
    """OpenWeatherMap figée elle aussi, Met.no fraîche : Met.no."""
    etats, _, _ = _maison(mf="unavailable", releve_owm=300, met="sunny")
    assert etats["sensor.tab5_meteo"].attributes["entite_effective"] == MET


def test_tout_perime_la_choisie_reste_mieux_que_rien():
    etats, env, _ = _maison(releve_mf=300, releve_owm=300)
    assert etats["sensor.tab5_meteo"].attributes["entite_effective"] == MF
    assert _poussee(env)[1]["meteo_en_panne"] is False


def test_retour_quand_la_source_choisie_releve_de_nouveau():
    assert _maison(releve_mf=180)[0]["sensor.tab5_meteo"].attributes["entite_effective"] == OWM
    assert _maison(releve_mf=1)[0]["sensor.tab5_meteo"].attributes["entite_effective"] == MF


def test_seuil_en_constante_nommee():
    texte = (__import__("pathlib").Path(__file__).resolve().parents[1] / "HomeAssistant_Config"
             / "custom_templates" / "tab5_meteo.jinja").read_text(encoding="utf-8")
    assert "{%- set TAB5_METEO_PERIMEE_S = 7200 -%}" in texte
    assert texte.count("7200") == 1


# ── Alerte « Météo : … » sur la carte centrale (canal des alertes HA) ───────────────

def _alerte(etats):
    """La source « meteo » de tab5_alertes_sources, rendue avec les attributs du vrai
    capteur « Tab5 Météo »."""
    m = etats["sensor.tab5_meteo"]
    env = _env_alertes([EtatAlerte("sensor.tab5_meteo", m.state, dict(m.attributes))], MAINTENANT.timestamp())
    rendu = env.from_string("{% from 'tab5_alertes.jinja' import tab5_alertes_sources %}"
                            "{{ tab5_alertes_sources({}, [], 20) }}").render()
    return json.loads(rendu)["actives"].get("ha:meteo_repli")


def test_alerte_source_indisponible():
    a = _alerte(_maison(mf="unavailable", panne_mf=26)[0])
    assert a == {"c": "indisponible|OpenWeatherMap", "g": "Jaune", "s": "meteo",
                 "t": "Météo : OpenWeatherMap utilisé, Météo-France indisponible depuis 20 h 04"}


def test_alerte_source_figee():
    a = _alerte(_maison(releve_mf=180)[0])
    assert a["t"] == "Météo : OpenWeatherMap utilisé, Météo-France ne se met plus à jour depuis 17 h 30"
    a = _alerte(_maison(releve_mf=60 * 24 + 1)[0])
    assert a["t"].endswith("ne se met plus à jour depuis le 01/10 à 20 h 29")


def test_alerte_sans_source_de_repli():
    a = _alerte(_maison(mf="unavailable", owm="unavailable")[0])
    assert a["t"] == "Météo : Météo-France indisponible depuis 20 h 04"
    a = _alerte(_maison(releve_mf=300, releve_owm=300)[0])
    assert a["t"] == "Météo : Météo-France ne se met plus à jour depuis 15 h 30"


def test_pas_d_alerte_pendant_les_5_premieres_minutes_ni_au_retour():
    assert _alerte(_maison(mf="unavailable", panne_mf=4)[0]) is None
    assert _alerte(_maison(mf="unavailable", panne_mf=5)[0]) is not None
    assert _alerte(_maison()[0]) is None


def test_l_alerte_s_affiche_puis_disparait_d_elle_meme():
    t = int(MAINTENANT.timestamp())
    ab = {"maj": "toutes", "probleme": True, "indispo": True, "vigilance": "Jaune"}
    panne = {"ha:meteo_repli": _alerte(_maison(mf="unavailable")[0])}
    m = logique({}, {"actives": panne}, "tick", t, ab)
    assert [a["i"] for a in m["affichees"]] == ["ha:meteo_repli#1"]
    assert m["affichees"][0]["t"].startswith("Météo : OpenWeatherMap utilisé")
    m = logique(m, {"actives": {}}, "tick", t + 60, ab)
    assert m["affichees"] == []


def test_texte_de_l_alerte_tient_dans_100_caracteres():
    etats = _maison(releve_mf=60 * 24 + 1)[0]
    etats["sensor.tab5_meteo"].attributes["nom_effectif"] = "x" * 40
    etats["sensor.tab5_meteo"].attributes["nom_choisi"] = "y" * 40
    assert len(_alerte(etats)["t"]) <= 100

