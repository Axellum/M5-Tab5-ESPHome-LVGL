/**
 * [AI-CONTEXT]
 * @file go_engine.h
 * @role Moteur Go pur (aucune dépendance LVGL / ESPHome / HA).
 * @architecture_constraint Plateau jusqu'à 19×19. Règles : capture par
 *      libertés, suicide interdit, ko simple (pas de superko positionnel),
 *      passe, score chinois (aire) + komi. Life/death avancé non résolu :
 *      en fin de partie toutes les chaînes encore en vie comptent ;
 *      documenté dans README.
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
static constexpr int PASS   = 255;            // coup « passe »

enum Color : uint8_t { EMPTY = 0, BLACK = 1, WHITE = 2 };

inline Color opp(Color c) { return c == BLACK ? WHITE : BLACK; }

struct Pos {
    uint8_t sq[MAX_SQ];   // Color
    uint8_t n;            // 9, 13 ou 19
    uint8_t side;         // Color to play (BLACK/WHITE)
    uint8_t ko;           // case interdite par ko simple, PASS = aucune
    uint8_t passes;       // passes consécutives (2 = fin)
    uint8_t reserved;
    uint16_t move_no;
    uint16_t captured_by_black;  // prisonniers pris par Noir (= Blancs capturés)
    uint16_t captured_by_white;
};

struct Score {
    float black;   // aire noire (pierres + territoire)
    float white;   // aire blanche + komi
    int   black_stones;
    int   white_stones;
    int   black_terr;
    int   white_terr;
    int   dame;    // points neutres
};

void pos_init(Pos& p, int n);
inline int  idx(int r, int c, int n) { return r * n + c; }
inline bool on(int r, int c, int n) { return r >= 0 && c >= 0 && r < n && c < n; }

// Libertés d'une chaîne contenant `sq` (0 si vide). Remplit optionnellement
// le masque des pierres de la chaîne (bits dans `group_mask`, taille MAX_SQ).
int count_liberties(const Pos& p, int sq, uint8_t* group_mask = nullptr);

// true si le coup est légal (case vide, pas suicide, pas ko).
bool is_legal(const Pos& p, int sq);

// Joue `sq` (ou PASS). Retourne false si illégal. Met à jour ko / passes / capturés.
bool play(Pos& p, int sq);

// Liste les coups légaux (placements) dans `out` ; retourne le nombre.
// N'inclut PAS la passe (l'appelant peut toujours passer).
int gen_moves(const Pos& p, int* out, int max_out);

// Score chinois (aire) : pierres + territoire contrôlé. Komi ajouté aux Blancs.
// `komi` typique 6.5f. Les chaînes encore présentes sont considérées vivantes.
void score_chinese(const Pos& p, float komi, Score& out);

// Éval simple (+Noir / −Blanc) pour l'IA : pierres + 0.5*territoire approx + captures.
int eval(const Pos& p);

// Fin de partie : deux passes consécutives.
inline bool is_over(const Pos& p) { return p.passes >= 2; }

}  // namespace Engine
}  // namespace Go
