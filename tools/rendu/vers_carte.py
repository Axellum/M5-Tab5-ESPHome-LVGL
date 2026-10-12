# -*- coding: utf-8 -*-
"""tools/rendu/vers_carte.py — Les écrans de tools/rendu/ecrans.py joués sur une autre carte
que le Tab5 (ADR-0061, lot 2 du portage 1024×600).

Les étapes d'ecrans.py sont écrites pour l'écran du Tab5 (1280×720). Sur une autre carte,
`adapter(ecran, carte)` les rejoue au même endroit de la mise en page, avec trois règles :

1. Un geste dont le Tab5 sait l'écran qu'il ouvre et dont le point tombe hors de l'écran de
   la carte (l'accueil n'est pas encore à sa taille : engrenage, manette, tuile − / +…) est
   remplacé par la navigation, comme le ferait Home Assistant : « Aller à l'écran », ou
   l'action rendu_ecran (Tab5/rendu/bouchons.yaml) pour l'Arcade, qui n'est pas une option.
2. Les boutons des roues (ADR-0036, ADR-0042) sont recalculés pour l'écran de la carte,
   autour de leur ancre ramenée sur la carte (disposer() de tab5_roue.cpp dépend de la
   taille de l'écran).
3. Les autres points suivent la mise en page : sur l'accueil, la colonne de l'horloge et la
   carte centrale sont centrées (décalées de la moitié de l'écart de largeur), le reste est
   en place ; dans un popup, la croix est collée au bord droit de la carte modale, les noms
   des pages réduits depuis elle (onglet_x de tab5_geometrie.h), le corps ramené à la taille
   du corps (kCorpsW × kCorpsH). Un point qui reste hors de l'écran arrête l'écran là (ses
   étapes suivantes et `fermer` ne sont pas jouées) : il ne s'ouvrira pas, le bilan
   (tools/rendu/bilan_carte.py) le compte.

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
# Accueil : au-dessus des tuiles (y 455 du Tab5), la colonne de l'horloge (x 440 à 840) et la
# carte centrale sont centrées dans l'écran ; le reste est posé à gauche, au pixel.
ACCUEIL_TUILES_Y = 455
ACCUEIL_CENTRE = (440, 840)
# Valeur d'Ecran::ARCADE (Tab5/ecran/tab5_zones.h) : tests/test_vers_carte.py la relit.
ECRAN_ARCADE = 22
# Marge des points ramenés dans l'écran (gestes, toucher qui ferme une roue).
BORD = 15


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

    def accueil(self, x: int, y: int) -> tuple[int, int]:
        if y < ACCUEIL_TUILES_Y and ACCUEIL_CENTRE[0] <= x <= ACCUEIL_CENTRE[1]:
            return x + self.dx // 2, y
        return x, y

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
    roues[E.NAV_BUREAU] = (E.Toucher(*choix[1]), False)
    salon = ancre(E.SALON)
    clim = E.roue_centres(min(max(salon[0], 480), 800), E.ROUE_CLIM_ANCRE[1], E.ROUE_CLIM_TEMPERATURE, ecran=ecran)
    roues[E.ROUE_CLIM_DETAILS] = (E.Toucher(*clim[-1]), True)
    roues[E.ROUE_CLIM_CLIMS] = (E.Toucher(*clim[E.ROUE_CLIMS]), False)
    return roues


def _ouvertures(c: Carte) -> dict:
    """Gestes de l'accueil qui ouvrent un écran : étape du Tab5 → navigation (règle 1)."""
    return {
        E.Toucher(*E.BOUTON_SYS): E.Aller("Réglages"),
        E.Long(*E.BOUTON_SYS): E.Aller("Console système"),
        E.Long(*E.BOUTON_TV): E.Aller("Télécommande TV"),
        E.Toucher(*E.BOUTON_TV, apres=1.0): E.Service("rendu_ecran", (("ecran", ECRAN_ARCADE),), apres=1.0),
        E.Long(*E.HEURES): E.Aller("Réveil"),
        E.Long(*E.DATE): E.Aller("Calendrier"),
        E.Long(*E.MICRO): E.Aller("Assistant vocal"),
        E.Long(*E.SOUS_HORLOGE): E.Aller("Plantes"),
        E.Toucher(*E.CONSIGNE_CLIM): E.Aller("Climatisation"),
        E.Long(*E.SERRE): E.Aller("Température"),
        E.Long(*E.SALON_TEMP): E.Aller("Température"),
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
    dans_popup = False
    retirees: list = []

    def point(x: int, y: int) -> tuple[int, int]:
        return c.popup(x, y) if dans_popup else c.accueil(x, y)

    def une(etape):
        nonlocal dans_popup
        if isinstance(etape, E.Aller):
            dans_popup = etape.option != "Accueil"
            return etape
        if isinstance(etape, E.Toucher):
            if etape in explicites:
                return explicites[etape]
            if etape in roues:
                nouvelle, ouvre = roues[etape]
                dans_popup = dans_popup or ouvre
                return replace(etape, x=nouvelle.x, y=nouvelle.y)
            if not dans_popup and etape in ouvertures:
                x, y = c.accueil(etape.x, etape.y)
                if not c.dedans(x, y):
                    navigation = ouvertures[etape]
                    dans_popup = isinstance(navigation, E.Aller)   # l'Arcade est une page
                    return navigation
            x, y = point(etape.x, etape.y)
            if not c.dedans(x, y):
                retirees.append(etape)
                return None
            autres = tuple((s, *point(sx, sy)) for s, sx, sy in etape.selon_langue)
            if etape == FERMER:
                dans_popup = False
            elif not dans_popup and (etape in ouvertures or etape in popups):
                dans_popup = True
            return replace(etape, x=x, y=y, selon_langue=autres)
        if isinstance(etape, E.Glisser):
            regle = c.popup if (etape.dans_popup or dans_popup) else c.accueil
            x1, y1 = c.borner(*regle(etape.x1, etape.y1))
            x2, y2 = c.borner(*regle(etape.x2, etape.y2))
            return replace(etape, x1=x1, y1=y1, x2=x2, y2=y2)
        return etape

    # Un geste impossible arrête l'écran là : la suite toucherait ce que l'écran affiche à
    # la place (un autre jeu du sélecteur, une autre tuile). `fermer` défait ce que les
    # étapes ont fait ; sans elles jusqu'au bout, rien : le retour à l'accueil suffit.
    etapes = []
    for etape in ecran.etapes:
        nouvelle = une(etape)
        if nouvelle is None:
            return tuple(etapes), (), retirees
        etapes.append(nouvelle)
    fermer = []
    for etape in ecran.fermer:
        nouvelle = une(etape)
        if nouvelle is None:
            break
        fermer.append(nouvelle)
    return tuple(etapes), tuple(fermer), retirees
