# -*- coding: utf-8 -*-
"""Réfrigérateurs et congélateurs (ADR-0055) : popup « Froid », alertes et icône de l'horloge.

Aucun compilateur ne relie le package `tab5_froid.yaml`, les listes de `tab5_reglages.yaml`,
la source d'alertes de `tab5_alertes.jinja` et le firmware. Ce fichier le fait :

- contrat : l'action `tab5_maj_froid` (payload), sa lecture (`froid_lire`, tab5_parse.h)
  et ses plafonds face au package et aux listes (quatre appareils EN TOUT) ;
- seuils : ceux de la demande d'Axel (10/10/2026), à un seul endroit du package ;
- package : ses VRAIS modèles Jinja sont rendus (bac à sable de Jinja, comme HA, imitation
  de tests/test_suivi.py) sur des réponses de `recorder.get_statistics` simulées, appel
  après appel (l'état gardé par sensor.tab5_froid est relu), et comparés à un calcul
  Python écrit à part : conforme, trop chaud, coup de chaud (30 min, ou valeur critique),
  porte ouverte puis mal fermée, capteur muet, trop froid, incident et sa fin, °F ;
- alertes : la source « froid » (id stable par appareil, Orange / Rouge, libellé codé
  « @froid: ») et son abonnement ;
- firmware : l'icône qui clignote (glyphe, police, timer créé une fois et mis en pause),
  le tap qui ouvre par la routine unique, le registre, le libellé des alertes.

Ce n'est pas Home Assistant : seules les fonctions de modèle que le package appelle sont
imitées."""
import datetime as dt
import json
import os
import re

import pytest
import yaml

from tests.commun import lire as _lire
from tests.test_alertes_ha import Etat, _env as _env_alertes
from tests.test_historique import EtatHA, _env as _env_historique, _rendre

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAB5 = os.path.join(REPO, "Tab5")
HA = os.path.join(REPO, "HomeAssistant_Config")
PACKAGE = os.path.join(HA, "packages", "tab5_froid.yaml")
REGLAGES = os.path.join(HA, "packages", "tab5_reglages.yaml")
ALERTES = os.path.join(HA, "packages", "tab5_alerts.yaml")
MACRO_ALERTES = os.path.join(HA, "custom_templates", "tab5_alertes.jinja")
PARSE_H = os.path.join(TAB5, "socle", "tab5_parse.h")
FROID_CPP = os.path.join(TAB5, "ecran", "tab5_froid.cpp")
FROID_H = os.path.join(TAB5, "ecran", "tab5_froid.h")
CENTRAL_CPP = os.path.join(TAB5, "ecran", "tab5_central.cpp")
API = os.path.join(TAB5, "paquets", "tab5-api-logic.yaml")
LVGL = os.path.join(TAB5, "paquets", "tab5-lvgl.yaml")
STYLES = os.path.join(TAB5, "paquets", "tab5-styles.yaml")
NAVIGATION = os.path.join(TAB5, "paquets", "tab5-navigation.yaml")

UTC = dt.timezone.utc
T0 = dt.datetime(2026, 10, 9, 14, 37, 20, tzinfo=UTC)
FRIGO = "sensor.frigo_temperature"
CONGEL = "sensor.congelateur_temperature"

# La demande d'Axel (10/10/2026), recopiée ici À PART du package : un seuil changé là-bas
# sans changer ici fait échouer test_seuils_de_la_demande.
DEMANDE = {
    "f": {"bas": 0, "haut": 5, "attention": 5, "gel": 0, "grave": 8, "critique": 10, "porte": 2},
    "c": {"bas": None, "haut": -18, "attention": -15, "gel": None, "grave": -12, "critique": -10, "porte": 4},
}
DUREES = {"duree_grave": 30, "duree_porte": 20, "duree_muet": 30}


def _constante(nom, fichier=PARSE_H):
    m = re.search(rf"constexpr int {nom} = (\d+);", _lire(fichier))
    assert m, nom
    return int(m.group(1))


# ─── Contrat, plafonds et seuils ───────────────────────────────────────────────

def test_action_du_firmware():
    api = _lire(API)
    bloc = api.split("- service: tab5_maj_froid\n", 1)[1].split("\n    - service:", 1)[0]
    variables = yaml.safe_load(bloc.split("variables:\n", 1)[1].split("      then:", 1)[0])
    assert list(variables) == ["payload"] and variables["payload"]["type"] == "string"
    assert "script.execute: tab5_froid_lier" in bloc and "froid_recu(payload);" in bloc
    assert bloc.index("tab5_froid_lier") < bloc.index("froid_recu(payload);"), "widgets liés avant la peinture"


def test_plafonds_du_package_et_des_listes():
    """Quatre appareils au plus partout : la lecture, les cartes du popup, les deux listes de
    HA (ensemble) et le calcul. 24 heures et la valeur actuelle : 25 points, sous le plafond
    de la lecture."""
    assert _constante("kFroidMax") == 4
    assert _constante("kFroidCartes", FROID_H) == 4
    reglages = _lire(REGLAGES).replace("\r\n", "\n")
    assert reglages.count("and entite not in autres and (choisis + autres) | unique | list | count < 4") == 2
    paquet = _lire(PACKAGE)
    assert paquet.count("| unique | list)[:4]") == 2, "signature : les quatre premiers seulement"
    assert "| map('regex_replace', '$', '|c') | list))[:4]" in paquet, "calcul : les quatre premiers seulement"
    assert "range(24)" in paquet and 24 + 1 <= _constante("kFroidPointsMax")
    popup = _lire(TAB5, "ui_components", "froid_popup.yaml")
    assert all(f"froid_carte.yaml, vars: {{ n: \"{i}\" }}" in popup for i in range(4))


def test_seuils_de_la_demande():
    """Un seul endroit (la variable `seuils` du script) et les valeurs demandées."""
    variables = _script()["sequence"][0]["variables"]
    assert variables["seuils"] == DEMANDE
    assert {k: variables[k] for k in DUREES} == DUREES
    paquet = _lire(PACKAGE)
    assert paquet.count("seuils:") == 1 and "critique: 10" in paquet


def test_listes_de_ha():
    """« Tab5 · réfrigérateurs » et « Tab5 · congélateurs » : entity_id fixés, mémoires
    déclarées et déclencheurs, seulement des capteurs de température à statistiques, un
    capteur choisi dans une liste absent de l'autre."""
    paquet = yaml.safe_load(_lire(REGLAGES).replace("!", "!!str "))
    for memoire in ("tab5_choix_frigos", "tab5_choix_congelateurs"):
        assert memoire in paquet["input_text"]
        assert "initial" not in paquet["input_text"][memoire]
    bloc = paquet["template"][0]
    assert {"input_text.tab5_choix_frigos", "input_text.tab5_choix_congelateurs"} <= set(bloc["triggers"][-1]["entity_id"])
    candidats = bloc["variables"]["froid_ha"]
    assert "selectattr('attributes.device_class', 'eq', 'temperature')" in candidats
    assert "selectattr('attributes.state_class', 'eq', 'measurement')" in candidats
    assert "reject('in', frigos_choisis)" in bloc["variables"]["congelateurs_choisis"]
    selects = {s["unique_id"]: s for s in bloc["select"]}
    for uid, nom in (("tab5_refrigerateurs", "Tab5 · réfrigérateurs · fridges"),
                     ("tab5_congelateurs", "Tab5 · congélateurs · freezers")):
        s = selects[uid]
        assert s["name"] == nom and s["default_entity_id"] == f"select.{uid}"
        assert "reject('in', frigos_choisis + congelateurs_choisis)" in s["options"]


# ─── Package : rendu des vrais modèles ─────────────────────────────────────────

def _script():
    return yaml.safe_load(_lire(PACKAGE))["script"]["tab5_froid_calculer"]


def _env(etats, maintenant):
    env = _env_historique(etats, maintenant.astimezone(dt.timezone(dt.timedelta(hours=2))))
    env.globals["utcnow"] = lambda: maintenant
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None  # test match de HA
    env.filters["regex_replace"] = lambda v, motif="", par="": re.sub(motif, par, str(v))  # filtre de HA
    return env


class Maison:
    """Les appareils et leurs états, et la mémoire (sensor.tab5_froid) d'un appel à l'autre."""

    def __init__(self, frigos=(FRIGO,), congelateurs=()):
        self.frigos, self.congelateurs = list(frigos), list(congelateurs)
        self.memoire = None
        self.capteurs = {}

    def capteur(self, entite, etat, nom="Frigo cuisine", unite="°C"):
        self.capteurs[entite] = EtatHA(entite, etat, friendly_name=nom, unit_of_measurement=unite)

    def calculer(self, maintenant, stats_h=None, stats_5=None):
        """Une exécution de script.tab5_froid_calculer : stats_* = réponses de
        get_statistics (None : l'action a échoué, sa variable n'existe pas)."""
        etats = list(self.capteurs.values()) + [
            EtatHA("input_text.tab5_choix_frigos", ",".join(self.frigos)),
            EtatHA("input_text.tab5_choix_congelateurs", ",".join(self.congelateurs))]
        if self.memoire is not None:
            etats.append(EtatHA("sensor.tab5_froid", str(self.memoire["niveau"]), appareils=self.memoire["appareils"]))
        env = _env(etats, maintenant)
        seq = _script()["sequence"]
        ctx = {}
        for nom, v in seq[0]["variables"].items():
            ctx[nom] = _rendre(env, v, ctx)
        demandes = []
        for i, nom, reponse in ((1, "stats_h", stats_h), (2, "stats_5", stats_5)):
            assert seq[i]["action"] == "recorder.get_statistics" and seq[i]["continue_on_error"] is True
            assert seq[i]["response_variable"] == nom
            demandes.append(_rendre(env, seq[i]["data"], ctx))
            if reponse is not None:
                ctx[nom] = {"statistics": reponse}
        for nom, v in seq[3]["variables"].items():
            ctx[nom] = _rendre(env, v, ctx)
        evenement = seq[4]
        assert evenement["event"] == "tab5_froid_bilan"
        donnees = _rendre(env, evenement["event_data"], ctx)
        # Le capteur à déclencheur (template:) garde l'état : sa définition est rendue aussi.
        capteur = _capteur_froid()
        tenv = _env(etats, maintenant)
        trig = {"event": {"data": donnees}}
        self.memoire = {"niveau": _rendre(tenv, capteur["state"], {"trigger": trig}),
                        "appareils": _rendre(tenv, capteur["attributes"]["appareils"], {"trigger": trig})}
        assert seq[5]["condition"] == "template" and "tab5_connectee() == 'oui'" in seq[5]["value_template"]
        pousse = seq[6]
        assert pousse["action"] == "esphome.tab5_ha_hmi_tab5_maj_froid" and pousse["continue_on_error"] is True
        return demandes, str(_rendre(env, pousse["data"]["payload"], ctx))

    def appareil(self, entite=FRIGO):
        return next(a for a in self.memoire["appareils"] if a["e"] == entite)


def _capteur_froid():
    blocs = yaml.safe_load(_lire(PACKAGE))["template"]
    bloc = next(b for b in blocs if "triggers" in b)
    assert bloc["triggers"] == [{"trigger": "event", "event_type": "tab5_froid_bilan"}]
    capteur = bloc["sensor"][0]
    assert capteur["default_entity_id"] == "sensor.tab5_froid"
    return capteur


def _heures(entite, valeurs, maintenant=T0):
    """Lignes horaires depuis l'heure ronde moins 24 h : valeurs = {heure: (mean, min, max)}."""
    debut = maintenant.replace(minute=0, second=0, microsecond=0) - dt.timedelta(hours=24)
    return {entite: [{"start": (debut + dt.timedelta(hours=h)).isoformat(),
                      "end": (debut + dt.timedelta(hours=h + 1)).isoformat(),
                      "mean": m, "min": lo, "max": hi} for h, (m, lo, hi) in sorted(valeurs.items())]}


def _cinq(entite, valeurs, maintenant=T0):
    """Lignes de 5 min : valeurs = [(minutes avant maintenant au début, mean, min)]."""
    return {entite: [{"start": (maintenant - dt.timedelta(minutes=m)).isoformat(),
                      "end": (maintenant - dt.timedelta(minutes=m - 5)).isoformat(), "mean": moy, "min": lo}
                     for m, moy, lo in valeurs]}


# ── Calcul indépendant (écrit à part, sans relire le package) ──

def _reference(t, v, cinq, prec, maintenant):
    """(niveau, cause, depuis) d'un appareil d'après les règles de la demande.
    cinq = [(minutes avant maintenant, mean, min)] ; prec = l'état d'avant (ou {})."""
    s = DEMANDE[t]
    ts = int(maintenant.timestamp())
    if v is None:
        u = prec.get("u") or ts
        if ts - u >= 30 * 60:
            return 1, "indispo", (prec["d"] if prec.get("c") == "indispo" else u)
        return prec.get("n", 0), prec.get("c", "ok"), prec.get("d", 0)
    moyennes = [moy for m, moy, _ in cinq if m <= 20] + [v]
    m15 = sum(moyennes) / len(moyennes)
    min30 = min([lo for m, lo in ((m, lo) for m, _, lo in cinq) if m <= 30] + [v])
    g = (prec.get("g") or ts) if v > s["grave"] else 0
    p = (prec.get("p") or ts) if v - min30 >= s["porte"] else 0
    pm = max(x for x in (prec.get("pm"), v) if x is not None) if p and prec.get("p") else (v if p else None)
    if v >= s["critique"] or (g and ts - g >= 30 * 60):
        n, c = 2, "chaud"
    elif p:
        n, c = (2 if ts - p >= 20 * 60 and v >= pm - 0.5 else 1), "porte"
    elif m15 > s["attention"]:
        n, c = 1, "chaud"
    elif s["gel"] is not None and m15 < s["gel"]:
        n, c = 1, "froid"
    else:
        n, c = 0, "ok"
    if c == "ok":
        d = 0
    elif prec.get("c") == c and prec.get("d"):
        d = prec["d"]
    else:
        d = p if c == "porte" else (g if c == "chaud" and g else ts)
    return n, c, d


def _champs(payload, i=0):
    return payload.split(";")[i].split("|")


def test_liste_vide():
    maison = Maison(frigos=())
    demandes, payload = maison.calculer(T0, {}, {})
    assert payload == ""
    assert maison.memoire == {"niveau": 0, "appareils": []}
    assert demandes[0]["statistic_ids"] == [] and demandes[1]["statistic_ids"] == []


def test_demandes_de_statistiques():
    maison = Maison(frigos=(FRIGO,), congelateurs=(CONGEL, FRIGO))  # en double : réfrigérateur seulement
    maison.capteur(FRIGO, "3.5")
    maison.capteur(CONGEL, "-19", nom="Congélateur")
    (h, cinq), _ = maison.calculer(T0, {}, {})
    assert h["statistic_ids"] == [FRIGO, CONGEL] and cinq["statistic_ids"] == [FRIGO, CONGEL]
    assert h["period"] == "hour" and set(h["types"]) == {"mean", "min", "max"}
    assert cinq["period"] == "5minute" and set(cinq["types"]) == {"mean", "min"}
    assert dt.datetime.fromisoformat(h["start_time"]) == dt.datetime(2026, 10, 8, 14, 0, tzinfo=UTC)
    assert dt.datetime.fromisoformat(cinq["start_time"]) == T0 - dt.timedelta(minutes=35)


def test_conforme_courbe_extremes_et_payload():
    maison = Maison(frigos=(FRIGO,), congelateurs=(CONGEL,))
    maison.capteur(FRIGO, "3.46", nom="Frigo | cuisine; bas")
    maison.capteur(CONGEL, "-19.04", nom="Congélateur")
    heures = {h: (3.0 + h / 10, 2.5 + h / 10, 3.5 + h / 10) for h in range(24) if h != 7}
    _, payload = maison.calculer(T0, {**_heures(FRIGO, heures), **_heures(CONGEL, {0: (-19, -20.5, -18.2)})},
                                 {**_cinq(FRIGO, [(15, 3.4, 3.3), (10, 3.5, 3.4)]), **_cinq(CONGEL, [(10, -19, -19.2)])})
    f = _champs(payload, 0)
    assert len(f) == 15
    assert f[:6] == ["Frigo / cuisine, bas", "f", "3.5", "0", "ok", "0"]
    # Extrêmes : les min / max horaires et la valeur actuelle.
    assert f[6] == "%.1f" % min([lo for _, lo, _ in heures.values()] + [3.46])
    assert f[7] == "%.1f" % max([hi for _, _, hi in heures.values()] + [3.46])
    assert f[8:10] == ["0.0", "5.0"]
    points = f[10].split(",")
    assert len(points) == 25 and points[7] == "" and points[0] == "3.0" and points[-1] == "3.5"
    assert points[1:7] == ["%.1f" % (3.0 + h / 10) for h in range(1, 7)]
    assert f[11:] == ["", "0", "0", ""], "aucun incident terminé"
    c = _champs(payload, 1)
    assert c[1:6] == ["c", "-19.0", "0", "ok", "0"] and c[8:10] == ["", "-18.0"], "congélateur : pas de limite basse"
    assert maison.memoire["niveau"] == 0


@pytest.mark.parametrize("v, cinq, attendu", [
    (4.8, [(15, 4.9, 4.7), (10, 4.8, 4.7)], (0, "ok")),
    (5.6, [(15, 5.4, 5.2), (10, 5.5, 5.3)], (1, "chaud")),        # moyenne de 15 min > 5
    (5.2, [(15, 4.6, 4.5), (10, 4.8, 4.7)], (0, "ok")),          # un pic : la moyenne reste sous 5
    (-0.4, [(15, -0.3, -0.5), (10, -0.4, -0.5)], (1, "froid")),   # risque de gel
    (10.2, [(15, 9.9, 9.8), (10, 10.1, 10.0)], (2, "chaud")),     # critique : tout de suite
    (6.4, [(25, 3.9, 3.8), (15, 4.5, 4.2), (10, 5.8, 5.2)], (1, "porte")),  # +2,6 depuis le min de 30 min
    (6.4, [(40, 3.9, 3.8), (15, 6.0, 5.9), (10, 6.2, 6.1)], (1, "chaud")),  # le minimum a plus de 30 min
])
def test_un_calcul_comme_la_reference(v, cinq, attendu):
    maison = Maison()
    maison.capteur(FRIGO, str(v))
    _, payload = maison.calculer(T0, {}, _cinq(FRIGO, cinq))
    ref = _reference("f", v, cinq, {}, T0)
    assert ref[:2] == attendu
    a = maison.appareil()
    assert (a["n"], a["c"], a["d"]) == ref
    f = _champs(payload)
    assert (int(f[3]), f[4], int(f[5])) == ref
    assert maison.memoire["niveau"] == ref[0]


def _suite(maison, entite, t, etapes):
    """Appels successifs : etapes = [(minutes depuis T0, valeur ou None, cinq)] ; chaque
    résultat est comparé à la référence, nourrie du même état précédent."""
    avant = (maison.memoire or {}).get("appareils") or []
    prec = next((a for a in avant if a["e"] == entite), {})
    for minutes, v, cinq in etapes:
        quand = T0 + dt.timedelta(minutes=minutes)
        maison.capteur(entite, "unavailable" if v is None else str(v),
                       nom="Congélateur" if t == "c" else "Frigo cuisine")
        maison.calculer(quand, {}, _cinq(entite, cinq, quand))
        a = maison.appareil(entite)
        ref = _reference(t, v, cinq, prec, quand)
        assert (a["n"], a["c"], a["d"]) == ref, (minutes, a, ref)
        prec = a
    return prec


def test_coup_de_chaud_apres_30_minutes():
    maison = Maison()
    t0 = int(T0.timestamp())
    a = _suite(maison, FRIGO, "f", [(0, 8.6, [(15, 8.5, 8.4), (10, 8.6, 8.5)]),
                                    (15, 8.7, [(15, 8.6, 8.5), (10, 8.7, 8.6)])])
    assert (a["n"], a["c"], a["g"]) == (1, "chaud", t0), "au-dessus de 8 depuis 15 min : attention"
    a = _suite(maison, FRIGO, "f", [(30, 8.9, [(15, 8.8, 8.7), (10, 8.9, 8.8)])])
    assert (a["n"], a["c"], a["d"]) == (2, "chaud", t0), "30 min : grave, depuis le début de la cause"
    a = _suite(maison, FRIGO, "f", [(35, 7.5, [(15, 8.0, 7.6), (10, 7.6, 7.5)])])
    assert (a["n"], a["c"], a["g"]) == (1, "chaud", 0), "sous 8 : le compte repart"


def test_porte_ouverte_puis_mal_fermee():
    maison = Maison()
    t0 = int(T0.timestamp())
    a = _suite(maison, FRIGO, "f", [(0, 5.9, [(25, 3.6, 3.5), (15, 3.8, 3.6), (10, 4.9, 4.0)])])
    assert (a["n"], a["c"], a["d"], a["pm"]) == (1, "porte", t0, 5.9)
    a = _suite(maison, FRIGO, "f", [(10, 6.6, [(25, 3.8, 3.6), (15, 5.9, 5.2), (10, 6.3, 6.0)]),
                                    (21, 6.8, [(30, 3.9, 3.7), (15, 6.6, 6.4), (10, 6.7, 6.6)])])
    assert (a["n"], a["c"], a["d"]) == (2, "porte", t0), "20 min sans redescendre : porte mal fermée"


def test_porte_refermee_ne_monte_pas_au_niveau_grave():
    maison = Maison()
    a = _suite(maison, FRIGO, "f", [(0, 6.0, [(25, 3.6, 3.5), (15, 3.8, 3.6), (10, 5.2, 4.0)]),
                                    (10, 6.4, [(25, 3.6, 3.5), (15, 6.0, 5.9), (10, 6.3, 6.1)]),
                                    (22, 5.6, [(30, 3.7, 3.5), (15, 6.0, 5.8), (10, 5.8, 5.6)])])
    assert (a["n"], a["c"], a["pm"]) == (1, "porte", 6.4), "elle redescend : pas « mal fermée »"


def test_capteur_muet_et_incident_termine():
    maison = Maison()
    t0 = int(T0.timestamp())
    a = _suite(maison, FRIGO, "f", [(0, 3.9, [(10, 3.9, 3.8)]), (5, None, [])])
    assert (a["n"], a["c"], a["u"]) == (0, "ok", t0 + 300), "muet depuis 0 min : l'état d'avant"
    a = _suite(maison, FRIGO, "f", [(36, None, [])])
    assert (a["n"], a["c"], a["d"]) == (1, "indispo", t0 + 300)
    assert a["i"]["c"] == "indispo" and a["x"] is None
    _, payload = maison.calculer(T0 + dt.timedelta(minutes=36), {}, {})
    assert _champs(payload)[2:6] == ["", "1", "indispo", str(t0 + 300)]
    a = _suite(maison, FRIGO, "f", [(50, 4.0, [(10, 4.0, 3.9)])])
    assert (a["n"], a["c"], a["i"]) == (0, "ok", None)
    assert a["x"] == {"c": "indispo", "d": t0 + 300, "l": 45, "m": None}
    _, payload = maison.calculer(T0 + dt.timedelta(minutes=50), {}, _cinq(FRIGO, [(10, 4.0, 3.9)],
                                                                          T0 + dt.timedelta(minutes=50)))
    assert _champs(payload)[11:] == ["indispo", str(t0 + 300), "45", ""]


def test_incident_garde_la_cause_la_plus_grave_et_le_plus_haut():
    maison = Maison()
    t0 = int(T0.timestamp())
    _suite(maison, FRIGO, "f", [(0, 5.8, [(15, 5.6, 5.5), (10, 5.7, 5.6)]),
                                (5, 10.4, [(15, 5.7, 5.6), (10, 9.0, 8.0)]),   # critique
                                (10, 6.2, [(15, 9.5, 8.2), (10, 7.0, 6.4)]),
                                (15, 4.1, [(15, 6.5, 4.5), (10, 4.3, 4.1)])])
    a = maison.appareil()
    assert a["x"]["c"] == "chaud" and a["x"]["d"] == t0 and a["x"]["m"] == 10.4 and a["x"]["l"] == 15
    _, payload = maison.calculer(T0 + dt.timedelta(minutes=15), {}, {})
    assert _champs(payload)[11:] == ["chaud", str(t0), "15", "10.4"]


def test_congelateur():
    maison = Maison(frigos=(), congelateurs=(CONGEL,))
    a = _suite(maison, CONGEL, "c", [(0, -19.2, [(10, -19.1, -19.3)])])
    assert (a["n"], a["c"]) == (0, "ok")
    a = _suite(maison, CONGEL, "c", [(5, -14.2, [(15, -14.5, -14.8), (10, -14.3, -14.5)])])
    assert (a["n"], a["c"]) == (1, "chaud"), "> -15 sur 15 min (la montée de 4 °C a plus de 30 min)"
    a = _suite(maison, CONGEL, "c", [(10, -11.5, [(15, -14.0, -14.2), (10, -12.5, -13.0)]),
                                     (41, -11.2, [(15, -11.4, -11.5), (10, -11.3, -11.4)])])
    assert (a["n"], a["c"]) == (2, "chaud"), "> -12 pendant 30 min"
    a = _suite(maison, CONGEL, "c", [(45, -30.0, [(10, -29.0, -30.0)])])
    assert (a["n"], a["c"]) == (0, "ok"), "pas de « trop froid » pour un congélateur"


def test_fahrenheit_converti():
    maison = Maison()
    maison.capteur(FRIGO, "50.0", unite="°F")  # 10 °C : critique
    _, payload = maison.calculer(T0, _heures(FRIGO, {0: (41.0, 39.2, 42.8)}), _cinq(FRIGO, [(10, 49.0, 48.2)]))
    f = _champs(payload)
    assert f[2:5] == ["10.0", "2", "chaud"]
    assert f[6:8] == ["4.0", "10.0"] and f[10].split(",")[0] == "5.0"


def test_sans_reponse_du_recorder():
    """get_statistics en échec (continue_on_error) : la valeur actuelle seule."""
    maison = Maison()
    maison.capteur(FRIGO, "6.3")
    _, payload = maison.calculer(T0, None, None)
    f = _champs(payload)
    assert f[2:5] == ["6.3", "1", "chaud"] and f[6:8] == ["6.3", "6.3"]
    assert f[10] == "," * 24 + "6.3"


def test_automatisations():
    paquet = yaml.safe_load(_lire(PACKAGE))
    autos = {a["id"]: a for a in paquet["automation"]}
    chg = autos["tab5_froid"]
    assert chg["mode"] == "single" and chg["max_exceeded"] == "silent"
    assert chg["triggers"] == [{"trigger": "state", "entity_id": "sensor.tab5_signature_du_froid"}]
    assert {"delay": {"minutes": 1}} in chg["actions"], "une minute au plus entre deux calculs"
    imm = autos["tab5_froid_immediat"]
    declencheurs = imm["triggers"]
    assert {"trigger": "time_pattern", "minutes": "/5"} in declencheurs
    assert {d.get("event_type") for d in declencheurs} >= {"esphome.tab5_connected", "esphome.tab5_maj_ecran"}
    assert any(d.get("trigger") == "homeassistant" for d in declencheurs)
    assert any(d.get("entity_id") == ["input_text.tab5_choix_frigos", "input_text.tab5_choix_congelateurs"]
               for d in declencheurs)
    assert "tab5_origine(trigger) == 'oui'" in imm["conditions"][0]["value_template"]
    assert _script()["mode"] == "queued"


# ─── Alertes de la carte centrale ──────────────────────────────────────────────

def _sources(etats):
    gabarit = _env_alertes(etats).from_string(
        "{% from 'tab5_alertes.jinja' import tab5_alertes_sources %}{{ tab5_alertes_sources(i, a, s) }}")
    return json.loads(gabarit.render(i={}, a=[], s=20))


def test_source_froid():
    appareils = [
        {"e": FRIGO, "nom": "Frigo: cuisine", "n": 2, "c": "porte", "v": 9.14},
        {"e": CONGEL, "nom": "Congélateur", "n": 1, "c": "indispo", "v": None},
        {"e": "sensor.cave", "nom": "Cave", "n": 0, "c": "ok", "v": 4.0},
    ]
    actives = _sources([Etat("sensor.tab5_froid", "2", {"appareils": appareils})])["actives"]
    froid = {k: v for k, v in actives.items() if k.startswith("froid:")}
    assert froid == {
        f"froid:{FRIGO}": {"c": "porte|2", "g": "Rouge", "s": "froid", "t": "@froid:porte:2:9.1:Frigo: cuisine"},
        f"froid:{CONGEL}": {"c": "indispo|1", "g": "Orange", "s": "froid", "t": "@froid:indispo:1::Congélateur"},
    }


def test_source_froid_en_doute_et_abonnement():
    texte = _lire(MACRO_ALERTES)
    assert "i.startswith('froid:')" in texte and "states('sensor.tab5_froid') in ['unavailable', 'unknown']" in texte
    assert "'froid': states('input_select.tab5_alertes_froid') != 'Non'" in texte
    paquet = yaml.safe_load(_lire(ALERTES).replace("!", "!!str "))
    assert paquet["input_select"]["tab5_alertes_froid"]["options"] == ["Oui", "Non"]
    declencheurs = paquet["template"][0]["triggers"]
    assert {"trigger": "state", "entity_id": "sensor.tab5_froid", "id": "tick"} in declencheurs
    assert any("input_select.tab5_alertes_froid" in (d.get("entity_id") or []) for d in declencheurs)


# ─── Firmware ──────────────────────────────────────────────────────────────────

def test_icone_de_l_horloge():
    """fridge-alert (F11B1) dans le coin haut droit de l'horloge, mdi_font_26, couleur par
    le rôle « erreur » ; le tap ouvre le popup par la routine unique."""
    lvgl = _lire(LVGL)
    bouton = lvgl.split("id: btn_froid_alerte", 1)[1].split("\n            - ", 1)[0]
    assert "hidden: true" in bouton
    assert "id(tab5_ecran_ouvrir).execute((int) Ecran::FROID);" in bouton
    assert re.search(r'id: froid_alerte_icone, text: "\\U000F11B1".*text_font: mdi_font_26.*style_text_error', bouton)
    styles = _lire(STYLES)
    police = styles.split("id: mdi_font_26", 1)[1].split("- file:", 1)[0]
    assert '"\\U000F11B1"' in police


def test_clignotement_sans_fondu():
    """Un lv_timer de 500 ms créé une fois, en pause quand l'icône est cachée ; visible au
    niveau 2 seulement ; aucune animation (pas de fondu)."""
    cpp = _lire(FROID_CPP)
    assert cpp.count("lv_timer_create(") == 1 and "kClignoteMs" in cpp.split("lv_timer_create(", 1)[1].split(";", 1)[0]
    assert re.search(r"constexpr uint32_t kClignoteMs = 500;", cpp)
    assert "lv_timer_pause(" in cpp and "lv_timer_resume(" in cpp
    assert "froid_niveau_max(s_lu, s_n) >= 2" in cpp
    assert "lv_anim" not in cpp


def test_registre_et_libelle_des_alertes():
    nav = _lire(NAVIGATION)
    assert re.search(r'ModalRegistry::add\(id\(froid_popup\),\s+"Froid",\s+ModalRegistry::POPUP', nav)
    assert "id(tab5_froid_ouvrir).execute();" in nav
    central = _lire(CENTRAL_CPP)
    bloc = central.split("case AlerteTexteCode::FROID:", 1)[1].split("default: return normalize_text_utf8(brut);", 1)[0]
    assert "froid_alerte_lire(lu.reste, f)" in bloc
    for mot in ("trop chaud", "coup de chaud", "trop froid", "porte ouverte ?", "porte mal fermée", "capteur muet"):
        assert f'tr("{mot}")' in bloc, mot
