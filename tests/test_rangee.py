# -*- coding: utf-8 -*-
"""Rangée sous l'horloge (ADR-0031) : jusqu'à trois lignes de quatre capteurs, plus la
ligne des plantes, choisies dans le blueprint « Tab5 — emplacements » et calées sur le
rotateur de la carte centrale.

Aucun compilateur ne relie les trois côtés ; ce fichier le fait :

- le calage : le rotateur attend 7,8 s, fait tourner la rangée, attend 0,2 s, puis fait
  avancer la carte centrale (un tour = 8 s = kTourCentralS du firmware) ; la durée du
  blueprint est en multiples de ce tour, son défaut celui du firmware ;
- les clés : hp, hd et hLI (sept champs) lues par le firmware, écrites par le blueprint
  et par la démo, avec les mêmes bornes (3 lignes, 4 éléments) ;
- le blueprint rendu (harnais de tests/test_tuiles_blueprint.py) : éléments, réglages,
  états, déclencheurs, et aucune commande possible sur un élément de la rangée ;
- le rendu hors tablette capture les lignes 2 et 3 de la démo, et revient à la première."""
import os
import re
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))
import test_tuiles_blueprint as bp  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "demo"))
sys.path.insert(0, os.path.join(REPO, "tools", "rendu"))
import ecrans  # noqa: E402
import scenarios  # noqa: E402


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _constante(source, nom):
    m = re.search(rf"constexpr [^=;]*?\b{nom}\b\s*=\s*([^;]+);", source)
    assert m, f"constexpr {nom} introuvable"
    return int(m.group(1).strip(), 0)


TUILES_CPP = _lire("Tab5", "tab5_tuiles.cpp")
RANGEE_CPP = _lire("Tab5", "tab5_rangee.cpp")
TOUR = _constante(TUILES_CPP, "kTourCentralS")


def _entrees():
    return bp._entrees(bp._blueprint())


# ─────────────────────────────────────────────────────────────────────────────
# Calage sur la carte centrale
# ─────────────────────────────────────────────────────────────────────────────

def test_la_rangee_tourne_juste_avant_la_carte_centrale():
    scripts = _lire("Tab5", "tab5-scripts.yaml")
    rotateur = scripts.split("  - id: tab5_central_rotator_auto\n", 1)[1].split("\n  - id:", 1)[0]
    etapes = re.findall(r"- delay: (\d+)ms|rangee_tour\(\);|advance_central_panel_rotator\(", rotateur)
    # [7800, rangee_tour, 200, advance] : findall rend '' pour les deux appels.
    assert etapes == ["7800", "", "200", ""], etapes
    assert rotateur.index("rangee_tour();") < rotateur.index("advance_central_panel_rotator(")
    assert (7800 + 200) == TOUR * 1000, "un tour de la rangée = une période de la carte centrale"
    # La rangée tourne écran allumé et sans fenêtre ouverte (comme la carte centrale),
    # sur toutes les pages des prévisions (l'horloge ne change pas de page).
    garde = rotateur.split("rangee_tour();", 1)[0].rsplit("- if:", 1)[1]
    assert "light.is_on: backlight" in garde and "any_popup_visible" in garde
    assert "forecast_page" not in garde
    # Un tour compté par appel ; la ligne change tous les rangee_tours() tours.
    tour = RANGEE_CPP.split("void rangee_tour()", 1)[1].split("\n}\n", 1)[0]
    assert "rangee_tours()" in tour and "suivante()" in tour


def test_duree_du_blueprint_en_tours_de_la_carte_centrale():
    duree = _entrees()["rangee_duree"]
    nombre = duree["selector"]["number"]
    assert nombre["step"] == TOUR and nombre["min"] == TOUR and nombre["unit_of_measurement"] == "s"
    assert nombre["max"] == TOUR * _constante(TUILES_CPP, "kToursMax")
    assert duree["default"] == TOUR * _constante(TUILES_CPP, "kToursDefaut")
    # Le firmware arrondit au tour le plus proche, borné (lire_entree, clé hd).
    assert "(s + kTourCentralS / 2) / kTourCentralS" in TUILES_CPP
    assert "std::max(1, std::min<int>(kToursMax, tours))" in TUILES_CPP


def test_place_des_plantes_par_defaut_la_premiere():
    plantes = _entrees()["rangee_plantes"]
    valeurs = [o["value"] for o in plantes["selector"]["select"]["options"]]
    assert valeurs == ["0", "1", "2", "masquees"] and plantes["default"] == "0"
    # Firmware : ModeleRangee{} met plantes à 0 (première), « hp|- » ou illisible : -1.
    assert "g.plantes = static_cast<int8_t>(ok ? place : -1);" in TUILES_CPP
    assert "rangee->tours = kToursDefaut;" in TUILES_CPP


# ─────────────────────────────────────────────────────────────────────────────
# Les clés, des deux côtés
# ─────────────────────────────────────────────────────────────────────────────

def test_bornes_identiques_firmware_blueprint_demo():
    assert _constante(TUILES_CPP, "kLignes") == _constante(RANGEE_CPP, "kPlaces") == scenarios.LIGNES_MAX == 3
    assert _constante(TUILES_CPP, "kElements") == _constante(RANGEE_CPP, "kParLigne") == scenarios.ELEMENTS_PAR_LIGNE == 4
    rangee = bp._blueprint()["variables"]["rangee"]
    assert "range(3)" in rangee and "ns.i < 4" in rangee
    assert [f"rangee_ligne_{n}" for n in (1, 2, 3)] == [k for k in _entrees() if k.startswith("rangee_ligne_")]
    # Quatre éléments par ligne : rangee_element.yaml inclus 2 panneaux × 4 fois.
    ui = _lire("Tab5", "ui_components", "rangee.yaml")
    assert sorted(re.findall(r'el: "([ab][0-3])"', ui)) == [f"{p}{i}" for p in "ab" for i in range(4)]


def test_le_firmware_lit_les_trois_cles():
    lire = TUILES_CPP.split("void lire_entree(", 1)[1].split("\n}\n", 1)[0]
    assert "Champ f[7];" in lire and "decouper(s, n, f, 7)" in lire
    assert "f[0].p[1] == 'p'" in lire and "f[0].p[1] == 'd'" in lire
    assert "copier_icone(e.classe, f[6].p, f[6].n)" in lire
    # Les états hLI passent par tuiles_etat_recu, comme les tRT.
    recu = TUILES_CPP.split("bool tuiles_etat_recu(", 1)[1].split("\n}\n", 1)[0]
    assert "cle[0] == 'h'" in recu and "rangee_element_change(r, t);" in recu
    # Préférence à part : le modèle des pièces garde sa taille (NVS depuis la 3.2).
    assert "make_preference<ModeleRangee>(kPrefKeyRangee)" in TUILES_CPP


def _champs(payload):
    return [e.split("|") for e in payload.split(";") if e]


def test_la_demo_suit_la_grammaire_du_firmware():
    payload = scenarios.build_rangee_payload(scenarios.RANGEE)
    entrees = _champs(payload)
    assert entrees[:2] == [["hp", "0"], ["hd", "32"]]
    for e in entrees[2:]:
        assert re.fullmatch(r"h[0-2][0-3]", e[0]) and len(e) == 7, e
        assert e[1] in scenarios.TYPES_TUILE and e[1] != "act", e
        assert re.fullmatch(r"[a-z0-9_]{0,15}", e[2]) and re.fullmatch(r"[a-z0-9_]{0,15}", e[6]), e
    # Trois lignes à l'écran dans la démo : les plantes et deux lignes de capteurs.
    assert len(scenarios.RANGEE.lignes) == 2 and scenarios.RANGEE.plantes == "0"
    etats = _champs(scenarios.build_etats_tuiles({}, None, scenarios.RANGEE))
    assert [e[0] for e in etats] == [e[0] for e in entrees[2:]] and all(len(e) == 4 for e in etats)
    # Maison minimale : les réglages seuls (défauts du firmware).
    assert scenarios.build_rangee_payload(scenarios.RANGEE_MINIMALE) == "hp|0;hd|32;"


# ─────────────────────────────────────────────────────────────────────────────
# Le blueprint rendu
# ─────────────────────────────────────────────────────────────────────────────

def _maison():
    return bp._maison() + [
        bp.Etat("sensor.hum_salon", "58", "Salon", friendly_name="Humidité salon",
                unit_of_measurement="%", device_class="humidity"),
        bp.Etat("sensor.solaire", "1450", friendly_name="Production | solaire", unit_of_measurement="W",
                device_class="power"),
        bp.Etat("sensor.batterie", "64", friendly_name="Batterie", unit_of_measurement="%",
                device_class="battery"),
    ]


RANGEE = {
    # Ligne 1 : six entités ; la scène (action) sautée, une absente sautée, quatre au plus.
    "rangee_ligne_1": ["sensor.temp_salon", "scene.soiree", "sensor.hum_salon", "sensor.inexistant",
                       "binary_sensor.porte", "switch.prise_pc", "light.chevet"],
    # Ligne 2 vide : sautée par l'écran (rien n'est envoyé pour elle).
    "rangee_ligne_3": ["sensor.solaire", "sensor.batterie", "person.alice"],
    "rangee_plantes": "masquees",
    "rangee_duree": 40,
}


def _passage(trigger=None, entrees=None, etats=None, tablettes=None):
    return bp._passage(trigger, {**bp.ENTREES, **RANGEE} if entrees is None else entrees,
                       _maison() if etats is None else etats, tablettes)


def test_definitions_de_la_rangee():
    p = _passage()
    defs = bp._defs(p.definitions())
    rangee = [d for d in defs if d[0].startswith("h")]
    icone = bp._icone
    assert rangee == [
        ["hp", "-"],
        ["hd", "40"],
        ["h00", "cap", icone(p.bp, domaine="sensor", classe="temperature"), "", "°C", "Température salon",
         "temperature"],
        ["h01", "cap", icone(p.bp, domaine="sensor", classe="humidity"), "", "%", "Humidité salon", "humidity"],
        ["h02", "bin", icone(p.bp, domaine="binary_sensor", classe="door"), "", "door", "Porte d'entrée", "door"],
        # La personnalisation vaut aussi ici (icône hors palette : défaut du domaine ;
        # « confirmer » donne k, sans effet sur l'écran : la rangée ne commande rien).
        ["h03", "int", icone(p.bp, "mdi:icone-hors-palette", "switch"), "k", "", "Prise PC", ""],
        ["h20", "cap", icone(p.bp, domaine="sensor", classe="power"), "", "W", "Production / solaire", "power"],
        ["h21", "cap", icone(p.bp, domaine="sensor", classe="battery"), "", "%", "Batterie", "battery"],
        ["h22", "bin", icone(p.bp, domaine="person"), "", "presence", "Alice", ""],
    ]
    # Les pièces d'abord, la rangée ensuite ; les tuiles inchangées par la rangée.
    assert defs.index(["hp", "-"]) > max(i for i, d in enumerate(defs) if d[0].startswith("t"))
    sans = bp._defs(bp._passage(None, bp.ENTREES, _maison()).definitions())
    assert [d for d in defs if not d[0].startswith("h")] == [d for d in sans if not d[0].startswith("h")]


@pytest.mark.parametrize("plantes, duree, attendu", [
    ("0", 32, ["hp|0", "hd|32"]),
    ("2", 8, ["hp|2", "hd|8"]),
    ("masquees", 120, ["hp|-", "hd|120"]),
    ("n'importe quoi", 32.0, ["hp|-", "hd|32"]),
])
def test_reglages_de_la_rangee(plantes, duree, attendu):
    p = _passage(entrees={"rangee_plantes": plantes, "rangee_duree": duree})
    assert p.definitions().split(";")[:2] == attendu


def test_etats_de_la_rangee_avec_ceux_des_tuiles():
    p = _passage(bp._evenement("connexion"))
    cles = [t["cle"] for t in p["tuiles"]] + ["h00", "h01", "h02", "h03", "h20", "h21", "h22"]
    assert p["tuiles_a_pousser"] == cles
    etats = {e[0]: e[1:] for e in bp._defs(p.etats_tuiles())}
    assert list(etats) == cles
    assert etats["h00"] == ["21.4", "21.4", ""] and etats["h20"] == ["1450", "1450.0", ""]
    assert etats["h02"] == ["off", "nan", ""] and etats["h22"] == ["home", "nan", ""]


def test_declencheurs_de_la_rangee():
    triggers = bp._blueprint()["triggers"]
    visibles = next(t for t in triggers if t["id"] == "piece_1")["to"]
    for n in (1, 2, 3):
        mes = [t for t in triggers if t["id"].startswith(f"rangee_{n}")]
        assert [t["id"] for t in mes] == [f"rangee_{n}", f"rangee_{n}_sortie"]
        assert all(t["entity_id"].nom == f"rangee_ligne_{n}" for t in mes)
        assert mes[0]["to"] == visibles and mes[1]["from"] == visibles and mes[1]["not_to"] == visibles
        assert not any("attribute" in t for t in mes), "l'état seulement : la rangée ne montre aucun attribut"


def test_un_changement_pousse_son_element_seulement():
    base = {e.entity_id: e for e in _maison()}
    porte = base["binary_sensor.porte"]
    ouverte = bp.Etat("binary_sensor.porte", "on", "Salon", **porte.attributes)
    etats = [ouverte if e.entity_id == "binary_sensor.porte" else e for e in _maison()]
    p = _passage(bp._declencheur("rangee_1", porte, ouverte), etats=etats)
    assert p["tuiles_a_pousser"] == ["h02"] and p.conditions()
    assert bp._defs(p.etats_tuiles()) == [["h02", "on", "nan", ""]]
    # La même porte est la tuile t14 : piece_2 la pousse, elle seule.
    assert _passage(bp._declencheur("piece_2", porte, ouverte), etats=etats)["tuiles_a_pousser"] == ["t14"]
    # Un capteur de la rangée part avec les mesures lentes (comme une tuile cap).
    for e in etats:
        if e.entity_id == "sensor.solaire":
            e.last_changed = e.last_updated = bp.MAINTENANT - bp.dt.timedelta(seconds=60)
    assert _passage({"id": "mesures", "platform": "time_pattern"}, etats=etats)["tuiles_a_pousser"] == ["h20"]
    # Une entité qui apparaît : tout est redéfini.
    p = _passage(bp._declencheur("rangee_3", None, base["sensor.batterie"]))
    assert p["redefinir"] is True and "h21" in p["tuiles_a_pousser"]


def test_aucune_commande_sur_la_rangee():
    """La rangée est hors de `tuiles` : la liste blanche des commandes ne la connaît pas."""
    for action in ("basculer", "allumer", "eteindre", "ouvrir", "position"):
        p = bp._passage(bp._evenement("action", emplacement="h03", action=action, valeur="50"),
                        {**bp.ENTREES, **RANGEE}, _maison())
        assert p.aiguillage() == (None, []), action
    assert "rangee" not in bp._blueprint()["variables"]["tuiles"]


def test_protocole_1_rien_de_la_rangee():
    p = _passage(bp._evenement("connexion"), tablettes=bp._tablette("3.1.0 (ESPHome 2026.9.0)"))
    assert p["protocole"] == 1
    # Calculé pour la trace, jamais envoyé : la garde protocole == 2 des deux actions.
    texte = _lire("HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
    assert texte.count('- if: "{{ protocole == 2 }}"') >= 1
    assert "protocole == 2 and (reglages_tuiles ~ etats_tuiles ~ etats_clims) != ''" in texte


# ─────────────────────────────────────────────────────────────────────────────
# Rendu hors tablette
# ─────────────────────────────────────────────────────────────────────────────

def test_le_rendu_capture_les_lignes_et_revient_a_la_premiere():
    par_nom = {e.nom: e for e in ecrans.ECRANS}
    lignes = 1 + len(scenarios.RANGEE.lignes)   # les plantes, puis les lignes de capteurs
    for n in (2, 3):
        ecran = par_nom[f"accueil-rangee-ligne-{n}"]
        appuis = [e for e in ecran.etapes + ecran.fermer if e == ecrans.Toucher(*ecrans.SOUS_HORLOGE)]
        assert len(ecran.etapes) == n - 1 and len(appuis) == lignes, ecran
    # La zone touchée est celle de btn_rangee (tab5-lvgl.yaml : TOP_MID, y 244, 401 × 70).
    lvgl = _lire("Tab5", "tab5-lvgl.yaml")
    bloc = lvgl.split("id: btn_rangee", 1)[1].split("\n          - ", 1)[0]
    y, largeur, hauteur = (int(re.search(rf"{c}: (\d+)", bloc).group(1)) for c in ("y", "width", "height"))
    x0 = 640 - largeur // 2
    assert x0 <= ecrans.SOUS_HORLOGE[0] <= x0 + largeur and y <= ecrans.SOUS_HORLOGE[1] <= y + hauteur
    # Le rendu arrête la rangée avec le rotateur central, sur sa première ligne.
    assert "rangee_recaler();" in _lire("Tab5", "rendu", "bouchons.yaml")
