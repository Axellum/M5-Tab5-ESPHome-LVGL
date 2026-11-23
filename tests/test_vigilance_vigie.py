# -*- coding: utf-8 -*-
"""Vigilances par Vigie (23/11/2026) : la source « Vigie » de la liste « Tab5 · weather
warnings source » lit l'intégration Home Assistant de Vigie (dépôt Axellum/vigie-api,
custom_components/vigie, v0.7.0), sans capteur au format de Météo-France.

Les vrais modèles sont rendus ici (même imitation du Jinja de HA que
test_meteo_sans_meteo_france.py) : « Tab5 · weather sources » trouve le capteur « niveau
maximal » de Vigie (packages/tab5_meteo_sources.yaml), puis « Tab5 Weather warnings » en
tire le niveau global et les 11 cases du Tab5 par la macro de
custom_templates/tab5_vigilance.jinja.

Les attributs du capteur ont la forme de VigieMaxLevelSensor (sensor.py) et de
build_attributes (models.py) de l'intégration, relue sur origin/main le 23/11/2026 et sur
le capteur réel d'une installation (mêmes clés : options, count, by_phenomenon, by_family,
sources, not_matchable, server_time, vigilances, omitted, attribution, device_class,
friendly_name). Un capteur indisponible ne garde que ses attributs de capacité (options) et
son nom. Lieux, entités et textes inventés."""
import os

import pytest

from tests.test_meteo_sans_meteo_france import MODELES, Etat, Etats, _environnement, _paquet, _rendre

LISTE = "input_select.tab5_source_vigilance"
CASES = ["vent", "inondation", "orages", "pluie-inondation", "neige-verglas", "grand froid",
         "vagues-submersion", "canicule", "avalanches", "brouillard", "feux de forêt"]
TOUT_VERT = "|".join(["Vert"] * 11)

# Les phénomènes de l'API (app/models/vigilance.py, VigilancePhenomenon) et la case du Tab5
# de chacun ; « other » n'en a pas. Météo-France y est rangée par
# app/collectors/meteofrance.py : Pluie-inondation → rain, Crues → flood,
# Vagues-submersion → coastal.
CASE_DE = {"wind": 0, "flood": 1, "thunderstorm": 2, "rain": 3, "snow-ice": 4, "cold": 5,
           "coastal": 6, "heat": 7, "avalanche": 8, "fog": 9, "fire": 10}
PHENOMENES_API = sorted(CASE_DE) + ["other"]
# models.py, FAMILY_OF_PHENOMENON.
FAMILLE = {"wind": "wind", "rain": "rain-flood", "flood": "rain-flood", "thunderstorm": "thunderstorm",
           "snow-ice": "snow-ice", "avalanche": "snow-ice", "heat": "heat", "cold": "cold", "fire": "fire",
           "coastal": "coastal", "fog": "fog", "other": "other"}
NIVEAUX = ("green", "yellow", "orange", "red")  # models.py, LEVEL_ORDER
OPTIONS = ["none", "yellow", "orange", "red"]  # const.py, MAX_LEVEL_STATES
ATTRIBUTION = ("Source: Météo-France, Vigilance météorologique (https://vigilance.meteofrance.fr). Licence Ouverte 2.0"
               " | Source: Vigicrues, Service central d'hydrométéorologie et d'appui à la prévision des inondations"
               " (SCHAPI), https://www.vigicrues.gouv.fr. Licence Ouverte 2.0")
SOURCES = [{"source": "meteofrance", "country": "FR", "status": "ok"},
           {"source": "vigicrues", "country": "FR", "status": "ok"}]

LIEU, AUTRE_LIEU = "vigie_lieu_test", "vigie_autre_lieu"


def _niveau(lieu):
    return f"sensor.{lieu}_niveau_maximal"


def _attributs(vigilances, lieu=LIEU):
    """(état, attributs) de VigieMaxLevelSensor pour ces vigilances [(phénomène, niveau)] :
    max_level, by_phenomenon et by_family comme models.py (le plus fort de chaque phénomène,
    « none » pour un vert), la liste des vigilances comme Vigilance.summary()."""
    rangs, familles = {}, {}
    for phenomene, niveau in vigilances:
        r = NIVEAUX.index(niveau)
        rangs[phenomene] = max(rangs.get(phenomene, -1), r)
        familles[FAMILLE[phenomene]] = max(familles.get(FAMILLE[phenomene], -1), r)
    def mot(r):
        return "none" if r == 0 else NIVEAUX[r]

    meilleur = max(rangs.values(), default=-1)
    etat = NIVEAUX[meilleur] if meilleur >= 1 else "none"
    return etat, {
        "options": OPTIONS,
        "count": len(vigilances),
        "by_phenomenon": {p: mot(r) for p, r in sorted(rangs.items())},
        "by_family": {f: mot(r) for f, r in sorted(familles.items())},
        "sources": SOURCES,
        "not_matchable": 0,
        "server_time": "2026-11-23T06:15:21+00:00",
        "vigilances": [{"id": f"meteofrance:test:{i}", "source": "meteofrance", "country": "FR", "level": niveau,
                        "phenomenon": phenomene, "headline": f"Vigilance {niveau} {phenomene}",
                        "area": "Département de test", "onset": "2026-11-23T06:00:00+00:00",
                        "expires": "2026-11-24T06:00:00+00:00", "issuer": "Météo-France",
                        "web": "https://vigilance.meteofrance.fr"}
                       for i, (phenomene, niveau) in enumerate(vigilances)],
        "omitted": 0,
        "attribution": ATTRIBUTION,
        "device_class": "enum",
        "friendly_name": f"Vigie {lieu} Niveau maximal",
    }


def _lieu(lieu, etat, attributs):
    """Les entités d'un lieu surveillé (un appareil), dans un ordre où le capteur cherché
    n'est PAS le premier : la préalerte expérimentale (autres options) et le séisme (un
    attribut `count` lui aussi) passent avant."""
    return [
        Etat(f"sensor.{lieu}_prealerte_experimentale", "none",
             options=["none", "watch", "strong", "severe"], device_class="enum"),
        Etat(f"sensor.{lieu}_seisme_le_plus_fort", "unknown", count=0),
        Etat(_niveau(lieu), etat, **attributs),
        Etat(f"sensor.{lieu}_pluie_dans_l_heure", "0.0", rate_5m_mm_h=[0.0] * 12),
        Etat(f"binary_sensor.{lieu}_vigilance_active", "off"),
        Etat(f"event.{lieu}_vigilance", "unknown"),
        Etat(f"weather.{lieu}", "rainy", temperature=11.0, humidity=90, supported_features=3),
    ]


def _maison(vigilances=(), indisponible=False, source="Vigie", choix=f"weather.{LIEU}", autre_lieu=False,
            integration=True, format_meteo_france=False, met_no=False):
    """La maison, « Tab5 · weather sources » puis « Tab5 Weather warnings » rendus dans
    l'ordre où HA les calcule. Rend (états, capteur des vigilances rendu)."""
    if indisponible:
        etat, attributs = "unavailable", {"options": OPTIONS, "device_class": "enum",
                                          "friendly_name": f"Vigie {LIEU} Niveau maximal"}
    else:
        etat, attributs = _attributs(list(vigilances))
    entites = _lieu(LIEU, etat, attributs)
    if autre_lieu:
        entites += _lieu(AUTRE_LIEU, *_attributs([("wind", "red")], AUTRE_LIEU))
    if met_no:
        # Une autre météo, hors de Vigie (celle que HA installe d'office).
        entites.append(Etat("weather.forecast_home", "cloudy", temperature=10.0, humidity=80, supported_features=3))
    if format_meteo_france:
        # Un capteur au format de Météo-France fait à la main (package privé d'un utilisateur) :
        # jamais pris pour Vigie.
        entites.append(Etat("sensor.vigie_lieu_test_weather_alert", "Rouge", **{
            "Vent violent": "Rouge", "source": _niveau(LIEU)}))
    etats = Etats(entites + [
        Etat("input_text.tab5_meteo_previsions", choix),
        Etat("input_select.tab5_source_pluie", "Météo-France"),
        Etat(LISTE, source),
    ])
    env = _environnement(etats)
    par_integration = [e.entity_id for e in entites if "vigie_" in e.entity_id
                       and not e.entity_id.endswith("_weather_alert")]
    env.globals.update(
        integration_entities=lambda domaine: par_integration if domaine == "vigie" and integration else [],
        # Un appareil par lieu : son nom est dans l'identifiant de ses entités.
        device_id=lambda e: next((lieu for lieu in (AUTRE_LIEU, LIEU) if lieu in (e or "")), None),
        device_entities=lambda d: [e for e in par_integration if d and d in e],
    )
    blocs = _paquet("tab5_meteo_sources.yaml")["template"]
    bloc = next(b for b in blocs if any(c.get("unique_id") == "tab5_sources_meteo" for c in b.get("sensor", [])))
    variables = {}
    for nom, modele in bloc["variables"].items():
        variables[nom] = _rendre(env, modele, variables)
    capteur = bloc["sensor"][0]
    etats.ajouter(Etat("sensor.tab5_sources_meteo", _rendre(env, capteur["state"], variables),
                       **_rendre(env, capteur["attributes"], variables)))
    capteur = next(c for b in blocs for c in b.get("sensor", []) if c.get("unique_id") == "tab5_vigilance")
    rendu = Etat("sensor.tab5_vigilance", _rendre(env, capteur["state"]), **_rendre(env, capteur["attributes"]))
    etats.ajouter(rendu)
    return etats, rendu


def _cases(niveaux):
    """Les 11 cases du Tab5, vertes sauf celles nommées ({nom de CASES: niveau})."""
    cases = ["Vert"] * 11
    for nom, niveau in niveaux.items():
        cases[CASES.index(nom)] = niveau
    return "|".join(cases)


# ─────────────────────────────────────────────────────────────────────────────
# Les quatre cas demandés : tout vert, un jaune, un orange, capteur indisponible
# ─────────────────────────────────────────────────────────────────────────────

def test_tout_vert():
    _, v = _maison([])
    assert (v.state, v.attributes["phenomenes"], v.attributes["source"]) == ("Vert", TOUT_VERT, "Vigie")


def test_un_phenomene_jaune():
    _, v = _maison([("thunderstorm", "yellow")])
    assert v.state == "Jaune"
    assert v.attributes["phenomenes"] == _cases({"orages": "Jaune"})


def test_un_phenomene_orange():
    """Pluie-inondation orange (rain) et vagues-submersion jaune (coastal) : le global prend
    le plus fort, chaque case le sien."""
    _, v = _maison([("rain", "orange"), ("coastal", "yellow")])
    assert v.state == "Orange"
    assert v.attributes["phenomenes"] == _cases({"pluie-inondation": "Orange", "vagues-submersion": "Jaune"})


def test_capteur_indisponible_tout_vert_et_toujours_trouve():
    """Indisponible (API muette, intégration en panne) : tout vert, comme un capteur
    Météo-France absent ; le capteur reste trouvé à ses options, donc les vigilances
    reviennent seules quand il répond de nouveau."""
    etats, v = _maison(indisponible=True)
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == _niveau(LIEU)
    assert (v.state, v.attributes["phenomenes"]) == ("Vert", TOUT_VERT)


# ─────────────────────────────────────────────────────────────────────────────
# Correspondance des phénomènes
# ─────────────────────────────────────────────────────────────────────────────

@pytest.mark.parametrize("phenomene", sorted(CASE_DE))
def test_chaque_phenomene_dans_sa_case(phenomene):
    _, v = _maison([(phenomene, "red")])
    attendu = ["Vert"] * 11
    attendu[CASE_DE[phenomene]] = "Rouge"
    assert (v.state, v.attributes["phenomenes"]) == ("Rouge", "|".join(attendu)), phenomene


def test_other_ne_monte_que_le_niveau_global():
    _, v = _maison([("other", "orange"), ("wind", "yellow")])
    assert (v.state, v.attributes["phenomenes"]) == ("Orange", _cases({"vent": "Jaune"}))


def test_vigilance_verte_reste_verte():
    """Une vigilance de niveau vert (« none » dans by_phenomenon) ne colore rien."""
    _, v = _maison([("heat", "green")])
    assert (v.state, v.attributes["phenomenes"]) == ("Vert", TOUT_VERT)


def test_plusieurs_vigilances_du_meme_phenomene_la_plus_forte():
    _, v = _maison([("flood", "yellow"), ("flood", "red"), ("snow-ice", "orange")])
    assert v.state == "Rouge"
    assert v.attributes["phenomenes"] == _cases({"inondation": "Rouge", "neige-verglas": "Orange"})


def test_table_de_la_macro_couvre_les_phenomenes_de_l_api():
    """Chaque phénomène de l'API sauf « other » a une case, déjà affichée par le Tab5 : aucune
    case inventée (11 cases, comme Météo-France + brouillard + feux de forêt)."""
    assert sorted(CASE_DE.values()) == list(range(11))
    assert set(PHENOMENES_API) - set(CASE_DE) == {"other"}
    with open(os.path.join(MODELES, "tab5_vigilance.jinja"), encoding="utf-8") as f:
        macro = f.read()
    for phenomene, case in CASE_DE.items():
        assert f"'{phenomene}': {case}" in macro, phenomene


# ─────────────────────────────────────────────────────────────────────────────
# Le capteur est trouvé par son intégration, jamais par son nom
# ─────────────────────────────────────────────────────────────────────────────

def test_trouve_par_l_integration_et_liste_des_sources():
    etats, _ = _maison([("wind", "yellow")])
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == _niveau(LIEU)
    assert "Vigie" in etats("sensor.tab5_sources_meteo").split(", ")


def test_le_lieu_de_l_entite_meteo_choisie():
    """Deux lieux surveillés : celui de l'entité météo choisie ; sinon le premier."""
    etats, v = _maison([], autre_lieu=True, choix=f"weather.{AUTRE_LIEU}")
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == _niveau(AUTRE_LIEU)
    assert (v.state, v.attributes["phenomenes"]) == ("Rouge", _cases({"vent": "Rouge"}))
    etats, v = _maison([], autre_lieu=True, choix="weather.forecast_home", met_no=True)
    assert etats.attr("sensor.tab5_sources_meteo", "meteo") == "weather.forecast_home"
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == _niveau(LIEU)
    assert v.state == "Vert"


def test_ni_la_prealerte_ni_un_capteur_au_format_meteo_france():
    etats, _ = _maison([], format_meteo_france=True)
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == _niveau(LIEU)
    # Sans l'intégration non plus : le capteur fait à la main n'a ni by_phenomenon ni by_family.
    etats, _ = _maison([], format_meteo_france=True, integration=False)
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == _niveau(LIEU)


def test_sans_l_integration_reconnu_a_ses_attributs():
    """Données de test du CI (aucune intégration installée) : by_phenomenon et by_family."""
    etats, v = _maison([("fog", "yellow")], integration=False)
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == _niveau(LIEU)
    assert (v.state, v.attributes["phenomenes"]) == ("Jaune", _cases({"brouillard": "Jaune"}))


def test_aucun_capteur_vigie_tout_vert():
    etats, v = _maison([("wind", "red")], indisponible=True, integration=False)
    # Indisponible ET sans intégration : rien à reconnaître.
    assert etats.attr("sensor.tab5_sources_meteo", "vigie") == ""
    assert "Vigie" not in etats("sensor.tab5_sources_meteo")
    assert (v.state, v.attributes["phenomenes"]) == ("Vert", TOUT_VERT)


def test_les_autres_sources_ne_lisent_pas_vigie():
    """Liste sur « None » ou sur Météo-France sans capteur Météo-France : la vigilance rouge
    de Vigie n'est pas montrée (le choix reste celui de l'utilisateur)."""
    for source, attendu in (("None", "none"), ("Météo-France", "Météo-France")):
        _, v = _maison([("wind", "red")], source=source)
        assert (v.state, v.attributes["phenomenes"], v.attributes["source"]) == ("Vert", TOUT_VERT, attendu)


def test_vigie_dans_la_liste_apres_meteo_france():
    options = _paquet("tab5_meteo_sources.yaml")["input_select"]["tab5_source_vigilance"]["options"]
    assert options[:2] == ["Météo-France", "Vigie"] and options[-1] == "None"
