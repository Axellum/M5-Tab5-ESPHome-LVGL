/**
 * [AI-CONTEXT]
 * @file go_engine.h
 * @role Moteur Go pur (aucune dépendance LVGL / ESPHome / HA).
 * @architecture_constraint Plateau jusqu'à 19×19. Règles : capture par
 *      libertés, suicide interdit, ko simple (pas de superko positionnel),
 *      passe, score chinois (aire) + komi, pierres mortes marquées à la main
 *      en fin de partie (le moteur ne résout PAS la vie/mort tout seul).
 * @architecture_constraint AUCUN gros tableau en pile : tous les scratchs sont
 *      des statiques de module. Le moteur tourne dans le contexte LVGL
 *      mono-thread — il n'est donc PAS réentrant, et n'a pas à l'être. C'était
 *      la cause du crash de la version précédente (≈5 Ko de pile par niveau de
 *      récursion de l'IA, dont 2,2 Ko rien que pour count_liberties).
 * @architecture_constraint PASS = -1 (hors domaine 0..360). Ne JAMAIS utiliser
 *      255 : en 19×19 l'indice 255 est une intersection réelle.
 * @ai_instruction Utilisable tel quel par go_ai / go_game et par
 *      tools/test_go_engine.cpp (tests host).
 */
#pragma once
#include <cstdint>
#include <cstring>

namespace Go {
namespace Engine {

static constexpr int MAX_N  = 19;
static constexpr int MAX_SQ = MAX_N * MAX_N;  // 361
static constexpr int PASS   = -1;             // coup « passe » (hors domaine)

enum Color : uint8_t { EMPTY = 0, BLACK = 1, WHITE = 2 };

// Valeurs de la carte de territoire (territory_map).
enum Terr : uint8_t { T_NONE = 0, T_BLACK = 1, T_WHITE = 2, T_DAME = 3 };

inline Color opp(Color c) { return c == BLACK ? WHITE : BLACK; }

struct Pos {
    uint8_t sq[MAX_SQ];   // Color
    uint8_t n;            // 9, 13 ou 19
    uint8_t side;         // Color to play (BLACK/WHITE)
    uint8_t passes;       // passes consécutives (2 = fin)
    uint8_t reserved;
    int16_t ko;           // case interdite par ko simple, PASS = aucune
    uint16_t move_no;
    uint16_t captured_by_black;  // prisonniers pris par Noir (= Blancs capturés)
    uint16_t captured_by_white;
};

struct Score {
    float black;         // aire noire (pierres vivantes + territoire)
    float white;         // aire blanche + komi
    int   black_stones;  // pierres noires VIVANTES sur le plateau
    int   white_stones;
    int   black_terr;    // territoire noir (inclut les pierres blanches mortes)
    int   white_terr;
    int   black_dead;    // pierres noires marquées mortes
    int   white_dead;
    int   dame;          // points neutres
};

void pos_init(Pos& p, int n);
inline int  idx(int r, int c, int n) { return r * n + c; }
inline bool on(int r, int c, int n) { return r >= 0 && c >= 0 && r < n && c < n; }

// Libertés de la chaîne contenant `sq` (0 si la case est vide).
// Si `group_out` est fourni (capacité MAX_SQ), il reçoit les indices des pierres
// de la chaîne et `*group_size` leur nombre.
int chain_liberties(const Pos& p, int sq, int16_t* group_out = nullptr,
                    int* group_size = nullptr);

// Raccourci lisible quand seul le nombre de libertés compte.
inline int count_liberties(const Pos& p, int sq) { return chain_liberties(p, sq); }

// true si le coup est légal (case vide, pas suicide, pas ko).
// Test EXACT et sans copie de position : on regarde les 4 voisins et les
// libertés des chaînes adjacentes, jamais le plateau entier.
bool is_legal(const Pos& p, int sq);

// Joue `sq` (ou PASS). Retourne false si illégal. Met à jour ko / passes /
// compteurs de prisonniers. Le coup est appliqué EN PLACE (pas de copie).
bool play(Pos& p, int sq);

// Liste les coups légaux (placements) dans `out` ; retourne le nombre.
// N'inclut PAS la passe (l'appelant peut toujours passer).
int gen_moves(const Pos& p, int* out, int max_out);

// true si `sq` est un œil simple de la couleur `col` : les 4 orthogonaux sont
// de `col` (ou hors plateau) et au plus une diagonale est adverse (aucune sur
// un bord). Sert à empêcher l'IA de se crever les yeux toute seule.
bool is_eye(const Pos& p, int sq, uint8_t col);

// Marque toute la chaîne contenant `sq` dans `flags` (tableau MAX_SQ) à `value`.
// Utilisé par l'écran de marquage des pierres mortes.
void mark_chain(const Pos& p, int sq, uint8_t* flags, uint8_t value);

// Score chinois (aire) : pierres vivantes + territoire contrôlé, komi aux
// Blancs. `dead` (optionnel, MAX_SQ) marque les pierres considérées MORTES :
// elles sont retirées du plateau avant le comptage, donc comptent comme
// territoire adverse. Passer nullptr = toutes les pierres sont vivantes.
void score_chinese(const Pos& p, float komi, const uint8_t* dead, Score& out);

// Remplit `out` (MAX_SQ) avec la propriété de chaque intersection VIDE
// (T_BLACK / T_WHITE / T_DAME) ; T_NONE sur les intersections occupées par une
// pierre vivante. Même convention de `dead` que score_chinese.
void territory_map(const Pos& p, const uint8_t* dead, uint8_t* out);

// Place les pierres de handicap (2..9) sur une position VIERGE de taille n et
// donne le trait aux Blancs. Retourne le nombre de pierres posées (0 si h < 2).
int place_handicap(Pos& p, int h);

// Fin de partie : deux passes consécutives.
inline bool is_over(const Pos& p) { return p.passes >= 2; }

}  // namespace Engine
}  // namespace Go
