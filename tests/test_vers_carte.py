# -*- coding: utf-8 -*-
"""Le robot du rendu sur une autre carte que le Tab5 (tools/rendu/vers_carte.py, ADR-0061,
lots 2 et 3 du portage 1024×600).

On vérifie :
- sur le Tab5, les étapes de chaque écran sont rendues telles quelles (captures de
  référence inchangées) ;
- sur chaque autre carte, tout geste tombe dans l'écran, toute option de navigation existe,
  l'Arcade est ouverte par sa valeur de l'enum Ecran, et la conversion reprend la géométrie
  de tab5_geometrie.h (carte modale, corps, onglets d'en-tête) ;
- les noms des pages des Réglages, touchés sur le Tab5, tombent sur les mêmes onglets de la
  carte (pages_onglets, tab5_pages.cpp) ;
- un écran coupé ne laisse pas d'état derrière lui (thème remis), l'accueil (ramené à
  l'écran de la carte depuis le lot 3 : ses gestes sont joués, plus remplacés par la
  navigation) et l'Arcade suivent leur vraie mise en page."""
import re

import pytest

import ecrans as E
import vers_carte as V
from tests.commun import CARTES, charger, geometrie, geometrie_cpp, lire
from tests.test_gestes import _zones

AUTRES = [c for c in CARTES if c != "tab5"]


def _options() -> list[str]:
    nav = charger("Tab5", "paquets", "tab5-navigation.yaml")
    return next(s for s in nav["select"] if s.get("name") == "Aller à l'écran")["options"]


def test_tab5_inchange():
    for ecran in E.ECRANS:
        etapes, fermer, retirees = V.adapter(ecran, "tab5")
        assert etapes is ecran.etapes and fermer is ecran.fermer and retirees == [], ecran.nom


def test_boutons_des_roues_inchanges_a_la_taille_du_tab5():
    """Les boutons des roues recalculés pour une carte (V._roues) redonnent, à la taille du
    Tab5, ceux d'ecrans.py : un mauvais index (le Bureau de la roue de navigation pris pour
    la première pièce, 18/10/2026) changeait de bouton sur la 7B sans que rien ne le dise."""
    for etape, (nouvelle, _ouvre) in V._roues(V.Carte(V.TAB5)).items():
        assert (nouvelle.x, nouvelle.y) == (etape.x, etape.y), etape


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
def test_boutons_du_haut_touches_a_leur_place(carte):
    """Depuis le lot 3, l'engrenage et la manette sont dans l'écran de la carte (boutons du
    haut ramenés à sa taille) : Réglages et l'Arcade s'ouvrent par un vrai toucher, au point
    du Tab5 ramené à l'écran ; la navigation n'est plus utilisée pour l'accueil."""
    c = V.Carte(V.taille_carte(carte))
    reglages = V.adapter(_ecran("reglages"), carte)[0]
    assert reglages == (E.Toucher(*c.accueil(*E.BOUTON_SYS)),)
    (toucher,) = V.adapter(_ecran("arcade"), carte)[0]
    assert (toucher.x, toucher.y) == c.accueil(*E.BOUTON_TV)
    for ecran in E.ECRANS:
        etapes, fermer, _r = V.adapter(ecran, carte)
        if not any(isinstance(x, (E.Aller, E.Service)) for x in ecran.etapes + ecran.fermer):
            assert not any(isinstance(x, (E.Aller, E.Service)) for x in etapes + fermer), ecran.nom


def _ecran(nom):
    return next(e for e in E.ECRANS if e.nom == nom)


@pytest.mark.parametrize("carte", AUTRES)
def test_etat_commun_remis_meme_ecran_coupe(carte):
    """Un écran coupé par un geste hors de l'écran remet quand même le thème : sans quoi
    toutes les captures suivantes de la carte seraient dans le thème Capsule. Depuis le lot 3
    aucun écran d'accueil n'est plus coupé : un écran construit pour l'être (une console,
    posée au pixel, touchée au bas à droite du Tab5) le vérifie."""
    theme, remis = E.Choisir("Thème", "Capsule"), E.Choisir("Thème", "Relief doux")
    coupe = E.Ecran("essai", (theme, E.Toucher(*E.BOUTON_TV, apres=1.0), E.Toucher(1270, 710), E.Toucher(10, 10)),
                    (E.Toucher(10, 10), remis))
    etapes, fermer, retirees = V.adapter(coupe, carte)
    assert retirees == [E.Toucher(1270, 710)] and etapes[0] == theme and len(etapes) == 2
    assert fermer == (remis,)
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
def test_changement_d_accueil_joue_et_defait(carte):
    """« Appareil suivant » de la tuile − / + ne se défait que par la tuile : au lot 2 elle
    était hors de l'écran de la 7B et l'écran n'était pas joué ; depuis le lot 3 il l'est,
    `fermer` compris (même nombre d'étapes que sur le Tab5, rien de retiré)."""
    ecran = _ecran("accueil-minutes-appareil-suivant")
    etapes, fermer, retirees = V.adapter(ecran, carte)
    assert retirees == [] and len(etapes) == len(ecran.etapes) and len(fermer) == len(ecran.fermer)


@pytest.mark.parametrize("carte", AUTRES)
def test_accueil_ramene_a_l_ecran_de_la_carte(carte):
    """L'accueil est celui du Tab5 ramené à l'écran de la carte (jetons acc_* ; acc_x / acc_y
    de tab5_geometrie.h) : chaque point de l'horloge tombe dans sa zone tactile de la carte
    (horloge_zone.yaml, jetons résolus pour elle), la carte centrale reste centrée."""
    c = V.Carte(V.taille_carte(carte))
    g = geometrie_cpp(carte)
    for v in (0, 20, 333, 640, 1280):
        assert c.acc_x(v) == (v * g["kEcranL"] + 640) // 1280 and c.acc_y(v) == (v * g["kEcranH"] + 360) // 720
    zones = _zones(carte)
    for point, ident in ((E.HEURES, "btn_horloge_heures"), (E.MINUTES, "btn_horloge_minutes"),
                         (E.DATE, "btn_horloge_date")):
        x, y, w, h = zones[ident][:4]
        px, py = c.accueil(*point)
        assert x + 5 <= px < x + w - 5 and y + 5 <= py < y + h - 5, (ident, (px, py), (x, y, w, h))
    assert c.accueil(*E.CARTE_CENTRALE)[0] == c.l // 2


@pytest.mark.parametrize("carte", AUTRES)
def test_arcade_puis_console(carte):
    """La manette touchée sur l'accueil ramené à la carte ouvre le sélecteur de l'Arcade,
    centré en largeur ; une console reste au pixel."""
    c = V.Carte(V.taille_carte(carte))
    etapes, _f, _r = V.adapter(_ecran("jeu-go-nouvelle-partie"), carte)
    manette, choix, menu = etapes
    assert (manette.x, manette.y) == c.accueil(*E.BOUTON_TV)
    ecran = _ecran("jeu-go-nouvelle-partie").etapes
    assert (choix.x, choix.y) == (ecran[1].x + c.dx // 2, ecran[1].y)
    assert (menu.x, menu.y) == (ecran[2].x, ecran[2].y)


@pytest.mark.parametrize("carte", AUTRES)
def test_popup_musique_au_meme_endroit_de_sa_carte(carte):
    """Rendu du 18/10/2026 (ADR-0067) : sur la 7B, les gestes du popup Musique, ramenés comme
    dans la carte modale, ne changeaient pas de page (même image que la page Lecture). Sa
    carte est la même sur toutes les cartes, centrée : un point y garde sa place."""
    jetons = charger("Tab5", "paquets", "tab5-ui-tokens.yaml")["substitutions"]
    assert V.LECTEUR_CARTE == (int(jetons["lecteur_card_w"]), int(jetons["lecteur_card_h"]))
    l, h = V.taille_carte(carte)
    dx = (V.TAB5[0] - V.LECTEUR_CARTE[0]) // 2 - (l - V.LECTEUR_CARTE[0]) // 2
    dy = (V.TAB5[1] - V.LECTEUR_CARTE[1]) // 2 - (h - V.LECTEUR_CARTE[1]) // 2
    if carte == "waveshare-7b":
        assert (dx, dy) == (128, 60)
    (glisser,) = [e for e in V.adapter(_ecran("musique-pieces"), carte)[0] if isinstance(e, E.Glisser)]
    s = E.MUSIQUE_PAGE_SUIVANTE
    assert (glisser.x1, glisser.y1, glisser.x2, glisser.y2) == (s.x1 - dx, s.y1 - dy, s.x2 - dx, s.y2 - dy)
    gestes = [e for e in V.adapter(_ecran("musique-sources"), carte)[0] if isinstance(e, (E.Glisser, E.Toucher))]
    assert len(gestes) == 3
    long, roue = gestes[1], gestes[2]
    ancre = (E.MUSIQUE_SALON_ICONE[0] - dx, E.MUSIQUE_SALON_ICONE[1] - dy)
    assert (long.x, long.y, long.duree) == (*ancre, 1200)
    assert (roue.x, roue.y) == E.roue_centres(*ancre, E.MUSIQUE_ROUE_SALON, ecran=(l, h))[0]


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
