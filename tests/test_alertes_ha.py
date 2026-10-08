# -*- coding: utf-8 -*-
"""Alertes de la carte centrale côté Home Assistant (packages/tab5_alerts.yaml et
custom_templates/tab5_alertes.jinja, plan des alertes du 06/10/2026).

Une alerte touchée sur la tablette ne revient plus, même après un redémarrage ou un
plantage de HA, sauf si elle change ou si elle s'arrête vraiment puis recommence. La
macro réelle est rendue ici comme dans HA (bac à sable Jinja, filtres to_json et
from_json) et rejouée sur des scénarios : redémarrages, coupures, versions, taps.
Le comportement de HA lui-même (restauration, plantage) est vérifié par le job
« Installation dans un HA neuf » (verifier_alertes).
"""
from __future__ import annotations

import datetime as dt
import json
import re
from pathlib import Path

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

RACINE = Path(__file__).resolve().parents[1]
HA = RACINE / "HomeAssistant_Config"
MACROS = HA / "custom_templates"

T0 = 1_790_000_000  # une heure quelconque (epoch), loin de tout démarrage
TOUT = {"maj": "toutes", "probleme": True, "indispo": True, "vigilance": "Jaune"}


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda *_: None)


def _charger(*chemin: str):
    return yaml.load((HA.joinpath(*chemin)).read_text(encoding="utf-8"), Loader=_Chargeur) or {}


# ─── Les états de HA, juste ce que lit la macro ──────────────────────────────────

class Etat:
    def __init__(self, entity_id, state, attributes=None, depuis=None, etiquettes=(), integration=""):
        self.entity_id = entity_id
        self.etiquettes = list(etiquettes)
        self.integration = integration
        self.state = state
        self.attributes = dict(attributes or {})
        self.domain = entity_id.split(".", 1)[0]
        self.name = self.attributes.get("friendly_name", entity_id)
        self.last_changed = depuis or dt.datetime.fromtimestamp(T0 - 3600, dt.timezone.utc)


class _Domaine:
    def __init__(self, etats, domaine):
        self._etats = [e for e in etats if e.domain == domaine]

    def __iter__(self):
        return iter(self._etats)

    def __getattr__(self, objet):
        return next((e for e in self._etats if e.entity_id.split(".", 1)[1] == objet), None)


class States:
    def __init__(self, etats):
        self._etats = {e.entity_id: e for e in etats}

    def __call__(self, entity_id):
        e = self._etats.get(entity_id)
        return e.state if e else "unknown"

    def __getitem__(self, entity_id):
        return self._etats.get(entity_id)

    def __iter__(self):
        return iter(sorted(self._etats.values(), key=lambda e: e.entity_id))

    def __getattr__(self, domaine):
        # Le bac à sable lit unsafe_callable et alters_data avant d'appeler states(…).
        if domaine in ("unsafe_callable", "alters_data") or domaine.startswith("__"):
            raise AttributeError(domaine)
        return _Domaine(self._etats.values(), domaine)


def _env(etats=(), maintenant=T0):
    env = ImmutableSandboxedEnvironment(loader=jinja2.FileSystemLoader(str(MACROS)),
                                        extensions=["jinja2.ext.loopcontrols"],
                                        undefined=jinja2.StrictUndefined)
    liste = list(etats)
    etats = States(liste)
    env.globals.update(
        states=etats, timedelta=dt.timedelta,
        label_entities=lambda nom: [e.entity_id for e in liste if nom in e.etiquettes],
        integration_entities=lambda nom: [e.entity_id for e in liste if e.integration == nom],
        now=lambda: dt.datetime.fromtimestamp(maintenant, dt.timezone.utc),
        state_attr=lambda e, a: etats[e].attributes.get(a) if etats[e] else None,
    )
    env.filters.update(to_json=lambda v: json.dumps(v, ensure_ascii=False), from_json=json.loads)
    env.tests["is_number"] = _est_un_nombre
    return env


def _est_un_nombre(valeur) -> bool:
    """Le test is_number de HA : un nombre fini, ou un texte qui en est un."""
    try:
        return float(valeur) not in (float("inf"), float("-inf")) and float(valeur) == float(valeur)
    except (TypeError, ValueError):
        return False


def logique(memoire, sources, evenement="tick", maintenant=T0, abonnements=None, data=None):
    gabarit = _env().from_string(
        "{% from 'tab5_alertes.jinja' import tab5_alertes_logique %}"
        "{{ tab5_alertes_logique(m, s, e, t, a) }}")
    return json.loads(gabarit.render(m=memoire, s=sources, e={"type": evenement, "data": data or {}},
                                     t=maintenant, a=abonnements or TOUT))


def maj(version, critique=False):
    return {"c": version, "g": "Rouge" if critique else "Orange", "t": "@maj:Paquet", "s": "maj"}


def probleme(nom="Fuite"):
    return {"c": "on", "g": "Rouge", "t": nom, "s": "probleme"}


def src(actives=None, incertains=(), absents=(), anciennes=()):
    return {"actives": actives or {}, "incertains": list(incertains), "absents": list(absents),
            "anciennes": list(anciennes)}


class Capteur:
    """Le capteur « Tab5 Alertes » : ses attributs passent d'un déclenchement au suivant."""

    def __init__(self, abonnements=None):
        self.m = {}
        self.t = T0
        self.ab = abonnements or TOUT

    def __call__(self, actives=None, evenement="tick", data=None, avance=60, **kw):
        self.t += avance
        self.m = logique(self.m, src(actives, **kw), evenement, self.t, self.ab, data)
        return [a["i"] for a in self.m["affichees"]]

    def tap(self, alert_id, actives=None, **kw):
        return self(actives, "lue", {"alert_id": alert_id}, avance=1, **kw)


# ─── Le paquet et la macro sont branchés comme prévu ─────────────────────────────

def test_aucun_input_text_des_packages_n_a_d_initial():
    """`initial:` empêche HA de restaurer la valeur au démarrage (input_text de HA
    2026.9 : restauration seulement si la valeur de départ est None)."""
    fautifs = []
    for fichier in sorted((HA / "packages").glob("*.yaml")):
        for cle, conf in (_charger("packages", fichier.name).get("input_text") or {}).items():
            if isinstance(conf, dict) and "initial" in conf:
                fautifs.append(f"{fichier.name} : input_text.{cle}")
    assert not fautifs, fautifs


def _bloc_alertes():
    blocs = [b for b in _charger("packages", "tab5_alerts.yaml")["template"]
             if any(s.get("unique_id") == "tab5_alertes" for s in b.get("sensor", []))]
    assert len(blocs) == 1
    return blocs[0]


def test_la_memoire_est_calculee_d_un_seul_tenant():
    """Les `variables:` d'un bloc à déclencheurs sont rendues sans interruption (HA
    2026.9.4, TriggerUpdateCoordinator._handle_triggered) : un tap et le calcul de la
    minute ne peuvent pas se doubler. Des `actions:` passeraient par un script en mode
    « single », qui ignore un déclenchement arrivé pendant le précédent : un tap perdu."""
    bloc = _bloc_alertes()
    assert "actions" not in bloc and "action" not in bloc
    assert "tab5_alertes_memoire" in bloc["variables"]["memoire"]
    ids = {t.get("id") for t in bloc["triggers"]}
    assert {"demarrage", "tick", "lue", "reglage"} <= ids
    types = {t.get("event_type") for t in bloc["triggers"] if t.get("trigger") == "event"}
    assert {"tab5_alertes_lue", "tab5_alertes_recalculer"} <= types
    capteur = bloc["sensor"][0]
    assert capteur["default_entity_id"] == "sensor.tab5_alertes"
    assert set(capteur["attributes"]) >= {"affichees", "suivi", "lues", "historique", "demarrage"}


def test_un_tap_est_ecrit_sur_le_disque_tout_de_suite():
    """HA n'écrit les états restaurés qu'à l'arrêt propre et toutes les 15 min : après
    un plantage, un tap non sauvegardé serait perdu. Une automatisation sauvegarde à
    chaque changement des alertes lues."""
    paquet = _charger("packages", "tab5_alerts.yaml")
    sauvegardes = [a for a in paquet["automation"]
                   if "homeassistant.save_persistent_states" in json.dumps(a.get("actions", a.get("action")))]
    assert len(sauvegardes) == 1
    declencheurs = sauvegardes[0].get("triggers", sauvegardes[0].get("trigger"))
    assert {"entity_id": "sensor.tab5_alertes", "attribute": "lues"}.items() <= declencheurs[0].items()
    script = json.dumps(paquet["script"]["tab5_dismiss_alert"]["sequence"])
    assert "tab5_alertes_lue" in script and "input_text.set_value" not in script


def test_la_purge_de_4_h_a_disparu():
    """Elle oubliait des alertes lues sur un état passager et gardait « ha:unavailable »
    muet pour toujours : la fin confirmée la remplace."""
    ids = [a.get("id") for a in _charger("packages", "tab5_alerts.yaml")["automation"]]
    assert "tab5_alerts_dismissed_cleanup" not in ids


# ─── Les règles ──────────────────────────────────────────────────────────────────

def test_une_alerte_lue_ne_revient_pas():
    c = Capteur()
    assert c({"update.x": maj("1.0")}) == ["update.x#1"]
    assert c.tap("update.x#1", {"update.x": maj("1.0")}) == []
    assert c({"update.x": maj("1.0")}) == []
    assert c.m["lues"] == {"update.x": 1}


def test_une_nouvelle_version_revient_meme_lue():
    c = Capteur()
    c({"update.x": maj("1.0")})
    c.tap("update.x#1", {"update.x": maj("1.0")})
    assert c({"update.x": maj("1.1")}) == ["update.x#2"]


def test_un_tap_sur_l_ancienne_version_laisse_la_nouvelle_a_lire():
    """La version a changé entre la poussée et le tap : la tablette renvoie #1."""
    c = Capteur()
    c({"update.x": maj("1.0")})
    c({"update.x": maj("1.1")})
    assert c.tap("update.x#1", {"update.x": maj("1.1")}) == ["update.x#2"]


def test_un_redemarrage_de_ha_ne_fait_rien_revenir():
    """Au démarrage, les entités sont d'abord inconnues ou absentes : ni une fin ni un
    changement. Revenues identiques, l'alerte reste lue."""
    c = Capteur()
    c({"update.x": maj("1.0"), "binary_sensor.f": probleme()})
    c.tap("update.x#1", {"update.x": maj("1.0"), "binary_sensor.f": probleme()})
    c.tap("binary_sensor.f#1", {"update.x": maj("1.0"), "binary_sensor.f": probleme()})
    assert c(evenement="demarrage", incertains=["binary_sensor.f"], absents=["update.x"]) == []
    for _ in range(20):  # 20 min sans elles : la grâce de 15 min les garde
        assert c(incertains=["binary_sensor.f"], absents=["update.x"]) == []
    assert c({"update.x": maj("1.0"), "binary_sensor.f": probleme()}) == []
    assert c.m["lues"] == {"update.x": 1, "binary_sensor.f": 1}


def test_une_source_indisponible_n_est_jamais_une_fin():
    c = Capteur()
    c({"binary_sensor.f": probleme()})
    c.tap("binary_sensor.f#1", {"binary_sensor.f": probleme()})
    for _ in range(120):
        c(incertains=["binary_sensor.f"])
    assert c({"binary_sensor.f": probleme()}) == []


def test_revenue_a_la_normale_elle_quitte_l_ecran_tout_de_suite():
    c = Capteur()
    assert c({"binary_sensor.f": probleme()}) == ["binary_sensor.f#1"]
    assert c() == []
    assert c.m["suivi"]["binary_sensor.f"]["f"] == c.t


def test_une_coupure_breve_ne_fait_pas_une_nouvelle_alerte():
    c = Capteur()
    c({"binary_sensor.f": probleme()})
    c.tap("binary_sensor.f#1", {"binary_sensor.f": probleme()})
    c(avance=60)
    c(avance=180)  # 4 min à la normale : pas encore une fin (5 min)
    assert c({"binary_sensor.f": probleme()}) == []


def test_apres_une_vraie_fin_elle_revient():
    c = Capteur()
    c({"binary_sensor.f": probleme()})
    c.tap("binary_sensor.f#1", {"binary_sensor.f": probleme()})
    c()
    c(avance=300)
    assert "binary_sensor.f" not in c.m["suivi"] and "binary_sensor.f" not in c.m["lues"]
    assert c({"binary_sensor.f": probleme()}) == ["binary_sensor.f#1"]


def test_rien_ne_se_termine_pendant_la_grace_d_un_demarrage():
    c = Capteur()
    c({"binary_sensor.f": probleme()})
    c.tap("binary_sensor.f#1", {"binary_sensor.f": probleme()})
    c(evenement="demarrage")
    c(avance=600)  # 10 min à la normale, mais HA vient de démarrer
    assert "binary_sensor.f" in c.m["suivi"]
    assert c({"binary_sensor.f": probleme()}) == []
    c()
    c(avance=900)
    assert "binary_sensor.f" not in c.m["suivi"]


def test_une_entite_disparue_attend_une_heure():
    c = Capteur()
    c({"update.x": maj("1.0")})
    c.tap("update.x#1", {"update.x": maj("1.0")})
    c(absents=["update.x"])
    c(absents=["update.x"], avance=3000)
    assert c({"update.x": maj("1.0")}) == []
    c(absents=["update.x"])
    c(absents=["update.x"], avance=3600)
    assert "update.x" not in c.m["suivi"]


def test_indisponibles_seulement_une_nouvelle_entite():
    def indispo(*ids):
        return {"ha:indispo": {"c": list(ids), "g": "Orange", "t": f"@indispo:{len(ids)}", "s": "indispo"}}

    c = Capteur()
    assert c(indispo("a", "b")) == ["ha:indispo#1"]
    c.tap("ha:indispo#1", indispo("a", "b"))
    assert c(indispo("a")) == []
    assert c(indispo("a", "b")) == []  # b retombe : déjà vue
    assert c(indispo("a", "c")) == ["ha:indispo#2"]
    assert c.m["affichees"][0]["t"] == "@indispo:2"
    assert c.m["suivi"]["ha:indispo"]["c"] == ["a", "b", "c"]


def test_vigilance_niveau_ou_phenomenes():
    def vigi(niveau, phen):
        return {"meteo:vigilance": {"c": f"{niveau}|{phen}", "g": niveau, "t": f"@vigi:{niveau}", "s": "vigilance"}}

    c = Capteur()
    assert c(vigi("Jaune", "Vert|Jaune")) == ["meteo:vigilance#1"]
    c.tap("meteo:vigilance#1", vigi("Jaune", "Vert|Jaune"))
    assert c(vigi("Jaune", "Jaune|Vert")) == ["meteo:vigilance#2"]
    c.tap("meteo:vigilance#2", vigi("Jaune", "Jaune|Vert"))
    assert c(vigi("Orange", "Orange|Vert")) == ["meteo:vigilance#3"]


def test_ordre_gravite_puis_la_plus_ancienne():
    c = Capteur()
    c({"update.b": maj("1")})
    c({"update.b": maj("1"), "update.a": maj("1")})
    actives = {"update.b": maj("1"), "update.a": maj("1"), "update.core": maj("1", critique=True),
               "binary_sensor.f": probleme()}
    # Rouges d'abord (même minute : par id), puis les orange, la plus ancienne d'abord.
    assert c(actives) == ["binary_sensor.f#1", "update.core#1", "update.b#1", "update.a#1"]


def test_anciens_identifiants_sans_revision():
    """Un bandeau poussé par l'ancien package, touché après la mise à jour de HA."""
    c = Capteur()
    actives = {"ha:indispo": {"c": ["a"], "g": "Orange", "t": "@indispo:1", "s": "indispo"},
               "meteo:vigilance": {"c": "Orange|x", "g": "Orange", "t": "@vigi:Orange", "s": "vigilance"},
               "update.x": maj("1.0")}
    c(actives)
    assert c.tap("ha:unavailable", actives) == ["meteo:vigilance#1", "update.x#1"]
    assert c.tap("meteo:orange", actives) == ["update.x#1"]
    assert c.tap("update.x", actives) == []


def test_reprise_de_l_ancienne_liste():
    """Première fois : les alertes déjà lues le restent, sauf « ha:unavailable »
    (l'ancienne liste le gardait muet pour toujours) et une vigilance d'un autre niveau."""
    actives = {"update.x": maj("1.0"), "update.y": maj("2.0"),
               "ha:indispo": {"c": ["a"], "g": "Orange", "t": "@indispo:1", "s": "indispo"},
               "meteo:vigilance": {"c": "Jaune|x", "g": "Jaune", "t": "@vigi:Jaune", "s": "vigilance"}}
    m = logique({}, src(actives, anciennes=["update.x", "ha:unavailable", "meteo:orange"]))
    assert [a["i"] for a in m["affichees"]] == ["ha:indispo#1", "update.y#1", "meteo:vigilance#1"]
    m = logique({}, src(actives, anciennes=["meteo:jaune"]))
    assert "meteo:vigilance" in m["lues"]
    # Plus jamais ensuite.
    m2 = logique(m, src(actives, anciennes=["update.x"]))
    assert "update.x" not in m2["lues"]


def test_tout_marquer_comme_lu():
    c = Capteur()
    actives = {"update.x": maj("1.0"), "binary_sensor.f": probleme()}
    c(actives)
    assert c.tap("*", actives) == []
    assert c.m["lues"] == {"update.x": 1, "binary_sensor.f": 1}


def test_abonnements():
    actives = {"update.x": maj("1.0"), "update.core": maj("1", critique=True), "binary_sensor.f": probleme(),
               "meteo:vigilance": {"c": "Jaune|x", "g": "Jaune", "t": "@vigi:Jaune", "s": "vigilance"}}
    c = Capteur({"maj": "ha", "probleme": False, "indispo": True, "vigilance": "Orange"})
    assert c(actives) == ["update.core#1"]
    # « tout lu » ne lit que ce qui est montré : réabonnée, la fuite reste à lire.
    c.tap("*", actives)
    c.ab = TOUT
    assert c(actives) == ["binary_sensor.f#1", "update.x#1", "meteo:vigilance#1"]


def test_historique():
    c = Capteur()
    c({"update.x": maj("1.0")})
    apparue = c.t
    c.tap("update.x#1", {"update.x": maj("1.0")})
    lue = c.t
    c({"update.x": maj("1.1")})
    h = c.m["historique"]
    assert [(e["i"], e["r"]) for e in h] == [("update.x", 2), ("update.x", 1)]
    assert h[1]["a"] == apparue and h[1]["l"] == lue and h[1]["f"] == c.t  # remplacée
    c()
    fin = c.t
    c(avance=300)
    assert c.m["historique"][0]["f"] == fin and c.m["historique"][0]["l"] == 0
    for i in range(40):
        c({f"update.n{i}": maj("1")})
    assert len(c.m["historique"]) == 30


def test_rien_ne_change_quand_rien_ne_change():
    """Le calcul tourne chaque minute : sans changement, HA n'écrit pas l'état (pas une
    ligne de plus en base par minute)."""
    c = Capteur()
    actives = {"update.x": maj("1.0"), "binary_sensor.f": probleme()}
    c(actives)
    avant = c.m
    c(actives)
    assert c.m == avant


# ─── Les états de HA ─────────────────────────────────────────────────────────────

def sources(etats, suivi_ids=(), anciennes=(), seuil=20):
    gabarit = _env(etats).from_string(
        "{% from 'tab5_alertes.jinja' import tab5_alertes_sources %}{{ tab5_alertes_sources(i, a, s) }}")
    return json.loads(gabarit.render(i={i: {} for i in suivi_ids}, a=list(anciennes), s=seuil))


def test_sources_lues_dans_les_etats_de_ha():
    recent = dt.datetime.fromtimestamp(T0 - 120, dt.timezone.utc)
    etats = [
        Etat("update.esphome_update", "on", {"title": "ESPHome | dev; test", "latest_version": "2026.9.2"}),
        Etat("update.home_assistant_core_update", "on", {"friendly_name": "Core", "latest_version": "2026.10.0"}),
        Etat("update.a_jour", "off", {"latest_version": "1"}),
        Etat("binary_sensor.fuite", "on", {"device_class": "problem", "friendly_name": "Fuite cuisine"}),
        Etat("binary_sensor.porte", "on", {"device_class": "door"}),
        Etat("sensor.vieux", "unavailable"),
        Etat("sensor.recent", "unavailable", depuis=recent),
        Etat("update.indispo", "unavailable"),
        Etat("binary_sensor.panne", "unavailable", {"device_class": "problem"}),
        Etat("sensor.tab5_vigilance", "Orange", {"phenomenes": "Orange|Vert"}),
    ]
    s = sources(etats, ["binary_sensor.panne", "update.disparue", "update.esphome_update"])
    a = s["actives"]
    assert a["update.esphome_update"] == {"c": "2026.9.2", "g": "Orange", "t": "@maj:ESPHome / dev, test", "s": "maj"}
    assert a["update.home_assistant_core_update"]["g"] == "Rouge"
    assert a["update.home_assistant_core_update"]["t"] == "@maj:Core"
    assert a["binary_sensor.fuite"] == {"c": "on", "g": "Rouge", "t": "Fuite cuisine", "s": "probleme"}
    assert a["ha:indispo"]["c"] == ["binary_sensor.panne", "sensor.vieux"]  # ni récent, ni update
    assert a["meteo:vigilance"] == {"c": "Orange|Orange|Vert", "g": "Orange", "t": "@vigi:Orange", "s": "vigilance"}
    assert set(a) == {"update.esphome_update", "update.home_assistant_core_update", "binary_sensor.fuite",
                      "ha:indispo", "meteo:vigilance"}
    assert s["incertains"] == ["binary_sensor.panne"] and s["absents"] == ["update.disparue"]


def test_vigilance_inconnue_dans_le_doute():
    s = sources([Etat("sensor.tab5_vigilance", "unknown")], ["meteo:vigilance"])
    assert s["incertains"] == ["meteo:vigilance"]
    s = sources([Etat("sensor.tab5_vigilance", "Vert")], ["meteo:vigilance"])
    assert s["incertains"] == [] and "meteo:vigilance" not in s["actives"]


@pytest.mark.parametrize("premiere", [True, False])
def test_le_capteur_lit_sa_propre_memoire(premiere):
    """tab5_alertes_memoire : mémoire = attributs du capteur ; l'ancienne liste n'est lue
    que la première fois."""
    etats = [Etat("update.x", "on", {"latest_version": "1"}),
             Etat("input_text.tab5_alerts_dismissed", "update.x")]
    if not premiere:
        etats.append(Etat("sensor.tab5_alertes", "1", {"suivi": {}, "lues": {}, "historique": []}))
    gabarit = _env(etats).from_string(
        "{% from 'tab5_alertes.jinja' import tab5_alertes_memoire %}{{ tab5_alertes_memoire('tick', {}) }}")
    m = json.loads(gabarit.render())
    assert ("update.x" in m["lues"]) is premiere


# ─── Abonnements (lot 2) ─────────────────────────────────────────────────────────

LISTES = {
    "tab5_alertes_maj": ["Toutes", "Home Assistant seulement", "Aucune"],
    "tab5_alertes_vigilance": ["Jaune", "Orange", "Rouge", "Aucune"],
    "tab5_alertes_problemes": ["Oui", "Non"],
    "tab5_alertes_indisponibles": ["Oui", "Non"],
    "tab5_alertes_etiquette": ["Oui", "Non"],
}


def test_les_abonnements_sont_des_listes_qui_gardent_le_choix():
    """Sans `initial`, une liste démarre sur sa première option (le choix par défaut,
    tout affiché, piles sous 20 %) puis HA restaure le choix. Chaque liste fait recalculer
    les alertes tout de suite, et la macro lit exactement ces options."""
    listes = _charger("packages", "tab5_alerts.yaml")["input_select"]
    for cle, options in LISTES.items():
        assert listes[cle]["options"] == options, cle
    piles = listes["tab5_alertes_piles"]["options"]
    assert piles[0] == "20 %" and piles[-1] == "Aucune"
    assert all(o.endswith(" %") and o.split(" ")[0].isdigit() for o in piles[:-1])
    for cle, conf in listes.items():
        assert "initial" not in conf, cle
        assert conf["name"].startswith("Tab5 · alertes : "), cle
    surveillees = {e for t in _bloc_alertes()["triggers"] if t.get("trigger") == "state"
                   for e in ([t["entity_id"]] if isinstance(t["entity_id"], str) else t["entity_id"])}
    assert {f"input_select.{cle}" for cle in listes} <= surveillees
    macro = (MACROS / "tab5_alertes.jinja").read_text(encoding="utf-8")
    for cle in listes:
        assert f"input_select.{cle}" in macro, cle


def memoire(etats):
    gabarit = _env(etats).from_string(
        "{% from 'tab5_alertes.jinja' import tab5_alertes_memoire %}{{ tab5_alertes_memoire('tick', {}) }}")
    return json.loads(gabarit.render())


MAISON = [
    Etat("update.esphome_update", "on", {"latest_version": "2"}),
    Etat("update.home_assistant_core_update", "on", {"latest_version": "2026.10.0"}),
    Etat("binary_sensor.fuite", "on", {"device_class": "problem"}),
    Etat("binary_sensor.porte", "on", {"device_class": "door"}, etiquettes=["Tab5 · alerte"]),
    Etat("sensor.pot_batterie", "12", {"device_class": "battery"}),
    Etat("sensor.vieux", "unavailable"),
    Etat("sensor.tab5_vigilance", "Orange", {"phenomenes": "Orange"}),
]


def test_abonnements_lus_dans_les_listes():
    tout = [a["i"].split("#")[0] for a in memoire(MAISON)["affichees"]]
    assert set(tout) == {"update.esphome_update", "update.home_assistant_core_update", "binary_sensor.fuite",
                         "binary_sensor.porte", "sensor.pot_batterie", "ha:indispo", "meteo:vigilance"}
    choix = [Etat("input_select.tab5_alertes_maj", "Home Assistant seulement"),
             Etat("input_select.tab5_alertes_vigilance", "Rouge"),
             Etat("input_select.tab5_alertes_problemes", "Non"),
             Etat("input_select.tab5_alertes_indisponibles", "Non"),
             Etat("input_select.tab5_alertes_etiquette", "Non"),
             Etat("input_select.tab5_alertes_piles", "Aucune")]
    m = memoire(MAISON + choix)
    assert [a["i"] for a in m["affichees"]] == ["update.home_assistant_core_update#1"]
    # Pas affichées, mais suivies : s'y réabonner ne fait pas revenir ce qui était lu.
    assert {"binary_sensor.fuite", "binary_sensor.porte", "sensor.pot_batterie"} <= set(m["suivi"])
    seuil_10 = memoire(MAISON[:-3] + [Etat("sensor.pot_batterie", "12", {"device_class": "battery"}),
                                      Etat("input_select.tab5_alertes_piles", "10 %")])
    assert "sensor.pot_batterie" not in seuil_10["suivi"]


def test_desabonnee_puis_reabonnee_une_alerte_lue_ne_revient_pas():
    porte = {"binary_sensor.porte": {"c": "on", "g": "Orange", "t": "Porte", "s": "etiquette"}}
    c = Capteur()
    c(porte)
    c.tap("binary_sensor.porte#1", porte)
    c.ab = dict(TOUT, etiquette=False)
    assert c(porte) == []
    c.ab = TOUT
    assert c(porte) == []


def test_etiquette_et_piles_lues_dans_les_etats_de_ha():
    alerte = ["Tab5 · alerte"]
    etats = [
        Etat("binary_sensor.porte", "on", {"device_class": "door", "friendly_name": "Porte"}, etiquettes=alerte),
        Etat("binary_sensor.fumee", "on", {"device_class": "smoke"}, etiquettes=alerte),
        Etat("binary_sensor.fenetre", "off", {"device_class": "window"}, etiquettes=alerte),
        Etat("lock.entree", "unlocked", etiquettes=alerte),
        Etat("alarm_control_panel.maison", "triggered", etiquettes=alerte),
        Etat("cover.garage", "open", etiquettes=alerte),
        Etat("binary_sensor.sans_etiquette", "on", {"device_class": "door"}),
        Etat("update.etiquetee", "on", {"latest_version": "3"}, etiquettes=alerte),
        Etat("sensor.pot_1", "15", {"device_class": "battery", "friendly_name": "Pot 1"}),
        Etat("sensor.pot_2", "25", {"device_class": "battery"}),
        Etat("sensor.pot_3", "25", {"device_class": "battery"}),
        Etat("sensor.pot_4", "31", {"device_class": "battery"}),
        Etat("sensor.pot_5", "unavailable", {"device_class": "battery"}),
        Etat("sensor.telephone", "5", {"device_class": "battery"}, integration="mobile_app"),
        Etat("binary_sensor.detecteur_pile", "on", {"device_class": "battery"}),
    ]
    s = sources(etats, ["sensor.pot_3", "sensor.pot_4", "sensor.pot_5"])
    a = s["actives"]
    assert a["binary_sensor.porte"] == {"c": "on", "g": "Orange", "t": "Porte", "s": "etiquette"}
    assert a["binary_sensor.fumee"]["g"] == "Rouge" and a["alarm_control_panel.maison"]["g"] == "Rouge"
    assert a["lock.entree"]["s"] == a["cover.garage"]["s"] == "etiquette"
    assert a["update.etiquetee"]["s"] == "maj"  # une mise à jour garde sa source
    assert a["sensor.pot_1"] == {"c": "bas", "g": "Orange", "t": "Pot 1 15 %", "s": "pile"}
    # Suivie, une pile le reste jusqu'au seuil + 10 % ; pas une nouvelle à 25 %.
    assert "sensor.pot_3" in a and "sensor.pot_2" not in a and "sensor.pot_4" not in a
    assert a["binary_sensor.detecteur_pile"]["s"] == "pile"
    assert set(a) == {"binary_sensor.porte", "binary_sensor.fumee", "lock.entree", "alarm_control_panel.maison",
                      "cover.garage", "update.etiquetee", "sensor.pot_1", "sensor.pot_3",
                      "binary_sensor.detecteur_pile", "ha:indispo"}
    # Une pile indisponible : dans le doute ; une rechargée (31 %) : fin.
    assert s["incertains"] == ["sensor.pot_5"] and s["absents"] == []


def test_une_pile_revient_apres_une_recharge():
    pile = {"sensor.pot": {"c": "bas", "g": "Orange", "t": "Pot 15 %", "s": "pile"}}
    c = Capteur()
    c(pile)
    c.tap("sensor.pot#1", pile)
    c()                       # rechargée
    c(avance=300)             # vraie fin
    assert "sensor.pot" not in c.m["suivi"]
    assert c(pile) == ["sensor.pot#1"]


# ─── Historique (lot 4 : popup « Alertes » de la tablette, tableau de bord) ───────

TAB5 = RACINE / "Tab5"


def historique(memoire, abonnements=None, payload=True):
    nom = "tab5_alertes_historique_payload" if payload else "tab5_alertes_historique_liste"
    args = "m, a" if payload else "m, a, 20"
    gabarit = _env().from_string(f"{{% from 'tab5_alertes.jinja' import {nom} %}}{{{{ {nom}({args}) }}}}")
    rendu = gabarit.render(m=memoire, a=abonnements or TOUT)
    return rendu if payload else json.loads(rendu)


def _entree(i, s=None, g="Orange", t="Porte", a=T0, lue=0, f=0):
    e = {"i": i, "r": 1, "t": t, "g": g, "a": a, "l": lue, "f": f}
    if s is not None:
        e["s"] = s
    return e


def test_une_nouvelle_entree_garde_sa_source():
    c = Capteur()
    c({"update.x": maj("1.0"), "binary_sensor.f": probleme()})
    assert {e["i"]: e["s"] for e in c.m["historique"]} == {"update.x": "maj", "binary_sensor.f": "probleme"}


def test_payload_de_l_historique():
    """« apparue|lue|terminée|gravité|libellé » séparés par « ; », 20 au plus, la plus
    récente d'abord : ce que lit alertes_historique_recu() (Tab5/tab5_alertes.cpp)."""
    h = [_entree(f"binary_sensor.p{i}", "etiquette", a=T0 - i, lue=T0 if i % 2 else 0, f=T0 + 5 if i % 3 == 0 else 0)
         for i in range(25)]
    h[1]["t"] = "A|B;C"
    entrees = historique({"historique": h}).split(";")
    assert len(entrees) == 20
    champs = [e.split("|") for e in entrees]
    assert all(len(c) == 5 for c in champs)
    assert [int(c[0]) for c in champs] == [T0 - i for i in range(20)]
    assert champs[0][1:3] == ["0", str(T0 + 5)] and champs[1][1:3] == [str(T0), "0"]
    assert champs[1][4] == "A/B,C"   # ni « | » ni « ; » dans un libellé


def test_l_historique_suit_les_abonnements():
    h = [_entree("update.x", "maj", "Orange", "@maj:Paquet"),
         _entree("update.core", "maj", "Rouge", "@maj:Core"),
         _entree("meteo:vigilance", "vigilance", "Jaune", "@vigi:Jaune"),
         _entree("binary_sensor.fuite", "probleme", "Rouge", "Fuite"),
         _entree("binary_sensor.porte", "etiquette"),
         # Entrées d'avant le lot 4 : sans source, déduite de l'id quand c'est possible.
         _entree("update.vieux"), _entree("meteo:vigilance", g="Jaune"), _entree("binary_sensor.ancien")]
    tout = [e["i"] for e in historique({"historique": h}, payload=False)]
    assert tout == [e["i"] for e in h]
    choix = {"maj": "ha", "vigilance": "Orange", "probleme": False, "indispo": True, "etiquette": True}
    assert [e["i"] for e in historique({"historique": h}, choix, payload=False)] == \
        ["update.core", "binary_sensor.porte", "binary_sensor.ancien"]
    assert historique({}) == "" and historique({"historique": "abîmé"}) == ""


def test_l_historique_lu_dans_ha():
    h = [_entree("update.x", "maj", t="@maj:Paquet"), _entree("binary_sensor.porte", "etiquette")]
    etats = [Etat("sensor.tab5_alertes", "1", {"historique": h}),
             Etat("input_select.tab5_alertes_maj", "Aucune")]
    gabarit = _env(etats).from_string(
        "{% from 'tab5_alertes.jinja' import tab5_alertes_historique %}"
        "{{ tab5_alertes_historique('payload') }}#{{ tab5_alertes_historique('liste') }}")
    payload, liste = gabarit.render().split("#", 1)
    assert payload == f"{T0}|0|0|Orange|Porte"
    assert [e["i"] for e in json.loads(liste)] == ["binary_sensor.porte"]
    # Sans le capteur (package absent) : rien, pas d'erreur.
    assert _env().from_string("{% from 'tab5_alertes.jinja' import tab5_alertes_historique %}"
                              "{{ tab5_alertes_historique('payload') }}").render() == ""


def test_l_historique_part_a_la_demande_de_la_tablette():
    """La tablette le demande à l'ouverture du popup (esphome.tab5_alertes_historique) ; HA
    le repousse quand il change pendant que « Écran courant » vaut « Alertes ». Un firmware
    d'avant n'émet pas l'événement et n'affiche jamais « Alertes » : l'action absente n'est
    jamais appelée (« Action not found » arrête un script, continue_on_error n'y peut rien)."""
    push = _charger("packages", "tab5_push.yaml")
    script = json.dumps(push["script"]["tab5_push_alertes_historique"]["sequence"], ensure_ascii=False)
    assert "esphome.tab5_ha_hmi_tab5_maj_alertes_historique" in script
    assert "tab5_alertes_historique('payload')" in script
    auto = next(a for a in push["automation"] if a["id"] == "tab5_ha_hmi_alertes_historique_push")
    assert {"entity_id": "sensor.tab5_alertes", "attribute": "historique"}.items() <= auto["trigger"][0].items()
    assert "is_state', 'Alertes')" in json.dumps(auto["condition"], ensure_ascii=False)
    # « Alertes » : le nom du popup dans le registre (« Écran courant ») et l'option du
    # select « Aller à l'écran ».
    assert '"Alertes",          ModalRegistry::POPUP' in (TAB5 / "tab5-scripts.yaml").read_text(encoding="utf-8")
    assert '      - "Alertes"\n' in (TAB5 / "tab5-ha-controls.yaml").read_text(encoding="utf-8")
    evenements = json.dumps(_charger("packages", "tab5_evenements.yaml"), ensure_ascii=False)
    assert "esphome.tab5_alertes_historique" in evenements and "script.tab5_push_alertes_historique" in evenements
    firmware = (TAB5 / "tab5-alertes.yaml").read_text(encoding="utf-8")
    assert "event: esphome.tab5_alertes_historique" in firmware
    assert 'alert_id: "*"' in firmware   # « Tout marquer comme lu »


def _entrees_valides(payload):
    entrees = payload.split(";")
    assert 0 < len(entrees) <= 20
    for e in entrees:
        a, lue, f, g, t = e.split("|")
        assert int(a) > 0 and int(lue) >= 0 and int(f) >= 0 and g in {"Rouge", "Orange", "Jaune"} and t
    return entrees


def test_les_donnees_du_rendu_et_du_fuzz_suivent_le_format():
    import sys
    sys.path.insert(0, str(RACINE / "tools" / "rendu"))
    import ecrans
    rendu = _entrees_valides(ecrans.HISTORIQUE_ALERTES)
    assert [int(e.split("|")[0]) for e in rendu] == sorted((int(e.split("|")[0]) for e in rendu), reverse=True)
    graine = re.search(r'"tab5_maj_alertes_historique": \{"payload": "([^"]+)"',
                       (RACINE / "tools" / "sanitizers" / "fuzz_services.py").read_text(encoding="utf-8"))
    assert graine and _entrees_valides(graine.group(1))
