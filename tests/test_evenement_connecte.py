# -*- coding: utf-8 -*-
"""esphome.tab5_connected envoyé à Home Assistant lui-même (17/10/2026).

Nuit du 16/10/2026 sur la Waveshare 7B : aux 10 redémarrages, un outil aioesphomeapi
s'abonnait aux états avant HA ; la garde `api.connected: state_subscription_only` du
démarrage se levait alors, et l'événement partait sans client abonné aux actions
(« dropped; client has not subscribed to actions (yet) ») : pas de poussée complète,
écran sans météo ni agenda jusqu'aux poussées périodiques.

Rien ne compile ces règles hors tablette : ce fichier relit le YAML et le C++.
- le démarrage et la reconnexion de HA envoient l'événement par `ha_evenement_envoyer()`
  dans un `wait_until`, plus par `homeassistant.event` derrière `api.connected` ;
- la fonction ne vise que les clients « Home Assistant » et ne dit vrai que si l'un d'eux
  a pris l'événement (abonné aux actions).
"""
import re

from tests.commun import lire, source


def test_le_demarrage_attend_que_ha_ecoute():
    racine = lire("tab5-ha-hmi.yaml")
    debut = racine.index("ha_evenement_envoyer(\"esphome.tab5_connected\")")
    bloc = racine[racine.rindex("- wait_until:", 0, debut):racine.index("boot_complete", debut)]
    assert "timeout: 30s" in bloc
    assert "id(boot_evt_connecte) = ha_evenement_envoyer(\"esphome.tab5_connected\")" in bloc
    assert "return !id(boot_evt_connecte);" in bloc and "HA injoignable au boot" in bloc
    assert "event: esphome.tab5_connected" not in racine, "plus de homeassistant.event pour lui"
    assert "- id: boot_evt_connecte" in lire("Tab5", "paquets", "tab5-globals.yaml")


def test_la_reconnexion_aussi():
    api = lire("Tab5", "paquets", "tab5-api-logic.yaml")
    bloc = api[api.index("on_client_connected:"):api.index("Journal des démarrages")]
    assert "client_info.find(\"Home Assistant\")" in bloc and "delay: 2s" in bloc
    assert "lambda: 'return ha_evenement_envoyer(\"esphome.tab5_connected\");'" in bloc
    assert "homeassistant.event" not in bloc and "state_subscription_only" not in bloc


def test_la_fonction_ne_vise_que_home_assistant_abonne():
    cpp = source("tab5_journal.cpp").read_text(encoding="utf-8")
    corps = cpp[cpp.index("bool ha_evenement_envoyer(const char* evenement) {"):]
    corps = corps[:corps.index("\n}\n") if "\n}\n" in corps else corps.index("\r\n}\r\n")]
    assert "active_clients()" in corps
    assert re.search(r'strstr\(nom, "Home Assistant"\) == nullptr\) continue;', corps)
    assert "envoye |= client->send_homeassistant_action(evt);" in corps
    assert "evt.is_event = true;" in corps
    assert "bool ha_evenement_envoyer(const char* evenement);" in source("tab5_journal.h").read_text(encoding="utf-8")
