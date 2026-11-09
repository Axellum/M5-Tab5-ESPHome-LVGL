# -*- coding: utf-8 -*-
"""tools/voitures_charges.py — Référence de l'analyse des charges des voitures (ADR-0074).

Le script Home Assistant `tab5_voitures_analyse` (`HomeAssistant_Config/packages/tab5_voitures.yaml`)
calcule, à partir des statistiques du recorder, les charges d'une ou deux voitures électriques,
la part du soleil dans chacune, les kWh par heure d'une journée et le bilan du mois. Home Assistant
(intégrations Tesla Fleet et Teslemetry) ne donne aucune de ces valeurs.

Ce fichier est la RÉFÉRENCE : la même méthode en Python, sans dépendance à Home Assistant, écrite
d'abord et testée sur des séries synthétiques (`tests/test_voitures_charges.py`). Le modèle Jinja du
package doit donner les mêmes chiffres sur les mêmes séries (`tests/test_voitures.py` le rend dans le
bac à sable de Jinja et compare chaîne à chaîne). Une modification de la méthode se fait ici, puis
dans le package, dans le même commit. Les opérations flottantes sont écrites dans le même ordre des
deux côtés (somme dans l'ordre du temps, `p * f * d / 3600`) pour que les arrondis tombent pareil.

La méthode (le détail et les raisons : ADR-0074) :

- Entrée : des lignes de statistiques `{statistic_id: [{start, end, mean}]}`, au format de
  `recorder.get_statistics` (puissances en W). Les lignes de 5 minutes ne sont gardées que
  `purge_keep_days` jours ; avant, il ne reste que l'heure. `creneaux()` assemble les deux : des
  lignes horaires avant la première heure pleine couverte par les 5 minutes, des lignes de 5 minutes
  ensuite. Un créneau vaut donc 300 s ou 3600 s.
- Une voiture CHARGE dans un créneau quand sa puissance moyenne dépasse SEUIL_W. Une CHARGE est une
  suite de créneaux qui chargent, séparés par moins de TROU_MAX_MIN minutes (un trou de données, une
  pause sous un nuage, une ligne manquante ne la coupe pas).
- Énergie d'une charge = somme des puissances × durée des créneaux qui chargent.
- Part du soleil, créneau par créneau : solaire = somme des puissances solaires qui ont une ligne
  (aucune ligne = 0, un onduleur éteint la nuit) ; maison hors voitures = maison − puissance de charge
  des voitures (si le capteur de la maison les compte) ; disponible = max(0, solaire − maison hors
  voitures) ; chaque voiture reçoit la même fraction f = min(1, disponible / puissance de charge de
  toutes les voitures) de sa puissance (deux voitures qui chargent ensemble ne comptent pas deux fois
  le même soleil). Un créneau sans ligne pour la maison est exclu du calcul (ni au numérateur ni au
  dénominateur). Part = Σ soleil / Σ énergie sur les créneaux connus, bornée de 0 à 100 %.
"""
from __future__ import annotations

import datetime as dt
import re

# Choix documentés (ADR-0074) ; le package en reprend les valeurs (tests/test_voitures.py les compare).
SEUIL_W = 100          # puissance moyenne d'un créneau au-dessus de laquelle la voiture charge
TROU_MAX_MIN = 30      # trou qui ne coupe pas une charge (minutes)
KM_MIN_CONSO = 20      # en dessous, pas de kWh/100 km (une énergie chargée n'est pas une consommation)
NB_CHARGES = 6         # charges de la liste
JOURS_RECHERCHE = 30   # profondeur de la liste des charges
NB_VOITURES = 2
DUREE_5MIN = 300
DUREE_HEURE = 3600


# ─── Entrées ─────────────────────────────────────────────────────────────────

def horodatage(iso: str) -> float:
    """Secondes depuis 1970 d'une date ISO (comme `as_timestamp` de Home Assistant)."""
    return dt.datetime.fromisoformat(iso).timestamp()


def normaliser(capteurs) -> dict:
    """Les capteurs reçus par le script : solaires (liste, sans doublon), maison (texte vide = non
    choisi), maison_inclut_voitures (vrai par défaut) et jusqu'à deux voitures qui ont une puissance."""
    c = capteurs if isinstance(capteurs, dict) else {}
    brut = c.get("solaires", [])
    brut = [brut] if isinstance(brut, str) else (list(brut) if brut else [])
    solaires = []
    for s in brut:
        if isinstance(s, str) and s and s not in solaires:
            solaires.append(s)
    maison = c.get("maison", "")
    maison = maison if isinstance(maison, str) else ""
    inclut = c.get("maison_inclut_voitures", True) not in (False, "false", "off")
    voitures = []
    liste = c.get("voitures", [])
    for v in (liste if isinstance(liste, (list, tuple)) else []):
        if not isinstance(v, dict) or not v.get("puissance_charge") or len(voitures) >= NB_VOITURES:
            continue
        odometre = v.get("odometre", "")
        voitures.append({
            "rang": str(v.get("rang") or len(voitures) + 1),
            "puissance": v["puissance_charge"],
            "odometre": odometre if isinstance(odometre, str) else "",
        })
    return {"solaires": solaires, "maison": maison, "inclut": bool(inclut), "voitures": voitures}


def fenetres(maintenant: dt.datetime, jour: str | None = None) -> dict:
    """Bornes de temps (secondes depuis 1970) : le jour demandé (AAAA-MM-JJ, par défaut aujourd'hui),
    le mois en cours, et le début des statistiques à lire (le plus ancien des trois : le jour, le
    1er du mois, 29 jours avant aujourd'hui)."""
    minuit = maintenant.replace(hour=0, minute=0, second=0, microsecond=0)
    p = (jour or "").split("-")
    valide = (len(p) == 3 and all(re.fullmatch(r"[0-9]+", x) for x in p)
              and 2000 < int(p[0]) < 3000 and 1 <= int(p[1]) <= 12 and 1 <= int(p[2]) <= 31)
    if valide:
        j0 = minuit.replace(year=int(p[0]), month=int(p[1]), day=1) + dt.timedelta(days=int(p[2]) - 1)
    else:
        j0 = minuit
    j1 = j0 + dt.timedelta(days=1)
    m0 = minuit.replace(day=1)
    debut = min(m0, minuit - dt.timedelta(days=29), j0)
    return {"jour": j0.strftime("%Y-%m-%d"), "j0": int(j0.timestamp()), "j1": int(j1.timestamp()),
            "mois": m0.strftime("%Y-%m-%d"), "m0": int(m0.timestamp()), "debut": debut.isoformat()}


def creneaux(stats5: dict, stats1: dict, ids: list[str]) -> dict:
    """Assemble les lignes de 5 minutes et d'une heure en une seule suite de créneaux par capteur :
    (début en secondes, durée en secondes, puissance moyenne en W, début ISO).

    Les lignes de 5 minutes commencent à la plus ancienne qui existe (toutes séries confondues),
    arrondie à l'heure pleine suivante : avant, ce sont les lignes horaires (celle de l'heure partielle
    existe toujours, compilée avant la purge) ; à partir de là, les lignes de 5 minutes. Une ligne sans
    moyenne est ignorée."""
    premier = None
    for lignes in stats5.values():
        if lignes:
            t = horodatage(lignes[0]["start"])
            if premier is None or t < premier:
                premier = t
    h1 = (int(premier // 3600) + (1 if premier % 3600 > 0 else 0)) * 3600 if premier is not None else None
    sortie = {}
    for sid in ids:
        rows = []
        for r in stats1.get(sid, []):
            t = horodatage(r["start"])
            if r.get("mean") is not None and (h1 is None or t < h1):
                rows.append((int(t), DUREE_HEURE, float(r["mean"]), r["start"]))
        for r in stats5.get(sid, []):
            t = horodatage(r["start"])
            if r.get("mean") is not None and h1 is not None and t >= h1:
                rows.append((int(t), DUREE_5MIN, float(r["mean"]), r["start"]))
        sortie[sid] = rows
    return sortie


# ─── Cœur : charges, part du soleil, heures, mois ────────────────────────────

def analyser(mesures: dict, cap: dict, f: dict) -> dict:
    """Par voiture (clé = rang) : ses charges (de la plus ancienne à la plus récente), ses kWh par
    heure du jour demandé, ses totaux du mois. `mesures` vient de `creneaux()`, `cap` de
    `normaliser()`, `f` de `fenetres()`. Énergies en Wh, `cwh` = énergie des créneaux dont la part
    du soleil est connue, `swh` = la part venue du soleil."""
    idx = {sid: {r[0]: r[2] for r in rows} for sid, rows in mesures.items()}
    sortie = {}
    for k, v in enumerate(cap["voitures"]):
        autre = cap["voitures"][1 - k] if len(cap["voitures"]) == 2 else None
        charges, ouvert = [], None
        mois = {"wh": 0, "cwh": 0, "swh": 0}
        heures = {}
        for (t, d, p, iso) in mesures.get(v["puissance"], []):
            if f["j0"] <= t < f["j1"]:
                h = _locale(iso, f).hour
                heures[h] = heures.get(h, 0) + (p if p > SEUIL_W else 0) * d / 3600
            if p <= SEUIL_W:
                continue
            solaire = 0
            for s in cap["solaires"]:
                x = idx.get(s, {}).get(t)
                if x is not None:
                    solaire = solaire + x
            m = idx.get(cap["maison"], {}).get(t) if cap["maison"] else None
            po = idx.get(autre["puissance"], {}).get(t) if autre else None
            po = po if po is not None and po > SEUIL_W else 0
            tot = p + po
            wh = p * d / 3600
            cwh = swh = 0
            if m is not None:
                hors = max(0, m - tot) if cap["inclut"] else max(0, m)
                dispo = max(0, solaire - hors)
                part = min(1, dispo / tot)
                cwh = wh
                swh = p * part * d / 3600
            if ouvert is not None and t - ouvert["f"] <= TROU_MAX_MIN * 60:
                ouvert["f"] = t + d
                ouvert["wh"] = ouvert["wh"] + wh
                ouvert["cwh"] = ouvert["cwh"] + cwh
                ouvert["swh"] = ouvert["swh"] + swh
            else:
                if ouvert is not None:
                    charges.append(ouvert)
                ouvert = {"d": t, "iso": iso, "f": t + d, "wh": wh, "cwh": cwh, "swh": swh}
            if t >= f["m0"]:
                mois = {"wh": mois["wh"] + wh, "cwh": mois["cwh"] + cwh, "swh": mois["swh"] + swh}
        if ouvert is not None:
            charges.append(ouvert)
        sortie[v["rang"]] = {"charges": charges, "mois": mois, "heures": heures}
    return sortie


def _locale(iso: str, f: dict) -> dt.datetime:
    """Date locale d'une ligne (fuseau du `maintenant` qui a servi à `fenetres`)."""
    return dt.datetime.fromisoformat(iso).astimezone(f["tz"])


def consommation_maison_mois(mesures: dict, cap: dict, f: dict) -> float | None:
    """Wh consommés par la maison depuis le 1er du mois (somme des puissances × durée), None sans
    capteur de maison."""
    if not cap["maison"]:
        return None
    wh = 0
    for (t, d, p, _i) in mesures.get(cap["maison"], []):
        if t >= f["m0"]:
            wh = wh + p * d / 3600
    return wh


# ─── Formats poussés à la tablette (plan §5) ─────────────────────────────────

def _part(cwh: float, swh: float, avec_solaire: bool) -> str:
    """Part du soleil en % entier (0 à 100) ; vide = pas de capteur solaire choisi, « nan » = rien
    de calculable (pas de capteur de maison, aucune ligne de maison pendant la charge)."""
    if not avec_solaire:
        return ""
    if cwh <= 0:
        return "nan"
    return str(min(100, max(0, int(round(100 * swh / cwh)))))


def ligne_charge(rang: str, c: dict, avec_solaire: bool, f: dict) -> str:
    """« rang|jour|heure|duree_min|kwh|part_soleil_pct » : jour AAAA-MM-JJ et heure HH:MM locaux
    du début, durée de la première à la dernière ligne qui charge, kWh à 2 décimales."""
    debut = _locale(c["iso"], f)
    return "|".join([rang, debut.strftime("%Y-%m-%d"), debut.strftime("%H:%M"),
                     str(int(round((c["f"] - c["d"]) / 60))), str(round(c["wh"] / 1000, 2)),
                     _part(c["cwh"], c["swh"], avec_solaire)])


def serie_heures(a: dict) -> str:
    """24 valeurs en kWh (3 décimales) séparées par « ; » : l'heure locale du jour demandé ; vide =
    aucune ligne pour la voiture dans cette heure (heure à venir, voiture muette)."""
    return ";".join(str(round(a["heures"][h] / 1000, 3)) if h in a["heures"] else "" for h in range(24))


def serie_maison(mesures: dict, cap: dict, f: dict, analyse: dict) -> str:
    """24 kWh par heure locale du jour demandé pour la maison SANS les voitures (ADR-0078, page
    « Aujourd'hui » : la pile de consommation) : le capteur de la maison, moins la charge des voitures
    quand ce capteur la compte (jamais sous 0). Vide = aucun capteur de maison, aucune voiture (rien à
    empiler), ou aucune ligne de maison dans cette heure (heure à venir)."""
    if not cap["maison"] or not cap["voitures"]:
        return ""
    wh = {}
    for (t, d, p, iso) in mesures.get(cap["maison"], []):
        if f["j0"] <= t < f["j1"]:
            h = _locale(iso, f).hour
            wh[h] = wh.get(h, 0) + p * d / 3600
    sortie = []
    for h in range(24):
        if h not in wh:
            sortie.append("")
            continue
        v = wh[h]
        if cap["inclut"]:
            voitures = 0
            for x in cap["voitures"]:
                voitures = voitures + analyse[x["rang"]]["heures"].get(h, 0)
            v = max(0, v - voitures)
        sortie.append(str(round(v / 1000, 3)))
    return ";".join(sortie)


def charge_en_texte(analyse: dict, cap: dict, f: dict) -> dict:
    """Les chaînes `charges` (toutes voitures, NB_CHARGES plus récentes d'abord) et `charges_<rang>`."""
    avec = bool(cap["solaires"])
    sortie, toutes = {}, []
    for v in cap["voitures"]:
        recentes = list(reversed(analyse[v["rang"]]["charges"]))[:NB_CHARGES]
        sortie["charges_" + v["rang"]] = ";".join(ligne_charge(v["rang"], c, avec, f) for c in recentes)
        toutes = toutes + [(v["rang"], c) for c in recentes]
    toutes = sorted(toutes, key=lambda x: x[1]["d"], reverse=True)[:NB_CHARGES]
    sortie["charges"] = ";".join(ligne_charge(r, c, avec, f) for r, c in toutes)
    return sortie


def bilan_en_texte(analyse: dict, cap: dict, f: dict, mesures: dict, odo: dict) -> str:
    """Le bilan du mois, 11 champs séparés par « | » (vide = non disponible) :
    pour chaque voiture (rang 1 puis 2, 5 champs chacune, vides si elle n'existe pas) :
      kwh (1 décimale) | part_soleil_pct | km | kwh_100km (1 décimale) | reseau_kwh (1 décimale)
    puis la part de la maison : voitures / consommation de la maison, en % entier.
    `odo` = {rang: km du mois}, None quand l'odomètre manque."""
    avec = bool(cap["solaires"])
    champs, total = [], 0
    for rang in ("1", "2"):
        v = next((x for x in cap["voitures"] if x["rang"] == rang), None)
        if v is None:
            champs += [""] * 5
            continue
        m = analyse[rang]["mois"]
        total = total + m["wh"]
        km = odo.get(rang)
        part = _part(m["cwh"], m["swh"], avec)
        conso = str(round(m["wh"] / 1000 / km * 100, 1)) if km is not None and km >= KM_MIN_CONSO else ""
        reseau = str(round(m["wh"] / 1000 * (1 - m["swh"] / m["cwh"]), 1)) if avec and m["cwh"] > 0 else ""
        champs += [str(round(m["wh"] / 1000, 1)), part, str(int(round(km))) if km is not None else "", conso, reseau]
    maison = consommation_maison_mois(mesures, cap, f)
    if maison is None:
        part_maison = ""
    else:
        base = maison if cap["inclut"] else maison + total
        part_maison = str(min(100, max(0, int(round(100 * total / base))))) if base > 0 else ""
    return "|".join(champs + [part_maison])


def kilometres(odo_stats: dict, cap: dict) -> dict:
    """{rang: km du mois} d'après la réponse de get_statistics (period month, types change, en km),
    None quand la voiture n'a pas d'odomètre ou que le recorder n'a pas de ligne."""
    sortie = {}
    for v in cap["voitures"]:
        lignes = odo_stats.get(v["odometre"], []) if v["odometre"] else []
        sortie[v["rang"]] = float(sum(r.get("change", 0) or 0 for r in lignes)) if lignes else None
    return sortie


# ─── Le tout : la réponse du script ──────────────────────────────────────────

def precision(mesures: dict, cap: dict) -> str:
    """Quelles statistiques ont servi aux voitures : « 5min », « heure », « mixte » ou « aucune »."""
    n5 = n1 = 0
    for v in cap["voitures"]:
        for (_t, d, _p, _i) in mesures.get(v["puissance"], []):
            if d == DUREE_5MIN:
                n5 += 1
            else:
                n1 += 1
    return "aucune" if n5 + n1 == 0 else ("heure" if n5 == 0 else ("5min" if n1 == 0 else "mixte"))


def reponse(stats5: dict, stats1: dict, odo: dict, capteurs, maintenant: dt.datetime, jour: str | None = None) -> dict:
    """Ce que renvoie `script.tab5_voitures_analyse` (chaînes seulement) :

    charges, charges_1, charges_2   lignes « rang|jour|heure|duree_min|kwh|part_soleil_pct » séparées
                                    par « ; » (NB_CHARGES au plus, la plus récente d'abord)
    jour_debut                      AAAA-MM-JJ du jour des heures
    jour_1, jour_2                  24 kWh par heure du jour, séparés par « ; »
    jour_maison                     24 kWh par heure du jour pour la maison sans les voitures (ADR-0078)
    bilan_debut                    AAAA-MM-01 du mois du bilan
    bilan                           11 champs « | » (voir bilan_en_texte)
    precision                       5min | heure | mixte | aucune
    """
    cap = normaliser(capteurs)
    f = fenetres(maintenant, jour)
    f["tz"] = maintenant.tzinfo
    ids = [s for s in dict.fromkeys(cap["solaires"] + ([cap["maison"]] if cap["maison"] else [])
                                    + [v["puissance"] for v in cap["voitures"]])]
    mesures = creneaux(stats5, stats1, ids)
    a = analyser(mesures, cap, f)
    r = {"charges": "", "charges_1": "", "charges_2": ""}
    r.update(charge_en_texte(a, cap, f))
    r["jour_debut"] = f["jour"]
    r["jour_1"] = serie_heures(a["1"]) if "1" in a else ""
    r["jour_2"] = serie_heures(a["2"]) if "2" in a else ""
    r["jour_maison"] = serie_maison(mesures, cap, f, a)
    r["bilan_debut"] = f["mois"]
    r["bilan"] = bilan_en_texte(a, cap, f, mesures, kilometres(odo, cap)) if cap["voitures"] else ""
    r["precision"] = precision(mesures, cap)
    return r
