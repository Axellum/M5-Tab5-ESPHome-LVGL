# -*- coding: utf-8 -*-
"""tools/rendu/ecrans.py — Les écrans que le rendu capture après les scènes du mode démo.

Chaque écran s'ouvre depuis l'accueil comme sur la dalle : appuis du doigt virtuel du
rendu (Toucher, Glisser : coordonnées LOGIQUES, celles des captures PNG, paysage
1280×720, portrait 720×1280 dans Neon Apron), ou select HA « Aller à l'écran » (Aller),
ou une poussée de HA (Service). tools/rendu/capturer.py joue les étapes, capture, joue
`fermer` puis revient à l'accueil par « Aller à l'écran » → Accueil, qui referme
fenêtres, sous-fenêtres et jeu en cours. Ce retour ne remet ni la page des prévisions
ni, avant la 3.2, le mode HA : un écran qui les change les rétablit dans `fermer`
(page 2, mode météo ; tests/test_rendu_ecrans.py le vérifie).

Les coordonnées viennent des captures et des positions déclarées dans Tab5/paquets/*.yaml,
Tab5/ui_components/*.yaml et les *_game.cpp (inventaire du 27/09/2026). Si la mise en
page change, capturer.py signale une capture identique à une autre : l'appui est tombé
à côté.

Module pur (stdlib, et tools/demo/scenarios.py), vérifié par tests/test_rendu_ecrans.py.
"""
from __future__ import annotations

import datetime as _dt
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "demo"))
from scenarios import (JOURS_FR, NABU, NABU_TROIS_LIGNES, NABU_UNE_LIGNE, PAGE_DE_LA_PIECE,  # noqa: E402
                       PIECES, RANGEE, REGLABLES, SCENES, SERVEUR_IA_SCENES, HeureForecast, JourForecast, Piece,
                       Tuile, build_alerte_payload, build_etats_tuiles, build_heures_bulk_payload, build_historique,
                       build_jours_bulk_payload, build_pluie_1h_bulk_payload, build_serveur_ia_payload,
                       build_tuiles_payload, code_pluie)


@dataclass(frozen=True)
class Toucher:
    """Appui du doigt en (x, y). `duree` en ms : 1000 et plus pour un appui long.
    `selon_langue` : ((suffixe, x, y), …), un autre point pour les captures de ce suffixe."""
    x: int
    y: int
    duree: int = 150
    apres: float = 0.6
    selon_langue: tuple = ()

    def point(self, suffixe: str) -> tuple[int, int]:
        return next(((x, y) for s, x, y in self.selon_langue if s == suffixe), (self.x, self.y))


def Long(x: int, y: int, apres: float = 0.8) -> Toucher:  # noqa: N802 — se lit comme une étape
    """Appui long (on_long_press d'LVGL : 400 ms par défaut, 1,2 s par sécurité)."""
    return Toucher(x, y, duree=1200, apres=apres)


@dataclass(frozen=True)
class Glisser:
    """Geste du doigt de (x1, y1) à (x2, y2), 300 ms. `dans_popup` : le geste reste dans
    un popup ouvert (pages des Réglages), il ne change ni les prévisions ni la pièce."""
    x1: int
    y1: int
    x2: int
    y2: int
    apres: float = 0.8
    dans_popup: bool = False


@dataclass(frozen=True)
class Aller:
    """Option du select HA « Aller à l'écran » (Tab5/paquets/tab5-ha-controls.yaml)."""
    option: str
    apres: float = 0.8


@dataclass(frozen=True)
class Service:
    """Action de l'API du rendu (tab5_maj_* comme HA, ou rendu_*) : `donnees` = paires."""
    nom: str
    donnees: tuple = ()
    apres: float = 0.8


@dataclass(frozen=True)
class Choisir:
    """Option d'un select de la tablette (nom et options de Tab5/paquets/*.yaml, jamais
    traduits : « Thème » → « Almanach imprimé »). Un thème se repeint à la boucle
    suivante : `apres` lui laisse le temps."""
    select: str
    option: str
    apres: float = 2.0


@dataclass(frozen=True)
class Attendre:
    secondes: float


@dataclass(frozen=True)
class Ecran:
    """Un écran à capturer : `nom` = nom du fichier (ASCII, tirets, sans suffixe de langue)."""
    nom: str
    etapes: tuple
    fermer: tuple = ()
    attente: float = 1.2
    portrait: bool = False
    # False : l'image change d'un run à l'autre sans que l'écran ait changé (graine tirée
    # de l'horloge monotone, qui n'est pas figée) ; capturée, mais pas comparée.
    stable: bool = True


# ---------------------------------------------------------------------------
# Données que HA pousserait, pour que les fenêtres ne soient pas vides. Date figée
# des captures : mardi 16 juin 2026, 07:45 à Paris (rendu-host.yml).
# ---------------------------------------------------------------------------

def _calendrier_juin_2026() -> tuple:
    """Arguments de tab5_maj_calendrier_mois : travail en semaine, un rendez-vous, un
    anniversaire, une fête, deux jours de vacances scolaires (codes : bits 1 travail,
    2 férié, 4 vacances, 8 rendez-vous, 16 anniversaire)."""
    codes, heures, details = [], [], []
    for jour in range(1, 32):
        code, horaire, lignes = 0, "", []
        if jour <= 30 and _dt.date(2026, 6, jour).weekday() < 5:
            code, horaire = 1, "09:00-17:30"
            lignes.append("travail|09:00-17:30")
        if jour == 18:
            code |= 8
            lignes.append("rdv|Dentiste 14:30")
        if jour == 21:
            lignes.append("fete|Fête de la musique")
        if jour == 24:
            code |= 16
            lignes.append("anniv|Anniversaire de Léa")
        if jour in (29, 30):
            code |= 4
            lignes.append("vacances|Vacances scolaires")
        codes.append(f"{code:02x}")
        heures.append(horaire)
        details.append(";".join(lignes))
    return (("annee", "2026"), ("mois", "6"), ("codes", "".join(codes)),
            ("heures", "|".join(heures)), ("details", "~".join(details)))


def _epoch(annee: int, mois: int, jour: int, heure: int, minute: int) -> int:
    """Epoch UTC d'une heure de Paris en juin (UTC+2)."""
    return int(_dt.datetime(annee, mois, jour, heure - 2, minute, tzinfo=_dt.timezone.utc).timestamp())


RDV = f"{_epoch(2026, 6, 18, 14, 30)}|Dentiste~{_epoch(2026, 6, 19, 9, 0)}|Réunion équipe"

REPONSE_ASSISTANT = (
    "**Demain à la maison** : ciel voilé, 24 °C l'après-midi.\n\n"
    "- Pluie : 10 % vers 18 h\n"
    "- Volet de la serre : fermé\n"
    "- Prochain rendez-vous : *Dentiste*, jeudi 14:30\n\n"
    "| Heure | Temp. | Pluie |\n|---|---|---|\n| 09:00 | 18 ° | 0 % |\n| 15:00 | 24 ° | 5 % |"
)

ALERTES_HA = "update.home_assistant_core_update|Rouge|@maj:Home Assistant Core;ha:unavailable|Orange|@indispo:3"
# Six alertes à lire (lot 3 des alertes, 06/10/2026) : en-tête « @n:6 » et les quatre
# premières, rangées par HA (rouge d'abord) ; chaque bandeau affiche son rang, « 2/6 ».
ALERTES_HA_SIX = ("@n:6;binary_sensor.fuite_cuisine#1|Rouge|Fuite cuisine;"
                  "update.home_assistant_core_update#1|Rouge|@maj:Home Assistant Core;"
                  "ha:indispo#1|Orange|@indispo:3;sensor.porte_entree_batterie#1|Orange|Porte entrée 12 %")

# Historique des alertes (popup « Alertes », lot 4 du plan des alertes du 06/10/2026),
# autour du jour figé : à lire, lue, terminée, d'aujourd'hui (l'heure seule) et des jours
# d'avant (« Lun 15 16 h 00 »). « apparue|lue|terminée|gravité|libellé », la plus récente
# d'abord, comme packages/tab5_push.yaml (script tab5_push_alertes_historique).
HISTORIQUE_ALERTES = ";".join(
    f"{_epoch(2026, 6, *a)}|{_epoch(2026, 6, *l) if l else 0}|{_epoch(2026, 6, *f) if f else 0}|{g}|{t}"
    for a, l, f, g, t in (
        ((16, 7, 31), None, None, "Rouge", "Fuite cuisine"),
        ((16, 6, 58), (16, 7, 20), None, "Rouge", "@maj:Home Assistant Core"),
        ((15, 16, 0), (15, 18, 5), (16, 6, 0), "Orange", "@vigi:Orange"),
        ((15, 9, 10), None, None, "Orange", "Porte entrée 12 %"),
        ((14, 21, 40), (14, 22, 2), (15, 8, 15), "Orange", "@indispo:3"),
        ((14, 18, 30), None, (14, 18, 52), "Orange", "Porte du garage"),
        ((12, 10, 0), (12, 10, 30), (13, 9, 0), "Orange", "@maj:ESPHome"),
        ((11, 14, 0), (11, 15, 0), (12, 6, 0), "Jaune", "@vigi:Jaune"),
    ))

# Vigilance de la scène 2 (orange), puis retour à celle de la scène 3, la dernière
# poussée par capturer.py. Heure figée : les codes de pluie sont ceux des scènes.
VIGILANCE_ORANGE = build_alerte_payload(phrase_pluie=code_pluie(*SCENES[1].pluie), **SCENES[1].alerte)
VIGILANCE_SCENE_3 = build_alerte_payload(phrase_pluie=code_pluie(*SCENES[2].pluie), **SCENES[2].alerte)


def _panneau(n: int) -> Service:
    """rendu_panneau : arrête le rotateur de la carte centrale sur le panneau n."""
    return Service("rendu_panneau", (("panneau", n),), apres=5.0)


# ---------------------------------------------------------------------------
# Écran principal (paysage 1280×720).
# ---------------------------------------------------------------------------

# Horloge en trois zones (09/10/2026, lot A, ui_components/horloge_zone.yaml) : heures
# (440..639 × 20..157), minutes (640..840 × 20..157), date (440..840 × 158..229). « auto » :
# heures court ligne suivante du panneau Ok Nabu (lot 3), long Réveil ; minutes court appareil suivant de la tuile − / +, long
# Réveil ; date court ligne suivante de la rangée, long Calendrier.
# tests/test_gestes.py les compare aux zones de Tab5/paquets/tab5-lvgl.yaml.
HEURES = (540, 90)
MINUTES = (740, 90)
DATE = (640, 195)
MICRO = (206, 145)            # long : Assistant vocal
DOMO, DISCU = (72, 150), (340, 150)
# Rangée du haut (06/10/2026 : fixe, avec ou sans TV) : HA (court : appareils, long :
# Énergie), engrenage (court : Réglages, long : Console système), manette (court :
# Arcade, long : télécommande TV). Le rendu pousse une maison complète (capturer.py,
# aucune zone absente) : la mini icône de la TV est sur la manette.
# tests/test_rendu_ecrans.py les compare aux boutons de Tab5/paquets/tab5-lvgl.yaml.
BOUTON_HA, BOUTON_SYS, BOUTON_TV = (917, 65), (1061, 65), (1205, 65)
# Rangée sous l'horloge (ADR-0031, zone btn_rangee) : court, ligne suivante ; long sur la
# ligne des plantes, Plantes.
SOUS_HORLOGE = (640, 270)
# Seconde température (btn_serre_games) : court, le contenu suivant de la zone à gauche de
# l'horloge (ADR-0051 : vocal, puis le graphique des prévisions ; l'Arcade avant le
# 10/10/2026, désormais par le tap du bouton manette, BOUTON_TV) ; long, son historique.
SERRE = (1172, 158)
# Température du salon (btn_reglables_liste, climate_card.yaml : carte en 855, 110, zone
# 4..196 × 31..95, centre 955, 173) : court, la roue de la clim (ADR-0048, ancrée plus bas) ;
# sans réglages reçus pour la clim (celle du blueprint avant « climr »), le carrousel des
# clims (popup Climatisation, ADR-0038).
# Tuile − / + (ADR-0033) : long sur la valeur entre − et + (CONSIGNE_CLIM,
# btn_clim_target_click), la liste ; ses lignes (reglables_liste.yaml : panneau en 740,
# 110, bord 2 + marge 6, lignes de 52 + 2).
SALON = (955, 173)
LIGNES_REGLABLES = tuple((1000, 110 + 2 + 6 + 54 * k + 26) for k in range(10))
CONSIGNE_CLIM = (1061, 251)   # court : Climatisation · long : la liste de la tuile − / +
TUILE_J1_TEMP = (390, 684)    # court : planning de ce jour, 6 s
CARTE_CENTRALE = (640, 375)   # long : la roue de navigation (ADR-0042), ancrée ici
# Long : Lumières. Avec les pièces (ADR-0023), les lampes T2 et T3 de la pièce de
# l'accueil (tools/demo/scenarios.py) : le popup liste les lumières de la pièce.
TUILES = {"chambre": (640, 572), "salon": (890, 572)}
# Long : le popup du volet (05/10/2026), sur le volet T1 de la même pièce (« Volet du
# salon », en train de s'ouvrir, 45 %). Sans position : un état poussé comme le ferait
# le blueprint (position nan), puis celui de la démo remis en place.
TUILE_VOLET = (390, 572)
VOLET_SANS_POSITION = Service("tab5_maj_emplacements", (("payload", "t01|closing|nan|;"),))
VOLET_DE_LA_DEMO = Service("tab5_maj_emplacements", (("payload", "t01|opening|45|;"),))
# Le volet dessiné du popup (06/10/2026), tiré du doigt vers le bas : 150 px de la fenêtre
# de 456 px, de 45 % à 13 % (volet_cadre_rappel, tab5_tuiles_popups.cpp). Vertical, au-dessus
# des tuiles : ni swipe de page ni bouton sous le doigt. Le relâcher envoie « position »,
# que personne n'applique ici : la capture montre le volet là où le doigt l'a laissé.
# Depuis l'ADR-0046 le volet dessiné est dans la carte de droite (x 426 + 56 dans la carte
# modale) : écran x 497 à 833.
VOLET_TIRE = Glisser(665, 250, 665, 400)
# Popups Lumières et Volets, une page par pièce (ADR-0046, 09/10/2026). La démo a des
# lumières au Salon, dans la Chambre d'amis et au Jardin (l'Applique de l'Entrée est en
# lecture seule), des volets au Salon et au Jardin. Le glissement part loin de l'arc et
# du volet dessiné (ils gardent leur geste) : bas de la carte COULEURS, droite de la carte
# POSITION.
LUMIERES_SUIVANTE = Glisser(1150, 660, 450, 660, dans_popup=True)
VOLETS_SUIVANTE = Glisser(1180, 300, 480, 300, dans_popup=True)
# Nom de la troisième pièce en haut (trois noms de 200 px collés à la croix : x 948 à 1148
# dans la carte modale, y 4 à 48 ; pages_onglets, tab5_pages.cpp).
LUMIERES_ONGLET_3 = (15 + 1048, 15 + 26)
# Première ligne de la carte AMPOULES (x 24 + 22, y 72 + 50 dans la carte modale, 342 × 84),
# sur le nom : l'appui long ouvre la roue de la tuile autour de sa pastille.
LUMIERES_LIGNE_0 = (15 + 24 + 22 + 200, 15 + 72 + 50 + 42)

# Roue d'actions rapides (ADR-0036, 07/10/2026) : l'appui long d'une lampe, d'un volet ou
# d'une clim pose un moyeu sur la tuile et deux anneaux de boutons au-dessus. Premier anneau :
# « Maison », les commandes et les familles de réglages, « Détails » (le popup complet) en
# dernier ; toucher une famille déplie ses choix sur le second, centré sur elle. Géométrie
# de disposer() (Tab5/ecran/tab5_roue.cpp) refaite à l'identique, tests/test_roue.py compare les
# constantes : un écran de popup touche le « Détails » calculé ici, un écran de roue une
# famille. Ancre en mode météo : le centre du bouton de la tuile.
ROUE_ECRAN = (1280, 720)
ROUE_RAYON = 180
ROUE_DIAMETRE = 72
ROUE_RAYON2 = 290
ROUE_DIAMETRE2 = 72
ROUE_MARGE = 12
ROUE_PAS_ANGLE = 30
ROUE_PAS_ANGLE2 = 20
ROUE_PIVOT = 5
ROUE_LEGENDE2 = 60
ROUE_LEGENDE_H = 28
ROUE_SIN5 = (0, 2856, 5690, 8481, 11207, 13848, 16383, 18794, 21062, 23170,
             25101, 26841, 28377, 29697, 30791, 31650, 32269, 32642, 32767)


def _roue_sin5(deg: int) -> int:
    deg %= 360
    signe = 1
    if deg >= 180:
        deg -= 180
        signe = -1
    if deg > 90:
        deg = 180 - deg
    return signe * ROUE_SIN5[deg // ROUE_PIVOT]


def _roue_echelle(r: int, v: int) -> int:
    """r · v / 32767 arrondi moitié loin de zéro, en entiers comme le C++."""
    p = r * v
    return (p + 16383) // 32767 if p >= 0 else -((-p + 16383) // 32767)


def roue_dessous(ya: int) -> bool:
    """La roue passe sous l'ancre quand le second anneau et ses mots n'y tiennent pas."""
    return ya - (ROUE_RAYON2 + ROUE_LEGENDE2 + ROUE_LEGENDE_H // 2) < 0


def _roue_disposer(xa: int, ya: int, n: int, rayon: int, pas: int, centre: int, r: int,
                   dessous: bool) -> list[tuple[int, int, int]]:
    """(x, y, angle) des n boutons, dans le sens horaire (de gauche à droite tant que
    l'éventail n'a pas pivoté) : disposer() de tab5_roue.cpp."""
    largeur, hauteur = ROUE_ECRAN
    decalage = 0
    for _ in range(180 // ROUE_PIVOT + 1):
        centres = []
        for i in range(n):
            a = centre + (n - 1) * pas // 2 - i * pas + decalage
            dy = _roue_echelle(rayon, _roue_sin5(a))
            centres.append((xa + _roue_echelle(rayon, _roue_sin5(a + 90)), ya + dy if dessous else ya - dy, a))
        gauche = min(x for x, _, _ in centres)
        droite = max(x for x, _, _ in centres)
        sens = 0
        if gauche - r < ROUE_MARGE:
            sens = -ROUE_PIVOT
        elif droite + r > largeur - ROUE_MARGE:
            sens = ROUE_PIVOT
        for _, y, a in centres:
            if sens == 0 and (y - r < ROUE_MARGE or y + r > hauteur - ROUE_MARGE):
                sens = ROUE_PIVOT if a < 90 else -ROUE_PIVOT
        if sens == 0:
            break
        decalage += sens
    return [(min(max(x, ROUE_MARGE + r), largeur - ROUE_MARGE - r),
             min(max(y, ROUE_MARGE + r), hauteur - ROUE_MARGE - r), a) for x, y, a in centres]


def roue_centres(xa: int, ya: int, n: int) -> list[tuple[int, int]]:
    """Centres des n boutons du premier anneau autour de l'ancre (xa, ya)."""
    return [(x, y) for x, y, _ in _roue_disposer(xa, ya, n, ROUE_RAYON, ROUE_PAS_ANGLE, 90,
                                                  ROUE_DIAMETRE // 2, roue_dessous(ya))]


def roue_choix_centres(xa: int, ya: int, n: int, famille: int, m: int) -> list[tuple[int, int]]:
    """Centres des m choix du second anneau, déplié au-dessus du bouton `famille` des n du
    premier."""
    a = _roue_disposer(xa, ya, n, ROUE_RAYON, ROUE_PAS_ANGLE, 90, ROUE_DIAMETRE // 2, roue_dessous(ya))[famille][2]
    return [(x, y) for x, y, _ in _roue_disposer(xa, ya, m, ROUE_RAYON2, ROUE_PAS_ANGLE2, a,
                                                  ROUE_DIAMETRE2 // 2, roue_dessous(ya))]


def roue_reglages(tuile: tuple[int, int], n: int) -> Toucher:
    """Toucher de « Détails » (dernier des n boutons) de la roue de la tuile météo `tuile`."""
    return Toucher(*roue_centres(*tuile, n)[-1])


def roue_famille(tuile: tuple[int, int], n: int, i: int) -> Toucher:
    """Toucher du bouton i (une famille) des n de la roue de la tuile météo `tuile`."""
    return Toucher(*roue_centres(*tuile, n)[i])


# Premier anneau des tuiles de la démo, « Maison » et « Détails » compris. Lampe T2
# (variateur et couleur, allumée) : Maison, Éteindre, Luminosité, Blancs, Couleurs,
# Détails ; lampe T3 (variateur) : Maison, Allumer, Luminosité, Détails ; volet T1 :
# Maison, Ouvrir, Stop, Fermer, Position (s'il donne la sienne), Détails ; clim du
# blueprint (dernière scène : consigne 20, Silence et Oscillation actifs) : Maison, Arrêt,
# Mode, Consigne, Options, Détails.
ROUE_BOUTONS = {"chambre": 6, "salon": 4, "volet": 6, "volet-sans-position": 5, "clim": 6}
# Rang des familles touchées par les écrans de roue.
ROUE_LUMINOSITE = 2
ROUE_COULEURS = 4
ROUE_POSITION = 4
ROUE_MODE = 2
# Toucher hors des boutons (coin bas gauche, loin de toute roue) : il replie le second
# anneau, puis ferme la roue.
ROUE_FERMER = Toucher(60, 700)
# Roue de navigation (ADR-0042, 09/10/2026) : l'appui long de la carte centrale ouvre la
# même roue, ancrée en son centre, sur les écrans de la tablette. Premier anneau de la démo
# (aucune famille vide) : Alertes, Pièces, Appareils, Agenda, Tablette, Assistant.
ROUE_NAVIGATION = 6
NAV_ALERTES, NAV_PIECES, NAV_APPAREILS, NAV_AGENDA, NAV_TABLETTE, NAV_ASSISTANT = range(ROUE_NAVIGATION)


def nav(i: int) -> Toucher:
    """Toucher du bouton i du premier anneau de la roue de navigation."""
    return Toucher(*roue_centres(*CARTE_CENTRALE, ROUE_NAVIGATION)[i])


# Roue de la lampe T2 à 50 % (128/255) : le choix « 50 % » marqué.
LAMPE_A_50 = Service("tab5_maj_emplacements", (("payload", "t02|on|128|FF8C1A;"),))
LAMPE_DE_LA_DEMO = Service("tab5_maj_emplacements", (("payload", "t02|on|180|FF8C1A;"),))
# Roue du volet T1 arrêté à 50 % : le choix « 50 % » marqué.
VOLET_A_50 = Service("tab5_maj_emplacements", (("payload", "t01|open|50|;"),))
# Roue de la clim du blueprint (tuile T2 de la pièce de la page 4, option m, en
# ventilation) : ses capacités viennent de « climr » (ADR-0026), que la démo ne pousse pas.
# Les valeurs par défaut de la tablette (16-30 °C, pas 0,5, toutes les lettres), sans nom ;
# marqué : le mode de la dernière scène (ventilation).
CLIM_CAPACITES = Service("tab5_maj_emplacements", (("payload", "climr|16|30|0.5|°C|chdfebqsw;"),))
# Roue d'une clim (ADR-0048), ouverte par la température de la pièce (SALON) : « Clims ▸ »
# (au moins deux clims : la démo a celle du blueprint et la clim propre du Bureau),
# Éteindre, Mode, Consigne, Options, « Détails » (le carrousel sur elle). La clim du Bureau
# (« chdfq ») et celle du blueprint après « climr » ont les trois familles. La température
# est trop haute (la roue passerait dessous, serrée) : le moyeu se pose sur un point bas, à
# la verticale de la zone touchée ramenée dans [480, 800], en y 460 (clim_ancre_basse,
# tab5_tuiles_roue.cpp) ; l'éventail s'ouvre au-dessus, entier.
ROUE_CLIM_ANCRE = (min(max(SALON[0], 480), 800), 460)
ROUE_CLIM_TEMPERATURE = 6
ROUE_CLIMS = 0
ROUE_CLIM_DETAILS = Toucher(*roue_centres(*ROUE_CLIM_ANCRE, ROUE_CLIM_TEMPERATURE)[-1])
ROUE_CLIM_CLIMS = Toucher(*roue_centres(*ROUE_CLIM_ANCRE, ROUE_CLIM_TEMPERATURE)[ROUE_CLIMS])

# Gestes sur les prévisions (mode météo) : départ et arrivée entre deux tuiles, pas sur
# un bouton (un bouton garde l'appui et se déclencherait au relâché).
VERS_LA_GAUCHE = Glisser(1015, 520, 265, 520)   # page 2 → 3 → 4 (journalières)
VERS_LA_DROITE = Glisser(265, 520, 1015, 520)   # page 2 → 1 → 0 (horaires)
# En mode HA, les cartes de la pièce sont centrées (formule des zones, ADR-0018 et
# ADR-0023) : leur place dépend du nombre d'appareils, et à quatre cartes x = 265 tombe
# sur le bouton de la première. Seuls les bords (x < 25, x > 1255) restent libres
# quelle que soit la pièce.
HA_VERS_LA_GAUCHE = Glisser(1265, 520, 15, 520)   # pièce suivante (page + 1)
HA_VERS_LA_DROITE = Glisser(15, 520, 1265, 520)   # pièce précédente (page − 1)


# Tensions lues par l'INA226 chargeur coupé : une batterie 2S (détectée) et la tablette
# sans batterie, sur l'USB (1,9 V relevé chez l'auteur le 08/10/2026 ; sous 3,0 V = pas
# de batterie, tab5_batterie.h).
TENSION_BATTERIE = 7.6
TENSION_SANS_BATTERIE = 1.9


def _batterie(montee: bool, niveau: float = float("nan"), en_charge: bool = False,
              tension: float = TENSION_BATTERIE) -> Service:
    """rendu_batterie (Tab5/rendu/bouchons.yaml) : interrupteur « Tab5 Batterie montée »,
    niveau en %, état de charge et tension (V) de la batterie de la tablette (icône du
    bandeau ; une prise si la tension dit qu'il n'y a pas de batterie)."""
    return Service("rendu_batterie", (("montee", montee), ("niveau", niveau), ("en_charge", en_charge),
                                      ("tension", tension)))


# Retour à l'état par défaut (interrupteur éteint, aucune mesure) : les autres écrans
# restent sans l'icône de la batterie.
SANS_BATTERIE = (_batterie(False),)


def _solaire(pourcent: str) -> Service:
    """Production solaire du bandeau d'état, comme la pousse le blueprint (clé solaire de
    tab5_maj_emplacements, % de la puissance crête ; « nan » = aucune valeur)."""
    return Service("tab5_maj_emplacements", (("payload", f"solaire|{pourcent};"),))


# Retour sans l'icône solaire : les autres écrans restent sans elle.
SANS_SOLAIRE = (_solaire("nan"),)


def _appuis(maison: str, engrenage: str, manette: str) -> Service:
    """Appuis longs des trois boutons du haut, comme les pousse un blueprint d'avant le lot A
    (clé appuis de tab5_maj_emplacements seule ; codes de kCodesGestes ou « auto »)."""
    return Service("tab5_maj_emplacements", (("payload", f"appuis|{maison}|{engrenage}|{manette};"),))


def _gestes(*codes: str) -> Service:
    """Les 12 gestes de l'accueil comme les pousse le blueprint (09/10/2026, lot A : clé
    appuis puis clé gestes, dans l'ordre de l'enum Geste ; codes de kCodesGestes ou
    « auto »)."""
    assert len(codes) == 12, codes
    appuis = f"appuis|{codes[7]}|{codes[9]}|{codes[11]};"
    return Service("tab5_maj_emplacements", (("payload", appuis + "gestes|" + "|".join(codes) + ";"),))


# Le choix reste en NVS : retour à « auto » (les icônes d'avant) pour les autres écrans.
APPUIS_AUTO = (_appuis("auto", "auto", "auto"),)
GESTES_AUTO = (_gestes(*["auto"] * 12),)


def _appareils_meteo(montres: bool) -> Service:
    """rendu_appareils_meteo (Tab5/rendu/bouchons.yaml) : interrupteur « Tab5 Appareils sur
    la météo » (discussion #278). Éteint, les prévisions ne montrent pas les appareils."""
    return Service("rendu_appareils_meteo", (("montres", montres),))


# Retour à l'état par défaut (allumé) : les autres écrans montrent les appareils.
AVEC_APPAREILS = (_appareils_meteo(True),)


def _previsions_recues(il_y_a_min: int) -> Service:
    """rendu_previsions_recues (Tab5/rendu/bouchons.yaml) : dernière poussée des
    prévisions `il_y_a_min` minutes avant l'heure figée (mention « prévisions
    périmées » au-delà de 30 min, tab5_forecast.cpp ; 0 = maintenant, masquée)."""
    return Service("rendu_previsions_recues", (("minutes", il_y_a_min),))


# Incident du 07-08/10/2026 : dernières prévisions de la veille à 21:04, 10 h 41 avant
# l'heure figée (07:45) → « Prévisions d'hier 21 h 04 ».
PREVISIONS_DE_LA_VEILLE = (_previsions_recues(10 * 60 + 41),)
# Retour à des prévisions fraîches : les autres écrans restent sans la mention.
PREVISIONS_FRAICHES = (_previsions_recues(0),)


def ecrans_des_pieces(pieces: dict) -> tuple:
    """Le mode HA sur chaque pièce de la démo (ADR-0023) : « HA » depuis l'accueil (pièce
    0, page 2), puis un geste par pièce occupée jusqu'à la bonne (le mode HA saute les
    pages sans appareil ; la pièce 0 doit en avoir, sinon « HA » sauterait à la plus
    proche). Retour : « HA » (la météo de la même page), puis les gestes météo jusqu'à
    la page 2, qui passent par toutes les pages : on ne compte pas sur « Aller à
    l'écran → Accueil » pour quitter le mode HA. Nom : « accueil-ha-piece-n », n = R + 1
    comme les pièces du blueprint."""
    occupees = sorted(PAGE_DE_LA_PIECE[r] for r, p in pieces.items() if p.tuiles)
    assert PAGE_DE_LA_PIECE[0] in occupees, "la pièce de l'accueil (R = 0) doit avoir un appareil"
    ecrans = []
    for r in sorted(pieces):
        if PAGE_DE_LA_PIECE[r] not in occupees:
            continue
        aller, retour = vers_la_piece(pieces, r)
        ecrans.append(Ecran(f"accueil-ha-piece-{r + 1}", aller, retour))
    return tuple(ecrans)


def vers_la_piece(pieces: dict, r: int) -> tuple[tuple, tuple]:
    """(aller, retour) : « HA » puis les gestes jusqu'à la pièce R ; « HA » puis les gestes
    météo jusqu'à la page 2 (voir ecrans_des_pieces)."""
    occupees = sorted(PAGE_DE_LA_PIECE[k] for k, p in pieces.items() if p.tuiles)
    page = PAGE_DE_LA_PIECE[r]
    assert page in occupees, f"pièce {r} sans appareil : le mode HA la saute"
    if page >= 2:
        aller = (HA_VERS_LA_GAUCHE,) * sum(1 for p in occupees if 2 < p <= page)
        retour = (VERS_LA_DROITE,) * (page - 2)
    else:
        aller = (HA_VERS_LA_DROITE,) * sum(1 for p in occupees if page <= p < 2)
        retour = (VERS_LA_GAUCHE,) * (2 - page)
    return (Toucher(*BOUTON_HA),) + aller, (Toucher(*BOUTON_HA),) + retour


# Climat de la pièce (ADR-0040) : le Bureau de la démo (R = 3, page 1) a une température,
# une humidité et une clim. En mode HA sur lui, appui long sur sa température : son
# historique (clé p3) ; toucher la consigne de la tuile − / + : la fenêtre de sa clim.
# `fermer` referme la fenêtre (croix du chrome modal) avant de quitter le mode HA.
PIECE_CLIMAT = 3
assert PIECES[PIECE_CLIMAT].climat is not None and PIECES[PIECE_CLIMAT].climat.reglages
ALLER_PIECE_CLIMAT, RETOUR_PIECE_CLIMAT = vers_la_piece(PIECES, PIECE_CLIMAT)
# Roue de navigation (ADR-0042) : la famille Pièces déplie Maison puis chaque pièce qui a
# des appareils, dans l'ordre du blueprint ; toucher le Bureau y met le mode HA, d'un coup.
# Retour : celui de vers_la_piece (« HA », puis les gestes météo jusqu'à l'accueil).
PIECES_OCCUPEES = sorted(r for r, p in PIECES.items() if p.tuiles)
NAV_BUREAU = Toucher(*roue_choix_centres(*CARTE_CENTRALE, ROUE_NAVIGATION, NAV_PIECES, 1 + len(PIECES_OCCUPEES))
                     [1 + PIECES_OCCUPEES.index(PIECE_CLIMAT)])

# Popup Réveil (alarm_popup.yaml), cinq pages depuis le 09/10/2026, sur le modèle des
# Réglages : noms des pages de 184 × 44 à x 196 + 190 × i de la carte modale (posée à
# 15 px des bords), sur la ligne du titre ; pages à y 72 de la carte, cartes à x 24
# (et 637). « Tester » : 360 × 100 à (16, 482) de la carte de droite de la page Heure.
REVEIL_PAGES = {"heure": (303, 41), "jours": (493, 41), "ouverture": (683, 41),
                "sonnerie": (873, 41), "annonces": (1063, 41)}
REVEIL_TESTER = (848, 619)
# Geste vers la gauche sur la page Jours, parti du préréglage « Week-end » (286 × 60 à
# (899, 302) de la carte, Lundi-Vendredi par défaut) : la page suivante (Ouverture), et
# le préréglage ne se déclenche pas au relâché (« reveil-jours », capturé ensuite, montre
# toujours « Lundi-Vendredi »).
REVEIL_GLISSER_DEPUIS_UN_BOUTON = Glisser(1150, 419, 550, 419, dans_popup=True)
SONNERIE_ARRETER = (440, 540)
CAL_JOUR_18 = (642, 342)      # cellule du jeudi 18 (rangée 2, colonne 3)
# Carte GESTION de la console (page Système des Réglages depuis le 08/10/2026 : carte de
# 589 × 290 à (652, 395) à l'écran) : deux rangées de deux boutons de 266 × 107.
CONSOLE_REDEMARRER_HA, CONSOLE_REBOOT = (807, 611), (1085, 611)
CONFIRMATION_ANNULER = (829, 592)   # jamais « Confirmer » (1065, 592)
# Popup Énergie (ADR-0028, energie_popup.yaml) : la tuile du capteur solaire de la démo
# (pièce « Bureau », page 1 des heures, T1 : deuxième tuile, x 275-505), la croix de
# l'en-tête (ADR-0009, commune à tous les popups) et les boutons de vue (carte du
# graphique, y 313-685 à l'écran ; boutons de 150 × 48 à 18, 178 et 338 px de son bord
# droit).
TUILE_SOLAIRE = (390, 572)
FERMER_POPUP = (1215, 41)
# Popup d'un appareil (06/10/2026, appareil_popup.yaml), par l'appui long de trois tuiles
# de la démo : « Ordinateur » (int, option o, allumé ; pièce « Bureau », page 1, T0),
# « Soirée cinéma » (act, accueil, T4) et « Je pars » (act à confirmer, option k ; pièce
# « Entrée », page 3, T3). Le grand bouton de la carte COMMANDE (180 × 380, à x 840 et
# y 72 + 64 de la carte modale posée à 15 px des bords) : un appui y arme la
# confirmation de « Je pars », la capture la montre (3 s).
TUILE_ORDINATEUR = (140, 572)
TUILE_SCENE = (1140, 572)
TUILE_JE_PARS = (890, 572)
BOUTON_APPAREIL = (1048, 341)
ENERGIE_VUES = {"heures": (828, 347), "jours": (988, 347), "mois": (1148, 347)}
# Popup Réglages (reglages_popup.yaml), quatre pages depuis le 08/10/2026. Noms des pages
# en haut : 200 × 44 à x 318 + 210 × i de la carte modale (posée à 15 px des bords), sur
# la ligne du titre. Cartes des pages Écran et Apparence à (39, 87) à l'écran, 1202 de
# large ; pastilles « English » et « Français » (x 311 et 17, y 457, 286 × 56 dans la
# carte) ; « Annuler » de la confirmation (centre de la carte − 130, + 60). Jamais
# « Confirmer » : la tablette redémarrerait. La langue de l'écran n'ouvre pas la
# confirmation : « Français » en anglais.
REGLAGES_PAGES = {"ecran": (433, 41), "apparence": (643, 41), "batterie": (853, 41), "systeme": (1063, 41)}
REGLAGES_LANGUE_EN, REGLAGES_LANGUE_FR = (493, 572), (199, 572)
REGLAGES_ANNULER = (510, 446)
# Gestes dans le popup : vers la gauche = page suivante, en boucle. Le premier part du
# bouton « Non » de « Rallumer l'écran d'une tape » (x 606-1185, y 523-579 de la carte) :
# la page change et le bouton ne se déclenche pas au relâché (« reglages », capturé
# ensuite, montre toujours « Oui »). Le second glisse le curseur de luminosité (y 205-225
# à l'écran) : son réglage, pas une page ; le dernier le remet à 100 %.
REGLAGES_GLISSER_DEPUIS_UN_BOUTON = Glisser(1100, 638, 500, 638, dans_popup=True)
REGLAGES_GLISSER_CURSEUR = Glisser(900, 215, 400, 215, dans_popup=True)
REGLAGES_CURSEUR_A_100 = Glisser(400, 215, 1270, 215, dans_popup=True)

# Popup Température (ADR-0032, historique_popup.yaml) : appui long sur la température de
# la pièce (btn_reglables_liste, aussi le carrousel des clims au toucher court ; x 859-1051
# et y 141-205 à l'écran) ou sur la seconde
# (SERRE). Boutons de vue : carte du graphique à y 253-685 à l'écran, boutons de 150 × 48
# à 18, 178 et 338 px de son bord droit (x 1241). Le rendu ne répond à aucun événement :
# il pousse lui-même la réponse de script.tab5_historique, datée de l'heure figée.
SALON_TEMP = (954, 158)
TEMPERATURE_VUES = {"jour": (828, 287), "semaine": (988, 287), "mois": (1148, 287)}
MOMENT_DES_CAPTURES = _dt.datetime(2026, 6, 16, 7, 45)
# Onglets du popup (ADR-0047) : une page par température connue — le salon, les pièces de
# la démo qui ont une température (Entrée p1, Bureau p3), la serre —, 200 × 44 calés à
# droite jusqu'à x 1148 de la carte modale, soit les places des pages des Réglages.
# Un glissement vers la gauche dans le graphique : la page suivante (salon → Entrée).
TEMPERATURE_ONGLETS = {"salon": (433, 41), "p1": (643, 41), "p3": (853, 41), "serre": (1063, 41)}
TEMPERATURE_SUIVANTE = Glisser(1100, 520, 450, 520, dans_popup=True)
assert PIECES[1].climat is not None and not PIECES[1].climat.humidite, "l'Entrée : sa température seule"


# Popup Maison (ADR-0037) avec deux pièces seulement : le Salon (cinq appareils) et la
# chambre au nom coupé (la clim, deux tuiles espacées). Les pièces 1, 3 et 4 retirées
# repartent grisées (tuiles_definir) : `fermer` repousse les définitions de la démo, puis
# les états de ces trois pièces (aucune clim parmi elles : rien d'autre n'est oublié).
DEUX_PIECES = {r: PIECES[r] for r in (0, 2)}
RETIREES = {r: p for r, p in PIECES.items() if r not in DEUX_PIECES}
assert not any(t.type == "cli" for p in RETIREES.values() for t in p.tuiles.values())
MAISON_DEUX_PIECES = Service("tab5_maj_tuiles", (("payload", build_tuiles_payload(DEUX_PIECES, RANGEE, REGLABLES)),))
# Ligne de la lampe d'ambiance (Salon, 3e ligne) : colonne 0 de 235 px, lignes de 104 px tous
# les 112 px à partir de y = 120 dans la carte (disposer(), Tab5/ecran/tab5_maison.cpp). Le doigt
# entre la pastille et le nom, loin du « ⋯ ».
MAISON_LAMPE = (104, 412)
MAISON_DE_LA_DEMO = (Service("tab5_maj_tuiles", (("payload", build_tuiles_payload(PIECES, RANGEE, REGLABLES)),)),
                     Service("tab5_maj_emplacements", (("payload", build_etats_tuiles(RETIREES)),)))

# Carrousel des clims (ADR-0038) : la démo n'a que la clim du blueprint (une page, ni
# pastilles ni glisse). Trois pages : deux clims de tuile sans l'option m au Bureau (pièce
# 3, page 1), T2 « Clim du bureau » en chauffage (toutes les capacités) et T3, que HA
# pousse sans nom (le titre est alors celui de la pièce) et qui n'a que Froid,
# Ventilation et Silence. Réglages et états (clés cr et ce, ADR-0027) après les
# définitions, comme le blueprint. `fermer` repousse celles de la démo : les deux tuiles
# redéfinies, la tablette oublie leurs clims (clim_tuile_oublier).
BUREAU_AVEC_CLIMS = {**PIECES, 3: Piece(PIECES[3].nom, {
    **PIECES[3].tuiles,
    2: Tuile("cli", "Clim du bureau", "clim", etat="heat", valeur="19.5"),
    3: Tuile("cli", "Clim de l'atelier", "clim", etat="off", valeur="17"),
})}
CLIMS_DE_TUILES = (
    Service("tab5_maj_tuiles", (("payload", build_tuiles_payload(BUREAU_AVEC_CLIMS, RANGEE, REGLABLES)),)),
    Service("tab5_maj_emplacements", (("payload", build_etats_tuiles({3: BUREAU_AVEC_CLIMS[3]})
                                       + "cr32|16|30|0.5|°C|chdfebqsw|Clim du bureau;"
                                       + "ce32|20.5|19.5|heat|none|quiet|swing;"
                                       + "cr33|18|30|1|°C|cfq|;ce33|22|17|off|none|auto|stop;"),)),
)
# Geste dans le popup, vers la gauche : la clim suivante. Il part du bas de la carte
# OPTIONS (verre vide sous ses boutons, x 857-1243 et y 631-687 à l'écran) et finit sur la
# carte TEMPÉRATURE, sous l'arc et ses boutons.
CARROUSEL_SUIVANTE = Glisser(1150, 668, 450, 668, dans_popup=True)


# Panneau Ok Nabu (lot 3, ADR-0041) : les définitions de la démo avec d'autres lignes pour
# lui (clés n…), puis leurs états. Les pièces, la rangée et la tuile − / + ne changent pas :
# l'écran garde leurs états. `fermer` repousse celui de la démo (l'écoute seule, NABU).
def _nabu(nabu) -> tuple:
    return (Service("tab5_maj_tuiles", (("payload", build_tuiles_payload(PIECES, RANGEE, REGLABLES, nabu)),)),
            Service("tab5_maj_emplacements", (("payload", build_etats_tuiles({}, None, None, nabu)),)))


NABU_DE_LA_DEMO = _nabu(NABU)[:1]
# Thème à la police de date la plus large (valeurs de 45 px, comme la date ; « 612 ppm » :
# 191 px en IBM Plex Serif 700 contre 188 en Nunito 800, celle de Relief doux, le défaut).
THEME_POLICE_LARGE = "Almanach imprimé"
# Thème au cadre le plus serré : une gélule (rayon 45, bordure de 4 px).
THEME_CADRE_GELULE = "Capsule"
THEME_PAR_DEFAUT = "Relief doux"


# Popup Musique (ADR-0050, lecteur_popup.yaml) : trois lecteurs choisis, le premier en
# pause (une position qui n'avance pas : la capture ne dépend pas de l'instant), toutes
# les commandes offertes, aléatoire actif, répétition de tout. Pas de pochette : le rendu
# ne télécharge rien (ImageMuette), la note de musique s'affiche. Format de
# packages/tab5_lecteur.yaml (script tab5_lecteur_pousser).
LECTEUR_LISTE = "Salon|speaker;Apple TV|tv;Freebox|receiver"
LECTEUR_EN_PAUSE = Service("tab5_maj_lecteur", (
    ("lecteurs", LECTEUR_LISTE),
    ("etat", "0|Salon|speaker|paused|Le vent du large|Les Marées|Carnets de voyage|Spotify|83|254|45|0|1|all|lsvmpnaro|"),
))
# Un lecteur éteint, hors de la liste (une tuile med) : « Lecteur éteint » et « Allumer ».
LECTEUR_ETEINT = Service("tab5_maj_lecteur", (
    ("lecteurs", LECTEUR_LISTE),
    ("etat", "-1|TV Samsung|tv|off" + "|" * 11 + "o|"),
))
# Après chaque capture : plus aucun lecteur, la barre « en lecture » s'en va et le cadre
# Ok Nabu revient pour les captures suivantes.
LECTEUR_AUCUN = Service("tab5_maj_lecteur", (("lecteurs", ""), ("etat", "")))
# Le voile autour de la carte (940 × 536 centrée) ferme le popup.
LECTEUR_VOILE = Toucher(60, 360)
# Lecteur compact de la zone à gauche de l'horloge (ADR-0051, lot 2) : le blueprint le met
# au départ (clé « gauche » de tab5_maj_emplacements, il s'affiche tout de suite) ;
# ZONE_VOCAL remet le vocal au départ et le cycle par défaut d'un blueprint d'avant.
ZONE_LECTEUR = Service("tab5_maj_emplacements", (("payload", "gauche|lecteur|vocal|graphique|lecteur;"),))
ZONE_VOCAL = Service("tab5_maj_emplacements", (("payload", "gauche|vocal|graphique;"),))
# Le lecteur de la démo allumé, rien en lecture : « Rien en lecture » et ses commandes.
LECTEUR_INACTIF = Service("tab5_maj_lecteur", (
    ("lecteurs", LECTEUR_LISTE),
    ("etat", "0|Salon|speaker|idle" + "|" * 7 + "45|0|0|off|lsvmpnaro|"),
))


# Capteurs suivis (ADR-0054, suivi_popup.yaml et suivi_zone.yaml) : quatre capteurs au
# format de packages/tab5_suivi.yaml (script tab5_suivi_pousser) — un cours en hausse
# du jour (change_pct), une température en baisse sur 24 h (écart, deux heures sans
# mesure), une puissance et un indice en baisse du jour. 24 moyennes horaires puis la
# valeur actuelle, ramenées de 0 à 100. Noms d'exemple, aucune entité.
def _suivi_points(*valeurs) -> str:
    return ",".join("" if v is None else str(v) for v in valeurs)


SUIVI_DONNEES = Service("tab5_maj_suivi", (("payload", ";".join((
    "Cours ACME|182.4|USD|2.05|p|" + _suivi_points(12, 8, 0, 6, 15, 22, 18, 25, 31, 28, 40, 46, 43, 52, 60, 57,
                                                    66, 71, 68, 77, 83, 80, 90, 94, 100),
    "Serre|21.4|°C|-1.5|a|" + _suivi_points(95, 100, 92, 85, None, None, 70, 61, 55, 48, 40, 36, 30, 34, 41,
                                             47, 52, 44, 35, 27, 18, 12, 6, 2, 0),
    "Consommation de la maison|612|W|148|a|" + _suivi_points(10, 8, 5, 4, 0, 3, 12, 35, 60, 42, 30, 28, 33,
                                                              45, 38, 30, 41, 58, 80, 100, 74, 52, 31, 22, 37),
    "Indice|7803.33|pts|-0.95|p|" + _suivi_points(70, 74, 79, 85, 100, 96, 90, 83, 80, 76, 71, 68, 72, 66, 59,
                                                  55, 48, 52, 44, 38, 31, 25, 18, 9, 0),
))),))
# La zone à gauche de l'horloge au départ sur le capteur (clé « gauche »).
ZONE_CAPTEUR = Service("tab5_maj_emplacements", (("payload", "gauche|capteur|vocal|graphique|lecteur|capteur;"),))


# Réfrigérateurs et congélateurs (ADR-0055, froid_popup.yaml) : au format de
# packages/tab5_froid.yaml (script tab5_froid_calculer), date figée des captures (mardi
# 16 juin 2026, 07:45 à Paris). Un réfrigérateur au niveau grave, porte mal fermée depuis
# 07:12 (la courbe monte la dernière heure), dont le dernier incident date d'hier 18:40
# (coup de chaud de 42 min, 8.7 °C au plus), et un congélateur conforme. Noms d'exemple,
# aucune entité.
def _froid_points(*valeurs) -> str:
    return ",".join("" if v is None else f"{v:.1f}" for v in valeurs)


FROID_FRIGO = ("Frigo cuisine|f|9.1|2|porte|1781586720|2.8|9.4|0|5|"
               + _froid_points(3.4, 3.6, 3.9, 3.7, 3.5, 3.3, 3.2, 3.6, 4.1, 4.4, 3.9, 3.6, 3.4, 3.3, 3.5,
                               3.8, 3.6, 3.2, 3.0, 2.8, None, 3.1, 3.4, 6.2, 9.1)
               + "|chaud|1781541600|42|8.7")
FROID_CONGELATEUR = ("Congélateur garage|c|-19.4|0|ok|0|-21.0|-17.6||-18|"
                     + _froid_points(-19.8, -20.1, -20.4, -20.6, -20.2, -19.7, -19.2, -18.6, -17.9, -18.4,
                                     -19.1, -19.6, -20.0, -20.3, -20.5, -20.1, -19.8, -19.5, -19.2, -19.0,
                                     -19.3, -19.6, -19.5, -19.4, -19.4)
                     + "||0|0|")
FROID_DONNEES = Service("tab5_maj_froid", (("payload", FROID_FRIGO + ";" + FROID_CONGELATEUR),))
# Remise après la capture : plus rien au niveau grave, l'icône de l'horloge s'éteint et
# les écrans suivants restent identiques à leurs références.
FROID_CONFORME = Service("tab5_maj_froid", (("payload", FROID_CONGELATEUR),))

# Serveur IA (ADR-0059, serveur_ia_popup.yaml) : au format de packages/tab5_serveur_ia.yaml.
# La courbe des tokens/s est gardée par la tablette, un point par poussée : 24 poussées
# la remplissent toute (les points des scènes d'avant n'y sont plus), deux salves de
# génération séparées par des temps morts, la dernière valeur celle du serveur. Puis un
# serveur hors ligne (« — » partout, « Hors ligne ») et aucun capteur choisi. Remise :
# le serveur de la dernière scène, comme après les scènes (la roue de navigation propose
# le popup dans les écrans suivants comme dans ceux d'avant).
SERVEUR_IA = SERVEUR_IA_SCENES[SCENES[0].nom]
SERVEUR_IA_COURBE = tuple(
    Service("tab5_maj_serveur_ia", (("payload", build_serveur_ia_payload({**SERVEUR_IA, "tps": f"{v:.1f}"})),),
            apres=0.05)
    for v in (0, 0, 12.5, 38.2, 41.7, 43.1, 42.4, 40.9, 0, 0, 0, 35.6, 44.8, 45.3, 44.1, 43.7, 42.9, 18.2, 0, 0,
              27.4, 41.1, 42.6, 42.7))
SERVEUR_IA_HORS_LIGNE = Service("tab5_maj_serveur_ia", (("payload", build_serveur_ia_payload(
    {"nom": "PC bureau", "etat": "0", "vram_total": "16.0"})),))
SERVEUR_IA_AUCUN = Service("tab5_maj_serveur_ia", (("payload", build_serveur_ia_payload(None)),))
SERVEUR_IA_REMISE = Service("tab5_maj_serveur_ia", (("payload", build_serveur_ia_payload(
    SERVEUR_IA_SCENES[SCENES[-1].nom])),))

# Télécommandes du popup (ADR-0056) : la clé « telecommandes|écran|nom|… » que le blueprint
# pousse avec tous les états (celles de l'auteur, sans identifiant), et la liste vide.
TELECOMMANDES_TV_D_ABORD = Service("tab5_maj_emplacements", (
    ("payload", "telecommandes|tv|TV Samsung|boitier|Apple TV|boitier|Freebox Player;"),))
TELECOMMANDES_BOITIER_D_ABORD = Service("tab5_maj_emplacements", (
    ("payload", "telecommandes|boitier|Apple TV|tv|TV Samsung|boitier|Freebox Player;"),))
TELECOMMANDES_AUCUNE = Service("tab5_maj_emplacements", (("payload", "telecommandes|;"),))


# Popup Météo (ADR-0043, meteo_popup.yaml) : une journée qui change (soleil le matin,
# orage l'après-midi, éclaircies le soir), de 07:00 (l'heure figée, 07:45 : la colonne
# de l'heure en cours) à 21:00, dix jours contrastés, une pluie dans l'heure qui monte
# puis retombe. Le rendu pousse ces données lui-même ; `fermer` remet celles de la
# scène 3 (la dernière jouée avant les écrans) : l'accueil les montre aussi. Le retour
# à l'accueil (« Aller à l'écran » → Accueil) referme le popup.
_METEO_HEURES = (
    ("partlycloudy", 14.2, 0.0), ("sunny", 15.8, 0.0), ("sunny", 17.9, 0.0), ("sunny", 20.1, 0.0),
    ("partlycloudy", 22.4, 0.0), ("partlycloudy", 23.6, 0.0), ("cloudy", 24.1, 0.0), ("rainy", 22.0, 1.2),
    ("pouring", 19.4, 4.6), ("lightning-rainy", 18.2, 8.8), ("rainy", 17.5, 2.1), ("cloudy", 17.1, 0.3),
    ("partlycloudy", 16.4, 0.0), ("partlycloudy", 15.2, 0.0), ("clear-night", 14.0, 0.0),
)
_METEO_JOURS = (
    ("lightning-rainy", 12.1, 24.3), ("sunny", 14.0, 27.5), ("sunny", 16.2, 30.1), ("partlycloudy", 15.4, 26.0),
    ("rainy", 12.8, 19.6), ("pouring", 10.9, 16.2), ("cloudy", 9.5, 17.8), ("partlycloudy", 11.0, 21.4),
    ("sunny", 13.3, 24.9), ("windy", 12.0, 20.5), ("fog", 8.7, 15.1), ("snowy-rainy", 1.4, 6.2),
    ("partlycloudy", 7.9, 14.8), ("sunny", 10.1, 19.0), ("hail", 9.2, 13.5),
)
_METEO_HEURES_PAYLOADS = tuple(build_heures_bulk_payload([
    HeureForecast(i, f"{7 + i:02d}:00", c, t, p) for i, (c, t, p) in enumerate(_METEO_HEURES)][d:d + 5])
    for d in (0, 5, 10))
_METEO_JOURS_PAYLOAD = build_jours_bulk_payload([
    JourForecast(i, "Auj" if i == 0 else JOURS_FR[((MOMENT_DES_CAPTURES + _dt.timedelta(days=i)).weekday() + 1) % 7],
                 c, tmin, tmax, (MOMENT_DES_CAPTURES + _dt.timedelta(days=i)).weekday() >= 5,
                 (MOMENT_DES_CAPTURES + _dt.timedelta(days=i)).weekday() == 6)
    for i, (c, tmin, tmax) in enumerate(_METEO_JOURS)])
_METEO_PLUIE_1H = ("Temps sec", "Temps sec", "Pluie faible", "Pluie modérée", "Pluie forte", "Pluie très forte",
                   "Pluie modérée", "Pluie faible", "Temps sec")
METEO_DONNEES = (
    Service("tab5_maj_meteo_actuelle", (("condition", "partlycloudy"), ("temperature", "15.3"), ("humidite", "72"))),
    Service("tab5_maj_probabilites", (("uv", "6"), ("gel", "0"), ("neige", "0"))),
    *(Service("tab5_maj_previsions_heures_bulk", (("payload", p),)) for p in _METEO_HEURES_PAYLOADS),
    Service("tab5_maj_previsions_jours_bulk", (("payload", _METEO_JOURS_PAYLOAD),)),
    Service("tab5_maj_pluie_1h_bulk", (("payload", build_pluie_1h_bulk_payload(_METEO_PLUIE_1H)),)),
    Service("tab5_maj_alerte_meteo_france",
            (("payload", build_alerte_payload(phrase_pluie=code_pluie(1, 10), **SCENES[2].alerte)),)),
)
_S3 = SCENES[2]
METEO_SCENE_3 = (
    Service("tab5_maj_meteo_actuelle", (("condition", _S3.meteo_condition), ("temperature", str(_S3.meteo_temperature)),
                                        ("humidite", str(_S3.meteo_humidite)))),
    Service("tab5_maj_probabilites", tuple((k, str(v)) for k, v in _S3.probabilites.items())),
    *(Service("tab5_maj_previsions_heures_bulk", (("payload", build_heures_bulk_payload(_S3.heures[d:d + 5])),))
      for d in (0, 5, 10)),
    Service("tab5_maj_previsions_jours_bulk", (("payload", build_jours_bulk_payload(_S3.jours)),)),
    Service("tab5_maj_pluie_1h_bulk", (("payload", build_pluie_1h_bulk_payload(_S3.pluie_1h)),)),
    Service("tab5_maj_alerte_meteo_france", (("payload", VIGILANCE_SCENE_3),)),
)
# Noms des pages en haut (reglages_onglet.yaml, mêmes places que ceux des Réglages) et un
# geste dans le popup, vers la gauche (page suivante), parti du verre de la carte des
# heures, sous ses barres de pluie (y 645 à l'écran : zone 1166 × 404 à y 87 + 166 + 14).
METEO_PAGES = {"jour": (433, 41), "jours": (643, 41), "details": (853, 41)}
METEO_GLISSER = Glisser(1100, 675, 500, 675, dans_popup=True)

# Popup Caméras (ADR-0049, ADR-0057, cameras_popup.yaml) : la liste que le blueprint
# renverrait à l'événement esphome.tab5_cameras (« nom|image|pièce|depuis »), poussée avant
# l'ouverture. Le rendu ne télécharge rien : le bouchon de tab5_cameras_charge.cpp (hors
# ESP_PLATFORM) rend une mire calculée, une teinte par caméra. Sept caméras, trois pièces
# et une sans pièce (« Autres ») ; l'abri est hors ligne depuis 06:58 (captures à 07:45).
CAMERAS_DONNEES = Service("tab5_maj_cameras", (
    ("adresse", "http://homeassistant.local:8123"),
    ("cameras", ";".join((
        "Portail|/api/camera_proxy/camera.portail?token=a|Entrée|",
        "Porte d'entrée|/api/camera_proxy/camera.porte?token=b|Entrée|",
        "Terrasse|/api/camera_proxy/camera.terrasse?token=c|Jardin|",
        "Potager|/api/camera_proxy/camera.potager?token=d|Jardin|",
        f"Abri de jardin|/api/camera_proxy/camera.abri?token=e|Jardin|{_epoch(2026, 6, 16, 6, 58)}",
        "Garage|/api/camera_proxy/camera.garage?token=f|Garage|",
        "Couloir|/api/camera_proxy/camera.couloir?token=g||",
    ))),
))
# Puces de la colonne des pièces (x 16 à 254 de la carte, 56 px de haut, 8 d'écart, à
# partir de y 72 ; carte à (15, 15) de l'écran) : 0 « Toutes », 1 Entrée, 2 Jardin…
def _camera_puce(n: int) -> Toucher:
    return Toucher(15 + 16 + 119, 15 + 72 + n * 64 + 28)


# Balayage vers la gauche sur le cadre : page suivante de la mosaïque, ou caméra suivante
# de la pièce en grand.
CAMERAS_SUIVANTE = Glisser(1100, 360, 500, 360, dans_popup=True)
CAMERAS_PRECEDENTE = Glisser(500, 360, 1100, 360, dans_popup=True)


# Cases de la mosaïque (lot 2, cameras_vignette.yaml) : le contenu du cadre (x 270 + 2 de
# bordure, y 72 + 2, dans la carte à (15, 15)) en cases de 476 × 266, 8 px d'écart. Centre
# de la case k pour `n` caméras sur la page (2 : au milieu ; 3 : la 3e centrée en bas).
def _camera_vignette(k: int, n: int = 4) -> Toucher:
    x0, y0 = 15 + 270 + 2, 15 + 72 + 2
    x, y = (k % 2) * 484, (k // 2) * 274
    if n == 2:
        y = 137
    elif n == 3 and k == 2:
        x = 242
    return Toucher(x0 + x + 238, y0 + y + 133)


# Tap sur l'image en grand : retour à la mosaïque.
CAMERAS_MOSAIQUE = Toucher(15 + 270 + 482, 15 + 72 + 272)


def _historique(cle: str, vue: str, exterieur: bool = False) -> Service:
    """Ce que pousserait script.tab5_historique (tools/demo/scenarios.py)."""
    return Service("tab5_maj_historique", tuple(build_historique(cle, vue, MOMENT_DES_CAPTURES, exterieur).items()))


# ---------------------------------------------------------------------------
# Arcade : sélecteur et consoles. Menus centrés en x = 640 (360 pour Neon Apron).
# ---------------------------------------------------------------------------

CARTES = {
    "fil-dor": (169, 258), "arcanoide": (483, 258), "neon-apron": (797, 258),
    "coureur-dor": (1111, 258), "go": (169, 528), "trial-poursuite": (483, 528),
    "dames": (797, 528), "roi-noir": (1111, 528),
}


def _menu(y: int, x: int = 640) -> Toucher:
    return Toucher(x, y, apres=0.8)


def _arcade(jeu: str, *etapes) -> tuple:
    """Accueil → sélecteur Arcade (tap du bouton manette, « auto ») → console `jeu` → étapes."""
    return (Toucher(*BOUTON_TV, apres=1.0), Toucher(*CARTES[jeu], apres=1.2)) + etapes


def _jeu(nom: str, jeu: str, etapes: tuple = (), fermer: tuple = (), portrait: bool = False,
         stable: bool = True) -> Ecran:
    return Ecran(f"jeu-{jeu}" + (f"-{nom}" if nom else ""), _arcade(jeu, *etapes), fermer,
                 portrait=portrait, stable=stable)


# Parties : chaque partie lancée est ABANDONNÉE avant le retour à l'accueil. Laissée
# en cours, elle serait sauvegardée (NVS) et ajouterait une ligne « Reprendre » aux
# menus de Go et du Roi Noir, décalant tous les appuis suivants.
FIL_DOR_PARTIE = (_menu(181),)
FIL_DOR_PAUSE = FIL_DOR_PARTIE + (_menu(24),)
FIL_DOR_ABANDON = (_menu(317),)
FIL_DOR_HUB = (_menu(589),)

ARCANOIDE_PARTIE = (_menu(181),)

NEON_PARTIE = (_menu(442, 360),)
NEON_PAUSE = NEON_PARTIE + (_menu(70, 360),)
NEON_ABANDON = (_menu(634, 360),)
NEON_HUB = (_menu(634, 360),)

COUREUR_PARTIE = (_menu(227),)
COUREUR_PAUSE = COUREUR_PARTIE + (_menu(24),)
COUREUR_QUITTER = (_menu(507),)
COUREUR_HUB = (_menu(367),)

GO_PARTIE = (_menu(180), _menu(550), Toucher(483, 390, apres=0.8))   # pierre fantôme au tengen
GO_MENU = (Toucher(1187, 665, apres=0.8),)
GO_ABANDON = (_menu(328),)
GO_APRES_SCORE = (Toucher(832, 535, apres=0.8),)

TRIAL_PLATEAU = (_menu(272), Toucher(944, 546, apres=1.2))
TRIAL_MENU = (Toucher(1205, 24, apres=0.8),)
TRIAL_ABANDON = (_menu(488),)
TRIAL_CONFIRMER = (Toucher(460, 418, apres=0.8),)

DAMES_PARTIE = (_menu(195), _menu(515), Toucher(266, 480, apres=0.8))  # pion choisi, coups montrés
DAMES_ABANDON = (Toucher(1125, 366, apres=0.8),)
DAMES_HUB = (_menu(275),)

ROI_PARTIE = (_menu(241), _menu(521), Toucher(401, 593, apres=0.8))   # pion e2 choisi
ROI_MENU = (Toucher(1164, 666, apres=0.8),)
ROI_ABANDON = (_menu(451),)
ROI_RETOUR = (_menu(311),)


ECRANS: tuple[Ecran, ...] = (
    # --- Écran principal : variantes -----------------------------------------------
    # Mode météo. Avec les pièces (ADR-0023), chaque page montre aussi, dans les épaules
    # de ses tuiles, les appareils de sa pièce (tools/demo/scenarios.py, PIECES).
    Ecran("accueil-previsions-jours-2", (VERS_LA_GAUCHE,), (VERS_LA_DROITE,)),
    Ecran("accueil-previsions-jours-3", (VERS_LA_GAUCHE, VERS_LA_GAUCHE), (VERS_LA_DROITE, VERS_LA_DROITE)),
    Ecran("accueil-previsions-heures-1", (VERS_LA_DROITE,), (VERS_LA_GAUCHE,)),
    Ecran("accueil-previsions-heures-2", (VERS_LA_DROITE, VERS_LA_DROITE), (VERS_LA_GAUCHE, VERS_LA_GAUCHE)),
    # Interrupteur « Tab5 Appareils sur la météo » éteint : l'accueil (pièce de la démo
    # avec ses appareils) montre les prévisions seules ; puis une page horaire.
    Ecran("accueil-sans-appareils", (_appareils_meteo(False),), AVEC_APPAREILS),
    Ecran("accueil-sans-appareils-heures-1", (_appareils_meteo(False), VERS_LA_DROITE),
          AVEC_APPAREILS + (VERS_LA_GAUCHE,)),
    # Prévisions périmées (08/10/2026) : HA muet depuis la veille à 21:04, mention
    # « Prévisions d'hier 21 h 04 » au-dessus des tuiles, à droite.
    Ecran("accueil-previsions-perimees", PREVISIONS_DE_LA_VEILLE, PREVISIONS_FRAICHES),
    # Mode HA : les cartes de chaque pièce (remplace « accueil-interrupteurs », les cinq
    # cartes fixes d'avant la 3.2, devenu « accueil-ha-piece-1 »).
    *ecrans_des_pieces(PIECES),
    Ecran("accueil-mode-discussion", (Toucher(*DISCU),), (Toucher(*DOMO),)),
    Ecran("accueil-vigilance",
          (Service("tab5_maj_alerte_meteo_france", (("payload", VIGILANCE_ORANGE),)), _panneau(2)),
          (Service("tab5_maj_alerte_meteo_france", (("payload", VIGILANCE_SCENE_3),)), _panneau(3))),
    Ecran("accueil-alertes-ha",
          (Service("tab5_maj_alertes_ha_bulk", (("payload", ALERTES_HA),)), _panneau(4)),
          (Service("tab5_maj_alertes_ha_bulk", (("payload", ""),)), _panneau(3))),
    Ecran("accueil-alertes-ha-compteur",
          (Service("tab5_maj_alertes_ha_bulk", (("payload", ALERTES_HA_SIX),)), _panneau(5)),
          (Service("tab5_maj_alertes_ha_bulk", (("payload", ""),)), _panneau(3))),
    # Le planning du jour touché reste 6 s ; la réponse vocale 8 s, puis relance le
    # rotateur (tab5-assist.yaml) : on l'arrête de nouveau sur le panneau des scènes.
    Ecran("accueil-planning-du-jour", (Toucher(*TUILE_J1_TEMP),), (Attendre(6.5),)),
    Ecran("accueil-reponse-vocale",
          (Service("tab5_maj_reponse_vocale", (("texte", "Le volet de la serre est fermé."),)),),
          (Attendre(8.5), _panneau(3))),
    # Batterie de la tablette montée : icône en fin de bandeau d'état, glyphe et couleur
    # selon le niveau (échelle du téléphone), éclair pendant la charge.
    Ecran("accueil-batterie-pleine", (_batterie(True, 95.0),), SANS_BATTERIE),
    Ecran("accueil-batterie-faible", (_batterie(True, 12.0),), SANS_BATTERIE),
    Ecran("accueil-batterie-en-charge", (_batterie(True, 60.0, True),), SANS_BATTERIE),
    # Sans batterie, sur l'USB (discussion #278, 05/10/2026) : une prise, niveau inconnu
    # (« Tab5 Batterie » ne vaut rien sans batterie), chargeur coupé (08/10/2026).
    Ecran("accueil-batterie-prise", (_batterie(True, tension=TENSION_SANS_BATTERIE),),
          SANS_BATTERIE),
    # Production solaire (clé solaire, % de la crête) : icône avant la batterie, couleur
    # du barème des batteries, panneau gris à 0 % (la nuit). Un écran par palier, puis
    # les deux icônes ensemble pour l'alignement.
    Ecran("accueil-solaire-nuit", (_solaire("0"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-faible", (_solaire("12"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-moyen", (_solaire("30"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-bon", (_solaire("60"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-fort", (_solaire("95"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-et-batterie", (_batterie(True, 95.0), _solaire("60")), SANS_BATTERIE + SANS_SOLAIRE),
    # Appuis longs au choix (07/10/2026) : la mini icône de chaque bouton du haut montre
    # l'écran choisi (en-tête de sa fenêtre) : alertes, calendrier, Arcade.
    Ecran("accueil-appuis-choisis", (_appuis("alertes", "calendrier", "arcade"),), APPUIS_AUTO),
    # Gestes au choix (09/10/2026, lot A) : taps des trois boutons changés (calendrier,
    # écoute « Ok Nabu », ligne suivante : leur icône centrale le dit), appuis longs changés
    # (mode Domo, appareil suivant, réveil : la mini icône).
    Ecran("accueil-gestes-choisis",
          (_gestes("auto", "auto", "auto", "auto", "auto", "auto",
                   "calendrier", "mode_domo", "ecoute", "appareil_suivant", "rangee_suivante", "reveil"),),
          GESTES_AUTO),
    # Tap sur la date (« auto ») : la ligne suivante de la rangée, comme un tap sur elle.
    Ecran("accueil-date-rangee-ligne-2", (Toucher(*DATE),), (Toucher(*SOUS_HORLOGE), Toucher(*SOUS_HORLOGE))),
    # Rangée sous l'horloge (ADR-0031) : trois lignes dans la démo (plantes, climat,
    # énergie et maison ; scenarios.RANGEE). Un appui passe à la suivante ; le retour à
    # l'accueil ne la remet pas, `fermer` finit le tour jusqu'à la première.
    Ecran("accueil-rangee-ligne-2", (Toucher(*SOUS_HORLOGE),), (Toucher(*SOUS_HORLOGE), Toucher(*SOUS_HORLOGE))),
    Ecran("accueil-rangee-ligne-3", (Toucher(*SOUS_HORLOGE), Toucher(*SOUS_HORLOGE)), (Toucher(*SOUS_HORLOGE),)),
    # Panneau Ok Nabu (lot 3, ADR-0041) : trois lignes (l'écoute, une ligne « air », une
    # ligne « ouvertures » ; scenarios.NABU_TROIS_LIGNES), trois pastilles dessous ; le tap
    # des heures (« auto ») passe à la suivante. Puis une ligne de capteurs seule, l'écoute
    # masquée (sans pastilles), et la ligne « air » dans le thème à la police la plus large
    # puis dans celui au cadre le plus serré.
    Ecran("accueil-nabu-ligne-1", _nabu(NABU_TROIS_LIGNES), NABU_DE_LA_DEMO),
    Ecran("accueil-nabu-ligne-2", _nabu(NABU_TROIS_LIGNES) + (Toucher(*HEURES),), NABU_DE_LA_DEMO),
    Ecran("accueil-nabu-ligne-3", _nabu(NABU_TROIS_LIGNES) + (Toucher(*HEURES), Toucher(*HEURES)),
          NABU_DE_LA_DEMO),
    Ecran("accueil-nabu-une-ligne", _nabu(NABU_UNE_LIGNE), NABU_DE_LA_DEMO),
    Ecran("accueil-nabu-police-large",
          (Choisir("Thème", THEME_POLICE_LARGE),) + _nabu(NABU_TROIS_LIGNES) + (Toucher(*HEURES),),
          NABU_DE_LA_DEMO + (Choisir("Thème", THEME_PAR_DEFAUT),)),
    Ecran("accueil-nabu-gelule",
          (Choisir("Thème", THEME_CADRE_GELULE),) + _nabu(NABU_TROIS_LIGNES) + (Toucher(*HEURES),),
          NABU_DE_LA_DEMO + (Choisir("Thème", THEME_PAR_DEFAUT),)),
    # Zone à gauche de l'horloge (ADR-0051) : le tap sur la seconde température passe du
    # vocal au graphique des 15 heures de la démo (cycle par défaut : vocal, graphique) ;
    # `fermer` revient au vocal (le choix est gardé en NVS). Puis le même graphique dans le
    # thème au cadre le plus arrondi (gélule).
    Ecran("accueil-zone-graphique", (Toucher(*SERRE),), (Toucher(*SERRE),)),
    Ecran("accueil-zone-graphique-gelule", (Choisir("Thème", THEME_CADRE_GELULE), Toucher(*SERRE)),
          (Toucher(*SERRE), Choisir("Thème", THEME_PAR_DEFAUT))),
    # Tuile − / + (ADR-0033) : la liste par l'appui long sur la valeur entre − et +
    # (ADR-0038 ; clim, les quatre appareils de la démo, scenarios.REGLABLES, la tablette ;
    # le retour à l'accueil la ferme), puis l'enceinte (ligne 4) choisie à la place de la
    # clim ; le choix reste en NVS : `fermer` remet la clim.
    Ecran("accueil-tuile-liste", (Long(*CONSIGNE_CLIM),)),
    Ecran("accueil-tuile-enceinte", (Long(*CONSIGNE_CLIM), Toucher(*LIGNES_REGLABLES[4])),
          (Long(*CONSIGNE_CLIM), Toucher(*LIGNES_REGLABLES[0]))),
    # Tap sur les minutes (« auto », lot A) : l'appareil suivant de la liste (le premier de
    # la démo après la clim) sans la dérouler ; `fermer` remet la clim (la liste par
    # l'appui long sur la valeur, ADR-0038).
    Ecran("accueil-minutes-appareil-suivant", (Toucher(*MINUTES),),
          (Long(*CONSIGNE_CLIM), Toucher(*LIGNES_REGLABLES[0]))),

    # --- Fenêtres ---------------------------------------------------------------------
    # Réveil : appui long sur les heures (lot A ; les minutes l'ouvrent aussi), toujours
    # sur la page Heure ; les autres pages par leur nom en haut, Ouverture par un geste.
    Ecran("reveil", (Service("tab5_maj_rdv_prochains", (("payload", RDV),)), Long(*HEURES))),
    Ecran("reveil-ouverture", (Long(*HEURES), Toucher(*REVEIL_PAGES["jours"]), REVEIL_GLISSER_DEPUIS_UN_BOUTON)),
    Ecran("reveil-jours", (Long(*HEURES), Toucher(*REVEIL_PAGES["jours"]))),
    Ecran("reveil-reglages-sonnerie", (Long(*HEURES), Toucher(*REVEIL_PAGES["sonnerie"]))),
    Ecran("reveil-annonces", (Service("tab5_maj_rdv_prochains", (("payload", RDV),)), Long(*HEURES),
                              Toucher(*REVEIL_PAGES["annonces"]))),
    Ecran("reveil-sonnerie", (Long(*HEURES), Toucher(*REVEIL_TESTER, apres=1.5)),
          (Toucher(*SONNERIE_ARRETER),)),
    Ecran("assistant", (Long(*MICRO),)),
    Ecran("assistant-reponse",
          (Service("tab5_assist_reponse", (("texte", REPONSE_ASSISTANT), ("image_url", ""))),)),
    # Calendrier : appui long sur la date (lot A).
    Ecran("calendrier", (Service("tab5_maj_calendrier_mois", _calendrier_juin_2026()), Long(*DATE))),
    Ecran("calendrier-jour", (Long(*DATE), Toucher(*CAL_JOUR_18))),
    Ecran("lumieres-chambre", (Long(*TUILES["chambre"]), roue_reglages(TUILES["chambre"], ROUE_BOUTONS["chambre"]))),
    Ecran("lumieres-salon", (Long(*TUILES["salon"]), roue_reglages(TUILES["salon"], ROUE_BOUTONS["salon"]))),
    Ecran("volet", (Long(*TUILE_VOLET), roue_reglages(TUILE_VOLET, ROUE_BOUTONS["volet"]))),
    Ecran("volet-sans-position", (VOLET_SANS_POSITION, Long(*TUILE_VOLET),
                                  roue_reglages(TUILE_VOLET, ROUE_BOUTONS["volet-sans-position"])),
          (VOLET_DE_LA_DEMO,)),
    Ecran("volet-glisse", (Long(*TUILE_VOLET), roue_reglages(TUILE_VOLET, ROUE_BOUTONS["volet"]), VOLET_TIRE)),
    # Popups Lumières et Volets en pages (ADR-0046) : par « Aller à l'écran » (la pièce de
    # l'accueil, ou la première qui en a ; Lumières : « aller-lumieres », plus bas), la page
    # suivante d'un glissement, une pièce par son nom, la roue d'une ligne ; une seule pièce
    # de volets (ni noms ni glissement).
    Ecran("lumieres-page-suivante", (Aller("Lumières"), LUMIERES_SUIVANTE)),
    Ecran("lumieres-onglet", (Aller("Lumières"), Toucher(*LUMIERES_ONGLET_3))),
    Ecran("lumieres-roue", (Aller("Lumières"), Long(*LUMIERES_LIGNE_0)), (ROUE_FERMER,)),
    Ecran("aller-volet", (Aller("Volet"),)),
    Ecran("volets-page-suivante", (Aller("Volet"), VOLETS_SUIVANTE)),
    Ecran("volets-une-piece", (MAISON_DEUX_PIECES, Aller("Volet")), MAISON_DE_LA_DEMO),
    # Roue d'actions rapides (ADR-0036) : la lampe, ses luminosités dépliées (50 % marqué)
    # puis ses couleurs ; le volet, ses positions dépliées (50 % marqué). Fermer : un
    # toucher replie le second anneau, le suivant ferme la roue. Celle de la clim est la
    # dernière capture (ses capacités restent reçues jusqu'au redémarrage).
    Ecran("roue-lampe", (LAMPE_A_50, Long(*TUILES["chambre"]),
                         roue_famille(TUILES["chambre"], ROUE_BOUTONS["chambre"], ROUE_LUMINOSITE)),
          (ROUE_FERMER, ROUE_FERMER, LAMPE_DE_LA_DEMO)),
    Ecran("roue-lampe-couleurs", (Long(*TUILES["chambre"]),
                                  roue_famille(TUILES["chambre"], ROUE_BOUTONS["chambre"], ROUE_COULEURS)),
          (ROUE_FERMER, ROUE_FERMER)),
    Ecran("roue-volet", (VOLET_A_50, Long(*TUILE_VOLET),
                         roue_famille(TUILE_VOLET, ROUE_BOUTONS["volet"], ROUE_POSITION)),
          (ROUE_FERMER, ROUE_FERMER, VOLET_DE_LA_DEMO)),
    # Popup d'un appareil : un interrupteur « allumer seulement », une scène, et une
    # scène à confirmer après un appui sur le grand bouton (« Confirmer ? »).
    Ecran("appareil", (VERS_LA_DROITE, Long(*TUILE_ORDINATEUR)), (Toucher(*FERMER_POPUP), VERS_LA_GAUCHE)),
    Ecran("appareil-scene", (Long(*TUILE_SCENE),)),
    Ecran("appareil-confirmer", (VERS_LA_GAUCHE, Long(*TUILE_JE_PARS), Toucher(*BOUTON_APPAREIL)),
          (Attendre(3.5), Toucher(*FERMER_POPUP), VERS_LA_DROITE)),
    Ecran("climatisation", (Toucher(*CONSIGNE_CLIM),)),
    # La clim d'une pièce (ADR-0040), ouverte par la tuile − / + en mode HA sur elle.
    Ecran("climatisation-piece", ALLER_PIECE_CLIMAT + (Toucher(*CONSIGNE_CLIM),),
          (Toucher(*FERMER_POPUP),) + RETOUR_PIECE_CLIMAT),
    # Carrousel des clims (ADR-0038) par la température de la pièce : la démo a la clim du
    # blueprint et la clim propre du Bureau (ADR-0040) : deux pastilles, sur la première.
    Ecran("climatisation-par-la-piece", (Toucher(*SALON),)),
    # Puis quatre clims (CLIMS_DE_TUILES, et la clim propre du Bureau, ADR-0040, en
    # dernier) : la clim du blueprint (première page), un geste vers la gauche, la clim du
    # bureau, un second, la clim sans nom (titre « Bureau », trois boutons). En mode HA sur
    # le Bureau, le toucher de la température ouvre sur la clim propre de la pièce
    # (« Climatiseur du bureau », quatrième page). `fermer` repousse les définitions de la
    # démo : les deux clims de tuile sont oubliées, rien ne reste pour les écrans suivants.
    Ecran("climatisation-carrousel", CLIMS_DE_TUILES + (Toucher(*SALON),), MAISON_DE_LA_DEMO),
    Ecran("climatisation-carrousel-page-2", CLIMS_DE_TUILES + (Toucher(*SALON), CARROUSEL_SUIVANTE),
          MAISON_DE_LA_DEMO),
    Ecran("climatisation-carrousel-page-3",
          CLIMS_DE_TUILES + (Toucher(*SALON), CARROUSEL_SUIVANTE, CARROUSEL_SUIVANTE), MAISON_DE_LA_DEMO),
    # En mode HA, la clim propre du Bureau a ses réglages : le toucher ouvre sa roue
    # (ADR-0048), son « Détails » le carrousel sur elle.
    Ecran("climatisation-carrousel-mode-ha",
          CLIMS_DE_TUILES + (Toucher(*BOUTON_HA), HA_VERS_LA_DROITE, Toucher(*SALON), ROUE_CLIM_DETAILS),
          (Toucher(*FERMER_POPUP), Toucher(*BOUTON_HA), VERS_LA_GAUCHE) + MAISON_DE_LA_DEMO),
    # Roue de la clim par la température de la pièce (ADR-0048), en mode HA sur le Bureau :
    # sa clim propre (« Climatiseur du bureau », en froid) ; puis « Clims ▸ » déplié (la
    # clim du blueprint et elle, marquée). Un toucher replie, le suivant ferme.
    Ecran("roue-clim-temperature", ALLER_PIECE_CLIMAT + (Toucher(*SALON),),
          (ROUE_FERMER,) + RETOUR_PIECE_CLIMAT),
    Ecran("roue-clim-temperature-clims", ALLER_PIECE_CLIMAT + (Toucher(*SALON), ROUE_CLIM_CLIMS),
          (ROUE_FERMER, ROUE_FERMER) + RETOUR_PIECE_CLIMAT),
    # Historique des alertes (lot 4 du plan des alertes) : la liste d'abord, comme HA la
    # pousserait, puis l'appui long sur la carte centrale et « Alertes », premier bouton de
    # la roue de navigation (ADR-0042).
    Ecran("alertes", (Service("tab5_maj_alertes_historique", (("payload", HISTORIQUE_ALERTES),)),
                      Long(*CARTE_CENTRALE), nav(NAV_ALERTES))),
    # Roue de navigation (ADR-0042) : repliée (un mot par bouton, « Aller à » au moyeu), puis
    # trois familles dépliées (le moyeu dit laquelle). Fermer : un toucher replie, le
    # suivant ferme.
    Ecran("roue-navigation", (Long(*CARTE_CENTRALE),), (ROUE_FERMER,)),
    Ecran("roue-navigation-pieces", (Long(*CARTE_CENTRALE), nav(NAV_PIECES)), (ROUE_FERMER, ROUE_FERMER)),
    # Appareils ▸ : Températures … Plantes, puis Musique et TV (10/10/2026) : sept choix
    # dans cette scène (pas d'Énergie), le second anneau le plus large de la démo.
    Ecran("roue-navigation-appareils", (Long(*CARTE_CENTRALE), nav(NAV_APPAREILS)), (ROUE_FERMER, ROUE_FERMER)),
    Ecran("roue-navigation-tablette", (Long(*CARTE_CENTRALE), nav(NAV_TABLETTE)), (ROUE_FERMER, ROUE_FERMER)),
    # Agenda ▸ : Météo (ADR-0043) en tête, puis Calendrier, Réveil et Caméras (ADR-0049).
    Ecran("roue-navigation-agenda", (Long(*CARTE_CENTRALE), nav(NAV_AGENDA)), (ROUE_FERMER, ROUE_FERMER)),
    # Une pièce choisie dans la roue : le mode HA sur elle, sans swipe.
    Ecran("roue-navigation-bureau", (Long(*CARTE_CENTRALE), nav(NAV_PIECES), NAV_BUREAU), RETOUR_PIECE_CLIMAT),
    # Les popups d'une tuile ouverts sans tuile (ADR-0042) : les lumières de la pièce
    # affichée (l'accueil : la Chambre et le Salon), par « Aller à l'écran ».
    Ecran("aller-lumieres", (Aller("Lumières"),)),
    # Popup Maison (ADR-0037) : les cinq pièces de la démo par « Aller à l'écran », puis par
    # un tap sur le titre de la pièce en mode HA, puis deux pièces (colonnes plus larges).
    Ecran("maison", (Aller("Maison"),)),
    Ecran("maison-par-le-titre", (Toucher(*BOUTON_HA), Toucher(*CARTE_CENTRALE)),
          (Toucher(*FERMER_POPUP), Toucher(*BOUTON_HA))),
    Ecran("maison-2-pieces", (MAISON_DEUX_PIECES, Aller("Maison")), MAISON_DE_LA_DEMO),
    # Roue d'actions rapides (ADR-0036) ouverte par l'appui long d'une ligne, devant le popup
    # Maison qui reste derrière : la lampe d'ambiance du Salon (variateur), ancrée sur la
    # pastille de sa ligne, sans le lien « Maison ». Toucher ailleurs ne ferme que la roue.
    Ecran("maison-roue", (Aller("Maison"), Long(*MAISON_LAMPE)), (ROUE_FERMER,)),
    Ecran("plantes", (Long(*SOUS_HORLOGE),)),
    # Énergie (ADR-0028) : ouvert par la tuile solaire de la démo (vue des heures), puis
    # les vues Jours et Mois par « Aller à l'écran ». Données : la scène (demo_pusher,
    # _pousser_energie), datée du jour figé des captures.
    Ecran("energie-heures", (VERS_LA_DROITE, Toucher(*TUILE_SOLAIRE)), (Toucher(*FERMER_POPUP), VERS_LA_GAUCHE)),
    Ecran("energie-jours", (Aller("Énergie"), Toucher(*ENERGIE_VUES["jours"]))),
    Ecran("energie-mois", (Aller("Énergie"), Toucher(*ENERGIE_VUES["mois"]))),
    # Température (ADR-0032) : la pièce sur 24 h ; la seconde température, une serre avec
    # la prévision de dehors à part, sur les trois vues ; puis dehors (case du blueprint
    # cochée), la prévision qui prolonge la courbe.
    Ecran("temperature-salon", (Long(*SALON_TEMP), _historique("salon", "jour"))),
    Ecran("temperature-serre", (Long(*SERRE), _historique("serre", "jour"))),
    Ecran("temperature-serre-semaine",
          (Long(*SERRE), Toucher(*TEMPERATURE_VUES["semaine"]), _historique("serre", "semaine"))),
    Ecran("temperature-serre-mois", (Long(*SERRE), Toucher(*TEMPERATURE_VUES["mois"]), _historique("serre", "mois"))),
    Ecran("temperature-dehors", (Long(*SERRE), _historique("serre", "jour", exterieur=True))),
    # Température d'une pièce en mode HA (ADR-0040) : sans prévision ; le Bureau a aussi son
    # humidité (ADR-0047), tracée avec la température.
    Ecran("temperature-piece",
          ALLER_PIECE_CLIMAT + (Long(*SALON_TEMP), _historique(f"p{PIECE_CLIMAT}", "jour")),
          (Toucher(*FERMER_POPUP),) + RETOUR_PIECE_CLIMAT),
    # Pages du popup (ADR-0047) : du salon, un glissement montre l'Entrée (sa température
    # seule : le rendu d'avant l'humidité) ; l'onglet du Bureau, sa semaine.
    Ecran("temperature-glisser",
          (Long(*SALON_TEMP), _historique("salon", "jour"), TEMPERATURE_SUIVANTE, _historique("p1", "jour"))),
    Ecran("temperature-onglet",
          (Long(*SALON_TEMP), _historique("salon", "jour"), Toucher(*TEMPERATURE_VUES["semaine"]),
           _historique("salon", "semaine"), Toucher(*TEMPERATURE_ONGLETS["p3"]), _historique("p3", "semaine"))),
    # Popup Météo (ADR-0043) : ses trois pages, avec des données qui varient ; la page
    # « 10 jours » par un geste dans le popup, « Détails » par son nom en haut.
    Ecran("meteo-aujourdhui", METEO_DONNEES + (Aller("Météo"),), METEO_SCENE_3),
    Ecran("meteo-jours", METEO_DONNEES + (Aller("Météo"), METEO_GLISSER), METEO_SCENE_3),
    Ecran("meteo-details", METEO_DONNEES + (Aller("Météo"), Toucher(*METEO_PAGES["details"])), METEO_SCENE_3),
    # Popup Musique (ADR-0050) : vide (« En attente de Home Assistant »), en pause avec ses
    # trois lecteurs, éteint ; la barre « en lecture » de l'accueil, sur le cadre Ok Nabu.
    Ecran("musique-vide", (Aller("Musique"),), (LECTEUR_VOILE,)),
    Ecran("musique", (LECTEUR_EN_PAUSE, Aller("Musique")), (LECTEUR_VOILE, LECTEUR_AUCUN)),
    Ecran("musique-eteint", (LECTEUR_ETEINT, Aller("Musique")), (LECTEUR_VOILE, LECTEUR_AUCUN)),
    Ecran("accueil-musique", (LECTEUR_EN_PAUSE,), (LECTEUR_AUCUN,)),
    # Lecteur compact dans la zone à gauche de l'horloge (ADR-0051, lot 2) : en pause (la
    # barre « en lecture » se masque, le cadre Ok Nabu reste), allumé sans rien en lecture,
    # puis dans le thème au cadre le plus arrondi (gélule).
    Ecran("accueil-zone-lecteur", (LECTEUR_EN_PAUSE, ZONE_LECTEUR), (ZONE_VOCAL, LECTEUR_AUCUN)),
    Ecran("accueil-zone-lecteur-vide", (LECTEUR_INACTIF, ZONE_LECTEUR), (ZONE_VOCAL, LECTEUR_AUCUN)),
    Ecran("accueil-zone-lecteur-gelule", (Choisir("Thème", THEME_CADRE_GELULE), LECTEUR_EN_PAUSE, ZONE_LECTEUR),
          (ZONE_VOCAL, LECTEUR_AUCUN, Choisir("Thème", THEME_PAR_DEFAUT))),
    # Popup Caméras (ADR-0049, ADR-0057) : sept caméras dans trois pièces et « Autres ». La
    # mosaïque de « Toutes » (2 × 2, première page), sa seconde page (trois cases, l'abri
    # hors ligne), la première caméra en grand (tap sur sa case), puis la pièce Jardin
    # (trois caméras) et son abri hors ligne en grand. `fermer` revient à la mosaïque de
    # « Toutes », première page, sur le Portail, quel que soit l'ordre des écrans (pièce,
    # caméra et vue sont gardées en NVS, la page tant que la tablette tourne ; « Toutes »
    # garde la caméra montrée : Entrée d'abord, qui montre le Portail).
    Ecran("cameras", (CAMERAS_DONNEES, Aller("Caméras"), Attendre(1.5))),
    Ecran("cameras-mosaique-page-2",
          (CAMERAS_DONNEES, Aller("Caméras"), Attendre(0.6), CAMERAS_SUIVANTE, Attendre(1.5)),
          (CAMERAS_PRECEDENTE,)),
    Ecran("cameras-plein-ecran",
          (CAMERAS_DONNEES, Aller("Caméras"), Attendre(0.6), _camera_vignette(0), Attendre(1.0)),
          (CAMERAS_MOSAIQUE,)),
    Ecran("cameras-piece-hors-ligne",
          (CAMERAS_DONNEES, Aller("Caméras"), _camera_puce(2), Attendre(0.6), _camera_vignette(2, 3),
           Attendre(1.0)),
          (CAMERAS_MOSAIQUE, _camera_puce(1), _camera_puce(0))),
    # Capteurs suivis (ADR-0054) : le popup avant toute poussée (« En attente de Home
    # Assistant »), avec quatre capteurs (deux rangées : 3 colonnes au plus), puis la carte
    # du premier dans la zone à gauche de l'horloge, dans le thème par défaut et en gélule.
    Ecran("suivi-vide", (Aller("Suivi"),)),
    Ecran("suivi", (SUIVI_DONNEES, Aller("Suivi"))),
    Ecran("accueil-zone-capteur", (SUIVI_DONNEES, ZONE_CAPTEUR), (ZONE_VOCAL,)),
    Ecran("accueil-zone-capteur-gelule", (Choisir("Thème", THEME_CADRE_GELULE), SUIVI_DONNEES, ZONE_CAPTEUR),
          (ZONE_VOCAL, Choisir("Thème", THEME_PAR_DEFAUT))),
    # Réfrigérateurs et congélateurs (ADR-0055) : le popup avant toute poussée (« En
    # attente de Home Assistant »), avec un réfrigérateur au niveau grave (porte mal
    # fermée) et un congélateur conforme, puis l'accueil, où l'icône clignote dans le coin
    # de l'horloge (capturée à un instant quelconque du clignotement : pas comparée).
    Ecran("froid-vide", (Aller("Froid"),)),
    Ecran("froid", (FROID_DONNEES, Aller("Froid")), (FROID_CONFORME,)),
    Ecran("accueil-froid", (FROID_DONNEES,), (FROID_CONFORME,), stable=False),
    # Serveur IA (ADR-0059) : un serveur qui génère (courbe des 24 dernières poussées), le
    # même hors ligne, puis aucun capteur choisi (où les choisir).
    Ecran("serveur-ia", SERVEUR_IA_COURBE + (Aller("Serveur IA"),), (SERVEUR_IA_REMISE,)),
    Ecran("serveur-ia-hors-ligne", SERVEUR_IA_COURBE + (SERVEUR_IA_HORS_LIGNE, Aller("Serveur IA")),
          (SERVEUR_IA_REMISE,)),
    Ecran("serveur-ia-vide", (SERVEUR_IA_AUCUN, Aller("Serveur IA")), (SERVEUR_IA_REMISE,)),
    Ecran("telecommande-tv", (Long(*BOUTON_TV),)),
    # Plusieurs télécommandes (ADR-0056) : la page de la TV avec les noms en haut, puis un
    # boîtier en première page (Stop, icône du volume, rangée de lecture). La liste vidée
    # ensuite : la page unique d'avant pour les écrans suivants.
    Ecran("telecommande-plusieurs", (TELECOMMANDES_TV_D_ABORD, Long(*BOUTON_TV)), (TELECOMMANDES_AUCUNE,)),
    Ecran("telecommande-boitier", (TELECOMMANDES_BOITIER_D_ABORD, Long(*BOUTON_TV)), (TELECOMMANDES_AUCUNE,)),
    # Réglages (quatre pages, 08/10/2026). L'engrenage ouvre la page Écran ; un glisser
    # vers la gauche parti d'un bouton montre la page Apparence sans appuyer le bouton.
    Ecran("reglages-apparence", (Toucher(*BOUTON_SYS), REGLAGES_GLISSER_DEPUIS_UN_BOUTON)),
    Ecran("reglages", (Toucher(*BOUTON_SYS),)),
    Ecran("reglages-curseur", (Toucher(*BOUTON_SYS), REGLAGES_GLISSER_CURSEUR),
          (REGLAGES_CURSEUR_A_100,)),
    Ecran("reglages-langue", (Toucher(*BOUTON_SYS), Toucher(*REGLAGES_PAGES["apparence"]),
                              Toucher(*REGLAGES_LANGUE_EN, selon_langue=(("en", *REGLAGES_LANGUE_FR),))),
          (Toucher(*REGLAGES_ANNULER),)),
    # Page Batterie, par son nom en haut : en charge, puis sans batterie (tension sous 3 V).
    Ecran("reglages-batterie-en-charge",
          (_batterie(True, 60.0, True), Toucher(*BOUTON_SYS), Toucher(*REGLAGES_PAGES["batterie"])),
          SANS_BATTERIE),
    Ecran("reglages-sans-batterie",
          (_batterie(True, tension=TENSION_SANS_BATTERIE), Toucher(*BOUTON_SYS),
           Toucher(*REGLAGES_PAGES["batterie"])),
          SANS_BATTERIE),
    # Page Système (l'ancienne console système) : appui long sur l'engrenage.
    Ecran("console-systeme", (Long(*BOUTON_SYS),)),
    # Ligne « Batterie » de la carte SYSTÈME (discussion #278, 06/10/2026) : batterie
    # détectée (niveau, tension, icône du bandeau), en charge, puis sans batterie (« Sur
    # USB », la prise). Sans l'interrupteur, « console-systeme » montre « Non montée ».
    Ecran("console-batterie", (_batterie(True, 78.0), Long(*BOUTON_SYS)), SANS_BATTERIE),
    Ecran("console-batterie-en-charge", (_batterie(True, 60.0, True), Long(*BOUTON_SYS)), SANS_BATTERIE),
    Ecran("console-sans-batterie",
          (_batterie(True, tension=TENSION_SANS_BATTERIE), Long(*BOUTON_SYS)), SANS_BATTERIE),
    Ecran("console-confirmer-redemarrage-ha", (Long(*BOUTON_SYS), Toucher(*CONSOLE_REDEMARRER_HA)),
          (Toucher(*CONFIRMATION_ANNULER),)),
    Ecran("console-confirmer-reboot", (Long(*BOUTON_SYS), Toucher(*CONSOLE_REBOOT)),
          (Toucher(*CONFIRMATION_ANNULER),)),

    # --- Arcade -----------------------------------------------------------------------
    Ecran("arcade", (Toucher(*BOUTON_TV, apres=1.0),)),

    _jeu("", "fil-dor"),
    _jeu("feu-de-camp", "fil-dor", (_menu(249),)),
    _jeu("marchand", "fil-dor", (_menu(317),)),
    _jeu("equipement", "fil-dor", (_menu(385),)),
    _jeu("reglages", "fil-dor", (_menu(453),)),
    _jeu("statistiques", "fil-dor", (_menu(521),)),
    # La salle de Fil d'Or est tirée d'une graine prise sur lv_tick_get() (marble_game.cpp).
    _jeu("partie", "fil-dor", FIL_DOR_PARTIE, (_menu(24),) + FIL_DOR_ABANDON + FIL_DOR_HUB, stable=False),
    _jeu("pause", "fil-dor", FIL_DOR_PAUSE, FIL_DOR_ABANDON + FIL_DOR_HUB, stable=False),
    _jeu("fin", "fil-dor", FIL_DOR_PAUSE + FIL_DOR_ABANDON, FIL_DOR_HUB, stable=False),

    _jeu("", "arcanoide"),
    _jeu("classement", "arcanoide", (_menu(249),)),
    _jeu("reglages", "arcanoide", (_menu(317),)),
    _jeu("partie", "arcanoide", ARCANOIDE_PARTIE),
    _jeu("pause", "arcanoide", ARCANOIDE_PARTIE + (_menu(24),)),

    _jeu("", "neon-apron", portrait=True),
    _jeu("classement", "neon-apron", (_menu(538, 360),), portrait=True),
    _jeu("reglages", "neon-apron", (_menu(634, 360),), portrait=True),
    _jeu("partie", "neon-apron", NEON_PARTIE, (_menu(70, 360),) + NEON_ABANDON + NEON_HUB, portrait=True),
    _jeu("pause", "neon-apron", NEON_PAUSE, NEON_ABANDON + NEON_HUB, portrait=True),
    _jeu("fin", "neon-apron", NEON_PAUSE + NEON_ABANDON, NEON_HUB, portrait=True),

    _jeu("", "coureur-dor"),
    _jeu("niveaux", "coureur-dor", (_menu(297),)),
    _jeu("classement", "coureur-dor", (_menu(367),)),
    _jeu("reglages", "coureur-dor", (_menu(437),)),
    # Les gardes avancent en temps réel : leur position dépend du moment de la capture.
    _jeu("partie", "coureur-dor", COUREUR_PARTIE, (_menu(24),) + COUREUR_QUITTER + COUREUR_HUB, stable=False),
    _jeu("pause", "coureur-dor", COUREUR_PAUSE, COUREUR_QUITTER + COUREUR_HUB, stable=False),
    _jeu("fin", "coureur-dor", COUREUR_PAUSE + COUREUR_QUITTER, COUREUR_HUB, stable=False),

    _jeu("", "go"),
    _jeu("nouvelle-partie", "go", (_menu(180),)),
    _jeu("statistiques", "go", (_menu(254),)),
    _jeu("reglages", "go", (_menu(328),)),
    _jeu("partie", "go", GO_PARTIE, GO_MENU + GO_ABANDON + GO_APRES_SCORE),
    _jeu("pause", "go", GO_PARTIE + GO_MENU, GO_ABANDON + GO_APRES_SCORE),
    _jeu("score", "go", GO_PARTIE + GO_MENU + GO_ABANDON, GO_APRES_SCORE),

    _jeu("", "trial-poursuite"),
    _jeu("nouvelle-partie", "trial-poursuite", (_menu(272),)),
    _jeu("statistiques", "trial-poursuite", (_menu(416),)),
    _jeu("regles", "trial-poursuite", (_menu(488),)),
    _jeu("reglages", "trial-poursuite", (_menu(560),)),
    _jeu("plateau", "trial-poursuite", TRIAL_PLATEAU, TRIAL_MENU + TRIAL_ABANDON + TRIAL_CONFIRMER),
    _jeu("pause", "trial-poursuite", TRIAL_PLATEAU + TRIAL_MENU, TRIAL_ABANDON + TRIAL_CONFIRMER),
    _jeu("abandon", "trial-poursuite", TRIAL_PLATEAU + TRIAL_MENU + TRIAL_ABANDON, TRIAL_CONFIRMER),

    _jeu("", "dames"),
    _jeu("nouvelle-partie", "dames", (_menu(195),)),
    _jeu("statistiques", "dames", (_menu(355),)),
    _jeu("reglages", "dames", (_menu(435),)),
    _jeu("partie", "dames", DAMES_PARTIE, DAMES_ABANDON + DAMES_HUB),
    _jeu("fin", "dames", DAMES_PARTIE + DAMES_ABANDON, DAMES_HUB),

    _jeu("", "roi-noir"),
    _jeu("nouvelle-partie", "roi-noir", (_menu(241),)),
    _jeu("statistiques", "roi-noir", (_menu(311),)),
    _jeu("reglages", "roi-noir", (_menu(381),)),
    _jeu("partie", "roi-noir", ROI_PARTIE, ROI_MENU + ROI_ABANDON + ROI_RETOUR),
    _jeu("pause", "roi-noir", ROI_PARTIE + ROI_MENU, ROI_ABANDON + ROI_RETOUR),
    _jeu("defaite", "roi-noir", ROI_PARTIE + ROI_MENU + ROI_ABANDON, ROI_RETOUR),

    # Roue de la clim (ADR-0036) en dernier, ses modes dépliés : « climr » reçu ne
    # s'efface pas, aucune capture d'après ne doit en dépendre.
    Ecran("roue-clim", (VERS_LA_GAUCHE, VERS_LA_GAUCHE, CLIM_CAPACITES, Long(*TUILES["chambre"]),
                        roue_famille(TUILES["chambre"], ROUE_BOUTONS["clim"], ROUE_MODE)),
          (ROUE_FERMER, ROUE_FERMER, VERS_LA_DROITE, VERS_LA_DROITE)),
    # Roue de la clim par la température de la pièce en mode météo (ADR-0048) : la clim du
    # blueprint, qui n'en a une qu'après « climr » (avant : le carrousel, écrans
    # « climatisation-par-la-piece » et « climatisation-carrousel* »).
    Ecran("roue-clim-temperature-meteo", (CLIM_CAPACITES, Toucher(*SALON)), (ROUE_FERMER,)),
)

# Captures en portrait (pas de rotation en PNG).
PORTRAITS: frozenset = frozenset(e.nom for e in ECRANS if e.portrait)
# Captures qui varient d'un run à l'autre : tools/rendu/comparer.py ne les compare pas.
VARIABLES: frozenset = frozenset(e.nom for e in ECRANS if not e.stable)
