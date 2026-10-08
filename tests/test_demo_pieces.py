# -*- coding: utf-8 -*-
"""Pièces du mode démo (tools/demo/scenarios.py) contre le contrat de l'ADR-0023.

La grammaire est relue dans l'ADR elle-même (types, options, code d'icône, longueurs,
table pièce ↔ page) : si le contrat change, ces tests le disent avant la tablette, qui
ignorerait en silence une tuile mal formée. Vérifiés aussi : l'échappement des champs
texte (« | » → « / », « ; » → « , »), la cohérence des états avec leur type, la
couverture du contrat par la maison de la démo, et la palette d'icônes."""
import os
import re

import pytest
from tests.commun import lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

import scenarios  # noqa: E402
from scenarios import (  # noqa: E402
    PIECES, PIECES_MINIMALES, Piece, Tuile, build_etats_tuiles, build_tuiles_payload, echapper,
)


ADR = _lire("docs", "decisions", "0023-rooms-generic-tiles.md")


def _regle(nom):
    """Partie droite d'une règle de la grammaire (« type       := lum | int … »)."""
    m = re.search(rf"^{nom}\s+:= (.+)$", ADR, re.M)
    assert m, f"règle « {nom} » introuvable dans l'ADR-0023"
    return m.group(1)


TYPES = tuple(_regle("type").split(" | "))
OPTIONS = "".join(re.fullmatch(r"letters among ([a-z ]+), '' = none", _regle("options")).group(1).split())
CODE_ICONE = re.search(r"a palette code \((\S+)\)", _regle("icon")).group(1)
UNITE_MAX = int(re.search(r"unit \(≤ (\d+) bytes", _regle("complement")).group(1))
NOM_MAX = int(re.search(r"keeps ≤ (\d+) bytes", _regle("name")).group(1))

_TEXTE = r"[^|;]*"
DEFINITION = re.compile(
    rf"p(?P<pr>[0-4])\|(?P<nom_piece>{_TEXTE})"
    rf"|t(?P<r>[0-4])(?P<t>[0-4])\|(?P<type>{'|'.join(TYPES)})\|(?P<icone>(?:{CODE_ICONE})?)"
    rf"\|(?P<options>[{OPTIONS}]*)\|(?P<complement>{_TEXTE})\|(?P<nom>{_TEXTE})")
ETAT = re.compile(rf"t(?P<r>[0-4])(?P<t>[0-4])\|(?P<etat>[^|;]+)\|(?P<valeur>nan|-?\d+(?:\.\d+)?)"
                  rf"\|(?P<couleur>(?:[0-9A-Fa-f]{{6}})?)")
MAISONS = {"complète": PIECES, "minimale": PIECES_MINIMALES}


def _entrees(payload):
    assert payload.endswith(";"), payload[-20:]
    return payload[:-1].split(";")


def test_grammaire_de_l_adr_dans_la_demo():
    assert scenarios.TYPES_TUILE == TYPES
    assert sorted(scenarios.OPTIONS_TUILE) == sorted(OPTIONS)
    assert scenarios.UNITE_OCTETS_MAX == UNITE_MAX
    assert scenarios.NOM_OCTETS_GARDES == NOM_MAX
    table = dict((int(r), int(p)) for r, p in re.findall(r"^\s*\| (\d) \| (\d) \|", ADR, re.M))
    assert len(table) == 5
    assert scenarios.PAGE_DE_LA_PIECE == table


@pytest.mark.parametrize("maison", MAISONS)
def test_definitions_suivent_la_grammaire(maison):
    pieces = MAISONS[maison]
    lues = {}
    for entree in _entrees(build_tuiles_payload(pieces)):
        m = DEFINITION.fullmatch(entree)
        assert m, entree
        if m["pr"] is not None:
            assert pieces[int(m["pr"])].nom == m["nom_piece"]
            continue
        cle = (int(m["r"]), int(m["t"]))
        assert cle not in lues, entree
        lues[cle] = m
        tuile = pieces[cle[0]].tuiles[cle[1]]
        assert (m["type"], m["icone"], m["options"], m["complement"], m["nom"]) == (
            tuile.type, tuile.icone, tuile.options, tuile.complement, tuile.nom)
        if tuile.type == "cap":
            assert 0 < len(m["complement"].encode("utf-8")) <= UNITE_MAX, entree
        elif tuile.type != "bin":
            assert m["complement"] == "", entree
    # Instantané complet : chaque tuile une fois, rien d'autre.
    assert set(lues) == {(r, t) for r, p in pieces.items() for t in p.tuiles}


@pytest.mark.parametrize("maison", MAISONS)
def test_etats_suivent_la_grammaire(maison):
    pieces = MAISONS[maison]
    for scene in scenarios.SCENES:
        cles = []
        for entree in _entrees(build_etats_tuiles(pieces, scene.clim)):
            m = ETAT.fullmatch(entree)
            assert m, entree
            cles.append((int(m["r"]), int(m["t"])))
        assert cles == [(r, t) for r, p in sorted(pieces.items()) for t in sorted(p.tuiles)]


def test_etats_a_la_suite_des_emplacements_3x():
    payload = scenarios.build_emplacements_payload(frozenset(), PIECES, scenarios.SCENES[0].clim)
    assert payload.startswith(scenarios.build_emplacements_payload())
    assert payload.endswith(build_etats_tuiles(PIECES, scenarios.SCENES[0].clim))
    assert "t00|" not in scenarios.build_emplacements_payload(), "sans pièces, pas de clé tRT"


def test_echappement_des_champs_texte():
    assert echapper("Bureau | atelier; 1") == "Bureau / atelier, 1"
    pieces = {1: Piece("Bureau | atelier", {3: Tuile("int", "Prise; imprimante|scanner", etat="a|b;c")})}
    definitions = _entrees(build_tuiles_payload(pieces))
    assert definitions == ["p1|Bureau / atelier", "t13|int||||Prise, imprimante/scanner"]
    for entree in definitions:
        assert DEFINITION.fullmatch(entree), entree
    assert build_etats_tuiles(pieces) == "t13|a/b,c|nan|;"


def test_une_piece_sans_nom_n_a_pas_d_entree_p():
    assert not any(e.startswith("p") for e in _entrees(build_tuiles_payload(PIECES_MINIMALES)))
    assert scenarios.nom_de_la_piece(0, PIECES_MINIMALES[0]) == "Pièce 1"


@pytest.mark.parametrize("tuile", [
    Tuile("lampe", "Type inconnu"),
    Tuile("lum", "Icône invalide", icone="Lampe-Salon"),
    Tuile("lum", "Option inconnue", options="dx"),
    Tuile("lum", "Option répétée", options="dd"),
    Tuile("int", "Variateur hors lampe", options="d"),
    Tuile("lum", "TV hors média", options="t"),
    Tuile("cap", "Capteur sans unité", etat="3", valeur="3"),
    Tuile("cap", "Unité trop longue", complement="kWh/jour", etat="3", valeur="3"),
    Tuile("bin", "Sans classe"),
    Tuile("int", "Complément hors cap/bin", complement="W"),
    Tuile("lum", " "),
])
def test_definition_hors_contrat_refusee(tuile):
    with pytest.raises(AssertionError):
        build_tuiles_payload({0: Piece("", {0: tuile})})


@pytest.mark.parametrize("tuile", [
    Tuile("lum", "Luminosité > 255", options="d", etat="on", valeur="300"),
    Tuile("lum", "Luminosité éteinte", options="d", etat="off", valeur="120"),
    Tuile("lum", "Couleur sans option c", options="d", etat="on", valeur="120", couleur="FF0000"),
    Tuile("lum", "Couleur éteinte", options="dc", etat="off", couleur="FF0000"),
    Tuile("lum", "Couleur mal écrite", options="dc", etat="on", valeur="120", couleur="rouge"),
    Tuile("vol", "Position > 100", etat="open", valeur="120"),
    Tuile("vol", "État de volet inconnu", etat="on", valeur="50"),
    Tuile("cap", "Capteur sans nombre", complement="W", etat="126"),
    Tuile("int", "Valeur hors type", etat="on", valeur="1"),
    Tuile("int", "Hors ligne avec valeur", etat="unavailable", valeur="1"),
    Tuile("int", "État vide", etat=""),
])
def test_etat_incoherent_refuse(tuile):
    with pytest.raises(AssertionError):
        build_etats_tuiles({0: Piece("", {0: tuile})})


def test_la_demo_couvre_le_contrat():
    tuiles = [t for p in PIECES.values() for t in p.tuiles.values()]
    assert {t.type for t in tuiles} == set(TYPES)
    assert set("".join(t.options for t in tuiles)) == set(OPTIONS)
    assert {"door", "motion", "presence"} <= {t.complement for t in tuiles if t.type == "bin"}
    assert any(t.type == "cap" and t.complement for t in tuiles)
    assert any(t.type == "lum" and t.couleur and "d" in t.options for t in tuiles), "lampe couleur à variateur"
    assert any(t.type == "vol" and t.etat in ("opening", "closing") for t in tuiles), "volet en mouvement"
    assert any(t.etat == "unavailable" for t in tuiles), "un appareil hors ligne"
    noms = [t.nom for t in tuiles] + [p.nom for p in PIECES.values()]
    assert any(len(n.encode("utf-8")) > NOM_MAX for n in noms), "un nom que la tablette coupe"
    assert any(not n.isascii() for n in noms), "des accents"
    assert len(PIECES) >= 4
    assert any(len(p.tuiles) == 2 for p in PIECES.values()), "une pièce de deux tuiles (recentrées)"


def test_maison_minimale():
    """Peu d'appareils : une pièce au plus, rien de ce que la maison minimale retire."""
    assert scenarios.pieces_de(scenarios.MAISON_MINIMALE) is PIECES_MINIMALES
    assert scenarios.pieces_de(frozenset()) is PIECES
    assert len(PIECES_MINIMALES) <= 1
    tuiles = [t for p in PIECES_MINIMALES.values() for t in p.tuiles.values()]
    assert len(tuiles) <= 3
    assert not any(t.type in ("med", "cli", "vol") for t in tuiles), "ni TV, ni clim, ni volet"


def test_noms_dans_les_polices_de_la_tablette():
    """Polices Latin-1 + cp1252 : un caractère hors de ces tables disparaîtrait."""
    for pieces in MAISONS.values():
        for piece in pieces.values():
            for nom in [piece.nom] + [t.nom for t in piece.tuiles.values()]:
                nom.encode("cp1252")


def test_icones_de_la_palette():
    """Un code hors palette prendrait en silence l'icône par défaut du type."""
    entete = _lire("Tab5", "tab5_tuiles_icones.h")
    palette = set(re.findall(r'\{\s*"([a-z0-9_]{1,15})"\s*,', entete.split("kPalette", 1)[1]))
    assert palette
    for pieces in MAISONS.values():
        for piece in pieces.values():
            for tuile in piece.tuiles.values():
                assert tuile.icone == "" or tuile.icone in palette, (tuile.nom, tuile.icone)


def test_la_clim_du_blueprint_suit_la_scene():
    """Option m : la tuile et la carte clim (tab5_maj_clim) disent la même chose."""
    (r, t), = [(r, t) for r, p in PIECES.items() for t, tu in p.tuiles.items() if "m" in tu.options]
    for scene in scenarios.SCENES:
        etats = dict(e.split("|", 1) for e in _entrees(build_etats_tuiles(PIECES, scene.clim)))
        assert etats[f"t{r}{t}"] == f"{scene.clim['mode']}|{scene.clim['current']}|"


def test_description_des_commandes():
    assert scenarios.decrire_emplacement("t02", PIECES) == "t02 (Salon › Lampe d'ambiance, lum)"
    assert scenarios.decrire_emplacement("p4", PIECES) == "p4 (Jardin, toutes ses lumières)"
    assert "vide" in scenarios.decrire_emplacement("t21", PIECES)
    assert scenarios.decrire_emplacement("lumiere_1", PIECES) == "lumiere_1"
