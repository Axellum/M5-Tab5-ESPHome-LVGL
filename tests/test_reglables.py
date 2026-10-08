# -*- coding: utf-8 -*-
"""Tuile − / + au choix (ADR-0033) : les boutons − / + de la carte clim de l'accueil
règlent l'appareil choisi dans une liste (la clim du blueprint, les appareils de la
section « Tuile − / + » du blueprint, le volume de la tablette).

Aucun compilateur ne relie le blueprint et le firmware ; ce fichier le fait :

- le modèle : types et icônes par défaut du firmware (tab5_reglables.cpp) = table du
  blueprint (types_reglables) = domaines du sélecteur ; huit appareils des deux côtés ;
  clés NVS propres au module ; une ligne de liste par entrée possible ;
- le blueprint rendu (harnais de tests/test_tuiles_blueprint.py) : liste blanche,
  bornes, définitions, états, déclencheurs, poussées, et l'aiguillage de « regler » et
  de « consigne » par domaine ;
- la carte : zone tactile du salon, − / + et valeur qui passent par le module."""
import os
import re

import pytest
import yaml

from tests import test_tuiles_blueprint as bp  # noqa: E402
from tests.commun import lire as _lire, sources

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


CPP = _lire("Tab5", "ecran", "tab5_reglables.cpp")
CUSTOM_H = _lire("Tab5", "ecran", "tab5_custom.h")


def _tableau(nom):
    m = re.search(rf"{nom}\[[^\]]*\] = \{{([^}}]*)\}};", CPP)
    assert m, f"{nom} introuvable dans tab5_reglables.cpp"
    return re.findall(r'"([^"]*)"', m.group(1))


def _constante(source, nom):
    m = re.search(rf"constexpr [^=;]*?\b{nom}\b\s*=\s*([^;]+);", source)
    assert m, f"constexpr {nom} introuvable"
    return int(m.group(1).strip(), 0)


# ─────────────────────────────────────────────────────────────────────────────
# Le modèle, des deux côtés
# ─────────────────────────────────────────────────────────────────────────────

def test_types_identiques_firmware_blueprint():
    types_bp = bp._blueprint()["variables"]["types_reglables"]
    assert _tableau("kTypes")[1:] == list(dict.fromkeys(types_bp.values())), \
        "kTypes (tab5_reglables.cpp) et types_reglables (blueprint) doivent avoir les mêmes types, dans l'ordre"


def test_domaines_du_selecteur_egaux_a_la_table():
    entree = bp._entrees(bp._blueprint())["reglables"]
    domaines = entree["selector"]["entity"]["filter"][0]["domain"]
    assert sorted(domaines) == sorted(bp._blueprint()["variables"]["types_reglables"])
    assert entree["default"] == [] and entree["selector"]["entity"]["multiple"] is True


def test_icones_par_defaut_dans_la_palette():
    codes = {i["code"] for i in yaml.safe_load(_lire("Tab5", "tuiles_icones.yaml"))["icones"]}
    defauts = _tableau("kIconesDefaut")
    assert len(defauts) == len(_tableau("kTypes"))
    assert set(defauts) <= codes, set(defauts) - codes
    # Le son de la tablette, dernière ligne de la liste : un haut-parleur (barré à 0 % ou
    # muet) et un nom qui dit ce qu'il règle (discussion #278, 07/10/2026).
    assert 'tuile_icone("enceinte", !muet && pct > 0.0f' in CPP and "enceinte" in codes
    assert 'tr("Son de la tablette")' in CPP and 'tr("Tablette")' not in CPP


def test_huit_appareils_des_deux_cotes():
    assert _constante(CPP, "kHA") == 8
    assert "ns.l | count < 8" in _lire("HomeAssistant_Config", "blueprints", "automation", "tab5",
                                       "tab5_emplacements.yaml")
    # Une ligne de liste par entrée possible : la clim, huit appareils, la tablette.
    lignes = _constante(CUSTOM_H, "kReglablesLignes")
    assert lignes == 8 + 2
    liste = _lire("Tab5", "ui_components", "reglables_liste.yaml")
    assert re.findall(r"vars: \{ n: (\d+) \}", liste) == [str(n) for n in range(lignes)]
    paquet = _lire("Tab5", "paquets", "tab5-reglables.yaml")
    for champ in ("ligne", "ligne_icone", "ligne_nom", "ligne_valeur"):
        assert len(re.findall(rf"u\.{champ}\[\d\] = ", paquet)) == lignes, champ


def test_cles_nvs_propres_au_module():
    """Deux enregistrements : le modèle (« regl ») et le choix (« rgch »). Une clé déjà
    prise par un autre module mélangerait leurs octets."""
    cles = {}
    for chemin in sources("*.cpp"):
        for k in re.findall(r"constexpr uint32_t kPrefKey\w* = (0x[0-9A-Fa-f]+);", _lire(chemin)):
            cles.setdefault(int(k, 16), []).append(chemin.name)
    for k in (0x7265676C, 0x72676368):
        assert cles.get(k) == ["tab5_reglables.cpp"], (hex(k), cles.get(k))


# ─────────────────────────────────────────────────────────────────────────────
# Le blueprint rendu
# ─────────────────────────────────────────────────────────────────────────────

def _maison():
    remplaces = {"media_player.tele"}
    return [e for e in bp._maison() if e.entity_id not in remplaces] + [
        bp.Etat("media_player.tele", "playing", "Salon", friendly_name="Télé du salon", volume_level=0.35),
        bp.Etat("media_player.barre", "off", friendly_name="Barre de son"),
        bp.Etat("climate.chambre", "heat", "Chambre", friendly_name="Radiateur chambre", min_temp=7, max_temp=35,
                target_temp_step=0.5, temperature=19),
        bp.Etat("water_heater.ballon", "eco", friendly_name="Ballon", min_temp=40, max_temp=65, temperature=55),
        bp.Etat("humidifier.deshu", "on", friendly_name="Déshumidificateur", min_humidity=30, max_humidity=80,
                humidity=50),
        bp.Etat("number.pac", "45", friendly_name="Consigne | PAC", min=20, max=60, step=0.5,
                unit_of_measurement="°C"),
        bp.Etat("input_number.bloque", "3", friendly_name="Bloqué", min=0, max=10, step=1),
    ]


REGLABLES = {
    "reglables": [
        "media_player.tele",       # r0 : la TV du blueprint (option t)
        "climate.clim",            # sautée : la clim du blueprint est déjà en tête
        "light.chevet",            # r1
        "sensor.temp_salon",       # sautée : pas de type
        "climate.chambre",         # r2
        "light.inexistante",       # sautée : n'existe pas
        "water_heater.ballon",     # r3
        "media_player.tele",       # sautée : déjà là
        "humidifier.deshu",        # r4
        "input_number.bloque",     # sautée : lecture seule (personnalisation)
        "fan.ventilo",             # r5
        "cover.store",             # r6
        "valve.arrosage",          # r7
        "number.pac",              # sautée : huit au plus
    ],
    "personnalisation": bp.ENTREES["personnalisation"] + [
        {"entite": "input_number.bloque", "comportement": "lecture_seule"},
    ],
}


def _passage(trigger=None, entrees=None, etats=None, tablettes=None):
    return bp._passage(trigger, {**bp.ENTREES, **REGLABLES} if entrees is None else entrees,
                       _maison() if etats is None else etats, tablettes)


def test_liste_blanche():
    p = _passage()
    assert [(r["cle"], r["e"], r["type"]) for r in p["reglables"]] == [
        ("r0", "media_player.tele", "son"),
        ("r1", "light.chevet", "lum"),
        ("r2", "climate.chambre", "cli"),
        ("r3", "water_heater.ballon", "eau"),
        ("r4", "humidifier.deshu", "hum"),
        ("r5", "fan.ventilo", "ven"),
        ("r6", "cover.store", "vol"),
        ("r7", "valve.arrosage", "vol"),
    ]
    # Rien de choisi : rien (et une seule entité, chaîne, comme une liste d'une).
    assert _passage(entrees=bp.ENTREES)["reglables"] == []
    assert [r["e"] for r in _passage(entrees={**bp.ENTREES, "reglables": "number.pac"})["reglables"]] == ["number.pac"]


def test_bornes_par_type():
    b = _passage()["reglables_bornes"]
    assert b == {
        "r0": [0, 100, 5, "%"],
        "r1": [0, 100, 10, "%"],
        "r2": [7.0, 35.0, 0.5, "°C"],
        "r3": [40.0, 65.0, 0.5, "°C"],
        "r4": [30.0, 80.0, 5, "%"],
        "r5": [0, 100, 10, "%"],
        "r6": [0, 100, 10, "%"],    # supported_features 15 : position réglable
        "r7": [0, 100, 100, "%"],   # vanne sans position : fermée ou ouverte
    }
    entrees = {**bp.ENTREES, "reglables": ["light.guirlande", "number.pac", "cover.volet_serre"]}
    b = _passage(entrees=entrees)["reglables_bornes"]
    # Lampe sans variateur, nombre (ses bornes, son pas, son unité), volet du package.
    assert b == {"r0": [0, 100, 100, "%"], "r1": [20.0, 60.0, 0.5, "°C"], "r2": [0, 100, 100, "%"]}


def test_bornes_avec_unites_enum_de_ha():
    """Dans un vrai HA, temperature_unit d'une météo et unit_of_measurement d'un nombre
    peuvent être des StrEnum (UnitOfTemperature). `| string` les laisse telles quelles :
    rangées dans la table, son texte n'est plus un littéral Python (« <UnitOfTemperature…> »)
    et HA garde la table en texte — « 'str object' has no attribute 'get' » à la connexion
    (job « Installation dans un HA neuf », 06/10/2026)."""
    from enum import StrEnum

    class UnitOfTemperature(StrEnum):
        CELSIUS = "°C"

    etats = _maison() + [
        bp.Etat("weather.maison", "sunny", friendly_name="Maison", temperature_unit=UnitOfTemperature.CELSIUS),
        bp.Etat("number.consigne_enum", "45", friendly_name="Consigne", min=20, max=60, step=0.5,
                unit_of_measurement=UnitOfTemperature.CELSIUS),
    ]
    entrees = {**bp.ENTREES, "reglables": ["climate.chambre", "number.consigne_enum"]}
    b = _passage(entrees=entrees, etats=etats)["reglables_bornes"]
    assert isinstance(b, dict), f"bornes restées en texte : {b!r}"
    assert b == {"r0": [7.0, 35.0, 0.5, "°C"], "r1": [20.0, 60.0, 0.5, "°C"]}
    assert all(type(v[3]) is str for v in b.values())


def test_definitions_avec_celles_des_tuiles():
    p = _passage()
    defs = bp._defs(p.definitions())
    reglables = [d for d in defs if re.fullmatch(r"r\d", d[0])]
    lien = {t["e"]: t["cle"] for t in reversed(p["tuiles"])}
    icone = bp._icone
    assert reglables == [
        ["r0", "son", icone(p.bp, domaine="media_player"), "t", lien["media_player.tele"], "0", "100", "5", "%",
         "Télé du salon"],
        ["r1", "lum", icone(p.bp, domaine="light"), "", lien["light.chevet"], "0", "100", "10", "%",
         "Lampe Chambre"],
        ["r2", "cli", icone(p.bp, domaine="climate"), "", "", "7.0", "35.0", "0.5", "°C", "Radiateur chambre"],
        ["r3", "eau", icone(p.bp, domaine="water_heater"), "", "", "40.0", "65.0", "0.5", "°C", "Ballon"],
        ["r4", "hum", icone(p.bp, domaine="humidifier"), "", "", "30.0", "80.0", "5", "%", "Déshumidificateur"],
        ["r5", "ven", icone(p.bp, domaine="fan"), "", lien["fan.ventilo"], "0", "100", "10", "%",
         "Ventilateur chambre"],
        ["r6", "vol", icone(p.bp, domaine="cover"), "", lien["cover.store"], "0", "100", "10", "%", "Store salon"],
        # La vanne est la sixième tuile de sa pièce (cinq au plus) : sans tuile, sans lien.
        ["r7", "vol", icone(p.bp, domaine="valve"), "", "", "0", "100", "100", "%", "Arrosage"],
    ]
    # Neuf champs après la clé, lus par lire_def() (tab5_reglables.cpp).
    assert all(len(d) == 10 for d in reglables) and "Champ f[9];" in CPP
    # Les tuiles ne changent pas.
    sans = bp._defs(bp._passage(None, bp.ENTREES, _maison()).definitions())
    assert [d for d in defs if not re.fullmatch(r"r\d", d[0])] == sans
    # Nom avec | et ; : remplacés comme ceux des tuiles.
    p = _passage(entrees={**bp.ENTREES, "reglables": ["number.pac"]})
    assert [d for d in bp._defs(p.definitions()) if d[0] == "r0"][0][-1] == "Consigne / PAC"


def _etats(p):
    p.variables_du_bloc("etats_reglables")
    return {e[0]: e[1:] for e in bp._defs(p["etats_reglables"])}


def test_etats_a_la_connexion():
    p = _passage(bp._evenement("connexion"))
    assert p["reglables_a_pousser"] == [f"r{n}" for n in range(8)]
    etats = _etats(p)
    assert etats == {
        "r0": ["playing", "35.0"],
        "r1": ["on", "50.0"],          # brightness 128 / 2,55
        "r2": ["heat", "19.0"],
        "r3": ["eco", "55.0"],
        "r4": ["on", "50.0"],
        "r5": ["off", "0"],            # ventilateur éteint
        "r6": ["open", "45.0"],
        "r7": ["closed", "0"],         # vanne sans position, fermée
    }
    # Lecteur éteint (sans volume), volet du package : « nan » et l'état du package.
    p = _passage(bp._evenement("connexion"),
                 entrees={**bp.ENTREES, "reglables": ["media_player.barre", "cover.volet_serre"]})
    assert _etats(p) == {"r0": ["off", "nan"], "r1": ["opening", "nan"]}


def test_declencheurs():
    triggers = [t for t in bp._blueprint()["triggers"] if t["id"].startswith("reglable")]
    assert [t["id"] for t in triggers] == ["reglable", "reglable_volume", "reglable_luminosite", "reglable_consigne",
                                           "reglable_humidite", "reglable_vitesse", "reglable_position"]
    assert all(t["entity_id"].nom == "reglables" for t in triggers)
    assert "attribute" not in triggers[0] and triggers[0]["to"] is None
    assert [t["attribute"] for t in triggers[1:]] == ["volume_level", "brightness", "temperature", "humidity",
                                                       "percentage", "current_position"]
    assert all(t["not_from"] == [None] and t["not_to"] == [None] for t in triggers[1:])


def test_un_changement_pousse_son_appareil_seulement():
    base = {e.entity_id: e for e in _maison()}
    avant = base["humidifier.deshu"]
    apres = bp.Etat("humidifier.deshu", "on", **{**avant.attributes, "humidity": 60})
    etats = [apres if e.entity_id == "humidifier.deshu" else e for e in _maison()]
    p = _passage(bp._declencheur("reglable_humidite", avant, apres), etats=etats)
    assert p["reglables_a_pousser"] == ["r4"] and p["tuiles_a_pousser"] == [] and p.conditions()
    assert _etats(p) == {"r4": ["on", "60.0"]}
    # Une entité qui apparaît : tout est redéfini.
    p = _passage(bp._declencheur("reglable", None, base["number.pac"]))
    assert p["redefinir"] is True and p["reglables_a_pousser"] == [f"r{n}" for n in range(8)]


def test_protocole_1_rien_de_la_tuile():
    p = _passage(bp._evenement("connexion"), tablettes=bp._tablette("3.1.0 (ESPHome 2026.9.0)"))
    assert p["protocole"] == 1
    texte = _lire("HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
    assert '- if: "{{ protocole == 2 and reglables_a_pousser | count > 0 }}"' in texte


# ─────────────────────────────────────────────────────────────────────────────
# Le blueprint rendu : commandes de l'écran
# ─────────────────────────────────────────────────────────────────────────────

def _regler(emplacement, action, valeur):
    """(alias, action, entité, données) de la commande, ou None si rien ne part."""
    p = _passage(bp._evenement("action", emplacement=emplacement, action=action, valeur=valeur))
    alias, sequence = p.aiguillage()
    if alias is None:
        return None
    if not alias.startswith("Tuile − / + : régler"):
        return alias, None, None, None
    for cle, v in sequence[0]["variables"].items():
        p.ctx[cle] = bp._rendre(p.env, v, p.ctx)
    for b in sequence[1]["choose"]:
        if p.modele(b["conditions"]):
            etape = b["sequence"][0]
            cible = etape["target"]["entity_id"]
            return (alias, p.modele(etape["action"]), p.modele(cible),
                    bp._rendre(p.env, etape.get("data", {}), p.ctx))
    return alias, None, None, None


@pytest.mark.parametrize("emplacement, valeur, attendu", [
    ("r0", "40", ("media_player.volume_set", "media_player.tele", {"volume_level": 0.4})),
    ("r0", "250", ("media_player.volume_set", "media_player.tele", {"volume_level": 1.0})),   # borné
    ("r1", "60", ("light.turn_on", "light.chevet", {"brightness_pct": 60})),
    ("r1", "0", ("light.turn_off", "light.chevet", {})),
    ("r3", "80", ("water_heater.set_temperature", "water_heater.ballon", {"temperature": 65.0})),  # borné
    ("r4", "55", ("humidifier.set_humidity", "humidifier.deshu", {"humidity": 55})),
    ("r5", "30", ("fan.set_percentage", "fan.ventilo", {"percentage": 30})),
    ("r5", "0", ("fan.turn_off", "fan.ventilo", {})),
    ("r6", "30", ("cover.set_cover_position", "cover.store", {"position": 30})),
    ("r7", "100", ("valve.open_valve", "valve.arrosage", {})),
    ("r7", "0", ("valve.close_valve", "valve.arrosage", {})),
])
def test_regler_par_domaine(emplacement, valeur, attendu):
    rendu = _regler(emplacement, "regler", valeur)
    assert rendu is not None and rendu[0].startswith("Tuile − / + : régler"), rendu
    assert rendu[1:] == attendu


def test_consigne_d_une_clim_de_la_liste():
    p = _passage(bp._evenement("action", emplacement="r2", action="consigne", valeur="20.5"))
    alias, _ = p.aiguillage()
    assert alias and alias.startswith("Clim : consigne") and p["clim_cible"] == "climate.chambre"
    # Une clim de la liste ne reçoit que sa consigne : ni « regler », ni « eteindre ».
    assert _regler("r2", "regler", "20") is None
    assert _regler("r2", "eteindre", "") is None


@pytest.mark.parametrize("emplacement, action, valeur", [
    ("r8", "regler", "20"),      # hors de la liste
    ("r9", "consigne", "20"),
    ("t00", "regler", "20"),     # une tuile ne connaît pas « regler »
    ("r0", "regler", "fort"),    # pas un nombre
    ("r0", "basculer", ""),      # une clé rN ne connaît que « regler » et « consigne »
])
def test_rien_hors_de_la_liste_blanche(emplacement, action, valeur):
    assert _regler(emplacement, action, valeur) is None


def test_volet_du_package_par_son_script():
    p = _passage(bp._evenement("action", emplacement="r0", action="regler", valeur="100"),
                 entrees={**bp.ENTREES, "reglables": ["cover.volet_serre"]})
    alias, sequence = p.aiguillage()
    assert alias and alias.startswith("Tuile − / + : régler")
    for cle, v in sequence[0]["variables"].items():
        p.ctx[cle] = bp._rendre(p.env, v, p.ctx)
    branche = next(b for b in sequence[1]["choose"] if p.modele(b["conditions"]))
    etape = branche["sequence"][0]
    assert etape["action"] == "script.turn_on" and etape["target"]["entity_id"] == "script.tab5_volet_action"
    assert bp._rendre(p.env, etape["data"], p.ctx) == {"variables": {"action": "open"}}


# ─────────────────────────────────────────────────────────────────────────────
# La carte clim et la liste
# ─────────────────────────────────────────────────────────────────────────────

CARTE = _lire("Tab5", "ui_components", "climate_card.yaml")


def test_moins_plus_et_valeur_passent_par_le_module():
    assert CARTE.count("lambda: 'return reglables_clim_choisie();'") == 2
    assert "reglables_pas(-1);" in CARTE and "reglables_pas(+1);" in CARTE
    assert "if (!reglables_clim_choisie()) {\n                      reglables_valeur_appui();" in CARTE
    # Clim choisie : le chemin d'avant (consigne optimiste, débounce, popup).
    assert CARTE.count("- script.execute: tab5_debounce_clim_temp") == 2


def _boite(bloc, champs=("x", "y", "width", "height")):
    boite = {}
    for c in champs:
        m = re.search(rf"\n\s+{c}: (-?\d+)", bloc)
        assert m, f"{c} introuvable"
        boite[c] = int(m.group(1))
    return boite


def test_zone_du_salon_sans_toucher_celle_de_la_serre():
    salon = _boite(CARTE.split("id: btn_reglables_liste", 1)[1].split("on_short_click", 1)[0])
    serre = _boite(CARTE.split("id: btn_serre_games", 1)[1].split("on_short_click", 1)[0])
    assert "reglables_liste_basculer();" in CARTE
    # Carte de 405 px : le salon à gauche (TOP_LEFT), la serre à droite (TOP_RIGHT).
    fin_salon = salon["x"] + salon["width"]
    debut_serre = 405 + serre["x"] - serre["width"]
    assert fin_salon <= debut_serre, (fin_salon, debut_serre)
    assert salon["y"] == serre["y"] and salon["height"] == serre["height"]


def test_liste_refermee_avec_les_popups_et_seule():
    scripts = _lire("Tab5", "paquets", "tab5-scripts.yaml")
    navigation = _lire("Tab5", "paquets", "tab5-navigation.yaml")
    assert "ModalRegistry::add(id(reglables_liste)," in navigation and "ModalRegistry::SUBWINDOW);" in navigation
    # Retour automatique : retour_auto_tick() (tab5_anim.cpp), lancé par l'interval de tab5-scripts.yaml.
    assert "if (idle >= UIIdle::POPUP_MS && reglables_liste_ouverte()) reglables_liste_fermer();" in _lire(
        "Tab5", "ecran", "tab5_anim.cpp")
    assert "retour_auto_tick(" in scripts
    # Le volume de la tablette repeint la tuile, quelle que soit sa source.
    volume = scripts.split("  - id: tab5_volume_apply\n", 1)[1].split("\n  - id:", 1)[0]
    assert "reglables_volume_tablette();" in volume


# ─────────────────────────────────────────────────────────────────────────────
# La démo et le rendu hors tablette
# ─────────────────────────────────────────────────────────────────────────────

import ecrans  # noqa: E402
import scenarios  # noqa: E402


def test_la_demo_suit_la_grammaire_du_firmware():
    assert scenarios.TYPES_REGLABLE == tuple(_tableau("kTypes")[1:])
    defs = [e.split("|") for e in scenarios.build_reglables_payload(scenarios.REGLABLES).split(";") if e]
    assert [d[0] for d in defs] == [f"r{n}" for n in range(len(scenarios.REGLABLES))]
    assert all(len(d) == 10 for d in defs)
    etats = [e.split("|") for e in scenarios.build_etats_reglables(scenarios.REGLABLES).split(";") if e]
    assert [e[0] for e in etats] == [d[0] for d in defs] and all(len(e) == 3 for e in etats)
    # Maison minimale : aucun appareil (la clim et la tablette seules).
    assert scenarios.reglables_de(scenarios.MAISON_MINIMALE) == ()


def test_le_rendu_touche_le_salon_et_les_lignes():
    zone = _boite(CARTE.split("id: btn_reglables_liste", 1)[1].split("on_short_click", 1)[0])
    x0, y0 = 855 + zone["x"], 110 + zone["y"]   # climate_card : x 855, y 110
    assert x0 <= ecrans.SALON[0] <= x0 + zone["width"] and y0 <= ecrans.SALON[1] <= y0 + zone["height"]
    liste = _lire("Tab5", "ui_components", "reglables_liste.yaml")
    panneau = _boite(liste.split("id: reglables_panneau", 1)[1], ("x", "y", "width"))
    ligne = int(re.search(r"\n  height: (\d+)", _lire("Tab5", "ui_components", "reglables_ligne.yaml")).group(1))
    assert ligne == 52 and "pad_all: 6" in liste and "pad_row: 2" in liste
    for k, (x, y) in enumerate(ecrans.LIGNES_REGLABLES):
        haut = panneau["y"] + 2 + 6 + k * (ligne + 2)
        assert panneau["x"] < x < panneau["x"] + panneau["width"] and haut < y < haut + ligne, k
    # L'enceinte de la démo est bien la ligne 4 (la clim, puis les appareils dans l'ordre).
    assert scenarios.REGLABLES[3].nom == "Enceinte"
    noms = [e.nom for e in ecrans.ECRANS]
    assert "accueil-tuile-liste" in noms and "accueil-tuile-enceinte" in noms
