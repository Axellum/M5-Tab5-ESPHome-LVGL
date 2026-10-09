# -*- coding: utf-8 -*-
"""Plan des écrans capturés par le rendu (tools/rendu/ecrans.py) : cohérent avec le firmware.

Un nom en double écraserait une capture ; un appui hors de l'écran, une option absente
du select « Aller à l'écran » ou une action inconnue de l'API ne s'apercevraient qu'en
CI, sous la forme d'une capture identique à une autre. De même pour les pièces
(ADR-0023) : un écran qui laisserait le mode HA ou une autre page que l'accueil
décalerait tous les suivants, et un geste parti d'un bouton le déclencherait."""
import os
import re
from tests.commun import lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

import ecrans  # noqa: E402
from ecrans import BOUTON_HA, ECRANS, Aller, Glisser, Service, Toucher  # noqa: E402
from scenarios import PAGE_DE_LA_PIECE, PIECES  # noqa: E402  (tools/demo, chemin ajouté par ecrans)


def _etapes(ecran):
    return ecran.etapes + ecran.fermer


def test_noms_uniques_et_en_ascii():
    noms = [e.nom for e in ECRANS]
    assert len(noms) == len(set(noms))
    for nom in noms:
        assert re.fullmatch(r"[a-z0-9]+(-[a-z0-9]+)*", nom), nom
        # Les scènes du mode démo commencent par leur numéro (capturer.nom_de).
        assert not nom[0].isdigit(), nom


def test_appuis_dans_l_ecran():
    for ecran in ECRANS:
        # Neon Apron s'ouvre depuis le sélecteur en paysage, puis passe en portrait.
        largeur, hauteur = (1280, 1280) if ecran.portrait else (1280, 720)
        points = []
        for etape in _etapes(ecran):
            if isinstance(etape, Toucher):
                points += [(etape.x, etape.y)] + [(x, y) for _, x, y in etape.selon_langue]
            elif isinstance(etape, Glisser):
                points += [(etape.x1, etape.y1), (etape.x2, etape.y2)]
        for x, y in points:
            assert 0 <= x < largeur and 0 <= y < hauteur, (ecran.nom, x, y)


def test_options_du_select_aller_a_l_ecran():
    bloc = _lire("Tab5", "paquets", "tab5-navigation.yaml").split('name: "Aller à l\'écran"', 1)[1]
    options = set(re.findall(r'^\s+- "([^"]+)"', bloc.split("on_value:", 1)[0], re.M))
    assert "Accueil" in options
    for ecran in ECRANS:
        for etape in _etapes(ecran):
            if isinstance(etape, Aller):
                assert etape.option in options, (ecran.nom, etape.option)


def test_actions_connues_de_l_api():
    textes = _lire("Tab5", "paquets", "tab5-api-logic.yaml") + _lire("Tab5", "rendu", "bouchons.yaml")
    actions = set(re.findall(r"- service: (\w+)", textes))
    assert {"rendu_capture", "rendu_toucher", "rendu_glisser"} <= actions
    for ecran in ECRANS:
        for etape in _etapes(ecran):
            if isinstance(etape, Service):
                assert etape.nom in actions, (ecran.nom, etape.nom)


def test_portraits():
    assert ecrans.PORTRAITS == {e.nom for e in ECRANS if e.portrait}


# ---------------------------------------------------------------------------
# Pages des prévisions et mode HA (ADR-0023). Modèle du contrat :
# - mode météo, handle_swipe_gesture (tab5_central.cpp) : vers la gauche 0→1→2→3→4, et
#   4 revient à 2 ; vers la droite 4→3→2→1→0, et 0 revient à 2 ; geste compté si le
#   doigt est à y ≥ FORECAST_SWIPE_Y_MIN ;
# - mode HA : pièce suivante (gauche) ou précédente (droite) qui a un appareil, sans
#   boucler ; « HA » bascule le mode ; « Aller à l'écran → Accueil » le quitte ; une pièce
#   choisie dans la roue de navigation (ADR-0042, NAV_BUREAU) y met le mode HA.
# ---------------------------------------------------------------------------

PAGES_OCCUPEES = sorted(PAGE_DE_LA_PIECE[r] for r, p in PIECES.items() if p.tuiles)
Y_MIN_GESTE = int(re.search(r"FORECAST_SWIPE_Y_MIN = (\d+);", _lire("Tab5", "ecran", "tab5_central.cpp")).group(1))


def _jouer(etapes, page=2, ha=False):
    """Page et mode HA après chaque étape : [(étape, page, ha avant l'étape)], page, ha."""
    trace = []
    for etape in etapes:
        trace.append((etape, page, ha))
        if isinstance(etape, Toucher) and (etape.x, etape.y) == BOUTON_HA:
            ha = not ha
        elif isinstance(etape, Aller) and etape.option == "Accueil":
            ha = False
        elif etape == ecrans.NAV_BUREAU:
            ha, page = True, PAGE_DE_LA_PIECE[ecrans.PIECE_CLIMAT]
        elif (isinstance(etape, Glisser) and not etape.dans_popup and etape.y2 >= Y_MIN_GESTE
              and etape.x1 != etape.x2):
            gauche = etape.x2 < etape.x1
            if ha:
                autres = [p for p in PAGES_OCCUPEES if (p > page if gauche else p < page)]
                if autres:
                    page = min(autres) if gauche else max(autres)
            elif gauche:
                page = 2 if page >= 4 else page + 1
            else:
                page = 2 if page <= 0 else page - 1
    return trace, page, ha


def test_chaque_ecran_revient_a_l_accueil_en_mode_meteo():
    """capturer.py n'a que « Aller à l'écran → Accueil » entre deux écrans : il ne remet
    pas la page des prévisions, ni le mode HA avant la 3.2. Chaque écran le fait."""
    for ecran in ECRANS:
        _, page, ha = _jouer(_etapes(ecran))
        assert (page, ha) == (2, False), (ecran.nom, page, ha)


POPUPS_EN_MODE_HA = {"temperature-piece", "climatisation-piece"}


def test_les_fenetres_du_climat_d_une_piece():
    """ADR-0040 : ouvertes en mode HA sur la pièce de la démo qui a température, humidité et
    clim ; la fenêtre refermée avant de quitter le mode HA (sinon « HA » tombe sur le voile)."""
    for nom in POPUPS_EN_MODE_HA:
        ecran = next(e for e in ECRANS if e.nom == nom)
        _, page, ha = _jouer(ecran.etapes)
        assert ha and page == PAGE_DE_LA_PIECE[ecrans.PIECE_CLIMAT], nom
        assert ecran.fermer[0] == Toucher(*ecrans.FERMER_POPUP), nom
    climat = PIECES[ecrans.PIECE_CLIMAT].climat
    assert climat.humidite and climat.reglages


def test_un_ecran_en_mode_ha_par_piece_de_la_demo():
    attendues = {PAGE_DE_LA_PIECE[r]: f"accueil-ha-piece-{r + 1}" for r in PIECES}
    obtenues = {}
    for ecran in ECRANS:
        _, page, ha = _jouer(ecran.etapes)
        # Le popup Maison (ADR-0037) s'ouvre en mode HA par le titre de la pièce, le carrousel
        # des clims (ADR-0038) par la température de la pièce : ils couvrent la rangée, ce
        # n'est pas l'écran d'une pièce. De même les fenêtres du climat d'une pièce
        # (ADR-0040), ouvertes en mode HA sur elle, et la roue de sa clim (ADR-0048).
        # La pièce choisie dans la roue de navigation (ADR-0042) l'est aussi par un swipe.
        if ha and not ecran.nom.startswith(("maison-", "climatisation-", "roue-clim-temperature",
                                            "roue-navigation-")) \
                and ecran.nom not in POPUPS_EN_MODE_HA:
            assert page not in obtenues, (ecran.nom, obtenues.get(page))
            obtenues[page] = ecran.nom
    assert obtenues == attendues
    assert len(PIECES) >= 4, "la démo montre au moins quatre pièces"


def _cartes(nombre):
    """Cartes du calque HA (x de début, x de fin) : centrées, pas de 250 px, 230 de large
    (switches_card.yaml ; formule de zones_apply_ui, Tab5/ecran/tab5_zones.cpp)."""
    x0 = (1280 - (nombre * 250 - 20)) // 2
    return [(x0 + 250 * i, x0 + 250 * i + 230) for i in range(nombre)]


# Rangée du bas (calques de 430 à 720, cartes à y = 5, 275 de haut) : là sont les
# boutons des tuiles et des cartes.
Y_CARTES = (435, 710)


def test_gestes_partent_hors_des_tuiles_et_des_cartes():
    """Un geste parti d'un bouton le déclenche au relâché (le bouton garde l'appui). En
    mode météo, les tuiles sont fixes ; en mode HA, les cartes se centrent selon le
    nombre d'appareils de la pièce : le geste doit éviter les cinq dispositions."""
    for ecran in ECRANS:
        trace, _, _ = _jouer(_etapes(ecran))
        for etape, _, ha in trace:
            # Un geste dans un popup (pages des Réglages) ne touche ni tuile ni carte : le
            # popup les couvre et garde le geste (tab5_reglages.cpp).
            if not isinstance(etape, Glisser) or etape.dans_popup:
                continue
            dispositions = [_cartes(n) for n in range(1, 6)] if ha else [_cartes(5)]
            for x, y in ((etape.x1, etape.y1), (etape.x2, etape.y2)):
                if not Y_CARTES[0] <= y <= Y_CARTES[1]:
                    continue
                for cartes in dispositions:
                    for debut, fin in cartes:
                        assert not debut <= x <= fin, (ecran.nom, "mode HA" if ha else "météo", x, y)


def _bouton(ident):
    """(x, y, largeur, hauteur) d'un bouton du haut : x de son inclusion dans
    Tab5/paquets/tab5-lvgl.yaml, le reste du gabarit bouton_haut.yaml (08/10/2026, audit YML-4)."""
    x = re.search(rf'file: \.\./ui_components/bouton_haut\.yaml, vars: \{{ id: {ident}, x: "(\d+)"',
                  _lire("Tab5", "paquets", "tab5-lvgl.yaml"))
    assert x, ident
    m = re.search(r"align: TOP_LEFT\s+x: \$\{x\}\s+y: (\d+)\s+width: (\d+)\s+height: (\d+)",
                  _lire("Tab5", "ui_components", "bouton_haut.yaml"))
    assert m, "bouton_haut.yaml"
    return (int(x.group(1)),) + tuple(int(v) for v in m.groups())


def test_boutons_du_haut():
    """Les boutons HA, Sys et TV, à leur place avec la TV (le rendu pousse une maison
    complète : capturer.py ne déclare aucune zone absente)."""
    assert "build_zones_absentes(frozenset())" in _lire("tools", "rendu", "capturer.py")
    for (x, y), ident in ((ecrans.BOUTON_HA, "btn_control_ha"), (ecrans.BOUTON_SYS, "btn_control_console"),
                          (ecrans.BOUTON_TV, "btn_control_tv")):
        bx, by, largeur, hauteur = _bouton(ident)
        assert bx < x < bx + largeur and by < y < by + hauteur, (ident, x, y)


def test_pages_des_reglages():
    """Les appuis sur les noms des pages des Réglages tombent au milieu de leur bouton
    (reglages_onglet.yaml : 200 × 44 à y 4 de la carte modale, posée à 15 px des bords),
    dans l'ordre des pages, et les gestes du popup y sont marqués comme tels."""
    popup = _lire("Tab5", "ui_components", "reglages_popup.yaml")
    xs = [int(x) for x in re.findall(r"file: reglages_onglet\.yaml, vars: \{ id: \w+, x: (\d+), page: \d+,", popup)]
    noms = ("ecran", "apparence", "batterie", "systeme")
    assert [ecrans.REGLAGES_PAGES[n] for n in noms] == [(15 + x + 100, 15 + 4 + 22) for x in xs]
    for geste in (ecrans.REGLAGES_GLISSER_DEPUIS_UN_BOUTON, ecrans.REGLAGES_GLISSER_CURSEUR,
                  ecrans.REGLAGES_CURSEUR_A_100):
        assert geste.dans_popup and geste.y1 == geste.y2, geste
