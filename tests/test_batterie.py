# -*- coding: utf-8 -*-
"""Batterie et chargeur de la tablette (08/10/2026, demande d'Axel).

Le chargeur était allumé à chaque démarrage depuis la 3.6.0 : sans batterie, il
chargeait dans le vide et faisait un léger souffle continu. Les règles sont dans
Tab5/socle/tab5_batterie.h (testées sur PC par tools/test_alarm_clock.cpp) ; rien ne compile
le câblage YAML hors tablette : ce fichier le relit.

- CHG_EN n'est commandé que par l'interval de 1 s (chargeur_pas), qui demande aussi la
  lecture de fin de sonde à l'INA226 ; coupé au setup, il n'est pas allumé au démarrage
  si le démarrage précédent n'a vu aucune batterie (mémoire en NVS, 10/10/2026) ;
- le select « Tab5 Limite de charge » suit l'ordre de LimiteCharge (la tablette garde
  l'INDEX), défaut « 100 % » ;
- chaque lecture de la tension passe par batterie_tension_ui ; le courant lu pendant une
  sonde (chargeur coupé, tablette peut-être branchée) n'est pas compté ;
- « Tab5 Consommation » en W, et l'événement esphome.tab5_batterie_faible (niveau, seuil)
  quand le niveau publié franchit un seuil ;
- verrous du chargeur (16/10/2026, après la panne du 11/10) : sonde avant tout allumage au
  démarrage, verrou thermique gardé en NVS, verrou électrique nourri par le courant de
  l'INA226, « Tab5 Sécurité chargeur » et l'événement esphome.tab5_securite_chargeur."""
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
    # Coupé au setup (10/10/2026) : le premier pas de l'interval décide du réveil de 30 s
    # d'après la mémoire du démarrage précédent (chargeur_demarrer, tab5_batterie.h).
    assert chg["restore_mode"] == "ALWAYS_OFF", "pas de charge dans le vide pendant le démarrage"
    assert chg["pin"]["number"] == 7
    intervals = [i for i in diag["interval"] if "chargeur_pas(" in _texte(i)]
    assert len(intervals) == 1 and intervals[0]["interval"] == "1s"
    code = _sans_commentaires(intervals[0]["then"][0]["lambda"])
    for motif in ("id(tab5_limite_charge).active_index()", "id(tab5_batterie_montee).state",
                  "id(tab5_batterie_memoire),", "journal_demarrage_anormal(), millis())",
                  "id(charge_enable).turn_on()", "id(charge_enable).turn_off()", "id(ina226_batterie).update()",
                  "const uint8_t memoire = chargeur_memoire(id(tab5_batterie_memoire));",
                  "if (memoire != id(tab5_batterie_memoire)) id(tab5_batterie_memoire) = memoire;"):
        assert motif in code, motif
    # Personne d'autre n'allume ni ne coupe le chargeur (le souffle revenait avec lui).
    for chemin in sources("*.yaml") + sorted((TAB5 / "ui_components").glob("*.yaml")):
        texte = chemin.read_text(encoding="utf-8")
        appels = len(re.findall(r"id\(charge_enable\)\.turn_(?:on|off)\(\)", texte))
        appels += len(re.findall(r"switch\.turn_(?:on|off):\s*charge_enable\b", texte))
        assert appels == (2 if chemin.name == DIAG else 0), f"{chemin.name} : {appels} commande(s) de charge_enable"


def test_memoire_du_demarrage_precedent():
    """Chargeur éteint au démarrage si le démarrage précédent n'a vu aucune batterie
    (10/10/2026, demande d'Axel) : la mémoire est un global restauré de NVS, dont les
    valeurs suivent MemoireBatterie (0 inconnue = comportement d'avant)."""
    g = next(g for g in _yaml(DIAG)["globals"] if g["id"] == "tab5_batterie_memoire")
    assert g["type"] == "uint8_t" and g["restore_value"] is True and str(g["initial_value"]) == "0"
    enum = re.search(r"enum class MemoireBatterie[^{]*\{([^}]*)\}", _entete())
    assert enum, "MemoireBatterie introuvable dans tab5_batterie.h"
    valeurs = {nom: int(v) for nom, v in re.findall(r"(\w+)\s*=\s*(\d+)", enum.group(1))}
    assert valeurs == {"INCONNUE": 0, "AUCUNE": 1, "VUE": 2}, valeurs
    # Seul l'interval du chargeur écrit la mémoire.
    for chemin in sources("*.yaml"):
        n = len(re.findall(r"id\(tab5_batterie_memoire\)\s*=[^=]", chemin.read_text(encoding="utf-8")))
        assert n == (1 if chemin.name == DIAG else 0), (chemin.name, n)


def test_select_mode_de_charge_et_charge_rapide_par_le_seul_interval():
    """Mode de charge (09/10/2026, ADR-0045) : « Classique » (défaut, charge rapide
    arrêtée comme avant) ou « Rapide ». La charge rapide (nCHG_QC_EN, P5) ne s'allume
    qu'avec le chargeur, par l'interval qui commande CHG_EN : jamais sans batterie, ni
    pendant une sonde, ni en pause à 80 %."""
    diag = _yaml(DIAG)
    s = next(s for s in diag["select"] if s.get("id") == "tab5_mode_charge")
    assert s["name"] == "Tab5 Mode de charge"
    # En anglais, lues par HA (ADR-0070 ; « Classique », « Rapide » avant le 24/10/2026).
    assert s["options"] == ["Classic", "Fast"]
    assert s["initial_option"] == "Classic", "défaut : le comportement d'avant"
    assert s["restore_value"] is True and s["optimistic"] is True
    assert s["entity_category"] == "config"
    enum = re.search(r"enum class ModeCharge[^{]*\{([^}]*)\}", _entete())
    assert enum, "ModeCharge introuvable dans tab5_batterie.h"
    valeurs = {nom: int(v) for nom, v in re.findall(r"(\w+)\s*=\s*(\d+)", enum.group(1))}
    assert valeurs == {"CLASSIQUE": 0, "RAPIDE": 1}, valeurs
    qc = next(s for s in diag["switch"] if s.get("id") == "quick_charge")
    assert qc["internal"] is True and qc["restore_mode"] == "ALWAYS_OFF", "charge rapide arrêtée au démarrage"
    assert qc["pin"]["number"] == 5 and qc["pin"]["inverted"] is True
    intervals = [i for i in diag["interval"] if "chargeur_pas(" in _texte(i)]
    code = _sans_commentaires(intervals[0]["then"][0]["lambda"])
    assert "charge_rapide_voulue(id(tab5_mode_charge).active_index().value_or(0), a.allumer)" in code
    for chemin in sources("*.yaml") + sorted((TAB5 / "ui_components").glob("*.yaml")):
        texte = chemin.read_text(encoding="utf-8")
        appels = len(re.findall(r"id\(quick_charge\)\.turn_(?:on|off)\(\)", texte))
        appels += len(re.findall(r"switch\.turn_(?:on|off):\s*quick_charge\b", texte))
        assert appels == (2 if chemin.name == DIAG else 0), f"{chemin.name} : {appels} commande(s) de quick_charge"


def test_wifi_eco_select_et_script():
    """Wi-Fi éco (09/10/2026, ADR-0045) : « Jamais » par défaut (power_save_mode NONE,
    le comportement d'avant), jamais pendant un flux (voix, son, OTA)."""
    diag = _yaml(DIAG)
    s = next(s for s in diag["select"] if s.get("id") == "tab5_wifi_eco")
    assert s["name"] == "Tab5 Wi-Fi éco"
    assert s["options"] == ["Never", "On battery", "Always"], "l'ordre de ChoixEconomie"
    assert s["initial_option"] == "Never"
    assert s["restore_value"] is True and s["optimistic"] is True
    assert {"script.execute": "tab5_wifi_eco_appliquer"} in s["on_value"]
    assert diag["wifi"]["power_save_mode"] == "NONE", "au démarrage : le comportement d'avant"
    script = next(x for x in diag["script"] if x["id"] == "tab5_wifi_eco_appliquer")
    code = _sans_commentaires(script["then"][0]["lambda"])
    for motif in ("wifi_eco_voulu(id(tab5_wifi_eco).active_index().value_or(0)", "economie_sur_batterie()",
                  "id(va)->is_running()", "id(ota_en_cours)", "MEDIA_PLAYER_STATE_PLAYING"):
        assert motif in code, motif
    assert any(i.get("interval") == "1s" and {"script.execute": "tab5_wifi_eco_appliquer"} in i["then"]
               for i in diag["interval"])
    # Le seul appel à esp_wifi_set_ps du firmware : wifi_eco_appliquer (tab5_console.cpp).
    for chemin in sources("*.cpp") + sources("*.yaml"):
        n = _sans_commentaires(chemin.read_text(encoding="utf-8")).count("esp_wifi_set_ps(")
        assert n == (1 if chemin.name == "tab5_console.cpp" else 0), (chemin.name, n)


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


def test_seuil_presence_batterie_et_securite_thermique():
    """Seuil de présence batterie rehaussé à 5,5 V et sécurité thermique anti-surchauffe
    (11/10/2026, ADR-0061) :
    - 5,5 V évite les faux positifs sur tension résiduelle sans batterie (3,29 V observés) ;
    - seuil critique à 55 °C et réarmement à 44 °C ;
    - surveillance rapprochée de sys_core_temp à 10 s appelant chargeur_thermique_releve."""
    entete = _entete()
    seuil = re.search(r"kBatterieSeuilV\s*=\s*([\d.]+)f", entete)
    assert seuil and float(seuil.group(1)) == 5.5, "seuil de détection batterie 2S rehaussé à 5,5 V"
    temp_max = re.search(r"kChargeurTempMaxC\s*=\s*([\d.]+)f", entete)
    assert temp_max and float(temp_max.group(1)) == 55.0, "seuil coupure thermique à 55 °C"
    temp_rearme = re.search(r"kChargeurTempRearmeC\s*=\s*([\d.]+)f", entete)
    assert temp_rearme and float(temp_rearme.group(1)) == 44.0, "seuil réarmement thermique à 44 °C"
    pente_max = re.search(r"kChargeurPenteMaxC\s*=\s*([\d.]+)f", entete)
    assert pente_max and float(pente_max.group(1)) == 4.0, "pente maximale autorisée à 4 °C"

    diag = _yaml(DIAG)
    temp_sensor = next(s for s in diag["sensor"] if s.get("id") == "sys_core_temp")
    assert temp_sensor["update_interval"] == "10s", "température surveillée toutes les 10 s"
    code_temp = _sans_commentaires(_texte(temp_sensor["on_value"]))
    assert "chargeur_thermique_releve(x, millis())" in code_temp


def test_verrous_du_chargeur_cables():
    """Verrous du chargeur (16/10/2026, audit anti-casse, trous 1 à 4) : ce que le C++ décide
    (tools/test_alarm_clock.cpp) n'agit que si le YAML le nourrit et le publie."""
    entete = _entete()
    for nom, valeur in (("kChargeurTensionMaxV", 8.45), ("kChargeurCourantMinA", 0.020)):
        m = re.search(rf"{nom}\s*=\s*([\d.]+)f", entete)
        assert m and float(m.group(1)) == valeur, nom
    assert re.search(r"kChargeurCourantFaibleMs\s*=\s*30u \* 1000u", entete), "courant nul pendant 30 s"
    assert re.search(r"kVerrouElectriqueFenetreMs\s*=\s*60u \* 60u \* 1000u", entete), "deux fois dans l'heure"
    enum = re.search(r"enum class CauseSecurite[^{]*\{([^}]*)\}", entete)
    valeurs = {nom: int(v) for nom, v in re.findall(r"(\w+)\s*=\s*(\d+)", enum.group(1))}
    assert valeurs == {"AUCUNE": 0, "THERMIQUE": 1, "TENSION": 2, "COURANT": 3}, valeurs
    # Démarrage : une sonde d'abord, jamais de réveil avant (trou n° 1).
    demarrer = _sans_commentaires(re.search(r"inline void chargeur_demarrer\(.*?\n\}", entete, re.S).group(0))
    for motif in ("c.reveil = false;", "c.sonde = true;", "== MemoireBatterie::VUE && !anormal"):
        assert motif in demarrer, motif
    # Une température illisible ne lève plus le verrou thermique (trou n° 3).
    tick = _sans_commentaires(re.search(r"inline ActionChargeur chargeur_tick\(.*?\n\}", entete, re.S).group(0))
    assert "!std::isfinite(c.derniere_temp_cpu) ||" not in tick
    assert "std::isfinite(c.derniere_temp_cpu) && c.derniere_temp_cpu <= kChargeurTempRearmeC" in tick
    # Verrou thermique gardé en NVS, écrit tout de suite ; redémarrage anormal lu au journal.
    cpp = _sans_commentaires(source("tab5_batterie.cpp").read_text(encoding="utf-8"))
    assert "make_preference<uint8_t>(kPrefVerrouThermique)" in cpp
    assert cpp.count("global_preferences->sync()") == 2, "verrou thermique et mémoire de la batterie"
    journal = _sans_commentaires(source("tab5_journal.cpp").read_text(encoding="utf-8"))
    assert "bool journal_demarrage_anormal() {" in journal and "reset_anormal(s_raison, s_rapport_plantage)" in journal

    diag = _yaml(DIAG)
    # Le courant de l'INA226 nourrit le verrou électrique, même pendant une sonde (ignoré
    # par le C++ chargeur coupé), avant le retour anticipé du mode économie.
    ina = next(c for c in diag["sensor"] if c.get("platform") == "ina226")
    courant = _sans_commentaires(ina["current"]["on_raw_value"][0]["lambda"])
    assert courant.index("chargeur_courant(x, millis());") < courant.index("if (chargeur_sonde_en_cours(millis())) return;")
    # « Tab5 Sécurité chargeur » : visible par défaut, publiée par l'interval du chargeur.
    capteur = next(b for b in diag["binary_sensor"] if b.get("id") == "chargeur_securite")
    assert capteur["name"] == "Tab5 Sécurité chargeur" and capteur["device_class"] == "problem"
    assert not capteur.get("disabled_by_default") and not capteur.get("internal")
    intervals = [i for i in diag["interval"] if "chargeur_pas(" in _texte(i)]
    code = _sans_commentaires(intervals[0]["then"][0]["lambda"])
    for motif in ("const bool securite = chargeur_securite_active();",
                  "id(chargeur_securite).publish_state(securite);",
                  "const SignalSecurite coupure = chargeur_securite_signal();",
                  "if (coupure.cause != CauseSecurite::AUCUNE)",
                  "id(tab5_securite_chargeur).execute(cause_securite_texte(coupure.cause), coupure.mesure, coupure.verrouille);"):
        assert motif in code, motif
    # L'événement (ADR-0025) et sa garde côté HA (règle 8 : un événement, une branche).
    script = next(s for s in diag["script"] if s["id"] == "tab5_securite_chargeur")
    assert script["parameters"] == {"cause": "string", "mesure": "float", "verrouille": "bool"}
    evt = script["then"][0]["homeassistant.event"]
    assert evt["event"] == "esphome.tab5_securite_chargeur"
    assert set(evt["data"]) == {"cause", "mesure", "verrouille"}
    sante = yaml.load((REPO / "HomeAssistant_Config" / "packages" / "tab5_health.yaml").read_text(encoding="utf-8"),
                      Loader=_Chargeur)
    (garde,) = [a for a in sante["automation"] if a.get("id") == "tab5_health_securite_chargeur"]
    assert garde["triggers"] == [{"trigger": "event", "event_type": "esphome.tab5_securite_chargeur"}]
    assert garde["actions"][0]["action"] == "script.tab5_health_notify"
    for texte in ("thermique", "tension", "courant"):
        assert f"cause == '{texte}'" in garde["variables"]["raison"], texte
