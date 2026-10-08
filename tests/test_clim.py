# -*- coding: utf-8 -*-
"""Clim de n'importe quelle marque (ADR-0026, 29/09/2026) : le popup suit l'appareil ;
et toute tuile de clim ouvre le popup pour SA clim (ADR-0027, même jour).

Le contrat tient en des chaînes qu'aucun compilateur ne compare :

- la clé « climr » de tab5_maj_emplacements, écrite par le blueprint et reconnue par le
  firmware (tab5_zones.cpp), et ses lettres de capacités (blueprint ↔ tab5_clim.cpp ↔
  tableau de l'ADR) ;
- les clés des clims de tuile « crRT » (mêmes réglages que climr, même modèle Jinja) et
  « ceRT » (champs de tab5_maj_clim, dans le même ordre) ;
- les noms qu'un mode « actif » peut porter : listes du blueprint (clim_eco, clim_silence,
  clim_oscillation) ↔ fonctions clim_*_actif() du firmware ;
- les positions de la carte OPTIONS : y du YAML ↔ constantes kOptions* du C++ ;
- la clim affichée par le popup : ses gestes et ses commandes (emplacement
  clim_affichee_cle()), la carte de l'accueil qui reste celle du blueprint, le retour à
  celle-ci à la fermeture.

Puis, au rendu (le bac à sable Jinja de tests/test_tuiles_blueprint.py) : la clé climr
d'une Daikin et d'appareils d'autres marques, les clés cr/ce des clims de tuile à chaque
déclencheur, et les commandes que les branches « Clim : … » envoient — les mêmes pour la
clim du blueprint et pour une clim de tuile. La Daikin de l'auteur (ses attributs relus
dans Home Assistant le 29/09/2026) doit recevoir exactement ce que l'ancien blueprint lui
envoyait : la valeur de l'écran telle quelle, froid quand elle est éteinte."""
import datetime as dt
import os
import re

import pytest

from tests.test_tuiles_blueprint import (
    MAINTENANT,
    Etat,
    Passage,
    _blueprint,
    _chercher,
    _declencheur,
    _defs,
    _evenement,
    _rendre,
    _tablette,
)
from tests.commun import lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ADR = os.path.join(REPO, "docs", "decisions", "0026-climate-from-device.md")


def _variables_actions():
    """Bloc `variables:` en tête des actions (payload, absentes, clim, reglages_clims,
    clim_reglages, reglages_changes…)."""
    bloc = _chercher(_blueprint()["actions"], lambda d: "clim_reglages" in (d.get("variables") or {}))
    assert bloc, "clim_reglages introuvable dans les actions du blueprint"
    return bloc["variables"]


def _corps_fonction(source, nom):
    m = re.search(rf"\b{nom}\([^)]*\) \{{(.*?)\n\}}", source, re.S)
    assert m, f"{nom}() introuvable"
    return m.group(1)


# ─────────────────────────────────────────────────────────────────────────────
# Contrat : clé, lettres, noms des modes, positions
# ─────────────────────────────────────────────────────────────────────────────

def test_cle_climr_identique_des_deux_cotes():
    m = re.search(r'kCleClimReglages\[\] = "(\w+)";', _lire("Tab5", "tab5_zones.cpp"))
    assert m and m.group(1) == "climr"
    assert "'climr|'" in _variables_actions()["clim_reglages"]
    # Routée avant la table des emplacements 3.x, vers clim_reglages_recu().
    zones = _lire("Tab5", "tab5_zones.cpp")
    assert zones.index("kCleClimReglages) == 0") < zones.index("for (size_t i = 0; i < n; i++)")
    assert "clim_reglages_recu(payload.data() + p1 + 1" in zones


def _lettres_blueprint():
    return re.findall(r"\('([a-z])' if ", _variables_actions()["reglages_clims"])


def _lettres_adr():
    texte = _lire("docs", "decisions", "0026-climate-from-device.md")
    tableau = texte.split("| Letter | Button |", 1)[1].split("\n\n", 1)[0]
    return [re.match(r"\| `([a-z])` \|", l).group(1) for l in tableau.splitlines()[2:]]


def test_lettres_des_capacites_identiques_partout():
    cartes = _lire("Tab5", "tab5_clim.cpp")
    firmware = re.findall(r"clim_capacite\('([a-z])'\)", cartes)
    blueprint = _lettres_blueprint()
    assert blueprint == list("chdfebqsw"), "une fois chacune, dans l'ordre de l'ADR"
    assert sorted(firmware) == sorted(blueprint), "chaque lettre émise masque un bouton, et réciproquement"
    assert _lettres_adr() == blueprint
    # Sans climr, tous les boutons : la valeur par défaut porte toutes les lettres.
    m = re.search(r'char capacites\[16\] = "(\w+)";', cartes)
    assert m and m.group(1) == "".join(blueprint)


@pytest.mark.parametrize("fonction, liste", [
    ("clim_eco_actif", "clim_eco"),
    ("clim_silence_actif", "clim_silence"),
    ("clim_oscillation_actif", "clim_oscillation"),
])
def test_modes_actifs_identiques_au_blueprint(fonction, liste):
    corps = _corps_fonction(_lire("Tab5", "tab5_clim.cpp"), fonction)
    assert set(re.findall(r'== "(\w+)"', corps)) == set(_blueprint()["variables"][liste])


def test_bascules_et_coloration_par_les_memes_fonctions():
    # Les bascules du popup sont dans le C++ depuis l'ADR-0027 (elles portent sur la clim
    # affichée) : le YAML les appelle, puis envoie ce qu'elles ont posé.
    cartes = _lire("Tab5", "tab5_clim.cpp")
    assert 'f = clim_silence_actif(f) ? std::string("auto") : std::string("quiet");' in \
        _corps_fonction(cartes, "clim_popup_silence")
    assert 's = clim_oscillation_actif(s) ? std::string("stop") : std::string("swing");' in \
        _corps_fonction(cartes, "clim_popup_oscillation")
    assert 'p = clim_preset_actif(p, bouton) ? std::string("none") : std::string(bouton);' in \
        _corps_fonction(cartes, "clim_popup_preset")
    popup = _lire("Tab5", "ui_components", "climate_popup.yaml")
    for geste in ("clim_popup_silence();", "clim_popup_oscillation();", "clim_popup_brise();"):
        assert popup.count(f"- lambda: '{geste}'") == 1, geste
    preset = _lire("Tab5", "ui_components", "climate_preset_toggle_btn.yaml")
    assert "- lambda: 'clim_popup_preset(\"${preset_name}\");'" in preset
    # Boost reste « preset == boost » : seul away (Éco) a deux noms.
    corps = _corps_fonction(cartes, "clim_preset_actif")
    assert '"away"' in corps and "clim_eco_actif(preset)" in corps
    # Coloration (l'ancien script tab5_clim_recolor) : les mêmes fonctions.
    recolor = _corps_fonction(cartes, "clim_recolorer")
    for f in ("clim_eco_actif(preset)", "clim_silence_actif(fan)", "clim_oscillation_actif(swing)"):
        assert f in recolor, f
    assert "- id: tab5_clim_recolor" not in _lire("Tab5", "tab5-scripts.yaml")


def _sans_commentaires(*chemin):
    return "\n".join(l for l in _lire(*chemin).splitlines() if not l.lstrip().startswith("#"))


def test_plus_de_pas_ni_de_bornes_en_dur_dans_les_boutons():
    for nom in ("climate_card.yaml", "climate_popup.yaml"):
        texte = _sans_commentaires("Tab5", "ui_components", nom)
        assert "0.5f" not in texte and "16.0f" not in texte and "30.0f" not in texte, nom
    # La carte : la clim du blueprint (clim_target_temp, ses réglages) ; le popup : la clim
    # affichée (clim_popup_pas compte sur ses réglages, ADR-0027).
    carte = _sans_commentaires("Tab5", "ui_components", "climate_card.yaml")
    assert carte.count("clim_consigne_suivante(id(clim_target_temp), -1)") == 1
    assert carte.count("clim_consigne_suivante(id(clim_target_temp), +1)") == 1
    popup = _sans_commentaires("Tab5", "ui_components", "climate_popup.yaml")
    assert popup.count("clim_popup_pas(-1)") == 1 and popup.count("clim_popup_pas(+1)") == 1
    assert "clim_target_temp" not in popup and "consigne_suivante(vue_reglages(), base, sens)" in \
        _corps_fonction(_lire("Tab5", "tab5_clim.cpp"), "clim_popup_pas")
    # Sans climr : 16-30 et 0,5, comme l'arc du YAML.
    cartes = _lire("Tab5", "tab5_clim.cpp")
    for defaut in ("float min = 16.0f;", "float max = 30.0f;", "float pas = 0.5f;"):
        assert defaut in cartes, defaut
    popup = _lire("Tab5", "ui_components", "climate_popup.yaml")
    arc = popup.split("id: arc_temp_popup", 1)[1]
    assert re.search(r"min_value: 16\n", arc) and re.search(r"max_value: 30\n", arc)


def _y_yaml(texte, ident):
    m = re.search(rf"id: {ident}\b[^\n]*?\by: (\d+)", texte) or \
        re.search(rf"id: {ident}\n(?:[^\n]*\n)*?\s+y: (\d+)\n", texte)
    assert m, f"y de {ident} introuvable"
    return int(m.group(1))


def test_positions_des_options_egales_aux_constantes_du_cpp():
    cartes = _lire("Tab5", "tab5_clim.cpp")
    k = {n: int(v) for n, v in re.findall(r"constexpr int32_t (kOptions\w+) = (\d+);", cartes)}
    popup = _lire("Tab5", "ui_components", "climate_popup.yaml")
    y = k["kOptionsY0"]
    assert _y_yaml(popup, "clim_titre_presets") == y
    assert _y_yaml(popup, "clim_rangee_presets") == y + k["kOptionsSousTitre"]
    y += k["kOptionsSousTitre"] + k["kOptionsRangee"] + k["kOptionsEntreSections"]
    assert _y_yaml(popup, "clim_titre_ventilation") == y
    assert _y_yaml(popup, "popup_btn_clim_quiet") == y + k["kOptionsSousTitre"]
    y += k["kOptionsSousTitre"] + k["kOptionsBouton"] + k["kOptionsEntreSections"]
    assert _y_yaml(popup, "clim_titre_flux") == y
    assert _y_yaml(popup, "popup_btn_clim_swing") == y + k["kOptionsSousTitre"]
    assert _y_yaml(popup, "popup_btn_clim_windnice") == \
        y + k["kOptionsSousTitre"] + k["kOptionsBouton"] + k["kOptionsEntreBoutons"]


def test_widgets_de_la_clim_poses_avant_toute_poussee():
    scripts = _lire("Tab5", "tab5-scripts.yaml")
    bloc = scripts.split("- id: tab5_clim_ui", 1)[1].split("\n  - id: ", 1)[0]
    champs = re.findall(r"u\.(\w+) = id\(", bloc)
    struct = re.search(r"struct ClimUI \{(.*?)\};", _lire("Tab5", "tab5_custom.h"), re.S).group(1)
    assert sorted(champs) == sorted(re.findall(r"lv_obj_t\* (\w+) = nullptr;", struct))
    # L'état de la clim du blueprint (ses globals) et les deux débounces (ADR-0027).
    pointeurs = dict(re.findall(r"u\.(\w+_bp) = &id\((\w+)\);", bloc))
    assert pointeurs == {"consigne_bp": "clim_target_temp", "mode_bp": "clim_hvac_mode",
                         "preset_bp": "clim_preset_mode", "ventilation_bp": "clim_fan_mode",
                         "oscillation_bp": "clim_swing_mode"}
    assert sorted(pointeurs) == sorted(re.findall(r"(?:float|std::string)\* (\w+) = nullptr;", struct))
    assert "u.debounce_blueprint = []() { id(tab5_debounce_clim_temp).execute(); };" in bloc
    assert "u.debounce_tuile = []() { id(tab5_debounce_clim_tuile).execute(); };" in bloc
    # Lancé par tab5_zones_apply (fin du setup, avant la première image).
    assert "- script.execute: tab5_clim_ui" in _lire("Tab5", "tab5-zones.yaml")


# ─────────────────────────────────────────────────────────────────────────────
# Rendu : la clé climr
# ─────────────────────────────────────────────────────────────────────────────

# La Daikin Onecta de l'auteur, attributs relus dans HA le 29/09/2026 (identifiant inventé).
DAIKIN = dict(
    hvac_modes=["off", "fan_only", "heat", "cool", "heat_cool", "dry"], min_temp=18, max_temp=32,
    target_temp_step=0.5, fan_modes=["auto", "quiet", "1", "2", "3", "4", "5"],
    preset_modes=["away", "boost", "none"], swing_modes=["stop", "swing", "windnice"],
    friendly_name="Clim", temperature=23, current_temperature=23, fan_mode="auto",
    preset_mode="none", swing_mode="stop",
)
METEO_C = Etat("weather.maison", "cloudy", temperature_unit="°C")


def _clim(etat="cool", **attributs):
    return Etat("climate.test", etat, **attributs)


def _reglages(clim, *autres):
    p = Passage({"clim": "climate.test"}, [clim, *autres], _evenement("connexion"))
    return p.variables_du_bloc("clim_reglages")


def test_climr_de_la_daikin():
    assert _reglages(_clim(**DAIKIN), METEO_C) == "climr|18.0|32.0|0.5|°C|chdfebqsw|Clim;"


def test_climr_d_autres_appareils():
    # Chauffage seul, en °F, sans entité météo : min_temp >= 40, pas de 1 par défaut.
    chauffage = _clim("off", hvac_modes=["off", "heat"], min_temp=45, max_temp=90, friendly_name="Radiateur")
    assert _reglages(chauffage) == "climr|45.0|90.0|1.0|°F|h|Radiateur;"
    # Une entité météo donne l'unité, même si l'heuristique dirait autre chose.
    assert _reglages(chauffage, METEO_C).split("|")[4] == "°C"
    meteo_f = Etat("weather.maison", "sunny", temperature_unit="°F")
    froid = _clim(hvac_modes=["off", "cool"], min_temp=16, max_temp=30, friendly_name="Clim")
    assert _reglages(froid, meteo_f).split("|")[3:5] == ["1.0", "°F"]
    # Autre marque : eco, low, vertical + off ; pas de windnice ni de boost.
    midea = _clim(hvac_modes=["off", "cool", "heat", "dry", "fan_only", "auto"], min_temp=17, max_temp=30,
                  target_temp_step=1, preset_modes=["none", "eco", "sleep"], fan_modes=["auto", "low", "high"],
                  swing_modes=["off", "vertical", "horizontal", "both"], friendly_name="Clim | chambre; 2")
    assert _reglages(midea, METEO_C) == "climr|17.0|30.0|1.0|°C|chdfeqs|Clim / chambre, 2;"
    # Oscillation sans arrêt possible : pas de bouton (il ne saurait pas l'éteindre).
    sans_arret = _clim(hvac_modes=["cool"], swing_modes=["vertical", "horizontal"])
    assert _reglages(sans_arret, METEO_C).split("|")[5] == "c"
    # Rien ne dit rien : 16-30, pas de 0,5, aucun bouton, nom vide.
    assert _reglages(_clim()) == "climr|16.0|30.0|0.5|°C||;"


def test_climr_vide_sans_clim():
    p = Passage({}, [METEO_C], _evenement("connexion"))
    assert p.variables_du_bloc("clim_reglages") == ""


def test_climr_part_avant_l_etat_de_la_clim():
    actions = _blueprint()["actions"]
    # Poussée complète : dans le même bloc que tab5_maj_clim, juste avant.
    bloc = _chercher(actions, lambda d: "tout_pousser and clim != ''" in str(d.get("if", "")))
    etapes = [str(e.get("action", "")) for e in bloc["then"]]
    i_clim = next(i for i, a in enumerate(etapes) if a.endswith("_tab5_maj_clim"))
    assert etapes[i_clim - 1].endswith("_tab5_maj_emplacements")
    assert bloc["then"][i_clim - 1]["data"]["payload"] == "{{ clim_reglages }}"
    # Changement de la clim : climr (s'il a pu changer), puis l'état.
    branche = _chercher(actions, lambda d: d.get("alias") == "Clim : son état a changé")
    seq = branche["sequence"]
    i_si = next(i for i, e in enumerate(seq) if "clim_reglages" in str(e.get("if", "")))
    i_clim = next(i for i, e in enumerate(seq) if str(e.get("action", "")).endswith("_tab5_maj_clim"))
    assert i_si < i_clim
    assert seq[i_si]["then"][0]["data"]["payload"] == "{{ clim_reglages }}"


@pytest.mark.parametrize("avant, apres, attendu", [
    (None, dict(DAIKIN), True),                                                   # la clim apparaît
    (("unavailable", {"friendly_name": "Clim"}), dict(DAIKIN), True),             # retour de panne
    (("cool", DAIKIN), {**DAIKIN, "current_temperature": 24}, False),            # un degré de la pièce
    (("cool", DAIKIN), {**DAIKIN, "temperature": 21.5}, False),                  # la consigne
    (("off", DAIKIN), dict(DAIKIN), False),                                       # allumée
    (("cool", DAIKIN), {**DAIKIN, "preset_modes": ["away", "none"]}, True),      # un mode en moins
    (("cool", DAIKIN), {**DAIKIN, "max_temp": 30}, True),                        # une borne
])
def test_climr_seulement_quand_les_reglages_peuvent_changer(avant, apres, attendu):
    etat_avant = None if avant is None else _clim(avant[0], **avant[1])
    etat_apres = _clim(**apres)
    p = Passage({"clim": "climate.test"}, [etat_apres, METEO_C], _declencheur("clim", etat_avant, etat_apres))
    # reglages_changes : premier bloc des actions depuis l'ADR-0027 (climr et crRT).
    p.variables_du_bloc("reglages_changes")
    branche = _chercher(p.corps["actions"], lambda d: d.get("alias") == "Clim : son état a changé")
    assert p.modele(next(e for e in branche["sequence"] if "if" in e)["if"]) is attendu


def test_reglages_changes_faux_hors_d_un_changement_d_etat():
    for trigger in (_evenement("connexion"), {"id": "mesures", "platform": "time_pattern"}):
        p = Passage({"clim": "climate.test"}, [_clim(**DAIKIN), METEO_C], trigger)
        assert p.variables_du_bloc("reglages_changes") is False


# ─────────────────────────────────────────────────────────────────────────────
# Rendu : commandes de l'écran vers les vrais modes de l'appareil
# ─────────────────────────────────────────────────────────────────────────────

def _executer(p, sequence, cibles=None):
    """(action, données) que la séquence enverrait : variables, if/then/else, actions.
    `cibles` (liste) reçoit l'entité visée par chaque action."""
    envoyees = []
    for etape in sequence:
        if "variables" in etape:
            for cle, valeur in etape["variables"].items():
                p.ctx[cle] = _rendre(p.env, valeur, p.ctx)
        elif "if" in etape:
            suite = etape["then"] if p.modele(etape["if"]) else etape.get("else", [])
            envoyees += _executer(p, suite, cibles)
        elif "action" in etape:
            envoyees.append((p.modele(etape["action"]), _rendre(p.env, etape.get("data", {}), p.ctx)))
            if cibles is not None:
                cibles.append(_rendre(p.env, etape.get("target", {}), p.ctx).get("entity_id"))
    return envoyees


def _commande(clim, commande, valeur, entrees=None, autres=(), cibles=None):
    """Commande du popup quand il montre la clim du blueprint (emplacement « clim »)."""
    p = Passage(entrees or {"clim": "climate.test"}, [clim, METEO_C, *autres],
                _evenement("action", emplacement="clim", action=commande, valeur=valeur))
    p.variables_du_bloc("clim")
    alias, sequence = p.aiguillage()
    assert alias is None or alias.startswith("Clim : "), (commande, valeur, alias)
    return _executer(p, sequence, cibles)


# Ce que la tablette envoie (boutons du popup, carte, débounce) → ce que l'ancien
# blueprint envoyait à la Daikin : la valeur telle quelle (et froid si elle est éteinte).
DAIKIN_AVANT = [
    ("cool", "consigne", "21.5", [("climate.set_temperature", {"temperature": 21.5, "hvac_mode": "cool"})]),
    ("off", "consigne", "21.5", [("climate.set_temperature", {"temperature": 21.5, "hvac_mode": "cool"})]),
    ("heat", "consigne", "23", [("climate.set_temperature", {"temperature": 23.0, "hvac_mode": "heat"})]),
    ("off", "eteindre", "", [("climate.turn_off", {})]),
    ("off", "mode", "cool", [("climate.set_hvac_mode", {"hvac_mode": "cool"})]),
    ("off", "mode", "heat", [("climate.set_hvac_mode", {"hvac_mode": "heat"})]),
    ("off", "mode", "dry", [("climate.set_hvac_mode", {"hvac_mode": "dry"})]),
    ("off", "mode", "fan_only", [("climate.set_hvac_mode", {"hvac_mode": "fan_only"})]),
    ("cool", "preset", "away", [("climate.set_preset_mode", {"preset_mode": "away"})]),
    ("cool", "preset", "boost", [("climate.set_preset_mode", {"preset_mode": "boost"})]),
    ("cool", "preset", "none", [("climate.set_preset_mode", {"preset_mode": "none"})]),
    ("cool", "ventilation", "quiet", [("climate.set_fan_mode", {"fan_mode": "quiet"})]),
    ("cool", "ventilation", "auto", [("climate.set_fan_mode", {"fan_mode": "auto"})]),
    ("cool", "oscillation", "swing", [("climate.set_swing_mode", {"swing_mode": "swing"})]),
    ("cool", "oscillation", "stop", [("climate.set_swing_mode", {"swing_mode": "stop"})]),
    ("cool", "oscillation", "windnice", [("climate.set_swing_mode", {"swing_mode": "windnice"})]),
]


@pytest.mark.parametrize("etat, commande, valeur, attendu", DAIKIN_AVANT)
def test_la_daikin_recoit_ce_qu_elle_recevait(etat, commande, valeur, attendu):
    assert _commande(_clim(etat, **DAIKIN), commande, valeur) == attendu


MIDEA = dict(hvac_modes=["off", "cool", "heat", "dry", "fan_only", "auto"], min_temp=17, max_temp=30,
             target_temp_step=1, preset_modes=["none", "eco", "boost", "sleep"], fan_modes=["low", "medium", "high"],
             swing_modes=["off", "vertical", "horizontal", "both"], friendly_name="Clim chambre")


AUTRES_MARQUES = [
    # Consigne bornée ; éteinte : froid d'abord, sinon chaud, sinon sans mode.
    ("cool", MIDEA, "consigne", "35", [("climate.set_temperature", {"temperature": 30.0, "hvac_mode": "cool"})]),
    ("cool", MIDEA, "consigne", "12", [("climate.set_temperature", {"temperature": 17.0, "hvac_mode": "cool"})]),
    ("off", dict(hvac_modes=["off", "heat"]), "consigne", "20",
     [("climate.set_temperature", {"temperature": 20.0, "hvac_mode": "heat"})]),
    ("off", dict(hvac_modes=["off", "heat_cool"]), "consigne", "20",
     [("climate.set_temperature", {"temperature": 20.0, "hvac_mode": "heat_cool"})]),
    ("off", dict(hvac_modes=["off", "fan_only"]), "consigne", "20", [("climate.set_temperature", {"temperature": 20.0})]),
    # Mode absent de l'appareil : rien (HA lèverait une erreur).
    ("off", dict(hvac_modes=["off", "heat"]), "mode", "cool", []),
    # Éco : eco s'il existe ; aucun préréglage : none, sinon home ou comfort.
    ("cool", MIDEA, "preset", "away", [("climate.set_preset_mode", {"preset_mode": "eco"})]),
    ("cool", dict(preset_modes=["home", "away", "sleep"]), "preset", "none",
     [("climate.set_preset_mode", {"preset_mode": "home"})]),
    ("cool", dict(preset_modes=["comfort", "eco"]), "preset", "none",
     [("climate.set_preset_mode", {"preset_mode": "comfort"})]),
    ("cool", dict(preset_modes=["none", "eco"]), "preset", "boost", []),
    ("cool", dict(), "preset", "away", []),
    # Silence : quiet, silence, Silence ou low ; auto, sinon une vitesse moyenne, sinon la première.
    ("cool", MIDEA, "ventilation", "quiet", [("climate.set_fan_mode", {"fan_mode": "low"})]),
    ("cool", MIDEA, "ventilation", "auto", [("climate.set_fan_mode", {"fan_mode": "medium"})]),
    ("cool", dict(fan_modes=["Silence", "Turbo"]), "ventilation", "quiet", [("climate.set_fan_mode", {"fan_mode": "Silence"})]),
    # Fin du silence sans auto ni vitesse moyenne : le premier mode qui n'est pas un silence.
    ("cool", dict(fan_modes=["Silence", "Turbo"]), "ventilation", "auto", [("climate.set_fan_mode", {"fan_mode": "Turbo"})]),
    ("cool", dict(fan_modes=["low"]), "ventilation", "auto", [("climate.set_fan_mode", {"fan_mode": "low"})]),
    ("cool", dict(fan_modes=["high"]), "ventilation", "quiet", []),
    # Oscillation : stop → off ; swing → le premier de swing, on, both, vertical…
    # (both ici) ; windnice seulement s'il existe.
    ("cool", MIDEA, "oscillation", "stop", [("climate.set_swing_mode", {"swing_mode": "off"})]),
    ("cool", MIDEA, "oscillation", "swing", [("climate.set_swing_mode", {"swing_mode": "both"})]),
    ("cool", dict(swing_modes=["off", "vertical", "horizontal"]), "oscillation", "swing",
     [("climate.set_swing_mode", {"swing_mode": "vertical"})]),
    ("cool", MIDEA, "oscillation", "windnice", []),
    ("cool", dict(swing_modes=["on", "off"]), "oscillation", "swing", [("climate.set_swing_mode", {"swing_mode": "on"})]),
]


@pytest.mark.parametrize("etat, attributs, commande, valeur, attendu", AUTRES_MARQUES)
def test_commandes_vers_les_modes_de_l_appareil(etat, attributs, commande, valeur, attendu):
    assert _commande(_clim(etat, **attributs), commande, valeur) == attendu


# ─────────────────────────────────────────────────────────────────────────────
# Clims des tuiles (ADR-0027) : contrat
# ─────────────────────────────────────────────────────────────────────────────

def _bloc_etats():
    """Bloc `variables:` des états des tuiles (etats_tuiles, reglages_tuiles, etats_clims)."""
    return _chercher(_blueprint()["actions"], lambda d: "etats_clims" in (d.get("variables") or {}))["variables"]


def test_cles_cr_et_ce_identiques_des_deux_cotes():
    cartes = _lire("Tab5", "tab5_clim.cpp")
    assert 'constexpr char kCleReglagesTuile[] = "cr";' in cartes
    assert 'constexpr char kCleEtatTuile[] = "ce";' in cartes
    etats = _bloc_etats()
    assert "'cr' ~ t.cle[1:] ~ '|' ~ reglages_clims[t.e] ~ ';'" in etats["reglages_tuiles"]
    assert "'ce' ~ t.cle[1:] ~ '|'" in etats["etats_clims"]
    # Une seule traduction des réglages : climr et crRT sortent de reglages_clims.
    assert "reglages_clims[clim]" in _variables_actions()["clim_reglages"]
    # Routées avant la table des emplacements 3.x, après les tuiles et climr.
    corps = _lire("Tab5", "tab5_zones.cpp").split("int emplacements_appliquer(", 1)[1]
    assert corps.index("tuiles_etat_recu(") < corps.index("clim_reglages_recu(") \
        < corps.index("clim_tuile_recu(") < corps.index("for (size_t i = 0; i < n; i++)")
    # Décrites dans le contrat de l'action.
    api = _lire("Tab5", "tab5-api-logic.yaml").split("- service: tab5_maj_emplacements", 1)[1].split("- service:", 1)[0]
    assert "« crRT|min|max|pas|unité|capacités|nom »" in api
    assert "« ceRT|consigne|pièce|mode|préréglage|ventilation|oscillation »" in api


def test_champs_de_ce_dans_l_ordre_de_tab5_maj_clim():
    api = _lire("Tab5", "tab5-api-logic.yaml").split("- service: tab5_maj_clim", 1)[1].split("\n    - service:", 1)[0]
    assert re.findall(r"^        (\w+):\n          type: string", api, re.M) == \
        ["target", "current", "mode", "preset", "fan", "swing"]
    # Ce que le blueprint envoie à tab5_maj_clim, champ par champ…
    branche = _chercher(_blueprint()["actions"], lambda d: d.get("alias") == "Clim : son état a changé")
    donnees = next(e for e in branche["sequence"] if str(e.get("action", "")).endswith("_tab5_maj_clim"))["data"]
    attendu = []
    for champ in ("target", "current", "mode", "preset", "fan", "swing"):
        m = re.search(r"state_attr\(clim, '(\w+)'\)(.*?)\}\}", donnees[champ])
        attendu.append((m.group(1), re.search(r"default\('(\w+)', true\)", m.group(2)).group(1)
                        if "default" in m.group(2) else None) if m else ("etat", None))
    # … et dans la clé ceRT, dans le même ordre, avec les mêmes défauts (nombres : nan).
    modele = _bloc_etats()["etats_clims"]
    vu = []
    for m in re.finditer(r"a\.get\('(\w+)'\)( \| default\('(\w+)', true\))?|(states)\(t\.e\)", modele):
        vu.append(("etat", None) if m.group(4) else (m.group(1), m.group(3)))
    assert vu == attendu
    assert modele.count("| float('nan')") == 2
    # Le firmware les lit dans cet ordre (consigne, pièce, puis les quatre modes).
    lire = _corps_fonction(_lire("Tab5", "tab5_clim.cpp"), "lire_etat")
    assert "e.consigne = k > 0 ?" in lire and "e.piece = k > 1 ?" in lire
    assert "std::string* modes[4] = {&e.mode, &e.preset, &e.ventilation, &e.oscillation};" in lire


def test_popup_commande_la_clim_affichee():
    for nom in ("climate_popup.yaml", "climate_hvac_mode_btn.yaml", "climate_preset_toggle_btn.yaml"):
        texte = _sans_commentaires("Tab5", "ui_components", nom)
        envois = re.findall(r"id: tab5_action, emplacement: ([^,]+), commande: (\w+)", texte)
        assert envois, nom
        assert {e for e, _ in envois} == {"!lambda 'return clim_affichee_cle();'"}, (nom, envois)
    # La carte de l'accueil : la clim du blueprint, par son débounce (emplacement clim).
    carte = _sans_commentaires("Tab5", "ui_components", "climate_card.yaml")
    assert carte.count("- script.execute: tab5_debounce_clim_temp") == 2 and "clim_popup" not in carte
    scripts = _lire("Tab5", "tab5-scripts.yaml")
    bp = scripts.split("- id: tab5_debounce_clim_temp", 1)[1].split("\n  - id: ", 1)[0]
    assert "emplacement: clim\n" in bp and "clim_consigne_texte(id(clim_target_temp))" in bp
    tuile = scripts.split("- id: tab5_debounce_clim_tuile", 1)[1].split("\n  - id: ", 1)[0]
    assert "emplacement: !lambda 'return clim_tuile_attente_cle();'" in tuile
    assert "valeur: !lambda 'return clim_tuile_attente_texte();'" in tuile
    assert "delay: 250ms" in bp and "delay: 250ms" in tuile and "mode: restart" in tuile
    cartes = _lire("Tab5", "tab5_clim.cpp")
    assert 'return vue_tuile() ? s_vue_cle : "clim";' in _corps_fonction(cartes, "clim_affichee_cle")
    # La consigne d'une tuile : clé et valeur prises au geste (popup refermé avant l'envoi).
    geste = _corps_fonction(cartes, "consigne_geste")
    assert "std::memcpy(s_attente.cle, s_vue_cle, sizeof(s_attente.cle));" in geste
    assert "u.debounce_tuile()" in geste and "u.debounce_blueprint()" in geste


def test_carte_de_l_accueil_reste_la_clim_du_blueprint():
    cartes = _lire("Tab5", "tab5_clim.cpp")
    recolor = _corps_fonction(cartes, "clim_recolorer")
    assert "ui_text_color(u.consigne_carte, couleur_consigne(u.mode_bp != nullptr ? *u.mode_bp : kSansMode));" in recolor
    recu = _corps_fonction(cartes, "clim_blueprint_recu")
    assert "carte_consigne_ui(consigne);" in recu and "if (!vue_tuile()) {" in recu
    api = _lire("Tab5", "tab5-api-logic.yaml").split("- service: tab5_maj_clim", 1)[1].split("\n    - service:", 1)[0]
    assert "clim_blueprint_recu(id(clim_target_temp), tab5_fini_ou_nan(atof(current.c_str())));" in api
    # « inf » reçu = consigne inconnue, jamais convertie en entier (lot A, audit du 30/09/2026).
    assert "id(clim_target_temp) = tab5_fini_ou_nan(atof(target.c_str()));" in api
    # Les clés d'une tuile ne touchent jamais la carte, et le popup seulement s'il la montre.
    tuile = _corps_fonction(cartes, "clim_tuile_recu")
    assert "consigne_carte" not in tuile and "carte_consigne_ui" not in tuile and "if (affichee)" in tuile
    # climr ne touche le popup que s'il montre la clim du blueprint.
    assert "if (vue_tuile()) return;" in _corps_fonction(cartes, "clim_reglages_recu")


def test_ouverture_et_fermeture_du_popup():
    tuiles = "\n".join(_lire("Tab5", f) for f in ("tab5_tuiles_priv.h", "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp"))
    # Le corps de l'appui d'une tuile (tuile_appui le passe à la pièce courante, le popup
    # d'un appareil à la sienne, 06/10/2026).
    appui = tuiles.split("void tuiles::tuile_appui_piece(int r, int t, bool long_appui) {", 1)[1].split("\n}\n", 1)[0]
    assert "type == Type::CLI && clim_tuile_connue(r, t)" in appui
    # La fenêtre CLIM (table des gestes, lot L7) : la clim du blueprint avec l'option m.
    fenetre = tuiles.split("static bool ouvrir_fenetre(Fenetre f, const Def& d, int r, int t) {", 1)[1].split("\n}\n", 1)[0]
    assert "if (c.r < 0) clim_afficher_blueprint();" in fenetre
    assert "else if (!clim_afficher_tuile(c.r, c.t)) return false;" in fenetre
    assert "if (d.options & OPT_M) return c;" in tuiles.split("ClimCible clim_cible(const Def& d, int r, int t) {", 1)[1]
    # Une tuile redéfinie oublie sa clim ; ses réglages reçus repeignent la tuile (bouton).
    assert "clim_tuile_oublier(r, t);" in tuiles.split("bool tuiles_definir(", 1)[1].split("\n}\n", 1)[0]
    cartes = _lire("Tab5", "tab5_clim.cpp")
    assert "tuiles_repeindre(r, t);" in _corps_fonction(cartes, "clim_tuile_recu")
    assert "if (!clim_tuile_connue(r, t)) return false;" in _corps_fonction(cartes, "clim_afficher_tuile")
    # Retour à la clim du blueprint : croix et voile, carte de l'accueil, « Aller à l'écran ».
    popup = _lire("Tab5", "ui_components", "climate_popup.yaml")
    assert popup.count('close_lambda: "animate_popup_close(id(clim_options_popup)); clim_afficher_blueprint();"') == 2
    carte = _lire("Tab5", "ui_components", "climate_card.yaml")
    assert "clim_afficher_blueprint();\n                    animate_popup_open(id(clim_options_popup));" in carte
    assert re.search(r'"Climatisation",\s+ModalRegistry::POPUP,\s+\[\] \{ clim_afficher_blueprint\(\);',
                     _lire("Tab5", "tab5-navigation.yaml"))
    # Popup refermé par close_all() (inactivité) : la clim affichée revient au premier
    # retour de HA, avant qu'il ne soit rangé.
    for f in ("clim_blueprint_recu", "clim_reglages_recu", "clim_tuile_recu"):
        assert "vue_verifier();" in _corps_fonction(cartes, f), f
    assert "if (s_vue < 0 || popup_visible()) return;" in _corps_fonction(cartes, "vue_verifier")


def test_table_des_clims_de_tuile_en_psram_a_la_demande():
    cartes = _lire("Tab5", "tab5_clim.cpp")
    assert "ClimTuile* s_ct = nullptr;" in cartes
    table = _corps_fonction(cartes, "tuile_clim")
    assert "if (!creer) return nullptr;" in table
    assert table.index("MALLOC_CAP_SPIRAM") < table.index("MALLOC_CAP_INTERNAL")
    assert "new (&table[i]) ClimTuile();" in table
    assert not re.search(r"^EXT_RAM_BSS_ATTR", cartes, re.M), "constructeurs : pas de BSS externe"
    # Modes bornés : la chaîne reste dans son std::string.
    assert "constexpr size_t kModeMax = 15;" in cartes


# ─────────────────────────────────────────────────────────────────────────────
# Clims des tuiles (ADR-0027) : rendu des poussées
# ─────────────────────────────────────────────────────────────────────────────

# La Daikin est la clim du blueprint et une tuile de la pièce 1 (option m) ; une clim
# d'une autre marque dans la pièce 1, un radiateur dans la pièce 2 (identifiants inventés).
CHAMBRE = {**MIDEA, "temperature": 21, "current_temperature": 19.5, "fan_mode": "low", "swing_mode": "off",
           "preset_mode": "eco"}
ENTREES_CLIMS = {"clim": "climate.daikin", "piece_1_tuiles": ["climate.daikin", "climate.chambre", "light.plafond"],
                 "piece_2_tuiles": ["climate.radiateur"]}


def _maison_clims(*remplacees):
    etats = {e.entity_id: e for e in (
        Etat("climate.daikin", "cool", **DAIKIN),
        Etat("climate.chambre", "heat", **CHAMBRE),
        Etat("climate.radiateur", "off", hvac_modes=["off", "heat"], min_temp=7, max_temp=35,
             friendly_name="Radiateur | bureau; 2"),
        Etat("light.plafond", "on", friendly_name="Plafonnier", supported_color_modes=["onoff"]),
    )}
    etats.update({e.entity_id: e for e in remplacees})
    return list(etats.values()) + [METEO_C]


def _poussee(trigger, etats=None, tablettes=None):
    """(passage, reglages_tuiles, etats_tuiles, etats_clims) d'un déclenchement."""
    p = Passage(ENTREES_CLIMS, etats or _maison_clims(), trigger, tablettes)
    if not p["tuiles_a_pousser"]:
        return p, "", "", ""
    p.etats_tuiles()
    return p, p["reglages_tuiles"], p["etats_tuiles"], p["etats_clims"]


def _bloc_envoi(p):
    return _chercher(p.corps["actions"], lambda d: "etats_tuiles" in str(d.get("if", "")))


def test_clims_des_tuiles_a_la_connexion():
    p, reglages, etats, clims = _poussee(_evenement("connexion"))
    # La Daikin (t00) a l'option m : climr et tab5_maj_clim, ni cr ni ce.
    assert [t["cle"] for t in p["tuiles_clim"]] == ["t01", "t10"]
    assert _defs(p.definitions())[1:3] == [["t00", "cli", "clim", "m", "", "Clim"],
                                           ["t01", "cli", "clim", "", "", "Clim chambre"]]
    assert _defs(reglages) == [
        ["cr01", "17.0", "30.0", "1.0", "°C", "chdfebqs", "Clim chambre"],
        ["cr10", "7.0", "35.0", "0.5", "°C", "h", "Radiateur / bureau, 2"],
    ]
    assert _defs(clims) == [
        ["ce01", "21.0", "19.5", "heat", "eco", "low", "off"],
        # Consigne et pièce inconnues : nan (« -- » à l'écran, pas un faux 20) ; préréglage,
        # ventilation, oscillation absents : les défauts de tab5_maj_clim.
        ["ce10", "nan", "nan", "off", "none", "auto", "stop"],
    ]
    # Une seule poussée, au protocole 2 : réglages, puis tRT, puis ce.
    bloc = _bloc_envoi(p)
    assert p.modele(bloc["if"]) is True
    assert bloc["then"][0]["data"]["payload"] == "{{ reglages_tuiles ~ etats_tuiles ~ etats_clims }}"
    # Protocole 1 (firmware 3.0/3.1) : rien.
    p1, *_ = _poussee(_evenement("connexion"), tablettes=_tablette("3.1.0 (ESPHome 2026.9.0)"))
    assert p1.modele(_bloc_envoi(p1)["if"]) is False


def test_cr_est_climr_pour_la_meme_clim():
    p, reglages, _, _ = _poussee(_evenement("connexion"))
    assert p.variables_du_bloc("clim_reglages") == "climr|" + p["reglages_clims"]["climate.daikin"] + ";"
    # La clim de la chambre choisie comme clim du blueprint : son climr = son cr.
    p2 = Passage({"clim": "climate.chambre"}, _maison_clims(), _evenement("connexion"))
    assert p2.variables_du_bloc("clim_reglages") == "climr|" + "|".join(_defs(reglages)[0][1:]) + ";"


@pytest.mark.parametrize("id_, avant, apres, tuiles, cr", [
    # Changement de mode (piece_1) : son état tout de suite, pas ses réglages.
    ("piece_1", ("heat", {}), ("cool", {}), ["t01"], False),
    # Consigne (piece_1_consigne, attribut temperature) : tout de suite, les − / + du
    # popup en partent.
    ("piece_1_consigne", ("heat", {}), ("heat", {"temperature": 22}), ["t01"], False),
    # Retour de panne : ses réglages aussi.
    ("piece_1", ("unavailable", None), ("heat", {}), ["t01"], True),
    # Un mode de moins en changeant de mode : ses réglages aussi.
    ("piece_1", ("heat", {}), ("cool", {"fan_modes": ["low", "high"]}), ["t01"], True),
])
def test_clim_d_une_tuile_quand_elle_change(id_, avant, apres, tuiles, cr):
    if avant[1] is None:  # indisponible : HA ne garde que le nom
        etat_avant = Etat("climate.chambre", avant[0], friendly_name="Clim chambre")
    else:
        etat_avant = Etat("climate.chambre", avant[0], **{**CHAMBRE, **avant[1]})
    etat_apres = Etat("climate.chambre", apres[0], **{**CHAMBRE, **apres[1]})
    p, reglages, _, clims = _poussee(_declencheur(id_, etat_avant, etat_apres), _maison_clims(etat_apres))
    assert p["tuiles_a_pousser"] == tuiles and p.conditions()
    assert [e[0] for e in _defs(clims)] == ["ce01"]
    assert _defs(clims)[0][1] == str(float(apres[1].get("temperature", CHAMBRE["temperature"])))
    assert _defs(clims)[0][3] == apres[0]
    assert [e[0] for e in _defs(reglages)] == (["cr01"] if cr else [])


def test_clim_du_blueprint_n_a_ni_cr_ni_ce():
    daikin = Etat("climate.daikin", "cool", **DAIKIN)
    eteinte = Etat("climate.daikin", "off", **DAIKIN)
    p, reglages, etats, clims = _poussee(_declencheur("piece_1", daikin, eteinte), _maison_clims(eteinte))
    assert p["tuiles_a_pousser"] == ["t00"] and _defs(etats) == [["t00", "off", "23.0", ""]]
    assert reglages == "" and clims == ""


def test_etat_d_une_clim_de_tuile_avec_les_mesures():
    """Température de la pièce, ventilation, oscillation, préréglage changés : au passage
    « mesures » (5 minutes), jamais à chaque dixième : aucun déclencheur de pièce ne voit
    current_temperature (test_declencheurs_des_pieces)."""
    etats = _maison_clims()
    for e in etats:
        if e.entity_id == "climate.chambre":
            e.last_changed = e.last_updated = MAINTENANT - dt.timedelta(seconds=100)
    p, reglages, _, clims = _poussee({"id": "mesures", "platform": "time_pattern"}, etats)
    assert p["tuiles_a_pousser"] == ["t01"] and reglages == ""
    assert [e[0] for e in _defs(clims)] == ["ce01"]
    # Rien de changé depuis 5 minutes : rien.
    p, _, _, clims = _poussee({"id": "mesures", "platform": "time_pattern"})
    assert p["tuiles_a_pousser"] == [] and clims == ""


# ─────────────────────────────────────────────────────────────────────────────
# Clims des tuiles (ADR-0027) : commandes, une seule traduction
# ─────────────────────────────────────────────────────────────────────────────

def _commande_tuile(clim, commande, valeur, comportement=None, cibles=None):
    """Commande du popup quand il montre la clim de la tuile t10 (`clim`, en climate.test)."""
    entrees = {"clim": "climate.daikin", "piece_2_tuiles": ["climate.test"]}
    if comportement:
        entrees["personnalisation"] = [{"entite": "climate.test", "comportement": comportement}]
    p = Passage(entrees, [clim, Etat("climate.daikin", "cool", **DAIKIN), METEO_C],
                _evenement("action", emplacement="t10", action=commande, valeur=valeur))
    p.variables_du_bloc("clim")
    alias, sequence = p.aiguillage()
    assert alias is None or alias.startswith("Clim : "), (commande, valeur, alias)
    return _executer(p, sequence, cibles)


@pytest.mark.parametrize("etat, attributs, commande, valeur, attendu",
                         [(e, DAIKIN, c, v, a) for e, c, v, a in DAIKIN_AVANT] + AUTRES_MARQUES)
def test_une_clim_de_tuile_traduite_comme_celle_du_blueprint(etat, attributs, commande, valeur, attendu):
    cibles = []
    assert _commande_tuile(_clim(etat, **attributs), commande, valeur, cibles=cibles) == attendu
    assert set(cibles) <= {"climate.test"}, "l'entité de la tuile, jamais la clim du blueprint"
    # Et la même chose que si elle était la clim du blueprint.
    assert _commande(_clim(etat, **attributs), commande, valeur) == attendu


@pytest.mark.parametrize("etat, commande, valeur, attendu", DAIKIN_AVANT)
def test_la_daikin_recoit_la_meme_chose_avec_des_clims_de_tuile(etat, commande, valeur, attendu):
    cibles = []
    daikin = Etat("climate.daikin", etat, **DAIKIN)
    autres = [e for e in _maison_clims() if e.entity_id != "climate.daikin"]
    assert _commande(daikin, commande, valeur, ENTREES_CLIMS, autres, cibles) == attendu
    assert set(cibles) == {"climate.daikin"}


def test_liste_blanche_des_clims_de_tuile():
    midea = _clim("cool", **MIDEA)
    # Lecture seule : rien ; allumer seulement : pas d'arrêt, le reste passe.
    assert _commande_tuile(midea, "consigne", "22", "lecture_seule") == []
    assert _commande_tuile(midea, "eteindre", "", "allumer_seulement") == []
    assert _commande_tuile(midea, "mode", "heat", "allumer_seulement") == \
        [("climate.set_hvac_mode", {"hvac_mode": "heat"})]
    # Une tuile qui n'est pas une clim, une tuile vide, une clé qui n'est pas une tuile.
    for cle in ("t00", "t44", "t1", "ce10", "cr10"):
        p = Passage({"piece_1_tuiles": ["light.plafond"], "piece_2_tuiles": ["climate.test"]},
                    [Etat("light.plafond", "on", supported_color_modes=["onoff"]), midea, METEO_C],
                    _evenement("action", emplacement=cle, action="consigne", valeur="21"))
        p.variables_du_bloc("clim")
        assert p.aiguillage() == (None, []), cle


# ─────────────────────────────────────────────────────────────────────────────
# Zone « discussion » (boutons Domo / Discu)
# ─────────────────────────────────────────────────────────────────────────────

def _absentes(*etats, trigger=None):
    p = Passage({}, list(etats), trigger or _evenement("zones"))
    return p.variables_du_bloc("absentes").split(",")


def test_discussion_absente_seulement_sur_aucun():
    assert "discussion" in _absentes(Etat("select.tab5_pipeline_de_discussion", "Aucun"))
    assert "discussion" not in _absentes(Etat("select.tab5_pipeline_de_discussion", "Mon LLM"))
    # Package plus ancien, sans la liste : les boutons restent.
    assert "discussion" not in _absentes()


def test_le_pipeline_change_renvoie_les_zones():
    bp = _blueprint()
    declencheur = [t for t in bp["triggers"] if t.get("id") == "pipeline_discussion"]
    assert declencheur == [{"trigger": "state", "entity_id": "select.tab5_pipeline_de_discussion",
                            "id": "pipeline_discussion"}]
    avant = Etat("select.tab5_pipeline_de_discussion", "Mon LLM")
    apres = Etat("select.tab5_pipeline_de_discussion", "Aucun")
    p = Passage({}, [apres], _declencheur("pipeline_discussion", avant, apres))
    assert p.conditions(), "le passage continue (tablette connectée)"
    branche = _chercher(p.corps["actions"], lambda d: d.get("alias") == "La tablette demande quelles zones sont absentes")
    assert p.modele(branche["conditions"]) is True
    assert branche["sequence"][0]["action"].endswith("_tab5_maj_zones")
    # Ni emplacement, ni tuile, ni définition à pousser pour ce déclencheur.
    assert p["cles"] == [] and p["tuiles_a_pousser"] == [] and p["redefinir"] is False


def test_discussion_masque_les_quatre_boutons():
    zones = _lire("Tab5", "tab5_zones.cpp")
    assert "zone_absente(Zone::DISCUSSION)" in zones
    for champ in ("btn_domo", "btn_discu", "assist_domo", "assist_discu", "assist_cerveau"):
        assert f"u.{champ}" in zones, champ
    yaml_zones = _lire("Tab5", "tab5-zones.yaml")
    for widget in ("btn_mode_domo", "btn_mode_discu", "btn_assist_pipe_domo", "btn_assist_pipe_discu",
                   "lbl_assist_cerveau"):
        assert f"= id({widget});" in yaml_zones, widget
    # Plus de bouton pour quitter le mode Discussion : retour au mode Domotique.
    assert "zone_absente(Zone::DISCUSSION) && id(conversation_mode)" in yaml_zones


def test_travail_traduit_dans_le_detail_du_jour():
    calendrier = _lire("Tab5", "tab5_calendar.cpp")
    corps = calendrier.split("void cal_render_day_detail(", 1)[1]
    assert 'strcmp(tok, "travail") == 0' in corps and 'txt.replace(0, 7, tr("Travail"))' in corps
    # C'est bien ce qu'envoie HA (packages/tab5_calendar.yaml).
    assert "'travail|Travail ' ~" in _lire("HomeAssistant_Config", "packages", "tab5_calendar.yaml")
