#!/usr/bin/env python3
"""tools/gen_blueprint_emplacements.py — déclencheurs répétés du blueprint « Tab5 — emplacements ».

[AI-CONTEXT] Audit du 07/10/2026, HA-8. Le blueprint
HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml déclare les mêmes
déclencheurs pour chaque pièce (6 × 5), chaque ligne de la rangée sous l'horloge (2 × 3) et,
depuis le lot 3 (09/10/2026, ADR-0041), chaque ligne du panneau « Ok Nabu » (2 × 3) :
recopiés à la main, une pièce oubliée ou un attribut mal recopié ne se voyait qu'à l'usage.
Ce script les écrit entre trois paires de marqueurs, sur le modèle de
tools/gen_tuiles_icones.py :

  # >>> déclencheurs des pièces (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< déclencheurs des pièces
  # >>> déclencheurs de la rangée (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< déclencheurs de la rangée
  # >>> déclencheurs du panneau Ok Nabu (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)
  # <<< déclencheurs du panneau Ok Nabu

    python tools/gen_blueprint_emplacements.py          # réécrit les parties générées
    python tools/gen_blueprint_emplacements.py --check  # exit 1 si elles sont périmées (n'écrit rien)

Source unique : les tables ci-dessous (PIECES, ATTRIBUTS_PIECE, LIGNES_RANGEE, LIGNES_NABU,
ETATS_VISIBLES). Ajouter un attribut suivi = une ligne de ATTRIBUTS_PIECE, puis relancer.
Les `id:` des déclencheurs (piece_n, piece_n_sortie, piece_n_<suffixe>, rangee_n,
rangee_n_sortie, nabu_n, nabu_n_sortie) sont lus par les variables du blueprint (redefinir, tuiles_a_pousser,
rangee…) : ne pas les renommer.
Le reste du fichier (commentaires autour, autres déclencheurs, entrées) s'édite à la main.
tests/test_blueprint_genere.py échoue si le blueprint commité n'est pas la sortie du script.

Fins de ligne : le fichier est réécrit avec les siennes (CRLF dans un checkout Windows, LF
sous Linux) ; `--check` compare en normalisant.
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BLUEPRINT = REPO / "HomeAssistant_Config" / "blueprints" / "automation" / "tab5" / "tab5_emplacements.yaml"

PIECES = 5          # ADR-0023 : 5 pièces de 5 tuiles, entrées piece_<n>_tuiles
LIGNES_RANGEE = 3   # ADR-0031 : 3 lignes sous l'horloge, entrées rangee_ligne_<n>
LIGNES_NABU = 3     # ADR-0041 : 3 lignes du panneau Ok Nabu, entrées nabu_ligne_<n>

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

MARQUES_PIECES = ("# >>> déclencheurs des pièces (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)",
                  "# <<< déclencheurs des pièces")
MARQUES_RANGEE = ("# >>> déclencheurs de la rangée (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)",
                  "# <<< déclencheurs de la rangée")
MARQUES_NABU = ("# >>> déclencheurs du panneau Ok Nabu (généré par tools/gen_blueprint_emplacements.py, ne pas éditer)",
                "# <<< déclencheurs du panneau Ok Nabu")


class ErreurBlueprint(ValueError):
    pass


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


def rendre_pieces(ind: str) -> list[str]:
    out: list[str] = []
    for n in range(1, PIECES + 1):
        entree, ident = f"piece_{n}_tuiles", f"piece_{n}"
        out += _etat(entree, ident, ind, premier=n == 1)
        out += _sortie(entree, ident, ind)
        for suffixe, attribut in ATTRIBUTS_PIECE:
            out += _attribut(entree, attribut, f"{ident}_{suffixe}", ind)
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
    for marques, rendre in ((MARQUES_PIECES, rendre_pieces), (MARQUES_RANGEE, rendre_rangee),
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
    resume = (f"{PIECES} pièces × {2 + len(ATTRIBUTS_PIECE)} déclencheurs, "
              f"{LIGNES_RANGEE} lignes de rangée × 2, {LIGNES_NABU} lignes du panneau Ok Nabu × 2")
    if "--check" in argv:
        if attendu != texte:
            print(f"❌ {BLUEPRINT.relative_to(REPO).as_posix()} n'est pas à jour : "
                  "python tools/gen_blueprint_emplacements.py")
            return 1
        print(f"✅ déclencheurs du blueprint à jour ({resume})")
        return 0
    if attendu != texte:
        BLUEPRINT.write_bytes(attendu.replace("\n", eol).encode("utf-8"))
        print(f"écrit {BLUEPRINT.relative_to(REPO).as_posix()}")
    print(f"déclencheurs : {resume}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
