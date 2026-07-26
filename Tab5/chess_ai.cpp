/**
 * [AI-CONTEXT]
 * @file chess_ai.cpp
 * @role Jeu « Roi Noir » — implementation du moteur d'echecs (modele + recherche).
 * @architecture_constraint Zero LVGL, zero Home Assistant. Les tampons de coups
 *      sont GLOBAUX et indexes par ply (g_mbuf / g_sbuf) : la recherche descend a
 *      ~11 plies, un tableau de 220 coups par ply sur la PILE ferait exploser la
 *      stack de la tache ESPHome. Cout mesure (riscv32-esp-elf-size, -Os) :
 *      12,6 Ko de .text et 33,0 Ko de .bss pour cette unite de compilation.
 * @ai_instruction Le hot-path est negamax()/qsearch()/make()/unmake() : pas
 *      d'allocation, pas de std::, pas de float. Toute modification du generateur
 *      DOIT etre revalidee par perft_selftest() (valeurs FIDE en dur).
 */
#include "chess_ai.h"
#include "esphome.h"
#include <cstring>
#include <cstdio>

namespace Chess {

static const char* const TAG = "chess";

// ===========================================================================
// 1. Tables statiques
// ===========================================================================

// Les 64 cases valides de la mailbox 0x88, dans l'ordre a1..h8.
// Iterer dessus evite de parcourir les 128 entrees dont la moitie est hors jeu.
static uint8_t SQ64[64];

// Deplacements elementaires, en index 0x88.
static const int8_t OFF_KNIGHT[8] = { 33,  31,  18,  14, -14, -18, -31, -33};
static const int8_t OFF_BISHOP[4] = { 17,  15, -15, -17};
static const int8_t OFF_ROOK[4]   = { 16, -16,   1,  -1};
static const int8_t OFF_KING[8]   = { 17,  16,  15,   1,  -1, -15, -16, -17};

// Masque applique aux droits de roque a chaque coup :
// castling &= MASK[from] & MASK[to]. Un roi ou une tour qui bouge (ou une tour
// qui se fait capturer sur sa case d'origine) perd le droit correspondant.
static uint8_t CASTLE_MASK[128];

// Valeur materielle par type (centiemes de pion). Le roi vaut 0 : il est
// toujours present des deux cotes, l'inclure ne ferait que du bruit.
static const int PVAL[7] = {0, 100, 320, 330, 500, 900, 0};

// Tables piece/case, ecrites en ordre VISUEL (premiere ligne = rangee 8), ce qui
// les rend relisibles. La conversion se fait a l'evaluation : une piece blanche
// sur l'index a1-base `s64` lit PST[s64 ^ 56], une noire lit PST[s64].
static const int8_t PST_PAWN[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0,
};
static const int8_t PST_KNIGHT[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
static const int8_t PST_BISHOP[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
static const int8_t PST_ROOK[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0,
};
static const int8_t PST_QUEEN[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};
// Roi en milieu de partie : on le pousse a se cacher derriere ses pions (les
// cases g1/c1 valent plus que le centre).
static const int8_t PST_KING_MG[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20,
};
// Roi en finale : l'inverse, il doit monter au centre et soutenir ses pions.
static const int8_t PST_KING_EG[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50,
};

// Prime de pion passe, indexee par l'avancement (0 = rangee de depart).
static const int PASSED[8] = {0, 5, 10, 20, 35, 60, 90, 0};

// --- Zobrist ---------------------------------------------------------------
// 15 codes de piece x 64 cases (~7,7 Ko). Sert UNIQUEMENT a hash_of(), donc a la
// detection de repetition au niveau partie : jamais dans le hot-path.
static uint64_t Z_PIECE[15][64];
static uint64_t Z_CASTLE[16];
static uint64_t Z_EP[8];
static uint64_t Z_SIDE;

static bool g_inited = false;

// --- Tampons de coups partages, indexes par ply ----------------------------
// [AI-CONTEXT] 16 x 220 x (4 + 2) = ~20,6 Ko de .bss echanges contre zero
// pression sur la pile de la tache ESPHome.
// gen_moves() n'ecrit jamais plus de MAX_MOVES coups (218 est le maximum
// theorique atteignable dans une position legale).
static Move    g_mbuf[MAX_PLY_BUF][MAX_MOVES];
static int16_t g_sbuf[MAX_PLY_BUF][MAX_MOVES];
static Move    g_killer[MAX_PLY_BUF][2];
// Tampons dedies a la generation SAN (appelee depuis l'UI, jamais depuis la
// recherche) : evite 1,7 Ko de pile a chaque coup joue.
static Move    g_sanbuf[MAX_MOVES];

// --- Etat partage de la recherche (garde-fou temporel) ---------------------
// Volontairement separes de SearchState : search_quick() (indice) doit pouvoir
// les sauver/restaurer sans copier les ~2,7 Ko de la structure complete.
static bool     g_abort    = false;
static uint32_t g_deadline = 0;
static uint32_t g_nodes    = 0;
static int      g_qdepth   = 0;

const AiLevel AI_LEVELS[AI_NLEVELS] = {
    // nom         description                                    prof  q  budget fen. elo
    {"Pion",     "Debutant : ne voit qu'un coup, se trompe",        1,  0,  120, 160,  600},
    {"Cavalier", "Voit les prises simples et les repond",           2,  0,  350,  60,  900},
    {"Fou",      "Calcule 3 coups + les prises en chaine",          3,  4,  800,  20, 1250},
    {"Dame",     "Calcule 4 coups, tactique correcte",              4,  6, 1800,   0, 1600},
    {"Roi",      "Effort maximal du Tab (5 coups vises)",           5,  6, 3500,   0, 1900},
};

// ===========================================================================
// 2. Utilitaires
// ===========================================================================

static inline int iabs(int v) { return v < 0 ? -v : v; }

// xorshift64 : suffisant pour des clefs Zobrist et pour le tirage des coups.
static uint64_t rnd64(uint64_t& s) {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return s;
}

static uint32_t g_rng = 0x2545F491u;
static inline uint32_t rnd32() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}

// Index 0..63 (a1 = 0) a partir d'un index 0x88.
static inline int to64(uint8_t sq) { return ((sq >> 4) << 3) | (sq & 7); }

void init() {
    if (g_inited) return;
    int k = 0;
    for (int r = 0; r < 8; r++)
        for (int f = 0; f < 8; f++) SQ64[k++] = (uint8_t)((r << 4) | f);

    uint64_t s = 0x9E3779B97F4A7C15ull;
    for (int p = 0; p < 15; p++)
        for (int i = 0; i < 64; i++) Z_PIECE[p][i] = rnd64(s);
    for (int i = 0; i < 16; i++) Z_CASTLE[i] = rnd64(s);
    for (int i = 0; i < 8; i++)  Z_EP[i] = rnd64(s);
    Z_SIDE = rnd64(s);

    for (int i = 0; i < 128; i++) CASTLE_MASK[i] = 0x0F;
    CASTLE_MASK[0x00] = (uint8_t) ~CR_WQ;              // a1 : tour dame blanche
    CASTLE_MASK[0x07] = (uint8_t) ~CR_WK;              // h1 : tour roi blanche
    CASTLE_MASK[0x04] = (uint8_t) ~(CR_WK | CR_WQ);    // e1 : roi blanc
    CASTLE_MASK[0x70] = (uint8_t) ~CR_BQ;              // a8
    CASTLE_MASK[0x77] = (uint8_t) ~CR_BK;              // h8
    CASTLE_MASK[0x74] = (uint8_t) ~(CR_BK | CR_BQ);    // e8

    g_inited = true;
}

uint64_t hash_of(const Position& p) {
    uint64_t h = 0;
    for (int i = 0; i < 64; i++) {
        uint8_t pc = p.board[SQ64[i]];
        if (pc) h ^= Z_PIECE[pc & 0x0F][i];
    }
    h ^= Z_CASTLE[p.castling & 0x0F];
    if (p.ep != NO_SQ) h ^= Z_EP[p.ep & 7];
    if (p.side == BLACK) h ^= Z_SIDE;
    return h;
}

// ===========================================================================
// 3. FEN
// ===========================================================================

void set_start(Position& p) {
    set_fen(p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

static uint8_t char_to_piece(char c) {
    switch (c) {
        case 'P': return WHITE | PAWN;   case 'p': return BLACK | PAWN;
        case 'N': return WHITE | KNIGHT; case 'n': return BLACK | KNIGHT;
        case 'B': return WHITE | BISHOP; case 'b': return BLACK | BISHOP;
        case 'R': return WHITE | ROOK;   case 'r': return BLACK | ROOK;
        case 'Q': return WHITE | QUEEN;  case 'q': return BLACK | QUEEN;
        case 'K': return WHITE | KING;   case 'k': return BLACK | KING;
        default:  return 0xFF;
    }
}

static char piece_to_char(uint8_t pc) {
    static const char W[7] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};
    static const char B[7] = {' ', 'p', 'n', 'b', 'r', 'q', 'k'};
    return (pc & COLOR_MASK) ? B[pc & TYPE_MASK] : W[pc & TYPE_MASK];
}

bool set_fen(Position& p, const char* fen) {
    init();
    if (!fen) return false;
    memset(p.board, EMPTY, sizeof(p.board));
    p.side = WHITE; p.castling = 0; p.ep = NO_SQ;
    p.halfmove = 0; p.fullmove = 1;
    p.king_sq[0] = NO_SQ; p.king_sq[1] = NO_SQ;

    const char* s = fen;
    int rank = 7, file = 0;
    for (; *s && *s != ' '; s++) {
        if (*s == '/') { rank--; file = 0; if (rank < 0) return false; continue; }
        if (*s >= '1' && *s <= '8') { file += *s - '0'; continue; }
        uint8_t pc = char_to_piece(*s);
        if (pc == 0xFF || file > 7 || rank < 0) return false;
        uint8_t sq = (uint8_t)((rank << 4) | file);
        p.board[sq] = pc;
        if ((pc & TYPE_MASK) == KING) p.king_sq[cidx(pc & COLOR_MASK)] = sq;
        file++;
    }
    if (p.king_sq[0] == NO_SQ || p.king_sq[1] == NO_SQ) return false;

    while (*s == ' ') s++;
    p.side = (*s == 'b') ? BLACK : WHITE;
    while (*s && *s != ' ') s++;
    while (*s == ' ') s++;

    for (; *s && *s != ' '; s++) {
        switch (*s) {
            case 'K': p.castling |= CR_WK; break;
            case 'Q': p.castling |= CR_WQ; break;
            case 'k': p.castling |= CR_BK; break;
            case 'q': p.castling |= CR_BQ; break;
            default: break;   // '-' ou notation Chess960 ignoree
        }
    }
    while (*s == ' ') s++;

    if (*s && *s != '-' && *s >= 'a' && *s <= 'h') {
        int f = *s - 'a';
        int r = (s[1] >= '1' && s[1] <= '8') ? (s[1] - '1') : -1;
        if (r >= 0) p.ep = (uint8_t)((r << 4) | f);
    }
    while (*s && *s != ' ') s++;
    while (*s == ' ') s++;

    if (*s) {
        int hm = 0;
        while (*s >= '0' && *s <= '9') { hm = hm * 10 + (*s - '0'); s++; }
        p.halfmove = (uint8_t)(hm > 200 ? 200 : hm);
        while (*s == ' ') s++;
        int fm = 0;
        while (*s >= '0' && *s <= '9') { fm = fm * 10 + (*s - '0'); s++; }
        p.fullmove = (uint16_t)(fm > 0 ? fm : 1);
    }
    return true;
}

void get_fen(const Position& p, char* out, int cap) {
    if (!out || cap < 24) return;
    int k = 0;
    for (int r = 7; r >= 0; r--) {
        int run = 0;
        for (int f = 0; f < 8; f++) {
            uint8_t pc = p.board[(r << 4) | f];
            if (!pc) { run++; continue; }
            if (run) { if (k < cap - 1) out[k++] = (char)('0' + run); run = 0; }
            if (k < cap - 1) out[k++] = piece_to_char(pc);
        }
        if (run && k < cap - 1) out[k++] = (char)('0' + run);
        if (r > 0 && k < cap - 1) out[k++] = '/';
    }
    char tail[32];
    char cs[6]; int c = 0;
    if (p.castling & CR_WK) cs[c++] = 'K';
    if (p.castling & CR_WQ) cs[c++] = 'Q';
    if (p.castling & CR_BK) cs[c++] = 'k';
    if (p.castling & CR_BQ) cs[c++] = 'q';
    if (!c) cs[c++] = '-';
    cs[c] = 0;
    char ep[4];
    if (p.ep == NO_SQ) { ep[0] = '-'; ep[1] = 0; }
    else { ep[0] = (char)('a' + (p.ep & 7)); ep[1] = (char)('1' + (p.ep >> 4)); ep[2] = 0; }
    snprintf(tail, sizeof(tail), " %c %s %s %u %u",
             p.side == WHITE ? 'w' : 'b', cs, ep,
             (unsigned) p.halfmove, (unsigned) p.fullmove);
    for (int i = 0; tail[i] && k < cap - 1; i++) out[k++] = tail[i];
    out[k] = 0;
}

// ===========================================================================
// 4. Attaques
// ===========================================================================

bool attacked(const Position& p, uint8_t sq, uint8_t by) {
    // --- Pions : un pion blanc en s attaque s+15 et s+17, donc `sq` est
    //     attaquee par un blanc si un pion blanc occupe sq-15 ou sq-17.
    if (by == WHITE) {
        int a = (int) sq - 17, b = (int) sq - 15;
        if (!(a & 0x88) && p.board[a] == (WHITE | PAWN)) return true;
        if (!(b & 0x88) && p.board[b] == (WHITE | PAWN)) return true;
    } else {
        int a = (int) sq + 17, b = (int) sq + 15;
        if (!(a & 0x88) && p.board[a] == (BLACK | PAWN)) return true;
        if (!(b & 0x88) && p.board[b] == (BLACK | PAWN)) return true;
    }
    const uint8_t kn = (uint8_t)(by | KNIGHT), kg = (uint8_t)(by | KING);
    for (int i = 0; i < 8; i++) {
        int t = (int) sq + OFF_KNIGHT[i];
        if (!(t & 0x88) && p.board[t] == kn) return true;
    }
    for (int i = 0; i < 8; i++) {
        int t = (int) sq + OFF_KING[i];
        if (!(t & 0x88) && p.board[t] == kg) return true;
    }
    for (int i = 0; i < 4; i++) {          // diagonales : fou / dame
        int t = (int) sq;
        for (;;) {
            t += OFF_BISHOP[i];
            if (t & 0x88) break;
            uint8_t q = p.board[t];
            if (!q) continue;
            if ((q & COLOR_MASK) == by) {
                uint8_t ty = q & TYPE_MASK;
                if (ty == BISHOP || ty == QUEEN) return true;
            }
            break;
        }
    }
    for (int i = 0; i < 4; i++) {          // lignes / colonnes : tour / dame
        int t = (int) sq;
        for (;;) {
            t += OFF_ROOK[i];
            if (t & 0x88) break;
            uint8_t q = p.board[t];
            if (!q) continue;
            if ((q & COLOR_MASK) == by) {
                uint8_t ty = q & TYPE_MASK;
                if (ty == ROOK || ty == QUEEN) return true;
            }
            break;
        }
    }
    return false;
}

bool in_check(const Position& p, uint8_t color) {
    return attacked(p, p.king_sq[cidx(color)], (uint8_t)(color ^ COLOR_MASK));
}

// ===========================================================================
// 5. Generation de coups
// ===========================================================================

static inline void add_move(Move* out, int& n, uint8_t from, uint8_t to, uint8_t promo, uint8_t flags) {
    if (n >= MAX_MOVES) return;
    out[n].from = from; out[n].to = to; out[n].promo = promo; out[n].flags = flags;
    n++;
}

// [FR] PROMOTION : un pion qui atteint la derniere rangee genere QUATRE coups
// distincts (dame, tour, fou, cavalier). Le sous-promotion en cavalier est le
// seul qui compte vraiment en pratique (echec a la decouverte), mais les quatre
// sont generes pour rester conforme aux regles FIDE et au perft.
static inline void add_promos(Move* out, int& n, uint8_t from, uint8_t to, uint8_t base_flags) {
    const uint8_t f = (uint8_t)(base_flags | MF_PROMO);
    add_move(out, n, from, to, QUEEN,  f);
    add_move(out, n, from, to, ROOK,   f);
    add_move(out, n, from, to, BISHOP, f);
    add_move(out, n, from, to, KNIGHT, f);
}

// Generateur unique : `caps_only` restreint aux captures et promotions
// (quiescence). Un seul corps de code = un seul endroit ou se tromper.
static int gen_impl(const Position& p, Move* out, bool caps_only) {
    int n = 0;
    const uint8_t us = p.side, them = (uint8_t)(us ^ COLOR_MASK);

    for (int i = 0; i < 64; i++) {
        const uint8_t sq = SQ64[i];
        const uint8_t pc = p.board[sq];
        if (!pc || (pc & COLOR_MASK) != us) continue;
        const uint8_t t = pc & TYPE_MASK;

        if (t == PAWN) {
            const int fwd        = (us == WHITE) ? 16 : -16;
            const int start_rank = (us == WHITE) ? 1 : 6;
            const int promo_rank = (us == WHITE) ? 7 : 0;

            const int to = (int) sq + fwd;
            if (!(to & 0x88) && p.board[to] == EMPTY) {
                if ((to >> 4) == promo_rank) {
                    add_promos(out, n, sq, (uint8_t) to, 0);
                } else if (!caps_only) {
                    add_move(out, n, sq, (uint8_t) to, 0, 0);
                    const int to2 = (int) sq + 2 * fwd;
                    if ((sq >> 4) == start_rank && !(to2 & 0x88) && p.board[to2] == EMPTY)
                        add_move(out, n, sq, (uint8_t) to2, 0, MF_DOUBLE);
                }
            }
            for (int d = 0; d < 2; d++) {
                const int c = (int) sq + fwd + (d ? 1 : -1);
                if (c & 0x88) continue;
                const uint8_t tp = p.board[c];
                if (tp && (tp & COLOR_MASK) == them) {
                    if ((c >> 4) == promo_rank) add_promos(out, n, sq, (uint8_t) c, MF_CAPTURE);
                    else                        add_move(out, n, sq, (uint8_t) c, 0, MF_CAPTURE);
                } else if (tp == EMPTY && p.ep != NO_SQ && c == (int) p.ep) {
                    // [FR] PRISE EN PASSANT : le pion adverse vient d'avancer de
                    // deux cases et passe a cote du notre ; on capture « au vol »
                    // sur la case qu'il a survolee. Le pion mange n'est donc PAS
                    // sur la case d'arrivee — voir make(), drapeau MF_EP.
                    add_move(out, n, sq, (uint8_t) c, 0, (uint8_t)(MF_CAPTURE | MF_EP));
                }
            }
            continue;
        }

        if (t == KNIGHT || t == KING) {
            const int8_t* off = (t == KNIGHT) ? OFF_KNIGHT : OFF_KING;
            for (int k = 0; k < 8; k++) {
                const int to = (int) sq + off[k];
                if (to & 0x88) continue;
                const uint8_t tp = p.board[to];
                if (tp && (tp & COLOR_MASK) == us) continue;
                if (caps_only && !tp) continue;
                add_move(out, n, sq, (uint8_t) to, 0, (uint8_t)(tp ? MF_CAPTURE : 0));
            }
            continue;
        }

        // Fou / tour / dame : glissement jusqu'a l'obstacle.
        const int8_t* off; int nd;
        if (t == BISHOP)    { off = OFF_BISHOP; nd = 4; }
        else if (t == ROOK) { off = OFF_ROOK;   nd = 4; }
        else                { off = OFF_KING;   nd = 8; }
        for (int k = 0; k < nd; k++) {
            int to = (int) sq;
            for (;;) {
                to += off[k];
                if (to & 0x88) break;
                const uint8_t tp = p.board[to];
                if (!tp) {
                    if (!caps_only) add_move(out, n, sq, (uint8_t) to, 0, 0);
                    continue;
                }
                if ((tp & COLOR_MASK) == them) add_move(out, n, sq, (uint8_t) to, 0, MF_CAPTURE);
                break;
            }
        }
    }

    // [FR] ROQUE : seul coup ou deux pieces bougent. Conditions FIDE verifiees
    // ici, donc un coup de roque genere est deja legal cote « traversee » :
    //   1. le droit n'a pas ete perdu (roi et tour jamais deplaces) ;
    //   2. les cases entre le roi et la tour sont vides ;
    //   3. le roi n'est ni en echec, ni ne TRAVERSE ni n'ARRIVE sur une case
    //      attaquee. La case traversee par la TOUR (b1/b8) peut, elle, l'etre.
    // Les tests de piece (roi/tour bien en place) protegent d'un FEN incoherent.
    if (!caps_only) {
        if (us == WHITE && p.board[0x04] == (WHITE | KING)) {
            if ((p.castling & CR_WK) && p.board[0x07] == (WHITE | ROOK) &&
                p.board[0x05] == EMPTY && p.board[0x06] == EMPTY &&
                !attacked(p, 0x04, them) && !attacked(p, 0x05, them) && !attacked(p, 0x06, them))
                add_move(out, n, 0x04, 0x06, 0, MF_CASTLE);
            if ((p.castling & CR_WQ) && p.board[0x00] == (WHITE | ROOK) &&
                p.board[0x03] == EMPTY && p.board[0x02] == EMPTY && p.board[0x01] == EMPTY &&
                !attacked(p, 0x04, them) && !attacked(p, 0x03, them) && !attacked(p, 0x02, them))
                add_move(out, n, 0x04, 0x02, 0, MF_CASTLE);
        } else if (us == BLACK && p.board[0x74] == (BLACK | KING)) {
            if ((p.castling & CR_BK) && p.board[0x77] == (BLACK | ROOK) &&
                p.board[0x75] == EMPTY && p.board[0x76] == EMPTY &&
                !attacked(p, 0x74, them) && !attacked(p, 0x75, them) && !attacked(p, 0x76, them))
                add_move(out, n, 0x74, 0x76, 0, MF_CASTLE);
            if ((p.castling & CR_BQ) && p.board[0x70] == (BLACK | ROOK) &&
                p.board[0x73] == EMPTY && p.board[0x72] == EMPTY && p.board[0x71] == EMPTY &&
                !attacked(p, 0x74, them) && !attacked(p, 0x73, them) && !attacked(p, 0x72, them))
                add_move(out, n, 0x74, 0x72, 0, MF_CASTLE);
        }
    }
    return n;
}

int gen_moves(const Position& p, Move* out)    { return gen_impl(p, out, false); }
int gen_captures(const Position& p, Move* out) { return gen_impl(p, out, true);  }

int gen_legal(const Position& p, Move* out) {
    Position q = p;
    const int n = gen_impl(q, out, false);
    int k = 0;
    for (int i = 0; i < n; i++) {
        Undo u;
        if (make(q, out[i], u)) { unmake(q, out[i], u); out[k++] = out[i]; }
    }
    return k;
}

// ===========================================================================
// 6. make / unmake
// ===========================================================================

bool make(Position& p, const Move& m, Undo& u) {
    const uint8_t mover = p.side;
    const uint8_t pc    = p.board[m.from];

    u.castling = p.castling;
    u.ep       = p.ep;
    u.halfmove = p.halfmove;
    u.captured = EMPTY;
    u.cap_sq   = NO_SQ;

    if (m.flags & MF_EP) {
        // [FR] La victime d'une prise en passant est sur la colonne d'arrivee
        // mais sur la rangee de DEPART du pion capturant.
        const uint8_t vic = (uint8_t)((m.from & 0xF0) | (m.to & 0x0F));
        u.captured = p.board[vic];
        u.cap_sq   = vic;
        p.board[vic] = EMPTY;
    } else if (p.board[m.to] != EMPTY) {
        u.captured = p.board[m.to];
        u.cap_sq   = m.to;
    }

    p.board[m.to]   = pc;
    p.board[m.from] = EMPTY;
    if (m.flags & MF_PROMO) p.board[m.to] = (uint8_t)(mover | m.promo);

    if (m.flags & MF_CASTLE) {
        // [FR] Deplacement de la tour, ecrit en clair pour les 4 cas.
        switch (m.to) {
            case 0x06: p.board[0x05] = p.board[0x07]; p.board[0x07] = EMPTY; break; // petit roque blanc
            case 0x02: p.board[0x03] = p.board[0x00]; p.board[0x00] = EMPTY; break; // grand roque blanc
            case 0x76: p.board[0x75] = p.board[0x77]; p.board[0x77] = EMPTY; break; // petit roque noir
            case 0x72: p.board[0x73] = p.board[0x70]; p.board[0x70] = EMPTY; break; // grand roque noir
            default: break;
        }
    }

    if ((pc & TYPE_MASK) == KING) p.king_sq[cidx(mover)] = m.to;
    p.castling &= (uint8_t)(CASTLE_MASK[m.from] & CASTLE_MASK[m.to]);

    // [FR] La case de prise en passant n'est posee que si un pion adverse peut
    // REELLEMENT capturer. Sans ce filtre, deux positions identiques du point de
    // vue du joueur auraient des clefs differentes et la triple repetition
    // passerait a cote.
    p.ep = NO_SQ;
    if (m.flags & MF_DOUBLE) {
        const uint8_t opp_pawn = (uint8_t)((mover ^ COLOR_MASK) | PAWN);
        const int l = (int) m.to - 1, r = (int) m.to + 1;
        const bool capturable = (!(l & 0x88) && p.board[l] == opp_pawn) ||
                                (!(r & 0x88) && p.board[r] == opp_pawn);
        if (capturable) p.ep = (uint8_t)(((int) m.from + (int) m.to) >> 1);
    }

    p.halfmove = ((pc & TYPE_MASK) == PAWN || u.captured != EMPTY)
               ? 0 : (uint8_t)(p.halfmove < 200 ? p.halfmove + 1 : 200);
    if (mover == BLACK) p.fullmove++;
    p.side = (uint8_t)(mover ^ COLOR_MASK);

    // [FR] Filtre de legalite : la generation est pseudo-legale, c'est ICI qu'on
    // rejette les coups qui laissent (ou mettent) son propre roi en prise.
    if (attacked(p, p.king_sq[cidx(mover)], p.side)) {
        unmake(p, m, u);
        return false;
    }
    return true;
}

void unmake(Position& p, const Move& m, const Undo& u) {
    const uint8_t mover = (uint8_t)(p.side ^ COLOR_MASK);
    uint8_t pc = p.board[m.to];
    if (m.flags & MF_PROMO) pc = (uint8_t)(mover | PAWN);

    p.board[m.from] = pc;
    p.board[m.to]   = EMPTY;
    if (u.captured != EMPTY) p.board[u.cap_sq] = u.captured;

    if (m.flags & MF_CASTLE) {
        switch (m.to) {
            case 0x06: p.board[0x07] = p.board[0x05]; p.board[0x05] = EMPTY; break;
            case 0x02: p.board[0x00] = p.board[0x03]; p.board[0x03] = EMPTY; break;
            case 0x76: p.board[0x77] = p.board[0x75]; p.board[0x75] = EMPTY; break;
            case 0x72: p.board[0x70] = p.board[0x73]; p.board[0x73] = EMPTY; break;
            default: break;
        }
    }

    if ((pc & TYPE_MASK) == KING) p.king_sq[cidx(mover)] = m.from;

    p.castling = u.castling;
    p.ep       = u.ep;
    p.halfmove = u.halfmove;
    if (mover == BLACK && p.fullmove > 1) p.fullmove--;
    p.side = mover;
}

// ===========================================================================
// 7. Evaluation
// ===========================================================================

bool insufficient_material(const Position& p) {
    int minors[2] = {0, 0};      // cavaliers + fous
    int bishops[2] = {0, 0};
    int bsq_color[2] = {-1, -1}; // couleur de case du (dernier) fou
    for (int i = 0; i < 64; i++) {
        const uint8_t sq = SQ64[i];
        const uint8_t pc = p.board[sq];
        if (!pc) continue;
        const uint8_t t = pc & TYPE_MASK;
        if (t == PAWN || t == ROOK || t == QUEEN) return false;   // mat toujours possible
        if (t == KING) continue;
        const int c = cidx(pc & COLOR_MASK);
        minors[c]++;
        if (t == BISHOP) { bishops[c]++; bsq_color[c] = ((sq >> 4) + (sq & 7)) & 1; }
    }
    // R/R, R+mineure/R : nulle immediate.
    if (minors[0] + minors[1] <= 1) return true;
    // R+F/R+F avec les deux fous sur des cases de meme couleur : nulle.
    if (minors[0] == 1 && minors[1] == 1 && bishops[0] == 1 && bishops[1] == 1 &&
        bsq_color[0] == bsq_color[1]) return true;
    // [AI-CONTEXT] Limitation assumee : R+C+C/R n'est PAS declare nul (le mat y
    // est possible avec une aide adverse), conformement aux regles FIDE.
    return false;
}

int eval(const Position& p) {
    int sc = 0;                  // toujours du point de vue des BLANCS
    int npm[2]  = {0, 0};        // materiel hors pions et roi
    int nb[2]   = {0, 0};        // nombre de fous
    int pc_f[2][8];              // pions par colonne
    int pmax[2][8], pmin[2][8];  // rangee la plus avancee / la moins avancee
    for (int c = 0; c < 2; c++)
        for (int f = 0; f < 8; f++) { pc_f[c][f] = 0; pmax[c][f] = -1; pmin[c][f] = 8; }

    // --- Passe 1 : materiel et structure de pions -------------------------
    for (int i = 0; i < 64; i++) {
        const uint8_t sq = SQ64[i];
        const uint8_t pc = p.board[sq];
        if (!pc) continue;
        const int c = cidx(pc & COLOR_MASK);
        const int t = pc & TYPE_MASK;
        if (t == PAWN) {
            const int f = sq & 7, r = sq >> 4;
            pc_f[c][f]++;
            if (r > pmax[c][f]) pmax[c][f] = r;
            if (r < pmin[c][f]) pmin[c][f] = r;
        } else if (t != KING) {
            npm[c] += PVAL[t];
            if (t == BISHOP) nb[c]++;
        }
    }
    const bool eg = (npm[0] <= 1300 && npm[1] <= 1300);

    // --- Passe 2 : materiel + tables + bonus positionnels -----------------
    for (int i = 0; i < 64; i++) {
        const uint8_t sq = SQ64[i];
        const uint8_t pc = p.board[sq];
        if (!pc) continue;
        const int c = cidx(pc & COLOR_MASK);
        const int t = pc & TYPE_MASK;
        const int s64  = to64(sq);
        const int vidx = (c == 0) ? (s64 ^ 56) : s64;   // index visuel (rangee 8 en tete)
        int v = PVAL[t];
        switch (t) {
            case PAWN:   v += PST_PAWN[vidx];   break;
            case KNIGHT: v += PST_KNIGHT[vidx]; break;
            case BISHOP: v += PST_BISHOP[vidx]; break;
            case ROOK:   v += PST_ROOK[vidx];   break;
            case QUEEN:  v += PST_QUEEN[vidx];  break;
            default:     v += eg ? PST_KING_EG[vidx] : PST_KING_MG[vidx]; break;
        }
        const int f = sq & 7, r = sq >> 4;
        if (t == PAWN) {
            if (pc_f[c][f] > 1) v -= 12;                                             // double
            if ((f == 0 || pc_f[c][f - 1] == 0) && (f == 7 || pc_f[c][f + 1] == 0))
                v -= 14;                                                             // isole
            const int o = c ^ 1;
            bool passed = true;
            for (int df = -1; df <= 1 && passed; df++) {
                const int ff = f + df;
                if (ff < 0 || ff > 7) continue;
                if (c == 0) { if (pmax[o][ff] > r) passed = false; }
                else        { if (pmin[o][ff] < r) passed = false; }
            }
            if (passed) v += PASSED[(c == 0) ? r : (7 - r)];
        } else if (t == ROOK) {
            if (pc_f[c][f] == 0) v += (pc_f[c ^ 1][f] == 0) ? 18 : 9;   // colonne ouverte / semi
        }
        sc += (c == 0) ? v : -v;
    }

    if (nb[0] >= 2) sc += 30;   // paire de fous
    if (nb[1] >= 2) sc -= 30;

    // --- Securite du roi : bouclier de pions (milieu de partie seulement) --
    if (!eg) {
        for (int c = 0; c < 2; c++) {
            const uint8_t k = p.king_sq[c];
            const int kf = k & 7, kr = k >> 4;
            const int dir = (c == 0) ? 1 : -1;
            const uint8_t own_pawn = (uint8_t)((c ? BLACK : WHITE) | PAWN);
            int shield = 0;
            for (int df = -1; df <= 1; df++) {
                const int ff = kf + df;
                if (ff < 0 || ff > 7) continue;
                const int s = (kr + dir) * 16 + ff;
                if (s < 0 || s > 127 || (s & 0x88)) continue;
                if (p.board[s] == own_pawn) shield++;
            }
            const int bonus = shield * 10 - 12;
            sc += (c == 0) ? bonus : -bonus;
        }
    }

    sc += (p.side == WHITE) ? 10 : -10;              // tempo
    return (p.side == WHITE) ? sc : -sc;             // convention negamax
}

// ===========================================================================
// 8. Perft — preuve du generateur
// ===========================================================================

uint64_t perft(Position& p, int depth) {
    init();
    if (depth <= 0) return 1;
    if (depth >= MAX_PLY_BUF) depth = MAX_PLY_BUF - 1;
    Move* mv = g_mbuf[depth];
    const int n = gen_impl(p, mv, false);
    uint64_t total = 0;
    for (int i = 0; i < n; i++) {
        Undo u;
        if (!make(p, mv[i], u)) continue;
        total += (depth == 1) ? 1u : perft(p, depth - 1);
        unmake(p, mv[i], u);
    }
    return total;
}

bool perft_selftest(int depth) {
    init();
    // Valeurs de reference FIDE pour la position initiale (litterature echiquenne).
    static const uint64_t REF[6] = {1ull, 20ull, 400ull, 8902ull, 197281ull, 4865609ull};
    if (depth < 1) depth = 1;
    if (depth > 5) depth = 5;
    bool ok = true;
    Position p;
    set_start(p);
    for (int d = 1; d <= depth; d++) {
        const uint32_t t0 = esphome::millis();
        const uint64_t got = perft(p, d);
        const uint32_t ms  = esphome::millis() - t0;
        const bool good = (got == REF[d]);
        if (!good) ok = false;
        ESP_LOGI(TAG, "perft(%d) = %llu (attendu %llu) %s — %u ms",
                 d, (unsigned long long) got, (unsigned long long) REF[d],
                 good ? "OK" : "ECHEC", (unsigned) ms);
    }
    ESP_LOGI(TAG, "perft_selftest : %s", ok ? "generateur VALIDE" : "GENERATEUR FAUX");
    return ok;
}

// ===========================================================================
// 9. Notation
// ===========================================================================

// Lettres francaises : Roi, Dame, Tour, Fou, Cavalier. Le pion n'a pas de lettre.
static char piece_letter_fr(uint8_t t) {
    switch (t) {
        case KING:   return 'R';
        case QUEEN:  return 'D';
        case ROOK:   return 'T';
        case BISHOP: return 'F';
        case KNIGHT: return 'C';
        default:     return 0;
    }
}

void move_to_uci(const Move& m, char* out, int cap) {
    if (!out || cap < 6) return;
    int k = 0;
    out[k++] = (char)('a' + (m.from & 7));
    out[k++] = (char)('1' + (m.from >> 4));
    out[k++] = (char)('a' + (m.to & 7));
    out[k++] = (char)('1' + (m.to >> 4));
    if (m.flags & MF_PROMO) {
        const char c = piece_letter_fr(m.promo);
        out[k++] = c ? (char)(c | 0x20) : 'd';
    }
    out[k] = 0;
}

void move_to_san(const Position& before, const Move& m, char* out, int cap) {
    if (!out || cap < 12) return;
    init();
    int k = 0;
    const uint8_t pc = before.board[m.from];
    const uint8_t t  = pc & TYPE_MASK;

    if (m.flags & MF_CASTLE) {
        // [FR] Petit roque = colonne g, grand roque = colonne c.
        const bool king_side = ((m.to & 7) == 6);
        const char* s = king_side ? "O-O" : "O-O-O";
        while (*s && k < cap - 6) out[k++] = *s++;
    } else {
        const char L = piece_letter_fr(t);
        if (L) {
            out[k++] = L;
            // Desambiguisation : une autre piece du meme type peut-elle aller
            // sur la meme case ? Si oui on precise la colonne, sinon la rangee,
            // sinon les deux (cas des 3 dames apres promotions).
            const int n = gen_legal(before, g_sanbuf);
            bool need = false, same_file = false, same_rank = false;
            for (int i = 0; i < n; i++) {
                const Move& o = g_sanbuf[i];
                if (o.to != m.to || o.from == m.from) continue;
                if (before.board[o.from] != pc) continue;
                need = true;
                if ((o.from & 7) == (m.from & 7)) same_file = true;
                if ((o.from >> 4) == (m.from >> 4)) same_rank = true;
            }
            if (need) {
                if (!same_file) out[k++] = (char)('a' + (m.from & 7));
                else if (!same_rank) out[k++] = (char)('1' + (m.from >> 4));
                else { out[k++] = (char)('a' + (m.from & 7)); out[k++] = (char)('1' + (m.from >> 4)); }
            }
            if (m.flags & MF_CAPTURE) out[k++] = 'x';
        } else if (m.flags & MF_CAPTURE) {
            // Capture de pion : on prefixe par la colonne de depart (« exd5 »).
            out[k++] = (char)('a' + (m.from & 7));
            out[k++] = 'x';
        }
        out[k++] = (char)('a' + (m.to & 7));
        out[k++] = (char)('1' + (m.to >> 4));
        if (m.flags & MF_PROMO) { out[k++] = '='; out[k++] = piece_letter_fr(m.promo); }
    }

    // Suffixe echec (+) ou mat (#).
    Position q = before;
    Undo u;
    if (make(q, m, u)) {
        if (in_check(q, q.side)) {
            const int nl = gen_legal(q, g_sanbuf);
            if (k < cap - 1) out[k++] = (nl == 0) ? '#' : '+';
        }
        unmake(q, m, u);
    }
    out[k] = 0;
}

// ===========================================================================
// 10. Recherche
// ===========================================================================

// Valeurs MVV/LVA compressees (score de tri sur int16_t).
static const int16_t MVV[7] = {0, 10, 32, 33, 50, 90, 200};

static void score_moves(const Position& p, const Move* mv, int16_t* sc, int n, int ply) {
    for (int i = 0; i < n; i++) {
        const Move& m = mv[i];
        int s = 0;
        if (m.flags & MF_PROMO) {
            s = 28000 + MVV[m.promo];
        } else if (m.flags & MF_CAPTURE) {
            const uint8_t vic = (m.flags & MF_EP) ? (uint8_t) PAWN
                                                  : (uint8_t)(p.board[m.to] & TYPE_MASK);
            const uint8_t att = p.board[m.from] & TYPE_MASK;
            s = 20000 + MVV[vic] * 8 - MVV[att];      // victime chere / attaquant bon marche
        } else if (move_eq(m, g_killer[ply][0])) {
            s = 15000;
        } else if (move_eq(m, g_killer[ply][1])) {
            s = 14000;
        }
        sc[i] = (int16_t) s;
    }
}

// Tri par selection incrementale : on ne trie que ce qu'on consomme (une coupe
// alpha-beta rend inutile le tri du reste).
static inline void pick_best(Move* mv, int16_t* sc, int n, int i) {
    int best = i;
    for (int j = i + 1; j < n; j++) if (sc[j] > sc[best]) best = j;
    if (best != i) {
        const Move tm = mv[i]; mv[i] = mv[best]; mv[best] = tm;
        const int16_t ts = sc[i]; sc[i] = sc[best]; sc[best] = ts;
    }
}

static int qsearch(Position& p, int alpha, int beta, int qd, int ply);

static int negamax(Position& p, int depth, int alpha, int beta, int ply) {
    if (g_abort) return 0;
    // Garde-fou temporel : teste tous les 512 nœuds pour ne pas payer esphome::millis()
    // a chaque appel (l'appel systeme domine sinon le cout du nœud).
    if ((++g_nodes & 511u) == 0u && esphome::millis() >= g_deadline) { g_abort = true; return 0; }
    if (ply >= MAX_PLY_BUF - 2) return eval(p);
    if (depth <= 0) return qsearch(p, alpha, beta, g_qdepth, ply);
    if (p.halfmove >= 100) return 0;      // regle des 50 coups

    Move*    mv = g_mbuf[ply];
    int16_t* sc = g_sbuf[ply];
    const int n = gen_impl(p, mv, false);
    score_moves(p, mv, sc, n, ply);

    int legal = 0;
    for (int i = 0; i < n; i++) {
        pick_best(mv, sc, n, i);
        Undo u;
        if (!make(p, mv[i], u)) continue;
        legal++;
        const int v = -negamax(p, depth - 1, -beta, -alpha, ply + 1);
        unmake(p, mv[i], u);
        if (g_abort) return 0;
        if (v > alpha) {
            alpha = v;
            if (alpha >= beta) {
                // Killer : coup tranquille ayant provoque une coupe a ce ply.
                if (!(mv[i].flags & (MF_CAPTURE | MF_PROMO))) {
                    g_killer[ply][1] = g_killer[ply][0];
                    g_killer[ply][0] = mv[i];
                }
                return beta;
            }
        }
    }
    // Aucun coup legal : mat si le roi est attaque, pat sinon. Le `ply` dans le
    // score du mat fait preferer les mats les plus courts.
    if (legal == 0) return in_check(p, p.side) ? -(MATE_SCORE - ply) : 0;
    return alpha;
}

static int qsearch(Position& p, int alpha, int beta, int qd, int ply) {
    if (g_abort) return 0;
    if ((++g_nodes & 511u) == 0u && esphome::millis() >= g_deadline) { g_abort = true; return 0; }
    if (ply >= MAX_PLY_BUF - 2) return eval(p);

    const bool chk = in_check(p, p.side);
    if (!chk) {
        const int stand = eval(p);
        if (stand >= beta) return beta;
        if (stand > alpha) alpha = stand;
        if (qd <= 0) return alpha;
    } else if (qd <= 0) {
        return eval(p);
    }

    Move*    mv = g_mbuf[ply];
    int16_t* sc = g_sbuf[ply];
    // En echec on doit examiner TOUTES les parades, pas seulement les captures :
    // sinon la quiescence croit qu'une position perdue est calme.
    const int n = gen_impl(p, mv, !chk);
    score_moves(p, mv, sc, n, ply);

    int legal = 0;
    for (int i = 0; i < n; i++) {
        pick_best(mv, sc, n, i);
        Undo u;
        if (!make(p, mv[i], u)) continue;
        legal++;
        const int v = -qsearch(p, -beta, -alpha, qd - 1, ply + 1);
        unmake(p, mv[i], u);
        if (g_abort) return 0;
        if (v > alpha) { alpha = v; if (alpha >= beta) return beta; }
    }
    if (chk && legal == 0) return -(MATE_SCORE - ply);
    return alpha;
}

// --- Machine a etats de la reflexion tranchee ------------------------------
struct SearchState {
    Position pos;
    Move     root[MAX_MOVES];      // coups racine, reordonnes a chaque iteration
    int16_t  rscore[MAX_MOVES];    // score de l'iteration en cours
    Move     fmove[MAX_MOVES];     // instantane de la derniere iteration COMPLETE
    int16_t  fscore[MAX_MOVES];
    int      n_root, n_final;
    int      idx;                  // prochain coup racine a explorer
    int      depth;                // profondeur en cours
    int      best_depth;           // derniere profondeur terminee
    int      alpha;
    int      iter_best;
    Move     best;
    int      best_score;
    uint32_t t_start;   // horodatage du debut de reflexion (mur), pour l'affichage
    uint32_t cpu_ms;    // temps CPU REELLEMENT consomme, cumule sur les tranches
    int      level;
    int      retries;
    bool     active;
};
static SearchState g_ss;

// Budget d'une tranche. SOFT = on arrete d'entamer un nouveau coup racine ;
// HARD = deadline dure passee au negamax, elargie a chaque nouvel echec pour
// garantir qu'un coup racine tres couteux finisse par aboutir.
static constexpr uint32_t SLICE_SOFT_MS = 18;
static const uint32_t SLICE_HARD_MS[3]  = {22, 40, 80};

static void search_finish() {
    const AiLevel& L = AI_LEVELS[g_ss.level];
    // Fenetre de choix : sur les niveaux faibles, on tire au sort parmi les coups
    // « pas trop pires » — c'est ce qui donne un adversaire battable sans le
    // rendre stupide (il garde les captures evidentes en tete de liste).
    if (L.window > 0 && g_ss.n_final > 0 && g_ss.best_depth > 0) {
        const int seuil = g_ss.best_score - (int) L.window;
        int cand[MAX_MOVES], nc = 0;
        for (int i = 0; i < g_ss.n_final; i++)
            if ((int) g_ss.fscore[i] >= seuil) cand[nc++] = i;
        if (nc > 0) g_ss.best = g_ss.fmove[cand[rnd32() % (uint32_t) nc]];
    }
    g_ss.active = false;
}

void search_start(const Position& p, int level, uint32_t seed) {
    init();
    if (level < 0) level = 0;
    if (level >= AI_NLEVELS) level = AI_NLEVELS - 1;
    if (seed) g_rng = seed | 1u;

    memset(g_killer, 0, sizeof(g_killer));
    g_ss.pos        = p;
    g_ss.level      = level;
    g_ss.n_root     = gen_legal(p, g_ss.root);
    g_ss.n_final    = 0;
    g_ss.idx        = 0;
    g_ss.depth      = 1;
    g_ss.best_depth = 0;
    g_ss.alpha      = -INF_SCORE;
    g_ss.iter_best  = 0;
    g_ss.best_score = 0;
    g_ss.t_start    = esphome::millis();
    g_ss.cpu_ms     = 0;
    g_ss.retries    = 0;
    g_nodes         = 0;
    g_qdepth        = AI_LEVELS[level].qdepth;

    if (g_ss.n_root <= 0) {
        g_ss.best = Move{0, 0, 0, 0};
        g_ss.active = false;
        return;
    }
    // Melange initial : deux parties identiques ne donnent pas la meme ouverture.
    for (int i = g_ss.n_root - 1; i > 0; i--) {
        const int j = (int)(rnd32() % (uint32_t)(i + 1));
        const Move t = g_ss.root[i]; g_ss.root[i] = g_ss.root[j]; g_ss.root[j] = t;
    }
    g_ss.best   = g_ss.root[0];
    g_ss.active = true;
}

// Une tranche de reflexion. `t_slice` = horodatage d'entree, sert a mesurer le
// temps CPU consomme (et NON le temps mural : entre deux tranches, LVGL et
// ESPHome tournent, ce temps-la ne doit pas etre facture au budget du niveau).
static bool search_slice(uint32_t t_slice) {
    const AiLevel& L = AI_LEVELS[g_ss.level];
    const uint32_t now = t_slice;
    const uint32_t slice_end = now + SLICE_SOFT_MS;
    const int r = g_ss.retries < 3 ? g_ss.retries : 2;
    g_deadline = now + SLICE_HARD_MS[r];
    g_abort    = false;
    g_qdepth   = L.qdepth;

    for (;;) {
        if (g_ss.idx >= g_ss.n_root) {
            // --- Iteration terminee : on fige le meilleur coup de ce niveau ---
            g_ss.best       = g_ss.root[g_ss.iter_best];
            g_ss.best_score = g_ss.alpha;
            g_ss.best_depth = g_ss.depth;
            g_ss.n_final    = g_ss.n_root;
            for (int i = 0; i < g_ss.n_root; i++) {
                g_ss.fmove[i]  = g_ss.root[i];
                g_ss.fscore[i] = g_ss.rscore[i];
            }
            // Tri decroissant : a la profondeur suivante, explorer d'abord les
            // meilleurs coups multiplie les coupes alpha-beta.
            for (int i = 1; i < g_ss.n_root; i++) {
                const Move   m = g_ss.root[i];
                const int16_t s = g_ss.rscore[i];
                int j = i - 1;
                while (j >= 0 && g_ss.rscore[j] < s) {
                    g_ss.root[j + 1] = g_ss.root[j];
                    g_ss.rscore[j + 1] = g_ss.rscore[j];
                    j--;
                }
                g_ss.root[j + 1] = m;
                g_ss.rscore[j + 1] = s;
            }

            const uint32_t elapsed = g_ss.cpu_ms + (esphome::millis() - t_slice);   // temps CPU
            const bool mate_found  = iabs(g_ss.best_score) > MATE_SCORE - 100;
            g_ss.depth++;
            g_ss.idx = 0;
            g_ss.alpha = -INF_SCORE;
            g_ss.iter_best = 0;
            if (g_ss.depth > L.max_depth || g_ss.depth > 8 ||
                elapsed >= L.budget_ms || mate_found || g_ss.n_root == 1) {
                search_finish();
                return true;
            }
            if (esphome::millis() >= slice_end) return false;
            continue;
        }

        Undo u;
        const Move m = g_ss.root[g_ss.idx];
        if (!make(g_ss.pos, m, u)) { g_ss.idx++; continue; }   // ne devrait pas arriver
        const int v = -negamax(g_ss.pos, g_ss.depth - 1, -INF_SCORE, -g_ss.alpha, 1);
        unmake(g_ss.pos, m, u);

        if (g_abort) {
            // Tranche epuisee au milieu d'un coup racine : on le re-tentera avec
            // un budget elargi. Rien n'est corrompu, la position est restauree.
            g_ss.retries++;
            // Garde-fou 1 : au-dela d'un budget total x3, on arrete la reflexion
            // et on joue le meilleur coup de la derniere profondeur terminee.
            if (g_ss.cpu_ms + (esphome::millis() - t_slice) > (uint32_t) L.budget_ms * 3u &&
                g_ss.best_depth > 0) {
                search_finish();
                return true;
            }
            // Garde-fou 2 : si meme la profondeur 1 n'aboutit pas (systeme tres
            // charge), on finit par jouer le coup racine courant plutot que de
            // relancer la meme tranche indefiniment. g_ss.best est toujours un
            // coup LEGAL (initialise avec root[0] par search_start()).
            if (g_ss.retries > 40) {
                search_finish();
                return true;
            }
            return false;
        }

        g_ss.retries = 0;
        g_ss.rscore[g_ss.idx] = (int16_t)(v > 32000 ? 32000 : (v < -32000 ? -32000 : v));
        if (v > g_ss.alpha) { g_ss.alpha = v; g_ss.iter_best = g_ss.idx; }
        g_ss.idx++;

        if (esphome::millis() >= slice_end) return false;
    }
}

bool search_step() {
    if (!g_ss.active) return true;
    const uint32_t t_slice = esphome::millis();
    const bool done = search_slice(t_slice);
    g_ss.cpu_ms += esphome::millis() - t_slice;
    return done;
}

bool     search_active()     { return g_ss.active; }
Move     search_best()       { return g_ss.best; }
int      search_depth_done() { return g_ss.best_depth; }
int      search_score()      { return g_ss.best_score; }
uint32_t search_nodes()      { return g_nodes; }
uint32_t search_cpu_ms()     { return g_ss.cpu_ms; }
uint16_t search_budget_ms()  { return AI_LEVELS[g_ss.level].budget_ms; }

Move search_quick(const Position& p, int depth, uint16_t max_ms, int* score_out) {
    init();
    Position q = p;
    Move* mv = g_mbuf[0];
    const int n = gen_legal(q, mv);
    if (n <= 0) { if (score_out) *score_out = 0; return Move{0, 0, 0, 0}; }

    // Sauvegarde des seuls scalaires partages : la reflexion en cours du Tab
    // (si elle existe) reprendra intacte a la tranche suivante.
    const bool     s_abort = g_abort;
    const uint32_t s_dead  = g_deadline;
    const uint32_t s_nodes = g_nodes;
    const int      s_qd    = g_qdepth;

    g_abort    = false;
    g_deadline = esphome::millis() + max_ms;
    g_qdepth   = 2;

    Move best = mv[0];
    int  alpha = -INF_SCORE;
    int16_t* sc = g_sbuf[0];
    score_moves(q, mv, sc, n, 0);
    for (int i = 0; i < n; i++) {
        pick_best(mv, sc, n, i);
        Undo u;
        if (!make(q, mv[i], u)) continue;
        const int v = -negamax(q, depth - 1, -INF_SCORE, -alpha, 1);
        unmake(q, mv[i], u);
        if (g_abort) break;               // budget epuise : on garde le meilleur trouve
        if (v > alpha) { alpha = v; best = mv[i]; }
    }
    if (score_out) *score_out = alpha;

    g_abort = s_abort; g_deadline = s_dead; g_nodes = s_nodes; g_qdepth = s_qd;
    return best;
}

}  // namespace Chess
