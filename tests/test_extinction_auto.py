# -*- coding: utf-8 -*-
"""Extinction automatique de l'écran (06/10/2026, discussion #278).

Le select « Tab5 Extinction auto de l'écran » (Tab5/paquets/tab5-ha-controls.yaml) choisit un
délai sans toucher ; un interval l'applique en éteignant le rétroéclairage comme HA.
Rien ne compile ces règles hors tablette : ce fichier relit le YAML.

- le défaut est « Jamais » (rien ne change pour qui ne choisit rien) et la table des
  délais de l'interval suit l'ordre des options (la tablette garde l'INDEX choisi) ;
- l'écran ne s'éteint jamais pendant le réveil qui sonne, l'assistant vocal, un jeu
  ou une mise à jour, ni avant la fin du démarrage ;
- le délai repart du dernier allumage, posé par `on_turn_on` du light `backlight` :
  LVGL est en pause écran éteint, sans ça un rallumage par HA ou par la tape
  laisserait une inactivité ancienne et l'écran se rééteindrait aussitôt ;
- les globals lus existent, et chaque `ota:` pose et lève `ota_en_cours`."""
import pathlib
import re

import yaml
from tests.commun import ChargeurBalisesBrutes as _Chargeur, source

REPO = pathlib.Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
OPTIONS = ["Jamais", "1 min", "2 min", "5 min", "10 min", "30 min"]


def _yaml(nom):
    return yaml.load(source(nom).read_text(encoding="utf-8"), Loader=_Chargeur)


def _select():
    return next(s for s in _yaml("tab5-ha-controls.yaml")["select"] if s.get("id") == "tab5_extinction_auto")


def _interval():
    """L'entrée d'interval dont l'action éteint le rétroéclairage."""
    for entree in _yaml("tab5-ha-controls.yaml")["interval"]:
        texte = yaml.safe_dump(entree, allow_unicode=True)
        if "light.turn_off" in texte and "tab5_extinction_auto" in texte:
            return entree
    raise AssertionError("interval de l'extinction auto introuvable dans tab5-ha-controls.yaml")


def _condition():
    entree = _interval()
    si = entree["then"][0]["if"]
    return si["condition"]["lambda"], si["then"]


def _sans_commentaires(code):
    return re.sub(r"//[^\n]*", "", code)


def test_select_options_et_defaut_jamais():
    s = _select()
    assert s["name"] == "Tab5 Extinction auto de l'écran"
    assert s["options"] == OPTIONS
    assert s["initial_option"] == "Jamais", "défaut : comportement d'avant inchangé"
    assert s["restore_value"] is True and s["optimistic"] is True
    assert s["entity_category"] == "config"


def test_delais_dans_l_ordre_des_options():
    code, _ = _condition()
    m = re.search(r"DELAIS_MIN\[\]\s*=\s*\{([^}]*)\}", code)
    assert m, "table DELAIS_MIN introuvable"
    delais = [int(v) for v in m.group(1).split(",")]
    attendus = [0] + [int(o.split()[0]) for o in OPTIONS[1:]]
    assert delais == attendus, f"{delais} ≠ {attendus} (options {OPTIONS})"
    assert re.search(r"\*choix\s*==\s*0", code), "« Jamais » (index 0) n'éteint jamais"


def test_exclusions():
    code = _sans_commentaires(_condition()[0])
    for motif, raison in [
        (r"id\(alarm_ringing\)", "réveil qui sonne"),
        (r"id\(va\)->is_running\(\)", "pipeline vocal en cours"),
        (r"id\(va_stop_armed\)", "réponse vocale pas finie"),
        (r"GameRegistry::any_open\(\)", "jeu de l'Arcade ouvert"),
        (r"id\(ota_en_cours\)", "mise à jour du firmware"),
        (r"id\(boot_complete\)", "démarrage pas fini"),
        (r"remote_values\.is_on\(\)", "écran déjà éteint"),
    ]:
        assert re.search(motif, code), f"exclusion manquante : {raison} ({motif})"


def test_eteint_comme_ha_et_compte_depuis_l_allumage():
    code, actions = _condition()
    code = _sans_commentaires(code)
    assert {"light.turn_off": "backlight"} in actions
    assert "ui_idle_ms()" in code and "id(ecran_allume_ms)" in code
    assert re.search(r"std::min\(ui_idle_ms\(\),", code), "le plus récent des deux instants"
    lumiere = next(l for l in _yaml("tab5-hardware.yaml")["light"] if l.get("id") == "backlight")
    allumage = yaml.safe_dump(lumiere["on_turn_on"], allow_unicode=True)
    assert "id(ecran_allume_ms) = millis();" in allumage
    assert "lvgl.resume" in allumage


def test_globals_declares():
    ids = {g["id"]: g for g in _yaml("tab5-globals.yaml")["globals"]}
    for nom in ("ecran_allume_ms", "ota_en_cours", "alarm_ringing", "va_stop_armed", "boot_complete"):
        assert nom in ids, f"global {nom} absent de tab5-globals.yaml"
    assert ids["ota_en_cours"].get("restore_value") is False


def test_okay_nabu_rallume_l_ecran_seulement_au_demarrage_du_pipeline():
    """« Okay Nabu » rallume l'écran éteint si l'interrupteur est allumé (défaut), et
    seulement dans la branche START_PIPELINE : ni « Stop », ni mot de réveil ignoré."""
    s = next(s for s in _yaml("tab5-ha-controls.yaml")["switch"] if s.get("id") == "tab5_ecran_okay_nabu")
    assert s["name"] == "Tab5 Rallumer l'écran à Okay Nabu"
    assert s["restore_mode"] == "RESTORE_DEFAULT_ON" and s["entity_category"] == "config"
    script = next(x for x in _yaml("tab5-assist.yaml")["script"] if x["id"] == "tab5_wake_word_dispatch")
    branches = {b["if"]["condition"]["lambda"]: b["if"]["then"] for b in script["then"] if "if" in b}
    demarre = next(v for k, v in branches.items() if "WakeWord::START_PIPELINE" in k)
    allume = demarre[0]["if"]
    assert allume["condition"] == {"and": [{"switch.is_on": "tab5_ecran_okay_nabu"}, {"light.is_off": "backlight"}]}
    assert allume["then"] == [{"light.turn_on": "backlight"}]
    assert "voice_assistant.start" in demarre[1], "l'écran se rallume avant que l'écoute démarre"
    for cle, actions in branches.items():
        if "START_PIPELINE" not in cle:
            assert "tab5_ecran_okay_nabu" not in yaml.safe_dump(actions, allow_unicode=True), cle


def test_chaque_ota_pose_et_leve_ota_en_cours():
    for fichier in ("tab5-hardware.yaml", "publication-commune.yaml"):
        for ota in _yaml(fichier)["ota"]:
            for declencheur, valeur in (("on_begin", "true"), ("on_abort", "false"), ("on_error", "false")):
                actions = ota.get(declencheur) or []
                assert {"globals.set": {"id": "ota_en_cours", "value": valeur}} in actions, \
                    f"{fichier} ({ota['platform']}) : {declencheur} ne pose pas ota_en_cours à {valeur}"
