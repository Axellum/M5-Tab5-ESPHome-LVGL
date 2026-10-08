# -*- coding: utf-8 -*-
"""Pluie dans l'heure : poussée légère, pas la chaîne complète (audit du 07/10/2026,
PERF-3 et HA-16).

Un changement du code de pluie relançait toute la poussée complète (calendrier de 15
jours, prévisions, ~8 envois) pour les seules 9 barres, et la poussée légère repartait à
chaque mise à jour d'attribut du capteur. Désormais :
- la poussée complète n'écoute plus sensor.tab5_pluie_dans_l_heure, mais envoie encore
  les barres à chaque passage (10 min, connexion, démarrage de HA) ;
- la poussée légère pousse le code (section 1) PUIS les barres, et seulement quand l'état
  ou l'attribut `barres` change ;
- les barres n'existent qu'une fois, dans script.tab5_push_pluie, avec le même appel et
  le même payload qu'avant.
"""
from __future__ import annotations

from pathlib import Path

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment
from tests.commun import ChargeurSansBalises as _Chargeur

RACINE = Path(__file__).resolve().parents[1]
PUSH = RACINE / "HomeAssistant_Config" / "packages" / "tab5_push.yaml"
CAPTEUR = "sensor.tab5_pluie_dans_l_heure"
SERVICE = "esphome.tab5_ha_hmi_tab5_maj_pluie_1h_bulk"


def _paquet():
    return yaml.load(PUSH.read_text(encoding="utf-8"), Loader=_Chargeur)


def _parcourir(noeud):
    if isinstance(noeud, dict):
        yield noeud
        for v in noeud.values():
            yield from _parcourir(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _parcourir(v)


def _auto(paquet, id_):
    return next(a for a in paquet["automation"] if a.get("id") == id_)


def _actions(noeud):
    return [n.get("action") for n in _parcourir(noeud) if isinstance(n.get("action"), str)]


def test_la_poussee_complete_n_ecoute_plus_la_pluie():
    paquet = _paquet()
    complete = _auto(paquet, "tab5_ha_hmi_updater")
    assert not [t for t in complete["triggers"] if t.get("entity_id") == CAPTEUR]
    # Les barres partent encore à chaque passage, par le script partagé, hors de tout `if`.
    assert {"action": "script.tab5_push_pluie", "continue_on_error": True} in complete["actions"]


def test_un_seul_endroit_pour_les_barres():
    paquet = _paquet()
    appels = [n for n in _parcourir(paquet) if n.get("action") == SERVICE]
    assert len(appels) == 1
    script = paquet["script"]["tab5_push_pluie"]
    assert appels[0] in script["sequence"]
    # Même payload qu'avant le 07/10/2026 : l'attribut, sinon 9 barres vides.
    assert appels[0]["data"]["payload"].strip() == (
        "{{ state_attr('sensor.tab5_pluie_dans_l_heure', 'barres') or "
        "'0|0;1|0;2|0;3|0;4|0;5|0;6|0;7|0;8|0;' }}")
    # Tablette absente ou hors ligne : rien (garde des autres scripts de poussée).
    assert script["sequence"][0]["condition"] == "template"
    assert "tab5_connectee() == 'oui'" in script["sequence"][0]["value_template"]


def test_la_poussee_legere_pousse_le_code_puis_les_barres():
    legere = _auto(_paquet(), "tab5_ha_hmi_alerts_push")
    pluie = [t for t in legere["triggers"] if t.get("entity_id") == CAPTEUR]
    # Un seul déclencheur, état nu (pas de `to:` ni d'`attribute:`) : la condition trie.
    assert pluie == [{"trigger": "state", "entity_id": CAPTEUR, "id": "pluie"}]
    assert _actions(legere["actions"]) == ["script.tab5_push_alertes", "script.tab5_push_pluie"]
    assert legere["actions"][0]["data"]["vigilance_seule"] == "{{ trigger.id == 'pluie' }}"
    assert legere["actions"][1]["if"] == [{"condition": "trigger", "id": "pluie"}]


class _Etat:
    def __init__(self, etat, **attributs):
        self.state = etat
        self.attributes = attributs


def _passe(trigger):
    legere = _auto(_paquet(), "tab5_ha_hmi_alerts_push")
    modele = next(c for c in legere["conditions"] if c.get("alias", "").startswith("Pluie"))["value_template"]
    env = ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined)
    return env.from_string(modele).render(trigger=trigger).strip() == "True"


BARRES = "0|1;1|1;2|2;3|0;4|0;5|0;6|0;7|0;8|0;"


@pytest.mark.parametrize("avant, apres, attendu", [
    (_Etat("@1,10", barres=BARRES, source="Météo-France"), _Etat("@2,5", barres=BARRES, source="Météo-France"), True),
    (_Etat("@1,10", barres=BARRES), _Etat("@1,10", barres=BARRES.replace("2|2", "2|3")), True),
    # Rafraîchissement sans changement du code ni des barres (autre attribut) : rien.
    (_Etat("@1,10", barres=BARRES, maj="20:00"), _Etat("@1,10", barres=BARRES, maj="20:05"), False),
    # Capteur créé ou retiré : on pousse.
    (None, _Etat("@1,10", barres=BARRES), True),
    (_Etat("@1,10", barres=BARRES), None, True),
])
def test_condition_pluie(avant, apres, attendu):
    assert _passe({"id": "pluie", "from_state": avant, "to_state": apres}) is attendu


def test_condition_laisse_passer_les_autres_declencheurs():
    assert _passe({"id": None, "from_state": _Etat("Vert"), "to_state": _Etat("Vert")})
