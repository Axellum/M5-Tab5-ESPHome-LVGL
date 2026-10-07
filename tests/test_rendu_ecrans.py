# -*- coding: utf-8 -*-
"""Plan des écrans capturés par le rendu (tools/rendu/ecrans.py) : cohérent avec le firmware.

Un nom en double écraserait une capture ; un appui hors de l'écran, une option absente
du select « Aller à l'écran » ou une action inconnue de l'API ne s'apercevraient qu'en
CI, sous la forme d'une capture identique à une autre. De même pour les pièces
(ADR-0023) : un écran qui laisserait le mode HA ou une autre page que l'accueil
décalerait tous les suivants, et un geste parti d'un bouton le déclencherait."""
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "rendu"))

import ecrans  # noqa: E402
from ecrans import BOUTON_HA, ECRANS, Aller, Glisser, Service, Toucher  # noqa: E402
from scenarios import PAGE_DE_LA_PIECE, PIECES  # noqa: E402  (tools/demo, chemin ajouté par ecrans)


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


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
    bloc = _lire("Tab5", "tab5-ha-controls.yaml").split('name: "Aller à l\'écran"', 1)[1]
    options = set(re.findall(r'^\s+- "([^"]+)"', bloc.split("on_value:", 1)[0], re.M))
    assert "Accueil" in options
    for ecran in ECRANS:
        for etape in _etapes(ecran):
            if isinstance(etape, Aller):
                assert etape.option in options, (ecran.nom, etape.option)


def test_actions_connues_de_l_api():
    textes = _lire("Tab5", "tab5-api-logic.yaml") + _lire("Tab5", "rendu", "bouchons.yaml")
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
#   boucler ; « HA » bascule le mode ; « Aller à l'écran → Accueil » le quitte.
# ---------------------------------------------------------------------------

PAGES_OCCUPEES = sorted(PAGE_DE_LA_PIECE[r] for r, p in PIECES.items() if p.tuiles)
Y_MIN_GESTE = int(re.search(r"FORECAST_SWIPE_Y_MIN = (\d+);", _lire("Tab5", "tab5_central.cpp")).group(1))


def _jouer(etapes, page=2, ha=False):
    """Page et mode HA après chaque étape : [(étape, page, ha avant l'étape)], page, ha."""
    trace = []
    for etape in etapes:
        trace.append((etape, page, ha))
        if isinstance(etape, Toucher) and (etape.x, etape.y) == BOUTON_HA:
            ha = not ha
        elif isinstance(etape, Aller) and etape.option == "Accueil":
            ha = False
        elif isinstance(etape, Glisser) and etape.y2 >= Y_MIN_GESTE and etape.x1 != etape.x2:
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


def test_un_ecran_en_mode_ha_par_piece_de_la_demo():
    attendues = {PAGE_DE_LA_PIECE[r]: f"accueil-ha-piece-{r + 1}" for r in PIECES}
    obtenues = {}
    for ecran in ECRANS:
        _, page, ha = _jouer(ecran.etapes)
        # Le popup Maison (ADR-0037) s'ouvre en mode HA par le titre de la pièce : il couvre
        # la rangée, ce n'est pas l'écran d'une pièce.
        if ha and not ecran.nom.startswith("maison-"):
            assert page not in obtenues, (ecran.nom, obtenues.get(page))
            obtenues[page] = ecran.nom
    assert obtenues == attendues
    assert len(PIECES) >= 4, "la démo montre au moins quatre pièces"


def _cartes(nombre):
    """Cartes du calque HA (x de début, x de fin) : centrées, pas de 250 px, 230 de large
    (switches_card.yaml ; formule de zones_apply_ui, Tab5/tab5_zones.cpp)."""
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
            if not isinstance(etape, Glisser):
                continue
            dispositions = [_cartes(n) for n in range(1, 6)] if ha else [_cartes(5)]
            for x, y in ((etape.x1, etape.y1), (etape.x2, etape.y2)):
                if not Y_CARTES[0] <= y <= Y_CARTES[1]:
                    continue
                for cartes in dispositions:
                    for debut, fin in cartes:
                        assert not debut <= x <= fin, (ecran.nom, "mode HA" if ha else "météo", x, y)


def _bouton(ident):
    """(x, y, largeur, hauteur) d'un bouton du haut de Tab5/tab5-lvgl.yaml."""
    m = re.search(rf"id: {ident}\s+align: TOP_LEFT\s+x: (\d+)\s+y: (\d+)\s+width: (\d+)\s+height: (\d+)",
                  _lire("Tab5", "tab5-lvgl.yaml"))
    assert m, ident
    return tuple(int(v) for v in m.groups())


def test_boutons_du_haut():
    """Les boutons HA, Sys et TV, à leur place avec la TV (le rendu pousse une maison
    complète : capturer.py ne déclare aucune zone absente)."""
    assert "build_zones_absentes(frozenset())" in _lire("tools", "rendu", "capturer.py")
    for (x, y), ident in ((ecrans.BOUTON_HA, "btn_control_ha"), (ecrans.BOUTON_SYS, "btn_control_console"),
                          (ecrans.BOUTON_TV, "btn_control_tv")):
        bx, by, largeur, hauteur = _bouton(ident)
        assert bx < x < bx + largeur and by < y < by + hauteur, (ident, x, y)
