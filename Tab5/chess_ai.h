/**
 * [AI-CONTEXT]
 * @file chess_ai.h
 * @role Jeu « Roi Noir » — MOTEUR d'echecs pur : representation du plateau,
 *      generation de coups legaux, make/unmake, evaluation, recherche
 *      negamax alpha-beta time-slicee, FEN, SAN, perft.
 * @architecture_constraint Ce fichier ne connait NI LVGL NI Home Assistant : il
 *      ne manipule que des entiers. Toute l'UI vit dans chess_game.cpp. C'est ce
 *      qui rend `perft()` verifiable et la recherche reutilisable telle quelle.
 * @ai_instruction Encodage du plateau = MAILBOX 0x88 (board[128], index =
 *      rang*16 + colonne, hors-echiquier ssi (sq & 0x88)). Ne PAS convertir en
 *      bitboards 64 bits : l'ESP32-P4 est un cœur 32 bits, les tests hors-plateau
 *      en un seul AND sont ici plus rapides qu'un jeu de masques 64 bits.
 *      La recherche NE DOIT JAMAIS tourner d'une traite : elle est decoupee en
 *      tranches par search_step(), appele par le lv_timer de chess_game.cpp.
 */
#pragma once
#include <cstdint>

namespace Chess {

// ===========================================================================
// 1. Codage des pieces et des cases
// ===========================================================================
// Une piece tient sur 4 bits : bit 3 = couleur, bits 0-2 = type.
// C'est ce qui permet `p & COLOR_MASK` (couleur) et `p & TYPE_MASK` (type) sans
// table de correspondance.
enum : uint8_t {
    EMPTY  = 0,
    PAWN   = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6,
    WHITE  = 0, BLACK  = 8,
    COLOR_MASK = 8, TYPE_MASK = 7,
};

// Case invalide (prise en passant indisponible, etc.). 0x7F est hors-plateau
// au sens 0x88, donc jamais confondue avec une vraie case.
static constexpr uint8_t NO_SQ = 0x7F;

// Drapeaux portes par un coup.
enum : uint8_t {
    MF_CAPTURE = 1,   // capture ordinaire
    MF_EP      = 2,   // prise en passant (la case videe n'est PAS `to`)
    MF_CASTLE  = 4,   // roque (la tour bouge aussi)
    MF_DOUBLE  = 8,   // poussee de pion de deux cases
    MF_PROMO   = 16,  // promotion (le type promu est dans Move::promo)
};

// Droits de roque, un bit par cote.
enum : uint8_t { CR_WK = 1, CR_WQ = 2, CR_BK = 4, CR_BQ = 8 };

// Bornes des tampons. 218 est le maximum theorique de coups legaux dans une
// position ; on arrondit a 220.
//
// MAX_PLY_BUF couvre profondeur max + quiescence : la recherche plafonne a la
// profondeur 8 (garde-fou de search_step) et la quiescence a 6 plies, soit 14
// au pire ; 16 laisse la marge du garde-fou `ply >= MAX_PLY_BUF - 2`.
// [AI-CONTEXT] Ce nombre coute cher : chaque ply reserve 220 coups (4 o) + 220
// scores de tri (2 o), soit 1,3 Ko de .bss par ply. Ne PAS l'augmenter « au cas
// ou » — mesurer d'abord avec riscv32-esp-elf-size.
static constexpr int MAX_MOVES   = 220;
static constexpr int MAX_PLY_BUF = 16;

// Historique de partie (demi-coups). 320 demi-coups = 160 coups, soit bien
// au-dela d'une partie normale ; au-dela on arrete d'empiler et la partie est
// declaree nulle (cf. play_move). Chaque ply coute 32 octets (coup, annulation,
// clef Zobrist, SAN, numero, trait).
static constexpr int MAX_HIST = 320;

// Scores en centiemes de pion. MATE_SCORE est volontairement loin des bornes
// d'un int16_t pour que MATE_SCORE - ply reste representable.
static constexpr int MATE_SCORE = 30000;
static constexpr int INF_SCORE  = 32000;

// ---------------------------------------------------------------------------
// Un coup. 4 octets : tient dans un registre, se copie sans indirection.
// ---------------------------------------------------------------------------
struct Move {
    uint8_t from;   // case de depart (index 0x88)
    uint8_t to;     // case d'arrivee (index 0x88)
    uint8_t promo;  // type promu (QUEEN/ROOK/BISHOP/KNIGHT), 0 si pas de promotion
    uint8_t flags;  // combinaison de MF_*
};

static inline bool move_eq(const Move& a, const Move& b) {
    return a.from == b.from && a.to == b.to && a.promo == b.promo;
}
static inline bool move_null(const Move& m) { return m.from == m.to; }

// ---------------------------------------------------------------------------
// Etat necessaire pour annuler un coup (unmake). Volontairement compact :
// il en existe un par ply de recherche.
// ---------------------------------------------------------------------------
struct Undo {
    uint8_t captured;  // piece capturee (EMPTY si aucune)
    uint8_t cap_sq;    // case reellement videe — differe de `to` en prise en passant
    uint8_t castling;  // droits de roque AVANT le coup
    uint8_t ep;        // case de prise en passant AVANT le coup
    uint8_t halfmove;  // compteur des 50 coups AVANT le coup
};

// ---------------------------------------------------------------------------
// Position complete. Trivially copyable : la recherche en fait une copie
// racine, et le mode « analyse d'indice » travaille sur un clone.
// ---------------------------------------------------------------------------
struct Position {
    uint8_t  board[128];   // mailbox 0x88
    uint8_t  side;         // WHITE ou BLACK : trait
    uint8_t  castling;     // masque CR_*
    uint8_t  ep;           // case de prise en passant ou NO_SQ
    uint8_t  halfmove;     // demi-coups depuis la derniere capture / poussee de pion
    uint16_t fullmove;     // numero du coup (commence a 1)
    uint8_t  king_sq[2];   // case du roi — index 0 = blancs, 1 = noirs
};

// Index de couleur (0 = blancs, 1 = noirs) a partir du code couleur.
static inline int cidx(uint8_t color) { return color >> 3; }

// ===========================================================================
// 2. Niveaux de difficulte
// ===========================================================================
// `window` = fenetre de choix aleatoire autour du meilleur score (en centiemes
// de pion). Plus elle est large, plus le Tab joue « humainement mal » : c'est
// ce qui rend le niveau Pion battable par un debutant sans le rendre absurde.
struct AiLevel {
    const char* name;
    const char* desc;
    uint8_t     max_depth;   // profondeur nominale de l'iterative deepening
    uint8_t     qdepth;      // plies de quiescence (0 = desactivee)
    uint16_t    budget_ms;   // temps de reflexion total vise
    uint16_t    window;      // fenetre de choix aleatoire (centiemes de pion)
    uint16_t    elo;         // Elo fictif de reference (classement local)
};

static constexpr int AI_NLEVELS = 5;
extern const AiLevel AI_LEVELS[AI_NLEVELS];

// ===========================================================================
// 3. API du moteur
// ===========================================================================

// Initialise les tables internes (Zobrist, masques de roque). Idempotent.
void init();

// Position de depart FIDE.
void set_start(Position& p);

// Charge une position FEN. Retourne false si la chaine est invalide (la
// position est alors laissee dans un etat indefini : recharger set_start()).
bool set_fen(Position& p, const char* fen);

// Serialise la position en FEN. `cap` doit valoir au moins 92.
void get_fen(const Position& p, char* out, int cap);

// Clef Zobrist de la position, RECALCULEE integralement.
// [AI-CONTEXT] Volontairement absente de make()/unmake() : la clef ne sert qu'a
// la detection de repetition au niveau PARTIE (quelques appels par minute), pas
// dans la recherche (des centaines de milliers d'appels). Sortir le hachage du
// hot-path est le compromis qui fait gagner le plus de nœuds/seconde ici.
uint64_t hash_of(const Position& p);

// Generation pseudo-legale (le roi peut rester en echec ; make() filtre).
// `out` doit pouvoir accueillir MAX_MOVES coups. Retourne le nombre de coups.
int gen_moves(const Position& p, Move* out);

// Captures + promotions uniquement (utilise par la quiescence).
int gen_captures(const Position& p, Move* out);

// Generation STRICTEMENT legale — utilisee par l'UI (surbrillance des coups),
// la detection mat/pat et la desambiguisation SAN. Plus lente : ne pas
// l'appeler dans la recherche.
int gen_legal(const Position& p, Move* out);

// Joue le coup. Retourne false si le coup laisse le roi du joueur en echec :
// dans ce cas la position est deja restauree, il n'y a PAS de unmake() a faire.
bool make(Position& p, const Move& m, Undo& u);

// Annule un coup precedemment accepte par make().
void unmake(Position& p, const Move& m, const Undo& u);

// La case `sq` est-elle attaquee par le camp `by` ?
bool attacked(const Position& p, uint8_t sq, uint8_t by);

// Le roi du camp `color` est-il en echec ?
bool in_check(const Position& p, uint8_t color);

// Evaluation statique, du point de vue du trait (convention negamax).
int eval(const Position& p);

// Materiel insuffisant pour mater (R/R, R+F/R, R+C/R, R+F/R+F meme couleur).
bool insufficient_material(const Position& p);

// Perft : compte les feuilles legales a `depth`. Sert a prouver le generateur.
uint64_t perft(Position& p, int depth);

// Perft de la position initiale sur 1..depth, resultats compares aux valeurs
// FIDE connues et journalises via ESP_LOGI. Retourne true si tout concorde.
bool perft_selftest(int depth);

// ===========================================================================
// 4. Notation
// ===========================================================================

// SAN francais (R D T F C, pas de lettre pour les pions), avec desambiguisation,
// « x », « =D », « + » / « # » et « O-O » / « O-O-O ».
// `before` = position AVANT le coup. `cap` >= 12.
void move_to_san(const Position& before, const Move& m, char* out, int cap);

// Coordonnees pures type « e2e4 » / « e7e8d ». `cap` >= 6.
void move_to_uci(const Move& m, char* out, int cap);

// ===========================================================================
// 5. Recherche time-slicee
// ===========================================================================
// [AI-CONTEXT] Modele de decoupage : la recherche est fractionnee AU NIVEAU DES
// COUPS RACINE. Chaque appel a search_step() explore autant de coups racine que
// la tranche le permet puis rend la main a LVGL. A l'interieur d'un coup racine,
// un garde-fou temporel (deadline dure) avorte la descente ; le coup est alors
// simplement re-tente a la tranche suivante avec un budget elargi. Consequence
// utile : des la profondeur 1 terminee, un coup jouable est toujours disponible.

// Demarre une reflexion sur `p` avec les parametres du niveau `level`.
// `seed` alimente le tirage aleatoire de la fenetre de choix.
void search_start(const Position& p, int level, uint32_t seed);

// Fait avancer la recherche d'une tranche (~18 ms). Retourne true quand la
// reflexion est terminee (budget epuise ou profondeur max atteinte).
bool search_step();

// True tant qu'une reflexion est en cours.
bool search_active();

// Meilleur coup connu a cet instant (valide des la profondeur 1 terminee).
Move search_best();

// Profondeur reellement terminee, score du meilleur coup (point de vue du
// joueur au trait a la racine), et nœuds explores — affiches au HUD.
int      search_depth_done();
int      search_score();
uint32_t search_nodes();

// Temps CPU reellement consomme par la reflexion en cours (somme des tranches).
// C'est LUI qu'il faut afficher : le temps mural inclut tout ce que LVGL et
// ESPHome ont fait entre deux tranches.
uint32_t search_cpu_ms();

// Budget CPU vise par le niveau courant (pour la barre de progression).
uint16_t search_budget_ms();

// Recherche BLOQUANTE courte, utilisee pour l'indice et pour l'acceptation
// d'une proposition de nulle. Bornee par `max_ms` (garde-fou dur) : ne jamais
// l'appeler avec plus de ~40 ms.
Move search_quick(const Position& p, int depth, uint16_t max_ms, int* score_out);

}  // namespace Chess
