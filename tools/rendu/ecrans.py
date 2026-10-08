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

Les coordonnées viennent des captures et des positions déclarées dans Tab5/*.yaml,
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
from scenarios import (PAGE_DE_LA_PIECE, PIECES, RANGEE, REGLABLES, SCENES, build_alerte_payload,  # noqa: E402
                       build_etats_tuiles, build_historique, build_tuiles_payload, code_pluie)


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
    """Geste du doigt de (x1, y1) à (x2, y2), 300 ms."""
    x1: int
    y1: int
    x2: int
    y2: int
    apres: float = 0.8


@dataclass(frozen=True)
class Aller:
    """Option du select HA « Aller à l'écran » (Tab5/tab5-ha-controls.yaml)."""
    option: str
    apres: float = 0.8


@dataclass(frozen=True)
class Service:
    """Action de l'API du rendu (tab5_maj_* comme HA, ou rendu_*) : `donnees` = paires."""
    nom: str
    donnees: tuple = ()
    apres: float = 0.8


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

HORLOGE = (640, 105)          # court : Réveil · long : Calendrier
MICRO = (206, 145)            # long : Assistant vocal
DOMO, DISCU = (72, 150), (340, 150)
# Rangée du haut (06/10/2026 : fixe, avec ou sans TV) : HA (court : appareils, long :
# Énergie), engrenage (court : Réglages, long : Console système), manette (court :
# Arcade, long : télécommande TV). Le rendu pousse une maison complète (capturer.py,
# aucune zone absente) : la mini icône de la TV est sur la manette.
# tests/test_rendu_ecrans.py les compare aux boutons de Tab5/tab5-lvgl.yaml.
BOUTON_HA, BOUTON_SYS, BOUTON_TV = (917, 65), (1061, 65), (1205, 65)
# Rangée sous l'horloge (ADR-0031, zone btn_rangee) : court, ligne suivante ; long sur la
# ligne des plantes, Plantes.
SOUS_HORLOGE = (640, 270)
SERRE = (1172, 158)           # court : Arcade
# Tuile − / + (ADR-0033) : court sur la température du salon (btn_reglables_liste,
# climate_card.yaml : carte en 855, 110, zone 4..196 × 22..86), la liste ; ses lignes
# (reglables_liste.yaml : panneau en 740, 110, bord 2 + marge 6, lignes de 52 + 2).
SALON = (955, 164)
LIGNES_REGLABLES = tuple((1000, 110 + 2 + 6 + 54 * k + 26) for k in range(10))
CONSIGNE_CLIM = (1061, 251)   # court : Climatisation
TUILE_J1_TEMP = (390, 684)    # court : planning de ce jour, 6 s
CARTE_CENTRALE = (640, 375)   # long : historique des alertes (popup « Alertes »)
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
# de 456 px, de 45 % à 13 % (volet_cadre_rappel, tab5_tuiles.cpp). Vertical, au-dessus
# des tuiles : ni swipe de page ni bouton sous le doigt. Le relâcher envoie « position »,
# que personne n'applique ici : la capture montre le volet là où le doigt l'a laissé.
VOLET_TIRE = Glisser(265, 250, 265, 400)

# Roue d'actions rapides (ADR-0036, 07/10/2026) : l'appui long d'une lampe, d'un volet ou
# d'une clim pose un moyeu sur la tuile et deux anneaux de boutons au-dessus. Premier anneau :
# « Maison », les commandes et les familles de réglages, « Détails » (le popup complet) en
# dernier ; toucher une famille déplie ses choix sur le second, centré sur elle. Géométrie
# de disposer() (Tab5/tab5_roue.cpp) refaite à l'identique, tests/test_roue.py compare les
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


# Tensions lues par l'INA226 : une batterie 2S (détectée) et la tablette sans batterie,
# sur l'USB (5,71 V relevé chez l'auteur le 04/10/2026 ; sous 6,0 V = pas de batterie).
TENSION_BATTERIE = 7.6
TENSION_SANS_BATTERIE = 5.71


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
    """Appuis longs des trois boutons du haut, comme les pousse le blueprint (clé appuis de
    tab5_maj_emplacements, section « Boutons du haut » ; codes de kCodesEcran ou « auto »)."""
    return Service("tab5_maj_emplacements", (("payload", f"appuis|{maison}|{engrenage}|{manette};"),))


# Le choix reste en NVS : retour à « auto » (les mini icônes d'avant) pour les autres écrans.
APPUIS_AUTO = (_appuis("auto", "auto", "auto"),)


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
        page = PAGE_DE_LA_PIECE[r]
        if page not in occupees:
            continue
        if page >= 2:
            aller = (HA_VERS_LA_GAUCHE,) * sum(1 for p in occupees if 2 < p <= page)
            retour = (VERS_LA_DROITE,) * (page - 2)
        else:
            aller = (HA_VERS_LA_DROITE,) * sum(1 for p in occupees if page <= p < 2)
            retour = (VERS_LA_GAUCHE,) * (2 - page)
        ecrans.append(Ecran(f"accueil-ha-piece-{r + 1}", (Toucher(*BOUTON_HA),) + aller,
                            (Toucher(*BOUTON_HA),) + retour))
    return tuple(ecrans)

REVEIL_TESTER = (550, 641)
SONNERIE_ARRETER = (440, 540)
CAL_JOUR_18 = (642, 342)      # cellule du jeudi 18 (rangée 2, colonne 3)
# Carte GESTION de la console : deux rangées de deux boutons de 247 × 107 (06/10/2026).
CONSOLE_REDEMARRER_HA, CONSOLE_REBOOT = (801, 611), (1060, 611)
CONFIRMATION_ANNULER = (813, 596)   # jamais « Confirmer » (1049, 596)
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
# Popup Réglages (reglages_popup.yaml) : carte APPARENCE à (652, 87) à l'écran ; pastilles
# « English » et « Français » (x 158 et 17, y 457, 131 × 56 dans la carte) ; « Annuler » de
# la confirmation (centre de la carte − 130, + 60). Jamais « Confirmer » : la tablette
# redémarrerait. La langue de l'écran n'ouvre pas la confirmation : « Français » en anglais.
REGLAGES_LANGUE_EN, REGLAGES_LANGUE_FR = (875, 572), (734, 572)
REGLAGES_ANNULER = (816, 446)

# Popup Température (ADR-0032, historique_popup.yaml) : appui long sur la température de
# la pièce (btn_reglables_liste, aussi la liste de la tuile − / + au toucher court ; x 859-1051
# et y 132-196 à l'écran) ou sur la seconde
# (SERRE). Boutons de vue : carte du graphique à y 253-685 à l'écran, boutons de 150 × 48
# à 18, 178 et 338 px de son bord droit (x 1241). Le rendu ne répond à aucun événement :
# il pousse lui-même la réponse de script.tab5_historique, datée de l'heure figée.
SALON_TEMP = (954, 158)
TEMPERATURE_VUES = {"jour": (828, 287), "semaine": (988, 287), "mois": (1148, 287)}
MOMENT_DES_CAPTURES = _dt.datetime(2026, 6, 16, 7, 45)


# Popup Maison (ADR-0037) avec deux pièces seulement : le Salon (cinq appareils) et la
# chambre au nom coupé (la clim, deux tuiles espacées). Les pièces 1, 3 et 4 retirées
# repartent grisées (tuiles_definir) : `fermer` repousse les définitions de la démo, puis
# les états de ces trois pièces (aucune clim parmi elles : rien d'autre n'est oublié).
DEUX_PIECES = {r: PIECES[r] for r in (0, 2)}
RETIREES = {r: p for r, p in PIECES.items() if r not in DEUX_PIECES}
assert not any(t.type == "cli" for p in RETIREES.values() for t in p.tuiles.values())
MAISON_DEUX_PIECES = Service("tab5_maj_tuiles", (("payload", build_tuiles_payload(DEUX_PIECES, RANGEE, REGLABLES)),))
# Ligne de la lampe d'ambiance (Salon, 3e ligne) : colonne 0 de 235 px, lignes de 104 px tous
# les 112 px à partir de y = 120 dans la carte (disposer(), Tab5/tab5_maison.cpp). Le doigt
# entre la pastille et le nom, loin du « ⋯ ».
MAISON_LAMPE = (104, 412)
MAISON_DE_LA_DEMO = (Service("tab5_maj_tuiles", (("payload", build_tuiles_payload(PIECES, RANGEE, REGLABLES)),)),
                     Service("tab5_maj_emplacements", (("payload", build_etats_tuiles(RETIREES)),)))


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
    """Accueil → sélecteur Arcade → console `jeu` → étapes."""
    return (Toucher(*SERRE, apres=1.0), Toucher(*CARTES[jeu], apres=1.2)) + etapes


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
    # (« Tab5 Batterie » ne vaut rien sans batterie) et le chargeur qui dit « en charge ».
    Ecran("accueil-batterie-prise", (_batterie(True, en_charge=True, tension=TENSION_SANS_BATTERIE),),
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
    # Rangée sous l'horloge (ADR-0031) : trois lignes dans la démo (plantes, climat,
    # énergie et maison ; scenarios.RANGEE). Un appui passe à la suivante ; le retour à
    # l'accueil ne la remet pas, `fermer` finit le tour jusqu'à la première.
    Ecran("accueil-rangee-ligne-2", (Toucher(*SOUS_HORLOGE),), (Toucher(*SOUS_HORLOGE), Toucher(*SOUS_HORLOGE))),
    Ecran("accueil-rangee-ligne-3", (Toucher(*SOUS_HORLOGE), Toucher(*SOUS_HORLOGE)), (Toucher(*SOUS_HORLOGE),)),
    # Tuile − / + (ADR-0033) : la liste (clim, les quatre appareils de la démo,
    # scenarios.REGLABLES, la tablette ; le retour à l'accueil la ferme), puis l'enceinte
    # (ligne 4) choisie à la place de la clim ; le choix reste en NVS : `fermer` remet la clim.
    Ecran("accueil-tuile-liste", (Toucher(*SALON),)),
    Ecran("accueil-tuile-enceinte", (Toucher(*SALON), Toucher(*LIGNES_REGLABLES[4])),
          (Toucher(*SALON), Toucher(*LIGNES_REGLABLES[0]))),

    # --- Fenêtres ---------------------------------------------------------------------
    Ecran("reveil", (Service("tab5_maj_rdv_prochains", (("payload", RDV),)), Toucher(*HORLOGE))),
    Ecran("reveil-sonnerie", (Toucher(*HORLOGE), Toucher(*REVEIL_TESTER, apres=1.5)),
          (Toucher(*SONNERIE_ARRETER),)),
    Ecran("assistant", (Long(*MICRO),)),
    Ecran("assistant-reponse",
          (Service("tab5_assist_reponse", (("texte", REPONSE_ASSISTANT), ("image_url", ""))),)),
    Ecran("calendrier", (Service("tab5_maj_calendrier_mois", _calendrier_juin_2026()), Long(*HORLOGE))),
    Ecran("calendrier-jour", (Long(*HORLOGE), Toucher(*CAL_JOUR_18))),
    Ecran("lumieres-chambre", (Long(*TUILES["chambre"]), roue_reglages(TUILES["chambre"], ROUE_BOUTONS["chambre"]))),
    Ecran("lumieres-salon", (Long(*TUILES["salon"]), roue_reglages(TUILES["salon"], ROUE_BOUTONS["salon"]))),
    Ecran("volet", (Long(*TUILE_VOLET), roue_reglages(TUILE_VOLET, ROUE_BOUTONS["volet"]))),
    Ecran("volet-sans-position", (VOLET_SANS_POSITION, Long(*TUILE_VOLET),
                                  roue_reglages(TUILE_VOLET, ROUE_BOUTONS["volet-sans-position"])),
          (VOLET_DE_LA_DEMO,)),
    Ecran("volet-glisse", (Long(*TUILE_VOLET), roue_reglages(TUILE_VOLET, ROUE_BOUTONS["volet"]), VOLET_TIRE)),
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
    # Historique des alertes (lot 4 du plan des alertes) : la liste d'abord, comme HA la
    # pousserait, puis l'appui long sur la carte centrale.
    Ecran("alertes", (Service("tab5_maj_alertes_historique", (("payload", HISTORIQUE_ALERTES),)),
                      Long(*CARTE_CENTRALE))),
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
    Ecran("telecommande-tv", (Long(*BOUTON_TV),)),
    Ecran("reglages", (Toucher(*BOUTON_SYS),)),
    Ecran("reglages-langue", (Toucher(*BOUTON_SYS),
                              Toucher(*REGLAGES_LANGUE_EN, selon_langue=(("en", *REGLAGES_LANGUE_FR),))),
          (Toucher(*REGLAGES_ANNULER),)),
    Ecran("console-systeme", (Long(*BOUTON_SYS),)),
    # Ligne « Batterie » de la carte SYSTÈME (discussion #278, 06/10/2026) : batterie
    # détectée (niveau, tension, icône du bandeau), en charge, puis sans batterie (« Sur
    # USB », la prise). Sans l'interrupteur, « console-systeme » montre « Non montée ».
    Ecran("console-batterie", (_batterie(True, 78.0), Long(*BOUTON_SYS)), SANS_BATTERIE),
    Ecran("console-batterie-en-charge", (_batterie(True, 60.0, True), Long(*BOUTON_SYS)), SANS_BATTERIE),
    Ecran("console-sans-batterie",
          (_batterie(True, en_charge=True, tension=TENSION_SANS_BATTERIE), Long(*BOUTON_SYS)), SANS_BATTERIE),
    Ecran("console-confirmer-redemarrage-ha", (Long(*BOUTON_SYS), Toucher(*CONSOLE_REDEMARRER_HA)),
          (Toucher(*CONFIRMATION_ANNULER),)),
    Ecran("console-confirmer-reboot", (Long(*BOUTON_SYS), Toucher(*CONSOLE_REBOOT)),
          (Toucher(*CONFIRMATION_ANNULER),)),

    # --- Arcade -----------------------------------------------------------------------
    Ecran("arcade", (Toucher(*SERRE, apres=1.0),)),

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
)

# Captures en portrait (pas de rotation en PNG).
PORTRAITS: frozenset = frozenset(e.nom for e in ECRANS if e.portrait)
# Captures qui varient d'un run à l'autre : tools/rendu/comparer.py ne les compare pas.
VARIABLES: frozenset = frozenset(e.nom for e in ECRANS if not e.stable)
