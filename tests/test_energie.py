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
import os
import re
import sys
from zoneinfo import ZoneInfo

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

from tests.test_tuiles_blueprint import Etat, Passage, _chercher, _defs, _evenement

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACKAGE = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_energie.yaml")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
ENERGIE_CPP = os.path.join(REPO, "Tab5", "tab5_energie.cpp")
TUILES_CPP = os.path.join(REPO, "Tab5", "tab5_tuiles.cpp")
API = os.path.join(REPO, "Tab5", "tab5-api-logic.yaml")

sys.path.insert(0, os.path.join(REPO, "tools", "demo"))
import demo_pusher  # noqa: E402
import scenarios  # noqa: E402

PARIS = ZoneInfo("Europe/Paris")
# Un après-midi de juin, 14:37 à Paris (12:37 UTC) : les 5 minutes de 14:30 sont la
# dernière ligne compilée, celles de 14:35 pas encore.
MAINTENANT = dt.datetime(2026, 6, 16, 14, 37, 20, tzinfo=PARIS)


def _lire(chemin):
    with open(chemin, encoding="utf-8") as f:
        return f.read()


# ─── Contrat ─────────────────────────────────────────────────────────────────

def test_actions_du_firmware():
    contrat = demo_pusher.lire_contrat()
    assert contrat[demo_pusher.SERVICE_ENERGIE] == ("payload",)
    assert contrat[demo_pusher.SERVICE_ENERGIE_HISTORIQUE] == ("vue", "debut", "valeurs")


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
    """Miroir de la boucle d'energie_historique() (champ_suivant + apres_sep)."""
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
    assert "apres_sep = p > d + n;" in corps
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
    # La tuile solaire de la démo ouvre le popup.
    tuiles = [t for p in scenarios.PIECES.values() for t in p.tuiles.values() if "e" in t.options]
    assert tuiles and all(t.type == "cap" for t in tuiles)


# ─── Package : imitation de ce que ses modèles appellent dans HA ─────────────

class _Chargeur(yaml.SafeLoader):
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

    def __init__(self, capteurs, etats, vue="heures", cinq=None, longues=None, index=1):
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
        self.ctx["repeat"] = {"index": index}
        for nom, v in _etape_variables("instantane").items():
            self.ctx[nom] = _rendre(self.env, v, self.ctx)
        self.longues = longues

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

    def historique(self):
        """(vue, debut, valeurs) poussés par la branche de l'historique, None si aucune."""
        branche = next(e for e in self.boucle["sequence"] if "if" in e and "then" in e
                       and "tab5_maj_energie_historique" in str(e["then"]))
        if not _rendre(self.env, branche["if"], self.ctx):
            return None
        interne = branche["then"][0]
        if _rendre(self.env, interne["if"], self.ctx):
            data = interne["then"][0]["data"]
        else:
            for nom, v in interne["else"][0]["variables"].items():
                self.ctx[nom] = _rendre(self.env, v, self.ctx)
            if self.longues is not None:
                self.ctx["longues"] = self.longues
            data = interne["else"][2]["data"]
        r = {k: _rendre(self.env, v, self.ctx) for k, v in data.items()}
        return r["vue"], str(r["debut"]), [_nombre(x) for x in str(r["valeurs"]).split(";")]


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
    longue = p.demandes_statistiques()[1]
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
    assert p.demandes_statistiques()[1]["period"] == "month"


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
    assert p.demandes_statistiques()[1]["statistic_ids"] == [PROD, PROD2]


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
    assert set(capteurs) == set(CAPTEURS) | {"solaires", "productions"}, \
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
    # Garde d'origine, « rien de neuf » et « tablette connectée » laissent passer « energie ».
    assert texte.count("'maj_ecran', 'energie'") == 2 and "'pipeline_discussion', 'energie'" in texte


# ─── Firmware ────────────────────────────────────────────────────────────────

def test_option_e_du_firmware():
    cpp = _lire(TUILES_CPP)
    lettres = re.search(r'kLettresOptions\[\] = "(\w+)"', cpp).group(1)
    assert lettres.index("e") == 7 and re.search(r"\bOPT_E = 128\b", cpp)
    assert "case Type::CAP: return (options & OPT_E) != 0;" in cpp
    assert re.search(r"d\.options & OPT_E\) && energie_formater\(", cpp)
    assert "g_tuiles_ui.energie_ouvrir()" in cpp
    assert lettres == scenarios.OPTIONS_TUILE


def test_popup_au_registre_et_au_select():
    scripts = _lire(os.path.join(REPO, "Tab5", "tab5-scripts.yaml"))
    assert re.search(r'ModalRegistry::add\(id\(energie_popup\),\s+"Énergie",\s+ModalRegistry::POPUP\);', scripts)
    controles = _lire(os.path.join(REPO, "Tab5", "tab5-ha-controls.yaml"))
    assert '- "Énergie"' in controles and "id(tab5_energie_ouvrir).execute();" in controles
    # Le package attend exactement ce nom d'écran.
    assert "states(ecran) != 'Énergie'" in _lire(PACKAGE)
