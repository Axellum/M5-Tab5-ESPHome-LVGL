# -*- coding: utf-8 -*-
"""Climat de la pièce en mode HA (ADR-0040) : température, humidité et clim par pièce.

Aucun compilateur ne relie le blueprint, le package de l'historique, la démo et le
firmware. Ce fichier le fait :

- blueprint : les trois entrées facultatives de chaque section « Pièce n », la variable
  pieces_climat (domaine et existence vérifiés), les pièces à pousser selon le
  déclencheur, le payload « crpR / pR / cepR » rendu sur une maison inventée, les
  commandes « cpR » envoyées à la clim de la pièce et à elle seule ;
- clés : pR du popup Température, les mêmes dans le firmware, le blueprint et le package ;
- firmware : ce que le C++ lit (routage de la clé pR, clés crpR / cepR, emplacement cpR)
  et où l'écran se repeint (mode HA, glissement, thème, zones) ;
- démo : son payload découpé comme le firmware le découpe.

Identifiants d'entités inventés ; ce n'est pas Home Assistant (voir test_tuiles_blueprint)."""
import os
import re

import pytest

from tests.test_clim import DAIKIN, METEO_C, _executer
from tests.test_tuiles_blueprint import Etat, Passage, _blueprint, _declencheur, _defs, _evenement, _tablette
from tests.commun import lire as _lire, source

import scenarios  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACKAGE_HISTORIQUE = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_historique.yaml")

BUREAU = dict(hvac_modes=["off", "cool", "heat", "fan_only"], min_temp=16, max_temp=31, target_temp_step=1,
              friendly_name="Climatiseur du bureau", temperature=22, current_temperature=23.1,
              fan_mode="auto", fan_modes=["auto", "low", "high"])
# Pièce 2 (R = 1) : température, humidité et clim ; pièce 3 : une clim qui n'en est pas
# une (un capteur choisi par erreur) ; pièce 4 : une sonde supprimée de HA ; pièce 5 :
# une température seule.
ENTREES = {
    "clim": "climate.daikin",
    "piece_2_temperature": "sensor.bureau_t", "piece_2_humidite": "sensor.bureau_h", "piece_2_clim": "climate.bureau",
    "piece_3_temperature": "sensor.chambre_t", "piece_3_clim": "sensor.chambre_t",
    "piece_4_temperature": "sensor.disparu",
    "piece_5_temperature": "sensor.jardin_t", "piece_5_humidite": "sensor.jardin_h",
}


def _maison(*remplacees, age=3600):
    etats = {e.entity_id: e for e in (
        Etat("climate.daikin", "cool", **DAIKIN),
        Etat("climate.bureau", "cool", age=age, **BUREAU),
        Etat("sensor.bureau_t", "22.8", age=age, device_class="temperature", unit_of_measurement="°C"),
        Etat("sensor.bureau_h", "45", age=age, device_class="humidity", unit_of_measurement="%"),
        Etat("sensor.chambre_t", "19.25", device_class="temperature", unit_of_measurement="°C"),
        Etat("sensor.jardin_t", "unavailable"),
        Etat("sensor.jardin_h", "61", device_class="humidity", unit_of_measurement="%"),
    )}
    etats.update({e.entity_id: e for e in remplacees})
    return list(etats.values()) + [METEO_C]


def _climat(p):
    """(pièces à pousser, payload du climat des pièces) d'un passage."""
    p.variables_du_bloc("reglages_clims")
    return p["pieces_climat_a_pousser"], p.variables_du_bloc("climat_pieces")


# ─── Blueprint : entrées et variable ─────────────────────────────────────────

def test_entrees_facultatives_de_chaque_piece():
    sections = _blueprint()["blueprint"]["input"]
    for n in range(1, 6):
        entrees = sections[f"piece_{n}"]["input"]
        for nom, domaine, classe in (("temperature", "sensor", "temperature"), ("humidite", "sensor", "humidity"),
                                     ("clim", "climate", None)):
            e = entrees[f"piece_{n}_{nom}"]
            assert e["default"] == [], f"piece_{n}_{nom} : facultative, vide par défaut"
            filtre = e["selector"]["entity"]["filter"]
            assert filtre == [{"domain": domaine, **({"device_class": classe} if classe else {})}], nom
            assert "multiple" not in e["selector"]["entity"], "une seule entité"


def test_pieces_climat_garde_les_entites_valides():
    p = Passage(ENTREES, _maison(), _evenement("connexion"))
    assert p["pieces_climat"] == [
        {"r": 0, "cle": "cp0", "temperature": "", "humidite": "", "clim": ""},
        {"r": 1, "cle": "cp1", "temperature": "sensor.bureau_t", "humidite": "sensor.bureau_h", "clim": "climate.bureau"},
        # Un capteur choisi comme clim : pas une clim.
        {"r": 2, "cle": "cp2", "temperature": "sensor.chambre_t", "humidite": "", "clim": ""},
        # Sonde supprimée de HA : rien.
        {"r": 3, "cle": "cp3", "temperature": "", "humidite": "", "clim": ""},
        {"r": 4, "cle": "cp4", "temperature": "sensor.jardin_t", "humidite": "sensor.jardin_h", "clim": ""},
    ]


# ─── Blueprint : poussées ────────────────────────────────────────────────────

def test_toutes_les_pieces_a_la_connexion():
    p = Passage(ENTREES, _maison(), _evenement("connexion"))
    pieces, payload = _climat(p)
    assert pieces == [0, 1, 2, 3, 4]
    assert _defs(payload) == [
        ["p0", "", "", "0"],
        # Réglages, puis la pièce, puis l'état de sa clim.
        ["crp1", "16.0", "31.0", "1.0", "°C", "chfq", "Climatiseur du bureau"],
        ["p1", "22.8", "45.0", "1"],
        ["cep1", "22.0", "23.1", "cool", "none", "auto", "stop"],
        ["p2", "19.25", "", "0"],
        ["p3", "", "", "0"],
        # Sonde indisponible : nan (« -- ° » à l'écran), l'humidité reste.
        ["p4", "nan", "61.0", "0"],
    ]
    bloc = [b for b in p.corps["actions"] if "climat_pieces" in str(b.get("then", ""))][0]
    assert p.modele(bloc["if"]) is True
    envoi = bloc["then"][1]
    assert p.modele(envoi["if"]) is True
    assert envoi["then"][0]["action"].endswith("_tab5_maj_emplacements")
    assert envoi["then"][0]["data"] == {"payload": "{{ climat_pieces }}"}


def test_rien_pour_un_firmware_d_avant():
    p =Passage(ENTREES, _maison(), _evenement("connexion"), _tablette("3.1.0 (ESPHome 2026.9.0)"))
    bloc = [b for b in p.corps["actions"] if "climat_pieces" in str(b.get("then", ""))][0]
    assert p.modele(bloc["if"]) is False


@pytest.mark.parametrize("id_, avant, apres, crp", [
    # Changement de mode : la pièce et l'état de sa clim, pas ses réglages.
    ("piece_2_clim", ("cool", {}), ("heat", {}), False),
    # Consigne : tout de suite, les − / + en partent.
    ("piece_2_clim_consigne", ("cool", {}), ("cool", {"temperature": 24}), False),
    # Retour de panne : ses réglages aussi.
    ("piece_2_clim", ("unavailable", None), ("cool", {}), True),
])
def test_la_clim_de_la_piece_change(id_, avant, apres, crp):
    if avant[1] is None:
        etat_avant = Etat("climate.bureau", avant[0], friendly_name="Climatiseur du bureau")
    else:
        etat_avant = Etat("climate.bureau", avant[0], **{**BUREAU, **avant[1]})
    etat_apres = Etat("climate.bureau", apres[0], **{**BUREAU, **apres[1]})
    p = Passage(ENTREES, _maison(etat_apres), _declencheur(id_, etat_avant, etat_apres))
    assert p.conditions(), "le passage continue"
    pieces, payload = _climat(p)
    assert pieces == [1]
    cles = [e[0] for e in _defs(payload)]
    assert cles == (["crp1", "p1", "cep1"] if crp else ["p1", "cep1"])
    cep = _defs(payload)[-1]
    assert cep[1] == str(float(apres[1].get("temperature", BUREAU["temperature"]))) and cep[3] == apres[0]


def test_une_autre_entite_ne_pousse_aucune_piece():
    """Le déclencheur piece_2_clim d'une entité qui n'est pas la clim de la pièce 2 (liste
    changée entre deux passages) : rien."""
    autre = Etat("climate.daikin", "heat", **DAIKIN)
    p = Passage(ENTREES, _maison(), _declencheur("piece_2_clim", Etat("climate.daikin", "cool", **DAIKIN), autre))
    assert p["pieces_climat_a_pousser"] == []


def test_avec_les_mesures_lentes():
    # Sonde, humidité et clim du bureau changées il y a 100 s : la pièce 2 part.
    p = Passage(ENTREES, _maison(age=100), {"id": "mesures", "platform": "time_pattern"})
    pieces, payload = _climat(p)
    assert pieces == [1] and [e[0] for e in _defs(payload)] == ["p1", "cep1"]
    # Rien de changé depuis 5 minutes : rien.
    p = Passage(ENTREES, _maison(), {"id": "mesures", "platform": "time_pattern"})
    assert p["pieces_climat_a_pousser"] == []


def test_la_condition_laisse_passer_le_climat():
    texte = _lire(os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml"))
    assert "or pieces_climat_a_pousser | count > 0" in texte


# ─── Blueprint : commandes de la clim de la pièce ────────────────────────────

def _commande(emplacement, commande, valeur, etats=None, cibles=None):
    p = Passage(ENTREES, etats or _maison(), _evenement("action", emplacement=emplacement, action=commande,
                                                        valeur=valeur))
    p.variables_du_bloc("clim")
    alias, sequence = p.aiguillage()
    assert alias is None or alias.startswith("Clim : "), (emplacement, commande, alias)
    return _executer(p, sequence, cibles)


@pytest.mark.parametrize("commande, valeur, attendu", [
    ("consigne", "21", [("climate.set_temperature", {"temperature": 21.0, "hvac_mode": "cool"})]),
    ("mode", "heat", [("climate.set_hvac_mode", {"hvac_mode": "heat"})]),
    ("eteindre", "", [("climate.turn_off", {})]),
    # Le mode silencieux de l'écran traduit dans ceux de l'appareil (ADR-0026).
    ("ventilation", "quiet", [("climate.set_fan_mode", {"fan_mode": "low"})]),
])
def test_la_clim_de_la_piece_et_elle_seule(commande, valeur, attendu):
    cibles = []
    assert _commande("cp1", commande, valeur, cibles=cibles) == attendu
    assert set(cibles) <= {"climate.bureau"}, "la clim de la pièce, jamais celle du blueprint"


def test_liste_blanche_des_clims_de_piece():
    # Pièce sans clim, capteur choisi comme clim, clé hors des cinq pièces, ancienne clé
    # « pR » (tout éteindre, une autre branche).
    for cle in ("cp0", "cp2", "cp5", "cp", "crp1", "cep1"):
        assert _commande(cle, "consigne", "21") == [], cle


# ─── Clés du popup Température ───────────────────────────────────────────────

def test_cles_de_l_historique_partout_les_memes():
    pieces = tuple(f"p{r}" for r in range(5))
    cpp = _lire(source("tab5_historique.cpp"))
    cles = re.findall(r'"(\w+)"', re.search(r"kCles\[NB_CLES\] = \{([^}]*)\}", cpp).group(1))
    assert tuple(cles) == ("salon", "serre") + pieces and "NB_CLES = 7" in cpp
    piece = _lire(source("tab5_piece_climat.cpp"))
    assert tuple(re.findall(r'"(p\d)"', re.search(r"kClesHistorique\[kPieces\] = \{([^}]*)\}", piece).group(1))) == pieces
    liste = "['salon', 'serre', 'p0', 'p1', 'p2', 'p3', 'p4']"
    assert liste in _lire(PACKAGE_HISTORIQUE)
    assert liste in _lire(os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5",
                                       "tab5_emplacements.yaml"))


# ─── Firmware ────────────────────────────────────────────────────────────────

def _fonction(texte, nom):
    debut = re.search(rf"^[\w:<>*& ]+\b{nom}\([^)]*\)\s*\{{", texte, re.M)
    assert debut, nom
    i, profondeur = debut.end(), 1
    while profondeur:
        profondeur += {"{": 1, "}": -1}.get(texte[i], 0)
        i += 1
    return texte[debut.end():i - 1]


def test_routage_des_cles():
    zones = _lire(source("tab5_zones.cpp"))
    appliquer = _fonction(zones, "emplacements_appliquer")
    assert "piece_climat_recu(" in appliquer
    assert appliquer.index("reglables_etat_recu(") < appliquer.index("piece_climat_recu(")
    piece = _fonction(_lire(source("tab5_piece_climat.cpp")), "piece_climat_recu")
    assert "if (n_cle != 2 || cle[0] != 'p' || cle[1] < '0' || cle[1] >= '0' + kPieces) return false;" in piece
    assert "champs_decouper(reste, n_reste, '|', f, 3)" in piece
    clim = _lire(source("tab5_clim.cpp"))
    recu = _fonction(clim, "clim_tuile_recu")
    assert "const bool piece = cle[2] == kPrefixePiece;" in recu and "kCasesTuiles + r" in recu
    assert "constexpr int kCases = kCasesTuiles + kPieces;" in clim
    # Emplacement des commandes : « cpR », écrit une seule fois.
    assert "c.s[0] = 'c';\n    c.s[1] = 'p';" in _lire(source("tab5_modele_ha.h")).replace("\r\n", "\n")


def test_repeint_quand_la_piece_change():
    tuiles = _lire(source("tab5_tuiles.cpp"))
    for f in ("tuiles_mode_ha", "tuiles_swipe_ha"):
        corps = _fonction(tuiles, f)
        assert "accueil_temperatures_ui();" in corps and "reglables_clim_changee();" in corps, f
    theme = _lire(source("tab5_theme.cpp"))
    assert theme.index("cartes_rejouer_theme();") < theme.index("accueil_temperatures_ui();"), \
        "la pièce après les cartes : update_temp_ui du salon ne repeint pas par-dessus"
    assert "accueil_temperatures_ui();" in _fonction(_lire(source("tab5_zones.cpp")), "zones_apply_ui")
    capteurs = _lire(os.path.join(REPO, "Tab5", "paquets", "tab5-sensors-domotique.yaml"))
    for f in ("accueil_salon_temperature(x)", "accueil_serre_temperature(x)", "accueil_salon_humidite(x)"):
        assert f in capteurs, f


def test_la_tuile_moins_plus_suit_la_clim_de_la_piece():
    reglables = _lire(source("tab5_reglables.cpp"))
    assert "piece_climat_clim()" in _fonction(reglables, "tuile_visible")
    assert "clim_piece_pas(" in _fonction(reglables, "reglables_pas")
    assert "clim_afficher_piece(" in _fonction(reglables, "reglables_valeur_appui")


# ─── Démo ────────────────────────────────────────────────────────────────────

def test_payload_de_la_demo_lu_comme_le_firmware():
    payload = scenarios.build_climat_pieces(scenarios.PIECES)
    vues = set()
    for champs in _defs(payload):
        cle = champs[0]
        if re.fullmatch(r"p[0-4]", cle):
            assert len(champs) == 4 and champs[3] in ("0", "1"), champs
            vues.add(cle)
        elif re.fullmatch(r"crp[0-4]", cle):
            assert len(champs) - 1 >= 5, "lire_reglages : cinq champs au moins"
        else:
            assert re.fullmatch(r"cep[0-4]", cle) and len(champs) == 7, champs
    assert vues == {f"p{r}" for r in range(5)}
    # Une pièce de la démo avec les trois, une avec la température seule.
    assert {r for r, p in scenarios.PIECES.items() if p.climat} == {1, 3}
