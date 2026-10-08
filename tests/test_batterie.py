# -*- coding: utf-8 -*-
"""Batterie et chargeur de la tablette (08/10/2026, demande d'Axel).

Le chargeur était allumé à chaque démarrage depuis la 3.6.0 : sans batterie, il
chargeait dans le vide et faisait un léger souffle continu. Les règles sont dans
Tab5/socle/tab5_batterie.h (testées sur PC par tools/test_alarm_clock.cpp) ; rien ne compile
le câblage YAML hors tablette : ce fichier le relit.

- CHG_EN n'est commandé que par l'interval de 1 s (chargeur_pas), qui demande aussi la
  lecture de fin de sonde à l'INA226 ;
- le select « Tab5 Limite de charge » suit l'ordre de LimiteCharge (la tablette garde
  l'INDEX), défaut « 100 % » ;
- chaque lecture de la tension passe par batterie_tension_ui ; le courant lu pendant une
  sonde (chargeur coupé, tablette peut-être branchée) n'est pas compté ;
- « Tab5 Consommation » en W, et l'événement esphome.tab5_batterie_faible (niveau, seuil)
  quand le niveau publié franchit un seuil."""
import pathlib
import re

import yaml
from tests.commun import source, sources

REPO = pathlib.Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
DIAG = "tab5-sensors-diagnostics.yaml"


class _Chargeur(yaml.SafeLoader):
    pass


# Balises ESPHome (!lambda, !extend, !include…) : leur valeur brute suffit ici.
_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: chargeur.construct_scalar(noeud)
                                if isinstance(noeud, yaml.ScalarNode) else None)


def _yaml(nom):
    return yaml.load(source(nom).read_text(encoding="utf-8"), Loader=_Chargeur)


def _texte(obj):
    return yaml.safe_dump(obj, allow_unicode=True)


def _entete():
    return (TAB5 / "socle" / "tab5_batterie.h").read_text(encoding="utf-8")


def _sans_commentaires(code):
    return re.sub(r"//[^\n]*", "", code)


def test_select_limite_de_charge_dans_l_ordre_de_l_enum():
    s = next(s for s in _yaml(DIAG)["select"] if s.get("id") == "tab5_limite_charge")
    assert s["name"] == "Tab5 Limite de charge"
    assert s["options"] == ["100 %", "80 %"]
    assert s["initial_option"] == "100 %", "défaut : la batterie se charge pleinement"
    assert s["restore_value"] is True and s["optimistic"] is True
    assert s["entity_category"] == "config"
    enum = re.search(r"enum class LimiteCharge[^{]*\{([^}]*)\}", _entete())
    assert enum, "LimiteCharge introuvable dans tab5_batterie.h"
    valeurs = {nom: int(v) for nom, v in re.findall(r"(\w+)\s*=\s*(\d+)", enum.group(1))}
    assert valeurs == {"COMPLETE": 0, "QUATRE_VINGTS": 1}, valeurs
    arret = re.search(r"kChargeArretPct\s*=\s*([\d.]+)f", _entete())
    assert arret and float(arret.group(1)) == 80.0, "l'option « 80 % » et l'arrêt de la charge"


def test_chg_en_commande_par_le_seul_interval_du_chargeur():
    diag = _yaml(DIAG)
    chg = next(s for s in diag["switch"] if s.get("id") == "charge_enable")
    assert chg["internal"] is True
    assert chg["restore_mode"] == "ALWAYS_ON", "allumé 30 s au démarrage : réveil d'une batterie"
    assert chg["pin"]["number"] == 7
    intervals = [i for i in diag["interval"] if "chargeur_pas(" in _texte(i)]
    assert len(intervals) == 1 and intervals[0]["interval"] == "1s"
    code = _sans_commentaires(intervals[0]["then"][0]["lambda"])
    for motif in ("id(tab5_limite_charge).active_index()", "id(tab5_batterie_montee).state",
                  "id(charge_enable).turn_on()", "id(charge_enable).turn_off()", "id(ina226_batterie).update()"):
        assert motif in code, motif
    # Personne d'autre n'allume ni ne coupe le chargeur (le souffle revenait avec lui).
    for chemin in sources("*.yaml") + sorted((TAB5 / "ui_components").glob("*.yaml")):
        texte = chemin.read_text(encoding="utf-8")
        appels = len(re.findall(r"id\(charge_enable\)\.turn_(?:on|off)\(\)", texte))
        appels += len(re.findall(r"switch\.turn_(?:on|off):\s*charge_enable\b", texte))
        assert appels == (2 if chemin.name == DIAG else 0), f"{chemin.name} : {appels} commande(s) de charge_enable"


def test_lectures_de_l_ina226():
    capteurs = _yaml(DIAG)["sensor"]
    ina = next(c for c in capteurs if c.get("platform") == "ina226")
    assert ina["id"] == "ina226_batterie" and ina["update_interval"] == "60s"
    tension = _texte(ina["bus_voltage"]["on_raw_value"])
    assert "batterie_tension_ui(x, millis())" in tension
    courant = _sans_commentaires(ina["current"]["on_raw_value"][0]["lambda"])
    sonde = courant.index("if (chargeur_sonde_en_cours(millis())) return;")
    assert sonde < courant.index("economie_courant("), "courant d'une sonde : ignoré avant le mode économie"
    assert "id(batterie_consommation_w).publish_state(batterie_consommation(x, economie_sur_batterie()));" in courant
    niveau = next(c for c in capteurs if c.get("id") == "batterie_niveau")
    assert _texte(niveau["filters"]).count("batterie_niveau_de(x)") == 1


def test_consommation_et_alerte_de_batterie_faible():
    diag = _yaml(DIAG)
    conso = next(c for c in diag["sensor"] if c.get("id") == "batterie_consommation_w")
    assert conso["name"] == "Tab5 Consommation"
    assert conso["unit_of_measurement"] == "W" and conso["device_class"] == "power"
    assert conso["state_class"] == "measurement"
    assert not conso.get("disabled_by_default"), "lue par le tableau de bord de HA"
    niveau = next(c for c in diag["sensor"] if c.get("id") == "batterie_niveau")
    alerte = _texte(niveau["on_value"])
    assert "batterie_alerte(x, economie_sur_batterie())" in alerte and "id(tab5_batterie_faible).execute(" in alerte
    script = next(s for s in diag["script"] if s["id"] == "tab5_batterie_faible")
    assert script["parameters"] == {"niveau": "int", "seuil": "int"}
    evt = script["then"][0]["homeassistant.event"]
    assert evt["event"] == "esphome.tab5_batterie_faible"
    assert set(evt["data"]) == {"niveau", "seuil"}
    # Côté HA, la garde « batterie faible » lit ces deux champs (tests/test_contrat.py).
    sante = (REPO / "HomeAssistant_Config" / "packages" / "tab5_health.yaml").read_text(encoding="utf-8")
    assert "esphome.tab5_batterie_faible" in sante
