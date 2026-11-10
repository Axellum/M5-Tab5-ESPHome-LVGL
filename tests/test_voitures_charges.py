# -*- coding: utf-8 -*-
"""Analyse des charges des voitures (ADR-0074) : la fonction de RÉFÉRENCE `tools/voitures_charges.py`
sur des séries synthétiques, au format de `recorder.get_statistics` (puissances moyennes en W).

Chaque cas dit ce que doit donner la méthode, chiffres calculés à la main dans le commentaire :
charge de nuit (0 %), de midi (100 %), coupée par un nuage, voiture branchée sans charge, trou de
données (court : une seule charge ; long : deux), un seul capteur solaire puis plusieurs, la maison qui
compte ou non les voitures, deux voitures qui chargent ensemble, statistiques horaires seules et mixtes,
heures d'une journée, bilan du mois.

Les mêmes séries (`SCENARIOS`) servent à `tests/test_voitures.py`, qui rend le VRAI modèle Jinja du
package et exige les mêmes chaînes que cette référence."""
import datetime as dt
from zoneinfo import ZoneInfo

import pytest

import voitures_charges as ref

PARIS = ZoneInfo("Europe/Paris")
UTC = dt.timezone.utc
# 14:37 à Paris (12:37 UTC) : les 5 minutes de 14:30 sont la dernière ligne compilée.
MAINTENANT = dt.datetime(2026, 6, 16, 14, 37, 20, tzinfo=PARIS)
DERNIERE_LIGNE_5MIN = dt.datetime(2026, 6, 16, 14, 35, tzinfo=PARIS)    # fin (exclue) des lignes de 5 minutes
DERNIERE_LIGNE_HEURE = dt.datetime(2026, 6, 16, 14, 0, tzinfo=PARIS)    # fin (exclue) des lignes horaires
DEBUT_5MIN = dt.datetime(2026, 6, 10, 0, 0, tzinfo=PARIS)               # 6 jours de lignes de 5 minutes
DEBUT_HEURE = dt.datetime(2026, 5, 18, 0, 0, tzinfo=PARIS)              # 29 jours avant le 16/06

PV, PVB, PVC = "sensor.solaire_a", "sensor.solaire_b", "sensor.solaire_c"
MAISON = "sensor.maison_puissance"
T1, T2 = "sensor.tesla_2023_puissance_de_charge", "sensor.tesla_2025_puissance_de_charge"
ODO1, ODO2 = "sensor.tesla_2023_odometre", "sensor.tesla_2025_odometre"


def D(jour, h, m=0, mois=6):
    """Un instant local de 2026."""
    return dt.datetime(2026, mois, jour, h, m, tzinfo=PARIS)


def ligne(debut_utc, moyenne, duree_s):
    fin = debut_utc + dt.timedelta(seconds=duree_s)
    return {"start": debut_utc.isoformat(), "end": fin.isoformat(), "mean": moyenne}


def lignes(f, debut, fin, pas_s):
    """Les lignes d'un capteur de `debut` à `fin` (exclue) : `f(instant local)` en W, None = pas de
    ligne. Pas d'une heure : la moyenne des 12 créneaux de 5 minutes qui ont une valeur."""
    sortie = []
    t = debut.astimezone(UTC)
    fin = fin.astimezone(UTC)
    while t < fin:
        if pas_s == 300:
            v = f(t.astimezone(PARIS))
        else:
            vs = [f((t + dt.timedelta(minutes=5 * i)).astimezone(PARIS)) for i in range(12)]
            vs = [x for x in vs if x is not None]
            v = sum(vs) / len(vs) if vs else None
        if v is not None:
            sortie.append(ligne(t, v, pas_s))
        t += dt.timedelta(seconds=pas_s)
    return sortie


def monde(fonctions, heures_seules=False, debut_5min=DEBUT_5MIN, debut_heure=DEBUT_HEURE):
    """(stats5, stats1) de `fonctions` {capteur: f(instant local)}."""
    stats1 = {s: lignes(f, debut_heure, DERNIERE_LIGNE_HEURE, 3600) for s, f in fonctions.items()}
    stats5 = {} if heures_seules else {s: lignes(f, debut_5min, DERNIERE_LIGNE_5MIN, 300) for s, f in fonctions.items()}
    return stats5, stats1


def entre(t, a, b):
    return a <= t < b


def soleil(w, de=7, a=20):
    """Une production : w entre `de` et `a` heures, sinon pas de ligne (onduleur éteint la nuit)."""
    return lambda t: w if de <= t.hour < a else None


def capteurs(solaires=(PV,), maison=MAISON, inclut=True, voitures=((T1, ""),)):
    return {"solaires": list(solaires), "maison": maison, "maison_inclut_voitures": inclut,
            "voitures": [{"rang": str(i + 1), "nom": "Tesla %d" % (i + 1), "puissance_charge": p,
                          "odometre": o} for i, (p, o) in enumerate(voitures)]}


def cas(fonctions, cap=None, odo=None, jour=None, heures_seules=False, **mondes):
    stats5, stats1 = monde(fonctions, heures_seules, **mondes)
    return {"stats5": stats5, "stats1": stats1, "odo": odo or {}, "capteurs": cap or capteurs(),
            "maintenant": MAINTENANT, "jour": jour}


def une_voiture(p1, pv=None, maison_hors=300, **kw):
    """Une voiture : puissance `p1(t)` (W, None = pas de ligne), solaire `pv`, maison = hors voitures + p1."""
    pv = pv or soleil(6000)
    return cas({T1: p1, PV: pv, MAISON: lambda t: maison_hors + (p1(t) or 0)}, **kw)


# ─── Les séries ──────────────────────────────────────────────────────────────

def _nuit():
    # 7 kW de 23:00 le 15 à 02:00 le 16 ; le soleil n'a pas de ligne la nuit
    return une_voiture(lambda t: 7000 if entre(t, D(15, 23), D(16, 2)) else 0)


def _midi():
    # 3,7 kW de 11:00 à 13:00 le 15, solaire 6 kW, maison 500 W hors voiture
    return une_voiture(lambda t: 3700 if entre(t, D(15, 11), D(15, 13)) else 0, maison_hors=500)


def _nuage():
    # 4 kW de 10:00 à 12:00 ; solaire 8 kW puis 2,5 kW de 11:00 à 12:00 ; maison 1 kW hors voiture
    pv = lambda t: (2500 if entre(t, D(15, 11), D(15, 12)) else 8000) if 7 <= t.hour < 20 else None
    return une_voiture(lambda t: 4000 if entre(t, D(15, 10), D(15, 12)) else 0, pv=pv, maison_hors=1000)


def _sans_charge():
    # branchée toute la journée du 15, puissance 0 (et 30 W de veille : sous le seuil)
    return une_voiture(lambda t: 30 if t.minute == 10 else 0)


def _trou(trou_min):
    # 4 kW de 20:00 à 20:30, puis de 20:30 + trou à 21:00 + trou ; aucune ligne pendant le trou
    debut2 = D(15, 20, 30) + dt.timedelta(minutes=trou_min)

    def p(t):
        if entre(t, D(15, 20), D(15, 20, 30)) or entre(t, debut2, debut2 + dt.timedelta(minutes=30)):
            return 4000
        if entre(t, D(15, 20, 30), debut2):
            return None
        return 0
    return une_voiture(p)


def _deux_heures(pv=None, maison=None, cap=None, **kw):
    # 4 kW de 10:00 à 12:00 le 15 ; solaire 8 kW ; maison 1 kW hors voiture, ou ce que dit `maison`
    p1 = lambda t: 4000 if entre(t, D(15, 10), D(15, 12)) else 0
    c = cas({T1: p1, PV: pv or soleil(8000), MAISON: maison or (lambda t: 1000 + p1(t))}, cap=cap, **kw)
    return c


def _maison_muette_une_heure():
    # la maison n'a pas de ligne de 11:00 à 12:00
    return _deux_heures(maison=lambda t: None if entre(t, D(15, 11), D(15, 12)) else 1000 + (4000 if entre(t, D(15, 10), D(15, 12)) else 0))


def _sans_capteur_maison():
    return _deux_heures(cap=capteurs(maison=""))


def _sans_capteur_solaire():
    return _deux_heures(cap=capteurs(solaires=()))


def _plusieurs_solaires(solaires, inclut=True, maison_hors=1000):
    p1 = lambda t: 4000 if entre(t, D(15, 12), D(15, 13)) else 0
    maison = (lambda t: maison_hors + p1(t)) if inclut else (lambda t: maison_hors)
    return cas({T1: p1, PV: soleil(3000), PVB: soleil(3000), MAISON: maison},
               cap=capteurs(solaires=solaires, inclut=inclut))


def _deux_voitures():
    # les deux à 4 kW de 12:00 à 13:00 ; solaire 6,2 kW ; maison 1 kW hors voitures
    p = lambda t: 4000 if entre(t, D(15, 12), D(15, 13)) else 0
    return cas({T1: p, T2: p, PV: soleil(6200), MAISON: lambda t: 1000 + 2 * p(t)},
               cap=capteurs(voitures=((T1, ODO1), (T2, ODO2))))


def _heures_seules_mal_alignee():
    # 7 kW de 22:40 à 00:40 (15 au 16) : en statistiques horaires, 22 h (1/3), 23 h, 0 h (2/3)
    return une_voiture(lambda t: 7000 if entre(t, D(15, 22, 40), D(16, 0, 40)) else 0, heures_seules=True)


def _heures_seules_nuit():
    return une_voiture(lambda t: 7000 if entre(t, D(15, 23), D(16, 2)) else 0, heures_seules=True)


def _mixte():
    # les lignes de 5 minutes commencent à 04:15 le 12 (purge en cours d'heure) : 7 kW de 03:00 à 06:00
    p = lambda t: 7000 if entre(t, D(12, 3), D(12, 6)) else 0
    return une_voiture(p, debut_5min=D(12, 4, 15))


def _bilan():
    # Deux voitures sur le mois : 1 -> 21 kWh le 03/06 de 23:00 à 02:00 (nuit) ;
    # 1 -> 7,4 kWh le 15/06 de 11:00 à 13:00 à midi (100 % solaire) ; 2 -> 6 kWh le 10/06 de 12:00 à 13:00.
    def p1(t):
        if entre(t, D(3, 23), D(4, 2)):
            return 7000
        return 3700 if entre(t, D(15, 11), D(15, 13)) else 0

    def p2(t):
        return 6000 if entre(t, D(10, 12), D(10, 13)) else 0
    return cas({T1: p1, T2: p2, PV: soleil(6000), MAISON: lambda t: 800 + p1(t) + p2(t)},
               cap=capteurs(voitures=((T1, ODO1), (T2, ODO2))),
               odo={ODO1: [{"start": "2026-05-31T22:00:00+00:00", "end": "2026-06-30T22:00:00+00:00", "change": 245.6}]},
               debut_heure=D(1, 0, mois=5))


def _sans_voiture():
    return cas({PV: soleil(6000), MAISON: lambda t: 800}, cap=capteurs(voitures=()))


SCENARIOS = {
    "nuit": _nuit, "midi": _midi, "nuage": _nuage, "sans_charge": _sans_charge,
    "trou_10min": lambda: _trou(10), "trou_30min": lambda: _trou(30), "trou_35min": lambda: _trou(35),
    "trou_40min": lambda: _trou(40),
    "maison_muette": _maison_muette_une_heure, "sans_maison": _sans_capteur_maison,
    "sans_solaire": _sans_capteur_solaire,
    "solaire_seul": lambda: _plusieurs_solaires([PV]),
    "deux_solaires": lambda: _plusieurs_solaires([PV, PVB]),
    "trois_dont_un_vide": lambda: _plusieurs_solaires([PV, PVB, PVC]),
    "maison_sans_voiture": lambda: _plusieurs_solaires([PV], inclut=False),
    "maison_avec_voiture_mal_declaree": lambda: _plusieurs_solaires([PV], inclut=True, maison_hors=0),
    "deux_voitures": _deux_voitures,
    "heures_seules_mal_alignee": _heures_seules_mal_alignee, "heures_seules_nuit": _heures_seules_nuit,
    "mixte": _mixte, "bilan": _bilan, "sans_voiture": _sans_voiture,
}


def _six_charges():
    # huit charges de 3 kW d'une heure, une par jour du 8 au 15 à 12 h
    return une_voiture(lambda t: 3000 if (8 <= t.day <= 15 and t.hour == 12) else 0)


def _deux_voitures_decalees():
    # voiture 1 de 9 h à 10 h, voiture 2 de 14 h à 15 h le 15
    p1 = lambda t: 3000 if entre(t, D(15, 9), D(15, 10)) else 0
    p2 = lambda t: 3000 if entre(t, D(15, 14), D(15, 15)) else 0
    return cas({T1: p1, T2: p2, PV: soleil(6000), MAISON: lambda t: 500 + p1(t) + p2(t)},
               cap=capteurs(voitures=((T1, ""), (T2, ""))))


def _changement_d_heure():
    # 29/03/2026, 18:07 à Paris : passage à l'heure d'été à 02:00 (de 2 h à 3 h, pas d'heure locale 2).
    # 7 kW de 01:00 à 05:00 locales (3 h écoulées) ; jour = ce jour-là ; ni solaire ni maison
    maintenant = dt.datetime(2026, 3, 29, 18, 7, tzinfo=PARIS)
    p = lambda t: 7000 if entre(t, D(29, 1, mois=3), D(29, 5, mois=3)) else 0
    fin5 = dt.datetime(2026, 3, 29, 18, 5, tzinfo=PARIS)
    fin1 = dt.datetime(2026, 3, 29, 18, 0, tzinfo=PARIS)
    return {"stats5": {T1: lignes(p, D(27, 0, mois=3), fin5, 300)},
            "stats1": {T1: lignes(p, D(1, 0, mois=3), fin1, 3600)}, "odo": {},
            "capteurs": capteurs(solaires=(), maison=""), "maintenant": maintenant, "jour": "2026-03-29"}


def _bilan_km(change):
    c = _bilan()
    c["capteurs"] = capteurs(voitures=((T1, ODO1),))
    c["odo"] = {ODO1: [{"start": "x", "end": "y", "change": change}]} if change is not None else {}
    return c


def _avec_jour(jour):
    c = _nuit()
    c["jour"] = jour
    return c


def _ligne_odo(jour, change, etat, mois=6):
    """Une ligne `day` de `recorder.get_statistics` pour l'odomètre : le jour local `jour` (de minuit à
    minuit, début donné en UTC comme le fait HA), `change` = km du jour, `etat` = relevé en fin de jour ;
    None : la clé manque (le recorder n'a pas pu la calculer)."""
    debut = D(jour, 0, mois=mois).astimezone(UTC)
    ligne_j = {"start": debut.isoformat(), "end": (D(jour, 0, mois=mois) + dt.timedelta(days=1)).astimezone(UTC).isoformat()}
    if change is not None:
        ligne_j["change"] = change
    if etat is not None:
        ligne_j["state"] = etat
    return ligne_j


def _trajets():
    # Le mois du bilan (voiture 1 : 245,6 km, 28,4 kWh ; voiture 2 : 6 kWh, pas de km du mois), plus la
    # lecture « jour » des odomètres du 10 au 16 juin. Voiture 1 : une ligne par jour, relevé 41 280 km à la
    # fin. Voiture 2 : pas de ligne les 11 et 12, un `change` de -3 le 13 (un odomètre ne recule pas : 0),
    # une ligne sans `state` le 13, relevé final 9 640.
    c = _bilan()
    c["odo_jours"] = {
        ODO1: [_ligne_odo(10 + i, km, 41040.0 + 40 * i) for i, km in enumerate([41.2, 0.0, 112.4, 56.8, 23.0, 5.6, 88.0])],
        ODO2: [_ligne_odo(10, 30.0, 9500.0), _ligne_odo(13, -3.0, None), _ligne_odo(14, 12.5, 9512.5),
               _ligne_odo(15, 0.0, 9512.5), _ligne_odo(16, 7.2, 9640.0)],
    }
    return c


def _trajets_12_km():
    c = _bilan_km(12.0)
    c["odo_jours"] = {ODO1: [_ligne_odo(16, 12.0, 20012.0)]}
    return c


SCENARIOS.update({
    "six_charges": _six_charges, "deux_voitures_decalees": _deux_voitures_decalees,
    "changement_d_heure": _changement_d_heure,
    "bilan_12_km": lambda: _bilan_km(12.0), "bilan_sans_odometre": lambda: _bilan_km(None),
    "jour_hier": lambda: _avec_jour("2026-06-15"), "jour_30_fevrier": lambda: _avec_jour("2026-02-30"),
    "jour_texte": lambda: _avec_jour("n'importe quoi"), "jour_ancien": lambda: _avec_jour("2025-01-01"),
    "jour_vide": lambda: _avec_jour(""),
    "trajets": _trajets, "trajets_12_km": _trajets_12_km,
})


# ─── Cas simples (chiffres à la main) ────────────────────────────────────────

def rep(nom, **surcharges):
    c = SCENARIOS[nom]()
    c.update(surcharges)
    return ref.reponse(**c)


def test_charge_de_nuit_zero_pour_cent():
    """7 kW pendant 3 h = 21 kWh ; le soleil est éteint (aucune ligne) : 0 %."""
    r = rep("nuit")
    assert r["charges"] == r["charges_1"] == "1|2026-06-15|23:00|180|21.0|0"
    assert r["charges_2"] == "" and r["precision"] == "mixte"


def test_charge_a_midi_cent_pour_cent():
    """3,7 kW × 2 h = 7,4 kWh ; maison hors voiture 0,5 kW, solaire 6 kW : 5,5 kW disponibles > 3,7."""
    assert rep("midi")["charges"] == "1|2026-06-15|11:00|120|7.4|100"


def test_charge_coupee_par_un_nuage():
    """4 kW : première heure à 8 kW de soleil (4 000 Wh), seconde à 2,5 kW de soleil - 1 kW de maison
    = 1,5 kW disponibles (1 500 Wh) : 5 500 / 8 000 = 68,75 % -> 69."""
    assert rep("nuage")["charges"] == "1|2026-06-15|10:00|120|8.0|69"


def test_voiture_branchee_sans_charge():
    """0 W, et 30 W de veille (sous le seuil de 100 W) : aucune charge, 0 kWh dans le bilan."""
    r = rep("sans_charge")
    assert r["charges"] == r["charges_1"] == ""
    champs = r["bilan"].split("|")
    assert len(champs) == 11 and champs[0] == "0.0" and champs[1] == "nan"   # rien de calculable, pas de division par 0
    assert champs[10] == "0"


@pytest.mark.parametrize("trou, attendu", [
    # 12 créneaux de 4 kW (20:00-20:30), trou, 12 créneaux : 4 kWh ; 20:00 -> 20:30 + trou + 30 min
    (10, "1|2026-06-15|20:00|70|4.0|0"),
    # trou de 30 min = la limite : encore une seule charge (le trou est de 20:30 à 21:00)
    (30, "1|2026-06-15|20:00|90|4.0|0"),
    # 35 min : deux charges, la plus récente d'abord (21:05 -> 21:35 et 20:00 -> 20:30)
    (35, "1|2026-06-15|21:05|30|2.0|0;1|2026-06-15|20:00|30|2.0|0"),
    (40, "1|2026-06-15|21:10|30|2.0|0;1|2026-06-15|20:00|30|2.0|0"),
])
def test_trou_de_donnees(trou, attendu):
    """Le trou n'a pas de ligne pour la voiture ; au plus 30 min il ne coupe pas la charge. Le solaire
    n'a pas de ligne à 20 h (onduleur éteint) : 0 %."""
    assert rep("trou_%dmin" % trou)["charges"] == attendu


def test_maison_sans_ligne_une_heure_exclut_ce_creneau():
    """Maison muette de 11 h à 12 h : la part se calcule sur la première heure seulement (100 %),
    mais les 8 kWh comptent en entier."""
    assert rep("maison_muette")["charges"] == "1|2026-06-15|10:00|120|8.0|100"


def test_sans_capteur_de_maison_part_inconnue():
    assert rep("sans_maison")["charges"] == "1|2026-06-15|10:00|120|8.0|nan"


def test_sans_capteur_solaire_part_vide():
    assert rep("sans_solaire")["charges"] == "1|2026-06-15|10:00|120|8.0|"


@pytest.mark.parametrize("nom, part", [
    # 4 kW de 12 h à 13 h, maison 1 kW hors voiture ; chaque capteur 3 kW
    ("solaire_seul", 50),                 # 3 000 - 1 000 = 2 000 disponibles pour 4 000 W : 50 %
    ("deux_solaires", 100),               # 6 000 - 1 000 = 5 000 >= 4 000
    ("trois_dont_un_vide", 100),          # un capteur sans ligne ne compte pas
    ("maison_sans_voiture", 50),          # la maison ne compte pas la voiture : rien à retrancher
    # la maison déclarée « avec voitures » alors que son capteur n'en a pas : max(0, 0 - 4 000) = 0,
    # 3 000 W disponibles : 75 %
    ("maison_avec_voiture_mal_declaree", 75),
])
def test_un_ou_plusieurs_capteurs_solaires_et_option_maison(nom, part):
    assert rep(nom)["charges"] == "1|2026-06-15|12:00|60|4.0|%d" % part


def test_deux_voitures_ne_comptent_pas_deux_fois_le_meme_soleil():
    """2 × 4 kW, solaire 6,2 kW, maison 1 kW hors voitures : 5,2 kW disponibles pour 8 kW, soit 65 %
    chacune (la formule d'une voiture seule leur donnerait 100 % à toutes les deux)."""
    r = rep("deux_voitures")
    assert r["charges_1"] == "1|2026-06-15|12:00|60|4.0|65"
    assert r["charges_2"] == "2|2026-06-15|12:00|60|4.0|65"
    assert r["charges"] == r["charges_1"] + ";" + r["charges_2"]


def test_statistiques_horaires_seules():
    """Plus de lignes de 5 minutes : l'énergie reste exacte, la durée et l'heure sont à l'heure près.
    7 kW de 22:40 à 00:40 = 14 kWh ; trois lignes horaires (22 h, 23 h, 0 h) -> 22:00, 180 min."""
    r = rep("heures_seules_mal_alignee")
    assert r["charges"] == "1|2026-06-15|22:00|180|14.0|0" and r["precision"] == "heure"
    # alignée sur l'heure : exacte
    assert rep("heures_seules_nuit")["charges"] == "1|2026-06-15|23:00|180|21.0|0"


def test_statistiques_mixtes_une_charge_a_cheval():
    """7 kW de 03:00 à 06:00 le 12 ; lignes de 5 minutes à partir de 04:15 : horaires pour 3 h et 4 h,
    5 minutes à partir de 5 h (heure pleine suivant 04:15) : une seule charge de 21 kWh."""
    r = rep("mixte")
    assert r["charges"] == "1|2026-06-12|03:00|180|21.0|0" and r["precision"] == "mixte"


def test_six_charges_au_plus_la_plus_recente_d_abord():
    """Huit charges de 1 h, une par jour du 8 au 15 à 12 h : les six dernières, du 15 au 10."""
    lignes_ = rep("six_charges")["charges"].split(";")
    assert [x.split("|")[1] for x in lignes_] == ["2026-06-%02d" % j for j in range(15, 9, -1)]
    assert all(x.endswith("|12:00|60|3.0|100") for x in lignes_)


def test_liste_de_deux_voitures_triee_par_date():
    """Deux voitures à des heures différentes : une liste commune, la plus récente d'abord."""
    r = rep("deux_voitures_decalees")
    assert [x[:1] for x in r["charges"].split(";")] == ["2", "1"]
    assert r["charges_1"].startswith("1|") and r["charges_2"].startswith("2|")


# ─── Heures du jour ──────────────────────────────────────────────────────────

def test_kwh_par_heure_du_jour():
    """Aujourd'hui (16/06 à 14:37) : 7 kW de 0 h à 2 h ; 0 h et 1 h = 7 kWh ; 2 h à 14 h = 0 ; le reste
    n'a pas de ligne (heures à venir). Le 15 : 23 h = 7 kWh ; les autres heures à 0."""
    r = rep("nuit")
    assert r["jour_debut"] == "2026-06-16"
    assert r["jour_1"].split(";") == ["7.0", "7.0"] + ["0.0"] * 13 + [""] * 9
    assert r["jour_2"] == ""
    hier = rep("jour_hier")
    assert hier["jour_debut"] == "2026-06-15"
    assert hier["jour_1"].split(";") == ["0.0"] * 23 + ["7.0"]


def test_maison_sans_les_voitures_par_heure():
    """ADR-0078. « nuit » (le 16/06 à 14:37) : 300 W de maison hors voiture, la voiture à 7 kW de 0 h à
    2 h, un capteur de maison qui la compte (7,3 kW) : la voiture est retranchée, 0,3 kWh à chaque heure ;
    14 h n'a que 35 minutes de lignes (14:00 à 14:30, 7 créneaux) : 0,175 kWh ; les heures à venir sont vides."""
    r = rep("nuit")
    assert r["jour_maison"].split(";") == ["0.3"] * 14 + ["0.175"] + [""] * 9


def test_maison_sans_les_voitures_selon_le_capteur():
    """Un capteur qui ne compte pas les voitures est pris tel quel (1 kW : 1 kWh par heure, 14 h partielle) ;
    sans capteur de maison ou sans voiture, rien à empiler : tout est vide."""
    assert rep("maison_sans_voiture")["jour_maison"].split(";") == ["1.0"] * 14 + ["0.583"] + [""] * 9
    assert rep("sans_maison")["jour_maison"] == ""
    assert rep("sans_voiture")["jour_maison"] == ""


def test_maison_jamais_sous_zero_si_la_voiture_depasse_la_maison():
    """Un capteur de maison qui compte la voiture mais lit moins qu'elle (retard d'une mesure, voiture
    déclarée à tort : ici 6,5 kW pour 7 kW de charge) ne donne pas une heure négative : 0."""
    cas_ = une_voiture(lambda t: 7000 if entre(t, D(15, 23), D(16, 2)) else 0, maison_hors=-500)
    h = ref.reponse(**cas_)["jour_maison"].split(";")
    assert h[:2] == ["0.0", "0.0"] and len(h) == 24


def test_jour_hors_fenetre_ou_invalide():
    assert rep("jour_30_fevrier")["jour_debut"] == "2026-03-02"      # débordement : un 30 février n'est pas un jour
    assert rep("jour_texte")["jour_debut"] == "2026-06-16"           # texte invalide : aujourd'hui
    assert rep("jour_ancien")["jour_1"] == ";" * 23                  # aucune ligne ce jour-là
    assert rep("jour_vide")["jour_debut"] == "2026-06-16"


def test_heures_du_jour_a_l_heure_locale_au_changement_d_heure():
    """29/03/2026 : de 2 h à 3 h il n'y a pas d'heure locale 2 ; 7 kW de 1 h à 5 h locales
    (3 h écoulées). Les heures 1, 3 et 4 reçoivent 7 kWh, l'heure 2 est vide, 21 kWh au total."""
    h = rep("changement_d_heure")["jour_1"].split(";")
    assert h[1] == "7.0" and h[2] == "" and h[3] == "7.0" and h[4] == "7.0" and h[0] == "0.0" and h[5] == "0.0"
    assert round(sum(float(x) for x in h if x), 3) == 21.0


# ─── Bilan du mois ───────────────────────────────────────────────────────────

def test_bilan_du_mois_une_et_deux_voitures():
    """Juin au 16 : voiture 1 = 21 kWh la nuit du 3 (0 %) + 7,4 kWh le 15 à midi (100 %) = 28,4 kWh,
    part du soleil = 7,4 / 28,4 = 26 % ; 245,6 km -> 246 ; 28,4 / 245,6 × 100 = 11,6 kWh/100 km ;
    réseau = 28,4 × (1 - 7,4 / 28,4) = 21,0 kWh. Voiture 2 = 6 kWh le 10 à midi : 6 000 W de charge
    pour 5 200 W de soleil disponible (6 000 - 800 de maison) = 87 %, réseau 0,8 kWh, sans odomètre.
    Part de la maison : 34,4 kWh sur ce que la maison a consommé depuis le 1er juin 00:00 jusqu'à
    14:35 le 16 (374,58 h à 800 W de base, plus les charges)."""
    r = rep("bilan")
    assert r["bilan_debut"] == "2026-06-01"
    c = r["bilan"].split("|")
    assert c[:5] == ["28.4", "26", "246", "11.6", "21.0"]
    assert c[5:10] == ["6.0", "87", "", "", "0.8"]
    attendu = round(100 * 34400 / (800 * 374.58333333333337 + 34400))
    assert c[10] == str(attendu) and 0 < attendu < 100


def test_kilometres_minimum_pour_les_kwh_aux_100_km():
    champs = rep("bilan_12_km")["bilan"].split("|")
    assert champs[2] == "12" and champs[3] == ""                    # moins de 20 km : pas de kWh/100 km
    assert rep("bilan_sans_odometre")["bilan"].split("|")[2:4] == ["", ""]    # pas d'odomètre : ni km ni kWh/100 km


# ─── Trajets (ADR-0083) ──────────────────────────────────────────────────────

def test_trajets_sept_jours_compteur_et_kwh():
    """Du 10 au 16 juin (aujourd'hui compris). Voiture 1 : km par jour tels que la lecture du recorder les
    donne, relevé final 41 280 km, 246 km ce mois-ci (245,6) et 28,4 kWh chargés : 28,4 / 245,6 × 100 =
    11,6 kWh/100 km ; elle a chargé 7,4 kWh le 15 (3,7 kW de 11 h à 13 h), 0 les autres jours (des lignes
    de puissance existent, la voiture ne chargeait pas). Voiture 2 : pas de ligne les 11 et 12 (ni 0 km
    ni valeur), -3 km le 13 ramené à 0,0, relevé final 9 640, pas de km du mois donc pas de kWh/100 km ;
    6 kWh le 10 (6 kW de 12 h à 13 h)."""
    r = rep("trajets")
    assert r["trajets_1"] == "2026-06-10|41280|246|11.6|41.2;0.0;112.4;56.8;23.0;5.6;88.0|0.0;0.0;0.0;0.0;0.0;7.4;0.0"
    assert r["trajets_2"] == "2026-06-10|9640|||30.0;;;0.0;12.5;0.0;7.2|6.0;0.0;0.0;0.0;0.0;0.0;0.0"
    # les km et le kWh/100 km du mois sont ceux du bilan : une seule méthode
    assert r["bilan"].split("|")[2:4] == r["trajets_1"].split("|")[2:4]


def test_trajets_odometre_choisi_sans_aucune_ligne_du_recorder():
    """Un odomètre désactivé dans l'intégration (le cas par défaut chez Tesla) n'a pas de statistiques :
    la chaîne existe (l'odomètre est choisi), sans relevé ni km, les kWh restent."""
    r = rep("deux_voitures")
    assert r["trajets_1"] == "2026-06-10||||;;;;;;|0.0;0.0;0.0;0.0;0.0;4.0;0.0"
    assert r["trajets_2"] == r["trajets_1"]


def test_trajets_sans_odometre_choisi_chaine_vide():
    r = rep("nuit")                                         # une voiture, odomètre vide
    assert r["trajets_1"] == "" and r["trajets_2"] == ""
    r = rep("trajets")
    assert r["trajets_1"] != ""                             # témoin : avec l'odomètre, une chaîne
    c = SCENARIOS["trajets"]()
    c["capteurs"] = capteurs(voitures=((T1, ODO1), (T2, "")))     # la voiture 2 sans odomètre
    r = ref.reponse(**c)
    assert r["trajets_1"] != "" and r["trajets_2"] == ""


def test_trajets_pas_de_kwh_aux_100_km_sous_20_km():
    t = rep("trajets_12_km")["trajets_1"].split("|")
    assert t[1:4] == ["20012", "12", ""]                    # 12 km ce mois-ci : pas de kWh/100 km
    assert t[4] == ";;;;;;12.0"


def test_fenetre_trajets_jours_locaux_et_changement_d_heure():
    w = ref.fenetre_trajets(MAINTENANT)
    assert w["jours"] == ["2026-06-10", "2026-06-11", "2026-06-12", "2026-06-13", "2026-06-14", "2026-06-15",
                          "2026-06-16"]
    assert w["debut"] == "2026-06-10" and w["iso"] == "2026-06-10T00:00:00+02:00"
    assert w["s0"] == int(D(10, 0).timestamp())
    # la semaine du passage à l'heure d'été (29/03/2026) : sept jours distincts, minuit à +01:00 puis jour de 23 h
    w = ref.fenetre_trajets(dt.datetime(2026, 3, 29, 18, 7, tzinfo=PARIS))
    assert w["jours"][0] == "2026-03-23" and w["jours"][-1] == "2026-03-29" and len(set(w["jours"])) == 7
    assert w["iso"] == "2026-03-23T00:00:00+01:00"
    # au passage à l'heure d'hiver (25/10/2026) : minuit à +02:00 le 19, sept jours
    w = ref.fenetre_trajets(dt.datetime(2026, 10, 25, 18, 7, tzinfo=PARIS))
    assert w["jours"][0] == "2026-10-19" and w["jours"][-1] == "2026-10-25"
    assert w["iso"] == "2026-10-19T00:00:00+02:00"


def test_trajets_un_jour_est_un_jour_local_pas_utc():
    """Le recorder donne le début du jour local en UTC : minuit à Paris le 12 juin est le 11 à 22:00 UTC,
    et ce jour-là compte pour le 12."""
    cap = ref.normaliser(capteurs(voitures=((T1, ODO1),)))
    w = ref.fenetre_trajets(MAINTENANT)
    f = {"tz": PARIS}
    lignes_j = [{"start": "2026-06-09T22:00:00+00:00", "change": 99.0, "state": 4000.0},     # le 10 à Paris
                {"start": "2026-06-11T22:00:00+00:00", "change": 10.0, "state": 5000.0}]     # le 12 à Paris
    sortie = ref.kilometres_jours({ODO1: lignes_j}, cap, w, f)["1"]
    assert sortie["jours"] == {"2026-06-10": 99.0, "2026-06-12": 10.0} and sortie["compteur"] == 5000.0  # le dernier relevé
    hors = ref.kilometres_jours({ODO1: [{"start": "2026-06-08T22:00:00+00:00", "change": 5.0}]}, cap, w, f)["1"]
    assert hors["jours"] == {} and hors["compteur"] is None                  # le 9, hors de la fenêtre


def test_sans_voiture_reponse_vide():
    r = rep("sans_voiture")
    assert r["trajets_1"] == r["trajets_2"] == ""
    assert r["charges"] == r["charges_1"] == r["charges_2"] == r["jour_1"] == r["jour_2"] == r["bilan"] == ""
    assert r["precision"] == "aucune" and r["jour_debut"] == "2026-06-16" and r["bilan_debut"] == "2026-06-01"


# ─── Les morceaux ────────────────────────────────────────────────────────────

def test_creneaux_assemble_les_deux_resolutions():
    """Lignes de 5 minutes à partir de 04:15 : l'heure pleine suivante (05:00) sépare les horaires des
    5 minutes ; les lignes de 5 minutes de 04:15 à 04:55 sont laissées à la ligne horaire de 04 h."""
    stats5 = {T1: [ligne(D(12, 4, 15).astimezone(UTC) + dt.timedelta(minutes=5 * i), 100.0 + i, 300) for i in range(30)]}
    stats1 = {T1: [ligne(D(12, 3).astimezone(UTC) + dt.timedelta(hours=i), 10.0 * i, 3600) for i in range(6)]}
    sortie = ref.creneaux(stats5, stats1, [T1])[T1]
    durees = [d for (_t, d, _m, _i) in sortie]
    assert durees == [3600] * 2 + [300] * 21         # 03 h et 04 h horaires, puis 05:00 à 06:40 (21 créneaux)
    assert sortie[2][0] - sortie[1][0] == 3600       # 05:00 suit 04:00 sans trou ni chevauchement


def test_ligne_sans_moyenne_ignoree():
    sortie = ref.creneaux({}, {T1: [{"start": D(12, 3).astimezone(UTC).isoformat(), "end": "x", "mean": None},
                                    ligne(D(12, 4).astimezone(UTC), 5.0, 3600)]}, [T1])
    assert len(sortie[T1]) == 1 and sortie[T1][0][2] == 5.0


def test_fenetres():
    f = ref.fenetres(MAINTENANT)
    assert f["jour"] == "2026-06-16" and f["mois"] == "2026-06-01"
    assert f["j1"] - f["j0"] == 86400 and f["debut"].startswith("2026-05-18T00:00:00+02:00")
    # début de mois à plus de 29 jours : il l'emporte (le 31 juillet, début le 1er juillet = 30 jours)
    f2 = ref.fenetres(dt.datetime(2026, 7, 31, 10, tzinfo=PARIS))
    assert f2["debut"].startswith("2026-07-01T00:00:00+02:00")
    # un jour ancien repousse le début
    assert ref.fenetres(MAINTENANT, "2026-04-02")["debut"].startswith("2026-04-02T00:00:00+02:00")


def test_normaliser():
    n = ref.normaliser({"solaires": "sensor.a", "maison": 3, "maison_inclut_voitures": "false",
                        "voitures": [{"puissance_charge": ""}, {"puissance_charge": T1, "odometre": ODO1},
                                     {"rang": "2", "puissance_charge": T2}, {"puissance_charge": "sensor.x3"}]})
    assert n["solaires"] == ["sensor.a"] and n["maison"] == "" and n["inclut"] is False
    assert [(v["rang"], v["puissance"]) for v in n["voitures"]] == [("1", T1), ("2", T2)]   # deux au plus
    assert ref.normaliser(None) == {"solaires": [], "maison": "", "inclut": True, "voitures": []}
    assert ref.normaliser({"solaires": [PV, PV, "", PVB]})["solaires"] == [PV, PVB]
