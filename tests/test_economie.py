# -*- coding: utf-8 -*-
"""Mode économie d'énergie (06/10/2026, demande d'Axel).

Le select « Tab5 Économie d'énergie » (Tab5/paquets/tab5-economie.yaml) choisit Jamais / Sur
batterie / Toujours ; les règles sont dans Tab5/socle/tab5_economie.h (testées sur PC par
tools/test_alarm_clock.cpp). Rien ne compile le câblage YAML hors tablette : ce fichier
le relit.

- les options sont dans l'ordre de ChoixEconomie (la tablette garde l'INDEX choisi) et
  le défaut est « Sur batterie » : rien ne change sur secteur ;
- la lumière écrit dans la sortie plafonnée, qui seule pilote le PWM : le plafond
  s'applique à la sortie, HA garde la luminosité choisie ;
- le courant de l'INA226 décide « Sur batterie », la charge et le niveau suivent ;
- l'extinction auto range sa liste de raisons dans `ecran_veille_permise` AVANT de
  regarder son délai (le mode économie la lit même quand l'extinction est sur
  « Jamais ») ;
- le toucher et l'allumage relancent le script tout de suite ;
- le plancher est le minimum du curseur de luminosité des Réglages."""
import pathlib
import re

import yaml
from tests.commun import ChargeurBalisesBrutes as _Chargeur, source

REPO = pathlib.Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
OPTIONS = ["Jamais", "Sur batterie", "Toujours"]
SCRIPT = "tab5_economie_appliquer"


def _yaml(nom):
    return yaml.load(source(nom).read_text(encoding="utf-8"), Loader=_Chargeur)


def _texte(obj):
    return yaml.safe_dump(obj, allow_unicode=True)


def _entete():
    return (TAB5 / "socle" / "tab5_economie.h").read_text(encoding="utf-8")


def _sans_commentaires(code):
    return re.sub(r"//[^\n]*", "", code)


def test_select_options_dans_l_ordre_de_l_enum_et_defaut_sur_batterie():
    s = next(s for s in _yaml("tab5-economie.yaml")["select"] if s.get("id") == "tab5_economie")
    assert s["name"] == "Tab5 Économie d'énergie"
    assert s["options"] == OPTIONS
    assert s["initial_option"] == "Sur batterie", "défaut : rien ne change sur secteur"
    assert s["restore_value"] is True and s["optimistic"] is True
    assert s["entity_category"] == "config"
    assert {"script.execute": SCRIPT} in s["on_value"]
    enum = re.search(r"enum class ChoixEconomie[^{]*\{([^}]*)\}", _entete())
    assert enum, "ChoixEconomie introuvable dans tab5_economie.h"
    valeurs = {nom: int(v) for nom, v in re.findall(r"(\w+)\s*=\s*(\d+)", enum.group(1))}
    assert valeurs == {"JAMAIS": 0, "SUR_BATTERIE": 1, "TOUJOURS": 2}, valeurs


def test_select_animations_dans_l_ordre_de_l_enum_et_defaut_completes():
    """Animations (09/10/2026) : l'index choisi est un ChoixAnimations ; « Complètes » =
    le comportement d'avant ; le mode éco actif force « Aucune » dans economie_decider
    (un seul point de décision, testé par tools/test_alarm_clock.cpp)."""
    s = next(s for s in _yaml("tab5-economie.yaml")["select"] if s.get("id") == "tab5_animations")
    assert s["name"] == "Tab5 Animations"
    assert s["options"] == ["Complètes", "Essentielles", "Aucune"]
    assert s["initial_option"] == "Complètes", "défaut : le comportement d'avant"
    assert s["restore_value"] is True and s["optimistic"] is True
    assert s["entity_category"] == "config"
    assert {"script.execute": SCRIPT} in s["on_value"]
    enum = re.search(r"enum class ChoixAnimations[^{]*\{([^}]*)\}", _entete())
    assert enum, "ChoixAnimations introuvable dans tab5_economie.h"
    valeurs = {nom: int(v) for nom, v in re.findall(r"(\w+)\s*=\s*(\d+)", enum.group(1))}
    assert valeurs == {"COMPLETES": 0, "ESSENTIELLES": 1, "AUCUNE": 2}, valeurs
    # Un seul point de décision : seul le script règle le niveau des animations.
    for chemin in (TAB5 / "paquets").glob("*.yaml"):
        texte = _sans_commentaires(chemin.read_text(encoding="utf-8"))
        n = texte.count("animations_niveau(")
        assert n == (1 if chemin.name == "tab5-economie.yaml" else 0), (chemin.name, n)


def test_la_lumiere_passe_par_la_sortie_plafonnee():
    hw = _yaml("tab5-hardware.yaml")
    sorties = {o["id"]: o for o in hw["output"]}
    pwm, plafonne = sorties["backlight_pwm"], sorties["backlight_plafonne"]
    assert pwm["platform"] == "ledc" and pwm["pin"] == "GPIO22"
    assert plafonne["platform"] == "template" and plafonne["type"] == "float"
    assert "id(backlight_pwm).set_level(economie_sortie_retro(state));" in _texte(plafonne["write_action"])
    lumiere = next(l for l in hw["light"] if l.get("id") == "backlight")
    assert lumiere["output"] == "backlight_plafonne", "la lumière doit écrire dans la sortie plafonnée"
    assert {"script.execute": SCRIPT} in lumiere["on_turn_on"], "un écran rallumé reprend sa luminosité"
    # Seuls la sortie plafonnée et le script écrivent le PWM.
    for nom in ("tab5-economie.yaml", "tab5-hardware.yaml"):
        texte = source(nom).read_text(encoding="utf-8")
        for appel in re.findall(r"id\(backlight_pwm\)\.set_level\(([^;]*)\);", texte):
            assert appel in ("economie_sortie_retro(state)", "economie_sortie_actuelle()"), (nom, appel)


def test_le_toucher_rend_la_luminosite_tout_de_suite():
    toucher = _yaml("tab5-hardware.yaml")["touchscreen"][0]
    actions = toucher["on_touch"]
    assert actions[0] == {"lambda": "ui_mark_activity();"}, "l'activité d'abord : LVGL lit le toucher plus tard"
    assert {"script.execute": SCRIPT} in actions[1:]


def test_le_courant_decide_sur_batterie():
    capteurs = _yaml("tab5-sensors-diagnostics.yaml")["sensor"]
    ina = next(c for c in capteurs if c.get("platform") == "ina226")
    courant = ina["current"]
    assert courant["name"] == "Tab5 Courant batterie"
    assert courant["disabled_by_default"] is True, "sans batterie il ne veut rien dire"
    actions = _texte(courant["on_raw_value"])
    assert "economie_courant(x, batterie_presente(), id(batterie_en_charge).state);" in actions
    assert "id(tab5_sur_batterie).publish_state(economie_sur_batterie());" in actions
    assert SCRIPT in actions
    niveau = next(c for c in capteurs if c.get("id") == "batterie_niveau")
    assert "economie_niveau(x);" in _texte(niveau["on_value"]) and SCRIPT in _texte(niveau["on_value"])
    charge = next(b for b in _yaml("tab5-sensors-diagnostics.yaml")["binary_sensor"]
                  if b.get("id") == "batterie_en_charge")
    assert "economie_charge(x)" in _texte(charge["on_state"]) and SCRIPT in _texte(charge["on_state"])
    sur_batterie = next(b for b in _yaml("tab5-economie.yaml")["binary_sensor"] if b.get("id") == "tab5_sur_batterie")
    assert sur_batterie["name"] == "Tab5 Sur batterie"


def test_l_extinction_range_ses_raisons_avant_son_delai():
    for entree in _yaml("tab5-ha-controls.yaml")["interval"]:
        texte = _texte(entree)
        if "light.turn_off" in texte and "tab5_extinction_auto" in texte:
            code = _sans_commentaires(entree["then"][0]["if"]["condition"]["lambda"])
            break
    else:
        raise AssertionError("interval de l'extinction auto introuvable")
    pose = code.index("id(ecran_veille_permise) = permise;")
    assert pose < code.index("*choix == 0"), "le mode économie lit la liste même extinction sur « Jamais »"
    assert re.search(r"if \(!permise\) return false;", code)
    globals_ = {g["id"]: g for g in _yaml("tab5-globals.yaml")["globals"]}
    assert globals_["ecran_veille_permise"]["restore_value"] is False


def test_le_script_lit_la_liste_et_s_applique_chaque_seconde():
    eco = _yaml("tab5-economie.yaml")
    script = next(s for s in eco["script"] if s["id"] == SCRIPT)
    code = _sans_commentaires(script["then"][0]["lambda"])
    for motif in ("id(ecran_veille_permise)", "GameRegistry::any_open()", "economie_decider(in)",
                  "id(backlight).gamma_correct_lut(d.plafond)", "set_refresh_interval(d.periode_ms)",
                  "animations_niveau(d.animations)", "std::min(ui_idle_ms(), depuis_allumage)",
                  "id(tab5_animations).active_index()"):
        assert motif in code, motif
    assert any(i.get("interval") == "1s" and {"script.execute": SCRIPT} in i["then"] for i in eco["interval"])


def test_plancher_egal_au_minimum_du_curseur_des_reglages():
    popup = (TAB5 / "ui_components" / "reglages_popup.yaml").read_text(encoding="utf-8")
    bloc = popup.split("id: reglages_lum_slider", 1)[1]
    m_min = re.search(r"min_value:\s*(\d+)", bloc)
    m_plancher = re.search(r"kEcoPlancher\s*=\s*([\d.]+)f", _entete())
    assert m_min and m_plancher, "min_value du curseur ou kEcoPlancher introuvable"
    minimum, plancher = int(m_min.group(1)), float(m_plancher.group(1))
    assert round(plancher * 100) == minimum, f"plancher {plancher} ≠ minimum du curseur {minimum} %"
