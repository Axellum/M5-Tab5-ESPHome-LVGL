# -*- coding: utf-8 -*-
"""Clim de n'importe quelle marque (ADR-0026, 29/09/2026) : le popup suit l'appareil.

Le contrat tient en des chaînes qu'aucun compilateur ne compare :

- la clé « climr » de tab5_maj_emplacements, écrite par le blueprint et reconnue par le
  firmware (tab5_zones.cpp), et ses lettres de capacités (blueprint ↔ tab5_cards.cpp ↔
  tableau de l'ADR) ;
- les noms qu'un mode « actif » peut porter : listes du blueprint (clim_eco, clim_silence,
  clim_oscillation) ↔ fonctions clim_*_actif() du firmware ;
- les positions de la carte OPTIONS : y du YAML ↔ constantes kOptions* du C++.

Puis, au rendu (le bac à sable Jinja de tests/test_tuiles_blueprint.py) : la clé climr
d'une Daikin et d'appareils d'autres marques, et les commandes que les branches
« Clim : … » envoient. La Daikin de l'auteur (ses attributs relus dans Home Assistant le
29/09/2026) doit recevoir exactement ce que l'ancien blueprint lui envoyait : la valeur
de l'écran telle quelle, froid quand elle est éteinte."""
import os
import re

import pytest

from tests.test_tuiles_blueprint import (
    Etat,
    Passage,
    _blueprint,
    _chercher,
    _declencheur,
    _evenement,
    _rendre,
)

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ADR = os.path.join(REPO, "docs", "decisions", "0026-climate-from-device.md")


def _lire(*parts):
    with open(os.path.join(REPO, *parts), encoding="utf-8") as f:
        return f.read()


def _variables_actions():
    """Bloc `variables:` en tête des actions (payload, absentes, clim, clim_reglages…)."""
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
    return re.findall(r"\('([a-z])' if ", _variables_actions()["clim_reglages"])


def _lettres_adr():
    texte = _lire("docs", "decisions", "0026-climate-from-device.md")
    tableau = texte.split("| Letter | Button |", 1)[1].split("\n\n", 1)[0]
    return [re.match(r"\| `([a-z])` \|", l).group(1) for l in tableau.splitlines()[2:]]


def test_lettres_des_capacites_identiques_partout():
    cartes = _lire("Tab5", "tab5_cards.cpp")
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
    corps = _corps_fonction(_lire("Tab5", "tab5_cards.cpp"), fonction)
    assert set(re.findall(r'== "(\w+)"', corps)) == set(_blueprint()["variables"][liste])


def test_bascules_et_coloration_par_les_memes_fonctions():
    popup = _lire("Tab5", "ui_components", "climate_popup.yaml")
    assert 'clim_silence_actif(id(clim_fan_mode)) ? std::string("auto") : std::string("quiet")' in popup
    assert 'clim_oscillation_actif(id(clim_swing_mode)) ? std::string("stop") : std::string("swing")' in popup
    preset = _lire("Tab5", "ui_components", "climate_preset_toggle_btn.yaml")
    assert 'clim_preset_actif(id(clim_preset_mode), "${preset_name}") ? std::string("none")' in preset
    # Boost reste « preset == boost » : seul away (Éco) a deux noms.
    corps = _corps_fonction(_lire("Tab5", "tab5_cards.cpp"), "clim_preset_actif")
    assert '"away"' in corps and "clim_eco_actif(preset)" in corps
    recolor = _lire("Tab5", "tab5-scripts.yaml").split("- id: tab5_clim_recolor", 1)[1].split("\n  - id: ", 1)[0]
    for f in ("clim_eco_actif(preset)", "clim_silence_actif(fan)", "clim_oscillation_actif(swing)"):
        assert f in recolor, f


def test_plus_de_pas_ni_de_bornes_en_dur_dans_les_boutons():
    for nom in ("climate_card.yaml", "climate_popup.yaml"):
        texte = "\n".join(l for l in _lire("Tab5", "ui_components", nom).splitlines()
                          if not l.lstrip().startswith("#"))
        assert "0.5f" not in texte and "16.0f" not in texte and "30.0f" not in texte, nom
        assert texte.count("clim_consigne_suivante(id(clim_target_temp), -1)") == 1, nom
        assert texte.count("clim_consigne_suivante(id(clim_target_temp), +1)") == 1, nom
    # Sans climr : 16-30 et 0,5, comme l'arc du YAML.
    cartes = _lire("Tab5", "tab5_cards.cpp")
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
    cartes = _lire("Tab5", "tab5_cards.cpp")
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
    bloc = _chercher(actions, lambda d: "'demarrage_ha'] and clim != ''" in str(d.get("if", "")))
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
    p.variables_du_bloc("clim_reglages")
    branche = _chercher(p.corps["actions"], lambda d: d.get("alias") == "Clim : son état a changé")
    etape = next(e for e in branche["sequence"] if "reglages_changes" in (e.get("variables") or {}))
    p.ctx["reglages_changes"] = _rendre(p.env, etape["variables"]["reglages_changes"], p.ctx)
    assert p.modele(next(e for e in branche["sequence"] if "if" in e)["if"]) is attendu


# ─────────────────────────────────────────────────────────────────────────────
# Rendu : commandes de l'écran vers les vrais modes de l'appareil
# ─────────────────────────────────────────────────────────────────────────────

def _executer(p, sequence):
    """(action, données) que la séquence enverrait : variables, if/then/else, actions."""
    envoyees = []
    for etape in sequence:
        if "variables" in etape:
            for cle, valeur in etape["variables"].items():
                p.ctx[cle] = _rendre(p.env, valeur, p.ctx)
        elif "if" in etape:
            suite = etape["then"] if p.modele(etape["if"]) else etape.get("else", [])
            envoyees += _executer(p, suite)
        elif "action" in etape:
            envoyees.append((p.modele(etape["action"]), _rendre(p.env, etape.get("data", {}), p.ctx)))
    return envoyees


def _commande(clim, commande, valeur):
    p = Passage({"clim": "climate.test"}, [clim, METEO_C],
                _evenement("action", emplacement="clim", action=commande, valeur=valeur))
    p.variables_du_bloc("clim")
    alias, sequence = p.aiguillage()
    assert alias is None or alias.startswith("Clim : "), (commande, valeur, alias)
    return _executer(p, sequence)


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


@pytest.mark.parametrize("etat, attributs, commande, valeur, attendu", [
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
])
def test_commandes_vers_les_modes_de_l_appareil(etat, attributs, commande, valeur, attendu):
    assert _commande(_clim(etat, **attributs), commande, valeur) == attendu


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
