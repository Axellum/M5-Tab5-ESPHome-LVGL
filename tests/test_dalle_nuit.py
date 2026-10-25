# -*- coding: utf-8 -*-
"""Écran éteint = dalle éteinte, et extinction de nuit (audit anti-casse du 16/10/2026).

Rien ne compile ces règles hors tablette : ce fichier relit le YAML et le Jinja.

- chaque carte et chaque révision de dalle du Tab5 dit ce que sa dalle reçoit à l'extinction
  (`carte_dalle_extinction` : 0 voile seul, 1 + display off, 2 + sleep in) et si elle s'éteint
  pendant une OTA (`carte_dalle_noire_ota`) ; la racine les passe au C++ ; les trois dalles du
  Tab5 restent au voile seul tant qu'aucune n'a été essayée, la 7B passe à 2 ;
- la lumière dessine le voile et envoie les commandes AVANT de figer LVGL, et chaque `ota:`
  (esphome des deux cartes, http_request des firmwares publiés) passe par le script de la
  carte ;
- « Tab5 · night screen off » propose 00:00-06:00 sans rien changer pour qui a choisi un
  capteur de présence, et le miroir « Tab5 · night screen off now » dit « dans la plage » comme un
  calcul refait ici en Python (plage à cheval sur minuit comprise).
"""
import datetime as dt
import re

import jinja2
import pytest
import yaml
from tests.commun import ChargeurBalisesBrutes as _Chargeur, HA, lire, source

REVISIONS = ("st7123", "st7121", "ili9881c")


def _yaml(nom):
    return yaml.load(source(nom).read_text(encoding="utf-8"), Loader=_Chargeur) or {}


# ─── Firmware : la dalle ─────────────────────────────────────────────────────

@pytest.mark.parametrize("fichier, attendu", [(f"ecran-{r}.yaml", ("0", "0")) for r in REVISIONS]
                         + [("carte-waveshare-7b.yaml", ("2", "1"))])
def test_ce_que_recoit_chaque_dalle(fichier, attendu):
    sub = _yaml(fichier)["substitutions"]
    assert (sub["carte_dalle_extinction"], sub["carte_dalle_noire_ota"]) == attendu


def test_la_racine_passe_les_modes_au_cpp():
    racine = lire("tab5-ha-hmi.yaml")
    assert "- -DTAB5_DALLE_EXTINCTION=${carte_dalle_extinction}" in racine
    assert "- -DTAB5_DALLE_NOIRE_OTA=${carte_dalle_noire_ota}" in racine
    for carte in ("tab5-hardware.yaml", "carte-waveshare-7b.yaml"):
        assert "Tab5/ecran/tab5_dalle.cpp" in _yaml(carte)["esphome"]["includes"], carte
    cpp = source("tab5_dalle.cpp").read_text(encoding="utf-8")
    for cmd in ("0x10", "0x11", "0x28", "0x29"):
        assert cmd in cpp, cmd
    assert re.search(r"kDelaiVeilleMs\s*=\s*120;", cpp), "120 ms entre deux changements de veille (norme DCS)"


@pytest.mark.parametrize("fichier", ["tab5-hardware.yaml", "carte-waveshare-7b.yaml"])
def test_voile_avant_la_pause_et_rallumage_par_le_script(fichier):
    lumiere = next(l for l in _yaml(fichier)["light"] if l["id"] == "backlight")
    eteindre = [yaml.safe_dump(a, allow_unicode=True) for a in lumiere["on_turn_off"]]
    i_dalle = next(i for i, a in enumerate(eteindre) if "dalle_eteindre(id(tab5_display))" in a)
    i_pause = next(i for i, a in enumerate(eteindre) if "lvgl.pause" in a)
    assert i_dalle < i_pause, "le voile se dessine avant que LVGL se fige"
    assert lumiere["on_turn_on"][0] == {"script.execute": "tab5_dalle_rallumer"}
    assert lumiere["initial_state"] == {"brightness": "75%"}, "premier démarrage à 75 %"
    script = next(s for s in _yaml(fichier)["script"] if s["id"] == "tab5_dalle_rallumer")
    texte = yaml.safe_dump(script, allow_unicode=True)
    assert script["mode"] == "restart"
    assert texte.index("dalle_reveiller") < texte.index("delay: 120ms") < texte.index("dalle_rallumer(id")
    assert texte.index("remote_values.is_on()") < texte.index("lvgl.resume"), "rien si rééteinte entre-temps"


@pytest.mark.parametrize("fichier", ["tab5-hardware.yaml", "carte-waveshare-7b.yaml", "publication-commune.yaml"])
def test_chaque_ota_passe_par_la_dalle(fichier):
    (ota,) = _yaml(fichier)["ota"]
    assert {"script.execute": {"id": "tab5_dalle_ota", "debut": True}} in ota["on_begin"]
    for fin in ("on_abort", "on_error"):
        assert {"script.execute": {"id": "tab5_dalle_ota", "debut": False}} in ota[fin], fin


def test_la_7b_tourne_a_400_mhz():
    """P4 v3 : ESP-IDF 5.5 n'offre 360 MHz qu'avant la v3, l'option était ignorée."""
    esp32 = _yaml("carte-waveshare-7b.yaml")["esp32"]
    assert esp32["cpu_frequency"] == "400MHz"
    assert not any(k.startswith("CONFIG_ESP_DEFAULT_CPU_FREQ") for k in esp32["framework"]["sdkconfig_options"])


# ─── Home Assistant : la nuit ────────────────────────────────────────────────

PLAGE = re.compile(r"^([01][0-9]|2[0-3]):[0-5][0-9]-([01][0-9]|2[0-3]):[0-5][0-9]$")


def _reglages():
    return yaml.load((HA / "packages" / "tab5_reglages.yaml").read_text(encoding="utf-8"), Loader=_Chargeur)


def _modele(domaine, nom):
    for bloc in _reglages()["template"]:
        for e in bloc.get(domaine) or []:
            if e["name"] == nom:
                return e
    raise AssertionError(nom)


def _rendre(texte, etats, maintenant=None):
    env = jinja2.Environment()
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None
    env.globals["states"] = lambda e: etats.get(e, "unknown")
    env.globals["now"] = lambda: maintenant
    return env.from_string(texte).render().strip()


@pytest.mark.parametrize("memoire, presence, attendu", [
    ("unknown", "unknown", "00:00-06:00"),           # jamais réglée : proposée
    ("", "Not set", "00:00-06:00"),
    ("unknown", "binary_sensor.salon", "Not set"),   # capteur de présence choisi : comme avant
    ("Not set", "unknown", "Not set"),               # choix gardé
    ("Aucune", "unknown", "Not set"),                # choix d'avant le 24/10/2026, gardé
    ("none", "unknown", "Not set"),                  # « none » du 24/10/2026, gardé
    ("23:00-07:00", "binary_sensor.salon", "23:00-07:00"),
    ("21:30-05:45", "unknown", "21:30-05:45"),       # plage écrite à la main
    ("25:00-06:00", "unknown", "00:00-06:00"),       # illisible : défaut
])
def test_liste_extinction_de_nuit(memoire, presence, attendu):
    liste = _modele("select", "Tab5 · night screen off")
    assert liste["default_entity_id"] == "select.tab5_extinction_de_nuit"
    etats = {"input_text.tab5_choix_extinction_nuit": memoire, "input_text.tab5_choix_presence": presence}
    assert _rendre(liste["state"], etats) == attendu
    options = yaml.safe_load(_rendre(liste["options"], etats))
    assert attendu in options and options[-1] == "Not set"
    assert all(o == "Not set" or PLAGE.match(o) for o in options)


def _dans_la_plage(plage, h):
    """Calcul indépendant : en minutes, plage à cheval sur minuit comprise, début = fin : jamais."""
    if not PLAGE.match(plage):
        return None
    d, f = ((int(x[:2]) * 60 + int(x[3:])) for x in plage.split("-"))
    m = h.hour * 60 + h.minute
    return d <= m < f if d < f else (m >= d or m < f) if d > f else False


@pytest.mark.parametrize("plage", ["00:00-06:00", "23:00-07:00", "22:00-06:00", "06:00-06:00", "12:15-13:45", "Not set"])
def test_miroir_nuit_de_l_ecran(plage):
    miroir = _modele("binary_sensor", "Tab5 · night screen off now")
    etats = {"select.tab5_extinction_de_nuit": plage}
    for minutes in range(0, 24 * 60, 5):
        h = dt.datetime(2026, 10, 16, minutes // 60, minutes % 60)
        attendu = _dans_la_plage(plage, h)
        disponible = _rendre(miroir["availability"], etats, h) == "True"
        assert disponible == (attendu is not None), (plage, h)
        if disponible:
            assert (_rendre(miroir["state"], etats, h) == "True") == attendu, (plage, h.time())


def test_automatisation_tient_l_interrupteur():
    push = yaml.load((HA / "packages" / "tab5_push.yaml").read_text(encoding="utf-8"), Loader=_Chargeur)
    auto = next(a for a in push["automation"] if a["id"] == "tab5_extinction_nuit")
    # Plage de chaque écran (ADR-0072) : la liste des écrans dans leur plage, tout changement.
    miroir = next(t for t in auto["triggers"] if t.get("entity_id") == "sensor.tab5_nuits_ecrans")
    assert miroir.get("attribute") == "nuit" and "to" not in miroir
    assert any(t.get("event_type") == "esphome.tab5_connected" for t in auto["triggers"])
    # Pas de déclencheur sur l'attribut de la tablette : il part pendant l'ajout, liaison pas prête.
    assert not any(t.get("entity_id") == "sensor.tab5_tablette" for t in auto["triggers"])
    attente, action = _boucle(auto)
    assert "nuit_ecran" in attente
    assert action["action"] == "switch.turn_{{ 'on' if repeat.item in (state_attr('sensor.tab5_nuits_ecrans', 'nuit') or []) else 'off' }}"
    assert action["target"] == {"entity_id": "{% from 'tab5_tablette.jinja' import tab5_entite %}{{ tab5_entite(repeat.item, 'nuit_ecran') }}"}
    assert action["continue_on_error"] is True
    macro = (HA / "custom_templates" / "tab5_tablette.jinja").read_text(encoding="utf-8")
    # Motif de tab5_entite() ; « (_[0-9]+)? » y est ajouté pour le « _2 » de HA (tests/test_entites_suffixees.py).
    assert "'nuit_ecran': 'switch[.].*_(nuit_ecran_eteint|night_screen_off)'" in macro, "fin de l'entity_id de « Tab5 Nuit (écran éteint) »"


def _boucle(auto):
    """La boucle sur les écrans (ADR-0071) : chaque écran attend sa propre liaison et son
    interrupteur (tab5_entite(repeat.item, …)), puis l'appel, dans un `if` : un écran pas
    prêt ne bloque pas les suivants."""
    (boucle,) = auto["actions"]
    assert "tab5_cibles(" in boucle["repeat"]["for_each"]
    assert "tab5_connected" in boucle["repeat"]["for_each"], "sur une connexion : l'écran qui se connecte seul"
    attente, si = boucle["repeat"]["sequence"]
    assert "tab5_entite(repeat.item, 'api')" in attente["wait_template"]
    assert attente["continue_on_timeout"] is True
    assert si["if"][0]["value_template"] == attente["wait_template"]
    (action,) = si["then"]
    return attente["wait_template"], action


@pytest.mark.parametrize("ident, attribut", [("tab5_theme_jour_nuit", "theme_nuit"), ("tab5_extinction_nuit", "nuit_ecran")])
def test_aucun_appel_pendant_l_ajout_de_la_tablette(ident, attribut):
    """Les deux automatisations qui tiennent un interrupteur de la tablette (17/10/2026, test
    « Installation dans un HA neuf », HA 2026.9.4) : un déclencheur sur l'attribut de
    sensor.tab5_tablette partait pendant l'ajout de l'appareil, liaison pas prête, et l'appel
    finissait en « Not connected ». Elles attendent la liaison et l'interrupteur, et l'appel
    garde continue_on_error."""
    push = yaml.load((HA / "packages" / "tab5_push.yaml").read_text(encoding="utf-8"), Loader=_Chargeur)
    auto = next(a for a in push["automation"] if a["id"] == ident)
    assert not any(t.get("entity_id") == "sensor.tab5_tablette" for t in auto["triggers"])
    assert any(t.get("event_type") == "esphome.tab5_connected" for t in auto["triggers"])
    attente, action = _boucle(auto)
    assert f"tab5_entite(repeat.item, '{attribut}')" in attente
    assert action["continue_on_error"] is True
    assert action["target"] == {"entity_id": f"{{% from 'tab5_tablette.jinja' import tab5_entite %}}{{{{ tab5_entite(repeat.item, '{attribut}') }}}}"}
