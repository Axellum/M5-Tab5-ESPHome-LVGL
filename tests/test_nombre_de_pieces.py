# -*- coding: utf-8 -*-
"""Nombre de pièces et vingt éléments par pièce (ADR-0066, lot C), côté Home Assistant.

Le blueprint « Tab5 — emplacements » prend une entrée « Nombre de pièces » (1 à 10, 5 par
défaut), envoie « pn|N; » en tête des définitions, jamais les pièces R ≥ N, et jusqu'à vingt
éléments par pièce (clés tRT, T = 0-9 puis a-j) dans les définitions, les états, les clims
des tuiles (crRT / ceRT) et la liste blanche des commandes (esphome.tab5_action).

Compatibilité, prouvée ici sur les VRAIS modèles Jinja (banc de tests/test_tuiles_blueprint.py) :
- une maison de 5 pièces de 5 éléments, « Nombre de pièces » laissé au défaut : chaque charge
  utile est celle d'avant l'ADR-0066, à « pn|5; » près en tête des définitions ;
- une maison plus grande (plus de cinq éléments, plus de cinq pièces) : ce qu'un firmware
  3.9 lit (ses clés à un chiffre de 0 à 4, LU_PAR_39) est exactement ce qu'il lisait.
REFERENCE : les charges rendues le 18/10/2026 par le blueprint de main (fe80f1eb, avant
l'ADR-0066) avec ce banc et ces maisons — `git show fe80f1eb:<blueprint>` dans un fichier,
BLUEPRINT du banc pointé dessus, puis charges() de chaque maison.
"""
import re

import pytest

from tests.commun import lire as _lire
from tests.test_clim import DAIKIN, METEO_C
from tests.test_tuiles_blueprint import (ENTREES, Etat, Passage, _action_rendue, _blueprint, _declencheur, _defs,
                                         _evenement, _maison, _rendre)

LETTRES = "0123456789abcdefghij"


# ─────────────────────────────────────────────────────────────────────────────
# Maisons de test
# ─────────────────────────────────────────────────────────────────────────────

def _etats():
    return _maison() + [
        Etat("climate.pac", "heat", "Bureau", **{**DAIKIN, "friendly_name": "PAC bureau"}),
        METEO_C,
    ]


# Cinq pièces de cinq éléments, climat des pièces, une clim de tuile (t24, sans m), la
# tuile − / + : tout ce qui part avec les tuiles.
MAISON_5X5 = {
    "piece_1_tuiles": ["light.chevet", "light.plafond", "light.guirlande", "switch.prise_pc", "cover.store"],
    "piece_2_nom": "Salon",
    "piece_2_tuiles": ["media_player.tele", "cover.store", "sensor.temp_salon", "climate.clim", "binary_sensor.porte"],
    "piece_2_temperature": "sensor.temp_salon",
    "piece_2_clim": "climate.clim",
    "piece_3_tuiles": ["light.chevet", "light.plafond", "fan.ventilo", "light.bureau", "climate.pac"],
    "piece_4_tuiles": ["person.alice", "lock.serrure", "scene.soiree", "script.cafe", "button.sonnette"],
    "piece_5_nom": "  Garage  ",
    "piece_5_tuiles": ["sensor.conso", "cover.volet_serre", "valve.arrosage", "switch.prise_pc", "light.guirlande"],
    "piece_5_temperature": "sensor.temp_salon",
    "reglables": ["cover.store", "light.bureau"],
    "personnalisation": ENTREES["personnalisation"],
    "tv": "media_player.tele",
    "clim": "climate.clim",
    "volet": "cover.volet_serre",
}

# Plus de cinq éléments : la pièce 1 en a sept (température du salon et télé en sixième et
# septième : t05, t06), la pièce 4 six (la vanne : t35). La télé est aussi t10 : le lien de
# la tuile − / + reste t10, qu'un firmware 3.9 sait lire, et pas t06.
MAISON_GRANDE = {
    **ENTREES,
    "piece_1_tuiles": ENTREES["piece_1_tuiles"] + ["media_player.tele"],
    "piece_2_temperature": "sensor.temp_salon",
    "piece_2_clim": "climate.clim",
    "piece_3_tuiles": ENTREES["piece_3_tuiles"] + ["climate.pac"],
    "reglables": ["media_player.tele", "cover.store"],
}

# Les pièces 6 et 7 en plus (pour « Nombre de pièces » = 7) : un 3.9 ne les lit pas.
PIECES_6_7 = {
    "piece_6_nom": "Atelier",
    "piece_6_tuiles": ["light.bureau", "switch.prise_pc"],
    "piece_6_temperature": "sensor.temp_salon",
    "piece_7_tuiles": ["climate.pac", "lock.serrure"],
}


def _passage(entrees, trigger=None):
    return Passage(entrees, _etats(), trigger or _evenement("connexion"))


def charges(p):
    """Ce que la connexion envoie à la tablette, charge par charge : définitions
    (tab5_maj_tuiles), états des tuiles avec les clims des tuiles, tuile − / + et climat des
    pièces (tab5_maj_emplacements)."""
    p.etats_tuiles()
    return {
        "definitions": p.definitions(),
        "tuiles": p["reglages_tuiles"] + p["etats_tuiles"] + p["etats_clims"],
        "reglables": p.variables_du_bloc("etats_reglables"),
        "climat": p.variables_du_bloc("climat_pieces"),
    }


# Clés qu'un firmware 3.9 lit (Tab5/ecran/tab5_tuiles.cpp lire_entree / tuiles_etat_recu,
# chiffre_0_4 ; tab5_clim.cpp clim_tuile_recu ; tab5_parse.cpp, climat des pièces « p » +
# chiffre < 5 ; tab5_reglables.cpp, r0 à r7) : toute autre entrée lui est inconnue, ignorée.
LU_PAR_39 = re.compile(r"(p[0-4]|t[0-4][0-4]|[hn][pd]|[hn][0-4][0-4]|r[0-7]|c[re][0-4][0-4]|c[re]p[0-4])")


def lu_par_39(charge):
    return [e for e in _defs(charge) if LU_PAR_39.fullmatch(e[0])]


# ─────────────────────────────────────────────────────────────────────────────
# Référence : les charges d'avant l'ADR-0066 (voir l'en-tête)
# ─────────────────────────────────────────────────────────────────────────────

REFERENCE = {
    "MAISON_5X5": {
        "definitions": (
            "p0|",
            "t00|lum|ampoule|dc||Lampe Chambre",
            "t01|lum|ampoule|d||Plafonnier de la chambre",
            "t02|lum|led|o||Sapin / Noël, 2026",
            "t03|int|interrupteur|k||Prise PC",
            "t04|vol|volet|||Store salon",
            "p1|Salon",
            "t10|med|tv|t||Télé",
            "t11|vol|volet|||Store",
            "t12|cap|thermometre||°C|Température",
            "t13|cli|clim|m||Clim",
            "t14|bin|porte||door|Porte d'entrée",
            "p2|",
            "t20|lum|ampoule|dc||Lampe Chambre",
            "t21|lum|ampoule|d||Plafonnier de la chambre",
            "t22|int|ventilateur|||Ventilateur chambre",
            "t23|lum|ampoule|d||Lampe bureau",
            "t24|cli|clim|||PAC bureau",
            "p3|",
            "t30|bin|presence||presence|Alice",
            "t31|bin|serrure||lock|Serrure",
            "t32|act|scene|||Soirée",
            "t33|act|script|||Café",
            "t34|act|bouton|||Sonnette",
            "p4|Garage",
            "t40|cap|mesure|r|kWh/100|Conso / voiture, électrique",
            "t41|vol|rideau|||Volet serre",
            "t42|vol|vanne|||Arrosage",
            "t43|int|interrupteur|k||Prise PC",
            "t44|lum|led|o||Sapin / Noël, 2026",
            "hp|0",
            "hd|32",
            "np|0",
            "nd|32",
            "r0|vol|volet||t04|0|100|10|%|Store salon",
            "r1|lum|ampoule||t23|0|100|10|%|Lampe bureau",
        ),
        "tuiles": (
            "cr24|18.0|32.0|0.5|°C|chdfebqsw|PAC bureau",
            "t00|on|128.0|FFC864",
            "t01|off|nan|",
            "t02|on|nan|",
            "t03|on|nan|",
            "t04|open|45.0|",
            "t10|playing|nan|",
            "t11|open|45.0|",
            "t12|21.4|21.4|",
            "t13|heat_cool|22.5|",
            "t14|off|nan|",
            "t20|on|128.0|FFC864",
            "t21|off|nan|",
            "t22|off|nan|",
            "t23|on|255.0|",
            "t24|heat|23.0|",
            "t30|home|nan|",
            "t31|locked|nan|",
            "t32|2026-09-27T20:00:00+00:00|nan|",
            "t33|off|nan|",
            "t34|unknown|nan|",
            "t40|3.2|3.2|",
            "t41|opening|nan|",
            "t42|closed|nan|",
            "t43|on|nan|",
            "t44|on|nan|",
            "ce24|23.0|23.0|heat|none|auto|stop",
        ),
        "reglables": (
            "r0|open|45.0",
            "r1|on|100.0",
        ),
        "climat": (
            "p0|||0",
            "crp1|16.0|30.0|0.5|°C||Clim salon",
            "p1|21.4||1",
            "cep1|nan|22.5|heat_cool|none|auto|stop",
            "p2|||0",
            "p3|||0",
            "p4|21.4||0",
        ),
    },
    "MAISON_GRANDE": {
        "definitions": (
            "p0|",
            "t00|lum|ampoule|dc||Lampe Chambre",
            "t01|lum|ampoule|d||Plafonnier de la chambre",
            "t02|lum|led|o||Sapin / Noël, 2026",
            "t03|int|interrupteur|k||Prise PC",
            "t04|vol|volet|||Store salon",
            "p1|Salon",
            "t10|med|tv|t||Télé",
            "t11|vol|volet|||Store",
            "t12|cap|thermometre||°C|Température",
            "t13|cli|clim|m||Clim",
            "t14|bin|porte||door|Porte d'entrée",
            "p2|",
            "t20|lum|ampoule|dc||Lampe Chambre",
            "t21|lum|ampoule|d||Plafonnier de la chambre",
            "t22|int|ventilateur|||Ventilateur chambre",
            "t23|cli|clim|||PAC bureau",
            "p3|",
            "t30|bin|presence||presence|Alice",
            "t31|bin|serrure||lock|Serrure",
            "t32|act|scene|||Soirée",
            "t33|act|script|||Café",
            "t34|act|bouton|||Sonnette",
            "p4|Garage",
            "t40|cap|mesure|r|kWh/100|Conso / voiture, électrique",
            "t41|vol|rideau|||Volet serre",
            "hp|0",
            "hd|32",
            "np|0",
            "nd|32",
            "r0|son|tv|t|t10|0|100|5|%|Télé du salon",
            "r1|vol|volet||t04|0|100|10|%|Store salon",
        ),
        "tuiles": (
            "cr23|18.0|32.0|0.5|°C|chdfebqsw|PAC bureau",
            "t00|on|128.0|FFC864",
            "t01|off|nan|",
            "t02|on|nan|",
            "t03|on|nan|",
            "t04|open|45.0|",
            "t10|playing|nan|",
            "t11|open|45.0|",
            "t12|21.4|21.4|",
            "t13|heat_cool|22.5|",
            "t14|off|nan|",
            "t20|on|128.0|FFC864",
            "t21|off|nan|",
            "t22|off|nan|",
            "t23|heat|23.0|",
            "t30|home|nan|",
            "t31|locked|nan|",
            "t32|2026-09-27T20:00:00+00:00|nan|",
            "t33|off|nan|",
            "t34|unknown|nan|",
            "t40|3.2|3.2|",
            "t41|opening|nan|",
            "ce23|23.0|23.0|heat|none|auto|stop",
        ),
        "reglables": (
            "r0|playing|nan",
            "r1|open|45.0",
        ),
        "climat": (
            "p0|||0",
            "crp1|16.0|30.0|0.5|°C||Clim salon",
            "p1|21.4||1",
            "cep1|nan|22.5|heat_cool|none|auto|stop",
            "p2|||0",
            "p3|||0",
            "p4|||0",
        ),
    },
}


def _charge(entrees):
    return ";".join(entrees) + ";"


# ─────────────────────────────────────────────────────────────────────────────
# Compatibilité
# ─────────────────────────────────────────────────────────────────────────────

def test_maison_5x5_les_memes_charges_qu_avant_a_pn_pres():
    """« Nombre de pièces » au défaut, cinq éléments par pièce : rien ne change que
    « pn|5; » en tête des définitions (un firmware 3.9 l'ignore : ni « p » + chiffre, ni
    lettre de zone)."""
    avant = {cle: _charge(v) for cle, v in REFERENCE["MAISON_5X5"].items()}
    nouvelles = charges(_passage(MAISON_5X5))
    assert nouvelles["definitions"] == "pn|5;" + avant.pop("definitions")
    nouvelles.pop("definitions")
    assert nouvelles == avant
    # 5 écrit dans l'automatisation (l'assistant l'écrit) : la même chose.
    assert charges(_passage({**MAISON_5X5, "nombre_pieces": 5})) == charges(_passage(MAISON_5X5))


@pytest.mark.parametrize("nombre, en_plus", [(5, {}), (5, PIECES_6_7), (7, PIECES_6_7), (10, PIECES_6_7)])
def test_un_firmware_39_lit_ce_qu_il_lisait(nombre, en_plus):
    """Plus de cinq éléments, plus de cinq pièces : un firmware 3.9 lit exactement les mêmes
    entrées qu'avant (les cinq premiers éléments des cinq premières pièces, tuile − / + et
    climat compris) ; il ignore le reste."""
    nouvelles = charges(_passage({**MAISON_GRANDE, **en_plus, "nombre_pieces": nombre}))
    for cle, avant in REFERENCE["MAISON_GRANDE"].items():
        assert lu_par_39(_charge(avant)) == _defs(_charge(avant)), f"{cle} : la référence a tout lisible"
        assert lu_par_39(nouvelles[cle]) == lu_par_39(_charge(avant)), cle
    # Le reste est bien parti, pour un firmware qui le lit.
    defs = _defs(nouvelles["definitions"])
    assert defs[0] == ["pn", str(nombre)]
    cles = {e[0] for e in defs}
    assert {"t05", "t06", "t35"} <= cles, "sixième et septième éléments"
    assert ({"p5", "t50", "t51", "t60", "t61"} <= cles) is (nombre >= 7)
    assert nombre >= 7 or not {"p5", "t50", "p6", "t60"} & cles


# ─────────────────────────────────────────────────────────────────────────────
# Nombre de pièces
# ─────────────────────────────────────────────────────────────────────────────

@pytest.mark.parametrize("valeur, attendu", [
    (None, 5), (1, 1), (0, 1), (-3, 1), (7, 7), (7.0, 7), ("8", 8), (10, 10), (12, 10), ("abc", 5)])
def test_nombre_de_pieces_borne(valeur, attendu):
    """1 à 10 ; une automatisation d'avant l'entrée (None : le défaut du blueprint) ou une
    valeur illisible écrite à la main : 5."""
    entrees = MAISON_5X5 if valeur is None else {**MAISON_5X5, "nombre_pieces": valeur}
    p = _passage(entrees)
    assert p["nombre_pieces"] == attendu
    assert p.definitions().startswith(f"pn|{attendu};")


def test_entree_nombre_de_pieces():
    section = _blueprint()["blueprint"]["input"]["pieces"]
    entree = section["input"]["nombre_pieces"]
    assert entree["default"] == 5
    assert entree["selector"]["number"] == {"min": 1, "max": 10, "step": 1, "mode": "slider"}
    assert "collapsed" not in section, "le premier réglage des pièces, ouvert"


def test_pieces_au_dela_du_nombre_jamais_envoyees():
    """Une pièce R ≥ N n'existe pas pour la tablette : ni nom, ni tuile, ni état, ni climat,
    ni commande (la liste blanche est `tuiles`)."""
    entrees = {**MAISON_GRANDE, **PIECES_6_7, "nombre_pieces": 3}
    p = _passage(entrees)
    c = charges(p)
    assert _defs(c["definitions"])[0] == ["pn", "3"]
    assert {t["r"] for t in p["tuiles"]} == {0, 1, 2}
    assert [x["r"] for x in p["pieces_climat"]] == [0, 1, 2]
    for cle, charge in c.items():
        assert not [e for e in _defs(charge) if re.fullmatch(r"(p|t|cr|ce|crp|cep)[3-9][0-9a-j]?", e[0])], cle
    # La serrure de la pièce 4 (t31) ne s'ouvre plus depuis l'écran ; une tuile de la pièce
    # 1 si.
    assert _aiguillage(entrees, "t31", "basculer") is None
    assert _aiguillage(entrees, "t00", "basculer") == ("light.toggle", "light.chevet")


# ─────────────────────────────────────────────────────────────────────────────
# Vingt éléments par pièce, dix pièces
# ─────────────────────────────────────────────────────────────────────────────

def _lampes(prefixe, n):
    return [Etat(f"light.{prefixe}{i:02d}", "on" if i % 2 else "off", friendly_name=f"Lampe {prefixe} {i}",
                 supported_color_modes=["onoff"]) for i in range(n)]


# Pièce 1 : 22 entités, dont la PAC en treizième (t0c) ; pièce 10 (R = 9) : 3 lampes et sa
# propre clim de pièce.
LAMPES_1, LAMPES_10 = _lampes("a", 21), _lampes("j", 3)
VINGT = {
    "nombre_pieces": 10,
    "piece_1_tuiles": [e.entity_id for e in LAMPES_1[:12]] + ["climate.pac"] + [e.entity_id for e in LAMPES_1[12:]],
    "piece_10_nom": "Grenier",
    "piece_10_tuiles": [e.entity_id for e in LAMPES_10],
    "piece_10_temperature": "sensor.temp_salon",
    "piece_10_clim": "climate.clim",
}


def _etats_vingt():
    return _etats() + LAMPES_1 + LAMPES_10


def _aiguillage(entrees, emplacement, action):
    p = Passage(entrees, _etats_vingt(), _evenement("action", emplacement=emplacement, action=action, valeur=""))
    alias, sequence = p.aiguillage()
    return _action_rendue(p, sequence) if alias else None


def test_vingt_elements_cles_0_a_9_puis_a_a_j():
    p = Passage(VINGT, _etats_vingt(), _evenement("connexion"))
    piece_1 = [t for t in p["tuiles"] if t["r"] == 0]
    assert [t["cle"] for t in piece_1] == [f"t0{c}" for c in LETTRES], "les vingt premiers, dans l'ordre"
    assert piece_1[12]["e"] == "climate.pac" and piece_1[19]["e"] == "light.a18"
    assert [t["cle"] for t in p["tuiles"] if t["r"] == 9] == ["t90", "t91", "t92"]
    defs = _defs(p.definitions())
    assert defs[0] == ["pn", "10"]
    assert ["p9", "Grenier"] in defs
    assert "t0j" in {e[0] for e in defs} and "t0k" not in {e[0] for e in defs}
    # États, clim de tuile (crRT / ceRT) : la même clé.
    c = charges(p)
    cles = [e[0] for e in _defs(c["tuiles"])]
    assert cles.count("t0j") == 1 and "cr0c" in cles and "ce0c" in cles
    # Climat de la pièce 10 : « p9 », sa clim « crp9 » / « cep9 ».
    assert {"p9", "crp9", "cep9"} <= {e[0] for e in _defs(c["climat"])}


@pytest.mark.parametrize("emplacement, action, attendu", [
    ("t0j", "basculer", ("light.toggle", "light.a18")),
    ("t0a", "eteindre", ("light.turn_off", "light.a10")),
    ("t92", "allumer", ("light.turn_on", "light.j02")),
    ("t0k", "basculer", None),  # vingt et unième : aucune tuile
    ("t9a", "basculer", None),  # la pièce 10 n'en a que trois
])
def test_commandes_des_nouvelles_cles(emplacement, action, attendu):
    assert _aiguillage(VINGT, emplacement, action) == attendu


def test_clim_d_une_tuile_lettre():
    """La PAC en t0c : ses commandes passent par les branches « Clim : … » (ADR-0027)."""
    p = Passage(VINGT, _etats_vingt(), _evenement("action", emplacement="t0c", action="eteindre", valeur=""))
    assert p.variables_du_bloc("clim_cible") == "climate.pac"


def test_tout_eteindre_la_piece_10():
    p = Passage(VINGT, _etats_vingt(), _evenement("action", emplacement="p9", action="eteindre"))
    alias, sequence = p.aiguillage()
    assert alias.startswith("Pièce : tout éteindre")
    assert _rendre(p.env, sequence[0]["variables"]["lampes"], p.ctx) == [e.entity_id for e in LAMPES_10]


def test_declencheurs_de_la_piece_10():
    """« piece_10… » : le numéro lu jusqu'au « _ » (il était lu sur un seul caractère)."""
    avant, apres = LAMPES_10[1], Etat("light.j01", "off", friendly_name="Lampe j 1", supported_color_modes=["onoff"])
    etats = _etats() + LAMPES_1 + [LAMPES_10[0], apres, LAMPES_10[2]]
    p = Passage(VINGT, etats, _declencheur("piece_10", avant, apres, "light.j01"))
    assert p["tuiles_a_pousser"] == ["t91"]
    # La même lampe, pièce 10 au-delà du nombre de pièces : rien.
    p = Passage({**VINGT, "nombre_pieces": 9}, etats, _declencheur("piece_10", avant, apres, "light.j01"))
    assert p["tuiles_a_pousser"] == []
    # Sa clim de pièce change de mode : la pièce 10 (R = 9) repart.
    clim = next(e for e in _etats() if e.entity_id == "climate.clim")
    clim_apres = Etat("climate.clim", "off", "Salon", friendly_name="Clim salon")
    p = Passage(VINGT, etats, _declencheur("piece_10_clim", clim, clim_apres, "climate.clim"))
    assert p["pieces_climat_a_pousser"] == [9]


def test_une_maison_pleine_tient_dans_la_charge_du_firmware():
    """Dix pièces de vingt éléments aux noms de trente caractères : les définitions tiennent
    dans kPayloadMax (Tab5/socle/tab5_champs.h), le plafond de tab5_maj_tuiles d'un firmware
    3.9 comme d'un firmware de l'ADR-0066 (au-delà, il refuse tout l'instantané). Les noms
    ne sont pas bornés côté HA : c'est une maison réaliste, pas une borne."""
    plafond = int(re.search(r"kPayloadMax = (\d+) \* 1024;", _lire("Tab5", "socle", "tab5_champs.h")).group(1)) * 1024
    etats: list = []
    entrees: dict = {"nombre_pieces": 10}
    for r in range(10):
        lampes = [Etat(f"light.p{r}_{i:02d}", "on", friendly_name=f"Lampe d'appoint numéro {r}-{i:02d}".ljust(30, "x"),
                       supported_color_modes=["hs"], brightness=200, rgb_color=(255, 128, 0)) for i in range(20)]
        etats += lampes
        entrees[f"piece_{r + 1}_nom"] = f"Pièce numéro {r + 1}"
        entrees[f"piece_{r + 1}_tuiles"] = [e.entity_id for e in lampes]
    p = Passage(entrees, etats, _evenement("connexion"))
    definitions, etats_tuiles = p.definitions(), p.etats_tuiles()
    assert len(p["tuiles"]) == 200
    taille = len(definitions.encode("utf-8"))
    assert taille < plafond, f"définitions de 200 éléments : {taille} octets, plafond {plafond}"
    assert len(etats_tuiles.encode("utf-8")) < plafond
