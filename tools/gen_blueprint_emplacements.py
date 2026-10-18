#!/usr/bin/env python3
"""tools/gen_blueprint_emplacements.py — parties répétées du blueprint « Tab5 — emplacements ».

[AI-CONTEXT] Audit du 07/10/2026, HA-8. Le blueprint
HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml déclare les mêmes
déclencheurs pour chaque pièce (6 × PIECES, plus 2 × PIECES pour la clim de la pièce depuis
l'ADR-0040), chaque ligne de la rangée sous l'horloge (2 × 3) et, depuis le lot 3
(09/10/2026, ADR-0041), chaque ligne du panneau « Ok Nabu » (2 × 3) :
recopiés à la main, une pièce oubliée ou un attribut mal recopié ne se voyait qu'à l'usage.
Depuis l'ADR-0066 (18/10/2026, lot C : jusqu'à dix pièces), il écrit aussi les sections
d'entrées des pièces (« Pièces » avec « Nombre de pièces », puis « Pièce 1 » à « Pièce 10 »,
mêmes entrées pour chacune) et les listes de variables qui les lisent (pieces, noms_saisis,
pieces_climat_choix). Ce script les écrit entre cinq paires de marqueurs, sur le modèle de
tools/gen_tuiles_icones.py :

  # >>> entrées des pièces (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< entrées des pièces
  # >>> variables des pièces (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< variables des pièces
  # >>> déclencheurs des pièces (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< déclencheurs des pièces
  # >>> déclencheurs de la rangée (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< déclencheurs de la rangée
  # >>> déclencheurs du panneau Ok Nabu (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< déclencheurs du panneau Ok Nabu

    python tools/gen_blueprint_emplacements.py          # réécrit les parties générées
    python tools/gen_blueprint_emplacements.py --check  # exit 1 si elles sont périmées (n'écrit rien)

Source unique : les tables ci-dessous (PIECES, PIECES_DEFAUT, ELEMENTS_MAX, PAGES_PIECES,
ATTRIBUTS_PIECE, LIGNES_RANGEE, LIGNES_NABU, ETATS_VISIBLES). Ajouter un attribut suivi =
une ligne de ATTRIBUTS_PIECE, puis relancer.
Les `id:` des déclencheurs (piece_n, piece_n_sortie, piece_n_<suffixe>, rangee_n,
rangee_n_sortie, nabu_n, nabu_n_sortie) sont lus par les variables du blueprint (redefinir, tuiles_a_pousser,
rangee…) : ne pas les renommer. Les clés des entrées (nombre_pieces, piece_n_nom,
piece_n_tuiles, piece_n_temperature, piece_n_humidite, piece_n_clim) sont celles des
automatisations existantes et de l'assistant de l'intégration (custom_components/tab5/
assistant.py) : ne pas les renommer non plus.
Le reste du fichier (commentaires autour, autres déclencheurs, autres entrées) s'édite à la main.
tests/test_blueprint_genere.py échoue si le blueprint commité n'est pas la sortie du script.

Fins de ligne : le fichier est réécrit avec les siennes (CRLF dans un checkout Windows, LF
sous Linux) ; `--check` compare en normalisant.
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BLUEPRINT = REPO / "HomeAssistant_Config" / "blueprints" / "automation" / "tab5" / "tab5_emplacements.yaml"

PIECES = 10         # ADR-0066 : jusqu'à 10 pièces (kPiecesMax du firmware), entrées piece_<n>_*
PIECES_DEFAUT = 5   # « Nombre de pièces » par défaut : les cinq pages de la météo (ADR-0023)
ELEMENTS_MAX = 20   # ADR-0066 : 20 éléments par pièce (kTuilesMax), clés T = 0-9 puis a-j
LIGNES_RANGEE = 3   # ADR-0031 : 3 lignes sous l'horloge, entrées rangee_ligne_<n>
LIGNES_NABU = 3     # ADR-0041 : 3 lignes du panneau Ok Nabu, entrées nabu_ligne_<n>

# Ce que chaque section « Pièce n » dit de sa page (ADR-0023 : pièces 1 à 5 sur les pages
# de la météo ; ADR-0066 : les suivantes, une page chacune après J+10 → J+14) : (titre FR,
# titre EN, phrase FR, phrase EN). La pièce 1 (accueil) a sa propre phrase, plus bas.
PAGES_PIECES = {
    2: ("J+5 → J+9", "day 5 → 9", "Un glissement vers la gauche depuis l'accueil.",
        "One swipe left from the home page."),
    3: ("J+10 → J+14", "day 10 → 14", "Deux glissements vers la gauche depuis l'accueil.",
        "Two swipes left from the home page."),
    4: ("prochaines heures", "next hours", "Un glissement vers la droite depuis l'accueil.",
        "One swipe right from the home page."),
    5: ("heures suivantes", "later hours", "Deux glissements vers la droite depuis l'accueil.",
        "Two swipes right from the home page."),
}
_NOMBRES = {3: ("Trois", "Three"), 4: ("Quatre", "Four"), 5: ("Cinq", "Five"), 6: ("Six", "Six"),
            7: ("Sept", "Seven")}
for _n in range(6, PIECES + 1):
    _fr, _en = _NOMBRES[_n - 3]
    PAGES_PIECES[_n] = ("sa page", "its own page",
                        f"{_fr} glissements vers la gauche depuis l'accueil, après J+10 → J+14 : une page à "
                        "elle, sans météo. Un firmware plus ancien ne montre que les pièces 1 à 5.",
                        f"{_en} swipes left from the home page, after day 10 → 14: a page of its own, "
                        "without weather. An older firmware shows rooms 1 to 5 only.")

# Domaines du sélecteur « Appareils » (ancre &domaines_tuiles, réutilisée par les autres
# pièces et la personnalisation) : la colonne « HA domains » du tableau de l'ADR-0023
# (tests/test_tuiles_blueprint.py compare, custom_components/tab5/assistant.py aussi).
DOMAINES_TUILES = (
    "light", "switch", "input_boolean", "fan", "humidifier", "automation", "cover", "valve",
    "media_player", "scene", "script", "button", "input_button", "sensor", "number",
    "input_number", "binary_sensor", "device_tracker", "person", "lock", "climate",
)

# Attributs d'une tuile suivis pendant que l'appareil reste allumé ou en place
# (suffixe de l'id, attribut HA), dans l'ordre du fichier.
ATTRIBUTS_PIECE = (
    ("luminosite", "brightness"),
    ("couleur", "rgb_color"),
    ("position", "current_position"),
    ("consigne", "temperature"),
)

# États qu'une tuile montre (ancre YAML &etats_visibles, réutilisée par la rangée).
# « on » et « off » entre guillemets : sans eux, YAML 1.1 lit des booléens.
ETATS_VISIBLES = (
    '"on"', '"off"', "open", "closed", "opening", "closing", "playing", "paused", "idle",
    "standby", "buffering", "home", "not_home", "locked", "unlocked", "locking", "unlocking",
    "jammed", "heat", "cool", "heat_cool", "auto", "dry", "fan_only", "unavailable", "unknown",
)

_ENTETE = "(généré par tools/gen_blueprint_emplacements.py, ne pas éditer)"
MARQUES_ENTREES = (f"# >>> entrées des pièces {_ENTETE}", "# <<< entrées des pièces")
MARQUES_VARIABLES = (f"# >>> variables des pièces {_ENTETE}", "# <<< variables des pièces")
MARQUES_PIECES = (f"# >>> déclencheurs des pièces {_ENTETE}", "# <<< déclencheurs des pièces")
MARQUES_RANGEE = (f"# >>> déclencheurs de la rangée {_ENTETE}", "# <<< déclencheurs de la rangée")
MARQUES_NABU = (f"# >>> déclencheurs du panneau Ok Nabu {_ENTETE}", "# <<< déclencheurs du panneau Ok Nabu")


class ErreurBlueprint(ValueError):
    pass


# ─── Entrées des pièces ──────────────────────────────────────────────────────

def _description(ind: str, fr: list[str], en: list[str]) -> list[str]:
    """Description `>-` « français, deux lignes vides, english » (le format du fichier)."""
    return [f"{ind}description: >-"] + [f"{ind}  {l}" for l in fr] + ["", ""] + [f"{ind}  {l}" for l in en]


def _entree_nombre(ind: str) -> list[str]:
    e = ind + "    "
    return ([f"{ind}pieces:",
             f"{ind}  name: Pièces · Rooms",
             f"{ind}  icon: mdi:floor-plan"]
            + _description(f"{ind}  ", [
                "Combien de pièces la tablette montre ; chacune a sa section ci-dessous."],
                ["How many rooms the tablet shows; each has its section below."])
            + [f"{ind}  input:",
               f"{ind}    nombre_pieces:",
               f"{e}  name: Nombre de pièces · Number of rooms"]
            + _description(f"{e}  ", [
                f"De 1 à {PIECES} : les sections « Pièce n » au-delà sont ignorées (rien n'en part vers",
                f"la tablette). Les pièces 1 à {PIECES_DEFAUT} suivent les pages de la météo, les suivantes",
                "ont chacune une page après J+10 → J+14. Un firmware plus ancien en montre cinq au",
                "plus."],
                [f"From 1 to {PIECES}: the « Room n » sections beyond are ignored (nothing of them is sent",
                 f"to the tablet). Rooms 1 to {PIECES_DEFAUT} follow the weather pages, the next ones each",
                 "get a page after day 10 → 14. An older firmware shows five at most."])
            + [f"{e}  default: {PIECES_DEFAUT}",
               f"{e}  selector:",
               f"{e}    number:",
               f"{e}      min: 1",
               f"{e}      max: {PIECES}",
               f"{e}      step: 1",
               f"{e}      mode: slider"])


def _entree_capteur(e: str, n: int, champ: str, nom: str, fr: list[str], en: list[str],
                    filtre: list[str]) -> list[str]:
    return ([f"{e}piece_{n}_{champ}:", f"{e}  name: {nom}"] + _description(f"{e}  ", fr, en)
            + [f"{e}  default: []", f"{e}  selector:", f"{e}    entity:", f"{e}      filter:"]
            + [f"{e}        {l}" for l in filtre])


def _section_piece(ind: str, n: int) -> list[str]:
    e = ind + "    "
    if n == 1:
        tete = [f'{ind}piece_1:',
                f'{ind}  name: "Pièce 1 — accueil (aujourd\'hui → J+4) · Room 1 — home (today → day 4)"',
                f"{ind}  icon: mdi:home"] + _description(f"{ind}  ", [
                    "Les tuiles de l'accueil. Laissée vide, l'accueil garde les appareils de la section",
                    "« Ancien accueil », tout en bas."],
                    ["The tiles of the home page. Left empty, the home page keeps the devices of the",
                     "« Former home page » section, at the bottom."])
    else:
        titre_fr, titre_en, fr, en = PAGES_PIECES[n]
        tete = ([f'{ind}piece_{n}:',
                 f'{ind}  name: "Pièce {n} — {titre_fr} · Room {n} — {titre_en}"',
                 f"{ind}  icon: mdi:home-outline"]
                + _description(f"{ind}  ", [fr, f"Comptée si « Nombre de pièces » vaut au moins {n}."],
                               [en, f"Used when « Number of rooms » is at least {n}."])
                + [f"{ind}  collapsed: true"])
    nom = ([f"{e}piece_{n}_nom:", f"{e}  name: Nom de la pièce · Room name"]
           + _description(f"{e}  ", [
               "Vide = l'aire de ses appareils, s'ils en ont une seule (et au moins la moitié",
               f"l'a), sinon « Pièce {n} »."],
               ["Empty = the area of its devices, if they share one (and at least half have",
                f"it), else « Pièce {n} »."])
           + [f"{e}  default: \"\"", f"{e}  selector:", f"{e}    text:"])
    fr = [f"Vingt au plus, dans l'ordre (de gauche à droite). Jusqu'à cinq, une tuile",
          "chacun ; au-delà, les dernières tuiles sont coupées en deux (haut, bas), puis en",
          "quatre. Plus de vingt : les vingt premiers. Un firmware plus ancien montre les",
          "cinq premiers."]
    en = ["Twenty at most, in order (left to right). Up to five, one tile each; beyond, the",
          "last tiles are cut in two (top, bottom), then in four. More than twenty: the first",
          "twenty. An older firmware shows the first five."]
    if n == 1:
        fr += ["Lumières, interrupteurs, volets, lecteurs, scènes et scripts, capteurs, climatisation."]
        en += ["Lights, switches, covers, media players, scenes and scripts, sensors, climate."]
        domaines = [f"{e}        - domain: &domaines_tuiles"] + [f"{e}            - {d}" for d in DOMAINES_TUILES]
    else:
        domaines = [f"{e}        - domain: *domaines_tuiles"]
    tuiles = ([f"{e}piece_{n}_tuiles:", f"{e}  name: Appareils · Devices"] + _description(f"{e}  ", fr, en)
              + [f"{e}  default: []", f"{e}  selector:", f"{e}    entity:", f"{e}      multiple: true",
                 f"{e}      reorder: true", f"{e}      filter:"] + domaines)
    temperature = _entree_capteur(e, n, "temperature", "Température de la pièce · Room temperature", [
        "Facultatif. En mode « HA » sur cette pièce, elle remplace la température du",
        "salon en haut de l'accueil ; un appui long montre son historique."], [
        "Optional. In « HA » mode on this room, it replaces the living-room temperature",
        "at the top of the home page; a long press shows its history."],
        ["- domain: sensor", "  device_class: temperature"])
    humidite = _entree_capteur(e, n, "humidite", "Humidité de la pièce · Room humidity", [
        "Facultatif, avec la température : elle prend la place de la seconde",
        "température (serre) et colore l'icône de la pièce."], [
        "Optional, with the temperature: it takes the place of the second temperature",
        "(greenhouse) and colours the room's icon."],
        ["- domain: sensor", "  device_class: humidity"])
    clim = _entree_capteur(e, n, "clim", "Climatisation de la pièce · Room climate", [
        "Facultatif, avec la température : les boutons − / + de l'accueil la règlent",
        "sur cette pièce, et toucher sa consigne ouvre sa fenêtre."], [
        "Optional, with the temperature: the home page's − / + buttons adjust it on this",
        "room, and touching its setpoint opens its window."],
        ["- domain: climate"])
    return tete + [f"{ind}  input:"] + nom + tuiles + temperature + humidite + clim


def rendre_entrees(ind: str) -> list[str]:
    out = [f"{ind}# Pièce n ↔ page de la rangée du bas, dans l'ordre où un glissement les atteint",
           f"{ind}# depuis l'accueil (ADR-0023, ADR-0066) : R = n − 1. Seules les « Nombre de",
           f"{ind}# pièces » premières comptent."]
    out += _entree_nombre(ind)
    for n in range(1, PIECES + 1):
        out += _section_piece(ind, n)
    return out


def rendre_variables(ind: str) -> list[str]:
    out = [f"{ind}# Pièces (ADR-0023, ADR-0066) : pieces[n] = entités de la pièce n + 1, dans l'ordre",
           f"{ind}# choisi, pour les {PIECES} sections ; nombre_pieces (plus bas) dit combien comptent.",
           f"{ind}nombre_pieces_choisi: !input nombre_pieces",
           f"{ind}pieces:"]
    out += [f"{ind}  - !input piece_{n}_tuiles" for n in range(1, PIECES + 1)]
    out += [f"{ind}noms_saisis:"] + [f"{ind}  - !input piece_{n}_nom" for n in range(1, PIECES + 1)]
    out += [f"{ind}# Climat des pièces (ADR-0040) : sonde de température, d'humidité et clim de chaque",
            f"{ind}# section « Pièce n », toutes facultatives.",
            f"{ind}pieces_climat_choix:"]
    for n in range(1, PIECES + 1):
        out += [f"{ind}  - temperature: !input piece_{n}_temperature",
                f"{ind}    humidite: !input piece_{n}_humidite",
                f"{ind}    clim: !input piece_{n}_clim"]
    return out


# ─── Déclencheurs ────────────────────────────────────────────────────────────

def _etat(entree: str, ident: str, ind: str, *, premier: bool) -> list[str]:
    """Déclencheur « passe à un état visible » ; le premier pose l'ancre."""
    to = f"&etats_visibles [{', '.join(ETATS_VISIBLES)}]" if premier else "*etats_visibles"
    return [f"{ind}- trigger: state", f"{ind}  entity_id: !input {entree}", f"{ind}  to: {to}",
            f"{ind}  id: {ident}"]


def _sortie(entree: str, ident: str, ind: str) -> list[str]:
    """Déclencheur « sort d'un état visible vers un autre »."""
    return [f"{ind}- trigger: state", f"{ind}  entity_id: !input {entree}", f"{ind}  from: *etats_visibles",
            f"{ind}  not_to: *etats_visibles", f"{ind}  id: {ident}_sortie"]


def _attribut(entree: str, attribut: str, ident: str, ind: str) -> list[str]:
    """Déclencheur « l'attribut change pendant que l'appareil reste allumé ou en place »."""
    return [f"{ind}- trigger: state", f"{ind}  entity_id: !input {entree}", f"{ind}  attribute: {attribut}",
            f"{ind}  not_from: [null]", f"{ind}  not_to: [null]", f"{ind}  id: {ident}"]


def _clim_piece(n: int, ind: str) -> list[str]:
    """Clim de la pièce n (ADR-0040, entrée piece_<n>_clim) : son état seulement (`to: ~`,
    sans les attributs : la température de la pièce bouge par dixièmes et part avec les
    mesures lentes), puis sa consigne tant qu'elle a une (les − / + comptent à partir
    d'elle)."""
    entree, ident = f"piece_{n}_clim", f"piece_{n}_clim"
    return ([f"{ind}- trigger: state", f"{ind}  entity_id: !input {entree}", f"{ind}  to: ~", f"{ind}  id: {ident}"]
            + _attribut(entree, "temperature", f"{ident}_consigne", ind))


def rendre_pieces(ind: str) -> list[str]:
    out: list[str] = []
    for n in range(1, PIECES + 1):
        entree, ident = f"piece_{n}_tuiles", f"piece_{n}"
        out += _etat(entree, ident, ind, premier=n == 1)
        out += _sortie(entree, ident, ind)
        for suffixe, attribut in ATTRIBUTS_PIECE:
            out += _attribut(entree, attribut, f"{ident}_{suffixe}", ind)
        out += _clim_piece(n, ind)
    return out


def _lignes(prefixe: str, nombre: int, ind: str) -> list[str]:
    """Une zone à lignes (rangée, panneau Ok Nabu) : état et sortie de chaque ligne."""
    out: list[str] = []
    for n in range(1, nombre + 1):
        entree, ident = f"{prefixe}_ligne_{n}", f"{prefixe}_{n}"
        out += _etat(entree, ident, ind, premier=False)
        out += _sortie(entree, ident, ind)
    return out


def rendre_rangee(ind: str) -> list[str]:
    return _lignes("rangee", LIGNES_RANGEE, ind)


def rendre_nabu(ind: str) -> list[str]:
    return _lignes("nabu", LIGNES_NABU, ind)


def _marques(lignes: list[str], debut: str, fin: str) -> tuple[int, int]:
    ia = [k for k, l in enumerate(lignes) if l.strip() == debut]
    ib = [k for k, l in enumerate(lignes) if l.strip() == fin]
    if len(ia) != 1 or len(ib) != 1 or ib[0] < ia[0]:
        raise ErreurBlueprint(f"tab5_emplacements.yaml : il faut exactement un « {debut} » suivi d'un « {fin} »")
    return ia[0], ib[0]


def rendre_blueprint(texte: str) -> str:
    """Le blueprint (texte en LF) avec ses parties générées à jour."""
    lignes = texte.split("\n")
    for marques, rendre in ((MARQUES_ENTREES, rendre_entrees), (MARQUES_VARIABLES, rendre_variables),
                            (MARQUES_PIECES, rendre_pieces), (MARQUES_RANGEE, rendre_rangee),
                            (MARQUES_NABU, rendre_nabu)):
        a, b = _marques(lignes, *marques)
        ind = lignes[a][: len(lignes[a]) - len(lignes[a].lstrip())]
        lignes[a + 1:b] = rendre(ind)
    return "\n".join(lignes)


def _lire(chemin: Path) -> tuple[str, str]:
    """(texte en LF, fin de ligne du fichier)."""
    brut = chemin.read_bytes().decode("utf-8")
    return brut.replace("\r\n", "\n"), ("\r\n" if "\r\n" in brut else "\n")


def main(argv: list[str]) -> int:
    texte, eol = _lire(BLUEPRINT)
    try:
        attendu = rendre_blueprint(texte)
    except ErreurBlueprint as e:
        print(f"❌ {e}")
        return 1
    resume = (f"{PIECES} pièces (entrées et {2 + len(ATTRIBUTS_PIECE) + 2} déclencheurs chacune), "
              f"{LIGNES_RANGEE} lignes de rangée × 2, {LIGNES_NABU} lignes du panneau Ok Nabu × 2")
    if "--check" in argv:
        if attendu != texte:
            print(f"❌ {BLUEPRINT.relative_to(REPO).as_posix()} n'est pas à jour : "
                  "python tools/gen_blueprint_emplacements.py")
            return 1
        print(f"✅ parties générées du blueprint à jour ({resume})")
        return 0
    if attendu != texte:
        BLUEPRINT.write_bytes(attendu.replace("\n", eol).encode("utf-8"))
        print(f"écrit {BLUEPRINT.relative_to(REPO).as_posix()}")
    print(f"parties générées : {resume}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
