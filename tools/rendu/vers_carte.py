# -*- coding: utf-8 -*-
"""tools/rendu/vers_carte.py — Les écrans de tools/rendu/ecrans.py joués sur une autre carte
que le Tab5 (ADR-0061, lot 2 du portage 1024×600).

Les étapes d'ecrans.py sont écrites pour l'écran du Tab5 (1280×720). Sur une autre carte,
`adapter(ecran, carte)` les rejoue au même endroit de la mise en page, avec trois règles :

1. Un geste dont le Tab5 sait l'écran qu'il ouvre et dont le point tomberait hors de l'écran
   de la carte est remplacé par la navigation, comme le ferait Home Assistant : « Aller à
   l'écran », ou l'action rendu_ecran (Tab5/rendu/bouchons.yaml) pour l'Arcade, qui n'est pas
   une option. Depuis le lot 3 (l'accueil ramené à l'écran de la carte), un point de
   l'accueil tombe toujours dans l'écran : la règle ne se déclenche plus, elle reste en filet.
   Le geste est joué pour de vrai, et la table dit seulement ce qu'il ouvre (un popup, ou le
   sélecteur de l'Arcade).
2. Les boutons des roues (ADR-0036, ADR-0042) sont recalculés pour l'écran de la carte,
   autour de leur ancre ramenée sur la carte (disposer() de tab5_roue.cpp dépend de la
   taille de l'écran).
3. Les autres points suivent la mise en page : l'accueil est celui du Tab5 ramené à l'écran
   de la carte (lot 3, jetons acc_* de tab5-ui-tokens.yaml : x × l / 1280, y × h / 720,
   arrondis) ; dans un popup, la croix est collée au bord droit de la carte modale, les noms
   des pages réduits depuis elle (onglet_x de tab5_geometrie.h), le corps ramené à la taille
   du corps (kCorpsW × kCorpsH) ; le popup Musique (« Aller à l'écran → Musique ») n'est
   pas la carte modale : sa carte (lecteur_card_w × lecteur_card_h, 940 × 536) est la même
   sur toutes les cartes, centrée, et un point y garde sa place dans elle (ADR-0067 : sur la
   7B, x − 128, y − 60), roues de ses pages comprises ; le sélecteur de l'Arcade est centré
   en largeur, les consoles restent au pixel. Un point qui reste hors de l'écran arrête l'écran là (ses
   étapes suivantes ne sont pas jouées) : il ne s'ouvrira pas, le bilan
   (tools/rendu/bilan_carte.py) le compte. `fermer` garde alors ses étapes qui ne sont pas
   des gestes (le thème remis, une option rétablie) : sans elles, l'écran suivant hériterait
   de l'état laissé (un thème changé, toutes les captures suivantes dans ce thème).

Sur le Tab5, rien ne change : `adapter` rend les étapes telles quelles (les captures de
référence restent identiques). Module pur (stdlib), vérifié par tests/test_vers_carte.py.
"""
from __future__ import annotations

import re
from dataclasses import replace
from pathlib import Path

import ecrans as E

RACINE = Path(__file__).resolve().parents[2]
TAB5 = (1280, 720)

# Carte modale (tab5-ui-tokens.yaml) : 15 px des bords de l'écran, corps à x 24, y 72 de la
# carte ; croix de 80 px à 10 px du bord droit (modal_header.yaml).
MARGE_CARTE = 15
CORPS_X, CORPS_Y = 24, 72
CROIX = 10 + 80
# Onglets d'en-tête du Tab5 (tab5_geometrie.h : kOngletsDebutTab5, kOngletsFinTab5) et leur
# fin sur une carte (kOngletsFin = kCarteL - 102).
ONGLETS_DEBUT_TAB5, ONGLETS_FIN_TAB5 = 196, 1148
ONGLETS_AVANT_LE_BORD = 102
# Accueil (tab5-lvgl.yaml, lot 3 du portage) : la mise en page du Tab5 ramenée à l'écran de
# la carte, comme acc_x / acc_y de tab5_geometrie.h : (v × l + 640) // 1280 en x,
# (v × h + 360) // 720 en y. Ancre basse de la roue d'une clim (clim_ancre_basse,
# tab5_tuiles_roue.cpp) : x ramené dans [480, 800], y 460 sur le Tab5.
CLIM_ANCRE_X, CLIM_ANCRE_Y = (480, 800), 460
# Valeur d'Ecran::ARCADE (Tab5/ecran/tab5_zones.h) : tests/test_vers_carte.py la relit.
ECRAN_ARCADE = 22
# Marge des points ramenés dans l'écran (gestes, toucher qui ferme une roue).
BORD = 15
# Carte du popup Musique (tab5-ui-tokens.yaml : lecteur_card_w, lecteur_card_h), centrée,
# la même sur toutes les cartes (ADR-0050, ADR-0067).
LECTEUR_CARTE = (940, 536)
# Où le doigt touche (adapter) : l'accueil, un popup, le popup Musique, le sélecteur de
# l'Arcade, une console.
ACCUEIL, POPUP, LECTEUR, ARCADE, JEU = "accueil", "popup", "lecteur", "arcade", "jeu"


def jeux_compiles(carte: str) -> bool:
    """Le rendu de cette carte a-t-il les jeux ? `tab5_jeux` par défaut (ADR-0065) : oui sur
    le Tab5 seulement, non sur une carte sans IMU (tests/test_module_jeux.py compare avec
    l'expression de tab5-rendu-host.yaml). Sans eux, ni l'Arcade ni les consoles."""
    return carte == "tab5"


def taille_carte(carte: str) -> tuple[int, int]:
    """Écran logique (paysage) de la carte : Tab5/paquets/geometrie-<carte>.yaml."""
    texte = (RACINE / "Tab5" / "paquets" / f"geometrie-{carte}.yaml").read_text(encoding="utf-8")
    return tuple(int(re.search(rf'^\s*carte_ecran_{c}:\s*"?(\d+)', texte, re.M).group(1)) for c in "lh")


class Carte:
    """Règles de conversion d'un point du Tab5 vers l'écran `taille` (l, h)."""

    def __init__(self, taille: tuple[int, int]):
        self.l, self.h = taille
        self.dx, self.dy = self.l - TAB5[0], self.h - TAB5[1]
        carte_l = self.l - 2 * MARGE_CARTE
        self.corps = (carte_l - 2 * CORPS_X, self.h - 2 * MARGE_CARTE - CORPS_Y - 20)
        self.corps_tab5 = (TAB5[0] - 2 * MARGE_CARTE - 2 * CORPS_X, TAB5[1] - 2 * MARGE_CARTE - CORPS_Y - 20)
        self.onglets_fin = carte_l - ONGLETS_AVANT_LE_BORD
        zone_tab5 = ONGLETS_FIN_TAB5 - ONGLETS_DEBUT_TAB5
        self.onglets = (min(self.onglets_fin - ONGLETS_DEBUT_TAB5, zone_tab5), zone_tab5)

    def dedans(self, x: int, y: int) -> bool:
        return 0 <= x < self.l and 0 <= y < self.h

    def acc_x(self, v: int) -> int:
        return (v * self.l + TAB5[0] // 2) // TAB5[0]

    def acc_y(self, v: int) -> int:
        return (v * self.h + TAB5[1] // 2) // TAB5[1]

    def accueil(self, x: int, y: int) -> tuple[int, int]:
        return self.acc_x(x), self.acc_y(y)

    def popup(self, x: int, y: int) -> tuple[int, int]:
        cx, cy = x - MARGE_CARTE, y - MARGE_CARTE
        carte_l_tab5 = TAB5[0] - 2 * MARGE_CARTE
        if cy < CORPS_Y:
            if cx >= carte_l_tab5 - CROIX:              # la croix, collée au bord droit
                cx += self.dx
            elif cx >= ONGLETS_DEBUT_TAB5:              # les noms des pages, depuis la croix
                n, d = self.onglets
                cx = self.onglets_fin - round((ONGLETS_FIN_TAB5 - cx) * n / d)
        else:                                           # le corps, à la taille du corps
            cx = CORPS_X + round((cx - CORPS_X) * self.corps[0] / self.corps_tab5[0])
            cy = CORPS_Y + round((cy - CORPS_Y) * self.corps[1] / self.corps_tab5[1])
        return cx + MARGE_CARTE, cy + MARGE_CARTE

    def lecteur(self, x: int, y: int) -> tuple[int, int]:
        """Popup Musique : la même place dans sa carte, centrée sur les deux écrans."""
        l, h = LECTEUR_CARTE
        return x - (TAB5[0] - l) // 2 + (self.l - l) // 2, y - (TAB5[1] - h) // 2 + (self.h - h) // 2

    def arcade(self, x: int, y: int) -> tuple[int, int]:
        """Sélecteur de l'Arcade : la grille des consoles centrée en largeur, au pixel en
        hauteur (capture arcade.png de la 7B, lot 1)."""
        return x + self.dx // 2, y

    def borner(self, x: int, y: int) -> tuple[int, int]:
        return min(max(x, BORD), self.l - BORD), min(max(y, BORD), self.h - BORD)


def _roues(c: Carte) -> dict:
    """Boutons des roues d'ecrans.py (étape du Tab5 → même bouton sur la carte), et s'il
    ouvre un popup (« Détails », « Alertes »)."""
    ecran = (c.l, c.h)

    def ancre(p: tuple[int, int]) -> tuple[int, int]:
        return c.accueil(*p)

    roues: dict = {}
    tuiles = (*E.TUILES.values(), E.TUILE_VOLET)
    for t in tuiles:
        for n in set(E.ROUE_BOUTONS.values()):
            centres = E.roue_centres(*ancre(t), n, ecran=ecran)
            for i in range(n):
                roues[E.roue_famille(t, n, i)] = (E.Toucher(*centres[i]), i == n - 1)
    nav = E.roue_centres(*ancre(E.CARTE_CENTRALE), E.ROUE_NAVIGATION, ecran=ecran)
    for i in range(E.ROUE_NAVIGATION):
        roues[E.nav(i)] = (E.Toucher(*nav[i]), i == E.NAV_ALERTES)
    choix = E.roue_choix_centres(*ancre(E.CARTE_CENTRALE), E.ROUE_NAVIGATION, E.NAV_PIECES,
                                 1 + len(E.PIECES_OCCUPEES), ecran=ecran)
    # Le Bureau : le bouton de sa pièce dans la famille Pièces (Maison d'abord), comme
    # NAV_BUREAU d'ecrans.py. Le premier (choix[1]) mettait la 7B sur la pièce 1, et le
    # retour laissait les prévisions sur la page 3 jusqu'à « energie-flux » (18/10/2026).
    roues[E.NAV_BUREAU] = (E.Toucher(*choix[1 + E.PIECES_OCCUPEES.index(E.PIECE_CLIMAT)]), False)
    salon = ancre(E.SALON)
    xmin, xmax = (c.acc_x(v) for v in CLIM_ANCRE_X)
    clim = E.roue_centres(min(max(salon[0], xmin), xmax), c.acc_y(CLIM_ANCRE_Y), E.ROUE_CLIM_TEMPERATURE, ecran=ecran)
    roues[E.ROUE_CLIM_DETAILS] = (E.Toucher(*clim[-1]), True)
    roues[E.ROUE_CLIM_CLIMS] = (E.Toucher(*clim[E.ROUE_CLIMS]), False)
    # Roue d'une carte de la page Pièces du popup Musique (ADR-0067) : son ancre garde sa
    # place dans la carte Musique ; le bouton reste dans le popup.
    musique = E.roue_centres(*c.lecteur(*E.MUSIQUE_SALON_ICONE), E.MUSIQUE_ROUE_SALON, ecran=ecran)
    roues[E.MUSIQUE_SOURCE] = (E.Toucher(*musique[0]), False)
    return roues


def _ouvertures(c: Carte) -> dict:
    """Gestes de l'accueil qui ouvrent un écran : étape du Tab5 → navigation (règle 1 ; le
    type de la navigation dit aussi où le geste joué mène : popup ou Arcade)."""
    return {
        E.Toucher(*E.BOUTON_SYS): E.Aller("Settings"),
        E.Long(*E.BOUTON_SYS): E.Aller("System console"),
        E.Long(*E.BOUTON_TV): E.Aller("TV remote"),
        E.Toucher(*E.BOUTON_TV, apres=1.0): E.Service("rendu_ecran", (("ecran", ECRAN_ARCADE),), apres=1.0),
        E.Long(*E.HEURES): E.Aller("Alarm"),
        E.Long(*E.DATE): E.Aller("Calendar"),
        E.Long(*E.MICRO): E.Aller("Voice assistant"),
        E.Long(*E.SOUS_HORLOGE): E.Aller("Plants"),
        E.Toucher(*E.CONSIGNE_CLIM): E.Aller("Air conditioning"),
        E.Long(*E.SERRE): E.Aller("Temperature"),
        E.Long(*E.SALON_TEMP): E.Aller("Temperature"),
    }


# Gestes de l'accueil qui ouvrent un popup sans passer par une option (tuiles, carte
# centrale en mode HA) : ce qui suit est dans le popup. La température de la pièce (SALON)
# ouvre une roue ou le carrousel : ses gestes sont marqués `dans_popup` ou sont des roues.
def _ouvrent_un_popup() -> set:
    return {E.Toucher(*E.TUILE_SOLAIRE), E.Long(*E.TUILE_ORDINATEUR), E.Long(*E.TUILE_SCENE),
            E.Long(*E.TUILE_JE_PARS), E.Toucher(*E.CARTE_CENTRALE)}


# La croix d'un popup : après elle, on est revenu à l'accueil.
FERMER = E.Toucher(*E.FERMER_POPUP)


def _explicites(c: Carte) -> dict:
    """Touchers posés hors de toute mise en page (coin d'écran, voile du lecteur)."""
    return {
        E.ROUE_FERMER: E.Toucher(E.ROUE_FERMER.x, c.h - 20),
        E.LECTEUR_VOILE: E.Toucher(BORD, c.h // 2),
    }


def adapter(ecran: E.Ecran, carte: str = "tab5", taille: tuple[int, int] | None = None) -> tuple[tuple, tuple, list]:
    """(étapes, fermer, retirées) de l'écran sur la carte. Sur le Tab5 : telles quelles."""
    taille = taille or (TAB5 if carte == "tab5" else taille_carte(carte))
    if taille == TAB5:
        return ecran.etapes, ecran.fermer, []
    c = Carte(taille)
    roues, ouvertures, explicites, popups = _roues(c), _ouvertures(c), _explicites(c), _ouvrent_un_popup()
    # Où est le doigt : ACCUEIL, POPUP (carte modale), ARCADE (le sélecteur, centré en
    # largeur seulement) ou JEU (une console : posée au pixel, pas encore à la taille).
    mode = ACCUEIL
    retirees: list = []

    def regle(m: str):
        return {ACCUEIL: c.accueil, POPUP: c.popup, LECTEUR: c.lecteur, ARCADE: c.arcade}.get(m, lambda x, y: (x, y))

    def une(etape):
        nonlocal mode
        if isinstance(etape, E.Aller):
            mode = {"Home": ACCUEIL, "Music": LECTEUR}.get(etape.option, POPUP)
            return etape
        if isinstance(etape, E.Service) and etape.nom == "rendu_ecran":
            mode = ARCADE
            return etape
        if isinstance(etape, E.Toucher):
            if etape in explicites:
                return explicites[etape]
            if etape in roues:
                nouvelle, ouvre = roues[etape]
                mode = POPUP if ouvre else mode
                return replace(etape, x=nouvelle.x, y=nouvelle.y)
            if mode == ACCUEIL and etape in ouvertures:
                x, y = c.accueil(etape.x, etape.y)
                if not c.dedans(x, y):
                    navigation = ouvertures[etape]
                    mode = POPUP if isinstance(navigation, E.Aller) else ARCADE
                    return navigation
            x, y = regle(mode)(etape.x, etape.y)
            if not c.dedans(x, y):
                retirees.append(etape)
                return None
            autres = tuple((s, *regle(mode)(sx, sy)) for s, sx, sy in etape.selon_langue)
            if etape == FERMER and mode == POPUP:
                mode = ACCUEIL
            elif mode == ACCUEIL and etape in ouvertures:
                # Le geste joué ouvre ce que sa navigation ouvrirait : un popup, ou l'Arcade.
                mode = POPUP if isinstance(ouvertures[etape], E.Aller) else ARCADE
            elif mode == ACCUEIL and etape in popups:
                mode = POPUP
            elif mode == ARCADE:
                mode = JEU
            return replace(etape, x=x, y=y, selon_langue=autres)
        if isinstance(etape, E.Glisser):
            r = c.popup if etape.dans_popup and mode != LECTEUR else regle(mode)
            x1, y1 = c.borner(*r(etape.x1, etape.y1))
            x2, y2 = c.borner(*r(etape.x2, etape.y2))
            return replace(etape, x1=x1, y1=y1, x2=x2, y2=y2)
        return etape

    # Un geste impossible arrête l'écran là : la suite toucherait ce que l'écran affiche à
    # la place (un autre jeu du sélecteur, une autre tuile). Dans `fermer`, les gestes
    # s'arrêtent aussi au premier impossible (ou dès le début si les étapes ont été coupées :
    # ils défont des gestes qui n'ont pas eu lieu), mais les étapes sans geste (Choisir,
    # Service, Aller, Attendre) restent : elles remettent l'état commun (thème, options).
    # Un écran resté sur l'accueil dont les gestes de `fermer` sont impossibles n'est pas
    # joué : le retour à l'accueil (capturer.py) referme un popup ou une console, pas un
    # changement fait sur l'accueil (l'appareil suivant de la tuile − / +).
    etapes = []
    gestes_ok = True
    for etape in ecran.etapes:
        nouvelle = une(etape)
        if nouvelle is None:
            gestes_ok = False
            break
        etapes.append(nouvelle)
    sur_accueil, complet = mode == ACCUEIL, gestes_ok
    fermer = []
    for etape in ecran.fermer:
        geste = isinstance(etape, (E.Toucher, E.Glisser))
        if geste and not gestes_ok:
            continue
        nouvelle = une(etape)
        if nouvelle is None:
            gestes_ok = False
            continue
        fermer.append(nouvelle)
    if complet and not gestes_ok and sur_accueil:
        return (), tuple(e for e in fermer if not isinstance(e, (E.Toucher, E.Glisser))), retirees
    return tuple(etapes), tuple(fermer), retirees
