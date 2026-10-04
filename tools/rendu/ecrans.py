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
from scenarios import PAGE_DE_LA_PIECE, PIECES, SCENES, build_alerte_payload, code_pluie  # noqa: E402


@dataclass(frozen=True)
class Toucher:
    """Appui du doigt en (x, y). `duree` en ms : 1000 et plus pour un appui long."""
    x: int
    y: int
    duree: int = 150
    apres: float = 0.6


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
# Rangée du haut avec la TV : le rendu pousse une maison complète (capturer.py, aucune
# zone absente). Sans TV, HA et Sys glisseraient d'une colonne (zones_apply_ui).
# tests/test_rendu_ecrans.py les compare aux boutons de Tab5/tab5-lvgl.yaml.
BOUTON_HA, BOUTON_SYS, BOUTON_TV = (917, 65), (1061, 65), (1205, 65)
POTS = (640, 270)             # long : Plantes
SERRE = (1172, 158)           # court : Arcade
CONSIGNE_CLIM = (1061, 251)   # court : Climatisation
TUILE_J1_TEMP = (390, 684)    # court : planning de ce jour, 6 s
# Long : Lumières. Avec les pièces (ADR-0023), les lampes T2 et T3 de la pièce de
# l'accueil (tools/demo/scenarios.py) : le popup liste les lumières de la pièce.
TUILES = {"chambre": (640, 572), "salon": (890, 572)}

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


def _batterie(montee: bool, niveau: float = float("nan"), en_charge: bool = False) -> Service:
    """rendu_batterie (Tab5/rendu/bouchons.yaml) : interrupteur « Tab5 Batterie montée »,
    niveau en % et état de charge de la batterie de la tablette (icône du bandeau)."""
    return Service("rendu_batterie", (("montee", montee), ("niveau", niveau), ("en_charge", en_charge)))


# Retour à l'état par défaut (interrupteur éteint, aucune mesure) : les autres écrans
# restent sans l'icône de la batterie.
SANS_BATTERIE = (_batterie(False),)


def _solaire(pourcent: str) -> Service:
    """Production solaire du bandeau d'état, comme la pousse le blueprint (clé solaire de
    tab5_maj_emplacements, % de la puissance crête ; « nan » = aucune valeur)."""
    return Service("tab5_maj_emplacements", (("payload", f"solaire|{pourcent};"),))


# Retour sans l'icône solaire : les autres écrans restent sans elle.
SANS_SOLAIRE = (_solaire("nan"),)


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
CONSOLE_REDEMARRER_HA, CONSOLE_REBOOT = (801, 588), (1060, 588)
CONFIRMATION_ANNULER = (813, 596)   # jamais « Confirmer » (1049, 596)
# Popup Énergie (ADR-0028, energie_popup.yaml) : la tuile du capteur solaire de la démo
# (pièce « Bureau », page 1 des heures, T1 : deuxième tuile, x 275-505), la croix de
# l'en-tête (ADR-0009, commune à tous les popups) et les boutons de vue (carte du
# graphique, y 313-685 à l'écran ; boutons de 150 × 48 à 18, 178 et 338 px de son bord
# droit).
TUILE_SOLAIRE = (390, 572)
FERMER_POPUP = (1215, 41)
ENERGIE_VUES = {"heures": (828, 347), "jours": (988, 347), "mois": (1148, 347)}


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
    # Production solaire (clé solaire, % de la crête) : icône avant la batterie, couleur
    # du barème des batteries, panneau gris à 0 % (la nuit). Un écran par palier, puis
    # les deux icônes ensemble pour l'alignement.
    Ecran("accueil-solaire-nuit", (_solaire("0"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-faible", (_solaire("12"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-moyen", (_solaire("30"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-bon", (_solaire("60"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-fort", (_solaire("95"),), SANS_SOLAIRE),
    Ecran("accueil-solaire-et-batterie", (_batterie(True, 95.0), _solaire("60")), SANS_BATTERIE + SANS_SOLAIRE),

    # --- Fenêtres ---------------------------------------------------------------------
    Ecran("reveil", (Service("tab5_maj_rdv_prochains", (("payload", RDV),)), Toucher(*HORLOGE))),
    Ecran("reveil-sonnerie", (Toucher(*HORLOGE), Toucher(*REVEIL_TESTER, apres=1.5)),
          (Toucher(*SONNERIE_ARRETER),)),
    Ecran("assistant", (Long(*MICRO),)),
    Ecran("assistant-reponse",
          (Service("tab5_assist_reponse", (("texte", REPONSE_ASSISTANT), ("image_url", ""))),)),
    Ecran("calendrier", (Service("tab5_maj_calendrier_mois", _calendrier_juin_2026()), Long(*HORLOGE))),
    Ecran("calendrier-jour", (Long(*HORLOGE), Toucher(*CAL_JOUR_18))),
    Ecran("lumieres-chambre", (Long(*TUILES["chambre"]),)),
    Ecran("lumieres-salon", (Long(*TUILES["salon"]),)),
    Ecran("climatisation", (Toucher(*CONSIGNE_CLIM),)),
    Ecran("plantes", (Long(*POTS),)),
    # Énergie (ADR-0028) : ouvert par la tuile solaire de la démo (vue des heures), puis
    # les vues Jours et Mois par « Aller à l'écran ». Données : la scène (demo_pusher,
    # _pousser_energie), datée du jour figé des captures.
    Ecran("energie-heures", (VERS_LA_DROITE, Toucher(*TUILE_SOLAIRE)), (Toucher(*FERMER_POPUP), VERS_LA_GAUCHE)),
    Ecran("energie-jours", (Aller("Énergie"), Toucher(*ENERGIE_VUES["jours"]))),
    Ecran("energie-mois", (Aller("Énergie"), Toucher(*ENERGIE_VUES["mois"]))),
    Ecran("telecommande-tv", (Toucher(*BOUTON_TV),)),
    Ecran("console-systeme", (Toucher(*BOUTON_SYS),)),
    Ecran("console-confirmer-redemarrage-ha", (Toucher(*BOUTON_SYS), Toucher(*CONSOLE_REDEMARRER_HA)),
          (Toucher(*CONFIRMATION_ANNULER),)),
    Ecran("console-confirmer-reboot", (Toucher(*BOUTON_SYS), Toucher(*CONSOLE_REBOOT)),
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
)

# Captures en portrait (pas de rotation en PNG).
PORTRAITS: frozenset = frozenset(e.nom for e in ECRANS if e.portrait)
# Captures qui varient d'un run à l'autre : tools/rendu/comparer.py ne les compare pas.
VARIABLES: frozenset = frozenset(e.nom for e in ECRANS if not e.stable)
