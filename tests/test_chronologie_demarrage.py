# -*- coding: utf-8 -*-
"""Chronologie du démarrage (10/10/2026, demande d'Axel).

Une tablette sur secteur n'a pas de journal série : « Tab5 Chronologie du démarrage »
publie vers Home Assistant l'instant de quelques étapes fixes (Tab5/socle/tab5_demarrage.h,
testé sur PC par tools/test_tab5_socle.cpp). Rien ne compile le câblage YAML hors
tablette : ce fichier le relit.

- chaque étape de EtapeDemarrage a son nom dans kEtapesNoms, dans le même ordre, et une
  seule marque dans le YAML ;
- observer sans retarder : les marques de setup() (expandeur, rétroéclairage, premier
  dessin) ne publient rien ; la séquence on_boot de tab5-ha-hmi.yaml n'est pas touchée
  ([AI-WARNING-CRITICAL], ADR-0005) ;
- le capteur est un text_sensor de diagnostic, publié par un seul script."""
import re

import yaml
from tests.commun import ChargeurBalisesBrutes, lire, source, sources

DIAG = "tab5-sensors-diagnostics.yaml"


def _yaml(nom):
    return yaml.load(source(nom).read_text(encoding="utf-8"), Loader=ChargeurBalisesBrutes)


def _texte(obj):
    return yaml.safe_dump(obj, allow_unicode=True, width=10**6)


def _entete():
    return source("tab5_demarrage.h").read_text(encoding="utf-8")


def _etapes():
    enum = re.search(r"enum class EtapeDemarrage[^{]*\{([^}]*)\}", _entete())
    assert enum, "EtapeDemarrage introuvable"
    noms = [n for n in re.findall(r"^\s*(\w+)", enum.group(1), re.M)]
    assert noms[-1] == "NOMBRE"
    return noms[:-1]


def test_noms_dans_l_ordre_de_l_enum():
    noms = re.search(r"kEtapesNoms\[kEtapesDemarrage\]\s*=\s*\{([^}]*)\}", _entete())
    assert noms, "kEtapesNoms introuvable"
    publies = re.findall(r'"([^"]+)"', noms.group(1))
    assert [p.upper() for p in publies] == [e.replace("_", "") for e in _etapes()], (publies, _etapes())


def test_chaque_etape_marquee_une_fois():
    textes = {chemin.name: chemin.read_text(encoding="utf-8") for chemin in sources("*.yaml")}
    for etape in _etapes():
        ou = [nom for nom, t in textes.items()
              for _ in re.findall(rf"demarrage_marquer\(EtapeDemarrage::{etape},", t)]
        assert len(ou) == 1, f"{etape} : {ou}"


def test_on_boot_pas_instrumente():
    racine = lire("tab5-ha-hmi.yaml")
    on_boot = racine.split("\n  on_boot:\n", 1)[1]
    assert "demarrage_" not in on_boot, "la séquence on_boot reste telle quelle (ADR-0005)"
    assert "delay(1000);" in on_boot


def test_marques_de_setup_sans_publication():
    diag = _yaml(DIAG)
    usb = next(s for s in diag["switch"] if s.get("id") == "usb_5v_power")
    assert usb["restore_mode"] == "ALWAYS_ON"
    assert _texte(usb["on_turn_on"]).count("demarrage_marquer(EtapeDemarrage::EXPANDEUR, millis())") == 1
    assert "publier" not in _texte(usb["on_turn_on"])
    debut = _texte(diag["lvgl"]["on_draw_start"])
    assert "demarrage_marquer(EtapeDemarrage::DESSIN, millis())" in debut and "publier" not in debut
    retro = next(o for o in _yaml("tab5-hardware.yaml")["output"] if o.get("id") == "backlight_plafonne")
    action = _texte(retro["write_action"])
    assert "if (state > 0.0f) demarrage_marquer(EtapeDemarrage::RETRO, millis());" in action
    assert "publier" not in action


def test_capteur_et_script():
    diag = _yaml(DIAG)
    capteur = next(t for t in diag["text_sensor"] if t.get("id") == "sys_chronologie")
    assert capteur["platform"] == "template"
    assert capteur["name"] == "Tab5 Chronologie du démarrage"
    assert capteur["entity_category"] == "diagnostic"
    assert capteur["update_interval"] == "never"
    script = next(s for s in diag["script"] if s["id"] == "tab5_chronologie_publier")
    code = script["then"][0]["lambda"]
    assert "char buf[kChronoTexteMax];" in code and "demarrage_texte(buf, sizeof(buf));" in code
    assert "id(sys_chronologie).publish_state(buf);" in code
    # Publié après setup() seulement : première image, Wi-Fi, API.
    fin = _texte(diag["lvgl"]["on_draw_end"])
    assert "if (demarrage_marquer(EtapeDemarrage::IMAGE, millis())) id(tab5_chronologie_publier).execute();" in fin
    assert "EtapeDemarrage::WIFI" in _texte(diag["wifi"]["on_connect"])
    status = next(b for b in diag["binary_sensor"] if b.get("id") == "status_ha")
    assert "if (x && demarrage_marquer(EtapeDemarrage::API, millis())) id(tab5_chronologie_publier).execute();" in \
        _texte(status["on_state"])
    n = sum(c.read_text(encoding="utf-8").count("id(sys_chronologie).publish_state(") for c in sources("*.yaml"))
    assert n == 1, "un seul endroit publie la chronologie"
