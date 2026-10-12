# -*- coding: utf-8 -*-
"""Le robot du rendu sur une autre carte que le Tab5 (tools/rendu/vers_carte.py, ADR-0061,
lot 2 du portage 1024×600).

On vérifie :
- sur le Tab5, les étapes de chaque écran sont rendues telles quelles (captures de
  référence inchangées) ;
- sur chaque autre carte, tout geste tombe dans l'écran, toute option de navigation existe,
  l'Arcade est ouverte par sa valeur de l'enum Ecran, et la conversion reprend la géométrie
  de tab5_geometrie.h (carte modale, corps, onglets d'en-tête) ;
- les noms des pages des Réglages, touchés sur le Tab5, tombent sur les mêmes onglets de la
  carte (pages_onglets, tab5_pages.cpp) ;
- un écran coupé ne laisse pas d'état derrière lui (thème remis, changement d'accueil
  impossible à défaire pas joué), l'accueil et l'Arcade suivent leur vraie mise en page."""
import re

import pytest

import ecrans as E
import vers_carte as V
from tests.commun import CARTES, charger, geometrie, geometrie_cpp, lire

AUTRES = [c for c in CARTES if c != "tab5"]


def _options() -> list[str]:
    nav = charger("Tab5", "paquets", "tab5-navigation.yaml")
    return next(s for s in nav["select"] if s.get("name") == "Aller à l'écran")["options"]


def test_tab5_inchange():
    for ecran in E.ECRANS:
        etapes, fermer, retirees = V.adapter(ecran, "tab5")
        assert etapes is ecran.etapes and fermer is ecran.fermer and retirees == [], ecran.nom


@pytest.mark.parametrize("carte", CARTES)
def test_taille_de_la_carte(carte):
    g = geometrie(carte)
    assert V.taille_carte(carte) == (int(g["carte_ecran_l"]), int(g["carte_ecran_h"]))


def test_geometrie_reprise_de_tab5_geometrie():
    g = geometrie_cpp("tab5")
    assert (g["kEcranL"], g["kEcranH"]) == V.TAB5
    assert V.MARGE_CARTE == (g["kEcranL"] - g["kCarteL"]) // 2
    assert (V.CORPS_X, V.CORPS_Y) == (g["kCorpsX"], g["kCorpsY"])
    assert (V.ONGLETS_DEBUT_TAB5, V.ONGLETS_FIN_TAB5) == (g["kOngletsDebutTab5"], g["kOngletsFinTab5"])
    assert V.ONGLETS_AVANT_LE_BORD == g["kCarteL"] - g["kOngletsFin"]
    for carte in CARTES:
        gc = geometrie_cpp(carte)
        assert V.Carte(V.taille_carte(carte)).corps == (gc["kCorpsW"], gc["kCorpsH"]), carte


def test_arcade_par_sa_valeur_d_ecran():
    zones = lire("Tab5", "ecran", "tab5_zones.h")
    enum = re.search(r"enum class Ecran : uint8_t \{(.*?)\};", zones, re.S).group(1)
    noms = [m for m in re.findall(r"^\s*([A-Z_]+),", enum, re.M)]
    assert noms.index("ARCADE") == V.ECRAN_ARCADE
    # Les options du select sont les valeurs de l'enum jusqu'à l'Arcade, qui n'en est pas une.
    assert len(_options()) == V.ECRAN_ARCADE
    bouchons = lire("Tab5", "rendu", "bouchons.yaml")
    assert "- service: rendu_ecran" in bouchons and "id: tab5_ecran_ouvrir" in bouchons


@pytest.mark.parametrize("carte", AUTRES)
def test_gestes_dans_l_ecran_et_options_connues(carte):
    l, h = V.taille_carte(carte)
    options = set(_options())
    for ecran in E.ECRANS:
        etapes, fermer, _retirees = V.adapter(ecran, carte)
        for etape in etapes + fermer:
            if isinstance(etape, E.Toucher):
                for x, y in [(etape.x, etape.y)] + [(sx, sy) for _s, sx, sy in etape.selon_langue]:
                    assert 0 <= x < l and 0 <= y < h, (ecran.nom, etape)
            elif isinstance(etape, E.Glisser):
                for x, y in ((etape.x1, etape.y1), (etape.x2, etape.y2)):
                    assert 0 <= x < l and 0 <= y < h, (ecran.nom, etape)
            elif isinstance(etape, E.Aller):
                assert etape.option in options, (ecran.nom, etape)
            elif isinstance(etape, E.Service) and etape.nom == "rendu_ecran":
                assert dict(etape.donnees) == {"ecran": V.ECRAN_ARCADE}


@pytest.mark.parametrize("carte", AUTRES)
def test_boutons_hors_ecran_remplaces_par_la_navigation(carte):
    """L'engrenage et la manette du Tab5 sont hors de l'écran de la 7B : Réglages et l'Arcade
    s'ouvrent par la navigation."""
    reglages = next(e for e in E.ECRANS if e.nom == "reglages")
    assert V.adapter(reglages, carte)[0] == (E.Aller("Réglages"),)
    arcade = next(e for e in E.ECRANS if e.nom == "arcade")
    (service,) = V.adapter(arcade, carte)[0]
    assert service.nom == "rendu_ecran"


def _ecran(nom):
    return next(e for e in E.ECRANS if e.nom == nom)


@pytest.mark.parametrize("carte", AUTRES)
def test_etat_commun_remis_meme_ecran_coupe(carte):
    """Un écran coupé par un geste hors de l'écran remet quand même le thème : sans quoi
    toutes les captures suivantes de la carte seraient dans le thème Capsule."""
    ecran = _ecran("accueil-zone-graphique-gelule")
    _etapes, fermer, retirees = V.adapter(ecran, carte)
    assert retirees and E.Choisir("Thème", "Relief doux", apres=2.0) in fermer
    for e in E.ECRANS:
        etapes, fermer, _r = V.adapter(e, carte)
        choisis = [x for x in e.etapes if isinstance(x, E.Choisir)]
        joues = [x for x in etapes if isinstance(x, E.Choisir)]
        if joues:
            # Ce que les étapes ont choisi, fermer le remet tel que sur le Tab5.
            assert [x for x in fermer if isinstance(x, E.Choisir)] == \
                [x for x in e.fermer if isinstance(x, E.Choisir)], e.nom
        assert len(joues) <= len(choisis)


@pytest.mark.parametrize("carte", AUTRES)
def test_changement_d_accueil_impossible_a_defaire_pas_joue(carte):
    """« Appareil suivant » de la tuile − / + ne se défait que par la tuile, hors de l'écran
    de la 7B : l'écran n'est pas joué (le retour à l'accueil ne le déferait pas)."""
    etapes, fermer, retirees = V.adapter(_ecran("accueil-minutes-appareil-suivant"), carte)
    assert etapes == () and fermer == () and retirees


@pytest.mark.parametrize("carte", AUTRES)
def test_accueil_horloge_au_pixel_carte_centrale_centree(carte):
    """Les zones tactiles de l'horloge sont posées au pixel (horloge_zone.yaml), la carte
    centrale est centrée (x 20, largeur de l'écran − 40)."""
    c = V.Carte(V.taille_carte(carte))
    assert c.accueil(*E.HEURES) == E.HEURES
    assert c.accueil(*E.CARTE_CENTRALE) == (c.l // 2, E.CARTE_CENTRALE[1])


@pytest.mark.parametrize("carte", AUTRES)
def test_arcade_puis_console(carte):
    """Le sélecteur de l'Arcade est centré en largeur ; une console reste au pixel."""
    c = V.Carte(V.taille_carte(carte))
    etapes, _f, _r = V.adapter(_ecran("jeu-go-nouvelle-partie"), carte)
    service, choix, menu = etapes
    assert service.nom == "rendu_ecran"
    ecran = _ecran("jeu-go-nouvelle-partie").etapes
    assert (choix.x, choix.y) == (ecran[1].x + c.dx // 2, ecran[1].y)
    assert (menu.x, menu.y) == (ecran[2].x, ecran[2].y)


@pytest.mark.parametrize("carte", AUTRES)
def test_onglets_des_reglages_sur_ceux_de_la_carte(carte):
    g = geometrie_cpp(carte)
    c = V.Carte(V.taille_carte(carte))
    zone = min(g["kOngletsFin"] - g["kOngletsDebutTab5"], g["kOngletsZoneTab5"])

    def echelle(v):
        return v if zone >= g["kOngletsZoneTab5"] else (v * zone + g["kOngletsZoneTab5"] // 2) // g["kOngletsZoneTab5"]

    for i, (_nom, (x, y)) in enumerate(E.REGLAGES_PAGES.items()):
        x5 = 318 + i * 210                                   # x de l'onglet sur le Tab5
        gauche = V.MARGE_CARTE + g["kOngletsFin"] - echelle(g["kOngletsFinTab5"] - x5)
        xc, yc = c.popup(x, y)
        assert gauche < xc < gauche + echelle(200), (carte, _nom, xc, gauche)
        assert yc == y
    # La croix reste à la même distance du bord droit de l'écran.
    xc, _ = c.popup(*E.FERMER_POPUP)
    assert c.l - xc == V.TAB5[0] - E.FERMER_POPUP[0]
