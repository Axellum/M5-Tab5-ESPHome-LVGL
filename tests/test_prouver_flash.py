# -*- coding: utf-8 -*-
"""Preuve d'un flash (tools/prouver_flash.py) : la comparaison avant / après, sans appareil.

Les instantanés sont construits par `instantane()` à partir de faux objets qui ont les
champs d'aioesphomeapi (DeviceInfo, SensorInfo, TextSensorInfo, SelectInfo, états avec
`missing_state`), puis comparés par `comparer()`. La lecture par l'API elle-même n'est
pas jouée ici : elle demande un vrai appareil."""
from types import SimpleNamespace

import pytest

import prouver_flash as pf

AVANT_T = 1_793_340_000.0          # heure de l'instantané d'avant
BOOT_ANCIEN = AVANT_T - 86_400     # démarré la veille
BOOT_NOUVEAU = AVANT_T + 60        # redémarré par le flash, une minute après


class SensorInfo(SimpleNamespace):
    pass


class TextSensorInfo(SimpleNamespace):
    pass


class SelectInfo(SimpleNamespace):
    pass


class BinarySensorInfo(SimpleNamespace):
    pass


def _appareil(version="3.9.0", compilation="2026-10-30 08:40:56 +0100", mac="aa:bb:cc:dd:ee:ff"):
    return SimpleNamespace(mac_address=mac, name="tab5-ha-hmi", friendly_name="Tab5",
                           project_version=version, compilation_time=compilation,
                           esphome_version="2026.9.1")


def _inst(t=AVANT_T, boot=BOOT_ANCIEN, raison="Reboot request from esphome.ota", theme="Slate",
          signal=-60.0, version="3.9.0", compilation="2026-10-30 08:40:56 +0100",
          mac="aa:bb:cc:dd:ee:ff", sans_etat=(), sans=()):
    entites = [
        SensorInfo(object_id="tab5_uptime", key=1, name="Tab5 Uptime", entity_category=0),
        TextSensorInfo(object_id="tab5_raison_du_redemarrage", key=2,
                       name="Tab5 Raison du redémarrage", entity_category=2),
        SelectInfo(object_id="theme", key=3, name="Thème", entity_category=1),
        SensorInfo(object_id="tab5_wifi_signal", key=4, name="Tab5 WiFi Signal", entity_category=2),
        BinarySensorInfo(object_id="ha_api_status", key=5, name="HA API Status", entity_category=2),
    ]
    etats = {1: SimpleNamespace(key=1, state=boot, missing_state=False),
             2: SimpleNamespace(key=2, state=raison, missing_state=False),
             3: SimpleNamespace(key=3, state=theme, missing_state=False),
             4: SimpleNamespace(key=4, state=signal, missing_state=False),
             5: SimpleNamespace(key=5, state=True, missing_state=False)}
    for k in sans_etat:
        etats[k] = SimpleNamespace(key=k, state=0.0, missing_state=True)
    entites = [e for e in entites if e.key not in sans]
    return pf.instantane(_appareil(version, compilation, mac), entites, etats, t, "192.0.2.10")


def _apres(**kw):
    kw.setdefault("t", AVANT_T + 120)
    kw.setdefault("boot", BOOT_NOUVEAU)
    kw.setdefault("version", "3.9.0-essai")
    kw.setdefault("compilation", "2026-10-31 10:00:00 +0100")
    kw.setdefault("signal", -55.0)        # un capteur qui bouge n'est pas un réglage
    return _inst(**kw)


def _par_nom(controles):
    return {c.nom: c for c in controles}


def test_instantane_lit_appareil_et_entites():
    i = _inst()
    assert i["appareil"]["mac"] == "AA:BB:CC:DD:EE:FF"
    assert i["appareil"]["version"] == "3.9.0"
    assert set(i["entites"]) == {"sensor.tab5_uptime", "text_sensor.tab5_raison_du_redemarrage",
                                 "select.theme", "sensor.tab5_wifi_signal", "binary_sensor.ha_api_status"}
    assert i["entites"]["select.theme"]["categorie"] == "config"
    assert pf.heure_demarrage(i) == BOOT_ANCIEN
    assert pf.raison(i) == "Reboot request from esphome.ota"


def test_flash_reussi_tout_passe():
    avant, apres = _inst(), _apres()
    fin = _apres(t=AVANT_T + 240)
    c = pf.comparer(avant, apres, fin, pf.Attentes(version="3.9.0-essai",
                                                    compilation="2026-10-31 10:00:00 +0100",
                                                    raison="esphome.ota"))
    assert pf.echecs(c) == [], pf.afficher(c)
    assert all(x.ok for x in c)


def test_pas_de_redemarrage_ni_nouveau_firmware():
    c = _par_nom(pf.comparer(_inst(), _inst(t=AVANT_T + 120), None, pf.Attentes()))
    assert c["firmware changé"].ok is False
    assert c["redémarrage"].ok is False and "AVANT" in c["redémarrage"].detail
    assert c["second redémarrage"].ok is None


def test_mauvais_appareil_et_mauvaise_version():
    c = _par_nom(pf.comparer(_inst(), _apres(mac="11:22:33:44:55:66"), None,
                             pf.Attentes(version="3.9.1")))
    assert c["appareil"].ok is False
    assert c["version"].ok is False
    c = _par_nom(pf.comparer(_inst(), _apres(), None, pf.Attentes(mac="11:22:33:44:55:66")))
    assert c["appareil"].ok is False


def test_compilation_formats_equivalents():
    assert pf.compilations_egales("2026-10-30 08:40:56 +0100", "Oct 30 2026, 08:40:56")
    assert not pf.compilations_egales("2026-10-30 08:40:56 +0100", "Oct 30 2026, 08:40:57")
    assert not pf.compilations_egales(None, "Oct 30 2026, 08:40:56")


def test_uptime_trop_long_et_heure_illisible():
    c = _par_nom(pf.comparer(_inst(), _apres(t=AVANT_T + 2000), None, pf.Attentes(uptime_max=600)))
    assert c["redémarrage"].ok is False
    c = _par_nom(pf.comparer(_inst(), _apres(sans_etat=(1,)), None, pf.Attentes()))
    assert c["redémarrage"].ok is False and "illisible" in c["redémarrage"].detail


@pytest.mark.parametrize("texte, plantage", [
    ("Reboot request from esphome.ota", False),
    ("First boot after install (other watchdogs)", False),
    ("Power button or RTC watchdog (rst 0x10)", False),
    ("Crash, other watchdogs (rst 0x10)", True),
    ("Exception/panic", True),
    ("Brownout reset", True),
    ("Task watchdog", True),
])
def test_raison_plantage(texte, plantage):
    assert pf.est_plantage(texte) is plantage
    c = _par_nom(pf.comparer(_inst(), _apres(raison=texte), None, pf.Attentes()))
    assert c["raison"].ok is (not plantage)


def test_raison_attendue():
    c = _par_nom(pf.comparer(_inst(), _apres(raison="Reboot request from api"), None,
                             pf.Attentes(raison="esphome\\.ota")))
    assert c["raison"].ok is False


def test_second_redemarrage():
    avant, apres = _inst(), _apres()
    c = _par_nom(pf.comparer(avant, apres, _apres(t=AVANT_T + 240, boot=AVANT_T + 200),
                             pf.Attentes(), coupures=1))
    assert c["second redémarrage"].ok is False
    c = _par_nom(pf.comparer(avant, apres, _apres(t=AVANT_T + 240), pf.Attentes(), coupures=1))
    assert c["second redémarrage"].ok is False   # coupure seule (Wi-Fi, reboot rapide)


def test_entite_sans_etat_ou_disparue():
    avant = _inst()
    c = _par_nom(pf.comparer(avant, _apres(sans_etat=(5,)), None, pf.Attentes()))
    assert c["entités"].ok is False and "binary_sensor.ha_api_status" in c["entités"].detail
    c = _par_nom(pf.comparer(avant, _apres(sans=(4,)), None, pf.Attentes()))
    assert c["entités"].ok is False and "sensor.tab5_wifi_signal" in c["entités"].detail
    c = _par_nom(pf.comparer(avant, _apres(sans_etat=(5,)), None, pf.Attentes(ignorer=["ha_api"])))
    assert c["entités"].ok is True


def test_entite_deja_sans_etat_avant_ne_compte_pas():
    c = _par_nom(pf.comparer(_inst(sans_etat=(5,)), _apres(sans_etat=(5,)), None, pf.Attentes()))
    assert c["entités"].ok is True


def test_reglage_change():
    c = _par_nom(pf.comparer(_inst(), _apres(theme="Relief doux"), None, pf.Attentes()))
    assert c["réglages"].ok is False and "select.theme" in c["réglages"].detail
    c = _par_nom(pf.comparer(_inst(), _apres(theme="Relief doux"), None, pf.Attentes(reglages_libres=True)))
    assert c["réglages"].ok is None


def test_fin_sert_de_reference_aux_entites():
    """Une entité vue après le flash puis perdue pendant l'attente est signalée."""
    avant, apres = _inst(), _apres()
    c = _par_nom(pf.comparer(avant, apres, _apres(t=AVANT_T + 240, sans_etat=(5,)), pf.Attentes()))
    assert c["entités"].ok is False


def test_meme_firmware_voulu():
    c = _par_nom(pf.comparer(_inst(), _inst(t=AVANT_T + 120, boot=BOOT_NOUVEAU), None,
                             pf.Attentes(meme_firmware=True)))
    assert c["firmware changé"].ok is None
    assert c["redémarrage"].ok is True


def test_domaine_de():
    assert pf.domaine_de(TextSensorInfo()) == "text_sensor"
    assert pf.domaine_de(BinarySensorInfo()) == "binary_sensor"
    assert pf.domaine_de(SelectInfo()) == "select"


def test_main_sans_instantane_lisible(tmp_path, capsys):
    assert pf.main(["--host", "192.0.2.10", "--cle", "x", "--apres", str(tmp_path / "absent.json")]) == 2
