# -*- coding: utf-8 -*-
"""Package `tab5_voitures.yaml` (ADR-0074) : le script `tab5_voitures_analyse`, ses VRAIS modèles Jinja
rendus dans le bac à sable de Jinja (comme HA) sur des réponses simulées de `recorder.get_statistics`,
et comparés à la référence Python `tools/voitures_charges.py` sur les mêmes séries que
`tests/test_voitures_charges.py` : les chaînes doivent être identiques.

Ce n'est pas Home Assistant : seules les fonctions de modèle que le package appelle sont imitées
(`now`, `timedelta`, `as_timestamp`, `as_datetime`, `as_local`, le test `match`), et l'exécution du
script se limite à ce qu'il contient (`variables`, `if`, `recorder.get_statistics`, `stop`).
Non vérifié ici : le script sur un vrai HA et de vraies statistiques ; la durée du calcul sur un
Raspberry Pi (le test mesure celle de ce PC)."""
import ast
import datetime as dt
import re
import time

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

import voitures_charges as ref
from tests.commun import BaseChargeur, HA, lire
from tests.test_voitures_charges import MAINTENANT, PARIS, SCENARIOS, T1, capteurs

PACKAGE = HA / "packages" / "tab5_voitures.yaml"
CLES = ["charges", "charges_1", "charges_2", "jour_debut", "jour_1", "jour_2", "jour_maison", "bilan_debut", "bilan",
        "precision"]


# ─── Imitation de ce que les modèles appellent dans HA ───────────────────────

def _script():
    return yaml.load(PACKAGE.read_text(encoding="utf-8"), Loader=BaseChargeur)["script"]["tab5_voitures_analyse"]


def _analyser(brut):
    """HA lit le résultat d'un modèle comme un littéral Python quand il en est un."""
    brut = brut.strip()
    try:
        valeur = ast.literal_eval(brut)
    except (ValueError, SyntaxError, TypeError, MemoryError):
        return brut
    return brut if isinstance(valeur, str) else valeur


def _env(maintenant):
    def as_datetime(v):
        return v if isinstance(v, dt.datetime) else dt.datetime.fromisoformat(str(v))

    env = ImmutableSandboxedEnvironment(extensions=["jinja2.ext.loopcontrols"], undefined=jinja2.StrictUndefined)
    env.globals.update(now=lambda: maintenant, timedelta=dt.timedelta, as_datetime=as_datetime,
                       as_local=lambda v: v.astimezone(maintenant.tzinfo),
                       as_timestamp=lambda v: as_datetime(v).timestamp())
    env.tests.update(match=lambda v, motif: bool(re.match(motif, str(v))))
    return env


def _rendre(env, valeur, ctx):
    if isinstance(valeur, str) and ("{{" in valeur or "{%" in valeur):
        return _analyser(env.from_string(valeur).render(ctx))
    if isinstance(valeur, list):
        return [_rendre(env, v, ctx) for v in valeur]
    if isinstance(valeur, dict):
        return {k: _rendre(env, v, ctx) for k, v in valeur.items()}
    return valeur


_AUCUN = object()


class Execution:
    """Un passage de script.tab5_voitures_analyse : variables, conditions, appels au recorder (réponses
    tirées du cas simulé, ou aucune réponse si `sans_recorder`), puis la réponse du `stop`."""

    def __init__(self, cas, sans_recorder=False, capteurs_bruts=_AUCUN, avec_capteurs=True):
        self.cas = cas
        self.sans_recorder = sans_recorder
        self.env = _env(cas["maintenant"])
        self.ctx = {}
        if avec_capteurs:
            self.ctx["capteurs"] = cas["capteurs"] if capteurs_bruts is _AUCUN else capteurs_bruts
        if cas.get("jour") is not None:
            self.ctx["jour"] = cas["jour"]
        self.appels = []
        self.reponse = None
        script = _script()
        for nom, v in script["variables"].items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)
        self._executer(script["sequence"])

    def _executer(self, etapes):
        for etape in etapes:
            if self.reponse is not None:
                return
            if "variables" in etape:
                for nom, v in etape["variables"].items():
                    self.ctx[nom] = _rendre(self.env, v, self.ctx)
            elif "if" in etape:
                if _rendre(self.env, etape["if"], self.ctx):
                    self._executer(etape["then"])
                elif "else" in etape:
                    self._executer(etape["else"])
            elif "action" in etape:
                self._appeler(etape)
            elif "stop" in etape:
                self.reponse = self.ctx[etape["response_variable"]]
            else:
                raise AssertionError("étape inconnue : %s" % list(etape))

    def _appeler(self, etape):
        assert etape["action"] == "recorder.get_statistics", etape["action"]
        assert etape["continue_on_error"] is True
        data = _rendre(self.env, etape["data"], self.ctx)
        self.appels.append(data)
        if self.sans_recorder:
            return
        source = {"5minute": self.cas["stats5"], "hour": self.cas["stats1"], "month": self.cas["odo"]}[data["period"]]
        ids = data["statistic_ids"]
        self.ctx[etape["response_variable"]] = {"statistics": {s: source[s] for s in ids if source.get(s)}}


def executer(nom, **kw):
    return Execution(SCENARIOS[nom](), **kw)


# ─── Mêmes chiffres que la référence ─────────────────────────────────────────

@pytest.mark.parametrize("nom", sorted(SCENARIOS))
def test_le_modele_donne_les_chiffres_de_la_reference(nom):
    cas = SCENARIOS[nom]()
    attendu = ref.reponse(**cas)
    obtenu = Execution(cas).reponse
    assert list(obtenu) == CLES
    for cle in CLES:
        assert obtenu[cle] == attendu[cle], (nom, cle)


def test_la_methode_est_la_meme_des_deux_cotes():
    """Les choix documentés (ADR-0074) sont écrits aux deux endroits : ils ne doivent pas diverger."""
    v = _script()["variables"]
    assert v["seuil_w"] == ref.SEUIL_W and v["trou_s"] == ref.TROU_MAX_MIN * 60
    assert v["nb_charges"] == ref.NB_CHARGES and v["km_min_conso"] == ref.KM_MIN_CONSO
    assert ref.NB_VOITURES == 2 and "ns.l | count < 2" in v["voitures"]
    assert ref.JOURS_RECHERCHE == 30 and "timedelta(days=29)" in v["fen"]


def test_capteurs_mal_formes_ou_absents_reponse_vide():
    """Pas de `capteurs`, un texte, pas de voiture, un capteur de voiture vide : rien ne plante, tout est
    vide (comme la référence) ; les dates, elles, restent."""
    cas = SCENARIOS["nuit"]()
    for brut in (None, "texte", [], {}, {"voitures": [{"puissance_charge": ""}]}, {"voitures": "x"}):
        obtenu = Execution(cas, capteurs_bruts=brut).reponse
        attendu = ref.reponse(cas["stats5"], cas["stats1"], cas["odo"], brut, MAINTENANT)
        assert obtenu == attendu, brut
        assert obtenu["charges"] == "" and obtenu["precision"] == "aucune" and obtenu["jour_debut"] == "2026-06-16"
    sans = Execution(cas, avec_capteurs=False)
    assert sans.reponse["charges"] == "" and sans.appels == []        # aucune demande au recorder sans voiture


def test_recorder_en_echec_reponse_vide_sans_erreur():
    """Si get_statistics échoue (continue_on_error), aucune réponse n'arrive : tout est vide."""
    r = executer("nuit", sans_recorder=True)
    assert len(r.appels) == 2
    rep = r.reponse
    assert rep["charges"] == "" and rep["precision"] == "aucune" and rep["jour_1"] == ";" * 23
    attendu = ref.reponse({}, {}, {}, SCENARIOS["nuit"]()["capteurs"], MAINTENANT)
    assert rep == attendu


# ─── Ce que le script demande au recorder ────────────────────────────────────

def test_les_demandes_au_recorder():
    r = executer("bilan")
    cinq, heure, mois = r.appels
    ids = ["sensor.solaire_a", "sensor.maison_puissance", "sensor.tesla_2023_puissance_de_charge",
           "sensor.tesla_2025_puissance_de_charge"]
    for d, periode in ((cinq, "5minute"), (heure, "hour")):
        assert d["period"] == periode and d["types"] == ["mean"] and d["units"] == {"power": "W"}
        assert d["statistic_ids"] == ids
        assert d["start_time"] == "2026-05-18T00:00:00+02:00"     # 29 jours avant le 16/06, heure locale avec son décalage
    assert mois["period"] == "month" and mois["types"] == ["change"] and mois["units"] == {"distance": "km"}
    assert mois["statistic_ids"] == ["sensor.tesla_2023_odometre", "sensor.tesla_2025_odometre"]
    assert mois["start_time"] == "2026-06-01T00:00:00+02:00"
    # sans odomètre : pas de troisième demande ; capteur solaire commun à deux voitures : sans doublon
    assert len(executer("nuit").appels) == 2
    assert executer("sans_voiture").appels == []


def test_fenetre_de_lecture_suit_le_jour_demande():
    r = Execution(dict(SCENARIOS["nuit"](), jour="2026-04-02"))
    assert r.appels[0]["start_time"] == "2026-04-02T00:00:00+02:00"
    # le 1er du mois à plus de 29 jours
    cas = dict(SCENARIOS["nuit"](), maintenant=MAINTENANT.replace(month=7, day=31))
    assert Execution(cas).appels[0]["start_time"] == "2026-07-01T00:00:00+02:00"


def test_aucun_appel_au_firmware_ni_a_la_voiture():
    """Ce lot ne pousse rien à la tablette (le lot suivant le fait) et ne réveille jamais la voiture :
    la seule action est la lecture du recorder."""
    actions = []

    def chercher(n):
        if isinstance(n, dict):
            if "action" in n:
                actions.append(n["action"])
            for v in n.values():
                chercher(v)
        elif isinstance(n, list):
            for v in n:
                chercher(v)
    chercher(_script()["sequence"])
    assert set(actions) == {"recorder.get_statistics"}
    texte = PACKAGE.read_text(encoding="utf-8")
    code = "\n".join(l for l in texte.splitlines() if not l.lstrip().startswith("#"))
    assert "esphome." not in code and not re.search(r"tesla|teslemetry|wake", code, re.I)


def test_fichier_public_sans_entite_ni_secret_ni_placeholder():
    """ADR-0024 : aucune entité réelle, aucun `!secret`, aucun `VOTRE_…`."""
    texte = PACKAGE.read_text(encoding="utf-8")
    assert "!secret" not in texte and not re.search(r"VOTRE_[A-Z]", texte)
    code = "\n".join(l for l in texte.splitlines() if not l.lstrip().startswith("#"))
    assert not re.search(r"\b(sensor|binary_sensor|select|input_\w+|switch|number|device_tracker|climate)\.[a-z0-9_]+", code)
    paquet = yaml.load(texte, Loader=BaseChargeur)
    assert set(paquet) == {"script"} and set(paquet["script"]) == {"tab5_voitures_analyse"}   # aucune entité créée


def test_unite_des_puissances_lue_par_le_recorder():
    """Le capteur de la voiture est en kW : la demande dit `power: W`, c'est HA qui convertit (la
    réponse du recorder est alors déjà en W)."""
    cinq = executer("nuit").appels[0]
    assert cinq["units"] == {"power": "W"}


# ─── Formats poussés par le lot suivant (plan §5) ────────────────────────────

LIGNE = re.compile(r"^[12]\|\d{4}-\d{2}-\d{2}\|\d{2}:\d{2}\|\d+\|\d+\.\d+\|(\d{1,3}|nan|)$")


@pytest.mark.parametrize("nom", ["nuit", "nuage", "deux_voitures", "six_charges", "sans_solaire", "sans_maison"])
def test_formats_des_charges(nom):
    rep = executer(nom).reponse
    lignes = rep["charges"].split(";")
    assert 1 <= len(lignes) <= 6 and all(LIGNE.match(x) for x in lignes), lignes
    dates = [x.split("|")[1] + x.split("|")[2] for x in lignes]
    assert dates == sorted(dates, reverse=True)


def test_formats_des_heures_et_du_bilan():
    rep = executer("bilan").reponse
    for cle in ("jour_1", "jour_2", "jour_maison"):
        assert len(rep[cle].split(";")) == 24 and all(re.fullmatch(r"\d+\.\d+|", x) for x in rep[cle].split(";"))
    assert rep["jour_debut"] == "2026-06-16" and rep["bilan_debut"] == "2026-06-01"
    champs = rep["bilan"].split("|")
    assert len(champs) == 11
    assert executer("nuit").reponse["bilan"].split("|")[5:10] == [""] * 5      # pas de voiture 2 : champs vides
    assert executer("sans_voiture").reponse["bilan"] == ""


# ─── Durée du calcul ─────────────────────────────────────────────────────────

def test_duree_du_calcul_sur_30_jours():
    """30 jours : 6 jours de lignes de 5 minutes et 29 jours d'heures pour 3 capteurs. Mesure de ce PC
    (HA est dans la boucle d'événements : voir @limits du package)."""
    cas = SCENARIOS["bilan"]()
    lignes = sum(len(v) for v in cas["stats5"].values()) + sum(len(v) for v in cas["stats1"].values())
    debut = time.perf_counter()
    Execution(cas)
    duree = time.perf_counter() - debut
    print("\n%d lignes de statistiques, %.2f s de rendu" % (lignes, duree))
    assert lignes > 4000 and duree < 15
