# -*- coding: utf-8 -*-
"""Garde « la liaison API vient de (re)démarrer » de la poussée complète (tab5_push.yaml).

25/10/2026, deux écrans chez l'auteur : la 7B redémarre après une mise à jour et demande
sa poussée complète (esphome.tab5_connected). La garde lisait le capteur « HA API Status »
de LA tablette trouvée par tab5_api(), ici l'autre, connectée depuis 25 min : condition
fausse, rien d'envoyé, alertes et planning vides jusqu'au passage des 10 min. Elle lit
maintenant le capteur de l'appareil qui a émis l'événement ; ce fichier la rend comme HA.
"""
from __future__ import annotations

import datetime as dt
import re

import jinja2
import yaml
from tests.commun import REPO, ChargeurSansBalises

HA = REPO / "HomeAssistant_Config"
MAINTENANT = dt.datetime(2026, 10, 25, 7, 12, tzinfo=dt.timezone.utc)

# Deux tablettes : la 7B vient de se reconnecter, l'autre l'est depuis 25 min.
CAPTEURS = {
    "binary_sensor.tab5_ha_api_status": ("ecran_cuisine", MAINTENANT - dt.timedelta(minutes=25)),
    "binary_sensor.m5stack_tab5_home_assistant_hmi_ha_api_status": ("ecran_7b", MAINTENANT - dt.timedelta(seconds=20)),
}


class _Etat:
    def __init__(self, depuis):
        self.last_changed = depuis
        self.state = "on"


def _garde() -> str:
    paquet = yaml.load((HA / "packages" / "tab5_push.yaml").read_text(encoding="utf-8"), Loader=ChargeurSansBalises)
    auto = next(a for a in paquet["automation"] if a.get("id") == "tab5_ha_hmi_updater")
    return next(c["value_template"] for c in auto["conditions"] if "vient de (re)demarrer" in c.get("alias", ""))


def _rendre(trigger: dict, capteurs=CAPTEURS) -> str:
    env = jinja2.Environment(loader=jinja2.FileSystemLoader(str(HA / "custom_templates")))
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None  # test « match » de HA
    appareils = {e: d for e, (d, _) in capteurs.items()}
    env.globals.update(
        integration_entities=lambda _: list(capteurs),
        device_attr=lambda e, attr: "tab5-ha-hmi" if attr == "model" and e in capteurs else None,
        device_id=lambda e: appareils.get(e),
        is_state=lambda e, s: e in capteurs and s == "on",
        states={e: _Etat(depuis) for e, (_, depuis) in capteurs.items()},
        now=lambda: MAINTENANT,
    )
    return env.from_string(_garde()).render(trigger=trigger).strip()


def _connexion(appareil):
    return {"id": "tab5_connected", "platform": "event",
            "event": {"event_type": "esphome.tab5_connected", "data": {"device_id": appareil}}}


def test_la_tablette_qui_vient_de_se_reconnecter_passe():
    assert _rendre(_connexion("ecran_7b")) == "True"


def test_un_evenement_reemis_par_une_tablette_connectee_depuis_longtemps_est_refuse():
    assert _rendre(_connexion("ecran_cuisine")) == "False"


def test_liaison_pas_encore_on_passe():
    # L'événement arrive ~0,3 s avant que HA passe le capteur à « on » (25/10/2026).
    seul = {"binary_sensor.tab5_ha_api_status": CAPTEURS["binary_sensor.tab5_ha_api_status"]}
    assert _rendre(_connexion("ecran_7b"), seul) == "True"


def test_les_autres_declencheurs_passent():
    assert _rendre({"id": "demarrage_ha", "platform": "homeassistant"}) == "True"
    assert _rendre({"id": None, "platform": "time_pattern"}) == "True"
