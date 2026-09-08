#!/usr/bin/env python3
"""
[AI-CONTEXT]
@file tools/test_draughts_engine.py
@role Miroir Python du générateur de coups de Tab5/draughts_game.cpp (jeu « Dames
      Tab », namespace Draughts::Engine), exécuté contre les valeurs perft de
      référence des dames internationales 10×10 et des dames anglaises 8×8.

@architecture_constraint Ce script est une TRANSLITTÉRATION du C++ : mêmes
      constantes, mêmes tables DR/DC, même ordre de génération (prises d'abord,
      filtre de la prise majoritaire en international seulement, coups
      silencieux sinon), même règle « les capturées restent sur le plateau
      jusqu'à la fin de la rafle et ne peuvent pas être reprises », même
      promotion en fin de coup seulement. Il valide l'ALGORITHME : si le
      moteur C++ et ce miroir donnent les perft standard, les règles sont
      justes ; s'ils divergent, le bug est dans l'un des deux (ou les deux).
      Il ne valide PAS le binaire compilé.

@ai_instruction Toute modification de gen_moves() / search_*_caps() /
      apply_move() dans draughts_game.cpp doit être répercutée ici, et ce
      script re-exécuté (`pytest` le joue aussi, donc la CI).

Références perft (position initiale, nombre de coups légaux complets — une
rafle = un coup) :
  - internationales 10×10 : 9, 81, 658, 4 265, 27 117, 167 140, 1 049 442 ;
  - anglaises 8×8         : 7, 49, 302, 1 469, 7 361, 36 768, 179 740, 845 931.

Usage :
    python tools/test_draughts_engine.py [profondeur_max]
"""
from __future__ import annotations

import sys
import time

# --- Miroir de draughts_game.h (namespace Engine) ---------------------------
MAX_N = 10
MAX_MOVES = 96   # borne du tableau C++ : vérifiée par les tests (jamais atteinte)
MAX_CAPS = 20
MAX_PATH = 24

VAR_INTL10, VAR_ENG8 = 0, 1
SIDE_WHITE, SIDE_BLACK = 0, 1
EMPTY, W_MAN, W_KING, B_MAN, B_KING = 0, 1, 2, 3, 4
NO_MUST = 255

DR = (-1, -1, +1, +1)
DC = (-1, +1, -1, +1)


def is_white(p: int) -> bool:
    return p == W_MAN or p == W_KING


def is_black(p: int) -> bool:
    return p == B_MAN or p == B_KING


def is_king(p: int) -> bool:
    return p == W_KING or p == B_KING


def is_man(p: int) -> bool:
    return p == W_MAN or p == B_MAN


def piece_side(p: int) -> int:
    return SIDE_WHITE if is_white(p) else SIDE_BLACK


def is_dark_sq(r: int, c: int) -> bool:
    return ((r + c) & 1) == 1


def idx(r: int, c: int, n: int) -> int:
    return r * n + c


def on_board(r: int, c: int, n: int) -> bool:
    return 0 <= r < n and 0 <= c < n


def enemy(p: int, s: int) -> bool:
    return is_black(p) if s == SIDE_WHITE else is_white(p)


class Pos:
    __slots__ = ("sq", "n", "side", "must_from", "variant", "no_progress")

    def __init__(self, variant: int = VAR_INTL10):
        self.n = 8 if variant == VAR_ENG8 else 10
        self.variant = variant
        self.sq = [EMPTY] * (self.n * self.n)
        self.side = SIDE_WHITE
        self.must_from = NO_MUST
        self.no_progress = 0

    def copy(self) -> "Pos":
        q = Pos.__new__(Pos)
        q.sq = self.sq[:]
        q.n, q.side, q.must_from, q.variant, q.no_progress = (
            self.n, self.side, self.must_from, self.variant, self.no_progress)
        return q


class Move:
    __slots__ = ("frm", "to", "caps", "path", "promote")

    def __init__(self, frm: int, to: int, caps: list[int], path: list[int], promote: bool):
        self.frm, self.to, self.caps, self.path, self.promote = frm, to, caps, path, promote

    @property
    def n_caps(self) -> int:
        return len(self.caps)


def pos_init(variant: int) -> Pos:
    """Miroir de Engine::pos_init : noirs en haut (3 ou 4 rangées), blancs en bas."""
    p = Pos(variant)
    n = p.n
    black_rows = 3 if variant == VAR_ENG8 else 4
    white_start = n - black_rows
    for r in range(n):
        for c in range(n):
            if not is_dark_sq(r, c):
                continue
            if r < black_rows:
                p.sq[idx(r, c, n)] = B_MAN
            elif r >= white_start:
                p.sq[idx(r, c, n)] = W_MAN
    return p


# --- Coups silencieux (miroir de gen_silent_*) -------------------------------
def gen_silent_man(p: Pos, r: int, c: int, out: list[Move]) -> None:
    n = p.n
    s = p.side
    forward = -1 if s == SIDE_WHITE else +1
    frm = idx(r, c, n)
    for d in range(4):
        if DR[d] != forward:
            continue
        nr, nc = r + DR[d], c + DC[d]
        if not on_board(nr, nc, n) or not is_dark_sq(nr, nc):
            continue
        if p.sq[idx(nr, nc, n)] != EMPTY:
            continue
        promo = (nr == 0) if s == SIDE_WHITE else (nr == n - 1)
        out.append(Move(frm, idx(nr, nc, n), [], [idx(nr, nc, n)], promo))


def gen_silent_king_eng(p: Pos, r: int, c: int, out: list[Move]) -> None:
    n = p.n
    frm = idx(r, c, n)
    for d in range(4):
        nr, nc = r + DR[d], c + DC[d]
        if not on_board(nr, nc, n) or not is_dark_sq(nr, nc):
            continue
        if p.sq[idx(nr, nc, n)] != EMPTY:
            continue
        out.append(Move(frm, idx(nr, nc, n), [], [idx(nr, nc, n)], False))


def gen_silent_king_intl(p: Pos, r: int, c: int, out: list[Move]) -> None:
    n = p.n
    frm = idx(r, c, n)
    for d in range(4):
        nr, nc = r + DR[d], c + DC[d]
        while on_board(nr, nc, n) and is_dark_sq(nr, nc):
            i = idx(nr, nc, n)
            if p.sq[i] != EMPTY:
                break
            out.append(Move(frm, i, [], [i], False))
            nr += DR[d]
            nc += DC[d]


# --- Rafles (miroir de search_*_caps) ----------------------------------------
def search_man_caps(p: Pos, s: int, r: int, c: int, frm: int,
                    caps: list[int], path: list[int], out: list[Move]) -> bool:
    """Retourne True si une extension existe depuis (r, c) (= found_ext)."""
    n = p.n
    extended = False
    fwd_only = p.variant == VAR_ENG8
    forward = -1 if s == SIDE_WHITE else +1
    for d in range(4):
        if fwd_only and DR[d] != forward:
            continue
        mr, mc = r + DR[d], c + DC[d]
        lr, lc = r + 2 * DR[d], c + 2 * DC[d]
        if not on_board(mr, mc, n) or not on_board(lr, lc, n):
            continue
        if not is_dark_sq(mr, mc) or not is_dark_sq(lr, lc):
            continue
        mi, li = idx(mr, mc, n), idx(lr, lc, n)
        if not enemy(p.sq[mi], s):
            continue
        if mi in caps:
            continue
        if p.sq[li] != EMPTY:
            continue
        extended = True
        search_man_caps(p, s, lr, lc, frm, caps + [mi], path + [li], out)
    if caps and not extended:
        promo = (r == 0) if s == SIDE_WHITE else (r == n - 1)
        out.append(Move(frm, idx(r, c, n), caps, path, promo))
    return extended


def search_king_caps_intl(p: Pos, s: int, r: int, c: int, frm: int,
                          caps: list[int], path: list[int], out: list[Move]) -> bool:
    n = p.n
    extended = False
    for d in range(4):
        nr, nc2 = r + DR[d], c + DC[d]
        while on_board(nr, nc2, n) and is_dark_sq(nr, nc2) and p.sq[idx(nr, nc2, n)] == EMPTY:
            nr += DR[d]
            nc2 += DC[d]
        if not on_board(nr, nc2, n) or not is_dark_sq(nr, nc2):
            continue
        mi = idx(nr, nc2, n)
        if not enemy(p.sq[mi], s):
            continue
        if mi in caps:
            continue
        lr, lc = nr + DR[d], nc2 + DC[d]
        any_land = False
        while on_board(lr, lc, n) and is_dark_sq(lr, lc) and p.sq[idx(lr, lc, n)] == EMPTY:
            any_land = True
            li = idx(lr, lc, n)
            child_ext = search_king_caps_intl(p, s, lr, lc, frm, caps + [mi], path + [li], out)
            if child_ext:
                extended = True
            else:
                out.append(Move(frm, li, caps + [mi], path + [li], False))
            lr += DR[d]
            lc += DC[d]
        if any_land:
            extended = True
    return extended


def search_king_caps_eng(p: Pos, s: int, r: int, c: int, frm: int,
                         caps: list[int], path: list[int], out: list[Move]) -> bool:
    n = p.n
    extended = False
    for d in range(4):
        mr, mc = r + DR[d], c + DC[d]
        lr, lc = r + 2 * DR[d], c + 2 * DC[d]
        if not on_board(mr, mc, n) or not on_board(lr, lc, n):
            continue
        if not is_dark_sq(mr, mc) or not is_dark_sq(lr, lc):
            continue
        mi, li = idx(mr, mc, n), idx(lr, lc, n)
        if not enemy(p.sq[mi], s):
            continue
        if mi in caps:
            continue
        if p.sq[li] != EMPTY:
            continue
        child = search_king_caps_eng(p, s, lr, lc, frm, caps + [mi], path + [li], out)
        if not child:
            out.append(Move(frm, li, caps + [mi], path + [li], False))
        extended = True
    return extended


def gen_caps_from(p: Pos, r: int, c: int, out: list[Move]) -> None:
    n = p.n
    piece = p.sq[idx(r, c, n)]
    s = piece_side(piece)
    if s != p.side:
        return
    frm = idx(r, c, n)
    if is_man(piece):
        search_man_caps(p, s, r, c, frm, [], [], out)
    elif p.variant == VAR_INTL10:
        search_king_caps_intl(p, s, r, c, frm, [], [], out)
    else:
        search_king_caps_eng(p, s, r, c, frm, [], [], out)


def gen_moves(p: Pos) -> list[Move]:
    """Miroir de Engine::gen_moves (sans la borne max_out du tableau C++)."""
    out: list[Move] = []
    n = p.n
    s = p.side
    # 1) toutes les prises (éventuellement depuis must_from)
    for r in range(n):
        for c in range(n):
            if not is_dark_sq(r, c):
                continue
            i = idx(r, c, n)
            if p.must_from != NO_MUST and i != p.must_from:
                continue
            pc = p.sq[i]
            if pc == EMPTY or piece_side(pc) != s:
                continue
            gen_caps_from(p, r, c, out)
    if out:
        if p.variant == VAR_INTL10:
            best = max(m.n_caps for m in out)
            out = [m for m in out if m.n_caps == best]
        return out
    # 2) sinon coups silencieux
    if p.must_from != NO_MUST:
        return out
    for r in range(n):
        for c in range(n):
            if not is_dark_sq(r, c):
                continue
            pc = p.sq[idx(r, c, n)]
            if pc == EMPTY or piece_side(pc) != s:
                continue
            if is_man(pc):
                gen_silent_man(p, r, c, out)
            elif p.variant == VAR_INTL10:
                gen_silent_king_intl(p, r, c, out)
            else:
                gen_silent_king_eng(p, r, c, out)
    return out


def apply_move(p: Pos, m: Move) -> Pos:
    """Miroir de Engine::apply_move, sur une copie (le C++ modifie en place)."""
    q = p.copy()
    piece = q.sq[m.frm]
    q.sq[m.frm] = EMPTY
    for ci in m.caps:
        q.sq[ci] = EMPTY
    if m.promote:
        piece = W_KING if is_white(piece) else B_KING
    q.sq[m.to] = piece
    if m.caps or m.promote:
        q.no_progress = 0
    elif q.no_progress < 250:
        q.no_progress += 1
    q.must_from = NO_MUST
    q.side = SIDE_BLACK if q.side == SIDE_WHITE else SIDE_WHITE
    return q


# --- Perft -------------------------------------------------------------------
class PerftStats:
    max_moves = 0


def perft(p: Pos, depth: int) -> int:
    if depth == 0:
        return 1
    moves = gen_moves(p)
    if len(moves) > PerftStats.max_moves:
        PerftStats.max_moves = len(moves)
    if depth == 1:
        return len(moves)
    total = 0
    for m in moves:
        total += perft(apply_move(p, m), depth - 1)
    return total


PERFT_INTL = [9, 81, 658, 4265, 27117, 167140, 1049442]
PERFT_ENG = [7, 49, 302, 1469, 7361, 36768, 179740, 845931]


# --- Outils de test : position vide + pièces posées --------------------------
def empty_pos(variant: int, side: int = SIDE_WHITE) -> Pos:
    p = Pos(variant)
    p.side = side
    return p


def put(p: Pos, r: int, c: int, piece: int) -> None:
    assert is_dark_sq(r, c), (r, c)
    p.sq[idx(r, c, p.n)] = piece


# --- Tests (joués par pytest via testpaths = ["tests", "tools"]) --------------
def test_perft_international_position_initiale():
    p = pos_init(VAR_INTL10)
    for depth, expected in enumerate(PERFT_INTL[:5], 1):
        assert perft(p, depth) == expected, f"perft intl {depth}"


def test_perft_anglaises_position_initiale():
    p = pos_init(VAR_ENG8)
    for depth, expected in enumerate(PERFT_ENG[:6], 1):
        assert perft(p, depth) == expected, f"perft eng {depth}"


def test_borne_max_moves_du_tableau_cpp():
    """Le C++ tronque silencieusement à MAX_MOVES = 96 : les perft joués ci-dessus
    ne doivent jamais s'en approcher."""
    assert 0 < PerftStats.max_moves <= MAX_MOVES, PerftStats.max_moves


def test_prise_majoritaire_internationale():
    # Pion blanc en (5,4) : à gauche une seule prise, à droite une rafle de deux.
    p = empty_pos(VAR_INTL10)
    put(p, 5, 4, W_MAN)
    put(p, 4, 3, B_MAN)          # prise simple vers (3,2)
    put(p, 4, 5, B_MAN)          # prise vers (3,6)…
    put(p, 2, 7, B_MAN)          # …puis (1,8) : rafle de 2
    moves = gen_moves(p)
    assert moves and all(m.n_caps == 2 for m in moves), [(m.frm, m.to, m.caps) for m in moves]
    assert moves[0].to == idx(1, 8, 10)


def test_prise_libre_en_anglais_pas_de_majorite():
    # Plateau 8×8 : pion blanc (5,2) ; à gauche une prise simple (4,1) → (3,0),
    # à droite une rafle (4,3) → (3,4) puis (2,5) → (1,6). Les deux sont légales.
    p = empty_pos(VAR_ENG8)
    put(p, 5, 2, W_MAN)
    put(p, 4, 1, B_MAN)
    put(p, 4, 3, B_MAN)
    put(p, 2, 5, B_MAN)
    moves = gen_moves(p)
    assert sorted(m.n_caps for m in moves) == [1, 2]


def test_pion_anglais_ne_prend_pas_en_arriere():
    p = empty_pos(VAR_ENG8)
    put(p, 3, 4, W_MAN)
    put(p, 4, 3, B_MAN)          # derrière le pion blanc
    assert all(m.n_caps == 0 for m in gen_moves(p))
    q = empty_pos(VAR_INTL10)
    put(q, 3, 4, W_MAN)
    put(q, 4, 3, B_MAN)
    assert any(m.n_caps == 1 for m in gen_moves(q))


def test_pas_de_promotion_en_cours_de_rafle():
    # Pion blanc (2,1) → prend (1,2) et atterrit (0,3) [dernière rangée] → doit
    # continuer en prenant (1,4) vers (2,5) : pas de promotion (fin ailleurs).
    p = empty_pos(VAR_INTL10)
    put(p, 2, 1, W_MAN)
    put(p, 1, 2, B_MAN)
    put(p, 1, 4, B_MAN)
    moves = gen_moves(p)
    assert len(moves) == 1 and moves[0].n_caps == 2 and not moves[0].promote
    assert moves[0].to == idx(2, 5, 10)


def test_dame_volante_atterrissages_multiples():
    p = empty_pos(VAR_INTL10)
    put(p, 9, 0, W_KING)
    put(p, 6, 3, B_MAN)
    moves = gen_moves(p)
    # Après la prise, tous les atterrissages libres de la diagonale : (5,4)…(0,9).
    assert sorted(m.to for m in moves) == sorted(idx(5 - k, 4 + k, 10) for k in range(6))
    assert all(m.n_caps == 1 for m in moves)


def test_capturee_bloque_et_ne_se_reprend_pas():
    # Dame blanche, un pion noir sur la diagonale : après la prise, revenir en
    # arrière par-dessus la même pièce est interdit (already_cap) — donc 1 seule
    # prise, pas de boucle infinie.
    p = empty_pos(VAR_INTL10)
    put(p, 5, 4, W_KING)
    put(p, 4, 5, B_MAN)
    moves = gen_moves(p)
    assert moves and all(m.n_caps == 1 for m in moves)


def test_apply_move_promotion_et_camp():
    p = empty_pos(VAR_INTL10)
    put(p, 1, 2, W_MAN)
    moves = [m for m in gen_moves(p) if m.promote]
    assert moves
    q = apply_move(p, moves[0])
    assert q.sq[moves[0].to] == W_KING and q.side == SIDE_BLACK and q.no_progress == 0


def main() -> int:
    max_depth = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    ok = True
    for name, variant, ref in (("internationales 10x10", VAR_INTL10, PERFT_INTL),
                               ("anglaises 8x8", VAR_ENG8, PERFT_ENG)):
        p = pos_init(variant)
        print(f"== Dames {name}")
        for depth in range(1, min(max_depth, len(ref)) + 1):
            t0 = time.perf_counter()
            got = perft(p, depth)
            dt = time.perf_counter() - t0
            verdict = "OK" if got == ref[depth - 1] else f"KO (attendu {ref[depth - 1]})"
            ok = ok and got == ref[depth - 1]
            print(f"  perft({depth}) = {got:>10} {verdict} [{dt:.2f}s]")
    print(f"max coups légaux vus : {PerftStats.max_moves} (borne C++ MAX_MOVES = {MAX_MOVES})")
    print("VERDICT :", "OK" if ok else "KO")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
