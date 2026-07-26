#!/usr/bin/env python3
"""
[AI-CONTEXT]
@file tools/test_chess_perft.py
@role Miroir Python du generateur de coups de Tab5/chess_ai.cpp (jeu « Roi Noir »),
      execute contre la suite perft standard.

@architecture_constraint Ce script est une TRANSLITTERATION LIGNE A LIGNE du C++ :
      memes tables d'offsets 0x88, meme CASTLE_MASK, meme calcul de la case de
      prise en passant, meme filtre de legalite dans make(). Il valide donc
      l'ALGORITHME et les TABLES — c'est la ou vivent les bugs d'un generateur.
      Il ne valide PAS le binaire compile : pour ca, appeler `Chess::perft_log(3)`
      sur la cible (voir Tab5/README.md, section « Roi Noir »).

@ai_instruction Toute modification de gen_impl() / make() / attacked() dans
      chess_ai.cpp doit etre repercutee ici, et ce script re-execute.

Usage :
    python tools/test_chess_perft.py
Sortie : une ligne par (position, profondeur), puis un verdict global.
"""

import sys

# --- Codage des pieces (miroir de chess_ai.h) -----------------------------
EMPTY = 0
PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING = 1, 2, 3, 4, 5, 6
WHITE, BLACK = 0, 8
COLOR_MASK, TYPE_MASK = 8, 7
NO_SQ = 0x7F

MF_CAPTURE, MF_EP, MF_CASTLE, MF_DOUBLE, MF_PROMO = 1, 2, 4, 8, 16
CR_WK, CR_WQ, CR_BK, CR_BQ = 1, 2, 4, 8

SQ64 = [(r << 4) | f for r in range(8) for f in range(8)]

OFF_KNIGHT = (33, 31, 18, 14, -14, -18, -31, -33)
OFF_BISHOP = (17, 15, -15, -17)
OFF_ROOK = (16, -16, 1, -1)
OFF_KING = (17, 16, 15, 1, -1, -15, -16, -17)

CASTLE_MASK = [0x0F] * 128
CASTLE_MASK[0x00] = 0x0F & ~CR_WQ
CASTLE_MASK[0x07] = 0x0F & ~CR_WK
CASTLE_MASK[0x04] = 0x0F & ~(CR_WK | CR_WQ)
CASTLE_MASK[0x70] = 0x0F & ~CR_BQ
CASTLE_MASK[0x77] = 0x0F & ~CR_BK
CASTLE_MASK[0x74] = 0x0F & ~(CR_BK | CR_BQ)

CHAR_TO_PIECE = {
    'P': WHITE | PAWN, 'p': BLACK | PAWN,
    'N': WHITE | KNIGHT, 'n': BLACK | KNIGHT,
    'B': WHITE | BISHOP, 'b': BLACK | BISHOP,
    'R': WHITE | ROOK, 'r': BLACK | ROOK,
    'Q': WHITE | QUEEN, 'q': BLACK | QUEEN,
    'K': WHITE | KING, 'k': BLACK | KING,
}


class Position:
    __slots__ = ('board', 'side', 'castling', 'ep', 'halfmove', 'fullmove', 'king_sq')

    def __init__(self):
        self.board = [EMPTY] * 128
        self.side = WHITE
        self.castling = 0
        self.ep = NO_SQ
        self.halfmove = 0
        self.fullmove = 1
        self.king_sq = [NO_SQ, NO_SQ]


def set_fen(p, fen):
    p.board = [EMPTY] * 128
    p.side, p.castling, p.ep = WHITE, 0, NO_SQ
    p.halfmove, p.fullmove = 0, 1
    p.king_sq = [NO_SQ, NO_SQ]

    parts = fen.split()
    rank, file = 7, 0
    for c in parts[0]:
        if c == '/':
            rank -= 1
            file = 0
        elif c.isdigit():
            file += int(c)
        else:
            pc = CHAR_TO_PIECE[c]
            sq = (rank << 4) | file
            p.board[sq] = pc
            if (pc & TYPE_MASK) == KING:
                p.king_sq[(pc & COLOR_MASK) >> 3] = sq
            file += 1

    p.side = BLACK if len(parts) > 1 and parts[1] == 'b' else WHITE
    if len(parts) > 2:
        for c in parts[2]:
            if c == 'K':
                p.castling |= CR_WK
            elif c == 'Q':
                p.castling |= CR_WQ
            elif c == 'k':
                p.castling |= CR_BK
            elif c == 'q':
                p.castling |= CR_BQ
    if len(parts) > 3 and parts[3] != '-':
        p.ep = ((int(parts[3][1]) - 1) << 4) | (ord(parts[3][0]) - ord('a'))
    if len(parts) > 4:
        p.halfmove = int(parts[4])
    if len(parts) > 5:
        p.fullmove = int(parts[5])
    return p


def attacked(p, sq, by):
    b = p.board
    if by == WHITE:
        for d in (-17, -15):
            t = sq + d
            if not (t & 0x88) and 0 <= t < 128 and b[t] == (WHITE | PAWN):
                return True
    else:
        for d in (17, 15):
            t = sq + d
            if not (t & 0x88) and 0 <= t < 128 and b[t] == (BLACK | PAWN):
                return True
    kn = by | KNIGHT
    for d in OFF_KNIGHT:
        t = sq + d
        if not (t & 0x88) and 0 <= t < 128 and b[t] == kn:
            return True
    kg = by | KING
    for d in OFF_KING:
        t = sq + d
        if not (t & 0x88) and 0 <= t < 128 and b[t] == kg:
            return True
    for d in OFF_BISHOP:
        t = sq + d
        while not (t & 0x88) and 0 <= t < 128:
            q = b[t]
            if q:
                if (q & COLOR_MASK) == by and (q & TYPE_MASK) in (BISHOP, QUEEN):
                    return True
                break
            t += d
    for d in OFF_ROOK:
        t = sq + d
        while not (t & 0x88) and 0 <= t < 128:
            q = b[t]
            if q:
                if (q & COLOR_MASK) == by and (q & TYPE_MASK) in (ROOK, QUEEN):
                    return True
                break
            t += d
    return False


def gen_impl(p, caps_only=False):
    """Coups PSEUDO-legaux. Un coup = (from, to, promo, flags)."""
    out = []
    b = p.board
    us = p.side
    them = us ^ COLOR_MASK

    for sq in SQ64:
        pc = b[sq]
        if not pc or (pc & COLOR_MASK) != us:
            continue
        t = pc & TYPE_MASK

        if t == PAWN:
            fwd = 16 if us == WHITE else -16
            start_rank = 1 if us == WHITE else 6
            promo_rank = 7 if us == WHITE else 0
            to = sq + fwd
            if not (to & 0x88) and 0 <= to < 128 and b[to] == EMPTY:
                if (to >> 4) == promo_rank:
                    for pr in (QUEEN, ROOK, BISHOP, KNIGHT):
                        out.append((sq, to, pr, MF_PROMO))
                elif not caps_only:
                    out.append((sq, to, 0, 0))
                    to2 = sq + 2 * fwd
                    if (sq >> 4) == start_rank and not (to2 & 0x88) and 0 <= to2 < 128 and b[to2] == EMPTY:
                        out.append((sq, to2, 0, MF_DOUBLE))
            for dd in (-1, 1):
                c = sq + fwd + dd
                if (c & 0x88) or not (0 <= c < 128):
                    continue
                tp = b[c]
                if tp and (tp & COLOR_MASK) == them:
                    if (c >> 4) == promo_rank:
                        for pr in (QUEEN, ROOK, BISHOP, KNIGHT):
                            out.append((sq, c, pr, MF_PROMO | MF_CAPTURE))
                    else:
                        out.append((sq, c, 0, MF_CAPTURE))
                elif tp == EMPTY and p.ep != NO_SQ and c == p.ep:
                    out.append((sq, c, 0, MF_CAPTURE | MF_EP))
            continue

        if t in (KNIGHT, KING):
            offs = OFF_KNIGHT if t == KNIGHT else OFF_KING
            for d in offs:
                to = sq + d
                if (to & 0x88) or not (0 <= to < 128):
                    continue
                tp = b[to]
                if tp and (tp & COLOR_MASK) == us:
                    continue
                if caps_only and not tp:
                    continue
                out.append((sq, to, 0, MF_CAPTURE if tp else 0))
            continue

        offs = OFF_BISHOP if t == BISHOP else (OFF_ROOK if t == ROOK else OFF_KING)
        for d in offs:
            to = sq + d
            while not (to & 0x88) and 0 <= to < 128:
                tp = b[to]
                if not tp:
                    if not caps_only:
                        out.append((sq, to, 0, 0))
                    to += d
                    continue
                if (tp & COLOR_MASK) == them:
                    out.append((sq, to, 0, MF_CAPTURE))
                break

    if not caps_only:
        if us == WHITE and b[0x04] == (WHITE | KING):
            if (p.castling & CR_WK) and b[0x07] == (WHITE | ROOK) and \
               b[0x05] == EMPTY and b[0x06] == EMPTY and \
               not attacked(p, 0x04, them) and not attacked(p, 0x05, them) and not attacked(p, 0x06, them):
                out.append((0x04, 0x06, 0, MF_CASTLE))
            if (p.castling & CR_WQ) and b[0x00] == (WHITE | ROOK) and \
               b[0x03] == EMPTY and b[0x02] == EMPTY and b[0x01] == EMPTY and \
               not attacked(p, 0x04, them) and not attacked(p, 0x03, them) and not attacked(p, 0x02, them):
                out.append((0x04, 0x02, 0, MF_CASTLE))
        elif us == BLACK and b[0x74] == (BLACK | KING):
            if (p.castling & CR_BK) and b[0x77] == (BLACK | ROOK) and \
               b[0x75] == EMPTY and b[0x76] == EMPTY and \
               not attacked(p, 0x74, them) and not attacked(p, 0x75, them) and not attacked(p, 0x76, them):
                out.append((0x74, 0x76, 0, MF_CASTLE))
            if (p.castling & CR_BQ) and b[0x70] == (BLACK | ROOK) and \
               b[0x73] == EMPTY and b[0x72] == EMPTY and b[0x71] == EMPTY and \
               not attacked(p, 0x74, them) and not attacked(p, 0x73, them) and not attacked(p, 0x72, them):
                out.append((0x74, 0x72, 0, MF_CASTLE))
    return out


ROOK_MOVE = {0x06: (0x07, 0x05), 0x02: (0x00, 0x03),
             0x76: (0x77, 0x75), 0x72: (0x70, 0x73)}


def make(p, m):
    """Retourne l'etat d'annulation, ou None si le coup est illegal (roi en prise)."""
    frm, to, promo, flags = m
    mover = p.side
    b = p.board
    pc = b[frm]

    u = {'castling': p.castling, 'ep': p.ep, 'halfmove': p.halfmove,
         'captured': EMPTY, 'cap_sq': NO_SQ, 'fullmove': p.fullmove,
         'king_sq': list(p.king_sq)}

    if flags & MF_EP:
        vic = (frm & 0xF0) | (to & 0x0F)
        u['captured'] = b[vic]
        u['cap_sq'] = vic
        b[vic] = EMPTY
    elif b[to] != EMPTY:
        u['captured'] = b[to]
        u['cap_sq'] = to

    b[to] = pc
    b[frm] = EMPTY
    if flags & MF_PROMO:
        b[to] = mover | promo

    if flags & MF_CASTLE:
        rf, rt = ROOK_MOVE[to]
        b[rt] = b[rf]
        b[rf] = EMPTY

    if (pc & TYPE_MASK) == KING:
        p.king_sq[mover >> 3] = to

    p.castling &= CASTLE_MASK[frm] & CASTLE_MASK[to]

    p.ep = NO_SQ
    if flags & MF_DOUBLE:
        opp_pawn = (mover ^ COLOR_MASK) | PAWN
        cap = False
        for d in (-1, 1):
            s = to + d
            if not (s & 0x88) and 0 <= s < 128 and b[s] == opp_pawn:
                cap = True
        if cap:
            p.ep = (frm + to) >> 1

    p.halfmove = 0 if ((pc & TYPE_MASK) == PAWN or u['captured'] != EMPTY) else p.halfmove + 1
    if mover == BLACK:
        p.fullmove += 1
    p.side = mover ^ COLOR_MASK

    if attacked(p, p.king_sq[mover >> 3], p.side):
        unmake(p, m, u)
        return None
    return u


def unmake(p, m, u):
    frm, to, promo, flags = m
    mover = p.side ^ COLOR_MASK
    b = p.board
    pc = b[to]
    if flags & MF_PROMO:
        pc = mover | PAWN
    b[frm] = pc
    b[to] = EMPTY
    if u['captured'] != EMPTY:
        b[u['cap_sq']] = u['captured']
    if flags & MF_CASTLE:
        rf, rt = ROOK_MOVE[to]
        b[rf] = b[rt]
        b[rt] = EMPTY
    p.king_sq = list(u['king_sq'])
    p.castling = u['castling']
    p.ep = u['ep']
    p.halfmove = u['halfmove']
    p.fullmove = u['fullmove']
    p.side = mover


def perft(p, depth):
    if depth <= 0:
        return 1
    total = 0
    for m in gen_impl(p):
        u = make(p, m)
        if u is None:
            continue
        total += 1 if depth == 1 else perft(p, depth - 1)
        unmake(p, m, u)
    return total


# --- Suite perft standard --------------------------------------------------
# Valeurs de reference issues de la litterature echiquenne (positions dites
# « initiale », « Kiwipete », 3, 4 et 5). Elles couvrent le roque, la prise en
# passant, les promotions, les clouages et les echecs a la decouverte.
SUITE = [
    ("Initiale",
     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     [20, 400, 8902, 197281]),
    ("Kiwipete (roques + clouages)",
     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     [48, 2039, 97862]),
    ("Position 3 (prise en passant)",
     "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     [14, 191, 2812, 43238]),
    ("Position 4 (promotions)",
     "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     [6, 264, 9467]),
    ("Position 5 (sous-promotions)",
     "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     [44, 1486, 62379]),
]


def main():
    ok = True
    for name, fen, refs in SUITE:
        print(f"\n=== {name} ===")
        print(f"    {fen}")
        for d, expected in enumerate(refs, start=1):
            p = set_fen(Position(), fen)
            got = perft(p, d)
            good = (got == expected)
            ok = ok and good
            print(f"    perft({d}) = {got:>9}  attendu {expected:>9}  {'OK' if good else 'ECHEC'}")
    print()
    print("RESULTAT :", "generateur VALIDE sur toute la suite" if ok else "GENERATEUR FAUX")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
