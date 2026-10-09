# -*- coding: utf-8 -*-
"""Popup Température (ADR-0032) : l'historique des deux températures de l'accueil.

Aucun compilateur ne relie le blueprint, le package `tab5_historique.yaml`, la démo, le
rendu et le firmware. Ce fichier le fait :

- contrat : l'action `tab5_maj_historique` et ses variables, les vues et les clés du
  firmware, ses limites (créneaux, points de prévision) face au package et à la démo ;
- package : ses VRAIS modèles Jinja sont rendus (bac à sable de Jinja, comme HA) sur des
  réponses de `recorder.get_statistics` et de `weather.get_forecasts` simulées, au format
  de HA (start / datetime en ISO UTC), et comparés à un calcul Python écrit à part :
  début du premier créneau, créneaux des trois vues (moyenne, minimum, maximum),
  minutes « à l'horloge » à travers les deux changements d'heure, prévision heure par
  heure et par jour, en-tête, capteur absent ou réponse manquante ;
- blueprint : l'événement esphome.tab5_historique lance le script avec le capteur de
  l'emplacement et la case « la seconde température est dehors » ;
- firmware : les appuis longs, le registre, et une lecture des payloads de la démo comme
  `historique_recu()` les découpe.

Ce n'est pas Home Assistant : seules les fonctions de modèle que le package appelle sont
imitées. Vérifié à part le 06/10/2026 sur le HA d'Axel : format des réponses de
get_statistics (liste vide acceptée) et de get_forecasts (Météo-France, par jour)."""
import ast
import datetime as dt
import math
import os
import re
from zoneinfo import ZoneInfo

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

from tests.test_tuiles_blueprint import Etat, Passage, _chercher, _evenement
from tests.commun import lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACKAGE = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_historique.yaml")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
CPP = os.path.join(REPO, "Tab5", "ecran", "tab5_historique.cpp")
PARSE = os.path.join(REPO, "Tab5", "socle", "tab5_parse.h")
CLIMAT = os.path.join(REPO, "Tab5", "ui_components", "climate_card.yaml")

import demo_pusher  # noqa: E402
import scenarios  # noqa: E402

PARIS = ZoneInfo("Europe/Paris")
UTC = dt.timezone.utc
# Un après-midi de juin, 14:37 à Paris : l'heure de 14:00 n'est pas encore compilée.
MAINTENANT = dt.datetime(2026, 6, 16, 14, 37, 20, tzinfo=PARIS)
CAPTEUR = "sensor.serre_temperature"
METEO = "weather.maison"


# ─── Contrat ─────────────────────────────────────────────────────────────────

def test_action_du_firmware():
    contrat = demo_pusher.lire_contrat()
    assert contrat[demo_pusher.SERVICE_HISTORIQUE] == ("cle", "vue", "entete", "mesures", "previsions")
    assert "historique_recu(cle, vue, entete, mesures, previsions);" in _lire(demo_pusher.API_LOGIC)


def test_vues_cles_et_limites_du_firmware():
    cpp = _lire(CPP)
    vues = re.findall(r'"(\w+)"', re.search(r"kVues\[NB_VUES\] = \{([^}]*)\}", cpp).group(1))
    cles = re.findall(r'"(\w+)"', re.search(r"kCles\[NB_CLES\] = \{([^}]*)\}", cpp).group(1))
    assert tuple(vues) == tuple(scenarios.HISTORIQUE_VUES) == ("jour", "semaine", "mois")
    assert tuple(cles) == scenarios.HISTORIQUE_CLES == ("salon", "serre", "p0", "p1", "p2", "p3", "p4")
    # Plafonds de la lecture (historique_lire, tab5_parse.h), ceux de l'écran.
    parse = _lire(PARSE)
    assert "kMesuresMax = kHistoriqueMesuresMax;" in cpp and "kPrevMax = kHistoriquePrevMax;" in cpp
    mesures_max = int(re.search(r"kHistoriqueMesuresMax = (\d+);", parse).group(1))
    prev_max = int(re.search(r"kHistoriquePrevMax = (\d+);", parse).group(1))
    assert mesures_max == scenarios.HISTORIQUE_MESURES_MAX and prev_max == scenarios.HISTORIQUE_PREV_MAX
    for vue, (_, nb) in scenarios.HISTORIQUE_VUES.items():
        assert nb + 1 <= mesures_max, f"{vue} : {nb + 1} créneaux pour {mesures_max} places"
    assert f"ns.l | count < {prev_max}" in _lire(PACKAGE)
    # Bandes mini/maxi : seulement par jour (type daily, templow), un point par jour sur
    # jours_prev jours au plus — 7 pour 30 jours. La demi-journée n'en a pas (DO-8, audit
    # du 07/10/2026 : 10 bandes suffisent).
    package = _lire(PACKAGE)
    jours_prev = max(int(n) for n in re.findall(r"'\w+': (\d+)", re.search(r'jours_prev: "([^"]*)"', package).group(1)))
    assert "if type_prev == 'daily' and f.get('templow') is number" in package
    assert jours_prev <= int(re.search(r"kBandesPrevMax = (\d+);", cpp).group(1))


def test_bornes_de_lecture_du_firmware():
    """historique_lire() (tab5_parse.h) rejette un pas, une minute ou une température hors
    de ses bornes (payload faux, et pas de débordement en entier) : celles du package
    doivent y tenir."""
    parse = _lire(PARSE)
    borne = lambda nom: float(re.search(rf"constexpr float {nom} = ([\d.e+]+)f;", parse).group(1))
    pas_max = max(pas for pas, _ in scenarios.HISTORIQUE_VUES.values())
    assert pas_max <= borne("kHistoriquePasMax")
    # Vue 30 jours : 30 jours + aujourd'hui, puis 7 jours de prévision au plus, à midi.
    pas, nb = scenarios.HISTORIQUE_VUES["mois"]
    assert (nb + 7) * pas + 720 <= borne("kHistoriqueMinutesMax")
    assert borne("kHistoriqueTempMax") >= 150, "une température en °F doit passer"


def _lire_comme_le_firmware(entete, mesures, previsions):
    """Découpe de historique_lire() : 6 champs « | », plus l'humidité actuelle en 7e avec
    un capteur d'humidité (ADR-0047) ; créneaux « ; » de 3 champs « , », ou 6 avec
    l'humidité en % entiers (un créneau vide final n'est pas lu) ; points
    « minute,moy[,min,max] » dans l'ordre."""
    champs = entete.split("|")
    assert len(champs) in (6, 7), entete
    nom, debut, pas, maintenant, actuel, exterieur = champs[:6]
    humidite = champs[6] if len(champs) == 7 else None
    assert humidite is None or humidite == "nan" or 0 <= int(humidite) <= 100, humidite
    assert re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}", debut), debut
    creneaux = mesures.split(";") if mesures else []
    if creneaux and creneaux[-1] == "":
        creneaux = creneaux[:-1]
    for c in creneaux:
        n = len(c.split(","))
        assert c == "" or n == 3 or (humidite is not None and n == 6), c
        if n == 6:
            assert all(re.fullmatch(r"\d{1,3}", h) and int(h) <= 100 for h in c.split(",")[3:]), c
    points = [p.split(",") for p in previsions.split(";")] if previsions else []
    minutes = [int(p[0]) for p in points]
    assert minutes == sorted(set(minutes)), "prévision hors de l'ordre : le firmware en sauterait"
    assert all(len(p) in (2, 4) for p in points)
    return {"nom": nom, "debut": debut, "pas": int(pas), "maintenant": int(maintenant), "actuel": actuel,
            "exterieur": exterieur, "humidite": humidite, "creneaux": creneaux, "points": points}


@pytest.mark.parametrize("cle", scenarios.HISTORIQUE_CLES)
@pytest.mark.parametrize("vue", tuple(scenarios.HISTORIQUE_VUES))
def test_demo_dans_le_format(cle, vue):
    moment = dt.datetime(2026, 6, 16, 7, 45)
    h = scenarios.build_historique(cle, vue, moment)
    assert set(h) == {"cle", "vue", "entete", "mesures", "previsions"} and (h["cle"], h["vue"]) == (cle, vue)
    lu = _lire_comme_le_firmware(h["entete"], h["mesures"], h["previsions"])
    pas, nb = scenarios.HISTORIQUE_VUES[vue]
    assert lu["actuel"] == scenarios.historique_actuel(cle), "la courbe finit sur la valeur de l'accueil"
    if lu["actuel"] == "nan":
        # Pièce sans température (ADR-0040) : la réponse du package sans capteur.
        assert cle.startswith("p") and lu["nom"] == "" and lu["creneaux"] == [] and lu["points"] == []
        return
    assert lu["pas"] == pas and len(lu["creneaux"]) == nb + 1
    # Humidité (ADR-0047) : celle de l'accueil, et trois valeurs de plus par créneau.
    assert lu["humidite"] == (scenarios.historique_humidite(cle) or None)
    assert all(len(c.split(",")) == (6 if lu["humidite"] else 3) for c in lu["creneaux"])
    assert (lu["humidite"] is not None) == (cle in ("salon", "p3")), "la démo : le salon et le Bureau"
    debut = dt.datetime.fromisoformat(lu["debut"])
    assert lu["maintenant"] == (moment - debut).total_seconds() // 60
    assert nb * pas <= lu["maintenant"] < (nb + 1) * pas, "maintenant tombe dans le dernier créneau"
    assert bool(lu["points"]) == (cle == "serre"), "la prévision : la seconde température seulement"
    assert all(int(p[0]) > lu["maintenant"] for p in lu["points"])
    if vue == "mois" and cle == "serre":
        assert all(len(p) == 4 for p in lu["points"]) and len(lu["points"]) == 7


def test_demo_dehors_prolonge_la_courbe():
    moment = dt.datetime(2026, 6, 16, 7, 45)
    serre = scenarios.build_historique("serre", "jour", moment)
    dehors = scenarios.build_historique("serre", "jour", moment, exterieur=True)
    assert serre["entete"].endswith("|0") and dehors["entete"].endswith("|1")
    # Dehors : le premier point prévu reste près de la valeur actuelle (22.1 à 07:45).
    premier = float(dehors["previsions"].split(";")[0].split(",")[1])
    assert abs(premier - 22.1) < 2.5
    # Sixième champ (le salon a aussi l'humidité en septième, ADR-0047).
    assert scenarios.build_historique("salon", "jour", moment, exterieur=True)["entete"].split("|")[5] == "0"


# ─── Package : imitation de ce que ses modèles appellent dans HA ─────────────

def _script():
    return yaml.safe_load(_lire(PACKAGE))["script"]["tab5_historique"]


class _Tuple(tuple):
    """Un tuple rendu par HA garde son texte (ResultWrapper) : « 1500,16.1 » reste tel quel
    une fois repassé dans un modèle."""

    def __new__(cls, valeur, brut):
        o = super().__new__(cls, valeur)
        o.brut = brut
        return o

    def __str__(self):
        return self.brut


def _analyser(brut):
    brut = brut.strip()
    try:
        valeur = ast.literal_eval(brut)
    except (ValueError, SyntaxError, TypeError, MemoryError):
        return brut
    if isinstance(valeur, str):
        return brut
    if isinstance(valeur, tuple):
        return _Tuple(valeur, brut)
    return valeur


def _booleen(v, defaut=None):
    """Filtre bool de HA (forgiving_boolean)."""
    if isinstance(v, bool):
        return v
    s = str(v).strip().lower()
    if s in ("true", "on", "yes", "1", "enable"):
        return True
    if s in ("false", "off", "no", "0", "disable"):
        return False
    return defaut


def _est_un_nombre(v):
    try:
        return math.isfinite(float(v))
    except (TypeError, ValueError):
        return False


class EtatHA:
    def __init__(self, entity_id, state, aire=None, appareil=None, **attributes):
        self.entity_id, self.state, self.aire, self.appareil, self.attributes = (
            entity_id, state, aire, appareil, attributes)


def _env(etats, maintenant, appareils=None):
    d = {e.entity_id: e for e in etats}
    appareils = appareils or {}

    class States:
        def __call__(self, e):
            return d[e].state if e in d else "unknown"

        def __getitem__(self, e):
            if "." not in e:   # HA : « Invalid domain name »
                raise jinja2.TemplateError(f"Invalid domain name '{e}'")
            return d.get(e)

    def as_datetime(v):
        return v if isinstance(v, dt.datetime) else dt.datetime.fromisoformat(str(v))

    env = ImmutableSandboxedEnvironment(extensions=["jinja2.ext.loopcontrols"], undefined=jinja2.StrictUndefined)
    env.globals.update(
        states=States(), state_attr=lambda e, a: d[e].attributes.get(a) if e in d else None,
        now=lambda: maintenant, timedelta=dt.timedelta, as_datetime=as_datetime,
        as_local=lambda v: v.astimezone(PARIS),
        area_name=lambda e: d[e].aire if e in d else None,
        device_id=lambda e: d[e].appareil if e in d else None,
        device_attr=lambda a, nom: appareils.get(a, {}).get(nom),
    )
    env.filters.update(bool=_booleen, is_number=_est_un_nombre)
    return env


def _rendre(env, valeur, ctx):
    if isinstance(valeur, str) and ("{{" in valeur or "{%" in valeur):
        return _analyser(env.from_string(valeur).render(ctx))
    if isinstance(valeur, list):
        return [_rendre(env, v, ctx) for v in valeur]
    if isinstance(valeur, dict):
        return {k: _rendre(env, v, ctx) for k, v in valeur.items()}
    return valeur


def _maison(valeur="18.46", heures_ok=True, type_jours="daily", meteo=METEO, aire="Serre"):
    return [
        EtatHA(CAPTEUR, valeur, aire=aire, appareil="thermo_1", friendly_name="Thermomètre serre Température"),
        EtatHA("sensor.salon_temperature", "21.04", appareil="thermo_2", friendly_name="Salon Température"),
        EtatHA("sensor.tab5_meteo", "sunny", entite=meteo, entite_effective=meteo, heures_ok=heures_ok,
               type_jours=type_jours),
        EtatHA(METEO, "sunny"),
    ]


APPAREILS = {"thermo_1": {"name": "Thermomètre serre", "name_by_user": None},
             "thermo_2": {"name": "Thermo 2", "name_by_user": "Salon | coin"}}


class Execution:
    """Une exécution de script.tab5_historique : réponses des deux actions fournies
    (None : l'action a échoué, continue_on_error, sa variable n'existe pas)."""

    def __init__(self, champs, etats=None, maintenant=MAINTENANT, stats=None, prev=None):
        script = _script()
        self.env = _env(_maison() if etats is None else etats, maintenant, APPAREILS)
        ctx = dict(champs)
        for nom, v in script["variables"].items():
            ctx[nom] = _rendre(self.env, v, ctx)
        seq = script["sequence"]
        assert seq[0]["action"] == "recorder.get_statistics" and seq[0]["continue_on_error"] is True
        self.demande_stats = _rendre(self.env, seq[0]["data"], ctx)
        if stats is not None:
            ctx["stats"] = stats
        for nom, v in seq[1]["variables"].items():
            ctx[nom] = _rendre(self.env, v, ctx)
        branche = seq[2]
        self.demande_prev = None
        if _rendre(self.env, branche["if"], ctx):
            appel = branche["then"][0]
            assert appel["action"] == "weather.get_forecasts" and appel["continue_on_error"] is True
            self.demande_prev = _rendre(self.env, {"data": appel["data"], "target": appel["target"]}, ctx)
            if prev is not None:
                ctx["prev"] = prev
            for nom, v in branche["then"][1]["variables"].items():
                ctx[nom] = _rendre(self.env, v, ctx)
            pousse = branche["then"][2]
        else:
            pousse = branche["else"][0]
        self.action = _rendre(self.env, pousse["action"], ctx)
        self.poussee = {k: str(_rendre(self.env, v, ctx)) for k, v in pousse["data"].items()}
        self.ctx = ctx

    def lu(self):
        p = self.poussee
        return _lire_comme_le_firmware(p["entete"], p["mesures"], p["previsions"])


def _champs(cle="serre", vue="jour", capteur=CAPTEUR, exterieur=False):
    return {"tablette": "tab5_ha_hmi", "cle": cle, "vue": vue, "capteur": capteur, "exterieur": exterieur}


# ─── Statistiques simulées et calcul indépendant ─────────────────────────────

def _temperature(t_utc):
    """Une journée de serre : 9 °C vers 4 h UTC, 25 °C vers 14 h UTC."""
    h = t_utc.hour + t_utc.minute / 60
    return 17.0 + 8.0 * math.sin(2 * math.pi * (h - 8) / 24) + 0.3 * (t_utc.toordinal() % 3)


def _lignes_heures(debut_local, maintenant):
    """Lignes horaires de HA depuis debut_local, jusqu'à la dernière heure compilée (celle
    qui contient maintenant ne l'est pas encore)."""
    lignes = []
    u = debut_local.astimezone(UTC)
    fin = maintenant.astimezone(UTC).replace(minute=0, second=0, microsecond=0)
    while u < fin:
        echantillons = [_temperature(u + dt.timedelta(minutes=m)) for m in range(0, 60, 10)]
        lignes.append({"start": u.isoformat(), "end": (u + dt.timedelta(hours=1)).isoformat(),
                       "mean": sum(echantillons) / 6, "min": min(echantillons), "max": max(echantillons)})
        u += dt.timedelta(hours=1)
    return lignes


def _lignes_jours(debut_local, maintenant):
    """Lignes par jour de HA (jours locaux, le jour en cours compris : HA le réduit des
    heures déjà compilées)."""
    lignes = []
    jour = debut_local
    while jour.date() <= maintenant.date():
        suivant = (jour.replace(tzinfo=None) + dt.timedelta(days=1)).replace(tzinfo=PARIS)
        heures = [r for r in _lignes_heures(jour, min(suivant, maintenant))]
        if heures:
            lignes.append({"start": jour.astimezone(UTC).isoformat(), "end": suivant.astimezone(UTC).isoformat(),
                           "mean": sum(r["mean"] for r in heures) / len(heures),
                           "min": min(r["min"] for r in heures), "max": max(r["max"] for r in heures)})
        jour = suivant
    return lignes


def _debut_attendu(vue, maintenant):
    """Calcul à part, en heure locale naïve : le créneau qui contient maintenant, moins nb."""
    pas, nb = scenarios.HISTORIQUE_VUES[vue]
    n = maintenant.replace(tzinfo=None, second=0, microsecond=0)
    if vue == "mois":
        return dt.datetime(n.year, n.month, n.day) - dt.timedelta(days=nb)
    h = pas // 60
    return dt.datetime(n.year, n.month, n.day, n.hour - n.hour % h) - dt.timedelta(hours=h * nb)


def _creneaux_attendus(lignes, debut_naif, pas, nb):
    """Chaque ligne dans le créneau de son début à l'horloge locale (date naïve)."""
    par_creneau = {}
    for r in lignes:
        local = dt.datetime.fromisoformat(r["start"]).astimezone(PARIS).replace(tzinfo=None)
        i = int((local - debut_naif).total_seconds() // 60) // pas
        if 0 <= i <= nb:
            par_creneau.setdefault(i, []).append(r)
    sortie = []
    for i in range(nb + 1):
        rs = par_creneau.get(i)
        sortie.append(None if not rs else (sum(r["mean"] for r in rs) / len(rs), min(r["min"] for r in rs),
                                           max(r["max"] for r in rs)))
    return sortie


def _comparer(lus, attendus):
    assert len(lus) == len(attendus)
    for k, (lu, a) in enumerate(zip(lus, attendus)):
        if a is None:
            assert lu == "", f"créneau {k} : {lu!r} au lieu de vide"
            continue
        valeurs = [float(x) for x in lu.split(",")]
        assert valeurs == pytest.approx([round(x, 1) for x in a], abs=0.051), f"créneau {k}"


def _reponse(lignes):
    return {"statistics": {CAPTEUR: lignes}}


# ─── Package : début, créneaux, en-tête ──────────────────────────────────────

@pytest.mark.parametrize("vue", tuple(scenarios.HISTORIQUE_VUES))
def test_debut_et_pas_comme_la_demo(vue):
    e = Execution(_champs(vue=vue), stats=_reponse([]))
    pas, nb = scenarios.HISTORIQUE_VUES[vue]
    assert (e.ctx["pas"], e.ctx["nb"]) == (pas, nb)
    debut = dt.datetime.fromisoformat(e.ctx["debut"])
    assert debut.replace(tzinfo=None) == _debut_attendu(vue, MAINTENANT)
    assert debut.replace(tzinfo=None) == scenarios.debut_historique(vue, MAINTENANT.replace(tzinfo=None))
    assert debut.utcoffset() == dt.timedelta(hours=2)
    assert e.demande_stats == {"start_time": e.ctx["debut"], "statistic_ids": [CAPTEUR],
                               "period": "day" if vue == "mois" else "hour", "types": ["mean", "min", "max"]}


@pytest.mark.parametrize("vue", tuple(scenarios.HISTORIQUE_VUES))
def test_creneaux_des_trois_vues(vue):
    pas, nb = scenarios.HISTORIQUE_VUES[vue]
    debut = _debut_attendu(vue, MAINTENANT)
    debut_local = debut.replace(tzinfo=PARIS)
    lignes = (_lignes_jours if vue == "mois" else _lignes_heures)(debut_local, MAINTENANT)
    # Une heure sans ligne (recorder arrêté) au milieu.
    if vue != "mois":
        lignes = [r for r in lignes if r is not lignes[len(lignes) // 2]]
    e = Execution(_champs(vue=vue), stats=_reponse(lignes))
    lu = e.lu()
    _comparer(e.poussee["mesures"].split(";"), _creneaux_attendus(lignes, debut, pas, nb))
    assert lu["debut"] == debut.strftime("%Y-%m-%dT%H:%M") and lu["pas"] == pas
    assert lu["maintenant"] == (MAINTENANT.replace(tzinfo=None) - debut).total_seconds() // 60
    assert e.action == "esphome.tab5_ha_hmi_tab5_maj_historique"
    assert (e.poussee["cle"], e.poussee["vue"]) == ("serre", vue)


@pytest.mark.parametrize("maintenant, double, absente", [
    # 25 octobre 2026, 3 h → 2 h : l'heure de 2 h passe deux fois (deux lignes, un créneau).
    (dt.datetime(2026, 10, 25, 14, 37, tzinfo=PARIS), 12, None),
    # 29 mars 2026, 2 h → 3 h : l'heure de 2 h n'existe pas (créneau vide).
    (dt.datetime(2026, 3, 29, 14, 37, tzinfo=PARIS), None, 12),
])
def test_changement_d_heure(maintenant, double, absente):
    debut = _debut_attendu("jour", maintenant)
    lignes = _lignes_heures(debut.replace(tzinfo=PARIS), maintenant)
    e = Execution(_champs(), maintenant=maintenant, stats=_reponse(lignes))
    creneaux = e.poussee["mesures"].split(";")
    attendus = _creneaux_attendus(lignes, debut, 60, 24)
    _comparer(creneaux, attendus)
    # L'axe garde 24 h d'horloge : 14:00 la veille → 14:37.
    assert e.lu()["maintenant"] == 24 * 60 + 37
    assert e.lu()["debut"] == debut.strftime("%Y-%m-%dT%H:%M")
    heures_locales = [dt.datetime.fromisoformat(r["start"]).astimezone(PARIS).hour for r in lignes]
    if double is not None:
        assert heures_locales.count(2) == 2 and creneaux[double] != ""
    if absente is not None:
        assert 2 not in heures_locales and creneaux[absente] == ""
    # Le créneau en cours (14:00) n'a pas de ligne : vide, en dernier.
    assert creneaux[-1] == "" and len(creneaux) == 25


def test_en_tete():
    e = Execution(_champs(exterieur=True), stats=_reponse([]))
    lu = e.lu()
    assert (lu["nom"], lu["actuel"], lu["exterieur"]) == ("Serre", "18.5", "1")
    # Sans pièce : le nom de l'appareil (« | » retiré), puis celui du capteur.
    e = Execution(_champs(cle="salon", capteur="sensor.salon_temperature", exterieur=True), stats=_reponse([]))
    assert e.lu()["nom"] == "Salon   coin" and e.lu()["exterieur"] == "0", "la pièce n'est jamais dehors"
    etats = _maison(aire=None)
    e = Execution(_champs(), etats=etats, stats=_reponse([]))
    assert e.lu()["nom"] == "Thermomètre serre"
    # Capteur sans valeur : nan.
    e = Execution(_champs(), etats=_maison(valeur="unavailable"), stats=_reponse([]))
    assert e.lu()["actuel"] == "nan"


def test_aucune_ligne_aucun_capteur_reponse_manquante():
    # Capteur sans statistiques (pas de state_class) : mesures vide → « Aucun historique ».
    e = Execution(_champs(), stats=_reponse([]))
    assert e.poussee["mesures"] == ""
    # Pas de capteur à cet emplacement : une demande vide (acceptée par HA), rien à montrer.
    e = Execution(_champs(capteur=""), stats={"statistics": {}})
    assert e.demande_stats["statistic_ids"] == [] and e.poussee["mesures"] == ""
    assert e.lu()["nom"] == "" and e.lu()["actuel"] == "nan"
    # Capteur supprimé de HA.
    e = Execution(_champs(capteur="sensor.disparu"), stats={"statistics": {}})
    assert e.demande_stats["statistic_ids"] == [] and e.poussee["mesures"] == ""
    # get_statistics a échoué : la variable n'existe pas, la tablette reçoit quand même sa réponse.
    e = Execution(_champs(), stats=None)
    assert e.poussee["mesures"] == "" and e.action.endswith("_tab5_maj_historique")


# ─── Package : humidité (ADR-0047) ───────────────────────────────────────────

HUMIDITE = "sensor.bureau_humidite"


def _humidite(t_utc):
    """Une humidité de pièce : 62 % vers 5 h UTC, 38 % vers 17 h UTC."""
    h = t_utc.hour + t_utc.minute / 60
    return 50.0 + 12.0 * math.cos(2 * math.pi * (h - 5) / 24)


def _lignes_humidite(lignes):
    """Les lignes d'une humidité aux mêmes heures que celles d'une température."""
    sortie = []
    for r in lignes:
        u = dt.datetime.fromisoformat(r["start"])
        fin = dt.datetime.fromisoformat(r["end"])
        pas = int((fin - u).total_seconds() // 600)
        echantillons = [_humidite(u + dt.timedelta(minutes=10 * k)) for k in range(pas)]
        sortie.append({"start": r["start"], "end": r["end"], "mean": sum(echantillons) / len(echantillons),
                       "min": min(echantillons), "max": max(echantillons)})
    return sortie


def _maison_humide(valeur="47.6"):
    return _maison() + [EtatHA(HUMIDITE, valeur, aire="Bureau", friendly_name="Bureau Humidité")]


@pytest.mark.parametrize("vue", tuple(scenarios.HISTORIQUE_VUES))
def test_humidite_par_creneau(vue):
    pas, nb = scenarios.HISTORIQUE_VUES[vue]
    debut = _debut_attendu(vue, MAINTENANT)
    lignes = (_lignes_jours if vue == "mois" else _lignes_heures)(debut.replace(tzinfo=PARIS), MAINTENANT)
    humides = _lignes_humidite(lignes)
    # Une heure sans température mais avec l'humidité, une autre sans humidité.
    if vue != "mois":
        lignes = [r for r in lignes if r is not lignes[3]]
        humides = [r for r in humides if r is not humides[7]]
    e = Execution(dict(_champs(cle="p3", vue=vue), humidite=HUMIDITE), etats=_maison_humide(),
                  stats={"statistics": {CAPTEUR: lignes, HUMIDITE: humides}})
    assert e.demande_stats["statistic_ids"] == [CAPTEUR, HUMIDITE]
    assert e.lu()["humidite"] == "48"
    t_attendus = _creneaux_attendus(lignes, debut, pas, nb)
    h_attendus = _creneaux_attendus(humides, debut, pas, nb)
    creneaux = e.poussee["mesures"].split(";")
    assert len(creneaux) == nb + 1
    for k, (c, t, h) in enumerate(zip(creneaux, t_attendus, h_attendus)):
        champs = c.split(",") if c else []
        if h is None:
            _comparer([c], [t])
            continue
        assert len(champs) == 6, f"créneau {k} : {c!r}"
        if t is None:
            assert champs[:3] == ["", "", ""], f"créneau {k} : pas de température"
        else:
            _comparer([",".join(champs[:3])], [t])
        assert [int(x) for x in champs[3:]] == [round(x) for x in h], f"créneau {k}"
    if vue == "jour":   # un créneau par heure : les deux heures retirées se voient
        assert creneaux[3].startswith(",,,") and len(creneaux[7].split(",")) == 3


def test_humidite_absente_inconnue_ou_supprimee():
    lignes = _lignes_heures(_debut_attendu("jour", MAINTENANT).replace(tzinfo=PARIS), MAINTENANT)
    sans = Execution(_champs(cle="p3"), etats=_maison_humide(), stats=_reponse(lignes))
    # Champ absent (blueprint d'avant l'ADR-0047), vide, ou capteur supprimé : la poussée
    # d'avant, octet pour octet (six champs d'en-tête, trois par créneau).
    for humidite in ("", "sensor.disparu", None):
        champs = _champs(cle="p3") if humidite is None else dict(_champs(cle="p3"), humidite=humidite)
        e = Execution(champs, etats=_maison_humide(), stats=_reponse(lignes))
        assert e.poussee == sans.poussee and e.demande_stats["statistic_ids"] == [CAPTEUR]
        assert e.lu()["humidite"] is None
    # Capteur sans valeur : « nan » ; sans statistiques : créneaux de trois champs.
    e = Execution(dict(_champs(cle="p3"), humidite=HUMIDITE), etats=_maison_humide("unavailable"),
                  stats=_reponse(lignes))
    assert e.lu()["humidite"] == "nan" and e.poussee["mesures"] == sans.poussee["mesures"]
    # Humidité seule (la température n'a pas de statistiques) : « ,,, » et ses trois valeurs.
    e = Execution(dict(_champs(cle="p3"), humidite=HUMIDITE), etats=_maison_humide(),
                  stats={"statistics": {HUMIDITE: _lignes_humidite(lignes)}})
    creneaux = e.poussee["mesures"].split(";")
    assert len(creneaux) == 25 and creneaux[-1] == "" and all(c.startswith(",,,") for c in creneaux[:-1])


# ─── Package : prévision ─────────────────────────────────────────────────────

def _prevision_heures(maintenant, n=48):
    """Réponse heure par heure, comme HA : à partir de l'heure en cours, en UTC."""
    u = maintenant.astimezone(UTC).replace(minute=0, second=0, microsecond=0)
    return {METEO: {"forecast": [{"datetime": (u + dt.timedelta(hours=k)).isoformat(), "condition": "sunny",
                                  "temperature": round(_temperature(u + dt.timedelta(hours=k)), 1)}
                                 for k in range(n)]}}


def _prevision_jours(maintenant, n=15):
    """Réponse par jour, comme Météo-France (vérifié sur le HA d'Axel le 06/10/2026) :
    minuit UTC de chaque jour à partir d'aujourd'hui, maxi temperature, mini templow."""
    j0 = maintenant.astimezone(UTC).replace(hour=0, minute=0, second=0, microsecond=0)
    return {METEO: {"forecast": [{"datetime": (j0 + dt.timedelta(days=k)).isoformat(), "condition": "rainy",
                                  "temperature": 20.0 + k, "templow": 10.0 + k / 2} for k in range(n)]}}


def _points_heures_attendus(reponse, maintenant, debut_naif, horizon, tous):
    sortie = []
    for f in reponse[METEO]["forecast"]:
        t = dt.datetime.fromisoformat(f["datetime"]).astimezone(PARIS)
        if maintenant < t <= maintenant + dt.timedelta(hours=horizon) and t.hour % tous == 0:
            sortie.append([int((t.replace(tzinfo=None) - debut_naif).total_seconds() // 60), f["temperature"]])
    return sortie


@pytest.mark.parametrize("vue, horizon, tous", [("jour", 24, 1), ("semaine", 72, 3)])
def test_prevision_heure_par_heure(vue, horizon, tous):
    reponse = _prevision_heures(MAINTENANT, 80)
    e = Execution(_champs(vue=vue), stats=_reponse([]), prev=reponse)
    assert e.demande_prev == {"data": {"type": "hourly"}, "target": {"entity_id": METEO}}
    attendus = _points_heures_attendus(reponse, MAINTENANT, _debut_attendu(vue, MAINTENANT), horizon, tous)
    lus = [[int(p[0]), float(p[1])] for p in e.lu()["points"]]
    assert lus == attendus and len(lus) == 24
    assert all(m > e.lu()["maintenant"] for m, _ in lus)


@pytest.mark.parametrize("vue, heures_ok, jours", [("mois", True, 7), ("jour", False, 1), ("semaine", False, 3)])
def test_prevision_par_jour(vue, heures_ok, jours):
    reponse = _prevision_jours(MAINTENANT)
    e = Execution(_champs(vue=vue), etats=_maison(heures_ok=heures_ok), stats=_reponse([]), prev=reponse)
    assert e.demande_prev["data"] == {"type": "daily"}
    debut = _debut_attendu(vue, MAINTENANT)
    attendus = []
    for f in reponse[METEO]["forecast"]:
        jour = dt.datetime.fromisoformat(f["datetime"]).astimezone(PARIS).date()
        ecart = (jour - MAINTENANT.date()).days
        if 1 <= ecart <= jours:
            midi = dt.datetime(jour.year, jour.month, jour.day, 12)
            attendus.append([int((midi - debut).total_seconds() // 60), (f["temperature"] + f["templow"]) / 2,
                             f["templow"], f["temperature"]])
    lus = [[int(p[0])] + [float(x) for x in p[1:]] for p in e.lu()["points"]]
    assert len(lus) == jours
    for lu, a in zip(lus, attendus):
        assert lu[0] == a[0] and lu[1:] == pytest.approx(a[1:], abs=0.051)


@pytest.mark.parametrize("heures_ok, type_jours, vue, attendu", [
    (True, "daily", "jour", "hourly"),
    (True, "daily", "semaine", "hourly"),
    (True, "daily", "mois", "daily"),
    (True, "twice_daily", "mois", "twice_daily"),
    (True, "hourly", "mois", "hourly"),       # l'entité ne sait que les heures
    (False, "daily", "jour", "daily"),
    (False, "", "jour", ""),                  # rien de connu : pas d'appel
])
def test_type_de_prevision(heures_ok, type_jours, vue, attendu):
    e = Execution(_champs(vue=vue), etats=_maison(heures_ok=heures_ok, type_jours=type_jours), stats=_reponse([]),
                  prev={METEO: {"forecast": []}})
    assert e.ctx["type_prev"] == attendu
    assert (e.demande_prev is None) == (attendu == "")


def test_pas_de_prevision_pour_la_piece_ni_sans_meteo():
    e = Execution(_champs(cle="salon", capteur="sensor.salon_temperature"), stats=_reponse([]),
                  prev=_prevision_heures(MAINTENANT))
    assert e.demande_prev is None and e.poussee["previsions"] == ""
    e = Execution(_champs(), etats=_maison(meteo=""), stats=_reponse([]))
    assert e.demande_prev is None and e.poussee["previsions"] == ""
    # get_forecasts a échoué : pas de point, la courbe part quand même.
    e = Execution(_champs(), stats=_reponse([]), prev=None)
    assert e.demande_prev is not None and e.poussee["previsions"] == ""


def test_une_seule_prevision_reste_un_texte():
    """HA lit « 1500,16.1 » comme un tuple ; repassé dans un modèle, il redevient le texte
    (sans parenthèses), ce que la tablette lit."""
    reponse = {METEO: {"forecast": [{"datetime": (MAINTENANT + dt.timedelta(hours=1)).astimezone(UTC).isoformat(),
                                     "temperature": 16.1}]}}
    e = Execution(_champs(), stats=_reponse([]), prev=reponse)
    assert re.fullmatch(r"\d+,16\.1", e.poussee["previsions"]), e.poussee["previsions"]


def test_mode_et_champs_du_script():
    script = _script()
    assert script["mode"] == "parallel", "deux vues demandées de suite : chacune sa réponse"
    assert set(script["fields"]) == {"tablette", "cle", "vue", "capteur", "exterieur", "humidite"}


# ─── Blueprint ───────────────────────────────────────────────────────────────

def _maison_bp():
    return [
        Etat("sensor.salon_t", "21.4", "Salon", friendly_name="Salon", unit_of_measurement="°C",
             device_class="temperature"),
        Etat(CAPTEUR, "18.5", "Serre", friendly_name="Serre", unit_of_measurement="°C", device_class="temperature"),
    ]


def _branche(p):
    branche = _chercher(p.corps["actions"], lambda d: d.get("alias") == "La tablette ouvre l'historique d'une température")
    assert branche, "branche « historique » du blueprint introuvable"
    return branche


def _etape_if(p):
    """Les variables de la branche (h_cle, h_capteur) rendues, puis l'étape « if »."""
    branche = _branche(p)
    p.variables_du_bloc("h_capteur")
    return branche["sequence"][1]


def _variables_envoyees(p):
    branche = _branche(p)
    assert p.modele(branche["conditions"]) is True
    etape = _etape_if(p)
    p.etats.d["script.tab5_historique"] = Etat("script.tab5_historique", "off")
    assert p.modele(etape["if"]) is True
    turn_on = etape["then"][0]
    assert turn_on["action"] == "script.turn_on" and turn_on["target"]["entity_id"] == "script.tab5_historique"
    p.env.filters["bool"] = _booleen
    return {k: p.modele(v) for k, v in turn_on["data"]["variables"].items()}


ENTREES = {"salon_temperature": "sensor.salon_t", "serre_temperature": CAPTEUR}


def test_blueprint_piece():
    p = Passage(ENTREES, _maison_bp(), _evenement("historique", cle="salon", vue="semaine"))
    v = _variables_envoyees(p)
    assert v["cle"] == "salon" and v["vue"] == "semaine" and v["capteur"] == "sensor.salon_t"
    assert v["exterieur"] is False and v["tablette"] == p["tablette"]
    assert set(v) == set(_script()["fields"]), "le blueprint et le script ne parlent pas des mêmes champs"


@pytest.mark.parametrize("coche", [False, True])
def test_blueprint_seconde_temperature(coche):
    p = Passage(dict(ENTREES, serre_exterieure=coche), _maison_bp(), _evenement("historique", cle="serre"))
    v = _variables_envoyees(p)
    assert v["capteur"] == CAPTEUR and v["vue"] == "jour" and v["exterieur"] is coche


def test_blueprint_cle_inconnue_ou_sans_le_package():
    p = Passage(ENTREES, _maison_bp(), _evenement("historique", cle="cuisine", vue="jour"))
    p.etats.d["script.tab5_historique"] = Etat("script.tab5_historique", "off")
    assert p.modele(_etape_if(p)["if"]) is False
    p = Passage(ENTREES, _maison_bp(), _evenement("historique", cle="salon", vue="jour"))
    assert p.modele(_etape_if(p)["if"]) is False


# Température d'une pièce en mode HA (ADR-0040) : clé pR, la sonde de la section
# « Pièce R + 1 », sans prévision ni case « dehors ».
@pytest.mark.parametrize("cle, capteur", [("p1", "sensor.bureau_t"), ("p0", ""), ("p4", "")])
def test_blueprint_temperature_de_la_piece(cle, capteur):
    etats = _maison_bp() + [Etat("sensor.bureau_t", "22.8", "Bureau", friendly_name="Bureau",
                                 unit_of_measurement="°C", device_class="temperature")]
    entrees = dict(ENTREES, serre_exterieure=True, piece_2_temperature="sensor.bureau_t")
    v = _variables_envoyees(Passage(entrees, etats, _evenement("historique", cle=cle, vue="mois")))
    assert v["cle"] == cle and v["vue"] == "mois" and v["capteur"] == capteur and v["exterieur"] is False


def _etats_humides():
    return _maison_bp() + [
        Etat("sensor.bureau_t", "22.8", "Bureau", friendly_name="Bureau", unit_of_measurement="°C",
             device_class="temperature"),
        Etat("sensor.bureau_h", "45", "Bureau", friendly_name="Bureau", unit_of_measurement="%",
             device_class="humidity"),
        Etat("sensor.salon_h", "48", "Salon", friendly_name="Salon", unit_of_measurement="%",
             device_class="humidity"),
    ]


# Humidité (ADR-0047) : celle du salon (« Salon — humidité ») ou de la pièce, jamais de la
# serre ; une sonde supprimée de HA n'est pas envoyée.
@pytest.mark.parametrize("cle, humidite", [
    ("salon", "sensor.salon_h"), ("serre", ""), ("p1", "sensor.bureau_h"), ("p0", ""), ("p2", ""),
])
def test_blueprint_humidite(cle, humidite):
    entrees = dict(ENTREES, salon_humidite="sensor.salon_h", piece_2_temperature="sensor.bureau_t",
                   piece_2_humidite="sensor.bureau_h", piece_3_temperature="sensor.bureau_t",
                   piece_3_humidite="sensor.disparu")
    v = _variables_envoyees(Passage(entrees, _etats_humides(), _evenement("historique", cle=cle)))
    assert v["humidite"] == humidite
    assert set(v) == set(_script()["fields"])


def test_blueprint_temperature_d_une_piece_absente():
    """Une sonde choisie puis supprimée de HA : pas de capteur (le package répond « aucun
    historique »), pas d'erreur."""
    v = _variables_envoyees(Passage(dict(ENTREES, piece_2_temperature="sensor.disparu"), _maison_bp(),
                                    _evenement("historique", cle="p1")))
    assert v["capteur"] == ""


def test_blueprint_ecoute_l_evenement():
    texte = _lire(BLUEPRINT)
    assert re.search(r"event_type: esphome\.tab5_historique\n\s+id: historique\n", texte)
    # Conditions « rien de neuf » et « tablette connectée » ; la garde d'origine, elle, suit
    # le type de l'événement (custom_templates/tab5_tablette.jinja, HA-7).
    assert texte.count("'energie', 'historique'") == 2
    assert "serre_exterieure: !input serre_exterieure" in texte


# ─── Firmware ────────────────────────────────────────────────────────────────

def test_appuis_longs_et_registre():
    climat = _lire(CLIMAT)
    # La clé vient de accueil_historique_cle() : salon / serre, ou pR en mode HA sur une
    # pièce qui a une température (ADR-0040), à droite aussi quand elle a une humidité
    # (ADR-0047) ; nullptr (zone absente, pièce sans humidité à droite) : rien.
    for droite in ("false", "true"):
        assert (f"const char* cle = accueil_historique_cle({droite});\n"
                "              if (cle != nullptr) id(tab5_historique_ouvrir).execute(std::string(cle));"
                in climat.replace("\r\n", "\n")), droite
    piece = _lire(os.path.join(REPO, "Tab5", "ecran", "tab5_piece_climat.cpp"))
    assert 'return zone_absente(Zone::SERRE) ? nullptr : "serre";' in piece
    assert 'return zone_absente(Zone::SALON) ? nullptr : "salon";' in piece
    assert "return droite && !s_pieces[r].humidite ? nullptr : kClesHistorique[r];" in piece
    serre = climat.split("id: btn_serre_games", 1)[1].split("- obj:", 1)[0]
    assert "script.execute: tab5_arcade_open" in serre and "on_long_press:" in serre, \
        "l'appui court sur la serre garde l'arcade"
    navigation = _lire(os.path.join(REPO, "Tab5", "paquets", "tab5-navigation.yaml"))
    assert re.search(r'ModalRegistry::add\(id\(historique_popup\),\s+"Température",\s+ModalRegistry::POPUP\);', navigation)
    yaml_hist = _lire(os.path.join(REPO, "Tab5", "paquets", "tab5-historique.yaml"))
    assert "esphome.tab5_historique" in yaml_hist and "cle: !lambda" in yaml_hist and "vue: !lambda" in yaml_hist
