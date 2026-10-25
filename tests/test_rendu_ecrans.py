# -*- coding: utf-8 -*-
"""Plan des écrans capturés par le rendu (tools/rendu/ecrans.py) : cohérent avec le firmware.

Un nom en double écraserait une capture ; un appui hors de l'écran, une option absente
du select « Aller à l'écran » ou une action inconnue de l'API ne s'apercevraient qu'en
CI, sous la forme d'une capture identique à une autre. De même pour les pièces
(ADR-0023) : un écran qui laisserait le mode HA ou une autre page que l'accueil
décalerait tous les suivants, et un geste parti d'un bouton le déclencherait."""
import os
import re
from tests.commun import _jetons, avec_jetons, geometrie_cpp, resoudre, lire as _lire

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
    assert "Home" in options
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
#   doigt est à y ≥ FORECAST_SWIPE_Y_MIN. Avec N > 5 pièces (« pn|N », ADR-0066), la
#   gauche continue de 4 sur les pages de pièce 5 … N − 1 avant de revenir à 2 ;
# - mode HA, tuiles_swipe_ha (tab5_tuiles.cpp, 18/10/2026) : pièce suivante (gauche) ou
#   précédente (droite) en rouleau, la N-ième revient à la 1, pièces vides comprises ;
#   « HA » bascule le mode (en sortant, la page de la pièce) ; « Aller à l'écran →
#   Accueil » le quitte ; une pièce choisie dans la roue de navigation (ADR-0042,
#   NAV_BUREAU) y met le mode HA.
# ---------------------------------------------------------------------------

PIECE_DE_LA_PAGE = {p: r for r, p in PAGE_DE_LA_PIECE.items()}


def _piece_de_la_page(page):
    return PIECE_DE_LA_PAGE.get(page, page)  # pages 5 et plus : page = pièce (ADR-0066)


def _page_de_la_piece(r):
    return PAGE_DE_LA_PIECE.get(r, r)


def _nombre_pousse(etape, n):
    """Nombre de pièces après une poussée de tab5_maj_tuiles (« pn|N » en tête ; sans lui,
    cinq), inchangé après une autre étape."""
    if isinstance(etape, Service) and etape.nom == "tab5_maj_tuiles":
        payload = dict(etape.donnees)["payload"]
        m = re.match(r"pn\|(\d+);", payload)
        return int(m.group(1)) if m else 5
    return n
# Haut de la carte centrale du Tab5 (kAccCentraleY de tab5_geometrie.h, lot 3 du portage).
assert "FORECAST_SWIPE_Y_MIN = kAccCentraleY;" in _lire("Tab5", "ecran", "tab5_central.cpp")
Y_MIN_GESTE = geometrie_cpp("tab5")["kAccCentraleY"]


def _jouer(etapes, page=2, ha=False, n=5):
    """Page et mode HA après chaque étape : [(étape, page, ha avant l'étape)], page, ha.
    `n` : nombre de pièces (« pn|N » d'une poussée de tab5_maj_tuiles, ADR-0066)."""
    trace = []
    for etape in etapes:
        trace.append((etape, page, ha))
        n = _nombre_pousse(etape, n)
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
                page = _page_de_la_piece((_piece_de_la_page(page) + (1 if gauche else n - 1)) % n)
            elif gauche:
                page = 2 if page >= max(5, n) - 1 else page + 1
            else:
                page = 2 if page <= 0 else page - 1
        if not ha and page >= max(5, n):
            page = 2  # une page de pièce disparue (moins de pièces) : l'accueil
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
        # La tuile − / + pièce par pièce (« reglables-… », 14/10/2026) a ses propres maisons,
        # comme les tuiles coupées (« accueil-ha-<n>-elements », ADR-0066 lot B : le Salon
        # chargé) ; les huit pièces (« pieces-8-… », ADR-0066 lot A) ont leurs propres captures.
        if ha and not ecran.nom.startswith(("maison-", "climatisation-", "roue-clim-temperature",
                                            "roue-navigation-", "reglables-", "pieces-8-")) \
                and not re.fullmatch(r"accueil-ha-\d+-elements", ecran.nom) \
                and ecran.nom not in POPUPS_EN_MODE_HA:
            assert page not in obtenues, (ecran.nom, obtenues.get(page))
            obtenues[page] = ecran.nom
    assert obtenues == attendues
    assert len(PIECES) >= 4, "la démo montre au moins quatre pièces"


def test_huit_pieces():
    """ADR-0066 (lot A) : huit pièces poussées avec « pn|8 » ; le mode HA sur la huitième par
    un geste vers la droite depuis la première (rouleau), la page de la sixième en mode
    météo après les jours 10-14 ; chaque écran remet la maison de la démo (cinq pièces)."""
    huit = [e for e in ECRANS if e.nom.startswith("pieces-8-")]
    assert len(huit) >= 6
    for ecran in huit:
        assert ecran.etapes[:len(ecrans.HUIT_PIECES)] == ecrans.HUIT_PIECES, ecran.nom
        assert ecran.fermer[-len(ecrans.CINQ_PIECES):] == ecrans.CINQ_PIECES, ecran.nom
        assert dict(ecran.etapes[0].donnees)["payload"].startswith("pn|8;"), ecran.nom
        assert not dict(ecran.fermer[-1].donnees)["payload"].startswith("pn|"), ecran.nom
    trace, page, ha = _jouer(next(e for e in huit if e.nom == "pieces-8-ha-atelier").etapes)
    assert ha and _piece_de_la_page(page) == 7 and page == 7
    _, page, ha = _jouer(next(e for e in huit if e.nom == "pieces-8-meteo-garage").etapes)
    assert not ha and page == 5 and _piece_de_la_page(page) == 5
    assert ecrans.PIECES_HUIT[5].nom == "Garage" and ecrans.PIECES_HUIT[7].nom == "Atelier"
    # Les deux lots ensemble (A et B) : le Garage de onze éléments, en mode HA et sur sa page
    # de la météo (pièce 6, au-delà des cinq pages de la météo).
    for nom, en_ha in (("pieces-8-ha-garage-11-elements", True), ("pieces-8-meteo-garage-11-elements", False)):
        ecran = next(e for e in huit if e.nom == nom)
        assert ecran.etapes[len(ecrans.HUIT_PIECES):][:len(ecrans.GARAGE_COUPE)] == ecrans.GARAGE_COUPE, nom
        _, page, ha = _jouer(ecran.etapes)
        assert ha is en_ha and page == 5 and _piece_de_la_page(page) == 5, nom
    assert len(ecrans.GARAGE_ONZE[5].tuiles) == 11 > 5
    assert dict(ecrans.GARAGE_COUPE[0].donnees)["payload"].startswith("pn|8;")


def test_la_tuile_moins_plus_piece_par_piece():
    """14/10/2026 (discussion #278) : la zone − / + et sa liste sur chaque pièce occupée,
    avec plusieurs clims, une seule et aucune ; chaque écran remet la maison de la démo."""
    occupees = [r for r, p in PIECES.items() if p.tuiles]
    for maison in ("clims-n", "clims-1", "clims-0"):
        for r in occupees:
            for suffixe in ("", "-liste"):
                nom = f"reglables-{maison}-piece-{r + 1}{suffixe}"
                ecran = next((e for e in ECRANS if e.nom == nom), None)
                assert ecran is not None, nom
                _, page, ha = _jouer(ecran.etapes)
                assert ha and page == PAGE_DE_LA_PIECE[r], nom
                assert ecran.etapes[:len(ecrans.REGLABLES_MAISONS[maison])] == ecrans.REGLABLES_MAISONS[maison]
                assert ecran.fermer[-len(ecrans.REGLABLES_MAISON_DE_LA_DEMO):] == ecrans.REGLABLES_MAISON_DE_LA_DEMO
                if suffixe:
                    # La liste ouverte par l'appui long sur la valeur, refermée avant « HA »
                    # (sinon le toucher de « HA » tomberait sur son capteur plein écran).
                    assert ecran.etapes[-1] == ecrans.Long(*ecrans.CONSIGNE_CLIM), nom
                    assert ecran.fermer[0] == ecrans.REGLABLES_LISTE_FERMER, nom
                    x, _y = ecran.fermer[0].x, ecran.fermer[0].y
                    assert x < 740, "hors du panneau de la liste (reglables_liste.yaml)"
    # La maison « plusieurs clims » a une tuile cli sans l'option m (sa clim de tuile).
    tuile = ecrans.CHAMBRE_AVEC_SA_CLIM[ecrans.PIECE_TUILE_CLIM].tuiles[2]
    assert tuile.type == "cli" and "m" not in tuile.options
    # La remise en état repousse le climat des cinq pièces (le Bureau retrouve sa clim).
    remise = ecrans.REGLABLES_MAISON_DE_LA_DEMO[-1].donnees[0][1]
    assert f"p{ecrans.PIECE_CLIMAT}|" in remise and f"crp{ecrans.PIECE_CLIMAT}|" in remise
    # … et l'état de la dernière scène sur la tuile de la clim du blueprint (option m) :
    # « roue-clim », plus loin, marque son mode (14/10/2026 : « cool » au lieu de
    # « fan_only » quand la remise prenait l'état par défaut de la tuile).
    etats = ecrans.REGLABLES_MAISON_DE_LA_DEMO[1].donnees[0][1]
    clim = ecrans.SCENES[-1].clim
    assert f"t{ecrans.PIECE_TUILE_CLIM}2|{clim['mode']}|{clim['current']}|;" in etats
    # La planche (tools/rendu/planche_reglables.py) lit exactement ces captures.
    import planche_reglables
    noms = {e.nom for e in ECRANS if e.nom.startswith("reglables-clims-")}
    lues = {n for _, cases in planche_reglables.plan() for _, zone, liste in cases for n in (zone, liste)}
    assert lues == noms
    assert set(planche_reglables.TITRES) == set(ecrans.REGLABLES_MAISONS)


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
    Tab5/paquets/tab5-lvgl.yaml, le reste du gabarit bouton_haut.yaml (08/10/2026, audit YML-4) ;
    jetons acc_* (lot 3 du portage 1024×600) résolus pour le Tab5."""
    x = re.search(rf'file: \.\./ui_components/bouton_haut\.yaml, vars: \{{ id: {ident}, x: "(\d+)"',
                  avec_jetons(_lire("Tab5", "paquets", "tab5-lvgl.yaml")))
    assert x, ident
    m = re.search(r"align: TOP_LEFT\s+x: \$\{x\}\s+y: (\d+)\s+width: (\d+)\s+height: (\d+)",
                  avec_jetons(_lire("Tab5", "ui_components", "bouton_haut.yaml")))
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
    xs = [int(x) for x in re.findall(r"file: reglages_onglet\.yaml, vars: \{ prefixe: reglages, id: \w+, x: (\d+), page: \d+,", popup)]
    noms = ("ecran", "apparence", "batterie", "systeme")
    assert [ecrans.REGLAGES_PAGES[n] for n in noms] == [(15 + x + 100, 15 + 4 + 22) for x in xs]
    for geste in (ecrans.REGLAGES_GLISSER_DEPUIS_UN_BOUTON, ecrans.REGLAGES_GLISSER_CURSEUR,
                  ecrans.REGLAGES_CURSEUR_A_100):
        assert geste.dans_popup and geste.y1 == geste.y2, geste


def test_pages_du_reveil():
    """Les appuis sur les noms des pages du Réveil tombent au milieu de leur bouton (même
    gabarit que les Réglages, largeur passée en `w`), dans l'ordre des pages ; « Tester » et
    le préréglage d'où part le geste, au milieu des leurs (pages à y 72 de la carte
    modale, cartes de la page à x 24 et 637)."""
    popup = _lire("Tab5", "ui_components", "alarm_popup.yaml")
    onglets = re.findall(r"file: reglages_onglet\.yaml, vars: \{ prefixe: reveil, id: \w+, x: (\d+), w: (\d+), page: \d+,", popup)
    noms = ("heure", "jours", "ouverture", "sonnerie", "annonces")
    assert len(onglets) == len(noms)
    assert [ecrans.REVEIL_PAGES[n] for n in noms] == [(15 + int(x) + int(w) // 2, 15 + 4 + 22) for x, w in onglets]
    # « Tester » : bouton de la carte de droite (x 637) de la page Heure.
    # Taille ramenée au corps de la carte (lot 4 du portage) : les valeurs du Tab5.
    m = re.search(r"id: btn_alarm_test\s+align: TOP_LEFT\s+x: (.+)\s+y: (.+)\s+width: (.+)\s+height: (.+)", popup)
    assert m, "btn_alarm_test"
    x, y, w, h = (int(resoudre(v.strip(), _jetons("tab5"))) for v in m.groups())
    assert ecrans.REVEIL_TESTER == (15 + 637 + x + w // 2, 15 + 72 + y + h // 2)
    # Le geste part du préréglage « Week-end » (carte de la page Jours à x 24).
    m = re.search(r"id: btn_alarm_jours_3, x: (\d+), y: (\d+), w: (\d+),", popup)
    assert m, "btn_alarm_jours_3"
    x, y, w = (int(v) for v in m.groups())
    geste = ecrans.REVEIL_GLISSER_DEPUIS_UN_BOUTON
    assert geste.dans_popup and geste.y1 == geste.y2 and geste.x2 < geste.x1, geste
    assert 15 + 24 + x < geste.x1 < 15 + 24 + x + w and 15 + 72 + y < geste.y1 < 15 + 72 + y + 60, geste


def test_captures_remises_a_l_endroit_selon_la_carte():
    """Le BMP est la dalle (ADR-0061) : Tab5 tourné de 270°, Waveshare 7B de 180° (dalle
    montée à l'envers), Neon Apron jamais ; la rotation vient de geometrie-<carte>.yaml."""
    import capturer

    assert capturer.rotation_carte("tab5") == 270
    assert capturer.rotation_carte("waveshare-7b") == 180
    assert capturer.angle_png("0-accueil", 720, 1280, 270) == 270
    assert capturer.angle_png("0-accueil", 1024, 600, 180) == 180
    assert capturer.angle_png(next(iter(capturer.PORTRAITS)), 720, 1280, 270) == 0
