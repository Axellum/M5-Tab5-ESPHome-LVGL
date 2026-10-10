# -*- coding: utf-8 -*-
"""tools/demo/scenarios.py — Données synthétiques et constructeurs de payload pour le mode démo Tab5.

Module pur (stdlib uniquement, aucune dépendance externe) pour rester
vérifiable sans matériel ni `aioesphomeapi` installé (cf. `demo_pusher.py --dry-run`).

Le contrat exact (nombre de champs, délimiteurs, valeurs acceptées) vient de la
lecture directe de Tab5/paquets/tab5-api-logic.yaml et Tab5/ecran/tab5_custom.cpp — voir
docs/demo_mode.md pour le détail et les sources. Ne pas modifier ces règles ici
sans revérifier contre le firmware réel.
"""
from __future__ import annotations

import datetime as _dt
import math
import re
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# Emplacements de la maison (lot 6a, ADR-0019) : la tablette ne connaît plus
# d'entité ; HA (le blueprint « Tab5 — emplacements ») lui pousse chaque
# emplacement par tab5_maj_emplacements, « clé|état|valeur;… ». La démo fait de
# même. Clés = table de tab5_maj_emplacements (Tab5/paquets/tab5-api-logic.yaml),
# vérifiées par tests/test_demo.py. Valeurs statiques pour la session : la
# variation vient des scènes météo qui tournent.
# ---------------------------------------------------------------------------

EMPLACEMENTS: dict[str, tuple[str, str]] = {
    # clé : (état tel que HA l'envoie, valeur affichée — luminosité 0-255, %, °C…)
    "lumiere_1": ("off", "nan"),
    "lumiere_2": ("on", "180"),
    "lumiere_3": ("off", "nan"),
    "pc": ("on", "nan"),
    "tv": ("off", "nan"),
    "telephone": ("68", "68"),
    "salon": ("21.4", "21.4"),
    "salon_hum": ("48", "48"),
    "serre": ("22.1", "22.1"),
    # Un pot volontairement asséché pour illustrer le tri dynamique des emplacements
    # plantes (sort_and_update_moisture_slots).
    "pot_1": ("61", "61"),
    "pot_2": ("12", "12"),
    "pot_3": ("74", "74"),
    "pot_4": ("45", "45"),
    "pot_5": ("38", "38"),
}
for _n, (_ec, _lux, _temp, _bat) in enumerate(
        [(350, 2400, 21.8, 90), (120, 800, 22.5, 35), (410, 5200, 23.1, 76),
         (280, 1500, 21.2, 18), (300, 3100, 22.0, 64)], start=1):
    EMPLACEMENTS[f"pot_{_n}_ec"] = (str(_ec), str(_ec))
    EMPLACEMENTS[f"pot_{_n}_lux"] = (str(_lux), str(_lux))
    EMPLACEMENTS[f"pot_{_n}_temp"] = (str(_temp), str(_temp))
    EMPLACEMENTS[f"pot_{_n}_bat"] = (str(_bat), str(_bat))


def zone_de(cle: str) -> str:
    """Zone d'une clé d'emplacement : pot_3_ec -> pot_3, salon_hum -> salon."""
    if cle.startswith("pot_"):
        return cle[:5]
    return "salon" if cle == "salon_hum" else cle


def build_emplacements_payload(absentes: frozenset = frozenset(), pieces: dict | None = None,
                               clim: dict | None = None, rangee: "Rangee | None" = None,
                               reglables: tuple = (), nabu: "Rangee | None" = None) -> str:
    """tab5_maj_emplacements : tous les emplacements, sauf ceux d'une zone retirée
    (`--maison-minimale` : comme le blueprint, rien n'est poussé pour une case vide).
    Avec `pieces` (firmware qui a tab5_maj_tuiles), les états des tuiles suivent (clés
    tRT, build_etats_tuiles) ; `clim` = celle de la scène, pour la tuile de la clim ;
    `rangee` : les états de la rangée sous l'horloge après eux (clés hLI, ADR-0031) ;
    `nabu` : ceux du panneau Ok Nabu (clés nLI, ADR-0041) ; `reglables` : puis ceux des
    appareils de la tuile − / + (clés rN, ADR-0033)."""
    payload = "".join(f"{cle}|{etat}|{valeur};" for cle, (etat, valeur) in EMPLACEMENTS.items()
                      if zone_de(cle) not in absentes)
    if pieces is not None:
        payload += (build_etats_tuiles(pieces, clim, rangee, nabu) + build_etats_reglables(reglables)
                    + build_climat_pieces(pieces))
    assert len(payload.encode("utf-8")) < 32 * 1024, "au-delà d'un message API ESPHome (32 Kio)"
    return payload


# ---------------------------------------------------------------------------
# Zones optionnelles (lot 5, Tab5/paquets/tab5-zones.yaml, ADR-0018) : la tablette envoie
# esphome.tab5_zones, HA répond tab5_maj_zones avec les clés des zones absentes.
# Clés = kCles de Tab5/ecran/tab5_zones.cpp (tests/test_demo.py vérifie la concordance).
# ---------------------------------------------------------------------------

# Zones suivies par la tablette, dans l'ordre de l'enum Zone.
ZONES_SUIVIES = ("lumiere_1", "lumiere_2", "lumiere_3", "pc", "tv", "telephone", "salon",
                 "serre", "pot_1", "pot_2", "pot_3", "pot_4", "pot_5")
# Zones que seul HA connaît : il les ajoute lui-même à sa réponse (« discussion » :
# aucun pipeline de discussion, boutons Domo / Discu masqués ; la maison minimale la garde).
ZONES_HA = ("clim", "volet", "planning", "discussion")

# `--maison-minimale` : la même maison que l'essai du 27/09/2026 sur la tablette de
# l'auteur (clim, TV, téléphone, LEDs, serre, pots 3 à 5 retirés), plus le volet et
# le planning. Reste : PC, deux lampes, salon, deux pots.
MAISON_MINIMALE: frozenset = frozenset({
    "lumiere_3", "tv", "telephone", "serre", "pot_3", "pot_4", "pot_5",
    "clim", "volet", "planning",
})

def build_zones_absentes(absentes: frozenset) -> str:
    """Réponse tab5_maj_zones : clés triées dans l'ordre de la tablette."""
    ordre = list(ZONES_SUIVIES) + list(ZONES_HA)
    inconnues = set(absentes) - set(ordre)
    assert not inconnues, f"clés de zone inconnues de la tablette : {sorted(inconnues)}"
    return ",".join(c for c in ordre if c in absentes)


# ---------------------------------------------------------------------------
# Pièces et tuiles (ADR-0023, firmware 3.2) : chaque page des cinq tuiles du bas est
# une pièce de cinq appareils au plus, décrits par HA. Le blueprint pousse d'abord les
# DÉFINITIONS (action tab5_maj_tuiles, instantané complet : ce qui n'est pas listé est
# vide), puis les ÉTATS dans tab5_maj_emplacements, sous des clés tRT à quatre champs.
# La démo fait de même, mais seulement si la tablette a l'action (demo_pusher.py) :
# comme le blueprint, qui n'appelle jamais une action absente (erreur que
# continue_on_error ne rattrape pas). Grammaire relue dans l'ADR par
# tests/test_demo_pieces.py.
# ---------------------------------------------------------------------------

TYPES_TUILE = ("lum", "int", "vol", "med", "act", "cap", "bin", "cli")
OPTIONS_TUILE = "dcokrtme"
# Lettres réservées à un type : variateur et couleur (lampe), TV du blueprint (média),
# clim du blueprint (clim), capteur de la section « Énergie » (capteur, ADR-0028). o, k
# et r valent pour tous.
OPTIONS_DU_TYPE = {"d": "lum", "c": "lum", "t": "med", "m": "cli", "e": "cap"}
# Le firmware garde 24 octets d'un nom (coupé entre deux caractères) ; une unité en
# fait 7 au plus (« °C », « kWh »…).
NOM_OCTETS_GARDES = 24
UNITE_OCTETS_MAX = 7
# Pièce R ↔ page de la rangée du bas, dans l'ordre où un geste les atteint depuis
# l'accueil (page 2) : 3 et 4 vers la gauche, 1 et 0 vers la droite.
PAGE_DE_LA_PIECE = {0: 2, 1: 3, 2: 4, 3: 1, 4: 0}

_CODE_ICONE = re.compile(r"[a-z0-9_]{0,15}")
_CLASSE_APPAREIL = re.compile(r"[a-z_]+")
_COULEUR = re.compile(r"[0-9A-F]{6}")
_HORS_LIGNE = ("unavailable", "unknown")
_ETATS_VOLET = ("open", "closed", "opening", "closing")


@dataclass(frozen=True)
class Tuile:
    """Un appareil d'une pièce : sa définition (tab5_maj_tuiles) et son état (clé tRT)."""
    type: str              # TYPES_TUILE
    nom: str               # texte affiché : le firmware en garde NOM_OCTETS_GARDES octets
    icone: str = ""        # code de la palette (Tab5/socle/tab5_tuiles_icones.h), '' = défaut du type
    options: str = ""      # lettres parmi OPTIONS_TUILE
    complement: str = ""   # cap : unité ; bin : classe d'appareil ; sinon ''
    etat: str = "off"      # état HA tel quel
    valeur: str = "nan"    # lum : luminosité 0-255 ; vol : position 0-100 ; cap : le nombre ;
                           # cli : température de la pièce ; sinon nan
    couleur: str = ""      # lum allumée qui donne sa rgb_color : « RRGGBB »


@dataclass(frozen=True)
class ClimatPiece:
    """Température, humidité et clim d'une pièce (ADR-0040, champs « Température de la
    pièce », « Humidité de la pièce » et « Climatisation de la pièce » du blueprint). En
    mode HA sur la pièce, la tablette les montre à la place du salon et de la serre, et
    la tuile − / + règle la clim. Poussés dans tab5_maj_emplacements comme le blueprint :
    « crpR|réglages; », « pR|température|humidité|clim; », « cepR|état; »."""
    temperature: str        # la mesure (« nan » : inconnue)
    humidite: str = ""      # '' : non déclarée (rien à droite)
    reglages: str = ""      # clim : « min|max|pas|unité|capacités|nom » (ceux de crRT) ; '' : aucune
    etat: str = ""          # clim : « consigne|pièce|mode|préréglage|ventilation|oscillation » (ceRT)


@dataclass(frozen=True)
class Piece:
    nom: str      # '' : pas d'entrée « p », la tablette écrit « Pièce n » dans sa langue
    tuiles: dict  # position T (0 = gauche … 4 = droite, sur toutes les pages) -> Tuile
    climat: ClimatPiece | None = None  # ADR-0040 ; None : l'écran d'avant en mode HA


# La maison de la démo : cinq pièces, tous les types et toutes les options, un nom que
# la tablette coupe, des accents, un appareil hors ligne, une pièce de deux tuiles
# (recentrées en mode HA) et une de trois tuiles espacées. États cohérents avec les
# emplacements 3.x ci-dessus : TV éteinte (« tv »), PC allumé (« pc »).
PIECES: dict = {
    # Accueil (jours 0-4). Les deux lampes en T2 et T3 : l'appui long des captures
    # « lumieres-chambre » et « lumieres-salon » (tools/rendu/ecrans.py) y ouvre le
    # popup des lumières de la pièce.
    0: Piece("Salon", {
        0: Tuile("med", "Télévision", "tv", "t"),
        1: Tuile("vol", "Volet du salon", "volet", etat="opening", valeur="45"),
        2: Tuile("lum", "Lampe d'ambiance", "canape", "dc", etat="on", valeur="180", couleur="FF8C1A"),
        3: Tuile("lum", "Plafonnier", "plafonnier", "d"),
        # Une scène a pour état l'heure de sa dernière activation.
        4: Tuile("act", "Soirée cinéma", "scene", etat="2026-06-15T20:45:00+00:00"),
    }),
    # Jours 5-9. Une température déclarée, sans humidité ni clim (ADR-0040).
    1: Piece("Entrée", climat=ClimatPiece("19.6"), tuiles={
        0: Tuile("bin", "Porte d'entrée", "porte", complement="door"),
        1: Tuile("bin", "Mouvement du couloir", "mouvement", complement="motion", etat="on"),
        # Lecture seule : c'est le détecteur qui l'allume.
        2: Tuile("lum", "Applique", "applique", "r", etat="on"),
        # Script à confirmer : un second appui dans les 3 s l'envoie.
        3: Tuile("act", "Je pars", options="k"),
        4: Tuile("int", "Prise du portail", "prise", etat="unavailable"),
    }),
    # Jours 10-14. Nom de 26 octets (la tablette en garde 24) ; trois tuiles espacées,
    # côte à côte et centrées en mode HA.
    2: Piece("Chambre d'amis à l'étage", {
        0: Tuile("lum", "Chevet", "lit", "d", etat="on", valeur="90"),
        # La clim du blueprint : état et température suivent tab5_maj_clim (etat_tuile).
        2: Tuile("cli", "Climatisation", "clim", "m", etat="cool", valeur="23.5"),
        4: Tuile("bin", "Présence", "presence", complement="presence"),
    }),
    # Heures 0-4. Deux tuiles à gauche : recentrées en mode HA. Température, humidité et
    # clim de la pièce (ADR-0040) : un climatiseur qui n'est sur aucune tuile, réglé par
    # la tuile − / + en mode HA.
    3: Piece("Bureau", climat=ClimatPiece(
        "22.8", "45", "16.0|30.0|0.5|°C|chdfq|Climatiseur du bureau", "22.0|22.8|cool|none|auto|stop"), tuiles={
        # Jamais éteint depuis l'écran (réveil par le réseau).
        0: Tuile("int", "Ordinateur", "ordinateur", "o", etat="on"),
        # Le capteur solaire de la section « Énergie » (option e, ADR-0028) : son appui
        # ouvre le popup Énergie. Même valeur que l'instantané (ENERGIE_INSTANTANE).
        1: Tuile("cap", "Solaire", "solaire", "e", complement="W", etat="1450", valeur="1450"),
    }),
    # Heures 5-9.
    4: Piece("Jardin", {
        0: Tuile("vol", "Store de la terrasse", "store", etat="open", valeur="60"),
        1: Tuile("cap", "Humidité du sol", "plante", complement="%", etat="34", valeur="34"),
        2: Tuile("med", "Enceinte", "enceinte", etat="playing"),
        # 36 octets, coupé ; indigo, trop sombre pour le fond : la tablette l'éclaircit.
        3: Tuile("lum", "Guirlande lumineuse de la terrasse", "guirlande", "dc", etat="on", valeur="255",
                 couleur="4B0082"),
        # 24 octets tout juste : gardé en entier.
        4: Tuile("cap", "Température extérieure", "thermometre", complement="°C", etat="17.8", valeur="17.8"),
    }),
}

# `--maison-minimale` : ses appareils (le PC et deux lampes) dans une seule pièce, sans
# nom (« Pièce 1 » à l'écran), aux places qu'ils ont en 3.x. Glisser en mode HA ne
# change rien : il n'y a pas d'autre pièce.
PIECES_MINIMALES: dict = {
    0: Piece("", {
        0: Tuile("int", "PC", "ordinateur", etat="on"),
        2: Tuile("lum", "Chambre", "lit", "d"),
        3: Tuile("lum", "Salon", "canape", "d", etat="on", valeur="180"),
    }),
}


def pieces_de(absentes: frozenset) -> dict:
    """Les pièces poussées : la maison minimale (des zones retirées) a les siennes."""
    return PIECES_MINIMALES if absentes else PIECES


def echapper(texte: str) -> str:
    """Champ texte d'un payload : « | » devient « / » et « ; » devient « , », comme le
    blueprint (tab5_emplacements.yaml) et le contrat (ADR-0023)."""
    return texte.replace("|", "/").replace(";", ",")


def _nombre(texte: str) -> float | None:
    try:
        v = float(texte)
    except ValueError:
        return None
    return None if v != v else v  # « nan » n'est pas un nombre


def verifier_definition(cle: str, tuile: Tuile) -> None:
    """Assertions du contrat (ADR-0023) sur la définition d'une tuile."""
    assert tuile.type in TYPES_TUILE, f"{cle} : type inconnu {tuile.type!r}"
    assert _CODE_ICONE.fullmatch(tuile.icone), f"{cle} : code d'icône hors palette {tuile.icone!r}"
    assert set(tuile.options) <= set(OPTIONS_TUILE), f"{cle} : option inconnue dans {tuile.options!r}"
    assert len(set(tuile.options)) == len(tuile.options), f"{cle} : option répétée"
    for lettre, type_ in OPTIONS_DU_TYPE.items():
        assert lettre not in tuile.options or tuile.type == type_, f"{cle} : option {lettre} hors {type_}"
    if tuile.type == "cap":
        assert 0 < len(tuile.complement.encode("utf-8")) <= UNITE_OCTETS_MAX, f"{cle} : unité"
    elif tuile.type == "bin":
        assert _CLASSE_APPAREIL.fullmatch(tuile.complement), f"{cle} : classe d'appareil"
    else:
        assert tuile.complement == "", f"{cle} : complément réservé à cap et bin"
    assert tuile.nom.strip(), f"{cle} : nom vide"


def etat_tuile(tuile: Tuile, clim: dict | None = None) -> tuple:
    """(état, valeur, couleur) poussés pour une tuile. La clim du blueprint (option m)
    est celle de tab5_maj_clim : la tuile et la carte clim disent la même chose."""
    if tuile.type == "cli" and "m" in tuile.options and clim:
        return clim["mode"], clim["current"], ""
    return tuile.etat, tuile.valeur, tuile.couleur


def verifier_etat(cle: str, tuile: Tuile, etat: str, valeur: str, couleur: str) -> None:
    """Assertions du contrat (ADR-0023) sur l'état d'une tuile, selon son type."""
    assert etat, f"{cle} : état vide"
    v = _nombre(valeur)
    assert v is not None or valeur == "nan", f"{cle} : valeur {valeur!r} ni nombre ni nan"
    assert couleur == "" or (tuile.type == "lum" and etat == "on" and "c" in tuile.options
                             and _COULEUR.fullmatch(couleur)), f"{cle} : couleur {couleur!r}"
    if etat in _HORS_LIGNE:
        assert valeur == "nan" and couleur == "", f"{cle} : hors ligne sans valeur"
    elif tuile.type == "lum":
        if etat == "on" and "d" in tuile.options:
            assert v is not None and 0 <= v <= 255 and v == int(v), f"{cle} : luminosité 0-255"
        else:
            assert valeur == "nan", f"{cle} : luminosité sans variateur allumé"
    elif tuile.type == "vol":
        assert etat in _ETATS_VOLET, f"{cle} : état de volet {etat!r}"
        assert valeur == "nan" or 0 <= v <= 100, f"{cle} : position 0-100"
    elif tuile.type == "cap":
        assert v is not None, f"{cle} : un capteur a une valeur"
    elif tuile.type != "cli":
        assert valeur == "nan", f"{cle} : pas de valeur pour {tuile.type}"


def _tuiles_de(pieces: dict):
    """(clé « tRT », tuile) vérifiée, pièce par pièce dans l'ordre de R, tuiles dans
    l'ordre de T."""
    for r, piece in sorted(pieces.items()):
        assert r in PAGE_DE_LA_PIECE, f"pièce {r} : R va de 0 à 4"
        assert piece.tuiles, f"pièce {r} sans appareil : ne pas la déclarer"
        for t, tuile in sorted(piece.tuiles.items()):
            assert 0 <= t <= 4, f"pièce {r} : T va de 0 à 4"
            verifier_definition(f"t{r}{t}", tuile)
            yield f"t{r}{t}", tuile


def build_tuiles_payload(pieces: dict, rangee: "Rangee | None" = None, reglables: tuple = (),
                         nabu: "Rangee | None" = None) -> str:
    """tab5_maj_tuiles : « pR|nom;tRT|type|icône|options|complément|nom;… » (ADR-0023).
    Instantané complet : une tuile ou une pièce absente est vide. Une pièce sans nom n'a
    pas d'entrée « p » (la tablette écrit « Pièce n »). `rangee` : la rangée sous
    l'horloge à la suite (build_rangee_payload, ADR-0031) ; `nabu` : le panneau Ok Nabu,
    au même format (clés n…, ADR-0041) ; `reglables` : les appareils de la tuile − / +
    (build_reglables_payload, ADR-0033)."""
    entrees = []
    for r, piece in sorted(pieces.items()):
        if piece.nom:
            entrees.append(f"p{r}|{echapper(piece.nom)}")
        for cle, tuile in _tuiles_de({r: piece}):
            entrees.append("|".join((cle, tuile.type, tuile.icone, tuile.options,
                                     echapper(tuile.complement), echapper(tuile.nom))))
    payload = "".join(f"{e};" for e in entrees) + (build_rangee_payload(rangee) if rangee is not None else "")
    payload += build_rangee_payload(nabu) if nabu is not None else ""
    payload += build_reglables_payload(reglables)
    assert len(payload.encode("utf-8")) < 32 * 1024, "au-delà d'un message API ESPHome (32 Kio)"
    return payload


def build_climat_pieces(pieces: dict) -> str:
    """Climat des cinq pièces (ADR-0040), comme la dernière poussée du blueprint après
    une connexion : pièce par pièce, « crpR|réglages; » (s'il y a une clim), « pR|
    température|humidité|clim; » (champs vides : rien de déclaré, la tablette oublie la
    pièce), « cepR|état; ». Une pièce absente ou sans climat : « pR|||0; »."""
    parts = []
    for r in range(len(PAGE_DE_LA_PIECE)):
        piece = pieces.get(r)
        c = piece.climat if piece is not None else None
        if c is None:
            parts.append(f"p{r}|||0;")
            continue
        assert _nombre(c.temperature) is not None or c.temperature == "nan", f"p{r} : température"
        assert c.humidite == "" or _nombre(c.humidite) is not None, f"p{r} : humidité"
        assert bool(c.reglages) == bool(c.etat), f"p{r} : une clim a ses réglages et son état"
        if c.reglages:
            assert len(c.reglages.split("|")) == 6 and len(c.etat.split("|")) == 6, f"p{r} : champs de la clim"
            parts.append(f"crp{r}|{c.reglages};")
        parts.append(f"p{r}|{c.temperature}|{c.humidite}|{'1' if c.reglages else '0'};")
        if c.etat:
            parts.append(f"cep{r}|{c.etat};")
    return "".join(parts)


def build_etats_tuiles(pieces: dict, clim: dict | None = None, rangee: "Rangee | None" = None,
                       nabu: "Rangee | None" = None) -> str:
    """Clés tRT de tab5_maj_emplacements : « tRT|état|valeur|couleur;… », toutes les
    tuiles, comme le blueprint juste après les définitions. `clim` : voir etat_tuile.
    `rangee` : puis les éléments de la rangée sous l'horloge (clés hLI, mêmes champs) ;
    `nabu` : puis ceux du panneau Ok Nabu (clés nLI)."""
    parts = []
    for cle, tuile in _tuiles_de(pieces):
        etat, valeur, couleur = etat_tuile(tuile, clim)
        verifier_etat(cle, tuile, etat, valeur, couleur)
        parts.append(f"{cle}|{echapper(etat)}|{valeur}|{couleur};")
    for zone in (rangee, nabu):
        for cle, element in _elements_de(zone) if zone is not None else ():
            t = element.tuile
            verifier_etat(cle, t, t.etat, t.valeur, t.couleur)
            parts.append(f"{cle}|{echapper(t.etat)}|{t.valeur}|{t.couleur};")
    return "".join(parts)


# ---------------------------------------------------------------------------
# Rangée sous l'horloge (ADR-0031) : jusqu'à trois lignes de quatre éléments, plus la
# ligne des plantes, qui tournent avec la carte centrale. Le blueprint pousse, après les
# pièces dans tab5_maj_tuiles, « hp|place des plantes;hd|secondes d'une ligne; » puis
# « hLI|type|icône|options|complément|nom|classe; » (élément I de la ligne L : les six
# champs d'une tuile, plus la classe d'appareil, qui colore la valeur) ; les états
# « hLI|état|valeur|couleur; » suivent ceux des tuiles. Pas d'action (les scènes,
# scripts et boutons n'ont rien à montrer) : la rangée ne commande rien.
# Le panneau « Ok Nabu » (lot 3, ADR-0041) a le même format, clés n… : « np|place de la
# ligne d'écoute;nd|secondes;nLI|…; » (Rangee(lettre="n")).
# ---------------------------------------------------------------------------

PLACES_PLANTES = ("0", "1", "2", "-")
LETTRES_ZONE = ("h", "n")   # h : rangée sous l'horloge ; n : panneau Ok Nabu
ELEMENTS_PAR_LIGNE = 4
LIGNES_MAX = 3


@dataclass(frozen=True)
class Element:
    """Un élément de la rangée : une tuile (définition et état) et sa classe d'appareil."""
    tuile: Tuile
    classe: str = ""   # device_class : temperature, humidity, battery, power… ('' : aucune)


@dataclass(frozen=True)
class Rangee:
    lignes: tuple          # LIGNES_MAX lignes au plus, de ELEMENTS_PAR_LIGNE éléments au plus
    plantes: str = "0"     # place de la ligne spéciale : les plantes sous l'horloge, l'écoute
                           # dans le panneau Ok Nabu (PLACES_PLANTES ; « - » : masquée)
    duree: int = 32        # secondes d'une ligne (arrondies aux tours de 8 s de la carte centrale)
    lettre: str = "h"      # LETTRES_ZONE : la zone, première lettre de ses clés


# La rangée de la démo : les plantes d'abord (les pots de EMPLACEMENTS), puis une ligne
# « climat » (quatre valeurs sur les échelles de température et d'humidité de l'écran)
# et une ligne « énergie et maison » (production en or, batterie sur son échelle, mêmes
# valeurs que l'instantané du popup Énergie, ENERGIE_INSTANTANE ; une
# présence et une prise en icônes). Trois lignes : le rendu les capture une à une
# (tools/rendu/ecrans.py, « accueil-rangee-ligne-2 » et « -3 »).
RANGEE = Rangee((
    (Element(Tuile("cap", "Salon", "thermometre", complement="°C", etat="21.4", valeur="21.4"), "temperature"),
     Element(Tuile("cap", "Chambre", "thermometre", complement="°C", etat="19.8", valeur="19.8"), "temperature"),
     Element(Tuile("cap", "Humidité du salon", "humidite", complement="%", etat="58", valeur="58"), "humidity"),
     Element(Tuile("cap", "Extérieur", "thermometre", complement="°C", etat="17.8", valeur="17.8"), "temperature")),
    (Element(Tuile("cap", "Solaire", "solaire", "e", complement="W", etat="1450", valeur="1450"), "power"),
     Element(Tuile("cap", "Batterie de la maison", "batterie", complement="%", etat="64", valeur="64"), "battery"),
     Element(Tuile("bin", "Présence", "presence", complement="presence", etat="on")),
     Element(Tuile("int", "Prise du bureau", "prise", etat="on"))),
))
# `--maison-minimale` : aucune ligne choisie, les réglages par défaut (plantes seules).
RANGEE_MINIMALE = Rangee(())

# Panneau Ok Nabu (ADR-0041) : celui du blueprint sans ligne choisie, l'écoute seule
# (« np|0;nd|32; ») ; la démo le garde, l'accueil reste celui des références du rendu.
NABU = Rangee((), lettre="n")
# Avec deux lignes de capteurs (l'écoute d'abord) : capturé par le rendu, lignes 1 à 3
# (tools/rendu/ecrans.py, « accueil-nabu-ligne-1 » à « -3 »). Ligne « air » (CO2, humidité,
# cave, fumée) et ligne « ouvertures » (porte, fenêtre, serrure, garage).
NABU_TROIS_LIGNES = Rangee((
    (Element(Tuile("cap", "CO2 du bureau", "co2", complement="ppm", etat="612", valeur="612"), "carbon_dioxide"),
     Element(Tuile("cap", "Humidité de la chambre", "humidite", complement="%", etat="47", valeur="47"), "humidity"),
     Element(Tuile("cap", "Cave", "thermometre", complement="°C", etat="14.2", valeur="14.2"), "temperature"),
     Element(Tuile("bin", "Fumée", "fumee", complement="smoke", etat="off"))),
    (Element(Tuile("bin", "Porte d'entrée", "porte", complement="door", etat="off")),
     Element(Tuile("bin", "Fenêtre de la chambre", "fenetre", complement="window", etat="on")),
     Element(Tuile("bin", "Serrure", "serrure", complement="lock", etat="locked")),
     Element(Tuile("bin", "Garage", "garage", complement="garage_door", etat="off"))),
), lettre="n")
# Une ligne de capteurs seule, l'écoute masquée : le panneau sans pastilles.
NABU_UNE_LIGNE = Rangee(NABU_TROIS_LIGNES.lignes[:1], plantes="-", lettre="n")


def rangee_de(absentes: frozenset) -> Rangee:
    """La rangée poussée : la maison minimale (des zones retirées) n'en a pas."""
    return RANGEE_MINIMALE if absentes else RANGEE


def nabu_de(absentes: frozenset) -> Rangee:
    """Le panneau Ok Nabu poussé : celui d'un blueprint sans ligne, dans les deux maisons."""
    return NABU


def _elements_de(rangee: Rangee):
    """(clé « hLI » ou « nLI », élément) vérifié, ligne par ligne, comme une tuile plus sa
    classe."""
    assert rangee.lettre in LETTRES_ZONE, f"zone {rangee.lettre!r}"
    assert rangee.plantes in PLACES_PLANTES, f"place de la ligne spéciale {rangee.plantes!r}"
    assert 1 <= rangee.duree <= 999, f"durée d'une ligne {rangee.duree}"
    assert len(rangee.lignes) <= LIGNES_MAX, "trois lignes au plus"
    for l, ligne in enumerate(rangee.lignes):
        assert 0 < len(ligne) <= ELEMENTS_PAR_LIGNE, f"ligne {l} : un à quatre éléments"
        for i, element in enumerate(ligne):
            cle = f"{rangee.lettre}{l}{i}"
            verifier_definition(cle, element.tuile)
            assert element.tuile.type != "act", f"{cle} : une action n'a rien à montrer"
            assert _CODE_ICONE.fullmatch(element.classe), f"{cle} : classe d'appareil {element.classe!r}"
            yield cle, element


def build_rangee_payload(rangee: Rangee) -> str:
    """Définitions de la rangée dans tab5_maj_tuiles : « hp|place;hd|secondes;
    hLI|type|icône|options|complément|nom|classe;… » (ADR-0031) ; « np|…;nd|…;nLI|…; »
    pour le panneau Ok Nabu (ADR-0041)."""
    z = rangee.lettre
    entrees = [f"{z}p|{rangee.plantes}", f"{z}d|{rangee.duree}"]
    for cle, element in _elements_de(rangee):
        t = element.tuile
        entrees.append("|".join((cle, t.type, t.icone, t.options, echapper(t.complement), echapper(t.nom),
                                 element.classe)))
    return "".join(f"{e};" for e in entrees)


# ---------------------------------------------------------------------------
# Tuile − / + (ADR-0033) : les − / + de la carte clim règlent l'appareil choisi sur la
# tablette dans une liste (la clim du blueprint, ces appareils, le volume de la
# tablette). Le blueprint pousse, après la rangée dans tab5_maj_tuiles,
# « rN|type|icône|options|lien|min|max|pas|unité|nom; » (N de 0 à 7 ; option t = la TV
# du blueprint ; lien = la tuile tRT qui porte la même entité) ; les états
# « rN|état|valeur; » suivent ceux de la rangée.
# ---------------------------------------------------------------------------

TYPES_REGLABLE = ("son", "lum", "cli", "eau", "hum", "ven", "vol", "nbr")
REGLABLES_MAX = 8


@dataclass(frozen=True)
class Reglable:
    """Un appareil de la tuile − / + : sa définition et son état."""
    type: str                                 # TYPES_REGLABLE
    nom: str
    icone: str = ""                           # code de la palette, '' = défaut du type
    options: str = ""                         # t : la TV du blueprint
    lien: str = ""                            # tRT de la tuile de la même entité, '' sinon
    bornes: tuple = ("0", "100", "5", "%")    # min, max, pas, unité
    etat: str = "on"                          # état HA tel quel
    valeur: str = "nan"                       # dans l'unité des bornes, nan sinon


# Les appareils de la démo : la TV du salon (éteinte, sans volume : « -- »), la lampe
# d'ambiance et l'enceinte du jardin (leurs tuiles t02 et t42 : la valeur ouvre leur
# popup), un radiateur sans tuile. Le rendu capture la liste et l'enceinte choisie
# (tools/rendu/ecrans.py, « accueil-tuile-liste » et « accueil-tuile-enceinte »).
REGLABLES = (
    Reglable("son", "Télévision", "tv", "t", "t00", etat="off"),
    Reglable("lum", "Lampe d'ambiance", "canape", lien="t02", bornes=("0", "100", "10", "%"), valeur="71"),
    Reglable("cli", "Radiateur de la chambre", "radiateur", bornes=("7", "30", "0.5", "°C"), etat="heat",
             valeur="19.5"),
    Reglable("son", "Enceinte", "enceinte", lien="t42", etat="playing", valeur="35"),
)


def reglables_de(absentes: frozenset) -> tuple:
    """Les appareils poussés : la maison minimale (des zones retirées) n'en a pas."""
    return () if absentes else REGLABLES


def _reglables_de(reglables: tuple):
    """(clé « rN », appareil) vérifié, dans l'ordre de la liste."""
    assert len(reglables) <= REGLABLES_MAX, "huit au plus"
    for n, r in enumerate(reglables):
        cle = f"r{n}"
        assert r.type in TYPES_REGLABLE, f"{cle} : type inconnu {r.type!r}"
        assert _CODE_ICONE.fullmatch(r.icone), f"{cle} : code d'icône hors palette {r.icone!r}"
        assert r.options in ("", "t") and (r.options != "t" or r.type == "son"), f"{cle} : option {r.options!r}"
        assert r.lien == "" or (len(r.lien) == 3 and r.lien[0] == "t" and r.lien[1] in "01234"
                                and r.lien[2] in "01234"), f"{cle} : lien {r.lien!r}"
        mn, mx, pas, unite = r.bornes
        assert float(mn) < float(mx) and float(pas) > 0, f"{cle} : bornes {r.bornes}"
        assert len(unite.encode("utf-8")) <= 7, f"{cle} : unité de 7 octets au plus"
        assert r.etat and (_nombre(r.valeur) is not None or r.valeur == "nan"), f"{cle} : état"
        yield cle, r


def build_reglables_payload(reglables: tuple) -> str:
    """Définitions de la tuile − / + dans tab5_maj_tuiles (ADR-0033)."""
    return "".join("|".join((cle, r.type, r.icone, r.options, r.lien, *r.bornes[:3], echapper(r.bornes[3]),
                             echapper(r.nom))) + ";" for cle, r in _reglables_de(reglables))


def build_etats_reglables(reglables: tuple) -> str:
    """États de la tuile − / + dans tab5_maj_emplacements : « rN|état|valeur; »."""
    return "".join(f"{cle}|{echapper(r.etat)}|{r.valeur};" for cle, r in _reglables_de(reglables))


def nom_de_la_piece(r: int, piece: Piece) -> str:
    return piece.nom or f"Pièce {r + 1}"


def decrire_emplacement(cle: str, pieces: dict) -> str:
    """Pour le journal des commandes de l'écran : « t02 (Salon › Lampe d'ambiance, lum) »."""
    m = re.fullmatch(r"t([0-4])([0-4])", cle)
    if m:
        r, t = int(m.group(1)), int(m.group(2))
        piece = pieces.get(r)
        tuile = piece.tuiles.get(t) if piece else None
        if tuile is None:
            return f"{cle} (tuile vide : absente des définitions poussées)"
        return f"{cle} ({nom_de_la_piece(r, piece)} › {tuile.nom}, {tuile.type})"
    m = re.fullmatch(r"p([0-4])", cle)
    if m:
        r = int(m.group(1))
        piece = pieces.get(r)
        if piece is None:
            return f"{cle} (pièce vide : absente des définitions poussées)"
        return f"{cle} ({nom_de_la_piece(r, piece)}, toutes ses lumières)"
    return cle


# ---------------------------------------------------------------------------
# Énergie (ADR-0028, discussion #278) : une maison solaire inventée, avec batterie, au
# matin du 16 juin (l'heure figée des captures : 07:45). Ce que pousserait
# script.tab5_energie (HomeAssistant_Config/packages/tab5_energie.yaml) :
#   - tab5_maj_energie : « solaire|maison|reseau|batterie|batterie_puissance|
#     batterie_temperature|unite_temperature|jour » — W entiers, réseau + achat / − vente,
#     batterie + charge / − décharge, jour en kWh ; vide = non choisi, nan = sans valeur ;
#   - tab5_maj_energie_historique : vue, debut (AAAA-MM-JJ), valeurs en kWh « ; ».
# Cohérent : solaire 1450 = maison 620 + charge 400 + vente 430.
# ---------------------------------------------------------------------------

ENERGIE_CHAMPS = ("solaire", "maison", "reseau", "batterie", "batterie_puissance",
                  "batterie_temperature", "unite_temperature", "jour")
ENERGIE_VUES = {"heures": 24, "jours": 30, "mois": 12}
ENERGIE_INSTANTANE = {
    "solaire": "1450", "maison": "620", "reseau": "-430", "batterie": "64",
    "batterie_puissance": "400", "batterie_temperature": "21.5", "unite_temperature": "°C",
    "jour": "1.47",
}
# Production de chaque heure du jour (heures à venir vides), des 30 derniers jours et
# des 12 derniers mois, en kWh. Le dernier créneau est le jour, le mois ou l'heure en
# cours ; le jour en cours = ENERGIE_INSTANTANE["jour"].
ENERGIE_HISTORIQUE = {
    "heures": ["0"] * 6 + ["0.53", "0.94"] + [""] * 16,
    "jours": ["24.8", "27.1", "18.4", "9.6", "21.3", "29.4", "31.2", "30.6", "26.9", "14.2",
              "12.7", "22.5", "28.8", "32.1", "33.4", "31.9", "25.6", "19.3", "27.7", "30.2",
              "33.8", "34.1", "29.5", "16.8", "23.9", "30.7", "32.6", "28.4", "31.5", "1.47"],
    "mois": ["821", "742", "563", "381", "192", "118", "151", "263", "472", "641", "758", "412"],
}


def _nombre_ou_vide(champ: str, v: str) -> None:
    assert v == "" or v == "nan" or _nombre(v) is not None, f"{champ} : {v!r} n'est pas un nombre"


def build_energie_payload(instantane: dict | None = None) -> str:
    """tab5_maj_energie : les huit champs dans l'ordre du contrat, séparés par « | »."""
    e = ENERGIE_INSTANTANE if instantane is None else instantane
    assert set(e) <= set(ENERGIE_CHAMPS), f"champ inconnu : {set(e) - set(ENERGIE_CHAMPS)}"
    valeurs = [str(e.get(c, "")) for c in ENERGIE_CHAMPS]
    for champ, v in zip(ENERGIE_CHAMPS, valeurs):
        assert "|" not in v and ";" not in v, f"{champ} : séparateur dans {v!r}"
        if champ != "unite_temperature":
            _nombre_ou_vide(champ, v)
    for champ in ("solaire", "maison", "reseau", "batterie_puissance"):
        assert valeurs[ENERGIE_CHAMPS.index(champ)] in ("", "nan") or             float(valeurs[ENERGIE_CHAMPS.index(champ)]).is_integer(), f"{champ} : W entiers"
    return "|".join(valeurs)


def debut_energie(vue: str, aujourd_hui: _dt.date) -> _dt.date:
    """Premier créneau d'une vue : le jour même (heures), 29 jours avant (jours), le 1er du
    mois 11 mois avant (mois) — comme packages/tab5_energie.yaml."""
    if vue == "heures":
        return aujourd_hui
    if vue == "jours":
        return aujourd_hui - _dt.timedelta(days=29)
    m = aujourd_hui.month - 11
    return _dt.date(aujourd_hui.year - (1 if m < 1 else 0), m + (12 if m < 1 else 0), 1)


def build_energie_historique(vue: str, aujourd_hui: _dt.date) -> dict:
    """Variables de tab5_maj_energie_historique pour une vue, datée de `aujourd_hui`."""
    assert vue in ENERGIE_VUES, vue
    valeurs = ENERGIE_HISTORIQUE[vue]
    assert len(valeurs) == ENERGIE_VUES[vue], f"{vue} : {len(valeurs)} valeurs"
    for v in valeurs:
        _nombre_ou_vide(vue, v)
        assert v in ("", "nan") or float(v) >= 0, f"{vue} : production négative {v!r}"
    return {"vue": vue, "debut": debut_energie(vue, aujourd_hui).isoformat(), "valeurs": ";".join(valeurs)}


# Page « Aujourd'hui » du popup Énergie (ADR-0058) : tab5_maj_energie_soleil, ce que pousserait
# script.tab5_energie. « lever|midi|coucher|prevu_jour|prevu_demain|source|creneau_debut|
# creneau_fin|prevu|clair » — HH:MM locaux, kWh, source a (courbe apprise × météo) ou e
# (prévision externe), créneau en heures entières 0-24 (fin exclue), prevu et clair = 24
# valeurs en kWh séparées par « ; » (vide = pas de donnée). La courbe « ciel clair » est en
# cloche entre le lever et le coucher ; la prévision en est une part (nuages l'après-midi).
ENERGIE_SOLEIL_CHAMPS = ("lever", "midi", "coucher", "prevu_jour", "prevu_demain", "source",
                         "creneau_debut", "creneau_fin", "prevu", "clair")
ENERGIE_SOLEIL = {
    "lever": "08:13", "midi": "13:52", "coucher": "19:31",
    "prevu_jour": "17.8", "prevu_demain": "12.1", "source": "a",
    "creneau_debut": "12", "creneau_fin": "15",
    "prevu": ["0"] * 8 + ["0.12", "0.79", "1.53", "2.2", "2.68", "2.93", "2.38", "2.12", "1.67",
                          "0.94", "0.42"] + ["0"] * 5,
    "clair": ["0"] * 8 + ["0.15", "0.99", "1.91", "2.75", "3.35", "3.66", "3.61", "3.22", "2.54",
                          "1.67", "0.75"] + ["0"] * 5,
}


def _heure_minutes(champ: str, v: str) -> int:
    m = re.fullmatch(r"([01]\d|2[0-3]):([0-5]\d)", v)
    assert m, f"{champ} : {v!r} n'est pas HH:MM"
    return int(m.group(1)) * 60 + int(m.group(2))


def build_energie_soleil(soleil: dict | None = None) -> str:
    """tab5_maj_energie_soleil : les dix champs dans l'ordre du contrat, séparés par « | »,
    prevu et clair par « ; »."""
    e = ENERGIE_SOLEIL if soleil is None else soleil
    assert set(e) <= set(ENERGIE_SOLEIL_CHAMPS), f"champ inconnu : {set(e) - set(ENERGIE_SOLEIL_CHAMPS)}"
    champs = []
    for c in ENERGIE_SOLEIL_CHAMPS:
        v = e.get(c, "")
        champs.append(";".join(v) if isinstance(v, (list, tuple)) else str(v))
    d = dict(zip(ENERGIE_SOLEIL_CHAMPS, champs))
    for c, v in d.items():
        assert "|" not in v, f"{c} : séparateur dans {v!r}"
        if c not in ("prevu", "clair"):
            assert ";" not in v, f"{c} : séparateur dans {v!r}"
    horaires = [_heure_minutes(c, d[c]) for c in ("lever", "midi", "coucher") if d[c]]
    assert len(horaires) in (0, 3) and horaires == sorted(horaires), "lever < midi < coucher, ou aucun"
    for c in ("prevu_jour", "prevu_demain"):
        _nombre_ou_vide(c, d[c])
    assert d["source"] in ("", "a", "e"), f"source : {d['source']!r}"
    debut, fin = d["creneau_debut"], d["creneau_fin"]
    assert (debut == "") == (fin == ""), "créneau : début et fin ensemble"
    if debut:
        assert debut.isdigit() and fin.isdigit() and 0 <= int(debut) < int(fin) <= 24, f"créneau {debut}-{fin}"
    series = {}
    for c in ("prevu", "clair"):
        valeurs = d[c].split(";") if d[c] else []
        assert len(valeurs) in (0, 24), f"{c} : {len(valeurs)} valeurs au lieu de 24"
        for v in valeurs:
            assert _nombre(v) is not None and float(v) >= 0, f"{c} : {v!r}"
        series[c] = [float(v) for v in valeurs]
    if series["prevu"] and d["prevu_jour"] not in ("", "nan"):
        assert abs(sum(series["prevu"]) - float(d["prevu_jour"])) <= 0.2, "prevu_jour ≠ somme de prevu"
    if series["prevu"] and debut:
        meilleur = max(range(0, 22), key=lambda a: sum(series["prevu"][a:a + 3]))
        assert int(debut) == meilleur and int(fin) == meilleur + 3, "créneau ≠ les 3 heures de plus forte prévision"
    return "|".join(champs)


# Page « Bilan » (ADR-0058) : tab5_maj_energie_bilan(vue, debut, payload), payload
# « devise|vente|achat|gain ». vente et achat = kWh exportés et importés par créneau
# (24, 30 ou 12 valeurs « ; », vides pour les créneaux à venir) ; gain = argent économisé
# par créneau, en `devise` : consommé sur place × prix d'achat + exporté × prix de revente
# (calculé par HA). La production du créneau est celle de ENERGIE_HISTORIQUE ; le consommé
# sur place = production − vente. Achat des heures inventé pour une maison qui tire la nuit
# et peu le matin (la batterie est à 64 %) ; jours et mois dérivés de la production.
ENERGIE_DEVISE = "€"
ENERGIE_PRIX_ACHAT, ENERGIE_PRIX_REVENTE = 0.2516, 0.13   # € par kWh
ENERGIE_BILAN_HEURES = {
    "vente": ["0"] * 7 + ["0.31"] + [""] * 16,
    "achat": ["0.22", "0.19", "0.18", "0.18", "0.21", "0.28", "0.12", "0.05"] + [""] * 16,
}
# Part de la production exportée (cycle de 7 jours, plus de soleil que de besoin le week-end)
# et consommation de la maison (kWh) par jour de la semaine et par mois (juillet → juin).
ENERGIE_PART_VENTE_JOURS = (0.34, 0.41, 0.38, 0.29, 0.44, 0.47, 0.40)
ENERGIE_PART_VENTE_MOIS = (0.46, 0.45, 0.40, 0.33, 0.24, 0.18, 0.20, 0.27, 0.35, 0.40, 0.44, 0.42)
ENERGIE_CONSO_JOURS = (15.2, 14.1, 16.4, 18.7, 15.8, 13.2, 14.6)
ENERGIE_CONSO_MOIS = (468.0, 452.0, 431.0, 518.0, 640.0, 724.0, 738.0, 655.0, 562.0, 488.0, 461.0, 433.0)


def _arrondi(v: float, decimales: int = 2) -> str:
    return f"{round(v, decimales):.{decimales}f}".rstrip("0").rstrip(".") or "0"


def _bilan_tranche(production: list[str], vente: list[str], achat: list[str]) -> list[str]:
    """Gain de chaque créneau ayant une production et une vente (le reste reste vide)."""
    gains = []
    for p, v in zip(production, vente):
        if p in ("", "nan") or v in ("", "nan"):
            gains.append("")
            continue
        sur_place = max(float(p) - float(v), 0.0)
        gains.append(_arrondi(sur_place * ENERGIE_PRIX_ACHAT + float(v) * ENERGIE_PRIX_REVENTE))
    return gains


def build_energie_bilan(vue: str, aujourd_hui: _dt.date) -> dict:
    """Variables de tab5_maj_energie_bilan pour une vue, datée de `aujourd_hui`."""
    assert vue in ENERGIE_VUES, vue
    production = ENERGIE_HISTORIQUE[vue]
    n = ENERGIE_VUES[vue]
    debut = debut_energie(vue, aujourd_hui)
    if vue == "heures":
        vente, achat = ENERGIE_BILAN_HEURES["vente"], ENERGIE_BILAN_HEURES["achat"]
    else:
        vente, achat = [], []
        heures = ENERGIE_BILAN_HEURES
        for i, p in enumerate(production):
            if vue == "jours" and i == n - 1:
                # Aujourd'hui : la somme des heures déjà écoulées, comme la production.
                v = sum(float(x) for x in heures["vente"] if x)
                a = sum(float(x) for x in heures["achat"] if x)
            elif vue == "jours":
                jour = debut + _dt.timedelta(days=i)
                v = float(p) * ENERGIE_PART_VENTE_JOURS[jour.weekday()]
                a = max(ENERGIE_CONSO_JOURS[jour.weekday()] - (float(p) - v), 0.6)
            else:
                v = float(p) * ENERGIE_PART_VENTE_MOIS[i]
                a = max(ENERGIE_CONSO_MOIS[i] - (float(p) - v), 25.0)
            vente.append(_arrondi(v))
            achat.append(_arrondi(a, 1 if vue == "mois" else 2))
    gain = _bilan_tranche(production, vente, achat)
    assert len(vente) == len(achat) == len(gain) == n, f"{vue} : {len(vente)} valeurs"
    for c, valeurs in (("vente", vente), ("achat", achat), ("gain", gain)):
        for v in valeurs:
            _nombre_ou_vide(c, v)
            assert v in ("", "nan") or float(v) >= 0, f"{vue} : {c} négatif {v!r}"
    assert len(ENERGIE_DEVISE.encode("utf-8")) <= 7, "devise : 7 octets au plus"
    payload = "|".join([ENERGIE_DEVISE, ";".join(vente), ";".join(achat), ";".join(gain)])
    assert payload.count("|") == 3 and "|" not in "".join(vente + achat + gain)
    return {"vue": vue, "debut": debut.isoformat(), "payload": payload}


# ---------------------------------------------------------------------------
# Popup Température (ADR-0032) : l'historique d'une des deux températures de l'accueil
# et, pour la seconde, la prévision de la météo. Ce que pousserait script.tab5_historique
# (HomeAssistant_Config/packages/tab5_historique.yaml) en réponse à l'événement
# esphome.tab5_historique (cle, vue), action tab5_maj_historique :
#   entete = « nom|debut|pas|maintenant|actuel|exterieur[|humidite] » (debut
#   AAAA-MM-JJTHH:MM local, minutes à l'horloge locale ; humidite : avec un capteur
#   d'humidité, ADR-0047), mesures = « moy,min,max[,h_moy,h_min,h_max] » par créneau,
#   « ; » (nb + 1 créneaux, le dernier en cours), previsions = « minute,moy[,min,max] », « ; ».
# Humidité : le salon (« salon_hum ») et le Bureau (p3) ; l'Entrée (p1) n'a que sa
# température.
# Courbes inventées, finies sur la valeur de l'accueil (EMPLACEMENTS) : la pièce chauffée,
# une serre qui monte au soleil ; dehors, la prévision (ou toute la courbe si la seconde
# température est dehors : la case du blueprint).
# ---------------------------------------------------------------------------

# salon, serre, puis la température de la pièce R en mode HA (ADR-0040) : pR.
HISTORIQUE_CLES = ("salon", "serre", "p0", "p1", "p2", "p3", "p4")
# vue : (minutes par créneau, créneaux complets avant celui en cours)
HISTORIQUE_VUES = {"jour": (60, 24), "semaine": (180, 56), "mois": (1440, 30)}
HISTORIQUE_PREV_MAX = 48     # kHistoriquePrevMax de Tab5/socle/tab5_parse.h
HISTORIQUE_MESURES_MAX = 64  # kHistoriqueMesuresMax


def _onde(t: _dt.datetime, pic: float, ampl: float, periode_j: float, ampl_j: float) -> float:
    """Une journée (maximum à l'heure `pic`) et une dérive de quelques jours."""
    h = t.hour + t.minute / 60
    jours = t.toordinal() + h / 24
    return ampl * math.sin(2 * math.pi * (h - pic + 6) / 24) + ampl_j * math.sin(2 * math.pi * jours / periode_j)


def _dehors(t: _dt.datetime) -> float:
    return 18.0 + _onde(t, 15, 6.0, 4.1, 1.5)


def climat_historique(cle: str) -> ClimatPiece | None:
    """Climat de la pièce d'une clé pR (ADR-0040), None pour salon, serre ou une pièce
    sans température : le package répond alors comme sans capteur (nom vide, actuel nan,
    aucune mesure)."""
    if not cle.startswith("p"):
        return None
    piece = PIECES.get(int(cle[1:]))
    return piece.climat if piece is not None else None


def historique_actuel(cle: str) -> str:
    """Valeur de l'accueil sur laquelle la courbe finit (« nan » : pas de capteur)."""
    if cle in ("salon", "serre"):
        return EMPLACEMENTS[cle][1]
    c = climat_historique(cle)
    return c.temperature if c is not None else "nan"


def historique_humidite(cle: str) -> str:
    """Humidité sur laquelle la courbe d'humidité finit (ADR-0047) : celle du salon
    (« salon_hum ») ou de la pièce ; '' sans capteur d'humidité (la serre, une pièce sans)."""
    if cle == "salon":
        return EMPLACEMENTS["salon_hum"][1]
    c = climat_historique(cle)
    return c.humidite if c is not None and c.temperature else ""


def _humidite(cle: str):
    """Humidité de la démo à l'instant t, avant recalage : plus humide la nuit, sèche
    l'après-midi quand la pièce chauffe."""
    decalage = 0 if cle == "salon" else int(cle[1:]) * 2
    return lambda t: 50.0 - _onde(t, 17 + decalage, 6.0, 4.3, 3.0)


def _courbe(cle: str, exterieur: bool):
    """Température de la démo à l'instant t (heure locale naïve), avant recalage."""
    if cle == "salon" or cle.startswith("p"):
        # Une pièce chauffée ; la phase change d'une pièce à l'autre.
        decalage = 0 if cle == "salon" else int(cle[1:]) * 2
        return lambda t: 20.8 + _onde(t, 18 + decalage, 0.8, 5.3, 0.4)
    if exterieur:
        return _dehors
    return lambda t: 19.0 + _onde(t, 14, 5.0, 6.7, 1.2)


def debut_historique(vue: str, maintenant: _dt.datetime) -> _dt.datetime:
    """Premier créneau : l'heure, les trois heures ou le jour en cours, moins nb créneaux
    (comme le package, à l'horloge locale)."""
    pas, nb = HISTORIQUE_VUES[vue]
    n = maintenant.replace(second=0, microsecond=0)
    if vue == "mois":
        return n.replace(hour=0, minute=0) - _dt.timedelta(days=nb)
    h = pas // 60
    return n.replace(hour=n.hour // h * h, minute=0) - _dt.timedelta(minutes=pas * nb)


def _minutes(a: _dt.datetime, b: _dt.datetime) -> int:
    return int((b - a).total_seconds() // 60)


def build_historique(cle: str, vue: str, maintenant: _dt.datetime, exterieur: bool = False) -> dict:
    """Variables de tab5_maj_historique pour une température et une vue, à `maintenant`
    (heure locale naïve). exterieur : la seconde température est dehors."""
    assert cle in HISTORIQUE_CLES and vue in HISTORIQUE_VUES, (cle, vue)
    exterieur = exterieur and cle == "serre"
    pas, nb = HISTORIQUE_VUES[vue]
    debut = debut_historique(vue, maintenant)
    if historique_actuel(cle) == "nan":
        # Pièce sans température déclarée : comme le package sans capteur.
        entete = "|".join(["", debut.strftime("%Y-%m-%dT%H:%M"), str(pas), str(_minutes(debut, maintenant)),
                           "nan", "0"])
        return {"cle": cle, "vue": vue, "entete": entete, "mesures": "", "previsions": ""}
    actuel = float(historique_actuel(cle))
    brute = _courbe(cle, exterieur)
    decalage = actuel - brute(maintenant)
    temp = (lambda t: brute(t) + decalage)
    # Humidité (ADR-0047) : recalée sur celle de l'accueil, en % entiers comme le package.
    h_actuelle = historique_humidite(cle)
    if h_actuelle:
        h_brute = _humidite(cle)
        h_decalage = float(h_actuelle) - h_brute(maintenant)
        humid = (lambda t: min(100.0, max(0.0, h_brute(t) + h_decalage)))

    def echantillons(f, t, fin):
        valeurs = []
        while t < fin:
            valeurs.append(f(t))
            t += _dt.timedelta(minutes=10)
        return valeurs or [f(fin)]

    mesures = []
    for i in range(nb + 1):
        t, fin = debut + _dt.timedelta(minutes=i * pas), min(debut + _dt.timedelta(minutes=(i + 1) * pas), maintenant)
        valeurs = echantillons(temp, t, fin)
        creneau = f"{sum(valeurs) / len(valeurs):.1f},{min(valeurs):.1f},{max(valeurs):.1f}"
        if h_actuelle:
            h = echantillons(humid, t, fin)
            creneau += f",{round(sum(h) / len(h))},{round(min(h))},{round(max(h))}"
        mesures.append(creneau)
    assert len(mesures) == nb + 1 <= HISTORIQUE_MESURES_MAX, vue

    previsions = []
    if cle == "serre":
        # Dehors : la courbe recalée la prolonge ; une serre : la prévision de dehors.
        prevue = temp if exterieur else _dehors
        if vue == "mois":
            for j in range(1, 8):
                jour = (maintenant + _dt.timedelta(days=j)).replace(hour=0, minute=0, second=0, microsecond=0)
                heures = [prevue(jour + _dt.timedelta(hours=h)) for h in range(24)]
                mn, mx = min(heures), max(heures)
                midi = jour.replace(hour=12)
                previsions.append(f"{_minutes(debut, midi)},{(mn + mx) / 2:.1f},{mn:.1f},{mx:.1f}")
        else:
            horizon, tous = (24, 1) if vue == "jour" else (72, 3)
            t = maintenant.replace(minute=0, second=0, microsecond=0) + _dt.timedelta(hours=1)
            while t <= maintenant + _dt.timedelta(hours=horizon):
                if t.hour % tous == 0:
                    previsions.append(f"{_minutes(debut, t)},{prevue(t):.1f}")
                t += _dt.timedelta(hours=1)
    assert len(previsions) <= HISTORIQUE_PREV_MAX, vue
    minutes = [int(p.split(",")[0]) for p in previsions]
    assert minutes == sorted(set(minutes)), "prévision hors de l'ordre"

    # Le package nomme la pièce du capteur : pour pR, celle de la démo.
    nom = {"salon": "Salon", "serre": "Jardin" if exterieur else "Serre"}.get(cle) or PIECES[int(cle[1:])].nom
    champs = [nom, debut.strftime("%Y-%m-%dT%H:%M"), str(pas), str(_minutes(debut, maintenant)),
              f"{actuel:.1f}", "1" if exterieur else "0"]
    if h_actuelle:
        champs.append(h_actuelle)   # septième champ : seulement avec un capteur d'humidité
    entete = "|".join(champs)
    assert len(entete.split("|")) == (7 if h_actuelle else 6) and ";" not in entete
    return {"cle": cle, "vue": vue, "entete": entete, "mesures": ";".join(mesures),
            "previsions": ";".join(previsions)}


# ---------------------------------------------------------------------------
# Vigilance météo (tab5_maj_alerte_meteo_france ; format : variable `payload` dans
# Tab5/paquets/tab5-api-logic.yaml, découpage : parse_and_update_vigilance() dans
# Tab5/ecran/tab5_services.cpp). Le firmware lit 11 à 13 champs '|' : la démo envoie les
# 11 de Météo-France ; brouillard et feux de forêt (MeteoAlarm, lot 4c) sont
# facultatifs en fin de payload. Jusqu'au correctif qui a suivi le lot F, strtok_r
# fusionnait les délimiteurs consécutifs et un champ vide au milieu décalait tous les
# suivants (un firmware 3.7.0 le fait encore) : on ne laisse donc jamais un champ
# vide, "Vert" par défaut pour les 9 niveaux de vigilance, comme HA.
# Nombres tenus par tests/test_doc_comptes.py.
# ---------------------------------------------------------------------------

ALERTE_CHAMPS = (
    "phrase_pluie", "globale", "vent", "inondation", "orages",
    "pluie_inondation", "neige_verglas", "grand_froid",
    "vagues_submersion", "canicule", "avalanches",
)
ALERTE_BUF_OCTETS = 1024  # char buf[1024] de parse_and_update_vigilance() (1023 octets utiles)


def build_alerte_payload(**champs: str) -> str:
    """Construit le payload de tab5_maj_alerte_meteo_france, forme Météo-France (11 champs)."""
    valeurs = []
    for nom in ALERTE_CHAMPS:
        defaut = "" if nom == "phrase_pluie" else "Vert"
        valeurs.append(str(champs.get(nom, defaut)) or defaut)
    payload = "|".join(valeurs)
    taille = len(payload.encode("utf-8"))
    assert taille < ALERTE_BUF_OCTETS, f"payload alerte trop long ({taille} octets >= {ALERTE_BUF_OCTETS})"
    return payload


# ---------------------------------------------------------------------------
# Prévisions horaires / journalières (bulk) : enregistrements séparés par ';',
# champs séparés par '|'. Rejet total et silencieux côté firmware au-delà de
# 2048 octets (tab5_custom.cpp:238-241/283-286) — d'où le pacing en plusieurs
# appels côté prod (repris ici, cf. demo_pusher.py).
# ---------------------------------------------------------------------------

BULK_MAX_OCTETS = 2048
JOURS_FR = ("Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam")


@dataclass
class HeureForecast:
    idx: int          # 0-14
    heure_texte: str  # "14:00"
    condition: str    # cf. update_meteo_icon() : sunny/cloudy/partlycloudy/rainy/pouring/...
    temp: float
    pluvio: float


@dataclass
class JourForecast:
    idx: int              # 0-14
    nom_jour: str          # "Auj", "Mer"… : le jour seul, comme tab5_push.yaml
    condition: str
    tmin: float
    tmax: float
    est_repos: bool
    est_dimanche: bool
    est_passe: bool = False
    heures_ouverture: str = ""


PLUIE_LIBELLES = ("Temps sec", "Pluie faible", "Pluie modérée", "Pluie forte", "Pluie très forte")


def build_pluie_1h_bulk_payload(intensites) -> str:
    """« idx|intensité;… » pour tab5_maj_pluie_1h_bulk (9 barres, un appel)."""
    assert len(intensites) == 9, f"9 intensités attendues, {len(intensites)} reçues"
    for lib in intensites:
        assert lib in PLUIE_LIBELLES, f"intensité inconnue du firmware : {lib!r}"
    payload = ";".join(f"{i}|{lib}" for i, lib in enumerate(intensites)) + ";"
    assert len(payload.encode("utf-8")) < 256, "payload pluie trop long (tampon firmware 256 octets)"
    return payload


def build_heures_bulk_payload(records) -> str:
    parts = [f"{r.idx}|{r.heure_texte}|{r.condition}|{r.temp}|{r.pluvio}" for r in records]
    payload = ";".join(parts) + ";"
    taille = len(payload.encode("utf-8"))
    assert taille <= BULK_MAX_OCTETS, f"payload heures trop long ({taille} octets, rejeté par le firmware)"
    return payload


def build_jours_bulk_payload(records) -> str:
    parts = []
    for r in records:
        parts.append("|".join([
            str(r.idx), r.nom_jour, r.condition, str(r.tmin), str(r.tmax),
            "1" if r.est_repos else "0",
            "1" if r.est_dimanche else "0",
            "1" if r.est_passe else "0",
            r.heures_ouverture,
        ]))
    payload = ";".join(parts) + ";"
    taille = len(payload.encode("utf-8"))
    assert taille <= BULK_MAX_OCTETS, f"payload jours trop long ({taille} octets, rejeté par le firmware)"
    return payload


def _heures_depuis_maintenant(condition: str, temp_base: float, pluvio_base: float) -> tuple:
    """15 tranches horaires (idx 0-14), variation légère autour d'une base."""
    maintenant = _dt.datetime.now()
    records = []
    for i in range(15):
        heure = maintenant + _dt.timedelta(hours=i)
        variation = (i % 5) - 2  # petite oscillation +/-2°C sur la journée
        records.append(HeureForecast(
            idx=i,
            heure_texte=heure.strftime("%H:00"),
            condition=condition,
            temp=round(temp_base + variation, 1),
            pluvio=pluvio_base,
        ))
    return tuple(records)


def _jours_depuis_aujourdhui(condition: str, tmax_base: float, tmin_base: float,
                              jours_repos_supplementaires: frozenset = frozenset()) -> tuple:
    """15 jours (idx 0-14) à partir d'aujourd'hui, week-ends marqués repos par défaut."""
    aujourdhui = _dt.datetime.now()
    records = []
    for i in range(15):
        jour = aujourdhui + _dt.timedelta(days=i)
        est_dimanche = jour.weekday() == 6
        est_weekend = jour.weekday() >= 5
        est_repos = est_weekend or i in jours_repos_supplementaires
        # Le jour SEUL, sans date, comme HA (tab5_push.yaml) : la tablette le traduit
        # (ha_day_name, tab5_forecast.cpp). « Auj 16 » n'était reconnu par aucune langue
        # et restait en français sur un écran anglais (vu sur le rendu hors tablette).
        nom = "Auj" if i == 0 else JOURS_FR[(jour.weekday() + 1) % 7]
        variation = (i % 4) - 1
        records.append(JourForecast(
            idx=i,
            nom_jour=nom,
            condition=condition,
            tmin=round(tmin_base + variation, 1),
            tmax=round(tmax_base + variation, 1),
            est_repos=est_repos,
            est_dimanche=est_dimanche,
            # « HH:MM-HH:MM », comme HA (tab5_push.yaml) : le réveil et le bandeau
            # planning le découpent (alarm_clock.cpp, cal_is_early_shift).
            heures_ouverture="" if est_repos else "09:00-17:30",
        ))
    return tuple(records)


def code_pluie(niveau: int, dans_min: int | None) -> str:
    """Code « @niveau,début » du lot 4c (tab5_central.cpp) : la tablette compose la
    phrase dans sa langue et décompte les minutes. niveau -1 pas de données, 0 sec,
    1 à 4 faible à très forte ; début = epoch UTC, 0 s'il pleut déjà (dans_min None).
    Calculé à l'envoi : un epoch figé à l'import vieillirait pendant la démo."""
    assert -1 <= niveau <= 5, f"niveau de pluie hors contrat : {niveau}"
    if dans_min is None:
        return f"@{niveau},0"
    debut = _dt.datetime.now(_dt.timezone.utc) + _dt.timedelta(minutes=dans_min)
    return f"@{niveau},{int(debut.timestamp())}"


# ---------------------------------------------------------------------------
# Scène : un jeu complet de valeurs pour les 10 services tab5_maj_*.
# ---------------------------------------------------------------------------

@dataclass
class Scene:
    nom: str
    meteo_condition: str
    meteo_temperature: float
    meteo_humidite: float
    probabilites: dict          # uv, gel, neige (str -> int)
    pluie_1h: tuple             # 9 intensités (index 0..8 = 0,5,10,15,20,25,35,45,55 min)
    pluie: tuple                # (niveau, dans_min) -> code_pluie(), 1er champ de l'alerte
    alerte: dict                # vigilances (cf. ALERTE_CHAMPS sauf phrase_pluie), absents = "Vert"
    heures: tuple                # 15 HeureForecast
    jours: tuple                 # 15 JourForecast
    clim: dict                  # target, current, mode, preset, fan, swing (toutes en str)
    volet_etat: str
    info_texte: tuple            # (texte, couleur, meteo_id) — texte = code du lot 4c
                                  # « @ha|nb MAJ|titre|nb erreurs|nb indispo|jaune 0/1|vigilance »,
                                  # composé par la tablette dans sa langue (compose_info_code,
                                  # tab5_central.cpp) ; vigilance « orange »/« rouge » affiche la
                                  # bannière de vigilance. meteo_id = identifiant de dismiss (tap sur
                                  # le bandeau) ; vide = bandeau non masquable. Les 3 champs sont
                                  # OBLIGATOIRES : le service tab5_maj_info_texte déclare 3 variables
                                  # et aioesphomeapi lève un KeyError si l'une manque.


SCENES: tuple = (
    Scene(
        nom="Journée ensoleillée",
        meteo_condition="sunny",
        meteo_temperature=27.0,
        meteo_humidite=38.0,
        probabilites={"uv": 6, "gel": 0, "neige": 0},
        pluie_1h=("Temps sec",) * 9,
        pluie=(0, None),
        alerte={},
        heures=_heures_depuis_maintenant("sunny", 26.0, 0.0),
        jours=_jours_depuis_aujourdhui("sunny", 28.0, 16.0),
        clim={"target": "22.0", "current": "23.5", "mode": "cool", "preset": "eco", "fan": "auto", "swing": "off"},
        volet_etat="Ouvert",
        # Rien à signaler côté HA : pas de bandeau info.
        info_texte=("@ha|0||0|0|0|", "Blanc", ""),
    ),
    Scene(
        nom="Pluie + alerte orange",
        meteo_condition="rainy",
        meteo_temperature=14.0,
        meteo_humidite=82.0,
        probabilites={"uv": 1, "gel": 0, "neige": 0},
        pluie_1h=("Pluie faible", "Pluie modérée", "Pluie modérée", "Pluie forte",
                   "Pluie forte", "Pluie modérée", "Pluie faible", "Temps sec", "Temps sec"),
        pluie=(2, 10),  # « Pluie modérée dans 10 mn », décomptée par la tablette
        alerte={
            "globale": "Orange",
            "pluie_inondation": "Orange",
            "orages": "Jaune",
        },
        heures=_heures_depuis_maintenant("lightning-rainy", 13.0, 3.5),
        jours=_jours_depuis_aujourdhui("rainy", 15.0, 10.0),
        clim={"target": "21.0", "current": "20.0", "mode": "heat", "preset": "none", "fan": "low", "swing": "off"},
        volet_etat="En_mouvement",
        # vigilance « orange » -> bannière de vigilance du firmware ; meteo_id non vide :
        # un tap sur le bandeau le masque jusqu'au prochain id différent — c'est la
        # démo de la fonction « tap pour masquer ».
        info_texte=("@ha|0||0|0|0|orange", "Orange", "meteo:orange"),
    ),
    Scene(
        nom="Jour de repos, plantes à surveiller",
        meteo_condition="partlycloudy",
        meteo_temperature=19.0,
        meteo_humidite=55.0,
        probabilites={"uv": 3, "gel": 0, "neige": 0},
        pluie_1h=("Temps sec",) * 7 + ("Pluie faible", "Pluie faible"),
        pluie=(1, 45),
        alerte={},
        heures=_heures_depuis_maintenant("partlycloudy", 18.0, 0.5),
        jours=_jours_depuis_aujourdhui("partlycloudy", 20.0, 12.0, jours_repos_supplementaires=frozenset({0})),
        clim={"target": "20.0", "current": "19.5", "mode": "fan_only", "preset": "none", "fan": "quiet", "swing": "vertical"},
        volet_etat="Ferme",
        # Une mise à jour et deux entités indisponibles : bandeau « 1 MAJ · … · 2 indispo ».
        info_texte=("@ha|1|Home Assistant Core 2026.10.0|0|2|0|", "Orange", ""),
    ),
)
