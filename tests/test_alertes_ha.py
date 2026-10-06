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
    def __init__(self, entity_id, state, attributes=None, depuis=None):
        self.entity_id = entity_id
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
    etats = States(etats)
    env.globals.update(
        states=etats, timedelta=dt.timedelta,
        now=lambda: dt.datetime.fromtimestamp(maintenant, dt.timezone.utc),
        state_attr=lambda e, a: etats[e].attributes.get(a) if etats[e] else None,
    )
    env.filters.update(to_json=lambda v: json.dumps(v, ensure_ascii=False), from_json=json.loads)
    return env


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


def test_le_snippet_de_l_ancienne_liste_n_a_pas_d_initial():
    texte = (HA / "snippets" / "tab5_alerts_dismissed_input_text.yaml").read_text(encoding="utf-8")
    assert "initial" not in yaml.load(texte, Loader=_Chargeur)["tab5_alerts_dismissed"]


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

def sources(etats, suivi_ids=(), anciennes=()):
    gabarit = _env(etats).from_string(
        "{% from 'tab5_alertes.jinja' import tab5_alertes_sources %}{{ tab5_alertes_sources(i, a) }}")
    return json.loads(gabarit.render(i=list(suivi_ids), a=list(anciennes)))


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
