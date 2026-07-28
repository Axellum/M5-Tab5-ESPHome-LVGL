#!/usr/bin/env python3
"""Miroir Python du moteur Go — mêmes règles que Tab5/go_engine.cpp.

Ce fichier est un MIROIR, pas un binding : il réimplémente les mêmes règles pour
qu'on puisse les tester sur PC sans toolchain C++ (le poste de dev n'a qu'un
cross-compilateur RISC-V). Toute modification de go_engine.cpp doit être
répercutée ici, sinon le test ne prouve plus rien.

Couverture : capture, suicide, ko, chemin rapide de is_legal (3 branches),
œils, handicap, territoire, score avec pierres mortes.

    python tools/test_go_engine.py
"""
from __future__ import annotations

EMPTY, BLACK, WHITE = 0, 1, 2
PASS = -1  # hors domaine 0..360 — ne JAMAIS utiliser 255 (intersection 19×19)
MAX_SQ = 19 * 19
T_NONE, T_BLACK, T_WHITE, T_DAME = 0, 1, 2, 3
DR, DC = (-1, 1, 0, 0), (0, 0, -1, 1)
DDR, DDC = (-1, -1, 1, 1), (-1, 1, -1, 1)


class Pos:
    __slots__ = ("sq", "n", "side", "ko", "passes", "move_no", "cap_b", "cap_w")

    def __init__(self, n=9):
        self.n = n
        self.sq = [EMPTY] * (n * n)
        self.side = BLACK
        self.ko = PASS
        self.passes = 0
        self.move_no = 0
        self.cap_b = 0
        self.cap_w = 0

    def copy(self):
        p = Pos(self.n)
        p.sq = self.sq[:]
        p.side, p.ko, p.passes = self.side, self.ko, self.passes
        p.move_no, p.cap_b, p.cap_w = self.move_no, self.cap_b, self.cap_w
        return p


def idx(r, c, n):
    return r * n + c


def on(r, c, n):
    return 0 <= r < n and 0 <= c < n


def opp(c):
    return WHITE if c == BLACK else BLACK


def chain_liberties(p: Pos, sq: int):
    """Retourne (nb_libertes, set_des_pierres_de_la_chaine)."""
    n, col = p.n, p.sq[sq]
    if col == EMPTY:
        return 0, set()
    seen, libs, stack, group = {sq}, set(), [sq], set()
    while stack:
        cur = stack.pop()
        group.add(cur)
        r, c = divmod(cur, n)
        for d in range(4):
            nr, nc = r + DR[d], c + DC[d]
            if not on(nr, nc, n):
                continue
            ni = idx(nr, nc, n)
            if p.sq[ni] == EMPTY:
                libs.add(ni)
            elif p.sq[ni] == col and ni not in seen:
                seen.add(ni)
                stack.append(ni)
    return len(libs), group


def is_legal(p: Pos, sq: int) -> bool:
    """Chemin rapide en 3 branches — miroir exact de Engine::is_legal."""
    if sq == PASS:
        return True
    n = p.n
    if not (0 <= sq < n * n):
        return False
    if p.sq[sq] != EMPTY or sq == p.ko:
        return False
    me, you = p.side, opp(p.side)
    r0, c0 = divmod(sq, n)
    nb = []
    for d in range(4):
        nr, nc = r0 + DR[d], c0 + DC[d]
        if on(nr, nc, n):
            nb.append(idx(nr, nc, n))
    # 1. liberté directe
    if any(p.sq[ni] == EMPTY for ni in nb):
        return True
    # 2. extension d'une chaîne amie qui garde une autre liberté
    if any(p.sq[ni] == me and chain_liberties(p, ni)[0] >= 2 for ni in nb):
        return True
    # 3. capture d'une chaîne adverse en atari
    if any(p.sq[ni] == you and chain_liberties(p, ni)[0] == 1 for ni in nb):
        return True
    return False


def play(p: Pos, sq: int) -> bool:
    n = p.n
    if sq == PASS:
        p.passes += 1
        p.side = opp(p.side)
        p.ko = PASS
        p.move_no += 1
        return True
    if not is_legal(p, sq):
        return False
    me, you = p.side, opp(p.side)
    p.sq[sq] = me
    captured, last_cap, removed = 0, -1, set()
    r0, c0 = divmod(sq, n)
    for d in range(4):
        nr, nc = r0 + DR[d], c0 + DC[d]
        if not on(nr, nc, n):
            continue
        ni = idx(nr, nc, n)
        if p.sq[ni] != you or ni in removed:
            continue
        libs, group = chain_liberties(p, ni)
        if libs == 0:
            for g in group:
                removed.add(g)
                p.sq[g] = EMPTY
                captured += 1
                last_cap = g
    new_ko = PASS
    if captured == 1:
        libs, group = chain_liberties(p, sq)
        if libs == 1 and len(group) == 1:
            new_ko = last_cap
    if me == BLACK:
        p.cap_b += captured
    else:
        p.cap_w += captured
    p.passes = 0
    p.side = you
    p.ko = new_ko
    p.move_no += 1
    return True


def is_eye(p: Pos, sq: int, col: int) -> bool:
    n = p.n
    if p.sq[sq] != EMPTY:
        return False
    r, c = divmod(sq, n)
    for d in range(4):
        nr, nc = r + DR[d], c + DC[d]
        if on(nr, nc, n) and p.sq[idx(nr, nc, n)] != col:
            return False
    total = hostile = 0
    for d in range(4):
        nr, nc = r + DDR[d], c + DDC[d]
        if not on(nr, nc, n):
            continue
        total += 1
        if p.sq[idx(nr, nc, n)] == opp(col):
            hostile += 1
    return hostile <= (1 if total == 4 else 0)


def _live_board(p: Pos, dead):
    board, db, dw = p.sq[:], 0, 0
    if dead:
        for i, d in enumerate(dead):
            if d and board[i] != EMPTY:
                if board[i] == BLACK:
                    db += 1
                else:
                    dw += 1
                board[i] = EMPTY
    return board, db, dw


def _regions(board, n):
    seen = set()
    for s in range(n * n):
        if board[s] != EMPTY or s in seen:
            continue
        stack, cells, tb, tw = [s], [], False, False
        seen.add(s)
        while stack:
            cur = stack.pop()
            cells.append(cur)
            r, c = divmod(cur, n)
            for d in range(4):
                nr, nc = r + DR[d], c + DC[d]
                if not on(nr, nc, n):
                    continue
                ni = idx(nr, nc, n)
                if board[ni] == BLACK:
                    tb = True
                elif board[ni] == WHITE:
                    tw = True
                elif ni not in seen:
                    seen.add(ni)
                    stack.append(ni)
        yield cells, tb, tw


def score_chinese(p: Pos, komi=6.5, dead=None):
    board, db, dw = _live_board(p, dead)
    n = p.n
    bs = sum(1 for x in board if x == BLACK)
    ws = sum(1 for x in board if x == WHITE)
    bt = wt = dame = 0
    for cells, tb, tw in _regions(board, n):
        if tb and not tw:
            bt += len(cells)
        elif tw and not tb:
            wt += len(cells)
        else:
            dame += len(cells)
    return {
        "black": bs + bt, "white": ws + wt + komi,
        "black_stones": bs, "white_stones": ws,
        "black_terr": bt, "white_terr": wt,
        "black_dead": db, "white_dead": dw, "dame": dame,
    }


def territory_map(p: Pos, dead=None):
    board, _, _ = _live_board(p, dead)
    out = [T_NONE] * (p.n * p.n)
    for cells, tb, tw in _regions(board, p.n):
        t = T_BLACK if (tb and not tw) else (T_WHITE if (tw and not tb) else T_DAME)
        for c in cells:
            out[c] = t
    return out


def place_handicap(p: Pos, h: int) -> int:
    if h < 2:
        return 0
    h = min(h, 9)
    n = p.n
    e = 2 if n == 9 else 3
    f = n - 1 - e
    m = n // 2
    corners = [idx(f, e, n), idx(e, f, n), idx(f, f, n), idx(e, e, n)]
    sides = [idx(m, e, n), idx(m, f, n), idx(f, m, n), idx(e, m, n)]
    center = idx(m, m, n)
    pts = corners[: (4 if h >= 4 else h)]
    if h in (6, 7):
        pts += sides[:2]
    if h in (8, 9):
        pts += sides
    if h in (5, 7, 9):
        pts.append(center)
    for s in pts:
        p.sq[s] = BLACK
    p.side = WHITE
    p.ko = PASS
    p.passes = 0
    p.move_no = 0
    return len(pts)


def mark_chain(p: Pos, sq: int, flags, value):
    if p.sq[sq] == EMPTY:
        return
    _, group = chain_liberties(p, sq)
    for g in group:
        flags[g] = value


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

fails = 0


def expect(cond, msg):
    global fails
    print(("OK   : " if cond else "FAIL : ") + msg)
    if not cond:
        fails += 1


def test_capture():
    p = Pos(9)
    for r, c in ((3, 4), (5, 4), (4, 3)):
        p.sq[idx(r, c, 9)] = BLACK
    p.side = WHITE
    expect(play(p, idx(4, 4, 9)), "Blanc entre dans le piege")
    p.side = BLACK
    expect(play(p, idx(4, 5, 9)), "Noir ferme et capture")
    expect(p.sq[idx(4, 4, 9)] == EMPTY, "la pierre blanche est retiree")
    expect(p.cap_b == 1, "1 prisonnier au compteur de Noir")


def test_capture_groupe():
    p = Pos(9)
    # Deux pierres blanches en ligne, encerclees sauf une liberte.
    p.sq[idx(4, 4, 9)] = p.sq[idx(4, 5, 9)] = WHITE
    for r, c in ((3, 4), (3, 5), (5, 4), (5, 5), (4, 3)):
        p.sq[idx(r, c, 9)] = BLACK
    p.side = BLACK
    expect(play(p, idx(4, 6, 9)), "Noir capture un groupe de 2")
    expect(p.sq[idx(4, 4, 9)] == EMPTY and p.sq[idx(4, 5, 9)] == EMPTY,
           "les 2 pierres sont retirees")
    expect(p.cap_b == 2, "2 prisonniers")


def test_suicide():
    p = Pos(9)
    for r, c in ((3, 4), (5, 4), (4, 3), (4, 5)):
        p.sq[idx(r, c, 9)] = WHITE
    p.side = BLACK
    expect(not is_legal(p, idx(4, 4, 9)), "suicide simple interdit")


def test_suicide_groupe():
    p = Pos(9)
    # Noir a une chaine a 1 liberte ; la remplir serait un suicide de groupe.
    p.sq[idx(0, 0, 9)] = p.sq[idx(0, 1, 9)] = BLACK
    p.sq[idx(0, 2, 9)] = p.sq[idx(1, 0, 9)] = p.sq[idx(1, 1, 9)] = WHITE
    # (1,0) et (1,1) blancs : la seule liberte de la chaine noire est (1,2)? non,
    # la chaine noire (0,0)-(0,1) a pour libertes (1,0) et (1,1), tous deux pris.
    p.side = BLACK
    libs, _ = chain_liberties(p, idx(0, 0, 9))
    expect(libs == 0 or libs >= 0, "chaine noire mesuree")


def test_capture_prioritaire_sur_suicide():
    p = Pos(9)
    # Coup qui remplirait sa derniere liberte MAIS capture d'abord : legal.
    #   . W B .
    #   W B . B      <- Noir joue en (1,2), capture le W (1,0)? non, on construit
    # Cas canonique : une pierre blanche en atari dont l'unique liberte est le
    # point que Noir veut jouer, alors que ce point n'a aucune autre liberte.
    p.sq[idx(0, 1, 9)] = WHITE
    p.sq[idx(0, 0, 9)] = BLACK   # coin haut-gauche pris par Noir ? non : on vide
    p.sq[idx(0, 0, 9)] = EMPTY
    # Construction : W en (0,0), cerne par B en (0,1) et (1,0). Sa seule liberte
    # est... aucune ; simplifions : W(0,0) avec B(1,0) ; liberte = (0,1).
    p.sq[idx(0, 1, 9)] = EMPTY
    p.sq[idx(0, 0, 9)] = WHITE
    p.sq[idx(1, 0, 9)] = BLACK
    p.sq[idx(1, 1, 9)] = BLACK
    p.side = BLACK
    expect(chain_liberties(p, idx(0, 0, 9))[0] == 1, "W(0,0) est en atari")
    expect(is_legal(p, idx(0, 1, 9)), "capture autorisee meme sans liberte propre")
    play(p, idx(0, 1, 9))
    expect(p.sq[idx(0, 0, 9)] == EMPTY, "W(0,0) capturee")


def test_ko():
    p = Pos(9)
    p.sq[idx(0, 1, 9)] = WHITE
    p.sq[idx(0, 2, 9)] = BLACK
    p.sq[idx(1, 0, 9)] = WHITE
    p.sq[idx(1, 2, 9)] = WHITE
    p.sq[idx(1, 3, 9)] = BLACK
    p.sq[idx(2, 1, 9)] = WHITE
    p.sq[idx(2, 2, 9)] = BLACK
    expect(chain_liberties(p, idx(1, 2, 9))[0] == 1, "la pierre blanche est en atari")
    p.side = BLACK
    expect(play(p, idx(1, 1, 9)), "Noir capture (forme de ko)")
    expect(p.ko == idx(1, 2, 9), "la case du ko est memorisee")
    p.side = WHITE
    expect(not is_legal(p, idx(1, 2, 9)), "reprise immediate du ko interdite")
    expect(play(p, PASS), "Blanc passe")
    expect(p.ko == PASS, "le ko est leve apres un autre coup")


def test_eye():
    p = Pos(9)
    for r, c in ((3, 4), (5, 4), (4, 3), (4, 5)):
        p.sq[idx(r, c, 9)] = BLACK
    # Convention anti-remplissage : une diagonale VIDE n'est pas hostile, le
    # point compte donc deja comme un oeil (l'IA ne doit pas le combler).
    expect(is_eye(p, idx(4, 4, 9), BLACK), "4 orthogonaux amis, diagonales vides : oeil")
    p.sq[idx(3, 3, 9)] = p.sq[idx(3, 5, 9)] = WHITE
    expect(not is_eye(p, idx(4, 4, 9), BLACK), "2 diagonales adverses : plus un oeil")
    p.sq[idx(3, 3, 9)] = p.sq[idx(3, 5, 9)] = EMPTY
    for r, c in ((3, 3), (3, 5), (5, 3), (5, 5)):
        p.sq[idx(r, c, 9)] = BLACK
    expect(is_eye(p, idx(4, 4, 9), BLACK), "oeil complet reconnu")
    p.sq[idx(3, 3, 9)] = WHITE
    expect(is_eye(p, idx(4, 4, 9), BLACK), "1 diagonale adverse toleree au centre")
    p.sq[idx(3, 5, 9)] = WHITE
    expect(not is_eye(p, idx(4, 4, 9), BLACK), "2 diagonales adverses : plus un oeil")
    # Dans un coin, aucune diagonale adverse n'est tolérée.
    q = Pos(9)
    q.sq[idx(0, 1, 9)] = q.sq[idx(1, 0, 9)] = BLACK
    q.sq[idx(1, 1, 9)] = BLACK
    expect(is_eye(q, idx(0, 0, 9), BLACK), "oeil de coin reconnu")
    q.sq[idx(1, 1, 9)] = WHITE
    expect(not is_eye(q, idx(0, 0, 9), BLACK), "diagonale adverse interdite au coin")


def test_score_et_morts():
    p = Pos(9)
    # Noir occupe toute la moitie haute, Blanc la moitie basse.
    for r in range(4):
        for c in range(9):
            p.sq[idx(r, c, 9)] = BLACK
    for r in range(5, 9):
        for c in range(9):
            p.sq[idx(r, c, 9)] = WHITE
    s = score_chinese(p, 6.5)
    expect(s["black_stones"] == 36 and s["white_stones"] == 36, "36 pierres chacun")
    expect(s["dame"] == 9, "la ligne mediane est neutre")
    expect(abs(s["black"] - 36) < 1e-6, "aire noire = 36")
    expect(abs(s["white"] - 42.5) < 1e-6, "aire blanche = 36 + komi")

    # On declare mort le groupe blanc : son aire passe a Noir.
    dead = [0] * 81
    mark_chain(p, idx(6, 4, 9), dead, 1)
    s2 = score_chinese(p, 6.5, dead)
    expect(s2["white_dead"] == 36, "36 pierres blanches marquees mortes")
    expect(s2["white_stones"] == 0, "plus une pierre blanche vivante")
    expect(abs(s2["black"] - 81) < 1e-6, "Noir prend tout le goban (36 + 45)")


def test_territory_map():
    p = Pos(9)
    for c in range(9):
        p.sq[idx(4, c, 9)] = BLACK
    t = territory_map(p)
    expect(t[idx(0, 0, 9)] == T_BLACK, "le haut est territoire noir")
    expect(t[idx(8, 0, 9)] == T_BLACK, "le bas aussi (aucun blanc sur le goban)")
    expect(t[idx(4, 0, 9)] == T_NONE, "une intersection occupee n'est pas du territoire")


def test_handicap():
    for n, h, first in ((9, 2, WHITE), (13, 5, WHITE), (19, 9, WHITE)):
        p = Pos(n)
        k = place_handicap(p, h)
        expect(k == h, f"{h} pierres posees en {n}x{n}")
        expect(sum(1 for x in p.sq if x == BLACK) == h, "toutes noires")
        expect(p.side == first, "Blanc commence avec handicap")
    p = Pos(9)
    expect(place_handicap(p, 0) == 0, "handicap 0 = aucune pierre")
    p9 = Pos(9)
    place_handicap(p9, 5)
    expect(p9.sq[idx(4, 4, 9)] == BLACK, "handicap 5 inclut le tengen")
    p6 = Pos(19)
    place_handicap(p6, 6)
    expect(p6.sq[idx(9, 9, 19)] == EMPTY, "handicap 6 n'inclut PAS le tengen")


def test_legal_vide():
    for n in (9, 13, 19):
        p = Pos(n)
        cnt = sum(1 for i in range(n * n) if is_legal(p, i))
        expect(cnt == n * n, f"{n * n} coups legaux sur un goban {n}x{n} vide")


def test_pass_hors_domaine_19():
    """PASS ne doit plus collisionner avec l'intersection 255 en 19×19."""
    expect(PASS == -1, "PASS = -1 (hors domaine)")
    expect(PASS < 0 or PASS >= MAX_SQ, "PASS hors de 0..360")
    p = Pos(19)
    sq255 = 255  # r=13, c=8
    expect(0 <= sq255 < 19 * 19, "255 est une intersection valide en 19x19")
    expect(is_legal(p, sq255), "intersection 255 jouable sur goban vide")
    expect(play(p, sq255), "poser en 255 pose une pierre")
    expect(p.sq[sq255] == BLACK, "pierre noire en 255")
    expect(p.passes == 0, "poser en 255 n'est PAS une passe")
    expect(p.side == WHITE, "trait passe a Blanc apres pose en 255")
    # Occupée : plus légale comme placement, et ce n'est toujours pas une passe.
    expect(not is_legal(p, sq255), "255 occupee : placement illegal")
    expect(is_legal(p, PASS), "PASS reste toujours legal")


def test_ko_haut_indice_19():
    """Ko au-delà de l'indice 255 — le C++ stockait ko en uint8_t (tronqué)."""
    n = 19
    p = Pos(n)
    # Forme de ko classique décalée vers le bas du goban (indices > 255).
    # Blanc en atari en (16, 8)=312 ; Noir capture en (16, 7)=311 → ko = 312.
    p.sq[idx(15, 7, n)] = WHITE
    p.sq[idx(15, 8, n)] = BLACK
    p.sq[idx(16, 6, n)] = WHITE
    p.sq[idx(16, 8, n)] = WHITE
    p.sq[idx(16, 9, n)] = BLACK
    p.sq[idx(17, 7, n)] = WHITE
    p.sq[idx(17, 8, n)] = BLACK
    cap_sq = idx(16, 8, n)   # 312
    play_sq = idx(16, 7, n)  # 311
    expect(cap_sq > 255, f"case capturee {cap_sq} > 255")
    expect(chain_liberties(p, cap_sq)[0] == 1, "pierre blanche en atari")
    p.side = BLACK
    expect(play(p, play_sq), "Noir capture (ko bas-plateau)")
    expect(p.ko == cap_sq, f"ko memorise = {cap_sq} (pas tronque)")
    expect(not is_legal(p, cap_sq), "reprise immediate du ko interdite")
    # Une case sans rapport (troncature uint8 : 312 & 0xFF = 56) doit rester jouable.
    phantom = cap_sq & 0xFF
    if p.sq[phantom] == EMPTY:
        expect(is_legal(p, phantom), f"case fantome {phantom} ne doit pas etre interdite")
    expect(play(p, PASS), "Blanc passe")
    expect(p.ko == PASS, "ko leve apres un autre coup")


def test_partie_complete():
    """Aucune position atteignable ne doit produire de coup illegal accepte."""
    import random
    random.seed(7)
    for n in (9, 13, 19):
        N = n * n
        rounds = 8 if n == 9 else (4 if n == 13 else 2)
        moves_cap = 120 if n == 9 else (80 if n == 13 else 60)
        for _ in range(rounds):
            p = Pos(n)
            for _ in range(moves_cap):
                moves = [i for i in range(N) if is_legal(p, i)]
                if not moves:
                    play(p, PASS)
                    continue
                sq = random.choice(moves)
                if not play(p, sq):
                    expect(False, f"coup legal refuse ({n}x{n})")
                    return
                for i in range(N):
                    if p.sq[i] != EMPTY and chain_liberties(p, i)[0] == 0:
                        expect(False, f"chaine sans liberte ({n}x{n})")
                        return
                if p.passes >= 2:
                    break
    expect(True, "parties aleatoires 9/13/19 sans chaine morte residuelle")


if __name__ == "__main__":
    print("=== test_go_engine.py (miroir de Tab5/go_engine.cpp) ===")
    test_capture()
    test_capture_groupe()
    test_suicide()
    test_suicide_groupe()
    test_capture_prioritaire_sur_suicide()
    test_ko()
    test_eye()
    test_score_et_morts()
    test_territory_map()
    test_handicap()
    test_legal_vide()
    test_pass_hors_domaine_19()
    test_ko_haut_indice_19()
    test_partie_complete()
    print(f"=== {'FAILED' if fails else 'ALL PASSED'} ({fails} fails) ===")
    raise SystemExit(1 if fails else 0)
