# -*- coding: utf-8 -*-
"""Popup Énergie (ADR-0028, discussion #278) : une installation solaire.

Aucun compilateur ne relie le blueprint, le package `tab5_energie.yaml`, la démo et le
firmware. Ce fichier le fait :

- contrat : les deux actions du firmware et leurs variables, les huit champs de
  l'instantané lus par `tab5_energie.cpp` dans l'ordre que poussent le package et la démo,
  le nombre de créneaux de chaque vue ;
- package : ses VRAIS modèles Jinja sont rendus (bac à sable de Jinja, comme HA) sur des
  réponses de `recorder.get_statistics` simulées, au format de HA (start / end en ISO UTC,
  change, state), et comparés à un calcul Python écrit à part : production du jour,
  barres des heures, des 30 jours et des 12 mois, instantané (unités, signes, maison
  calculée, capteur indisponible) ;
- blueprint : section vide = aucune tuile `e`, réponse vide ; remplie = option `e` et icône
  `solaire`, et le script reçoit les capteurs ;
- firmware : la lettre `e` (bit 128) et ce qu'elle fait sur une tuile cap.

Ce n'est pas Home Assistant : seules les fonctions de modèle que le package appelle sont
imitées. Non vérifié ici : les modèles sur un vrai HA et de vraies statistiques."""
import ast
import datetime as dt
import math
import os
import re
import sys
from zoneinfo import ZoneInfo

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

from tests.test_tuiles_blueprint import Etat, Passage, _chercher, _defs, _evenement
from tests.commun import BaseChargeur, lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACKAGE = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_energie.yaml")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
ENERGIE_CPP = os.path.join(REPO, "Tab5", "ecran", "tab5_energie.cpp")
TUILES_CPP = os.path.join(REPO, "Tab5", "ecran", "tab5_tuiles.cpp")
API = os.path.join(REPO, "Tab5", "paquets", "tab5-api-logic.yaml")

import demo_pusher  # noqa: E402
import scenarios  # noqa: E402

PARIS = ZoneInfo("Europe/Paris")
# Un après-midi de juin, 14:37 à Paris (12:37 UTC) : les 5 minutes de 14:30 sont la
# dernière ligne compilée, celles de 14:35 pas encore.
MAINTENANT = dt.datetime(2026, 6, 16, 14, 37, 20, tzinfo=PARIS)


# ─── Contrat ─────────────────────────────────────────────────────────────────

def test_actions_du_firmware():
    contrat = demo_pusher.lire_contrat()
    assert contrat[demo_pusher.SERVICE_ENERGIE] == ("payload",)
    assert contrat[demo_pusher.SERVICE_ENERGIE_HISTORIQUE] == ("vue", "debut", "valeurs")
    # ADR-0058 : le soleil et la prévision du jour, le bilan d'une vue.
    assert contrat[demo_pusher.SERVICE_ENERGIE_SOLEIL] == ("payload",)
    assert contrat[demo_pusher.SERVICE_ENERGIE_BILAN] == ("vue", "debut", "payload")


def test_huit_champs_dans_le_meme_ordre():
    """energie_instantane() lit 6 mesures, l'unité, puis le jour : l'ordre de la démo et du
    package (dernière ligne du modèle `instantane`)."""
    corps = _lire(ENERGIE_CPP).split("void energie_instantane(", 1)[1].split("\n}\n", 1)[0]
    mesures = re.search(r"Mesure\* champs\[\] = \{([^}]*)\}", corps).group(1)
    lus = re.findall(r"&i\.(\w+)", mesures)
    assert "texte_ha_copier(i.unite_temperature" in corps and "i.jour = lire_mesure" in corps
    assert tuple(lus + ["unite_temperature", "jour"]) == scenarios.ENERGIE_CHAMPS
    assert len(scenarios.build_energie_payload().split("|")) == 8
    instantane = _etape_variables("instantane")["instantane"]
    assert re.sub(r"\s+", "", instantane).endswith(
        "{{[solaire,maison,reseau,nombre(c.get('batterie','')),batterie_p,nombre(temp),unite,jour|string]|join('|')}}")


def test_creneaux_des_vues():
    cpp = _lire(ENERGIE_CPP)
    vues = re.search(r'kVues\[NB_VUES\] = \{([^}]*)\}', cpp).group(1)
    creneaux = re.search(r"kSlots\[NB_VUES\] = \{([^}]*)\}", cpp).group(1)
    firmware = dict(zip(re.findall(r'"(\w+)"', vues), (int(n) for n in creneaux.split(","))))
    assert firmware == scenarios.ENERGIE_VUES == {"heures": 24, "jours": 30, "mois": 12}
    ha = _lire(PACKAGE)
    assert "for h in range(24)" in ha and "nb = 30 if jours else 12" in ha


def _champs_comme_le_firmware(valeurs, maxi):
    """Miroir de la boucle d'energie_historique() (champ_suivant de tab5_champs.h + apres_sep)."""
    p, fin, n, apres_sep, lus = 0, len(valeurs), 0, False, []
    while (p < fin or apres_sep) and n < maxi:
        d = p
        while p < fin and valeurs[p] != ";":
            p += 1
        champ = valeurs[d:p]
        if p < fin:
            p += 1
        apres_sep = p > d + len(champ)
        lus.append(champ)
        n += 1
    return lus


def test_champs_vides_comptes_jusqu_au_dernier():
    """DO-11 (audit du 07/10/2026) : un champ vide après le dernier « ; » (heure à venir)
    compte comme une barre absente, sans décaler les autres ; DO-2 : année bornée comme
    dans l'historique."""
    corps = _lire(ENERGIE_CPP).split("void energie_historique(", 1)[1].split("\n}\n", 1)[0]
    assert "while ((p < fin || apres_sep) && s.n < kSlots[v])" in corps
    assert "apres_sep = p > c.p + c.n;" in corps
    assert "s.annee < 1970 ||" in corps and "s.annee > 2200" in corps
    for valeurs in ("1;2;3", "1;;3", "3.1;;", "", ";", "1;2;3;"):
        attendu = valeurs.split(";") if valeurs else []
        assert _champs_comme_le_firmware(valeurs, 30) == attendu, valeurs
    heures = ";".join(["0.1"] * 15 + [""] * 9)   # 14:37 : les heures 15 à 23 à venir
    assert len(_champs_comme_le_firmware(heures, 24)) == 24


def test_demo_dans_le_format():
    aujourd_hui = dt.date(2026, 6, 16)
    for vue, n in scenarios.ENERGIE_VUES.items():
        h = scenarios.build_energie_historique(vue, aujourd_hui)
        assert set(h) == {"vue", "debut", "valeurs"} and h["vue"] == vue
        assert len(h["valeurs"].split(";")) == n
        assert re.fullmatch(r"\d{4}-\d{2}-\d{2}", h["debut"])
    assert scenarios.build_energie_historique("jours", aujourd_hui)["debut"] == "2026-05-18"
    assert scenarios.build_energie_historique("mois", aujourd_hui)["debut"] == "2025-07-01"
    # ADR-0058 : dix champs pour le soleil (24 valeurs de prévision et de ciel clair), quatre pour
    # le bilan (autant de créneaux que la production de la même vue).
    soleil = scenarios.build_energie_soleil().split("|")
    assert len(soleil) == 10 and all(len(soleil[i].split(";")) == 24 for i in (8, 9))
    for vue, n in scenarios.ENERGIE_VUES.items():
        b = scenarios.build_energie_bilan(vue, aujourd_hui)
        devise, *series = b["payload"].split("|")
        assert devise == "€" and len(series) == 3 and all(len(s.split(";")) == n for s in series), vue
        assert b["debut"] == scenarios.build_energie_historique(vue, aujourd_hui)["debut"]
    # Falsifiable : un créneau qui n'est pas la meilleure plage, une série trop courte se voient.
    import pytest
    with pytest.raises(AssertionError):
        scenarios.build_energie_soleil({**scenarios.ENERGIE_SOLEIL, "creneau_debut": "9", "creneau_fin": "12"})
    with pytest.raises(AssertionError):
        scenarios.build_energie_soleil({**scenarios.ENERGIE_SOLEIL, "clair": ["0"] * 23})
    # La tuile solaire de la démo ouvre le popup.
    tuiles = [t for p in scenarios.PIECES.values() for t in p.tuiles.values() if "e" in t.options]
    assert tuiles and all(t.type == "cap" for t in tuiles)


# ─── Package : imitation de ce que ses modèles appellent dans HA ─────────────

class _Chargeur(BaseChargeur):
    pass


def _script():
    return yaml.load(_lire(PACKAGE), Loader=_Chargeur)["script"]["tab5_energie"]


def _etape_variables(nom):
    """Le bloc `variables:` de la boucle qui définit `nom`."""
    bloc = _chercher(_script()["sequence"], lambda d: nom in (d.get("variables") or {}))
    assert bloc, nom
    return bloc["variables"]


class EtatHA:
    def __init__(self, entity_id, state, age=600, **attributes):
        self.entity_id = entity_id
        self.state = state
        self.attributes = attributes
        self.last_changed = MAINTENANT - dt.timedelta(seconds=age)


def _analyser(brut):
    brut = brut.strip()
    try:
        valeur = ast.literal_eval(brut)
    except (ValueError, SyntaxError, TypeError, MemoryError):
        return brut
    return brut if isinstance(valeur, str) else valeur


def _env(etats, appareils=None):
    d = {e.entity_id: e for e in etats}
    appareils = appareils or {}

    class States:
        def __call__(self, e):
            return d[e].state if e in d else "unknown"

        def __getitem__(self, e):
            return d.get(e)

    def as_datetime(v):
        return v if isinstance(v, dt.datetime) else dt.datetime.fromisoformat(str(v))

    def as_local(v):
        return v.astimezone(PARIS)

    def as_timestamp(v):
        return as_datetime(v).timestamp()

    env = ImmutableSandboxedEnvironment(extensions=["jinja2.ext.loopcontrols"], undefined=jinja2.StrictUndefined)
    env.globals.update(
        states=States(), state_attr=lambda e, a: d[e].attributes.get(a) if e in d else None,
        now=lambda: MAINTENANT, timedelta=dt.timedelta, as_datetime=as_datetime, as_local=as_local,
        as_timestamp=as_timestamp, expand=lambda ids: [d[i] for i in ids if i in d],
        device_entities=lambda a: appareils.get(a, []),
    )
    env.filters.update(as_datetime=as_datetime, as_local=as_local, as_timestamp=as_timestamp)
    env.tests.update(match=lambda v, motif: bool(re.match(motif, str(v))))
    return env


def _rendre(env, valeur, ctx):
    if isinstance(valeur, str) and ("{{" in valeur or "{%" in valeur):
        return _analyser(env.from_string(valeur).render(ctx))
    return valeur


class Passe:
    """Un passage de la boucle de script.tab5_energie, réponses de get_statistics fournies."""

    def __init__(self, capteurs, etats, vue="heures", cinq=None, longues=None, index=1,
                 cinq_r=None, longues_r=None, trente=None, horaires=None):
        script = _script()
        self.env = _env(etats, {"tablette_1": ["sensor.tab5_ha_hmi_ecran_courant", "sensor.tab5_ha_hmi_uptime"]})
        self.ctx = {"appareil": "tablette_1", "tablette": "tab5_ha_hmi", "vue": vue, "capteurs": capteurs}
        for nom, v in script["variables"].items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)
        boucle = script["sequence"][0]["repeat"]
        self.boucle = boucle
        for nom, v in boucle["sequence"][0]["variables"].items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)
        if cinq is not None:
            self.ctx["cinq"] = cinq
        if cinq_r is not None:
            self.ctx["cinq_r"] = cinq_r
        self.ctx["repeat"] = {"index": index}
        for nom, v in _etape_variables("instantane").items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)
        self.longues = longues
        self.longues_r = longues_r
        self.trente = trente
        self.horaires = horaires

    def __getitem__(self, nom):
        return self.ctx[nom]

    def demandes_statistiques(self):
        """Les données des deux get_statistics, rendues."""
        appels = []

        def chercher(noeud):
            if isinstance(noeud, dict):
                if noeud.get("action") == "recorder.get_statistics":
                    try:
                        appels.append({k: _rendre_tout(self.env, v, self.ctx) for k, v in noeud["data"].items()})
                    except jinja2.UndefinedError:   # la seconde, avant historique() : sa date manque
                        appels.append(None)
                for v in noeud.values():
                    chercher(v)
            elif isinstance(noeud, list):
                for v in noeud:
                    chercher(v)
        chercher(self.boucle["sequence"])
        return appels

    def _branche_historique(self):
        branche = next(e for e in self.boucle["sequence"] if "if" in e and "then" in e
                       and "tab5_maj_energie_historique" in str(e["then"]))
        return branche, branche["then"][0]

    def historique(self):
        """(vue, debut, valeurs) poussés par la branche de l'historique, None si aucune."""
        branche, interne = self._branche_historique()
        if not _rendre(self.env, branche["if"], self.ctx):
            return None
        if _rendre(self.env, interne["if"], self.ctx):
            data = interne["then"][0]["data"]
        else:
            self._rendre_vue_longue()
            data = interne["else"][3]["data"]
        r = {k: _rendre(self.env, v, self.ctx) for k, v in data.items()}
        return r["vue"], str(r["debut"]), [_nombre(x) for x in str(r["valeurs"]).split(";")]

    def _rendre_vue_longue(self):
        """Les variables de la branche jours / mois, dans l'ordre où HA les rend."""
        _, interne = self._branche_historique()
        for nom, v in interne["else"][0]["variables"].items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)
        if self.longues is not None:
            self.ctx["longues"] = self.longues
        for nom, v in interne["else"][2]["variables"].items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)

    def heures_du_premier_passage(self):
        """Les heures poussées en plus au premier passage d'une vue jours / mois."""
        branche, interne = self._branche_historique()
        assert not _rendre(self.env, interne["if"], self.ctx)
        self._rendre_vue_longue()
        data = interne["else"][4]["data"]
        r = {k: _rendre(self.env, v, self.ctx) for k, v in data.items()}
        return r["vue"], str(r["debut"]), [_nombre(x) for x in str(r["valeurs"]).split(";")]

    def bilan(self):
        """(vue, debut, devise, vente, achat, gain) poussés par le bilan de la vue ; None si
        aucun (pas de compteur d'achat ni de vente, ou pas d'historique à ce passage)."""
        branche, interne = self._branche_historique()
        if not _rendre(self.env, branche["if"], self.ctx):
            return None
        if _rendre(self.env, interne["if"], self.ctx):
            suite = interne["then"][1]
        else:
            self._rendre_vue_longue()
            suite = interne["else"][5]
        if not _rendre(self.env, suite["if"], self.ctx):
            return None
        etapes = suite["then"]
        if etapes[0].get("action") == "recorder.get_statistics":
            if self.longues_r is not None:
                self.ctx["longues_r"] = self.longues_r
            etapes = etapes[1:]
        for nom, v in etapes[0]["variables"].items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)
        assert etapes[1]["action"] == "esphome.{{ tablette }}_tab5_maj_energie_bilan"
        assert etapes[1]["continue_on_error"] is True
        r = {k: _rendre(self.env, v, self.ctx) for k, v in etapes[1]["data"].items() if k != "payload"}
        devise, vente, achat, gain = str(self.ctx["bilan_payload"]).split("|")
        return r["vue"], str(r["debut"]), devise, _liste(vente), _liste(achat), _liste(gain)

    def _bloc_soleil(self):
        etape = _chercher(self.boucle["sequence"], lambda d: "soleil_cle != cle_heure" in str(d.get("if", "")))
        assert etape, "bloc du soleil introuvable"
        return etape

    def soleil_a_pousser(self):
        return _rendre(self.env, self._bloc_soleil()["if"], self.ctx)

    def soleil(self):
        """Les champs du payload de tab5_maj_energie_soleil (liste de 10 textes)."""
        etapes = self._bloc_soleil()["then"]
        if self.trente is not None:
            self.ctx["trente"] = self.trente
        if self.horaires is not None:
            self.ctx["horaires"] = self.horaires
        for e in etapes:
            if "variables" in e:
                for nom, v in e["variables"].items():
                    self.ctx[nom] = _rendre(self.env, v, self.ctx)
        action = etapes[-1]
        assert action["action"] == "esphome.{{ tablette }}_tab5_maj_energie_soleil"
        assert action["continue_on_error"] is True
        champs = str(self.ctx["so_payload"]).split("|")
        assert len(champs) == 10, champs
        return champs


def _liste(texte):
    """Une liste « a;b;;c » : nombres, None pour un champ vide ; [] pour un champ vide entier."""
    return [] if texte == "" else [_nombre(x) for x in texte.split(";")]


def _longue(p):
    """La demande jours / mois de get_statistics des productions (la première day / month)."""
    return next(d for d in p.demandes_statistiques() if d and d["period"] in ("day", "month"))


def _rendre_tout(env, valeur, ctx):
    if isinstance(valeur, list):
        return [_rendre_tout(env, v, ctx) for v in valeur]
    if isinstance(valeur, dict):
        return {k: _rendre_tout(env, v, ctx) for k, v in valeur.items()}
    return _rendre(env, valeur, ctx)


def _nombre(x):
    x = str(x)
    return None if x == "" else float(x)


# ─── Une installation simulée et ses statistiques ────────────────────────────

PROD = "sensor.solaire_energie"
COMPTEUR_DEBUT = 1000.0


def _prod_5min(t_local):
    """kWh produits pendant les 5 minutes qui commencent à t_local (une cloche 6 h - 21 h)."""
    h = t_local.hour + t_local.minute / 60
    if h < 6 or h >= 21:
        return 0.0
    return round(0.12 * (1 - abs(h - 13.5) / 7.5), 4)


def _lignes_5min(trous=()):
    """Lignes de 5 minutes depuis minuit local jusqu'à la dernière compilée (14:30), au
    format de HA. `trous` : débuts locaux (h, m) sans ligne (recorder arrêté)."""
    lignes, compteur = [], COMPTEUR_DEBUT
    t = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0)
    fin = MAINTENANT.replace(minute=30, second=0, microsecond=0)
    while t <= fin:
        c = _prod_5min(t)
        compteur += c
        if (t.hour, t.minute) not in trous:
            u = t.astimezone(dt.timezone.utc)
            lignes.append({"start": u.isoformat(), "end": (u + dt.timedelta(minutes=5)).isoformat(),
                           "change": c, "state": round(compteur, 4)})
        t += dt.timedelta(minutes=5)
    return lignes


def _reponse(lignes, statistic_id=PROD):
    return {"statistics": {statistic_id: lignes}}


def _lignes_longues(debuts_locaux, valeurs):
    return [{"start": d.astimezone(dt.timezone.utc).isoformat(), "change": v} for d, v in zip(debuts_locaux, valeurs)]


PARTIEL = 0.0425   # kWh produits depuis la dernière ligne de 5 minutes


def _maison(**surcharges):
    lignes = _lignes_5min()
    etats = {
        "sensor.solaire_puissance": EtatHA("sensor.solaire_puissance", "1.45", unit_of_measurement="kW"),
        PROD: EtatHA(PROD, str(round(lignes[-1]["state"] + PARTIEL, 4)), unit_of_measurement="kWh"),
        "sensor.reseau": EtatHA("sensor.reseau", "-430", unit_of_measurement="W"),
        "sensor.batterie": EtatHA("sensor.batterie", "64", unit_of_measurement="%"),
        "sensor.batterie_puissance": EtatHA("sensor.batterie_puissance", "400", unit_of_measurement="W"),
        "sensor.batterie_temperature": EtatHA("sensor.batterie_temperature", "21.54", unit_of_measurement="°C"),
        "sensor.tab5_ha_hmi_ecran_courant": EtatHA("sensor.tab5_ha_hmi_ecran_courant", "Énergie"),
    }
    for e, v in surcharges.items():
        etats[e] = v
    return [v for v in etats.values() if v is not None]


CAPTEURS = {
    "solaire": "sensor.solaire_puissance", "production": PROD, "reseau": "sensor.reseau", "reseau_export": "",
    "maison": "", "batterie": "sensor.batterie", "batterie_puissance": "sensor.batterie_puissance",
    "batterie_temperature": "sensor.batterie_temperature", "reseau_inverse": False, "batterie_inverse": False,
}


# ─── Calcul indépendant (Python, sans rien reprendre des modèles) ────────────

def _attendu_jour(lignes, actuel):
    return sum(r["change"] for r in lignes) + (actuel - lignes[-1]["state"])


def _attendu_heures(lignes, actuel):
    par_heure = {}
    for r in lignes:
        h = dt.datetime.fromisoformat(r["start"]).astimezone(PARIS).hour
        par_heure[h] = par_heure.get(h, 0.0) + r["change"]
    partiel = actuel - lignes[-1]["state"]
    sortie = []
    for h in range(24):
        if h > MAINTENANT.hour:
            sortie.append(None)
        elif h == MAINTENANT.hour:
            sortie.append(par_heure.get(h, 0.0) + partiel)
        else:
            sortie.append(par_heure.get(h))
    return sortie


def _proche(obtenu, attendu):
    assert len(obtenu) == len(attendu), (len(obtenu), len(attendu))
    for i, (o, a) in enumerate(zip(obtenu, attendu)):
        if a is None:
            assert o is None, (i, o)
        else:
            assert o is not None and abs(o - a) < 0.002, (i, o, a)


# ─── Package : historique ────────────────────────────────────────────────────

def test_les_statistiques_demandees():
    p = Passe(CAPTEURS, _maison(), cinq=_reponse(_lignes_5min()))
    cinq = p.demandes_statistiques()[0]
    assert cinq["statistic_ids"] == [PROD] and cinq["period"] == "5minute"
    assert dt.datetime.fromisoformat(cinq["start_time"]) == MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0)
    assert set(cinq["types"]) == {"change", "state"} and cinq["units"] == {"energy": "kWh"}


def test_production_du_jour_et_barres_des_heures():
    lignes = _lignes_5min()
    p = Passe(CAPTEURS, _maison(), cinq=_reponse(lignes))
    actuel = lignes[-1]["state"] + PARTIEL
    assert abs(p["jour"] - _attendu_jour(lignes, actuel)) < 0.002
    vue, debut, valeurs = p.historique()
    assert (vue, debut) == ("heures", "2026-06-16")
    _proche(valeurs, _attendu_heures(lignes, actuel))
    assert valeurs[5] == 0.0 and valeurs[15] is None


def test_heures_sans_ligne_restent_vides():
    """Recorder arrêté de 09:00 à 09:55 : l'heure 9 est vide, pas 0."""
    trous = {(9, m) for m in range(0, 60, 5)}
    lignes = _lignes_5min(trous)
    etats = _maison(**{PROD: EtatHA(PROD, str(round(lignes[-1]["state"] + PARTIEL, 4)), unit_of_measurement="kWh")})
    p = Passe(CAPTEURS, etats, cinq=_reponse(lignes))
    _, _, valeurs = p.historique()
    assert valeurs[9] is None and valeurs[8] is not None
    _proche(valeurs, _attendu_heures(lignes, lignes[-1]["state"] + PARTIEL))


def test_compteur_en_wh_et_compteur_remis_a_zero():
    lignes = _lignes_5min()
    en_wh = EtatHA(PROD, str((lignes[-1]["state"] + PARTIEL) * 1000), unit_of_measurement="Wh")
    p = Passe(CAPTEURS, _maison(**{PROD: en_wh}), cinq=_reponse(lignes))
    assert abs(p["partiel"] - PARTIEL) < 0.002
    # Remis à zéro depuis la dernière ligne : l'état actuel est la production depuis.
    remis = EtatHA(PROD, "0.03", unit_of_measurement="kWh")
    p = Passe(CAPTEURS, _maison(**{PROD: remis}), cinq=_reponse(lignes))
    assert abs(p["partiel"] - 0.03) < 1e-6


def test_trente_jours():
    lignes = _lignes_5min()
    aujourd_hui = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0)
    premier = aujourd_hui - dt.timedelta(days=29)
    debuts = [premier + dt.timedelta(days=i) for i in range(29) if i != 10]   # 1 jour sans ligne
    valeurs = [round(10 + i * 0.7, 3) for i in range(len(debuts))]
    # HA donne aussi la ligne d'aujourd'hui, partielle : la production du jour la remplace.
    longues = _reponse(_lignes_longues(debuts + [aujourd_hui], valeurs + [0.5]))
    p = Passe(CAPTEURS, _maison(), vue="jours", cinq=_reponse(lignes), longues=longues)
    vue, debut, obtenu = p.historique()
    assert (vue, debut) == ("jours", "2026-05-18")
    attendu = [None] * 30
    for d, v in zip(debuts, valeurs):
        attendu[(d.date() - premier.date()).days] = v
    attendu[29] = _attendu_jour(lignes, lignes[-1]["state"] + PARTIEL)
    _proche(obtenu, attendu)
    longue = _longue(p)
    assert longue["period"] == "day" and longue["statistic_ids"] == [PROD]
    assert dt.datetime.fromisoformat(longue["start_time"]) == premier


def test_douze_mois():
    lignes = _lignes_5min()
    debuts = [dt.datetime(2025 + (6 + m) // 12, (6 + m) % 12 + 1, 1, tzinfo=PARIS) for m in range(12)]
    assert debuts[0] == dt.datetime(2025, 7, 1, tzinfo=PARIS) and debuts[-1] == dt.datetime(2026, 6, 1, tzinfo=PARIS)
    valeurs = [round(400 + 30 * m, 1) for m in range(12)]
    p = Passe(CAPTEURS, _maison(), vue="mois", cinq=_reponse(lignes),
              longues=_reponse(_lignes_longues(debuts, valeurs)))
    vue, debut, obtenu = p.historique()
    assert (vue, debut) == ("mois", "2025-07-01")
    # Mois en cours : les heures complètes (ligne du mois) + les 5 minutes de l'heure en
    # cours + le partiel.
    heure = MAINTENANT.replace(minute=0, second=0, microsecond=0)
    cette_heure = sum(r["change"] for r in lignes if dt.datetime.fromisoformat(r["start"]) >= heure)
    attendu = valeurs[:11] + [valeurs[11] + cette_heure + PARTIEL]
    _proche(obtenu, attendu)
    assert _longue(p)["period"] == "month"


def test_mois_sans_reponse_du_recorder():
    """get_statistics en échec (continue_on_error) : barres vides, pas d'erreur de modèle."""
    p = Passe(CAPTEURS, _maison(), vue="jours", cinq=_reponse(_lignes_5min()))
    p.ctx.pop("longues", None)
    _, _, obtenu = p.historique()
    assert obtenu[:29] == [None] * 29 and obtenu[29] is not None


def test_historique_seulement_au_premier_passage_hors_heures():
    p = Passe(CAPTEURS, _maison(), vue="jours", cinq=_reponse(_lignes_5min()), index=2)
    assert p.historique() is None
    p = Passe(CAPTEURS, _maison(), vue="heures", cinq=_reponse(_lignes_5min()), index=3)
    assert p.historique()[0] == "heures"


def _redemander(**ctx):
    """La condition qui relance recorder.get_statistics (5 minutes) dans la boucle."""
    etape = _chercher(_script()["sequence"], lambda d: "if" in d and any(
        a.get("action") == "recorder.get_statistics" for a in d.get("then", []) if isinstance(a, dict)))
    assert etape, "get_statistics de 5 minutes hors d'un `if`"
    minuit = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0).isoformat()
    base = {"stats_t": 0, "stats_minuit": "", "minuit": minuit}
    return _rendre(_env(_maison()), etape["if"], {**base, **ctx})


def test_statistiques_redemandees_au_plus_toutes_les_5_minutes():
    """Audit du 07/10/2026 (PERF-4) : popup ouvert, la requête au recorder n'est refaite
    qu'au premier tour, après 5 minutes ou au changement de jour ; entre deux, la réponse
    précédente sert (le partiel couvre ce qui a été produit depuis)."""
    minuit = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0).isoformat()
    t = MAINTENANT.timestamp()
    cinq = _reponse(_lignes_5min())
    assert _redemander() is True                                   # premier tour : pas de réponse
    assert _redemander(cinq=cinq, stats_t=t - 5, stats_minuit=minuit) is False
    assert _redemander(cinq=cinq, stats_t=t - 299, stats_minuit=minuit) is False
    assert _redemander(cinq=cinq, stats_t=t - 300, stats_minuit=minuit) is True
    veille = (MAINTENANT - dt.timedelta(days=1)).replace(hour=0, minute=0, second=0, microsecond=0).isoformat()
    assert _redemander(cinq=cinq, stats_t=t - 5, stats_minuit=veille) is True
    # Variables de départ du script, et leur mise à jour juste après la demande.
    script = _script()
    assert script["variables"]["stats_t"] == 0 and script["variables"]["stats_minuit"] == ""
    etape = _chercher(script["sequence"], lambda d: "if" in d and any(
        a.get("action") == "recorder.get_statistics" for a in d.get("then", []) if isinstance(a, dict)))
    assert set(etape["then"][1]["variables"]) == {"stats_t", "stats_minuit"}
    # L'instantané, lui, reste rendu et poussé à chaque tour, hors de ce `if`.
    boucle = script["sequence"][0]["repeat"]["sequence"]
    assert any(e.get("action") == "esphome.{{ tablette }}_tab5_maj_energie" for e in boucle)


def test_jour_exact_avec_une_reponse_ancienne():
    """Réponse vieille de quelques minutes : la production du jour reste la même, le
    partiel (état actuel − dernier « state » de la réponse) couvrant la suite."""
    lignes = _lignes_5min()
    complet = Passe(CAPTEURS, _maison(), cinq=_reponse(lignes))
    ancien = Passe(CAPTEURS, _maison(), cinq=_reponse(lignes[:-2]))
    # Arrondi au Wh près (round(3)) de part et d'autre.
    assert abs(float(ancien["jour"]) - float(complet["jour"])) < 0.002


def test_sans_capteur_d_energie_ni_historique_ni_jour():
    capteurs = dict(CAPTEURS, production="")
    p = Passe(capteurs, _maison(), cinq=_reponse([], "sensor.solaire_puissance"))
    assert p.historique() is None and p["jour"] == ""
    # get_statistics est quand même appelé (même niveau que ce qui le lit), sur un capteur
    # choisi, jamais sur un identifiant inventé.
    assert p.demandes_statistiques()[0]["statistic_ids"] == ["sensor.solaire_puissance"]


# ─── Package : instantané ────────────────────────────────────────────────────

def _champs(p):
    return p["instantane"].split("|")


def test_instantane_complet():
    p = Passe(CAPTEURS, _maison(), cinq=_reponse(_lignes_5min()))
    s, m, r, b, bp, t, u, j = _champs(p)
    # 1.45 kW = 1450 W ; maison = 1450 + (−430) − 400 = 620.
    assert (s, m, r, b, bp, t, u) == ("1450", "620", "-430", "64.0", "400", "21.5", "°C")
    assert abs(float(j) - p["jour"]) < 1e-9


@pytest.mark.parametrize("surcharges, capteurs, attendu", [
    # Compteur qui sépare achat et vente : réseau = achat − vente.
    ({"sensor.reseau": EtatHA("sensor.reseau", "0", unit_of_measurement="W"),
      "sensor.vente": EtatHA("sensor.vente", "0.43", unit_of_measurement="kW")},
     {"reseau_export": "sensor.vente"}, {"reseau": "-430", "maison": "620"}),
    # Vente seule.
    ({"sensor.vente": EtatHA("sensor.vente", "430", unit_of_measurement="W")},
     {"reseau": "", "reseau_export": "sensor.vente"}, {"reseau": "-430", "maison": "620"}),
    # Signes inversés par les cases du blueprint.
    ({"sensor.reseau": EtatHA("sensor.reseau", "430", unit_of_measurement="W"),
      "sensor.batterie_puissance": EtatHA("sensor.batterie_puissance", "-400", unit_of_measurement="W")},
     {"reseau_inverse": True, "batterie_inverse": True}, {"reseau": "-430", "batterie_puissance": "400", "maison": "620"}),
    # Maison choisie : prise telle quelle.
    ({"sensor.maison": EtatHA("sensor.maison", "700", unit_of_measurement="W")},
     {"maison": "sensor.maison"}, {"maison": "700"}),
    # Jamais négative : vente plus forte que la production (batterie qui se vide vers le réseau).
    ({"sensor.reseau": EtatHA("sensor.reseau", "-3000", unit_of_measurement="W")},
     {}, {"maison": "0"}),
    # Indisponible : « nan » pour lui et pour la maison calculée.
    ({"sensor.reseau": EtatHA("sensor.reseau", "unavailable")},
     {}, {"reseau": "nan", "maison": "nan"}),
    # Batterie absente : la maison se calcule sans elle.
    ({}, {"batterie": "", "batterie_puissance": "", "batterie_temperature": ""},
     {"batterie": "", "batterie_puissance": "", "batterie_temperature": "", "unite_temperature": "", "maison": "1020"}),
])
def test_instantane_variantes(surcharges, capteurs, attendu):
    p = Passe(dict(CAPTEURS, **capteurs), _maison(**surcharges), cinq=_reponse(_lignes_5min()))
    obtenu = dict(zip(scenarios.ENERGIE_CHAMPS, _champs(p)))
    for champ, valeur in attendu.items():
        assert obtenu[champ] == valeur, (champ, obtenu)
    scenarios.build_energie_payload(obtenu)   # format de la démo et du firmware


def test_solaire_seul():
    capteurs = {k: "" for k in CAPTEURS if not k.endswith("inverse")}
    capteurs["solaire"] = "sensor.solaire_puissance"
    p = Passe(capteurs, _maison(), cinq=_reponse([], "sensor.solaire_puissance"))
    assert p["instantane"] == "1450|||||||"


def test_get_statistics_en_echec_jour_inconnu():
    p = Passe(CAPTEURS, _maison())
    assert p["jour"] == "nan" and _champs(p)[7] == "nan"


# ─── Package : plusieurs sources solaires (discussion #278, 05/10/2026) ──────
# « pv1 + pv2 + … » : le blueprint envoie solaire / production (la première entité, ce
# que lit un package plus ancien) et solaires / productions (toutes). Valeurs attendues
# écrites à la main.

PV2 = "sensor.pv2_puissance"
PV3 = "sensor.pv3_puissance"


def _avec_sources(*solaires, **autres):
    """Capteurs au format du blueprint actuel : solaire = la première, solaires = toutes."""
    return dict(CAPTEURS, solaire=solaires[0] if solaires else "", solaires=list(solaires),
                productions=[PROD], **autres)


@pytest.mark.parametrize("etats, solaire, maison", [
    # 1450 W (1.45 kW) + 850 W = 2300 W ; maison = 2300 − 430 − 400 = 1470.
    ({PV2: EtatHA(PV2, "850", unit_of_measurement="W")}, "2300", "1470"),
    # 1450 W + 0.35 kW = 1800 W ; maison = 1800 − 430 − 400 = 970.
    ({PV2: EtatHA(PV2, "0.35", unit_of_measurement="kW")}, "1800", "970"),
    # Un onduleur éteint la nuit : ignoré, 1450 seul ; maison = 620 comme avec une source.
    ({PV2: EtatHA(PV2, "unavailable")}, "1450", "620"),
    ({PV2: EtatHA(PV2, "unknown", unit_of_measurement="W")}, "1450", "620"),
])
def test_deux_puissances_additionnees(etats, solaire, maison):
    p = Passe(_avec_sources("sensor.solaire_puissance", PV2), _maison(**etats), cinq=_reponse(_lignes_5min()))
    obtenu = dict(zip(scenarios.ENERGIE_CHAMPS, _champs(p)))
    assert (obtenu["solaire"], obtenu["maison"]) == (solaire, maison)
    scenarios.build_energie_payload(obtenu)


def test_trois_puissances_dont_une_absente_de_ha():
    # 1450 + 200 = 1650 W ; PV3 n'existe pas (supprimée) : ignorée comme une indisponible.
    etats = _maison(**{PV2: EtatHA(PV2, "0.2", unit_of_measurement="kW")})
    p = Passe(_avec_sources("sensor.solaire_puissance", PV2, PV3), etats, cinq=_reponse(_lignes_5min()))
    assert _champs(p)[0] == "1650"


def test_toutes_les_puissances_indisponibles_inconnu():
    etats = _maison(**{"sensor.solaire_puissance": EtatHA("sensor.solaire_puissance", "unavailable"),
                       PV2: EtatHA(PV2, "unknown")})
    p = Passe(_avec_sources("sensor.solaire_puissance", PV2), etats, cinq=_reponse(_lignes_5min()))
    s, m = _champs(p)[:2]
    assert (s, m) == ("nan", "nan"), "toutes indisponibles : inconnu, comme avec une seule source"


@pytest.mark.parametrize("capteurs", [
    # Blueprint d'avant : une chaîne dans solaire, ni solaires ni productions.
    CAPTEURS,
    # Blueprint actuel avec une seule source.
    dict(CAPTEURS, solaires=["sensor.solaire_puissance"], productions=[PROD]),
    # Listes seules, ou une liste dans le champ d'une entité (script appelé à la main).
    dict(CAPTEURS, solaire="", production="", solaires=["sensor.solaire_puissance"], productions=[PROD]),
    dict(CAPTEURS, solaire=["sensor.solaire_puissance"], production=[PROD]),
    # Listes vides à côté de la chaîne.
    dict(CAPTEURS, solaires=[], productions=[]),
])
def test_une_seule_source_meme_resultat_qu_avant(capteurs):
    lignes = _lignes_5min()
    p = Passe(capteurs, _maison(), cinq=_reponse(lignes))
    s, m, r, b, bp, t, u, j = _champs(p)
    # Les valeurs de test_instantane_complet, d'avant les sources multiples.
    assert (s, m, r, b, bp, t, u) == ("1450", "620", "-430", "64.0", "400", "21.5", "°C")
    assert abs(float(j) - _attendu_jour(lignes, lignes[-1]["state"] + PARTIEL)) < 0.002
    assert p.demandes_statistiques()[0]["statistic_ids"] == [PROD]
    _, _, valeurs = p.historique()
    _proche(valeurs, _attendu_heures(lignes, lignes[-1]["state"] + PARTIEL))


# Deux compteurs, peu de lignes, tout à la main. HA convertit les statistiques en kWh
# (units: energy: kWh) : les lignes du compteur en Wh arrivent en kWh, son ÉTAT reste en
# Wh (le partiel le convertit).
PROD2 = "sensor.pv2_energie"


def _ligne(h, m, change, state):
    debut = MAINTENANT.replace(hour=h, minute=m, second=0, microsecond=0).astimezone(dt.timezone.utc)
    return {"start": debut.isoformat(), "end": (debut + dt.timedelta(minutes=5)).isoformat(),
            "change": change, "state": state}


def _deux_compteurs(vue="heures", longues=None):
    cinq = {"statistics": {
        # Compteur 1 (kWh, parti de 1000) : 08:00, 08:05, 09:00 ; dernier state 1000.6.
        PROD: [_ligne(8, 0, 0.10, 1000.1), _ligne(8, 5, 0.20, 1000.3), _ligne(9, 0, 0.30, 1000.6)],
        # Compteur 2 (Wh, lignes en kWh, parti de 50) : 08:00 et 10:00, rien à 9 h ; dernier state 50.45.
        PROD2: [_ligne(8, 0, 0.05, 50.05), _ligne(10, 0, 0.40, 50.45)],
    }}
    etats = _maison(**{PROD: EtatHA(PROD, "1000.65", unit_of_measurement="kWh"),
                       PROD2: EtatHA(PROD2, "50500", unit_of_measurement="Wh")})
    capteurs = dict(CAPTEURS, solaires=["sensor.solaire_puissance"], productions=[PROD, PROD2])
    return Passe(capteurs, etats, vue=vue, cinq=cinq, longues=longues)


def test_deux_compteurs_kwh_et_wh_heure_par_heure():
    p = _deux_compteurs()
    assert p.demandes_statistiques()[0]["statistic_ids"] == [PROD, PROD2]
    # Partiel : (1000.65 − 1000.6) + (50500 Wh = 50.5 − 50.45) = 0.05 + 0.05 = 0.10.
    assert abs(p["partiel"] - 0.10) < 1e-6
    # Jour : 0.10 + 0.20 + 0.30 + 0.05 + 0.40 + 0.10 = 1.15.
    assert abs(p["jour"] - 1.15) < 1e-6 and abs(float(_champs(p)[7]) - 1.15) < 1e-6
    vue, _, valeurs = p.historique()
    attendu = [None] * 24
    attendu[8] = 0.35    # 0.10 + 0.20 + 0.05
    attendu[9] = 0.30    # compteur 1 seul : le 2 n'a pas de ligne, il compte pour 0
    attendu[10] = 0.40   # compteur 2 seul
    attendu[14] = 0.10   # heure en cours : le partiel des deux
    assert vue == "heures"
    _proche(valeurs, attendu)


def test_deux_compteurs_jour_par_jour():
    premier = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0) - dt.timedelta(days=29)
    jour = lambda i: (premier + dt.timedelta(days=i)).astimezone(dt.timezone.utc).isoformat()  # noqa: E731
    longues = {"statistics": {
        PROD: [{"start": jour(0), "change": 10.0}, {"start": jour(5), "change": 12.0}],
        PROD2: [{"start": jour(5), "change": 3.0}, {"start": jour(7), "change": 4.0}],
    }}
    p = _deux_compteurs("jours", longues)
    _, debut, valeurs = p.historique()
    attendu = [None] * 30
    attendu[0] = 10.0     # compteur 1 seul
    attendu[5] = 15.0     # 12 + 3
    attendu[7] = 4.0      # compteur 2 seul
    attendu[29] = 1.15    # aujourd'hui : la production du jour des deux
    assert debut == "2026-05-18"
    _proche(valeurs, attendu)
    assert _longue(p)["statistic_ids"] == [PROD, PROD2]


def test_deux_compteurs_mois_par_mois():
    mois = lambda a, m: dt.datetime(a, m, 1, tzinfo=PARIS).astimezone(dt.timezone.utc).isoformat()  # noqa: E731
    longues = {"statistics": {
        PROD: [{"start": mois(2025, 7), "change": 400.0}, {"start": mois(2026, 6), "change": 200.0}],
        PROD2: [{"start": mois(2025, 7), "change": 100.0}, {"start": mois(2025, 10), "change": 50.0}],
    }}
    p = _deux_compteurs("mois", longues)
    _, debut, valeurs = p.historique()
    attendu = [None] * 12
    attendu[0] = 500.0    # juillet 2025 : 400 + 100
    attendu[3] = 50.0     # octobre 2025 : compteur 2 seul
    # Juin 2026 (en cours) : 200 + aucune ligne de 5 minutes depuis 14:00 + partiel 0.10.
    attendu[11] = 200.10
    assert debut == "2025-07-01"
    _proche(valeurs, attendu)


# ─── Package : fin de la boucle ──────────────────────────────────────────────

@pytest.mark.parametrize("ecran, age, attendu", [
    ("Énergie", 30, False),        # popup ouvert : on continue
    ("Accueil", 30, True),         # fermé, après la grâce : fin
    ("Accueil", 5, False),         # grâce : l'« Écran courant » a 5 s de retard
    ("Énergie", 1000, True),       # 15 minutes au plus
])
def test_fin_de_boucle(ecran, age, attendu):
    etats = _maison(**{"sensor.tab5_ha_hmi_ecran_courant": EtatHA("sensor.tab5_ha_hmi_ecran_courant", ecran)})
    p = Passe(CAPTEURS, etats, cinq=_reponse(_lignes_5min()))
    p.ctx["debut"] = MAINTENANT.timestamp() - age
    assert _rendre(p.env, p.boucle["until"], p.ctx) is attendu


def test_sans_ecran_courant_un_seul_passage():
    etats = _maison(**{"sensor.tab5_ha_hmi_ecran_courant": None})
    p = Passe(CAPTEURS, etats, cinq=_reponse(_lignes_5min()))
    p.env.globals["device_entities"] = lambda a: []
    p.ctx["ecran"] = _rendre(p.env, _script()["variables"]["ecran"], p.ctx)
    assert p["ecran"] == "" and _rendre(p.env, p.boucle["until"], p.ctx) is True


# ─── Blueprint ───────────────────────────────────────────────────────────────

def _maison_bp():
    return [
        Etat("sensor.solaire_puissance", "1450", "Bureau", friendly_name="Production solaire",
             unit_of_measurement="W", device_class="power"),
        Etat(PROD, "1234.5", friendly_name="Énergie solaire", unit_of_measurement="kWh", device_class="energy"),
        Etat("sensor.temp_bureau", "21.4", "Bureau", friendly_name="Température bureau",
             unit_of_measurement="°C", device_class="temperature"),
    ]


PIECE = {"piece_1_nom": "Bureau", "piece_1_tuiles": ["sensor.solaire_puissance", "sensor.temp_bureau"]}
REMPLIE = dict(PIECE, energie_solaire="sensor.solaire_puissance", energie_production=PROD,
               energie_reseau_inverse=True)


def _branche_energie(p):
    branche = _chercher(p.corps["actions"], lambda d: d.get("alias") == "La tablette ouvre le popup Énergie")
    assert branche, "branche « popup Énergie » du blueprint introuvable"
    return branche


def _tuiles(p):
    return {d[0]: d for d in _defs(p.definitions()) if d[0].startswith("t")}


def test_blueprint_section_vide_rien_ne_change():
    p = Passage(PIECE, _maison_bp(), _evenement("energie", vue="heures"))
    assert p["energie_capteurs"] == []
    tuiles = _tuiles(p)
    assert "e" not in tuiles["t00"][3] and tuiles["t00"][2] != "solaire"
    branche = _branche_energie(p)
    assert p.modele(branche["conditions"])
    etape = branche["sequence"][0]
    assert p.modele(etape["if"]) is True
    assert etape["then"][0]["action"] == "esphome.{{ tablette }}_tab5_maj_energie"
    assert etape["then"][0]["data"]["payload"] == "|" * 7


def test_blueprint_section_remplie():
    p = Passage(REMPLIE, _maison_bp(), _evenement("energie", vue="jours", device_id="tablette_1"))
    assert sorted(p["energie_capteurs"]) == sorted(["sensor.solaire_puissance", PROD])
    tuiles = _tuiles(p)
    assert "e" in tuiles["t00"][3] and tuiles["t00"][2] == "solaire" and tuiles["t00"][1] == "cap"
    assert "e" not in tuiles["t01"][3], "une tuile hors de la section ne prend pas e"
    etape = _branche_energie(p)["sequence"][0]
    assert p.modele(etape["if"]) is False
    appel = etape["else"][0]
    p.etats.d["script.tab5_energie"] = Etat("script.tab5_energie", "off")
    assert p.modele(appel["if"]) is True
    turn_on = appel["then"][0]
    assert turn_on["action"] == "script.turn_on" and turn_on["target"]["entity_id"] == "script.tab5_energie"
    p.env.filters["bool"] = lambda v, defaut=None: v if isinstance(v, bool) else defaut
    variables = {k: p.modele(v) for k, v in turn_on["data"]["variables"].items()}
    assert variables["vue"] == "jours" and variables["appareil"] == "tablette_1"
    capteurs = variables["capteurs"]
    assert capteurs["solaire"] == "sensor.solaire_puissance" and capteurs["production"] == PROD
    assert capteurs["reseau"] == "" and capteurs["reseau_inverse"] is True and capteurs["batterie_inverse"] is False
    # solaires / productions : toutes les sources (une ici) ; solaire / production restent
    # la première, pour un package tab5_energie plus ancien.
    assert capteurs["solaires"] == ["sensor.solaire_puissance"] and capteurs["productions"] == [PROD]
    assert set(capteurs) == set(CAPTEURS) | {"solaires", "productions"} | NOUVELLES_CLES, \
        "le script et le blueprint ne parlent pas des mêmes capteurs"


def test_blueprint_sans_le_package_ne_lance_rien():
    p = Passage(REMPLIE, _maison_bp(), _evenement("energie", vue="heures"))
    assert p.modele(_branche_energie(p)["sequence"][0]["else"][0]["if"]) is False


def _capteurs_envoyes(p):
    turn_on = _branche_energie(p)["sequence"][0]["else"][0]["then"][0]
    p.env.filters["bool"] = lambda v, defaut=None: v if isinstance(v, bool) else defaut
    return p.modele(turn_on["data"]["variables"]["capteurs"])


def _maison_bp_pv():
    return _maison_bp() + [
        Etat("sensor.pv2_puissance", "800", "Garage", friendly_name="Onduleur 2",
             unit_of_measurement="W", device_class="power"),
        Etat("sensor.pv2_energie", "512.3", friendly_name="Énergie onduleur 2",
             unit_of_measurement="kWh", device_class="energy"),
    ]


def test_blueprint_plusieurs_sources():
    """Les autres sources : option e et icône solaire sur leur tuile, toutes envoyées au
    script, la première restant dans solaire / production (package plus ancien)."""
    entrees = dict(REMPLIE, piece_1_tuiles=["sensor.solaire_puissance", "sensor.temp_bureau", "sensor.pv2_puissance"],
                   energie_solaire_autres=["sensor.pv2_puissance"], energie_production_autres=["sensor.pv2_energie"])
    p = Passage(entrees, _maison_bp_pv(), _evenement("energie", vue="heures", device_id="tablette_1"))
    assert p["energie_solaires"] == ["sensor.solaire_puissance", "sensor.pv2_puissance"]
    assert p["energie_productions"] == [PROD, "sensor.pv2_energie"]
    assert sorted(p["energie_capteurs"]) == sorted(["sensor.solaire_puissance", PROD, "sensor.pv2_puissance",
                                                    "sensor.pv2_energie"])
    tuiles = _tuiles(p)
    assert "e" in tuiles["t02"][3] and tuiles["t02"][2] == "solaire", "la tuile de la 2e source ouvre le popup"
    assert "e" in tuiles["t00"][3] and "e" not in tuiles["t01"][3]
    p.etats.d["script.tab5_energie"] = Etat("script.tab5_energie", "off")
    capteurs = _capteurs_envoyes(p)
    assert capteurs["solaire"] == "sensor.solaire_puissance" and capteurs["production"] == PROD
    assert capteurs["solaires"] == ["sensor.solaire_puissance", "sensor.pv2_puissance"]
    assert capteurs["productions"] == [PROD, "sensor.pv2_energie"]


@pytest.mark.parametrize("entrees, solaires, principal", [
    # Automatisation d'avant : une chaîne, pas de champ « autres » (défaut []).
    ({"energie_solaire": "sensor.solaire_puissance"}, ["sensor.solaire_puissance"], "sensor.solaire_puissance"),
    # Champ vide, autres seuls.
    ({"energie_solaire_autres": ["sensor.pv2_puissance"]}, ["sensor.pv2_puissance"], ""),
    # Une liste dans le champ principal (YAML écrit à la main), doublon avec « autres ».
    ({"energie_solaire": ["sensor.solaire_puissance", "sensor.pv2_puissance"],
      "energie_solaire_autres": ["sensor.pv2_puissance"]}, ["sensor.solaire_puissance", "sensor.pv2_puissance"],
     "sensor.solaire_puissance"),
    # Une chaîne dans « autres ».
    ({"energie_solaire": "sensor.solaire_puissance", "energie_solaire_autres": "sensor.pv2_puissance"},
     ["sensor.solaire_puissance", "sensor.pv2_puissance"], "sensor.solaire_puissance"),
    ({}, [], ""),
])
def test_blueprint_chaine_liste_ou_vide(entrees, solaires, principal):
    p = Passage(dict(PIECE, **entrees), _maison_bp_pv(), _evenement("energie", vue="heures"))
    assert p["energie_solaires"] == solaires
    assert p["energie"]["solaire"] == principal


def test_blueprint_ecoute_l_evenement():
    texte = _lire(BLUEPRINT)
    assert re.search(r"event_type: esphome\.tab5_energie\n\s+id: energie\n", texte)
    # « Rien de neuf » et « tablette connectée » laissent passer « energie » ; la garde
    # d'origine suit le type de l'événement (custom_templates/tab5_tablette.jinja, HA-7).
    assert texte.count("'maj_ecran', 'energie'") == 1 and "'pipeline_discussion', 'energie'" in texte


# ─── ADR-0058 : le soleil, la prévision du jour et le bilan ──────────────────
# Statistiques horaires et prévisions SIMULÉES (l'auteur n'a pas de panneaux) ; les
# valeurs attendues viennent d'un calcul Python écrit à part, sans rien reprendre des
# modèles. Format des prévisions horaires relevé sur une entité météo réelle (datetime
# UTC, condition, parfois pas de cloud_coverage, pas de 3 h puis 6 h plus loin).

METEO = "weather.maison"
SOLEIL = "sun.sun"
PREV_JOUR = "sensor.forecast_energie_aujourd_hui"
PREV_DEMAIN = "sensor.forecast_energie_demain"

# Lever, midi, coucher du 16 juin 2026 à Paris (UTC) ; un jour plus tard : 1 minute de plus
# au coucher, 1 de moins au lever.
SOLEIL_UTC = {"rising": dt.time(3, 47), "noon": dt.time(11, 52), "setting": dt.time(19, 56)}


def _utc_iso(d):
    return d.astimezone(dt.timezone.utc).isoformat()


def _evenement_soleil(nom, jour, decalage_minutes=0):
    h = SOLEIL_UTC[nom]
    base = dt.datetime.combine(jour, h, tzinfo=dt.timezone.utc)
    return (base + dt.timedelta(minutes=decalage_minutes)).isoformat()


def _etat_soleil(maintenant=None):
    """sun.sun tel que HA le donne : les PROCHAINS lever, midi et coucher."""
    maintenant = maintenant or MAINTENANT
    aujourd_hui = maintenant.astimezone(dt.timezone.utc).date()
    attributs = {}
    for nom, cle in (("rising", "next_rising"), ("noon", "next_noon"), ("setting", "next_setting")):
        passe = dt.datetime.combine(aujourd_hui, SOLEIL_UTC[nom], tzinfo=dt.timezone.utc)
        demain = passe <= maintenant
        attributs[cle] = _evenement_soleil(nom, aujourd_hui + dt.timedelta(days=1 if demain else 0),
                                           (-1 if nom == "rising" else 1) if demain else 0)
    return EtatHA(SOLEIL, "above_horizon", **attributs)


def _etats_meteo(fonctions=3):
    return {
        "sensor.tab5_meteo": EtatHA("sensor.tab5_meteo", "cloudy", entite_effective=METEO),
        METEO: EtatHA(METEO, "cloudy", supported_features=fonctions),
    }


def _base(h):
    """kWh d'une heure locale par ciel clair (cloche 6 h - 21 h, pic à 13 h)."""
    return 0.0 if h < 6 or h >= 21 else round(3.0 * math.sin(math.pi * (h + 0.5 - 6) / 15), 4)


def _facteur_jour(d):
    """Part du ciel clair du jour d (−30 = il y a 30 jours, 0 = aujourd'hui) : une pointe à
    1,5 (un nuage qui réfléchit) au jour −12, sinon des jours de 0,2 à 1."""
    return 1.5 if d == -12 else [1.0, 0.35, 0.8, 1.0, 0.2, 0.6, 0.9][(d + 30) % 7]


def _horaire(decalage, h, valeur):
    jour = (MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0) + dt.timedelta(days=decalage)).replace(hour=h)
    return {"start": _utc_iso(jour), "end": _utc_iso(jour + dt.timedelta(hours=1)), "change": valeur}


def _stats_30_jours(compteurs=(PROD,), facteur=_facteur_jour, jours=range(-30, 1), poison=True):
    """Statistiques `hour` des 30 derniers jours, au format de HA. Aujourd'hui : les heures
    complètes seulement, plus (poison) une ligne de l'heure en cours qui doit être écartée."""
    stats = {}
    for i, c in enumerate(compteurs):
        lignes = []
        for d in jours:
            for h in range(24):
                if d == 0 and h > MAINTENANT.hour:
                    continue
                if d == 0 and h == MAINTENANT.hour and not poison:
                    continue
                v = round(_base(h) * facteur(d) * (1 if i == 0 else 0.5), 4)
                if d == 0 and h == MAINTENANT.hour:
                    v = 9.9
                lignes.append(_horaire(d, h, v))
        stats[c] = lignes
    return {"statistics": stats}


def _clair_attendu(compteurs=(PROD,), facteur=_facteur_jour):
    """Pour chaque heure locale, la 2e plus haute valeur des 30 jours (somme des compteurs)."""
    sortie = []
    for h in range(24):
        valeurs = []
        for d in range(-30, 1):
            if d == 0 and h >= MAINTENANT.hour:
                continue
            valeurs.append(sum(round(_base(h) * facteur(d) * (1 if i == 0 else 0.5), 4) for i in range(len(compteurs))))
        valeurs.sort(reverse=True)
        sortie.append(valeurs[1])
    return sortie


CONDITION_NUAGES = {"sunny": 0, "clear-night": 0, "partlycloudy": 50, "cloudy": 90, "rainy": 100, "fog": 100,
                    "lightning": 100, "pouring": 100, "snowy": 100}


def _entree_prevision(quand, condition, nuages=None):
    e = {"datetime": _utc_iso(quand), "condition": condition, "temperature": 18.0, "humidity": 70}
    if nuages is not None:
        e["cloud_coverage"] = nuages
    return e


def _heure_locale(decalage, h):
    return (MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0) + dt.timedelta(days=decalage)).replace(hour=h)


def _previsions_horaires():
    """15 h aujourd'hui à 23 h demain, heure par heure : des entrées avec cloud_coverage,
    d'autres seulement avec une condition."""
    entrees = []
    for decalage, plage in ((0, range(15, 24)), (1, range(0, 24))):
        for h in plage:
            quand = _heure_locale(decalage, h)
            if decalage == 0 and h < 18:
                entrees.append(_entree_prevision(quand, "cloudy"))                  # sans cloud_coverage : 90
            elif decalage == 0 and h < 20:
                entrees.append(_entree_prevision(quand, "partlycloudy"))            # 50
            elif decalage == 0:
                entrees.append(_entree_prevision(quand, "sunny", 20))               # cloud_coverage : 20
            else:
                entrees.append(_entree_prevision(quand, "cloudy", (h * 7) % 101))   # 0 à 100
    return entrees


def _reponse_meteo(entrees):
    return {METEO: {"forecast": entrees}}


def _nuage_attendu(entrees, quand):
    """Nuages (%) de l'heure `quand` : l'entrée la plus récente qui la précède de moins de 6 h."""
    candidats = []
    for e in entrees:
        t = dt.datetime.fromisoformat(e["datetime"])
        n = e["cloud_coverage"] if "cloud_coverage" in e else CONDITION_NUAGES.get(e["condition"])
        if n is not None and t <= quand and quand - t < dt.timedelta(hours=6):
            candidats.append((t, n))
    return max(candidats)[1] if candidats else None


def _prevu_attendu(clair, entrees, decalage=0):
    sortie = []
    for h in range(24):
        n = _nuage_attendu(entrees, _heure_locale(decalage, h)) if entrees else None
        sortie.append(clair[h] * (1 - 0.75 * (n / 100) ** 3.4) if n is not None else clair[h])
    return sortie


def _creneau_attendu(prevu, clair, heure, coucher_h):
    meilleur = None
    for a in range(heure, 22):
        if a + 3 <= coucher_h:
            s = sum(prevu[a:a + 3])
            if meilleur is None or s > meilleur[1]:
                meilleur = (a, s)
    if meilleur and meilleur[1] >= 0.05 * max(clair):
        return meilleur[0], meilleur[0] + 3
    return None


def _passe_soleil(horaires=None, trente=None, extra=(), capteurs=None, **surcharges):
    etats = _maison(**{SOLEIL: _etat_soleil(), **_etats_meteo(), **surcharges})
    p = Passe(capteurs or CAPTEURS, etats + list(extra), cinq=_reponse(_lignes_5min()),
              trente=_stats_30_jours() if trente is None else trente,
              horaires=_reponse_meteo(_previsions_horaires()) if horaires is None else horaires)
    return p


def _proches(obtenu, attendu, tol=0.001):
    assert len(obtenu) == len(attendu), (len(obtenu), len(attendu))
    for i, (o, a) in enumerate(zip(obtenu, attendu)):
        assert o is not None and abs(o - a) < tol, (i, o, a)


def _nombres(texte):
    return [float(x) for x in texte.split(";")] if texte else []


# Champs de tab5_maj_energie_soleil : lever|midi|coucher|prevu_jour|prevu_demain|source|
# creneau_debut|creneau_fin|prevu|clair
LEVER, MIDI, COUCHER, PREVU_JOUR, PREVU_DEMAIN, SOURCE, CRENEAU_DEBUT, CRENEAU_FIN, PREVU, CLAIR = range(10)


def test_soleil_lever_midi_coucher_locaux():
    c = _passe_soleil().soleil()
    # Après le coucher d'hier et avant celui d'aujourd'hui : lever et midi sont ceux de
    # demain (1 minute de décalage), le coucher est celui d'aujourd'hui.
    assert c[LEVER:COUCHER + 1] == ["05:46", "13:53", "21:56"]


def test_soleil_avant_le_lever_tout_est_aujourd_hui(monkeypatch):
    monkeypatch.setattr(sys.modules[__name__], "MAINTENANT",
                        dt.datetime(2026, 6, 16, 4, 10, 0, tzinfo=PARIS))
    etats = _maison(**{SOLEIL: _etat_soleil(), **_etats_meteo()})
    p = Passe(CAPTEURS, etats, trente=_stats_30_jours(), horaires=_reponse_meteo([]))
    assert p.soleil()[LEVER:COUCHER + 1] == ["05:47", "13:52", "21:56"]


@pytest.mark.parametrize("etat, attendu", [
    (None, ["", "", ""]),                                                                    # pas de sun.sun
    (EtatHA(SOLEIL, "above_horizon", next_rising="2026-07-20T01:00:00+00:00",
            next_noon="2026-06-17T11:52:00+00:00", next_setting="2026-06-16T19:56:00+00:00"),
     ["", "", ""]),                                                                          # nuit polaire : lever dans plus d'un jour
    (EtatHA(SOLEIL, "above_horizon", next_rising="2026-06-17T03:47:00+00:00",
            next_noon="2026-06-17T11:52:00+00:00", next_setting="2026-06-17T01:00:00+00:00"),
     ["", "", ""]),                                                                          # coucher avant le lever : incohérent
    (EtatHA(SOLEIL, "above_horizon"), ["", "", ""]),                                          # attributs absents
])
def test_soleil_inconnu_champs_vides(etat, attendu):
    p = _passe_soleil(**{SOLEIL: etat})
    assert p.soleil()[LEVER:COUCHER + 1] == attendu


def test_courbe_ciel_clair_apprise():
    c = _passe_soleil().soleil()
    attendu = _clair_attendu()
    _proches(_nombres(c[CLAIR]), attendu)
    # À la main : la pointe de 1,5 ne compte pas, la 2e plus haute est un jour clair (1,0).
    assert abs(_nombres(c[CLAIR])[13] - _base(13)) < 0.001
    # La nuit : 0 ; l'heure en cours (14 h) ignore la ligne partielle de 9,9 kWh.
    assert _nombres(c[CLAIR])[3] == 0 and _nombres(c[CLAIR])[14] < 3.1


def test_courbe_somme_des_compteurs():
    deux = (PROD, PROD2)
    p = _passe_soleil(trente=_stats_30_jours(deux), extra=[EtatHA(PROD2, "500", unit_of_measurement="kWh")],
                      capteurs=dict(CAPTEURS, productions=list(deux)))
    _proches(_nombres(p.soleil()[CLAIR]), _clair_attendu(deux))


def test_courbe_pas_apprise_avec_moins_de_trois_jours():
    deux_jours = _stats_30_jours(jours=range(-1, 1))
    c = _passe_soleil(trente=deux_jours).soleil()
    assert c[CLAIR] == "" and c[PREVU] == "" and c[PREVU_JOUR] == "" and c[SOURCE] == ""
    assert (c[CRENEAU_DEBUT], c[CRENEAU_FIN]) == ("", "")


def test_courbe_sans_reponse_du_recorder_ou_sans_compteur():
    p = _passe_soleil(trente={})
    assert p.soleil()[CLAIR] == ""
    sans = _passe_soleil(capteurs=dict(CAPTEURS, production="", productions=[]), trente={})
    c = sans.soleil()
    assert c[CLAIR] == "" and c[LEVER] != ""        # l'arc du soleil se passe de compteur


def test_prevision_kasten_czeplak():
    entrees = _previsions_horaires()
    c = _passe_soleil().soleil()
    clair = _clair_attendu()
    prevu = _nombres(c[PREVU])
    _proches(prevu, _prevu_attendu(clair, entrees))
    # À la main : 16 h = condition « cloudy » sans cloud_coverage = 90 % ;
    # 20 h = cloud_coverage 20 % ; 8 h (matin, avant la prévision) = ciel clair.
    assert abs(prevu[16] - clair[16] * (1 - 0.75 * 0.9 ** 3.4)) < 0.001
    assert abs(prevu[20] - clair[20] * (1 - 0.75 * 0.2 ** 3.4)) < 0.001
    assert prevu[8] == pytest.approx(clair[8], abs=0.001)
    assert c[SOURCE] == "a"


def test_prevision_demain_et_total_du_jour():
    entrees = _previsions_horaires()
    clair = _clair_attendu()
    p = _passe_soleil()
    c = p.soleil()
    demain = sum(_prevu_attendu(clair, entrees, decalage=1))
    assert abs(float(c[PREVU_DEMAIN]) - demain) < 0.01
    # Total du jour : le modèle, mais les heures déjà passées valent la production réelle.
    reel = _attendu_heures(_lignes_5min(), _lignes_5min()[-1]["state"] + PARTIEL)
    modele = _prevu_attendu(clair, entrees)
    total = sum(reel[h] if h < MAINTENANT.hour and reel[h] is not None else modele[h] for h in range(24))
    assert abs(float(c[PREVU_JOUR]) - total) < 0.02


def test_prevision_pas_de_3_puis_6_heures():
    """Les entrées passent à 3 h puis 6 h : une heure sans entrée prolonge la précédente."""
    entrees = []
    for h in range(15, 24):
        entrees.append(_entree_prevision(_heure_locale(0, h), "sunny"))
    for h in range(0, 24, 3):                                          # demain : toutes les 3 h
        entrees.append(_entree_prevision(_heure_locale(1, h), "rainy" if 9 <= h <= 11 else "sunny"))
    clair = _clair_attendu()
    c = _passe_soleil(horaires=_reponse_meteo(entrees)).soleil()
    # 9, 10 et 11 h demain : « rainy » (100 %) de l'entrée de 9 h ; 12 h : « sunny ».
    _proches([float(c[PREVU_DEMAIN])], [sum(_prevu_attendu(clair, entrees, decalage=1))], tol=0.01)
    pluvieux = sum(clair[h] * 0.25 for h in (9, 10, 11))
    clair_h = sum(clair[h] for h in range(24) if h not in (9, 10, 11))
    assert abs(float(c[PREVU_DEMAIN]) - (pluvieux + clair_h)) < 0.01


def test_prevision_sans_horaire_prevu_egal_clair():
    # Entité sans prévision horaire (daily seul) : pas d'appel, prévision = courbe apprise.
    p = _passe_soleil(horaires={}, **_etats_meteo(fonctions=1))
    assert p["meteo_prevision"] == ""
    c = p.soleil()
    _proches(_nombres(c[PREVU]), _clair_attendu())
    assert c[SOURCE] == "a"
    # Avec la prévision horaire : l'entité de la prévision est trouvée.
    assert _passe_soleil()["meteo_prevision"] == METEO


def test_prevision_entite_de_repli_et_sans_meteo():
    # sensor.tab5_meteo absent : la source choisie de sensor.tab5_sources_meteo.
    etats = {"sensor.tab5_meteo": None,
             "sensor.tab5_sources_meteo": EtatHA("sensor.tab5_sources_meteo", "ok", meteo=METEO),
             METEO: EtatHA(METEO, "sunny", supported_features=3)}
    assert _passe_soleil(**etats)["meteo_prevision"] == METEO
    rien = {"sensor.tab5_meteo": None, "sensor.tab5_sources_meteo": None, METEO: None}
    p = _passe_soleil(horaires={}, **rien)
    assert p["meteo_prevision"] == ""
    _proches(_nombres(p.soleil()[PREVU]), _clair_attendu())


@pytest.mark.parametrize("horaires", [None, {}, {METEO: {"forecast": []}}, {"weather.autre": {"forecast": []}}])
def test_prevision_reponse_meteo_absente_ou_vide(horaires):
    c = _passe_soleil(horaires=horaires if horaires is not None else {}).soleil()
    _proches(_nombres(c[PREVU]), _clair_attendu())


def test_prevision_externe_met_la_forme_a_l_echelle():
    capteurs = dict(CAPTEURS, prevision_jour=PREV_JOUR, prevision_demain=PREV_DEMAIN)
    ext = [EtatHA(PREV_JOUR, "17800", unit_of_measurement="Wh"), EtatHA(PREV_DEMAIN, "12.1", unit_of_measurement="kWh")]
    c = _passe_soleil(capteurs=capteurs, extra=ext).soleil()
    forme = _prevu_attendu(_clair_attendu(), _previsions_horaires())
    echelle = 17.8 / sum(forme)
    _proches(_nombres(c[PREVU]), [v * echelle for v in forme])
    assert abs(sum(_nombres(c[PREVU])) - 17.8) < 0.05
    assert float(c[PREVU_JOUR]) == pytest.approx(17.8, abs=0.005) and float(c[PREVU_DEMAIN]) == pytest.approx(12.1, abs=0.005)
    assert c[SOURCE] == "e"


def test_prevision_externe_sans_valeur():
    capteurs = dict(CAPTEURS, prevision_jour=PREV_JOUR, prevision_demain=PREV_DEMAIN)
    ext = [EtatHA(PREV_JOUR, "unavailable"), EtatHA(PREV_DEMAIN, "unknown")]
    # Une forme apprise : le modèle sert, comme sans capteur.
    c = _passe_soleil(capteurs=capteurs, extra=ext).soleil()
    sans = _passe_soleil().soleil()
    assert (c[PREVU_JOUR], c[PREVU_DEMAIN], c[SOURCE]) == (sans[PREVU_JOUR], sans[PREVU_DEMAIN], "a")
    # Rien d'appris : « nan », le capteur est choisi mais n'a pas de valeur.
    c = _passe_soleil(capteurs=capteurs, extra=ext, trente={}).soleil()
    assert (c[PREVU_JOUR], c[PREVU_DEMAIN], c[PREVU], c[CLAIR], c[SOURCE]) == ("nan", "nan", "", "", "")
    # Capteur valide mais rien d'appris : le total seul.
    ext = [EtatHA(PREV_JOUR, "9.5", unit_of_measurement="kWh"), EtatHA(PREV_DEMAIN, "unknown")]
    c = _passe_soleil(capteurs=capteurs, extra=ext, trente={}).soleil()
    assert (c[PREVU_JOUR], c[PREVU_DEMAIN], c[PREVU], c[SOURCE]) == ("9.5", "nan", "", "e")


def test_meilleur_creneau_apres_midi():
    c = _passe_soleil().soleil()
    clair = _clair_attendu()
    prevu = _prevu_attendu(clair, _previsions_horaires())
    attendu = _creneau_attendu(prevu, clair, MAINTENANT.hour, 22)     # coucher 21:56 : jusqu'à 22 h
    assert attendu and (int(c[CRENEAU_DEBUT]), int(c[CRENEAU_FIN])) == attendu
    # À la main : la production décroît après 13 h, donc dès 14 h ; 14 h → 17 h.
    assert (c[CRENEAU_DEBUT], c[CRENEAU_FIN]) == ("14", "17")


def test_meilleur_creneau_le_matin_autour_du_pic(monkeypatch):
    monkeypatch.setattr(sys.modules[__name__], "MAINTENANT",
                        dt.datetime(2026, 6, 16, 9, 20, 0, tzinfo=PARIS))
    etats = _maison(**{SOLEIL: _etat_soleil(), **_etats_meteo()})
    stats = _stats_30_jours(poison=False)
    # Ciel clair, pas de prévision : la fenêtre de 3 h la plus forte entoure le pic de 13 h.
    stats["statistics"][PROD] = [r for r in stats["statistics"][PROD]
                                 if dt.datetime.fromisoformat(r["start"]) < MAINTENANT.replace(minute=0, second=0)]
    p = Passe(CAPTEURS, etats, trente=stats, horaires=_reponse_meteo([]))
    c = p.soleil()
    assert (c[CRENEAU_DEBUT], c[CRENEAU_FIN]) == ("12", "15")


def test_meilleur_creneau_aucun():
    # 20 h 05 : il reste moins de 3 heures avant le coucher (21 h 56) ; 22 h 30 : nuit.
    for heure, minute in ((20, 5), (22, 30)):
        quand = MAINTENANT.replace(hour=heure, minute=minute)
        p = _passe_soleil()
        p.env.globals["now"] = lambda quand=quand: quand
        p.ctx["cle_heure"] = quand.strftime("%Y-%m-%dT%H")
        c = p.soleil()
        assert (c[CRENEAU_DEBUT], c[CRENEAU_FIN]) == ("", ""), heure


def test_meilleur_creneau_cinq_pour_cent():
    # La prévision externe réduit le jour à presque rien : sous 5 % de l'heure la plus forte.
    capteurs = dict(CAPTEURS, prevision_jour=PREV_JOUR)
    ext = [EtatHA(PREV_JOUR, "0.1", unit_of_measurement="kWh")]
    c = _passe_soleil(capteurs=capteurs, extra=ext).soleil()
    assert (c[CRENEAU_DEBUT], c[CRENEAU_FIN]) == ("", "")
    ext = [EtatHA(PREV_JOUR, "20", unit_of_measurement="kWh")]
    c = _passe_soleil(capteurs=capteurs, extra=ext).soleil()
    assert c[CRENEAU_DEBUT] == "14"


def test_soleil_une_fois_par_heure():
    p = _passe_soleil()
    assert p["soleil_cle"] == "" and p["cle_heure"] == "2026-06-16T14"
    assert p.soleil_a_pousser() is True                    # premier tour
    p.ctx["soleil_cle"] = "2026-06-16T14"
    assert p.soleil_a_pousser() is False                   # même heure : rien
    p.ctx["soleil_cle"] = "2026-06-16T13"
    assert p.soleil_a_pousser() is True                    # l'heure a changé
    # Après la poussée, la clé prend l'heure.
    p.soleil()
    assert p["soleil_cle"] == "2026-06-16T14"


def test_les_deux_nouvelles_actions_continuent_en_cas_d_echec():
    """Un firmware plus ancien n'a pas ces actions : sans continue_on_error, HA arrêterait
    le script et les poussées suivantes ne partiraient jamais."""
    trouvees = []

    def parcourir(noeud):
        if isinstance(noeud, dict):
            a = noeud.get("action")
            if isinstance(a, str) and a.endswith(("_tab5_maj_energie_soleil", "_tab5_maj_energie_bilan")):
                trouvees.append(noeud)
            for v in noeud.values():
                parcourir(v)
        elif isinstance(noeud, list):
            for v in noeud:
                parcourir(v)
    parcourir(_script()["sequence"])
    assert len(trouvees) == 3 and all(n.get("continue_on_error") is True for n in trouvees)
    for appel in trouvees:    # appelées sans lire la réponse
        assert "response_variable" not in appel


def test_heures_poussees_au_premier_tour_quelle_que_soit_la_vue():
    lignes = _lignes_5min()
    actuel = lignes[-1]["state"] + PARTIEL
    for vue, longues in (("jours", _reponse([])), ("mois", _reponse([]))):
        p = Passe(CAPTEURS, _maison(), vue=vue, cinq=_reponse(lignes), longues=longues)
        vue_poussee, debut, valeurs = p.heures_du_premier_passage()
        assert (vue_poussee, debut) == ("heures", "2026-06-16")
        _proche(valeurs, _attendu_heures(lignes, actuel))
        # Deuxième passage : ni les heures ni la vue ne sont repoussées.
        assert Passe(CAPTEURS, _maison(), vue=vue, cinq=_reponse(lignes), index=2).historique() is None


# ─── Bilan : achat, vente, prix, gains ───────────────────────────────────────

ACHAT = "sensor.reseau_energie_achetee"
VENTE = "sensor.reseau_energie_vendue"
TARIF = "sensor.tarif_kwh"


def _vente_5min(t):
    h = t.hour + t.minute / 60
    return round(0.03 * (1 - abs(h - 13.5) / 3.5), 4) if 10 <= h < 17 else 0.0


def _achat_5min(t):
    h = t.hour + t.minute / 60
    return 0.015 if (h < 6 or h >= 18) else (0.004 if h < 10 else 0.0)


def _lignes_compteur(f, depart):
    lignes, compteur = [], depart
    t = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0)
    fin = MAINTENANT.replace(minute=30, second=0, microsecond=0)
    while t <= fin:
        c = f(t)
        compteur += c
        u = t.astimezone(dt.timezone.utc)
        lignes.append({"start": u.isoformat(), "end": (u + dt.timedelta(minutes=5)).isoformat(),
                       "change": c, "state": round(compteur, 4)})
        t += dt.timedelta(minutes=5)
    return lignes


PARTIEL_ACHAT, PARTIEL_VENTE = 0.011, 0.006


def _bilan_des_heures(capteurs, **surcharges):
    prod = _lignes_5min()
    achat, vente = _lignes_compteur(_achat_5min, 300.0), _lignes_compteur(_vente_5min, 80.0)
    etats = _maison(**{
        ACHAT: EtatHA(ACHAT, str(round(achat[-1]["state"] + PARTIEL_ACHAT, 4)), unit_of_measurement="kWh"),
        VENTE: EtatHA(VENTE, str(round((vente[-1]["state"] + PARTIEL_VENTE) * 1000, 1)), unit_of_measurement="Wh"),
        TARIF: EtatHA(TARIF, "0.2516", unit_of_measurement="EUR/kWh"),
        **surcharges})
    cinq_r = {"statistics": {ACHAT: achat, VENTE: vente}}
    return Passe(capteurs, etats, cinq=_reponse(prod), cinq_r=cinq_r), prod, achat, vente


CAPTEURS_BILAN = dict(CAPTEURS, compteur_achat=ACHAT, compteur_vente=VENTE, prix_achat=0.2, prix_revente=0.1,
                      devise="€")


def _attendu_gains(production, vente, prix_achat, prix_revente):
    return [None if p is None or v is None else max(p - v, 0) * prix_achat + v * prix_revente
            for p, v in zip(production, vente)]


def test_bilan_des_heures():
    p, prod, achat, vente = _bilan_des_heures(CAPTEURS_BILAN)
    vue, debut, devise, v, a, g = p.bilan()
    assert (vue, debut, devise) == ("heures", "2026-06-16", "€")
    att_vente = _attendu_heures(vente, vente[-1]["state"] + PARTIEL_VENTE)
    att_achat = _attendu_heures(achat, achat[-1]["state"] + PARTIEL_ACHAT)
    _proche(v, att_vente)       # le compteur de vente est en Wh : converti en kWh
    _proche(a, att_achat)
    att_prod = _attendu_heures(prod, prod[-1]["state"] + PARTIEL)
    _proche(g, _attendu_gains(att_prod, att_vente, 0.2, 0.1))
    assert g[15] is None and v[15] is None and g[12] is not None
    # À la main : 3 h, rien produit, rien vendu : 0 de gain.
    assert g[3] == 0 and v[3] == 0 and a[3] > 0


def test_bilan_le_prix_d_un_capteur_l_emporte():
    capteurs = dict(CAPTEURS_BILAN, prix_achat_entite=TARIF, prix_revente_entite="sensor.inexistant")
    p, prod, achat, vente = _bilan_des_heures(capteurs)
    _, _, devise, v, _, g = p.bilan()
    att_vente = _attendu_heures(vente, vente[-1]["state"] + PARTIEL_VENTE)
    att_prod = _attendu_heures(prod, prod[-1]["state"] + PARTIEL)
    # Achat : le capteur (0,2516) ; revente : le capteur n'existe pas, le nombre (0,1).
    _proche(g, _attendu_gains(att_prod, att_vente, 0.2516, 0.1))
    # Un capteur indisponible : le nombre.
    p, *_ = _bilan_des_heures(capteurs, **{TARIF: EtatHA(TARIF, "unavailable")})
    _proche(p.bilan()[5], _attendu_gains(att_prod, att_vente, 0.2, 0.1))


@pytest.mark.parametrize("devise, attendu", [("", "€"), ("CHF", "CHF"), ("$", "$"), ("EURO!", "EUR"), ("a|b;c", "abc"),
                                             ("  £ ", "£")])
def test_bilan_la_devise(devise, attendu):
    p, *_ = _bilan_des_heures(dict(CAPTEURS_BILAN, devise=devise))
    assert p.bilan()[2] == attendu


def test_bilan_sans_prix_pas_de_devise_ni_de_gain():
    p, prod, achat, vente = _bilan_des_heures(dict(CAPTEURS_BILAN, prix_achat=0, prix_revente=0))
    _, _, devise, v, a, g = p.bilan()
    assert devise == "" and g == [] and len(v) == 24 and len(a) == 24
    # Un seul prix suffit : la vente seule, par exemple.
    p, *_ = _bilan_des_heures(dict(CAPTEURS_BILAN, prix_achat=0))
    _, _, devise, v, _, g = p.bilan()
    att_prod = _attendu_heures(prod, prod[-1]["state"] + PARTIEL)
    att_vente = _attendu_heures(vente, vente[-1]["state"] + PARTIEL_VENTE)
    assert devise == "€"
    _proche(g, _attendu_gains(att_prod, att_vente, 0, 0.1))


def test_bilan_un_seul_compteur():
    # Vente seule : le champ achat est vide, les gains comptent tout ce qui n'est pas vendu.
    p, prod, achat, vente = _bilan_des_heures(dict(CAPTEURS_BILAN, compteur_achat=""))
    _, _, devise, v, a, g = p.bilan()
    assert a == [] and len(v) == 24 and devise == "€"
    # Achat seul : pas de vente, les gains valent la production au prix d'achat.
    p, prod, *_ = _bilan_des_heures(dict(CAPTEURS_BILAN, compteur_vente=""))
    _, _, devise, v, a, g = p.bilan()
    assert v == [] and len(a) == 24
    att_prod = _attendu_heures(prod, prod[-1]["state"] + PARTIEL)
    _proche(g, [None if x is None else x * 0.2 for x in att_prod])


def test_bilan_un_compteur_inexistant_est_ignore():
    p, *_ = _bilan_des_heures(dict(CAPTEURS_BILAN, compteur_vente="sensor.n_existe_pas"))
    _, _, _, v, a, _ = p.bilan()
    assert v == [] and len(a) == 24


def test_bilan_rien_sans_compteur_ni_pour_un_blueprint_plus_ancien():
    # Blueprint d'avant l'ADR-0058 : aucune des clés. Tout continue comme avant.
    p = Passe(CAPTEURS, _maison(), cinq=_reponse(_lignes_5min()))
    assert p["reseau_ids"] == {"achat": "", "vente": ""} and p["compteurs_reseau"] == []
    assert p["devise"] == "" and p.bilan() is None
    assert p.historique()[0] == "heures"
    # Les prix sans compteur n'y changent rien.
    p = Passe(dict(CAPTEURS, prix_achat=0.2, devise="€"), _maison(), cinq=_reponse(_lignes_5min()))
    assert p.bilan() is None
    # Le soleil, lui, se calcule sans aucune des clés (pas de prévision externe).
    c = _passe_soleil().soleil()
    assert len(c) == 10 and c[SOURCE] == "a"


def test_bilan_les_statistiques_des_compteurs_sont_demandees():
    p, *_ = _bilan_des_heures(CAPTEURS_BILAN)
    demandes = [d for d in p.demandes_statistiques() if d]
    cinq_r = next(d for d in demandes if d["statistic_ids"] == [ACHAT, VENTE] and d["period"] == "5minute")
    assert set(cinq_r["types"]) == {"change", "state"} and cinq_r["units"] == {"energy": "kWh"}
    assert dt.datetime.fromisoformat(cinq_r["start_time"]) == MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0)
    # Sans compteur, pas de demande en plus au recorder pour le bilan.
    sans = Passe(CAPTEURS, _maison(), cinq=_reponse(_lignes_5min()))
    assert sans["compteurs_reseau"] == []


def _longues_reseau(premier, jours):
    """Lignes day ou month d'achat et de vente (kWh), quelques créneaux sans ligne."""
    def debut(i):
        if jours:
            return (premier + dt.timedelta(days=i)).astimezone(dt.timezone.utc).isoformat()
        m = premier.month - 1 + i
        return dt.datetime(premier.year + m // 12, m % 12 + 1, 1, tzinfo=PARIS).astimezone(dt.timezone.utc).isoformat()
    nb = 30 if jours else 12
    valeurs_a = {i: round(6 + 0.3 * i, 3) for i in range(nb) if i not in (4, 20)}
    valeurs_v = {i: round(2 + 0.2 * i, 3) for i in range(nb) if i != 7}
    return valeurs_a, valeurs_v, {"statistics": {
        ACHAT: [{"start": debut(i), "change": v} for i, v in valeurs_a.items()],
        VENTE: [{"start": debut(i), "change": v} for i, v in valeurs_v.items()]}}


def test_bilan_des_jours():
    premier = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0) - dt.timedelta(days=29)
    prod_debuts = [premier + dt.timedelta(days=i) for i in range(29) if i != 10]
    prod_vals = [round(10 + i * 0.7, 3) for i in range(len(prod_debuts))]
    longues = _reponse(_lignes_longues(prod_debuts, prod_vals))
    va, vv, longues_r = _longues_reseau(premier, True)
    prod, achat, vente = _lignes_5min(), _lignes_compteur(_achat_5min, 300.0), _lignes_compteur(_vente_5min, 80.0)
    etats = _maison(**{ACHAT: EtatHA(ACHAT, str(round(achat[-1]["state"] + PARTIEL_ACHAT, 4)), unit_of_measurement="kWh"),
                       VENTE: EtatHA(VENTE, str(round(vente[-1]["state"] + PARTIEL_VENTE, 4)), unit_of_measurement="kWh")})
    p = Passe(CAPTEURS_BILAN, etats, vue="jours", cinq=_reponse(prod), longues=longues,
              cinq_r={"statistics": {ACHAT: achat, VENTE: vente}}, longues_r=longues_r)
    vue, debut, devise, v, a, g = p.bilan()
    assert (vue, debut, devise) == ("jours", "2026-05-18", "€")
    # Aujourd'hui (dernier créneau) : lignes de 5 minutes + partiel, comme la production.
    jour_a = sum(r["change"] for r in achat) + PARTIEL_ACHAT
    jour_v = sum(r["change"] for r in vente) + PARTIEL_VENTE
    att_a = [va.get(i) for i in range(29)] + [jour_a]
    att_v = [vv.get(i) for i in range(29)] + [jour_v]
    _proche(a, att_a)
    _proche(v, att_v)
    att_prod = [None] * 30
    for d, x in zip(prod_debuts, prod_vals):
        att_prod[(d.date() - premier.date()).days] = x
    att_prod[29] = _attendu_jour(prod, prod[-1]["state"] + PARTIEL)
    _proche(g, _attendu_gains(att_prod, att_v, 0.2, 0.1))
    assert g[10] is None and g[7] is None, "production ou vente sans ligne : pas de gain"
    assert a[4] is None and v[7] is None
    # Le compteur d'achat n'entre pas dans les gains : seul ce qui est produit et vendu.
    assert g[5] == pytest.approx(max(att_prod[5] - att_v[5], 0) * 0.2 + att_v[5] * 0.1, abs=0.005)
    longue = _longue(p)
    assert longue["period"] == "day"
    demandes = [d for d in p.demandes_statistiques() if d and d["statistic_ids"] == [ACHAT, VENTE] and d["period"] == "day"]
    assert demandes and dt.datetime.fromisoformat(demandes[0]["start_time"]) == premier


def test_bilan_des_mois():
    premier = dt.datetime(2025, 7, 1, tzinfo=PARIS)
    prod_debuts = [dt.datetime(2025 + (6 + m) // 12, (6 + m) % 12 + 1, 1, tzinfo=PARIS) for m in range(12)]
    prod_vals = [round(400 + 30 * m, 1) for m in range(12)]
    longues = _reponse(_lignes_longues(prod_debuts, prod_vals))
    va, vv, longues_r = _longues_reseau(premier, False)
    prod, achat, vente = _lignes_5min(), _lignes_compteur(_achat_5min, 300.0), _lignes_compteur(_vente_5min, 80.0)
    etats = _maison(**{ACHAT: EtatHA(ACHAT, str(round(achat[-1]["state"] + PARTIEL_ACHAT, 4)), unit_of_measurement="kWh"),
                       VENTE: EtatHA(VENTE, str(round(vente[-1]["state"] + PARTIEL_VENTE, 4)), unit_of_measurement="kWh")})
    p = Passe(CAPTEURS_BILAN, etats, vue="mois", cinq=_reponse(prod), longues=longues,
              cinq_r={"statistics": {ACHAT: achat, VENTE: vente}}, longues_r=longues_r)
    vue, debut, devise, v, a, g = p.bilan()
    assert (vue, debut) == ("mois", "2025-07-01")
    heure = MAINTENANT.replace(minute=0, second=0, microsecond=0)
    # Mois en cours : la ligne du mois + les 5 minutes de l'heure en cours + le partiel.
    cette_heure_a = sum(r["change"] for r in achat if dt.datetime.fromisoformat(r["start"]) >= heure)
    cette_heure_v = sum(r["change"] for r in vente if dt.datetime.fromisoformat(r["start"]) >= heure)
    att_a = [va.get(i) for i in range(11)] + [va[11] + cette_heure_a + PARTIEL_ACHAT]
    att_v = [vv.get(i) for i in range(11)] + [vv[11] + cette_heure_v + PARTIEL_VENTE]
    _proche(a, att_a)
    _proche(v, att_v)
    att_prod = prod_vals[:11] + [prod_vals[11] + sum(r["change"] for r in prod if dt.datetime.fromisoformat(r["start"]) >= heure)
                                 + PARTIEL]
    _proche(g, _attendu_gains(att_prod, att_v, 0.2, 0.1))


def test_bilan_sans_reponse_du_recorder():
    prod = _lignes_5min()
    p = Passe(CAPTEURS_BILAN, _maison(**{ACHAT: EtatHA(ACHAT, "300", unit_of_measurement="kWh"),
                                         VENTE: EtatHA(VENTE, "80", unit_of_measurement="kWh")}),
              vue="jours", cinq=_reponse(prod), longues=_reponse([]))
    vue, debut, devise, v, a, g = p.bilan()
    assert len(v) == 30 and all(x is None for x in v) and all(x is None for x in a)
    assert all(x is None for x in g)


def test_bilan_un_creneau_de_vente_vide_donne_un_gain_vide():
    """Compteur de vente choisi mais sans ligne pour un jour : pas de gain inventé."""
    premier = MAINTENANT.replace(hour=0, minute=0, second=0, microsecond=0) - dt.timedelta(days=29)
    debuts = [premier + dt.timedelta(days=i) for i in range(29)]
    longues = _reponse(_lignes_longues(debuts, [5.0] * 29))
    _, _, longues_r = _longues_reseau(premier, True)
    p = Passe(CAPTEURS_BILAN, _maison(**{ACHAT: EtatHA(ACHAT, "300", unit_of_measurement="kWh"),
                                         VENTE: EtatHA(VENTE, "80", unit_of_measurement="kWh")}),
              vue="jours", cinq=_reponse(_lignes_5min()), longues=longues, longues_r=longues_r)
    g = p.bilan()[5]
    assert g[7] is None and g[8] is not None


# ─── Blueprint : les entrées de l'ADR-0058 ───────────────────────────────────

NOUVELLES_CLES = {"compteur_achat", "compteur_vente", "prix_achat", "prix_achat_entite", "prix_revente",
                  "prix_revente_entite", "devise", "prevision_jour", "prevision_demain"}


def _blueprint_etendu():
    return [
        *_maison_bp(),
        Etat(ACHAT, "300", friendly_name="Achat", unit_of_measurement="kWh", device_class="energy"),
        Etat(VENTE, "80", friendly_name="Vente", unit_of_measurement="kWh", device_class="energy"),
        Etat(TARIF, "0.25", friendly_name="Tarif", unit_of_measurement="EUR/kWh"),
        Etat(PREV_JOUR, "17.8", friendly_name="Prévision", unit_of_measurement="kWh", device_class="energy"),
        Etat(PREV_DEMAIN, "12.1", friendly_name="Prévision", unit_of_measurement="kWh", device_class="energy"),
    ]


def test_blueprint_envoie_les_nouvelles_entrees():
    entrees = dict(REMPLIE, energie_compteur_achat=ACHAT, energie_compteur_vente=VENTE, energie_prix_achat=0.2,
                   energie_prix_revente=0.1, energie_prix_achat_entite=TARIF, energie_devise="CHF",
                   energie_prevision_jour=PREV_JOUR, energie_prevision_demain=PREV_DEMAIN)
    p = Passage(entrees, _blueprint_etendu(), _evenement("energie", vue="jours", device_id="tablette_1"))
    p.etats.d["script.tab5_energie"] = Etat("script.tab5_energie", "off")
    capteurs = _capteurs_envoyes(p)
    assert set(capteurs) == set(CAPTEURS) | {"solaires", "productions"} | NOUVELLES_CLES
    assert capteurs["compteur_achat"] == ACHAT and capteurs["compteur_vente"] == VENTE
    assert capteurs["prix_achat"] == 0.2 and capteurs["prix_revente"] == 0.1
    assert capteurs["prix_achat_entite"] == TARIF and capteurs["prix_revente_entite"] == ""
    assert capteurs["devise"] == "CHF"
    assert capteurs["prevision_jour"] == PREV_JOUR and capteurs["prevision_demain"] == PREV_DEMAIN
    # Ces capteurs ne comptent pas comme « énergie choisie » : ni tuile e, ni popup rempli.
    assert sorted(p["energie_capteurs"]) == sorted(["sensor.solaire_puissance", PROD])


def test_blueprint_nouvelles_entrees_vides_par_defaut():
    p = Passage(REMPLIE, _maison_bp(), _evenement("energie", vue="heures", device_id="tablette_1"))
    p.etats.d["script.tab5_energie"] = Etat("script.tab5_energie", "off")
    capteurs = _capteurs_envoyes(p)
    for cle in ("compteur_achat", "compteur_vente", "prix_achat_entite", "prix_revente_entite", "devise",
                "prevision_jour", "prevision_demain"):
        assert capteurs[cle] == "", cle
    assert capteurs["prix_achat"] == 0 and capteurs["prix_revente"] == 0
    # Seuls ces compteurs, sans solaire ni réseau, ne rendent pas le popup « rempli ».
    p = Passage(dict(PIECE, energie_compteur_achat=ACHAT), _blueprint_etendu(), _evenement("energie", vue="heures"))
    assert p["energie_capteurs"] == []


def test_blueprint_les_entrees_et_les_cles_du_script_concordent():
    """Chaque clé facultative de `capteurs` que lit le script est envoyée par le blueprint,
    et le script ne lit aucune clé que le blueprint n'envoie pas."""
    lues = set(re.findall(r"c\.get\('(\w+)'", _lire(PACKAGE)))
    assert NOUVELLES_CLES <= lues
    assert lues <= set(CAPTEURS) | {"solaires", "productions"} | NOUVELLES_CLES
    entrees = _lire(BLUEPRINT)
    for nom in ("energie_compteur_achat", "energie_compteur_vente", "energie_prix_achat", "energie_prix_achat_entite",
                "energie_prix_revente", "energie_prix_revente_entite", "energie_devise", "energie_prevision_jour",
                "energie_prevision_demain"):
        assert f"        {nom}:\n" in entrees and f"!input {nom}" in entrees, nom


# ─── Firmware ────────────────────────────────────────────────────────────────

def test_option_e_du_firmware():
    # Les options et leurs lettres : l'en-tête des tuiles (lot L7, 08/10/2026).
    cpp = _lire(TUILES_CPP) + _lire(os.path.join(REPO, "Tab5", "ecran", "tab5_tuiles_priv.h"))
    lettres = re.search(r'kLettresOptions\[\] = "(\w+)"', cpp).group(1)
    assert lettres.index("e") == 7 and re.search(r"\bOPT_E = 128\b", cpp)
    # Un capteur n'agit qu'avec l'option e (gestes(), table kGestes) : le popup Énergie.
    assert "(type == Type::CAP && (d.options & OPT_E))" in cpp
    assert "{false, nullptr, false, Fenetre::ENERGIE},    // cap" in cpp
    assert re.search(r"d\.options & OPT_E\) && energie_formater\(", cpp)
    assert "g_tuiles_ui.energie_ouvrir()" in cpp
    assert lettres == scenarios.OPTIONS_TUILE


def test_popup_au_registre_et_au_select():
    navigation = _lire(os.path.join(REPO, "Tab5", "paquets", "tab5-navigation.yaml"))
    assert re.search(r'ModalRegistry::add\(id\(energie_popup\),\s+"Énergie",\s+ModalRegistry::POPUP,'
                     r'\s+\[\] \{ id\(tab5_energie_ouvrir\)\.execute\(\); \}\);', navigation)
    assert '- "Énergie"' in navigation
    # Le package attend exactement ce nom d'écran.
    assert "states(ecran) != 'Énergie'" in _lire(PACKAGE)
